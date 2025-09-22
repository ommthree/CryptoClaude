#pragma once
#include <string>
#include <vector>
#include <chrono>
#include "../Database/Models/MarketData.h"
#include "../Database/Models/SentimentData.h"

namespace CryptoClaude {
namespace DataIngestion {

// Abstract base class for all data providers
class IDataProvider {
public:
    virtual ~IDataProvider() = default;

    // Provider information
    virtual std::string getName() const = 0;
    virtual std::string getDescription() const = 0;
    virtual bool isAvailable() const = 0;

    // Rate limiting and status
    virtual int getRateLimitPerMinute() const = 0;
    virtual int getRemainingRequests() const = 0;
    virtual std::chrono::system_clock::time_point getRateLimitReset() const = 0;

    // Error handling
    virtual std::string getLastError() const = 0;
    virtual bool hasError() const = 0;
    virtual void clearError() = 0;
};

// Interface for market data providers (CryptoCompare, etc.)
class IMarketDataProvider : public IDataProvider {
public:
    virtual ~IMarketDataProvider() = default;

    // Basic market data retrieval
    virtual std::vector<Database::Models::MarketData> getDailyData(
        const std::string& symbol,
        const std::string& currency = "USD",
        int days = 365) = 0;

    virtual std::vector<Database::Models::MarketData> getHourlyData(
        const std::string& symbol,
        const std::string& currency = "USD",
        int hours = 24 * 30) = 0;

    // Batch operations
    virtual std::vector<Database::Models::MarketData> getDailyDataBatch(
        const std::vector<std::string>& symbols,
        const std::string& currency = "USD",
        int days = 365) = 0;

    // Market cap and coin information
    virtual std::vector<Database::Models::MarketCapData> getTopCoinsByMarketCap(int count = 100) = 0;
    virtual Database::Models::MarketCapData getMarketCap(const std::string& symbol) = 0;

    // Supported symbols
    virtual std::vector<std::string> getSupportedSymbols() = 0;
    virtual bool isSymbolSupported(const std::string& symbol) = 0;

    // Historical data availability
    virtual std::chrono::system_clock::time_point getEarliestDataDate(const std::string& symbol) = 0;
    virtual std::chrono::system_clock::time_point getLatestDataDate(const std::string& symbol) = 0;
};

// Interface for sentiment data providers (Crypto News API, etc.)
class ISentimentDataProvider : public IDataProvider {
public:
    virtual ~ISentimentDataProvider() = default;

    // News and sentiment retrieval
    virtual std::vector<Database::Models::NewsArticle> getNews(
        const std::vector<std::string>& tickers,
        const std::string& startDate,
        const std::string& endDate,
        int maxArticles = 100) = 0;

    virtual std::vector<Database::Models::SentimentData> getSentimentData(
        const std::vector<std::string>& tickers,
        const std::string& startDate,
        const std::string& endDate) = 0;

    // News sources
    virtual std::vector<Database::Models::NewsSource> getNewsSources() = 0;
    virtual bool isSourceSupported(const std::string& sourceName) = 0;

    // Sentiment analysis
    virtual double analyzeSentiment(const std::string& text) = 0;
    virtual std::string getSentimentCategory(double score) = 0; // "Positive", "Neutral", "Negative"

    // Supported tickers
    virtual std::vector<std::string> getSupportedTickers() = 0;
    virtual bool isTickerSupported(const std::string& ticker) = 0;
};

// Factory class for creating data providers
class DataProviderFactory {
public:
    enum class MarketDataProvider {
        CryptoCompare,
        CoinGecko,
        Binance
    };

    enum class SentimentDataProvider {
        CryptoNewsAPI,
        NewsAPI,
        TwitterAPI
    };

    static std::unique_ptr<IMarketDataProvider> createMarketDataProvider(
        MarketDataProvider provider,
        const std::string& apiKey = "");

    static std::unique_ptr<ISentimentDataProvider> createSentimentDataProvider(
        SentimentDataProvider provider,
        const std::string& apiKey = "");

    static std::vector<std::string> getAvailableMarketDataProviders();
    static std::vector<std::string> getAvailableSentimentDataProviders();
};

} // namespace DataIngestion
} // namespace CryptoClaude