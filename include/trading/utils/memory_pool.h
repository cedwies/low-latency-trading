#pragma once

#include <atomic>
#include <cassert>
#include <cstdint>
#include <memory>
#include <mutex>
#include <new>
#include <utility>

namespace trading {

// A fixed-size, thread-safe memory pool
template<typename T, size_t BlockSize = 4096>
class MemoryPool {
public:
    // Constructor
    MemoryPool() : next_block_(nullptr) {
        // Allocate the first block
        allocate_block();
    }
    
    // Destructor
    ~MemoryPool() {
        // Free all blocks
        while (next_block_) {
            Block* block = next_block_;
            next_block_ = next_block_->next;
            std::free(block);
        }
    }
    
    // Allocate memory for an object
    void* allocate() {
        // Try to get a free slot
        void* result = free_list_.exchange(nullptr, std::memory_order_acquire);
        
        if (result) {
            // We got a free slot
            Slot* slot = static_cast<Slot*>(result);
            result = free_list_.exchange(slot->next, std::memory_order_release);
        } else {
            // No free slots, allocate a new block
            allocate_block();
            
            // Try again
            result = free_list_.exchange(nullptr, std::memory_order_acquire);
            
            if (result) {
                // We got a free slot
                Slot* slot = static_cast<Slot*>(result);
                result = free_list_.exchange(slot->next, std::memory_order_release);
            } else {
                // Still no free slots, fallback to standard allocation
                result = std::malloc(sizeof(T));
            }
        }
        
        return result;
    }
    
    // Deallocate memory for an object
    void deallocate(void* ptr) {
        if (!ptr) {
            return;
        }
        
        // Add the slot to the free list
        Slot* slot = static_cast<Slot*>(ptr);
        slot->next = free_list_.exchange(slot, std::memory_order_acq_rel);
    }
    
    // Create an object - compatibility method for executionengine.h version
    T* get() {
        void* ptr = allocate();
        return new(ptr) T();
    }
    
    // Destroy an object - compatibility method for executionengine.h version
    void release(T* ptr) {
        if (!ptr) {
            return;
        }
        
        // Call the destructor
        ptr->~T();
        
        // Deallocate the memory
        deallocate(ptr);
    }
    
    // Create an object with arguments (for benchmark compatibility)
    template<typename... Args>
    T* create(Args&&... args) {
        void* ptr = allocate();
        if (!ptr) return nullptr; // Add null check
        return new(ptr) T(std::forward<Args>(args)...);
    }
    
    // Destroy an object (for benchmark compatibility)
    void destroy(T* ptr) {
        release(ptr);
    }
    
private:
    // Slot in the free list
    struct Slot {
        Slot* next;
    };
    
    // Block of memory
    struct Block {
        Block* next;
        char data[BlockSize - sizeof(Block*)];
    };
    
    // Atomic pointer to the free list
    std::atomic<Slot*> free_list_{nullptr};
    
    // Pointer to the next block
    Block* next_block_;
    
    // Allocate a new block of memory
    void allocate_block() {
        // Allocate memory for the block
        Block* block = static_cast<Block*>(std::malloc(BlockSize));
        assert(block != nullptr);
        
        // Add the block to the list
        block->next = next_block_;
        next_block_ = block;
        
        // Initialize the free list
        size_t slot_size = sizeof(T) < sizeof(Slot) ? sizeof(Slot) : sizeof(T);
        size_t num_slots = (BlockSize - sizeof(Block*)) / slot_size;
        
        for (size_t i = 0; i < num_slots; ++i) {
            char* ptr = block->data + i * slot_size;
            Slot* slot = reinterpret_cast<Slot*>(ptr);
            slot->next = free_list_.load(std::memory_order_relaxed);
            free_list_.store(slot, std::memory_order_relaxed);
        }
    }
};

} // namespace trading