#include "ApiConfigurationManager.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <random>
#include <iostream>

namespace CryptoClaude {
namespace Config {


ApiConfigurationManager::ApiConfigurationManager(DatabaseManager& dbManager)
    : dbManager_(dbManager), currentEnvironment_(Environment::DEVELOPMENT), cacheValid_(false) {
    cacheLastUpdated_ = std::chrono::system_clock::now() - std::chrono::hours(2); // Force initial cache load
}

bool ApiConfigurationManager::initialize() {
    try {
        if (!initializeDatabaseTables()) {
            std::cerr << "Failed to initialize configuration database tables" << std::endl;
            return false;
        }

        // Load or create default global configuration
        auto globalConfig = loadGlobalConfigFromDatabase();
        if (globalConfig.defaultHealthCheckInterval.count() == 0) {
            // First time initialization - set defaults
            globalConfig = createDevelopmentConfig();
            setGlobalConfig(globalConfig);
        }
        globalConfigCache_ = globalConfig;

        // Apply environment-specific defaults
        applyEnvironmentDefaults();

        // Update cache
        updateConfigCache();

        std::cout << "ApiConfigurationManager initialized successfully" << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "ApiConfigurationManager initialization failed: " << e.what() << std::endl;
        return false;
    }
}

void ApiConfigurationManager::shutdown() {
    clearConfigurationCache();
    std::cout << "ApiConfigurationManager shutdown completed" << std::endl;
}

bool ApiConfigurationManager::initializeDatabaseTables() {
    std::vector<std::string> createTableQueries = {
        R"(
            CREATE TABLE IF NOT EXISTS api_configurations (
                config_key TEXT PRIMARY KEY,
                config_value TEXT NOT NULL,
                security_level INTEGER DEFAULT 0,
                description TEXT DEFAULT '',
                default_value TEXT DEFAULT '',
                allowed_values TEXT DEFAULT '',
                last_modified INTEGER,
                modified_by TEXT DEFAULT 'system',
                is_encrypted INTEGER DEFAULT 0
            )
        )",
        R"(
            CREATE TABLE IF NOT EXISTS api_providers (
                provider_id TEXT PRIMARY KEY,
                provider_type TEXT NOT NULL,
                is_enabled INTEGER DEFAULT 1,
                priority INTEGER DEFAULT 1,
                api_key TEXT,
                api_secret TEXT,
                passphrase TEXT,
                base_url TEXT NOT NULL,
                max_requests_per_second INTEGER DEFAULT 10,
                max_requests_per_minute INTEGER DEFAULT 600,
                max_requests_per_hour INTEGER DEFAULT 10000,
                request_timeout INTEGER DEFAULT 30000,
                max_retries INTEGER DEFAULT 3,
                retry_delay_base INTEGER DEFAULT 1000,
                retry_backoff_multiplier REAL DEFAULT 2.0,
                supported_symbols TEXT DEFAULT '',
                supported_timeframes TEXT DEFAULT '',
                supports_realtime INTEGER DEFAULT 1,
                supports_historical INTEGER DEFAULT 1,
                min_data_quality_threshold REAL DEFAULT 0.90,
                max_data_age INTEGER DEFAULT 300,
                latency_warning_threshold REAL DEFAULT 500.0,
                latency_critical_threshold REAL DEFAULT 2000.0,
                success_rate_warning_threshold REAL DEFAULT 0.95,
                success_rate_critical_threshold REAL DEFAULT 0.80,
                created_at INTEGER,
                updated_at INTEGER
            )
        )",
        R"(
            CREATE TABLE IF NOT EXISTS global_api_config (
                config_id INTEGER PRIMARY KEY CHECK (config_id = 1),
                default_health_check_interval INTEGER DEFAULT 60,
                default_connection_test_interval INTEGER DEFAULT 300,
                enable_global_rate_limiting INTEGER DEFAULT 1,
                enable_global_retries INTEGER DEFAULT 1,
                enable_global_logging INTEGER DEFAULT 1,
                enable_provider_failover INTEGER DEFAULT 1,
                max_failover_attempts INTEGER DEFAULT 3,
                failover_cooldown_period INTEGER DEFAULT 300,
                global_data_quality_threshold REAL DEFAULT 0.90,
                global_max_data_age INTEGER DEFAULT 300,
                enable_data_validation INTEGER DEFAULT 1,
                encrypt_sensitive_configs INTEGER DEFAULT 1,
                config_cache_timeout INTEGER DEFAULT 3600,
                audit_configuration_changes INTEGER DEFAULT 1,
                max_concurrent_connections INTEGER DEFAULT 50,
                connection_pool_timeout INTEGER DEFAULT 300,
                enable_connection_pooling INTEGER DEFAULT 1,
                enable_health_monitoring INTEGER DEFAULT 1,
                enable_performance_monitoring INTEGER DEFAULT 1,
                enable_alert_notifications INTEGER DEFAULT 1,
                alert_email_recipients TEXT DEFAULT '',
                updated_at INTEGER
            )
        )",
        R"(
            CREATE TABLE IF NOT EXISTS config_change_audit (
                change_id INTEGER PRIMARY KEY AUTOINCREMENT,
                config_key TEXT NOT NULL,
                old_value TEXT,
                new_value TEXT,
                changed_at INTEGER NOT NULL,
                changed_by TEXT NOT NULL,
                change_reason TEXT DEFAULT ''
            )
        )",
        R"(
            CREATE TABLE IF NOT EXISTS configuration_profiles (
                profile_name TEXT PRIMARY KEY,
                description TEXT,
                config_overrides TEXT,
                enabled_providers TEXT,
                global_settings TEXT,
                created_at INTEGER,
                updated_at INTEGER
            )
        )",
        R"(
            CREATE TABLE IF NOT EXISTS configuration_backups (
                backup_id TEXT PRIMARY KEY,
                backup_time INTEGER NOT NULL,
                backup_description TEXT,
                configurations TEXT,
                providers TEXT,
                global_config TEXT
            )
        )"
    };

    for (const auto& query : createTableQueries) {
        if (!dbManager_.executeQuery(query)) {
            std::cerr << "Failed to create configuration table" << std::endl;
            return false;
        }
    }

    return true;
}

