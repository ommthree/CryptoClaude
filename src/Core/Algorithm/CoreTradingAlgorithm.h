#pragma once

#include <vector>
#include <string>
#include <map>
#include <memory>
#include <chrono>
#include <functional>
#include "../Database/Models/MarketData.h"
#include "../Analytics/TechnicalIndicators.h"
#include "../Sentiment/SentimentQualityManager.h"
#include "../Risk/ProductionRiskManager.h"

namespace CryptoClaude {
namespace Algorithm {

/**
 * Core Trading Algorithm - The main prediction and signal generation engine
 * Implements the sorting algorithm that ranks cryptocurrency pairs by predicted performance
 * This replaces the simulated correlation with real algorithm-based predictions
 */
class CoreTradingAlgorithm {
public:
    // Algorithm prediction result structure
    struct PredictionResult {
        std::string pair_name;                    // e.g., "BTC/ETH"
        std::string base_symbol;                  // e.g., "BTC"
        std::string quote_symbol;                 // e.g., "ETH"

        // Core prediction metrics
        double predicted_return;                  // Expected return over prediction horizon
        double confidence_score;                  // Prediction confidence (0.0 to 1.0)
        std::chrono::system_clock::time_point prediction_time;
        std::chrono::hours prediction_horizon;    // Time horizon for prediction

        // Feature contributions for explainability
        std::map<std::string, double> feature_contributions;

        // Risk metrics
        double predicted_volatility;              // Expected volatility
        double risk_adjusted_return;              // Return/risk ratio

        // Market regime context
        std::string market_regime;                // "bull", "bear", "sideways"
        double regime_confidence;                 // Confidence in regime identification

        // Validation tracking
        bool is_out_of_sample;                    // True if prediction is out-of-sample
        std::string prediction_id;                // Unique ID for tracking actual outcomes

        PredictionResult() : predicted_return(0.0), confidence_score(0.0),
                           predicted_volatility(0.0), risk_adjusted_return(0.0),
                           regime_confidence(0.0), is_out_of_sample(false) {}
    };

    // Ranked pair structure for sorting output
    struct RankedPair {
        PredictionResult prediction;
        int rank;                                 // 1 = best predicted performance
        double relative_score;                    // Score relative to other pairs
        bool recommended_for_trading;             // Trade recommendation

        RankedPair() : rank(0), relative_score(0.0), recommended_for_trading(false) {}
    };

    // Algorithm configuration parameters
    struct AlgorithmConfig {
        // Prediction parameters
        std::chrono::hours default_prediction_horizon{24};  // Default 24-hour predictions
        double minimum_confidence_threshold{0.6};           // Min confidence for recommendations
        int max_pairs_to_rank{50};                         // Maximum pairs to rank

        // Feature weights
        double technical_indicator_weight{0.4};             // Weight for technical analysis
        double sentiment_weight{0.3};                       // Weight for sentiment analysis
        double market_regime_weight{0.2};                   // Weight for regime detection
        double cross_correlation_weight{0.1};               // Weight for pair correlation

        // Risk parameters
        double volatility_penalty_factor{0.1};              // Penalty for high volatility
        double correlation_penalty_factor{0.05};            // Penalty for high pair correlation

        // Market regime parameters
        double regime_lookback_days{30};                     // Days to look back for regime
        double regime_confidence_threshold{0.7};             // Min confidence for regime use

        AlgorithmConfig() {}
    };

    // Market regime enumeration
    enum class MarketRegime {
        BULL,           // Strong upward trend
        BEAR,           // Strong downward trend
        SIDEWAYS,       // Range-bound/consolidation
        VOLATILE,       // High volatility, unclear direction
        UNKNOWN         // Insufficient data for classification
    };

    // Feature vector for ML-style processing
    struct FeatureVector {
        // Technical indicators
        double rsi_base, rsi_quote;                          // RSI for base and quote assets
        double macd_base, macd_quote;                        // MACD signals
        double bb_position_base, bb_position_quote;          // Bollinger Band positions
        double volume_ratio_base, volume_ratio_quote;        // Volume ratios

        // Sentiment features
        double news_sentiment_base, news_sentiment_quote;    // News sentiment scores
        double social_sentiment_base, social_sentiment_quote; // Social media sentiment
        double sentiment_momentum_base, sentiment_momentum_quote; // Sentiment momentum

        // Market structure features
        double correlation_to_btc_base, correlation_to_btc_quote; // BTC correlation
        double market_cap_ratio;                             // Base/Quote market cap ratio
        double liquidity_ratio;                              // Base/Quote liquidity ratio

        // Regime features
        MarketRegime overall_market_regime;
        double regime_strength;                              // How strong the regime is
        double trend_momentum;                               // Overall trend momentum

