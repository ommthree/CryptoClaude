#pragma once

#include "RandomForestPredictor.h"
#include "EnsembleMLPredictor.h"
#include "../Data/RealDataPipeline.h"
#include "../Database/DatabaseManager.h"
#include "../Database/Models/MarketData.h"
#include "../Database/Models/SentimentData.h"
#include "../Analytics/MarketDataAnalyzer.h"
#include <memory>
#include <map>
#include <vector>
#include <string>
#include <chrono>
#include <mutex>
#include <atomic>
#include <functional>

namespace CryptoClaude {
namespace ML {

/**
 * Real Data ML Pipeline for production machine learning model preparation,
 * training, and signal generation using real market and sentiment data
 */
class RealDataMLPipeline {
public:
    RealDataMLPipeline(Data::RealDataPipeline& dataPipeline,
                      Database::DatabaseManager& dbManager);
    ~RealDataMLPipeline();

    // === INITIALIZATION AND LIFECYCLE ===

    bool initialize();
    void shutdown();
    bool isInitialized() const { return initialized_; }

    // === PIPELINE CONFIGURATION ===

    struct MLPipelineConfig {
        // Model configuration
        std::vector<std::string> targetSymbols = {"BTC", "ETH", "ADA", "DOT", "SOL"};
        std::chrono::hours predictionHorizon = std::chrono::hours(24);
        int featureCount = 16;

        // Training parameters
        int randomForestTrees = 100;
        int maxTreeDepth = 10;
        double featureSubsampleRatio = 0.7;
        double bootstrapSampleRatio = 0.8;
        int minTrainingSamples = 100;

        // Data preparation
        int historicalDaysForTraining = 365;
        int rollingWindowSize = 30;
        double trainTestSplit = 0.8;
        bool enableCrossValidation = true;
        int crossValidationFolds = 5;

        // Feature engineering
        bool enableTechnicalIndicators = true;
        bool enableSentimentFeatures = true;
        bool enableCrossAssetFeatures = true;
        bool enableVolumeFeatures = true;
        bool enableMomentumFeatures = true;

        // Model update settings
        std::chrono::hours modelRetrainingInterval = std::chrono::hours(24);
        double minModelAccuracy = 0.55;
        double modelDegradationThreshold = 0.05;
        bool enableAutomaticRetraining = true;

        // Signal generation
        double signalConfidenceThreshold = 0.6;
        double minSignalStrength = 0.1;
        bool enableMultiHorizonPredictions = false;
        std::vector<std::chrono::hours> predictionHorizons = {
            std::chrono::hours(1),
            std::chrono::hours(4),
            std::chrono::hours(24),
            std::chrono::hours(168) // 1 week
        };
    };

    void setMLPipelineConfig(const MLPipelineConfig& config);
    MLPipelineConfig getMLPipelineConfig() const { return config_; }

    // === MODEL MANAGEMENT ===

    // Model training and retraining
    bool trainModelsForAllSymbols();
    bool trainModelForSymbol(const std::string& symbol);
    bool retrainModelIfNeeded(const std::string& symbol);

    // Model validation and testing
    struct ModelValidationResult {
        std::string symbol;
        std::string modelType;
        bool isValid;
        double accuracy;
        double precision;
        double recall;
        double f1Score;
        double sharpeRatio;
        double maxDrawdown;
        std::chrono::system_clock::time_point validationTime;
        std::vector<std::string> validationErrors;
        std::vector<std::string> validationWarnings;
    };

    ModelValidationResult validateModel(const std::string& symbol);
    std::map<std::string, ModelValidationResult> validateAllModels();

    // Model performance monitoring
    struct ModelPerformance {
        std::string symbol;
        std::chrono::system_clock::time_point lastTraining;
        std::chrono::system_clock::time_point lastValidation;

        double currentAccuracy;
        double initialAccuracy;
        double accuracyDrift;

        int totalPredictions;
        int correctPredictions;
        int incorrectPredictions;

        double averageConfidence;
        double currentConfidence;

        std::chrono::milliseconds averageInferenceTime;
        std::chrono::milliseconds lastInferenceTime;

        bool needsRetraining;
        std::string lastError;
    };

    ModelPerformance getModelPerformance(const std::string& symbol) const;
    std::map<std::string, ModelPerformance> getAllModelPerformance() const;

    // === FEATURE ENGINEERING ===

    // Real-time feature extraction
    MLFeatureVector extractFeaturesForSymbol(const std::string& symbol);
    std::map<std::string, MLFeatureVector> extractFeaturesForAllSymbols();

    // Feature validation and quality
    struct FeatureValidationResult {
        std::string symbol;
        bool isValid;
        double qualityScore;
        int validFeatures;
        int invalidFeatures;
        std::vector<std::string> missingFeatures;
        std::vector<std::string> invalidFeatureValues;
        std::chrono::system_clock::time_point extractionTime;
    };

    FeatureValidationResult validateFeatures(const std::string& symbol,
                                            const MLFeatureVector& features);

