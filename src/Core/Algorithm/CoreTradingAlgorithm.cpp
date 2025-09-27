#include "CoreTradingAlgorithm.h"
#include <algorithm>
#include <numeric>
#include <random>
#include <cmath>
#include <stdexcept>

namespace CryptoClaude {
namespace Algorithm {

CoreTradingAlgorithm::CoreTradingAlgorithm(const AlgorithmConfig& config)
    : config_(config) {
    // Initialize technical analyzer and sentiment analyzer
    // These would be injected or created based on configuration
    technical_analyzer_ = std::make_unique<Analytics::TechnicalIndicators>();
    sentiment_analyzer_ = std::make_unique<Sentiment::SentimentAnalyzer>();
}

std::vector<CoreTradingAlgorithm::PredictionResult>
CoreTradingAlgorithm::generatePredictions(
    const std::vector<std::string>& pairs,
    const Data::MarketData& current_data,
    std::chrono::hours horizon) {

    std::vector<PredictionResult> predictions;
    predictions.reserve(pairs.size());

    for (const auto& pair : pairs) {
        try {
            auto prediction = predictPairPerformance(pair, current_data, horizon);
            predictions.push_back(prediction);
        } catch (const std::exception& e) {
            // Log error and continue with other pairs
            // In production, this would use proper logging
            continue;
        }
    }

    return predictions;
}

std::vector<CoreTradingAlgorithm::RankedPair>
CoreTradingAlgorithm::sortPairsByPredictedPerformance(
    const std::vector<std::string>& pairs,
    const Data::MarketData& current_data,
    std::chrono::hours horizon) {

    // Generate predictions for all pairs
    auto predictions = generatePredictions(pairs, current_data, horizon);

    // Create ranked pairs
    std::vector<RankedPair> ranked_pairs;
    ranked_pairs.reserve(predictions.size());

    for (const auto& prediction : predictions) {
        RankedPair ranked_pair;
        ranked_pair.prediction = prediction;
        ranked_pair.relative_score = prediction.risk_adjusted_return;
        ranked_pair.recommended_for_trading =
            prediction.confidence_score >= config_.minimum_confidence_threshold;
        ranked_pairs.push_back(ranked_pair);
    }

    // Sort by risk-adjusted return (descending)
    std::sort(ranked_pairs.begin(), ranked_pairs.end(),
        [](const RankedPair& a, const RankedPair& b) {
            return a.relative_score > b.relative_score;
        });

    // Assign ranks
    for (size_t i = 0; i < ranked_pairs.size(); ++i) {
        ranked_pairs[i].rank = static_cast<int>(i + 1);

        // Normalize relative scores (0-1 range)
        if (!ranked_pairs.empty()) {
            double max_score = ranked_pairs[0].relative_score;
            double min_score = ranked_pairs.back().relative_score;
            if (max_score != min_score) {
                ranked_pairs[i].relative_score =
                    (ranked_pairs[i].relative_score - min_score) / (max_score - min_score);
            }
        }
    }

    // Limit to configured maximum
    if (ranked_pairs.size() > static_cast<size_t>(config_.max_pairs_to_rank)) {
        ranked_pairs.resize(config_.max_pairs_to_rank);
    }

    return ranked_pairs;
}

CoreTradingAlgorithm::PredictionResult
CoreTradingAlgorithm::predictPairPerformance(
    const std::string& pair,
    const Data::MarketData& current_data,
    std::chrono::hours horizon) {

    PredictionResult result;
    result.pair_name = pair;
    result.prediction_time = std::chrono::system_clock::now();
    result.prediction_horizon = horizon;

    // Parse pair name to get base and quote symbols
    auto delimiter_pos = pair.find('/');
    if (delimiter_pos == std::string::npos) {
        throw std::invalid_argument("Invalid pair format: " + pair);
    }

    result.base_symbol = pair.substr(0, delimiter_pos);
    result.quote_symbol = pair.substr(delimiter_pos + 1);

    // Extract features for this pair
    auto features = extractFeatures(pair, current_data);

    // Calculate component scores
    double technical_score = calculateTechnicalScore(features);
    double sentiment_score = calculateSentimentScore(features);
    double regime_score = calculateRegimeScore(features);
    double correlation_score = calculatePairCorrelationScore(features);

    // Combine scores with configured weights
    double raw_score =
        technical_score * config_.technical_indicator_weight +
        sentiment_score * config_.sentiment_weight +
        regime_score * config_.market_regime_weight +
        correlation_score * config_.cross_correlation_weight;

    // Apply risk adjustment
    double risk_adjusted_score = applyRiskAdjustment(raw_score, features);

    // Calculate confidence
    double confidence = calculateConfidenceScore(features, raw_score);

    // Populate result
    result.predicted_return = risk_adjusted_score;
    result.confidence_score = confidence;
    result.predicted_volatility = std::abs(features.volatility_ratio - 1.0);
    result.risk_adjusted_return = risk_adjusted_score;
    result.market_regime = getCurrentMarketRegime(result.base_symbol) == MarketRegime::BULL ? "bull" :
                          getCurrentMarketRegime(result.base_symbol) == MarketRegime::BEAR ? "bear" :
                          getCurrentMarketRegime(result.base_symbol) == MarketRegime::SIDEWAYS ? "sideways" : "unknown";
    result.regime_confidence = getRegimeConfidence(result.base_symbol);

    // Store feature contributions for explainability
    result.feature_contributions["technical"] = technical_score * config_.technical_indicator_weight;
    result.feature_contributions["sentiment"] = sentiment_score * config_.sentiment_weight;
    result.feature_contributions["regime"] = regime_score * config_.market_regime_weight;
    result.feature_contributions["correlation"] = correlation_score * config_.cross_correlation_weight;

    // Generate unique prediction ID for tracking
    result.prediction_id = pair + "_" +
        std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
            result.prediction_time.time_since_epoch()).count());

