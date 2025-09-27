#pragma once

#include "RandomForestPredictor.h"
#include "../Analytics/CrossAssetCorrelationMonitor.h"
#include "../Analytics/StatisticalTools.h"
#include <vector>
#include <map>
#include <string>
#include <chrono>
#include <memory>

namespace CryptoClaude {
namespace ML {

// Enhanced ML feature vector for correlation analysis
struct CorrelationMLFeatures : public MLFeatureVector {
    // Cross-asset correlation features
    double btc_spx_correlation;        // BTC vs S&P 500 correlation
    double btc_gold_correlation;       // BTC vs Gold correlation
    double btc_dxy_correlation;        // BTC vs US Dollar correlation
    double eth_spx_correlation;        // ETH vs S&P 500 correlation

    // Correlation dynamics
    double correlation_momentum_3d;     // 3-day correlation momentum
    double correlation_momentum_7d;     // 7-day correlation momentum
    double correlation_volatility;      // Correlation volatility (rolling std)
    double correlation_z_score;         // Z-score of current correlation vs history

    // Market regime indicators
    double market_stress_level;         // Overall market stress (0-1)
    double vix_level;                  // VIX volatility indicator
    double flight_to_quality_score;    // Flight to quality sentiment score
    double risk_appetite_index;        // Risk appetite composite score

    // Cross-asset momentum features
    double traditional_asset_momentum; // Average momentum of traditional assets
    double crypto_traditional_divergence; // Divergence between crypto and traditional momentum

    CorrelationMLFeatures() : MLFeatureVector() {
        // Initialize correlation-specific features
        btc_spx_correlation = 0.0;
        btc_gold_correlation = 0.0;
        btc_dxy_correlation = 0.0;
        eth_spx_correlation = 0.0;
        correlation_momentum_3d = 0.0;
        correlation_momentum_7d = 0.0;
        correlation_volatility = 0.0;
        correlation_z_score = 0.0;
        market_stress_level = 0.0;
        vix_level = 0.0;
        flight_to_quality_score = 0.0;
        risk_appetite_index = 0.0;
        traditional_asset_momentum = 0.0;
        crypto_traditional_divergence = 0.0;
    }
};

// Correlation regime classification
enum class CorrelationRegime {
    NORMAL,           // Typical correlation patterns
    RISK_OFF,         // High correlation during market stress
    DECOUPLING,       // Crypto decoupling from traditional assets
    FLIGHT_TO_QUALITY, // Traditional assets outperforming
    RISK_ON,          // Risk assets moving together
    MIXED_SIGNALS     // Inconsistent correlation patterns
};

struct CorrelationRegimeResult {
    CorrelationRegime current_regime;
    CorrelationRegime predicted_regime;
    double regime_confidence;
    double regime_stability_score;
    std::chrono::system_clock::time_point prediction_time;
    std::vector<std::string> regime_indicators;
};

// ML-enhanced correlation prediction
struct CorrelationPrediction {
    std::string asset_pair;
    double predicted_correlation;
    double current_correlation;
    double correlation_change_forecast;
    double prediction_confidence;
    double prediction_horizon_hours;

    // Regime change prediction
    bool regime_change_likely;
    double regime_change_probability;
    CorrelationRegime predicted_regime;

    std::chrono::system_clock::time_point prediction_time;
    std::string model_version;
};

// Correlation regime detector using ML
class CorrelationRegimeDetector {
public:
    CorrelationRegimeDetector();
    ~CorrelationRegimeDetector();

    // Regime detection and prediction
    CorrelationRegimeResult detectCurrentRegime(
        const std::vector<Analytics::CorrelationSnapshot>& correlations);
    CorrelationRegimeResult predictRegimeChange(
        const std::vector<Analytics::CorrelationSnapshot>& correlation_history);

    // Regime analysis
    CorrelationRegime classifyRegime(const CorrelationMLFeatures& features);
    double calculateRegimeStability(
        const std::vector<Analytics::CorrelationSnapshot>& correlation_history);

