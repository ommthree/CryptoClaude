#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <stdexcept>
#include <optional>
#include <cmath>
#include <algorithm>

namespace CryptoClaude {
namespace Database {
namespace Models {

class SentimentData {
private:
    std::string ticker_;
    std::string sourceName_;
    std::string date_;
    int articleCount_;
    double avgSentiment_;
    std::chrono::system_clock::time_point timestamp_;

    // Aggregated sentiment metrics (calculated/cached)
    mutable std::optional<double> sentiment1d_;
    mutable std::optional<double> sentiment7dAvg_;
    mutable std::optional<double> sentimentTrend_;

    void validateTicker(const std::string& ticker) const {
        if (ticker.empty()) {
            throw std::invalid_argument("Ticker cannot be empty");
        }
        if (ticker.length() > 20) {
            throw std::invalid_argument("Ticker too long (max 20 characters)");
        }
    }

    void validateSourceName(const std::string& source) const {
        if (source.empty()) {
            throw std::invalid_argument("Source name cannot be empty");
        }
        if (source.length() > 100) {
            throw std::invalid_argument("Source name too long (max 100 characters)");
        }
    }

    void validateArticleCount(int count) const {
        if (count < 0) {
            throw std::invalid_argument("Article count cannot be negative");
        }
    }

    void validateSentiment(double sentiment) const {
        if (std::isnan(sentiment) || std::isinf(sentiment)) {
            throw std::invalid_argument("Sentiment cannot be NaN or infinite");
        }
        if (sentiment < -1.0 || sentiment > 1.0) {
            throw std::invalid_argument("Sentiment must be between -1.0 and 1.0");
        }
    }

public:
    SentimentData() = default;

    SentimentData(const std::string& tick,
                  const std::string& source,
                  const std::string& dt,
                  int count,
                  double sentiment)
        : ticker_(tick), sourceName_(source), date_(dt),
          articleCount_(count), avgSentiment_(sentiment) {

        validateTicker(tick);
        validateSourceName(source);
        validateArticleCount(count);
        validateSentiment(sentiment);

        // Parse date to timestamp if needed
        // For now, use current time - this could be enhanced with date parsing
        timestamp_ = std::chrono::system_clock::now();
    }

    // Getters
    const std::string& getTicker() const { return ticker_; }
    const std::string& getSourceName() const { return sourceName_; }
    const std::string& getDate() const { return date_; }
    int getArticleCount() const { return articleCount_; }
    double getAvgSentiment() const { return avgSentiment_; }
    const std::chrono::system_clock::time_point& getTimestamp() const { return timestamp_; }

    // Aggregated metrics (cached/calculated)
    std::optional<double> getSentiment1d() const { return sentiment1d_; }
    std::optional<double> getSentiment7dAvg() const { return sentiment7dAvg_; }
    std::optional<double> getSentimentTrend() const { return sentimentTrend_; }

    // Setters with validation
    void setTicker(const std::string& ticker) {
        validateTicker(ticker);
        ticker_ = ticker;
    }

    void setSourceName(const std::string& source) {
        validateSourceName(source);
        sourceName_ = source;
    }

    void setDate(const std::string& date) {
        date_ = date;
        // TODO: Parse date string to timestamp
        timestamp_ = std::chrono::system_clock::now();
    }

    void setArticleCount(int count) {
        validateArticleCount(count);
        articleCount_ = count;
    }

    void setAvgSentiment(double sentiment) {
        validateSentiment(sentiment);
        avgSentiment_ = sentiment;
        clearAggregatedMetrics();
    }

    void setTimestamp(const std::chrono::system_clock::time_point& ts) {
        timestamp_ = ts;
    }

    // Aggregated metrics setters
    void setSentiment1d(double sentiment) {
        validateSentiment(sentiment);
        sentiment1d_ = sentiment;
    }

    void setSentiment7dAvg(double sentiment) {
        validateSentiment(sentiment);
        sentiment7dAvg_ = sentiment;
    }

    void setSentimentTrend(double trend) {
        if (std::isnan(trend) || std::isinf(trend)) {
            throw std::invalid_argument("Sentiment trend cannot be NaN or infinite");
        }
        sentimentTrend_ = trend;
    }

    void clearAggregatedMetrics() const {
        sentiment1d_.reset();
        sentiment7dAvg_.reset();
        sentimentTrend_.reset();
    }

    // Business logic methods
    bool hasSignificantSentiment() const {
        return articleCount_ >= 5; // At least 5 articles for significance
    }

