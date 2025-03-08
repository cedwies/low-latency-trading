#include "trading/utils/timekeeper.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <numeric>
#include <sstream>
#include <thread>

namespace trading {

Timekeeper::Timekeeper(size_t max_samples)
    : max_samples_(max_samples), sorted_(false) {
    samples_.reserve(max_samples_);
}

void Timekeeper::start() {
    start_time_ = std::chrono::high_resolution_clock::now();
}

uint64_t Timekeeper::end() {
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time_).count();
    
    if (samples_.size() < max_samples_) {
        samples_.push_back(static_cast<uint64_t>(duration));
        sorted_ = false;
    }
    
    return static_cast<uint64_t>(duration);
}

double Timekeeper::average() const {
    if (samples_.empty()) {
        return 0.0;
    }
    
    double sum = std::accumulate(samples_.begin(), samples_.end(), 0.0);
    return sum / samples_.size();
}

double Timekeeper::median() {
    if (samples_.empty()) {
        return 0.0;
    }
    
    sort_samples();
    
    size_t mid = samples_.size() / 2;
    if (samples_.size() % 2 == 0) {
        return (samples_[mid - 1] + samples_[mid]) / 2.0;
    } else {
        return samples_[mid];
    }
}

double Timekeeper::percentile(double p) {
    if (samples_.empty()) {
        return 0.0;
    }
    
    sort_samples();
    
    size_t idx = static_cast<size_t>(std::ceil(p * samples_.size())) - 1;
    idx = std::min(idx, samples_.size() - 1);
    
    return samples_[idx];
}

uint64_t Timekeeper::min() const {
    if (samples_.empty()) {
        return 0;
    }
    
    return *std::min_element(samples_.begin(), samples_.end());
}

uint64_t Timekeeper::max() const {
    if (samples_.empty()) {
        return 0;
    }
    
    return *std::max_element(samples_.begin(), samples_.end());
}

void Timekeeper::clear() {
    samples_.clear();
    sorted_ = true;
}

size_t Timekeeper::count() const {
    return samples_.size();
}

const std::vector<uint64_t>& Timekeeper::samples() const {
    return samples_;
}

std::vector<std::pair<uint64_t, uint64_t>> Timekeeper::histogram(size_t bins) const {
    if (samples_.empty()) {
        return {};
    }
    
    uint64_t min_val = min();
    uint64_t max_val = max();
    
    if (min_val == max_val) {
        return {{min_val, samples_.size()}};
    }
    
    std::vector<std::pair<uint64_t, uint64_t>> result(bins);
    uint64_t bin_width = (max_val - min_val) / bins + 1;
    
    for (size_t i = 0; i < bins; ++i) {
        result[i].first = min_val + i * bin_width;
        result[i].second = 0;
    }
    
    for (uint64_t sample : samples_) {
        size_t bin = std::min(static_cast<size_t>((sample - min_val) / bin_width), bins - 1);
        result[bin].second++;
    }
    
    return result;
}

std::string Timekeeper::summary() const {
    std::ostringstream oss;
    oss << "Samples: " << count() << "\n";
    if (count() > 0) {
        oss << "Min: " << min() << " ns\n";
        oss << "Max: " << max() << " ns\n";
        oss << "Avg: " << average() << " ns\n";
        
        // Make a copy for const-correctness
        Timekeeper copy(*this);
        oss << "50th: " << copy.percentile(0.5) << " ns\n";
        oss << "90th: " << copy.percentile(0.9) << " ns\n";
        oss << "99th: " << copy.percentile(0.99) << " ns\n";
        oss << "99.9th: " << copy.percentile(0.999) << " ns\n";
    }
    return oss.str();
}

void Timekeeper::sort_samples() {
    if (!sorted_) {
        std::sort(samples_.begin(), samples_.end());
        sorted_ = true;
    }
}

double CycleCounter::cpu_frequency_ghz() {
    static double freq = 0.0;
    
    if (freq == 0.0) {
        // Measure CPU frequency
        auto start = std::chrono::high_resolution_clock::now();
        uint64_t start_cycles = CycleCounter::start();
        
        // Sleep for a short time
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        uint64_t end_cycles = CycleCounter::end();
        auto end = std::chrono::high_resolution_clock::now();
        
        auto time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        uint64_t cycles = end_cycles - start_cycles;
        
        freq = static_cast<double>(cycles) / time_ns;
    }
    
    return freq;
}

double CycleCounter::cycles_to_ns(uint64_t cycles) {
    return static_cast<double>(cycles) / cpu_frequency_ghz();
}

uint64_t CycleCounter::ns_to_cycles(double ns) {
    return static_cast<uint64_t>(ns * cpu_frequency_ghz());
}

} // namespace trading