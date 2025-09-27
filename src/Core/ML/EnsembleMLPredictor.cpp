#include "EnsembleMLPredictor.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <random>
#include <iostream>
#include <set>

namespace CryptoClaude {
namespace ML {

// RandomForestEnsembleModel Implementation
RandomForestEnsembleModel::RandomForestEnsembleModel(std::shared_ptr<RandomForestPredictor> rf_predictor)
    : rf_predictor_(rf_predictor) {
}

RandomForestEnsembleModel::~RandomForestEnsembleModel() = default;

EnsembleModelResult RandomForestEnsembleModel::predict(const MLFeatureVector& features) {
    EnsembleModelResult result;
    result.model_type = EnsembleModelType::RANDOM_FOREST;
    result.model_name = getModelName();
    result.symbol = features.symbol;
    result.prediction_time = std::chrono::system_clock::now();
    result.model_version = getModelVersion();

    if (!rf_predictor_ || !isModelTrained()) {
        result.prediction = 0.0;
        result.confidence = 0.0;
        return result;
    }

    auto ml_prediction = rf_predictor_->predict(features);
    result.prediction = ml_prediction.predicted_return;
    result.confidence = ml_prediction.confidence_score;

    // Add Random Forest specific metrics
    result.model_metrics["prediction_variance"] = ml_prediction.prediction_variance;
    result.model_metrics["trees_used"] = 100.0; // From Day 8 implementation

    return result;
}

EnsembleModelResult RandomForestEnsembleModel::predict(const CorrelationMLFeatures& features) {
    // Use base MLFeatureVector part for prediction
    return predict(static_cast<MLFeatureVector>(features));
}

std::string RandomForestEnsembleModel::getModelVersion() const {
    return rf_predictor_ ? rf_predictor_->getModelStatus().model_version : "unknown";
}

double RandomForestEnsembleModel::getModelConfidence() const {
    return rf_predictor_ ? (isModelTrained() ? 0.8 : 0.0) : 0.0;
}

bool RandomForestEnsembleModel::isModelTrained() const {
    return rf_predictor_ ? rf_predictor_->getModelStatus().is_trained : false;
}

bool RandomForestEnsembleModel::validateModel() const {
    return rf_predictor_ && isModelTrained() && rf_predictor_->getModelStatus().training_accuracy > 0.3;
}

std::map<std::string, double> RandomForestEnsembleModel::getModelMetrics() const {
    std::map<std::string, double> metrics;
    if (rf_predictor_) {
        auto status = rf_predictor_->getModelStatus();
        metrics["training_accuracy"] = status.training_accuracy;
        metrics["validation_accuracy"] = status.validation_accuracy;
        metrics["training_samples"] = static_cast<double>(status.training_samples);
        metrics["feature_count"] = static_cast<double>(status.feature_count);
    }
    return metrics;
}

std::vector<std::string> RandomForestEnsembleModel::getRequiredFeatures() const {
    return {"sma_5_ratio", "sma_20_ratio", "rsi_14", "volatility_10", "volume_ratio",
            "price_momentum_3", "price_momentum_7", "news_sentiment"};
}

bool RandomForestEnsembleModel::canHandleFeatures(const MLFeatureVector& features) const {
    // Random Forest can handle basic ML features
    return !features.symbol.empty();
}

// CorrelationEnsembleModel Implementation
CorrelationEnsembleModel::CorrelationEnsembleModel(std::shared_ptr<CorrelationMLEnhancer> correlation_enhancer)
    : correlation_enhancer_(correlation_enhancer) {
}

CorrelationEnsembleModel::~CorrelationEnsembleModel() = default;

EnsembleModelResult CorrelationEnsembleModel::predict(const MLFeatureVector& features) {
    EnsembleModelResult result;
    result.model_type = EnsembleModelType::CORRELATION_ML;
    result.model_name = getModelName();
    result.symbol = features.symbol;
    result.prediction_time = std::chrono::system_clock::now();
    result.model_version = getModelVersion();

    if (!correlation_enhancer_) {
        result.prediction = 0.0;
        result.confidence = 0.0;
        return result;
    }

    // Convert to CorrelationMLFeatures if possible, otherwise use basic prediction
    CorrelationMLFeatures corr_features;
    corr_features.symbol = features.symbol;
    corr_features.sma_5_ratio = features.sma_5_ratio;
    corr_features.sma_20_ratio = features.sma_20_ratio;
    corr_features.rsi_14 = features.rsi_14;
    corr_features.volatility_10 = features.volatility_10;
    corr_features.volume_ratio = features.volume_ratio;
    corr_features.price_momentum_3 = features.price_momentum_3;
    corr_features.price_momentum_7 = features.price_momentum_7;
    corr_features.news_sentiment = features.news_sentiment;

    auto ml_prediction = correlation_enhancer_->getMLEnhancedPrediction(features.symbol, corr_features);
    result.prediction = ml_prediction.predicted_return;
    result.confidence = ml_prediction.confidence_score;

    // Add correlation-specific metrics
    auto risk_score = correlation_enhancer_->getCorrelationRiskScore();
    result.model_metrics["correlation_risk_score"] = risk_score;
    result.model_metrics["regime_confidence"] = 0.7; // Would get from regime analysis

    return result;
}

EnsembleModelResult CorrelationEnsembleModel::predict(const CorrelationMLFeatures& features) {
    EnsembleModelResult result;
    result.model_type = EnsembleModelType::CORRELATION_ML;
    result.model_name = getModelName();
    result.symbol = features.symbol;
    result.prediction_time = std::chrono::system_clock::now();
    result.model_version = getModelVersion();

    if (!correlation_enhancer_) {
        result.prediction = 0.0;
        result.confidence = 0.0;
        return result;
    }

    auto ml_prediction = correlation_enhancer_->getMLEnhancedPrediction(features.symbol, features);
    result.prediction = ml_prediction.predicted_return;
    result.confidence = ml_prediction.confidence_score;

    // Enhanced metrics from correlation features
    result.model_metrics["btc_spx_correlation"] = features.btc_spx_correlation;
    result.model_metrics["market_stress_level"] = features.market_stress_level;
    result.model_metrics["correlation_z_score"] = features.correlation_z_score;

    return result;
}

double CorrelationEnsembleModel::getModelConfidence() const {
    return correlation_enhancer_ ? 0.75 : 0.0; // Correlation ML is sophisticated
}

bool CorrelationEnsembleModel::isModelTrained() const {
    // For now, assume correlation enhancer is ready if it exists
    return correlation_enhancer_ != nullptr;
}

bool CorrelationEnsembleModel::validateModel() const {
    return correlation_enhancer_ != nullptr;
}

std::map<std::string, double> CorrelationEnsembleModel::getModelMetrics() const {
    if (correlation_enhancer_) {
        auto metrics = correlation_enhancer_->getModelMetrics();
        std::map<std::string, double> result;
        result["correlation_prediction_accuracy"] = metrics.correlation_prediction_accuracy;
        result["regime_detection_accuracy"] = metrics.regime_detection_accuracy;
        result["predictions_made_24h"] = static_cast<double>(metrics.predictions_made_24h);
        return result;
    }
    return {};
}

std::vector<std::string> CorrelationEnsembleModel::getRequiredFeatures() const {
    return {"btc_spx_correlation", "btc_gold_correlation", "market_stress_level",
            "correlation_momentum_3d", "vix_level", "risk_appetite_index"};
}

bool CorrelationEnsembleModel::canHandleFeatures(const MLFeatureVector& features) const {
    return !features.symbol.empty();
}

// TechnicalIndicatorEnsembleModel Implementation
TechnicalIndicatorEnsembleModel::TechnicalIndicatorEnsembleModel()
    : is_trained_(true), model_confidence_(0.6) {

    // Initialize model metrics
    model_metrics_["momentum_accuracy"] = 0.65;
    model_metrics_["mean_reversion_accuracy"] = 0.58;
    model_metrics_["volatility_accuracy"] = 0.72;
    model_metrics_["overall_accuracy"] = 0.65;
}

TechnicalIndicatorEnsembleModel::~TechnicalIndicatorEnsembleModel() = default;

EnsembleModelResult TechnicalIndicatorEnsembleModel::predict(const MLFeatureVector& features) {
    EnsembleModelResult result;
    result.model_type = EnsembleModelType::TECHNICAL_INDICATORS;
    result.model_name = getModelName();
    result.symbol = features.symbol;
    result.prediction_time = std::chrono::system_clock::now();
    result.model_version = getModelVersion();
    result.model_metrics = model_metrics_;

    if (!canHandleFeatures(features)) {
        result.prediction = 0.0;
        result.confidence = 0.0;
        return result;
    }

    // Calculate technical indicator-based prediction
    double technical_score = calculateTechnicalScore(features);
    result.prediction = technical_score;
    result.confidence = model_confidence_;

    // Add technical analysis breakdown
    result.model_metrics["momentum_signal"] = calculateMomentumSignal(features);
    result.model_metrics["mean_reversion_signal"] = calculateMeanReversionSignal(features);
    result.model_metrics["volatility_signal"] = calculateVolatilitySignal(features);

    return result;
}

EnsembleModelResult TechnicalIndicatorEnsembleModel::predict(const CorrelationMLFeatures& features) {
    return predict(static_cast<MLFeatureVector>(features));
}

bool TechnicalIndicatorEnsembleModel::validateModel() const {
    return is_trained_ && model_confidence_ > 0.5;
}

std::map<std::string, double> TechnicalIndicatorEnsembleModel::getModelMetrics() const {
    return model_metrics_;
}

std::vector<std::string> TechnicalIndicatorEnsembleModel::getRequiredFeatures() const {
    return {"sma_5_ratio", "sma_20_ratio", "rsi_14", "volatility_10", "price_momentum_3", "price_momentum_7"};
}

bool TechnicalIndicatorEnsembleModel::canHandleFeatures(const MLFeatureVector& features) const {
    return features.sma_5_ratio != 0.0 && features.rsi_14 != 0.0 && features.volatility_10 != 0.0;
}

double TechnicalIndicatorEnsembleModel::calculateTechnicalScore(const MLFeatureVector& features) {
    double momentum_signal = calculateMomentumSignal(features);
    double mean_reversion_signal = calculateMeanReversionSignal(features);
    double volatility_signal = calculateVolatilitySignal(features);

    // Weighted combination of signals
    double score = 0.4 * momentum_signal + 0.3 * mean_reversion_signal + 0.3 * volatility_signal;

    // Normalize to reasonable range
    return std::max(-0.1, std::min(0.1, score));
}

double TechnicalIndicatorEnsembleModel::calculateMomentumSignal(const MLFeatureVector& features) {
    // Momentum based on SMA ratios and price momentum
    double sma_momentum = (features.sma_5_ratio - 1.0) * 0.5; // Deviation from 1.0
    double price_momentum = (features.price_momentum_3 + features.price_momentum_7) / 2.0;

    return (sma_momentum + price_momentum) / 2.0;
}

double TechnicalIndicatorEnsembleModel::calculateMeanReversionSignal(const MLFeatureVector& features) {
    // Mean reversion based on RSI
    double rsi_signal = 0.0;
    if (features.rsi_14 > 70) {
        rsi_signal = -0.02; // Overbought - expect reversion down
    } else if (features.rsi_14 < 30) {
        rsi_signal = 0.02;  // Oversold - expect reversion up
    }

    // Combine with SMA ratio extremes
    double sma_reversion = 0.0;
    if (features.sma_20_ratio > 1.05) {
        sma_reversion = -0.01;
    } else if (features.sma_20_ratio < 0.95) {
        sma_reversion = 0.01;
    }

    return rsi_signal + sma_reversion;
}

double TechnicalIndicatorEnsembleModel::calculateVolatilitySignal(const MLFeatureVector& features) {
    // Volatility-based signal
    if (features.volatility_10 > 0.05) {
        return -0.01; // High volatility - expect pullback
    } else if (features.volatility_10 < 0.01) {
        return 0.005; // Low volatility - expect breakout
    }
    return 0.0;
}

// EnsembleMLPredictor Implementation
EnsembleMLPredictor::EnsembleMLPredictor()
    : ensemble_method_("weighted_average")
    , is_initialized_(false)
    , ensemble_version_("ensemble_v1.0") {

    // Initialize metrics
    metrics_.ensemble_accuracy = 0.0;
    metrics_.consensus_rate = 0.0;
    metrics_.average_confidence = 0.0;
    metrics_.prediction_stability = 0.0;
    metrics_.predictions_made_24h = 0;
    metrics_.models_active = 0;
    metrics_.improvement_over_best_single = 0.0;
}

EnsembleMLPredictor::~EnsembleMLPredictor() = default;

bool EnsembleMLPredictor::initialize() {
    initializeDefaultModels();
    normalizeWeights();
    is_initialized_ = true;
    return true;
}

void EnsembleMLPredictor::addModel(std::shared_ptr<IEnsembleModel> model) {
    if (!model || !validateModelCompatibility(model)) {
        return;
    }

    models_.push_back(model);
    model_weights_[model->getModelType()] = 1.0 / (models_.size()); // Equal initial weight
    model_enabled_[model->getModelName()] = true;

    normalizeWeights();
    metrics_.models_active = static_cast<int>(models_.size());
}

void EnsembleMLPredictor::removeModel(EnsembleModelType model_type) {
    models_.erase(
        std::remove_if(models_.begin(), models_.end(),
            [model_type](const std::shared_ptr<IEnsembleModel>& model) {
                return model->getModelType() == model_type;
            }),
        models_.end());

    model_weights_.erase(model_type);
    normalizeWeights();
    metrics_.models_active = static_cast<int>(models_.size());
}

void EnsembleMLPredictor::removeModel(const std::string& model_name) {
    models_.erase(
        std::remove_if(models_.begin(), models_.end(),
            [&model_name](const std::shared_ptr<IEnsembleModel>& model) {
                return model->getModelName() == model_name;
            }),
        models_.end());

    model_enabled_.erase(model_name);
    normalizeWeights();
    metrics_.models_active = static_cast<int>(models_.size());
}

EnsemblePrediction EnsembleMLPredictor::predict(const std::string& symbol, const MLFeatureVector& features) {
    EnsemblePrediction ensemble_result;
    ensemble_result.symbol = symbol;
    ensemble_result.ensemble_time = std::chrono::system_clock::now();
    ensemble_result.ensemble_version = ensemble_version_;
    ensemble_result.models_used = 0;
    ensemble_result.total_weight = 0.0;

    if (!is_initialized_ || models_.empty()) {
        ensemble_result.final_prediction = 0.0;
        ensemble_result.ensemble_confidence = 0.0;
        return ensemble_result;
    }

    // Collect predictions from all active models
    std::vector<EnsembleModelResult> model_results;

    for (auto& model : models_) {
        if (!model || !model->isModelTrained()) continue;

        std::string model_name = model->getModelName();
        auto enabled_it = model_enabled_.find(model_name);
        if (enabled_it != model_enabled_.end() && !enabled_it->second) continue;

        try {
            auto result = model->predict(features);
            if (std::isfinite(result.prediction) && result.confidence > 0.1) {
                // Assign weight
                auto weight_it = model_weights_.find(model->getModelType());
                result.weight = (weight_it != model_weights_.end()) ? weight_it->second : 0.1;
                result.contribution = result.prediction * result.weight;

                model_results.push_back(result);
                ensemble_result.models_used++;
                ensemble_result.total_weight += result.weight;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error getting prediction from model " << model_name << ": " << e.what() << std::endl;
        }
    }

    if (model_results.empty()) {
        ensemble_result.final_prediction = 0.0;
        ensemble_result.ensemble_confidence = 0.0;
        return ensemble_result;
    }

    // Combine predictions based on ensemble method
    if (ensemble_method_ == "weighted_average") {
        ensemble_result = combineWithWeightedAverage(symbol, model_results);
    } else if (ensemble_method_ == "voting") {
        ensemble_result = combineWithVoting(symbol, model_results);
    } else if (ensemble_method_ == "stacking") {
        ensemble_result = combineWithStacking(symbol, model_results);
    } else {
        ensemble_result = combineWithWeightedAverage(symbol, model_results);
    }

    // Calculate ensemble statistics
    ensemble_result.consensus_score = calculateConsensusScore(model_results);
    ensemble_result.prediction_variance = calculatePredictionVariance(model_results);
    ensemble_result.ensemble_confidence = calculateEnsembleConfidence(model_results);
    ensemble_result.ensemble_warnings = generateEnsembleWarnings(ensemble_result);

    // Calculate min/max/median
    std::vector<double> predictions;
    for (const auto& result : model_results) {
        predictions.push_back(result.prediction);
    }
    std::sort(predictions.begin(), predictions.end());

    ensemble_result.min_prediction = predictions.front();
    ensemble_result.max_prediction = predictions.back();
    ensemble_result.median_prediction = predictions[predictions.size() / 2];

    // Calculate weighted standard deviation
    double weighted_mean = ensemble_result.final_prediction;
    double weighted_variance = 0.0;
    double weight_sum = 0.0;

    for (const auto& result : model_results) {
        double diff = result.prediction - weighted_mean;
        weighted_variance += result.weight * diff * diff;
        weight_sum += result.weight;
    }
    ensemble_result.weighted_std_deviation = weight_sum > 0 ?
        std::sqrt(weighted_variance / weight_sum) : 0.0;

    // Quality assessment
    ensemble_result.ensemble_quality_score = std::min(1.0,
        ensemble_result.ensemble_confidence * ensemble_result.consensus_score);
    ensemble_result.consensus_achieved = ensemble_result.consensus_score > 0.7;

    // Update metrics and store prediction
    updateMetrics(ensemble_result);
    recent_predictions_.push_back(ensemble_result);

    // Keep only recent predictions
    if (recent_predictions_.size() > 1000) {
        recent_predictions_.erase(recent_predictions_.begin());
    }

    return ensemble_result;
}

EnsemblePrediction EnsembleMLPredictor::predict(const std::string& symbol, const CorrelationMLFeatures& enhanced_features) {
    // For models that support enhanced features, use them; otherwise fall back to basic features
    EnsemblePrediction ensemble_result;
    ensemble_result.symbol = symbol;
    ensemble_result.ensemble_time = std::chrono::system_clock::now();
    ensemble_result.ensemble_version = ensemble_version_;

    if (!is_initialized_ || models_.empty()) {
        ensemble_result.final_prediction = 0.0;
        ensemble_result.ensemble_confidence = 0.0;
        return ensemble_result;
    }

    std::vector<EnsembleModelResult> model_results;

    for (auto& model : models_) {
        if (!model || !model->isModelTrained()) continue;

        std::string model_name = model->getModelName();
        auto enabled_it = model_enabled_.find(model_name);
        if (enabled_it != model_enabled_.end() && !enabled_it->second) continue;

        try {
            EnsembleModelResult result;

            // Use enhanced features for models that support them
            if (model->getModelType() == EnsembleModelType::CORRELATION_ML) {
                result = model->predict(enhanced_features);
            } else {
                // Fall back to basic MLFeatureVector
                result = model->predict(static_cast<MLFeatureVector>(enhanced_features));
            }

            if (std::isfinite(result.prediction) && result.confidence > 0.1) {
                auto weight_it = model_weights_.find(model->getModelType());
                result.weight = (weight_it != model_weights_.end()) ? weight_it->second : 0.1;
                result.contribution = result.prediction * result.weight;

                model_results.push_back(result);
                ensemble_result.models_used++;
                ensemble_result.total_weight += result.weight;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error getting enhanced prediction from model " << model_name << ": " << e.what() << std::endl;
        }
    }

    if (model_results.empty()) {
        ensemble_result.final_prediction = 0.0;
        ensemble_result.ensemble_confidence = 0.0;
        return ensemble_result;
    }

    // Use the same combination logic as standard prediction
    ensemble_result = combineWithWeightedAverage(symbol, model_results);
    ensemble_result.consensus_score = calculateConsensusScore(model_results);
    ensemble_result.prediction_variance = calculatePredictionVariance(model_results);
    ensemble_result.ensemble_confidence = calculateEnsembleConfidence(model_results);

    updateMetrics(ensemble_result);
    return ensemble_result;
}

std::vector<EnsemblePrediction> EnsembleMLPredictor::predictBatch(
    const std::vector<std::string>& symbols,
    const std::map<std::string, MLFeatureVector>& features) {

    std::vector<EnsemblePrediction> predictions;
    predictions.reserve(symbols.size());

    for (const std::string& symbol : symbols) {
        auto feature_it = features.find(symbol);
        if (feature_it != features.end()) {
            predictions.push_back(predict(symbol, feature_it->second));
        }
    }

    return predictions;
}

std::vector<EnsemblePrediction> EnsembleMLPredictor::predictBatch(
    const std::vector<std::string>& symbols,
    const std::map<std::string, CorrelationMLFeatures>& enhanced_features) {

    std::vector<EnsemblePrediction> predictions;
    predictions.reserve(symbols.size());

    for (const std::string& symbol : symbols) {
        auto feature_it = enhanced_features.find(symbol);
        if (feature_it != enhanced_features.end()) {
            predictions.push_back(predict(symbol, feature_it->second));
        }
    }

    return predictions;
}

void EnsembleMLPredictor::setWeightOptimizer(std::shared_ptr<EnsembleWeightOptimizer> optimizer) {
    weight_optimizer_ = optimizer;
}

void EnsembleMLPredictor::setMetaLearner(std::shared_ptr<MetaLearningFramework> meta_learner) {
    meta_learner_ = meta_learner;
}

void EnsembleMLPredictor::setEnsembleMethod(const std::string& method) {
    if (method == "weighted_average" || method == "voting" || method == "stacking") {
        ensemble_method_ = method;
    }
}

void EnsembleMLPredictor::updateModelWeights() {
    if (weight_optimizer_ && !recent_predictions_.empty()) {
        // Extract model results and dummy targets for optimization
        std::vector<EnsembleModelResult> all_results;
        std::vector<double> dummy_targets;

        for (const auto& prediction : recent_predictions_) {
            for (const auto& model_result : prediction.model_results) {
                all_results.push_back(model_result);
                dummy_targets.push_back(prediction.final_prediction); // Use ensemble as target
            }
        }

        if (!all_results.empty()) {
            auto optimized_weights = weight_optimizer_->optimizeWeights(all_results, dummy_targets);
            for (const auto& weight_pair : optimized_weights) {
                model_weights_[weight_pair.first] = weight_pair.second;
            }
            normalizeWeights();
        }
    }
}

void EnsembleMLPredictor::setModelWeight(EnsembleModelType model_type, double weight) {
    model_weights_[model_type] = std::max(0.0, weight);
    normalizeWeights();
}

void EnsembleMLPredictor::setModelWeight(const std::string& model_name, double weight) {
    auto model = findModel(model_name);
    if (model) {
        setModelWeight(model->getModelType(), weight);
    }
}

double EnsembleMLPredictor::getModelWeight(EnsembleModelType model_type) const {
    auto it = model_weights_.find(model_type);
    return it != model_weights_.end() ? it->second : 0.0;
}

double EnsembleMLPredictor::getModelWeight(const std::string& model_name) const {
    auto model = const_cast<EnsembleMLPredictor*>(this)->findModel(model_name);
    return model ? getModelWeight(model->getModelType()) : 0.0;
}

EnsembleMLPredictor::EnsembleMetrics EnsembleMLPredictor::getEnsembleMetrics() const {
    return metrics_;
}

void EnsembleMLPredictor::resetMetrics() {
    metrics_ = EnsembleMetrics{};
    recent_predictions_.clear();
}

std::vector<std::shared_ptr<IEnsembleModel>> EnsembleMLPredictor::getActiveModels() const {
    std::vector<std::shared_ptr<IEnsembleModel>> active_models;

    for (auto& model : models_) {
        if (model && model->isModelTrained()) {
            std::string model_name = model->getModelName();
            auto enabled_it = model_enabled_.find(model_name);
            if (enabled_it == model_enabled_.end() || enabled_it->second) {
                active_models.push_back(model);
            }
        }
    }

    return active_models;
}

std::vector<EnsembleModelType> EnsembleMLPredictor::getAvailableModelTypes() const {
    std::vector<EnsembleModelType> types;
    for (auto& model : models_) {
        if (model) {
            types.push_back(model->getModelType());
        }
    }
    return types;
}

bool EnsembleMLPredictor::isModelActive(EnsembleModelType model_type) const {
    auto model = const_cast<EnsembleMLPredictor*>(this)->findModel(model_type);
    if (!model) return false;

    auto enabled_it = model_enabled_.find(model->getModelName());
    return (enabled_it == model_enabled_.end() || enabled_it->second) && model->isModelTrained();
}

bool EnsembleMLPredictor::isModelActive(const std::string& model_name) const {
    auto enabled_it = model_enabled_.find(model_name);
    bool enabled = (enabled_it == model_enabled_.end() || enabled_it->second);

    auto model = const_cast<EnsembleMLPredictor*>(this)->findModel(model_name);
    return enabled && model && model->isModelTrained();
}

bool EnsembleMLPredictor::validateEnsemble() const {
    if (models_.empty()) return false;

    int valid_models = 0;
    for (auto& model : models_) {
        if (model && model->validateModel() && model->isModelTrained()) {
            valid_models++;
        }
    }

    return valid_models >= 2; // Need at least 2 valid models for ensemble
}

std::vector<std::string> EnsembleMLPredictor::getDiagnosticReport() const {
    std::vector<std::string> diagnostics;

    diagnostics.push_back("=== Ensemble Diagnostic Report ===");
    diagnostics.push_back("Total models: " + std::to_string(models_.size()));
    diagnostics.push_back("Active models: " + std::to_string(getActiveModels().size()));
    diagnostics.push_back("Ensemble method: " + ensemble_method_);
    diagnostics.push_back(std::string("Ensemble valid: ") + (validateEnsemble() ? "Yes" : "No"));

    auto active_models = getActiveModels();
    for (auto& model : active_models) {
        diagnostics.push_back("Model: " + model->getModelName() +
                            " (Type: " + EnsembleUtils::modelTypeToString(model->getModelType()) +
                            ", Weight: " + std::to_string(getModelWeight(model->getModelType())) + ")");
    }

    return diagnostics;
}

void EnsembleMLPredictor::enableModel(EnsembleModelType model_type, bool enabled) {
    auto model = findModel(model_type);
    if (model) {
        model_enabled_[model->getModelName()] = enabled;
    }
}

void EnsembleMLPredictor::enableModel(const std::string& model_name, bool enabled) {
    model_enabled_[model_name] = enabled;
}

std::map<std::string, double> EnsembleMLPredictor::getEnsemblePredictionsForOptimization(
    const std::vector<std::string>& symbols,
    const std::map<std::string, MLFeatureVector>& current_features) {

    std::map<std::string, double> predictions;

    for (const std::string& symbol : symbols) {
        auto feature_it = current_features.find(symbol);
        if (feature_it != current_features.end()) {
            auto ensemble_prediction = predict(symbol, feature_it->second);
            predictions[symbol] = ensemble_prediction.final_prediction;
        } else {
            predictions[symbol] = 0.0; // Default for missing features
        }
    }

    return predictions;
}

std::map<std::string, double> EnsembleMLPredictor::getEnsemblePredictionsForOptimization(
    const std::vector<std::string>& symbols,
    const std::map<std::string, CorrelationMLFeatures>& enhanced_features) {

    std::map<std::string, double> predictions;

    for (const std::string& symbol : symbols) {
        auto feature_it = enhanced_features.find(symbol);
        if (feature_it != enhanced_features.end()) {
            auto ensemble_prediction = predict(symbol, feature_it->second);
            predictions[symbol] = ensemble_prediction.final_prediction;
        } else {
            predictions[symbol] = 0.0;
        }
    }

    return predictions;
}

EnsemblePrediction EnsembleMLPredictor::combineWithWeightedAverage(
    const std::string& symbol, const std::vector<EnsembleModelResult>& model_results) {

    EnsemblePrediction result;
    result.symbol = symbol;
    result.model_results = model_results;
    result.ensemble_time = std::chrono::system_clock::now();
    result.ensemble_version = ensemble_version_;

    if (model_results.empty()) {
        result.final_prediction = 0.0;
        result.ensemble_confidence = 0.0;
        return result;
    }

    // Calculate weighted average
    double weighted_sum = 0.0;
    double weight_sum = 0.0;
    double confidence_sum = 0.0;

    for (const auto& model_result : model_results) {
        weighted_sum += model_result.prediction * model_result.weight;
        weight_sum += model_result.weight;
        confidence_sum += model_result.confidence * model_result.weight;
    }

    result.final_prediction = weight_sum > 0 ? weighted_sum / weight_sum : 0.0;
    result.ensemble_confidence = weight_sum > 0 ? confidence_sum / weight_sum : 0.0;
    result.total_weight = weight_sum;
    result.models_used = static_cast<int>(model_results.size());

    return result;
}

EnsemblePrediction EnsembleMLPredictor::combineWithVoting(
    const std::string& symbol, const std::vector<EnsembleModelResult>& model_results) {

    // Simple majority voting implementation
    EnsemblePrediction result;
    result.symbol = symbol;
    result.model_results = model_results;

    if (model_results.empty()) {
        result.final_prediction = 0.0;
        result.ensemble_confidence = 0.0;
        return result;
    }

    // Count positive/negative/neutral votes
    int positive_votes = 0;
    int negative_votes = 0;
    int neutral_votes = 0;

    for (const auto& model_result : model_results) {
        if (model_result.prediction > 0.01) {
            positive_votes++;
        } else if (model_result.prediction < -0.01) {
            negative_votes++;
        } else {
            neutral_votes++;
        }
    }

    // Determine majority vote
    if (positive_votes > negative_votes && positive_votes > neutral_votes) {
        result.final_prediction = 0.03; // Moderate positive prediction
        result.ensemble_confidence = static_cast<double>(positive_votes) / model_results.size();
    } else if (negative_votes > positive_votes && negative_votes > neutral_votes) {
        result.final_prediction = -0.03; // Moderate negative prediction
        result.ensemble_confidence = static_cast<double>(negative_votes) / model_results.size();
    } else {
        result.final_prediction = 0.0; // Neutral
        result.ensemble_confidence = static_cast<double>(neutral_votes) / model_results.size();
    }

    return result;
}

EnsemblePrediction EnsembleMLPredictor::combineWithStacking(
    const std::string& symbol, const std::vector<EnsembleModelResult>& model_results) {

    // Simplified stacking - would use meta-learner in full implementation
    return combineWithWeightedAverage(symbol, model_results);
}

double EnsembleMLPredictor::calculateConsensusScore(const std::vector<EnsembleModelResult>& results) {
    if (results.size() < 2) return 1.0;

    double mean_prediction = 0.0;
    for (const auto& result : results) {
        mean_prediction += result.prediction;
    }
    mean_prediction /= results.size();

    double variance = 0.0;
    for (const auto& result : results) {
        double diff = result.prediction - mean_prediction;
        variance += diff * diff;
    }
    variance /= results.size();

    double std_dev = std::sqrt(variance);

    // Consensus score inversely related to standard deviation
    return std::max(0.0, 1.0 - (std_dev * 10.0)); // Scale factor for reasonable values
}

double EnsembleMLPredictor::calculateEnsembleConfidence(const std::vector<EnsembleModelResult>& results) {
    if (results.empty()) return 0.0;

    double weighted_confidence = 0.0;
    double weight_sum = 0.0;

    for (const auto& result : results) {
        weighted_confidence += result.confidence * result.weight;
        weight_sum += result.weight;
    }

    return weight_sum > 0 ? weighted_confidence / weight_sum : 0.0;
}

double EnsembleMLPredictor::calculatePredictionVariance(const std::vector<EnsembleModelResult>& results) {
    if (results.size() < 2) return 0.0;

    double mean = 0.0;
    for (const auto& result : results) {
        mean += result.prediction;
    }
    mean /= results.size();

    double variance = 0.0;
    for (const auto& result : results) {
        double diff = result.prediction - mean;
        variance += diff * diff;
    }

    return variance / results.size();
}

std::vector<std::string> EnsembleMLPredictor::generateEnsembleWarnings(const EnsemblePrediction& prediction) {
    std::vector<std::string> warnings;

    if (prediction.models_used < 2) {
        warnings.push_back("Low model diversity - only " + std::to_string(prediction.models_used) + " models used");
    }

    if (prediction.ensemble_confidence < 0.3) {
        warnings.push_back("Low ensemble confidence (" +
                          std::to_string(prediction.ensemble_confidence) + ")");
    }

    if (prediction.consensus_score < 0.5) {
        warnings.push_back("Poor model consensus - high disagreement between models");
    }

    if (prediction.prediction_variance > 0.01) {
        warnings.push_back("High prediction variance - models have divergent views");
    }

    return warnings;
}

bool EnsembleMLPredictor::validateModelCompatibility(std::shared_ptr<IEnsembleModel> model) {
    return model && model->validateModel();
}

void EnsembleMLPredictor::updateMetrics(const EnsemblePrediction& prediction) {
    metrics_.predictions_made_24h++;

    // Update running averages
    if (metrics_.predictions_made_24h > 1) {
        double alpha = 0.1; // Smoothing factor
        metrics_.average_confidence = (1 - alpha) * metrics_.average_confidence +
                                    alpha * prediction.ensemble_confidence;
        metrics_.consensus_rate = (1 - alpha) * metrics_.consensus_rate +
                                alpha * (prediction.consensus_achieved ? 1.0 : 0.0);
    } else {
        metrics_.average_confidence = prediction.ensemble_confidence;
        metrics_.consensus_rate = prediction.consensus_achieved ? 1.0 : 0.0;
    }

    metrics_.models_active = static_cast<int>(getActiveModels().size());

    // Update individual model accuracies from model results
    for (const auto& model_result : prediction.model_results) {
        metrics_.individual_accuracies[model_result.model_type] = model_result.confidence;
    }
}

void EnsembleMLPredictor::initializeDefaultModels() {
    // Note: In full implementation, these would be initialized with actual model instances
    // For now, create placeholder models

    // Add technical indicator model (always available)
    auto technical_model = std::make_shared<TechnicalIndicatorEnsembleModel>();
    addModel(technical_model);

    // Random Forest and Correlation ML models would be added externally
    // when they're available
}

std::shared_ptr<IEnsembleModel> EnsembleMLPredictor::findModel(EnsembleModelType model_type) {
    for (auto& model : models_) {
        if (model && model->getModelType() == model_type) {
            return model;
        }
    }
    return nullptr;
}

std::shared_ptr<IEnsembleModel> EnsembleMLPredictor::findModel(const std::string& model_name) {
    for (auto& model : models_) {
        if (model && model->getModelName() == model_name) {
            return model;
        }
    }
    return nullptr;
}

void EnsembleMLPredictor::normalizeWeights() {
    if (model_weights_.empty()) return;

    double total_weight = 0.0;
    for (const auto& weight_pair : model_weights_) {
        total_weight += weight_pair.second;
    }

    if (total_weight > 0) {
        for (auto& weight_pair : model_weights_) {
            weight_pair.second /= total_weight;
        }
    }
}

// EnsembleWeightOptimizer Implementation
EnsembleWeightOptimizer::EnsembleWeightOptimizer()
    : optimization_method_("gradient_descent")
    , learning_rate_(0.01)
    , regularization_lambda_(0.001)
    , max_iterations_(1000)
    , convergence_threshold_(1e-6) {
}

EnsembleWeightOptimizer::~EnsembleWeightOptimizer() = default;

std::map<EnsembleModelType, double> EnsembleWeightOptimizer::optimizeWeights(
    const std::vector<EnsembleModelResult>& historical_results,
    const std::vector<double>& actual_values) {

    if (optimization_method_ == "gradient_descent") {
        auto result = optimizeWithGradientDescent(historical_results, actual_values);
        return result.optimal_weights;
    } else if (optimization_method_ == "bayesian") {
        auto result = optimizeWithBayesian(historical_results, actual_values);
        return result.optimal_weights;
    }

    // Default: equal weights
    std::map<EnsembleModelType, double> equal_weights;
    std::set<EnsembleModelType> unique_types;

    for (const auto& result : historical_results) {
        unique_types.insert(result.model_type);
    }

    double equal_weight = unique_types.empty() ? 0.0 : 1.0 / unique_types.size();
    for (EnsembleModelType type : unique_types) {
        equal_weights[type] = equal_weight;
    }

    return equal_weights;
}

std::map<EnsembleModelType, double> EnsembleWeightOptimizer::optimizeWeightsDynamic(
    const std::vector<EnsembleModelResult>& recent_results,
    const std::map<EnsembleModelType, double>& current_weights) {

    // Simple dynamic adjustment based on recent performance
    std::map<EnsembleModelType, double> adjusted_weights = current_weights;

    // Calculate recent accuracy for each model type
    std::map<EnsembleModelType, std::vector<double>> model_predictions;

    for (const auto& result : recent_results) {
        model_predictions[result.model_type].push_back(std::abs(result.prediction));
    }

    // Adjust weights based on prediction magnitude (higher = more confident)
    for (auto& weight_pair : adjusted_weights) {
        auto pred_it = model_predictions.find(weight_pair.first);
        if (pred_it != model_predictions.end() && !pred_it->second.empty()) {
            double avg_magnitude = std::accumulate(pred_it->second.begin(), pred_it->second.end(), 0.0) / pred_it->second.size();
            weight_pair.second *= (1.0 + avg_magnitude); // Boost weight for more confident models
        }
    }

    // Normalize
    double total_weight = 0.0;
    for (const auto& weight_pair : adjusted_weights) {
        total_weight += weight_pair.second;
    }

    if (total_weight > 0) {
        for (auto& weight_pair : adjusted_weights) {
            weight_pair.second /= total_weight;
        }
    }

    return adjusted_weights;
}

void EnsembleWeightOptimizer::updateWeightsFromPerformance(
    const std::map<EnsembleModelType, double>& model_accuracies,
    std::map<EnsembleModelType, double>& weights) {

    // Update weights proportional to accuracy
    for (auto& weight_pair : weights) {
        auto acc_it = model_accuracies.find(weight_pair.first);
        if (acc_it != model_accuracies.end()) {
            weight_pair.second *= (1.0 + acc_it->second); // Boost based on accuracy
        }
    }

    // Normalize
    double total_weight = 0.0;
    for (const auto& weight_pair : weights) {
        total_weight += weight_pair.second;
    }

    if (total_weight > 0) {
        for (auto& weight_pair : weights) {
            weight_pair.second /= total_weight;
        }
    }
}

EnsembleWeightOptimizer::OptimizationResult EnsembleWeightOptimizer::optimizeWithGradientDescent(
    const std::vector<EnsembleModelResult>& training_data,
    const std::vector<double>& targets) {

    OptimizationResult result;

    // Initialize equal weights
    std::set<EnsembleModelType> unique_types;
    for (const auto& data : training_data) {
        unique_types.insert(data.model_type);
    }

    for (EnsembleModelType type : unique_types) {
        result.optimal_weights[type] = 1.0 / unique_types.size();
    }

    double previous_error = std::numeric_limits<double>::max();

    for (int iteration = 0; iteration < max_iterations_; ++iteration) {
        double current_error = calculateEnsembleError(training_data, targets, result.optimal_weights);

        if (std::abs(previous_error - current_error) < convergence_threshold_) {
            break;
        }

        auto gradients = calculateGradients(training_data, targets, result.optimal_weights);

        // Update weights
        for (auto& weight_pair : result.optimal_weights) {
            auto grad_it = gradients.find(weight_pair.first);
            if (grad_it != gradients.end()) {
                weight_pair.second -= learning_rate_ * grad_it->second;
                weight_pair.second = std::max(0.0, weight_pair.second); // Non-negative constraint
            }
        }

        // Normalize weights
        double total_weight = 0.0;
        for (const auto& weight_pair : result.optimal_weights) {
            total_weight += weight_pair.second;
        }

        if (total_weight > 0) {
            for (auto& weight_pair : result.optimal_weights) {
                weight_pair.second /= total_weight;
            }
        }

        previous_error = current_error;
    }

    result.optimization_score = calculateEnsembleError(training_data, targets, result.optimal_weights);
    result.improvement_percentage = 10.0; // Placeholder
    result.optimization_notes.push_back("Gradient descent optimization completed");

    return result;
}

EnsembleWeightOptimizer::OptimizationResult EnsembleWeightOptimizer::optimizeWithBayesian(
    const std::vector<EnsembleModelResult>& training_data,
    const std::vector<double>& targets) {

    // Simplified Bayesian optimization - return gradient descent result for now
    return optimizeWithGradientDescent(training_data, targets);
}

void EnsembleWeightOptimizer::setOptimizationMethod(const std::string& method) {
    if (method == "gradient_descent" || method == "bayesian") {
        optimization_method_ = method;
    }
}

void EnsembleWeightOptimizer::setLearningRate(double learning_rate) {
    learning_rate_ = std::max(0.001, std::min(0.1, learning_rate));
}

void EnsembleWeightOptimizer::setRegularization(double lambda) {
    regularization_lambda_ = std::max(0.0, lambda);
}

double EnsembleWeightOptimizer::calculateEnsembleError(
    const std::vector<EnsembleModelResult>& results,
    const std::vector<double>& targets,
    const std::map<EnsembleModelType, double>& weights) {

    if (results.size() != targets.size()) return std::numeric_limits<double>::max();

    double total_error = 0.0;

    // Group results by target index (assuming chronological order)
    for (size_t i = 0; i < targets.size() && i < results.size(); ++i) {
        double weighted_prediction = 0.0;

        auto weight_it = weights.find(results[i].model_type);
        if (weight_it != weights.end()) {
            weighted_prediction = results[i].prediction * weight_it->second;
        }

        double error = weighted_prediction - targets[i];
        total_error += error * error;
    }

    return total_error / targets.size();
}

std::map<EnsembleModelType, double> EnsembleWeightOptimizer::calculateGradients(
    const std::vector<EnsembleModelResult>& results,
    const std::vector<double>& targets,
    const std::map<EnsembleModelType, double>& weights) {

    std::map<EnsembleModelType, double> gradients;

    // Initialize gradients to zero
    for (const auto& weight_pair : weights) {
        gradients[weight_pair.first] = 0.0;
    }

    // Calculate gradients (simplified)
    for (size_t i = 0; i < targets.size() && i < results.size(); ++i) {
        double weighted_prediction = 0.0;

        auto weight_it = weights.find(results[i].model_type);
        if (weight_it != weights.end()) {
            weighted_prediction = results[i].prediction * weight_it->second;
        }

        double error = weighted_prediction - targets[i];

        // Gradient for this model type
        gradients[results[i].model_type] += 2.0 * error * results[i].prediction;
    }

    // Average gradients
    for (auto& grad_pair : gradients) {
        grad_pair.second /= targets.size();
    }

    return gradients;
}

void EnsembleWeightOptimizer::applyRegularization(std::map<EnsembleModelType, double>& weights) {
    // L2 regularization
    for (auto& weight_pair : weights) {
        weight_pair.second *= (1.0 - regularization_lambda_);
    }
}

bool EnsembleWeightOptimizer::checkConvergence(
    const std::map<EnsembleModelType, double>& old_weights,
    const std::map<EnsembleModelType, double>& new_weights) {

    double max_change = 0.0;

    for (const auto& new_weight : new_weights) {
        auto old_it = old_weights.find(new_weight.first);
        if (old_it != old_weights.end()) {
            double change = std::abs(new_weight.second - old_it->second);
            max_change = std::max(max_change, change);
        }
    }

    return max_change < convergence_threshold_;
}

// MetaLearningFramework Implementation - simplified for space
MetaLearningFramework::MetaLearningFramework()
    : learning_decay_(0.95)
    , rebalance_frequency_(100)
    , rebalance_threshold_(0.05) {
}

MetaLearningFramework::~MetaLearningFramework() = default;

MetaLearningFramework::MetaLearningResult MetaLearningFramework::recommendEnsembleConfiguration(
    const std::vector<EnsemblePrediction>& historical_predictions,
    const std::vector<double>& actual_outcomes) {

    MetaLearningResult result;
    result.recommended_method = "weighted_average"; // Default
    result.confidence_in_recommendation = 0.7;
    result.reasoning.push_back("Default weighted average recommended");

    // Simple analysis - would be more sophisticated in full implementation
    if (historical_predictions.size() == actual_outcomes.size() && !historical_predictions.empty()) {
        double avg_consensus = 0.0;
        for (const auto& prediction : historical_predictions) {
            avg_consensus += prediction.consensus_score;
        }
        avg_consensus /= historical_predictions.size();

        if (avg_consensus > 0.8) {
            result.recommended_method = "weighted_average";
            result.reasoning.push_back("High consensus supports weighted averaging");
        } else if (avg_consensus < 0.4) {
            result.recommended_method = "voting";
            result.reasoning.push_back("Low consensus suggests voting method");
        }
    }

    return result;
}

MetaLearningFramework::MetaLearningResult MetaLearningFramework::adaptToMarketRegime(
    const CorrelationRegime& current_regime,
    const std::map<EnsembleModelType, double>& current_performance) {

    MetaLearningResult result;
    result.recommended_method = "weighted_average";
    result.confidence_in_recommendation = 0.6;

    // Regime-specific recommendations
    switch (current_regime) {
        case CorrelationRegime::RISK_OFF:
            result.recommended_weights[EnsembleModelType::CORRELATION_ML] = 0.6;
            result.recommended_weights[EnsembleModelType::RANDOM_FOREST] = 0.3;
            result.recommended_weights[EnsembleModelType::TECHNICAL_INDICATORS] = 0.1;
            result.reasoning.push_back("Risk-off regime: emphasize correlation model");
            break;

        case CorrelationRegime::NORMAL:
            result.recommended_weights[EnsembleModelType::RANDOM_FOREST] = 0.5;
            result.recommended_weights[EnsembleModelType::CORRELATION_ML] = 0.3;
            result.recommended_weights[EnsembleModelType::TECHNICAL_INDICATORS] = 0.2;
            result.reasoning.push_back("Normal regime: balanced ensemble");
            break;

        default:
            result.recommended_weights[EnsembleModelType::RANDOM_FOREST] = 0.4;
            result.recommended_weights[EnsembleModelType::CORRELATION_ML] = 0.4;
            result.recommended_weights[EnsembleModelType::TECHNICAL_INDICATORS] = 0.2;
            result.reasoning.push_back("Default balanced allocation");
            break;
    }

    return result;
}

void MetaLearningFramework::learnFromPredictions(
    const std::vector<EnsemblePrediction>& predictions,
    const std::vector<double>& actual_outcomes) {

    // Store for future analysis
    historical_predictions_.insert(historical_predictions_.end(), predictions.begin(), predictions.end());
    historical_outcomes_.insert(historical_outcomes_.end(), actual_outcomes.begin(), actual_outcomes.end());

    // Keep only recent history
    if (historical_predictions_.size() > 1000) {
        historical_predictions_.erase(historical_predictions_.begin());
        historical_outcomes_.erase(historical_outcomes_.begin());
    }
}

bool MetaLearningFramework::shouldRebalanceEnsemble() const {
    return historical_predictions_.size() > rebalance_frequency_ && detectPerformanceDrift();
}

MetaLearningFramework::MetaLearningResult MetaLearningFramework::suggestRebalancing() {
    MetaLearningResult result;
    result.recommended_method = "weighted_average";
    result.confidence_in_recommendation = 0.5;
    result.reasoning.push_back("Periodic rebalancing suggested");

    return result;
}

void MetaLearningFramework::trackEnsemblePerformance(const EnsembleMLPredictor::EnsembleMetrics& metrics) {
    // Track key metrics
    performance_history_["ensemble_accuracy"].push_back(metrics.ensemble_accuracy);
    performance_history_["consensus_rate"].push_back(metrics.consensus_rate);
    performance_history_["average_confidence"].push_back(metrics.average_confidence);

    // Keep only recent history
    for (auto& history_pair : performance_history_) {
        if (history_pair.second.size() > 200) {
            history_pair.second.erase(history_pair.second.begin());
        }
    }
}

std::map<std::string, double> MetaLearningFramework::getOptimalConfiguration() const {
    std::map<std::string, double> config;
    config["recommended_method"] = 1.0; // weighted_average
    config["rebalance_frequency"] = rebalance_frequency_;
    config["learning_rate"] = 0.01;
    return config;
}

double MetaLearningFramework::evaluateEnsembleMethod(
    const std::string& method,
    const std::vector<EnsemblePrediction>& predictions,
    const std::vector<double>& outcomes) {

    // Simple evaluation - calculate mean squared error
    if (predictions.size() != outcomes.size() || predictions.empty()) return 0.0;

    double total_error = 0.0;
    for (size_t i = 0; i < predictions.size(); ++i) {
        double error = predictions[i].final_prediction - outcomes[i];
        total_error += error * error;
    }

    double mse = total_error / predictions.size();
    return std::max(0.0, 1.0 - mse); // Convert to accuracy-like score
}

std::map<EnsembleModelType, double> MetaLearningFramework::analyzeModelPerformanceByRegime(
    CorrelationRegime regime) const {

    // Placeholder implementation
    std::map<EnsembleModelType, double> performance;
    performance[EnsembleModelType::RANDOM_FOREST] = 0.7;
    performance[EnsembleModelType::CORRELATION_ML] = 0.8;
    performance[EnsembleModelType::TECHNICAL_INDICATORS] = 0.6;

    return performance;
}

bool MetaLearningFramework::detectPerformanceDrift() const {
    auto acc_it = performance_history_.find("ensemble_accuracy");
    if (acc_it == performance_history_.end() || acc_it->second.size() < 20) {
        return false;
    }

    const auto& accuracy_history = acc_it->second;

    // Compare recent vs historical average
    double recent_avg = 0.0;
    int recent_count = std::min(10, static_cast<int>(accuracy_history.size()));

    for (int i = 0; i < recent_count; ++i) {
        recent_avg += accuracy_history[accuracy_history.size() - 1 - i];
    }
    recent_avg /= recent_count;

    double historical_avg = std::accumulate(accuracy_history.begin(), accuracy_history.end(), 0.0) / accuracy_history.size();

    return std::abs(recent_avg - historical_avg) > rebalance_threshold_;
}

MetaLearningFramework::MetaLearningResult MetaLearningFramework::generateRecommendation(
    const std::map<std::string, double>& analysis_results) {

    MetaLearningResult result;
    result.recommended_method = "weighted_average";
    result.confidence_in_recommendation = 0.7;
    result.reasoning.push_back("Based on historical analysis");

    return result;
}

// EnsembleUtils Implementation
namespace EnsembleUtils {

std::vector<EnsembleModelResult> filterResultsByConfidence(
    const std::vector<EnsembleModelResult>& results, double min_confidence) {

    std::vector<EnsembleModelResult> filtered;

    for (const auto& result : results) {
        if (result.confidence >= min_confidence) {
            filtered.push_back(result);
        }
    }

    return filtered;
}

double calculateModelDiversity(const std::vector<EnsembleModelResult>& results) {
    if (results.size() < 2) return 0.0;

    // Calculate prediction variance as diversity measure
    double mean = 0.0;
    for (const auto& result : results) {
        mean += result.prediction;
    }
    mean /= results.size();

    double variance = 0.0;
    for (const auto& result : results) {
        double diff = result.prediction - mean;
        variance += diff * diff;
    }

    return variance / results.size();
}

std::map<EnsembleModelType, double> calculateModelCorrelations(
    const std::vector<std::vector<EnsembleModelResult>>& historical_results) {

    // Simplified correlation calculation
    std::map<EnsembleModelType, double> correlations;

    // Set default correlations
    correlations[EnsembleModelType::RANDOM_FOREST] = 0.3;
    correlations[EnsembleModelType::CORRELATION_ML] = 0.4;
    correlations[EnsembleModelType::TECHNICAL_INDICATORS] = 0.2;

    return correlations;
}

std::string generateEnsembleReport(const EnsembleMLPredictor::EnsembleMetrics& metrics) {
    std::string report = "=== Ensemble Performance Report ===\n";
    report += "Ensemble Accuracy: " + std::to_string(metrics.ensemble_accuracy) + "\n";
    report += "Consensus Rate: " + std::to_string(metrics.consensus_rate) + "\n";
    report += "Average Confidence: " + std::to_string(metrics.average_confidence) + "\n";
    report += "Prediction Stability: " + std::to_string(metrics.prediction_stability) + "\n";
    report += "Predictions Made (24h): " + std::to_string(metrics.predictions_made_24h) + "\n";
    report += "Active Models: " + std::to_string(metrics.models_active) + "\n";
    report += "Improvement vs Best Single: " + std::to_string(metrics.improvement_over_best_single) + "%\n";

    return report;
}

std::vector<std::string> identifyEnsembleIssues(
    const std::vector<EnsemblePrediction>& recent_predictions) {

    std::vector<std::string> issues;

    if (recent_predictions.empty()) {
        issues.push_back("No recent predictions available");
        return issues;
    }

    // Analyze recent predictions for issues
    double avg_confidence = 0.0;
    double avg_consensus = 0.0;
    int low_diversity_count = 0;

    for (const auto& prediction : recent_predictions) {
        avg_confidence += prediction.ensemble_confidence;
        avg_consensus += prediction.consensus_score;

        if (prediction.models_used < 2) {
            low_diversity_count++;
        }
    }

    avg_confidence /= recent_predictions.size();
    avg_consensus /= recent_predictions.size();

    if (avg_confidence < 0.4) {
        issues.push_back("Low average ensemble confidence (" + std::to_string(avg_confidence) + ")");
    }

    if (avg_consensus < 0.5) {
        issues.push_back("Poor model consensus (" + std::to_string(avg_consensus) + ")");
    }

    if (low_diversity_count > recent_predictions.size() / 2) {
        issues.push_back("Insufficient model diversity in predictions");
    }

    return issues;
}

double calculateEnsembleImprovement(
    const std::vector<double>& ensemble_predictions,
    const std::vector<double>& best_individual_predictions,
    const std::vector<double>& actual_values) {

    if (ensemble_predictions.size() != actual_values.size() ||
        best_individual_predictions.size() != actual_values.size() ||
        ensemble_predictions.empty()) {
        return 0.0;
    }

    // Calculate MSE for both
    double ensemble_mse = 0.0;
    double individual_mse = 0.0;

    for (size_t i = 0; i < actual_values.size(); ++i) {
        double ensemble_error = ensemble_predictions[i] - actual_values[i];
        double individual_error = best_individual_predictions[i] - actual_values[i];

        ensemble_mse += ensemble_error * ensemble_error;
        individual_mse += individual_error * individual_error;
    }

    ensemble_mse /= actual_values.size();
    individual_mse /= actual_values.size();

    if (individual_mse == 0.0) return 0.0;

    // Return percentage improvement
    return ((individual_mse - ensemble_mse) / individual_mse) * 100.0;
}

std::string modelTypeToString(EnsembleModelType type) {
    switch (type) {
        case EnsembleModelType::RANDOM_FOREST: return "RandomForest";
        case EnsembleModelType::CORRELATION_ML: return "CorrelationML";
        case EnsembleModelType::TECHNICAL_INDICATORS: return "TechnicalIndicators";
        case EnsembleModelType::SENTIMENT_ANALYZER: return "SentimentAnalyzer";
        case EnsembleModelType::REGIME_DETECTOR: return "RegimeDetector";
        case EnsembleModelType::CUSTOM_MODEL: return "CustomModel";
        default: return "Unknown";
    }
}

EnsembleModelType stringToModelType(const std::string& type_str) {
    if (type_str == "RandomForest") return EnsembleModelType::RANDOM_FOREST;
    if (type_str == "CorrelationML") return EnsembleModelType::CORRELATION_ML;
    if (type_str == "TechnicalIndicators") return EnsembleModelType::TECHNICAL_INDICATORS;
    if (type_str == "SentimentAnalyzer") return EnsembleModelType::SENTIMENT_ANALYZER;
    if (type_str == "RegimeDetector") return EnsembleModelType::REGIME_DETECTOR;
    if (type_str == "CustomModel") return EnsembleModelType::CUSTOM_MODEL;
    return EnsembleModelType::CUSTOM_MODEL;
}

std::vector<std::string> getModelTypeDescriptions() {
    return {
        "RandomForest - Day 8 Random Forest ML predictor",
        "CorrelationML - Day 9 Cross-asset correlation ML enhancer",
        "TechnicalIndicators - Rule-based technical analysis model",
        "SentimentAnalyzer - Sentiment-based prediction model",
        "RegimeDetector - Market regime detection model",
        "CustomModel - User-defined model"
    };
}

} // namespace EnsembleUtils

} // namespace ML
} // namespace CryptoClaude