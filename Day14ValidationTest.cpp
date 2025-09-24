#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include "src/Core/Risk/PredictiveRiskEngine.h"
#include "src/Core/Trading/TradingEngine.h"
#include "src/Core/Strategy/LongShortStrategy.h"
#include "src/Core/Database/Models/MarketData.h"
#include "src/Core/Database/Models/SentimentData.h"
#include "src/Core/Database/Models/PortfolioData.h"

using namespace CryptoClaude::Risk;
using namespace CryptoClaude::Trading;
using namespace CryptoClaude::Strategy;
using namespace CryptoClaude::Database::Models;

// Test data generators
MarketData createMarketData(const std::string& symbol, double price, double volume, double spread = 0.001) {
    MarketData data;
    data.symbol = symbol;
    data.price = price;
    data.volume = volume;
    data.bidPrice = price * (1.0 - spread/2.0);
    data.askPrice = price * (1.0 + spread/2.0);
    data.timestamp = std::chrono::system_clock::now();
    return data;
}

SentimentData createSentimentData(const std::string& symbol, double score) {
    SentimentData data;
    data.symbol = symbol;
    data.sentimentScore = score;
    data.hasRecentNews = false;
    data.timestamp = std::chrono::system_clock::now();
    return data;
}

Portfolio createTestPortfolio(double totalValue = 1000000.0) {
    Portfolio portfolio("TEST", totalValue, totalValue);
    return portfolio;
}

void testCoinExclusionEngineStandalone() {
    std::cout << "Testing CoinExclusionEngine standalone functionality..." << std::endl;

    CoinExclusionEngine engine;

    // Test with mixed quality coins
    std::vector<std::string> testCoins = {
        "BTC", "ETH", "LOWVOL", "HIGHVOL", "THINLIQ", "GOODCOIN"
    };

    // Add market data with varying quality
    engine.updateMarketData("BTC", createMarketData("BTC", 45000, 1000e6, 0.0005));      // Excellent
    engine.updateMarketData("ETH", createMarketData("ETH", 3000, 800e6, 0.001));         // Excellent
    engine.updateMarketData("LOWVOL", createMarketData("LOWVOL", 100, 200e6, 0.002));   // Good
    engine.updateMarketData("HIGHVOL", createMarketData("HIGHVOL", 50, 150e6, 0.008));  // Risky spread
    engine.updateMarketData("THINLIQ", createMarketData("THINLIQ", 10, 30e6, 0.003));   // Low volume
    engine.updateMarketData("GOODCOIN", createMarketData("GOODCOIN", 25, 300e6, 0.0015)); // Good

    // Test individual exclusion checks
    auto btcLiquidityAlert = engine.checkLiquidityExclusion("BTC");
    assert(!btcLiquidityAlert.shouldExclude);
    std::cout << "✅ BTC liquidity check: " << btcLiquidityAlert.message << std::endl;

    auto thinLiqAlert = engine.checkLiquidityExclusion("THINLIQ");
    assert(thinLiqAlert.shouldExclude);
    std::cout << "✅ THINLIQ exclusion: " << thinLiqAlert.message << std::endl;

    // Test batch filtering
    auto filtered = engine.filterExcludedCoins(testCoins);
    assert(filtered.size() < testCoins.size());
    assert(std::find(filtered.begin(), filtered.end(), "BTC") != filtered.end());
    assert(std::find(filtered.begin(), filtered.end(), "ETH") != filtered.end());
    std::cout << "✅ Batch filtering: " << testCoins.size() << " -> " << filtered.size() << " coins" << std::endl;
}