    // Model training for regime detection
    bool trainRegimeModel(
        const std::vector<CorrelationMLFeatures>& training_features,
        const std::vector<CorrelationRegime>& training_labels);

    // Configuration
    void setRegimeThresholds(double risk_off_threshold, double decoupling_threshold);

private:
    std::unique_ptr<RandomForestPredictor> regime_model_;

    // Regime classification thresholds
    double risk_off_threshold_;      // BTC-SPX correlation for risk-off detection
    double decoupling_threshold_;    // Correlation threshold for decoupling
    double flight_to_quality_threshold_; // VIX threshold for flight-to-quality

    // Model parameters
    bool is_trained_;
    std::string model_version_;

    // Helper methods
    CorrelationRegime determineRegimeFromCorrelations(
        const std::vector<Analytics::CorrelationSnapshot>& correlations);
    std::vector<std::string> generateRegimeIndicators(
        CorrelationRegime regime, const CorrelationMLFeatures& features);
};

// Main ML-enhanced correlation analysis engine
class CorrelationMLEnhancer {
public:
    CorrelationMLEnhancer();
    ~CorrelationMLEnhancer();

    // Initialization
    bool initialize(const std::shared_ptr<Analytics::CrossAssetCorrelationMonitor>& correlation_monitor);

    // ML-enhanced correlation analysis
    CorrelationPrediction predictCorrelationChange(
        const std::string& crypto_symbol,
        const std::string& traditional_symbol,
        int horizon_hours = 24);

    std::vector<CorrelationPrediction> predictAllCorrelations(int horizon_hours = 24);

    // Regime analysis
    CorrelationRegimeResult analyzeCurrentRegime();
    CorrelationRegimeResult forecastRegimeChange(int horizon_hours = 72);

    // Feature engineering for correlation ML
    CorrelationMLFeatures createCorrelationFeatures(
        const std::string& crypto_symbol,
        const std::vector<Analytics::CorrelationSnapshot>& correlation_history);

    CorrelationMLFeatures createMarketRegimeFeatures(
        const std::vector<Analytics::CorrelationSnapshot>& all_correlations);

    // Model training and management
    bool trainCorrelationModel(
        const std::vector<CorrelationMLFeatures>& training_features,
        const std::vector<double>& correlation_targets);

    bool trainRegimeModel(
        const std::vector<CorrelationMLFeatures>& training_features,
        const std::vector<CorrelationRegime>& regime_labels);

    // Integration with existing Random Forest
    MLPrediction getMLEnhancedPrediction(
        const std::string& symbol,
        const CorrelationMLFeatures& enhanced_features);

    // Performance analytics
    struct CorrelationModelMetrics {
        double correlation_prediction_accuracy;
        double regime_detection_accuracy;
        double average_prediction_error;
        double model_confidence_score;
        int predictions_made_24h;
        int regime_changes_detected_24h;
    };

    CorrelationModelMetrics getModelMetrics() const;

    // Risk management integration
    double getCorrelationRiskScore() const;
    std::vector<std::string> getCorrelationBasedRecommendations() const;

private:
    // Core components
    std::shared_ptr<Analytics::CrossAssetCorrelationMonitor> correlation_monitor_;
    std::unique_ptr<RandomForestPredictor> correlation_predictor_;
    std::unique_ptr<CorrelationRegimeDetector> regime_detector_;

    // Model state
    bool is_initialized_;
    bool correlation_model_trained_;
    bool regime_model_trained_;
    std::string model_version_;

    // Feature engineering components
    std::map<std::string, std::vector<double>> correlation_history_cache_;
    std::vector<Analytics::CorrelationSnapshot> recent_correlations_;

    // Performance tracking
    mutable CorrelationModelMetrics model_metrics_;

    // Configuration parameters
    int default_lookback_days_;
    double correlation_change_threshold_;
    double regime_confidence_threshold_;

