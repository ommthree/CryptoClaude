#pragma once

#include <vector>
#include <string>
#include <map>
#include <memory>
#include <chrono>
#include <random>
// Forward declarations to avoid circular dependencies
namespace CryptoClaude {
namespace Analytics {
    struct MarketDataPoint;
}
}
#include "../Database/Models/SentimentData.h"
#include "../AI/ClaudeFeatureProvider.h"
#include "../Risk/VolatilityManager.h"

namespace CryptoClaude {
namespace ML {

/**
 * Feature vector for Random Forest prediction
 * Contains technical indicators, market data, and sentiment features
 */
struct MLFeatureVector {
    // Technical indicators
    double sma_5_ratio;          // Current price / SMA(5)
    double sma_20_ratio;         // Current price / SMA(20)
    double rsi_14;               // RSI(14) value
    double volatility_10;        // 10-day rolling volatility
    double volume_ratio;         // Volume / Average volume(20)

    // Market structure features
    double price_momentum_3;     // 3-day price momentum
    double price_momentum_7;     // 7-day price momentum
    double high_low_ratio;       // (High - Low) / Close
    double open_close_gap;       // (Open - Previous Close) / Previous Close

    // Cross-asset features
    double btc_correlation_30;   // 30-day correlation with BTC
    double market_beta;          // Beta vs broader crypto market

    // Sentiment features
    double news_sentiment;       // Aggregated news sentiment score
    double sentiment_momentum;   // Sentiment change over time
    double sentiment_quality;    // News sentiment quality score

    // Temporal features
    double day_of_week;          // 1-7 encoding for weekly patterns
    double hour_of_day;          // 0-23 encoding for daily patterns

    // === CLAUDE AI FEATURES (New Integration) ===
    // Sentiment and narrative features
    double claude_market_sentiment;      // -1.0 to +1.0 from Claude analysis
    double claude_news_impact;           // 0.0 to 1.0 news impact magnitude
    double claude_social_momentum;       // -1.0 to +1.0 social media momentum
    double claude_narrative_strength;    // 0.0 to 1.0 narrative strength

    // Market regime features from Claude
    double claude_regime_change_prob;    // 0.0 to 1.0 regime change probability
    double claude_volatility_forecast;   // 0.0 to 1.0 volatility forecast
    double claude_uncertainty_level;     // 0.0 to 1.0 market uncertainty

    // Risk and timing features from Claude
    double claude_tail_risk;             // 0.0 to 1.0 tail risk indicator
    double claude_momentum_sustainability; // 0.0 to 1.0 momentum sustainability
    double claude_mean_reversion;        // 0.0 to 1.0 mean reversion signal

    // Macro and institutional features from Claude
    double claude_institutional_sentiment; // -1.0 to +1.0 institutional interest
    double claude_regulatory_risk;       // 0.0 to 1.0 regulatory risk
    double claude_analysis_confidence;   // 0.0 to 1.0 Claude's confidence

    // === ENHANCED VOLATILITY FEATURES (TRS Requirement) ===
    // Advanced volatility measures for better volatility handling
    double normalized_volatility;        // 0.0 to 1.0 normalized volatility
    double volatility_regime_score;      // 0.0 to 1.0 based on regime (low/normal/high/extreme)
    double volatility_trend_indicator;   // -1.0 to +1.0 (decreasing to increasing volatility)
    double relative_volatility_score;    // Volatility relative to market average
    double volatility_stability;         // How stable the volatility is over time

    std::chrono::system_clock::time_point timestamp;
    std::string symbol;

    MLFeatureVector() : sma_5_ratio(1.0), sma_20_ratio(1.0), rsi_14(50.0),
                       volatility_10(0.0), volume_ratio(1.0), price_momentum_3(0.0),
                       price_momentum_7(0.0), high_low_ratio(0.0), open_close_gap(0.0),
                       btc_correlation_30(0.0), market_beta(1.0), news_sentiment(0.0),
                       sentiment_momentum(0.0), sentiment_quality(0.0), day_of_week(1.0),
                       hour_of_day(0.0),
                       // Initialize Claude features to neutral values
                       claude_market_sentiment(0.0), claude_news_impact(0.0),
                       claude_social_momentum(0.0), claude_narrative_strength(0.0),
                       claude_regime_change_prob(0.0), claude_volatility_forecast(0.0),
                       claude_uncertainty_level(0.5), claude_tail_risk(0.0),
                       claude_momentum_sustainability(0.5), claude_mean_reversion(0.0),
                       claude_institutional_sentiment(0.0), claude_regulatory_risk(0.0),
                       claude_analysis_confidence(0.0),
                       // Initialize enhanced volatility features
                       normalized_volatility(0.0), volatility_regime_score(0.5),
                       volatility_trend_indicator(0.0), relative_volatility_score(1.0),
                       volatility_stability(0.5) {
        timestamp = std::chrono::system_clock::now();
    }
};

/**
 * Random Forest prediction result with confidence metrics
 */
struct MLPrediction {
    std::string symbol;
    double predicted_return;        // Predicted return (e.g., next 24h)
    double confidence_score;        // Model confidence [0.0, 1.0]
    double prediction_variance;     // Variance across trees

