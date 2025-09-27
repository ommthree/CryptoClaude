#pragma once

#include "SmartCacheManager.h"
#include "../Config/PersonalApiConfig.h"
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <memory>
#include <functional>
#include <queue>

namespace CryptoClaude {
namespace Data {

using namespace CryptoClaude::Config;

// API request priority levels
enum class RequestPriority {
    CRITICAL = 1,    // Essential for trading decisions
    HIGH = 2,        // Important for analysis
    MEDIUM = 3,      // Nice to have
    LOW = 4,         // Optional background updates
    BACKGROUND = 5   // Least important
};

// API request metadata
struct ApiRequest {
    std::string requestId;
    std::string provider;
    std::string endpoint;
    std::string symbol;
    RequestPriority priority;
    std::chrono::system_clock::time_point scheduledTime;
    std::chrono::system_clock::time_point createdTime;
    std::map<std::string, std::string> parameters;
    std::function<void(const std::string&)> successCallback;
    std::function<void(const std::string&)> errorCallback;
    int retryCount;
    bool canUseCache;

    ApiRequest() : priority(RequestPriority::MEDIUM), retryCount(0), canUseCache(true) {
        createdTime = std::chrono::system_clock::now();
        scheduledTime = createdTime;
    }
};

// Fallback data source configuration
struct FallbackSource {
    std::string sourceId;
    std::string sourceType; // "cache", "provider", "static", "interpolated"
    std::string provider; // For provider fallbacks
    double reliability; // 0.0 to 1.0
    std::chrono::minutes maxAge; // Maximum acceptable data age
    int priority; // Lower numbers = higher priority
    bool isAvailable;

    FallbackSource() : reliability(0.5), maxAge(std::chrono::minutes(60)),
                      priority(5), isAvailable(true) {}
};

// API limit status and tracking
struct ApiLimitStatus {
    std::string provider;
    int dailyLimit;
    int monthlyLimit;
    int dailyUsed;
    int monthlyUsed;
    int remainingDaily;
    int remainingMonthly;
    double utilizationPercent;
    std::chrono::system_clock::time_point lastRequest;
    std::chrono::system_clock::time_point dailyResetTime;
    std::chrono::system_clock::time_point monthlyResetTime;
    bool isTemporarilyBlocked;
    std::chrono::system_clock::time_point blockUntil;

    ApiLimitStatus() : dailyLimit(0), monthlyLimit(0), dailyUsed(0), monthlyUsed(0),
                      remainingDaily(0), remainingMonthly(0), utilizationPercent(0.0),
                      isTemporarilyBlocked(false) {}
};

// Graceful degradation strategies
enum class DegradationStrategy {
    CACHE_FIRST,        // Always try cache first
    PROVIDER_FALLBACK,  // Switch to alternative providers
    REDUCE_FREQUENCY,   // Reduce update frequency
    PRIORITIZE_CRITICAL, // Only serve critical requests
    INTERPOLATE_DATA,   // Use data interpolation/estimation
    STATIC_FALLBACK     // Use static/historical averages
};

// API limit handler with intelligent degradation
class ApiLimitHandler {
public:
    ApiLimitHandler(SmartCacheManager& cacheManager, PersonalUsageTracker& usageTracker);
    ~ApiLimitHandler() = default;

    // Initialization
    bool initialize();
    void shutdown();

    // Request management
    std::string submitRequest(const ApiRequest& request);
    bool cancelRequest(const std::string& requestId);
    std::vector<std::string> getPendingRequests() const;
    ApiRequest getRequestStatus(const std::string& requestId) const;

    // Batch request handling for efficiency
    std::vector<std::string> submitBatchRequest(const std::vector<ApiRequest>& requests);
    void optimizeBatchRequests(); // Combine compatible requests

    // Limit monitoring and management
    ApiLimitStatus getProviderStatus(const std::string& provider) const;
    std::map<std::string, ApiLimitStatus> getAllProviderStatuses() const;
    bool isProviderAvailable(const std::string& provider) const;
    std::chrono::milliseconds getNextAvailableTime(const std::string& provider) const;

    // Degradation strategy management
    void setDegradationStrategy(const std::string& provider, DegradationStrategy strategy);
    DegradationStrategy getDegradationStrategy(const std::string& provider) const;
    void enableGracefulDegradation(bool enable = true);

