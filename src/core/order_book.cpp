#include "trading/core/order_book.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace trading {

OrderBook::OrderBook(std::string_view symbol, uint32_t price_levels)
    : symbol_(symbol), best_bid_(std::nullopt), best_ask_(std::nullopt) {
    // Pre-allocate space for price levels
    bid_levels_.resize(price_levels);
    ask_levels_.resize(price_levels);
}

void OrderBook::add_order(const Order& order) {
    // Store the order
    orders_[order.id] = order;
    
    // Update the price level
    auto index = price_to_index(order.price, order.side);
    if (order.side == Side::BUY) {
        bid_levels_[index].price = order.price;
        bid_levels_[index].quantity += order.quantity;
    } else {
        ask_levels_[index].price = order.price;
        ask_levels_[index].quantity += order.quantity;
    }
    
    // Update best prices
    update_best_prices();
}

bool OrderBook::modify_order(OrderId order_id, Quantity new_quantity) {
    // Find the order
    auto it = orders_.find(order_id);
    if (it == orders_.end()) {
        return false;
    }
    
    Order& order = it->second;
    auto index = price_to_index(order.price, order.side);
    
    // Update the price level
    if (order.side == Side::BUY) {
        bid_levels_[index].quantity -= order.quantity;
        bid_levels_[index].quantity += new_quantity;
    } else {
        ask_levels_[index].quantity -= order.quantity;
        ask_levels_[index].quantity += new_quantity;
    }
    
    // Update the order
    order.quantity = new_quantity;
    
    // Update best prices if needed
    update_best_prices();
    return true;
}

bool OrderBook::cancel_order(OrderId order_id) {
    // Find the order
    auto it = orders_.find(order_id);
    if (it == orders_.end()) {
        return false;
    }
    
    Order& order = it->second;
    auto index = price_to_index(order.price, order.side);
    
    // Update the price level
    if (order.side == Side::BUY) {
        bid_levels_[index].quantity -= order.quantity;
    } else {
        ask_levels_[index].quantity -= order.quantity;
    }
    
    // Remove the order
    orders_.erase(it);
    
    // Update best prices if needed
    update_best_prices();
    return true;
}

bool OrderBook::execute_order(OrderId order_id, Quantity exec_quantity) {
    // Find the order
    auto it = orders_.find(order_id);
    if (it == orders_.end()) {
        return false;
    }
    
    Order& order = it->second;
    if (order.quantity < exec_quantity) {
        return false;
    }
    
    auto index = price_to_index(order.price, order.side);
    
    // Update the price level
    if (order.side == Side::BUY) {
        bid_levels_[index].quantity -= exec_quantity;
    } else {
        ask_levels_[index].quantity -= exec_quantity;
    }
    
    // Update the order
    order.quantity -= exec_quantity;
    
    // If fully executed, remove the order
    if (order.quantity == 0) {
        orders_.erase(it);
    }
    
    // Update best prices if needed
    update_best_prices();
    return true;
}

std::optional<Price> OrderBook::best_bid() const {
    return best_bid_;
}

std::optional<Price> OrderBook::best_ask() const {
    return best_ask_;
}

std::pair<size_t, size_t> OrderBook::depth() const {
    size_t bid_depth = 0;
    size_t ask_depth = 0;
    
    for (const auto& level : bid_levels_) {
        if (level.quantity > 0) {
            bid_depth++;
        }
    }
    
    for (const auto& level : ask_levels_) {
        if (level.quantity > 0) {
            ask_depth++;
        }
    }
    
    return {bid_depth, ask_depth};
}

std::optional<Price> OrderBook::spread() const {
    if (best_bid_ && best_ask_) {
        return *best_ask_ - *best_bid_;
    }
    return std::nullopt;
}

std::optional<Price> OrderBook::mid_price() const {
    if (best_bid_ && best_ask_) {
        return (*best_bid_ + *best_ask_) / 2;
    }
    return std::nullopt;
}

std::vector<OrderBookLevel> OrderBook::get_levels(Side side, size_t depth) const {
    std::vector<OrderBookLevel> result;
    result.reserve(depth);
    
    const auto& levels = (side == Side::BUY) ? bid_levels_ : ask_levels_;
    
    // For bids, we want to start from the highest price (best bid)
    // For asks, we want to start from the lowest price (best ask)
    if (side == Side::BUY) {
        // Find non-empty levels and sort by price descending
        for (const auto& level : levels) {
            if (level.quantity > 0) {
                result.push_back(level);
            }
        }
        std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
            return a.price > b.price;
        });
    } else {
        // Find non-empty levels and sort by price ascending
        for (const auto& level : levels) {
            if (level.quantity > 0) {
                result.push_back(level);
            }
        }
        std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
            return a.price < b.price;
        });
    }
    
    // Limit to requested depth
    if (result.size() > depth) {
        result.resize(depth);
    }
    
    return result;
}

// Fix for the unused parameter warning in price_to_index
size_t OrderBook::price_to_index(Price price, Side /*side*/) const {
    // Simple hashing function for price to index mapping
    // This is a very basic implementation for demonstration
    // A more sophisticated approach would be needed for production
    return static_cast<size_t>(price) % bid_levels_.size();
}

void OrderBook::update_best_prices() {
    best_bid_ = std::nullopt;
    best_ask_ = std::nullopt;
    
    // Find best bid (highest price)
    for (const auto& level : bid_levels_) {
        if (level.quantity > 0) {
            if (!best_bid_ || level.price > *best_bid_) {
                best_bid_ = level.price;
            }
        }
    }
    
    // Find best ask (lowest price)
    for (const auto& level : ask_levels_) {
        if (level.quantity > 0) {
            if (!best_ask_ || level.price < *best_ask_) {
                best_ask_ = level.price;
            }
        }
    }
}

} // namespace trading