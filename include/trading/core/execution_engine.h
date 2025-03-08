#pragma once

#include "trading/core/order_book.h"
#include "trading/core/strategy_engine.h"
#include "trading/utils/memory_pool.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace trading {

// Forward declarations
class MarketDataHandler;

// Order status
enum class OrderStatus : uint8_t {
    NEW = 0,
    PENDING = 1,
    PARTIALLY_FILLED = 2,
    FILLED = 3,
    CANCELED = 4,
    REJECTED = 5
};

// Execution report
struct ExecutionReport {
    OrderId order_id;
    OrderStatus status;
    Price price;
    Quantity exec_quantity;
    Quantity leaves_quantity;
    std::string symbol;
    Timestamp timestamp;
    
    // Constructor
    ExecutionReport(OrderId id, OrderStatus st, Price p, Quantity exec_qty, Quantity leaves_qty, 
                    std::string_view sym, Timestamp ts)
        : order_id(id), status(st), price(p), exec_quantity(exec_qty), leaves_quantity(leaves_qty),
          symbol(sym), timestamp(ts) {}
    
    // Default constructor
    ExecutionReport() : order_id(0), status(OrderStatus::NEW), price(0), 
                        exec_quantity(0), leaves_quantity(0), timestamp(0) {}
};

// Execution order
struct ExecutionOrder {
    OrderId order_id;
    Price price;
    Quantity quantity;
    Side side;
    std::string symbol;
    Timestamp timestamp;
    
    // Constructor
    ExecutionOrder(OrderId id, Price p, Quantity q, Side s, std::string_view sym, Timestamp ts)
        : order_id(id), price(p), quantity(q), side(s), symbol(sym), timestamp(ts) {}
    
    // Default constructor
    ExecutionOrder() : order_id(0), price(0), quantity(0), side(Side::BUY), timestamp(0) {}
};

// Execution engine class
class ExecutionEngine {
public:
    // Constructor
    ExecutionEngine(std::shared_ptr<MarketDataHandler> market_data);
    
    // Destructor
    ~ExecutionEngine();
    
    // Start the execution engine
    void start();
    
    // Stop the execution engine
    void stop();
    
    // Submit an order for execution
    OrderId submit_order(const Signal& signal);
    
    // Cancel an order
    bool cancel_order(OrderId order_id);
    
    // Set execution report callback
    void set_execution_callback(std::function<void(const ExecutionReport&)> callback);
    
    // Get order status
    OrderStatus get_order_status(OrderId order_id) const;
    
private:
    // Market data handler
    std::shared_ptr<MarketDataHandler> market_data_;
    
    // Order ID counter
    std::atomic<OrderId> next_order_id_;
    
    // Pending orders
    mutable std::unordered_map<OrderId, ExecutionOrder> pending_orders_;
    
    // Memory pool for execution reports
    MemoryPool<ExecutionReport> report_pool_;
    
    // Execution callback
    std::function<void(const ExecutionReport&)> execution_callback_;
    
    // Order processing thread
    std::thread processing_thread_;
    
    // Queue of orders to process
    mutable std::deque<OrderId> order_queue_;
    
    // Mutex for thread safety
    mutable std::mutex mutex_;
    
    // Condition variable for thread signaling
    std::condition_variable condition_;
    
    // Running flag
    std::atomic<bool> running_;
    
    // Order processing function
    void process_orders();
    
    // Simulate order execution
    void simulate_execution(const ExecutionOrder& order);
};

} // namespace trading