bool ApiConfigurationManager::setConfig(const std::string& key, const std::string& value,
                                      SecurityLevel level, const std::string& description,
                                      const std::string& modifiedBy) {
    try {
        // Get old value for audit
        std::string oldValue = getConfig(key, "");

        // Create config value
        ConfigValue config(key, value, level, description);
        config.modifiedBy = modifiedBy;

        // Encrypt if required and sensitive
        if ((level == SecurityLevel::SENSITIVE || level == SecurityLevel::SECRET) &&
            globalConfigCache_.encryptSensitiveConfigs) {
            config.value = encryptValue(value);
            config.isEncrypted = true;
        }

        // Persist to database
        if (!persistConfig(config)) {
            return false;
        }

        // Update cache
        configCache_[key] = config;

        // Notify change
        if (changeCallback_) {
            ConfigChangeEvent event;
            event.configKey = key;
            event.oldValue = oldValue;
            event.newValue = (level <= SecurityLevel::INTERNAL) ? value : "[HIDDEN]";
            event.changedAt = std::chrono::system_clock::now();
            event.changedBy = modifiedBy;
            changeCallback_(event);
        }

        // Audit configuration change
        if (globalConfigCache_.auditConfigurationChanges) {
            auditConfigurationChange(key, oldValue, value, modifiedBy);
        }

        return true;

    } catch (const std::exception& e) {
        std::cerr << "Failed to set configuration " << key << ": " << e.what() << std::endl;
        return false;
    }
}

