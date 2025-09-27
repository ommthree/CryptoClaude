#pragma once

#include "RealTimeSignalProcessor.h"
#include "RealCorrelationValidator.h"
#include "../Database/DatabaseManager.h"
#include <memory>
#include <vector>
#include <map>
#include <chrono>
#include <atomic>
#include <mutex>
#include <thread>
#include <queue>

namespace CryptoClaude {
namespace Algorithm {

/**
 * TRS Compliance Engine - Day 22 Implementation
 * Continuous monitoring and enforcement of Target Risk Standard (TRS) compliance
 * Ensures algorithm maintains ≥0.85 correlation with regulatory requirements
 */
class TRSComplianceEngine {
public:
    // TRS compliance configuration
    struct TRSConfig {
        // Core TRS requirements
        double target_correlation = 0.85;       // Primary TRS target
        double warning_threshold = 0.80;        // Warning level
        double critical_threshold = 0.75;       // Critical level
        double emergency_threshold = 0.70;      // Emergency stop level

        // Monitoring parameters
        std::chrono::minutes monitoring_interval{1};    // 1-minute monitoring
        int correlation_measurement_window = 30;        // 30 data points for correlation
        int trend_analysis_window = 60;                 // 1 hour trend analysis

        // Compliance requirements
        std::chrono::hours compliance_evaluation_period{24};   // 24-hour compliance evaluation
        double minimum_compliance_percentage = 80.0;           // 80% of time must be compliant
        int max_consecutive_violations = 3;                    // Max consecutive violations

        // Corrective action parameters
        bool enable_automatic_correction = true;
        double correlation_boost_factor = 0.05;               // 5% boost attempt
        std::chrono::minutes correction_timeout{10};          // Time to allow correction

        // Alert and reporting
        bool enable_real_time_alerts = true;
        bool enable_regulatory_reporting = true;
        std::chrono::hours reporting_frequency{6};             // 6-hour reports

        TRSConfig() {}
    };

    // TRS compliance status
    enum class ComplianceStatus {
        COMPLIANT,      // Above target correlation
        WARNING,        // Below target but above warning threshold
        CRITICAL,       // Below warning threshold
        EMERGENCY,      // Below emergency threshold - immediate action required
        UNKNOWN         // Insufficient data for assessment
    };

    // Real-time compliance measurement
    struct ComplianceMeasurement {
        std::chrono::system_clock::time_point timestamp;
        double measured_correlation;
        double target_correlation;
        double correlation_gap;              // target - measured
        ComplianceStatus status;

        // Statistical validation
        double p_value;                      // Statistical significance
        double confidence_interval_lower;
        double confidence_interval_upper;
        int sample_size;

        // Context information
        int algorithm_signals_count;
        double data_quality_score;
        std::string measurement_method;      // "realtime", "windowed", "historical"

        // Trend analysis
        double correlation_trend;            // Trend over recent period
        double trend_stability;              // Stability of trend

        ComplianceMeasurement() : measured_correlation(0), target_correlation(0.85),
                                 correlation_gap(0.85), status(ComplianceStatus::UNKNOWN),
                                 p_value(1.0), confidence_interval_lower(0), confidence_interval_upper(1),
                                 sample_size(0), algorithm_signals_count(0), data_quality_score(0),
                                 correlation_trend(0), trend_stability(0) {}
    };

    // Compliance violation record
    struct ComplianceViolation {
        std::string violation_id;
        std::chrono::system_clock::time_point detected_at;
        std::chrono::system_clock::time_point resolved_at;
        ComplianceStatus severity;

        // Violation details
        double correlation_at_violation;
        double correlation_gap;
        std::string violation_trigger;       // What triggered the violation
        std::vector<std::string> contributing_factors;

        // Response and resolution
        std::vector<std::string> corrective_actions_taken;
        bool was_auto_corrected;
        std::chrono::minutes resolution_time;
        std::string resolution_method;

        // Impact assessment
        double estimated_impact;             // Impact on algorithm performance
        int affected_signals;               // Number of signals affected
        bool regulatory_reportable;         // Whether violation must be reported

        ComplianceViolation() : severity(ComplianceStatus::UNKNOWN), correlation_at_violation(0),
                               correlation_gap(0), was_auto_corrected(false),
                               resolution_time(std::chrono::minutes{0}), estimated_impact(0),
                               affected_signals(0), regulatory_reportable(false) {}
    };

    // Compliance performance metrics
    struct CompliancePerformance {
        std::chrono::system_clock::time_point evaluation_period_start;
        std::chrono::system_clock::time_point evaluation_period_end;

        // Compliance statistics
        double average_correlation;
        double correlation_volatility;       // Standard deviation
        double time_in_compliance_percentage;
        int total_measurements;
        int compliant_measurements;

