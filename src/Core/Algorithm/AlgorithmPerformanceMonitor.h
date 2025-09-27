#pragma once

#include "AlgorithmDecisionEngine.h"
#include "../Database/DatabaseManager.h"
#include <memory>
#include <vector>
#include <map>
#include <chrono>
#include <atomic>
#include <thread>
#include <mutex>

namespace CryptoClaude {
namespace Algorithm {

/**
 * Algorithm Performance Monitor - Day 20 Implementation
 * Real-time performance tracking and correlation measurement
 * Critical component for achieving and maintaining ≥0.85 correlation target
 */
class AlgorithmPerformanceMonitor {
public:
    // Performance monitoring configuration
    struct MonitoringConfig {
        // Measurement intervals
        std::chrono::minutes real_time_interval{5};        // Real-time performance updates
        std::chrono::hours correlation_update_interval{1}; // Correlation recalculation
        std::chrono::hours performance_report_interval{6}; // Detailed performance reports

        // Performance thresholds
        double target_correlation = 0.85;                  // TRS compliance target
        double min_acceptable_correlation = 0.70;          // Minimum acceptable correlation
        double performance_alert_threshold = 0.60;         // Alert threshold

        // Tracking parameters
        int min_decisions_for_correlation = 30;            // Minimum decisions for correlation calc
        int rolling_window_hours = 168;                    // 1 week rolling window
        int max_decision_tracking = 1000;                  // Maximum decisions to track

        // Alert settings
        bool enable_real_time_alerts = true;               // Real-time performance alerts
        bool enable_correlation_alerts = true;             // Correlation threshold alerts
        bool enable_performance_logging = true;            // Detailed performance logging
    };

    // Real-time performance metrics
    struct RealTimeMetrics {
        // Current performance
        double current_correlation;                         // Current algorithm correlation
        double current_accuracy;                           // Current prediction accuracy
        double current_sharpe_ratio;                       // Risk-adjusted performance

        // Recent performance (last 24 hours)
        double daily_return;                               // Today's algorithm return
        double daily_accuracy;                             // Today's prediction accuracy
        int daily_decisions_made;                          // Decisions made today
        int daily_profitable_decisions;                    // Profitable decisions today

        // Rolling performance (configurable window)
        double rolling_correlation;                        // Rolling correlation
        double rolling_return;                             // Rolling total return
        double rolling_volatility;                         // Rolling volatility
        double rolling_max_drawdown;                       // Rolling maximum drawdown

        // Decision statistics
        int total_decisions_tracked;                       // Total decisions in tracking window
        int profitable_decisions;                          // Profitable decisions count
        double hit_rate;                                   // Percentage of profitable decisions

        // Timing information
        std::chrono::system_clock::time_point last_update;
        std::chrono::system_clock::time_point measurement_start;
        std::chrono::hours measurement_period;

        RealTimeMetrics() : current_correlation(0.0), current_accuracy(0.0), current_sharpe_ratio(0.0),
                          daily_return(0.0), daily_accuracy(0.0), daily_decisions_made(0),
                          daily_profitable_decisions(0), rolling_correlation(0.0), rolling_return(0.0),
                          rolling_volatility(0.0), rolling_max_drawdown(0.0), total_decisions_tracked(0),
                          profitable_decisions(0), hit_rate(0.0), measurement_period(std::chrono::hours{24}) {}
    };

    // Decision tracking for correlation calculation
    struct DecisionOutcome {
        std::string decision_id;
        std::string pair_name;
        AlgorithmDecisionEngine::TradingDecision::Action action;

        // Prediction vs actual
        double predicted_return;
        double actual_return;
        double prediction_error;

        // Decision details
        double confidence_score;
        double recommended_weight;
        std::chrono::system_clock::time_point decision_time;
        std::chrono::system_clock::time_point outcome_time;

        // Performance metrics
        bool was_profitable;
        double return_contribution;                        // Contribution to portfolio return
        double risk_adjusted_return;

        DecisionOutcome() : predicted_return(0.0), actual_return(0.0), prediction_error(0.0),
                          confidence_score(0.0), recommended_weight(0.0), was_profitable(false),
                          return_contribution(0.0), risk_adjusted_return(0.0) {}
    };

    // Performance alerts
    struct PerformanceAlert {
        enum class AlertType {
            CORRELATION_BELOW_TARGET,
            CORRELATION_BELOW_MINIMUM,
            PERFORMANCE_DEGRADATION,
            HIGH_PREDICTION_ERROR,
            UNUSUAL_MARKET_CONDITIONS,
            SYSTEM_PERFORMANCE_ISSUE
        } type;