void testTradingEngineIntegration() {
    std::cout << "Testing TradingEngine integration with CoinExclusionEngine..." << std::endl;

    // Create pairing parameters for trading engine
    PairingParameters params;
    params.maxPairsToCreate = 5;
    params.totalInvestmentRatio = 0.9; // 90% invested, 10% cash buffer
    params.minConfidenceThreshold = 0.3;

    TradingEngine engine(params);

    // Add test market data
    std::vector<std::string> universe = {
        "BTC", "ETH", "ADA", "DOT", "LINK", "UNI", "LOWVOL", "BADCOIN"
    };

    engine.updateMarketData("BTC", createMarketData("BTC", 45000, 1000e6));
    engine.updateMarketData("ETH", createMarketData("ETH", 3000, 800e6));
    engine.updateMarketData("ADA", createMarketData("ADA", 0.5, 400e6));
    engine.updateMarketData("DOT", createMarketData("DOT", 6, 200e6));
    engine.updateMarketData("LINK", createMarketData("LINK", 15, 300e6));
    engine.updateMarketData("UNI", createMarketData("UNI", 8, 250e6));
    engine.updateMarketData("LOWVOL", createMarketData("LOWVOL", 100, 50e6));  // Low volume
    engine.updateMarketData("BADCOIN", createMarketData("BADCOIN", 1, 5e6, 0.02)); // Low volume, wide spread

    // Test exclusion filtering
    auto filtered = engine.excludeUnsuitableCoins(universe);
    assert(filtered.size() < universe.size());
    std::cout << "✅ Universe filtering: " << universe.size() << " -> " << filtered.size() << " coins" << std::endl;

    // Generate predictions for filtered universe (simulated)
    std::vector<PredictionWithConfidence> predictions;
    for (const auto& symbol : filtered) {
        PredictionWithConfidence pred;
        pred.symbol = symbol;
        pred.predictedReturn = (symbol == "BTC") ? 0.05 :
                              (symbol == "ETH") ? 0.03 :
                              (symbol == "BADCOIN") ? -0.04 : 0.01;
        pred.confidence = 0.6;
        pred.predictionTimestamp = std::chrono::system_clock::now();
        predictions.push_back(pred);
    }

    // Test pair creation
    auto pairs = engine.createTradingPairs(predictions);
    assert(!pairs.empty());
    std::cout << "✅ Trading pairs created: " << pairs.size() << " pairs" << std::endl;

    // Test capital allocation
    auto allocatedPairs = engine.allocateCapitalToPairs(pairs);
    assert(allocatedPairs.size() == pairs.size());

    double totalAllocation = 0.0;
    for (const auto& pair : allocatedPairs) {
        totalAllocation += pair.allocationWeight;
        assert(pair.allocationWeight > 0);
        assert(pair.allocationWeight <= 0.5); // Reasonable single pair limit
    }
    assert(totalAllocation <= 0.95); // Should not exceed investment ratio
    std::cout << "✅ Capital allocation: " << totalAllocation * 100 << "% total allocation" << std::endl;

    // Test conversion to target positions
    auto targets = engine.convertPairsToTargetPositions(allocatedPairs);
    assert(targets.size() == allocatedPairs.size() * 2); // Long + short for each pair

    int longCount = 0, shortCount = 0;
    for (const auto& target : targets) {
        if (target.isLongPosition) longCount++;
        if (target.isShortPosition) shortCount++;
    }
    assert(longCount == shortCount); // Equal number of longs and shorts
    std::cout << "✅ Target positions: " << longCount << " longs, " << shortCount << " shorts" << std::endl;
}