    bool isPositiveSentiment() const {
        return avgSentiment_ > 0.1; // Threshold for positive sentiment
    }

    bool isNegativeSentiment() const {
        return avgSentiment_ < -0.1; // Threshold for negative sentiment
    }

    bool isNeutralSentiment() const {
        return !isPositiveSentiment() && !isNegativeSentiment();
    }

    double getSentimentStrength() const {
        return std::abs(avgSentiment_);
    }

    // Weighted sentiment by article count
    double getWeightedSentiment(int maxWeight = 100) const {
        double weight = std::min(static_cast<double>(articleCount_), static_cast<double>(maxWeight)) / maxWeight;
        return avgSentiment_ * weight;
    }

    // Compatibility methods for CoinExclusionEngine
    bool hasRecentNews() const {
        return articleCount_ > 0;
    }

    double getSentimentScore() const {
        return avgSentiment_;
    }

    std::string getSymbol() const {
        return ticker_;
    }
};

class NewsSource {
private:
    int sourceId_;
    std::string name_;
    std::string urlMap_;
    int priority_;
    bool isActive_;
    std::chrono::system_clock::time_point lastUpdated_;

    void validateName(const std::string& name) const {
        if (name.empty()) {
            throw std::invalid_argument("Source name cannot be empty");
        }
        if (name.length() > 100) {
            throw std::invalid_argument("Source name too long (max 100 characters)");
        }
    }

    void validatePriority(int priority) const {
        if (priority < 1 || priority > 10) {
            throw std::invalid_argument("Priority must be between 1 and 10");
        }
    }

public:
    NewsSource() = default;

    NewsSource(const std::string& sourceName,
               const std::string& url,
               int prio = 1)
        : sourceId_(0), name_(sourceName), urlMap_(url), priority_(prio),
          isActive_(true), lastUpdated_(std::chrono::system_clock::now()) {

        validateName(sourceName);
        validatePriority(prio);
    }

    // Getters
    int getSourceId() const { return sourceId_; }
    const std::string& getName() const { return name_; }
    const std::string& getUrlMap() const { return urlMap_; }
    int getPriority() const { return priority_; }
    bool isActive() const { return isActive_; }
    const std::chrono::system_clock::time_point& getLastUpdated() const { return lastUpdated_; }

    // Setters with validation
    void setSourceId(int id) { sourceId_ = id; }

    void setName(const std::string& name) {
        validateName(name);
        name_ = name;
    }

    void setUrlMap(const std::string& url) {
        urlMap_ = url;
    }

    void setPriority(int priority) {
        validatePriority(priority);
        priority_ = priority;
    }

    void setActive(bool active) {
        isActive_ = active;
    }

    void updateLastUpdated() {
        lastUpdated_ = std::chrono::system_clock::now();
    }

    // Business logic
    bool isHighPriority() const {
        return priority_ <= 3;
    }

    bool needsUpdate(std::chrono::hours maxAge = std::chrono::hours(24)) const {
        auto now = std::chrono::system_clock::now();
        return (now - lastUpdated_) > maxAge;
    }
};

class NewsArticle {
private:
    std::string title_;
    std::string content_;
    std::string source_;
    std::string date_;
    std::string url_;
    double sentimentScore_;
    std::vector<std::string> tickers_;
    std::chrono::system_clock::time_point timestamp_;
    bool processed_;

    void validateTitle(const std::string& title) const {
        if (title.empty()) {
            throw std::invalid_argument("Article title cannot be empty");
        }
        if (title.length() > 500) {
            throw std::invalid_argument("Article title too long (max 500 characters)");
        }
    }

    void validateSource(const std::string& source) const {
        if (source.empty()) {
            throw std::invalid_argument("Article source cannot be empty");
        }
    }

    void validateSentiment(double sentiment) const {
        if (std::isnan(sentiment) || std::isinf(sentiment)) {
            throw std::invalid_argument("Sentiment score cannot be NaN or infinite");
        }
        if (sentiment < -1.0 || sentiment > 1.0) {
            throw std::invalid_argument("Sentiment score must be between -1.0 and 1.0");
        }
    }

public:
    NewsArticle() : sentimentScore_(0.0), processed_(false) {}

    NewsArticle(const std::string& title, const std::string& content, const std::string& source)
        : title_(title), content_(content), source_(source),
          sentimentScore_(0.0), timestamp_(std::chrono::system_clock::now()),
          processed_(false) {

        validateTitle(title);
        validateSource(source);
    }

