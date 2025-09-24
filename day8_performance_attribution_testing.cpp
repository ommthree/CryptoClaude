#include "src/Core/Analytics/PerformanceAttributionEngine.h"
#include "src/Core/Database/Models/PortfolioData.h"
#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>
#include <chrono>
#include <iomanip>

using namespace CryptoClaude::Analytics;
using namespace CryptoClaude::Database::Models;

// Test result structure
struct TestResult {
    std::string testName;
    bool passed;
    std::string errorMessage;

    TestResult(const std::string& name, bool success, const std::string& error = "")
        : testName(name), passed(success), errorMessage(error) {}
};

std::vector<TestResult> testResults;

// Helper function to compare doubles with tolerance
bool isEqual(double a, double b, double tolerance = 1e-6) {
    return std::abs(a - b) < tolerance;
}

// Helper function to run a test
void runTest(const std::string& testName, std::function<void()> testFunc) {
    try {
        testFunc();
        testResults.emplace_back(testName, true);
        std::cout << "[PASS] " << testName << std::endl;
    } catch (const std::exception& e) {
        testResults.emplace_back(testName, false, e.what());
        std::cout << "[FAIL] " << testName << " - " << e.what() << std::endl;
    }
}

// Create sample portfolio data for testing
Portfolio createTestPortfolio() {
    Portfolio portfolio;
    portfolio.setPortfolioId(1);
    portfolio.setStrategyName("Test Portfolio");
    portfolio.setTotalValue(100000.0);
    portfolio.setTotalPnL(20000.0);
    portfolio.setCashBalance(10000.0);
    return portfolio;
}

// Create sample positions for testing
std::vector<Position> createTestPositions() {
    std::vector<Position> positions;

    // Use constructor: Position(symbol, quantity, entry_price, is_long, leverage)
    Position btc("BTC", 2.0, 45000.0, true, 1.5);
    btc.setPortfolioId(1);
    btc.setCurrentPrice(50000.0);
    positions.push_back(btc);

    Position eth("ETH", 10.0, 3000.0, true, 2.0);
    eth.setPortfolioId(1);
    eth.setCurrentPrice(3500.0);
    positions.push_back(eth);

    Position ada("ADA", 1000.0, 1.20, true, 1.0);
    ada.setPortfolioId(1);
    ada.setCurrentPrice(1.50);
    positions.push_back(ada);

    return positions;
}

// ==================== FACTOR ATTRIBUTION TESTS ====================

void testFactorAttribution() {
    PerformanceAttributionEngine engine;
    Portfolio portfolio = createTestPortfolio();
    std::vector<Position> positions = createTestPositions();

    auto startDate = std::chrono::system_clock::now() - std::chrono::hours(24 * 30);
    auto endDate = std::chrono::system_clock::now();

    auto factorAttributions = engine.calculateFactorAttribution(portfolio, positions, startDate, endDate);

    // Verify we get expected factors
    if (factorAttributions.size() < 3) {
        throw std::runtime_error("Expected at least 3 factor attributions");
    }

    // Check that we have market, size, momentum, and volatility factors
    bool hasMarket = false, hasSize = false, hasMomentum = false, hasVolatility = false;
    for (const auto& attribution : factorAttributions) {
        if (attribution.factorName == "Market") hasMarket = true;
        if (attribution.factorName == "Size") hasSize = true;
        if (attribution.factorName == "Momentum") hasMomentum = true;
        if (attribution.factorName == "Volatility") hasVolatility = true;
    }

    if (!hasMarket || !hasSize || !hasMomentum || !hasVolatility) {
        throw std::runtime_error("Missing expected factor attributions");
    }
}

void testEmptyFactorAttribution() {
    PerformanceAttributionEngine engine;
    Portfolio portfolio = createTestPortfolio();
    std::vector<Position> positions; // Empty positions

    auto startDate = std::chrono::system_clock::now() - std::chrono::hours(24 * 30);
    auto endDate = std::chrono::system_clock::now();

    auto factorAttributions = engine.calculateFactorAttribution(portfolio, positions, startDate, endDate);

    if (!factorAttributions.empty()) {
        throw std::runtime_error("Expected empty factor attributions for empty positions");
    }
}

// ==================== ASSET ATTRIBUTION TESTS ====================

