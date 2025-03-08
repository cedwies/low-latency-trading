#pragma once

#include "trading/utils/lockfree_queue.h"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace trading {

// Log levels
enum class LogLevel : uint8_t {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARNING = 3,
    ERROR = 4,
    FATAL = 5
};

// Logger class
class Logger {
public:
    // Get the singleton instance
    static Logger& instance();
    
    // Initialize the logger
    void initialize(std::string_view log_file, LogLevel min_level = LogLevel::INFO);
    
    // Set minimum log level
    void set_min_level(LogLevel level);
    
    // Log a message
    void log(LogLevel level, std::string_view message);
    
    // Start the logger thread
    void start();
    
    // Stop the logger thread
    void stop();
    
    // Flush the log buffer
    void flush();
    
    // Log a message with formatted arguments
    template<typename... Args>
    void log_fmt(LogLevel level, std::string_view format, Args&&... args);
    
    // Utility functions for different log levels
    void trace(std::string_view message);
    void debug(std::string_view message);
    void info(std::string_view message);
    void warning(std::string_view message);
    void error(std::string_view message);
    void fatal(std::string_view message);
    
    // Is a specific log level enabled?
    bool is_enabled(LogLevel level) const;
    
private:
    // Constructor (private for singleton)
    Logger();
    
    // Destructor
    ~Logger();
    
    // Log entry structure
    struct LogEntry {
        LogLevel level;
        std::chrono::system_clock::time_point timestamp;
        std::string message;
        
        // Constructor
        LogEntry(LogLevel lvl, std::string_view msg)
            : level(lvl), 
              timestamp(std::chrono::system_clock::now()), 
              message(msg) {}
        
        // Default constructor
        LogEntry() : level(LogLevel::INFO) {}
    };
    
    // Queue of log entries
    LockFreeQueue<LogEntry, 1024> log_queue_;
    
    // Minimum log level
    std::atomic<LogLevel> min_level_;
    
    // Log file stream
    std::ofstream log_file_;
    
    // Logger thread
    std::thread logger_thread_;
    
    // Mutex for thread synchronization
    std::mutex mutex_;
    
    // Running flag
    std::atomic<bool> running_;
    
    // Logger thread function
    void logger_thread_func();
    
    // Format a log entry
    std::string format_entry(const LogEntry& entry);
    
    // Convert a log level to a string
    std::string_view level_to_string(LogLevel level) const;
};

// Macro for conditional logging
#define LOG_IF(level, condition, message) \
    do { \
        if ((condition) && Logger::instance().is_enabled(level)) { \
            Logger::instance().log(level, message); \
        } \
    } while (0)

// Macros for different log levels
#define LOG_TRACE(message) Logger::instance().trace(message)
#define LOG_DEBUG(message) Logger::instance().debug(message)
#define LOG_INFO(message) Logger::instance().info(message)
#define LOG_WARNING(message) Logger::instance().warning(message)
#define LOG_ERROR(message) Logger::instance().error(message)
#define LOG_FATAL(message) Logger::instance().fatal(message)

} // namespace trading