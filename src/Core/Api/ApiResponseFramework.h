#pragma once

#include "../Http/HttpClient.h"
#include "../Json/JsonParser.h"
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <memory>
#include <optional>
#include <functional>
#include <queue>
#include <mutex>
#include <thread>

namespace CryptoClaude {
namespace Api {

// ================================
// Core Alert System
// ================================

enum class AlertType {
    INFO,       // Normal operation info
    WARNING,    // Non-critical issues
    ERROR,      // Serious issues requiring attention
    CRITICAL    // System-stopping issues
};

struct Alert {
    std::string id;
    AlertType type;
    std::string message;
    std::string source;
    std::chrono::system_clock::time_point timestamp;
};

class AlertManager {
private:
    std::vector<Alert> alerts_;
    mutable std::mutex mutex_;

public:
    void addAlert(AlertType type, const std::string& message, const std::string& source);
    std::vector<Alert> getRecentAlerts(std::chrono::minutes timeWindow) const;
    bool hasCriticalAlerts(std::chrono::minutes timeWindow) const;
    void clearOldAlerts(std::chrono::hours maxAge);

private:
    std::string generateAlertId();
};

// ================================
// Response Status and Types
// ================================

enum class ResponseStatus {
    SUCCESS,
    PARTIAL_SUCCESS,    // Some data retrieved, some failed
    RETRY_NEEDED,       // Temporary failure, retry recommended
    RATE_LIMITED,       // Hit rate limit, backoff needed
    CLIENT_ERROR,       // 4xx - client side issue, don't retry
    SERVER_ERROR,       // 5xx - server side issue, retry possible
    NETWORK_ERROR,      // Connection/timeout issues
    DATA_STALE,         // Data retrieved but too old
    VALIDATION_ERROR,   // Data format issues
    FATAL_ERROR         // Unrecoverable error, stop processing
};

enum class ApiProvider {
    CRYPTO_COMPARE,
    NEWS_API,
    UNKNOWN
};

struct ProcessedResponse {
    Http::HttpResponse originalResponse;
    Json::JsonValue parsedData;
    ResponseStatus status;
    std::optional<std::string> errorMessage;
    ApiProvider provider;
    std::string endpoint;
    std::chrono::system_clock::time_point processedAt;
    std::optional<std::chrono::system_clock::time_point> dataTimestamp;
};

struct ApiResponse {
    ResponseStatus status;
    std::optional<std::string> errorMessage;
    Json::JsonValue processedData;
    ApiProvider provider;
    std::string url;
    int httpStatusCode;
    std::chrono::system_clock::time_point requestedAt;
    std::chrono::milliseconds responseTime{0};
};

// ================================
// Conservative Rate Limiter
// ================================

struct RateLimitConfig {
    int maxRequestsPerMinute = 50;     // Conservative default
    bool enableAdaptiveScaling = true;
    double adaptiveScaleFactor = 0.8;  // Scale down to 80% on failures (conservative)
    int minRequestsPerMinute = 10;     // Never go below this
};

class RateLimiter {
private:
    RateLimitConfig config_;
    std::shared_ptr<AlertManager> alertManager_;
    mutable std::mutex mutex_;

    // Token bucket state
    double tokens_;
    std::chrono::steady_clock::time_point lastRefill_;
    int currentLimit_;

    // Adaptive learning
    std::vector<std::chrono::system_clock::time_point> recentRequests_;
    int recentFailures_ = 0;
    int consecutiveSuccesses_ = 0;

public:
    explicit RateLimiter(const RateLimitConfig& config, std::shared_ptr<AlertManager> alertManager);

    bool canMakeRequest();
    std::chrono::milliseconds getRetryDelay();
    void recordSuccess();
    void recordFailure();
    int getCurrentLimit() const { return currentLimit_; }

private:
    void refillTokens();
    void recordRequest();
};

// ================================
// Data Freshness Configuration
// ================================

struct DataFreshnessConfig {
    std::chrono::hours maxMarketDataAge{24};     // Max 1 day for market data
    std::chrono::hours maxNewsDataAge{168};      // Max 7 days for news
    bool alertOnStaleData = true;
};

class ResponseProcessor {
private:
    DataFreshnessConfig config_;
    std::shared_ptr<AlertManager> alertManager_;

public:
    explicit ResponseProcessor(const DataFreshnessConfig& config, std::shared_ptr<AlertManager> alertManager);

    ProcessedResponse processResponse(const Http::HttpResponse& httpResponse,
                                    ApiProvider provider,
                                    const std::string& endpoint);

    bool isDataStale(const ProcessedResponse& response);

private:
    void validateProviderResponse(ProcessedResponse& response);
    void checkDataFreshness(ProcessedResponse& response);
    void generateAlertsForResponse(const ProcessedResponse& response);
};

// ================================
// Main API Client with Alert-and-Stop
// ================================

class ApiClient {
private:
    std::shared_ptr<Http::IHttpClient> httpClient_;
    std::shared_ptr<RateLimiter> rateLimiter_;
    std::shared_ptr<ResponseProcessor> processor_;
    std::shared_ptr<AlertManager> alertManager_;

public:
    ApiClient(std::shared_ptr<Http::IHttpClient> httpClient,
              std::shared_ptr<RateLimiter> rateLimiter,
              std::shared_ptr<ResponseProcessor> processor,
              std::shared_ptr<AlertManager> alertManager);

    ApiResponse makeRequest(const std::string& url, ApiProvider provider);
    bool isHealthy() const;
    std::vector<Alert> getRecentAlerts() const;
};

// ================================
// Provider-Specific Adapters
// ================================

class CryptoCompareAdapter {
private:
    std::shared_ptr<ApiClient> client_;
    std::string apiKey_;

public:
    CryptoCompareAdapter(std::shared_ptr<ApiClient> client, const std::string& apiKey);

    ApiResponse getCurrentPrice(const std::string& fromSymbol, const std::string& toSymbol);
    ApiResponse getHistoricalData(const std::string& symbol, int days);
};

class NewsApiAdapter {
private:
    std::shared_ptr<ApiClient> client_;
    std::string apiKey_;

public:
    NewsApiAdapter(std::shared_ptr<ApiClient> client, const std::string& apiKey);

    ApiResponse getTopHeadlines(const std::string& category = "business", const std::string& country = "us");
    ApiResponse searchNews(const std::string& query, const std::string& sortBy = "publishedAt");
};

} // namespace Api
} // namespace CryptoClaude