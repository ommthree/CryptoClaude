#pragma once

#include "../Database/DatabaseManager.h"
#include <string>
#include <map>
#include <vector>
#include <memory>
#include <chrono>
#include <functional>

namespace CryptoClaude {
namespace Config {

using namespace CryptoClaude::Database;

// Configuration security levels
enum class SecurityLevel {
    PUBLIC,        // Can be logged and displayed
    INTERNAL,      // Can be logged but not displayed to users
    SENSITIVE,     // Should not be logged or displayed
    SECRET         // Encrypted at rest, never logged or displayed
};

// Configuration value with metadata
struct ConfigValue {
    std::string key;
    std::string value;
    SecurityLevel securityLevel = SecurityLevel::PUBLIC;
    std::string description;
    std::string defaultValue;
    std::vector<std::string> allowedValues; // For enum-like configs
    std::chrono::system_clock::time_point lastModified;
    std::string modifiedBy;
    bool isEncrypted = false;

    ConfigValue() = default;
    ConfigValue(const std::string& k, const std::string& v,
               SecurityLevel level = SecurityLevel::PUBLIC,
               const std::string& desc = "")
        : key(k), value(v), securityLevel(level), description(desc),
          lastModified(std::chrono::system_clock::now()) {}
};

// API provider configuration
struct ApiProviderConfig {
    std::string providerId;
    std::string providerType; // "cryptocompare", "coinbase", "binance", etc.
    bool isEnabled = true;
    int priority = 1; // Lower numbers = higher priority

    // Authentication
    std::string apiKey;
    std::string apiSecret;
    std::string passphrase; // For some providers
    std::string baseUrl;

    // Rate limiting
    int maxRequestsPerSecond = 10;
    int maxRequestsPerMinute = 600;
    int maxRequestsPerHour = 10000;
    std::chrono::milliseconds requestTimeout = std::chrono::milliseconds(30000);

    // Retry configuration
    int maxRetries = 3;
    std::chrono::milliseconds retryDelayBase = std::chrono::milliseconds(1000);
    double retryBackoffMultiplier = 2.0;

    // Data configuration
    std::vector<std::string> supportedSymbols;
    std::vector<std::string> supportedTimeframes;
    bool supportsRealTime = true;
    bool supportsHistorical = true;

    // Quality settings
    double minDataQualityThreshold = 0.90;
    std::chrono::minutes maxDataAge = std::chrono::minutes(5);

    // Health monitoring
    double latencyWarningThreshold = 500.0;    // milliseconds
    double latencyCriticalThreshold = 2000.0;
    double successRateWarningThreshold = 0.95;
    double successRateCriticalThreshold = 0.80;

    ApiProviderConfig() = default;
    ApiProviderConfig(const std::string& id, const std::string& type)
        : providerId(id), providerType(type) {}

    bool isValid() const {
        return !providerId.empty() && !providerType.empty() && !apiKey.empty() && !baseUrl.empty();
    }

    std::vector<std::string> validate() const {
        std::vector<std::string> errors;
        if (providerId.empty()) errors.push_back("Provider ID is required");
        if (providerType.empty()) errors.push_back("Provider type is required");
        if (apiKey.empty()) errors.push_back("API key is required");
        if (baseUrl.empty()) errors.push_back("Base URL is required");
        if (maxRequestsPerSecond <= 0) errors.push_back("Max requests per second must be positive");
        if (requestTimeout.count() <= 0) errors.push_back("Request timeout must be positive");
        return errors;
    }
};

// Global configuration settings
struct GlobalApiConfig {
    // Default settings
    std::chrono::seconds defaultHealthCheckInterval = std::chrono::seconds(60);
    std::chrono::seconds defaultConnectionTestInterval = std::chrono::seconds(300);
    bool enableGlobalRateLimiting = true;
    bool enableGlobalRetries = true;
    bool enableGlobalLogging = true;

    // Failover settings
    bool enableProviderFailover = true;
    int maxFailoverAttempts = 3;
    std::chrono::minutes failoverCooldownPeriod = std::chrono::minutes(5);

    // Data quality defaults
    double globalDataQualityThreshold = 0.90;
    std::chrono::minutes globalMaxDataAge = std::chrono::minutes(5);
    bool enableDataValidation = true;

    // Security settings
    bool encryptSensitiveConfigs = true;
    std::chrono::hours configCacheTimeout = std::chrono::hours(1);
    bool auditConfigurationChanges = true;

    // Performance settings
    int maxConcurrentConnections = 50;
    std::chrono::seconds connectionPoolTimeout = std::chrono::seconds(300);
    bool enableConnectionPooling = true;

