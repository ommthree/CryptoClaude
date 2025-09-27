#pragma once

#include "RandomForestPredictor.h"
#include "CorrelationMLEnhancer.h"
#include "../Analytics/StatisticalTools.h"
#include <vector>
#include <map>
#include <string>
#include <memory>
#include <chrono>
#include <functional>

namespace CryptoClaude {
namespace ML {

// Forward declarations
class EnsembleWeightOptimizer;
class MetaLearningFramework;

// Ensemble model types
enum class EnsembleModelType {
    RANDOM_FOREST,          // Day 8 Random Forest implementation
    CORRELATION_ML,         // Day 9 Correlation ML enhancer
    TECHNICAL_INDICATORS,   // Technical indicator-based model
    SENTIMENT_ANALYZER,     // Sentiment-based model
    REGIME_DETECTOR,        // Correlation regime detection model
    CUSTOM_MODEL           // User-defined model
};

// Individual model result in ensemble
struct EnsembleModelResult {
    EnsembleModelType model_type;
    std::string model_name;
    std::string symbol;
    double prediction;
    double confidence;
    double weight;              // Dynamic weight assigned by ensemble
    double contribution;        // Weighted contribution to final prediction
    std::chrono::system_clock::time_point prediction_time;
    std::string model_version;

    // Model-specific metadata
    std::map<std::string, double> model_metrics;
    std::vector<std::string> feature_importance;
};

// Comprehensive ensemble prediction result
struct EnsemblePrediction {
    std::string symbol;
    double final_prediction;     // Weighted ensemble prediction
    double ensemble_confidence;  // Combined confidence score
    double prediction_variance;  // Variance across models
    double consensus_score;      // Agreement between models (0-1)

    // Individual model results
    std::vector<EnsembleModelResult> model_results;

    // Ensemble statistics
    double min_prediction;
    double max_prediction;
    double median_prediction;
    double weighted_std_deviation;

    // Meta information
    std::chrono::system_clock::time_point ensemble_time;
    std::string ensemble_version;
    int models_used;
    double total_weight;

    // Performance indicators
    double ensemble_quality_score; // Overall ensemble quality
    bool consensus_achieved;        // True if models generally agree
    std::vector<std::string> ensemble_warnings;
};

// Base interface for ensemble models
class IEnsembleModel {
public:
    virtual ~IEnsembleModel() = default;

    // Core prediction interface
    virtual EnsembleModelResult predict(const MLFeatureVector& features) = 0;
    virtual EnsembleModelResult predict(const CorrelationMLFeatures& features) = 0;

    // Model information
    virtual EnsembleModelType getModelType() const = 0;
    virtual std::string getModelName() const = 0;
    virtual std::string getModelVersion() const = 0;
    virtual double getModelConfidence() const = 0;

    // Model training and validation
    virtual bool isModelTrained() const = 0;
    virtual bool validateModel() const = 0;
    virtual std::map<std::string, double> getModelMetrics() const = 0;

    // Feature requirements
    virtual std::vector<std::string> getRequiredFeatures() const = 0;
    virtual bool canHandleFeatures(const MLFeatureVector& features) const = 0;
};

// Wrapper for Day 8 Random Forest model
class RandomForestEnsembleModel : public IEnsembleModel {
public:
    RandomForestEnsembleModel(std::shared_ptr<RandomForestPredictor> rf_predictor);
    ~RandomForestEnsembleModel();

    // IEnsembleModel interface
    EnsembleModelResult predict(const MLFeatureVector& features) override;
    EnsembleModelResult predict(const CorrelationMLFeatures& features) override;
    EnsembleModelType getModelType() const override { return EnsembleModelType::RANDOM_FOREST; }
    std::string getModelName() const override { return "RandomForest_v1.0"; }
    std::string getModelVersion() const override;
    double getModelConfidence() const override;
    bool isModelTrained() const override;
    bool validateModel() const override;
    std::map<std::string, double> getModelMetrics() const override;
    std::vector<std::string> getRequiredFeatures() const override;
    bool canHandleFeatures(const MLFeatureVector& features) const override;

private:
    std::shared_ptr<RandomForestPredictor> rf_predictor_;
};

// Wrapper for Day 9 Correlation ML model
class CorrelationEnsembleModel : public IEnsembleModel {
public:
    CorrelationEnsembleModel(std::shared_ptr<CorrelationMLEnhancer> correlation_enhancer);
    ~CorrelationEnsembleModel();

