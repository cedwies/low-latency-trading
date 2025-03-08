#pragma once

#include <any>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace trading {

// Config value class
class ConfigValue {
public:
    // Default constructor (required for std::unordered_map)
    ConfigValue() : value_("") {}
    
    // Constructor with string value
    explicit ConfigValue(std::string_view value);
    
    // Get value as string
    std::string_view as_string() const;
    
    // Get value as integer
    int as_int() const;
    
    // Get value as unsigned integer
    unsigned int as_uint() const;
    
    // Get value as long
    long as_long() const;
    
    // Get value as double
    double as_double() const;
    
    // Get value as boolean
    bool as_bool() const;
    
    // Get value as vector of strings
    std::vector<std::string> as_string_list() const;
    
    // Get value as vector of integers
    std::vector<int> as_int_list() const;
    
    // Get value as vector of doubles
    std::vector<double> as_double_list() const;
    
private:
    // Value as string
    std::string value_;
};

// Config listener callback
using ConfigListener = std::function<void(std::string_view, const ConfigValue&)>;

// Config manager class
class ConfigManager {
public:
    // Get the singleton instance
    static ConfigManager& instance();
    
    // Load configuration from file
    bool load_file(std::string_view filename);
    
    // Load configuration from environment variables
    void load_environment();
    
    // Get a configuration value
    ConfigValue get(std::string_view key, std::string_view default_value = "") const;
    
    // Set a configuration value
    void set(std::string_view key, std::string_view value);
    
    // Check if a configuration key exists
    bool has(std::string_view key) const;
    
    // Register a listener for a specific key
    void register_listener(std::string_view key, ConfigListener listener);
    
    // Unregister all listeners for a specific key
    void unregister_listeners(std::string_view key);
    
    // Get all configuration keys
    std::vector<std::string> get_keys() const;
    
private:
    // Constructor (private for singleton)
    ConfigManager();
    
    // Map of configuration values
    std::unordered_map<std::string, ConfigValue> config_;
    
    // Map of listeners for specific keys
    std::unordered_map<std::string, std::vector<ConfigListener>> listeners_;
    
    // Trim a string
    static std::string trim(std::string_view str);
    
    // Parse a line from the configuration file
    std::pair<std::string, std::string> parse_line(std::string_view line) const;
    
    // Notify listeners of a configuration change
    void notify_listeners(std::string_view key, const ConfigValue& value);
};

} // namespace trading