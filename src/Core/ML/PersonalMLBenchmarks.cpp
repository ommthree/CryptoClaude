#include "PersonalMLBenchmarks.h"
#include <iostream>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <random>
#include <fstream>
#include <sstream>
#include <iomanip>

namespace CryptoClaude {
namespace ML {

// PersonalMLBenchmarks Implementation
PersonalMLBenchmarks::PersonalMLBenchmarks(PersonalMLPipeline& mlPipeline)
    : mlPipeline_(mlPipeline) {
    // Set default validation thresholds for personal use
    thresholds_.minAccuracy = 0.55;        // 55% minimum for personal trading
    thresholds_.minSharpeRatio = 0.5;      // Moderate Sharpe ratio
    thresholds_.maxDrawdown = 0.15;        // 15% maximum drawdown
    thresholds_.minInformationRatio = 0.3; // Modest information ratio
    thresholds_.minWinRate = 0.45;         // 45% minimum win rate
    thresholds_.minTrades = 10;            // At least 10 trades for validity
    thresholds_.maxVolatility = 0.25;      // 25% maximum volatility
}

std::vector<BenchmarkResult> PersonalMLBenchmarks::runComprehensiveBenchmarks(const BenchmarkConfig& config) {
    std::vector<BenchmarkResult> allResults;

    std::cout << "Starting comprehensive benchmarks for " << config.symbols.size()
              << " symbols and " << config.timeframes.size() << " timeframes" << std::endl;

    for (const auto& symbol : config.symbols) {
        for (const auto& timeframe : config.timeframes) {
            std::cout << "Benchmarking " << symbol << " (" << timeframe << ")" << std::endl;

            auto result = runSingleSymbolBenchmark(symbol, timeframe, config);
            if (!result.testId.empty()) {
                allResults.push_back(result);
            }
        }
    }

    std::cout << "Completed " << allResults.size() << " benchmark tests" << std::endl;
    return allResults;
}

BenchmarkResult PersonalMLBenchmarks::runSingleSymbolBenchmark(const std::string& symbol,
                                                              const std::string& timeframe,
                                                              const BenchmarkConfig& config) {
    BenchmarkResult result;
    result.testId = symbol + "_" + timeframe + "_" + std::to_string(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    result.symbol = symbol;
    result.timeframe = timeframe;
    result.testDate = std::chrono::system_clock::now();

    auto startTime = std::chrono::high_resolution_clock::now();

    // Extract features for the symbol
    auto features = mlPipeline_.extractFeaturesFromCache(symbol, timeframe,
                                                        config.minTrainingDays + config.backtestDays);

    if (!mlPipeline_.validateFeatureSet(features)) {
        result.failures.push_back("Insufficient or invalid feature data");
        return result;
    }

    // Split data into training and testing sets
    int totalSamples = features.totalSamples;
    int trainingSamples = static_cast<int>(totalSamples * config.trainTestSplit);
    int testSamples = totalSamples - trainingSamples;

    if (testSamples < config.minTrades) {
        result.warnings.push_back("Limited test samples: " + std::to_string(testSamples));
    }

    result.trainingSamples = trainingSamples;
    result.testSamples = testSamples;

    // Create training feature set
    PersonalFeatureSet trainingData = features;
    trainingData.prices.resize(trainingSamples);
    trainingData.returns.resize(std::min(trainingSamples - 1, (int)trainingData.returns.size()));
    trainingData.totalSamples = trainingSamples;

    // Train model
    auto model = mlPipeline_.trainPersonalModel(symbol, trainingData);
    if (model.modelId.empty()) {
        result.failures.push_back("Model training failed");
        return result;
    }

    result.modelType = model.modelType;

    // Create test feature set
    PersonalFeatureSet testData = features;
    if (trainingSamples < (int)testData.prices.size()) {
        testData.prices.erase(testData.prices.begin(), testData.prices.begin() + trainingSamples);
        if (trainingSamples < (int)testData.returns.size()) {
            testData.returns.erase(testData.returns.begin(), testData.returns.begin() + trainingSamples - 1);
        }
        testData.totalSamples = testSamples;
    }

    // Run accuracy tests
    auto accuracyResult = testModelAccuracy(model, testData);
    result.accuracy = accuracyResult.accuracy;
    result.precision = accuracyResult.precision;
    result.recall = accuracyResult.recall;
    result.f1Score = accuracyResult.f1Score;

    // Run trading performance tests
    auto performanceResult = testTradingPerformance(model, testData, config.backtestDays);
    result.totalReturn = performanceResult.totalReturn;
    result.sharpeRatio = performanceResult.sharpeRatio;
    result.maxDrawdown = performanceResult.maxDrawdown;
    result.volatility = performanceResult.volatility;
    result.winRate = performanceResult.winRate;
    result.totalTrades = performanceResult.totalTrades;
    result.winningTrades = performanceResult.winningTrades;

    // Run risk metric tests
    auto riskResult = testRiskMetrics(model, testData);
    result.informationRatio = riskResult.informationRatio;
    result.calmarRatio = riskResult.calmarRatio;
    result.sortinoRatio = riskResult.sortinoRatio;

    // Calculate test duration
    auto endTime = std::chrono::high_resolution_clock::now();
    result.testDurationSeconds = std::chrono::duration<double>(endTime - startTime).count();

    // Validate results
    result.passedAccuracyTest = validateAccuracy(result);
    result.passedPerformanceTest = validatePerformance(result);
    result.passedRiskTest = validateRiskMetrics(result);
    result.overallPassed = result.passedAccuracyTest && result.passedPerformanceTest && result.passedRiskTest;

    if (!result.passedAccuracyTest) {
        result.failures.push_back("Failed accuracy test (min: " + std::to_string(thresholds_.minAccuracy) + ")");
    }
    if (!result.passedPerformanceTest) {
        result.failures.push_back("Failed performance test");
    }
    if (!result.passedRiskTest) {
        result.failures.push_back("Failed risk test");
    }

    return result;
}

BenchmarkResult PersonalMLBenchmarks::testModelAccuracy(const PersonalMLModel& model,
                                                       const PersonalFeatureSet& testData) {
    BenchmarkResult result;

    if (testData.returns.empty() || testData.returns.size() < 10) {
        result.failures.push_back("Insufficient test data for accuracy testing");
        return result;
    }

    std::vector<double> predictions;
    std::vector<double> actual;

    // Generate predictions for test data
    for (int i = 0; i < (int)testData.returns.size() - 1; ++i) {
        // Simple prediction logic based on model type
        double prediction = 0.0;

        if (model.modelType == "linear" && !model.weights.empty()) {
            // Use momentum for linear prediction
            if (i < (int)testData.momentum.size()) {
                auto momentumIt = model.weights.find("momentum");
                auto interceptIt = model.weights.find("intercept");

                if (momentumIt != model.weights.end() && interceptIt != model.weights.end()) {
                    prediction = momentumIt->second * testData.momentum[i] + interceptIt->second;
                }
            }
        } else if (model.modelType == "decision_tree") {
            // Simple tree-based prediction
            if (i < (int)testData.rsi.size() && i < (int)testData.momentum.size()) {
                double rsi = testData.rsi[i];
                double momentum = testData.momentum[i];

                if (rsi < 30 && momentum > 0.02) {
                    prediction = 0.05; // Predict positive return
                } else if (rsi > 70 && momentum < -0.02) {
                    prediction = -0.05; // Predict negative return
                } else {
                    prediction = 0.0; // Neutral
                }
            }
        }

        predictions.push_back(prediction);
        actual.push_back(testData.returns[i + 1]);
    }

    // Calculate accuracy metrics
    if (!predictions.empty() && predictions.size() == actual.size()) {
        // Direction accuracy
        int correct = 0;
        int totalPositive = 0, totalNegative = 0;
        int truePositive = 0, falsePositive = 0;
        int trueNegative = 0, falseNegative = 0;

        for (size_t i = 0; i < predictions.size(); ++i) {
            bool predictedUp = predictions[i] > 0;
            bool actualUp = actual[i] > 0;

            if (predictedUp == actualUp) correct++;

            if (actualUp) totalPositive++;
            else totalNegative++;

            if (predictedUp && actualUp) truePositive++;
            else if (predictedUp && !actualUp) falsePositive++;
            else if (!predictedUp && !actualUp) trueNegative++;
            else if (!predictedUp && actualUp) falseNegative++;
        }

        result.accuracy = (double)correct / predictions.size();

        // Precision and Recall
        if (truePositive + falsePositive > 0) {
            result.precision = (double)truePositive / (truePositive + falsePositive);
        }
        if (truePositive + falseNegative > 0) {
            result.recall = (double)truePositive / (truePositive + falseNegative);
        }

        // F1 Score
        if (result.precision + result.recall > 0) {
            result.f1Score = 2.0 * (result.precision * result.recall) / (result.precision + result.recall);
        }
    }

    return result;
}

BenchmarkResult PersonalMLBenchmarks::testTradingPerformance(const PersonalMLModel& model,
                                                           const PersonalFeatureSet& historicalData,
                                                           int backtestDays) {
    BenchmarkResult result;

    if (historicalData.returns.empty()) {
        result.failures.push_back("No return data for performance testing");
        return result;
    }

    // Simple backtesting simulation
    std::vector<double> portfolioReturns;
    double totalReturn = 0.0;
    int trades = 0;
    int winningTrades = 0;
    double currentPosition = 0.0; // 1.0 = long, -1.0 = short, 0.0 = neutral

    for (int i = 1; i < std::min(backtestDays, (int)historicalData.returns.size()); ++i) {
        // Generate trading signal
        double signal = 0.0;

        if (model.modelType == "linear" && !model.weights.empty() && i < (int)historicalData.momentum.size()) {
            auto momentumIt = model.weights.find("momentum");
            if (momentumIt != model.weights.end()) {
                signal = momentumIt->second * historicalData.momentum[i];
            }
        }

        // Convert signal to position (simplified)
        double newPosition = 0.0;
        if (signal > 0.02) newPosition = 1.0;        // Long
        else if (signal < -0.02) newPosition = -1.0; // Short
        else newPosition = 0.0;                       // Neutral

        // Execute trade if position changes
        if (newPosition != currentPosition) {
            trades++;
        }

        // Calculate returns
        double dayReturn = 0.0;
        if (std::abs(currentPosition) > 0.01) {
            dayReturn = currentPosition * historicalData.returns[i];
            if (dayReturn > 0) winningTrades++;
        }

        portfolioReturns.push_back(dayReturn);
        totalReturn += dayReturn;
        currentPosition = newPosition;
    }

    // Calculate performance metrics
    result.totalReturn = totalReturn;
    result.totalTrades = trades;
    result.winningTrades = winningTrades;

    if (trades > 0) {
        result.winRate = (double)winningTrades / trades;
    }

    if (!portfolioReturns.empty()) {
        result.sharpeRatio = calculateSharpeRatio(portfolioReturns);
        result.maxDrawdown = calculateMaxDrawdown(portfolioReturns);

        // Calculate volatility
        double meanReturn = totalReturn / portfolioReturns.size();
        double variance = 0.0;
        for (double r : portfolioReturns) {
            variance += (r - meanReturn) * (r - meanReturn);
        }
        result.volatility = std::sqrt(variance / portfolioReturns.size());
    }

    return result;
}

BenchmarkResult PersonalMLBenchmarks::testRiskMetrics(const PersonalMLModel& model,
                                                     const PersonalFeatureSet& testData) {
    BenchmarkResult result;

    if (testData.returns.empty()) {
        result.failures.push_back("No data for risk metric calculation");
        return result;
    }

    // Use test data returns as proxy for strategy returns
    std::vector<double> returns = testData.returns;

    // Calculate risk-adjusted metrics
    result.sortinoRatio = calculateSortinoRatio(returns);

    // Create a simple benchmark (buy-and-hold)
    std::vector<double> benchmarkReturns = returns; // Simplified
    result.informationRatio = calculateInformationRatio(returns, benchmarkReturns);

    // Calmar ratio (return / max drawdown)
    if (result.maxDrawdown > 0) {
        double annualizedReturn = result.totalReturn * (252.0 / returns.size()); // Annualize
        result.calmarRatio = annualizedReturn / result.maxDrawdown;
    }

    return result;
}

PersonalMLBenchmarks::QuickValidation PersonalMLBenchmarks::quickValidateModel(
    const PersonalMLModel& model,
    const PersonalFeatureSet& testData) {

    QuickValidation validation;

    // Run basic tests
    auto accuracyResult = testModelAccuracy(model, testData);
    auto performanceResult = testTradingPerformance(model, testData, 30);

    // Calculate component scores (0-100)
    validation.componentScores["accuracy"] = std::min(100.0, accuracyResult.accuracy * 100.0);
    validation.componentScores["sharpe"] = std::min(100.0, std::max(0.0, performanceResult.sharpeRatio * 50.0));
    validation.componentScores["drawdown"] = std::min(100.0, std::max(0.0, (1.0 - performanceResult.maxDrawdown) * 100.0));
    validation.componentScores["win_rate"] = std::min(100.0, performanceResult.winRate * 100.0);

    // Calculate overall score
    validation.overallScore = 0.0;
    for (const auto& score : validation.componentScores) {
        validation.overallScore += score.second;
    }
    validation.overallScore /= validation.componentScores.size();

    // Check individual thresholds
    if (accuracyResult.accuracy >= thresholds_.minAccuracy) {
        validation.passedTests.push_back("Accuracy test passed");
    } else {
        validation.failedTests.push_back("Accuracy below threshold");
    }

    if (performanceResult.sharpeRatio >= thresholds_.minSharpeRatio) {
        validation.passedTests.push_back("Sharpe ratio test passed");
    } else {
        validation.failedTests.push_back("Sharpe ratio below threshold");
    }

    if (performanceResult.maxDrawdown <= thresholds_.maxDrawdown) {
        validation.passedTests.push_back("Drawdown test passed");
    } else {
        validation.failedTests.push_back("Drawdown exceeds threshold");
    }

    if (performanceResult.totalTrades >= thresholds_.minTrades) {
        validation.passedTests.push_back("Sufficient trading activity");
    } else {
        validation.warnings.push_back("Limited trading activity");
    }

    // Determine overall pass/fail
    validation.passed = validation.failedTests.empty() && validation.overallScore >= 60.0;

    return validation;
}

// Helper method implementations
double PersonalMLBenchmarks::calculateMaxDrawdown(const std::vector<double>& returns) {
    if (returns.empty()) return 0.0;

    std::vector<double> cumulativeReturns;
    double cumReturn = 0.0;

    for (double r : returns) {
        cumReturn += r;
        cumulativeReturns.push_back(cumReturn);
    }

    double maxDrawdown = 0.0;
    double peak = cumulativeReturns[0];

    for (size_t i = 1; i < cumulativeReturns.size(); ++i) {
        if (cumulativeReturns[i] > peak) {
            peak = cumulativeReturns[i];
        }

        double drawdown = (peak - cumulativeReturns[i]) / (1.0 + peak);
        maxDrawdown = std::max(maxDrawdown, drawdown);
    }

    return maxDrawdown;
}

double PersonalMLBenchmarks::calculateSharpeRatio(const std::vector<double>& returns, double riskFreeRate) {
    if (returns.empty()) return 0.0;

    double meanReturn = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
    double excessReturn = meanReturn - (riskFreeRate / 252.0); // Daily risk-free rate

    double variance = 0.0;
    for (double r : returns) {
        variance += (r - meanReturn) * (r - meanReturn);
    }
    double stdDev = std::sqrt(variance / returns.size());

    return (stdDev > 0) ? (excessReturn / stdDev) : 0.0;
}

double PersonalMLBenchmarks::calculateSortinoRatio(const std::vector<double>& returns, double targetReturn) {
    if (returns.empty()) return 0.0;

    double meanReturn = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();

    double downsideVariance = 0.0;
    int downsideCount = 0;

    for (double r : returns) {
        if (r < targetReturn) {
            downsideVariance += (r - targetReturn) * (r - targetReturn);
            downsideCount++;
        }
    }

    if (downsideCount == 0) return 0.0;

    double downsideStdDev = std::sqrt(downsideVariance / downsideCount);
    return (downsideStdDev > 0) ? ((meanReturn - targetReturn) / downsideStdDev) : 0.0;
}

double PersonalMLBenchmarks::calculateInformationRatio(const std::vector<double>& returns,
                                                      const std::vector<double>& benchmarkReturns) {
    if (returns.empty() || benchmarkReturns.empty() || returns.size() != benchmarkReturns.size()) {
        return 0.0;
    }

    std::vector<double> excessReturns;
    for (size_t i = 0; i < returns.size(); ++i) {
        excessReturns.push_back(returns[i] - benchmarkReturns[i]);
    }

    double meanExcess = std::accumulate(excessReturns.begin(), excessReturns.end(), 0.0) / excessReturns.size();

    double variance = 0.0;
    for (double r : excessReturns) {
        variance += (r - meanExcess) * (r - meanExcess);
    }
    double stdDev = std::sqrt(variance / excessReturns.size());

    return (stdDev > 0) ? (meanExcess / stdDev) : 0.0;
}

bool PersonalMLBenchmarks::validateAccuracy(const BenchmarkResult& result) {
    return result.accuracy >= thresholds_.minAccuracy;
}

bool PersonalMLBenchmarks::validatePerformance(const BenchmarkResult& result) {
    return result.sharpeRatio >= thresholds_.minSharpeRatio &&
           result.totalTrades >= thresholds_.minTrades &&
           result.winRate >= thresholds_.minWinRate;
}

bool PersonalMLBenchmarks::validateRiskMetrics(const BenchmarkResult& result) {
    return result.maxDrawdown <= thresholds_.maxDrawdown &&
           result.volatility <= thresholds_.maxVolatility;
}

// BenchmarkConfigFactory Implementation
BenchmarkConfig BenchmarkConfigFactory::createConfig(BenchmarkLevel level) {
    BenchmarkConfig config;

    switch (level) {
        case BenchmarkLevel::QUICK:
            config.symbols = {"BTC"};
            config.timeframes = {"1d"};
            config.backtestDays = 30;
            config.minTrainingDays = 90;
            config.minAccuracy = 0.50;
            break;

        case BenchmarkLevel::STANDARD:
            config.symbols = {"BTC", "ETH", "ADA"};
            config.timeframes = {"1d"};
            config.backtestDays = 60;
            config.minTrainingDays = 180;
            config.minAccuracy = 0.55;
            break;

        case BenchmarkLevel::COMPREHENSIVE:
            config.symbols = {"BTC", "ETH", "ADA", "DOT", "LINK"};
            config.timeframes = {"1d", "1h"};
            config.backtestDays = 90;
            config.minTrainingDays = 365;
            config.minAccuracy = 0.60;
            break;

        case BenchmarkLevel::MINIMAL:
            config.symbols = {"BTC"};
            config.timeframes = {"1d"};
            config.backtestDays = 14;
            config.minTrainingDays = 60;
            config.minAccuracy = 0.45;
            break;
    }

    return config;
}

PersonalMLBenchmarks::ValidationThresholds BenchmarkConfigFactory::getThresholds(BenchmarkLevel level) {
    PersonalMLBenchmarks::ValidationThresholds thresholds;

    switch (level) {
        case BenchmarkLevel::QUICK:
        case BenchmarkLevel::MINIMAL:
            thresholds.minAccuracy = 0.50;
            thresholds.minSharpeRatio = 0.3;
            thresholds.maxDrawdown = 0.20;
            break;

        case BenchmarkLevel::STANDARD:
            thresholds.minAccuracy = 0.55;
            thresholds.minSharpeRatio = 0.5;
            thresholds.maxDrawdown = 0.15;
            break;

        case BenchmarkLevel::COMPREHENSIVE:
            thresholds.minAccuracy = 0.60;
            thresholds.minSharpeRatio = 0.7;
            thresholds.maxDrawdown = 0.12;
            break;
    }

    return thresholds;
}

} // namespace ML
} // namespace CryptoClaude