    // IEnsembleModel interface
    EnsembleModelResult predict(const MLFeatureVector& features) override;
    EnsembleModelResult predict(const CorrelationMLFeatures& features) override;
    EnsembleModelType getModelType() const override { return EnsembleModelType::CORRELATION_ML; }
    std::string getModelName() const override { return "CorrelationML_v1.0"; }
    std::string getModelVersion() const override { return "correlation_v1.0"; }
    double getModelConfidence() const override;
    bool isModelTrained() const override;
    bool validateModel() const override;
    std::map<std::string, double> getModelMetrics() const override;
    std::vector<std::string> getRequiredFeatures() const override;
    bool canHandleFeatures(const MLFeatureVector& features) const override;

private:
    std::shared_ptr<CorrelationMLEnhancer> correlation_enhancer_;
};

// Technical indicator-based ensemble model
class TechnicalIndicatorEnsembleModel : public IEnsembleModel {
public:
    TechnicalIndicatorEnsembleModel();
    ~TechnicalIndicatorEnsembleModel();

    // IEnsembleModel interface
    EnsembleModelResult predict(const MLFeatureVector& features) override;
    EnsembleModelResult predict(const CorrelationMLFeatures& features) override;
    EnsembleModelType getModelType() const override { return EnsembleModelType::TECHNICAL_INDICATORS; }
    std::string getModelName() const override { return "TechnicalIndicators_v1.0"; }
    std::string getModelVersion() const override { return "technical_v1.0"; }
    double getModelConfidence() const override { return model_confidence_; }
    bool isModelTrained() const override { return is_trained_; }
    bool validateModel() const override;
    std::map<std::string, double> getModelMetrics() const override;
    std::vector<std::string> getRequiredFeatures() const override;
    bool canHandleFeatures(const MLFeatureVector& features) const override;

private:
    bool is_trained_;
    double model_confidence_;
    std::map<std::string, double> model_metrics_;

    // Technical analysis methods
    double calculateTechnicalScore(const MLFeatureVector& features);
    double calculateMomentumSignal(const MLFeatureVector& features);
    double calculateMeanReversionSignal(const MLFeatureVector& features);
    double calculateVolatilitySignal(const MLFeatureVector& features);
};

// Main ensemble ML prediction system
class EnsembleMLPredictor {
public:
    EnsembleMLPredictor();
    ~EnsembleMLPredictor();

    // Ensemble initialization and configuration
    bool initialize();
    void addModel(std::shared_ptr<IEnsembleModel> model);
    void removeModel(EnsembleModelType model_type);
    void removeModel(const std::string& model_name);

    // Core ensemble prediction
    EnsemblePrediction predict(const std::string& symbol, const MLFeatureVector& features);
    EnsemblePrediction predict(const std::string& symbol, const CorrelationMLFeatures& enhanced_features);

    // Batch prediction
    std::vector<EnsemblePrediction> predictBatch(
        const std::vector<std::string>& symbols,
        const std::map<std::string, MLFeatureVector>& features);

    std::vector<EnsemblePrediction> predictBatch(
        const std::vector<std::string>& symbols,
        const std::map<std::string, CorrelationMLFeatures>& enhanced_features);

    // Ensemble configuration
    void setWeightOptimizer(std::shared_ptr<EnsembleWeightOptimizer> optimizer);
    void setMetaLearner(std::shared_ptr<MetaLearningFramework> meta_learner);
    void setEnsembleMethod(const std::string& method); // "weighted_average", "voting", "stacking"

    // Dynamic weight management
    void updateModelWeights();
    void setModelWeight(EnsembleModelType model_type, double weight);
    void setModelWeight(const std::string& model_name, double weight);
    double getModelWeight(EnsembleModelType model_type) const;
    double getModelWeight(const std::string& model_name) const;

    // Ensemble performance and analysis
    struct EnsembleMetrics {
        double ensemble_accuracy;           // Overall prediction accuracy
        double consensus_rate;              // Rate of model agreement
        double average_confidence;          // Average ensemble confidence
        double prediction_stability;        // Consistency across time
        int predictions_made_24h;           // Volume metrics
        int models_active;                  // Number of active models
        double improvement_over_best_single; // Improvement vs best individual model
        std::map<EnsembleModelType, double> individual_accuracies;
        std::vector<std::string> ensemble_warnings;
    };

