#pragma once

#include "NewsApiProvider.h"
#include "../../Database/Models/SentimentData.h"
#include "../../Http/HttpClient.h"
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <functional>

namespace CryptoClaude {
namespace Data {
namespace Providers {

// CryptoNews-specific data structures
struct CryptoNewsArticle : NewsArticle {
    // Cryptocurrency-specific fields
    std::vector<std::string> mentionedCoins;
    std::map<std::string, double> coinSentimentScores; // Per-coin sentiment
    double overallCryptoSentiment;
    double marketImpactScore; // Expected market impact (0-1)

    // Enhanced sentiment analysis
    std::string sentimentCategory; // "BULLISH", "BEARISH", "NEUTRAL", "MIXED"
    double confidenceScore; // Confidence in sentiment analysis (0-1)
    std::vector<std::string> keyTopics; // Extracted key topics/themes
    double virality; // Social media engagement/share potential (0-1)

    // Source credibility
    double sourceCredibility; // Credibility score of news source (0-1)
    int sourcePriority; // Priority ranking of source (1-10)
    bool isVerifiedSource; // Whether source is verified/whitelisted

    CryptoNewsArticle() : NewsArticle(),
                         overallCryptoSentiment(0.0),
                         marketImpactScore(0.0),
                         sentimentCategory("NEUTRAL"),
                         confidenceScore(0.0),
                         virality(0.0),
                         sourceCredibility(0.5),
                         sourcePriority(5),
                         isVerifiedSource(false) {}
};

struct CryptoNewsQuery {
    std::vector<std::string> coinSymbols; // BTC, ETH, etc.
    std::vector<std::string> keywords; // Additional keywords
    std::string timeRange = "24h"; // 1h, 6h, 24h, 7d, 30d
    int maxResults = 100;

    // Quality filters
    double minCredibilityScore = 0.3;
    double minSentimentConfidence = 0.5;
    bool verifiedSourcesOnly = false;

    // Content filters
    std::vector<std::string> requiredTopics;
    std::vector<std::string> excludedTopics;
    std::string languageFilter = "en";
};

struct CryptoNewsBatch {
    std::vector<CryptoNewsArticle> articles;
    std::map<std::string, Database::Models::SentimentData> aggregatedSentiment;
    std::chrono::system_clock::time_point fetchTime;
    int totalArticlesProcessed;
    int articlesPassedFilters;
    double averageCredibilityScore;
    std::string queryHash; // For caching/deduplication
};

// CryptoNews provider with advanced sentiment processing
class CryptoNewsProvider {
public:
    CryptoNewsProvider(std::shared_ptr<Http::HttpClient> httpClient);
    ~CryptoNewsProvider() = default;

    // Configuration
    void setApiKey(const std::string& apiKey);
    void setRateLimitDelay(int milliseconds);
    void setMaxRetries(int maxRetries);
    void enableCaching(bool enable, int cacheDurationMinutes = 15);

    // Core fetching functionality
    CryptoNewsBatch fetchCryptoNews(const CryptoNewsQuery& query);
    std::vector<CryptoNewsArticle> fetchNewsForCoin(const std::string& symbol,
                                                   const std::string& timeRange = "24h");
    std::vector<CryptoNewsArticle> fetchMarketNews(const std::string& timeRange = "24h");

    // Sentiment aggregation
    Database::Models::SentimentData aggregateSentimentForCoin(
        const std::string& symbol,
        const std::vector<CryptoNewsArticle>& articles);

    std::map<std::string, Database::Models::SentimentData> aggregateMarketSentiment(
        const std::vector<CryptoNewsArticle>& articles);

    // Advanced analysis
    struct SentimentTrend {
        std::string symbol;
        std::vector<std::pair<std::chrono::system_clock::time_point, double>> timeline;
        double currentSentiment;
        double trendStrength; // -1 to 1, negative = declining sentiment
        std::string dominantNarrative; // Key theme driving sentiment
        double confidence;
    };

    std::vector<SentimentTrend> analyzeSentimentTrends(
        const std::vector<std::string>& symbols,
        int daysBack = 7);

    // Market event detection
    struct MarketEvent {
        std::string eventType; // "ANNOUNCEMENT", "PARTNERSHIP", "REGULATION", etc.
        std::string description;
        std::vector<std::string> affectedCoins;
        double marketImpactScore;
        std::chrono::system_clock::time_point eventTime;
        std::vector<std::string> supportingArticleUrls;
    };

    std::vector<MarketEvent> detectMarketEvents(
        const std::vector<CryptoNewsArticle>& articles);

    // Quality and credibility management
    void addVerifiedSource(const std::string& domain, double credibilityScore = 1.0);
    void removeVerifiedSource(const std::string& domain);
    void updateSourceCredibility(const std::string& domain, double newScore);

    struct SourceMetrics {
        std::string domain;
        double credibilityScore;
        int articlesProcessed;
        double averageSentimentAccuracy;
        int verificationLevel; // 0=unverified, 1=basic, 2=premium, 3=institutional
    };

    std::vector<SourceMetrics> getSourceMetrics() const;