    return result;
}

CoreTradingAlgorithm::FeatureVector
CoreTradingAlgorithm::extractFeatures(const std::string& pair, const Data::MarketData& data) {
    FeatureVector features;

    // Parse pair
    auto delimiter_pos = pair.find('/');
    std::string base_symbol = pair.substr(0, delimiter_pos);
    std::string quote_symbol = pair.substr(delimiter_pos + 1);

    // Extract technical indicators using the technical analyzer
    // This is a placeholder implementation - in practice, this would use real market data

    // RSI calculations (placeholder)
    features.rsi_base = 45.0 + (std::hash<std::string>{}(base_symbol) % 20); // 45-65 range
    features.rsi_quote = 45.0 + (std::hash<std::string>{}(quote_symbol) % 20);

    // MACD signals (placeholder)
    features.macd_base = -1.0 + 2.0 * (std::hash<std::string>{}(base_symbol + "macd") % 100) / 100.0;
    features.macd_quote = -1.0 + 2.0 * (std::hash<std::string>{}(quote_symbol + "macd") % 100) / 100.0;

    // Volume ratios (placeholder)
    features.volume_ratio_base = 0.5 + (std::hash<std::string>{}(base_symbol + "vol") % 100) / 100.0;
    features.volume_ratio_quote = 0.5 + (std::hash<std::string>{}(quote_symbol + "vol") % 100) / 100.0;

    // Market regime detection
    features.overall_market_regime = detectMarketRegime(base_symbol, data);
    features.regime_strength = calculateRegimeStrength(base_symbol, data);

    // Correlation and relative strength (placeholder calculations)
    features.pair_correlation = 0.3 + 0.4 * (std::hash<std::string>{}(pair + "corr") % 100) / 100.0;
    features.relative_strength = -0.5 + (std::hash<std::string>{}(pair + "strength") % 100) / 100.0;
    features.volatility_ratio = 0.8 + 0.4 * (std::hash<std::string>{}(pair + "vol") % 100) / 100.0;

    return features;
}

CoreTradingAlgorithm::MarketRegime
CoreTradingAlgorithm::detectMarketRegime(const std::string& symbol, const Data::MarketData& data) {
    // Placeholder regime detection logic
    // In practice, this would analyze price trends, volatility patterns, etc.

    auto hash_value = std::hash<std::string>{}(symbol + "regime") % 5;
    switch (hash_value) {
        case 0: return MarketRegime::BULL;
        case 1: return MarketRegime::BEAR;
        case 2: return MarketRegime::SIDEWAYS;
        case 3: return MarketRegime::VOLATILE;
        default: return MarketRegime::UNKNOWN;
    }
}

double CoreTradingAlgorithm::calculateRegimeStrength(const std::string& symbol, const Data::MarketData& data) {
    // Placeholder calculation
    return 0.5 + 0.4 * (std::hash<std::string>{}(symbol + "strength") % 100) / 100.0;
}

