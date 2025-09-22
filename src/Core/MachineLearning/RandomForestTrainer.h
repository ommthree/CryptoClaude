#pragma once
#include "IModelTrainer.h"
#include <alglib/dataanalysis.h>

namespace CryptoClaude {
namespace MachineLearning {

class RandomForestTrainer : public IModelTrainer {
public:
    RandomForestTrainer();
    ~RandomForestTrainer() override = default;

    // IModelTrainer interface implementation
    std::string getName() const override { return "Random Forest"; }
    std::string getDescription() const override { return "Random Forest regression using ALGLIB"; }
    std::string getVersion() const override { return "1.0"; }

    bool train(const std::vector<FeatureEngineering::FeatureVector>& trainingData) override;
    std::vector<PredictionResult> predict(const std::vector<FeatureEngineering::FeatureVector>& features) override;
    double predict(const FeatureEngineering::FeatureVector& feature) override;

    ModelMetrics evaluate(const std::vector<FeatureEngineering::FeatureVector>& testData) override;
    std::vector<PredictionResult> validatePredictions(const std::vector<FeatureEngineering::FeatureVector>& validationData) override;

    bool saveModel(const std::string& filepath) override;
    bool loadModel(const std::string& filepath) override;

    void setHyperParameters(const std::map<std::string, double>& params) override;
    std::map<std::string, double> getHyperParameters() const override;

    bool isTrained() const override { return m_isTrained; }
    ModelMetrics getLastTrainingMetrics() const override { return m_lastMetrics; }
    std::string getLastError() const override { return m_lastError; }

    std::vector<std::string> getRequiredFeatures() const override;
    bool validateFeatures(const std::vector<FeatureEngineering::FeatureVector>& features) override;

    // Random Forest specific methods
    void setTreeCount(int treeCount) { m_treeCount = treeCount; }
    void setSampleRatio(double ratio) { m_sampleRatio = ratio; }
    void setRandomSeed(int seed) { m_randomSeed = seed; }
    void setImportanceCalculation(bool enabled) { m_calculateImportance = enabled; }

    std::vector<double> getFeatureImportances() const { return m_featureImportances; }
    std::vector<std::string> getFeatureNames() const { return m_featureNames; }

    // Model diagnostics
    struct DiagnosticInfo {
        std::vector<PredictionResult> predictions;
        std::vector<double> residuals;
        std::vector<double> absoluteErrors;
        std::vector<PredictionResult> worstPredictions;
        std::map<std::string, double> errorStatistics;
    };

    DiagnosticInfo generateDiagnostics(const std::vector<FeatureEngineering::FeatureVector>& data);
    bool exportDiagnosticsToDatabase(const DiagnosticInfo& diagnostics);

private:
    // ALGLIB objects
    alglib::decisionforest m_model;
    alglib::dfreport m_report;

    // Model state
    bool m_isTrained;
    ModelMetrics m_lastMetrics;
    std::string m_lastError;

    // Hyperparameters
    int m_treeCount;
    double m_sampleRatio;
    int m_randomSeed;
    bool m_calculateImportance;

    // Feature information
    std::vector<std::string> m_featureNames;
    std::vector<double> m_featureImportances;
    int m_featureCount;

    // Training data statistics (for normalization if needed)
    std::vector<double> m_featureMeans;
    std::vector<double> m_featureStds;
    double m_targetMean;
    double m_targetStd;

    // Helper methods
    bool convertToAlglibFormat(
        const std::vector<FeatureEngineering::FeatureVector>& data,
        alglib::real_2d_array& dataset,
        alglib::real_1d_array& labels);

    PredictionResult createPredictionResult(
        const FeatureEngineering::FeatureVector& feature,
        double prediction);

    void calculateFeatureImportances();
    void updateLastMetrics();

    // Data validation
    bool validateDataConsistency(const std::vector<FeatureEngineering::FeatureVector>& data);
    bool checkForMissingValues(const std::vector<FeatureEngineering::FeatureVector>& data);

    // Statistical utilities
    double calculateRMSE(const std::vector<double>& actual, const std::vector<double>& predicted);
    double calculateMAE(const std::vector<double>& actual, const std::vector<double>& predicted);
    double calculateRSquared(const std::vector<double>& actual, const std::vector<double>& predicted);

    // Error handling
    void setError(const std::string& error);
    void clearError() { m_lastError.clear(); }

    // Serialization helpers (for save/load functionality)
    struct ModelState {
        std::vector<std::string> featureNames;
        std::vector<double> featureImportances;
        std::vector<double> featureMeans;
        std::vector<double> featureStds;
        double targetMean;
        double targetStd;
        int treeCount;
        double sampleRatio;
        int randomSeed;
        ModelMetrics metrics;
    };

    bool saveModelState(const std::string& filepath, const ModelState& state);
    bool loadModelState(const std::string& filepath, ModelState& state);
};

// Specialized Random Forest utilities
class RandomForestUtils {
public:
    // Feature selection using Random Forest feature importance
    static std::vector<std::string> selectImportantFeatures(
        const RandomForestTrainer& trainer,
        double importanceThreshold = 0.01);

    // Ensemble methods
    static std::vector<PredictionResult> averageEnsemblePredictions(
        const std::vector<std::vector<PredictionResult>>& predictions);

    // Tree analysis
    static void analyzeTreeStructure(const alglib::decisionforest& forest);

    // Performance analysis
    static void plotFeatureImportances(
        const std::vector<std::string>& featureNames,
        const std::vector<double>& importances,
        const std::string& outputFile);

private:
    static double calculateEnsembleVariance(
        const std::vector<std::vector<PredictionResult>>& predictions);
};

} // namespace MachineLearning
} // namespace CryptoClaude