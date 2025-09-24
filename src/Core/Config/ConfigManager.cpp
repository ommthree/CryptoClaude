#include "ConfigManager.h"
#include <fstream>
#include <iostream>
#include <cstdlib>

namespace CryptoClaude {
namespace Config {

ConfigManager* ConfigManager::instance_ = nullptr;

ConfigManager& ConfigManager::getInstance() {
    if (instance_ == nullptr) {
        instance_ = new ConfigManager();
    }
    return *instance_;
}

bool ConfigManager::loadConfig(const std::string& config_file) {
    config_file_ = config_file;

    std::ifstream file(config_file);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open config file: " << config_file << std::endl;
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();

    if (!Json::JsonHelper::parseString(content, config_)) {
        std::cerr << "Error: Failed to parse config file: " << config_file << std::endl;
        return false;
    }

    // Detect production mode from config file name or environment
    is_production_mode_ = (config_file.find("production") != std::string::npos);
    const char* env_mode = std::getenv("CRYPTOCLAUDE_MODE");
    if (env_mode && std::string(env_mode) == "production") {
        is_production_mode_ = true;
    }

    return validateConfig();
}

bool ConfigManager::loadProductionConfig() {
    return loadConfig("config/production.json");
}

bool ConfigManager::loadDevelopmentConfig() {
    return loadConfig("config/development.json");
}

bool ConfigManager::isProductionMode() const {
    return is_production_mode_;
}

void ConfigManager::setProductionMode(bool production) {
    is_production_mode_ = production;
}

std::string ConfigManager::getDatabasePath() const {
    return getValue<std::string>("database.path", "cryptoClaude.db");
}

int ConfigManager::getDatabaseConnectionPoolSize() const {
    return getValue<int>("database.connection_pool_size", 5);
}

int ConfigManager::getDatabaseTimeout() const {
    return getValue<int>("database.timeout_seconds", 10);
}

std::string ConfigManager::getApiBaseUrl(const std::string& service) const {
    return getValue<std::string>("api." + service + ".base_url", "");
}

int ConfigManager::getApiTimeout(const std::string& service) const {
    return getValue<int>("api." + service + ".timeout_seconds", 30);
}

int ConfigManager::getApiRetryAttempts(const std::string& service) const {
    return getValue<int>("api." + service + ".retry_attempts", 3);
}

std::string ConfigManager::getApiKeyEnvVar(const std::string& service) const {
    return getValue<std::string>("api." + service + ".api_key_env_var", "");
}

std::string ConfigManager::getLogLevel() const {
    return getValue<std::string>("logging.level", "INFO");
}

std::string ConfigManager::getLogFilePath() const {
    return getValue<std::string>("logging.file_path", "logs/cryptoClaude.log");
}

bool ConfigManager::isConsoleLoggingEnabled() const {
    return getValue<bool>("logging.enable_console", true);
}

bool ConfigManager::isApiKeyEncryptionEnabled() const {
    return getValue<bool>("security.api_key_encryption", false);
}

bool ConfigManager::shouldLogSensitiveData() const {
    return getValue<bool>("security.log_sensitive_data", false);
}

std::string ConfigManager::getConfigFilePermissions() const {
    return getValue<std::string>("security.config_file_permissions", "644");
}

int ConfigManager::getMaxMemoryUsageMB() const {
    return getValue<int>("performance.max_memory_usage_mb", 256);
}

int ConfigManager::getCpuUsageLimit() const {
    return getValue<int>("performance.cpu_usage_limit_percent", 80);
}

int ConfigManager::getHealthCheckInterval() const {
    return getValue<int>("performance.health_check_interval_seconds", 60);
}

double ConfigManager::getMaxPositionSizeUSD() const {
    return getValue<double>("trading.max_position_size_usd", 1000.0);
}

int ConfigManager::getMaxDailyTrades() const {
    return getValue<int>("trading.max_daily_trades", 50);
}

double ConfigManager::getRiskLimitPercent() const {
    return getValue<double>("trading.risk_limit_percent", 1.0);
}

double ConfigManager::getEmergencyStopLossPercent() const {
    return getValue<double>("trading.emergency_stop_loss_percent", 3.0);
}

bool ConfigManager::validateConfig() const {
    if (!Json::JsonHelper::isJsonLibraryAvailable()) {
        std::cerr << "Warning: JSON library not available for config validation" << std::endl;
        return true; // Allow operation with basic config
    }

    // Validate required sections exist
    std::vector<std::string> required_sections = {"database", "api", "logging", "security"};
    for (const auto& section : required_sections) {
        if (!Json::JsonHelper::hasKey(config_, section)) {
            std::cerr << "Error: Missing required config section: " << section << std::endl;
            return false;
        }
    }

    return validateSecuritySettings() && validatePerformanceSettings();
}

bool ConfigManager::validateSecuritySettings() const {
    if (is_production_mode_) {
        // Production security requirements
        if (!isApiKeyEncryptionEnabled()) {
            std::cerr << "Warning: API key encryption disabled in production mode" << std::endl;
        }

        if (shouldLogSensitiveData()) {
            std::cerr << "Error: Sensitive data logging enabled in production mode" << std::endl;
            return false;
        }

        std::string permissions = getConfigFilePermissions();
        if (permissions != "600") {
            std::cerr << "Warning: Config file permissions should be 600 in production" << std::endl;
        }
    }

    return true;
}

bool ConfigManager::validatePerformanceSettings() const {
    int max_memory = getMaxMemoryUsageMB();
    if (max_memory < 64 || max_memory > 2048) {
        std::cerr << "Warning: Memory limit outside recommended range (64-2048 MB): "
                  << max_memory << " MB" << std::endl;
    }

    int cpu_limit = getCpuUsageLimit();
    if (cpu_limit < 50 || cpu_limit > 95) {
        std::cerr << "Warning: CPU limit outside recommended range (50-95%): "
                  << cpu_limit << "%" << std::endl;
    }

    return true;
}

Json::json ConfigManager::getSection(const std::string& section) const {
    if (!Json::JsonHelper::isJsonLibraryAvailable()) {
        return Json::json{}; // Return empty JSON object
    }

    if (Json::JsonHelper::hasKey(config_, section)) {
        // This would need actual JSON object extraction - simplified for demo
        return config_; // In real implementation, would return config_[section]
    }

    return Json::json{};
}

} // namespace Config
} // namespace CryptoClaude