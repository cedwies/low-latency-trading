#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace trading {

// Forward declarations
struct Order;
struct OrderBookLevel;

// Price representation as fixed-point for performance (integer math vs floating point)
using Price = std::int64_t;
using OrderId = std::uint64_t;
using Quantity = std::uint32_t;
using Timestamp = std::uint64_t;

// Order side (Buy/Sell)
enum class Side : uint8_t {
    BUY = 0,
    SELL = 1
};

// Level in the order book (price level with total quantity)
struct OrderBookLevel {
    Price price;
    Quantity quantity;
    
    // Constructor
    OrderBookLevel(Price p, Quantity q) : price(p), quantity(q) {}
    
    // Default constructor for empty levels
    OrderBookLevel() : price(0), quantity(0) {}
};

// Representation of an order
struct Order {
    OrderId id;
    Price price;
    Quantity quantity;
    Quantity original_quantity;
    Side side;
    Timestamp timestamp;
    std::string symbol;
    
    // Create a new order
    Order(OrderId order_id, Price p, Quantity q, Side s, Timestamp ts, std::string_view sym)
        : id(order_id), price(p), quantity(q), original_quantity(q), side(s), timestamp(ts), symbol(std::string(sym)) {}
    
    // Default constructor
    Order() : id(0), price(0), quantity(0), original_quantity(0), side(Side::BUY), timestamp(0) {}
};

// The main OrderBook class - this maintains the state of the market
class OrderBook {
public:
    // Constructor
    explicit OrderBook(std::string_view symbol, uint32_t price_levels = 256);
    
    // Add a new order to the book
    void add_order(const Order& order);
    
    // Modify an existing order
    bool modify_order(OrderId order_id, Quantity new_quantity);
    
    // Cancel an existing order
    bool cancel_order(OrderId order_id);
    
    // Execute a trade against the book
    bool execute_order(OrderId order_id, Quantity exec_quantity);
    
    // Get best bid price
    std::optional<Price> best_bid() const;
    
    // Get best ask price
    std::optional<Price> best_ask() const;
    
    // Get order book depth (number of bids and asks)
    std::pair<size_t, size_t> depth() const;
    
    // Get the spread (difference between best ask and best bid)
    std::optional<Price> spread() const;
    
    // Get the mid price ((best bid + best ask) / 2)
    std::optional<Price> mid_price() const;
    
    // Get the current state of the order book for a specific side
    std::vector<OrderBookLevel> get_levels(Side side, size_t depth = 10) const;
    
    // Get symbol for this order book
    std::string_view symbol() const { return symbol_; }
    
private:
    // Price levels for bids (indexed by price)
    std::vector<OrderBookLevel> bid_levels_;
    
    // Price levels for asks (indexed by price)
    std::vector<OrderBookLevel> ask_levels_;
    
    // Map of order ID to Order
    std::unordered_map<OrderId, Order> orders_;
    
    // Symbol for this order book
    std::string symbol_;
    
    // Track best bid/ask for O(1) access
    std::optional<Price> best_bid_;
    std::optional<Price> best_ask_;
    
    // Convert price to index in the price array
    size_t price_to_index(Price price, Side side) const;
    
    // Update best bid/ask after a change
    void update_best_prices();
};

} // namespace trading