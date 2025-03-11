#pragma once

#include <atomic>
#include <cassert>
#include <cstdint>
#include <memory>
#include <optional>
#include <type_traits>

namespace trading {

// A lock-free single-producer, single-consumer queue
template<typename T, size_t Capacity = 1024>
class LockFreeQueue {
private:
    // Queue data structure
    struct alignas(64) {  // Cache line alignment
        // Buffer to hold elements
        std::aligned_storage_t<sizeof(T), alignof(T)> buffer[Capacity];
        
        // Read and write positions
        alignas(64) std::atomic<size_t> read_pos{0};  // Cache line alignment
        alignas(64) std::atomic<size_t> write_pos{0};  // Cache line alignment
    } queue_;
    
    // Helper to get a reference to an element in the buffer
    T& at(size_t pos) {
        return *reinterpret_cast<T*>(&queue_.buffer[pos % Capacity]);
    }
    
    // Helper to get a const reference to an element in the buffer
    const T& at(size_t pos) const {
        return *reinterpret_cast<const T*>(&queue_.buffer[pos % Capacity]);
    }
    
public:
    // Default constructor
    LockFreeQueue() = default;
    
    // Destructor - destroy any remaining elements
    ~LockFreeQueue() {
        size_t read_pos = queue_.read_pos.load(std::memory_order_relaxed);
        size_t write_pos = queue_.write_pos.load(std::memory_order_relaxed);
        
        for (size_t i = read_pos; i < write_pos; ++i) {
            at(i).~T();
        }
    }
    
    // Try to push an element to the queue
    // Returns true if successful, false if the queue is full
    bool try_push(const T& value) {
        size_t write_pos = queue_.write_pos.load(std::memory_order_relaxed);
        size_t read_pos = queue_.read_pos.load(std::memory_order_acquire);
        
        // Check if the queue is full
        if (write_pos - read_pos >= Capacity) {
            return false;
        }
        
        // Construct the element in place
        new (&queue_.buffer[write_pos % Capacity]) T(value);
        
        // Update the write position
        queue_.write_pos.store(write_pos + 1, std::memory_order_release);
        
        return true;
    }
    
    // Try to push an element to the queue (move semantics)
    // Returns true if successful, false if the queue is full
    bool try_push(T&& value) {
        size_t write_pos = queue_.write_pos.load(std::memory_order_relaxed);
        size_t read_pos = queue_.read_pos.load(std::memory_order_acquire);
        
        // Check if the queue is full
        if (write_pos - read_pos >= Capacity) {
            return false;
        }
        
        // Construct the element in place
        new (&queue_.buffer[write_pos % Capacity]) T(std::move(value));
        
        // Update the write position
        queue_.write_pos.store(write_pos + 1, std::memory_order_release);
        
        return true;
    }
    
    // Try to pop an element from the queue
    // Returns the element if successful, nullopt if the queue is empty
    // Edit: I don't know yet if std::optional is the best choice regarding performance
    std::optional<T> try_pop() {
        size_t read_pos = queue_.read_pos.load(std::memory_order_relaxed);
        size_t write_pos = queue_.write_pos.load(std::memory_order_acquire);
        
        // Check if the queue is empty
        if (read_pos >= write_pos) {
            return std::nullopt;
        }
        
        // Get the element
        T value = std::move(at(read_pos));
        
        // Destroy the element
        at(read_pos).~T();
        
        // Update the read position
        queue_.read_pos.store(read_pos + 1, std::memory_order_release);
        
        return value;
    }
    
    // Get the number of elements in the queue
    size_t size() const {
        size_t write_pos = queue_.write_pos.load(std::memory_order_acquire);
        size_t read_pos = queue_.read_pos.load(std::memory_order_acquire);
        return write_pos - read_pos;
    }
    
    // Check if the queue is empty
    bool empty() const {
        return size() == 0;
    }
    
    // Check if the queue is full
    bool full() const {
        return size() >= Capacity;
    }
    
    // Clear the queue
    void clear() {
        T value;
        while (try_pop()) {
            // Just pop and discard
        }
    }
    
    // Get the capacity of the queue
    size_t capacity() const {
        return Capacity;
    }
};

} // namespace trading