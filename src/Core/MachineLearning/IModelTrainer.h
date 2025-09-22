#pragma once
#include "../FeatureEngineering/FeatureCalculator.h"
#include <vector>
#include <map>
#include <string>
#include <memory>

namespace CryptoClaude {
namespace MachineLearning {

// Model performance metrics
struct ModelMetrics {
    double rmse;                // Root Mean Square Error
    double mae;                 // Mean Absolute Error
    double rSquared;            // R-squared
    double oobRmse;             // Out-of-bag RMSE (for ensemble methods)
    double oobRelError;         // Out-of-bag relative error
    std::vector<double> featureImportances;
    std::vector<std::string> featureNames;
    int trainingSize;
    int validationSize;

    ModelMetrics()
        : rmse(0.0), mae(0.0), rSquared(0.0), oobRmse(0.0),
          oobRelError(0.0), trainingSize(0), validationSize(0) {}
};

// Model prediction result
struct PredictionResult {
    std::string symbol;
    std::string date;
    double predicted;
    double actual;              // If available for validation
    double confidence;          // Prediction confidence [0,1]
    std::vector<double> featureVector;

    PredictionResult()
        : predicted(0.0), actual(0.0), confidence(0.0) {}
};

// Abstract interface for all model trainers
class IModelTrainer {
public:
    virtual ~IModelTrainer() = default;

    // Model information
    virtual std::string getName() const = 0;
    virtual std::string getDescription() const = 0;
    virtual std::string getVersion() const = 0;

    // Training and prediction
    virtual bool train(const std::vector<FeatureEngineering::FeatureVector>& trainingData) = 0;
    virtual std::vector<PredictionResult> predict(const std::vector<FeatureEngineering::FeatureVector>& features) = 0;
    virtual double predict(const FeatureEngineering::FeatureVector& feature) = 0;

    // Model evaluation
    virtual ModelMetrics evaluate(const std::vector<FeatureEngineering::FeatureVector>& testData) = 0;
    virtual std::vector<PredictionResult> validatePredictions(const std::vector<FeatureEngineering::FeatureVector>& validationData) = 0;

    // Model persistence
    virtual bool saveModel(const std::string& filepath) = 0;
    virtual bool loadModel(const std::string& filepath) = 0;

    // Model configuration
    virtual void setHyperParameters(const std::map<std::string, double>& params) = 0;
    virtual std::map<std::string, double> getHyperParameters() const = 0;

    // Status and diagnostics
    virtual bool isTrained() const = 0;
    virtual ModelMetrics getLastTrainingMetrics() const = 0;
    virtual std::string getLastError() const = 0;

    // Feature handling
    virtual std::vector<std::string> getRequiredFeatures() const = 0;
    virtual bool validateFeatures(const std::vector<FeatureEngineering::FeatureVector>& features) = 0;
};

// Model trainer factory
class ModelTrainerFactory {
public:
    enum class ModelType {
        RandomForest,
        GradientBoosting,
        NeuralNetwork,
        LinearRegression,
        SVM
    };

    static std::unique_ptr<IModelTrainer> createTrainer(ModelType type);
    static std::vector<std::string> getAvailableModels();
    static ModelType getModelTypeFromString(const std::string& modelName);
};

// Cross-validation utilities
class CrossValidator {
public:
    struct CVResult {
        std::vector<double> foldScores;
        double meanScore;
        double stdScore;
        ModelMetrics aggregatedMetrics;
    };

    CrossValidator(int folds = 5) : m_folds(folds) {}

    CVResult performCrossValidation(
        std::unique_ptr<IModelTrainer>& trainer,
        const std::vector<FeatureEngineering::FeatureVector>& data);

    CVResult performTimeSeriesCrossValidation(
        std::unique_ptr<IModelTrainer>& trainer,
        const std::vector<FeatureEngineering::FeatureVector>& data);

private:
    int m_folds;

    std::vector<std::vector<FeatureEngineering::FeatureVector>> createFolds(
        const std::vector<FeatureEngineering::FeatureVector>& data);

    std::vector<std::vector<FeatureEngineering::FeatureVector>> createTimeSeriesFolds(
        const std::vector<FeatureEngineering::FeatureVector>& data);
};

// Hyperparameter optimization
class HyperparameterOptimizer {
public:
    struct ParameterRange {
        std::string name;
        double minValue;
        double maxValue;
        double stepSize;
        std::vector<double> discreteValues;
        bool isDiscrete;

        ParameterRange(const std::string& n, double min, double max, double step = 0.1)
            : name(n), minValue(min), maxValue(max), stepSize(step), isDiscrete(false) {}

        ParameterRange(const std::string& n, const std::vector<double>& values)
            : name(n), minValue(0), maxValue(0), stepSize(0), discreteValues(values), isDiscrete(true) {}
    };

    struct OptimizationResult {
        std::map<std::string, double> bestParameters;
        double bestScore;
        std::vector<std::map<std::string, double>> allParameters;
        std::vector<double> allScores;
        int iterations;
    };

    HyperparameterOptimizer() = default;

    OptimizationResult optimizeGridSearch(
        std::unique_ptr<IModelTrainer>& trainer,
        const std::vector<FeatureEngineering::FeatureVector>& data,
        const std::vector<ParameterRange>& parameterRanges);

    OptimizationResult optimizeRandomSearch(
        std::unique_ptr<IModelTrainer>& trainer,
        const std::vector<FeatureEngineering::FeatureVector>& data,
        const std::vector<ParameterRange>& parameterRanges,
        int iterations = 100);

private:
    std::vector<std::map<std::string, double>> generateParameterGrid(
        const std::vector<ParameterRange>& ranges);

    std::map<std::string, double> generateRandomParameters(
        const std::vector<ParameterRange>& ranges);

    double evaluateParameters(
        std::unique_ptr<IModelTrainer>& trainer,
        const std::vector<FeatureEngineering::FeatureVector>& data,
        const std::map<std::string, double>& parameters);
};

} // namespace MachineLearning
} // namespace CryptoClaude