std::string ApiConfigurationManager::getConfig(const std::string& key, const std::string& defaultValue) const {
    try {
        // Check cache first
        if (!isCacheValid()) {
            updateConfigCache();
        }

        auto it = configCache_.find(key);
        if (it != configCache_.end()) {
            const auto& config = it->second;

            // Decrypt if encrypted
            if (config.isEncrypted) {
                return decryptValue(config.value);
            }

            return config.value;
        }

        return defaultValue;

    } catch (const std::exception& e) {
        std::cerr << "Failed to get configuration " << key << ": " << e.what() << std::endl;
        return defaultValue;
    }
}

bool ApiConfigurationManager::addApiProvider(const ApiProviderConfig& config) {
    try {
        // Validate configuration
        auto errors = config.validate();
        if (!errors.empty()) {
            std::cerr << "Invalid API provider configuration: " << errors[0] << std::endl;
            return false;
        }

        // Persist to database
        if (!persistApiProvider(config)) {
            return false;
        }

        // Update cache
        providerCache_[config.providerId] = config;

        std::cout << "Added API provider: " << config.providerId << " (" << config.providerType << ")" << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Failed to add API provider " << config.providerId << ": " << e.what() << std::endl;
        return false;
    }
}

ApiProviderConfig ApiConfigurationManager::getApiProviderConfig(const std::string& providerId) const {
    try {
        // Check cache first
        if (!isCacheValid()) {
            updateConfigCache();
        }

        auto it = providerCache_.find(providerId);
        if (it != providerCache_.end()) {
            return it->second;
        }

        // Load from database if not in cache
        return loadApiProviderFromDatabase(providerId);

    } catch (const std::exception& e) {
        std::cerr << "Failed to get API provider config " << providerId << ": " << e.what() << std::endl;
        return ApiProviderConfig();
    }
}

std::vector<ApiProviderConfig> ApiConfigurationManager::getAllApiProviders(bool enabledOnly) const {
    try {
        if (!isCacheValid()) {
            updateConfigCache();
        }

        std::vector<ApiProviderConfig> providers;
        for (const auto& pair : providerCache_) {
            if (!enabledOnly || pair.second.isEnabled) {
                providers.push_back(pair.second);
            }
        }

        // Sort by priority (lower number = higher priority)
        std::sort(providers.begin(), providers.end(),
            [](const ApiProviderConfig& a, const ApiProviderConfig& b) {
                return a.priority < b.priority;
            });

        return providers;

    } catch (const std::exception& e) {
        std::cerr << "Failed to get all API providers: " << e.what() << std::endl;
        return {};
    }
}

void ApiConfigurationManager::setGlobalConfig(const GlobalApiConfig& config) {
    try {
        if (!persistGlobalConfig(config)) {
            std::cerr << "Failed to persist global configuration" << std::endl;
            return;
        }

        globalConfigCache_ = config;
        std::cout << "Global API configuration updated" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Failed to set global configuration: " << e.what() << std::endl;
    }
}

GlobalApiConfig ApiConfigurationManager::getGlobalConfig() const {
    if (!isCacheValid()) {
        updateConfigCache();
    }
    return globalConfigCache_;
}

bool ApiConfigurationManager::persistConfig(const ConfigValue& config) {
    try {
        std::string query = R"(
            INSERT OR REPLACE INTO api_configurations
            (config_key, config_value, security_level, description, default_value,
             last_modified, modified_by, is_encrypted)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?)
        )";

        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
            config.lastModified.time_since_epoch()).count();

        return dbManager_.executeParameterizedQuery(query, {
            config.key,
            config.value,
            std::to_string(static_cast<int>(config.securityLevel)),
            config.description,
            config.defaultValue,
            std::to_string(timestamp),
            config.modifiedBy,
            config.isEncrypted ? "1" : "0"
        });

    } catch (const std::exception& e) {
        std::cerr << "Failed to persist configuration: " << e.what() << std::endl;
        return false;
    }
}

