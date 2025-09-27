#pragma once

#include "VaRCalculator.h"
#include "../Data/HistoricalDataManager.h"
#include "../Database/DatabaseManager.h"
#include <memory>
#include <map>
#include <vector>
#include <chrono>

namespace CryptoClaude {
namespace Risk {

/**
 * VaR Model Recalibrator - Day 28 Production Calibration
 * Recalibrates all VaR methodologies with comprehensive historical data
 * Target: Improve VaR accuracy from 25% to 85%+ through real data calibration
 */
class VaRModelRecalibrator {
public:
    // Recalibration configuration
    struct RecalibrationConfig {
        // Target accuracy requirements
        double target_accuracy_percentage = 85.0;      // 85%+ accuracy target
        double minimum_accuracy_threshold = 70.0;      // Minimum acceptable accuracy

        // Statistical validation parameters
        double confidence_level_95 = 0.95;             // 95% confidence level for VaR
        double confidence_level_99 = 0.99;             // 99% confidence level for VaR
        int backtesting_window_days = 252;             // 1 year backtesting window
        double kupiec_test_significance = 0.05;        // 5% significance level for Kupiec test

        // Performance requirements
        int max_calculation_time_ms = 100;             // Sub-100ms performance requirement
        bool enable_parallel_processing = true;        // Enable multi-threading
        int max_concurrent_calculations = 4;           // Concurrent VaR calculations

        // Model calibration parameters
        double volatility_decay_factor = 0.94;         // EWMA decay factor (0.94 is RiskMetrics standard)
        int correlation_estimation_window = 252;       // 1 year correlation estimation window
        int monte_carlo_simulations = 10000;          // Monte Carlo simulation count
        bool enable_variance_reduction = true;         // Monte Carlo variance reduction

        RecalibrationConfig() = default;
    };

    // VaR model accuracy metrics
    struct VaRAccuracyMetrics {
        std::string model_name;
        double accuracy_percentage;                     // Overall accuracy percentage
        double coverage_ratio_95;                       // 95% VaR coverage ratio
        double coverage_ratio_99;                       // 99% VaR coverage ratio

        // Statistical test results
        double kupiec_test_pvalue_95;                   // Kupiec test p-value for 95% VaR
        double kupiec_test_pvalue_99;                   // Kupiec test p-value for 99% VaR
        bool passes_kupiec_test;                        // True if passes Kupiec test

        // Violation clustering analysis
        int total_violations_95;                        // Total 95% VaR violations
        int total_violations_99;                        // Total 99% VaR violations
        double max_consecutive_violations;              // Maximum consecutive violations
        double average_violation_magnitude;             // Average magnitude of violations

        // Performance metrics
        std::chrono::milliseconds avg_calculation_time; // Average calculation time
        std::chrono::milliseconds max_calculation_time; // Maximum calculation time
        bool meets_performance_requirements;            // True if meets <100ms requirement

        // Improvement metrics
        double accuracy_improvement;                    // Improvement vs previous calibration
        double pre_calibration_accuracy;               // Accuracy before recalibration

        std::chrono::system_clock::time_point calibration_time;
        std::vector<std::string> validation_notes;     // Additional validation notes

        VaRAccuracyMetrics() : accuracy_percentage(0), coverage_ratio_95(0), coverage_ratio_99(0),
                              kupiec_test_pvalue_95(0), kupiec_test_pvalue_99(0), passes_kupiec_test(false),
                              total_violations_95(0), total_violations_99(0), max_consecutive_violations(0),
                              average_violation_magnitude(0), avg_calculation_time(0), max_calculation_time(0),
                              meets_performance_requirements(false), accuracy_improvement(0),
                              pre_calibration_accuracy(0) {}
    };

    // Comprehensive recalibration result
    struct RecalibrationResult {
        bool success;
        std::string error_message;

        // Individual model metrics
        VaRAccuracyMetrics parametric_var_metrics;
        VaRAccuracyMetrics historical_simulation_metrics;
        VaRAccuracyMetrics monte_carlo_metrics;
        VaRAccuracyMetrics cornish_fisher_metrics;

        // Overall statistics
        double overall_accuracy_improvement;           // Weighted average improvement
        double best_model_accuracy;                    // Best performing model accuracy
        std::string best_model_name;                   // Name of best performing model

        // Validation results
        bool meets_target_accuracy;                    // True if meets 85%+ target
        bool meets_performance_requirements;           // True if meets <100ms requirement
        bool passes_statistical_validation;           // True if passes all statistical tests

        // Recalibration statistics
        std::chrono::milliseconds total_recalibration_time;
        int symbols_calibrated;
        int correlation_pairs_updated;
        std::chrono::system_clock::time_point completion_time;

