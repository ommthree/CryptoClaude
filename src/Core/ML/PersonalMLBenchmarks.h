#pragma once

#include "PersonalMLPipeline.h"
#include <string>
#include <vector>
#include <map>
#include <chrono>

namespace CryptoClaude {
namespace ML {

// Benchmark test configuration
struct BenchmarkConfig {
    std::vector<std::string> symbols;
    std::vector<std::string> timeframes;
    int backtestDays;
    int minTrainingDays;
    double trainTestSplit; // 0.7 = 70% train, 30% test

    // Evaluation criteria
    double minAccuracy;
    double minSharpeRatio;
    double maxDrawdown;
    int minTrades;

    BenchmarkConfig()
        : backtestDays(90), minTrainingDays(180), trainTestSplit(0.7),
          minAccuracy(0.55), minSharpeRatio(0.5), maxDrawdown(0.15), minTrades(10) {
        symbols = {"BTC", "ETH", "ADA"};
        timeframes = {"1d"};
    }
};

// Individual benchmark result
struct BenchmarkResult {
    std::string testId;
    std::string symbol;
    std::string timeframe;
    std::string modelType;

    // Accuracy metrics
    double accuracy;
    double precision;
    double recall;
    double f1Score;

    // Trading performance metrics
    double totalReturn;
    double sharpeRatio;
    double maxDrawdown;
    double volatility;
    double winRate;
    int totalTrades;
    int winningTrades;

    // Benchmark-specific metrics
    double informationRatio;
    double calmarRatio;
    double sortinoRatio;

    // Test parameters
    int trainingSamples;
    int testSamples;
    std::chrono::system_clock::time_point testDate;
    double testDurationSeconds;

    // Validation status
    bool passedAccuracyTest;
    bool passedPerformanceTest;
    bool passedRiskTest;
    bool overallPassed;

    std::vector<std::string> warnings;
    std::vector<std::string> failures;

    BenchmarkResult()
        : accuracy(0.0), precision(0.0), recall(0.0), f1Score(0.0),
          totalReturn(0.0), sharpeRatio(0.0), maxDrawdown(0.0), volatility(0.0),
          winRate(0.0), totalTrades(0), winningTrades(0),
          informationRatio(0.0), calmarRatio(0.0), sortinoRatio(0.0),
          trainingSamples(0), testSamples(0), testDurationSeconds(0.0),
          passedAccuracyTest(false), passedPerformanceTest(false),
          passedRiskTest(false), overallPassed(false) {}
};

// Comprehensive benchmark suite for personal trading
class PersonalMLBenchmarks {
public:
    PersonalMLBenchmarks(PersonalMLPipeline& mlPipeline);
    ~PersonalMLBenchmarks() = default;

    // Main benchmark functions
    std::vector<BenchmarkResult> runComprehensiveBenchmarks(const BenchmarkConfig& config);
    BenchmarkResult runSingleSymbolBenchmark(const std::string& symbol,
                                            const std::string& timeframe,
                                            const BenchmarkConfig& config);

    // Specific benchmark tests
    BenchmarkResult testModelAccuracy(const PersonalMLModel& model,
                                    const PersonalFeatureSet& testData);

    BenchmarkResult testTradingPerformance(const PersonalMLModel& model,
                                         const PersonalFeatureSet& historicalData,
                                         int backtestDays);

    BenchmarkResult testRiskMetrics(const PersonalMLModel& model,
                                   const PersonalFeatureSet& testData);

    // Comparative benchmarks
    struct ComparativeResult {
        std::string benchmarkName;
        std::map<std::string, BenchmarkResult> modelResults;
        std::string bestModelType;
        double improvementPercent;
        std::vector<std::string> recommendations;

        ComparativeResult() : improvementPercent(0.0) {}
    };

    ComparativeResult compareModels(const std::vector<PersonalMLModel>& models,
                                   const PersonalFeatureSet& testData);

