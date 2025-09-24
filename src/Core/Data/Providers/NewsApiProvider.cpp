#include "NewsApiProvider.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <thread>
#include <regex>
#include <unordered_set>

namespace CryptoClaude {
namespace Data {
namespace Providers {

using json = nlohmann::json;

NewsApiProvider::NewsApiProvider(std::shared_ptr<IHttpClient> httpClient,
                               const std::string& apiKey,
                               const std::string& baseUrl)
    : httpClient_(httpClient), apiKey_(apiKey), baseUrl_(baseUrl),
      enableLogging_(false), enableSentimentAnalysis_(true),
      requestsThisHour_(0), totalRequests_(0), successfulRequests_(0),
      failedRequests_(0), articlesProcessed_(0), sentimentAnalysisCount_(0) {

    lastRequestTime_ = std::chrono::system_clock::now() - std::chrono::hours(1);

    if (!httpClient_) {
        throw std::invalid_argument("HttpClient cannot be null");
    }
}

NewsProviderResponse NewsApiProvider::searchNews(const NewsRequest& request) {
    if (!isConfigured()) {
        return createErrorResponse("News API provider not configured with API key");
    }

    if (!isValidNewsRequest(request)) {
        return createErrorResponse("Invalid news request parameters");
    }

    enforceRateLimit();

    try {
        auto httpRequest = buildNewsRequest("everything", request);

        if (enableLogging_) {
            logRequest(httpRequest);
        }

        auto startTime = std::chrono::high_resolution_clock::now();
        auto httpResponse = httpClient_->execute(httpRequest);
        auto endTime = std::chrono::high_resolution_clock::now();

        auto responseTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        auto response = parseNewsResponse(httpResponse);

        updateStatistics(response.success, response.articles.size(), responseTime);

        if (enableLogging_) {
            logResponse(httpResponse, response.success);
        }

        // Apply sentiment analysis if enabled
        if (response.success && enableSentimentAnalysis_) {
            analyzeSentiment(response.articles);
            updateSentimentStatistics(response);
        }

        return response;

    } catch (const std::exception& e) {
        updateStatistics(false, 0, std::chrono::milliseconds(0));
        return createErrorResponse("News search failed: " + std::string(e.what()));
    }
}

NewsProviderResponse NewsApiProvider::getCryptoNews(const std::string& symbol, int hours) {
    NewsRequest request;
    request.query = buildCryptoQuery(symbol);
    request.sortBy = "publishedAt";
    request.pageSize = 100;

    // Set time filter for recent news
    auto now = std::chrono::system_clock::now();
    request.from = now - std::chrono::hours(hours);
    request.to = now;

    auto response = searchNews(request);

    // Filter articles for relevance to the specific symbol
    if (response.success && !symbol.empty()) {
        filterRelevantArticles(response.articles, symbol);
    }

    return response;
}

NewsProviderResponse NewsApiProvider::getTopHeadlines(const std::string& category, const std::string& country) {
    if (!isConfigured()) {
        return createErrorResponse("News API provider not configured with API key");
    }

    enforceRateLimit();

    try {
        auto httpRequest = buildHeadlinesRequest(category, country);
        auto httpResponse = httpClient_->execute(httpRequest);
        auto response = parseNewsResponse(httpResponse);

        updateStatistics(response.success, response.articles.size(), std::chrono::milliseconds(100));

        if (response.success && enableSentimentAnalysis_) {
            analyzeSentiment(response.articles);
            updateSentimentStatistics(response);
        }

        return response;

    } catch (const std::exception& e) {
        updateStatistics(false, 0, std::chrono::milliseconds(0));
        return createErrorResponse("Headlines request failed: " + std::string(e.what()));
    }
}

NewsProviderResponse NewsApiProvider::analyzeSentiment(const std::vector<NewsArticle>& articles) {
    NewsProviderResponse response;
    response.success = true;
    response.timestamp = std::chrono::system_clock::now();

    if (articles.empty()) {
        response.errorMessage = "No articles to analyze";
        return response;
    }

    double totalSentiment = 0.0;
    int validAnalyses = 0;

    for (auto& article : const_cast<std::vector<NewsArticle>&>(articles)) {
        try {
            double sentiment = analyzeSingleArticleSentiment(article);
            article.sentimentScore = sentiment;
            article.confidenceScore = calculateConfidence(article.title + " " + article.description);

            totalSentiment += sentiment;
            ++validAnalyses;
            ++sentimentAnalysisCount_;

            // Extract keywords
            article.keywords = extractKeywords(article.title + " " + article.description);

        } catch (const std::exception& e) {
            if (enableLogging_) {
                std::cerr << "Sentiment analysis failed for article: " << e.what() << std::endl;
            }
        }
    }

    response.averageSentiment = validAnalyses > 0 ? totalSentiment / validAnalyses : 0.0;
    response.articles = articles;

    return response;
}

SentimentData NewsApiProvider::createSentimentData(const std::string& ticker, const std::vector<NewsArticle>& articles) {
    SentimentData sentimentData;

    if (articles.empty()) {
        return sentimentData;
    }

    // Analyze sentiment if not already done
    double totalSentiment = 0.0;
    int validArticles = 0;
    int positiveCount = 0;
    int negativeCount = 0;
    auto now = std::chrono::system_clock::now();

    for (const auto& article : articles) {
        if (article.sentimentScore.has_value()) {
            double sentiment = article.sentimentScore.value();
            totalSentiment += sentiment;
            ++validArticles;

            if (sentiment > 0.1) ++positiveCount;
            else if (sentiment < -0.1) ++negativeCount;
        }
    }

    if (validArticles > 0) {
        double avgSentiment = totalSentiment / validArticles;

        sentimentData.setTicker(ticker);
        sentimentData.setTimestamp(now);
        sentimentData.setAvgSentiment(avgSentiment);
        sentimentData.setArticleCount(validArticles);

        // Calculate confidence based on article count and sentiment consistency
        double confidence = std::min(1.0, validArticles / 10.0); // More articles = higher confidence
        double sentimentVariance = 0.0; // Simplified - could calculate actual variance

        // Additional confidence boost for clear positive/negative trends
        double sentimentClarity = std::abs(avgSentiment);
        confidence = std::min(1.0, confidence + sentimentClarity * 0.2);
    }

    return sentimentData;
}

HttpRequest NewsApiProvider::buildNewsRequest(const std::string& endpoint, const NewsRequest& request) const {
    std::string url = baseUrl_ + "/" + endpoint;

    HttpRequest httpRequest(HttpMethod::GET, url);
    httpRequest.setApiKeyHeader(apiKey_)
               .addQueryParam("q", request.query)
               .addQueryParam("language", request.language)
               .addQueryParam("sortBy", request.sortBy)
               .addQueryParam("pageSize", std::to_string(request.pageSize))
               .addQueryParam("page", std::to_string(request.page));

    // Add time filters if specified
    if (request.from.has_value()) {
        auto fromTime = std::chrono::system_clock::to_time_t(request.from.value());
        httpRequest.addQueryParam("from", std::to_string(fromTime));
    }

    if (request.to.has_value()) {
        auto toTime = std::chrono::system_clock::to_time_t(request.to.value());
        httpRequest.addQueryParam("to", std::to_string(toTime));
    }

    // Add domain filters
    if (!request.domains.empty()) {
        std::string domainsStr;
        for (size_t i = 0; i < request.domains.size(); ++i) {
            if (i > 0) domainsStr += ",";
            domainsStr += request.domains[i];
        }
        httpRequest.addQueryParam("domains", domainsStr);
    }

    return httpRequest;
}

HttpRequest NewsApiProvider::buildHeadlinesRequest(const std::string& category, const std::string& country) const {
    std::string url = baseUrl_ + "/top-headlines";

    HttpRequest httpRequest(HttpMethod::GET, url);
    httpRequest.setApiKeyHeader(apiKey_);

    if (!category.empty()) {
        httpRequest.addQueryParam("category", category);
    }

    if (!country.empty()) {
        httpRequest.addQueryParam("country", country);
    }

    return httpRequest;
}

NewsProviderResponse NewsApiProvider::parseNewsResponse(const HttpResponse& httpResponse) const {
    NewsProviderResponse response;
    response.timestamp = std::chrono::system_clock::now();
    response.httpStatusCode = httpResponse.getStatusCode();

    if (!httpResponse.isSuccess()) {
        response.success = false;
        response.errorMessage = "HTTP " + std::to_string(httpResponse.getStatusCode()) + ": " + httpResponse.getBody();
        return response;
    }

    try {
        json jsonData = json::parse(httpResponse.getBody());

        if (jsonData.contains("status") && jsonData["status"] != "ok") {
            response.success = false;
            response.errorMessage = jsonData.value("message", "Unknown API error");
            return response;
        }

        if (jsonData.contains("articles")) {
            response.articles = parseArticlesFromJson(jsonData["articles"]);
            response.totalResults = jsonData.value("totalResults", response.articles.size());
        }

        // Remove duplicates
        removeDuplicateArticles(response.articles);

        response.success = true;

    } catch (const json::parse_error& e) {
        response.success = false;
        response.errorMessage = "JSON parse error: " + std::string(e.what());
    } catch (const std::exception& e) {
        response.success = false;
        response.errorMessage = "Response parsing error: " + std::string(e.what());
    }

    return response;
}

std::vector<NewsArticle> NewsApiProvider::parseArticlesFromJson(const nlohmann::json& articlesJson) const {
    std::vector<NewsArticle> articles;

    for (const auto& articleJson : articlesJson) {
        try {
            auto article = parseArticleFromJson(articleJson);
            if (article.isValid()) {
                articles.push_back(article);
            }
        } catch (const std::exception& e) {
            if (enableLogging_) {
                std::cerr << "Skipped invalid article: " << e.what() << std::endl;
            }
        }
    }

    return articles;
}

NewsArticle NewsApiProvider::parseArticleFromJson(const nlohmann::json& articleJson) const {
    NewsArticle article;

    article.title = articleJson.value("title", "");
    article.description = articleJson.value("description", "");
    article.content = articleJson.value("content", "");
    article.url = articleJson.value("url", "");
    article.urlToImage = articleJson.value("urlToImage", "");
    article.author = articleJson.value("author", "");

    // Parse source
    if (articleJson.contains("source") && articleJson["source"].contains("name")) {
        article.source = articleJson["source"]["name"];
    }

    // Parse publication date
    if (articleJson.contains("publishedAt")) {
        std::string publishedAtStr = articleJson["publishedAt"];
        // Simplified date parsing - in production, use proper ISO 8601 parser
        article.publishedAt = std::chrono::system_clock::now(); // Placeholder
    }

    return article;
}

double NewsApiProvider::analyzeSingleArticleSentiment(const NewsArticle& article) const {
    std::string combinedText = article.title + " " + article.description;

    // Check cache first
    std::string cacheKey = generateCacheKey(combinedText);
    auto cacheIt = sentimentCache_.find(cacheKey);
    if (cacheIt != sentimentCache_.end()) {
        return cacheIt->second;
    }

    double sentiment = calculateSentimentFromText(combinedText);

    // Cache the result (with size limit)
    if (sentimentCache_.size() < MAX_CACHE_SIZE) {
        sentimentCache_[cacheKey] = sentiment;
    }

    return sentiment;
}

double NewsApiProvider::calculateSentimentFromText(const std::string& text) const {
    // Simple sentiment analysis using keyword matching
    return SimpleSentimentAnalyzer::analyzeSentiment(text);
}

std::string NewsApiProvider::buildCryptoQuery(const std::string& symbol) const {
    if (symbol.empty()) {
        return "cryptocurrency OR bitcoin OR ethereum OR crypto";
    }

    std::string normalized = normalizeCryptoSymbol(symbol);
    return normalized + " OR cryptocurrency";
}

std::string NewsApiProvider::normalizeCryptoSymbol(const std::string& symbol) const {
    std::string normalized = symbol;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);