void testLongShortStrategyIntegration() {
    std::cout << "Testing LongShortStrategy integration with CoinExclusionEngine..." << std::endl;

    LongShortStrategy strategy;

    // Configure strategy parameters
    strategy.setMaxPairs(3);
    strategy.setCashBufferRatio(0.1);
    strategy.setMinConfidenceThreshold(0.3);
    strategy.setMaxPairAllocation(0.15); // 15% max per pair

    // Create test portfolio
    Portfolio portfolio = createTestPortfolio();

    // Create test predictions with various quality
    std::vector<MachineLearning::DailyPrediction> predictions;

    MachineLearning::DailyPrediction btcPred;
    btcPred.symbol = "BTC";
    btcPred.predictedReturn = 0.04;
    btcPred.confidence = 0.8;
    btcPred.dataTimestamp = std::chrono::system_clock::now();
    predictions.push_back(btcPred);

    MachineLearning::DailyPrediction ethPred;
    ethPred.symbol = "ETH";
    ethPred.predictedReturn = 0.02;
    ethPred.confidence = 0.7;
    ethPred.dataTimestamp = std::chrono::system_clock::now();
    predictions.push_back(ethPred);

    MachineLearning::DailyPrediction adaPred;
    adaPred.symbol = "ADA";
    adaPred.predictedReturn = -0.02;
    adaPred.confidence = 0.6;
    adaPred.dataTimestamp = std::chrono::system_clock::now();
    predictions.push_back(adaPred);

    MachineLearning::DailyPrediction badPred;
    badPred.symbol = "BADCOIN";
    badPred.predictedReturn = -0.05;
    badPred.confidence = 0.2; // Low confidence - should be excluded
    badPred.dataTimestamp = std::chrono::system_clock::now();
    predictions.push_back(badPred);

    // Test signal generation
    std::string currentDate = "2024-01-15";
    auto signals = strategy.generateSignals(currentDate, portfolio, predictions);

    // Should have pairs (long + short positions)
    assert(!signals.empty());

    int buySignals = 0, sellSignals = 0;
    double totalLongWeight = 0.0, totalShortWeight = 0.0;

    for (const auto& signal : signals) {
        if (signal.signalType == SignalType::BUY) {
            buySignals++;
            totalLongWeight += signal.targetWeight;
        } else if (signal.signalType == SignalType::SELL) {
            sellSignals++;
            totalShortWeight += std::abs(signal.targetWeight);
        }

        // Verify no low-confidence coins made it through
        assert(signal.symbol != "BADCOIN");
    }

    assert(buySignals > 0 && sellSignals > 0); // Should have both longs and shorts
    assert(buySignals == sellSignals); // Market-neutral pairing
    assert(totalLongWeight <= 0.95 && totalShortWeight <= 0.95); // Within investment limits

    std::cout << "✅ Strategy signals: " << buySignals << " buys, " << sellSignals << " sells" << std::endl;
    std::cout << "✅ Total exposure: " << totalLongWeight * 100 << "% long, " << totalShortWeight * 100 << "% short" << std::endl;

    // Test risk checking
    bool passedRisk = strategy.checkRiskLimits(signals, portfolio);
    assert(passedRisk); // Should pass risk limits with our test data
    std::cout << "✅ Risk limits check: PASSED" << std::endl;

    // Test configuration validation
    bool validConfig = strategy.validateConfiguration();
    assert(validConfig);
    std::cout << "✅ Configuration validation: PASSED" << std::endl;
}

