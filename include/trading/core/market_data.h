#pragma once

#include "trading/core/order_book.h"
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace trading {

// Forward declarations
class RingBuffer;

// Market data message types
enum class MessageType : uint8_t {
    ADD_ORDER = 1,
    MODIFY_ORDER = 2,
    CANCEL_ORDER = 3,
    EXECUTE_ORDER = 4,
    TRADE = 5,
    SNAPSHOT = 6,
    HEARTBEAT = 7
};

// Market data message structure (compact binary format)
#pragma pack(push, 1)
// Define struct types outside the union
struct AddOrderData {
    OrderId order_id;
    Price price;
    Quantity quantity;
    uint8_t side;    // 0 = Buy, 1 = Sell
};

struct ModifyOrderData {
    OrderId order_id;
    Quantity quantity;
};

struct CancelOrderData {
    OrderId order_id;
};

struct ExecuteOrderData {
    OrderId order_id;
    Quantity exec_quantity;
    Price exec_price;
};

struct TradeData {
    Price price;
    Quantity quantity;
    uint8_t aggressor_side; // 0 = Buy, 1 = Sell
};

// Market data message structure (compact binary format)
struct MarketDataMessage {
    // Common header
    uint64_t timestamp;      // Nanoseconds since epoch
    MessageType type;        // Message type
    uint8_t symbol_length;   // Length of symbol
    
    // Payload (varies by message type)
    union {
        AddOrderData add_order;
        ModifyOrderData modify_order;
        CancelOrderData cancel_order;
        ExecuteOrderData execute_order;
        TradeData trade;
        // Heartbeat has no additional fields
    };
    
    // Symbol follows the fixed portion (variable length)
    // char symbol[symbol_length];
};
#pragma pack(pop)

// Callback type for market data events
using MarketDataCallback = std::function<void(const MarketDataMessage&, std::string_view)>;

// Market data handler class
class MarketDataHandler {
public:
    // Constructor
    MarketDataHandler(size_t buffer_size = 1024 * 1024);
    
    // Destructor
    ~MarketDataHandler();
    
    // Process a raw buffer of market data
    size_t process_buffer(const uint8_t* data, size_t length);
    
    // Subscribe to market data for a specific symbol
    void subscribe(std::string_view symbol, MarketDataCallback callback);
    
    // Unsubscribe from market data for a specific symbol
    void unsubscribe(std::string_view symbol);
    
    // Update order books based on market data
    void update_order_books(const MarketDataMessage& msg, std::string_view symbol);
    
    // Get order book for a specific symbol
    std::shared_ptr<OrderBook> get_order_book(std::string_view symbol);
    
private:
    // Parse a single market data message
    std::pair<const MarketDataMessage*, std::string_view> parse_message(const uint8_t* data, size_t& offset, size_t max_length);
    
    // Ring buffer for zero-copy message processing
    std::unique_ptr<RingBuffer> buffer_;
    
    // Map of symbol to callbacks
    std::unordered_map<std::string, std::vector<MarketDataCallback>> callbacks_;
    
    // Map of symbol to order book
    std::unordered_map<std::string, std::shared_ptr<OrderBook>> order_books_;
};

// Ring buffer implementation for zero-copy data processing
class RingBuffer {
public:
    // Constructor
    explicit RingBuffer(size_t capacity);
    
    // Destructor
    ~RingBuffer();
    
    // Write data to the buffer
    size_t write(const uint8_t* data, size_t length);
    
    // Read data from the buffer
    size_t read(uint8_t* data, size_t length);
    
    // Get available space for writing
    size_t write_available() const;
    
    // Get available data for reading
    size_t read_available() const;
    
    // Reset buffer
    void reset();
    
    // Get buffer capacity
    size_t capacity() const { return capacity_; }
    
private:
    // Buffer memory
    std::unique_ptr<uint8_t[]> buffer_;
    
    // Buffer capacity
    size_t capacity_;
    
    // Read position
    size_t read_pos_;
    
    // Write position
    size_t write_pos_;
    
    // Return the next position after advancing by length
    size_t advance(size_t pos, size_t length) const;
};

} // namespace trading