    std::chrono::system_clock::time_point prediction_time;
    std::chrono::hours prediction_horizon;  // Prediction time horizon

    // Feature importance for interpretability
    std::map<std::string, double> feature_importance;

    MLPrediction() : predicted_return(0.0), confidence_score(0.0),
                    prediction_variance(0.0), prediction_horizon(std::chrono::hours(24)) {
        prediction_time = std::chrono::system_clock::now();
    }
};

/**
 * Decision Tree node for Random Forest implementation
 */
struct DecisionTreeNode {
    bool is_leaf;
    std::string split_feature;      // Feature name for split
    double split_value;             // Threshold value for split
    double prediction_value;        // Prediction if leaf node

    std::unique_ptr<DecisionTreeNode> left_child;
    std::unique_ptr<DecisionTreeNode> right_child;

    // Node statistics
    int sample_count;
    double mse;                     // Mean squared error at node

    DecisionTreeNode() : is_leaf(false), split_value(0.0), prediction_value(0.0),
                        sample_count(0), mse(0.0) {}
};

/**
 * Individual Decision Tree implementation
 */
class DecisionTree {
public:
    DecisionTree(int max_depth = 10, int min_samples_split = 20, double min_impurity_decrease = 1e-7);

    // Training
    void train(const std::vector<MLFeatureVector>& features,
              const std::vector<double>& targets);

    // Prediction
    double predict(const MLFeatureVector& features) const;

    // Feature importance
    std::map<std::string, double> getFeatureImportance() const;

private:
    std::unique_ptr<DecisionTreeNode> root_;
    int max_depth_;
    int min_samples_split_;
    double min_impurity_decrease_;
    mutable std::mt19937 rng_;

    // Tree building
    std::unique_ptr<DecisionTreeNode> buildTree(
        const std::vector<MLFeatureVector>& features,
        const std::vector<double>& targets,
        const std::vector<int>& sample_indices,
        int depth
    );

    // Split finding
    struct SplitResult {
        std::string feature;
        double threshold;
        double impurity_decrease;
        std::vector<int> left_indices;
        std::vector<int> right_indices;
    };

    SplitResult findBestSplit(
        const std::vector<MLFeatureVector>& features,
        const std::vector<double>& targets,
        const std::vector<int>& sample_indices
    );

    double calculateMSE(const std::vector<double>& targets,
                       const std::vector<int>& indices);
    double calculateMean(const std::vector<double>& targets,
                        const std::vector<int>& indices);

    // Prediction helper
    double predictNode(const MLFeatureVector& features,
                      const DecisionTreeNode* node) const;
};

/**
 * Random Forest ML Predictor for cryptocurrency trading
 * Integrates with PortfolioOptimizer framework for ML-enhanced strategies
 */
class RandomForestPredictor {
public:
    RandomForestPredictor(int n_trees = 100, int max_depth = 10,
                         double feature_subsample_ratio = 0.7,
                         double bootstrap_sample_ratio = 0.8);

    virtual ~RandomForestPredictor() = default;

    // === TRAINING AND MODEL MANAGEMENT ===

    // Train model on historical data
    bool trainModel(const std::vector<MLFeatureVector>& features,
                   const std::vector<double>& targets,
                   const std::string& model_version = "v1.0");

    // Cross-validation training
    struct CrossValidationResult {
        double mean_accuracy;
        double std_accuracy;
        double mean_mse;
        double std_mse;
        std::vector<double> fold_accuracies;
        std::vector<double> fold_mses;
    };

    CrossValidationResult crossValidateModel(
        const std::vector<MLFeatureVector>& features,
        const std::vector<double>& targets,
        int n_folds = 5
    );

    // Model persistence
    bool saveModel(const std::string& filepath);
    bool loadModel(const std::string& filepath);

    // === PREDICTION INTERFACE ===

    // Single prediction
    MLPrediction predict(const MLFeatureVector& features);

