#include "trading/core/execution_engine.h"
#include "trading/core/market_data.h"
#include "trading/core/order_book.h"
#include "trading/core/strategy_engine.h"
#include "trading/support/config.h"
#include "trading/support/logger.h"
#include "trading/utils/timekeeper.h"

#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

using namespace trading;

// Global flag for running the simulator
std::atomic<bool> g_running = true;

// Signal handler for graceful shutdown
void signal_handler(int signal) {
    std::cout << "Received signal " << signal << ", shutting down..." << std::endl;
    g_running = false;
}

// Generate random market data for testing
std::vector<uint8_t> generate_market_data(const std::vector<std::string>& symbols, size_t num_messages) {
    std::vector<uint8_t> data;
    data.reserve(num_messages * 64);  // Reserve space for messages
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> msg_type_dist(1, 5);  // Message types 1-5
    std::uniform_int_distribution<> symbol_dist(0, symbols.size() - 1);
    std::uniform_int_distribution<> price_dist(9000, 11000);  // Price between 90.00 and 110.00
    std::uniform_int_distribution<> quantity_dist(1, 100);
    std::uniform_int_distribution<> side_dist(0, 1);  // 0 = Buy, 1 = Sell
    
    uint64_t order_id = 1;
    
    for (size_t i = 0; i < num_messages; ++i) {
        // Create a new market data message
        MarketDataMessage msg;
        msg.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
        msg.type = static_cast<MessageType>(msg_type_dist(gen));
        
        // Select a random symbol
        const std::string& symbol = symbols[symbol_dist(gen)];
        msg.symbol_length = static_cast<uint8_t>(symbol.size());
        
        // Fill in message-specific fields
        switch (msg.type) {
            case MessageType::ADD_ORDER:
                msg.add_order.order_id = order_id++;
                msg.add_order.price = price_dist(gen);
                msg.add_order.quantity = quantity_dist(gen);
                msg.add_order.side = side_dist(gen);
                break;
                
            case MessageType::MODIFY_ORDER:
                msg.modify_order.order_id = std::max(1ULL, order_id - 1);
                msg.modify_order.quantity = quantity_dist(gen);
                break;
                
            case MessageType::CANCEL_ORDER:
                msg.cancel_order.order_id = std::max(1ULL, order_id - 1);
                break;
                
            case MessageType::EXECUTE_ORDER:
                msg.execute_order.order_id = std::max(1ULL, order_id - 1);
                msg.execute_order.exec_quantity = quantity_dist(gen);
                msg.execute_order.exec_price = price_dist(gen);
                break;
                
            case MessageType::TRADE:
                msg.trade.price = price_dist(gen);
                msg.trade.quantity = quantity_dist(gen);
                msg.trade.aggressor_side = side_dist(gen);
                break;
                
            default:
                break;
        }
        
        // Copy message to buffer
        const uint8_t* msg_bytes = reinterpret_cast<const uint8_t*>(&msg);
        data.insert(data.end(), msg_bytes, msg_bytes + sizeof(MarketDataMessage));
        
        // Copy symbol to buffer
        data.insert(data.end(), symbol.begin(), symbol.end());
    }
    
    return data;
}

// Execution report callback
void on_execution_report(const ExecutionReport& report) {
    std::string status;
    switch (report.status) {
        case OrderStatus::NEW: status = "NEW"; break;
        case OrderStatus::PENDING: status = "PENDING"; break;
        case OrderStatus::PARTIALLY_FILLED: status = "PARTIALLY_FILLED"; break;
        case OrderStatus::FILLED: status = "FILLED"; break;
        case OrderStatus::CANCELED: status = "CANCELED"; break;
        case OrderStatus::REJECTED: status = "REJECTED"; break;
        default: status = "UNKNOWN"; break;
    }
    
    LOG_INFO("Execution report: id=" + std::to_string(report.order_id) + 
             ", status=" + status + 
             ", price=" + std::to_string(report.price) + 
             ", exec_qty=" + std::to_string(report.exec_quantity) + 
             ", leaves_qty=" + std::to_string(report.leaves_quantity) + 
             ", symbol=" + report.symbol);
}

// Signal callback
void on_signal(const Signal& signal) {
    std::string type;
    switch (signal.type) {
        case SignalType::BUY: type = "BUY"; break;
        case SignalType::SELL: type = "SELL"; break;
        default: type = "NONE"; break;
    }
    
    LOG_INFO("Signal: type=" + type + 
             ", symbol=" + signal.symbol + 
             ", price=" + std::to_string(signal.price) + 
             ", quantity=" + std::to_string(signal.quantity) + 
             ", confidence=" + std::to_string(signal.confidence));
}

