#pragma once

#include "../Data/SmartCacheManager.h"
#include "../FeatureEngineering/FeatureCalculator.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <chrono>
#include <functional>

namespace CryptoClaude {
namespace ML {

using namespace CryptoClaude::Data;
using namespace CryptoClaude::FeatureEngineering;

// Personal-use ML configuration optimized for small data volumes
struct PersonalMLConfig {
    // Data volume constraints
    int maxSymbols;                    // Maximum symbols to process
    int maxHistoricalDays;            // Maximum historical data to use
    int minSamplesPerSymbol;          // Minimum samples required per symbol

    // Feature engineering constraints
    int maxFeatures;                   // Maximum number of features
    bool useSimpleFeatures;           // Use only basic features
    bool enableFeatureSelection;      // Enable automatic feature selection

    // Model constraints for personal use
    int maxModelComplexity;           // Limit model complexity
    bool useLightweightModels;        // Prefer lightweight models
    double maxTrainingTimeMinutes;    // Maximum training time

    // Validation constraints
    double minValidationAccuracy;     // Minimum acceptable accuracy
    int validationFolds;              // Number of CV folds
    bool useSimpleValidation;         // Use simple validation methods

    PersonalMLConfig()
        : maxSymbols(5), maxHistoricalDays(365), minSamplesPerSymbol(100),
          maxFeatures(20), useSimpleFeatures(true), enableFeatureSelection(true),
          maxModelComplexity(100), useLightweightModels(true), maxTrainingTimeMinutes(5.0),
          minValidationAccuracy(0.55), validationFolds(3), useSimpleValidation(true) {}
};

// Lightweight feature set for personal trading
struct PersonalFeatureSet {
    // Price-based features (always available)
    std::vector<double> prices;
    std::vector<double> returns;
    std::vector<double> volatility;
    std::vector<double> momentum;

    // Volume-based features (if available)
    std::vector<double> volume;
    std::vector<double> volumeMA;

    // Technical indicators (simple ones)
    std::vector<double> sma20;
    std::vector<double> sma50;
    std::vector<double> rsi;
    std::vector<double> bollingerBands;

    // Meta information
    std::string symbol;
    std::string timeframe;
    std::chrono::system_clock::time_point startDate;
    std::chrono::system_clock::time_point endDate;
    int totalSamples;

    PersonalFeatureSet() : totalSamples(0) {}
};

// Simple ML model for personal use
struct PersonalMLModel {
    std::string modelId;
    std::string symbol;
    std::string modelType; // "linear", "decision_tree", "simple_ensemble"

    // Model parameters (simplified)
    std::map<std::string, double> weights;
    std::vector<std::string> selectedFeatures;

    // Performance metrics
    double accuracy;
    double precision;
    double recall;
    double sharpeRatio;

    // Training info
    std::chrono::system_clock::time_point trainedAt;
    int trainingSamples;
    double trainingTime;

    PersonalMLModel() : accuracy(0.0), precision(0.0), recall(0.0),
                       sharpeRatio(0.0), trainingSamples(0), trainingTime(0.0) {}
};

// Personal ML prediction result
struct PersonalPrediction {
    std::string symbol;
    std::string signal; // "BUY", "SELL", "HOLD"
    double confidence; // 0.0 to 1.0
    double expectedReturn;
    double riskScore;

    // Supporting data
    std::map<std::string, double> featureImportance;
    std::vector<std::string> reasons;
    std::chrono::system_clock::time_point predictionTime;

    PersonalPrediction() : confidence(0.0), expectedReturn(0.0), riskScore(0.0) {
        predictionTime = std::chrono::system_clock::now();
    }
};

// Personal ML pipeline optimized for free API tiers and small data volumes
class PersonalMLPipeline {
public:
    PersonalMLPipeline(SmartCacheManager& cacheManager);
    ~PersonalMLPipeline() = default;

    // Initialization
    bool initialize(const PersonalMLConfig& config);
    void shutdown();

