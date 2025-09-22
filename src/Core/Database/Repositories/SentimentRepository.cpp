#include "SentimentRepository.h"
#include <iostream>
#include <sstream>

namespace CryptoClaude {
namespace Database {
namespace Repositories {

// Constructor
SentimentRepository::SentimentRepository(DatabaseManager& dbManager)
    : dbManager_(dbManager) {
}

// Destructor
SentimentRepository::~SentimentRepository() {
    // Clean up prepared statements
    for (auto& pair : preparedStatements_) {
        // The unique_ptr will automatically call sqlite3_finalize
    }
    preparedStatements_.clear();
}

// Helper methods (minimal implementations for compilation)
void SentimentRepository::logError(const std::string& operation, const std::string& error) const {
    lastError_ = formatError(operation, error);
    std::cerr << "SentimentRepository Error: " << lastError_ << std::endl;
}

std::string SentimentRepository::formatError(const std::string& operation, const std::string& detail) const {
    return operation + ": " + detail;
}

sqlite3_stmt* SentimentRepository::getOrCreatePreparedStatement(const std::string& key, const std::string& sql) const {
    // Stub implementation - would cache prepared statements in real implementation
    return nullptr;
}

Models::SentimentData SentimentRepository::mapRowToSentimentData(StatementWrapper& stmt) const {
    // Create sentiment data with basic constructor
    Models::SentimentData data(
        stmt.getStringColumn(0),    // ticker
        stmt.getStringColumn(1),    // source_name
        stmt.getStringColumn(2),    // date
        stmt.getIntColumn(3),       // article_count
        stmt.getDoubleColumn(4)     // avg_sentiment
    );

    // Set timestamp
    data.setTimestamp(unixToTime(stmt.getIntColumn(5)));

    // Set aggregated metrics if calculated
    if (!stmt.isColumnNull(6)) data.setSentiment1d(stmt.getDoubleColumn(6));
    if (!stmt.isColumnNull(7)) data.setSentiment7dAvg(stmt.getDoubleColumn(7));
    if (!stmt.isColumnNull(8)) data.setSentimentTrend(stmt.getDoubleColumn(8));

    return data;
}

Models::NewsSource SentimentRepository::mapRowToNewsSource(StatementWrapper& stmt) const {
    // Create news source
    Models::NewsSource source(
        stmt.getStringColumn(1),    // name
        stmt.getStringColumn(2),    // url_map
        stmt.getIntColumn(3)        // priority
    );

    source.setSourceId(stmt.getIntColumn(0));
    source.setActive(stmt.getIntColumn(4) == 1);
    source.updateLastUpdated(); // Set to current time, or could parse from DB

    return source;
}

Models::NewsArticle SentimentRepository::mapRowToNewsArticle(StatementWrapper& stmt) const {
    // Create news article
    Models::NewsArticle article(
        stmt.getStringColumn(0),    // title
        stmt.getStringColumn(1),    // content
        stmt.getStringColumn(2)     // source
    );

    article.setDate(stmt.getStringColumn(3));
    article.setUrl(stmt.getStringColumn(4));
    article.setSentimentScore(stmt.getDoubleColumn(5));
    article.setProcessed(stmt.getIntColumn(6) == 1);

    // Parse tickers (assuming comma-separated in DB)
    std::string tickersStr = stmt.getStringColumn(7);
    if (!tickersStr.empty()) {
        std::vector<std::string> tickers;
        std::stringstream ss(tickersStr);
        std::string ticker;
        while (std::getline(ss, ticker, ',')) {
            if (!ticker.empty()) {
                tickers.push_back(ticker);
            }
        }
        article.setTickers(tickers);
    }

    return article;
}

Models::AggregatedSentiment SentimentRepository::mapRowToAggregatedSentiment(StatementWrapper& stmt) const {
    Models::AggregatedSentiment aggregated(
        stmt.getStringColumn(0),    // ticker
        stmt.getStringColumn(1)     // date
    );

    aggregated.setTotalArticles(stmt.getIntColumn(2));
    aggregated.setWeightedSentiment(stmt.getDoubleColumn(3));
    aggregated.setSentimentVolatility(stmt.getDoubleColumn(4));

    // Parse top sources (assuming comma-separated)
    std::string sourcesStr = stmt.getStringColumn(5);
    if (!sourcesStr.empty()) {
        std::vector<std::string> sources;
        std::stringstream ss(sourcesStr);
        std::string source;
        while (std::getline(ss, source, ',')) {
            if (!source.empty()) {
                sources.push_back(source);
            }
        }
        aggregated.setTopSources(sources);
    }

    return aggregated;
}

int64_t SentimentRepository::timeToUnix(const std::chrono::system_clock::time_point& tp) const {
    return std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch()).count();
}

std::chrono::system_clock::time_point SentimentRepository::unixToTime(int64_t unixTime) const {
    return std::chrono::system_clock::from_time_t(static_cast<time_t>(unixTime));
}

double SentimentRepository::convertSentimentStringToScore(const std::string& sentiment) const {
    if (sentiment == "very_positive") return 0.8;
    if (sentiment == "positive") return 0.4;
    if (sentiment == "neutral") return 0.0;
    if (sentiment == "negative") return -0.4;
    if (sentiment == "very_negative") return -0.8;

    // Try to parse as numeric
    try {
        double score = std::stod(sentiment);
        return std::max(-1.0, std::min(1.0, score)); // Clamp to [-1, 1]
    } catch (...) {
        return 0.0; // Default to neutral
    }
}

std::string SentimentRepository::convertSentimentScoreToString(double score) const {
    if (score >= 0.6) return "very_positive";
    if (score >= 0.2) return "positive";
    if (score <= -0.6) return "very_negative";
    if (score <= -0.2) return "negative";
    return "neutral";
}

// SQL constants (real implementations)
const std::string SentimentRepository::SQL_INSERT_SENTIMENT = R"(
    INSERT OR REPLACE INTO sentiment_data (
        ticker, source_name, date, article_count, avg_sentiment, timestamp,
        sentiment_1d, sentiment_7d_avg, sentiment_trend
    ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
)";

const std::string SentimentRepository::SQL_INSERT_NEWS_SOURCE = R"(
    INSERT INTO news_sources (name, url_map, priority, is_active, last_updated)
    VALUES (?, ?, ?, ?, ?)
)";

const std::string SentimentRepository::SQL_INSERT_ARTICLE = R"(
    INSERT INTO news_articles (
        title, content, source, date, url, sentiment_score, processed, tickers
    ) VALUES (?, ?, ?, ?, ?, ?, ?, ?)
)";