    ComparativeResult benchmarkAgainstBaseline(const PersonalMLModel& model,
                                              const PersonalFeatureSet& testData);

    // Performance analysis
    struct PerformanceAnalysis {
        std::string symbol;
        std::map<std::string, double> monthlyReturns;
        std::vector<double> dailyReturns;
        std::vector<double> drawdownSeries;
        double averageMonthlyReturn;
        double bestMonth;
        double worstMonth;
        int consecutiveWins;
        int consecutiveLosses;

        PerformanceAnalysis() : averageMonthlyReturn(0.0), bestMonth(0.0),
                               worstMonth(0.0), consecutiveWins(0), consecutiveLosses(0) {}
    };

    PerformanceAnalysis analyzePerformance(const std::vector<PersonalPrediction>& predictions,
                                          const std::vector<double>& actualReturns);

    // Baseline strategies for comparison
    enum class BaselineStrategy {
        BUY_AND_HOLD,
        RANDOM,
        MOVING_AVERAGE,
        RSI_SIMPLE,
        MOMENTUM
    };

    BenchmarkResult runBaselineStrategy(BaselineStrategy strategy,
                                       const PersonalFeatureSet& data);

    // Statistical significance tests
    struct StatisticalTest {
        std::string testName;
        double pValue;
        double testStatistic;
        bool isSignificant;
        std::string interpretation;

        StatisticalTest() : pValue(1.0), testStatistic(0.0), isSignificant(false) {}
    };

    std::vector<StatisticalTest> testStatisticalSignificance(
        const std::vector<double>& modelReturns,
        const std::vector<double>& baselineReturns);

    // Out-of-sample testing
    struct OutOfSampleTest {
        std::string testPeriod;
        std::vector<BenchmarkResult> periodResults;
        double averageAccuracy;
        double consistencyScore;
        bool passedConsistencyTest;

        OutOfSampleTest() : averageAccuracy(0.0), consistencyScore(0.0),
                           passedConsistencyTest(false) {}
    };

    OutOfSampleTest runOutOfSampleTest(const PersonalMLModel& model,
                                      const PersonalFeatureSet& fullData,
                                      int windowDays = 30,
                                      int stepDays = 7);

    // Walk-forward analysis
    struct WalkForwardResult {
        std::vector<BenchmarkResult> periodResults;
        double averageAccuracy;
        double accuracyStdDev;
        double averageReturn;
        double returnStdDev;
        bool passedStabilityTest;

        WalkForwardResult() : averageAccuracy(0.0), accuracyStdDev(0.0),
                             averageReturn(0.0), returnStdDev(0.0),
                             passedStabilityTest(false) {}
    };

    WalkForwardResult runWalkForwardAnalysis(const std::string& symbol,
                                           int windowDays = 90,
                                           int stepDays = 30);

    // Model stability tests
    struct StabilityTest {
        std::string testType;
        double stabilityScore; // 0.0 to 1.0
        std::vector<double> metricVariation;
        bool passedStabilityThreshold;
        std::vector<std::string> instabilityWarnings;

        StabilityTest() : stabilityScore(0.0), passedStabilityThreshold(false) {}
    };

    StabilityTest testModelStability(const PersonalMLModel& model,
                                   const PersonalFeatureSet& data,
                                   int numberOfRuns = 10);

    // Data sufficiency analysis
    struct DataSufficiencyTest {
        std::string symbol;
        int minimumSamplesNeeded;
        int currentSamples;
        bool hasSufficientData;
        double dataQualityScore;
        std::vector<std::string> dataIssues;

        DataSufficiencyTest() : minimumSamplesNeeded(0), currentSamples(0),
                               hasSufficientData(false), dataQualityScore(0.0) {}
    };

    DataSufficiencyTest testDataSufficiency(const PersonalFeatureSet& data);

