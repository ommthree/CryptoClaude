#pragma once

#include <vector>
#include <string>
#include <map>
#include <memory>
#include <chrono>
#include <functional>

namespace CryptoClaude {
namespace Algorithm {

/**
 * Real Correlation Validator - Day 21 Core Implementation
 * Provides statistically rigorous correlation calculation and validation
 * Critical component for eliminating "simulated correlation fraud risk"
 * Ensures TRS compliance with ≥0.85 correlation target
 */
class RealCorrelationValidator {
public:
    // Statistical correlation result with full validation
    struct CorrelationResult {
        std::string pair_name;
        double pearson_correlation;              // Primary correlation coefficient
        double spearman_correlation;             // Rank-based correlation (robust to outliers)
        double kendall_tau;                      // Alternative rank correlation

        // Statistical significance testing
        double p_value_pearson;                  // p-value for Pearson correlation
        double p_value_spearman;                 // p-value for Spearman correlation
        double t_statistic;                      // t-test statistic
        int degrees_of_freedom;                  // n-2 for correlation testing

        // Confidence intervals
        double confidence_interval_lower;        // Lower bound (95% CI)
        double confidence_interval_upper;        // Upper bound (95% CI)
        double confidence_level;                 // Confidence level (e.g., 0.95)

        // Data quality assessment
        int sample_size;                         // Number of data points used
        int valid_pairs;                         // Non-missing paired observations
        double data_completeness;                // valid_pairs / expected_pairs

        // Correlation stability metrics
        std::vector<double> rolling_correlations; // Rolling correlation timeline
        double correlation_stability;            // Standard deviation of rolling correlations
        double correlation_trend;                // Linear trend in correlation over time

        // TRS compliance assessment
        bool meets_trs_target;                   // True if correlation ≥ 0.85
        double trs_gap;                          // 0.85 - current_correlation
        std::string trs_status;                  // "compliant", "warning", "critical"

        // Timestamps and metadata
        std::chrono::system_clock::time_point calculation_time;
        std::chrono::system_clock::time_point data_start_time;
        std::chrono::system_clock::time_point data_end_time;
        std::string calculation_method;          // "real_data", "simulated", "mixed"

        CorrelationResult() : pearson_correlation(0.0), spearman_correlation(0.0), kendall_tau(0.0),
                             p_value_pearson(1.0), p_value_spearman(1.0), t_statistic(0.0),
                             degrees_of_freedom(0), confidence_interval_lower(-1.0),
                             confidence_interval_upper(1.0), confidence_level(0.95),
                             sample_size(0), valid_pairs(0), data_completeness(0.0),
                             correlation_stability(0.0), correlation_trend(0.0),
                             meets_trs_target(false), trs_gap(0.85),
                             trs_status("unknown"), calculation_time(std::chrono::system_clock::now()) {}
    };

    // Configuration for correlation calculation
    struct CorrelationConfig {
        // Statistical parameters
        double confidence_level = 0.95;         // For confidence intervals
        double significance_alpha = 0.05;       // Significance level for p-values
        int minimum_sample_size = 30;           // Minimum data points for reliable correlation

        // TRS compliance parameters
        double trs_target_correlation = 0.85;   // Target correlation for TRS compliance
        double trs_warning_threshold = 0.80;    // Warning threshold
        double trs_critical_threshold = 0.75;   // Critical threshold

        // Rolling correlation parameters
        int rolling_window_size = 30;           // Window size for rolling correlations
        int rolling_step_size = 1;              // Step size for rolling calculation
        bool calculate_rolling = true;          // Whether to calculate rolling correlations

        // Data quality thresholds
        double min_data_completeness = 0.90;    // Minimum data completeness required
        int max_missing_consecutive = 3;        // Max consecutive missing points allowed

        // Outlier detection
        bool remove_outliers = true;            // Whether to detect and handle outliers
        double outlier_z_threshold = 3.0;       // Z-score threshold for outlier detection
        std::string outlier_method = "z_score";  // "z_score", "iqr", "modified_z_score"

        CorrelationConfig() {}
    };