    EnsembleMetrics getEnsembleMetrics() const;
    void resetMetrics();

    // Model management and status
    std::vector<std::shared_ptr<IEnsembleModel>> getActiveModels() const;
    std::vector<EnsembleModelType> getAvailableModelTypes() const;
    bool isModelActive(EnsembleModelType model_type) const;
    bool isModelActive(const std::string& model_name) const;

    // Ensemble validation and diagnostics
    bool validateEnsemble() const;
    std::vector<std::string> getDiagnosticReport() const;
    void enableModel(EnsembleModelType model_type, bool enabled);
    void enableModel(const std::string& model_name, bool enabled);

    // Portfolio optimization integration
    std::map<std::string, double> getEnsemblePredictionsForOptimization(
        const std::vector<std::string>& symbols,
        const std::map<std::string, MLFeatureVector>& current_features);

    std::map<std::string, double> getEnsemblePredictionsForOptimization(
        const std::vector<std::string>& symbols,
        const std::map<std::string, CorrelationMLFeatures>& enhanced_features);

private:
    // Core ensemble components
    std::vector<std::shared_ptr<IEnsembleModel>> models_;
    std::map<EnsembleModelType, double> model_weights_;
    std::map<std::string, bool> model_enabled_;

    // Optimization and meta-learning
    std::shared_ptr<EnsembleWeightOptimizer> weight_optimizer_;
    std::shared_ptr<MetaLearningFramework> meta_learner_;

    // Configuration
    std::string ensemble_method_;
    bool is_initialized_;
    std::string ensemble_version_;

    // Performance tracking
    mutable EnsembleMetrics metrics_;
    std::vector<EnsemblePrediction> recent_predictions_;

    // Prediction combination methods
    EnsemblePrediction combineWithWeightedAverage(
        const std::string& symbol,
        const std::vector<EnsembleModelResult>& model_results);

    EnsemblePrediction combineWithVoting(
        const std::string& symbol,
        const std::vector<EnsembleModelResult>& model_results);

    EnsemblePrediction combineWithStacking(
        const std::string& symbol,
        const std::vector<EnsembleModelResult>& model_results);

    // Ensemble analysis
    double calculateConsensusScore(const std::vector<EnsembleModelResult>& results);
    double calculateEnsembleConfidence(const std::vector<EnsembleModelResult>& results);
    double calculatePredictionVariance(const std::vector<EnsembleModelResult>& results);
    std::vector<std::string> generateEnsembleWarnings(const EnsemblePrediction& prediction);

    // Model validation helpers
    bool validateModelCompatibility(std::shared_ptr<IEnsembleModel> model);
    void updateMetrics(const EnsemblePrediction& prediction);

    // Default model initialization
    void initializeDefaultModels();

    // Utility methods
    std::shared_ptr<IEnsembleModel> findModel(EnsembleModelType model_type);
    std::shared_ptr<IEnsembleModel> findModel(const std::string& model_name);
    void normalizeWeights();
};

// Advanced ensemble weight optimization
class EnsembleWeightOptimizer {
public:
    EnsembleWeightOptimizer();
    ~EnsembleWeightOptimizer();

    // Weight optimization methods
    std::map<EnsembleModelType, double> optimizeWeights(
        const std::vector<EnsembleModelResult>& historical_results,
        const std::vector<double>& actual_values);

    std::map<EnsembleModelType, double> optimizeWeightsDynamic(
        const std::vector<EnsembleModelResult>& recent_results,
        const std::map<EnsembleModelType, double>& current_weights);

    // Performance-based weight adjustment
    void updateWeightsFromPerformance(
        const std::map<EnsembleModelType, double>& model_accuracies,
        std::map<EnsembleModelType, double>& weights);

    // Optimization algorithms
    struct OptimizationResult {
        std::map<EnsembleModelType, double> optimal_weights;
        double optimization_score;
        double improvement_percentage;
        std::vector<std::string> optimization_notes;
    };

    OptimizationResult optimizeWithGradientDescent(
        const std::vector<EnsembleModelResult>& training_data,
        const std::vector<double>& targets);

    OptimizationResult optimizeWithBayesian(
        const std::vector<EnsembleModelResult>& training_data,
        const std::vector<double>& targets);

