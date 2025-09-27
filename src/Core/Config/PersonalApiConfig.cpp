#include "PersonalApiConfig.h"
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <regex>
#include <thread>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace CryptoClaude {
namespace Config {

// Static member definitions
const char* PersonalApiConfig::PersonalApiKeys::CRYPTOCOMPARE_KEY_VAR = "CRYPTOCOMPARE_PERSONAL_API_KEY";
const char* PersonalApiConfig::PersonalApiKeys::NEWSAPI_KEY_VAR = "NEWSAPI_PERSONAL_API_KEY";
const char* PersonalApiConfig::PersonalApiKeys::ALPHAVANTAGE_KEY_VAR = "ALPHAVANTAGE_PERSONAL_API_KEY";

// PersonalApiConfig Implementation
PersonalApiConfig::PersonalApiConfig() {}

std::map<std::string, PersonalApiTier> PersonalApiConfig::getPersonalApiTiers() {
    std::map<std::string, PersonalApiTier> tiers;

    // CryptoCompare Free Tier
    tiers["cryptocompare"] = PersonalApiTier(
        "cryptocompare",
        PersonalRateLimits::CRYPTOCOMPARE_DAILY,
        PersonalRateLimits::CRYPTOCOMPARE_MONTHLY,
        PersonalRateLimits::CRYPTOCOMPARE_PER_SECOND,
        PersonalRateLimits::CRYPTOCOMPARE_PER_SECOND * 60,
        std::chrono::milliseconds(1000), // 1 second between requests
        true,
        "https://min-api.cryptocompare.com/data/"
    );

    // NewsAPI Free Tier
    tiers["newsapi"] = PersonalApiTier(
        "newsapi",
        PersonalRateLimits::NEWSAPI_DAILY,
        PersonalRateLimits::NEWSAPI_MONTHLY,
        PersonalRateLimits::NEWSAPI_PER_SECOND,
        PersonalRateLimits::NEWSAPI_PER_SECOND * 60,
        std::chrono::milliseconds(3600000), // 1 hour between requests (very conservative)
        true,
        "https://newsapi.org/v2/"
    );

    // Alpha Vantage Free Tier
    tiers["alphavantage"] = PersonalApiTier(
        "alphavantage",
        PersonalRateLimits::ALPHAVANTAGE_DAILY,
        PersonalRateLimits::ALPHAVANTAGE_DAILY * 30, // Approximate monthly
        PersonalRateLimits::ALPHAVANTAGE_PER_SECOND,
        PersonalRateLimits::ALPHAVANTAGE_PER_MINUTE,
        std::chrono::milliseconds(12000), // 12 seconds between requests
        true,
        "https://www.alphavantage.co/query"
    );

    return tiers;
}

ApiProviderConfig PersonalApiConfig::createPersonalCryptoCompareConfig(const std::string& apiKey) {
    ApiProviderConfig config("cryptocompare_personal", "cryptocompare");
    config.apiKey = apiKey;
    config.baseUrl = "https://min-api.cryptocompare.com/data/";

    // Personal tier rate limits
    config.maxRequestsPerSecond = PersonalRateLimits::CRYPTOCOMPARE_PER_SECOND;
    config.maxRequestsPerMinute = PersonalRateLimits::CRYPTOCOMPARE_PER_SECOND * 60;
    config.maxRequestsPerHour = PersonalRateLimits::CRYPTOCOMPARE_DAILY / 24;

    // Conservative timeouts for personal use
    config.requestTimeout = std::chrono::milliseconds(30000);
    config.maxRetries = 2;
    config.retryDelayBase = std::chrono::milliseconds(2000);
    config.retryBackoffMultiplier = 2.0;

    // Supported data
    config.supportedSymbols = PersonalUsageStrategy::getTargetSymbols();
    config.supportedTimeframes = {"1m", "5m", "15m", "1h", "1d"};
    config.supportsRealTime = true;
    config.supportsHistorical = true;

    // Quality settings
    config.minDataQualityThreshold = 0.95;
    config.maxDataAge = std::chrono::minutes(15);

    // Health monitoring thresholds
    config.latencyWarningThreshold = 1000.0;
    config.latencyCriticalThreshold = 3000.0;
    config.successRateWarningThreshold = 0.90;
    config.successRateCriticalThreshold = 0.75;

    config.priority = 1; // Highest priority
    config.isEnabled = true;

    return config;
}

ApiProviderConfig PersonalApiConfig::createPersonalNewsApiConfig(const std::string& apiKey) {
    ApiProviderConfig config("newsapi_personal", "newsapi");
    config.apiKey = apiKey;
    config.baseUrl = "https://newsapi.org/v2/";

    // Very conservative rate limits for news
    config.maxRequestsPerSecond = PersonalRateLimits::NEWSAPI_PER_SECOND;
    config.maxRequestsPerMinute = PersonalRateLimits::NEWSAPI_PER_SECOND * 60;
    config.maxRequestsPerHour = PersonalRateLimits::NEWSAPI_DAILY / 24;

    config.requestTimeout = std::chrono::milliseconds(45000);
    config.maxRetries = 1; // Minimize retries for news
    config.retryDelayBase = std::chrono::milliseconds(5000);

    config.supportedSymbols = PersonalUsageStrategy::getTargetSymbols();
    config.supportsRealTime = false; // News is not real-time critical
    config.supportsHistorical = true;

    config.minDataQualityThreshold = 0.80; // News can be lower quality
    config.maxDataAge = std::chrono::hours(6);

    config.priority = 3; // Lower priority
    config.isEnabled = true;

    return config;
}

ApiProviderConfig PersonalApiConfig::createPersonalAlphaVantageConfig(const std::string& apiKey) {
    ApiProviderConfig config("alphavantage_personal", "alphavantage");
    config.apiKey = apiKey;
    config.baseUrl = "https://www.alphavantage.co/query";

    // Conservative rate limits
    config.maxRequestsPerSecond = PersonalRateLimits::ALPHAVANTAGE_PER_SECOND;
    config.maxRequestsPerMinute = PersonalRateLimits::ALPHAVANTAGE_PER_MINUTE;
    config.maxRequestsPerHour = PersonalRateLimits::ALPHAVANTAGE_DAILY / 24;

    config.requestTimeout = std::chrono::milliseconds(60000); // 1 minute timeout
    config.maxRetries = 1;
    config.retryDelayBase = std::chrono::milliseconds(15000); // 15 second delay

    config.supportedSymbols = PersonalUsageStrategy::getTargetSymbols();
    config.supportedTimeframes = {"1min", "5min", "15min", "30min", "60min", "daily"};
    config.supportsRealTime = true;
    config.supportsHistorical = true;

    config.minDataQualityThreshold = 0.90;
    config.maxDataAge = std::chrono::minutes(30);

    config.priority = 2; // Medium priority
    config.isEnabled = true;

    return config;
}

GlobalApiConfig PersonalApiConfig::createPersonalGlobalConfig() {
    GlobalApiConfig config;

    // Conservative health check intervals
    config.defaultHealthCheckInterval = std::chrono::seconds(300); // 5 minutes
    config.defaultConnectionTestInterval = std::chrono::seconds(900); // 15 minutes

    // Enable essential features
    config.enableGlobalRateLimiting = true;
    config.enableGlobalRetries = true;
    config.enableGlobalLogging = true;

    // Failover settings for personal use
    config.enableProviderFailover = true;
    config.maxFailoverAttempts = 2; // Limited failover attempts
    config.failoverCooldownPeriod = std::chrono::minutes(10);

    // Data quality for personal trading
    config.globalDataQualityThreshold = 0.85;
    config.globalMaxDataAge = std::chrono::minutes(30);
    config.enableDataValidation = true;

    // Security settings
    config.encryptSensitiveConfigs = true;
    config.configCacheTimeout = std::chrono::hours(2);
    config.auditConfigurationChanges = true;

    // Performance settings for personal use
    config.maxConcurrentConnections = 5; // Limited connections
    config.connectionPoolTimeout = std::chrono::seconds(600);
    config.enableConnectionPooling = true;

    // Monitoring
    config.enableHealthMonitoring = true;
    config.enablePerformanceMonitoring = true;
    config.enableAlertNotifications = false; // Disable for personal use

    return config;
}

// PersonalApiKeys Implementation
std::string PersonalApiConfig::PersonalApiKeys::getApiKey(const std::string& provider) {
    const char* envVar = nullptr;

    if (provider == "cryptocompare") {
        envVar = CRYPTOCOMPARE_KEY_VAR;
    } else if (provider == "newsapi") {
        envVar = NEWSAPI_KEY_VAR;
    } else if (provider == "alphavantage") {
        envVar = ALPHAVANTAGE_KEY_VAR;
    }

    if (envVar) {
        const char* key = std::getenv(envVar);
        return key ? std::string(key) : std::string("");
    }

    return std::string("");
}

bool PersonalApiConfig::PersonalApiKeys::validateApiKeyFormat(const std::string& provider, const std::string& key) {
    if (key.empty()) return false;

    if (provider == "cryptocompare") {
        // CryptoCompare keys are typically 64-character hex strings
        std::regex pattern("^[a-fA-F0-9]{64}$");
        return std::regex_match(key, pattern);
    } else if (provider == "newsapi") {
        // NewsAPI keys are typically 32-character alphanumeric
        std::regex pattern("^[a-zA-Z0-9]{32}$");
        return std::regex_match(key, pattern);
    } else if (provider == "alphavantage") {
        // Alpha Vantage keys are typically mixed case alphanumeric
        std::regex pattern("^[a-zA-Z0-9]{8,32}$");
        return std::regex_match(key, pattern);
    }

    return false;
}

std::map<std::string, bool> PersonalApiConfig::PersonalApiKeys::checkAllApiKeys() {
    std::map<std::string, bool> status;

    std::vector<std::string> providers = {"cryptocompare", "newsapi", "alphavantage"};

    for (const auto& provider : providers) {
        std::string key = getApiKey(provider);
        status[provider] = !key.empty() && validateApiKeyFormat(provider, key);
    }

    return status;
}

// PersonalApiKeyManager Implementation
PersonalApiKeyManager::PersonalApiKeyManager() {}

bool PersonalApiKeyManager::loadApiKeysFromEnvironment() {
    std::vector<std::string> providers = {"cryptocompare", "newsapi", "alphavantage"};
    bool allLoaded = true;

    for (const auto& provider : providers) {
        std::string key = PersonalApiConfig::PersonalApiKeys::getApiKey(provider);
        if (!key.empty()) {
            apiKeys_[provider] = key;
            keyValidationStatus_[provider] = PersonalApiConfig::PersonalApiKeys::validateApiKeyFormat(provider, key);
        } else {
            keyValidationStatus_[provider] = false;
            allLoaded = false;
        }
    }

    return allLoaded;
}

bool PersonalApiKeyManager::setApiKey(const std::string& provider, const std::string& key) {
    if (PersonalApiConfig::PersonalApiKeys::validateApiKeyFormat(provider, key)) {
        apiKeys_[provider] = key;
        keyValidationStatus_[provider] = true;
        return true;
    }

    keyValidationStatus_[provider] = false;
    return false;
}

std::string PersonalApiKeyManager::getApiKey(const std::string& provider) const {
    auto it = apiKeys_.find(provider);
    return (it != apiKeys_.end()) ? it->second : std::string("");
}

bool PersonalApiKeyManager::hasApiKey(const std::string& provider) const {
    return apiKeys_.find(provider) != apiKeys_.end() && !apiKeys_.at(provider).empty();
}

bool PersonalApiKeyManager::validateAllKeys() const {
    for (const auto& pair : keyValidationStatus_) {
        if (!pair.second) return false;
    }
    return !keyValidationStatus_.empty();
}

std::vector<std::string> PersonalApiKeyManager::getMissingKeys() const {
    std::vector<std::string> missing;
    std::vector<std::string> providers = {"cryptocompare", "newsapi", "alphavantage"};

    for (const auto& provider : providers) {
        if (!hasApiKey(provider)) {
            missing.push_back(provider);
        }
    }

    return missing;
}

std::vector<std::string> PersonalApiKeyManager::getInvalidKeys() const {
    std::vector<std::string> invalid;

    for (const auto& pair : keyValidationStatus_) {
        if (!pair.second && hasApiKey(pair.first)) {
            invalid.push_back(pair.first);
        }
    }

    return invalid;
}

bool PersonalApiKeyManager::isKeySecurelyStored(const std::string& provider) const {
    // Check if key comes from environment variable (more secure)
    std::string envKey = PersonalApiConfig::PersonalApiKeys::getApiKey(provider);
    std::string storedKey = getApiKey(provider);
    return !envKey.empty() && envKey == storedKey;
}

void PersonalApiKeyManager::clearApiKeys() {
    apiKeys_.clear();
    keyValidationStatus_.clear();
}

std::vector<ApiProviderConfig> PersonalApiKeyManager::generatePersonalConfigurations() const {
    std::vector<ApiProviderConfig> configs;

    if (hasApiKey("cryptocompare")) {
        configs.push_back(PersonalApiConfig::createPersonalCryptoCompareConfig(getApiKey("cryptocompare")));
    }

    if (hasApiKey("newsapi")) {
        configs.push_back(PersonalApiConfig::createPersonalNewsApiConfig(getApiKey("newsapi")));
    }

    if (hasApiKey("alphavantage")) {
        configs.push_back(PersonalApiConfig::createPersonalAlphaVantageConfig(getApiKey("alphavantage")));
    }

    return configs;
}

bool PersonalApiKeyManager::applyPersonalConfiguration(ApiConfigurationManager& configManager) const {
    auto configs = generatePersonalConfigurations();
    bool allApplied = true;

    for (const auto& config : configs) {
        if (!configManager.addApiProvider(config)) {
            allApplied = false;
            std::cerr << "Failed to add provider: " << config.providerId << std::endl;
        }
    }

    // Apply personal global configuration
    configManager.setGlobalConfig(PersonalApiConfig::createPersonalGlobalConfig());

    return allApplied;
}

// PersonalUsageTracker Implementation
PersonalUsageTracker::PersonalUsageTracker() {
    // Initialize usage stats for all providers
    std::vector<std::string> providers = {"cryptocompare", "newsapi", "alphavantage"};
    for (const auto& provider : providers) {
        providerStats_[provider] = UsageStats();

        // Set reset times
        auto now = std::chrono::system_clock::now();
        auto daily = std::chrono::system_clock::from_time_t(
            std::chrono::system_clock::to_time_t(now + std::chrono::hours(24))
        );
        auto monthly = std::chrono::system_clock::from_time_t(
            std::chrono::system_clock::to_time_t(now + std::chrono::hours(24 * 30))
        );

        providerStats_[provider].dailyResetTime = daily;
        providerStats_[provider].monthlyResetTime = monthly;
    }
}

void PersonalUsageTracker::recordRequest(const std::string& provider, const std::string& endpoint) {
    auto now = std::chrono::system_clock::now();

    // Clean up old requests first
    cleanupOldRequests(provider);

    // Check for day/month rollover
    if (isNewDay(provider)) {
        resetDailyStats(provider);
    }
    if (isNewMonth(provider)) {
        resetMonthlyStats(provider);
    }

    // Record the request
    requestHistory_[provider].push_back(now);

    // Update stats
    updateUsageStats(provider);
}

bool PersonalUsageTracker::canMakeRequest(const std::string& provider) const {
    auto it = providerStats_.find(provider);
    if (it == providerStats_.end()) return false;

    const auto& stats = it->second;

    // Check daily and monthly limits
    return stats.remainingDaily > 0 && stats.remainingMonthly > 0;
}

int PersonalUsageTracker::getRemainingRequests(const std::string& provider, const std::string& period) const {
    auto it = providerStats_.find(provider);
    if (it == providerStats_.end()) return 0;

    const auto& stats = it->second;

    if (period == "daily") {
        return stats.remainingDaily;
    } else if (period == "monthly") {
        return stats.remainingMonthly;
    }

    return std::min(stats.remainingDaily, stats.remainingMonthly);
}

PersonalUsageTracker::UsageStats PersonalUsageTracker::getUsageStats(const std::string& provider) const {
    auto it = providerStats_.find(provider);
    if (it != providerStats_.end()) {
        return it->second;
    }
    return UsageStats();
}

std::map<std::string, PersonalUsageTracker::UsageStats> PersonalUsageTracker::getAllUsageStats() const {
    return providerStats_;
}

std::chrono::milliseconds PersonalUsageTracker::getMinDelayForNextRequest(const std::string& provider) const {
    auto tiers = PersonalApiConfig::getPersonalApiTiers();
    auto it = tiers.find(provider);

    if (it != tiers.end()) {
        return it->second.minRequestInterval;
    }

    return std::chrono::milliseconds(1000); // Default 1 second
}

bool PersonalUsageTracker::isWithinRateLimit(const std::string& provider) const {
    auto now = std::chrono::system_clock::now();
    auto it = providerStats_.find(provider);

    if (it == providerStats_.end()) return true;

    const auto& stats = it->second;
    auto minDelay = getMinDelayForNextRequest(provider);

    return (now - stats.lastRequest) >= minDelay;
}

void PersonalUsageTracker::waitForRateLimit(const std::string& provider) const {
    if (!isWithinRateLimit(provider)) {
        auto delay = getMinDelayForNextRequest(provider);
        auto it = providerStats_.find(provider);
        if (it != providerStats_.end()) {
            auto elapsed = std::chrono::system_clock::now() - it->second.lastRequest;
            auto remaining = delay - std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);
            if (remaining > std::chrono::milliseconds(0)) {
                std::this_thread::sleep_for(remaining);
            }
        }
    }
}