    // Fallback source management
    void addFallbackSource(const std::string& provider, const FallbackSource& source);
    void removeFallbackSource(const std::string& provider, const std::string& sourceId);
    std::vector<FallbackSource> getFallbackSources(const std::string& provider) const;

    // Smart request scheduling
    struct SchedulingOptions {
        bool respectRateLimits;
        bool optimizeForCritical;
        bool allowBatching;
        bool useOffPeakTimes;
        std::chrono::minutes maxDelay;

        SchedulingOptions() : respectRateLimits(true), optimizeForCritical(true),
                             allowBatching(true), useOffPeakTimes(true),
                             maxDelay(std::chrono::minutes(30)) {}
    };

    void setSchedulingOptions(const SchedulingOptions& options);
    void processRequestQueue();

    // Emergency mode for severe limit constraints
    void enableEmergencyMode(bool enable = true);
    bool isInEmergencyMode() const { return emergencyMode_; }

    struct EmergencyModeConfig {
        RequestPriority minPriority; // Only serve requests above this priority
        std::chrono::minutes maxCacheAge; // Accept older cached data
        bool disableNonCriticalUpdates;
        bool enableDataInterpolation;
        std::vector<std::string> criticalSymbols; // Only update these symbols

        EmergencyModeConfig() : minPriority(RequestPriority::HIGH),
                               maxCacheAge(std::chrono::minutes(120)),
                               disableNonCriticalUpdates(true),
                               enableDataInterpolation(true) {
            criticalSymbols = {"BTC", "ETH"}; // Most important symbols
        }
    };

    void setEmergencyModeConfig(const EmergencyModeConfig& config);

    // Data quality management during degradation
    struct QualityMetrics {
        double freshness; // How recent the data is (0.0 to 1.0)
        double accuracy; // Estimated accuracy (0.0 to 1.0)
        double completeness; // Data completeness (0.0 to 1.0)
        std::string source; // Where the data came from
        std::chrono::system_clock::time_point timestamp;

        QualityMetrics() : freshness(1.0), accuracy(1.0), completeness(1.0) {
            timestamp = std::chrono::system_clock::now();
        }
    };

    QualityMetrics assessDataQuality(const std::string& data, const std::string& source) const;

    // Intelligent data requests with fallback
    std::string requestDataWithFallback(const std::string& provider,
                                       const std::string& endpoint,
                                       const std::string& symbol,
                                       RequestPriority priority = RequestPriority::MEDIUM,
                                       bool allowCache = true);

    // Conservation strategies
    struct ConservationStrategy {
        std::string name;
        std::string description;
        double expectedSavings; // Percentage of API calls saved
        int priority; // Implementation priority

        ConservationStrategy() : expectedSavings(0.0), priority(5) {}
    };

    std::vector<ConservationStrategy> getRecommendedConservationStrategies() const;
    bool implementConservationStrategy(const std::string& strategyName);

    // Monitoring and alerting
    struct LimitAlert {
        std::string provider;
        std::string alertType; // "approaching", "exceeded", "reset"
        std::string message;
        double utilizationPercent;
        std::chrono::system_clock::time_point alertTime;
    };

    void setAlertCallback(std::function<void(const LimitAlert&)> callback);
    std::vector<LimitAlert> getRecentAlerts(int hours = 24) const;

    // Statistics and reporting
    struct UsageStatistics {
        std::map<std::string, int> requestsByProvider;
        std::map<std::string, int> requestsByPriority;
        std::map<std::string, int> fallbackUsage;
        int totalRequests;
        int cachedResponses;
        int failedRequests;
        double averageResponseTime;
        std::chrono::system_clock::time_point periodStart;
        std::chrono::system_clock::time_point periodEnd;

        UsageStatistics() : totalRequests(0), cachedResponses(0), failedRequests(0),
                           averageResponseTime(0.0) {}
    };

    UsageStatistics getUsageStatistics(std::chrono::hours period = std::chrono::hours(24)) const;
    void resetStatistics();

private:
    SmartCacheManager& cacheManager_;
    PersonalUsageTracker& usageTracker_;

    // Internal state
    std::map<std::string, DegradationStrategy> providerStrategies_;
    std::map<std::string, std::vector<FallbackSource>> fallbackSources_;
    std::map<std::string, ApiLimitStatus> providerStatuses_;