    // Batch predictions for multiple symbols
    std::map<std::string, MLPrediction> predictBatch(
        const std::map<std::string, MLFeatureVector>& features_map
    );

    // Prediction with uncertainty quantification
    struct PredictionWithUncertainty {
        double prediction;
        double lower_bound;    // e.g., 5th percentile
        double upper_bound;    // e.g., 95th percentile
        double variance;
    };

    PredictionWithUncertainty predictWithUncertainty(
        const MLFeatureVector& features,
        double confidence_level = 0.9
    );

    // === FEATURE ENGINEERING ===

    // Create feature vector from market data and sentiment
    MLFeatureVector createFeatures(
        const std::string& symbol,
        const std::vector<Analytics::MarketDataPoint>& market_data,
        const std::vector<Database::Models::SentimentData>& sentiment_data,
        const std::map<std::string, std::vector<Analytics::MarketDataPoint>>& market_context = {}
    );

    // Feature importance analysis
    std::map<std::string, double> getGlobalFeatureImportance() const;

    // Feature engineering pipeline
    std::vector<MLFeatureVector> createFeaturePipeline(
        const std::map<std::string, std::vector<Analytics::MarketDataPoint>>& historical_data,
        const std::map<std::string, std::vector<Database::Models::SentimentData>>& sentiment_data,
        const std::chrono::hours& prediction_horizon = std::chrono::hours(24)
    );

    // === CLAUDE AI FEATURE INTEGRATION ===

    // Enhanced feature creation with Claude AI features
    MLFeatureVector createFeaturesWithClaude(
        const std::string& symbol,
        const std::vector<Analytics::MarketDataPoint>& market_data,
        const std::vector<Database::Models::SentimentData>& sentiment_data,
        const AI::ClaudeFeatureProvider::ClaudeFeatureSet& claude_features,
        const std::map<std::string, std::vector<Analytics::MarketDataPoint>>& market_context = {}
    );

    // Batch feature creation with Claude integration
    std::map<std::string, MLFeatureVector> createBatchFeaturesWithClaude(
        const std::map<std::string, std::vector<Analytics::MarketDataPoint>>& market_data,
        const std::map<std::string, std::vector<Database::Models::SentimentData>>& sentiment_data,
        const std::map<std::string, AI::ClaudeFeatureProvider::ClaudeFeatureSet>& claude_features
    );

    // Set Claude feature provider for automatic integration
    void setClaudeFeatureProvider(std::shared_ptr<AI::ClaudeFeatureProvider> provider);

    // Enable/disable Claude features in predictions
    void setClaudeFeaturesEnabled(bool enabled);
    bool isClaudeFeaturesEnabled() const;

    // Get feature importance including Claude features
    std::map<std::string, double> getClaudeFeatureImportance() const;

    // === VOLATILITY-ENHANCED PREDICTIONS (TRS Requirement) ===

    // Set volatility manager for volatility-aware predictions
    void setVolatilityManager(std::shared_ptr<Risk::VolatilityManager> manager);

    // Enhanced prediction with volatility downweighting
    struct VolatilityAdjustedPrediction {
        MLPrediction base_prediction;
        Risk::VolatilityManager::VolatilityAdjustedScore volatility_adjustment;
        double final_score;                // Base score adjusted for volatility
        double final_confidence;           // Confidence adjusted for volatility
        double position_size_limit;        // Recommended max position size
        bool is_downweighted;             // True if significantly downweighted due to volatility
    };

    // Generate volatility-adjusted predictions (TRS requirement)
    VolatilityAdjustedPrediction predictWithVolatilityAdjustment(const MLFeatureVector& features);

    // Batch predictions with volatility adjustments
    std::map<std::string, VolatilityAdjustedPrediction> predictBatchWithVolatilityAdjustment(
        const std::map<std::string, MLFeatureVector>& features_map
    );

    // Enable/disable volatility-based downweighting
    void setVolatilityDownweightingEnabled(bool enabled);
    bool isVolatilityDownweightingEnabled() const;

    // === PERFORMANCE ANALYSIS ===

    // Backtest performance metrics
    struct BacktestResults {
        double total_return;
        double sharpe_ratio;
        double max_drawdown;
        double accuracy;           // Directional accuracy
        double precision;          // True positives / (True positives + False positives)
        double recall;             // True positives / (True positives + False negatives)
        std::vector<double> daily_returns;
        std::vector<double> predictions;
        std::vector<double> actual_returns;
    };

