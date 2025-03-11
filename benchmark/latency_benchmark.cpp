#include "trading/core/execution_engine.h"
#include "trading/core/market_data.h"
#include "trading/core/order_book.h"
#include "trading/core/strategy_engine.h"
#include "trading/support/config.h"
#include "trading/support/logger.h"
#include "trading/utils/timekeeper.h"
#include "trading/utils/lockfree_queue.h"
#include "trading/utils/memory_pool.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <random>
#include <string>
#include <thread>
#include <vector>

using namespace trading;

// Benchmark parameters
const size_t NUM_ITERATIONS = 1000000;
const size_t NUM_WARMUP = 100000;
const size_t CACHE_LINE_SIZE = 64;

// Align to cache line
alignas(CACHE_LINE_SIZE) uint64_t g_dummy = 0;

// Measure function execution time
template<typename Func>
uint64_t measure_time_ns(Func&& func) {
    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
}

// Measure function execution time with CPU cycles
template<typename Func>
uint64_t measure_cycles(Func&& func) {
    uint64_t start = CycleCounter::start();
    func();
    uint64_t end = CycleCounter::end();
    return end - start;
}

// Print benchmark results
void print_results(const std::string& name, const std::vector<uint64_t>& times) {
    // Skip warmup iterations
    std::vector<uint64_t> filtered_times(times.begin() + NUM_WARMUP, times.end());
    
    if (filtered_times.empty()) {
        std::cout << "No data for " << name << std::endl;
        return;
    }
    
    // Calculate statistics
    std::sort(filtered_times.begin(), filtered_times.end());
    
    double mean = std::accumulate(filtered_times.begin(), filtered_times.end(), 0.0) / filtered_times.size();
    
    size_t mid = filtered_times.size() / 2;
    double median = (filtered_times.size() % 2 == 0)
        ? (filtered_times[mid - 1] + filtered_times[mid]) / 2.0
        : filtered_times[mid];
    
    uint64_t min = filtered_times.front();
    uint64_t max = filtered_times.back();
    
    size_t p90_idx = filtered_times.size() * 90 / 100;
    size_t p99_idx = filtered_times.size() * 99 / 100;
    size_t p999_idx = filtered_times.size() * 999 / 1000;
    
    uint64_t p90 = filtered_times[p90_idx];
    uint64_t p99 = filtered_times[p99_idx];
    uint64_t p999 = filtered_times[p999_idx];
    
    // Print results
    std::cout << "Benchmark: " << name << std::endl;
    std::cout << "  Iterations: " << filtered_times.size() << std::endl;
    std::cout << "  Min:      " << std::setw(10) << min << " ns" << std::endl;
    std::cout << "  Max:      " << std::setw(10) << max << " ns" << std::endl;
    std::cout << "  Mean:     " << std::setw(10) << std::fixed << std::setprecision(2) << mean << " ns" << std::endl;
    std::cout << "  Median:   " << std::setw(10) << std::fixed << std::setprecision(2) << median << " ns" << std::endl;
    std::cout << "  90th:     " << std::setw(10) << p90 << " ns" << std::endl;
    std::cout << "  99th:     " << std::setw(10) << p99 << " ns" << std::endl;
    std::cout << "  99.9th:   " << std::setw(10) << p999 << " ns" << std::endl;
    std::cout << std::endl;
}

// Benchmark OrderBook operations
void benchmark_order_book() {
    std::cout << "Benchmarking OrderBook..." << std::endl;
    
    // Create order book
    OrderBook order_book("AAPL");
    
    // Create random orders
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> price_dist(9000, 11000);  // Price between 90.00 and 110.00
    std::uniform_int_distribution<> quantity_dist(1, 100);
    std::uniform_int_distribution<> side_dist(0, 1);  // 0 = Buy, 1 = Sell
    
    std::vector<Order> orders;
    orders.reserve(NUM_ITERATIONS);
    
    for (size_t i = 0; i < NUM_ITERATIONS; ++i) {
        orders.emplace_back(
            i + 1,  // order_id
            price_dist(gen),  // price
            quantity_dist(gen),  // quantity
            side_dist(gen) == 0 ? Side::BUY : Side::SELL,  // side
            std::chrono::system_clock::now().time_since_epoch().count(),  // timestamp
            "AAPL"  // symbol
        );
    }
    
    // Benchmark add_order
    {
        std::vector<uint64_t> times;
        times.reserve(NUM_ITERATIONS);
        
        for (size_t i = 0; i < NUM_ITERATIONS; ++i) {
            // Measure time
            uint64_t time = measure_time_ns([&]() {
                order_book.add_order(orders[i]);
            });
            
            times.push_back(time);
        }
        
        print_results("OrderBook::add_order", times);
    }
    
    // Benchmark best_bid/best_ask
    {
        std::vector<uint64_t> times;
        times.reserve(NUM_ITERATIONS);
        
        for (size_t i = 0; i < NUM_ITERATIONS; ++i) {
            // Measure time
            uint64_t time = measure_time_ns([&]() {
                auto bid = order_book.best_bid();
                auto ask = order_book.best_ask();
                g_dummy = bid.value_or(0) + ask.value_or(0);  // Prevent optimization
            });
            
            times.push_back(time);
        }
        
        print_results("OrderBook::best_bid/best_ask", times);
    }
    
    // Benchmark cancel_order
    {
        std::vector<uint64_t> times;
        times.reserve(NUM_ITERATIONS);
        
        for (size_t i = 0; i < NUM_ITERATIONS; ++i) {
            // Measure time
            uint64_t time = measure_time_ns([&]() {
                order_book.cancel_order(i + 1);  // order_id
            });
            
            times.push_back(time);
        }
        
        print_results("OrderBook::cancel_order", times);
    }
}

