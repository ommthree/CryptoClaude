#pragma once

#include "ApiConfigurationManager.h"
#include <chrono>
#include <map>

namespace CryptoClaude {
namespace Config {

// Personal-use API tier configuration for free APIs
struct PersonalApiTier {
    std::string provider;
    int dailyLimit;
    int monthlyLimit;
    int requestsPerSecond;
    int requestsPerMinute;
    std::chrono::milliseconds minRequestInterval;
    bool requiresApiKey;
    std::string baseUrl;

    PersonalApiTier() = default;
    PersonalApiTier(const std::string& p, int daily, int monthly, int rps, int rpm,
                   std::chrono::milliseconds interval, bool requiresKey, const std::string& url)
        : provider(p), dailyLimit(daily), monthlyLimit(monthly),
          requestsPerSecond(rps), requestsPerMinute(rpm),
          minRequestInterval(interval), requiresApiKey(requiresKey), baseUrl(url) {}
};

// Personal trading API configuration optimized for cost-effective trading
class PersonalApiConfig {
public:
    PersonalApiConfig();
    ~PersonalApiConfig() = default;

    // Get predefined personal-use API configurations
    static std::map<std::string, PersonalApiTier> getPersonalApiTiers();

    // Create optimized configurations for personal use
    static ApiProviderConfig createPersonalCryptoCompareConfig(const std::string& apiKey);
    static ApiProviderConfig createPersonalNewsApiConfig(const std::string& apiKey);
    static ApiProviderConfig createPersonalAlphaVantageConfig(const std::string& apiKey);

    // Personal-use global configuration
    static GlobalApiConfig createPersonalGlobalConfig();

    // Rate limiting configurations for free tiers
    struct PersonalRateLimits {
        // CryptoCompare Free: 100,000 requests/month (~3,225/day)
        static constexpr int CRYPTOCOMPARE_MONTHLY = 100000;
        static constexpr int CRYPTOCOMPARE_DAILY = 3225;
        static constexpr int CRYPTOCOMPARE_PER_SECOND = 1;

        // NewsAPI Free: 1,000 requests/month (~33/day)
        static constexpr int NEWSAPI_MONTHLY = 1000;
        static constexpr int NEWSAPI_DAILY = 33;
        static constexpr int NEWSAPI_PER_SECOND = 1;

        // Alpha Vantage Free: 500 requests/day
        static constexpr int ALPHAVANTAGE_DAILY = 500;
        static constexpr int ALPHAVANTAGE_PER_MINUTE = 5;
        static constexpr int ALPHAVANTAGE_PER_SECOND = 1;
    };

    // Expanded universe strategy for proper diversification
    struct PersonalUsageStrategy {
        // Expanded target universe for diversification and pair formation
        static std::vector<std::string> getTargetSymbols() {
            return getTradingUniverse(); // 50-100 most liquid coins
        }

        // Get tiered trading universe based on market cap and liquidity
        static std::vector<std::string> getTradingUniverse();
        static std::vector<std::string> getTier1Coins();  // Top 20 - full data
        static std::vector<std::string> getTier2Coins();  // 21-50 - 1yr data
        static std::vector<std::string> getTier3Coins();  // 51-100 - 6mo data

        // Tiered historical data periods
        static constexpr int TIER1_HISTORICAL_DAYS = 730; // 2 years for top 20
        static constexpr int TIER2_HISTORICAL_DAYS = 365; // 1 year for mid caps
        static constexpr int TIER3_HISTORICAL_DAYS = 180; // 6 months for others

        // Smart update frequencies to handle larger universe
        static constexpr int PRICE_UPDATE_MINUTES = 30;    // Every 30 minutes (spread load)
        static constexpr int NEWS_UPDATE_HOURS = 8;        // Every 8 hours (conservation)
        static constexpr int SENTIMENT_UPDATE_HOURS = 12;  // Every 12 hours

        // Optimized batch sizes for 50-100 coins
        static constexpr int PRICE_BATCH_SIZE = 10;        // 10 symbols per request
        static constexpr int NEWS_BATCH_SIZE = 1;          // 1 symbol per request (due to limits)
        static constexpr int HISTORICAL_BATCH_SIZE = 5;    // 5 symbols per historical batch

        // API request distribution for expanded universe
        static constexpr int TIER1_NEWS_WEIGHT = 50;       // 50% of news requests
        static constexpr int TIER2_NEWS_WEIGHT = 35;       // 35% of news requests
        static constexpr int TIER3_NEWS_WEIGHT = 15;       // 15% of news requests
    };

