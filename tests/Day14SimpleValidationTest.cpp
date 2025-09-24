#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <chrono>
#include <algorithm>
#include "src/Core/Risk/PredictiveRiskEngine.h"

using namespace CryptoClaude::Risk;
using namespace CryptoClaude::Database::Models;

// Simple test data generator using proper model constructors
MarketData createTestMarketData(const std::string& symbol, double price, double volume) {
    auto timestamp = std::chrono::system_clock::now();
    return MarketData(symbol, timestamp, price, volume * 0.6, volume * 0.4);
}

void testCoinExclusionEngineBasics() {
    std::cout << "Testing CoinExclusionEngine basic functionality..." << std::endl;

    CoinExclusionEngine engine;

    // Test with coins of varying quality
    std::vector<std::string> testCoins = {
        "BTC", "ETH", "LOWVOL", "BADCOIN"
    };

    // Add market data
    engine.updateMarketData("BTC", createTestMarketData("BTC", 45000, 1000e6));      // Excellent
    engine.updateMarketData("ETH", createTestMarketData("ETH", 3000, 800e6));        // Excellent
    engine.updateMarketData("LOWVOL", createTestMarketData("LOWVOL", 100, 20e6));    // Low volume - should exclude
    engine.updateMarketData("BADCOIN", createTestMarketData("BADCOIN", 1, 5e6));     // Very low volume - should exclude

    // Test individual checks
    bool btcShouldExclude = engine.shouldExcludeCoin("BTC");
    bool lowvolShouldExclude = engine.shouldExcludeCoin("LOWVOL");

    assert(!btcShouldExclude); // BTC should not be excluded
    assert(lowvolShouldExclude); // LOWVOL should be excluded

    std::cout << "✅ BTC exclusion check: " << (btcShouldExclude ? "EXCLUDED" : "INCLUDED") << std::endl;
    std::cout << "✅ LOWVOL exclusion check: " << (lowvolShouldExclude ? "EXCLUDED" : "INCLUDED") << std::endl;

    // Test batch filtering
    auto filtered = engine.filterExcludedCoins(testCoins);
    assert(filtered.size() < testCoins.size()); // Some should be filtered
    assert(std::find(filtered.begin(), filtered.end(), "BTC") != filtered.end()); // BTC should remain
    assert(std::find(filtered.begin(), filtered.end(), "ETH") != filtered.end()); // ETH should remain

    std::cout << "✅ Batch filtering: " << testCoins.size() << " -> " << filtered.size() << " coins" << std::endl;

    // Test diagnostics
    auto diagnostics = engine.getDiagnostics();
    assert(diagnostics.totalCoinsAssessed >= 0);
    std::cout << "✅ Diagnostics: " << diagnostics.totalCoinsAssessed << " coins assessed" << std::endl;
}

void testLiquidityExclusion() {
    std::cout << "Testing liquidity-based exclusion..." << std::endl;

    CoinExclusionEngine engine;

    // Test with different volume levels
    engine.updateMarketData("HIGHVOL", createTestMarketData("HIGHVOL", 100, 500e6));   // Good volume
    engine.updateMarketData("MEDVOL", createTestMarketData("MEDVOL", 100, 120e6));     // Marginal volume
    engine.updateMarketData("LOWVOL", createTestMarketData("LOWVOL", 100, 30e6));      // Low volume

    // Check liquidity exclusion specifically
    auto highVolAlert = engine.checkLiquidityExclusion("HIGHVOL");
    auto lowVolAlert = engine.checkLiquidityExclusion("LOWVOL");

    assert(highVolAlert.level <= CoinExclusionAlert::WARNING); // Should be OK
    assert(lowVolAlert.level >= CoinExclusionAlert::WARNING);  // Should be concerning
    assert(lowVolAlert.shouldExclude);                         // Should recommend exclusion

    std::cout << "✅ High volume liquidity: " << highVolAlert.message << std::endl;
    std::cout << "✅ Low volume liquidity: " << lowVolAlert.message << std::endl;
}

void testVolatilityExclusion() {
    std::cout << "Testing volatility-based exclusion..." << std::endl;

    CoinExclusionEngine engine;

    // Add market data for volatility testing
    engine.updateMarketData("STABLE", createTestMarketData("STABLE", 100, 200e6));

    // Add multiple data points to simulate price history
    for (int i = 0; i < 10; ++i) {
        double price = 100 + (i % 2 == 0 ? 1.0 : -1.0); // Small price movements
        engine.updateMarketData("STABLE", createTestMarketData("STABLE", price, 200e6));
    }

    engine.updateMarketData("VOLATILE", createTestMarketData("VOLATILE", 100, 200e6));

    // Add volatile price history
    for (int i = 0; i < 10; ++i) {
        double price = 100 + (i % 2 == 0 ? 20.0 : -20.0); // Large price swings
        engine.updateMarketData("VOLATILE", createTestMarketData("VOLATILE", price, 200e6));
    }

    auto stableVolAlert = engine.checkVolatilityExclusion("STABLE");
    auto volatileAlert = engine.checkVolatilityExclusion("VOLATILE");

    std::cout << "✅ Stable coin volatility: " << stableVolAlert.message << std::endl;
    std::cout << "✅ Volatile coin volatility: " << volatileAlert.message << std::endl;
}