        RecalibrationResult() : success(false), overall_accuracy_improvement(0), best_model_accuracy(0),
                               meets_target_accuracy(false), meets_performance_requirements(false),
                               passes_statistical_validation(false), total_recalibration_time(0),
                               symbols_calibrated(0), correlation_pairs_updated(0) {}
    };

public:
    explicit VaRModelRecalibrator(
        std::shared_ptr<VaRCalculator> var_calculator,
        std::shared_ptr<Data::HistoricalDataManager> historical_data_manager,
        std::shared_ptr<Database::DatabaseManager> db_manager,
        const RecalibrationConfig& config = RecalibrationConfig()
    );

    // === CORE RECALIBRATION METHODS ===

    // Execute comprehensive VaR model recalibration
    RecalibrationResult executeComprehensiveRecalibration();

    // Recalibrate specific VaR methodology
    VaRAccuracyMetrics recalibrateParametricVaR();
    VaRAccuracyMetrics recalibrateHistoricalSimulationVaR();
    VaRAccuracyMetrics recalibrateMonteCarloVaR();
    VaRAccuracyMetrics recalibrateCornishFisherVaR();

    // === STATISTICAL VALIDATION ===

    // Comprehensive backtesting framework
    VaRAccuracyMetrics performComprehensiveBacktesting(const std::string& model_name,
                                                        VaRCalculator::VaRMethodology methodology);

    // Kupiec likelihood ratio test
    double performKupiecTest(const std::vector<double>& var_estimates,
                            const std::vector<double>& actual_returns,
                            double confidence_level);

    // Coverage ratio testing
    double calculateCoverageRatio(const std::vector<double>& var_estimates,
                                 const std::vector<double>& actual_returns,
                                 double confidence_level);

    // Independence testing for VaR violations
    bool testVaRViolationIndependence(const std::vector<bool>& violations);

    // === PERFORMANCE VALIDATION ===

    // Benchmark VaR calculation performance
    std::chrono::milliseconds benchmarkVaRPerformance(VaRCalculator::VaRMethodology methodology,
                                                       int iterations = 1000);

    // Validate performance requirements
    bool validatePerformanceRequirements();

    // === MODEL PARAMETER OPTIMIZATION ===

    // Optimize EWMA decay factor for parametric VaR
    double optimizeEWMADecayFactor();

    // Optimize Monte Carlo simulation count
    int optimizeMonteCarloSimulationCount();

    // Optimize correlation estimation window
    int optimizeCorrelationEstimationWindow();

    // === HISTORICAL DATA INTEGRATION ===

    // Update volatility estimates with historical data
    bool updateVolatilityEstimates();

    // Update correlation matrices with historical correlations
    bool updateCorrelationMatrices();

    // Validate historical data quality for VaR calibration
    bool validateHistoricalDataQuality();

    // === REPORTING AND ANALYTICS ===

    // Generate comprehensive recalibration report
    struct RecalibrationReport {
        RecalibrationResult summary;
        std::map<std::string, VaRAccuracyMetrics> detailed_metrics;
        std::vector<std::pair<std::string, double>> parameter_changes;
        std::string recommendations;
        std::chrono::system_clock::time_point report_time;
    };

    RecalibrationReport generateComprehensiveReport(const RecalibrationResult& result);

    // Export recalibration results
    bool exportRecalibrationResults(const RecalibrationResult& result, const std::string& filename);

    // === CONFIGURATION AND UTILITIES ===

    void updateConfig(const RecalibrationConfig& config) { config_ = config; }
    const RecalibrationConfig& getConfig() const { return config_; }

    // Get pre-recalibration baseline metrics
    VaRAccuracyMetrics getBaselineAccuracyMetrics(VaRCalculator::VaRMethodology methodology);

private:
    // Dependencies and configuration
    RecalibrationConfig config_;
    std::shared_ptr<VaRCalculator> var_calculator_;
    std::shared_ptr<Data::HistoricalDataManager> historical_data_manager_;
    std::shared_ptr<Database::DatabaseManager> db_manager_;

    // Calibration state
    std::map<std::string, VaRAccuracyMetrics> baseline_metrics_;
    std::chrono::system_clock::time_point recalibration_start_time_;

    // Helper methods
    std::vector<double> loadHistoricalReturns(const std::string& symbol, int days);
    std::vector<double> calculateRollingVaR(VaRCalculator::VaRMethodology methodology,
                                           const std::string& symbol,
                                           int window_size);

    // Statistical helper functions
    double calculateLikelihoodRatio(int violations, int total_observations, double expected_rate);
    double calculateChiSquaredPValue(double chi_squared, int degrees_of_freedom);
    bool detectViolationClustering(const std::vector<bool>& violations);

    // Performance optimization
    void optimizeCalculationPerformance();
    bool enableParallelProcessing();

    // Database integration
    bool storeRecalibrationResults(const RecalibrationResult& result);
    bool loadBaselineMetrics();

    // Validation helpers
    bool validateModelParameters(VaRCalculator::VaRMethodology methodology);
    double calculateAccuracyImprovement(double new_accuracy, double baseline_accuracy);
};

}} // namespace CryptoClaude::Risk