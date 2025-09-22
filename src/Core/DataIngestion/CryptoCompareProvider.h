#pragma once
#include "IDataProvider.h"
#include "../Utils/HttpClient.h"
#include "../Utils/JsonHelper.h"
#include <memory>
#include <map>

namespace CryptoClaude {
namespace DataIngestion {

class CryptoCompareProvider : public IMarketDataProvider {
public:
    explicit CryptoCompareProvider(const std::string& apiKey);
    ~CryptoCompareProvider() override = default;

    // IDataProvider interface
    std::string getName() const override { return "CryptoCompare"; }
    std::string getDescription() const override { return "CryptoCompare API for cryptocurrency market data"; }
    bool isAvailable() const override;

    int getRateLimitPerMinute() const override { return 100; } // Default rate limit
    int getRemainingRequests() const override { return m_remainingRequests; }
    std::chrono::system_clock::time_point getRateLimitReset() const override { return m_rateLimitReset; }

    std::string getLastError() const override { return m_lastError; }
    bool hasError() const override { return !m_lastError.empty(); }
    void clearError() override { m_lastError.clear(); }

    // IMarketDataProvider interface
    std::vector<Database::Models::MarketData> getDailyData(
        const std::string& symbol,
        const std::string& currency = "USD",
        int days = 365) override;

    std::vector<Database::Models::MarketData> getHourlyData(
        const std::string& symbol,
        const std::string& currency = "USD",
        int hours = 24 * 30) override;

    std::vector<Database::Models::MarketData> getDailyDataBatch(
        const std::vector<std::string>& symbols,
        const std::string& currency = "USD",
        int days = 365) override;

    std::vector<Database::Models::MarketCapData> getTopCoinsByMarketCap(int count = 100) override;
    Database::Models::MarketCapData getMarketCap(const std::string& symbol) override;

    std::vector<std::string> getSupportedSymbols() override;
    bool isSymbolSupported(const std::string& symbol) override;

    std::chrono::system_clock::time_point getEarliestDataDate(const std::string& symbol) override;
    std::chrono::system_clock::time_point getLatestDataDate(const std::string& symbol) override;

    // CryptoCompare specific methods
    void setApiKey(const std::string& apiKey);
    void setRateLimitDelay(int delayMs) { m_rateLimitDelayMs = delayMs; }

private:
    std::string m_apiKey;
    std::unique_ptr<Utils::HttpClient> m_httpClient;
    std::string m_baseUrl;
    mutable std::string m_lastError;

    // Rate limiting
    int m_remainingRequests;
    std::chrono::system_clock::time_point m_rateLimitReset;
    int m_rateLimitDelayMs;

    // Caching
    mutable std::map<std::string, std::vector<std::string>> m_symbolCache;
    mutable std::chrono::system_clock::time_point m_symbolCacheTime;

    // Helper methods
    std::string buildUrl(const std::string& endpoint, const std::map<std::string, std::string>& params = {});
    Utils::JsonHelper::Json fetchJson(const std::string& endpoint, const std::map<std::string, std::string>& params = {});

    std::vector<Database::Models::MarketData> parseMarketData(
        const Utils::JsonHelper::Json& json,
        const std::string& symbol);

    std::vector<Database::Models::MarketCapData> parseMarketCapData(const Utils::JsonHelper::Json& json);

    void updateRateLimit(const Utils::HttpResponse& response);
    void handleApiError(const Utils::JsonHelper::Json& json);
    void waitForRateLimit();

    // Data conversion helpers
    Database::Models::MarketData jsonToMarketData(const Utils::JsonHelper::Json& item, const std::string& symbol);
    Database::Models::MarketCapData jsonToMarketCap(const Utils::JsonHelper::Json& item);

    // Validation
    bool validateSymbol(const std::string& symbol) const;
    bool validateCurrency(const std::string& currency) const;
    bool validateTimeRange(int days) const;

    // Logging
    void logApiRequest(const std::string& endpoint, int responseCode) const;
};

} // namespace DataIngestion
} // namespace CryptoClaude