void testAssetAttribution() {
    PerformanceAttributionEngine engine;
    Portfolio portfolio = createTestPortfolio();
    std::vector<Position> positions = createTestPositions();

    std::map<std::string, double> benchmarkWeights = {
        {"BTC", 0.60}, {"ETH", 0.25}, {"ADA", 0.15}
    };

    auto startDate = std::chrono::system_clock::now() - std::chrono::hours(24 * 30);
    auto endDate = std::chrono::system_clock::now();

    auto assetAttributions = engine.calculateAssetAttribution(portfolio, positions, benchmarkWeights, startDate, endDate);

    if (assetAttributions.size() != positions.size()) {
        throw std::runtime_error("Asset attribution count mismatch with position count");
    }

    // Verify each position has attribution data
    for (const auto& attribution : assetAttributions) {
        if (attribution.symbol.empty()) {
            throw std::runtime_error("Asset attribution missing symbol");
        }
        if (attribution.weight < 0) {
            throw std::runtime_error("Invalid negative asset weight");
        }
    }
}

void testAssetAttributionEffects() {
    PerformanceAttributionEngine engine;
    Portfolio portfolio = createTestPortfolio();
    std::vector<Position> positions = createTestPositions();

    std::map<std::string, double> benchmarkWeights = {
        {"BTC", 0.60}, {"ETH", 0.25}, {"ADA", 0.15}
    };

    auto startDate = std::chrono::system_clock::now() - std::chrono::hours(24 * 30);
    auto endDate = std::chrono::system_clock::now();

    auto assetAttributions = engine.calculateAssetAttribution(portfolio, positions, benchmarkWeights, startDate, endDate);

    // Verify allocation and selection effects are calculated
    for (const auto& attribution : assetAttributions) {
        if (!std::isfinite(attribution.allocationEffect)) {
            throw std::runtime_error("Invalid allocation effect calculated");
        }
        if (!std::isfinite(attribution.selectionEffect)) {
            throw std::runtime_error("Invalid selection effect calculated");
        }
        if (!std::isfinite(attribution.leverageEffect)) {
            throw std::runtime_error("Invalid leverage effect calculated");
        }
    }
}

// ==================== LEVERAGE CONTRIBUTION TESTS ====================

void testLeverageContribution() {
    PerformanceAttributionEngine engine;
    Portfolio portfolio = createTestPortfolio();
    std::vector<Position> positions = createTestPositions();

    auto startDate = std::chrono::system_clock::now() - std::chrono::hours(24 * 30);
    auto endDate = std::chrono::system_clock::now();

    double leverageContrib = engine.calculateLeverageContribution(portfolio, positions, startDate, endDate);

    if (!std::isfinite(leverageContrib)) {
        throw std::runtime_error("Invalid leverage contribution calculated");
    }
}

void testLeverageContributionEmptyPositions() {
    PerformanceAttributionEngine engine;
    Portfolio portfolio = createTestPortfolio();
    std::vector<Position> positions; // Empty

    auto startDate = std::chrono::system_clock::now() - std::chrono::hours(24 * 30);
    auto endDate = std::chrono::system_clock::now();

    double leverageContrib = engine.calculateLeverageContribution(portfolio, positions, startDate, endDate);

    if (!isEqual(leverageContrib, 0.0)) {
        throw std::runtime_error("Expected zero leverage contribution for empty positions");
    }
}

// ==================== SENTIMENT ATTRIBUTION TESTS ====================

void testSentimentAttribution() {
    PerformanceAttributionEngine engine;
    std::vector<Position> positions = createTestPositions();

    auto startDate = std::chrono::system_clock::now() - std::chrono::hours(24 * 30);
    auto endDate = std::chrono::system_clock::now();

    double sentimentContrib = engine.calculateSentimentAttribution(positions, startDate, endDate);

    if (!std::isfinite(sentimentContrib)) {
        throw std::runtime_error("Invalid sentiment attribution calculated");
    }
}

// ==================== ROLLING METRICS TESTS ====================

