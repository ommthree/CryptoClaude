#pragma once

#include "../../Http/HttpClient.h"
#include "../../Database/Models/SentimentData.h"
#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <chrono>
#include <map>

#include <nlohmann/json.hpp>

namespace CryptoClaude {
namespace Data {
namespace Providers {

using namespace CryptoClaude::Http;
using namespace CryptoClaude::Database::Models;

// News search criteria
struct NewsRequest {
    std::string query;              // Search query (e.g., "Bitcoin" or "BTC")
    std::string language = "en";    // Language filter
    std::string sortBy = "publishedAt";  // Sort by: publishedAt, relevancy, popularity
    int pageSize = 100;             // Max results per request (max 100)
    int page = 1;                   // Page number for pagination

    // Time filters
    std::optional<std::chrono::system_clock::time_point> from;
    std::optional<std::chrono::system_clock::time_point> to;

    // Domain filters (optional)
    std::vector<std::string> domains;      // Include only these domains
    std::vector<std::string> excludeDomains;  // Exclude these domains

    // Source filters (optional)
    std::vector<std::string> sources;      // Specific news sources
};

// Individual news article
struct NewsArticle {
    std::string title;
    std::string description;
    std::string content;
    std::string url;
    std::string urlToImage;
    std::string source;
    std::string author;
    std::chrono::system_clock::time_point publishedAt;

    // Sentiment analysis results (filled by provider)
    std::optional<double> sentimentScore;     // -1.0 to +1.0
    std::optional<double> confidenceScore;    // 0.0 to 1.0
    std::vector<std::string> keywords;        // Extracted keywords
    std::string category = "general";        // Article category

    bool isValid() const {
        return !title.empty() && !url.empty();
    }
};

// Provider response for news data
struct NewsProviderResponse {
    bool success = false;
    std::string errorMessage;
    std::vector<NewsArticle> articles;
    std::chrono::system_clock::time_point timestamp;
    int totalResults = 0;           // Total available results
    int rateLimitRemaining = -1;    // Remaining API calls
    int httpStatusCode = 0;

    // Summary statistics
    double averageSentiment = 0.0;   // Average sentiment of all articles
    int positiveCount = 0;           // Count of positive articles
    int neutralCount = 0;            // Count of neutral articles
    int negativeCount = 0;           // Count of negative articles
};

// News API provider interface
class INewsApiProvider {
public:
    virtual ~INewsApiProvider() = default;

    // News retrieval
    virtual NewsProviderResponse searchNews(const NewsRequest& request) = 0;
    virtual NewsProviderResponse getTopHeadlines(const std::string& category = "", const std::string& country = "us") = 0;
    virtual NewsProviderResponse getCryptoNews(const std::string& symbol = "", int hours = 24) = 0;

    // Sentiment analysis
    virtual NewsProviderResponse analyzeSentiment(const std::vector<NewsArticle>& articles) = 0;
    virtual SentimentData createSentimentData(const std::string& ticker, const std::vector<NewsArticle>& articles) = 0;

    // Configuration
    virtual void setApiKey(const std::string& apiKey) = 0;
    virtual bool isConfigured() const = 0;
};

// Main News API provider implementation
class NewsApiProvider : public INewsApiProvider {
private:
    std::shared_ptr<IHttpClient> httpClient_;
    std::string apiKey_;
    std::string baseUrl_;
    bool enableLogging_;
    bool enableSentimentAnalysis_;

    // Rate limiting
    mutable std::chrono::system_clock::time_point lastRequestTime_;
    mutable int requestsThisHour_;
    static constexpr int MAX_REQUESTS_PER_HOUR = 1000;  // NewsAPI limit

    // Sentiment analysis cache (to avoid re-analyzing same content)
    mutable std::map<std::string, double> sentimentCache_;
    static constexpr size_t MAX_CACHE_SIZE = 10000;

    // Statistics
    mutable int totalRequests_;
    mutable int successfulRequests_;
    mutable int failedRequests_;
    mutable int articlesProcessed_;
    mutable int sentimentAnalysisCount_;

public:
    explicit NewsApiProvider(std::shared_ptr<IHttpClient> httpClient,
                           const std::string& apiKey = "",
                           const std::string& baseUrl = "https://newsapi.org/v2");

    virtual ~NewsApiProvider() = default;

    // Configuration
    void setApiKey(const std::string& apiKey) override { apiKey_ = apiKey; }
    bool isConfigured() const override { return !apiKey_.empty(); }
    void enableLogging(bool enable) { enableLogging_ = enable; }
    void enableSentimentAnalysis(bool enable) { enableSentimentAnalysis_ = enable; }

    // News retrieval
    NewsProviderResponse searchNews(const NewsRequest& request) override;
    NewsProviderResponse getTopHeadlines(const std::string& category = "", const std::string& country = "us") override;
    NewsProviderResponse getCryptoNews(const std::string& symbol = "", int hours = 24) override;

    // Sentiment analysis
    NewsProviderResponse analyzeSentiment(const std::vector<NewsArticle>& articles) override;
    SentimentData createSentimentData(const std::string& ticker, const std::vector<NewsArticle>& articles) override;

    // Crypto-specific convenience methods
    NewsProviderResponse getBitcoinNews(int hours = 24) { return getCryptoNews("Bitcoin", hours); }
    NewsProviderResponse getEthereumNews(int hours = 24) { return getCryptoNews("Ethereum", hours); }
    NewsProviderResponse getDeFiNews(int hours = 24) { return getCryptoNews("DeFi", hours); }