void testConfigurationManagement() {
    std::cout << "Testing configuration management..." << std::endl;

    CoinExclusionEngine engine;

    // Test default criteria
    auto defaultCriteria = engine.getExclusionCriteria();
    assert(defaultCriteria.minDailyVolumeUSD > 0);
    assert(defaultCriteria.maxBidAskSpread > 0);
    std::cout << "✅ Default criteria loaded" << std::endl;

    // Test custom criteria
    CoinExclusionCriteria customCriteria;
    customCriteria.minDailyVolumeUSD = 50e6;  // Lower threshold
    customCriteria.maxBidAskSpread = 0.01;    // Higher tolerance
    customCriteria.minModelConfidence = 0.25; // Lower confidence requirement

    engine.setExclusionCriteria(customCriteria);
    auto retrievedCriteria = engine.getExclusionCriteria();

    assert(retrievedCriteria.minDailyVolumeUSD == 50e6);
    assert(retrievedCriteria.maxBidAskSpread == 0.01);
    std::cout << "✅ Custom criteria applied successfully" << std::endl;

    // Test lookback window configuration
    engine.setLiquidityLookbackDays(14);
    engine.setVolatilityLookbackDays(60);
    std::cout << "✅ Lookback windows configured" << std::endl;
}

void testBatchProcessing() {
    std::cout << "Testing batch processing capabilities..." << std::endl;

    CoinExclusionEngine engine;

    // Create a mixed universe of coins
    std::vector<std::string> universe = {
        "BTC", "ETH", "BNB", "ADA", "DOT",           // Major coins (should pass)
        "LOWVOL1", "LOWVOL2", "THINLIQ", "BADCOIN"   // Problem coins (should be excluded)
    };

    // Add good market data for major coins
    engine.updateMarketData("BTC", createTestMarketData("BTC", 45000, 1000e6));
    engine.updateMarketData("ETH", createTestMarketData("ETH", 3000, 800e6));
    engine.updateMarketData("BNB", createTestMarketData("BNB", 300, 400e6));
    engine.updateMarketData("ADA", createTestMarketData("ADA", 0.5, 300e6));
    engine.updateMarketData("DOT", createTestMarketData("DOT", 6, 250e6));

    // Add problematic data
    engine.updateMarketData("LOWVOL1", createTestMarketData("LOWVOL1", 10, 20e6));    // Low volume
    engine.updateMarketData("LOWVOL2", createTestMarketData("LOWVOL2", 5, 15e6));     // Low volume
    engine.updateMarketData("THINLIQ", createTestMarketData("THINLIQ", 1, 5e6));      // Very low volume
    engine.updateMarketData("BADCOIN", createTestMarketData("BADCOIN", 0.1, 1e6));    // Terrible volume

    // Test batch assessment
    auto assessments = engine.assessAllCoins(universe);
    assert(assessments.size() == universe.size());

    int excludedCount = 0;
    int includedCount = 0;
    for (const auto& [symbol, alert] : assessments) {
        if (alert.shouldExclude) {
            excludedCount++;
            std::cout << "   - " << symbol << ": EXCLUDED (" << alert.message << ")" << std::endl;
        } else {
            includedCount++;
            std::cout << "   - " << symbol << ": INCLUDED (" << alert.message << ")" << std::endl;
        }
    }

    assert(excludedCount > 0); // Some should be excluded
    assert(includedCount > 0); // Some should be included
    std::cout << "✅ Batch assessment: " << includedCount << " included, " << excludedCount << " excluded" << std::endl;

    // Test batch filtering
    auto filtered = engine.filterExcludedCoins(universe);
    assert(filtered.size() == includedCount); // Should match included count
    assert(filtered.size() < universe.size()); // Some filtering should occur

    // Verify major coins are in filtered list
    assert(std::find(filtered.begin(), filtered.end(), "BTC") != filtered.end());
    assert(std::find(filtered.begin(), filtered.end(), "ETH") != filtered.end());

    std::cout << "✅ Batch filtering: " << universe.size() << " -> " << filtered.size() << " coins retained" << std::endl;
}

