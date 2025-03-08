#include "trading/core/strategy_engine.h"
#include "trading/core/market_data.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace trading {

//
// StatArbitrageStrategy Implementation
//

StatArbitrageStrategy::StatArbitrageStrategy(std::vector<std::string> symbols, 
                                         double z_score_threshold, 
                                         size_t window_size)
    : symbols_(std::move(symbols)), 
      z_score_threshold_(z_score_threshold), 
      window_size_(window_size) {
}

void StatArbitrageStrategy::initialize() {
    // Initialize price history for each symbol
    for (const auto& symbol : symbols_) {
        price_history_[symbol] = std::vector<double>();
        price_history_[symbol].reserve(window_size_ * 2);  // Extra space for efficiency
    }
}

std::vector<Signal> StatArbitrageStrategy::process_update(const std::shared_ptr<OrderBook>& order_book) {
    std::vector<Signal> signals;
    
    // Get symbol from order book
    std::string symbol(order_book->symbol());
    
    // Check if we're tracking this symbol
    auto it = price_history_.find(symbol);
    if (it == price_history_.end()) {
        return signals;  // Not tracking this symbol
    }
    
    // Get mid price
    auto mid_price_opt = order_book->mid_price();
    if (!mid_price_opt) {
        return signals;  // No mid price available
    }
    
    // Convert to double and store in history
    double mid_price = static_cast<double>(*mid_price_opt);
    it->second.push_back(mid_price);
    
    // Limit history size
    if (it->second.size() > window_size_) {
        it->second.erase(it->second.begin());
    }
    
    // We need at least window_size samples to calculate signals
    if (it->second.size() < window_size_) {
        return signals;
    }
    
    // Calculate signals for pairs
    for (const auto& other_symbol : symbols_) {
        if (other_symbol == symbol) {
            continue;  // Skip self
        }
        
        // Calculate Z-score
        double z_score = calculate_z_score(symbol, other_symbol);
        
        // Generate signals based on Z-score
        if (std::abs(z_score) > z_score_threshold_) {
            SignalType signal_type = (z_score > 0) ? SignalType::SELL : SignalType::BUY;
            
            // Generate signal with market price and confidence based on Z-score
            double confidence = std::min(std::abs(z_score) / (2 * z_score_threshold_), 1.0);
            
            signals.emplace_back(
                signal_type,
                symbol,
                *mid_price_opt,
                100,  // Default quantity
                confidence,
                static_cast<Timestamp>(std::chrono::system_clock::now().time_since_epoch().count())
            );
        }
    }
    
    return signals;
}

std::string StatArbitrageStrategy::name() const {
    return "StatisticalArbitrage";
}

double StatArbitrageStrategy::calculate_z_score(const std::string& symbol1, const std::string& symbol2) {
    const auto& prices1 = price_history_[symbol1];
    const auto& prices2 = price_history_[symbol2];
    
    size_t min_size = std::min(prices1.size(), prices2.size());
    if (min_size < 2) {
        return 0.0;  // Not enough data
    }
    
    // Calculate price ratio series
    std::vector<double> ratios;
    ratios.reserve(min_size);
    
    for (size_t i = 0; i < min_size; ++i) {
        ratios.push_back(prices1[prices1.size() - min_size + i] / prices2[prices2.size() - min_size + i]);
    }
    
    // Calculate mean
    double mean = std::accumulate(ratios.begin(), ratios.end(), 0.0) / ratios.size();
    
    // Calculate standard deviation
    double sq_sum = std::inner_product(ratios.begin(), ratios.end(), ratios.begin(), 0.0,
                                      std::plus<>(), [mean](double x, double y) {
                                          return (x - mean) * (y - mean);
                                      });
    double std_dev = std::sqrt(sq_sum / ratios.size());
    
    if (std_dev == 0.0) {
        return 0.0;  // Avoid division by zero
    }
    
    // Calculate current ratio
    double current_ratio = prices1.back() / prices2.back();
    
    // Calculate Z-score
    return (current_ratio - mean) / std_dev;
}

//
// StrategyEngine Implementation
//

StrategyEngine::StrategyEngine(std::shared_ptr<MarketDataHandler> market_data)
    : market_data_(std::move(market_data)), running_(false) {
}

void StrategyEngine::start() {
    if (running_) {
        return;  // Already running
    }
    
    running_ = true;
    
    // Initialize all strategies
    for (auto& strategy : strategies_) {
        strategy->initialize();
    }
}

void StrategyEngine::stop() {
    running_ = false;
}

void StrategyEngine::register_strategy(std::shared_ptr<Strategy> strategy) {
    strategies_.push_back(std::move(strategy));
}

void StrategyEngine::set_signal_callback(std::function<void(const Signal&)> callback) {
    signal_callback_ = std::move(callback);
}

void StrategyEngine::process_order_book(const std::shared_ptr<OrderBook>& order_book) {
    if (!running_) {
        return;  // Not running
    }
    
    // Process the order book with each strategy
    for (auto& strategy : strategies_) {
        auto signals = strategy->process_update(order_book);
        
        // Emit signals
        if (signal_callback_) {
            for (const auto& signal : signals) {
                signal_callback_(signal);
            }
        }
    }
}

} // namespace trading