double CoreTradingAlgorithm::calculateTechnicalScore(const FeatureVector& features) {
    double score = 0.0;

    // RSI contribution (mean reversion signal)
    double rsi_signal = 0.0;
    if (features.rsi_base < 30) rsi_signal += 0.5; // Oversold
    if (features.rsi_base > 70) rsi_signal -= 0.5; // Overbought
    if (features.rsi_quote < 30) rsi_signal -= 0.5; // Quote oversold (negative for long base)
    if (features.rsi_quote > 70) rsi_signal += 0.5; // Quote overbought (positive for long base)

    // MACD momentum contribution
    double macd_signal = features.macd_base - features.macd_quote;

    // Volume confirmation
    double volume_signal = (features.volume_ratio_base - features.volume_ratio_quote) * 0.5;

    // Combine technical signals
    score = 0.4 * rsi_signal + 0.4 * macd_signal + 0.2 * volume_signal;

    // Normalize to [-1, 1] range
    return std::tanh(score);
}

double CoreTradingAlgorithm::calculateSentimentScore(const FeatureVector& features) {
    // Combine news and social sentiment
    double news_differential = features.news_sentiment_base - features.news_sentiment_quote;
    double social_differential = features.social_sentiment_base - features.social_sentiment_quote;
    double momentum_differential = features.sentiment_momentum_base - features.sentiment_momentum_quote;

    // Weight the sentiment components
    double combined_sentiment =
        0.5 * news_differential +
        0.3 * social_differential +
        0.2 * momentum_differential;

    // Normalize to [-1, 1] range
    return std::tanh(combined_sentiment);
}

double CoreTradingAlgorithm::calculateRegimeScore(const FeatureVector& features) {
    double base_score = 0.0;

    // Adjust based on market regime
    switch (features.overall_market_regime) {
        case MarketRegime::BULL:
            base_score = 0.2 * features.regime_strength; // Slightly positive bias in bull market
            break;
        case MarketRegime::BEAR:
            base_score = -0.2 * features.regime_strength; // Slightly negative bias in bear market
            break;
        case MarketRegime::SIDEWAYS:
            base_score = 0.0; // Neutral in sideways market
            break;
        case MarketRegime::VOLATILE:
            base_score = -0.1 * features.regime_strength; // Slight negative bias in volatile periods
            break;
        default:
            base_score = 0.0;
    }

    // Add trend momentum
    base_score += 0.3 * features.trend_momentum;

    return std::tanh(base_score);
}

double CoreTradingAlgorithm::calculatePairCorrelationScore(const FeatureVector& features) {
    // Lower correlation is generally better for pair trading
    double correlation_penalty = -0.5 * features.pair_correlation;

    // Relative strength signal
    double strength_signal = features.relative_strength;

    // BTC correlation differential
    double btc_correlation_signal =
        0.3 * (features.correlation_to_btc_quote - features.correlation_to_btc_base);

    double combined_score = correlation_penalty + strength_signal + btc_correlation_signal;

    return std::tanh(combined_score);
}

double CoreTradingAlgorithm::applyRiskAdjustment(double raw_score, const FeatureVector& features) {
    // Volatility penalty
    double volatility_penalty = config_.volatility_penalty_factor *
        std::abs(features.volatility_ratio - 1.0);

    // High correlation penalty (for risk management)
    double correlation_penalty = config_.correlation_penalty_factor *
        std::max(0.0, features.pair_correlation - 0.7);

    // Apply penalties
    double adjusted_score = raw_score - volatility_penalty - correlation_penalty;

    return adjusted_score;
}

double CoreTradingAlgorithm::calculateConfidenceScore(const FeatureVector& features, double raw_score) {
    double base_confidence = 0.5; // Base confidence level

    // Higher confidence for stronger signals
    double signal_strength_boost = 0.2 * std::abs(raw_score);

    // Higher confidence with strong regime identification
    double regime_boost = 0.1 * features.regime_strength;

    // Lower confidence in volatile markets
    double volatility_penalty = 0.1 * std::abs(features.volatility_ratio - 1.0);

    // Data quality boost (placeholder)
    double data_quality_boost = 0.1; // Would be based on actual data quality metrics

    double confidence = base_confidence + signal_strength_boost + regime_boost +
                       data_quality_boost - volatility_penalty;

    // Clamp to [0, 1] range
    return std::max(0.0, std::min(1.0, confidence));
}