const std::string SentimentRepository::SQL_SELECT_BY_TICKER = R"(
    SELECT ticker, source_name, date, article_count, avg_sentiment, timestamp,
           sentiment_1d, sentiment_7d_avg, sentiment_trend
    FROM sentiment_data
    WHERE ticker = ?
    ORDER BY timestamp DESC
)";

const std::string SentimentRepository::SQL_SELECT_LATEST_FOR_TICKER = R"(
    SELECT ticker, source_name, date, article_count, avg_sentiment, timestamp,
           sentiment_1d, sentiment_7d_avg, sentiment_trend
    FROM sentiment_data
    WHERE ticker = ?
    ORDER BY timestamp DESC
    LIMIT 1
)";

const std::string SentimentRepository::SQL_SELECT_AGGREGATED_SENTIMENT = R"(
    SELECT ticker, date, total_articles, weighted_sentiment, sentiment_volatility, top_sources
    FROM aggregated_sentiment
    WHERE ticker = ? AND date = ?
)";

const std::string SentimentRepository::SQL_UPDATE_SENTIMENT = R"(
    UPDATE sentiment_data SET
        article_count = ?, avg_sentiment = ?, timestamp = ?,
        sentiment_1d = ?, sentiment_7d_avg = ?, sentiment_trend = ?
    WHERE ticker = ? AND source_name = ? AND date = ?
)";

const std::string SentimentRepository::SQL_INSERT_AGGREGATED_SENTIMENT = R"(
    INSERT OR REPLACE INTO aggregated_sentiment (
        ticker, date, total_articles, weighted_sentiment, sentiment_volatility, top_sources
    ) VALUES (?, ?, ?, ?, ?, ?)
)";

// === SENTIMENT DATA CRUD IMPLEMENTATIONS ===