    // Feature calculation methods
    double calculateCorrelationMomentum(
        const std::vector<Analytics::CorrelationSnapshot>& history, int days);
    double calculateCorrelationVolatility(
        const std::vector<Analytics::CorrelationSnapshot>& history, int window);
    double calculateMarketStressLevel(
        const std::vector<Analytics::CorrelationSnapshot>& all_correlations);
    double calculateFlightToQualityScore(
        const std::vector<Analytics::CorrelationSnapshot>& correlations);
    double calculateRiskAppetiteIndex(
        const std::vector<Analytics::CorrelationSnapshot>& correlations);

    // Prediction helpers
    CorrelationPrediction createPredictionResult(
        const std::string& asset_pair,
        const MLPrediction& ml_result,
        const Analytics::CorrelationSnapshot& current_correlation);

    // Model validation
    bool validateCorrelationModel() const;
    bool validateRegimeModel() const;

    // Cache management
    void updateCorrelationCache();
    void cleanupStaleCache();
};

// Correlation-based portfolio signal generator
class CorrelationSignalGenerator {
public:
    CorrelationSignalGenerator(const std::shared_ptr<CorrelationMLEnhancer>& ml_enhancer);
    ~CorrelationSignalGenerator();

    // Signal generation
    struct CorrelationSignal {
        std::string signal_type;      // "correlation_change", "regime_shift", "stress_alert"
        std::string affected_assets;  // Assets affected by signal
        double signal_strength;       // 0.0 to 1.0
        std::string direction;        // "bullish", "bearish", "neutral"
        std::vector<std::string> recommendations;
        std::chrono::system_clock::time_point signal_time;
        int signal_horizon_hours;
    };

    std::vector<CorrelationSignal> generateCorrelationSignals();
    std::vector<CorrelationSignal> generateRegimeChangeSignals();
    std::vector<CorrelationSignal> generateRiskAdjustmentSignals();

    // Portfolio optimization signals
    std::map<std::string, double> getCorrelationBasedWeights(
        const std::vector<std::string>& portfolio_assets);
    double getRecommendedLeverage() const;
    std::vector<std::string> getHedgingRecommendations() const;

private:
    std::shared_ptr<CorrelationMLEnhancer> ml_enhancer_;

    // Signal thresholds
    double correlation_change_threshold_;
    double regime_change_threshold_;
    double stress_threshold_;

    CorrelationSignal createSignal(const std::string& type,
                                 const std::string& assets,
                                 double strength,
                                 const std::string& direction);
};

// Utility functions for correlation ML
namespace CorrelationMLUtils {
    // Feature engineering utilities
    std::vector<double> calculateRollingCorrelation(
        const std::vector<double>& x,
        const std::vector<double>& y,
        int window);

    double calculateCorrelationDivergence(
        const std::vector<Analytics::CorrelationSnapshot>& correlations);

    std::vector<double> extractCorrelationTimeSeries(
        const std::vector<Analytics::CorrelationSnapshot>& snapshots);

    // Regime analysis utilities
    std::string regimeToString(CorrelationRegime regime);
    CorrelationRegime stringToRegime(const std::string& regime_str);

    // Model evaluation utilities
    double calculateCorrelationPredictionAccuracy(
        const std::vector<double>& predicted,
        const std::vector<double>& actual);

    double calculateRegimeClassificationAccuracy(
        const std::vector<CorrelationRegime>& predicted,
        const std::vector<CorrelationRegime>& actual);

    // Risk management utilities
    double calculateCorrelationVaR(
        const std::vector<Analytics::CorrelationSnapshot>& correlations,
        double confidence_level = 0.95);

    std::map<std::string, double> calculateCorrelationRiskContributions(
        const std::vector<Analytics::CorrelationSnapshot>& correlations,
        const std::vector<std::string>& portfolio_assets);
}

} // namespace ML
} // namespace CryptoClaude