int main(int argc, char* argv[]) {
    // Register signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    // Initialize logger
    Logger::instance().initialize("trading_simulator.log", LogLevel::INFO);
    Logger::instance().start();
    
    LOG_INFO("Trading Simulator starting up");
    
    // Load configuration
    ConfigManager::instance().set("market_data.buffer_size", "1048576"); // 1 MB
    ConfigManager::instance().set("symbols", "AAPL,MSFT,GOOG,AMZN,FB");
    ConfigManager::instance().set("strategy.stat_arb.z_score_threshold", "2.0");
    ConfigManager::instance().set("strategy.stat_arb.window_size", "100");
    
    // Parse command-line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg.find("--config=") == 0) {
            std::string config_file = arg.substr(9);
            if (ConfigManager::instance().load_file(config_file)) {
                LOG_INFO("Loaded configuration from " + config_file);
            } else {
                LOG_ERROR("Failed to load configuration from " + config_file);
            }
        }
    }
    
    // Get list of symbols
    std::vector<std::string> symbols;
    auto symbols_value = ConfigManager::instance().get("symbols");
    symbols = symbols_value.as_string_list();
    
    LOG_INFO("Trading " + std::to_string(symbols.size()) + " symbols: " + std::string(symbols_value.as_string()));
    
    // Create market data handler
    auto market_data = std::make_shared<MarketDataHandler>(
        ConfigManager::instance().get("market_data.buffer_size").as_uint()
    );
    
    // Subscribe to market data for all symbols
    for (const auto& symbol : symbols) {
        market_data->subscribe(symbol, [](const MarketDataMessage&, std::string_view) {
            // Just a dummy callback for demonstration
        });
    }
    
    // Create strategy engine
    auto strategy_engine = std::make_shared<StrategyEngine>(market_data);
    
    // Register statistical arbitrage strategy
    auto stat_arb = std::make_shared<StatArbitrageStrategy>(
        symbols,
        ConfigManager::instance().get("strategy.stat_arb.z_score_threshold").as_double(),
        ConfigManager::instance().get("strategy.stat_arb.window_size").as_uint()
    );
    strategy_engine->register_strategy(stat_arb);
    
    // Set signal callback
    strategy_engine->set_signal_callback(on_signal);
    
    // Create execution engine
    auto execution_engine = std::make_shared<ExecutionEngine>(market_data);
    
    // Set execution report callback
    execution_engine->set_execution_callback(on_execution_report);
    
    // Start engines
    strategy_engine->start();
    execution_engine->start();
    
    LOG_INFO("Engines started, beginning simulation");
    
    // Simulation loop
    Timekeeper timer;
    size_t message_count = 0;
    const size_t messages_per_batch = 1000;
    
    while (g_running) {
        // Generate some random market data
        auto data = generate_market_data(symbols, messages_per_batch);
        
                // Fix for the unused variable warning
        timer.start();
        market_data->process_buffer(data.data(), data.size());
        timer.end();
        
        message_count += messages_per_batch;
        
        // Log statistics
        if (message_count % 10000 == 0) {
            LOG_INFO("Processed " + std::to_string(message_count) + " messages, " +
                     "last batch in " + std::to_string(timer.average() / 1000.0) + " μs, " +
                     "avg latency: " + std::to_string(timer.average()) + " ns");
            
            // Print order book snapshots
            for (const auto& symbol : symbols) {
                auto order_book = market_data->get_order_book(symbol);
                if (order_book) {
                    auto best_bid = order_book->best_bid();
                    auto best_ask = order_book->best_ask();
                    auto [bid_depth, ask_depth] = order_book->depth();
                    
                    LOG_INFO(symbol + " order book: " +
                             "bid=" + (best_bid ? std::to_string(*best_bid) : "N/A") + " (" + std::to_string(bid_depth) + "), " +
                             "ask=" + (best_ask ? std::to_string(*best_ask) : "N/A") + " (" + std::to_string(ask_depth) + "), " +
                             "spread=" + (order_book->spread() ? std::to_string(*order_book->spread()) : "N/A"));
                }
            }
        }
        
        // Throttle simulation speed
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    LOG_INFO("Simulation complete, processed " + std::to_string(message_count) + " messages");
    LOG_INFO("Average processing latency: " + std::to_string(timer.average()) + " ns");
    LOG_INFO("99th percentile latency: " + std::to_string(timer.percentile(0.99)) + " ns");
    
    // Stop engines
    execution_engine->stop();
    strategy_engine->stop();
    
    // Stop logger
    Logger::instance().stop();
    
    return 0;
}