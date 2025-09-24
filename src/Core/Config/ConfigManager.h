#pragma once

#include <string>
#include <unordered_map>
#include "../Json/JsonHelper.h"

namespace CryptoClaude {
namespace Config {

class ConfigManager {
private:
    static ConfigManager* instance_;
    Json::json config_;
    std::string config_file_;
    bool is_production_mode_;

    ConfigManager() = default;

public:
    static ConfigManager& getInstance();

    // Configuration loading
    bool loadConfig(const std::string& config_file);
    bool loadProductionConfig();
    bool loadDevelopmentConfig();

    // Environment detection
    bool isProductionMode() const;
    void setProductionMode(bool production);

    // Configuration getters
    std::string getDatabasePath() const;
    int getDatabaseConnectionPoolSize() const;
    int getDatabaseTimeout() const;

    std::string getApiBaseUrl(const std::string& service) const;
    int getApiTimeout(const std::string& service) const;
    int getApiRetryAttempts(const std::string& service) const;
    std::string getApiKeyEnvVar(const std::string& service) const;

    std::string getLogLevel() const;
    std::string getLogFilePath() const;
    bool isConsoleLoggingEnabled() const;

    // Security settings
    bool isApiKeyEncryptionEnabled() const;
    bool shouldLogSensitiveData() const;
    std::string getConfigFilePermissions() const;

    // Performance settings
    int getMaxMemoryUsageMB() const;
    int getCpuUsageLimit() const;
    int getHealthCheckInterval() const;

    // Trading settings
    double getMaxPositionSizeUSD() const;
    int getMaxDailyTrades() const;
    double getRiskLimitPercent() const;
    double getEmergencyStopLossPercent() const;

    // Validation
    bool validateConfig() const;
    bool validateSecuritySettings() const;
    bool validatePerformanceSettings() const;

    // Utility methods
    Json::json getSection(const std::string& section) const;
    template<typename T>
    T getValue(const std::string& path, const T& default_value) const;
};

// Template implementation
template<typename T>
T ConfigManager::getValue(const std::string& path, const T& default_value) const {
    try {
        // Simple path parsing (e.g., "database.timeout_seconds")
        size_t dot_pos = path.find('.');
        if (dot_pos == std::string::npos) {
            if constexpr (std::is_same_v<T, std::string>) {
                return Json::JsonHelper::getString(config_, path, default_value);
            } else if constexpr (std::is_same_v<T, int>) {
                return Json::JsonHelper::getInt(config_, path, default_value);
            } else if constexpr (std::is_same_v<T, double>) {
                return Json::JsonHelper::getDouble(config_, path, default_value);
            } else if constexpr (std::is_same_v<T, bool>) {
                return Json::JsonHelper::getBool(config_, path, default_value);
            }
        } else {
            std::string section = path.substr(0, dot_pos);
            std::string key = path.substr(dot_pos + 1);

            Json::json section_obj = getSection(section);
            if constexpr (std::is_same_v<T, std::string>) {
                return Json::JsonHelper::getString(section_obj, key, default_value);
            } else if constexpr (std::is_same_v<T, int>) {
                return Json::JsonHelper::getInt(section_obj, key, default_value);
            } else if constexpr (std::is_same_v<T, double>) {
                return Json::JsonHelper::getDouble(section_obj, key, default_value);
            } else if constexpr (std::is_same_v<T, bool>) {
                return Json::JsonHelper::getBool(section_obj, key, default_value);
            }
        }
    } catch (const std::exception&) {
        // Return default value on any error
    }

    return default_value;
}

} // namespace Config
} // namespace CryptoClaude