    // Configuration
    void setOptimizationMethod(const std::string& method);
    void setLearningRate(double learning_rate);
    void setRegularization(double lambda);

private:
    std::string optimization_method_;
    double learning_rate_;
    double regularization_lambda_;
    int max_iterations_;
    double convergence_threshold_;

    // Optimization helper methods
    double calculateEnsembleError(
        const std::vector<EnsembleModelResult>& results,
        const std::vector<double>& targets,
        const std::map<EnsembleModelType, double>& weights);

    std::map<EnsembleModelType, double> calculateGradients(
        const std::vector<EnsembleModelResult>& results,
        const std::vector<double>& targets,
        const std::map<EnsembleModelType, double>& weights);

    void applyRegularization(std::map<EnsembleModelType, double>& weights);
    bool checkConvergence(const std::map<EnsembleModelType, double>& old_weights,
                         const std::map<EnsembleModelType, double>& new_weights);
    void normalizeWeights();
};

// Meta-learning framework for ensemble configuration
class MetaLearningFramework {
public:
    MetaLearningFramework();
    ~MetaLearningFramework();

    // Meta-learning for ensemble configuration
    struct MetaLearningResult {
        std::string recommended_method;     // Best ensemble method for current conditions
        std::map<EnsembleModelType, double> recommended_weights;
        double confidence_in_recommendation;
        std::vector<std::string> reasoning;
    };

    MetaLearningResult recommendEnsembleConfiguration(
        const std::vector<EnsemblePrediction>& historical_predictions,
        const std::vector<double>& actual_outcomes);

    // Market regime-based configuration
    MetaLearningResult adaptToMarketRegime(
        const CorrelationRegime& current_regime,
        const std::map<EnsembleModelType, double>& current_performance);

    // Dynamic ensemble optimization
    void learnFromPredictions(
        const std::vector<EnsemblePrediction>& predictions,
        const std::vector<double>& actual_outcomes);

    bool shouldRebalanceEnsemble() const;
    MetaLearningResult suggestRebalancing();

    // Configuration learning
    void trackEnsemblePerformance(const EnsembleMLPredictor::EnsembleMetrics& metrics);
    std::map<std::string, double> getOptimalConfiguration() const;

private:
    // Learning storage
    std::vector<EnsemblePrediction> historical_predictions_;
    std::vector<double> historical_outcomes_;
    std::map<std::string, std::vector<double>> performance_history_;

    // Configuration tracking
    std::map<std::string, double> method_performance_;
    std::map<CorrelationRegime, std::map<EnsembleModelType, double>> regime_optimal_weights_;

    // Learning parameters
    double learning_decay_;
    int rebalance_frequency_;
    double rebalance_threshold_;

    // Analysis methods
    double evaluateEnsembleMethod(
        const std::string& method,
        const std::vector<EnsemblePrediction>& predictions,
        const std::vector<double>& outcomes);

    std::map<EnsembleModelType, double> analyzeModelPerformanceByRegime(
        CorrelationRegime regime) const;

    bool detectPerformanceDrift() const;
    MetaLearningResult generateRecommendation(
        const std::map<std::string, double>& analysis_results);
};

// Utility functions for ensemble operations
namespace EnsembleUtils {
    // Model result analysis
    std::vector<EnsembleModelResult> filterResultsByConfidence(
        const std::vector<EnsembleModelResult>& results, double min_confidence);

    double calculateModelDiversity(const std::vector<EnsembleModelResult>& results);
    std::map<EnsembleModelType, double> calculateModelCorrelations(
        const std::vector<std::vector<EnsembleModelResult>>& historical_results);

    // Ensemble diagnostics
    std::string generateEnsembleReport(const EnsembleMLPredictor::EnsembleMetrics& metrics);
    std::vector<std::string> identifyEnsembleIssues(
        const std::vector<EnsemblePrediction>& recent_predictions);

    // Performance utilities
    double calculateEnsembleImprovement(
        const std::vector<double>& ensemble_predictions,
        const std::vector<double>& best_individual_predictions,
        const std::vector<double>& actual_values);

    // Model type utilities
    std::string modelTypeToString(EnsembleModelType type);
    EnsembleModelType stringToModelType(const std::string& type_str);
    std::vector<std::string> getModelTypeDescriptions();
}

} // namespace ML
} // namespace CryptoClaude