    // Feature importance analysis
    std::map<std::string, double> getFeatureImportance(const std::string& symbol) const;
    std::map<std::string, std::map<std::string, double>> getAllFeatureImportance() const;

    // === PREDICTION AND SIGNAL GENERATION ===

    // Real-time predictions
    MLPrediction generatePrediction(const std::string& symbol);
    std::map<std::string, MLPrediction> generatePredictionsForAllSymbols();

    // Multi-horizon predictions
    std::map<std::chrono::hours, MLPrediction> generateMultiHorizonPredictions(
        const std::string& symbol);

    // Trading signals
    struct TradingSignal {
        std::string symbol;
        std::chrono::system_clock::time_point timestamp;
        std::chrono::hours horizon;

        enum class SignalType { BUY, SELL, HOLD } signal;
        double strength; // [0.0, 1.0]
        double confidence; // [0.0, 1.0]
        double expectedReturn;
        double riskScore;

        // Supporting data
        double currentPrice;
        MLPrediction prediction;
        std::map<std::string, double> featureContributions;
        std::string reasoning;
    };

    TradingSignal generateTradingSignal(const std::string& symbol);
    std::map<std::string, TradingSignal> generateTradingSignalsForAllSymbols();

    // Signal filtering and validation
    std::vector<TradingSignal> getHighConfidenceSignals(double minConfidence = 0.7,
                                                       double minStrength = 0.3);
    bool isSignalValid(const TradingSignal& signal) const;

    // === BACKTESTING AND PERFORMANCE ANALYSIS ===

    // Backtesting framework
    struct BacktestConfig {
        std::chrono::system_clock::time_point startDate;
        std::chrono::system_clock::time_point endDate;
        std::vector<std::string> symbols;
        double initialCapital = 100000.0;
        double transactionCosts = 0.001; // 0.1%
        bool enableRebalancing = true;
        std::chrono::hours rebalanceFrequency = std::chrono::hours(24);
    };

    struct BacktestResults {
        double totalReturn;
        double annualizedReturn;
        double sharpeRatio;
        double sortino_ratio;
        double maxDrawdown;
        double volatility;

        int totalTrades;
        int profitableTrades;
        double winRate;
        double averageTradeReturn;

        std::vector<double> dailyReturns;
        std::vector<double> cumulativeReturns;
        std::map<std::string, double> symbolReturns;

        std::chrono::system_clock::time_point backtestStart;
        std::chrono::system_clock::time_point backtestEnd;
    };

    BacktestResults runBacktest(const BacktestConfig& config);

    // Walk-forward analysis
    struct WalkForwardResult {
        std::vector<BacktestResults> periodResults;
        double averageReturn;
        double returnStability;
        double consistencyScore;
        bool isStrategyRobust;
    };

    WalkForwardResult runWalkForwardAnalysis(const BacktestConfig& baseConfig,
                                           std::chrono::hours windowSize = std::chrono::hours(24 * 30));

    // === MONITORING AND DIAGNOSTICS ===

    // Pipeline health monitoring
    struct PipelineHealth {
        bool overallHealth;
        int totalModels;
        int healthyModels;
        int modelsNeedingRetraining;
        double averageAccuracy;
        double averageConfidence;
        std::chrono::system_clock::time_point lastHealthCheck;
        std::vector<std::string> warnings;
        std::vector<std::string> errors;
    };

    PipelineHealth getPipelineHealth();
    void performHealthCheck();

    // Model diagnostics
    std::string generateModelDiagnosticsReport(const std::string& symbol) const;
    std::string generateSystemDiagnosticsReport() const;
    bool exportDiagnosticsToFile(const std::string& filePath) const;

    // === CALLBACKS AND EVENTS ===

    using ModelUpdateCallback = std::function<void(const std::string& symbol, const std::string& updateType)>;
    using SignalCallback = std::function<void(const TradingSignal& signal)>;
    using ErrorCallback = std::function<void(const std::string& symbol, const std::string& error)>;

    void setModelUpdateCallback(ModelUpdateCallback callback) { modelUpdateCallback_ = callback; }
    void setSignalCallback(SignalCallback callback) { signalCallback_ = callback; }
    void setErrorCallback(ErrorCallback callback) { errorCallback_ = callback; }

    // === PERFORMANCE OPTIMIZATION ===

    // Model caching and persistence
    bool saveModelToFile(const std::string& symbol, const std::string& filePath);
    bool loadModelFromFile(const std::string& symbol, const std::string& filePath);
    void clearModelCache();

    // Parallel processing
    void setMaxParallelThreads(int threads) { maxParallelThreads_ = threads; }
    int getMaxParallelThreads() const { return maxParallelThreads_; }

    // Resource monitoring
    struct ResourceUsage {
        double cpuUsage;
        size_t memoryUsage;
        size_t modelCacheSize;
        int activeInferences;
        std::chrono::milliseconds averageInferenceTime;
    };