        // Violation statistics
        int total_violations;
        int warning_violations;
        int critical_violations;
        int emergency_violations;
        std::chrono::minutes average_resolution_time;

        // Performance trends
        double correlation_improvement_trend;
        double compliance_trend;             // Improving/deteriorating compliance
        bool is_performance_acceptable;

        CompliancePerformance() : average_correlation(0), correlation_volatility(0),
                                 time_in_compliance_percentage(0), total_measurements(0),
                                 compliant_measurements(0), total_violations(0),
                                 warning_violations(0), critical_violations(0),
                                 emergency_violations(0), average_resolution_time(std::chrono::minutes{0}),
                                 correlation_improvement_trend(0), compliance_trend(0),
                                 is_performance_acceptable(false) {}
    };

    // Corrective action framework
    struct CorrectiveAction {
        std::string action_id;
        std::string action_type;             // "parameter_adjustment", "signal_filtering", etc.
        std::chrono::system_clock::time_point initiated_at;
        std::chrono::system_clock::time_point completed_at;

        // Action parameters
        std::map<std::string, double> parameter_changes;
        double expected_correlation_improvement;
        std::chrono::minutes expected_resolution_time;

        // Results
        bool was_successful;
        double actual_correlation_improvement;
        std::vector<std::string> side_effects;
        std::string failure_reason;         // If unsuccessful

        CorrectiveAction() : expected_correlation_improvement(0),
                           expected_resolution_time(std::chrono::minutes{0}),
                           was_successful(false), actual_correlation_improvement(0) {}
    };

private:
    // Core components
    TRSConfig config_;
    std::unique_ptr<RealTimeSignalProcessor> signal_processor_;
    std::unique_ptr<RealCorrelationValidator> correlation_validator_;
    std::unique_ptr<Database::DatabaseManager> db_manager_;

    // State management
    std::atomic<bool> is_monitoring_{false};
    std::atomic<ComplianceStatus> current_status_{ComplianceStatus::UNKNOWN};
    mutable std::mutex compliance_mutex_;
    mutable std::mutex violation_mutex_;

    // Threading
    std::thread monitoring_thread_;
    std::thread alert_thread_;

    // Data storage
    std::deque<ComplianceMeasurement> measurement_history_;
    std::vector<ComplianceViolation> violation_history_;
    std::queue<CorrectiveAction> pending_actions_;

    // Alert and callback system
    std::function<void(const ComplianceMeasurement&)> measurement_callback_;
    std::function<void(const ComplianceViolation&)> violation_callback_;
    std::function<void(const CorrectiveAction&)> action_callback_;
    std::function<void(const std::string&)> regulatory_callback_;

    // Core monitoring methods
    void runComplianceMonitoring();
    ComplianceMeasurement performComplianceMeasurement();
    void evaluateComplianceStatus(const ComplianceMeasurement& measurement);
    void handleComplianceViolation(const ComplianceMeasurement& measurement);

    // Corrective action methods
    void initiateCorrectiveAction(const ComplianceViolation& violation);
    CorrectiveAction designCorrectiveAction(const ComplianceViolation& violation);
    bool executeCorrectiveAction(const CorrectiveAction& action);
    void monitorActionEffectiveness(const CorrectiveAction& action);

    // Data management
    void recordComplianceMeasurement(const ComplianceMeasurement& measurement);
    void recordComplianceViolation(const ComplianceViolation& violation);
    void cleanupOldData();

    // Alert and reporting methods
    void sendComplianceAlert(const ComplianceMeasurement& measurement);
    void sendViolationAlert(const ComplianceViolation& violation);
    void generateRegulatoryReport();

public:
    explicit TRSComplianceEngine(const TRSConfig& config = TRSConfig());
    ~TRSComplianceEngine();

    // Monitoring control
    bool startMonitoring();
    void stopMonitoring();
    bool isMonitoring() const { return is_monitoring_.load(); }

    // Configuration management
    void updateConfig(const TRSConfig& new_config);
    const TRSConfig& getConfig() const { return config_; }

    // Integration with signal processor
    bool integrateWithSignalProcessor(std::unique_ptr<RealTimeSignalProcessor> processor);
    bool integrateWithCorrelationValidator(std::unique_ptr<RealCorrelationValidator> validator);

    // Current status access
    ComplianceStatus getCurrentStatus() const { return current_status_.load(); }
    ComplianceMeasurement getCurrentMeasurement() const;
    double getCurrentCorrelation() const;
    double getCurrentTRSGap() const;

