#include "LiveDataManager.h"
#include "../Json/JsonHelper.h"
#include "../Http/HttpClient.h"
#include <iostream>
#include <sstream>
#include <thread>
#include <numeric>
#include <algorithm>

namespace CryptoClaude {
namespace Data {

// NewsAPI Provider Implementation
NewsApiProvider::NewsApiProvider(const std::string& apiKey)
    : apiKey_(apiKey), healthy_(false) {
    httpClient_ = std::make_unique<Http::HttpClient>();
    httpClient_->setUserAgent("CryptoClaude/1.0");
    healthy_ = testConnection();
}

bool NewsApiProvider::testConnection() {
    try {
        Http::HttpRequest request(Http::HttpMethod::GET,
            "https://newsapi.org/v2/everything");
        request.addQueryParam("q", "bitcoin")
              .addQueryParam("pageSize", "1")
              .addQueryParam("apiKey", apiKey_);

        auto response = httpClient_->execute(request);

        if (response.isSuccess() && response.getStatusCode() == 200) {
            healthy_ = true;
            lastError_ = "";
            return true;
        } else {
            healthy_ = false;
            lastError_ = "HTTP " + std::to_string(response.getStatusCode()) +
                        ": " + response.getBody();
            return false;
        }
    } catch (const std::exception& e) {
        healthy_ = false;
        lastError_ = "Connection error: " + std::string(e.what());
        return false;
    }
}

std::vector<NewsData> NewsApiProvider::getCryptoNews(
    const std::vector<std::string>& tickers, int limit) {

    std::vector<NewsData> news;

    if (!healthy_) {
        return news;
    }

    try {
        // Build query with tickers
        std::string query = "cryptocurrency OR bitcoin";
        for (const auto& ticker : tickers) {
            query += " OR " + ticker;
        }

        Http::HttpRequest request(Http::HttpMethod::GET,
            "https://newsapi.org/v2/everything");
        request.addQueryParam("q", query)
              .addQueryParam("sortBy", "publishedAt")
              .addQueryParam("pageSize", std::to_string(std::min(limit, 100)))
              .addQueryParam("apiKey", apiKey_);

        auto response = httpClient_->execute(request);

        if (response.isSuccess()) {
            auto json = Json::JsonHelper::parse(response.getBody());

            if (json.contains("articles")) {
                for (const auto& article : json["articles"]) {
                    NewsData newsItem;
                    newsItem.title = Json::JsonHelper::getString(article, "title");
                    newsItem.content = Json::JsonHelper::getString(article, "description");
                    newsItem.url = Json::JsonHelper::getString(article, "url");
                    // Get source safely
                    if (article.contains("source") && article["source"].is_object()) {
                        newsItem.source = Json::JsonHelper::getString(article["source"], "name");
                    }

                    // Parse timestamp
                    std::string publishedAt = Json::JsonHelper::getString(article, "publishedAt");
                    // TODO: Parse ISO 8601 timestamp
                    newsItem.publishedAt = std::chrono::system_clock::now();

                    newsItem.provider = "NewsAPI";
                    newsItem.sentiment = "Neutral"; // Default, could add sentiment analysis

                    // Extract relevant tickers from title/content
                    for (const auto& ticker : tickers) {
                        if (newsItem.title.find(ticker) != std::string::npos ||
                            newsItem.content.find(ticker) != std::string::npos) {
                            newsItem.tickers.push_back(ticker);
                        }
                    }

                    news.push_back(newsItem);
                }
            }
        }
    } catch (const std::exception& e) {
        lastError_ = "News fetch error: " + std::string(e.what());
    }

    return news;
}

// Alpha Vantage Provider Implementation
AlphaVantageProvider::AlphaVantageProvider(const std::string& apiKey)
    : apiKey_(apiKey), healthy_(false) {
    httpClient_ = std::make_unique<Http::HttpClient>();
    httpClient_->setUserAgent("CryptoClaude/1.0");
    healthy_ = testConnection();
}

bool AlphaVantageProvider::testConnection() {
    try {
        Http::HttpRequest request(Http::HttpMethod::GET,
            "https://www.alphavantage.co/query");
        request.addQueryParam("function", "CURRENCY_EXCHANGE_RATE")
              .addQueryParam("from_currency", "BTC")
              .addQueryParam("to_currency", "USD")
              .addQueryParam("apikey", apiKey_);

        auto response = httpClient_->execute(request);

        if (response.isSuccess() && response.getStatusCode() == 200) {
            auto json = Json::JsonHelper::parse(response.getBody());

            if (json.contains("Realtime Currency Exchange Rate")) {
                healthy_ = true;
                lastError_ = "";
                return true;
            } else if (json.contains("Error Message")) {
                healthy_ = false;
                lastError_ = Json::JsonHelper::getString(json, "Error Message");
                return false;
            } else if (json.contains("Information")) {
                healthy_ = false;
                lastError_ = "Rate limit exceeded: " + Json::JsonHelper::getString(json, "Information");
                return false;
            }
        }

        healthy_ = false;
        lastError_ = "HTTP " + std::to_string(response.getStatusCode());
        return false;

    } catch (const std::exception& e) {
        healthy_ = false;
        lastError_ = "Connection error: " + std::string(e.what());
        return false;
    }
}

std::map<std::string, double> AlphaVantageProvider::getCurrentPrices(
    const std::vector<std::string>& symbols) {

    std::map<std::string, double> prices;

    if (!healthy_) {
        return prices;
    }

    // Alpha Vantage requires individual calls for each symbol
    for (const auto& symbol : symbols) {
        try {
            Http::HttpRequest request(Http::HttpMethod::GET,
                "https://www.alphavantage.co/query");
            request.addQueryParam("function", "CURRENCY_EXCHANGE_RATE")
                  .addQueryParam("from_currency", symbol)
                  .addQueryParam("to_currency", "USD")
                  .addQueryParam("apikey", apiKey_);

            auto response = httpClient_->execute(request);

            if (response.isSuccess()) {
                auto json = Json::JsonHelper::parse(response.getBody());

                if (json.contains("Realtime Currency Exchange Rate")) {
                    auto rateData = json["Realtime Currency Exchange Rate"];
                    double price = Json::JsonHelper::getDouble(rateData, "5. Exchange Rate");
                    prices[symbol] = price;
                }
            }

            // Rate limiting - Alpha Vantage allows 5 calls/minute for free tier
            std::this_thread::sleep_for(std::chrono::milliseconds(12000)); // 12 seconds between calls

        } catch (const std::exception& e) {
            lastError_ = "Price fetch error for " + symbol + ": " + std::string(e.what());
        }
    }

    return prices;
}

std::vector<PriceData> AlphaVantageProvider::getHistoricalData(
    const std::string& symbol, int days) {

    std::vector<PriceData> data;

    if (!healthy_) {
        return data;
    }

    try {
        Http::HttpRequest request(Http::HttpMethod::GET,
            "https://www.alphavantage.co/query");
        request.addQueryParam("function", "DIGITAL_CURRENCY_DAILY")
              .addQueryParam("symbol", symbol)
              .addQueryParam("market", "USD")
              .addQueryParam("apikey", apiKey_);

        auto response = httpClient_->execute(request);

        if (response.isSuccess()) {
            auto json = Json::JsonHelper::parse(response.getBody());

            if (json.contains("Time Series (Digital Currency Daily)")) {
                auto timeSeries = json["Time Series (Digital Currency Daily)"];
                int count = 0;

                for (auto it = timeSeries.begin(); it != timeSeries.end() && count < days; ++it, ++count) {
                    PriceData priceData;
                    priceData.symbol = symbol;
                    priceData.price = Json::JsonHelper::getDouble(it.value(), "4a. close (USD)");
                    priceData.volume24h = Json::JsonHelper::getDouble(it.value(), "5. volume");
                    // TODO: Parse date and calculate change
                    priceData.timestamp = std::chrono::system_clock::now();
                    priceData.provider = "AlphaVantage";

                    data.push_back(priceData);
                }
            }
        }
    } catch (const std::exception& e) {
        lastError_ = "Historical data error: " + std::string(e.what());
    }

    return data;
}

// Binance Provider Implementation
BinanceProvider::BinanceProvider() : healthy_(false) {
    httpClient_ = std::make_unique<Http::HttpClient>();
    httpClient_->setUserAgent("CryptoClaude/1.0");
    healthy_ = testConnection();
}

bool BinanceProvider::testConnection() {
    try {
        Http::HttpRequest request(Http::HttpMethod::GET,
            "https://api.binance.com/api/v3/ping");

        auto response = httpClient_->execute(request);

        if (response.isSuccess() && response.getStatusCode() == 200) {
            healthy_ = true;
            lastError_ = "";
            return true;
        } else {
            healthy_ = false;
            lastError_ = "HTTP " + std::to_string(response.getStatusCode());
            return false;
        }
    } catch (const std::exception& e) {
        healthy_ = false;
        lastError_ = "Connection error: " + std::string(e.what());
        return false;
    }
}

std::map<std::string, double> BinanceProvider::getCurrentPrices(
    const std::vector<std::string>& symbols) {

    std::map<std::string, double> prices;

    if (!healthy_) {
        return prices;
    }

    try {
        // Binance allows batch requests for multiple symbols
        std::vector<std::string> binanceSymbols;
        for (const auto& symbol : symbols) {
            binanceSymbols.push_back("\"" + symbol + "USDT\"");
        }

        std::string symbolsParam = "[" + std::accumulate(
            binanceSymbols.begin(), binanceSymbols.end(), std::string{},
            [](const std::string& a, const std::string& b) {
                return a.empty() ? b : a + "," + b;
            }) + "]";

        Http::HttpRequest request(Http::HttpMethod::GET,
            "https://api.binance.com/api/v3/ticker/price");
        request.addQueryParam("symbols", symbolsParam);

        auto response = httpClient_->execute(request);

        if (response.isSuccess()) {
            auto json = Json::JsonHelper::parse(response.getBody());

            if (json.is_array()) {
                for (const auto& item : json) {
                    std::string symbol = Json::JsonHelper::getString(item, "symbol");
                    double price = Json::JsonHelper::getDouble(item, "price");

                    // Convert BTCUSDT back to BTC
                    if (symbol.length() > 4 && symbol.substr(symbol.length() - 4) == "USDT") {
                        std::string baseSymbol = symbol.substr(0, symbol.length() - 4);
                        prices[baseSymbol] = price;
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        lastError_ = "Price fetch error: " + std::string(e.what());
    }

    return prices;
}

std::vector<PriceData> BinanceProvider::get24hStats(
    const std::vector<std::string>& symbols) {

    std::vector<PriceData> data;

    if (!healthy_) {
        return data;
    }

    try {
        Http::HttpRequest request(Http::HttpMethod::GET,
            "https://api.binance.com/api/v3/ticker/24hr");

        auto response = httpClient_->execute(request);

        if (response.isSuccess()) {
            auto json = Json::JsonHelper::parse(response.getBody());

            if (json.is_array()) {
                for (const auto& item : json) {
                    std::string symbol = Json::JsonHelper::getString(item, "symbol");

                    // Filter for our symbols
                    bool isRelevant = false;
                    std::string baseSymbol;
                    for (const auto& targetSymbol : symbols) {
                        if (symbol == targetSymbol + "USDT") {
                            isRelevant = true;
                            baseSymbol = targetSymbol;
                            break;
                        }
                    }

                    if (isRelevant) {
                        PriceData priceData;
                        priceData.symbol = baseSymbol;
                        priceData.price = Json::JsonHelper::getDouble(item, "lastPrice");
                        priceData.volume24h = Json::JsonHelper::getDouble(item, "volume");
                        priceData.change24h = Json::JsonHelper::getDouble(item, "priceChangePercent");
                        priceData.timestamp = std::chrono::system_clock::now();
                        priceData.provider = "Binance";

                        data.push_back(priceData);
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        lastError_ = "24h stats error: " + std::string(e.what());
    }

    return data;
}

// CryptoNews Provider Implementation
CryptoNewsProvider::CryptoNewsProvider(const std::string& apiToken)
    : apiToken_(apiToken), healthy_(false) {
    httpClient_ = std::make_unique<Http::HttpClient>();
    httpClient_->setUserAgent("CryptoClaude/1.0");
    healthy_ = testConnection();
}

bool CryptoNewsProvider::testConnection() {
    try {
        Http::HttpRequest request(Http::HttpMethod::GET,
            "https://cryptonews-api.com/api/v1");
        request.addQueryParam("tickers", "BTC")
              .addQueryParam("items", "1")
              .addQueryParam("page", "1")
              .addQueryParam("token", apiToken_);

        auto response = httpClient_->execute(request);

        if (response.isSuccess() && response.getStatusCode() == 200) {
            auto json = Json::JsonHelper::parse(response.getBody());

            if (json.contains("data") && json["data"].is_array() && !json["data"].empty()) {
                healthy_ = true;
                lastError_ = "";
                return true;
            } else {
                healthy_ = false;
                lastError_ = "No data returned or unexpected format";
                return false;
            }
        } else {
            healthy_ = false;
            lastError_ = "HTTP " + std::to_string(response.getStatusCode()) +
                        ": " + response.getBody();
            return false;
        }
    } catch (const std::exception& e) {
        healthy_ = false;
        lastError_ = "Connection error: " + std::string(e.what());
        return false;
    }
}

std::vector<NewsData> CryptoNewsProvider::getCryptoNews(
    const std::vector<std::string>& tickers, int limit) {

    std::vector<NewsData> news;

    if (!healthy_) {
        return news;
    }

    try {
        // Build tickers string
        std::string tickersParam;
        for (size_t i = 0; i < tickers.size(); ++i) {
            tickersParam += tickers[i];
            if (i < tickers.size() - 1) {
                tickersParam += ",";
            }
        }

        Http::HttpRequest request(Http::HttpMethod::GET,
            "https://cryptonews-api.com/api/v1");
        request.addQueryParam("tickers", tickersParam)
              .addQueryParam("items", std::to_string(std::min(limit, 100)))
              .addQueryParam("page", "1")
              .addQueryParam("token", apiToken_);

        auto response = httpClient_->execute(request);

        if (response.isSuccess()) {
            auto json = Json::JsonHelper::parse(response.getBody());

            if (json.contains("data") && json["data"].is_array()) {
                for (const auto& article : json["data"]) {
                    NewsData newsItem;
                    newsItem.title = Json::JsonHelper::getString(article, "title");
                    newsItem.content = Json::JsonHelper::getString(article, "text");
                    newsItem.url = Json::JsonHelper::getString(article, "news_url");
                    newsItem.source = Json::JsonHelper::getString(article, "source_name");

                    // Parse date - CryptoNews format: "Fri, 26 Sep 2025 09:02:04 -0400"
                    std::string dateStr = Json::JsonHelper::getString(article, "date");
                    // TODO: Parse RFC2822 timestamp
                    newsItem.publishedAt = std::chrono::system_clock::now();

                    newsItem.provider = "CryptoNews";
                    newsItem.sentiment = Json::JsonHelper::getString(article, "sentiment");

                    // Get tickers array
                    if (article.contains("tickers") && article["tickers"].is_array()) {
                        for (const auto& ticker : article["tickers"]) {
                            if (ticker.is_string()) {
                                newsItem.tickers.push_back(ticker.get<std::string>());
                            }
                        }
                    }

                    news.push_back(newsItem);
                }
            }
        }
    } catch (const std::exception& e) {
        lastError_ = "News fetch error: " + std::string(e.what());
    }

    return news;
}

// LiveDataManager Implementation
LiveDataManager::LiveDataManager(const std::string& newsApiKey,
                                const std::string& alphaVantageApiKey,
                                const std::string& cryptoNewsToken)
    : enableNewsApi_(true), enableAlphaVantage_(true), enableBinance_(true), enableCryptoNews_(true) {

    newsProvider_ = std::make_unique<NewsApiProvider>(newsApiKey);
    alphaVantageProvider_ = std::make_unique<AlphaVantageProvider>(alphaVantageApiKey);
    binanceProvider_ = std::make_unique<BinanceProvider>();
    cryptoNewsProvider_ = std::make_unique<CryptoNewsProvider>(cryptoNewsToken);

    lastHealthCheck_ = std::chrono::system_clock::now();
}

void LiveDataManager::enableProvider(const std::string& providerName, bool enable) {
    if (providerName == "NewsAPI") {
        enableNewsApi_ = enable;
    } else if (providerName == "AlphaVantage") {
        enableAlphaVantage_ = enable;
    } else if (providerName == "Binance") {
        enableBinance_ = enable;
    } else if (providerName == "CryptoNews") {
        enableCryptoNews_ = enable;
    }
}

std::vector<std::string> LiveDataManager::getHealthyProviders() const {
    std::vector<std::string> healthy;

    if (enableNewsApi_ && newsProvider_->isHealthy()) {
        healthy.push_back("NewsAPI");
    }
    if (enableAlphaVantage_ && alphaVantageProvider_->isHealthy()) {
        healthy.push_back("AlphaVantage");
    }
    if (enableBinance_ && binanceProvider_->isHealthy()) {
        healthy.push_back("Binance");
    }
    if (enableCryptoNews_ && cryptoNewsProvider_->isHealthy()) {
        healthy.push_back("CryptoNews");
    }

    return healthy;
}

std::map<std::string, std::string> LiveDataManager::getProviderStatus() const {
    std::map<std::string, std::string> status;

    status["NewsAPI"] = newsProvider_->isHealthy() ? "Healthy" : newsProvider_->getLastError();
    status["AlphaVantage"] = alphaVantageProvider_->isHealthy() ? "Healthy" : alphaVantageProvider_->getLastError();
    status["Binance"] = binanceProvider_->isHealthy() ? "Healthy" : binanceProvider_->getLastError();
    status["CryptoNews"] = cryptoNewsProvider_->isHealthy() ? "Healthy" : cryptoNewsProvider_->getLastError();

    return status;
}

std::vector<PriceData> LiveDataManager::getCurrentPrices(
    const std::vector<std::string>& symbols) {

    std::vector<PriceData> allPrices;

    // Prefer Binance for current prices (fastest, no API key required)
    if (enableBinance_ && binanceProvider_->isHealthy()) {
        auto binancePrices = binanceProvider_->get24hStats(symbols);
        allPrices.insert(allPrices.end(), binancePrices.begin(), binancePrices.end());
    }

    // Alpha Vantage as fallback (slower due to rate limits)
    if (allPrices.empty() && enableAlphaVantage_ && alphaVantageProvider_->isHealthy()) {
        auto avPrices = alphaVantageProvider_->getCurrentPrices(symbols);
        for (const auto& [symbol, price] : avPrices) {
            PriceData priceData;
            priceData.symbol = symbol;
            priceData.price = price;
            priceData.timestamp = std::chrono::system_clock::now();
            priceData.provider = "AlphaVantage";
            allPrices.push_back(priceData);
        }
    }

    return allPrices;
}

std::vector<NewsData> LiveDataManager::getLatestNews(
    const std::vector<std::string>& tickers, int limit) {

    std::vector<NewsData> allNews;

    // Get news from both providers
    if (enableNewsApi_ && newsProvider_->isHealthy()) {
        auto news = newsProvider_->getCryptoNews(tickers, limit / 2);
        allNews.insert(allNews.end(), news.begin(), news.end());
    }

    if (enableCryptoNews_ && cryptoNewsProvider_->isHealthy()) {
        auto news = cryptoNewsProvider_->getCryptoNews(tickers, limit / 2);
        allNews.insert(allNews.end(), news.begin(), news.end());
    }

    return allNews;
}

void LiveDataManager::performHealthCheck() {
    auto now = std::chrono::system_clock::now();
    if (now - lastHealthCheck_ < healthCheckInterval_) {
        return; // Too soon for another check
    }

    if (enableNewsApi_) {
        newsProvider_->testConnection();
    }
    if (enableAlphaVantage_) {
        alphaVantageProvider_->testConnection();
    }
    if (enableBinance_) {
        binanceProvider_->testConnection();
    }
    if (enableCryptoNews_) {
        cryptoNewsProvider_->testConnection();
    }

    lastHealthCheck_ = now;
}

bool LiveDataManager::isHealthy() const {
    int healthyCount = 0;
    int enabledCount = 0;

    if (enableNewsApi_) {
        enabledCount++;
        if (newsProvider_->isHealthy()) healthyCount++;
    }
    if (enableAlphaVantage_) {
        enabledCount++;
        if (alphaVantageProvider_->isHealthy()) healthyCount++;
    }
    if (enableBinance_) {
        enabledCount++;
        if (binanceProvider_->isHealthy()) healthyCount++;
    }
    if (enableCryptoNews_) {
        enabledCount++;
        if (cryptoNewsProvider_->isHealthy()) healthyCount++;
    }

    // Consider healthy if at least 3/4 of enabled providers are working
    return enabledCount > 0 && (double)healthyCount / enabledCount >= 0.75;
}

std::string LiveDataManager::getHealthReport() const {
    std::ostringstream report;
    report << "Live Data Manager Health Report:\n";

    auto status = getProviderStatus();
    for (const auto& [provider, health] : status) {
        report << "  " << provider << ": " << health << "\n";
    }

    auto healthyProviders = getHealthyProviders();
    report << "Healthy providers: " << healthyProviders.size() << "/3\n";
    report << "Overall status: " << (isHealthy() ? "HEALTHY" : "DEGRADED") << "\n";

    return report.str();
}

bool LiveDataManager::testAllConnections() {
    bool allGood = true;

    std::cout << "Testing NewsAPI connection..." << std::endl;
    if (!newsProvider_->testConnection()) {
        std::cout << "  FAILED: " << newsProvider_->getLastError() << std::endl;
        allGood = false;
    } else {
        std::cout << "  OK" << std::endl;
    }

    std::cout << "Testing Alpha Vantage connection..." << std::endl;
    if (!alphaVantageProvider_->testConnection()) {
        std::cout << "  FAILED: " << alphaVantageProvider_->getLastError() << std::endl;
        allGood = false;
    } else {
        std::cout << "  OK" << std::endl;
    }

    std::cout << "Testing Binance connection..." << std::endl;
    if (!binanceProvider_->testConnection()) {
        std::cout << "  FAILED: " << binanceProvider_->getLastError() << std::endl;
        allGood = false;
    } else {
        std::cout << "  OK" << std::endl;
    }

    std::cout << "Testing CryptoNews connection..." << std::endl;
    if (!cryptoNewsProvider_->testConnection()) {
        std::cout << "  FAILED: " << cryptoNewsProvider_->getLastError() << std::endl;
        allGood = false;
    } else {
        std::cout << "  OK" << std::endl;
    }

    return allGood;
}

} // namespace Data
} // namespace CryptoClaude