// Helper methods
void PersonalUsageTracker::updateUsageStats(const std::string& provider) {
    auto now = std::chrono::system_clock::now();
    auto& stats = providerStats_[provider];

    stats.lastRequest = now;
    stats.dailyRequests = requestHistory_[provider].size();
    stats.monthlyRequests = requestHistory_[provider].size(); // Simplified for this implementation

    // Calculate remaining based on provider limits
    auto tiers = PersonalApiConfig::getPersonalApiTiers();
    auto it = tiers.find(provider);
    if (it != tiers.end()) {
        stats.remainingDaily = std::max(0, it->second.dailyLimit - stats.dailyRequests);
        stats.remainingMonthly = std::max(0, it->second.monthlyLimit - stats.monthlyRequests);

        if (it->second.dailyLimit > 0) {
            stats.utilizationPercent = (double)stats.dailyRequests / it->second.dailyLimit * 100.0;
        }
    }
}

void PersonalUsageTracker::cleanupOldRequests(const std::string& provider) {
    auto now = std::chrono::system_clock::now();
    auto& history = requestHistory_[provider];

    // Remove requests older than 24 hours
    auto cutoff = now - std::chrono::hours(24);
    history.erase(
        std::remove_if(history.begin(), history.end(),
            [cutoff](const std::chrono::system_clock::time_point& time) {
                return time < cutoff;
            }),
        history.end()
    );
}

bool PersonalUsageTracker::isNewDay(const std::string& provider) const {
    auto now = std::chrono::system_clock::now();
    auto it = providerStats_.find(provider);
    return it != providerStats_.end() && now >= it->second.dailyResetTime;
}

bool PersonalUsageTracker::isNewMonth(const std::string& provider) const {
    auto now = std::chrono::system_clock::now();
    auto it = providerStats_.find(provider);
    return it != providerStats_.end() && now >= it->second.monthlyResetTime;
}

void PersonalUsageTracker::resetDailyStats(const std::string& provider) {
    auto& stats = providerStats_[provider];
    stats.dailyRequests = 0;
    stats.dailyResetTime += std::chrono::hours(24);

    // Update remaining counts
    updateUsageStats(provider);
}

void PersonalUsageTracker::resetMonthlyStats(const std::string& provider) {
    auto& stats = providerStats_[provider];
    stats.monthlyRequests = 0;
    stats.monthlyResetTime += std::chrono::hours(24 * 30);

    // Update remaining counts
    updateUsageStats(provider);
}

} // namespace Config
} // namespace CryptoClaude