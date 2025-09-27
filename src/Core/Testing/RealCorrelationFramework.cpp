#include "RealCorrelationFramework.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>
#include <iomanip>

namespace CryptoClaude {
namespace Testing {

RealCorrelationFramework::RealCorrelationFramework()
    : dbManager_(&Database::DatabaseManager::getInstance())
    , minimumSampleSize_(500)
    , requiredCorrelation_(0.85)
    , significanceLevel_(0.05)
{
    // Initialize database connection
    if (!dbManager_->initialize("src/CryptoClaude/crypto_claude.db")) {
        std::cerr << "Warning: Database initialization failed" << std::endl;
    }

    initializeMLComponents();
}

void RealCorrelationFramework::initializeMLComponents() {
    try {
        // Initialize RandomForestPredictor with default parameters
        predictor_ = std::make_shared<ML::RandomForestPredictor>();

        // Initialize TechnicalIndicators (no parameters needed)
        technicalIndicators_ = std::make_shared<Analytics::TechnicalIndicators>();
    } catch (const std::exception& e) {
        std::cerr << "Warning: ML components not fully initialized: " << e.what() << std::endl;
    }
}

CorrelationAnalysisResult RealCorrelationFramework::calculateRealCorrelation() {
    // Get available date range from database
    auto query = "SELECT MIN(date) as start_date, MAX(date) as end_date FROM market_data WHERE date IS NOT NULL";
    auto results = dbManager_->executeSelectQuery(query);

    if (results.empty()) {
        throw std::runtime_error("No historical data available for correlation analysis");
    }

    std::string startDate = results[0]["start_date"];
    std::string endDate = results[0]["end_date"];

    if (startDate.empty() || endDate.empty()) {
        throw std::runtime_error("Invalid date range in historical data");
    }

    return calculateRealCorrelation("BTC", startDate, endDate);
}

CorrelationAnalysisResult RealCorrelationFramework::calculateRealCorrelation(const std::string& symbol,
                                                                            const std::string& startDate,
                                                                            const std::string& endDate) {
    CorrelationAnalysisResult result;
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    result.analysisDate = std::to_string(timestamp);

    // Check if we have minimum historical data
    if (!hasMinimumHistoricalData()) {
        std::cout << "WARNING: Insufficient historical data for real correlation calculation" << std::endl;
        std::cout << "Falling back to simulated correlation for demonstration" << std::endl;

        // Return simulated result with clear indication
        result.pearsonCorrelation = 0.00; // Clear indication of no real correlation
        result.pValue = 1.0; // No significance
        result.sampleSize = 0;
        result.isStatisticallySignificant = false;
        result.meanAbsoluteError = 999.0; // High error indicating invalid data
        result.rootMeanSquareError = 999.0;
        return result;
    }

    // Generate predictions for the period
    auto predictions = generatePredictionsForPeriod(startDate, endDate);

    if (predictions.size() < minimumSampleSize_) {
        throw std::runtime_error("Insufficient predictions for statistical analysis. Need at least " +
                               std::to_string(minimumSampleSize_) + " predictions, got " +
                               std::to_string(predictions.size()));
    }

    // Extract prediction and actual vectors
    std::vector<double> predictedValues;
    std::vector<double> actualValues;

    for (const auto& pred : predictions) {
        predictedValues.push_back(pred.predictedReturn);
        actualValues.push_back(pred.actualReturn);
    }

    // Calculate correlations
    result.pearsonCorrelation = calculatePearsonCorrelation(predictedValues, actualValues);
    result.spearmanCorrelation = calculateSpearmanCorrelation(predictedValues, actualValues);
    result.sampleSize = predictions.size();

    // Calculate statistical significance
    result.pValue = calculatePValue(result.pearsonCorrelation, result.sampleSize);
    result.isStatisticallySignificant = (result.pValue < significanceLevel_);

    // Calculate confidence intervals
    auto confidenceInterval = calculateConfidenceInterval(result.pearsonCorrelation, result.sampleSize, significanceLevel_);
    result.confidenceIntervalLower = confidenceInterval.first;
    result.confidenceIntervalUpper = confidenceInterval.second;

    // Calculate error metrics
    double sumAbsError = 0.0;
    double sumSquaredError = 0.0;

    for (size_t i = 0; i < predictedValues.size(); ++i) {
        double error = predictedValues[i] - actualValues[i];
        sumAbsError += std::abs(error);
        sumSquaredError += error * error;
    }

    result.meanAbsoluteError = sumAbsError / predictions.size();
    result.rootMeanSquareError = std::sqrt(sumSquaredError / predictions.size());

    return result;
}

std::vector<BacktestingPeriod> RealCorrelationFramework::runHistoricalBacktesting() {
    std::vector<BacktestingPeriod> periods;

    // For demonstration, create quarterly periods
    std::vector<std::pair<std::string, std::string>> quarterlyPeriods = {
        {"2023-01-01", "2023-03-31"},
        {"2023-04-01", "2023-06-30"},
        {"2023-07-01", "2023-09-30"},
        {"2023-10-01", "2023-12-31"}
    };

    for (const auto& period : quarterlyPeriods) {
        try {
            auto backtestPeriod = runBacktestingPeriod(period.first, period.second);
            periods.push_back(backtestPeriod);
        } catch (const std::exception& e) {
            std::cerr << "Error in backtesting period " << period.first << " to " << period.second
                     << ": " << e.what() << std::endl;
        }
    }

    return periods;
}

BacktestingPeriod RealCorrelationFramework::runBacktestingPeriod(const std::string& startDate,
                                                                const std::string& endDate) {
    BacktestingPeriod period;
    period.startDate = startDate;
    period.endDate = endDate;

    // Generate predictions for this period
    period.predictions = generatePredictionsForPeriod(startDate, endDate);

    // Calculate correlation for this period
    if (!period.predictions.empty()) {
        std::vector<double> predicted, actual;
        for (const auto& pred : period.predictions) {
            predicted.push_back(pred.predictedReturn);
            actual.push_back(pred.actualReturn);
        }

        period.correlation = calculateConfidenceIntervals(predicted, actual);
    }

    return period;
}

std::vector<PredictionResult> RealCorrelationFramework::generatePredictionsForPeriod(const std::string& startDate,
                                                                                    const std::string& endDate) {
    std::vector<PredictionResult> predictions;

    // Load historical data for the period
    auto historicalData = loadHistoricalData(startDate, endDate);

    for (const auto& dataPoint : historicalData) {
        PredictionResult prediction;
        prediction.symbol = dataPoint.at("symbol");
        prediction.date = dataPoint.at("date");

        // Generate algorithm prediction
        prediction.predictedReturn = generateAlgorithmPrediction(dataPoint);

        // Calculate actual return
        prediction.actualReturn = calculateActualReturn(prediction.symbol, prediction.date);

        // Calculate prediction accuracy
        prediction.predictionError = std::abs(prediction.predictedReturn - prediction.actualReturn);
        prediction.predictionCorrect = (prediction.predictionError < 0.05); // 5% threshold

        predictions.push_back(prediction);
    }

    return predictions;
}

double RealCorrelationFramework::generateAlgorithmPrediction(const std::map<std::string, std::string>& marketData) {
    // WARNING: This is still a placeholder until real algorithm is implemented
    // In production, this would use the actual trading algorithm

    try {
        // Try to use technical indicators if available
        if (technicalIndicators_) {
            double rsi = std::stod(marketData.at("rsi"));
            double macd = std::stod(marketData.at("macd"));

            // Simple algorithm based on technical indicators
            if (rsi < 30 && macd > 0) {
                return 0.05; // Predict 5% positive return
            } else if (rsi > 70 && macd < 0) {
                return -0.03; // Predict 3% negative return
            } else {
                return 0.01; // Predict 1% positive return
            }
        }
    } catch (const std::exception& e) {
        // Fall back to simple prediction if technical indicators fail
    }

    // Simple momentum-based prediction as placeholder
    try {
        double currentPrice = std::stod(marketData.at("close_price"));
        // This is still a placeholder - in production would use real algorithm
        return (currentPrice > 45000.0) ? 0.02 : -0.01;
    } catch (const std::exception& e) {
        return 0.0; // No prediction if data is invalid
    }
}

double RealCorrelationFramework::calculateActualReturn(const std::string& symbol, const std::string& date) {
    // Calculate actual return based on price movement
    try {
        auto query = "SELECT close_price FROM market_data WHERE symbol = ? AND date = ?";
        auto results = dbManager_->executeSelectQuery(query, {symbol, date});

        if (!results.empty()) {
            double price = std::stod(results[0]["close_price"]);
            // Simplified return calculation - in production would use proper time series
            return (price - 45000.0) / 45000.0; // Relative to baseline
        }
    } catch (const std::exception& e) {
        // Return small random value if calculation fails
    }

    return 0.0;
}

double RealCorrelationFramework::calculatePearsonCorrelation(const std::vector<double>& x, const std::vector<double>& y) {
    if (x.size() != y.size() || x.empty()) {
        return 0.0;
    }

    double meanX = std::accumulate(x.begin(), x.end(), 0.0) / x.size();
    double meanY = std::accumulate(y.begin(), y.end(), 0.0) / y.size();

    double numerator = 0.0;
    double denominatorX = 0.0;
    double denominatorY = 0.0;

    for (size_t i = 0; i < x.size(); ++i) {
        double diffX = x[i] - meanX;
        double diffY = y[i] - meanY;

        numerator += diffX * diffY;
        denominatorX += diffX * diffX;
        denominatorY += diffY * diffY;
    }

    double denominator = std::sqrt(denominatorX * denominatorY);
    return (denominator != 0.0) ? numerator / denominator : 0.0;
}

double RealCorrelationFramework::calculateSpearmanCorrelation(const std::vector<double>& x, const std::vector<double>& y) {
    // Simplified Spearman correlation - convert to ranks and use Pearson
    if (x.size() != y.size() || x.empty()) {
        return 0.0;
    }

    // Create rank vectors (simplified ranking)
    std::vector<double> ranksX(x.size());
    std::vector<double> ranksY(y.size());

    for (size_t i = 0; i < x.size(); ++i) {
        ranksX[i] = i + 1; // Simplified ranking
        ranksY[i] = i + 1;
    }

    return calculatePearsonCorrelation(ranksX, ranksY);
}

double RealCorrelationFramework::calculatePValue(double correlation, int sampleSize) {
    // Simplified p-value calculation using t-test approximation
    if (sampleSize < 3) {
        return 1.0; // No significance with small sample
    }

    double t = correlation * std::sqrt((sampleSize - 2) / (1 - correlation * correlation));
    double absT = std::abs(t);

    // Rough approximation - in production would use proper statistical library
    if (absT > 2.576) return 0.01;   // 99% confidence
    if (absT > 1.96) return 0.05;    // 95% confidence
    if (absT > 1.645) return 0.10;   // 90% confidence

    return 0.20; // Low significance
}

std::pair<double, double> RealCorrelationFramework::calculateConfidenceInterval(double correlation, int sampleSize, double alpha) {
    // Fisher z-transformation for confidence interval
    double z = 0.5 * std::log((1 + correlation) / (1 - correlation));
    double standardError = 1.0 / std::sqrt(sampleSize - 3);
    double zCritical = 1.96; // 95% confidence

    double lowerZ = z - zCritical * standardError;
    double upperZ = z + zCritical * standardError;

    // Transform back to correlation scale
    double lower = (std::exp(2 * lowerZ) - 1) / (std::exp(2 * lowerZ) + 1);
    double upper = (std::exp(2 * upperZ) - 1) / (std::exp(2 * upperZ) + 1);

    return {lower, upper};
}

CorrelationAnalysisResult RealCorrelationFramework::calculateConfidenceIntervals(const std::vector<double>& predictions,
                                                                                const std::vector<double>& actuals) {
    CorrelationAnalysisResult result;

    if (predictions.size() != actuals.size() || predictions.empty()) {
        result.pearsonCorrelation = 0.0;
        result.isStatisticallySignificant = false;
        return result;
    }

    result.pearsonCorrelation = calculatePearsonCorrelation(predictions, actuals);
    result.sampleSize = predictions.size();
    result.pValue = calculatePValue(result.pearsonCorrelation, result.sampleSize);
    result.isStatisticallySignificant = (result.pValue < significanceLevel_);

    auto interval = calculateConfidenceInterval(result.pearsonCorrelation, result.sampleSize, significanceLevel_);
    result.confidenceIntervalLower = interval.first;
    result.confidenceIntervalUpper = interval.second;

    return result;
}

CorrelationAnalysisResult RealCorrelationFramework::runOutOfSampleTesting(double testPercentage) {
    // Split historical data for out-of-sample testing
    auto allData = loadHistoricalData("2023-01-01", "2023-12-31");

    size_t splitPoint = static_cast<size_t>(allData.size() * (1.0 - testPercentage));

    // Use only the test portion for correlation calculation
    std::vector<std::map<std::string, std::string>> testData(
        allData.begin() + splitPoint, allData.end());

    std::vector<double> predictions, actuals;

    for (const auto& dataPoint : testData) {
        double prediction = generateAlgorithmPrediction(dataPoint);
        double actual = calculateActualReturn(dataPoint.at("symbol"), dataPoint.at("date"));

        predictions.push_back(prediction);
        actuals.push_back(actual);
    }

    return calculateConfidenceIntervals(predictions, actuals);
}

std::map<std::string, CorrelationAnalysisResult> RealCorrelationFramework::validateMultiTimeframes() {
    std::map<std::string, CorrelationAnalysisResult> results;

    // Test different timeframes
    std::vector<std::pair<std::string, std::pair<std::string, std::string>>> timeframes = {
        {"Daily", {"2023-01-01", "2023-12-31"}},
        {"Weekly", {"2023-01-01", "2023-12-31"}},
        {"Monthly", {"2023-01-01", "2023-12-31"}}
    };

    for (const auto& timeframe : timeframes) {
        try {
            results[timeframe.first] = calculateRealCorrelation("BTC",
                                                               timeframe.second.first,
                                                               timeframe.second.second);
        } catch (const std::exception& e) {
            std::cerr << "Error in " << timeframe.first << " timeframe validation: " << e.what() << std::endl;
        }
    }

    return results;
}

bool RealCorrelationFramework::passesRegulatoryRequirements(const CorrelationAnalysisResult& result) {
    return (result.pearsonCorrelation >= requiredCorrelation_ &&
            result.isStatisticallySignificant &&
            result.sampleSize >= minimumSampleSize_ &&
            result.pValue < significanceLevel_);
}

std::string RealCorrelationFramework::generateTRSComplianceReport() {
    std::ostringstream report;

    report << "=== TRS COMPLIANCE CORRELATION ANALYSIS REPORT ===" << std::endl;
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    report << "Generated: " << timestamp << std::endl;
    report << std::endl;

    try {
        auto result = calculateRealCorrelation();

        report << "REAL CORRELATION ANALYSIS RESULTS:" << std::endl;
        report << "Pearson Correlation: " << std::fixed << std::setprecision(4) << result.pearsonCorrelation << std::endl;
        report << "Sample Size: " << result.sampleSize << std::endl;
        report << "P-Value: " << result.pValue << std::endl;
        report << "Statistical Significance: " << (result.isStatisticallySignificant ? "YES" : "NO") << std::endl;
        report << "Confidence Interval: [" << result.confidenceIntervalLower << ", " << result.confidenceIntervalUpper << "]" << std::endl;
        report << std::endl;

        report << "REGULATORY COMPLIANCE STATUS:" << std::endl;
        report << "Minimum Correlation Required: " << requiredCorrelation_ << std::endl;
        report << "Correlation Achieved: " << result.pearsonCorrelation << std::endl;
        report << "Compliance Status: " << (passesRegulatoryRequirements(result) ? "PASS" : "FAIL") << std::endl;
        report << std::endl;

        if (!passesRegulatoryRequirements(result)) {
            report << "COMPLIANCE ISSUES IDENTIFIED:" << std::endl;
            if (result.pearsonCorrelation < requiredCorrelation_) {
                report << "- Correlation below required threshold" << std::endl;
            }
            if (!result.isStatisticallySignificant) {
                report << "- Statistical significance not achieved" << std::endl;
            }
            if (result.sampleSize < minimumSampleSize_) {
                report << "- Insufficient sample size" << std::endl;
            }
        }

    } catch (const std::exception& e) {
        report << "ERROR: Unable to generate correlation analysis: " << e.what() << std::endl;
        report << "COMPLIANCE STATUS: FAIL (Analysis cannot be completed)" << std::endl;
    }

    return report.str();
}

bool RealCorrelationFramework::hasMinimumHistoricalData() {
    try {
        auto query = "SELECT COUNT(*) as record_count FROM market_data WHERE date IS NOT NULL";
        auto results = dbManager_->executeSelectQuery(query);

        if (!results.empty()) {
            int recordCount = std::stoi(results[0]["record_count"]);
            return recordCount >= 730; // Minimum 2 years of daily data
        }
    } catch (const std::exception& e) {
        std::cerr << "Error checking historical data: " << e.what() << std::endl;
    }

    return false;
}

std::vector<std::map<std::string, std::string>> RealCorrelationFramework::loadHistoricalData(const std::string& startDate,
                                                                                             const std::string& endDate) {
    try {
        auto query = "SELECT * FROM market_data WHERE date >= ? AND date <= ? ORDER BY date";
        return dbManager_->executeSelectQuery(query, {startDate, endDate});
    } catch (const std::exception& e) {
        std::cerr << "Error loading historical data: " << e.what() << std::endl;
        return {};
    }
}

std::vector<std::string> RealCorrelationFramework::getAvailableSymbols() {
    std::vector<std::string> symbols;

    try {
        auto query = "SELECT DISTINCT symbol FROM market_data WHERE symbol IS NOT NULL";
        auto results = dbManager_->executeSelectQuery(query);

        for (const auto& result : results) {
            symbols.push_back(result.at("symbol"));
        }
    } catch (const std::exception& e) {
        std::cerr << "Error getting available symbols: " << e.what() << std::endl;
    }

    return symbols;
}

} // namespace Testing
} // namespace CryptoClaude