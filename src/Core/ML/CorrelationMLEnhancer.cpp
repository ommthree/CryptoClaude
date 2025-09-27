#include "CorrelationMLEnhancer.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <random>
#include <iostream>

namespace CryptoClaude {
namespace ML {

// CorrelationRegimeDetector Implementation
CorrelationRegimeDetector::CorrelationRegimeDetector()
    : regime_model_(std::make_unique<RandomForestPredictor>(50, 8))
    , risk_off_threshold_(0.7)
    , decoupling_threshold_(-0.2)
    , flight_to_quality_threshold_(25.0)
    , is_trained_(false)
    , model_version_("v1.0") {
}

CorrelationRegimeDetector::~CorrelationRegimeDetector() = default;

CorrelationRegimeResult CorrelationRegimeDetector::detectCurrentRegime(
    const std::vector<Analytics::CorrelationSnapshot>& correlations) {

    CorrelationRegimeResult result;
    result.prediction_time = std::chrono::system_clock::now();

    if (correlations.empty()) {
        result.current_regime = CorrelationRegime::MIXED_SIGNALS;
        result.regime_confidence = 0.0;
        result.regime_stability_score = 0.0;
        return result;
    }

    // Determine regime from current correlations
    result.current_regime = determineRegimeFromCorrelations(correlations);

    // Calculate regime confidence based on correlation consistency
    double total_correlation_strength = 0.0;
    int significant_correlations = 0;

    for (const auto& correlation : correlations) {
        if (correlation.is_significant) {
            total_correlation_strength += std::abs(correlation.correlation);
            significant_correlations++;
        }
    }

    result.regime_confidence = significant_correlations > 0 ?
        total_correlation_strength / significant_correlations : 0.0;

    // Calculate stability score
    result.regime_stability_score = calculateRegimeStability(correlations);

    // Generate regime indicators
    CorrelationMLFeatures dummy_features; // Create from correlations in full implementation
    result.regime_indicators = generateRegimeIndicators(result.current_regime, dummy_features);

    return result;
}

CorrelationRegimeResult CorrelationRegimeDetector::predictRegimeChange(
    const std::vector<Analytics::CorrelationSnapshot>& correlation_history) {

    CorrelationRegimeResult result;
    result.prediction_time = std::chrono::system_clock::now();

    if (correlation_history.size() < 10) {
        result.current_regime = CorrelationRegime::MIXED_SIGNALS;
        result.predicted_regime = CorrelationRegime::MIXED_SIGNALS;
        result.regime_confidence = 0.0;
        return result;
    }

    // Detect current regime
    std::vector<Analytics::CorrelationSnapshot> recent_correlations(
        correlation_history.end() - 5, correlation_history.end());
    result.current_regime = determineRegimeFromCorrelations(recent_correlations);

    // Simple regime change prediction based on trends
    std::vector<Analytics::CorrelationSnapshot> historical_correlations(
        correlation_history.end() - 10, correlation_history.end() - 5);
    CorrelationRegime historical_regime = determineRegimeFromCorrelations(historical_correlations);

    // Predict regime persistence or change
    if (result.current_regime == historical_regime) {
        result.predicted_regime = result.current_regime; // Regime persistence
        result.regime_confidence = 0.8;
    } else {
        // Regime is already changing, predict continuation of new regime
        result.predicted_regime = result.current_regime;
        result.regime_confidence = 0.6;
    }

    result.regime_stability_score = calculateRegimeStability(correlation_history);

    return result;
}

CorrelationRegime CorrelationRegimeDetector::classifyRegime(const CorrelationMLFeatures& features) {
    // Rule-based regime classification using correlation features

    if (features.btc_spx_correlation > risk_off_threshold_ && features.market_stress_level > 0.6) {
        return CorrelationRegime::RISK_OFF;
    }

    if (features.btc_spx_correlation < decoupling_threshold_ &&
        features.btc_gold_correlation < decoupling_threshold_) {
        return CorrelationRegime::DECOUPLING;
    }

    if (features.vix_level > flight_to_quality_threshold_ &&
        features.flight_to_quality_score > 0.7) {
        return CorrelationRegime::FLIGHT_TO_QUALITY;
    }

    if (features.risk_appetite_index > 0.7 && features.market_stress_level < 0.3) {
        return CorrelationRegime::RISK_ON;
    }

    if (std::abs(features.btc_spx_correlation) < 0.3 &&
        std::abs(features.btc_gold_correlation) < 0.3) {
        return CorrelationRegime::NORMAL;
    }

    return CorrelationRegime::MIXED_SIGNALS;
}

double CorrelationRegimeDetector::calculateRegimeStability(
    const std::vector<Analytics::CorrelationSnapshot>& correlation_history) {

    if (correlation_history.size() < 5) return 0.0;

    // Calculate variance in correlations to measure stability
    std::map<std::string, std::vector<double>> correlation_series;

    for (const auto& snapshot : correlation_history) {
        std::string pair_key = snapshot.pair.crypto_symbol + "/" + snapshot.pair.traditional_symbol;
        correlation_series[pair_key].push_back(snapshot.correlation);
    }

    double total_variance = 0.0;
    int pair_count = 0;

    for (const auto& series : correlation_series) {
        if (series.second.size() > 1) {
            double mean = std::accumulate(series.second.begin(), series.second.end(), 0.0) / series.second.size();
            double variance = 0.0;
            for (double corr : series.second) {
                variance += (corr - mean) * (corr - mean);
            }
            variance /= (series.second.size() - 1);
            total_variance += variance;
            pair_count++;
        }
    }

    // Return stability score (lower variance = higher stability)
    if (pair_count > 0) {
        double avg_variance = total_variance / pair_count;
        return std::max(0.0, 1.0 - (avg_variance * 5.0)); // Scale to 0-1
    }

    return 0.0;
}

bool CorrelationRegimeDetector::trainRegimeModel(
    const std::vector<CorrelationMLFeatures>& training_features,
    const std::vector<CorrelationRegime>& training_labels) {

    if (training_features.size() != training_labels.size() || training_features.empty()) {
        return false;
    }

    // Convert regime labels to numeric values for Random Forest training
    std::vector<double> numeric_targets;
    numeric_targets.reserve(training_labels.size());

    for (CorrelationRegime regime : training_labels) {
        numeric_targets.push_back(static_cast<double>(regime));
    }

    // Convert CorrelationMLFeatures to standard MLFeatureVector for training
    std::vector<MLFeatureVector> standard_features;
    standard_features.reserve(training_features.size());

    for (const auto& corr_feature : training_features) {
        MLFeatureVector std_feature = static_cast<MLFeatureVector>(corr_feature);
        // In full implementation, copy correlation-specific features too
        standard_features.push_back(std_feature);
    }

    bool success = regime_model_->trainModel(standard_features, numeric_targets, "regime_" + model_version_);
    is_trained_ = success;
    return success;
}

void CorrelationRegimeDetector::setRegimeThresholds(double risk_off_threshold, double decoupling_threshold) {
    risk_off_threshold_ = risk_off_threshold;
    decoupling_threshold_ = decoupling_threshold;
}

CorrelationRegime CorrelationRegimeDetector::determineRegimeFromCorrelations(
    const std::vector<Analytics::CorrelationSnapshot>& correlations) {

    if (correlations.empty()) return CorrelationRegime::MIXED_SIGNALS;

    // Find BTC-SPX correlation
    double btc_spx_correlation = 0.0;
    double avg_market_stress = 0.0;
    bool found_btc_spx = false;

    for (const auto& corr : correlations) {
        if (corr.pair.crypto_symbol == "BTC" && corr.pair.traditional_symbol == "SPX") {
            btc_spx_correlation = corr.correlation;
            found_btc_spx = true;
        }
        if (corr.correlation_spike || corr.regime_change) {
            avg_market_stress += 1.0;
        }
    }

    avg_market_stress /= correlations.size();

    if (!found_btc_spx) return CorrelationRegime::MIXED_SIGNALS;

    // Simple regime classification
    if (btc_spx_correlation > risk_off_threshold_ && avg_market_stress > 0.5) {
        return CorrelationRegime::RISK_OFF;
    } else if (btc_spx_correlation < decoupling_threshold_) {
        return CorrelationRegime::DECOUPLING;
    } else if (std::abs(btc_spx_correlation) < 0.2) {
        return CorrelationRegime::NORMAL;
    } else {
        return CorrelationRegime::MIXED_SIGNALS;
    }
}

std::vector<std::string> CorrelationRegimeDetector::generateRegimeIndicators(
    CorrelationRegime regime, const CorrelationMLFeatures& features) {

    std::vector<std::string> indicators;

    switch (regime) {
        case CorrelationRegime::RISK_OFF:
            indicators.push_back("High correlation with traditional risk assets");
            indicators.push_back("Elevated market stress indicators");
            indicators.push_back("Risk-off sentiment detected");
            break;

        case CorrelationRegime::DECOUPLING:
            indicators.push_back("Crypto assets decoupling from traditional markets");
            indicators.push_back("Low correlation with S&P 500 and Gold");
            indicators.push_back("Independent crypto market dynamics");
            break;

        case CorrelationRegime::FLIGHT_TO_QUALITY:
            indicators.push_back("Flight to quality detected");
            indicators.push_back("Traditional safe haven assets outperforming");
            indicators.push_back("Elevated volatility in risk assets");
            break;

        case CorrelationRegime::RISK_ON:
            indicators.push_back("Risk-on sentiment prevailing");
            indicators.push_back("Positive correlation with growth assets");
            indicators.push_back("Low market stress environment");
            break;

        case CorrelationRegime::NORMAL:
            indicators.push_back("Normal correlation regime");
            indicators.push_back("Moderate correlations with traditional assets");
            indicators.push_back("Stable market conditions");
            break;

        case CorrelationRegime::MIXED_SIGNALS:
        default:
            indicators.push_back("Mixed correlation signals");
            indicators.push_back("Inconsistent market regime indicators");
            indicators.push_back("Monitor for regime clarification");
            break;
    }

    return indicators;
}

// CorrelationMLEnhancer Implementation
CorrelationMLEnhancer::CorrelationMLEnhancer()
    : correlation_predictor_(std::make_unique<RandomForestPredictor>(100, 10))
    , regime_detector_(std::make_unique<CorrelationRegimeDetector>())
    , is_initialized_(false)
    , correlation_model_trained_(false)
    , regime_model_trained_(false)
    , model_version_("correlation_v1.0")
    , default_lookback_days_(30)
    , correlation_change_threshold_(0.2)
    , regime_confidence_threshold_(0.7) {

    // Initialize performance metrics
    model_metrics_.correlation_prediction_accuracy = 0.0;
    model_metrics_.regime_detection_accuracy = 0.0;
    model_metrics_.average_prediction_error = 0.0;
    model_metrics_.model_confidence_score = 0.0;
    model_metrics_.predictions_made_24h = 0;
    model_metrics_.regime_changes_detected_24h = 0;
}

CorrelationMLEnhancer::~CorrelationMLEnhancer() = default;

bool CorrelationMLEnhancer::initialize(
    const std::shared_ptr<Analytics::CrossAssetCorrelationMonitor>& correlation_monitor) {

    correlation_monitor_ = correlation_monitor;

    if (!correlation_monitor_) {
        std::cerr << "Error: Correlation monitor is null" << std::endl;
        return false;
    }

    // Initialize correlation cache
    updateCorrelationCache();

    is_initialized_ = true;
    return true;
}

CorrelationPrediction CorrelationMLEnhancer::predictCorrelationChange(
    const std::string& crypto_symbol,
    const std::string& traditional_symbol,
    int horizon_hours) {

    CorrelationPrediction prediction;
    prediction.asset_pair = crypto_symbol + "/" + traditional_symbol;
    prediction.prediction_horizon_hours = horizon_hours;
    prediction.prediction_time = std::chrono::system_clock::now();
    prediction.model_version = model_version_;

    if (!is_initialized_ || !correlation_model_trained_) {
        prediction.prediction_confidence = 0.0;
        return prediction;
    }

    try {
        // Get correlation history for this pair
        Analytics::CrossAssetPair pair(crypto_symbol, traditional_symbol, "");
        auto correlation_history = correlation_monitor_->getCorrelationHistory(pair, default_lookback_days_);

        if (correlation_history.empty()) {
            prediction.prediction_confidence = 0.0;
            return prediction;
        }

        // Get current correlation
        prediction.current_correlation = correlation_history.back().correlation;

        // Create enhanced features
        auto enhanced_features = createCorrelationFeatures(crypto_symbol, correlation_history);

        // Get ML prediction
        auto ml_result = correlation_predictor_->predict(enhanced_features);

        // Convert ML prediction to correlation change forecast
        prediction.predicted_correlation = prediction.current_correlation + ml_result.predicted_return;
        prediction.correlation_change_forecast = ml_result.predicted_return;
        prediction.prediction_confidence = ml_result.confidence_score;

        // Predict regime change
        auto regime_result = regime_detector_->predictRegimeChange(correlation_history);
        prediction.regime_change_likely = regime_result.current_regime != regime_result.predicted_regime;
        prediction.regime_change_probability = 1.0 - regime_result.regime_confidence;
        prediction.predicted_regime = regime_result.predicted_regime;

        model_metrics_.predictions_made_24h++;

    } catch (const std::exception& e) {
        std::cerr << "Error predicting correlation change: " << e.what() << std::endl;
        prediction.prediction_confidence = 0.0;
    }

    return prediction;
}

std::vector<CorrelationPrediction> CorrelationMLEnhancer::predictAllCorrelations(int horizon_hours) {
    std::vector<CorrelationPrediction> predictions;

    if (!correlation_monitor_) return predictions;

    auto monitored_pairs = correlation_monitor_->getMonitoredPairs();

    for (const auto& pair : monitored_pairs) {
        auto prediction = predictCorrelationChange(
            pair.crypto_symbol, pair.traditional_symbol, horizon_hours);
        if (prediction.prediction_confidence > 0.0) {
            predictions.push_back(prediction);
        }
    }

    return predictions;
}

CorrelationRegimeResult CorrelationMLEnhancer::analyzeCurrentRegime() {
    if (!correlation_monitor_) {
        CorrelationRegimeResult result;
        result.current_regime = CorrelationRegime::MIXED_SIGNALS;
        return result;
    }

    auto current_correlations = correlation_monitor_->getCurrentCorrelations();
    return regime_detector_->detectCurrentRegime(current_correlations);
}

CorrelationRegimeResult CorrelationMLEnhancer::forecastRegimeChange(int horizon_hours) {
    if (!correlation_monitor_) {
        CorrelationRegimeResult result;
        result.current_regime = CorrelationRegime::MIXED_SIGNALS;
        result.predicted_regime = CorrelationRegime::MIXED_SIGNALS;
        return result;
    }

    // Get historical correlations for all pairs
    auto monitored_pairs = correlation_monitor_->getMonitoredPairs();
    std::vector<Analytics::CorrelationSnapshot> all_history;

    for (const auto& pair : monitored_pairs) {
        auto pair_history = correlation_monitor_->getCorrelationHistory(pair, default_lookback_days_);
        all_history.insert(all_history.end(), pair_history.begin(), pair_history.end());
    }

    return regime_detector_->predictRegimeChange(all_history);
}

CorrelationMLFeatures CorrelationMLEnhancer::createCorrelationFeatures(
    const std::string& crypto_symbol,
    const std::vector<Analytics::CorrelationSnapshot>& correlation_history) {

    CorrelationMLFeatures features;
    features.symbol = crypto_symbol;
    features.timestamp = std::chrono::system_clock::now();

    if (correlation_history.empty()) return features;

    // Extract cross-asset correlations
    for (const auto& snapshot : correlation_history) {
        if (snapshot.pair.crypto_symbol == crypto_symbol) {
            if (snapshot.pair.traditional_symbol == "SPX") {
                features.btc_spx_correlation = snapshot.correlation;
            } else if (snapshot.pair.traditional_symbol == "GLD") {
                features.btc_gold_correlation = snapshot.correlation;
            } else if (snapshot.pair.traditional_symbol == "DXY") {
                features.btc_dxy_correlation = snapshot.correlation;
            }
        }
        if (snapshot.pair.crypto_symbol == "ETH" && snapshot.pair.traditional_symbol == "SPX") {
            features.eth_spx_correlation = snapshot.correlation;
        }
    }

    // Calculate correlation dynamics
    features.correlation_momentum_3d = calculateCorrelationMomentum(correlation_history, 3);
    features.correlation_momentum_7d = calculateCorrelationMomentum(correlation_history, 7);
    features.correlation_volatility = calculateCorrelationVolatility(correlation_history, 10);

    // Calculate z-score for most recent correlation
    if (!correlation_history.empty()) {
        auto correlation_values = CorrelationMLUtils::extractCorrelationTimeSeries(correlation_history);
        if (correlation_values.size() > 1) {
            double mean = std::accumulate(correlation_values.begin(), correlation_values.end(), 0.0) / correlation_values.size();
            double variance = 0.0;
            for (double val : correlation_values) {
                variance += (val - mean) * (val - mean);
            }
            variance /= (correlation_values.size() - 1);
            double std_dev = std::sqrt(variance);
            features.correlation_z_score = std_dev > 0 ? (correlation_values.back() - mean) / std_dev : 0.0;
        }
    }

    // Market regime features (would be calculated from broader market data)
    features.market_stress_level = calculateMarketStressLevel(correlation_history);
    features.vix_level = 20.0; // Would fetch from traditional asset data
    features.flight_to_quality_score = calculateFlightToQualityScore(correlation_history);
    features.risk_appetite_index = calculateRiskAppetiteIndex(correlation_history);

    return features;
}

CorrelationMLFeatures CorrelationMLEnhancer::createMarketRegimeFeatures(
    const std::vector<Analytics::CorrelationSnapshot>& all_correlations) {

    CorrelationMLFeatures features;
    features.symbol = "MARKET";
    features.timestamp = std::chrono::system_clock::now();

    if (all_correlations.empty()) return features;

    // Calculate market-wide features
    features.market_stress_level = calculateMarketStressLevel(all_correlations);
    features.flight_to_quality_score = calculateFlightToQualityScore(all_correlations);
    features.risk_appetite_index = calculateRiskAppetiteIndex(all_correlations);

    // Extract key cross-asset correlations
    for (const auto& snapshot : all_correlations) {
        if (snapshot.pair.crypto_symbol == "BTC" && snapshot.pair.traditional_symbol == "SPX") {
            features.btc_spx_correlation = snapshot.correlation;
        }
        // Add other key correlations...
    }

    return features;
}

bool CorrelationMLEnhancer::trainCorrelationModel(
    const std::vector<CorrelationMLFeatures>& training_features,
    const std::vector<double>& correlation_targets) {

    if (training_features.size() != correlation_targets.size() || training_features.empty()) {
        return false;
    }

    // Convert to standard MLFeatureVector format
    std::vector<MLFeatureVector> standard_features;
    for (const auto& corr_feature : training_features) {
        standard_features.push_back(static_cast<MLFeatureVector>(corr_feature));
    }

    bool success = correlation_predictor_->trainModel(
        standard_features, correlation_targets, "correlation_" + model_version_);

    correlation_model_trained_ = success;
    return success;
}

bool CorrelationMLEnhancer::trainRegimeModel(
    const std::vector<CorrelationMLFeatures>& training_features,
    const std::vector<CorrelationRegime>& regime_labels) {

    bool success = regime_detector_->trainRegimeModel(training_features, regime_labels);
    regime_model_trained_ = success;
    return success;
}

MLPrediction CorrelationMLEnhancer::getMLEnhancedPrediction(
    const std::string& symbol, const CorrelationMLFeatures& enhanced_features) {

    if (correlation_model_trained_) {
        return correlation_predictor_->predict(enhanced_features);
    } else {
        MLPrediction result;
        result.symbol = symbol;
        result.predicted_return = 0.0;
        result.confidence_score = 0.0;
        return result;
    }
}

CorrelationMLEnhancer::CorrelationModelMetrics CorrelationMLEnhancer::getModelMetrics() const {
    return model_metrics_;
}

double CorrelationMLEnhancer::getCorrelationRiskScore() const {
    if (!correlation_monitor_) return 0.0;

    double current_stress = correlation_monitor_->getCurrentMarketStress();
    double risk_adjustment = correlation_monitor_->getPortfolioRiskAdjustment();

    return (current_stress + (risk_adjustment - 1.0)) / 2.0; // Normalize to 0-1
}

std::vector<std::string> CorrelationMLEnhancer::getCorrelationBasedRecommendations() const {
    if (!correlation_monitor_) {
        return {"Correlation monitor not available"};
    }

    return correlation_monitor_->getRiskRecommendations();
}

double CorrelationMLEnhancer::calculateCorrelationMomentum(
    const std::vector<Analytics::CorrelationSnapshot>& history, int days) {

    if (history.size() < days + 1) return 0.0;

    auto recent = history.end() - 1;
    auto past = history.end() - days - 1;

    return recent->correlation - past->correlation;
}

double CorrelationMLEnhancer::calculateCorrelationVolatility(
    const std::vector<Analytics::CorrelationSnapshot>& history, int window) {

    if (history.size() < window || window < 2) return 0.0;

    auto recent_history = std::vector<Analytics::CorrelationSnapshot>(
        history.end() - window, history.end());

    double mean = 0.0;
    for (const auto& snapshot : recent_history) {
        mean += snapshot.correlation;
    }
    mean /= recent_history.size();

    double variance = 0.0;
    for (const auto& snapshot : recent_history) {
        variance += (snapshot.correlation - mean) * (snapshot.correlation - mean);
    }
    variance /= (recent_history.size() - 1);

    return std::sqrt(variance);
}

double CorrelationMLEnhancer::calculateMarketStressLevel(
    const std::vector<Analytics::CorrelationSnapshot>& all_correlations) {

    if (all_correlations.empty()) return 0.0;

    double stress_sum = 0.0;
    int stress_count = 0;

    for (const auto& correlation : all_correlations) {
        if (correlation.correlation_spike || correlation.regime_change) {
            stress_sum += std::abs(correlation.z_score) / 5.0;
            stress_count++;
        }
    }

    return stress_count > 0 ? std::min(1.0, stress_sum / stress_count) : 0.0;
}

double CorrelationMLEnhancer::calculateFlightToQualityScore(
    const std::vector<Analytics::CorrelationSnapshot>& correlations) {

    double score = 0.0;
    int gold_correlations = 0;

    for (const auto& correlation : correlations) {
        if (correlation.pair.traditional_symbol == "GLD") {
            // Higher gold correlation during flight to quality
            score += std::max(0.0, correlation.correlation);
            gold_correlations++;
        }
    }

    return gold_correlations > 0 ? score / gold_correlations : 0.0;
}

double CorrelationMLEnhancer::calculateRiskAppetiteIndex(
    const std::vector<Analytics::CorrelationSnapshot>& correlations) {

    double positive_correlations = 0.0;
    int spx_correlations = 0;

    for (const auto& correlation : correlations) {
        if (correlation.pair.traditional_symbol == "SPX") {
            // Higher SPX correlation indicates higher risk appetite
            positive_correlations += std::max(0.0, correlation.correlation);
            spx_correlations++;
        }
    }

    return spx_correlations > 0 ? positive_correlations / spx_correlations : 0.0;
}

CorrelationPrediction CorrelationMLEnhancer::createPredictionResult(
    const std::string& asset_pair,
    const MLPrediction& ml_result,
    const Analytics::CorrelationSnapshot& current_correlation) {

    CorrelationPrediction prediction;
    prediction.asset_pair = asset_pair;
    prediction.current_correlation = current_correlation.correlation;
    prediction.predicted_correlation = current_correlation.correlation + ml_result.predicted_return;
    prediction.correlation_change_forecast = ml_result.predicted_return;
    prediction.prediction_confidence = ml_result.confidence_score;
    prediction.prediction_time = std::chrono::system_clock::now();
    prediction.model_version = model_version_;

    return prediction;
}

bool CorrelationMLEnhancer::validateCorrelationModel() const {
    return correlation_model_trained_ && correlation_predictor_ != nullptr;
}

bool CorrelationMLEnhancer::validateRegimeModel() const {
    return regime_model_trained_ && regime_detector_ != nullptr;
}

void CorrelationMLEnhancer::updateCorrelationCache() {
    if (!correlation_monitor_) return;

    recent_correlations_ = correlation_monitor_->getCurrentCorrelations();
}

void CorrelationMLEnhancer::cleanupStaleCache() {
    // Remove correlations older than 24 hours from cache
    auto now = std::chrono::system_clock::now();
    auto cutoff = now - std::chrono::hours(24);

    for (auto& pair : correlation_history_cache_) {
        // Clean up individual pair histories (implementation would be more detailed)
    }
}

// CorrelationSignalGenerator Implementation
CorrelationSignalGenerator::CorrelationSignalGenerator(
    const std::shared_ptr<CorrelationMLEnhancer>& ml_enhancer)
    : ml_enhancer_(ml_enhancer)
    , correlation_change_threshold_(0.2)
    , regime_change_threshold_(0.7)
    , stress_threshold_(0.6) {
}

CorrelationSignalGenerator::~CorrelationSignalGenerator() = default;

std::vector<CorrelationSignalGenerator::CorrelationSignal>
CorrelationSignalGenerator::generateCorrelationSignals() {

    std::vector<CorrelationSignal> signals;

    if (!ml_enhancer_) return signals;

    auto predictions = ml_enhancer_->predictAllCorrelations(24);

    for (const auto& prediction : predictions) {
        if (std::abs(prediction.correlation_change_forecast) > correlation_change_threshold_) {
            CorrelationSignal signal = createSignal(
                "correlation_change",
                prediction.asset_pair,
                prediction.prediction_confidence,
                prediction.correlation_change_forecast > 0 ? "increasing" : "decreasing"
            );
            signal.signal_horizon_hours = prediction.prediction_horizon_hours;
            signals.push_back(signal);
        }
    }

    return signals;
}

std::vector<CorrelationSignalGenerator::CorrelationSignal>
CorrelationSignalGenerator::generateRegimeChangeSignals() {

    std::vector<CorrelationSignal> signals;

    if (!ml_enhancer_) return signals;

    auto regime_result = ml_enhancer_->forecastRegimeChange(72);

    if (regime_result.current_regime != regime_result.predicted_regime &&
        regime_result.regime_confidence > regime_change_threshold_) {

        CorrelationSignal signal = createSignal(
            "regime_change",
            "MARKET",
            regime_result.regime_confidence,
            "neutral"
        );
        signal.signal_horizon_hours = 72;
        signal.recommendations = regime_result.regime_indicators;
        signals.push_back(signal);
    }

    return signals;
}

std::vector<CorrelationSignalGenerator::CorrelationSignal>
CorrelationSignalGenerator::generateRiskAdjustmentSignals() {

    std::vector<CorrelationSignal> signals;

    if (!ml_enhancer_) return signals;

    double risk_score = ml_enhancer_->getCorrelationRiskScore();

    if (risk_score > stress_threshold_) {
        CorrelationSignal signal = createSignal(
            "risk_adjustment",
            "PORTFOLIO",
            risk_score,
            "bearish"
        );
        signal.recommendations = ml_enhancer_->getCorrelationBasedRecommendations();
        signals.push_back(signal);
    }

    return signals;
}

std::map<std::string, double> CorrelationSignalGenerator::getCorrelationBasedWeights(
    const std::vector<std::string>& portfolio_assets) {

    // Simple equal weight for now - would implement correlation-based optimization
    std::map<std::string, double> weights;
    double equal_weight = 1.0 / portfolio_assets.size();

    for (const std::string& asset : portfolio_assets) {
        weights[asset] = equal_weight;
    }

    return weights;
}

double CorrelationSignalGenerator::getRecommendedLeverage() const {
    if (!ml_enhancer_) return 1.0;

    double risk_score = ml_enhancer_->getCorrelationRiskScore();
    return std::max(0.5, 1.0 - risk_score); // Reduce leverage as risk increases
}

std::vector<std::string> CorrelationSignalGenerator::getHedgingRecommendations() const {
    if (!ml_enhancer_) return {};

    return ml_enhancer_->getCorrelationBasedRecommendations();
}

CorrelationSignalGenerator::CorrelationSignal CorrelationSignalGenerator::createSignal(
    const std::string& type, const std::string& assets, double strength, const std::string& direction) {

    CorrelationSignal signal;
    signal.signal_type = type;
    signal.affected_assets = assets;
    signal.signal_strength = strength;
    signal.direction = direction;
    signal.signal_time = std::chrono::system_clock::now();
    signal.signal_horizon_hours = 24;

    return signal;
}

// CorrelationMLUtils Implementation
namespace CorrelationMLUtils {

std::vector<double> calculateRollingCorrelation(
    const std::vector<double>& x, const std::vector<double>& y, int window) {

    std::vector<double> rolling_correlations;

    if (x.size() != y.size() || x.size() < window) return rolling_correlations;

    for (size_t i = window - 1; i < x.size(); ++i) {
        std::vector<double> x_window(x.begin() + i - window + 1, x.begin() + i + 1);
        std::vector<double> y_window(y.begin() + i - window + 1, y.begin() + i + 1);

        double correlation = Analytics::StatisticalTools::calculateCorrelation(x_window, y_window);
        rolling_correlations.push_back(correlation);
    }

    return rolling_correlations;
}

double calculateCorrelationDivergence(const std::vector<Analytics::CorrelationSnapshot>& correlations) {
    if (correlations.size() < 2) return 0.0;

    // Calculate divergence between current and historical average correlation
    double current_avg = 0.0;
    double historical_avg = 0.0;
    int recent_days = 5;

    for (size_t i = 0; i < std::min(recent_days, static_cast<int>(correlations.size())); ++i) {
        current_avg += correlations[correlations.size() - 1 - i].correlation;
    }
    current_avg /= std::min(recent_days, static_cast<int>(correlations.size()));

    for (const auto& correlation : correlations) {
        historical_avg += correlation.correlation;
    }
    historical_avg /= correlations.size();

    return std::abs(current_avg - historical_avg);
}

std::vector<double> extractCorrelationTimeSeries(
    const std::vector<Analytics::CorrelationSnapshot>& snapshots) {

    std::vector<double> time_series;
    time_series.reserve(snapshots.size());

    for (const auto& snapshot : snapshots) {
        time_series.push_back(snapshot.correlation);
    }

    return time_series;
}

std::string regimeToString(CorrelationRegime regime) {
    switch (regime) {
        case CorrelationRegime::NORMAL: return "Normal";
        case CorrelationRegime::RISK_OFF: return "Risk Off";
        case CorrelationRegime::DECOUPLING: return "Decoupling";
        case CorrelationRegime::FLIGHT_TO_QUALITY: return "Flight to Quality";
        case CorrelationRegime::RISK_ON: return "Risk On";
        case CorrelationRegime::MIXED_SIGNALS: return "Mixed Signals";
        default: return "Unknown";
    }
}

CorrelationRegime stringToRegime(const std::string& regime_str) {
    if (regime_str == "Normal") return CorrelationRegime::NORMAL;
    if (regime_str == "Risk Off") return CorrelationRegime::RISK_OFF;
    if (regime_str == "Decoupling") return CorrelationRegime::DECOUPLING;
    if (regime_str == "Flight to Quality") return CorrelationRegime::FLIGHT_TO_QUALITY;
    if (regime_str == "Risk On") return CorrelationRegime::RISK_ON;
    return CorrelationRegime::MIXED_SIGNALS;
}

double calculateCorrelationPredictionAccuracy(
    const std::vector<double>& predicted, const std::vector<double>& actual) {

    if (predicted.size() != actual.size() || predicted.empty()) return 0.0;

    double total_error = 0.0;
    for (size_t i = 0; i < predicted.size(); ++i) {
        total_error += std::abs(predicted[i] - actual[i]);
    }

    double mean_absolute_error = total_error / predicted.size();
    return std::max(0.0, 1.0 - mean_absolute_error); // Convert to accuracy score
}

double calculateRegimeClassificationAccuracy(
    const std::vector<CorrelationRegime>& predicted,
    const std::vector<CorrelationRegime>& actual) {

    if (predicted.size() != actual.size() || predicted.empty()) return 0.0;

    int correct = 0;
    for (size_t i = 0; i < predicted.size(); ++i) {
        if (predicted[i] == actual[i]) correct++;
    }

    return static_cast<double>(correct) / predicted.size();
}

double calculateCorrelationVaR(
    const std::vector<Analytics::CorrelationSnapshot>& correlations, double confidence_level) {

    if (correlations.empty()) return 0.0;

    std::vector<double> correlation_changes;
    for (size_t i = 1; i < correlations.size(); ++i) {
        correlation_changes.push_back(
            correlations[i].correlation - correlations[i-1].correlation);
    }

    if (correlation_changes.empty()) return 0.0;

    std::sort(correlation_changes.begin(), correlation_changes.end());

    int var_index = static_cast<int>((1.0 - confidence_level) * correlation_changes.size());
    var_index = std::max(0, std::min(var_index, static_cast<int>(correlation_changes.size() - 1)));

    return correlation_changes[var_index];
}

std::map<std::string, double> calculateCorrelationRiskContributions(
    const std::vector<Analytics::CorrelationSnapshot>& correlations,
    const std::vector<std::string>& portfolio_assets) {

    std::map<std::string, double> risk_contributions;

    // Initialize with equal risk contribution
    double equal_contribution = 1.0 / portfolio_assets.size();
    for (const std::string& asset : portfolio_assets) {
        risk_contributions[asset] = equal_contribution;
    }

    // Adjust based on correlation risk
    for (const auto& correlation : correlations) {
        auto it = std::find(portfolio_assets.begin(), portfolio_assets.end(),
                          correlation.pair.crypto_symbol);
        if (it != portfolio_assets.end()) {
            // Higher correlation = higher risk contribution
            double correlation_risk = std::abs(correlation.correlation) *
                                    (correlation.correlation_spike ? 2.0 : 1.0);
            risk_contributions[correlation.pair.crypto_symbol] *= (1.0 + correlation_risk);
        }
    }

    // Normalize to sum to 1.0
    double total_contribution = 0.0;
    for (const auto& contribution : risk_contributions) {
        total_contribution += contribution.second;
    }

    if (total_contribution > 0) {
        for (auto& contribution : risk_contributions) {
            contribution.second /= total_contribution;
        }
    }

    return risk_contributions;
}

} // namespace CorrelationMLUtils

} // namespace ML
} // namespace CryptoClaude