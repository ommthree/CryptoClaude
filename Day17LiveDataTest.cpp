#include "src/Core/Data/LiveDataManager.h"
#include <iostream>
#include <vector>

int main() {
    std::cout << "=== Day 17 Live Data Integration Test ===" << std::endl;
    std::cout << "Testing all 4 API providers..." << std::endl << std::endl;

    // Initialize with API keys
    CryptoClaude::Data::LiveDataManager dataManager(
        "27c2ee8b2b4c4d3693a5650e8b6e5fb2",              // NewsAPI key
        "L6Y7WECGSWZFHHGX",                              // Alpha Vantage key
        getenv("CRYPTONEWS_API_KEY") ? getenv("CRYPTONEWS_API_KEY") : "CRYPTONEWS_KEY_NOT_SET"       // CryptoNews token
    );

    // Test all connections
    std::cout << "=== Connection Testing ===" << std::endl;
    bool allConnected = dataManager.testAllConnections();
    std::cout << std::endl;

    // Health report
    std::cout << "=== Provider Health Report ===" << std::endl;
    std::cout << dataManager.getHealthReport() << std::endl;

    // Test current prices from Binance
    std::cout << "=== Current Prices Test (Binance) ===" << std::endl;
    std::vector<std::string> testSymbols = {"BTC", "ETH", "ADA", "DOT", "LINK"};
    auto prices = dataManager.getCurrentPrices(testSymbols);

    if (!prices.empty()) {
        std::cout << "Successfully retrieved " << prices.size() << " price points:" << std::endl;
        for (const auto& price : prices) {
            std::cout << "  " << price.symbol << ": $" << price.price
                      << " (24h change: " << price.change24h << "%)"
                      << " [" << price.provider << "]" << std::endl;
        }
    } else {
        std::cout << "No price data retrieved." << std::endl;
    }
    std::cout << std::endl;

    // Test news from NewsAPI
    std::cout << "=== Latest News Test (NewsAPI) ===" << std::endl;
    auto news = dataManager.getLatestNews(testSymbols, 3);

    if (!news.empty()) {
        std::cout << "Successfully retrieved " << news.size() << " news items:" << std::endl;
        for (const auto& article : news) {
            std::cout << "  Title: " << article.title << std::endl;
            std::cout << "  Source: " << article.source << " [" << article.provider << "]" << std::endl;
            std::cout << "  URL: " << article.url << std::endl << std::endl;
        }
    } else {
        std::cout << "No news data retrieved." << std::endl;
    }

    // Overall status
    std::cout << "=== Overall Integration Status ===" << std::endl;
    if (dataManager.isHealthy()) {
        std::cout << "✅ Live data integration is HEALTHY" << std::endl;
        std::cout << "✅ Ready for Day 17 algorithm optimization with live data" << std::endl;
    } else {
        std::cout << "⚠️  Live data integration is DEGRADED" << std::endl;
        std::cout << "   Some providers are not working optimally" << std::endl;
    }

    auto healthyProviders = dataManager.getHealthyProviders();
    std::cout << "Working providers: " << healthyProviders.size() << "/4 (";
    for (size_t i = 0; i < healthyProviders.size(); ++i) {
        std::cout << healthyProviders[i];
        if (i < healthyProviders.size() - 1) std::cout << ", ";
    }
    std::cout << ")" << std::endl;

    return allConnected ? 0 : 1;
}