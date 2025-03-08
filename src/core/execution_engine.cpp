#include "trading/core/execution_engine.h"
#include "trading/core/market_data.h"
#include <algorithm>
#include <chrono>
#include <random>
#include <thread>

namespace trading {

ExecutionEngine::ExecutionEngine(std::shared_ptr<MarketDataHandler> market_data)
    : market_data_(std::move(market_data)), next_order_id_(1), running_(false) {
}

ExecutionEngine::~ExecutionEngine() {
    stop();
}

void ExecutionEngine::start() {
    if (running_) {
        return;  // Already running
    }
    
    running_ = true;
    
    // Start order processing thread
    processing_thread_ = std::thread(&ExecutionEngine::process_orders, this);
}

void ExecutionEngine::stop() {
    if (!running_) {
        return;  // Already stopped
    }
    
    running_ = false;
    
    // Signal processing thread to exit
    condition_.notify_all();
    
    // Wait for processing thread to exit
    if (processing_thread_.joinable()) {
        processing_thread_.join();
    }
}

OrderId ExecutionEngine::submit_order(const Signal& signal) {
    // Generate a new order ID
    OrderId order_id = next_order_id_++;
    
    // Convert signal to execution order
    ExecutionOrder order(
        order_id,
        signal.price,
        signal.quantity,
        signal.type == SignalType::BUY ? Side::BUY : Side::SELL,
        signal.symbol,
        signal.timestamp
    );
    
    // Create execution report
    ExecutionReport* report = report_pool_.get();
    *report = ExecutionReport(
        order_id,
        OrderStatus::NEW,
        signal.price,
        0,
        signal.quantity,
        signal.symbol,
        static_cast<Timestamp>(std::chrono::system_clock::now().time_since_epoch().count())
    );
    
    {
        // Add order to pending orders
        std::lock_guard<std::mutex> lock(mutex_);
        pending_orders_[order_id] = order;
        
        // Add order to processing queue
        order_queue_.push_back(order_id);
    }
    
    // Send execution report
    if (execution_callback_) {
        execution_callback_(*report);
    }
    
    // Return report to pool
    report_pool_.release(report);
    
    // Signal processing thread
    condition_.notify_one();
    
    return order_id;
}

bool ExecutionEngine::cancel_order(OrderId order_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Find order
    auto it = pending_orders_.find(order_id);
    if (it == pending_orders_.end()) {
        return false;  // Order not found
    }
    
    // Cannot cancel filled orders
    OrderStatus status = get_order_status(order_id);
    if (status == OrderStatus::FILLED) {
        return false;
    }
    
    // Create execution report
    ExecutionReport* report = report_pool_.get();
    *report = ExecutionReport(
        order_id,
        OrderStatus::CANCELED,
        it->second.price,
        0,
        it->second.quantity,
        it->second.symbol,
        static_cast<Timestamp>(std::chrono::system_clock::now().time_since_epoch().count())
    );
    
    // Remove order from pending orders
    pending_orders_.erase(it);
    
    // Send execution report
    if (execution_callback_) {
        execution_callback_(*report);
    }
    
    // Return report to pool
    report_pool_.release(report);
    
    return true;
}

void ExecutionEngine::set_execution_callback(std::function<void(const ExecutionReport&)> callback) {
    execution_callback_ = std::move(callback);
}

OrderStatus ExecutionEngine::get_order_status(OrderId order_id) const {
    // Note: we're using a const_cast here because the implementation needs to lock the mutex
    // but doesn't actually modify the logical state of the object
    auto& non_const_this = const_cast<ExecutionEngine&>(*this);
    std::lock_guard<std::mutex> lock(non_const_this.mutex_);
    
    // Find order
    auto it = pending_orders_.find(order_id);
    if (it == pending_orders_.end()) {
        return OrderStatus::REJECTED;  // Order not found
    }
    
    // Determine status based on order queue position
    auto queue_it = std::find(order_queue_.begin(), order_queue_.end(), order_id);
    if (queue_it == order_queue_.end()) {
        return OrderStatus::FILLED;  // Not in queue, must be filled
    }
    
    if (queue_it == order_queue_.begin()) {
        return OrderStatus::PENDING;  // At front of queue, processing
    }
    
    return OrderStatus::NEW;  // In queue, waiting
}

void ExecutionEngine::process_orders() {
    while (running_) {
        OrderId order_id = 0;
        
        {
            // Wait for an order to process
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait(lock, [this] {
                return !running_ || !order_queue_.empty();
            });
            
            if (!running_) {
                break;  // Exit the thread
            }
            
            if (order_queue_.empty()) {
                continue;  // Spurious wakeup
            }
            
            // Get the next order to process
            order_id = order_queue_.front();
            order_queue_.pop_front();
        }
        
        // Get the order
        ExecutionOrder order;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = pending_orders_.find(order_id);
            if (it == pending_orders_.end()) {
                continue;  // Order not found
            }
            order = it->second;
        }
        
        // Simulate order execution
        simulate_execution(order);
    }
}