        enum class Severity {
            INFO,
            WARNING,
            CRITICAL
        } severity;

        std::string message;
        std::string details;
        std::map<std::string, double> metrics;
        std::chrono::system_clock::time_point alert_time;

        PerformanceAlert() : type(AlertType::SYSTEM_PERFORMANCE_ISSUE), severity(Severity::INFO),
                           alert_time(std::chrono::system_clock::now()) {}
    };

    // Detailed performance analysis
    struct PerformanceReport {
        std::chrono::system_clock::time_point report_time;
        std::chrono::system_clock::time_point period_start;
        std::chrono::system_clock::time_point period_end;

        // Overall performance
        RealTimeMetrics overall_metrics;

        // Performance breakdown by action type
        std::map<AlgorithmDecisionEngine::TradingDecision::Action, double> action_performance;

        // Performance breakdown by pair
        std::map<std::string, double> pair_performance;

        // Performance breakdown by time period
        std::vector<std::pair<std::chrono::system_clock::time_point, double>> time_series_performance;

        // Statistical analysis
        double correlation_confidence_interval_low;
        double correlation_confidence_interval_high;
        double statistical_significance_p_value;

        // Prediction analysis
        std::vector<double> prediction_errors;
        double mean_prediction_error;
        double prediction_error_std_dev;

        // Risk analysis
        double var_95;                                     // 95% Value at Risk
        double expected_shortfall;                         // Expected Shortfall
        double beta_to_market;                            // Market beta

        // Recommendations
        std::vector<std::string> performance_recommendations;
        std::vector<PerformanceAlert> alerts_generated;
    };

private:
    MonitoringConfig config_;
    std::unique_ptr<Database::DatabaseManager> db_manager_;

    // Real-time tracking
    std::atomic<bool> monitoring_active_;
    std::thread monitoring_thread_;
    std::mutex data_mutex_;

    // Performance data
    RealTimeMetrics current_metrics_;
    std::vector<DecisionOutcome> decision_outcomes_;
    std::vector<PerformanceAlert> recent_alerts_;

    // Calculation methods
    double calculatePearsonCorrelation(const std::vector<double>& predictions,
                                      const std::vector<double>& outcomes);
    double calculateStatisticalSignificance(double correlation, int sample_size);
    std::pair<double, double> calculateCorrelationConfidenceInterval(double correlation,
                                                                    int sample_size,
                                                                    double confidence_level = 0.95);

    // Performance analysis methods
    double calculateSharpeRatio(const std::vector<double>& returns);
    double calculateMaxDrawdown(const std::vector<double>& cumulative_returns);
    double calculateVaR(const std::vector<double>& returns, double confidence_level = 0.95);

    // Alert generation
    void checkPerformanceThresholds();
    void generateAlert(PerformanceAlert::AlertType type,
                      PerformanceAlert::Severity severity,
                      const std::string& message,
                      const std::map<std::string, double>& metrics = {});

    // Data management
    void updateRealTimeMetrics();
    void cleanupOldData();
    void savePerformanceData();

    // Monitoring thread
    void monitoringLoop();

public:
    explicit AlgorithmPerformanceMonitor(const MonitoringConfig& config = MonitoringConfig{});
    ~AlgorithmPerformanceMonitor();

    // Lifecycle management
    bool initialize();
    bool start();
    void stop();
    bool isRunning() const;

    // Decision tracking interface - main interface for algorithm integration
    void recordDecision(const AlgorithmDecisionEngine::TradingDecision& decision);
    void recordDecisionOutcome(const std::string& decision_id, double actual_return);

    // Real-time performance access
    RealTimeMetrics getCurrentMetrics() const;
    double getCurrentCorrelation() const;
    bool meetsTargetCorrelation() const;
    bool meetsMinimumCorrelation() const;

    // Performance analysis
    PerformanceReport generatePerformanceReport(
        const std::chrono::system_clock::time_point& start_time,
        const std::chrono::system_clock::time_point& end_time) const;

    PerformanceReport generateDailyReport() const;
    PerformanceReport generateWeeklyReport() const;

    // Alert management
    std::vector<PerformanceAlert> getRecentAlerts(int max_alerts = 10) const;
    std::vector<PerformanceAlert> getUnresolvedAlerts() const;
    void acknowledgeAlert(const PerformanceAlert& alert);