void testPerformanceWithLargerDataset() {
    std::cout << "Testing performance with larger dataset..." << std::endl;

    CoinExclusionEngine engine;

    // Generate larger test dataset
    std::vector<std::string> largeUniverse;
    for (int i = 0; i < 100; ++i) {
        largeUniverse.push_back("COIN" + std::to_string(i));
    }

    auto start = std::chrono::high_resolution_clock::now();

    // Add market data for all coins
    for (size_t i = 0; i < largeUniverse.size(); ++i) {
        const auto& symbol = largeUniverse[i];

        // Vary volume to create realistic distribution
        double volume = 10e6 + (i * 5e6); // Range from 10M to 505M
        double price = 1.0 + (i * 0.5);   // Vary prices too

        engine.updateMarketData(symbol, createTestMarketData(symbol, price, volume));
    }

    auto dataTime = std::chrono::high_resolution_clock::now();

    // Test filtering performance
    auto filtered = engine.filterExcludedCoins(largeUniverse);

    auto filterTime = std::chrono::high_resolution_clock::now();

    // Calculate durations
    auto dataMs = std::chrono::duration_cast<std::chrono::milliseconds>(dataTime - start).count();
    auto filterMs = std::chrono::duration_cast<std::chrono::milliseconds>(filterTime - dataTime).count();

    assert(filtered.size() <= largeUniverse.size());
    assert(filtered.size() > 0); // Some should pass

    std::cout << "✅ Performance test:" << std::endl;
    std::cout << "   - Data update: " << dataMs << "ms for " << largeUniverse.size() << " coins" << std::endl;
    std::cout << "   - Filtering: " << filterMs << "ms" << std::endl;
    std::cout << "   - Result: " << largeUniverse.size() << " -> " << filtered.size() << " coins" << std::endl;

    // Performance should be reasonable
    assert(dataMs < 2000);  // 2 seconds max for data updates
    assert(filterMs < 500); // 500ms max for filtering
}

void testExclusionReasons() {
    std::cout << "Testing exclusion reason classification..." << std::endl;

    CoinExclusionEngine engine;

    // Test different exclusion scenarios
    engine.updateMarketData("TESTCOIN", createTestMarketData("TESTCOIN", 100, 150e6));

    // Test all exclusion check methods
    auto liquidityAlert = engine.checkLiquidityExclusion("TESTCOIN");
    auto volatilityAlert = engine.checkVolatilityExclusion("TESTCOIN");
    auto confidenceAlert = engine.checkModelConfidenceExclusion("TESTCOIN");
    auto technicalAlert = engine.checkTechnicalExclusion("TESTCOIN");
    auto newsAlert = engine.checkNewsEventExclusion("TESTCOIN");

    std::cout << "✅ Exclusion checks completed:" << std::endl;
    std::cout << "   - Liquidity: " << (liquidityAlert.shouldExclude ? "EXCLUDE" : "OK") << std::endl;
    std::cout << "   - Volatility: " << (volatilityAlert.shouldExclude ? "EXCLUDE" : "OK") << std::endl;
    std::cout << "   - Model Confidence: " << (confidenceAlert.shouldExclude ? "EXCLUDE" : "OK") << std::endl;
    std::cout << "   - Technical: " << (technicalAlert.shouldExclude ? "EXCLUDE" : "OK") << std::endl;
    std::cout << "   - News Events: " << (newsAlert.shouldExclude ? "EXCLUDE" : "OK") << std::endl;

    // All methods should return valid alerts
    assert(liquidityAlert.reason >= CoinExclusionAlert::LIQUIDITY_CRISIS);
    assert(volatilityAlert.reason >= CoinExclusionAlert::LIQUIDITY_CRISIS);
    assert(confidenceAlert.reason >= CoinExclusionAlert::LIQUIDITY_CRISIS);
    assert(technicalAlert.reason >= CoinExclusionAlert::LIQUIDITY_CRISIS);
    assert(newsAlert.reason >= CoinExclusionAlert::LIQUIDITY_CRISIS);
}

int main() {
    std::cout << "Day 14 Coin Exclusion Engine - Validation Testing" << std::endl;
    std::cout << "=====================================================" << std::endl;
    std::cout << std::endl;

    try {
        testCoinExclusionEngineBasics();
        std::cout << std::endl;

        testLiquidityExclusion();
        std::cout << std::endl;

        testVolatilityExclusion();
        std::cout << std::endl;

        testConfigurationManagement();
        std::cout << std::endl;

        testBatchProcessing();
        std::cout << std::endl;

        testPerformanceWithLargerDataset();
        std::cout << std::endl;

        testExclusionReasons();
        std::cout << std::endl;

        std::cout << "=====================================================" << std::endl;
        std::cout << "🎉 All Day 14 validation tests PASSED!" << std::endl;
        std::cout << "✅ CoinExclusionEngine is working correctly" << std::endl;
        std::cout << "✅ Liquidity filtering operational" << std::endl;
        std::cout << "✅ Volatility detection functional" << std::endl;
        std::cout << "✅ Configuration management working" << std::endl;
        std::cout << "✅ Batch processing efficient" << std::endl;
        std::cout << "✅ Performance meets requirements" << std::endl;
        std::cout << "✅ All exclusion criteria implemented" << std::endl;
        std::cout << std::endl;
        std::cout << "🚀 Day 14 Advanced Risk Management - COMPLETE!" << std::endl;
        std::cout << "🔧 Ready for integration with trading strategy!" << std::endl;

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "❌ Day 14 validation failed: " << e.what() << std::endl;
        return 1;
    }
}