    // Performance and analytics
    struct ProviderStats {
        int totalArticlesProcessed;
        int totalAPIRequests;
        double averageProcessingTime;
        double cacheHitRate;
        int rateLimitHits;
        double overallSentimentAccuracy;
        std::map<std::string, int> articlesPerCoin;
        std::chrono::system_clock::time_point lastUpdate;
    };

    ProviderStats getProviderStats() const;
    void resetStats();

private:
    std::shared_ptr<Http::HttpClient> httpClient_;
    std::string apiKey_;
    int rateLimitDelay_;
    int maxRetries_;
    bool cachingEnabled_;
    int cacheDurationMinutes_;

    // Source credibility and verification
    std::map<std::string, double> verifiedSources_;
    std::map<std::string, SourceMetrics> sourceMetrics_;

    // Caching
    struct CacheEntry {
        CryptoNewsBatch data;
        std::chrono::system_clock::time_point expiry;
    };
    mutable std::map<std::string, CacheEntry> cache_;

    // Performance tracking
    mutable ProviderStats stats_;

    // Core API interaction
    std::string buildApiUrl(const CryptoNewsQuery& query) const;
    nlohmann::json makeApiRequest(const std::string& url) const;
    void handleRateLimit(const Http::HttpResponse& response) const;

    // Article processing
    std::vector<CryptoNewsArticle> parseArticleResponse(const nlohmann::json& response) const;
    CryptoNewsArticle parseArticle(const nlohmann::json& articleJson) const;
    void enhanceArticleWithSentiment(CryptoNewsArticle& article) const;
    void detectMentionedCoins(CryptoNewsArticle& article) const;

    // Sentiment processing
    double calculateSentimentScore(const std::string& content) const;
    std::string categorizeSentiment(double score) const;
    double calculateConfidenceScore(const std::string& content, double sentimentScore) const;
    std::vector<std::string> extractKeyTopics(const std::string& content) const;
    double calculateMarketImpactScore(const CryptoNewsArticle& article) const;
    double calculateVirality(const CryptoNewsArticle& article) const;

    // Source credibility
    double getSourceCredibility(const std::string& domain) const;
    void updateSourceMetrics(const std::string& domain, const CryptoNewsArticle& article);
    bool isSourceVerified(const std::string& domain) const;

    // Event detection
    MarketEvent detectEventFromArticles(const std::vector<CryptoNewsArticle>& relatedArticles) const;
    std::string classifyEventType(const std::vector<CryptoNewsArticle>& articles) const;

    // Utility functions
    std::string generateCacheKey(const CryptoNewsQuery& query) const;
    bool isCacheValid(const CacheEntry& entry) const;
    std::vector<std::string> extractCoinMentions(const std::string& text) const;
    double calculateTextSimilarity(const std::string& text1, const std::string& text2) const;
    void logApiUsage(const std::string& endpoint, int responseSize) const;
};

// Sentiment analysis enhancements
class AdvancedSentimentAnalyzer {
public:
    AdvancedSentimentAnalyzer();
    ~AdvancedSentimentAnalyzer() = default;

    // Configuration
    void loadSentimentModel(const std::string& modelPath);
    void setConfidenceThreshold(double threshold);
    void enableCoinSpecificAnalysis(bool enable);

    // Core sentiment analysis
    struct SentimentAnalysisResult {
        double overallSentiment; // -1 to 1
        double confidence; // 0 to 1
        std::string category; // BULLISH/BEARISH/NEUTRAL/MIXED
        std::map<std::string, double> aspectSentiments; // price, technology, adoption, etc.
        std::vector<std::string> keyPhrases;
        double emotionalIntensity; // 0 to 1
    };

    SentimentAnalysisResult analyzeSentiment(const std::string& text) const;
    std::map<std::string, SentimentAnalysisResult> analyzeCoinSpecificSentiment(
        const std::string& text,
        const std::vector<std::string>& mentionedCoins) const;

    // Advanced features
    double calculateBias(const std::string& text) const;
    std::vector<std::string> extractKeyTopics(const std::string& text) const;
    double predictMarketImpact(const SentimentAnalysisResult& sentiment,
                              const std::string& coinSymbol) const;

    // Model validation and improvement
    struct ValidationResult {
        double accuracy;
        double precision;
        double recall;
        double f1Score;
        std::vector<std::pair<std::string, double>> confusionMatrix;
    };

    ValidationResult validateModel(const std::vector<std::pair<std::string, std::string>>& testData) const;
    void updateModelWeights(const std::vector<std::pair<std::string, double>>& trainingData);

private:
    std::string modelPath_;
    double confidenceThreshold_;
    bool coinSpecificAnalysisEnabled_;

    // Sentiment lexicons and models
    std::map<std::string, double> cryptoSentimentLexicon_;
    std::map<std::string, double> generalSentimentLexicon_;
    std::vector<std::string> positiveIndicators_;
    std::vector<std::string> negativeIndicators_;

    // Helper functions
    std::vector<std::string> tokenizeText(const std::string& text) const;
    double calculateLexiconScore(const std::vector<std::string>& tokens,
                                const std::map<std::string, double>& lexicon) const;
    std::string preprocessText(const std::string& text) const;
    double adjustForContext(double baseScore, const std::vector<std::string>& context) const;
};

} // namespace Providers
} // namespace Data
} // namespace CryptoClaude