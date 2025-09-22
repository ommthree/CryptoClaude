#pragma once
#include "IModelTrainer.h"
#include "../FeatureEngineering/FeatureCalculator.h"
#include "../Database/Repositories/MarketDataRepository.h"
#include "../Database/Repositories/SentimentRepository.h"
#include <memory>
#include <vector>
#include <map>

namespace CryptoClaude {
namespace MachineLearning {

// Daily prediction output
struct DailyPrediction {
    std::string symbol;
    std::string date;
    double predictedInflow;
    double expectedPriceChange;
    double confidence;
    int rank;                   // Ranking among all predictions
    double alpha;               // Expected excess return

    DailyPrediction()
        : predictedInflow(0.0), expectedPriceChange(0.0),
          confidence(0.0), rank(0), alpha(0.0) {}
};

// Portfolio recommendations based on predictions
struct PortfolioRecommendation {
    std::string date;
    std::vector<std::string> longPositions;
    std::vector<std::string> shortPositions;
    std::vector<DailyPrediction> allPredictions;
    double expectedPortfolioReturn;
    double portfolioRisk;

    PortfolioRecommendation() : expectedPortfolioReturn(0.0), portfolioRisk(0.0) {}
};

class PredictionService {
public:
    PredictionService();
    ~PredictionService() = default;

    // Service initialization
    void setModelTrainer(std::unique_ptr<IModelTrainer> trainer);
    void setFeatureCalculator(std::shared_ptr<FeatureEngineering::FeatureCalculator> calculator);
    void setMarketDataRepository(std::shared_ptr<Database::Repositories::MarketDataRepository> repository);
    void setSentimentRepository(std::shared_ptr<Database::Repositories::SentimentRepository> repository);

    // Model management
    bool trainModel(const std::string& startDate = "", const std::string& endDate = "");
    bool loadModel(const std::string& modelPath);
    bool saveModel(const std::string& modelPath);

    // Prediction generation
    std::vector<DailyPrediction> generateDailyPredictions(
        const std::string& targetDate,
        const std::vector<std::string>& symbols = {});

    PortfolioRecommendation generatePortfolioRecommendation(
        const std::string& targetDate,
        int longPositions = 10,
        int shortPositions = 10);

    // Batch predictions for backtesting
    std::map<std::string, std::vector<DailyPrediction>> generatePredictionSeries(
        const std::string& startDate,
        const std::string& endDate,
        const std::vector<std::string>& symbols = {});

    // Real-time prediction updates
    bool updatePredictionsForToday();
    std::vector<DailyPrediction> getLatestPredictions();

    // Model evaluation and monitoring
    ModelMetrics evaluateModel(
        const std::string& testStartDate,
        const std::string& testEndDate);

    std::vector<PredictionResult> validatePredictions(
        const std::string& validationStartDate,
        const std::string& validationEndDate);

    // Performance tracking
    struct PredictionPerformance {
        std::string date;
        int correctDirections;
        int totalPredictions;
        double meanAbsoluteError;
        double rootMeanSquareError;
        double hitRate;
        std::vector<PredictionResult> details;
    };

    std::vector<PredictionPerformance> analyzePredictionPerformance(
        const std::string& startDate,
        const std::string& endDate);

    // Configuration and settings
    void setMinConfidenceThreshold(double threshold) { m_minConfidenceThreshold = threshold; }
    void setMaxPositionsPerSide(int maxPositions) { m_maxPositionsPerSide = maxPositions; }
    void setRiskFreeRate(double rate) { m_riskFreeRate = rate; }
    void setLiquidityFilters(bool enabled) { m_useLiquidityFilters = enabled; }

    // Status and diagnostics
    bool isModelTrained() const;
    std::string getModelInfo() const;
    std::string getLastError() const { return m_lastError; }
    ModelMetrics getLastTrainingMetrics() const;

    // Data export for analysis
    bool exportPredictionsToCSV(
        const std::vector<DailyPrediction>& predictions,
        const std::string& filename);