    // Monitoring and alerting
    bool enableHealthMonitoring = true;
    bool enablePerformanceMonitoring = true;
    bool enableAlertNotifications = true;
    std::string alertEmailRecipients; // Comma-separated email addresses
};

// Configuration change event
struct ConfigChangeEvent {
    std::string configKey;
    std::string oldValue;
    std::string newValue;
    std::chrono::system_clock::time_point changedAt;
    std::string changedBy;
    std::string changeReason;
};

// Main configuration manager
class ApiConfigurationManager {
public:
    ApiConfigurationManager(DatabaseManager& dbManager);
    ~ApiConfigurationManager() = default;

    // Initialization
    bool initialize();
    void shutdown();

    // Basic configuration management
    bool setConfig(const std::string& key, const std::string& value,
                  SecurityLevel level = SecurityLevel::PUBLIC,
                  const std::string& description = "",
                  const std::string& modifiedBy = "system");

    std::string getConfig(const std::string& key, const std::string& defaultValue = "") const;
    bool hasConfig(const std::string& key) const;
    bool removeConfig(const std::string& key);

    // Typed configuration getters
    int getConfigInt(const std::string& key, int defaultValue = 0) const;
    double getConfigDouble(const std::string& key, double defaultValue = 0.0) const;
    bool getConfigBool(const std::string& key, bool defaultValue = false) const;
    std::vector<std::string> getConfigList(const std::string& key) const;

    // Configuration metadata
    ConfigValue getConfigDetails(const std::string& key) const;
    std::vector<std::string> getAllConfigKeys() const;
    std::map<std::string, ConfigValue> getAllConfigs(bool includeSensitive = false) const;

    // API provider configuration
    bool addApiProvider(const ApiProviderConfig& config);
    bool updateApiProvider(const std::string& providerId, const ApiProviderConfig& config);
    bool removeApiProvider(const std::string& providerId);

    ApiProviderConfig getApiProviderConfig(const std::string& providerId) const;
    std::vector<std::string> getApiProviderIds() const;
    std::vector<ApiProviderConfig> getAllApiProviders(bool enabledOnly = false) const;
    std::vector<ApiProviderConfig> getApiProvidersByType(const std::string& type) const;

    // Provider management
    bool enableProvider(const std::string& providerId);
    bool disableProvider(const std::string& providerId);
    bool setProviderPriority(const std::string& providerId, int priority);

    // Global configuration
    void setGlobalConfig(const GlobalApiConfig& config);
    GlobalApiConfig getGlobalConfig() const;

    // Configuration validation
    bool validateConfiguration() const;
    std::vector<std::string> getConfigurationErrors() const;
    bool validateApiProviderConfig(const ApiProviderConfig& config) const;

    // Environment-specific configuration
    enum class Environment {
        DEVELOPMENT,
        STAGING,
        PRODUCTION
    };

    void setEnvironment(Environment env);
    Environment getEnvironment() const { return currentEnvironment_; }
    std::string getEnvironmentString() const;

    // Configuration profiles for different trading scenarios
    struct ConfigurationProfile {
        std::string profileName;
        std::string description;
        std::map<std::string, std::string> configOverrides;
        std::vector<std::string> enabledProviders;
        GlobalApiConfig globalSettings;
    };

    bool saveConfigurationProfile(const ConfigurationProfile& profile);
    bool loadConfigurationProfile(const std::string& profileName);
    std::vector<std::string> getAvailableProfiles() const;
    bool deleteConfigurationProfile(const std::string& profileName);

    // Security and encryption
    bool encryptSensitiveConfig(const std::string& key);
    bool decryptSensitiveConfig(const std::string& key);
    void setEncryptionKey(const std::string& key);
    bool isConfigEncrypted(const std::string& key) const;

    // Configuration backup and restore
    struct ConfigurationBackup {
        std::chrono::system_clock::time_point backupTime;
        std::string backupDescription;
        std::map<std::string, ConfigValue> configurations;
        std::vector<ApiProviderConfig> providers;
        GlobalApiConfig globalConfig;
    };

    bool createConfigurationBackup(const std::string& description);
    std::vector<std::string> getAvailableBackups() const;
    bool restoreConfigurationBackup(const std::string& backupId);
    bool deleteConfigurationBackup(const std::string& backupId);

    // Configuration monitoring and changes
    using ConfigChangeCallback = std::function<void(const ConfigChangeEvent&)>;
    void setConfigChangeCallback(ConfigChangeCallback callback) { changeCallback_ = callback; }

