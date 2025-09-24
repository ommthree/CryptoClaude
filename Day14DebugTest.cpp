#include <iostream>
#include "src/Core/Risk/PredictiveRiskEngine.h"

using namespace CryptoClaude::Risk;
using namespace CryptoClaude::Database::Models;

MarketData createTestMarketData(const std::string& symbol, double price, double volume) {
    auto timestamp = std::chrono::system_clock::now();
    return MarketData(symbol, timestamp, price, volume * 0.6, volume * 0.4);
}

int main() {
    std::cout << "Debug: Testing why BTC is being excluded..." << std::endl;

    CoinExclusionEngine engine;

    // Add BTC with excellent metrics
    auto btcData = createTestMarketData("BTC", 45000, 1000e6);
    engine.updateMarketData("BTC", btcData);

    std::cout << "BTC Data:" << std::endl;
    std::cout << "  Price: " << btcData.getClose() << std::endl;
    std::cout << "  Volume: " << btcData.getTotalVolume() << std::endl;

    // Check each exclusion type
    auto liquidityAlert = engine.checkLiquidityExclusion("BTC");
    std::cout << "\nLiquidity Check:" << std::endl;
    std::cout << "  Should exclude: " << (liquidityAlert.shouldExclude ? "YES" : "NO") << std::endl;
    std::cout << "  Alert level: " << static_cast<int>(liquidityAlert.level) << std::endl;
    std::cout << "  Message: " << liquidityAlert.message << std::endl;
    std::cout << "  Exclusion score: " << liquidityAlert.exclusionScore << std::endl;

    auto volatilityAlert = engine.checkVolatilityExclusion("BTC");
    std::cout << "\nVolatility Check:" << std::endl;
    std::cout << "  Should exclude: " << (volatilityAlert.shouldExclude ? "YES" : "NO") << std::endl;
    std::cout << "  Alert level: " << static_cast<int>(volatilityAlert.level) << std::endl;
    std::cout << "  Message: " << volatilityAlert.message << std::endl;
    std::cout << "  Exclusion score: " << volatilityAlert.exclusionScore << std::endl;

    auto confidenceAlert = engine.checkModelConfidenceExclusion("BTC");
    std::cout << "\nModel Confidence Check:" << std::endl;
    std::cout << "  Should exclude: " << (confidenceAlert.shouldExclude ? "YES" : "NO") << std::endl;
    std::cout << "  Alert level: " << static_cast<int>(confidenceAlert.level) << std::endl;
    std::cout << "  Message: " << confidenceAlert.message << std::endl;
    std::cout << "  Exclusion score: " << confidenceAlert.exclusionScore << std::endl;

    auto technicalAlert = engine.checkTechnicalExclusion("BTC");
    std::cout << "\nTechnical Check:" << std::endl;
    std::cout << "  Should exclude: " << (technicalAlert.shouldExclude ? "YES" : "NO") << std::endl;
    std::cout << "  Alert level: " << static_cast<int>(technicalAlert.level) << std::endl;
    std::cout << "  Message: " << technicalAlert.message << std::endl;
    std::cout << "  Exclusion score: " << technicalAlert.exclusionScore << std::endl;

    auto newsAlert = engine.checkNewsEventExclusion("BTC");
    std::cout << "\nNews Event Check:" << std::endl;
    std::cout << "  Should exclude: " << (newsAlert.shouldExclude ? "YES" : "NO") << std::endl;
    std::cout << "  Alert level: " << static_cast<int>(newsAlert.level) << std::endl;
    std::cout << "  Message: " << newsAlert.message << std::endl;
    std::cout << "  Exclusion score: " << newsAlert.exclusionScore << std::endl;

    // Overall exclusion decision
    bool shouldExclude = engine.shouldExcludeCoin("BTC");
    std::cout << "\nOverall Decision:" << std::endl;
    std::cout << "  Should exclude BTC: " << (shouldExclude ? "YES" : "NO") << std::endl;

    return 0;
}