    // Benchmark reporting
    struct BenchmarkReport {
        std::string reportId;
        std::chrono::system_clock::time_point generatedAt;
        BenchmarkConfig config;
        std::vector<BenchmarkResult> allResults;
        ComparativeResult comparison;
        PerformanceAnalysis performance;
        std::vector<StatisticalTest> statisticalTests;
        std::vector<std::string> summary;
        std::vector<std::string> recommendations;
        bool overallPassed;

        BenchmarkReport() : overallPassed(false) {}
    };

    BenchmarkReport generateComprehensiveReport(const BenchmarkConfig& config);
    bool saveBenchmarkReport(const BenchmarkReport& report, const std::string& filePath);

    // Validation thresholds for personal use
    struct ValidationThresholds {
        double minAccuracy;
        double minSharpeRatio;
        double maxDrawdown;
        double minInformationRatio;
        double minWinRate;
        int minTrades;
        double maxVolatility;

        ValidationThresholds()
            : minAccuracy(0.55), minSharpeRatio(0.5), maxDrawdown(0.15),
              minInformationRatio(0.3), minWinRate(0.45), minTrades(10),
              maxVolatility(0.25) {}
    };

    void setValidationThresholds(const ValidationThresholds& thresholds);
    ValidationThresholds getValidationThresholds() const { return thresholds_; }

    // Quick validation for personal models
    struct QuickValidation {
        bool passed;
        double overallScore; // 0.0 to 100.0
        std::map<std::string, double> componentScores;
        std::vector<std::string> passedTests;
        std::vector<std::string> failedTests;
        std::vector<std::string> warnings;

        QuickValidation() : passed(false), overallScore(0.0) {}
    };

    QuickValidation quickValidateModel(const PersonalMLModel& model,
                                     const PersonalFeatureSet& testData);

    // Batch validation for multiple models
    std::vector<QuickValidation> batchValidateModels(
        const std::vector<PersonalMLModel>& models,
        const std::map<std::string, PersonalFeatureSet>& testDataBySymbol);

private:
    PersonalMLPipeline& mlPipeline_;
    ValidationThresholds thresholds_;

    // Helper methods for calculations
    std::vector<double> calculateDailyReturns(const std::vector<PersonalPrediction>& predictions,
                                            const std::vector<double>& actualReturns);

    double calculateMaxDrawdown(const std::vector<double>& returns);
    double calculateSharpeRatio(const std::vector<double>& returns, double riskFreeRate = 0.02);
    double calculateSortinoRatio(const std::vector<double>& returns, double targetReturn = 0.0);
    double calculateInformationRatio(const std::vector<double>& returns,
                                   const std::vector<double>& benchmarkReturns);

    // Statistical test implementations
    StatisticalTest tTestForReturns(const std::vector<double>& returns1,
                                   const std::vector<double>& returns2);

    // Validation helpers
    bool validateAccuracy(const BenchmarkResult& result);
    bool validatePerformance(const BenchmarkResult& result);
    bool validateRiskMetrics(const BenchmarkResult& result);

    // Report generation helpers
    std::string generateSummary(const std::vector<BenchmarkResult>& results);
    std::vector<std::string> generateRecommendations(const BenchmarkReport& report);

    // Data quality helpers
    bool hasMinimumDataQuality(const PersonalFeatureSet& data);
    double assessDataQuality(const PersonalFeatureSet& data);
};

// Factory for creating benchmark configurations
class BenchmarkConfigFactory {
public:
    enum class BenchmarkLevel {
        QUICK,           // Fast validation for development
        STANDARD,        // Normal benchmark for personal use
        COMPREHENSIVE,   // Thorough benchmark for production
        MINIMAL          // Absolute minimum for proof of concept
    };

    static BenchmarkConfig createConfig(BenchmarkLevel level);
    static PersonalMLBenchmarks::ValidationThresholds getThresholds(BenchmarkLevel level);
};

} // namespace ML
} // namespace CryptoClaude