    // Environment variable configuration for personal API keys
    struct PersonalApiKeys {
        static const char* CRYPTOCOMPARE_KEY_VAR;
        static const char* NEWSAPI_KEY_VAR;
        static const char* ALPHAVANTAGE_KEY_VAR;

        static std::string getApiKey(const std::string& provider);
        static bool validateApiKeyFormat(const std::string& provider, const std::string& key);
        static std::map<std::string, bool> checkAllApiKeys();
    };

    // Personal trading configuration optimizations
    struct PersonalTradingConfig {
        // Portfolio size considerations for personal use
        static constexpr double MAX_POSITION_SIZE_USD = 500.0;
        static constexpr double MAX_DAILY_VOLUME_USD = 2000.0;
        static constexpr int MAX_POSITIONS = 5;

        // Risk management for personal accounts
        static constexpr double POSITION_RISK_PERCENT = 0.02; // 2% per position
        static constexpr double PORTFOLIO_RISK_PERCENT = 0.05; // 5% total portfolio
        static constexpr double STOP_LOSS_PERCENT = 0.03; // 3% stop loss

        // ML validation thresholds for personal use
        static constexpr double MIN_PREDICTION_ACCURACY = 0.55; // 55% minimum
        static constexpr double MIN_SHARPE_RATIO = 0.5;
        static constexpr int MIN_HISTORICAL_SAMPLES = 100;
    };
};

// Personal API key manager with secure environment variable handling
class PersonalApiKeyManager {
public:
    PersonalApiKeyManager();
    ~PersonalApiKeyManager() = default;

    // API key management
    bool loadApiKeysFromEnvironment();
    bool setApiKey(const std::string& provider, const std::string& key);
    std::string getApiKey(const std::string& provider) const;
    bool hasApiKey(const std::string& provider) const;

    // Validation
    bool validateAllKeys() const;
    std::vector<std::string> getMissingKeys() const;
    std::vector<std::string> getInvalidKeys() const;

    // Security
    bool isKeySecurelyStored(const std::string& provider) const;
    void clearApiKeys(); // For security on shutdown

    // Configuration generation
    std::vector<ApiProviderConfig> generatePersonalConfigurations() const;
    bool applyPersonalConfiguration(ApiConfigurationManager& configManager) const;

private:
    std::map<std::string, std::string> apiKeys_;
    std::map<std::string, bool> keyValidationStatus_;

    // Security helpers
    bool validateEnvironmentKeyFormat(const std::string& key) const;
    std::string sanitizeApiKey(const std::string& key) const;
};

// Personal usage tracker for API rate limiting
class PersonalUsageTracker {
public:
    PersonalUsageTracker();
    ~PersonalUsageTracker() = default;

    // Usage tracking
    void recordRequest(const std::string& provider, const std::string& endpoint = "");
    bool canMakeRequest(const std::string& provider) const;
    int getRemainingRequests(const std::string& provider, const std::string& period = "daily") const;

    // Usage statistics
    struct UsageStats {
        int dailyRequests = 0;
        int monthlyRequests = 0;
        int remainingDaily = 0;
        int remainingMonthly = 0;
        double utilizationPercent = 0.0;
        std::chrono::system_clock::time_point lastRequest;
        std::chrono::system_clock::time_point dailyResetTime;
        std::chrono::system_clock::time_point monthlyResetTime;
    };

    UsageStats getUsageStats(const std::string& provider) const;
    std::map<std::string, UsageStats> getAllUsageStats() const;

    // Rate limiting
    std::chrono::milliseconds getMinDelayForNextRequest(const std::string& provider) const;
    bool isWithinRateLimit(const std::string& provider) const;
    void waitForRateLimit(const std::string& provider) const;

    // Usage optimization
    struct OptimizationSuggestion {
        std::string provider;
        std::string suggestion;
        double potentialSaving; // Percentage of daily limit
        int priority; // 1-5, 1 being highest
    };

    std::vector<OptimizationSuggestion> getUsageOptimizationSuggestions() const;

    // Persistence
    bool saveUsageStats(const std::string& filePath) const;
    bool loadUsageStats(const std::string& filePath);
    void resetUsageStats();

private:
    std::map<std::string, UsageStats> providerStats_;
    std::map<std::string, std::vector<std::chrono::system_clock::time_point>> requestHistory_;

    // Helper methods
    void updateUsageStats(const std::string& provider);
    void cleanupOldRequests(const std::string& provider);
    bool isNewDay(const std::string& provider) const;
    bool isNewMonth(const std::string& provider) const;
    void resetDailyStats(const std::string& provider);
    void resetMonthlyStats(const std::string& provider);
};

} // namespace Config
} // namespace CryptoClaude