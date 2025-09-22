#pragma once
#include "../Models/SentimentData.h"
#include "../DatabaseManager.h"
#include <vector>
#include <optional>
#include <chrono>
#include <string>
#include <memory>
#include <map>

namespace CryptoClaude {
namespace Database {
namespace Repositories {

using CryptoClaude::Database::StatementWrapper;

class SentimentRepository {
private:
    DatabaseManager& dbManager_;

    // Prepared statement cache for bulk operations
    mutable std::map<std::string, std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>> preparedStatements_;

    // Helper methods
    sqlite3_stmt* getOrCreatePreparedStatement(const std::string& key, const std::string& sql) const;
    Models::SentimentData mapRowToSentimentData(StatementWrapper& stmt) const;
    Models::NewsSource mapRowToNewsSource(StatementWrapper& stmt) const;
    Models::NewsArticle mapRowToNewsArticle(StatementWrapper& stmt) const;
    Models::AggregatedSentiment mapRowToAggregatedSentiment(StatementWrapper& stmt) const;

    // Transaction wrapper (automatic transaction handling)
    template<typename Func>
    bool executeInTransaction(Func operation, const std::string& operationName = "");

    // Error handling
    void logError(const std::string& operation, const std::string& error) const;
    std::string formatError(const std::string& operation, const std::string& detail) const;

public:
    explicit SentimentRepository(DatabaseManager& dbManager);
    ~SentimentRepository();

    // === SENTIMENT DATA OPERATIONS ===

    // Basic CRUD
    bool insertSentimentData(const Models::SentimentData& data);
    std::optional<Models::SentimentData> getSentimentDataById(const std::string& ticker, const std::string& source, const std::string& date);
    bool updateSentimentData(const Models::SentimentData& data);
    bool deleteSentimentData(const std::string& ticker, const std::string& source, const std::string& date);

    // Additional methods used in implementation
    std::vector<Models::SentimentData> getSentimentByTicker(const std::string& ticker, int limit = 0);
    std::optional<Models::SentimentData> getLatestSentimentForTicker(const std::string& ticker);

    // Essential domain methods (moderate specialization)
    std::vector<Models::SentimentData> getByTicker(const std::string& ticker, int limit = 100);
    std::vector<Models::SentimentData> getByDateRange(
        const std::chrono::system_clock::time_point& start,
        const std::chrono::system_clock::time_point& end);
    std::vector<Models::SentimentData> getLatestForAllTickers();
    std::optional<Models::SentimentData> getLatestForTicker(const std::string& ticker);

    // Aggregation methods (efficient in SQL)
    std::map<std::string, std::pair<int, double>> getAggregatedSentiment(
        const std::vector<std::string>& tickers, const std::string& date);
    double getAverageSentimentForTicker(const std::string& ticker, int days);
    std::vector<std::pair<std::string, double>> getTopSentimentTickers(int limit, bool positive = true);

    // Time-based sentiment analysis
    std::vector<Models::SentimentData> getSentimentTrend(const std::string& ticker, int days);
    double getSentimentVolatility(const std::string& ticker, int days);

    // === NEWS SOURCE OPERATIONS ===

    // Basic CRUD
    bool insertNewsSource(const Models::NewsSource& source);
    std::optional<Models::NewsSource> getNewsSourceById(int sourceId);
    bool updateNewsSource(const Models::NewsSource& source);
    bool deleteNewsSource(int sourceId);

    // Essential domain methods
    std::vector<Models::NewsSource> getActiveNewsSources();
    std::vector<Models::NewsSource> getHighPriorityNewsSources();
    std::vector<Models::NewsSource> getSourcesNeedingUpdate();

    // === NEWS ARTICLE OPERATIONS ===

    // Basic CRUD
    bool insertNewsArticle(const Models::NewsArticle& article);
    std::optional<Models::NewsArticle> getArticleById(int articleId);
    bool updateNewsArticle(const Models::NewsArticle& article);
    bool deleteArticle(int articleId);

    // Essential domain methods
    std::vector<Models::NewsArticle> getUnprocessedArticles(int limit = 100);
    std::vector<Models::NewsArticle> getArticlesByTicker(const std::string& ticker, int limit = 50);
    std::vector<Models::NewsArticle> getArticlesByDateRange(
        const std::chrono::system_clock::time_point& start,
        const std::chrono::system_clock::time_point& end);

    // Sentiment-specific queries
    std::vector<Models::NewsArticle> getPositiveArticles(const std::string& ticker = "", double threshold = 0.1);
    std::vector<Models::NewsArticle> getNegativeArticles(const std::string& ticker = "", double threshold = -0.1);
    std::vector<Models::NewsArticle> getHighConfidenceArticles(double confidenceThreshold = 0.7);

    // === AGGREGATED SENTIMENT OPERATIONS ===