bool SentimentRepository::insertSentimentData(const Models::SentimentData& data) {
    return executeInTransaction([&]() {
        auto stmt = dbManager_.prepareStatement(SQL_INSERT_SENTIMENT);
        if (!stmt) {
            logError("insertSentimentData", "Failed to prepare statement");
            return false;
        }

        StatementWrapper wrapper(stmt);

        // Bind sentiment data
        wrapper.bindString(1, data.getTicker());
        wrapper.bindString(2, data.getSourceName());
        wrapper.bindString(3, data.getDate());
        wrapper.bindInt(4, data.getArticleCount());
        wrapper.bindDouble(5, data.getAvgSentiment());
        wrapper.bindInt(6, static_cast<int>(timeToUnix(data.getTimestamp())));

        // Bind aggregated metrics (may be null)
        if (data.getSentiment1d().has_value()) {
            wrapper.bindDouble(7, data.getSentiment1d().value());
        } else {
            wrapper.bindNull(7);
        }

        if (data.getSentiment7dAvg().has_value()) {
            wrapper.bindDouble(8, data.getSentiment7dAvg().value());
        } else {
            wrapper.bindNull(8);
        }

        if (data.getSentimentTrend().has_value()) {
            wrapper.bindDouble(9, data.getSentimentTrend().value());
        } else {
            wrapper.bindNull(9);
        }

        return sqlite3_step(stmt) == SQLITE_DONE;
    }, "insertSentimentData");
}

std::vector<Models::SentimentData> SentimentRepository::getSentimentByTicker(const std::string& ticker, int limit) {
    std::vector<Models::SentimentData> results;

    std::string sql = SQL_SELECT_BY_TICKER;
    if (limit > 0) {
        sql += " LIMIT " + std::to_string(limit);
    }

    auto stmt = dbManager_.prepareStatement(sql);
    if (!stmt) {
        logError("getSentimentByTicker", "Failed to prepare statement");
        return results;
    }

    StatementWrapper wrapper(stmt);
    wrapper.bindString(1, ticker);

    while (wrapper.step()) {
        results.push_back(mapRowToSentimentData(wrapper));
    }

    return results;
}

std::optional<Models::SentimentData> SentimentRepository::getLatestSentimentForTicker(const std::string& ticker) {
    auto stmt = dbManager_.prepareStatement(SQL_SELECT_LATEST_FOR_TICKER);
    if (!stmt) {
        logError("getLatestSentimentForTicker", "Failed to prepare statement");
        return std::nullopt;
    }

    StatementWrapper wrapper(stmt);
    wrapper.bindString(1, ticker);

    if (!wrapper.step()) {
        return std::nullopt;
    }

    return mapRowToSentimentData(wrapper);
}

bool SentimentRepository::updateSentimentData(const Models::SentimentData& data) {
    return executeInTransaction([&]() {
        auto stmt = dbManager_.prepareStatement(SQL_UPDATE_SENTIMENT);
        if (!stmt) {
            logError("updateSentimentData", "Failed to prepare statement");
            return false;
        }

        StatementWrapper wrapper(stmt);

        // Bind updated data
        wrapper.bindInt(1, data.getArticleCount());
        wrapper.bindDouble(2, data.getAvgSentiment());
        wrapper.bindInt(3, static_cast<int>(timeToUnix(data.getTimestamp())));

        // Bind aggregated metrics
        if (data.getSentiment1d().has_value()) {
            wrapper.bindDouble(4, data.getSentiment1d().value());
        } else {
            wrapper.bindNull(4);
        }

        if (data.getSentiment7dAvg().has_value()) {
            wrapper.bindDouble(5, data.getSentiment7dAvg().value());
        } else {
            wrapper.bindNull(5);
        }

        if (data.getSentimentTrend().has_value()) {
            wrapper.bindDouble(6, data.getSentimentTrend().value());
        } else {
            wrapper.bindNull(6);
        }

        // WHERE clause
        wrapper.bindString(7, data.getTicker());
        wrapper.bindString(8, data.getSourceName());
        wrapper.bindString(9, data.getDate());

        return sqlite3_step(stmt) == SQLITE_DONE;
    }, "updateSentimentData");
}

bool SentimentRepository::deleteSentimentData(const std::string& ticker, const std::string& source, const std::string& date) {
    return executeInTransaction([&]() {
        std::string deleteSQL = "DELETE FROM sentiment_data WHERE ticker = ? AND source_name = ? AND date = ?";
        auto stmt = dbManager_.prepareStatement(deleteSQL);
        if (!stmt) {
            logError("deleteSentimentData", "Failed to prepare statement");
            return false;
        }

        StatementWrapper wrapper(stmt);
        wrapper.bindString(1, ticker);
        wrapper.bindString(2, source);
        wrapper.bindString(3, date);

        return sqlite3_step(stmt) == SQLITE_DONE;
    }, "deleteSentimentData");
}