    BacktestResults backtestStrategy(
        const std::vector<MLFeatureVector>& test_features,
        const std::vector<double>& actual_returns,
        const std::chrono::system_clock::time_point& start_date,
        const std::chrono::system_clock::time_point& end_date
    );

    // Model diagnostics
    struct ModelDiagnostics {
        double training_accuracy;
        double validation_accuracy;
        double overfitting_score;     // Training accuracy - Validation accuracy
        std::map<std::string, double> feature_correlations;
        std::vector<double> residual_distribution;
    };

    ModelDiagnostics generateModelDiagnostics(
        const std::vector<MLFeatureVector>& validation_features,
        const std::vector<double>& validation_targets
    );

    // === INTEGRATION WITH PORTFOLIO OPTIMIZER ===

    // Get predictions formatted for portfolio optimization
    std::map<std::string, double> getPredictionsForOptimization(
        const std::vector<std::string>& symbols,
        const std::map<std::string, MLFeatureVector>& current_features
    );

    // ML-enhanced expected returns for optimization
    std::map<std::string, double> getMLEnhancedExpectedReturns(
        const std::map<std::string, MLFeatureVector>& features,
        const std::map<std::string, double>& baseline_expected_returns,
        double ml_confidence_threshold = 0.6
    );

    // === CONFIGURATION AND UTILITIES ===

    // Model parameters
    struct ModelParameters {
        int n_trees;
        int max_depth;
        double feature_subsample_ratio;
        double bootstrap_sample_ratio;
        int min_samples_split;
        double min_impurity_decrease;
    };

    void setModelParameters(const ModelParameters& params);
    ModelParameters getModelParameters() const;

    // Model status and metadata
    struct ModelStatus {
        bool is_trained;
        std::string model_version;
        std::chrono::system_clock::time_point training_date;
        int training_samples;
        int feature_count;
        double training_accuracy;
        double validation_accuracy;
    };

    ModelStatus getModelStatus() const;

private:
    // Model configuration
    ModelParameters params_;
    ModelStatus status_;

    // Random Forest trees
    std::vector<std::unique_ptr<DecisionTree>> trees_;

    // Feature engineering cache
    mutable std::map<std::string, MLFeatureVector> feature_cache_;
    mutable std::chrono::system_clock::time_point cache_timestamp_;

    // Random number generation
    mutable std::mt19937 rng_;

    // === CLAUDE AI INTEGRATION ===

    // Claude feature provider
    std::shared_ptr<AI::ClaudeFeatureProvider> claude_provider_;
    bool claude_features_enabled_;

    // Claude feature integration helpers
    void integrateClaudeFeatures(MLFeatureVector& feature_vector,
                                const AI::ClaudeFeatureProvider::ClaudeFeatureSet& claude_features) const;

    // Claude feature validation
    bool validateClaudeFeatures(const AI::ClaudeFeatureProvider::ClaudeFeatureSet& claude_features) const;

    // === VOLATILITY MANAGEMENT ===

    // Volatility manager for downweighting
    std::shared_ptr<Risk::VolatilityManager> volatility_manager_;
    bool volatility_downweighting_enabled_;

    // Volatility integration helpers
    void integrateVolatilityFeatures(MLFeatureVector& feature_vector,
                                    const Risk::VolatilityManager::VolatilityMetrics& vol_metrics) const;

    // Feature processing helpers
    double calculateTechnicalIndicator(
        const std::vector<Analytics::MarketDataPoint>& data,
        const std::string& indicator_type,
        int period
    ) const;

    double calculateSentimentFeature(
        const std::vector<Database::Models::SentimentData>& sentiment_data,
        const std::string& feature_type
    ) const;

    double calculateCrossAssetFeature(
        const std::string& symbol,
        const std::map<std::string, std::vector<Analytics::MarketDataPoint>>& market_data,
        const std::string& feature_type
    ) const;

    // Training helpers
    std::vector<int> createBootstrapSample(int sample_size) const;
    std::vector<std::string> selectRandomFeatures(const MLFeatureVector& feature_template) const;

    // Validation helpers
    double calculateAccuracy(const std::vector<double>& predictions,
                           const std::vector<double>& targets,
                           double threshold = 0.0) const;

    double calculateMSE(const std::vector<double>& predictions,
                       const std::vector<double>& targets) const;

    // Cache management
    void updateFeatureCache(const std::string& key, const MLFeatureVector& features) const;
    bool isFeatureCacheValid(const std::chrono::minutes& max_age = std::chrono::minutes(15)) const;
    void clearFeatureCache() const;
};

}} // namespace CryptoClaude::ML