    // Data processing optimized for personal use
    PersonalFeatureSet extractFeaturesFromCache(const std::string& symbol,
                                               const std::string& timeframe,
                                               int days = 365);

    bool validateFeatureSet(const PersonalFeatureSet& features) const;
    PersonalFeatureSet selectBestFeatures(const PersonalFeatureSet& features) const;

    // Lightweight model training
    PersonalMLModel trainPersonalModel(const std::string& symbol,
                                      const PersonalFeatureSet& features);

    PersonalMLModel trainSimpleLinearModel(const PersonalFeatureSet& features);
    PersonalMLModel trainSimpleTreeModel(const PersonalFeatureSet& features);
    PersonalMLModel trainSimpleEnsemble(const PersonalFeatureSet& features);

    // Model management
    bool saveModel(const PersonalMLModel& model);
    PersonalMLModel loadModel(const std::string& modelId);
    std::vector<std::string> getAvailableModels(const std::string& symbol = "") const;
    bool deleteModel(const std::string& modelId);

    // Prediction with cached data
    PersonalPrediction makePrediction(const std::string& symbol,
                                    const std::string& modelId = "");

    std::vector<PersonalPrediction> makeBatchPredictions(
        const std::vector<std::string>& symbols);

    // Model validation for personal use
    struct ValidationResult {
        double accuracy;
        double precision;
        double recall;
        double f1Score;
        double sharpeRatio;
        int totalPredictions;
        int correctPredictions;
        std::vector<std::string> validationErrors;

        ValidationResult() : accuracy(0.0), precision(0.0), recall(0.0),
                           f1Score(0.0), sharpeRatio(0.0), totalPredictions(0),
                           correctPredictions(0) {}
    };

    ValidationResult validateModel(const PersonalMLModel& model,
                                 const PersonalFeatureSet& testData);

    ValidationResult crossValidateModel(const std::string& symbol,
                                      const PersonalFeatureSet& features);

    // Performance tracking for personal trading
    struct PerformanceMetrics {
        std::string symbol;
        std::map<std::string, double> dailyReturns;
        double totalReturn;
        double sharpeRatio;
        double maxDrawdown;
        int totalTrades;
        int winningTrades;
        double winRate;

        PerformanceMetrics() : totalReturn(0.0), sharpeRatio(0.0), maxDrawdown(0.0),
                              totalTrades(0), winningTrades(0), winRate(0.0) {}
    };

    void recordPredictionResult(const PersonalPrediction& prediction,
                               double actualReturn);

    PerformanceMetrics getPerformanceMetrics(const std::string& symbol,
                                           int days = 30) const;

    // Model optimization for personal constraints
    PersonalMLModel optimizeForPersonalUse(const PersonalMLModel& model);
    std::vector<std::string> recommendBestFeatures(const std::string& symbol) const;

    // Simple ensemble methods
    struct SimpleEnsemble {
        std::vector<PersonalMLModel> models;
        std::vector<double> weights;
        std::string combinationMethod; // "average", "weighted", "voting"

        SimpleEnsemble() : combinationMethod("weighted") {}
    };

    SimpleEnsemble createPersonalEnsemble(const std::vector<PersonalMLModel>& models);
    PersonalPrediction ensemblePrediction(const SimpleEnsemble& ensemble,
                                        const PersonalFeatureSet& features);

    // Automated model selection for personal use
    PersonalMLModel selectBestModel(const std::string& symbol);
    bool shouldRetrainModel(const PersonalMLModel& model) const;
    void autoRetrainModels(); // Retrain models that need updates

    // Feature importance analysis
    std::map<std::string, double> analyzeFeatureImportance(const PersonalMLModel& model);
    std::vector<std::string> getTopFeatures(const PersonalMLModel& model, int topN = 5);

    // Backtesting for personal strategies
    struct BacktestResult {
        std::string symbol;
        std::string strategy;
        double totalReturn;
        double sharpeRatio;
        double maxDrawdown;
        int totalTrades;
        double winRate;
        std::vector<double> dailyReturns;

