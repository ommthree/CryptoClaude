#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <chrono>
#include <functional>
#include "../Http/HttpClient.h"

namespace CryptoClaude {
namespace Data {

// Live data provider interface
class ILiveDataProvider {
public:
    virtual ~ILiveDataProvider() = default;
    virtual std::string getProviderName() const = 0;
    virtual bool isHealthy() const = 0;
    virtual std::string getLastError() const = 0;
};

// Price data structure
struct PriceData {
    std::string symbol;
    double price;
    double volume24h;
    double change24h;
    std::chrono::system_clock::time_point timestamp;
    std::string provider;
};

// News data structure
struct NewsData {
    std::string title;
    std::string content;
    std::string url;
    std::string source;
    std::chrono::system_clock::time_point publishedAt;
    std::vector<std::string> tickers;
    std::string sentiment;
    std::string provider;
};

// NewsAPI provider
class NewsApiProvider : public ILiveDataProvider {
private:
    std::unique_ptr<Http::IHttpClient> httpClient_;
    std::string apiKey_;
    std::string lastError_;
    bool healthy_;

public:
    explicit NewsApiProvider(const std::string& apiKey);

    std::string getProviderName() const override { return "NewsAPI"; }
    bool isHealthy() const override { return healthy_; }
    std::string getLastError() const override { return lastError_; }

    std::vector<NewsData> getCryptoNews(const std::vector<std::string>& tickers, int limit = 10);
    bool testConnection();
};

// Alpha Vantage provider
class AlphaVantageProvider : public ILiveDataProvider {
private:
    std::unique_ptr<Http::IHttpClient> httpClient_;
    std::string apiKey_;
    std::string lastError_;
    bool healthy_;

public:
    explicit AlphaVantageProvider(const std::string& apiKey);

    std::string getProviderName() const override { return "AlphaVantage"; }
    bool isHealthy() const override { return healthy_; }
    std::string getLastError() const override { return lastError_; }

    std::map<std::string, double> getCurrentPrices(const std::vector<std::string>& symbols);
    std::vector<PriceData> getHistoricalData(const std::string& symbol, int days = 30);
    bool testConnection();
};

// Binance provider
class BinanceProvider : public ILiveDataProvider {
private:
    std::unique_ptr<Http::IHttpClient> httpClient_;
    std::string lastError_;
    bool healthy_;

public:
    BinanceProvider();

    std::string getProviderName() const override { return "Binance"; }
    bool isHealthy() const override { return healthy_; }
    std::string getLastError() const override { return lastError_; }

    std::map<std::string, double> getCurrentPrices(const std::vector<std::string>& symbols);
    std::vector<PriceData> get24hStats(const std::vector<std::string>& symbols);
    bool testConnection();
};

// CryptoNews provider
class CryptoNewsProvider : public ILiveDataProvider {
private:
    std::unique_ptr<Http::IHttpClient> httpClient_;
    std::string apiToken_;
    std::string lastError_;
    bool healthy_;

public:
    explicit CryptoNewsProvider(const std::string& apiToken);

    std::string getProviderName() const override { return "CryptoNews"; }
    bool isHealthy() const override { return healthy_; }
    std::string getLastError() const override { return lastError_; }

    std::vector<NewsData> getCryptoNews(const std::vector<std::string>& tickers, int limit = 10);
    bool testConnection();
};

// Main live data manager
class LiveDataManager {
private:
    std::unique_ptr<NewsApiProvider> newsProvider_;
    std::unique_ptr<AlphaVantageProvider> alphaVantageProvider_;
    std::unique_ptr<BinanceProvider> binanceProvider_;
    std::unique_ptr<CryptoNewsProvider> cryptoNewsProvider_;

    // Configuration
    bool enableNewsApi_;
    bool enableAlphaVantage_;
    bool enableBinance_;
    bool enableCryptoNews_;

    // Health monitoring
    std::chrono::system_clock::time_point lastHealthCheck_;
    std::chrono::minutes healthCheckInterval_{5}; // Check every 5 minutes

public:
    LiveDataManager(const std::string& newsApiKey, const std::string& alphaVantageApiKey,
                   const std::string& cryptoNewsToken);

    // Provider management
    void enableProvider(const std::string& providerName, bool enable);
    std::vector<std::string> getHealthyProviders() const;
    std::map<std::string, std::string> getProviderStatus() const;

    // Data retrieval
    std::vector<PriceData> getCurrentPrices(const std::vector<std::string>& symbols);
    std::vector<NewsData> getLatestNews(const std::vector<std::string>& tickers, int limit = 20);

    // Health monitoring
    void performHealthCheck();
    bool isHealthy() const;
    std::string getHealthReport() const;

    // Testing
    bool testAllConnections();
};

} // namespace Data
} // namespace CryptoClaude