    std::vector<ConfigChangeEvent> getRecentConfigChanges(int hours = 24) const;
    bool auditConfigurationChange(const std::string& key, const std::string& oldValue,
                                 const std::string& newValue, const std::string& changedBy,
                                 const std::string& reason = "");

    // Configuration import/export
    bool exportConfigurationToJson(const std::string& filePath, bool includeSecrets = false) const;
    bool importConfigurationFromJson(const std::string& filePath);
    std::string exportConfigurationToString(bool includeSecrets = false) const;
    bool importConfigurationFromString(const std::string& jsonConfig);

    // Runtime configuration updates
    void reloadConfiguration();
    void clearConfigurationCache();
    bool isConfigurationCacheValid() const;

    // Default configurations for common setups
    static ApiProviderConfig createCryptoCompareConfig(const std::string& apiKey);
    static ApiProviderConfig createCoinbaseProConfig(const std::string& apiKey,
                                                     const std::string& secret,
                                                     const std::string& passphrase);
    static ApiProviderConfig createBinanceConfig(const std::string& apiKey,
                                                const std::string& secret);

    static GlobalApiConfig createDevelopmentConfig();
    static GlobalApiConfig createProductionConfig();
    static GlobalApiConfig createHighFrequencyTradingConfig();

private:
    DatabaseManager& dbManager_;
    Environment currentEnvironment_;
    std::string encryptionKey_;
    ConfigChangeCallback changeCallback_;

    // Configuration cache
    mutable std::map<std::string, ConfigValue> configCache_;
    mutable std::map<std::string, ApiProviderConfig> providerCache_;
    mutable GlobalApiConfig globalConfigCache_;
    mutable std::chrono::system_clock::time_point cacheLastUpdated_;
    mutable bool cacheValid_;

    // Database operations
    bool initializeDatabaseTables();
    bool persistConfig(const ConfigValue& config);
    bool persistApiProvider(const ApiProviderConfig& provider);
    bool persistGlobalConfig(const GlobalApiConfig& config);

    ConfigValue loadConfigFromDatabase(const std::string& key) const;
    ApiProviderConfig loadApiProviderFromDatabase(const std::string& providerId) const;
    GlobalApiConfig loadGlobalConfigFromDatabase() const;

    // Cache management
    void updateConfigCache() const;
    void invalidateCache();
    bool isCacheValid() const;

    // Encryption helpers
    std::string encryptValue(const std::string& value) const;
    std::string decryptValue(const std::string& encryptedValue) const;
    bool isValueEncrypted(const std::string& value) const;

    // Configuration validation helpers
    bool validateGlobalConfig(const GlobalApiConfig& config) const;
    std::vector<std::string> validateProviderConfiguration(const ApiProviderConfig& provider) const;

    // Event and audit helpers
    void notifyConfigurationChange(const std::string& key, const std::string& oldValue,
                                 const std::string& newValue, const std::string& changedBy);
    bool logConfigurationChange(const ConfigChangeEvent& event);

    // Environment-specific defaults
    void applyEnvironmentDefaults();
    std::map<std::string, std::string> getDevelopmentDefaults() const;
    std::map<std::string, std::string> getProductionDefaults() const;
};

// Utility class for configuration validation and testing
class ConfigurationValidator {
public:
    explicit ConfigurationValidator(ApiConfigurationManager& configManager);

    struct ValidationResult {
        bool isValid = true;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
        std::vector<std::string> recommendations;
        int totalConfigsValidated = 0;
        int validConfigs = 0;
        int invalidConfigs = 0;
    };

    ValidationResult validateAllConfigurations();
    ValidationResult validateApiProviders();
    ValidationResult validateGlobalConfiguration();
    ValidationResult validateSecuritySettings();

    // Configuration testing
    bool testApiProviderConnection(const std::string& providerId);
    std::map<std::string, bool> testAllApiProviderConnections();

    // Configuration optimization recommendations
    struct OptimizationRecommendation {
        std::string configKey;
        std::string currentValue;
        std::string recommendedValue;
        std::string reason;
        double expectedImprovement; // 0.0 to 1.0
    };

    std::vector<OptimizationRecommendation> getOptimizationRecommendations();

private:
    ApiConfigurationManager& configManager_;

    bool validateConfigValue(const ConfigValue& config);
    bool validateApiProvider(const ApiProviderConfig& provider);
    bool validateUrlFormat(const std::string& url);
    bool validateApiKeyFormat(const std::string& apiKey);
};

} // namespace Config
} // namespace CryptoClaude