bool ApiConfigurationManager::persistApiProvider(const ApiProviderConfig& provider) {
    try {
        std::string query = R"(
            INSERT OR REPLACE INTO api_providers
            (provider_id, provider_type, is_enabled, priority, api_key, api_secret,
             passphrase, base_url, max_requests_per_second, max_requests_per_minute,
             max_requests_per_hour, request_timeout, max_retries, retry_delay_base,
             retry_backoff_multiplier, min_data_quality_threshold, max_data_age,
             latency_warning_threshold, latency_critical_threshold,
             success_rate_warning_threshold, success_rate_critical_threshold, updated_at)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        )";

        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        // Encrypt sensitive data
        std::string encryptedApiKey = provider.apiKey;
        std::string encryptedApiSecret = provider.apiSecret;
        std::string encryptedPassphrase = provider.passphrase;

        if (globalConfigCache_.encryptSensitiveConfigs) {
            if (!provider.apiKey.empty()) encryptedApiKey = encryptValue(provider.apiKey);
            if (!provider.apiSecret.empty()) encryptedApiSecret = encryptValue(provider.apiSecret);
            if (!provider.passphrase.empty()) encryptedPassphrase = encryptValue(provider.passphrase);
        }

        return dbManager_.executeParameterizedQuery(query, {
            provider.providerId,
            provider.providerType,
            provider.isEnabled ? "1" : "0",
            std::to_string(provider.priority),
            encryptedApiKey,
            encryptedApiSecret,
            encryptedPassphrase,
            provider.baseUrl,
            std::to_string(provider.maxRequestsPerSecond),
            std::to_string(provider.maxRequestsPerMinute),
            std::to_string(provider.maxRequestsPerHour),
            std::to_string(provider.requestTimeout.count()),
            std::to_string(provider.maxRetries),
            std::to_string(provider.retryDelayBase.count()),
            std::to_string(provider.retryBackoffMultiplier),
            std::to_string(provider.minDataQualityThreshold),
            std::to_string(provider.maxDataAge.count()),
            std::to_string(provider.latencyWarningThreshold),
            std::to_string(provider.latencyCriticalThreshold),
            std::to_string(provider.successRateWarningThreshold),
            std::to_string(provider.successRateCriticalThreshold),
            std::to_string(timestamp)
        });

    } catch (const std::exception& e) {
        std::cerr << "Failed to persist API provider: " << e.what() << std::endl;
        return false;
    }
}

bool ApiConfigurationManager::persistGlobalConfig(const GlobalApiConfig& config) {
    try {
        std::string query = R"(
            INSERT OR REPLACE INTO global_api_config
            (config_id, default_health_check_interval, default_connection_test_interval,
             enable_global_rate_limiting, enable_global_retries, enable_global_logging,
             enable_provider_failover, max_failover_attempts, failover_cooldown_period,
             global_data_quality_threshold, global_max_data_age, enable_data_validation,
             encrypt_sensitive_configs, config_cache_timeout, audit_configuration_changes,
             max_concurrent_connections, connection_pool_timeout, enable_connection_pooling,
             enable_health_monitoring, enable_performance_monitoring, enable_alert_notifications,
             alert_email_recipients, updated_at)
            VALUES (1, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        )";

        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        return dbManager_.executeParameterizedQuery(query, {
            std::to_string(config.defaultHealthCheckInterval.count()),
            std::to_string(config.defaultConnectionTestInterval.count()),
            config.enableGlobalRateLimiting ? "1" : "0",
            config.enableGlobalRetries ? "1" : "0",
            config.enableGlobalLogging ? "1" : "0",
            config.enableProviderFailover ? "1" : "0",
            std::to_string(config.maxFailoverAttempts),
            std::to_string(config.failoverCooldownPeriod.count()),
            std::to_string(config.globalDataQualityThreshold),
            std::to_string(config.globalMaxDataAge.count()),
            config.enableDataValidation ? "1" : "0",
            config.encryptSensitiveConfigs ? "1" : "0",
            std::to_string(config.configCacheTimeout.count()),
            config.auditConfigurationChanges ? "1" : "0",
            std::to_string(config.maxConcurrentConnections),
            std::to_string(config.connectionPoolTimeout.count()),
            config.enableConnectionPooling ? "1" : "0",
            config.enableHealthMonitoring ? "1" : "0",
            config.enablePerformanceMonitoring ? "1" : "0",
            config.enableAlertNotifications ? "1" : "0",
            config.alertEmailRecipients,
            std::to_string(timestamp)
        });

    } catch (const std::exception& e) {
        std::cerr << "Failed to persist global configuration: " << e.what() << std::endl;
        return false;
    }
}

