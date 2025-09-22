#pragma once
#include "IDataProvider.h"
#include "../Utils/HttpClient.h"
#include "../Utils/JsonHelper.h"
#include <memory>
#include <map>
#include <unordered_map>

namespace CryptoClaude {
namespace DataIngestion {

class NewsApiProvider : public ISentimentDataProvider {
public:
    explicit NewsApiProvider(const std::string& apiKey);
    ~NewsApiProvider() override = default;

    // IDataProvider interface
    std::string getName() const override { return "Crypto News API"; }
    std::string getDescription() const override { return "Crypto News API for sentiment analysis"; }
    bool isAvailable() const override;

    int getRateLimitPerMinute() const override { return 60; } // Typical rate limit
    int getRemainingRequests() const override { return m_remainingRequests; }
    std::chrono::system_clock::time_point getRateLimitReset() const override { return m_rateLimitReset; }

    std::string getLastError() const override { return m_lastError; }
    bool hasError() const override { return !m_lastError.empty(); }
    void clearError() override { m_lastError.clear(); }

    // ISentimentDataProvider interface
    std::vector<Database::Models::NewsArticle> getNews(
        const std::vector<std::string>& tickers,
        const std::string& startDate,
        const std::string& endDate,
        int maxArticles = 100) override;

    std::vector<Database::Models::SentimentData> getSentimentData(
        const std::vector<std::string>& tickers,
        const std::string& startDate,
        const std::string& endDate) override;

    std::vector<Database::Models::NewsSource> getNewsSources() override;
    bool isSourceSupported(const std::string& sourceName) override;

    double analyzeSentiment(const std::string& text) override;
    std::string getSentimentCategory(double score) override;

    std::vector<std::string> getSupportedTickers() override;
    bool isTickerSupported(const std::string& ticker) override;

    // NewsApiProvider specific methods
    void setApiKey(const std::string& apiKey);
    void setBatchSize(int coinsPerBatch, int daysPerBatch);
    void setPageNumber(int pageNumber) { m_currentPage = pageNumber; }

    // Batch processing for large datasets
    void fetchAllNewsSentiment(
        const std::vector<std::string>& allCoins,
        const std::string& endDate,
        int totalDays,
        std::function<void(const std::vector<Database::Models::SentimentData>&)> batchCallback);

private:
    std::string m_apiKey;
    std::unique_ptr<Utils::HttpClient> m_httpClient;
    std::string m_baseUrl;
    mutable std::string m_lastError;

    // Rate limiting
    int m_remainingRequests;
    std::chrono::system_clock::time_point m_rateLimitReset;
    int m_rateLimitDelayMs;

    // Batch processing parameters
    int m_coinsPerBatch;
    int m_daysPerBatch;
    int m_currentPage;
    int m_maxArticlesPerBatch;

    // Caching
    mutable std::vector<Database::Models::NewsSource> m_sourcesCache;
    mutable std::chrono::system_clock::time_point m_sourcesCacheTime;
    mutable std::vector<std::string> m_supportedTickersCache;
    mutable std::chrono::system_clock::time_point m_tickersCacheTime;

    // Helper methods
    std::string buildUrl(const std::string& endpoint, const std::map<std::string, std::string>& params = {});
    Utils::JsonHelper::Json fetchJson(const std::string& endpoint, const std::map<std::string, std::string>& params = {});

    // Date mapping utilities
    std::map<std::string, std::string> generateDateMappings(const std::string& endDate, int totalDays);
    std::string mapApiDateToSqlDate(const std::string& apiDate);

    // Data parsing
    std::vector<Database::Models::NewsArticle> parseNewsArticles(const Utils::JsonHelper::Json& json);
    std::vector<Database::Models::SentimentData> aggregateSentimentData(
        const std::vector<Database::Models::NewsArticle>& articles);

    std::vector<Database::Models::NewsSource> parseNewsSources(const Utils::JsonHelper::Json& json);

    // Sentiment analysis helpers
    double convertSentimentStringToScore(const std::string& sentiment);
    std::unordered_map<std::string, std::pair<int, double>> aggregateSentimentByKey(
        const std::vector<Database::Models::NewsArticle>& articles);

    // Data conversion
    Database::Models::NewsArticle jsonToNewsArticle(const Utils::JsonHelper::Json& item);
    Database::Models::SentimentData jsonToSentimentData(const Utils::JsonHelper::Json& item);
    Database::Models::NewsSource jsonToNewsSource(const Utils::JsonHelper::Json& item);

    // Validation and utilities
    bool validateTickers(const std::vector<std::string>& tickers) const;
    bool validateDateRange(const std::string& startDate, const std::string& endDate) const;
    std::vector<std::string> splitString(const std::string& str, char delimiter);
    std::string joinStrings(const std::vector<std::string>& strings, const std::string& delimiter);

    // Rate limiting and error handling
    void updateRateLimit(const Utils::HttpResponse& response);
    void handleApiError(const Utils::JsonHelper::Json& json);
    void waitForRateLimit();

    // Logging
    void logApiRequest(const std::string& endpoint, int responseCode) const;
    void logBatchProgress(int currentBatch, int totalBatches, const std::string& operation) const;
};

} // namespace DataIngestion
} // namespace CryptoClaude