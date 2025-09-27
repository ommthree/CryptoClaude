#pragma once

#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <map>
#include "../Database/DatabaseManager.h"
#include "../ML/RandomForestPredictor.h"
#include "../Analytics/TechnicalIndicators.h"

namespace CryptoClaude {
namespace Testing {

struct PredictionResult {
    std::string symbol;
    std::string date;
    double predictedReturn;
    double actualReturn;
    double predictionError;
    bool predictionCorrect;
};

struct CorrelationAnalysisResult {
    double pearsonCorrelation;
    double spearmanCorrelation;
    double pValue;
    double confidenceIntervalLower;
    double confidenceIntervalUpper;
    int sampleSize;
    bool isStatisticallySignificant;
    double meanAbsoluteError;
    double rootMeanSquareError;
    std::string analysisDate;
};

struct BacktestingPeriod {
    std::string startDate;
    std::string endDate;
    std::vector<PredictionResult> predictions;
    CorrelationAnalysisResult correlation;
};

class RealCorrelationFramework {
public:
    RealCorrelationFramework();
    ~RealCorrelationFramework() = default;

    // Main correlation measurement functions
    CorrelationAnalysisResult calculateRealCorrelation();
    CorrelationAnalysisResult calculateRealCorrelation(const std::string& symbol,
                                                       const std::string& startDate,
                                                       const std::string& endDate);

    // Backtesting framework
    std::vector<BacktestingPeriod> runHistoricalBacktesting();
    BacktestingPeriod runBacktestingPeriod(const std::string& startDate,
                                          const std::string& endDate);

    // Statistical validation
    bool validateStatisticalSignificance(const CorrelationAnalysisResult& result);
    CorrelationAnalysisResult calculateConfidenceIntervals(const std::vector<double>& predictions,
                                                          const std::vector<double>& actuals);

    // Out-of-sample testing
    CorrelationAnalysisResult runOutOfSampleTesting(double testPercentage = 0.2);

    // Multi-timeframe validation
    std::map<std::string, CorrelationAnalysisResult> validateMultiTimeframes();

    // Regulatory compliance validation
    bool passesRegulatoryRequirements(const CorrelationAnalysisResult& result);
    std::string generateTRSComplianceReport();

    // Configuration
    void setMinimumSampleSize(int minSize) { minimumSampleSize_ = minSize; }
    void setRequiredCorrelation(double minCorr) { requiredCorrelation_ = minCorr; }
    void setSignificanceLevel(double alpha) { significanceLevel_ = alpha; }

private:
    Database::DatabaseManager* dbManager_;
    std::shared_ptr<ML::RandomForestPredictor> predictor_;
    std::shared_ptr<Analytics::TechnicalIndicators> technicalIndicators_;

    // Configuration parameters
    int minimumSampleSize_;
    double requiredCorrelation_;
    double significanceLevel_;

    // Internal methods
    std::vector<PredictionResult> generatePredictionsForPeriod(const std::string& startDate,
                                                              const std::string& endDate);
    double calculateActualReturn(const std::string& symbol, const std::string& date);
    double calculatePearsonCorrelation(const std::vector<double>& x, const std::vector<double>& y);
    double calculateSpearmanCorrelation(const std::vector<double>& x, const std::vector<double>& y);
    double calculatePValue(double correlation, int sampleSize);
    std::pair<double, double> calculateConfidenceInterval(double correlation, int sampleSize, double alpha);

    // Data loading methods
    std::vector<std::map<std::string, std::string>> loadHistoricalData(const std::string& startDate,
                                                                       const std::string& endDate);
    bool hasMinimumHistoricalData();
    std::vector<std::string> getAvailableSymbols();

    // Algorithm integration
    double generateAlgorithmPrediction(const std::map<std::string, std::string>& marketData);
    void initializeMLComponents();
};

} // namespace Testing
} // namespace CryptoClaude