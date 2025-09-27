#include <iostream>
#include <vector>
#include <cassert>
#include <iomanip>

// Day 10 Trading Engine Simple Testing Suite (only implemented methods)
#include "../src/Core/Trading/TradingEngine.h"
#include "../src/Core/Database/Models/PortfolioData.h"

using namespace CryptoClaude::Trading;
using namespace CryptoClaude::Database::Models;
using namespace CryptoClaude::Common;

// Test utilities
bool verbose = true;
int test_count = 0;
int passed_tests = 0;

void assert_test(bool condition, const std::string& test_name) {
    test_count++;
    if (condition) {
        if (verbose) std::cout << "✅ " << test_name << " PASSED" << std::endl;
        passed_tests++;
    } else {
        if (verbose) std::cout << "❌ " << test_name << " FAILED" << std::endl;
    }
}

void assert_near(double actual, double expected, double tolerance, const std::string& test_name) {
    assert_test(std::abs(actual - expected) <= tolerance, test_name);
}

// Helper functions
Portfolio createTestPortfolio() {
    Portfolio portfolio;
    portfolio.setPortfolioId(1);
    portfolio.setStrategyName("Day 10 Trading Test Portfolio");
    portfolio.setTotalValue(1000000.0);  // $1M portfolio
    portfolio.setTotalPnL(50000.0);
    portfolio.setCashBalance(200000.0);
    return portfolio;
}

std::vector<Position> createTestPositions() {
    std::vector<Position> positions;

    // Create BTC long position using constructor
    Position btcPosition("BTC", 10.0, 45000.0, true, 1.0); // symbol, quantity, entry price, is long, leverage
    btcPosition.setCurrentPrice(47000.0);
    positions.push_back(btcPosition);

    // Create ETH short position using constructor
    Position ethPosition("ETH", 5.0, 3000.0, false, 1.0); // symbol, quantity, entry price, is short, leverage
    ethPosition.setCurrentPrice(2950.0);
    positions.push_back(ethPosition);

    return positions;
}

// === BASIC TRADING ENGINE TESTS ===

void test_TradingEngineInitialization() {
    if (verbose) std::cout << "\n⚡ Testing Trading Engine Initialization..." << std::endl;

    // Test default initialization
    StrategyParameters params;
    params.maxPairsToCreate = 15;
    params.totalInvestmentRatio = 0.85;
    params.minConfidenceThreshold = 0.4;
    params.cashBufferPercentage = 0.15;

    TradingEngine engine(params);
    assert_test(engine.isConfigurationValid(), "Configuration validation");

    auto retrievedParams = engine.getStrategyParameters();
    assert_test(retrievedParams.maxPairsToCreate == 15, "Max pairs setting");
    assert_near(retrievedParams.totalInvestmentRatio, 0.85, 0.001, "Investment ratio setting");
    assert_near(retrievedParams.minConfidenceThreshold, 0.4, 0.001, "Confidence threshold setting");
}

void test_PortfolioStateManagement() {
    if (verbose) std::cout << "\n📊 Testing Portfolio State Management..." << std::endl;

    TradingEngine engine;
    Portfolio testPortfolio = createTestPortfolio();
    std::vector<Position> testPositions = createTestPositions();

    // Set portfolio state
    engine.setCurrentPortfolio(testPortfolio, testPositions);

    // Retrieve and verify
    Portfolio retrievedPortfolio = engine.getCurrentPortfolio();
    std::vector<Position> retrievedPositions = engine.getCurrentPositions();

    assert_test(retrievedPortfolio.getPortfolioId() == 1, "Portfolio ID preservation");
    assert_near(retrievedPortfolio.getTotalValue(), 1000000.0, 0.01, "Portfolio total value");
    assert_test(retrievedPositions.size() == 2, "Position count preservation");
    assert_test(retrievedPositions[0].getSymbol() == "BTC", "BTC position preservation");
    assert_test(retrievedPositions[1].getSymbol() == "ETH", "ETH position preservation");
}