        BacktestResult() : totalReturn(0.0), sharpeRatio(0.0), maxDrawdown(0.0),
                          totalTrades(0), winRate(0.0) {}
    };

    BacktestResult backtestPersonalStrategy(const std::string& symbol,
                                          const PersonalMLModel& model,
                                          int backtestDays = 90);

    // Configuration management
    void setPersonalConfig(const PersonalMLConfig& config) { config_ = config; }
    PersonalMLConfig getPersonalConfig() const { return config_; }

    // Statistics and monitoring
    struct MLStatistics {
        int totalModelsCreated;
        int activeModels;
        int totalPredictions;
        int accuratePredictions;
        double averageAccuracy;
        std::chrono::system_clock::time_point lastTraining;
        std::map<std::string, double> symbolPerformance;

        MLStatistics() : totalModelsCreated(0), activeModels(0), totalPredictions(0),
                        accuratePredictions(0), averageAccuracy(0.0) {}
    };

    MLStatistics getMLStatistics() const;
    void resetStatistics();

private:
    SmartCacheManager& cacheManager_;
    PersonalMLConfig config_;

    // Model storage
    std::map<std::string, PersonalMLModel> models_;
    std::map<std::string, PerformanceMetrics> performance_;
    MLStatistics statistics_;

    // Feature engineering
    std::vector<double> calculateSimpleReturns(const std::vector<double>& prices) const;
    std::vector<double> calculateVolatility(const std::vector<double>& returns, int window = 20) const;
    std::vector<double> calculateSMA(const std::vector<double>& prices, int period) const;
    std::vector<double> calculateRSI(const std::vector<double>& prices, int period = 14) const;
    std::vector<double> calculateMomentum(const std::vector<double>& prices, int period = 10) const;

    // Data validation helpers
    bool hasMinimumDataQuality(const PersonalFeatureSet& features) const;
    std::vector<int> findOutliers(const std::vector<double>& data) const;
    PersonalFeatureSet cleanFeatureData(const PersonalFeatureSet& features) const;

    // Simple ML implementations
    std::map<std::string, double> trainLinearRegression(const PersonalFeatureSet& features) const;
    std::map<std::string, double> trainSimpleTree(const PersonalFeatureSet& features) const;

    // Model evaluation
    double calculateAccuracy(const std::vector<double>& predictions,
                           const std::vector<double>& actual) const;
    double calculateSharpeRatio(const std::vector<double>& returns) const;

    // Utility functions
    std::string generateModelId(const std::string& symbol, const std::string& type) const;
    std::vector<double> parseCSVColumn(const std::string& csvData, int column) const;
    std::string serializeModel(const PersonalMLModel& model) const;
    PersonalMLModel deserializeModel(const std::string& serializedModel) const;
};

// Factory for creating optimized personal ML pipelines
class PersonalMLPipelineFactory {
public:
    enum class PersonalStrategy {
        CONSERVATIVE,     // Minimum features, simple models
        BALANCED,        // Moderate complexity for personal use
        PERFORMANCE,     // Higher complexity within personal limits
        MINIMAL          // Absolute minimum for proof of concept
    };

    static std::unique_ptr<PersonalMLPipeline> create(SmartCacheManager& cacheManager,
                                                     PersonalStrategy strategy);

    static PersonalMLConfig getConfigForStrategy(PersonalStrategy strategy);
};

// Utility functions for personal ML operations
class PersonalMLUtils {
public:
    // Data preprocessing for small datasets
    static std::vector<double> normalizeData(const std::vector<double>& data);
    static std::vector<double> removeOutliers(const std::vector<double>& data, double threshold = 3.0);

    // Simple feature selection
    static std::vector<int> selectTopFeatures(const PersonalFeatureSet& features, int maxFeatures);

    // Model comparison
    static PersonalMLModel selectBestModel(const std::vector<PersonalMLModel>& models);

    // Performance calculation
    static double calculatePersonalROI(const std::vector<PersonalPrediction>& predictions,
                                     const std::vector<double>& actualReturns);
};

} // namespace ML
} // namespace CryptoClaude