void testEndToEndWorkflow() {
    std::cout << "Testing complete end-to-end workflow..." << std::endl;

    // 1. Set up exclusion engine with realistic data
    CoinExclusionEngine exclusionEngine;

    std::vector<std::string> cryptoUniverse = {
        "BTC", "ETH", "BNB", "XRP", "ADA", "SOL", "DOGE", "DOT", "AVAX", "MATIC",
        "LTC", "UNI", "LINK", "ATOM", "XLM", "ALGO", "VET", "ICP", "AAVE", "COMP",
        "LOWVOL", "THINLIQ", "WIDESPREAD", "VOLATILE" // Test coins that should be excluded
    };

    // Add realistic market data
    for (const auto& symbol : cryptoUniverse) {
        double volume, spread;

        if (symbol == "BTC" || symbol == "ETH") {
            volume = 1000e6; spread = 0.0005;
        } else if (symbol.find("LOW") != std::string::npos) {
            volume = 30e6; spread = 0.002;  // Should be excluded for low volume
        } else if (symbol.find("THIN") != std::string::npos) {
            volume = 20e6; spread = 0.003;  // Should be excluded
        } else if (symbol.find("WIDE") != std::string::npos) {
            volume = 200e6; spread = 0.015; // Should be excluded for wide spread
        } else if (symbol.find("VOLATILE") != std::string::npos) {
            volume = 150e6; spread = 0.002; // Will be excluded for volatility
        } else {
            volume = 200e6; spread = 0.002; // Good coins
        }

        double price = (symbol == "BTC") ? 45000 :
                      (symbol == "ETH") ? 3000 : 10.0;

        exclusionEngine.updateMarketData(symbol, createMarketData(symbol, price, volume, spread));
    }

    // 2. Filter universe
    auto suitableCoins = exclusionEngine.filterExcludedCoins(cryptoUniverse);
    assert(suitableCoins.size() < cryptoUniverse.size());
    assert(suitableCoins.size() >= 6); // Need enough for pairs
    std::cout << "✅ Universe filtered: " << cryptoUniverse.size() << " -> " << suitableCoins.size() << " coins" << std::endl;

    // 3. Set up trading engine with pairing parameters
    PairingParameters params;
    params.maxPairsToCreate = 5;
    params.totalInvestmentRatio = 0.9;
    params.minConfidenceThreshold = 0.4;

    TradingEngine tradingEngine(params);

    // Update trading engine with market data
    for (const auto& symbol : suitableCoins) {
        auto data = createMarketData(symbol, 100.0, 200e6);
        tradingEngine.updateMarketData(symbol, data);
        tradingEngine.updateSentimentData(symbol, createSentimentData(symbol, 0.1));
    }

    // 4. Set up long-short strategy
    LongShortStrategy strategy;
    strategy.setMaxPairs(5);
    strategy.setCashBufferRatio(0.10);
    strategy.setMinConfidenceThreshold(0.4);
    strategy.setMaxPairAllocation(0.15);

    // 5. Generate predictions for suitable coins
    std::vector<MachineLearning::DailyPrediction> predictions;
    for (size_t i = 0; i < suitableCoins.size(); ++i) {
        MachineLearning::DailyPrediction pred;
        pred.symbol = suitableCoins[i];
        // Create spread of returns: some positive, some negative
        pred.predictedReturn = 0.06 - (static_cast<double>(i) / suitableCoins.size()) * 0.12; // Range: +6% to -6%
        pred.confidence = 0.5 + (static_cast<double>(i) / suitableCoins.size()) * 0.3; // Range: 50% to 80%
        pred.dataTimestamp = std::chrono::system_clock::now();
        predictions.push_back(pred);
    }

    // 6. Generate trading signals
    Portfolio portfolio = createTestPortfolio();
    auto signals = strategy.generateSignals("2024-01-15", portfolio, predictions);

    assert(!signals.empty());
    assert(signals.size() % 2 == 0); // Even number for pairs

    // 7. Verify signal quality
    double totalLongExposure = 0.0, totalShortExposure = 0.0;
    int longCount = 0, shortCount = 0;

    for (const auto& signal : signals) {
        if (signal.signalType == SignalType::BUY) {
            longCount++;
            totalLongExposure += signal.targetWeight;
        } else if (signal.signalType == SignalType::SELL) {
            shortCount++;
            totalShortExposure += std::abs(signal.targetWeight);
        }

        // Verify no excluded coins appear in signals
        assert(std::find(suitableCoins.begin(), suitableCoins.end(), signal.symbol) != suitableCoins.end());
    }

    assert(longCount == shortCount); // Market-neutral pairs
    assert(totalLongExposure <= 0.95 && totalShortExposure <= 0.95);
    assert(totalLongExposure + riskLimits.cashBufferPercentage <= 1.05); // Allow small rounding error

    std::cout << "✅ End-to-end workflow complete:" << std::endl;
    std::cout << "   - " << signals.size() << " total signals (" << longCount << " pairs)" << std::endl;
    std::cout << "   - " << totalLongExposure * 100 << "% long exposure" << std::endl;
    std::cout << "   - " << totalShortExposure * 100 << "% short exposure" << std::endl;
    std::cout << "   - " << (1.0 - totalLongExposure) * 100 << "% cash buffer maintained" << std::endl;

    // 8. Test risk management
    bool riskPassed = strategy.checkRiskLimits(signals, portfolio);
    assert(riskPassed);
    std::cout << "✅ Risk management: All limits respected" << std::endl;
}

