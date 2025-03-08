#include "trading/support/logger.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace trading {

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

Logger::Logger() : min_level_(LogLevel::INFO), running_(false) {
}

Logger::~Logger() {
    stop();
}

void Logger::initialize(std::string_view log_file, LogLevel min_level) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Close existing log file if open
    if (log_file_.is_open()) {
        log_file_.close();
    }
    
    // Open log file
    log_file_.open(std::string(log_file), std::ios::out | std::ios::app);
    if (!log_file_.is_open()) {
        std::cerr << "Failed to open log file: " << log_file << std::endl;
    }
    
    // Set minimum log level
    min_level_ = min_level;
}

void Logger::set_min_level(LogLevel level) {
    min_level_ = level;
}

void Logger::log(LogLevel level, std::string_view message) {
    if (level < min_level_) {
        return;  // Skip messages below minimum level
    }
    
    // Create log entry
    LogEntry entry(level, message);
    
    // Add to queue
    if (!log_queue_.try_push(std::move(entry))) {
        // Queue is full, print to stderr
        std::cerr << "Logger queue full, discarding message: " << message << std::endl;
    }
}

void Logger::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (running_) {
        return;  // Already running
    }
    
    running_ = true;
    
    // Start logger thread
    logger_thread_ = std::thread(&Logger::logger_thread_func, this);
}

void Logger::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!running_) {
            return;  // Already stopped
        }
        
        running_ = false;
    }
    
    // Wait for logger thread to exit
    if (logger_thread_.joinable()) {
        logger_thread_.join();
    }
    
    // Flush any remaining entries
    flush();
    
    // Close log file
    if (log_file_.is_open()) {
        log_file_.close();
    }
}

void Logger::flush() {
    // Process all entries in the queue
    LogEntry entry;
    while (auto opt_entry = log_queue_.try_pop()) {
        entry = std::move(*opt_entry);
        
        // Format and write to log file
        std::string formatted = format_entry(entry);
        
        if (log_file_.is_open()) {
            log_file_ << formatted << std::endl;
        } else {
            std::cout << formatted << std::endl;
        }
    }
    
    // Flush log file
    if (log_file_.is_open()) {
        log_file_.flush();
    }
}

void Logger::trace(std::string_view message) {
    log(LogLevel::TRACE, message);
}

void Logger::debug(std::string_view message) {
    log(LogLevel::DEBUG, message);
}

void Logger::info(std::string_view message) {
    log(LogLevel::INFO, message);
}

void Logger::warning(std::string_view message) {
    log(LogLevel::WARNING, message);
}

void Logger::error(std::string_view message) {
    log(LogLevel::ERROR, message);
}

void Logger::fatal(std::string_view message) {
    log(LogLevel::FATAL, message);
}

bool Logger::is_enabled(LogLevel level) const {
    return level >= min_level_;
}

void Logger::logger_thread_func() {
    // Process log entries
    while (running_) {
        // Try to get an entry from the queue
        auto opt_entry = log_queue_.try_pop();
        
        if (opt_entry) {
            // Format and write to log file
            std::string formatted = format_entry(*opt_entry);
            
            if (log_file_.is_open()) {
                log_file_ << formatted << std::endl;
                log_file_.flush();
            } else {
                std::cout << formatted << std::endl;
            }
        } else {
            // No entries, sleep for a bit
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

std::string Logger::format_entry(const LogEntry& entry) {
    std::ostringstream oss;
    
    // Format timestamp
    auto time = std::chrono::system_clock::to_time_t(entry.timestamp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        entry.timestamp.time_since_epoch() % std::chrono::seconds(1)
    ).count();
    
    std::tm tm_buf;
    
#ifdef _WIN32
    localtime_s(&tm_buf, &time);
#else
    localtime_r(&time, &tm_buf);
#endif
    
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S") << "." 
        << std::setfill('0') << std::setw(3) << ms << " "
        << "[" << level_to_string(entry.level) << "] "
        << entry.message;
    
    return oss.str();
}

std::string_view Logger::level_to_string(LogLevel level) const {
    switch (level) {
        case LogLevel::TRACE:   return "TRACE";
        case LogLevel::DEBUG:   return "DEBUG";
        case LogLevel::INFO:    return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR:   return "ERROR";
        case LogLevel::FATAL:   return "FATAL";
        default:                return "UNKNOWN";
    }
}

} // namespace trading