    // Bulk sentiment analysis for multiple symbols
    std::map<std::string, SentimentData> getBulkSentiment(const std::vector<std::string>& symbols, int hours = 24);

    // Statistics and monitoring
    struct NewsProviderStats {
        int totalRequests = 0;
        int successfulRequests = 0;
        int failedRequests = 0;
        double successRate = 0.0;
        int articlesProcessed = 0;
        int sentimentAnalysisCount = 0;
        std::chrono::milliseconds averageResponseTime = std::chrono::milliseconds(0);
        std::chrono::system_clock::time_point lastSuccessfulRequest;
        std::string lastError;
    };

    NewsProviderStats getStatistics() const;
    void resetStatistics();
    void clearSentimentCache();

    // Health and quality checks
    bool testConnection();
    double getSentimentCacheHitRate() const;

    // Data quality assessment
    struct NewsDataQuality {
        bool isValid = false;
        std::string errorMessage;
        int articleCount = 0;
        int validArticleCount = 0;
        double completenessRatio = 0.0;
        int duplicateCount = 0;
        std::chrono::system_clock::time_point oldestArticle;
        std::chrono::system_clock::time_point newestArticle;
        double averageContentLength = 0.0;
        std::vector<std::string> missingSources;
    };

    NewsDataQuality assessDataQuality(const std::vector<NewsArticle>& articles) const;

private:
    // Rate limiting
    void enforceRateLimit() const;

    // Request building
    HttpRequest buildNewsRequest(const std::string& endpoint, const NewsRequest& request) const;
    HttpRequest buildHeadlinesRequest(const std::string& category, const std::string& country) const;

    // Response parsing
    NewsProviderResponse parseNewsResponse(const HttpResponse& httpResponse) const;
    std::vector<NewsArticle> parseArticlesFromJson(const nlohmann::json& articlesJson) const;
    NewsArticle parseArticleFromJson(const nlohmann::json& articleJson) const;

    // Sentiment analysis implementation
    double analyzeSingleArticleSentiment(const NewsArticle& article) const;
    double calculateSentimentFromText(const std::string& text) const;
    void updateSentimentStatistics(NewsProviderResponse& response) const;

    // Utility methods
    std::string generateCacheKey(const std::string& content) const;
    std::vector<std::string> extractKeywords(const std::string& text) const;
    std::string normalizeCryptoSymbol(const std::string& symbol) const;
    std::string buildCryptoQuery(const std::string& symbol) const;

    // Error handling and logging
    NewsProviderResponse createErrorResponse(const std::string& message, int statusCode = 0) const;
    void logRequest(const HttpRequest& request) const;
    void logResponse(const HttpResponse& response, bool success) const;
    void updateStatistics(bool success, int articlesCount, std::chrono::milliseconds responseTime) const;

    // Data validation
    bool isValidNewsRequest(const NewsRequest& request) const;
    void removeDuplicateArticles(std::vector<NewsArticle>& articles) const;
    void filterRelevantArticles(std::vector<NewsArticle>& articles, const std::string& symbol) const;
};

// Factory for creating News API providers
class NewsApiProviderFactory {
public:
    static std::unique_ptr<NewsApiProvider> create(const std::string& apiKey);
    static std::unique_ptr<NewsApiProvider> createWithCustomClient(std::shared_ptr<IHttpClient> client, const std::string& apiKey);
    static std::unique_ptr<NewsApiProvider> createForTesting(); // Mock provider for tests
};

// Configuration helper
struct NewsApiConfig {
    std::string apiKey;
    std::string baseUrl = "https://newsapi.org/v2";
    bool enableLogging = false;
    bool enableSentimentAnalysis = true;
    int maxRequestsPerHour = 1000;
    std::chrono::seconds defaultTimeout = std::chrono::seconds(30);

    // Default crypto symbols to track
    std::vector<std::string> defaultSymbols = {"Bitcoin", "Ethereum", "BTC", "ETH", "DeFi", "cryptocurrency"};

    // Default news sources (high quality crypto sources)
    std::vector<std::string> preferredSources = {
        "coindesk.com", "cointelegraph.com", "decrypt.co",
        "theblockcrypto.com", "coinbase.com", "kraken.com"
    };

    bool isValid() const {
        return !apiKey.empty() && !baseUrl.empty();
    }

    std::vector<std::string> getConfigurationErrors() const {
        std::vector<std::string> errors;
        if (apiKey.empty()) errors.push_back("API key is required");
        if (baseUrl.empty()) errors.push_back("Base URL is required");
        if (maxRequestsPerHour <= 0) errors.push_back("Max requests per hour must be positive");
        return errors;
    }
};

// Sentiment analysis utilities (simple implementation)
class SimpleSentimentAnalyzer {
public:
    static double analyzeSentiment(const std::string& text);
    static std::vector<std::string> extractKeywords(const std::string& text);
    static double calculateConfidence(const std::string& text);

private:
    static const std::map<std::string, double> POSITIVE_WORDS;
    static const std::map<std::string, double> NEGATIVE_WORDS;
    static const std::vector<std::string> CRYPTO_KEYWORDS;
};

} // namespace Providers
} // namespace Data
} // namespace CryptoClaude