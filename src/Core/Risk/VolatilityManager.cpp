#include "VolatilityManager.h"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace CryptoClaude {
namespace Risk {

VolatilityManager::VolatilityManager(const VolatilityControlConfig& config)
    : config_(config), market_average_volatility_(0.0) {
    last_market_analysis_ = std::chrono::system_clock::now();
}

// === CONTINUOUS VOLATILITY WEIGHTING IMPLEMENTATION ===

double VolatilityManager::calculateContinuousVolatilityWeight(double annualized_volatility) const {
    // Apply floor and ceiling
    double clamped_vol = std::max(config_.volatility_sensitivity_floor,
                                 std::min(config_.volatility_sensitivity_ceiling, annualized_volatility));

    // If below baseline, no penalty (weight = 1.0 or potentially bonus)
    if (clamped_vol <= config_.baseline_volatility) {
        return 1.0;
    }

    // Calculate excess volatility above baseline
    double excess_volatility = clamped_vol - config_.baseline_volatility;
    double max_excess = config_.volatility_sensitivity_ceiling - config_.baseline_volatility;

    // Normalized excess (0.0 to 1.0)
    double normalized_excess = excess_volatility / max_excess;

    // Apply curve steepness (power function)
    double shaped_penalty = std::pow(normalized_excess, config_.volatility_curve_steepness);

    // Scale by penalty strength
    double penalty_factor = shaped_penalty * config_.volatility_penalty_strength * config_.max_volatility_penalty;

    // Calculate final weight
    double raw_weight = 1.0 - penalty_factor;

    // Apply minimum weight floor
    double final_weight = std::max(config_.min_volatility_weight, raw_weight);

    return final_weight;
}

VolatilityManager::VolatilityWeightBreakdown
VolatilityManager::calculateVolatilityWeightBreakdown(double annualized_volatility) const {
    VolatilityWeightBreakdown breakdown;
    breakdown.annualized_volatility = annualized_volatility;

    // Apply floor and ceiling
    double clamped_vol = std::max(config_.volatility_sensitivity_floor,
                                 std::min(config_.volatility_sensitivity_ceiling, annualized_volatility));

    breakdown.hit_ceiling = (annualized_volatility > config_.volatility_sensitivity_ceiling);

    // Calculate raw weight
    if (clamped_vol <= config_.baseline_volatility) {
        breakdown.raw_weight = 1.0;
        breakdown.penalty_factor = 0.0;
        breakdown.weight_explanation = "Below baseline volatility - no penalty applied";
    } else {
        // Calculate excess volatility above baseline
        double excess_volatility = clamped_vol - config_.baseline_volatility;
        double max_excess = config_.volatility_sensitivity_ceiling - config_.baseline_volatility;

        // Normalized excess (0.0 to 1.0)
        double normalized_excess = excess_volatility / max_excess;

        // Apply curve steepness
        double shaped_penalty = std::pow(normalized_excess, config_.volatility_curve_steepness);

        // Scale by penalty strength
        breakdown.penalty_factor = shaped_penalty * config_.volatility_penalty_strength * config_.max_volatility_penalty;
        breakdown.raw_weight = 1.0 - breakdown.penalty_factor;

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1)
            << "Excess vol: " << (excess_volatility * 100) << "%, "
            << "Penalty: " << (breakdown.penalty_factor * 100) << "%";
        breakdown.weight_explanation = oss.str();
    }

    // Apply minimum weight floor
    breakdown.final_weight = std::max(config_.min_volatility_weight, breakdown.raw_weight);
    breakdown.hit_floor = (breakdown.raw_weight < config_.min_volatility_weight);

    if (breakdown.hit_floor) {
        breakdown.weight_explanation += " (floor applied)";
    }
    if (breakdown.hit_ceiling) {
        breakdown.weight_explanation += " (ceiling applied)";
    }

    return breakdown;
}

// === VOLATILITY METRICS CALCULATION ===

VolatilityManager::VolatilityMetrics
VolatilityManager::calculateVolatilityMetrics(const std::string& symbol,
                                             const std::vector<double>& price_history,
                                             const std::vector<double>& returns_history,
                                             std::chrono::hours lookback_period) {
    VolatilityMetrics metrics;
    metrics.symbol = symbol;
    metrics.calculation_time = std::chrono::system_clock::now();

    if (returns_history.empty()) {
        return metrics; // Return default-initialized metrics
    }

    // Calculate basic volatility measures
    metrics.daily_volatility = calculateVolatility(returns_history, 1);
    metrics.weekly_volatility = calculateVolatility(returns_history, 7);
    metrics.monthly_volatility = calculateVolatility(returns_history, 30);
    metrics.annualized_volatility = annualizeVolatility(metrics.daily_volatility);

    // Classify regime
    double regime_confidence;
    metrics.regime = determineRegimeWithConfidence(metrics.annualized_volatility, regime_confidence);
    metrics.regime_confidence = regime_confidence;

    // Calculate relative measures (simplified for this implementation)
    metrics.market_relative_volatility = metrics.annualized_volatility / std::max(0.01, market_average_volatility_);
    metrics.volatility_percentile = 50.0; // Placeholder - would need historical data
    metrics.volatility_trend = 0.0; // Placeholder - would need trend calculation

    return metrics;
}