    // Basic CRUD for aggregated sentiment
    bool insertAggregatedSentiment(const Models::AggregatedSentiment& aggSentiment);
    std::optional<Models::AggregatedSentiment> getAggregatedSentiment(
        const std::string& ticker, const std::string& date);
    bool updateAggregatedSentiment(const Models::AggregatedSentiment& aggSentiment);

    // Analysis methods
    std::vector<Models::AggregatedSentiment> getSignificantSentimentDays(
        const std::string& ticker, double sentimentThreshold = 0.2);
    std::vector<Models::AggregatedSentiment> getHighVolatilitySentimentDays(
        const std::string& ticker, double volatilityThreshold = 0.3);

    // === BULK OPERATIONS ===

    bool insertSentimentBatch(const std::vector<Models::SentimentData>& dataList);
    bool insertNewsSourceBatch(const std::vector<Models::NewsSource>& sources);
    bool insertArticleBatch(const std::vector<Models::NewsArticle>& articles);

    // Batch processing for sentiment analysis
    bool processPendingArticles(int batchSize = 100);
    bool generateDailyAggregatedSentiment(const std::string& date);

    // === SENTIMENT INTEGRATION ===

    // Integration with market data
    bool mergeSentimentWithMarketData();
    bool updateMarketDataSentimentMetrics();

    // Date mapping for API integration
    bool insertDateMapping(const std::string& apiDate, const std::string& sqlDate);
    std::map<std::string, std::string> getDateMappings();
    std::map<std::string, std::string> generateAndStoreDateMappings(
        const std::string& endDate, int totalDays);

    // === ANALYTICS & REPORTING ===

    // Sentiment analysis metrics
    struct SentimentMetrics {
        int totalArticles;
        double averageSentiment;
        double sentimentVolatility;
        int positiveDays;
        int negativeDays;
        int neutralDays;
        double strongestPositive;
        double strongestNegative;
    };

    SentimentMetrics getSentimentMetrics(const std::string& ticker, int days);
    std::vector<std::pair<std::string, SentimentMetrics>> getAllTickerSentimentMetrics(int days);

    // Source reliability analysis
    std::vector<std::pair<std::string, double>> getSourceReliabilityScores();
    std::vector<Models::NewsSource> getTopPerformingSources(int limit = 10);

    // === MAINTENANCE & UTILITY ===

    bool clearStaleData(const std::chrono::system_clock::time_point& cutoffDate);
    bool optimizeSentimentTables();
    bool rebuildSentimentAggregations();

    size_t getSentimentDataCount();
    size_t getNewsSourceCount();
    size_t getNewsArticleCount();
    std::vector<std::string> getAllTrackedTickers();

    // === ERROR REPORTING ===

    std::string getLastError() const { return lastError_; }
    void clearLastError() { lastError_.clear(); }

private:
    mutable std::string lastError_; // Detailed error for debugging

    // Utility functions
    int64_t timeToUnix(const std::chrono::system_clock::time_point& tp) const;
    std::chrono::system_clock::time_point unixToTime(int64_t unixTime) const;
    double convertSentimentStringToScore(const std::string& sentiment) const;
    std::string convertSentimentScoreToString(double score) const;

    // SQL constants for prepared statements
    static const std::string SQL_INSERT_SENTIMENT;
    static const std::string SQL_INSERT_NEWS_SOURCE;
    static const std::string SQL_INSERT_ARTICLE;
    static const std::string SQL_SELECT_BY_TICKER;
    static const std::string SQL_SELECT_LATEST_FOR_TICKER;
    static const std::string SQL_SELECT_AGGREGATED_SENTIMENT;
    static const std::string SQL_UPDATE_SENTIMENT;
    static const std::string SQL_INSERT_AGGREGATED_SENTIMENT;
};

// Template implementation for transaction wrapper
template<typename Func>
bool SentimentRepository::executeInTransaction(Func operation, const std::string& operationName) {
    if (!dbManager_.beginTransaction()) {
        logError(operationName.empty() ? "Transaction" : operationName,
                "Failed to begin transaction: " + dbManager_.getLastError());
        return false;
    }

    try {
        bool result = operation();

        if (result) {
            if (!dbManager_.commitTransaction()) {
                logError(operationName.empty() ? "Transaction" : operationName,
                        "Failed to commit transaction: " + dbManager_.getLastError());
                dbManager_.rollbackTransaction();
                return false;
            }
        } else {
            dbManager_.rollbackTransaction();
        }

        return result;

    } catch (const std::exception& e) {
        dbManager_.rollbackTransaction();
        logError(operationName.empty() ? "Transaction" : operationName,
                "Exception during transaction: " + std::string(e.what()));
        return false;
    }
}

} // namespace Repositories
} // namespace Database
} // namespace CryptoClaude