    // Getters
    const std::string& getTitle() const { return title_; }
    const std::string& getContent() const { return content_; }
    const std::string& getSource() const { return source_; }
    const std::string& getDate() const { return date_; }
    const std::string& getUrl() const { return url_; }
    double getSentimentScore() const { return sentimentScore_; }
    const std::vector<std::string>& getTickers() const { return tickers_; }
    const std::chrono::system_clock::time_point& getTimestamp() const { return timestamp_; }
    bool isProcessed() const { return processed_; }

    // Setters with validation
    void setTitle(const std::string& title) {
        validateTitle(title);
        title_ = title;
    }

    void setContent(const std::string& content) {
        content_ = content;
    }

    void setSource(const std::string& source) {
        validateSource(source);
        source_ = source;
    }

    void setDate(const std::string& date) {
        date_ = date;
        // TODO: Parse date string to timestamp
    }

    void setUrl(const std::string& url) {
        url_ = url;
    }

    void setSentimentScore(double sentiment) {
        validateSentiment(sentiment);
        sentimentScore_ = sentiment;
    }

    void setTickers(const std::vector<std::string>& tickers) {
        // Validate each ticker
        for (const auto& ticker : tickers) {
            if (ticker.empty() || ticker.length() > 20) {
                throw std::invalid_argument("Invalid ticker in list");
            }
        }
        tickers_ = tickers;
    }

    void addTicker(const std::string& ticker) {
        if (ticker.empty() || ticker.length() > 20) {
            throw std::invalid_argument("Invalid ticker");
        }
        tickers_.push_back(ticker);
    }

    void setProcessed(bool processed) {
        processed_ = processed;
    }

    // Business logic methods
    bool isRelevantToTicker(const std::string& ticker) const {
        return std::find(tickers_.begin(), tickers_.end(), ticker) != tickers_.end();
    }

    bool hasPositiveSentiment() const {
        return sentimentScore_ > 0.1;
    }

    bool hasNegativeSentiment() const {
        return sentimentScore_ < -0.1;
    }

    bool isNeutralSentiment() const {
        return !hasPositiveSentiment() && !hasNegativeSentiment();
    }

    size_t getWordCount() const {
        if (content_.empty()) return 0;

        size_t count = 1;
        for (char c : content_) {
            if (c == ' ' || c == '\t' || c == '\n') {
                count++;
            }
        }
        return count;
    }

    bool isLongForm() const {
        return getWordCount() > 100; // Articles with more than 100 words
    }

    double getSentimentConfidence() const {
        // Higher confidence for longer articles
        size_t wordCount = getWordCount();
        double lengthFactor = std::min(wordCount / 200.0, 1.0); // Max confidence at 200+ words
        return std::abs(sentimentScore_) * lengthFactor;
    }
};

// Aggregated sentiment metrics for efficient querying
class AggregatedSentiment {
private:
    std::string ticker_;
    std::string date_;
    int totalArticles_;
    double weightedSentiment_;
    double sentimentVolatility_;
    std::vector<std::string> topSources_;

public:
    AggregatedSentiment() = default;

    AggregatedSentiment(const std::string& ticker, const std::string& date)
        : ticker_(ticker), date_(date), totalArticles_(0),
          weightedSentiment_(0.0), sentimentVolatility_(0.0) {}

    // Getters
    const std::string& getTicker() const { return ticker_; }
    const std::string& getDate() const { return date_; }
    int getTotalArticles() const { return totalArticles_; }
    double getWeightedSentiment() const { return weightedSentiment_; }
    double getSentimentVolatility() const { return sentimentVolatility_; }
    const std::vector<std::string>& getTopSources() const { return topSources_; }

    // Setters
    void setTicker(const std::string& ticker) { ticker_ = ticker; }
    void setDate(const std::string& date) { date_ = date; }
    void setTotalArticles(int count) { totalArticles_ = count; }
    void setWeightedSentiment(double sentiment) { weightedSentiment_ = sentiment; }
    void setSentimentVolatility(double volatility) { sentimentVolatility_ = volatility; }
    void setTopSources(const std::vector<std::string>& sources) { topSources_ = sources; }

    // Business logic
    bool hasSignificantCoverage() const {
        return totalArticles_ >= 10; // Need at least 10 articles
    }

    bool hasHighVolatility() const {
        return sentimentVolatility_ > 0.3; // High disagreement between sources
    }

    double getSentimentStrength() const {
        return std::abs(weightedSentiment_);
    }
};

} // namespace Models
} // namespace Database
} // namespace CryptoClaude