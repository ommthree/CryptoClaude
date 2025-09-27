#include "NewsApiProvider.h"
#include "../Utils/Logger.h"
#include <thread>
#include <chrono>
#include <sstream>
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <regex>

namespace CryptoClaude {
namespace DataIngestion {

NewsApiProvider::NewsApiProvider(const std::string& apiKey)
    : m_apiKey(apiKey),
      m_baseUrl("https://newsapi.org/v2/"),
      m_remainingRequests(1000), // Default, will be updated from API response
      m_rateLimitDelayMs(100),   // 10 requests per second = 100ms delay
      m_coinsPerBatch(10),
      m_daysPerBatch(7),
      m_currentPage(1),
      m_maxArticlesPerBatch(100) {

    m_httpClient = std::make_unique<Utils::HttpClient>();
    m_rateLimitReset = std::chrono::system_clock::now() + std::chrono::hours(1);
    m_sourcesCacheTime = std::chrono::system_clock::now() - std::chrono::hours(1); // Force initial load
    m_tickersCacheTime = std::chrono::system_clock::now() - std::chrono::hours(1);
}

bool NewsApiProvider::isAvailable() const {
    try {
        // Test with a simple sources endpoint call
        std::map<std::string, std::string> params;
        params["category"] = "business";
        params["language"] = "en";

        auto response = m_httpClient->get(buildUrl("sources", params));

        if (response.statusCode == 200 && !response.body.empty()) {
            auto json = Utils::JsonHelper::parse(response.body);
            if (json.contains("status") && json["status"] == "ok") {
                return true;
            } else if (json.contains("message")) {
                m_lastError = json["message"].get<std::string>();
                return false;
            }
        } else if (response.statusCode == 401) {
            m_lastError = "Invalid API key";
            return false;
        } else if (response.statusCode == 429) {
            m_lastError = "Rate limit exceeded";
            return false;
        } else {
            m_lastError = "HTTP Error " + std::to_string(response.statusCode);
            return false;
        }

        return false;
    } catch (const std::exception& e) {
        m_lastError = "Connection test failed: " + std::string(e.what());
        return false;
    }
}

std::vector<Database::Models::NewsArticle> NewsApiProvider::getNews(
    const std::vector<std::string>& tickers,
    const std::string& startDate,
    const std::string& endDate,
    int maxArticles) {

    std::vector<Database::Models::NewsArticle> articles;

    try {
        if (!validateTickers(tickers) || !validateDateRange(startDate, endDate)) {
            return articles;
        }

        waitForRateLimit();

        // Build search query for cryptocurrency news
        std::string query = "cryptocurrency OR bitcoin OR crypto";
        for (const auto& ticker : tickers) {
            query += " OR " + ticker;
        }

        std::map<std::string, std::string> params;
        params["q"] = query;
        params["from"] = startDate;
        params["to"] = endDate;
        params["language"] = "en";
        params["sortBy"] = "publishedAt";
        params["pageSize"] = std::to_string(std::min(maxArticles, 100)); // API limit per request
        params["page"] = std::to_string(m_currentPage);

        auto json = fetchJson("everything", params);

        if (json.contains("status") && json["status"] == "ok") {
            articles = parseNewsArticles(json);
            logApiRequest("everything", 200);
        } else {
            handleApiError(json);
        }

    } catch (const std::exception& e) {
        m_lastError = "Failed to fetch news: " + std::string(e.what());
        Utils::Logger::error("NewsApiProvider::getNews - " + m_lastError);
        logApiRequest("everything", 500);
    }

    return articles;
}

std::vector<Database::Models::SentimentData> NewsApiProvider::getSentimentData(
    const std::vector<std::string>& tickers,
    const std::string& startDate,
    const std::string& endDate) {

    std::vector<Database::Models::SentimentData> sentimentData;

    try {
        // First get news articles
        auto articles = getNews(tickers, startDate, endDate, 200);

        if (!articles.empty()) {
            // Aggregate sentiment from articles
            sentimentData = aggregateSentimentData(articles);
        }

    } catch (const std::exception& e) {
        m_lastError = "Failed to fetch sentiment data: " + std::string(e.what());
        Utils::Logger::error("NewsApiProvider::getSentimentData - " + m_lastError);
    }

    return sentimentData;
}

std::vector<Database::Models::NewsSource> NewsApiProvider::getNewsSources() {
    // Check cache first
    auto now = std::chrono::system_clock::now();
    if (std::chrono::duration_cast<std::chrono::hours>(now - m_sourcesCacheTime).count() < 24 &&
        !m_sourcesCache.empty()) {
        return m_sourcesCache;
    }

    std::vector<Database::Models::NewsSource> sources;

    try {
        waitForRateLimit();

        std::map<std::string, std::string> params;
        params["category"] = "business";
        params["language"] = "en";

        auto json = fetchJson("sources", params);

        if (json.contains("status") && json["status"] == "ok") {
            sources = parseNewsSources(json);

            // Cache the results
            m_sourcesCache = sources;
            m_sourcesCacheTime = now;

            logApiRequest("sources", 200);
        } else {
            handleApiError(json);
        }

    } catch (const std::exception& e) {
        m_lastError = "Failed to fetch news sources: " + std::string(e.what());
        Utils::Logger::error("NewsApiProvider::getNewsSources - " + m_lastError);
        logApiRequest("sources", 500);
    }

    return sources;
}

bool NewsApiProvider::isSourceSupported(const std::string& sourceName) {
    auto sources = getNewsSources();
    return std::find_if(sources.begin(), sources.end(),
                       [&sourceName](const Database::Models::NewsSource& source) {
                           return source.name == sourceName || source.id == sourceName;
                       }) != sources.end();
}

double NewsApiProvider::analyzeSentiment(const std::string& text) {
    // Simple sentiment analysis using keyword matching
    // In production, this would use a proper sentiment analysis service

    if (text.empty()) {
        return 0.0;
    }

    std::vector<std::string> positiveWords = {
        "bull", "bullish", "rally", "surge", "moon", "pump", "gain", "profit",
        "buy", "long", "hodl", "positive", "optimistic", "breakthrough", "adoption",
        "institutional", "investment", "growth", "rise", "increase", "good", "great"
    };

    std::vector<std::string> negativeWords = {
        "bear", "bearish", "crash", "dump", "fall", "loss", "sell", "short",
        "fear", "panic", "decline", "drop", "negative", "pessimistic", "regulatory",
        "ban", "hack", "scam", "fraud", "bubble", "volatile", "risk", "bad"
    };

    std::string lowerText = text;
    std::transform(lowerText.begin(), lowerText.end(), lowerText.begin(), ::tolower);

    int positiveCount = 0;
    int negativeCount = 0;
    int totalWords = 0;

    std::istringstream iss(lowerText);
    std::string word;
    while (iss >> word) {
        totalWords++;
        if (std::find(positiveWords.begin(), positiveWords.end(), word) != positiveWords.end()) {
            positiveCount++;
        } else if (std::find(negativeWords.begin(), negativeWords.end(), word) != negativeWords.end()) {
            negativeCount++;
        }
    }

    if (totalWords == 0) {
        return 0.0;
    }

    // Calculate normalized sentiment score [-1.0 to 1.0]
    double positiveRatio = static_cast<double>(positiveCount) / totalWords;
    double negativeRatio = static_cast<double>(negativeCount) / totalWords;

    return (positiveRatio - negativeRatio) * 10.0; // Scale up for more sensitivity
}

std::string NewsApiProvider::getSentimentCategory(double score) {
    if (score > 0.3) return "Positive";
    else if (score < -0.3) return "Negative";
    else return "Neutral";
}

std::vector<std::string> NewsApiProvider::getSupportedTickers() {
    // Check cache first
    auto now = std::chrono::system_clock::now();
    if (std::chrono::duration_cast<std::chrono::hours>(now - m_tickersCacheTime).count() < 24 &&
        !m_supportedTickersCache.empty()) {
        return m_supportedTickersCache;
    }

    // Return common cryptocurrency tickers
    std::vector<std::string> tickers = {
        "BTC", "ETH", "XRP", "LTC", "BCH", "ADA", "DOT", "BNB", "LINK", "XLM",
        "DOGE", "UNI", "AAVE", "COMP", "MKR", "YFI", "SUSHI", "CRV", "SNX", "1INCH"
    };

    m_supportedTickersCache = tickers;
    m_tickersCacheTime = now;

    return tickers;
}

bool NewsApiProvider::isTickerSupported(const std::string& ticker) {
    auto supportedTickers = getSupportedTickers();
    return std::find(supportedTickers.begin(), supportedTickers.end(), ticker) != supportedTickers.end();
}

void NewsApiProvider::setApiKey(const std::string& apiKey) {
    m_apiKey = apiKey;
    clearError();
}

void NewsApiProvider::setBatchSize(int coinsPerBatch, int daysPerBatch) {
    m_coinsPerBatch = std::max(1, coinsPerBatch);
    m_daysPerBatch = std::max(1, daysPerBatch);
}

void NewsApiProvider::fetchAllNewsSentiment(
    const std::vector<std::string>& allCoins,
    const std::string& endDate,
    int totalDays,
    std::function<void(const std::vector<Database::Models::SentimentData>&)> batchCallback) {

    if (!batchCallback) {
        m_lastError = "Batch callback function is required";
        return;
    }

    try {
        // Calculate total batches
        int coinBatches = (allCoins.size() + m_coinsPerBatch - 1) / m_coinsPerBatch;
        int dayBatches = (totalDays + m_daysPerBatch - 1) / m_daysPerBatch;
        int totalBatches = coinBatches * dayBatches;

        int currentBatch = 0;

        for (int coinBatch = 0; coinBatch < coinBatches; ++coinBatch) {
            // Get coins for this batch
            int startCoinIdx = coinBatch * m_coinsPerBatch;
            int endCoinIdx = std::min(startCoinIdx + m_coinsPerBatch, static_cast<int>(allCoins.size()));

            std::vector<std::string> batchCoins(allCoins.begin() + startCoinIdx,
                                              allCoins.begin() + endCoinIdx);

            for (int dayBatch = 0; dayBatch < dayBatches; ++dayBatch) {
                currentBatch++;
                logBatchProgress(currentBatch, totalBatches, "Fetching news sentiment");

                // Calculate date range for this batch
                int startDayOffset = dayBatch * m_daysPerBatch;
                int endDayOffset = std::min(startDayOffset + m_daysPerBatch, totalDays);

                // Calculate actual dates (working backwards from endDate)
                auto endTime = std::chrono::system_clock::now();
                auto batchEndTime = endTime - std::chrono::hours(24 * startDayOffset);
                auto batchStartTime = endTime - std::chrono::hours(24 * endDayOffset);

                std::time_t batchStartTimeT = std::chrono::system_clock::to_time_t(batchStartTime);
                std::time_t batchEndTimeT = std::chrono::system_clock::to_time_t(batchEndTime);

                std::ostringstream batchStartStream, batchEndStream;
                batchStartStream << std::put_time(std::gmtime(&batchStartTimeT), "%Y-%m-%d");
                batchEndStream << std::put_time(std::gmtime(&batchEndTimeT), "%Y-%m-%d");

                // Fetch sentiment data for this batch
                try {
                    auto sentimentData = getSentimentData(batchCoins,
                                                        batchStartStream.str(),
                                                        batchEndStream.str());

                    if (!sentimentData.empty()) {
                        batchCallback(sentimentData);
                    }

                    // Rate limiting between batches
                    waitForRateLimit();

                } catch (const std::exception& e) {
                    Utils::Logger::warning("Failed to process batch " + std::to_string(currentBatch) +
                                         ": " + e.what());
                    continue; // Continue with next batch
                }
            }
        }

        Utils::Logger::info("Completed fetching news sentiment for " + std::to_string(totalBatches) + " batches");

    } catch (const std::exception& e) {
        m_lastError = "Batch processing failed: " + std::string(e.what());
        Utils::Logger::error("NewsApiProvider::fetchAllNewsSentiment - " + m_lastError);
    }
}

// === PRIVATE HELPER METHODS ===

std::string NewsApiProvider::buildUrl(const std::string& endpoint,
                                     const std::map<std::string, std::string>& params) {
    std::string url = m_baseUrl + endpoint;

    bool firstParam = true;
    for (const auto& param : params) {
        url += (firstParam ? "?" : "&") + param.first + "=" + param.second;
        firstParam = false;
    }

    // Add API key
    url += (firstParam ? "?" : "&") + std::string("apiKey=") + m_apiKey;

    return url;
}

Utils::JsonHelper::Json NewsApiProvider::fetchJson(const std::string& endpoint,
                                                  const std::map<std::string, std::string>& params) {
    std::string url = buildUrl(endpoint, params);

    auto response = m_httpClient->get(url);
    updateRateLimit(response);

    if (response.statusCode != 200) {
        throw std::runtime_error("HTTP Error " + std::to_string(response.statusCode) + ": " + response.body);
    }

    return Utils::JsonHelper::parse(response.body);
}

std::vector<Database::Models::NewsArticle> NewsApiProvider::parseNewsArticles(const Utils::JsonHelper::Json& json) {
    std::vector<Database::Models::NewsArticle> articles;

    if (json.contains("articles") && json["articles"].is_array()) {
        for (const auto& item : json["articles"]) {
            auto article = jsonToNewsArticle(item);
            if (!article.title.empty() && !article.description.empty()) {
                // Analyze sentiment
                std::string fullText = article.title + " " + article.description;
                article.sentimentScore = analyzeSentiment(fullText);
                article.sentimentLabel = getSentimentCategory(article.sentimentScore);

                articles.push_back(article);
            }
        }
    }

    return articles;
}

std::vector<Database::Models::SentimentData> NewsApiProvider::aggregateSentimentData(
    const std::vector<Database::Models::NewsArticle>& articles) {

    std::unordered_map<std::string, std::pair<int, double>> dailyAggregates;

    for (const auto& article : articles) {
        // Group by date (YYYY-MM-DD)
        std::time_t publishTime = std::chrono::system_clock::to_time_t(article.publishedAt);
        std::ostringstream dateStream;
        dateStream << std::put_time(std::gmtime(&publishTime), "%Y-%m-%d");
        std::string dateKey = dateStream.str();

        // Aggregate sentiment by date
        if (dailyAggregates.find(dateKey) == dailyAggregates.end()) {
            dailyAggregates[dateKey] = {0, 0.0};
        }

        dailyAggregates[dateKey].first++;  // Count
        dailyAggregates[dateKey].second += article.sentimentScore; // Sum
    }

    std::vector<Database::Models::SentimentData> sentimentData;

    for (const auto& aggregate : dailyAggregates) {
        Database::Models::SentimentData sentiment;
        sentiment.symbol = "CRYPTO_NEWS"; // General crypto news sentiment
        sentiment.source = "newsapi";

        // Parse date back to time_point
        std::tm tm = {};
        std::istringstream ss(aggregate.first);
        ss >> std::get_time(&tm, "%Y-%m-%d");
        sentiment.timestamp = std::chrono::system_clock::from_time_t(std::mktime(&tm));

        // Calculate average sentiment
        sentiment.sentiment = aggregate.second.second / aggregate.second.first;
        sentiment.confidence = std::min(1.0, static_cast<double>(aggregate.second.first) / 10.0); // More articles = higher confidence
        sentiment.volume = aggregate.second.first; // Number of articles as volume

        sentimentData.push_back(sentiment);
    }

    // Sort by date
    std::sort(sentimentData.begin(), sentimentData.end(),
              [](const Database::Models::SentimentData& a, const Database::Models::SentimentData& b) {
                  return a.timestamp < b.timestamp;
              });

    return sentimentData;
}

std::vector<Database::Models::NewsSource> NewsApiProvider::parseNewsSources(const Utils::JsonHelper::Json& json) {
    std::vector<Database::Models::NewsSource> sources;

    if (json.contains("sources") && json["sources"].is_array()) {
        for (const auto& item : json["sources"]) {
            auto source = jsonToNewsSource(item);
            if (!source.id.empty() && !source.name.empty()) {
                sources.push_back(source);
            }
        }
    }

    return sources;
}

void NewsApiProvider::updateRateLimit(const Utils::HttpResponse& response) {
    // NewsAPI provides rate limit headers
    auto remainingIt = response.headers.find("X-RateLimit-Remaining");
    if (remainingIt != response.headers.end()) {
        try {
            m_remainingRequests = std::stoi(remainingIt->second);
        } catch (...) {
            // Ignore parsing errors
        }
    }

    auto resetIt = response.headers.find("X-RateLimit-Reset");
    if (resetIt != response.headers.end()) {
        try {
            auto resetTime = std::stoi(resetIt->second);
            m_rateLimitReset = std::chrono::system_clock::from_time_t(resetTime);
        } catch (...) {
            // Ignore parsing errors
        }
    }
}

void NewsApiProvider::handleApiError(const Utils::JsonHelper::Json& json) {
    if (json.contains("message")) {
        m_lastError = json["message"].get<std::string>();
    } else if (json.contains("status") && json["status"] == "error") {
        m_lastError = "API returned error response";
    } else {
        m_lastError = "Unknown API error";
    }

    Utils::Logger::warning("NewsApiProvider API Error: " + m_lastError);
}

void NewsApiProvider::waitForRateLimit() {
    // NewsAPI has generous rate limits but we still want to be respectful
    // 10 requests per second = 100ms delay
    if (m_rateLimitDelayMs > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(m_rateLimitDelayMs));
    }

