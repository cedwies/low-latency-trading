#include "trading/core/market_data.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace trading {

//
// RingBuffer Implementation
//

RingBuffer::RingBuffer(size_t capacity)
    : capacity_(capacity), read_pos_(0), write_pos_(0) {
    // Allocate memory for buffer
    buffer_ = std::make_unique<uint8_t[]>(capacity);
}

RingBuffer::~RingBuffer() = default;

size_t RingBuffer::write(const uint8_t* data, size_t length) {
    if (length > write_available()) {
        length = write_available();
    }
    
    if (length == 0) {
        return 0;
    }
    
    // Handle wrap-around
    size_t first_chunk = std::min(length, capacity_ - write_pos_);
    std::memcpy(buffer_.get() + write_pos_, data, first_chunk);
    
    if (first_chunk < length) {
        // Write remaining data at the beginning of the buffer
        std::memcpy(buffer_.get(), data + first_chunk, length - first_chunk);
    }
    
    write_pos_ = advance(write_pos_, length);
    return length;
}

size_t RingBuffer::read(uint8_t* data, size_t length) {
    if (length > read_available()) {
        length = read_available();
    }
    
    if (length == 0) {
        return 0;
    }
    
    // Handle wrap-around
    size_t first_chunk = std::min(length, capacity_ - read_pos_);
    std::memcpy(data, buffer_.get() + read_pos_, first_chunk);
    
    if (first_chunk < length) {
        // Read remaining data from the beginning of the buffer
        std::memcpy(data + first_chunk, buffer_.get(), length - first_chunk);
    }
    
    read_pos_ = advance(read_pos_, length);
    return length;
}

size_t RingBuffer::write_available() const {
    if (read_pos_ <= write_pos_) {
        return capacity_ - (write_pos_ - read_pos_) - 1;
    } else {
        return read_pos_ - write_pos_ - 1;
    }
}

size_t RingBuffer::read_available() const {
    if (read_pos_ <= write_pos_) {
        return write_pos_ - read_pos_;
    } else {
        return capacity_ - (read_pos_ - write_pos_);
    }
}

void RingBuffer::reset() {
    read_pos_ = 0;
    write_pos_ = 0;
}

size_t RingBuffer::advance(size_t pos, size_t length) const {
    return (pos + length) % capacity_;
}

//
// MarketDataHandler Implementation
//

MarketDataHandler::MarketDataHandler(size_t buffer_size)
    : buffer_(std::make_unique<RingBuffer>(buffer_size)) {
}

MarketDataHandler::~MarketDataHandler() = default;

size_t MarketDataHandler::process_buffer(const uint8_t* data, size_t length) {
    size_t offset = 0;
    size_t processed = 0;
    
    while (offset < length) {
        // Parse a single message
        auto [msg, symbol] = parse_message(data, offset, length);
        
        if (!msg) {
            // Not enough data for a complete message
            break;
        }
        
        // Process callbacks for this symbol
        auto it = callbacks_.find(std::string(symbol));
        if (it != callbacks_.end()) {
            for (const auto& callback : it->second) {
                callback(*msg, symbol);
            }
        }
        
        // Update order books
        update_order_books(*msg, symbol);
        
        processed += offset;
    }
    
    return processed;
}

void MarketDataHandler::subscribe(std::string_view symbol, MarketDataCallback callback) {
    // Convert symbol to string for map
    std::string sym_str(symbol);
    
    // Add callback to the list for this symbol
    callbacks_[sym_str].push_back(std::move(callback));
    
    // Create order book for this symbol if it doesn't exist
    if (order_books_.find(sym_str) == order_books_.end()) {
        order_books_[sym_str] = std::make_shared<OrderBook>(symbol);
    }
}

void MarketDataHandler::unsubscribe(std::string_view symbol) {
    // Convert symbol to string for map
    std::string sym_str(symbol);
    
    // Remove all callbacks for this symbol
    callbacks_.erase(sym_str);
}

void MarketDataHandler::update_order_books(const MarketDataMessage& msg, std::string_view symbol) {
    // Convert symbol to string for map
    std::string sym_str(symbol);
    
    // Get order book for this symbol
    auto it = order_books_.find(sym_str);
    if (it == order_books_.end()) {
        // No order book for this symbol
        return;
    }
    
    auto& order_book = it->second;
    
    // Process message based on type
    switch (msg.type) {
        case MessageType::ADD_ORDER: {
            Order order(
                msg.add_order.order_id,
                msg.add_order.price,
                msg.add_order.quantity,
                msg.add_order.side == 0 ? Side::BUY : Side::SELL,
                msg.timestamp,
                symbol
            );
            order_book->add_order(order);
            break;
        }
        
        case MessageType::MODIFY_ORDER:
            order_book->modify_order(msg.modify_order.order_id, msg.modify_order.quantity);
            break;
        
        case MessageType::CANCEL_ORDER:
            order_book->cancel_order(msg.cancel_order.order_id);
            break;
        
        case MessageType::EXECUTE_ORDER:
            order_book->execute_order(msg.execute_order.order_id, msg.execute_order.exec_quantity);
            break;
        
        // Other message types are not directly relevant for order book updates
        default:
            break;
    }
}

std::shared_ptr<OrderBook> MarketDataHandler::get_order_book(std::string_view symbol) {
    // Convert symbol to string for map
    std::string sym_str(symbol);
    
    // Get order book for this symbol
    auto it = order_books_.find(sym_str);
    if (it == order_books_.end()) {
        // No order book for this symbol
        return nullptr;
    }
    
    return it->second;
}

std::pair<const MarketDataMessage*, std::string_view> MarketDataHandler::parse_message(const uint8_t* data, size_t& offset, size_t max_length) {
    // Check if we have enough data for the fixed part of the header
    if (offset + sizeof(MarketDataMessage) > max_length) {
        return {nullptr, ""};
    }
    
    // Parse the fixed part of the header
    const MarketDataMessage* msg = reinterpret_cast<const MarketDataMessage*>(data + offset);
    
    // Check if we have enough data for the symbol
    size_t total_size = sizeof(MarketDataMessage) + msg->symbol_length;
    if (offset + total_size > max_length) {
        return {nullptr, ""};
    }
    
    // Get the symbol
    std::string_view symbol(reinterpret_cast<const char*>(data + offset + sizeof(MarketDataMessage)), msg->symbol_length);
    
    // Advance offset
    offset += total_size;
    
    return {msg, symbol};
}

} // namespace trading