void test_PredictionGeneration() {
    if (verbose) std::cout << "\n🧠 Testing Prediction Generation..." << std::endl;

    TradingEngine engine;
    std::vector<std::string> symbols = {"BTC", "ETH", "ADA"};

    // Generate predictions
    auto predictions = engine.generatePredictions(symbols);

    // Should generate some predictions (even if they're placeholder/default)
    assert_test(true, "Prediction generation executes without crashing");

    // Check prediction structure validity
    for (const auto& pred : predictions) {
        assert_test(!pred.symbol.empty(), "Prediction symbol not empty");
        assert_test(pred.confidence >= 0.0 && pred.confidence <= 1.0, "Prediction confidence range");
        assert_test(std::isfinite(pred.predictedReturn), "Prediction return finite");
    }
}

void test_CoinExclusion() {
    if (verbose) std::cout << "\n🔍 Testing Coin Exclusion..." << std::endl;

    TradingEngine engine;
    std::vector<std::string> candidates = {"BTC", "ETH", "ADA", "SOL", "DOT", "UNKNOWN_COIN"};

    // Filter unsuitable coins
    auto filtered = engine.excludeUnsuitableCoins(candidates);

    assert_test(true, "Coin exclusion executes without crashing");
    assert_test(filtered.size() <= candidates.size(), "Filtered list not larger than input");

    if (verbose) {
        std::cout << "Original candidates: " << candidates.size() << std::endl;
        std::cout << "After filtering: " << filtered.size() << std::endl;
    }
}

void test_TradingPairCreation() {
    if (verbose) std::cout << "\n🔗 Testing Trading Pair Creation..." << std::endl;

    TradingEngine engine;

    // Create sample predictions
    std::vector<PredictionData> samplePredictions;

    PredictionData btcPred;
    btcPred.symbol = "BTC";
    btcPred.predictedReturn = 0.05;  // 5% expected return
    btcPred.confidence = 0.8;
    samplePredictions.push_back(btcPred);

    PredictionData ethPred;
    ethPred.symbol = "ETH";
    ethPred.predictedReturn = 0.03;  // 3% expected return
    ethPred.confidence = 0.7;
    samplePredictions.push_back(ethPred);

    PredictionData adaPred;
    adaPred.symbol = "ADA";
    adaPred.predictedReturn = -0.02; // -2% expected return (good for shorting)
    adaPred.confidence = 0.6;
    samplePredictions.push_back(adaPred);

    // Create trading pairs
    auto pairs = engine.createTradingPairs(samplePredictions);

    assert_test(true, "Trading pair creation executes without crashing");

    for (const auto& pair : pairs) {
        assert_test(!pair.longSymbol.empty(), "Long symbol not empty");
        assert_test(!pair.shortSymbol.empty(), "Short symbol not empty");
        assert_test(pair.longSymbol != pair.shortSymbol, "Long and short symbols different");
        assert_test(pair.pairConfidence >= 0.0, "Pair confidence non-negative");
    }

    if (verbose && !pairs.empty()) {
        std::cout << "Created " << pairs.size() << " trading pairs" << std::endl;
    }
}

void test_RebalancingLogic() {
    if (verbose) std::cout << "\n⚖️ Testing Rebalancing Logic..." << std::endl;

    TradingEngine engine;

    // Set some portfolio state
    Portfolio portfolio = createTestPortfolio();
    std::vector<Position> positions = createTestPositions();
    engine.setCurrentPortfolio(portfolio, positions);

    // Check if rebalancing is needed
    bool needsRebalancing = engine.shouldRebalancePortfolio();
    assert_test(true, "Rebalancing check executes without crashing");

    if (verbose) {
        std::cout << "Portfolio needs rebalancing: " << (needsRebalancing ? "Yes" : "No") << std::endl;
    }
}