void ApiConfigurationManager::updateConfigCache() const {
    try {
        // Load configurations from database
        std::string configQuery = "SELECT * FROM api_configurations";
        auto configResults = dbManager_.executeSelectQuery(configQuery);

        configCache_.clear();
        for (const auto& row : configResults) {
            ConfigValue config;
            config.key = row.at("config_key");
            config.value = row.at("config_value");
            config.securityLevel = static_cast<SecurityLevel>(std::stoi(row.at("security_level")));
            config.description = row.at("description");
            config.defaultValue = row.at("default_value");
            config.modifiedBy = row.at("modified_by");
            config.isEncrypted = (row.at("is_encrypted") == "1");

            auto timestamp = std::stoll(row.at("last_modified"));
            config.lastModified = std::chrono::system_clock::from_time_t(timestamp);

            configCache_[config.key] = config;
        }

        // Load API providers from database
        std::string providerQuery = "SELECT * FROM api_providers";
        auto providerResults = dbManager_.executeSelectQuery(providerQuery);

        providerCache_.clear();
        for (const auto& row : providerResults) {
            ApiProviderConfig provider;
            provider.providerId = row.at("provider_id");
            provider.providerType = row.at("provider_type");
            provider.isEnabled = (row.at("is_enabled") == "1");
            provider.priority = std::stoi(row.at("priority"));
            provider.baseUrl = row.at("base_url");

            // Decrypt sensitive data if encrypted
            provider.apiKey = row.at("api_key");
            provider.apiSecret = row.at("api_secret");
            provider.passphrase = row.at("passphrase");

            if (globalConfigCache_.encryptSensitiveConfigs) {
                if (!provider.apiKey.empty() && isValueEncrypted(provider.apiKey)) {
                    provider.apiKey = decryptValue(provider.apiKey);
                }
                if (!provider.apiSecret.empty() && isValueEncrypted(provider.apiSecret)) {
                    provider.apiSecret = decryptValue(provider.apiSecret);
                }
                if (!provider.passphrase.empty() && isValueEncrypted(provider.passphrase)) {
                    provider.passphrase = decryptValue(provider.passphrase);
                }
            }

            // Load other numeric fields
            provider.maxRequestsPerSecond = std::stoi(row.at("max_requests_per_second"));
            provider.maxRequestsPerMinute = std::stoi(row.at("max_requests_per_minute"));
            provider.maxRequestsPerHour = std::stoi(row.at("max_requests_per_hour"));
            provider.requestTimeout = std::chrono::milliseconds(std::stoll(row.at("request_timeout")));
            provider.maxRetries = std::stoi(row.at("max_retries"));
            provider.retryDelayBase = std::chrono::milliseconds(std::stoll(row.at("retry_delay_base")));
            provider.retryBackoffMultiplier = std::stod(row.at("retry_backoff_multiplier"));
            provider.minDataQualityThreshold = std::stod(row.at("min_data_quality_threshold"));
            provider.maxDataAge = std::chrono::minutes(std::stoll(row.at("max_data_age")));
            provider.latencyWarningThreshold = std::stod(row.at("latency_warning_threshold"));
            provider.latencyCriticalThreshold = std::stod(row.at("latency_critical_threshold"));
            provider.successRateWarningThreshold = std::stod(row.at("success_rate_warning_threshold"));
            provider.successRateCriticalThreshold = std::stod(row.at("success_rate_critical_threshold"));

            providerCache_[provider.providerId] = provider;
        }

        // Load global configuration
        globalConfigCache_ = loadGlobalConfigFromDatabase();

        cacheLastUpdated_ = std::chrono::system_clock::now();
        cacheValid_ = true;

    } catch (const std::exception& e) {
        std::cerr << "Failed to update configuration cache: " << e.what() << std::endl;
        cacheValid_ = false;
    }
}