    // Data point structure for correlation calculation
    struct DataPoint {
        std::chrono::system_clock::time_point timestamp;
        double value;
        bool is_valid;
        bool is_outlier;
        std::string source;

        DataPoint() : value(0.0), is_valid(true), is_outlier(false) {}
        DataPoint(const std::chrono::system_clock::time_point& t, double v)
            : timestamp(t), value(v), is_valid(true), is_outlier(false) {}
    };

    // Time series data for correlation analysis
    struct TimeSeries {
        std::string identifier;
        std::vector<DataPoint> data;
        std::chrono::system_clock::time_point start_time;
        std::chrono::system_clock::time_point end_time;

        void sortByTime() {
            std::sort(data.begin(), data.end(),
                     [](const DataPoint& a, const DataPoint& b) {
                         return a.timestamp < b.timestamp;
                     });
            if (!data.empty()) {
                start_time = data.front().timestamp;
                end_time = data.back().timestamp;
            }
        }

        std::vector<double> getValidValues() const {
            std::vector<double> values;
            for (const auto& point : data) {
                if (point.is_valid && !point.is_outlier) {
                    values.push_back(point.value);
                }
            }
            return values;
        }
    };

private:
    CorrelationConfig config_;

    // Core statistical calculation methods
    double calculatePearsonCorrelation(const std::vector<double>& x, const std::vector<double>& y);
    double calculateSpearmanCorrelation(const std::vector<double>& x, const std::vector<double>& y);
    double calculateKendallTau(const std::vector<double>& x, const std::vector<double>& y);

    // Statistical significance testing
    double calculatePearsonPValue(double correlation, int sample_size);
    double calculateSpearmanPValue(double correlation, int sample_size);
    double calculateTStatistic(double correlation, int sample_size);
    std::pair<double, double> calculateConfidenceInterval(double correlation, int sample_size, double confidence_level);

    // Data preprocessing and validation
    std::pair<TimeSeries, TimeSeries> alignTimeSeries(const TimeSeries& ts1, const TimeSeries& ts2);
    void detectOutliers(TimeSeries& ts);
    std::pair<std::vector<double>, std::vector<double>> extractAlignedValues(const TimeSeries& ts1, const TimeSeries& ts2);

    // Rolling correlation calculation
    std::vector<double> calculateRollingCorrelations(const TimeSeries& ts1, const TimeSeries& ts2);

    // Utility methods
    std::vector<double> calculateRanks(const std::vector<double>& values);
    double calculateLinearTrend(const std::vector<double>& y_values);
    bool isStatisticallySignificant(double p_value, double alpha = 0.05);

    // TRS compliance assessment
    std::string determineTRSStatus(double correlation, const CorrelationConfig& config);

public:
    explicit RealCorrelationValidator(const CorrelationConfig& config = CorrelationConfig());
    ~RealCorrelationValidator() = default;

    // Main correlation calculation interface
    CorrelationResult calculateCorrelation(const TimeSeries& series1, const TimeSeries& series2);

    // Convenience method for vector data
    CorrelationResult calculateCorrelation(const std::vector<double>& data1,
                                         const std::vector<double>& data2,
                                         const std::string& pair_name = "unknown");

    // Batch correlation calculation
    std::map<std::string, CorrelationResult> calculateMultipleCorrelations(
        const std::map<std::string, TimeSeries>& time_series_map,
        const std::vector<std::pair<std::string, std::string>>& pairs);

    // Real-time correlation monitoring
    struct RealTimeCorrelationMonitor {
        std::string pair_name;
        std::deque<DataPoint> data_buffer_1;
        std::deque<DataPoint> data_buffer_2;
        int buffer_size;
        CorrelationResult latest_result;
        std::chrono::system_clock::time_point last_update;

        RealTimeCorrelationMonitor(const std::string& name, int size = 100)
            : pair_name(name), buffer_size(size), last_update(std::chrono::system_clock::now()) {}
    };

    // Real-time monitoring methods
    std::unique_ptr<RealTimeCorrelationMonitor> createRealTimeMonitor(const std::string& pair_name, int buffer_size = 100);
    CorrelationResult updateRealTimeCorrelation(RealTimeCorrelationMonitor& monitor,
                                              const DataPoint& point1, const DataPoint& point2);