CoreTradingAlgorithm::MarketRegime
CoreTradingAlgorithm::getCurrentMarketRegime(const std::string& symbol) const {
    auto it = current_regimes_.find(symbol);
    if (it != current_regimes_.end()) {
        return it->second;
    }
    return MarketRegime::UNKNOWN;
}

double CoreTradingAlgorithm::getRegimeConfidence(const std::string& symbol) const {
    auto it = regime_confidence_.find(symbol);
    if (it != regime_confidence_.end()) {
        return it->second;
    }
    return 0.0;
}

void CoreTradingAlgorithm::updatePredictionOutcome(const std::string& prediction_id, double actual_return) {
    // Find the prediction and record the outcome
    // In a full implementation, this would maintain a database of predictions and outcomes
    // for continuous model improvement and correlation calculation
}

double CoreTradingAlgorithm::getHistoricalAccuracy() const {
    if (prediction_outcomes_.empty()) {
        return 0.0;
    }

    // Calculate correlation between predictions and outcomes
    std::vector<double> predictions, outcomes;
    for (const auto& pair : prediction_outcomes_) {
        predictions.push_back(pair.first.predicted_return);
        outcomes.push_back(pair.second);
    }

    RealCorrelationValidator validator;
    return validator.calculateCorrelation(predictions, outcomes);
}

// RealCorrelationValidator implementation
double RealCorrelationValidator::calculateCorrelation(
    const std::vector<double>& predictions,
    const std::vector<double>& actual_outcomes) {

    if (predictions.size() != actual_outcomes.size() || predictions.empty()) {
        throw std::invalid_argument("Prediction and outcome vectors must have the same non-zero size");
    }

    size_t n = predictions.size();

    // Calculate means
    double pred_mean = std::accumulate(predictions.begin(), predictions.end(), 0.0) / n;
    double outcome_mean = std::accumulate(actual_outcomes.begin(), actual_outcomes.end(), 0.0) / n;

    // Calculate correlation coefficient
    double numerator = 0.0;
    double pred_var = 0.0;
    double outcome_var = 0.0;

    for (size_t i = 0; i < n; ++i) {
        double pred_diff = predictions[i] - pred_mean;
        double outcome_diff = actual_outcomes[i] - outcome_mean;

        numerator += pred_diff * outcome_diff;
        pred_var += pred_diff * pred_diff;
        outcome_var += outcome_diff * outcome_diff;
    }

    if (pred_var == 0.0 || outcome_var == 0.0) {
        return 0.0; // No variance in one of the variables
    }

    double correlation = numerator / std::sqrt(pred_var * outcome_var);
    return correlation;
}

RealCorrelationValidator::CorrelationAnalysis
RealCorrelationValidator::analyzeCorrelation(
    const std::vector<CoreTradingAlgorithm::PredictionResult>& predictions,
    const std::vector<double>& actual_outcomes) {

    CorrelationAnalysis analysis;

    if (predictions.size() != actual_outcomes.size() || predictions.empty()) {
        analysis.correlation_coefficient = 0.0;
        analysis.p_value = 1.0;
        analysis.sample_size = 0;
        return analysis;
    }

    // Extract prediction values
    std::vector<double> pred_values;
    pred_values.reserve(predictions.size());
    for (const auto& pred : predictions) {
        pred_values.push_back(pred.predicted_return);
    }

    // Calculate correlation
    analysis.correlation_coefficient = calculateCorrelation(pred_values, actual_outcomes);
    analysis.sample_size = static_cast<int>(predictions.size());

    // Calculate statistical significance (simplified t-test)
    if (analysis.sample_size > 2) {
        double t_stat = analysis.correlation_coefficient *
            std::sqrt((analysis.sample_size - 2) / (1 - analysis.correlation_coefficient * analysis.correlation_coefficient));

        // Approximate p-value calculation (would use proper statistical libraries in production)
        analysis.p_value = std::max(0.001, 1.0 - std::abs(t_stat) / 3.0); // Very rough approximation

        // 95% confidence interval (rough approximation)
        double se = 1.0 / std::sqrt(analysis.sample_size - 3);
        analysis.confidence_interval_low = analysis.correlation_coefficient - 1.96 * se;
        analysis.confidence_interval_high = analysis.correlation_coefficient + 1.96 * se;
    }

    return analysis;
}

} // namespace Algorithm
} // namespace CryptoClaude