bool ApiConfigurationManager::isCacheValid() const {
    auto now = std::chrono::system_clock::now();
    auto cacheAge = std::chrono::duration_cast<std::chrono::hours>(now - cacheLastUpdated_);
    return cacheValid_ && (cacheAge < globalConfigCache_.configCacheTimeout);
}

std::string ApiConfigurationManager::encryptValue(const std::string& value) const {
    // Simple XOR encryption for demonstration
    // In production, use proper encryption like AES
    if (encryptionKey_.empty()) {
        return value; // No encryption key set
    }

    std::string encrypted = value;
    for (size_t i = 0; i < encrypted.length(); ++i) {
        encrypted[i] ^= encryptionKey_[i % encryptionKey_.length()];
    }

    // Add prefix to identify encrypted values
    return "ENC:" + encrypted;
}

std::string ApiConfigurationManager::decryptValue(const std::string& encryptedValue) const {
    if (!isValueEncrypted(encryptedValue) || encryptionKey_.empty()) {
        return encryptedValue;
    }

    // Remove encryption prefix
    std::string encrypted = encryptedValue.substr(4);

    // XOR decrypt
    for (size_t i = 0; i < encrypted.length(); ++i) {
        encrypted[i] ^= encryptionKey_[i % encryptionKey_.length()];
    }

    return encrypted;
}

bool ApiConfigurationManager::isValueEncrypted(const std::string& value) const {
    return value.length() > 4 && value.substr(0, 4) == "ENC:";
}

// Static factory methods for common configurations
ApiProviderConfig ApiConfigurationManager::createCryptoCompareConfig(const std::string& apiKey) {
    ApiProviderConfig config("cryptocompare", "cryptocompare");
    config.apiKey = apiKey;
    config.baseUrl = "https://api.cryptocompare.com/data/v2";
    config.maxRequestsPerSecond = 10;
    config.maxRequestsPerMinute = 600;
    config.maxRequestsPerHour = 10000;
    config.requestTimeout = std::chrono::milliseconds(30000);
    config.supportedSymbols = {"BTC", "ETH", "ADA", "DOT", "LINK", "UNI", "AAVE", "SUSHI"};
    config.supportedTimeframes = {"1m", "5m", "15m", "1h", "4h", "1d"};
    return config;
}

GlobalApiConfig ApiConfigurationManager::createDevelopmentConfig() {
    GlobalApiConfig config;
    config.defaultHealthCheckInterval = std::chrono::seconds(30);
    config.defaultConnectionTestInterval = std::chrono::seconds(120);
    config.enableGlobalLogging = true;
    config.encryptSensitiveConfigs = false; // Easier for development
    config.auditConfigurationChanges = true;
    config.maxConcurrentConnections = 10;
    return config;
}

GlobalApiConfig ApiConfigurationManager::createProductionConfig() {
    GlobalApiConfig config;
    config.defaultHealthCheckInterval = std::chrono::seconds(60);
    config.defaultConnectionTestInterval = std::chrono::seconds(300);
    config.enableGlobalLogging = true;
    config.encryptSensitiveConfigs = true;
    config.auditConfigurationChanges = true;
    config.enableProviderFailover = true;
    config.maxConcurrentConnections = 100;
    config.enableHealthMonitoring = true;
    config.enablePerformanceMonitoring = true;
    config.enableAlertNotifications = true;
    return config;
}