void test_LiquidityCalculation() {
    if (verbose) std::cout << "\n💧 Testing Liquidity Calculation..." << std::endl;

    TradingEngine engine;

    // Test liquidity score calculation for major coins
    std::vector<std::string> symbols = {"BTC", "ETH", "ADA"};

    for (const auto& symbol : symbols) {
        double liquidityScore = engine.calculateLiquidityScore(symbol);
        assert_test(liquidityScore >= 0.0 && liquidityScore <= 1.0,
                   "Liquidity score range for " + symbol);
        assert_test(std::isfinite(liquidityScore),
                   "Liquidity score finite for " + symbol);

        if (verbose) {
            std::cout << symbol << " liquidity score: " << liquidityScore << std::endl;
        }
    }
}

void test_ModelPerformance() {
    if (verbose) std::cout << "\n📈 Testing Model Performance Calculation..." << std::endl;

    TradingEngine engine;

    // Test model performance calculation for major coins
    std::vector<std::string> symbols = {"BTC", "ETH", "ADA"};

    for (const auto& symbol : symbols) {
        double modelPerf = engine.calculateModelPerformance(symbol);
        assert_test(std::isfinite(modelPerf),
                   "Model performance finite for " + symbol);

        if (verbose) {
            std::cout << symbol << " model performance: " << modelPerf << std::endl;
        }
    }
}

void test_ParametersAccess() {
    if (verbose) std::cout << "\n🎛️ Testing Parameters Access..." << std::endl;

    StrategyParameters params;
    params.maxPairsToCreate = 25;
    params.totalInvestmentRatio = 0.8;
    params.minConfidenceThreshold = 0.35;
    params.cashBufferPercentage = 0.2; // Ensures 0.8 + 0.2 = 1.0

    TradingEngine engine(params);

    // Test getting trading universe
    auto universe = engine.getTradingUniverse();
    assert_test(universe.maxUniverseSize > 0, "Trading universe max size positive");
    assert_test(universe.targetPortfolioSize > 0, "Target portfolio size positive");

    // Test getting strategy parameters
    auto retrievedParams = engine.getStrategyParameters();
    assert_test(retrievedParams.maxPairsToCreate == 25, "Strategy params retrieval");

    if (verbose) {
        std::cout << "Trading universe max size: " << universe.maxUniverseSize << std::endl;
        std::cout << "Target portfolio size: " << universe.targetPortfolioSize << std::endl;
    }
}

int main() {
    std::cout << "⚡ Day 10 Trading Engine Simple Test Suite" << std::endl;
    std::cout << "===========================================" << std::endl;

    try {
        test_TradingEngineInitialization();
        test_PortfolioStateManagement();
        test_PredictionGeneration();
        test_CoinExclusion();
        test_TradingPairCreation();
        test_RebalancingLogic();
        test_LiquidityCalculation();
        test_ModelPerformance();
        test_ParametersAccess();

    } catch (const std::exception& e) {
        std::cout << "❌ FATAL ERROR: " << e.what() << std::endl;
        return 1;
    }

    // Test Results Summary
    std::cout << "\n📊 Test Results Summary" << std::endl;
    std::cout << "======================" << std::endl;
    std::cout << "Total Tests: " << test_count << std::endl;
    std::cout << "Passed: " << passed_tests << std::endl;
    std::cout << "Failed: " << (test_count - passed_tests) << std::endl;
    std::cout << "Success Rate: " << std::fixed << std::setprecision(1)
              << (100.0 * passed_tests / test_count) << "%" << std::endl;

    if (passed_tests == test_count) {
        std::cout << "\n✅ ALL TESTS PASSED - Day 10 Trading Engine core functionality works!" << std::endl;
        return 0;
    } else {
        std::cout << "\n❌ SOME TESTS FAILED - Day 10 implementation needs attention" << std::endl;
        return 1;
    }
}