    // Historical data access
    std::vector<ComplianceMeasurement> getMeasurementHistory(std::chrono::hours lookback = std::chrono::hours{24}) const;
    std::vector<ComplianceViolation> getViolationHistory(std::chrono::hours lookback = std::chrono::hours{168}) const; // 1 week
    CompliancePerformance getCompliancePerformance(std::chrono::hours evaluation_period = std::chrono::hours{24}) const;

    // Compliance analysis
    struct ComplianceAnalysis {
        ComplianceStatus current_status;
        double current_correlation;
        double correlation_trend_1h;
        double correlation_trend_24h;

        // Risk assessment
        double probability_of_violation_1h;
        double probability_of_violation_24h;
        std::vector<std::string> risk_factors;

        // Recommendations
        std::vector<std::string> recommendations;
        bool immediate_action_required;
        std::chrono::minutes estimated_time_to_compliance;
    };

    ComplianceAnalysis performComplianceAnalysis() const;

    // Predictive compliance monitoring
    struct ComplianceForecast {
        std::chrono::system_clock::time_point forecast_time;
        std::chrono::hours forecast_horizon;

        // Predicted compliance metrics
        double predicted_correlation;
        ComplianceStatus predicted_status;
        double confidence_in_prediction;

        // Risk projections
        double violation_probability;
        std::chrono::minutes expected_violation_duration;
        std::vector<std::string> predicted_triggers;
    };

    std::vector<ComplianceForecast> generateComplianceForecast(std::chrono::hours horizon = std::chrono::hours{24}) const;

    // Corrective action management
    std::vector<CorrectiveAction> getAvailableCorrectiveActions(const ComplianceViolation& violation) const;
    bool executeManualCorrectiveAction(const std::string& action_type, const std::map<std::string, double>& parameters);
    std::vector<CorrectiveAction> getPendingActions() const;
    std::vector<CorrectiveAction> getCompletedActions(std::chrono::hours lookback = std::chrono::hours{24}) const;

    // Emergency controls
    void triggerEmergencyStop();
    void emergencyOverrideCompliance(const std::string& justification, std::chrono::hours duration = std::chrono::hours{1});
    bool isInEmergencyMode() const;
    void clearEmergencyMode();

    // Regulatory reporting
    struct RegulatoryReport {
        std::string report_id;
        std::chrono::system_clock::time_point report_date;
        std::chrono::system_clock::time_point period_start;
        std::chrono::system_clock::time_point period_end;

        // Compliance summary
        CompliancePerformance compliance_summary;
        std::vector<ComplianceViolation> reportable_violations;
        std::vector<CorrectiveAction> corrective_actions_taken;

        // Risk assessment
        double overall_risk_rating;
        std::vector<std::string> identified_risks;
        std::vector<std::string> risk_mitigation_measures;

        // Forward-looking statements
        std::vector<std::string> planned_improvements;
        ComplianceForecast short_term_outlook;
    };

    RegulatoryReport generateRegulatoryReport(std::chrono::hours reporting_period = std::chrono::hours{24}) const;
    bool submitRegulatoryReport(const RegulatoryReport& report);

    // Performance optimization
    struct OptimizationRecommendation {
        std::string recommendation_type;
        std::string description;
        double expected_correlation_improvement;
        double implementation_difficulty;      // 0.0 to 1.0
        std::chrono::hours implementation_time;
        std::vector<std::string> prerequisites;
    };

    std::vector<OptimizationRecommendation> generateOptimizationRecommendations() const;

    // Callback registration
    void setMeasurementCallback(std::function<void(const ComplianceMeasurement&)> callback);
    void setViolationCallback(std::function<void(const ComplianceViolation&)> callback);
    void setActionCallback(std::function<void(const CorrectiveAction&)> callback);
    void setRegulatoryCallback(std::function<void(const std::string&)> callback);

    // Database integration
    bool enableDataPersistence(bool enable = true);
    bool saveMeasurementToDatabase(const ComplianceMeasurement& measurement);
    bool saveViolationToDatabase(const ComplianceViolation& violation);

    // Stress testing and validation
    struct StressTestResult {
        std::string test_scenario;
        std::vector<ComplianceMeasurement> simulated_measurements;
        std::vector<ComplianceViolation> predicted_violations;
        double worst_case_correlation;
        std::chrono::minutes longest_violation_duration;
        bool passes_stress_test;
    };

    std::vector<StressTestResult> runComplianceStressTest() const;

    // Static utility methods
    static std::string complianceStatusToString(ComplianceStatus status);
    static ComplianceStatus stringToComplianceStatus(const std::string& status_str);
    static double calculateComplianceScore(const std::vector<ComplianceMeasurement>& measurements);
    static bool isViolationRegulatorlyReportable(const ComplianceViolation& violation);
    static std::string generateViolationId();
    static std::string generateActionId();
};

} // namespace Algorithm
} // namespace CryptoClaude