    ResourceUsage getCurrentResourceUsage() const;

private:
    // Core dependencies
    Data::RealDataPipeline& dataPipeline_;
    Database::DatabaseManager& dbManager_;
    std::unique_ptr<Analytics::MarketDataAnalyzer> marketAnalyzer_;

    // ML models
    std::map<std::string, std::unique_ptr<RandomForestPredictor>> randomForestModels_;
    std::map<std::string, std::unique_ptr<EnsembleMLPredictor>> ensembleModels_;

    // Configuration and state
    MLPipelineConfig config_;
    std::atomic<bool> initialized_;
    std::atomic<bool> shutdownRequested_;
    std::atomic<int> maxParallelThreads_;

    // Threading and synchronization
    mutable std::mutex modelsMutex_;
    mutable std::mutex performanceMutex_;
    std::unique_ptr<std::thread> monitoringThread_;

    // Performance tracking
    mutable std::map<std::string, ModelPerformance> modelPerformance_;
    mutable std::chrono::system_clock::time_point lastPerformanceUpdate_;

    // Feature caching
    mutable std::map<std::string, MLFeatureVector> featureCache_;
    mutable std::map<std::string, std::chrono::system_clock::time_point> featureCacheTimestamps_;
    mutable std::mutex featureCacheMutex_;

    // Callbacks
    ModelUpdateCallback modelUpdateCallback_;
    SignalCallback signalCallback_;
    ErrorCallback errorCallback_;

    // === PRIVATE IMPLEMENTATION METHODS ===

    // Initialization helpers
    bool initializeMarketAnalyzer();
    bool createMLModelsForSymbols();
    bool loadExistingModels();

    // Data preparation
    std::vector<MLFeatureVector> prepareTrainingData(const std::string& symbol);
    std::vector<double> prepareTrainingTargets(const std::string& symbol,
                                              const std::vector<MLFeatureVector>& features);

    // Feature engineering implementation
    MLFeatureVector extractTechnicalFeatures(const std::string& symbol);
    MLFeatureVector extractSentimentFeatures(const std::string& symbol);
    MLFeatureVector extractCrossAssetFeatures(const std::string& symbol);
    void enrichFeaturesWithMarketContext(MLFeatureVector& features, const std::string& symbol);

    // Model training implementation
    bool trainRandomForestModel(const std::string& symbol,
                               const std::vector<MLFeatureVector>& features,
                               const std::vector<double>& targets);
    bool trainEnsembleModel(const std::string& symbol,
                           const std::vector<MLFeatureVector>& features,
                           const std::vector<double>& targets);

    // Prediction implementation
    MLPrediction generateRandomForestPrediction(const std::string& symbol,
                                               const MLFeatureVector& features);
    MLPrediction generateEnsemblePrediction(const std::string& symbol,
                                           const MLFeatureVector& features);

    // Signal generation implementation
    TradingSignal::SignalType classifyPredictionToSignal(const MLPrediction& prediction);
    double calculateSignalStrength(const MLPrediction& prediction);
    double calculateRiskScore(const std::string& symbol, const MLPrediction& prediction);

    // Validation implementation
    ModelValidationResult performModelValidation(const std::string& symbol);
    FeatureValidationResult performFeatureValidation(const std::string& symbol,
                                                     const MLFeatureVector& features);

    // Performance tracking
    void updateModelPerformance(const std::string& symbol, const MLPrediction& prediction,
                               bool wasAccurate, std::chrono::milliseconds inferenceTime);
    void checkModelPerformanceDegradation(const std::string& symbol);

    // Data retrieval helpers
    std::vector<Database::Models::MarketData> getHistoricalMarketData(
        const std::string& symbol, int days);
    std::vector<Database::Models::SentimentData> getHistoricalSentimentData(
        const std::string& symbol, int days);

    // Model management helpers
    bool isModelTrainingRequired(const std::string& symbol);
    bool isModelPerformanceDegraded(const std::string& symbol);
    void markModelForRetraining(const std::string& symbol, const std::string& reason);

    // Caching management
    void updateFeatureCache(const std::string& symbol, const MLFeatureVector& features);
    bool isFeatureCacheValid(const std::string& symbol,
                            std::chrono::minutes maxAge = std::chrono::minutes(15));
    void clearExpiredFeatureCache();

    // Threading helpers
    void monitoringThreadMain();
    std::vector<std::future<bool>> executeParallelModelTraining(const std::vector<std::string>& symbols);
    std::vector<std::future<MLPrediction>> executeParallelPredictions(const std::vector<std::string>& symbols);

    // Error handling
    void recordModelError(const std::string& symbol, const std::string& operation,
                         const std::string& error);
    void notifyError(const std::string& symbol, const std::string& error);

    // Utility functions
    double calculateReturnFromPrediction(const MLPrediction& prediction, double currentPrice);
    bool isMarketDataSufficient(const std::string& symbol, int requiredDays);
    std::vector<std::string> getActiveSymbols() const;
};

} // namespace ML
} // namespace CryptoClaude