    // Check if we're hitting rate limits
    if (m_remainingRequests <= 10) {
        auto now = std::chrono::system_clock::now();
        if (now < m_rateLimitReset) {
            auto waitTime = std::chrono::duration_cast<std::chrono::milliseconds>(m_rateLimitReset - now);
            if (waitTime.count() > 0 && waitTime.count() < 3600000) { // Don't wait more than 1 hour
                Utils::Logger::info("Rate limit approaching, waiting " +
                                   std::to_string(waitTime.count()) + "ms");
                std::this_thread::sleep_for(waitTime);
            }
        }
    }
}

Database::Models::NewsArticle NewsApiProvider::jsonToNewsArticle(const Utils::JsonHelper::Json& item) {
    Database::Models::NewsArticle article;

    article.title = item.value("title", "");
    article.description = item.value("description", "");
    article.content = item.value("content", "");
    article.url = item.value("url", "");
    article.author = item.value("author", "");
    article.source = "newsapi";

    // Parse published date
    std::string publishedAt = item.value("publishedAt", "");
    if (!publishedAt.empty()) {
        // Parse ISO 8601 format: 2023-07-15T14:30:00Z
        std::tm tm = {};
        std::istringstream ss(publishedAt);
        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
        if (!ss.fail()) {
            article.publishedAt = std::chrono::system_clock::from_time_t(std::mktime(&tm));
        } else {
            article.publishedAt = std::chrono::system_clock::now();
        }
    } else {
        article.publishedAt = std::chrono::system_clock::now();
    }

    // Extract source info
    if (item.contains("source") && item["source"].is_object()) {
        auto sourceObj = item["source"];
        article.sourceId = sourceObj.value("id", "");
        article.sourceName = sourceObj.value("name", "");
    }

    return article;
}

Database::Models::NewsSource NewsApiProvider::jsonToNewsSource(const Utils::JsonHelper::Json& item) {
    Database::Models::NewsSource source;

    source.id = item.value("id", "");
    source.name = item.value("name", "");
    source.description = item.value("description", "");
    source.url = item.value("url", "");
    source.category = item.value("category", "");
    source.language = item.value("language", "");
    source.country = item.value("country", "");

    return source;
}

bool NewsApiProvider::validateTickers(const std::vector<std::string>& tickers) const {
    if (tickers.empty()) {
        m_lastError = "No tickers provided";
        return false;
    }

    for (const auto& ticker : tickers) {
        if (ticker.empty() || ticker.length() > 10) {
            m_lastError = "Invalid ticker: " + ticker;
            return false;
        }
    }

    return true;
}

bool NewsApiProvider::validateDateRange(const std::string& startDate, const std::string& endDate) const {
    // Basic date format validation (YYYY-MM-DD)
    std::regex datePattern(R"(\d{4}-\d{2}-\d{2})");

    if (!std::regex_match(startDate, datePattern)) {
        m_lastError = "Invalid start date format: " + startDate;
        return false;
    }

    if (!std::regex_match(endDate, datePattern)) {
        m_lastError = "Invalid end date format: " + endDate;
        return false;
    }

    if (startDate > endDate) {
        m_lastError = "Start date must be before end date";
        return false;
    }

    return true;
}

void NewsApiProvider::logApiRequest(const std::string& endpoint, int responseCode) const {
    std::string logMessage = "NewsAPI " + endpoint + " - Status: " + std::to_string(responseCode) +
                            " - Remaining: " + std::to_string(m_remainingRequests);
    if (responseCode == 200) {
        Utils::Logger::debug(logMessage);
    } else {
        Utils::Logger::warning(logMessage);
    }
}

void NewsApiProvider::logBatchProgress(int currentBatch, int totalBatches, const std::string& operation) const {
    double progress = static_cast<double>(currentBatch) / totalBatches * 100.0;
    Utils::Logger::info(operation + ": " + std::to_string(currentBatch) + "/" +
                       std::to_string(totalBatches) + " (" +
                       std::to_string(static_cast<int>(progress)) + "%)");
}

} // namespace DataIngestion
} // namespace CryptoClaude