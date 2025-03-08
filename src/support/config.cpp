#include "trading/support/config.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

namespace trading {

//
// ConfigValue Implementation
//

ConfigValue::ConfigValue(std::string_view value) : value_(value) {
}

std::string_view ConfigValue::as_string() const {
    return value_;
}

int ConfigValue::as_int() const {
    try {
        return std::stoi(value_);
    } catch (const std::exception& e) {
        return 0;
    }
}

unsigned int ConfigValue::as_uint() const {
    try {
        return static_cast<unsigned int>(std::stoul(value_));
    } catch (const std::exception& e) {
        return 0;
    }
}

long ConfigValue::as_long() const {
    try {
        return std::stol(value_);
    } catch (const std::exception& e) {
        return 0;
    }
}

double ConfigValue::as_double() const {
    try {
        return std::stod(value_);
    } catch (const std::exception& e) {
        return 0.0;
    }
}

bool ConfigValue::as_bool() const {
    std::string lower_value = value_;
    std::transform(lower_value.begin(), lower_value.end(), lower_value.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    
    return lower_value == "true" || lower_value == "yes" || lower_value == "1";
}

std::vector<std::string> ConfigValue::as_string_list() const {
    std::vector<std::string> result;
    std::istringstream ss(value_);
    std::string item;
    
    while (std::getline(ss, item, ',')) {
        // Trim whitespace
        item.erase(0, item.find_first_not_of(" \t"));
        item.erase(item.find_last_not_of(" \t") + 1);
        
        result.push_back(item);
    }
    
    return result;
}

std::vector<int> ConfigValue::as_int_list() const {
    std::vector<int> result;
    std::istringstream ss(value_);
    std::string item;
    
    while (std::getline(ss, item, ',')) {
        // Trim whitespace
        item.erase(0, item.find_first_not_of(" \t"));
        item.erase(item.find_last_not_of(" \t") + 1);
        
        try {
            result.push_back(std::stoi(item));
        } catch (const std::exception& e) {
            // Skip invalid items
        }
    }
    
    return result;
}

std::vector<double> ConfigValue::as_double_list() const {
    std::vector<double> result;
    std::istringstream ss(value_);
    std::string item;
    
    while (std::getline(ss, item, ',')) {
        // Trim whitespace
        item.erase(0, item.find_first_not_of(" \t"));
        item.erase(item.find_last_not_of(" \t") + 1);
        
        try {
            result.push_back(std::stod(item));
        } catch (const std::exception& e) {
            // Skip invalid items
        }
    }
    
    return result;
}

//
// ConfigManager Implementation
//

ConfigManager& ConfigManager::instance() {
    static ConfigManager instance;
    return instance;
}

ConfigManager::ConfigManager() {
}

bool ConfigManager::load_file(std::string_view filename) {
    // Fix the most vexing parse issue - use {} initialization 
    std::ifstream file{std::string(filename)};
    if (!file.is_open()) {
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        // Parse line
        auto [key, value] = parse_line(line);
        if (!key.empty()) {
            set(key, value);
        }
    }
    
    return true;
}

void ConfigManager::load_environment() {
    // This is platform-specific, but here's a basic implementation
    #ifdef _WIN32
    char* env = nullptr;
    size_t len = 0;
    if (_dupenv_s(&env, &len, "PATH") == 0 && env) {
        set("PATH", env);
        free(env);
    }
    #else
    // On Unix-like systems, we could use the environ variable
    // But that's beyond the scope of this example
    #endif
}

ConfigValue ConfigManager::get(std::string_view key, std::string_view default_value) const {
    auto it = config_.find(std::string(key));
    if (it != config_.end()) {
        return it->second;
    }
    
    return ConfigValue(std::string(default_value));
}

void ConfigManager::set(std::string_view key, std::string_view value) {
    std::string key_str(key);
    // Fix the most vexing parse issue - use {} initialization
    ConfigValue config_value{std::string(value)};
    
    // Update config
    config_[key_str] = config_value;
    
    // Notify listeners
    notify_listeners(key, config_value);
}

bool ConfigManager::has(std::string_view key) const {
    return config_.find(std::string(key)) != config_.end();
}

void ConfigManager::register_listener(std::string_view key, ConfigListener listener) {
    std::string key_str(key);
    listeners_[key_str].push_back(std::move(listener));
}

void ConfigManager::unregister_listeners(std::string_view key) {
    std::string key_str(key);
    listeners_.erase(key_str);
}

std::vector<std::string> ConfigManager::get_keys() const {
    std::vector<std::string> keys;
    keys.reserve(config_.size());
    
    for (const auto& [key, _] : config_) {
        keys.push_back(key);
    }
    
    return keys;
}

std::string ConfigManager::trim(std::string_view str) {
    const auto begin = str.find_first_not_of(" \t");
    if (begin == std::string::npos) {
        return "";
    }
    
    const auto end = str.find_last_not_of(" \t");
    const auto range = end - begin + 1;
    
    return std::string(str.substr(begin, range));
}

std::pair<std::string, std::string> ConfigManager::parse_line(std::string_view line) const {
    const auto pos = line.find('=');
    if (pos == std::string::npos) {
        return {"", ""};
    }
    
    std::string key = trim(line.substr(0, pos));
    std::string value = trim(line.substr(pos + 1));
    
    return {key, value};
}

void ConfigManager::notify_listeners(std::string_view key, const ConfigValue& value) {
    std::string key_str(key);
    auto it = listeners_.find(key_str);
    if (it != listeners_.end()) {
        for (const auto& listener : it->second) {
            listener(key, value);
        }
    }
}

} // namespace trading