    // Historical correlation validation
    struct HistoricalValidationResult {
        std::string pair_name;
        std::vector<CorrelationResult> historical_correlations;
        double average_correlation;
        double correlation_volatility;
        double trend_stability_score;       // How stable the correlation trend is
        bool consistently_meets_trs;        // Whether correlation consistently meets TRS target
        int periods_above_target;           // Number of periods above TRS target
        int total_periods;                  // Total periods analyzed

        HistoricalValidationResult() : average_correlation(0.0), correlation_volatility(0.0),
                                     trend_stability_score(0.0), consistently_meets_trs(false),
                                     periods_above_target(0), total_periods(0) {}
    };

    HistoricalValidationResult validateHistoricalCorrelation(
        const std::vector<TimeSeries>& historical_series1,
        const std::vector<TimeSeries>& historical_series2,
        const std::string& pair_name,
        std::chrono::hours period_length = std::chrono::hours{24});

    // Algorithm prediction correlation validation
    struct PredictionCorrelationResult {
        std::string algorithm_id;
        std::vector<double> predictions;
        std::vector<double> actual_outcomes;
        CorrelationResult correlation_analysis;

        // Performance metrics
        double prediction_accuracy;          // Percentage of correct direction predictions
        double mean_absolute_error;          // Average magnitude error
        double root_mean_square_error;       // RMSE of predictions

        // Statistical validation
        bool predictions_are_significantly_correlated;
        double correlation_strength_rating;  // 0-100 score based on correlation and significance

        PredictionCorrelationResult() : prediction_accuracy(0.0), mean_absolute_error(0.0),
                                      root_mean_square_error(0.0), predictions_are_significantly_correlated(false),
                                      correlation_strength_rating(0.0) {}
    };

    PredictionCorrelationResult validateAlgorithmPredictions(
        const std::vector<double>& predictions,
        const std::vector<double>& actual_outcomes,
        const std::string& algorithm_id = "unknown");

    // Configuration management
    void updateConfiguration(const CorrelationConfig& new_config);
    const CorrelationConfig& getConfiguration() const { return config_; }

    // Diagnostic and reporting methods
    struct DiagnosticReport {
        std::string pair_name;
        std::chrono::system_clock::time_point report_time;

        // Data quality metrics
        int total_data_points_series1;
        int total_data_points_series2;
        int aligned_data_points;
        double data_alignment_quality;

        // Statistical validity
        bool sufficient_sample_size;
        bool data_passes_normality_tests;
        bool correlation_is_statistically_significant;

        // Outlier analysis
        int outliers_detected_series1;
        int outliers_detected_series2;
        double outlier_impact_on_correlation;

        // Recommendations
        std::vector<std::string> data_quality_issues;
        std::vector<std::string> statistical_warnings;
        std::vector<std::string> improvement_recommendations;

        DiagnosticReport() : total_data_points_series1(0), total_data_points_series2(0),
                           aligned_data_points(0), data_alignment_quality(0.0),
                           sufficient_sample_size(false), data_passes_normality_tests(false),
                           correlation_is_statistically_significant(false),
                           outliers_detected_series1(0), outliers_detected_series2(0),
                           outlier_impact_on_correlation(0.0) {}
    };

    DiagnosticReport generateDiagnosticReport(const TimeSeries& series1, const TimeSeries& series2);

    // Export and visualization support
    bool exportCorrelationResults(const CorrelationResult& result, const std::string& filename, const std::string& format = "csv");
    bool exportTimeSeries(const TimeSeries& series, const std::string& filename);

    // Static utility methods for external use
    static double calculateSimpleCorrelation(const std::vector<double>& x, const std::vector<double>& y);
    static bool isCorrelationStatisticallySignificant(double correlation, int sample_size, double alpha = 0.05);
    static std::string interpretCorrelationStrength(double correlation);
    static double calculateCorrelationPower(int sample_size, double effect_size, double alpha = 0.05);
};

} // namespace Algorithm
} // namespace CryptoClaude