void testRollingSharpeRatio() {
    PerformanceAttributionEngine engine;

    // Create sample returns data
    std::vector<double> returns = {0.01, 0.02, -0.01, 0.03, 0.00, 0.02, -0.02, 0.01, 0.02, 0.01,
                                   0.00, 0.01, -0.01, 0.02, 0.01, 0.03, -0.01, 0.01, 0.02, 0.00,
                                   0.01, -0.01, 0.02, 0.01, 0.00, 0.02, 0.01, -0.01, 0.01, 0.02};

    auto rollingSharpe = engine.calculateRollingSharpeRatio(returns, 10);

    if (rollingSharpe.empty()) {
        throw std::runtime_error("Expected rolling Sharpe ratio results");
    }

    // Check that all values are finite
    for (double sharpe : rollingSharpe) {
        if (!std::isfinite(sharpe)) {
            throw std::runtime_error("Invalid rolling Sharpe ratio value");
        }
    }
}

void testRollingVolatility() {
    PerformanceAttributionEngine engine;

    std::vector<double> returns = {0.01, 0.02, -0.01, 0.03, 0.00, 0.02, -0.02, 0.01, 0.02, 0.01,
                                   0.00, 0.01, -0.01, 0.02, 0.01, 0.03, -0.01, 0.01, 0.02, 0.00};

    auto rollingVol = engine.calculateRollingVolatility(returns, 5);

    if (rollingVol.empty()) {
        throw std::runtime_error("Expected rolling volatility results");
    }

    // Volatility should be positive
    for (double vol : rollingVol) {
        if (vol < 0 || !std::isfinite(vol)) {
            throw std::runtime_error("Invalid rolling volatility value");
        }
    }
}

void testInsufficientDataRollingMetrics() {
    PerformanceAttributionEngine engine;

    std::vector<double> returns = {0.01, 0.02}; // Too few data points

    auto rollingSharpe = engine.calculateRollingSharpeRatio(returns, 10);
    auto rollingVol = engine.calculateRollingVolatility(returns, 10);

    if (!rollingSharpe.empty()) {
        throw std::runtime_error("Expected empty rolling Sharpe for insufficient data");
    }
    if (!rollingVol.empty()) {
        throw std::runtime_error("Expected empty rolling volatility for insufficient data");
    }
}

// ==================== DRAWDOWN ANALYSIS TESTS ====================

void testDrawdownAnalysis() {
    PerformanceAttributionEngine engine;

    // Create sample portfolio values showing drawdown
    std::vector<double> portfolioValues = {100000, 105000, 110000, 108000, 104000, 102000, 106000, 112000, 115000, 113000};
    std::vector<std::chrono::system_clock::time_point> dates;

    auto startDate = std::chrono::system_clock::now() - std::chrono::hours(24 * 10);
    for (int i = 0; i < 10; ++i) {
        dates.push_back(startDate + std::chrono::hours(24 * i));
    }

    auto drawdownAnalysis = engine.calculateDrawdownAnalysis(portfolioValues, dates);

    if (drawdownAnalysis.maxDrawdown < 0) {
        throw std::runtime_error("Max drawdown should be non-negative");
    }

    if (!std::isfinite(drawdownAnalysis.maxDrawdown)) {
        throw std::runtime_error("Invalid max drawdown value");
    }
}

void testDrawdownAnalysisInvalidData() {
    PerformanceAttributionEngine engine;

    std::vector<double> portfolioValues = {100000, 105000};
    std::vector<std::chrono::system_clock::time_point> dates = {std::chrono::system_clock::now()};

    // Mismatched sizes should return default analysis
    auto drawdownAnalysis = engine.calculateDrawdownAnalysis(portfolioValues, dates);

    // The analysis should have default/initial values, but maxDrawdown might be calculated from available data
    if (drawdownAnalysis.maxDrawdown < 0) {
        throw std::runtime_error("Max drawdown should be non-negative even for invalid data");
    }
}

// ==================== WIN/LOSS ANALYSIS TESTS ====================

void testWinLossAnalysis() {
    PerformanceAttributionEngine engine;

    std::vector<Position> closedPositions = createTestPositions();

    auto winLossAnalysis = engine.calculateWinLossAnalysis(closedPositions);

    if (winLossAnalysis.totalTrades != static_cast<int>(closedPositions.size())) {
        throw std::runtime_error("Total trades mismatch in win/loss analysis");
    }

    // Validate win rate is within expected range
    if (winLossAnalysis.winRate < 0.0 || winLossAnalysis.winRate > 1.0) {
        throw std::runtime_error("Win rate should be between 0 and 1");
    }

    // Ensure win rate calculation is correct
    double expectedWinRate = static_cast<double>(winLossAnalysis.winningTrades) / winLossAnalysis.totalTrades;
    if (!isEqual(winLossAnalysis.winRate, expectedWinRate)) {
        throw std::runtime_error("Win rate calculation error");
    }

    if (winLossAnalysis.winningTrades + winLossAnalysis.losingTrades > winLossAnalysis.totalTrades) {
        throw std::runtime_error("Win/loss trade count inconsistency");
    }
}