    // Request queue and processing
    std::priority_queue<ApiRequest, std::vector<ApiRequest>, std::function<bool(const ApiRequest&, const ApiRequest&)>> requestQueue_;
    std::map<std::string, ApiRequest> pendingRequests_;
    std::map<std::string, ApiRequest> completedRequests_;

    // Configuration
    SchedulingOptions schedulingOptions_;
    EmergencyModeConfig emergencyConfig_;
    bool gracefulDegradationEnabled_;
    bool emergencyMode_;

    // Monitoring
    std::function<void(const LimitAlert&)> alertCallback_;
    std::vector<LimitAlert> recentAlerts_;
    UsageStatistics currentStatistics_;

    // Thread safety
    mutable std::mutex handlerMutex_;

    // Private methods
    void updateProviderStatuses();
    bool canMakeRequest(const std::string& provider, RequestPriority priority) const;
    std::string selectBestProvider(const std::string& endpoint, const std::string& symbol) const;
    std::string executeRequest(const ApiRequest& request);

    // Fallback implementations
    std::string tryCache(const ApiRequest& request);
    std::string tryProviderFallback(const ApiRequest& request);
    std::string tryDataInterpolation(const ApiRequest& request);
    std::string tryStaticFallback(const ApiRequest& request);

    // Request optimization
    std::vector<ApiRequest> optimizeRequestBatch(const std::vector<ApiRequest>& requests);
    bool canBatchRequests(const ApiRequest& req1, const ApiRequest& req2) const;
    ApiRequest createBatchRequest(const std::vector<ApiRequest>& requests);

    // Emergency mode handling
    bool shouldServeInEmergencyMode(const ApiRequest& request) const;
    void activateEmergencyMode(const std::string& reason);
    void deactivateEmergencyMode();

    // Alert management
    void checkAndFireAlerts();
    void fireAlert(const LimitAlert& alert);

    // Statistics tracking
    void updateStatistics(const ApiRequest& request, const std::string& result, bool fromCache);

    // Utility methods
    std::string generateRequestId() const;
    RequestPriority determineRequestPriority(const std::string& endpoint, const std::string& symbol) const;
    bool isOffPeakTime() const;
};

// Request builder for common API operations
class ApiRequestBuilder {
public:
    ApiRequestBuilder();

    // Fluent interface for building requests
    ApiRequestBuilder& provider(const std::string& provider);
    ApiRequestBuilder& endpoint(const std::string& endpoint);
    ApiRequestBuilder& symbol(const std::string& symbol);
    ApiRequestBuilder& priority(RequestPriority priority);
    ApiRequestBuilder& parameter(const std::string& key, const std::string& value);
    ApiRequestBuilder& onSuccess(std::function<void(const std::string&)> callback);
    ApiRequestBuilder& onError(std::function<void(const std::string&)> callback);
    ApiRequestBuilder& allowCache(bool allow = true);
    ApiRequestBuilder& scheduleIn(std::chrono::minutes delay);

    ApiRequest build();

    // Common request types
    static ApiRequest createPriceRequest(const std::string& symbol, const std::string& provider = "");
    static ApiRequest createHistoricalRequest(const std::string& symbol, int days, const std::string& provider = "");
    static ApiRequest createNewsRequest(const std::string& symbol, const std::string& provider = "");

private:
    ApiRequest request_;
};

// Utility functions for degradation handling
class DegradationUtils {
public:
    // Data interpolation for missing data points
    static std::string interpolatePriceData(const std::vector<std::string>& knownDataPoints,
                                          const std::chrono::system_clock::time_point& targetTime);

    // Generate synthetic data based on historical patterns
    static std::string generateSyntheticData(const std::string& symbol,
                                           const std::string& dataType,
                                           const std::vector<std::string>& historicalData);

    // Quality assessment for degraded data
    static double assessDegradedDataQuality(const std::string& originalData,
                                          const std::string& degradedData);

    // Conservative estimation for missing data
    static std::string generateConservativeEstimate(const std::string& symbol,
                                                   const std::string& lastKnownValue,
                                                   std::chrono::minutes timeSinceLastUpdate);
};

} // namespace Data
} // namespace CryptoClaude