GlobalApiConfig ApiConfigurationManager::loadGlobalConfigFromDatabase() const {
    GlobalApiConfig config;

    try {
        std::string query = "SELECT * FROM global_api_config WHERE config_id = 1 LIMIT 1";
        auto results = dbManager_.executeSelectQuery(query);

        if (!results.empty()) {
            const auto& row = results[0];
            config.defaultHealthCheckInterval = std::chrono::seconds(std::stoll(row.at("default_health_check_interval")));
            config.defaultConnectionTestInterval = std::chrono::seconds(std::stoll(row.at("default_connection_test_interval")));
            config.enableGlobalRateLimiting = (row.at("enable_global_rate_limiting") == "1");
            config.enableGlobalRetries = (row.at("enable_global_retries") == "1");
            config.enableGlobalLogging = (row.at("enable_global_logging") == "1");
            config.enableProviderFailover = (row.at("enable_provider_failover") == "1");
            config.maxFailoverAttempts = std::stoi(row.at("max_failover_attempts"));
            config.failoverCooldownPeriod = std::chrono::minutes(std::stoll(row.at("failover_cooldown_period")));
            config.globalDataQualityThreshold = std::stod(row.at("global_data_quality_threshold"));
            config.globalMaxDataAge = std::chrono::minutes(std::stoll(row.at("global_max_data_age")));
            config.enableDataValidation = (row.at("enable_data_validation") == "1");
            config.encryptSensitiveConfigs = (row.at("encrypt_sensitive_configs") == "1");
            config.configCacheTimeout = std::chrono::hours(std::stoll(row.at("config_cache_timeout")));
            config.auditConfigurationChanges = (row.at("audit_configuration_changes") == "1");
            config.maxConcurrentConnections = std::stoi(row.at("max_concurrent_connections"));
            config.connectionPoolTimeout = std::chrono::seconds(std::stoll(row.at("connection_pool_timeout")));
            config.enableConnectionPooling = (row.at("enable_connection_pooling") == "1");
            config.enableHealthMonitoring = (row.at("enable_health_monitoring") == "1");
            config.enablePerformanceMonitoring = (row.at("enable_performance_monitoring") == "1");
            config.enableAlertNotifications = (row.at("enable_alert_notifications") == "1");
            config.alertEmailRecipients = row.at("alert_email_recipients");
        }

    } catch (const std::exception& e) {
        std::cerr << "Failed to load global configuration from database: " << e.what() << std::endl;
        // Return default configuration
        config = createDevelopmentConfig();
    }

    return config;
}

void ApiConfigurationManager::applyEnvironmentDefaults() {
    std::map<std::string, std::string> defaults;

    switch (currentEnvironment_) {
        case Environment::DEVELOPMENT:
            defaults = getDevelopmentDefaults();
            break;
        case Environment::PRODUCTION:
            defaults = getProductionDefaults();
            break;
        case Environment::STAGING:
            // Use production defaults for staging
            defaults = getProductionDefaults();
            break;
    }

    for (const auto& pair : defaults) {
        if (!hasConfig(pair.first)) {
            setConfig(pair.first, pair.second, SecurityLevel::PUBLIC,
                     "Environment default for " + getEnvironmentString(), "system");
        }
    }
}

std::map<std::string, std::string> ApiConfigurationManager::getDevelopmentDefaults() const {
    return {
        {"logging.level", "DEBUG"},
        {"monitoring.enabled", "true"},
        {"encryption.enabled", "false"},
        {"cache.timeout", "300"}, // 5 minutes
        {"alerts.enabled", "false"}
    };
}

std::map<std::string, std::string> ApiConfigurationManager::getProductionDefaults() const {
    return {
        {"logging.level", "INFO"},
        {"monitoring.enabled", "true"},
        {"encryption.enabled", "true"},
        {"cache.timeout", "3600"}, // 1 hour
        {"alerts.enabled", "true"}
    };
}

} // namespace Config
} // namespace CryptoClaude