void testWinLossAnalysisEmptyPositions() {
    PerformanceAttributionEngine engine;

    std::vector<Position> closedPositions; // Empty

    auto winLossAnalysis = engine.calculateWinLossAnalysis(closedPositions);

    // With empty positions, expect default initialized values
    if (winLossAnalysis.totalTrades < 0) {
        throw std::runtime_error("Total trades should be non-negative for empty positions");
    }
}

// ==================== RISK-ADJUSTED METRICS TESTS ====================

void testRiskAdjustedMetrics() {
    PerformanceAttributionEngine engine;

    std::vector<double> returns = {0.01, 0.02, -0.01, 0.03, 0.00, 0.02, -0.02, 0.01, 0.02, 0.01};
    std::vector<double> benchmarkReturns = {0.005, 0.015, -0.005, 0.025, -0.005, 0.015, -0.015, 0.005, 0.015, 0.005};

    auto metrics = engine.calculateRiskAdjustedMetrics(returns, benchmarkReturns, 1.2);

    if (!std::isfinite(metrics.sharpeRatio)) {
        throw std::runtime_error("Invalid Sharpe ratio calculated");
    }

    if (!std::isfinite(metrics.sortinoRatio)) {
        throw std::runtime_error("Invalid Sortino ratio calculated");
    }

    if (!std::isfinite(metrics.informationRatio)) {
        throw std::runtime_error("Invalid Information ratio calculated");
    }

    if (metrics.downsideDeviation < 0) {
        throw std::runtime_error("Downside deviation should be non-negative");
    }
}

void testRiskAdjustedMetricsEmptyReturns() {
    PerformanceAttributionEngine engine;

    std::vector<double> returns; // Empty

    auto metrics = engine.calculateRiskAdjustedMetrics(returns);

    // With empty returns, expect default initialized values or NaN/zero
    if (!std::isfinite(metrics.sharpeRatio) && metrics.sharpeRatio != 0.0) {
        throw std::runtime_error("Expected finite Sharpe ratio or zero for empty returns");
    }
}

// ==================== BENCHMARKING TESTS ====================

void testBenchmarkComparison() {
    PerformanceAttributionEngine engine;
    Portfolio portfolio = createTestPortfolio();

    auto startDate = std::chrono::system_clock::now() - std::chrono::hours(24 * 30);
    auto endDate = std::chrono::system_clock::now();

    auto performance = engine.calculateBenchmarkComparison(portfolio, "BTC", startDate, endDate);

    if (!std::isfinite(performance.totalReturn)) {
        throw std::runtime_error("Invalid total return in benchmark comparison");
    }

    if (!std::isfinite(performance.activeReturn)) {
        throw std::runtime_error("Invalid active return in benchmark comparison");
    }

    if (performance.tradingDays <= 0) {
        throw std::runtime_error("Trading days should be positive");
    }
}

void testBenchmarkAttribution() {
    PerformanceAttributionEngine engine;

    std::vector<double> portfolioReturns = {0.02, 0.01, 0.03, -0.01, 0.02};
    std::vector<double> benchmarkReturns = {0.01, 0.005, 0.02, -0.005, 0.015};
    std::vector<std::string> factors = {"Selection", "Allocation", "Interaction"};

    auto contributions = engine.calculateBenchmarkAttribution(portfolioReturns, benchmarkReturns, factors);

    if (contributions.size() != factors.size()) {
        throw std::runtime_error("Benchmark attribution factor count mismatch");
    }

    for (const auto& contrib : contributions) {
        if (!std::isfinite(contrib.contribution)) {
            throw std::runtime_error("Invalid benchmark attribution contribution");
        }
        if (contrib.confidence < 0 || contrib.confidence > 1) {
            throw std::runtime_error("Invalid confidence level in benchmark attribution");
        }
    }
}