void ExecutionEngine::simulate_execution(const ExecutionOrder& order) {
    // Get the order book for this symbol
    auto order_book = market_data_->get_order_book(order.symbol);
    if (!order_book) {
        // Order book not found, reject the order
        ExecutionReport* report = report_pool_.get();
        *report = ExecutionReport(
            order.order_id,
            OrderStatus::REJECTED,
            order.price,
            0,
            order.quantity,
            order.symbol,
            static_cast<Timestamp>(std::chrono::system_clock::now().time_since_epoch().count())
        );
        
        // Send execution report
        if (execution_callback_) {
            execution_callback_(*report);
        }
        
        // Return report to pool
        report_pool_.release(report);
        
        // Remove order from pending orders
        std::lock_guard<std::mutex> lock(mutex_);
        pending_orders_.erase(order.order_id);
        
        return;
    }
    
    // Get best bid/ask
    auto best_bid = order_book->best_bid();
    auto best_ask = order_book->best_ask();
    
    // Check if the order can be filled
    bool can_fill = false;
    Price fill_price = order.price;
    
    if (order.side == Side::BUY && best_ask && order.price >= *best_ask) {
        can_fill = true;
        fill_price = *best_ask;
    } else if (order.side == Side::SELL && best_bid && order.price <= *best_bid) {
        can_fill = true;
        fill_price = *best_bid;
    }
    
    // Simulate latency
    std::this_thread::sleep_for(std::chrono::microseconds(100));
    
    if (can_fill) {
        // Simulate full fill
        ExecutionReport* report = report_pool_.get();
        *report = ExecutionReport(
            order.order_id,
            OrderStatus::FILLED,
            fill_price,
            order.quantity,
            0,
            order.symbol,
            static_cast<Timestamp>(std::chrono::system_clock::now().time_since_epoch().count())
        );
        
        // Send execution report
        if (execution_callback_) {
            execution_callback_(*report);
        }
        
        // Return report to pool
        report_pool_.release(report);
        
        // Remove order from pending orders
        std::lock_guard<std::mutex> lock(mutex_);
        pending_orders_.erase(order.order_id);
    } else {
        // Simulate partial fill with random quantity
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<Quantity> dist(1, order.quantity);
        Quantity exec_quantity = dist(gen);
        
        ExecutionReport* report = report_pool_.get();
        *report = ExecutionReport(
            order.order_id,
            OrderStatus::PARTIALLY_FILLED,
            order.price,
            exec_quantity,
            order.quantity - exec_quantity,
            order.symbol,
            static_cast<Timestamp>(std::chrono::system_clock::now().time_since_epoch().count())
        );
        
        // Send execution report
        if (execution_callback_) {
            execution_callback_(*report);
        }
        
        // Return report to pool
        report_pool_.release(report);
        
        // Update order in pending orders
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = pending_orders_.find(order.order_id);
            if (it != pending_orders_.end()) {
                it->second.quantity -= exec_quantity;
            }
        }
        
        // Put order back in queue for further processing
        {
            std::lock_guard<std::mutex> lock(mutex_);
            order_queue_.push_back(order.order_id);
        }
        
        // Signal processing thread
        condition_.notify_one();
    }
}

} // namespace trading