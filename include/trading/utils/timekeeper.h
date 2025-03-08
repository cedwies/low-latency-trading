#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace trading {

// High precision timekeeper for latency measurements
class Timekeeper {
public:
    // Constructor
    Timekeeper(size_t max_samples = 1000000);
    
    // Start timing
    void start();
    
    // End timing and record sample
    uint64_t end();
    
    // Get average latency
    double average() const;
    
    // Get median latency
    double median();
    
    // Get percentile latency
    double percentile(double p);
    
    // Get minimum latency
    uint64_t min() const;
    
    // Get maximum latency
    uint64_t max() const;
    
    // Clear samples
    void clear();
    
    // Get number of samples
    size_t count() const;
    
    // Get all samples
    const std::vector<uint64_t>& samples() const;
    
    // Get histogram data
    std::vector<std::pair<uint64_t, uint64_t>> histogram(size_t bins = 20) const;
    
    // Get summary statistics as string
    std::string summary() const;
    
private:
    // Vector of latency samples in nanoseconds
    std::vector<uint64_t> samples_;
    
    // Start time
    std::chrono::high_resolution_clock::time_point start_time_;
    
    // Maximum number of samples to keep
    size_t max_samples_;
    
    // Sort samples for percentile calculations
    void sort_samples();
    
    // Flag indicating if samples are sorted
    bool sorted_;
};

// CPU cycle counter for ultra-precise timing
class CycleCounter {
public:
    // Start counting CPU cycles
    static inline uint64_t start() {
        #if defined(__x86_64__) || defined(_M_X64)
            uint32_t low, high;
            asm volatile("rdtsc" : "=a" (low), "=d" (high));
            return static_cast<uint64_t>(high) << 32 | low;
        #else
            return std::chrono::high_resolution_clock::now().time_since_epoch().count();
        #endif
    }
    
    // End counting CPU cycles
    static inline uint64_t end() {
        #if defined(__x86_64__) || defined(_M_X64)
            uint32_t low, high;
            asm volatile("rdtscp" : "=a" (low), "=d" (high) :: "rcx");
            return static_cast<uint64_t>(high) << 32 | low;
        #else
            return std::chrono::high_resolution_clock::now().time_since_epoch().count();
        #endif
    }
    
    // Get CPU frequency in GHz
    static double cpu_frequency_ghz();
    
    // Convert CPU cycles to nanoseconds
    static double cycles_to_ns(uint64_t cycles);
    
    // Convert nanoseconds to CPU cycles
    static uint64_t ns_to_cycles(double ns);
};

} // namespace trading