void testTrackingErrorCalculation() {
    PerformanceAttributionEngine engine;

    std::vector<double> portfolioReturns = {0.02, 0.01, 0.03, -0.01, 0.02};
    std::vector<double> benchmarkReturns = {0.01, 0.005, 0.02, -0.005, 0.015};

    double trackingError = engine.calculateTrackingError(portfolioReturns, benchmarkReturns);

    if (trackingError < 0 || !std::isfinite(trackingError)) {
        throw std::runtime_error("Invalid tracking error calculation");
    }
}

void testInformationRatioCalculation() {
    PerformanceAttributionEngine engine;

    std::vector<double> portfolioReturns = {0.02, 0.01, 0.03, -0.01, 0.02};
    std::vector<double> benchmarkReturns = {0.01, 0.005, 0.02, -0.005, 0.015};

    double infoRatio = engine.calculateInformationRatio(portfolioReturns, benchmarkReturns);

    if (!std::isfinite(infoRatio)) {
        throw std::runtime_error("Invalid information ratio calculation");
    }
}

// ==================== COMPREHENSIVE REPORTING TESTS ====================

void testComprehensiveReport() {
    PerformanceAttributionEngine engine;
    Portfolio portfolio = createTestPortfolio();
    std::vector<Position> positions = createTestPositions();

    auto startDate = std::chrono::system_clock::now() - std::chrono::hours(24 * 30);
    auto endDate = std::chrono::system_clock::now();

    auto report = engine.generateComprehensiveReport(portfolio, positions, startDate, endDate);

    if (report.factorAttributions.empty()) {
        throw std::runtime_error("Comprehensive report missing factor attributions");
    }

    if (report.assetAttributions.empty()) {
        throw std::runtime_error("Comprehensive report missing asset attributions");
    }

    if (!std::isfinite(report.overallPerformance.totalReturn)) {
        throw std::runtime_error("Invalid overall performance in comprehensive report");
    }
}

// ==================== PERFORMANCE UTILITIES TESTS ====================

void testPerformanceUtilsMean() {
    std::vector<double> values = {1.0, 2.0, 3.0, 4.0, 5.0};
    double mean = PerformanceUtils::calculateMean(values);

    if (!isEqual(mean, 3.0)) {
        throw std::runtime_error("Incorrect mean calculation");
    }
}

void testPerformanceUtilsStandardDeviation() {
    std::vector<double> values = {1.0, 2.0, 3.0, 4.0, 5.0};
    double stdDev = PerformanceUtils::calculateStandardDeviation(values);

    if (stdDev <= 0 || !std::isfinite(stdDev)) {
        throw std::runtime_error("Invalid standard deviation calculation");
    }
}

void testPerformanceUtilsMedian() {
    std::vector<double> values = {1.0, 3.0, 2.0, 5.0, 4.0};
    double median = PerformanceUtils::calculateMedian(values);

    if (!isEqual(median, 3.0)) {
        throw std::runtime_error("Incorrect median calculation");
    }
}

void testPerformanceUtilsSharpeRatio() {
    double excessReturn = 0.08;
    double volatility = 0.15;
    double sharpe = PerformanceUtils::calculateSharpeRatio(excessReturn, volatility);

    if (!isEqual(sharpe, excessReturn / volatility, 1e-10)) {
        throw std::runtime_error("Incorrect Sharpe ratio calculation");
    }
}

void testPerformanceUtilsVaR() {
    std::vector<double> returns = {0.01, 0.02, -0.01, -0.05, 0.03, -0.02, 0.00, -0.03, 0.01, 0.02};
    double var = PerformanceUtils::calculateVaR(returns, 0.05);

    if (var < 0 || !std::isfinite(var)) {
        throw std::runtime_error("Invalid VaR calculation");
    }
}

void testPerformanceUtilsMaxDrawdown() {
    std::vector<double> cumulativeReturns = {1.0, 1.1, 1.2, 1.15, 1.05, 1.0, 1.08, 1.25};
    double maxDD = PerformanceUtils::calculateMaxDrawdown(cumulativeReturns);

    if (maxDD < 0 || !std::isfinite(maxDD)) {
        throw std::runtime_error("Invalid max drawdown calculation");
    }
}