        // Cross-pair features
        double pair_correlation;                             // Historical pair correlation
        double relative_strength;                            // Base vs Quote strength
        double volatility_ratio;                             // Base vs Quote volatility

        FeatureVector() : rsi_base(50), rsi_quote(50), macd_base(0), macd_quote(0),
                         bb_position_base(0.5), bb_position_quote(0.5),
                         volume_ratio_base(1.0), volume_ratio_quote(1.0),
                         news_sentiment_base(0), news_sentiment_quote(0),
                         social_sentiment_base(0), social_sentiment_quote(0),
                         sentiment_momentum_base(0), sentiment_momentum_quote(0),
                         correlation_to_btc_base(0), correlation_to_btc_quote(0),
                         market_cap_ratio(1.0), liquidity_ratio(1.0),
                         overall_market_regime(MarketRegime::UNKNOWN),
                         regime_strength(0), trend_momentum(0),
                         pair_correlation(0), relative_strength(0), volatility_ratio(1.0) {}
    };

private:
    AlgorithmConfig config_;
    std::unique_ptr<Analytics::TechnicalIndicators> technical_analyzer_;
    std::unique_ptr<Sentiment::SentimentQualityManager> sentiment_analyzer_;

    // Market regime detector
    std::map<std::string, MarketRegime> current_regimes_;
    std::map<std::string, double> regime_confidence_;

    // Historical performance tracking for model improvement
    std::vector<std::pair<PredictionResult, double>> prediction_outcomes_;

    // Feature processing methods
    FeatureVector extractFeatures(const std::string& pair, const Database::Models::MarketData& data);
    MarketRegime detectMarketRegime(const std::string& symbol, const Database::Models::MarketData& data);
    double calculateRegimeStrength(const std::string& symbol, const Database::Models::MarketData& data);

    // Core prediction logic
    double calculateTechnicalScore(const FeatureVector& features);
    double calculateSentimentScore(const FeatureVector& features);
    double calculateRegimeScore(const FeatureVector& features);
    double calculatePairCorrelationScore(const FeatureVector& features);

    // Risk adjustment
    double applyRiskAdjustment(double raw_score, const FeatureVector& features);
    double calculateConfidenceScore(const FeatureVector& features, double raw_score);

    // Model validation and improvement
    void recordPredictionOutcome(const std::string& prediction_id, double actual_outcome);
    double getCurrentModelAccuracy() const;

public:
    explicit CoreTradingAlgorithm(const AlgorithmConfig& config = AlgorithmConfig());
    ~CoreTradingAlgorithm() = default;

    // Main algorithm interface
    std::vector<PredictionResult> generatePredictions(
        const std::vector<std::string>& pairs,
        const Database::Models::MarketData& current_data,
        std::chrono::hours horizon = std::chrono::hours{24}
    );

    // Sorting algorithm - core functionality
    std::vector<RankedPair> sortPairsByPredictedPerformance(
        const std::vector<std::string>& pairs,
        const Database::Models::MarketData& current_data,
        std::chrono::hours horizon = std::chrono::hours{24}
    );

    // Single pair prediction
    PredictionResult predictPairPerformance(
        const std::string& pair,
        const Database::Models::MarketData& current_data,
        std::chrono::hours horizon = std::chrono::hours{24}
    );

    // Configuration management
    void updateConfiguration(const AlgorithmConfig& new_config);
    const AlgorithmConfig& getConfiguration() const { return config_; }

    // Performance tracking and validation
    void updatePredictionOutcome(const std::string& prediction_id, double actual_return);
    double getHistoricalAccuracy() const;
    std::map<std::string, double> getFeatureImportance() const;

    // Market regime information
    MarketRegime getCurrentMarketRegime(const std::string& symbol) const;
    double getRegimeConfidence(const std::string& symbol) const;

    // Model diagnostics
    struct ModelDiagnostics {
        double overall_accuracy;
        double recent_accuracy;                    // Last 30 predictions
        int total_predictions;
        int successful_predictions;                // Above confidence threshold
        std::map<std::string, double> feature_performance;
        std::map<MarketRegime, double> regime_accuracy;
    };

    ModelDiagnostics getDiagnostics() const;

    // Backtesting interface
    struct BacktestResult {
        std::vector<PredictionResult> all_predictions;
        std::vector<double> actual_outcomes;
        double correlation_coefficient;
        double average_accuracy;
        double sharpe_ratio;
        ModelDiagnostics diagnostics;
    };

    BacktestResult runBacktest(
        const std::vector<std::string>& pairs,
        const Database::Models::MarketData& historical_data,
        const std::chrono::system_clock::time_point& start_date,
        const std::chrono::system_clock::time_point& end_date
    );
};

// Forward declaration - RealCorrelationValidator is defined in RealCorrelationValidator.h
class RealCorrelationValidator;

} // namespace Algorithm
} // namespace CryptoClaude