// === VOLATILITY-BASED SCORE ADJUSTMENT ===

VolatilityManager::VolatilityAdjustedScore
VolatilityManager::adjustScoreForVolatility(const std::string& symbol,
                                           double original_score,
                                           double original_confidence,
                                           const VolatilityMetrics& volatility_metrics) {
    VolatilityAdjustedScore adjusted;
    adjusted.symbol = symbol;
    adjusted.original_score = original_score;
    adjusted.regime = volatility_metrics.regime;

    // Calculate continuous volatility weight
    double volatility_weight = calculateContinuousVolatilityWeight(volatility_metrics.annualized_volatility);

    // Apply volatility adjustment
    adjusted.adjusted_score = original_score * volatility_weight;
    adjusted.volatility_penalty = 1.0 - volatility_weight;

    // Confidence adjustment based on volatility
    adjusted.confidence_adjustment = original_confidence * (1.0 - config_.volatility_confidence_penalty * adjusted.volatility_penalty);

    // Position size limit
    adjusted.position_size_limit = getPositionSizeLimit(volatility_metrics);

    // Determine if significantly downweighted
    adjusted.downweighted = (volatility_weight < 0.80); // 20%+ penalty considered significant

    // Generate reasoning
    auto breakdown = calculateVolatilityWeightBreakdown(volatility_metrics.annualized_volatility);
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1)
        << "Vol: " << (volatility_metrics.annualized_volatility * 100) << "%, "
        << "Weight: " << (volatility_weight * 100) << "%, "
        << breakdown.weight_explanation;
    adjusted.adjustment_reasoning = oss.str();

    return adjusted;
}

// === REGIME CLASSIFICATION ===

VolatilityManager::VolatilityRegime
VolatilityManager::classifyVolatilityRegime(double annualized_volatility) const {
    // Use baseline volatility for more dynamic regime classification
    if (annualized_volatility < config_.baseline_volatility * 0.5) {
        return VolatilityRegime::LOW;
    } else if (annualized_volatility < config_.baseline_volatility * 1.5) {
        return VolatilityRegime::NORMAL;
    } else if (annualized_volatility < config_.baseline_volatility * 3.0) {
        return VolatilityRegime::HIGH;
    } else {
        return VolatilityRegime::EXTREME;
    }
}

double VolatilityManager::getPositionSizeLimit(const VolatilityMetrics& metrics) const {
    // Continuous position sizing based on volatility weight
    double volatility_weight = calculateContinuousVolatilityWeight(metrics.annualized_volatility);

    // Base position size limit (e.g., 10% for normal volatility)
    double base_limit = 0.10;

    // Scale position size by volatility weight
    // Higher volatility = lower weight = smaller position size
    double position_limit = base_limit * volatility_weight;

    // Apply minimum and maximum bounds
    return std::max(0.01, std::min(0.15, position_limit)); // Min 1%, Max 15%
}

// === UTILITY METHODS ===

std::string VolatilityManager::volatilityRegimeToString(VolatilityRegime regime) {
    switch (regime) {
        case VolatilityRegime::LOW: return "LOW";
        case VolatilityRegime::NORMAL: return "NORMAL";
        case VolatilityRegime::HIGH: return "HIGH";
        case VolatilityRegime::EXTREME: return "EXTREME";
        default: return "UNKNOWN";
    }
}

double VolatilityManager::calculateVolatility(const std::vector<double>& returns, int lookback_days) {
    if (returns.empty() || lookback_days <= 0) return 0.0;

    size_t n = std::min(static_cast<size_t>(lookback_days), returns.size());
    if (n < 2) return 0.0;

    // Calculate mean
    double sum = 0.0;
    for (size_t i = returns.size() - n; i < returns.size(); ++i) {
        sum += returns[i];
    }
    double mean = sum / n;

    // Calculate variance
    double variance = 0.0;
    for (size_t i = returns.size() - n; i < returns.size(); ++i) {
        double diff = returns[i] - mean;
        variance += diff * diff;
    }
    variance /= (n - 1);

    return std::sqrt(variance);
}

double VolatilityManager::annualizeVolatility(double daily_volatility) {
    return daily_volatility * std::sqrt(252.0); // 252 trading days per year
}

// === PARAMETER MANAGEMENT ===

void VolatilityManager::updateConfig(const VolatilityControlConfig& new_config) {
    config_ = new_config;
}

// === PRIVATE HELPER METHODS ===

VolatilityManager::VolatilityRegime
VolatilityManager::determineRegimeWithConfidence(double volatility, double& confidence) const {
    auto regime = classifyVolatilityRegime(volatility);

    // Simple confidence calculation based on distance from regime boundaries
    // In practice, this would be more sophisticated
    confidence = 0.8; // Placeholder

    return regime;
}

}} // namespace CryptoClaude::Risk