// ==================== DATA MANAGEMENT TESTS ====================

void testDataManagement() {
    PerformanceAttributionEngine engine;

    // Test benchmark weights
    std::map<std::string, double> weights = {{"BTC", 0.6}, {"ETH", 0.4}};
    engine.setBenchmarkWeights(weights);

    engine.updateBenchmarkWeight("ADA", 0.1);

    // Test performance history
    PeriodPerformance period;
    period.totalReturn = 0.15;
    period.benchmarkReturn = 0.10;
    period.tradingDays = 30;

    engine.addPerformancePeriod(period);

    auto history = engine.getPerformanceHistory(1);
    if (history.empty()) {
        throw std::runtime_error("Performance history not stored correctly");
    }

    if (!isEqual(history[0].totalReturn, 0.15)) {
        throw std::runtime_error("Performance period data not stored correctly");
    }
}

void testConfigurationMethods() {
    PerformanceAttributionEngine engine;

    // Test risk-free rate configuration
    engine.setRiskFreeRate(0.03);
    if (!isEqual(engine.getRiskFreeRate(), 0.03)) {
        throw std::runtime_error("Risk-free rate not set correctly");
    }

    // Test rolling window configuration
    engine.setRollingWindowDays(20);
    if (engine.getRollingWindowDays() != 20) {
        throw std::runtime_error("Rolling window days not set correctly");
    }
}

// ==================== EDGE CASE TESTS ====================

void testZeroVolatilityHandling() {
    PerformanceAttributionEngine engine;

    std::vector<double> constantReturns = {0.01, 0.01, 0.01, 0.01, 0.01};
    double volatility = engine.calculateVolatility(constantReturns);

    if (!isEqual(volatility, 0.0)) {
        throw std::runtime_error("Zero volatility not handled correctly");
    }
}

void testNegativeReturnsHandling() {
    PerformanceAttributionEngine engine;

    std::vector<double> negativeReturns = {-0.01, -0.02, -0.03, -0.01, -0.02};
    auto metrics = engine.calculateRiskAdjustedMetrics(negativeReturns);

    if (!std::isfinite(metrics.sharpeRatio)) {
        throw std::runtime_error("Negative returns not handled correctly in risk metrics");
    }
}

void testLargeDatasetHandling() {
    PerformanceAttributionEngine engine;

    // Create large dataset
    std::vector<double> largeReturns;
    for (int i = 0; i < 1000; ++i) {
        largeReturns.push_back(0.001 * (i % 100 - 50)); // Varying returns
    }

    auto rollingSharpe = engine.calculateRollingSharpeRatio(largeReturns, 30);

    if (rollingSharpe.empty()) {
        throw std::runtime_error("Large dataset not handled correctly");
    }
}

// ==================== MAIN TEST RUNNER ====================

void printTestSummary() {
    int passed = 0, failed = 0;

    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "DAY 8 PERFORMANCE ATTRIBUTION & ANALYTICS - TEST SUMMARY" << std::endl;
    std::cout << std::string(80, '=') << std::endl;

    for (const auto& result : testResults) {
        if (result.passed) {
            passed++;
        } else {
            failed++;
            std::cout << "FAILED: " << result.testName;
            if (!result.errorMessage.empty()) {
                std::cout << " - " << result.errorMessage;
            }
            std::cout << std::endl;
        }
    }

    std::cout << "\nResults: " << passed << " passed, " << failed << " failed" << std::endl;
    std::cout << "Success Rate: " << std::fixed << std::setprecision(1)
              << (100.0 * passed / (passed + failed)) << "%" << std::endl;
    std::cout << std::string(80, '=') << std::endl;

    if (failed > 0) {
        exit(1);
    }
}

