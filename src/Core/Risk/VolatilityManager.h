#pragma once

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <memory>

namespace CryptoClaude {
namespace Risk {

/**
 * Enhanced Volatility Manager for CryptoClaude
 * Provides tunable volatility controls and downweighting mechanisms
 *
 * TRS Requirement: "excessively volatile coins should be downweighted"
 */
class VolatilityManager {
public:
    // Volatility regime classification
    enum class VolatilityRegime {
        LOW,        // <15% annualized volatility
        NORMAL,     // 15-40% annualized volatility
        HIGH,       // 40-80% annualized volatility
        EXTREME     // >80% annualized volatility
    };

    // Volatility metrics for a symbol
    struct VolatilityMetrics {
        std::string symbol;
        std::chrono::system_clock::time_point calculation_time;

        // Raw volatility measures
        double daily_volatility;           // 1-day volatility
        double weekly_volatility;          // 7-day volatility
        double monthly_volatility;         // 30-day volatility
        double annualized_volatility;      // Annualized (252-day) volatility

        // Regime classification
        VolatilityRegime regime;
        double regime_confidence;          // 0.0 to 1.0

        // Relative volatility measures
        double volatility_percentile;      // Percentile vs own history (0-100)
        double market_relative_volatility; // Volatility vs market average
        double volatility_trend;           // -1.0 to +1.0 (decreasing to increasing)

        // Risk-adjusted measures
        double sharpe_adjusted_volatility; // Volatility adjusted for returns
        double downside_volatility;        // Volatility of negative returns only
        double volatility_of_volatility;   // Second-order volatility measure

        VolatilityMetrics() : daily_volatility(0), weekly_volatility(0),
                             monthly_volatility(0), annualized_volatility(0),
                             regime(VolatilityRegime::NORMAL), regime_confidence(0),
                             volatility_percentile(50), market_relative_volatility(1.0),
                             volatility_trend(0), sharpe_adjusted_volatility(0),
                             downside_volatility(0), volatility_of_volatility(0) {
            calculation_time = std::chrono::system_clock::now();
        }
    };

    // Tunable volatility control parameters (TRS requirement)
    struct VolatilityControlConfig {
        // === CONTINUOUS VOLATILITY WEIGHTING SYSTEM ===
        double baseline_volatility = 0.30;          // 30% annualized - neutral point (1.0x weight)
        double volatility_penalty_strength = 2.0;   // Calibrated penalty strength factor (user tunable)
        double max_volatility_penalty = 0.80;       // Maximum penalty: 80% weight reduction
        double min_volatility_weight = 0.20;        // Minimum weight floor: 20% (80% penalty max)

        // === VOLATILITY CURVE PARAMETERS ===
        double volatility_curve_steepness = 1.5;    // Controls curve steepness (1.0 = linear, >1.0 = steeper)
        double volatility_sensitivity_floor = 0.10; // Below 10% vol, no penalty applied
        double volatility_sensitivity_ceiling = 2.0; // Above 200% vol, maximum penalty applied

        // === CONFIDENCE ADJUSTMENTS ===
        double volatility_confidence_penalty = 0.2; // Reduce confidence by volatility
        double regime_uncertainty_penalty = 0.1;    // Penalty for regime uncertainty

        // === DYNAMIC ADJUSTMENTS ===
        bool enable_dynamic_thresholds = true;      // Adapt thresholds to market conditions
        bool enable_relative_volatility = true;     // Use relative vs absolute volatility
        bool enable_volatility_trend = true;        // Consider volatility trends

        // === POSITION SIZING ADJUSTMENTS ===
        double max_position_high_volatility = 0.05; // 5% max position for high vol
        double max_position_extreme_volatility = 0.02; // 2% max position for extreme vol

        // === TUNABLE PARAMETERS (TRS Focus) ===
        double volatility_sensitivity = 1.0;        // 0.5-2.0 range: how sensitive to volatility
        double alpha_vs_volatility_tradeoff = 0.7;  // 0.0-1.0: prefer alpha vs low volatility
        double regime_adaptation_speed = 0.1;       // How quickly to adapt to regime changes

        VolatilityControlConfig() = default;
    };

    // Volatility-adjusted scoring result
    struct VolatilityAdjustedScore {
        std::string symbol;
        double original_score;
        double adjusted_score;
        double volatility_penalty;
        double confidence_adjustment;

        VolatilityRegime regime;
        double position_size_limit;    // Maximum position size recommendation
        bool downweighted;            // True if significantly downweighted

        std::string adjustment_reasoning;

        VolatilityAdjustedScore() : original_score(0), adjusted_score(0),
                                   volatility_penalty(0), confidence_adjustment(0),
                                   regime(VolatilityRegime::NORMAL), position_size_limit(0.1),
                                   downweighted(false) {}
    };

public:
    explicit VolatilityManager(const VolatilityControlConfig& config = VolatilityControlConfig());

    // === VOLATILITY CALCULATION ===