    bool exportPerformanceReport(
        const std::vector<PredictionPerformance>& performance,
        const std::string& filename);

private:
    // Core components
    std::unique_ptr<IModelTrainer> m_modelTrainer;
    std::shared_ptr<FeatureEngineering::FeatureCalculator> m_featureCalculator;
    std::shared_ptr<Database::Repositories::MarketDataRepository> m_marketDataRepository;
    std::shared_ptr<Database::Repositories::SentimentRepository> m_sentimentRepository;

    // Configuration
    double m_minConfidenceThreshold;
    int m_maxPositionsPerSide;
    double m_riskFreeRate;
    bool m_useLiquidityFilters;

    // State
    std::string m_lastError;
    std::vector<DailyPrediction> m_latestPredictions;
    std::chrono::system_clock::time_point m_lastUpdateTime;

    // Helper methods
    std::vector<FeatureEngineering::FeatureVector> prepareTrainingData(
        const std::string& startDate,
        const std::string& endDate);

    std::vector<FeatureEngineering::FeatureVector> prepareFeatureData(
        const std::string& targetDate,
        const std::vector<std::string>& symbols);

    DailyPrediction convertToDailyPrediction(
        const PredictionResult& result,
        const std::map<std::string, double>& liquidityLambdas);

    std::vector<DailyPrediction> rankPredictions(
        const std::vector<DailyPrediction>& predictions);

    PortfolioRecommendation buildPortfolioRecommendation(
        const std::vector<DailyPrediction>& rankedPredictions,
        int longCount,
        int shortCount);

    // Risk and portfolio utilities
    double calculateExpectedReturn(
        const std::vector<std::string>& positions,
        const std::map<std::string, DailyPrediction>& predictionMap);

    double calculatePortfolioRisk(
        const std::vector<std::string>& longPositions,
        const std::vector<std::string>& shortPositions);

    // Liquidity filtering
    std::vector<std::string> applyLiquidityFilters(
        const std::vector<std::string>& symbols);

    std::map<std::string, double> getLiquidityMetrics(
        const std::vector<std::string>& symbols);

    // Validation and error handling
    bool validateInputData(const std::vector<FeatureEngineering::FeatureVector>& data);
    bool validatePredictionInputs(const std::string& date, const std::vector<std::string>& symbols);

    void setError(const std::string& error);
    void clearError() { m_lastError.clear(); }

    // Data retrieval helpers
    std::vector<Database::Models::MarketData> getMarketDataForDate(
        const std::string& date,
        const std::vector<std::string>& symbols = {});

    std::vector<Database::Models::SentimentData> getSentimentDataForDate(
        const std::string& date,
        const std::vector<std::string>& symbols = {});

    // Caching for performance
    std::map<std::string, std::vector<FeatureEngineering::FeatureVector>> m_featureCache;
    std::map<std::string, std::vector<DailyPrediction>> m_predictionCache;

    void clearCache();
    bool isCacheValid(const std::string& key) const;
};

// Prediction monitoring and alerts
class PredictionMonitor {
public:
    struct Alert {
        std::string type;       // "HIGH_CONFIDENCE", "UNUSUAL_PREDICTION", "MODEL_DRIFT"
        std::string message;
        std::string symbol;
        std::string date;
        double severity;        // [0,1] where 1 is most severe

        Alert(const std::string& t, const std::string& m, double s = 0.5)
            : type(t), message(m), severity(s) {}
    };

    PredictionMonitor() = default;

    std::vector<Alert> checkPredictions(const std::vector<DailyPrediction>& predictions);
    std::vector<Alert> checkModelDrift(const ModelMetrics& currentMetrics, const ModelMetrics& baselineMetrics);
    std::vector<Alert> checkDataQuality(const std::vector<FeatureEngineering::FeatureVector>& features);

    void setAlertThresholds(const std::map<std::string, double>& thresholds);

private:
    std::map<std::string, double> m_alertThresholds;

    Alert createAlert(const std::string& type, const std::string& message, double severity = 0.5);
};

} // namespace MachineLearning
} // namespace CryptoClaude