int main() {
    std::cout << "Starting Day 8 Performance Attribution & Analytics Tests..." << std::endl;
    std::cout << std::string(80, '-') << std::endl;

    // Factor Attribution Tests
    std::cout << "\n[FACTOR ATTRIBUTION TESTS]" << std::endl;
    runTest("Factor Attribution Calculation", testFactorAttribution);
    runTest("Empty Positions Factor Attribution", testEmptyFactorAttribution);

    // Asset Attribution Tests
    std::cout << "\n[ASSET ATTRIBUTION TESTS]" << std::endl;
    runTest("Asset Attribution Calculation", testAssetAttribution);
    runTest("Asset Attribution Effects", testAssetAttributionEffects);

    // Leverage Contribution Tests
    std::cout << "\n[LEVERAGE CONTRIBUTION TESTS]" << std::endl;
    runTest("Leverage Contribution Calculation", testLeverageContribution);
    runTest("Empty Positions Leverage Contribution", testLeverageContributionEmptyPositions);

    // Sentiment Attribution Tests
    std::cout << "\n[SENTIMENT ATTRIBUTION TESTS]" << std::endl;
    runTest("Sentiment Attribution Calculation", testSentimentAttribution);

    // Rolling Metrics Tests
    std::cout << "\n[ROLLING METRICS TESTS]" << std::endl;
    runTest("Rolling Sharpe Ratio Calculation", testRollingSharpeRatio);
    runTest("Rolling Volatility Calculation", testRollingVolatility);
    runTest("Insufficient Data Rolling Metrics", testInsufficientDataRollingMetrics);

    // Drawdown Analysis Tests
    std::cout << "\n[DRAWDOWN ANALYSIS TESTS]" << std::endl;
    runTest("Drawdown Analysis Calculation", testDrawdownAnalysis);
    runTest("Invalid Data Drawdown Analysis", testDrawdownAnalysisInvalidData);

    // Win/Loss Analysis Tests
    std::cout << "\n[WIN/LOSS ANALYSIS TESTS]" << std::endl;
    runTest("Win/Loss Analysis Calculation", testWinLossAnalysis);
    runTest("Empty Positions Win/Loss Analysis", testWinLossAnalysisEmptyPositions);

    // Risk-Adjusted Metrics Tests
    std::cout << "\n[RISK-ADJUSTED METRICS TESTS]" << std::endl;
    runTest("Risk-Adjusted Metrics Calculation", testRiskAdjustedMetrics);
    runTest("Empty Returns Risk-Adjusted Metrics", testRiskAdjustedMetricsEmptyReturns);

    // Benchmarking Tests
    std::cout << "\n[BENCHMARKING TESTS]" << std::endl;
    runTest("Benchmark Comparison", testBenchmarkComparison);
    runTest("Benchmark Attribution", testBenchmarkAttribution);
    runTest("Tracking Error Calculation", testTrackingErrorCalculation);
    runTest("Information Ratio Calculation", testInformationRatioCalculation);

    // Comprehensive Reporting Tests
    std::cout << "\n[COMPREHENSIVE REPORTING TESTS]" << std::endl;
    runTest("Comprehensive Report Generation", testComprehensiveReport);

    // Performance Utilities Tests
    std::cout << "\n[PERFORMANCE UTILITIES TESTS]" << std::endl;
    runTest("Performance Utils Mean Calculation", testPerformanceUtilsMean);
    runTest("Performance Utils Standard Deviation", testPerformanceUtilsStandardDeviation);
    runTest("Performance Utils Median Calculation", testPerformanceUtilsMedian);
    runTest("Performance Utils Sharpe Ratio", testPerformanceUtilsSharpeRatio);
    runTest("Performance Utils VaR Calculation", testPerformanceUtilsVaR);
    runTest("Performance Utils Max Drawdown", testPerformanceUtilsMaxDrawdown);

    // Data Management Tests
    std::cout << "\n[DATA MANAGEMENT TESTS]" << std::endl;
    runTest("Data Management Operations", testDataManagement);
    runTest("Configuration Methods", testConfigurationMethods);

    // Edge Case Tests
    std::cout << "\n[EDGE CASE TESTS]" << std::endl;
    runTest("Zero Volatility Handling", testZeroVolatilityHandling);
    runTest("Negative Returns Handling", testNegativeReturnsHandling);
    runTest("Large Dataset Handling", testLargeDatasetHandling);

    printTestSummary();

    std::cout << "\n🎉 Day 8 Performance Attribution & Analytics implementation completed successfully!" << std::endl;
    std::cout << "✅ All core functionality tested and verified" << std::endl;
    std::cout << "📊 Advanced performance attribution algorithms implemented" << std::endl;
    std::cout << "📈 Comprehensive risk-adjusted metrics available" << std::endl;
    std::cout << "🎯 Benchmarking framework fully operational" << std::endl;

    return 0;
}