void testPerformanceAndScaling() {
    std::cout << "Testing performance with large universe..." << std::endl;

    CoinExclusionEngine engine;

    // Generate large test universe
    std::vector<std::string> largeUniverse;
    for (int i = 0; i < 200; ++i) {
        largeUniverse.push_back("COIN" + std::to_string(i));
    }

    // Add market data for all coins
    auto start = std::chrono::high_resolution_clock::now();

    for (const auto& symbol : largeUniverse) {
        double volume = 50e6 + (std::hash<std::string>{}(symbol) % 500) * 1e6; // 50M-550M range
        double spread = 0.001 + (std::hash<std::string>{}(symbol) % 20) * 0.0005; // 0.1%-1% range
        engine.updateMarketData(symbol, createMarketData(symbol, 100.0, volume, spread));
    }

    auto dataUpdateTime = std::chrono::high_resolution_clock::now();

    // Test batch filtering performance
    auto filtered = engine.filterExcludedCoins(largeUniverse);

    auto filterTime = std::chrono::high_resolution_clock::now();

    auto dataUpdateDuration = std::chrono::duration_cast<std::chrono::milliseconds>(dataUpdateTime - start);
    auto filterDuration = std::chrono::duration_cast<std::chrono::milliseconds>(filterTime - dataUpdateTime);

    assert(filtered.size() < largeUniverse.size());
    assert(filtered.size() > largeUniverse.size() * 0.5); // Should keep most coins

    std::cout << "✅ Performance test results:" << std::endl;
    std::cout << "   - Data update: " << dataUpdateDuration.count() << "ms for " << largeUniverse.size() << " coins" << std::endl;
    std::cout << "   - Filtering: " << filterDuration.count() << "ms" << std::endl;
    std::cout << "   - Filtered: " << largeUniverse.size() << " -> " << filtered.size() << " coins" << std::endl;

    // Should complete within reasonable time
    assert(dataUpdateDuration.count() < 5000);  // 5 seconds max for data updates
    assert(filterDuration.count() < 1000);      // 1 second max for filtering
}

int main() {
    std::cout << "Day 14 Advanced Risk Management - Validation Testing" << std::endl;
    std::cout << "====================================================================" << std::endl;
    std::cout << std::endl;

    try {
        testCoinExclusionEngineStandalone();
        std::cout << std::endl;

        testTradingEngineIntegration();
        std::cout << std::endl;

        testLongShortStrategyIntegration();
        std::cout << std::endl;

        testEndToEndWorkflow();
        std::cout << std::endl;

        testPerformanceAndScaling();
        std::cout << std::endl;

        std::cout << "====================================================================" << std::endl;
        std::cout << "🎉 Day 14 Validation: ALL TESTS PASSED!" << std::endl;
        std::cout << "🔍 Coin Exclusion Engine: Filtering unsuitable coins effectively" << std::endl;
        std::cout << "🏗️  Trading Engine Integration: Seamless pairing workflow" << std::endl;
        std::cout << "📈 Long-Short Strategy: Market-neutral pair generation working" << std::endl;
        std::cout << "🔄 End-to-End Workflow: Complete strategy pipeline validated" << std::endl;
        std::cout << "⚡ Performance: Scales efficiently with large universes" << std::endl;
        std::cout << std::endl;
        std::cout << "✅ Day 14 Advanced Risk Management - COMPLETE!" << std::endl;
        std::cout << "✅ Coin exclusion framework ready for production use!" << std::endl;
        std::cout << "✅ Long-short pairing strategy fully integrated!" << std::endl;

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "❌ Day 14 validation failed: " << e.what() << std::endl;
        return 1;
    }
}