    // Map common symbols to full names
    if (normalized == "btc") return "bitcoin";
    if (normalized == "eth") return "ethereum";
    if (normalized == "ada") return "cardano";
    if (normalized == "dot") return "polkadot";

    return normalized;
}

void NewsApiProvider::enforceRateLimit() const {
    auto now = std::chrono::system_clock::now();
    auto timeSinceLastRequest = std::chrono::duration_cast<std::chrono::hours>(now - lastRequestTime_);

    if (timeSinceLastRequest < std::chrono::hours(1)) {
        if (requestsThisHour_ >= MAX_REQUESTS_PER_HOUR) {
            auto sleepTime = std::chrono::hours(1) - timeSinceLastRequest;
            std::this_thread::sleep_for(sleepTime);
            requestsThisHour_ = 0;
        }
    } else {
        requestsThisHour_ = 0;
    }

    ++requestsThisHour_;
    lastRequestTime_ = std::chrono::system_clock::now();
}

bool NewsApiProvider::testConnection() {
    try {
        auto response = getTopHeadlines("technology", "us");
        return response.success;
    } catch (...) {
        return false;
    }
}

NewsApiProvider::NewsProviderStats NewsApiProvider::getStatistics() const {
    NewsProviderStats stats;
    stats.totalRequests = totalRequests_;
    stats.successfulRequests = successfulRequests_;
    stats.failedRequests = failedRequests_;
    stats.articlesProcessed = articlesProcessed_;
    stats.sentimentAnalysisCount = sentimentAnalysisCount_;
    stats.successRate = totalRequests_ > 0 ?
        static_cast<double>(successfulRequests_) / totalRequests_ : 0.0;

    return stats;
}

void NewsApiProvider::updateStatistics(bool success, int articlesCount, std::chrono::milliseconds responseTime) const {
    ++totalRequests_;
    articlesProcessed_ += articlesCount;

    if (success) {
        ++successfulRequests_;
    } else {
        ++failedRequests_;
    }
}

NewsProviderResponse NewsApiProvider::createErrorResponse(const std::string& message, int statusCode) const {
    NewsProviderResponse response;
    response.success = false;
    response.errorMessage = message;
    response.httpStatusCode = statusCode;
    response.timestamp = std::chrono::system_clock::now();
    return response;
}

// Simple sentiment analyzer implementation
const std::map<std::string, double> SimpleSentimentAnalyzer::POSITIVE_WORDS = {
    {"good", 0.5}, {"great", 0.8}, {"excellent", 0.9}, {"amazing", 0.8},
    {"positive", 0.6}, {"gain", 0.6}, {"rise", 0.6}, {"bull", 0.8},
    {"bullish", 0.8}, {"up", 0.4}, {"high", 0.4}, {"strong", 0.6}
};

const std::map<std::string, double> SimpleSentimentAnalyzer::NEGATIVE_WORDS = {
    {"bad", -0.5}, {"terrible", -0.8}, {"awful", -0.8}, {"negative", -0.6},
    {"loss", -0.6}, {"fall", -0.6}, {"bear", -0.8}, {"bearish", -0.8},
    {"down", -0.4}, {"low", -0.4}, {"weak", -0.6}, {"crash", -0.9}
};

double SimpleSentimentAnalyzer::analyzeSentiment(const std::string& text) {
    if (text.empty()) return 0.0;

    std::string lowerText = text;
    std::transform(lowerText.begin(), lowerText.end(), lowerText.begin(), ::tolower);

    double sentiment = 0.0;
    int wordCount = 0;

    // Check positive words
    for (const auto& [word, score] : POSITIVE_WORDS) {
        if (lowerText.find(word) != std::string::npos) {
            sentiment += score;
            ++wordCount;
        }
    }

    // Check negative words
    for (const auto& [word, score] : NEGATIVE_WORDS) {
        if (lowerText.find(word) != std::string::npos) {
            sentiment += score; // score is already negative
            ++wordCount;
        }
    }

    // Normalize by word count and clamp to [-1, 1]
    if (wordCount > 0) {
        sentiment /= wordCount;
    }

    return std::clamp(sentiment, -1.0, 1.0);
}

// Factory implementations
std::unique_ptr<NewsApiProvider> NewsApiProviderFactory::create(const std::string& apiKey) {
    auto httpClient = std::make_shared<HttpClient>();
    httpClient->setUserAgent("CryptoClaude/1.0 News");
    return std::make_unique<NewsApiProvider>(httpClient, apiKey);
}

std::unique_ptr<NewsApiProvider> NewsApiProviderFactory::createWithCustomClient(
    std::shared_ptr<IHttpClient> client, const std::string& apiKey) {
    return std::make_unique<NewsApiProvider>(client, apiKey);
}

} // namespace Providers
} // namespace Data
} // namespace CryptoClaude