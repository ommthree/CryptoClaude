#pragma once

#include "../../Http/HttpClient.h"
#include "../../Database/Models/MarketData.h"
#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <chrono>

// Forward declaration for JSON
namespace nlohmann {
    class json;
}

namespace CryptoClaude {
namespace Data {
namespace Providers {

using namespace CryptoClaude::Http;
using namespace CryptoClaude::Database::Models;

// Data request configuration
struct MarketDataRequest {
    std::string symbol;
    std::string toSymbol = "USD";
    int limit = 100;
    std::string aggregate = "1";  // Aggregation period
    std::string exchange = "CCCAGG";  // Aggregate from all exchanges
    bool tryConversion = true;

    // Time range (optional)
    std::optional<std::chrono::system_clock::time_point> startTime;
    std::optional<std::chrono::system_clock::time_point> endTime;
};

// Provider response structure
struct ProviderResponse {
    bool success = false;
    std::string errorMessage;
    std::vector<MarketData> data;
    std::chrono::system_clock::time_point timestamp;
    double rateLimitRemaining = -1;
    int httpStatusCode = 0;
};

// CryptoCompare API provider interface
class ICryptoCompareProvider {
public:
    virtual ~ICryptoCompareProvider() = default;

    // Market data retrieval
    virtual ProviderResponse getHistoricalDaily(const MarketDataRequest& request) = 0;
    virtual ProviderResponse getHistoricalHourly(const MarketDataRequest& request) = 0;
    virtual ProviderResponse getCurrentPrice(const std::string& symbol, const std::string& toSymbol = "USD") = 0;
    virtual ProviderResponse getMultiplePrices(const std::vector<std::string>& symbols, const std::string& toSymbol = "USD") = 0;

    // Configuration
    virtual void setApiKey(const std::string& apiKey) = 0;
    virtual void setBaseUrl(const std::string& baseUrl) = 0;
    virtual bool isConfigured() const = 0;
};

// Main CryptoCompare provider implementation
class CryptoCompareProvider : public ICryptoCompareProvider {
private:
    std::shared_ptr<IHttpClient> httpClient_;
    std::string apiKey_;
    std::string baseUrl_;
    bool enableLogging_;

    // Rate limiting
    mutable std::chrono::system_clock::time_point lastRequestTime_;
    mutable int requestsThisSecond_;
    static constexpr int MAX_REQUESTS_PER_SECOND = 10;  // Conservative limit

    // Statistics
    mutable int totalRequests_;
    mutable int successfulRequests_;
    mutable int failedRequests_;

public:
    explicit CryptoCompareProvider(std::shared_ptr<IHttpClient> httpClient,
                                  const std::string& apiKey = "",
                                  const std::string& baseUrl = "https://api.cryptocompare.com/data/v2");

    virtual ~CryptoCompareProvider() = default;

    // Configuration
    void setApiKey(const std::string& apiKey) override { apiKey_ = apiKey; }
    void setBaseUrl(const std::string& baseUrl) override { baseUrl_ = baseUrl; }
    bool isConfigured() const override { return !apiKey_.empty(); }
    void enableLogging(bool enable) { enableLogging_ = enable; }

    // Market data retrieval
    ProviderResponse getHistoricalDaily(const MarketDataRequest& request) override;
    ProviderResponse getHistoricalHourly(const MarketDataRequest& request) override;
    ProviderResponse getCurrentPrice(const std::string& symbol, const std::string& toSymbol = "USD") override;
    ProviderResponse getMultiplePrices(const std::vector<std::string>& symbols, const std::string& toSymbol = "USD") override;

    // Advanced data requests
    ProviderResponse getOHLCData(const std::string& symbol, const std::string& timeframe = "day", int limit = 100);
    ProviderResponse getVolumeData(const std::string& symbol, int limit = 30);
    ProviderResponse getExchangeData(const std::string& symbol, const std::vector<std::string>& exchanges = {});

    // Statistics and monitoring
    struct ProviderStats {
        int totalRequests = 0;
        int successfulRequests = 0;
        int failedRequests = 0;
        double successRate = 0.0;
        std::chrono::milliseconds averageResponseTime = std::chrono::milliseconds(0);
        std::chrono::system_clock::time_point lastSuccessfulRequest;
        std::string lastError;
    };

    ProviderStats getStatistics() const;
    void resetStatistics();

    // Health check
    bool testConnection();

    // Data quality validation
    struct DataQuality {
        bool isValid = false;
        std::string errorMessage;
        int dataPoints = 0;
        double completenessRatio = 0.0;  // Percentage of expected data points received
        bool hasGaps = false;
        std::chrono::system_clock::time_point oldestData;
        std::chrono::system_clock::time_point newestData;
    };

    DataQuality validateDataQuality(const std::vector<MarketData>& data, const MarketDataRequest& originalRequest) const;

private:
    // Rate limiting
    void enforceRateLimit() const;

    // Request construction
    HttpRequest buildHistoricalRequest(const std::string& endpoint, const MarketDataRequest& request) const;
    HttpRequest buildPriceRequest(const std::string& endpoint, const std::string& symbol, const std::string& toSymbol) const;

    // Response parsing
    ProviderResponse parseHistoricalResponse(const HttpResponse& httpResponse, const MarketDataRequest& originalRequest) const;
    ProviderResponse parsePriceResponse(const HttpResponse& httpResponse, const std::string& symbol) const;
    ProviderResponse parseMultiplePriceResponse(const HttpResponse& httpResponse, const std::vector<std::string>& symbols) const;

    // Data transformation
    MarketData createMarketDataFromJson(const std::string& symbol, const nlohmann::json& jsonData) const;
    std::vector<MarketData> createMarketDataListFromJson(const std::string& symbol, const nlohmann::json& jsonArray) const;

    // Error handling
    ProviderResponse createErrorResponse(const std::string& message, int statusCode = 0) const;
    void logRequest(const HttpRequest& request) const;
    void logResponse(const HttpResponse& response, bool success) const;

    // Validation helpers
    bool isValidSymbol(const std::string& symbol) const;
    bool isValidTimeRange(const MarketDataRequest& request) const;
    void updateStatistics(bool success, std::chrono::milliseconds responseTime) const;
};

// Factory for creating providers with different configurations
class CryptoCompareProviderFactory {
public:
    static std::unique_ptr<CryptoCompareProvider> create(const std::string& apiKey);
    static std::unique_ptr<CryptoCompareProvider> createWithCustomClient(std::shared_ptr<IHttpClient> client, const std::string& apiKey);
    static std::unique_ptr<CryptoCompareProvider> createForTesting(); // Mock provider for tests
};

// Configuration helper
struct CryptoCompareConfig {
    std::string apiKey;
    std::string baseUrl = "https://api.cryptocompare.com/data/v2";
    bool enableLogging = false;
    int maxRequestsPerSecond = 10;
    std::chrono::seconds defaultTimeout = std::chrono::seconds(30);

    // Default symbols to track
    std::vector<std::string> defaultSymbols = {"BTC", "ETH", "ADA", "DOT", "LINK", "UNI", "AAVE", "SUSHI", "COMP", "MKR"};

    bool isValid() const {
        return !apiKey.empty() && !baseUrl.empty();
    }

    std::vector<std::string> getConfigurationErrors() const {
        std::vector<std::string> errors;
        if (apiKey.empty()) errors.push_back("API key is required");
        if (baseUrl.empty()) errors.push_back("Base URL is required");
        if (maxRequestsPerSecond <= 0) errors.push_back("Max requests per second must be positive");
        return errors;
    }
};

} // namespace Providers
} // namespace Data
} // namespace CryptoClaude