    // Calculate comprehensive volatility metrics for a symbol
    VolatilityMetrics calculateVolatilityMetrics(
        const std::string& symbol,
        const std::vector<double>& price_history,
        const std::vector<double>& returns_history,
        std::chrono::hours lookback_period = std::chrono::hours(24 * 30) // 30 days
    );

    // Batch calculate volatility for multiple symbols
    std::map<std::string, VolatilityMetrics> calculateBatchVolatilityMetrics(
        const std::map<std::string, std::vector<double>>& price_histories,
        const std::map<std::string, std::vector<double>>& returns_histories
    );

    // === VOLATILITY-BASED SCORE ADJUSTMENT ===

    // Apply volatility-based downweighting to trading scores (TRS requirement)
    VolatilityAdjustedScore adjustScoreForVolatility(
        const std::string& symbol,
        double original_score,
        double original_confidence,
        const VolatilityMetrics& volatility_metrics
    );

    // Batch adjust scores for multiple symbols
    std::map<std::string, VolatilityAdjustedScore> adjustBatchScoresForVolatility(
        const std::map<std::string, double>& original_scores,
        const std::map<std::string, double>& original_confidences,
        const std::map<std::string, VolatilityMetrics>& volatility_metrics
    );

    // === REGIME-BASED ADJUSTMENTS ===

    // Classify volatility regime for a symbol
    VolatilityRegime classifyVolatilityRegime(double annualized_volatility) const;

    // Get recommended position size limit based on volatility
    double getPositionSizeLimit(const VolatilityMetrics& metrics) const;

    // === CONTINUOUS VOLATILITY WEIGHTING ===

    // Calculate continuous volatility weight for a coin (core TRS function)
    double calculateContinuousVolatilityWeight(double annualized_volatility) const;

    // Get volatility weight with detailed breakdown
    struct VolatilityWeightBreakdown {
        double annualized_volatility;
        double raw_weight;                    // Before floors/ceilings
        double final_weight;                  // After floors/ceilings applied
        double penalty_factor;                // How much penalty was applied (0.0 to max_penalty)
        bool hit_floor;                      // True if minimum weight floor was applied
        bool hit_ceiling;                    // True if maximum weight ceiling was applied
        std::string weight_explanation;      // Human-readable explanation
    };

    VolatilityWeightBreakdown calculateVolatilityWeightBreakdown(double annualized_volatility) const;

    // === TUNABLE PARAMETER MANAGEMENT ===

    // Update volatility control configuration (TRS requirement: tunable)
    void updateConfig(const VolatilityControlConfig& new_config);
    const VolatilityControlConfig& getConfig() const { return config_; }

    // Dynamic threshold adjustment based on market conditions
    void adaptThresholdsToMarketConditions(const std::map<std::string, VolatilityMetrics>& market_metrics);

    // === INTEGRATION WITH RANDOM FOREST ===

    // Create volatility features for ML model
    struct VolatilityFeatures {
        double normalized_volatility;        // 0.0 to 1.0 normalized volatility
        double volatility_regime_score;      // 0.0 to 1.0 based on regime
        double volatility_trend_indicator;   // -1.0 to +1.0
        double relative_volatility_score;    // Relative to market
        double volatility_stability;         // How stable the volatility is
    };

    VolatilityFeatures createVolatilityFeatures(const VolatilityMetrics& metrics) const;

    // === MONITORING AND ANALYSIS ===

    struct VolatilityReport {
        std::chrono::system_clock::time_point report_time;

        // Market-wide statistics
        double market_average_volatility;
        double market_volatility_trend;
        std::map<VolatilityRegime, int> regime_distribution;

        // Top/bottom volatility symbols
        std::vector<std::string> most_volatile_symbols;
        std::vector<std::string> least_volatile_symbols;

        // Adjustment statistics
        int symbols_downweighted;
        double average_volatility_penalty;
        int position_size_limited_symbols;
    };

    VolatilityReport generateVolatilityReport(
        const std::map<std::string, VolatilityMetrics>& all_metrics
    ) const;

    // === UTILITIES ===

    // Convert volatility regime to string
    static std::string volatilityRegimeToString(VolatilityRegime regime);

    // Calculate volatility from price returns
    static double calculateVolatility(const std::vector<double>& returns, int lookback_days = 30);

    // Annualize volatility (daily to annual)
    static double annualizeVolatility(double daily_volatility);

private:
    VolatilityControlConfig config_;

    // Cached market-wide statistics
    double market_average_volatility_;
    std::chrono::system_clock::time_point last_market_analysis_;

    // Helper methods
    double calculateVolatilityPercentile(const std::string& symbol, double current_volatility) const;
    double calculateMarketRelativeVolatility(double symbol_volatility, double market_average) const;
    double calculateVolatilityTrend(const std::vector<double>& volatility_history) const;
    VolatilityRegime determineRegimeWithConfidence(double volatility, double& confidence) const;

    // Dynamic threshold calculation
    void updateDynamicThresholds(const std::map<std::string, VolatilityMetrics>& market_data);
};

}} // namespace CryptoClaude::Risk