    // Historical data access
    std::vector<DecisionOutcome> getDecisionOutcomes(int max_outcomes = 100) const;
    std::vector<DecisionOutcome> getDecisionOutcomesForPair(const std::string& pair,
                                                           int max_outcomes = 50) const;

    // Performance optimization recommendations
    struct OptimizationRecommendation {
        std::string category;                              // e.g., "Signal Generation", "Risk Management"
        std::string recommendation;                        // Specific recommendation
        double potential_impact;                           // Estimated correlation improvement
        std::string implementation_priority;               // "High", "Medium", "Low"
        std::vector<std::string> required_actions;        // Specific actions needed
    };

    std::vector<OptimizationRecommendation> generateOptimizationRecommendations() const;

    // Configuration management
    void updateConfiguration(const MonitoringConfig& new_config);
    const MonitoringConfig& getConfiguration() const { return config_; }

    // Database integration
    bool saveDecisionOutcome(const DecisionOutcome& outcome);
    std::vector<DecisionOutcome> loadHistoricalOutcomes(int days = 30);

    // Performance benchmarking
    struct BenchmarkComparison {
        double algorithm_return;
        double benchmark_return;                           // Market benchmark return
        double excess_return;                              // Algorithm - benchmark
        double information_ratio;                          // Excess return / tracking error
        double tracking_error;                             // Volatility of excess returns
    };

    BenchmarkComparison compareToBenchmark(const std::string& benchmark_symbol = "BTC") const;

    // Real-time correlation monitoring (critical for TRS compliance)
    struct CorrelationMonitoring {
        double current_correlation;
        double target_correlation;
        bool meets_target;
        std::chrono::system_clock::time_point last_calculation;
        int sample_size;
        double statistical_significance;
        std::vector<std::pair<std::chrono::system_clock::time_point, double>> correlation_history;
    };

    CorrelationMonitoring getCorrelationMonitoring() const;

    // Performance visualization data
    struct VisualizationData {
        std::vector<std::pair<std::chrono::system_clock::time_point, double>> correlation_timeline;
        std::vector<std::pair<std::chrono::system_clock::time_point, double>> returns_timeline;
        std::vector<std::pair<std::chrono::system_clock::time_point, double>> accuracy_timeline;
        std::vector<std::pair<double, double>> prediction_vs_actual_scatter;
    };

    VisualizationData getVisualizationData() const;
};

/**
 * Performance Alert Manager - Manages performance alerts and notifications
 */
class PerformanceAlertManager {
public:
    using AlertCallback = std::function<void(const AlgorithmPerformanceMonitor::PerformanceAlert&)>;

    // Register callback for specific alert types
    void registerAlertCallback(AlgorithmPerformanceMonitor::PerformanceAlert::AlertType type,
                              AlertCallback callback);

    // Process incoming alert
    void processAlert(const AlgorithmPerformanceMonitor::PerformanceAlert& alert);

    // Alert suppression (prevent spam)
    void suppressAlertType(AlgorithmPerformanceMonitor::PerformanceAlert::AlertType type,
                          std::chrono::minutes duration);

private:
    std::map<AlgorithmPerformanceMonitor::PerformanceAlert::AlertType, std::vector<AlertCallback>> callbacks_;
    std::map<AlgorithmPerformanceMonitor::PerformanceAlert::AlertType, std::chrono::system_clock::time_point> suppression_until_;
    std::mutex callback_mutex_;
};

/**
 * Real-Time Correlation Tracker - Specialized tracker for TRS compliance
 */
class RealTimeCorrelationTracker {
public:
    struct CorrelationUpdate {
        double correlation_value;
        int sample_size;
        double p_value;
        bool statistically_significant;
        std::chrono::system_clock::time_point update_time;
    };

    // Track prediction and outcome pairs
    void addPredictionOutcomePair(double prediction, double outcome);

    // Get current correlation
    CorrelationUpdate getCurrentCorrelation() const;

    // Check if correlation meets TRS requirements
    bool meetsTRSRequirements(double minimum_correlation = 0.85,
                             double significance_level = 0.05) const;

    // Get correlation trend (improving/declining)
    std::string getCorrelationTrend() const;

private:
    std::vector<std::pair<double, double>> prediction_outcome_pairs_;
    mutable std::mutex data_mutex_;

    double calculatePearsonCorrelation() const;
    double calculatePValue(double correlation, int sample_size) const;
};

} // namespace Algorithm
} // namespace CryptoClaude