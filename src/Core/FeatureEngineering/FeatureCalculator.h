#pragma once
#include "../Database/Models/MarketData.h"
#include <vector>
#include <map>
#include <string>
#include <functional>

namespace CryptoClaude {
namespace FeatureEngineering {

struct FeatureVector {
    std::string symbol;
    std::string date;

    // Sentiment features
    double sentiment1d = 0.0;
    double sentiment7dAvg = 0.0;

    // Momentum features
    double inflowGradient = 0.0;
    double priceGradient = 0.0;

    // Mean reversion features
    double inflow100dAvg = 0.0;
    double priceToMovingAvg = 0.0;

    // Volume features
    double volumeRatio = 0.0;
    double volumeMovingAvg = 0.0;

    // Technical indicators
    double rsi = 0.0;
    double bollinger = 0.0;

    // Target variable
    double targetF = 0.0;  // The fractional inflow we want to predict

    FeatureVector() = default;
    FeatureVector(const std::string& sym, const std::string& dt)
        : symbol(sym), date(dt) {}

    std::vector<double> toVector() const;
    void fromVector(const std::vector<double>& vec);
    std::vector<std::string> getFeatureNames() const;
    int getFeatureCount() const;
};

class FeatureCalculator {
public:
    FeatureCalculator() = default;
    ~FeatureCalculator() = default;

    // Main feature calculation methods
    std::vector<FeatureVector> calculateFeatures(
        const std::vector<Database::Models::MarketData>& marketData);

    std::vector<FeatureVector> calculateFeaturesForSymbol(
        const std::string& symbol,
        const std::vector<Database::Models::MarketData>& symbolData);

    // Individual feature calculations
    double calculateSentiment1d(
        const std::vector<Database::Models::MarketData>& data,
        size_t currentIndex);

    double calculateSentiment7dAvg(
        const std::vector<Database::Models::MarketData>& data,
        size_t currentIndex);

    double calculateInflowGradient(
        const std::vector<Database::Models::MarketData>& data,
        size_t currentIndex);

    double calculatePriceGradient(
        const std::vector<Database::Models::MarketData>& data,
        size_t currentIndex);

    double calculateInflow100dAvg(
        const std::vector<Database::Models::MarketData>& data,
        size_t currentIndex);

    double calculatePriceToMovingAvg(
        const std::vector<Database::Models::MarketData>& data,
        size_t currentIndex,
        int windowSize = 20);

    // Technical indicators
    double calculateRSI(
        const std::vector<Database::Models::MarketData>& data,
        size_t currentIndex,
        int period = 14);

    double calculateBollingerPosition(
        const std::vector<Database::Models::MarketData>& data,
        size_t currentIndex,
        int period = 20,
        double stdMultiplier = 2.0);

    // Volume-based features
    double calculateVolumeRatio(
        const std::vector<Database::Models::MarketData>& data,
        size_t currentIndex);

    double calculateVolumeMovingAvg(
        const std::vector<Database::Models::MarketData>& data,
        size_t currentIndex,
        int windowSize = 7);

    // Target variable calculation
    double calculateTargetF(
        const std::vector<Database::Models::MarketData>& data,
        size_t currentIndex);

    // Statistical utilities
    double calculateMovingAverage(
        const std::vector<double>& values,
        size_t endIndex,
        int windowSize);

    double calculateStandardDeviation(
        const std::vector<double>& values,
        size_t endIndex,
        int windowSize);

    double calculateCorrelation(
        const std::vector<double>& x,
        const std::vector<double>& y);

    // Data preprocessing
    std::vector<FeatureVector> filterOutliers(
        const std::vector<FeatureVector>& features,
        double targetMin = -100.0,
        double targetMax = 100.0);

    std::vector<FeatureVector> normalizeFeatures(
        const std::vector<FeatureVector>& features);

    std::vector<FeatureVector> removeInvalidFeatures(
        const std::vector<FeatureVector>& features);

    // Configuration
    void setLagWindows(int sentiment1dLag, int sentiment7dWindow, int inflow100dWindow);
    void setOutlierThresholds(double minTarget, double maxTarget);
    void enableFeature(const std::string& featureName, bool enabled);

    // Validation
    bool validateFeatures(const std::vector<FeatureVector>& features) const;
    std::vector<std::string> getValidationErrors(const std::vector<FeatureVector>& features) const;

    // Export/Import
    bool exportFeaturesToCSV(const std::vector<FeatureVector>& features, const std::string& filename);
    std::vector<FeatureVector> importFeaturesFromCSV(const std::string& filename);

private:
    // Configuration parameters
    int m_sentiment1dLag = 1;
    int m_sentiment7dWindow = 7;
    int m_inflow100dWindow = 100;
    double m_minTargetValue = -100.0;
    double m_maxTargetValue = 100.0;

    std::map<std::string, bool> m_enabledFeatures;

    // Helper methods
    bool isValidIndex(size_t index, size_t dataSize, int lookback = 0) const;
    bool isValidValue(double value) const;

    std::vector<double> extractValues(
        const std::vector<Database::Models::MarketData>& data,
        size_t endIndex,
        int count,
        std::function<double(const Database::Models::MarketData&)> extractor);

    // Statistical helpers
    double calculatePercentileRank(const std::vector<double>& values, double targetValue);
    double calculateZScore(const std::vector<double>& values, double targetValue);

    // Time series utilities
    std::vector<double> calculateDifferences(const std::vector<double>& values);
    std::vector<double> calculateReturns(const std::vector<double>& prices);

    // Feature validation helpers
    bool isFeatureNameValid(const std::string& featureName) const;
    std::vector<std::string> getSupportedFeatureNames() const;
};

// Feature importance analysis
class FeatureImportanceAnalyzer {
public:
    struct FeatureImportance {
        std::string featureName;
        double importance;
        double pValue;
        double correlation;

        FeatureImportance(const std::string& name, double imp, double p = 0.0, double corr = 0.0)
            : featureName(name), importance(imp), pValue(p), correlation(corr) {}
    };

    FeatureImportanceAnalyzer() = default;

    std::vector<FeatureImportance> analyzeFeatureImportance(
        const std::vector<FeatureVector>& features);

    std::vector<FeatureImportance> calculateCorrelations(
        const std::vector<FeatureVector>& features);

    std::vector<std::string> selectTopFeatures(
        const std::vector<FeatureVector>& features,
        int topN = 10);

    // Feature selection methods
    std::vector<FeatureVector> selectFeaturesByImportance(
        const std::vector<FeatureVector>& features,
        const std::vector<std::string>& selectedFeatures);

private:
    double calculateMutualInformation(
        const std::vector<double>& feature,
        const std::vector<double>& target);

    double calculateLinearRegression(
        const std::vector<double>& feature,
        const std::vector<double>& target);
};

} // namespace FeatureEngineering
} // namespace CryptoClaude