// Benchmark LockFreeQueue operations
void benchmark_lockfree_queue() {
    std::cout << "Benchmarking LockFreeQueue..." << std::endl;
    
    // Create queue
    LockFreeQueue<uint64_t> queue;
    
    // Benchmark try_push
    {
        std::vector<uint64_t> times;
        times.reserve(NUM_ITERATIONS);
        
        for (size_t i = 0; i < NUM_ITERATIONS; ++i) {
            // Measure time
            uint64_t time = measure_time_ns([&]() {
                queue.try_push(i);
            });
            
            times.push_back(time);
        }
        
        print_results("LockFreeQueue::try_push", times);
    }
    
    // Benchmark try_pop
    {
        std::vector<uint64_t> times;
        times.reserve(NUM_ITERATIONS);
        
        for (size_t i = 0; i < NUM_ITERATIONS; ++i) {
            // Measure time
            uint64_t time = measure_time_ns([&]() {
                auto value = queue.try_pop();
                g_dummy = value.value_or(0);  // Prevent optimization
            });
            
            times.push_back(time);
        }
        
        print_results("LockFreeQueue::try_pop", times);
    }
}

// Benchmark MemoryPool operations
void benchmark_memory_pool() {
    std::cout << "Benchmarking MemoryPool..." << std::endl;
    
    // Create memory pool
    MemoryPool<int> pool;  // Use int instead of ExecutionReport
    
    // Use a much smaller number of iterations
    const size_t POOL_ITERATIONS = 1000;
    
    // Create a fixed-size array instead of a vector
    int* objects[POOL_ITERATIONS] = {nullptr};
    
    // Static array for timing results
    uint64_t create_times[POOL_ITERATIONS] = {0};
    uint64_t destroy_times[POOL_ITERATIONS] = {0};
    
    // Benchmark create
    {
        size_t success_count = 0;
        
        for (size_t i = 0; i < POOL_ITERATIONS; ++i) {
            // Measure time
            uint64_t start = std::chrono::high_resolution_clock::now().time_since_epoch().count();
            objects[i] = pool.create();
            uint64_t end = std::chrono::high_resolution_clock::now().time_since_epoch().count();
            
            // Store timing
            create_times[i] = end - start;
            
            // Count successful creations
            if (objects[i]) {
                success_count++;
            }
        }
        
        std::cout << "Successfully created " << success_count << " objects" << std::endl;
        
        // Calculate statistics manually
        std::cout << "MemoryPool::create average time: ";
        uint64_t sum = 0;
        for (size_t i = 0; i < POOL_ITERATIONS; ++i) {
            sum += create_times[i];
        }
        std::cout << (sum / POOL_ITERATIONS) << " ns" << std::endl;
    }
    
    // Benchmark destroy
    {
        size_t success_count = 0;
        
        for (size_t i = 0; i < POOL_ITERATIONS; ++i) {
            if (!objects[i]) continue;
            
            // Measure time
            uint64_t start = std::chrono::high_resolution_clock::now().time_since_epoch().count();
            pool.destroy(objects[i]);
            uint64_t end = std::chrono::high_resolution_clock::now().time_since_epoch().count();
            
            // Store timing
            destroy_times[i] = end - start;
            
            // Mark as destroyed
            objects[i] = nullptr;
            success_count++;
        }
        
        std::cout << "Successfully destroyed " << success_count << " objects" << std::endl;
        
        // Calculate statistics manually
        std::cout << "MemoryPool::destroy average time: ";
        uint64_t sum = 0;
        size_t count = 0;
        for (size_t i = 0; i < POOL_ITERATIONS; ++i) {
            if (destroy_times[i] > 0) {
                sum += destroy_times[i];
                count++;
            }
        }
        if (count > 0) {
            std::cout << (sum / count) << " ns" << std::endl;
        } else {
            std::cout << "N/A (no objects destroyed)" << std::endl;
        }
    }
    
    // Double-check cleanup
    for (size_t i = 0; i < POOL_ITERATIONS; ++i) {
        if (objects[i]) {
            pool.destroy(objects[i]);
            objects[i] = nullptr;
        }
    }
    
    std::cout << "Memory pool benchmark completed" << std::endl;
}

// Benchmark CycleCounter
void benchmark_cycle_counter() {
    std::cout << "Benchmarking CycleCounter..." << std::endl;
    
    // Benchmark start/end
    {
        std::vector<uint64_t> times;
        times.reserve(NUM_ITERATIONS);
        
        for (size_t i = 0; i < NUM_ITERATIONS; ++i) {
            // Measure time
            uint64_t time = measure_time_ns([&]() {
                uint64_t start = CycleCounter::start();
                uint64_t end = CycleCounter::end();
                g_dummy = end - start;  // Prevent optimization (!!)
            });
            
            times.push_back(time);
        }
        
        print_results("CycleCounter::start/end", times);
    }
}

int main() {
    // Initialize logger
    Logger::instance().initialize("benchmark.log", LogLevel::INFO);
    Logger::instance().start();
    
    LOG_INFO("Starting benchmarks");
    
    // Configure priority and affinity for more consistent results
    #ifdef _WIN32
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
    #else
    // On Unix one could use sched_setaffinity or nice
    #endif
    
    // Run benchmarks
    benchmark_order_book();
    benchmark_lockfree_queue();
    benchmark_memory_pool();
    benchmark_cycle_counter();
    
    LOG_INFO("Benchmarks complete");
    
    // Stop logger
    Logger::instance().stop();
    
    return 0;
}