// === NEWS SOURCE METHODS ===

bool SentimentRepository::insertNewsSource(const Models::NewsSource& source) {
    return executeInTransaction([&]() {
        auto stmt = dbManager_.prepareStatement(SQL_INSERT_NEWS_SOURCE);
        if (!stmt) {
            logError("insertNewsSource", "Failed to prepare statement");
            return false;
        }

        StatementWrapper wrapper(stmt);
        wrapper.bindString(1, source.getName());
        wrapper.bindString(2, source.getUrlMap());
        wrapper.bindInt(3, source.getPriority());
        wrapper.bindInt(4, source.isActive() ? 1 : 0);
        wrapper.bindInt(5, static_cast<int>(timeToUnix(source.getLastUpdated())));

        return sqlite3_step(stmt) == SQLITE_DONE;
    }, "insertNewsSource");
}

// === NEWS ARTICLE METHODS ===

bool SentimentRepository::insertNewsArticle(const Models::NewsArticle& article) {
    return executeInTransaction([&]() {
        auto stmt = dbManager_.prepareStatement(SQL_INSERT_ARTICLE);
        if (!stmt) {
            logError("insertNewsArticle", "Failed to prepare statement");
            return false;
        }

        StatementWrapper wrapper(stmt);
        wrapper.bindString(1, article.getTitle());
        wrapper.bindString(2, article.getContent());
        wrapper.bindString(3, article.getSource());
        wrapper.bindString(4, article.getDate());
        wrapper.bindString(5, article.getUrl());
        wrapper.bindDouble(6, article.getSentimentScore());
        wrapper.bindInt(7, article.isProcessed() ? 1 : 0);

        // Convert tickers vector to comma-separated string
        std::string tickersStr;
        const auto& tickers = article.getTickers();
        for (size_t i = 0; i < tickers.size(); ++i) {
            if (i > 0) tickersStr += ",";
            tickersStr += tickers[i];
        }
        wrapper.bindString(8, tickersStr);

        return sqlite3_step(stmt) == SQLITE_DONE;
    }, "insertNewsArticle");
}

// === AGGREGATED SENTIMENT METHODS ===

bool SentimentRepository::insertAggregatedSentiment(const Models::AggregatedSentiment& aggregated) {
    return executeInTransaction([&]() {
        auto stmt = dbManager_.prepareStatement(SQL_INSERT_AGGREGATED_SENTIMENT);
        if (!stmt) {
            logError("insertAggregatedSentiment", "Failed to prepare statement");
            return false;
        }

        StatementWrapper wrapper(stmt);
        wrapper.bindString(1, aggregated.getTicker());
        wrapper.bindString(2, aggregated.getDate());
        wrapper.bindInt(3, aggregated.getTotalArticles());
        wrapper.bindDouble(4, aggregated.getWeightedSentiment());
        wrapper.bindDouble(5, aggregated.getSentimentVolatility());

        // Convert top sources vector to comma-separated string
        std::string sourcesStr;
        const auto& sources = aggregated.getTopSources();
        for (size_t i = 0; i < sources.size(); ++i) {
            if (i > 0) sourcesStr += ",";
            sourcesStr += sources[i];
        }
        wrapper.bindString(6, sourcesStr);

        return sqlite3_step(stmt) == SQLITE_DONE;
    }, "insertAggregatedSentiment");
}

std::optional<Models::AggregatedSentiment> SentimentRepository::getAggregatedSentiment(const std::string& ticker, const std::string& date) {
    auto stmt = dbManager_.prepareStatement(SQL_SELECT_AGGREGATED_SENTIMENT);
    if (!stmt) {
        logError("getAggregatedSentiment", "Failed to prepare statement");
        return std::nullopt;
    }

    StatementWrapper wrapper(stmt);
    wrapper.bindString(1, ticker);
    wrapper.bindString(2, date);

    if (!wrapper.step()) {
        return std::nullopt;
    }

    return mapRowToAggregatedSentiment(wrapper);
}

} // namespace Repositories
} // namespace Database
} // namespace CryptoClaude