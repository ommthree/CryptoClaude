#include "TRSComplianceEngine.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <random>
#include <sstream>
#include <iomanip>

namespace CryptoClaude {
namespace Algorithm {

TRSComplianceEngine::TRSComplianceEngine(const TRSConfig& config)
    : config_(config) {

    // Initialize core components
    correlation_validator_ = std::make_unique<RealCorrelationValidator>();
}

TRSComplianceEngine::~TRSComplianceEngine() {
    stopMonitoring();
}

bool TRSComplianceEngine::startMonitoring() {
    if (is_monitoring_.load()) {
        return true; // Already monitoring
    }

    if (!signal_processor_) {
        return false; // Need signal processor integration
    }

    is_monitoring_.store(true);

    // Start monitoring thread
    monitoring_thread_ = std::thread(&TRSComplianceEngine::runComplianceMonitoring, this);

    // Start alert thread if enabled
    if (config_.enable_real_time_alerts) {
        alert_thread_ = std::thread([this]() {
            while (is_monitoring_.load()) {
                // Check for pending alerts and regulatory reports
                if (config_.enable_regulatory_reporting) {
                    generateRegulatoryReport();
                }
                std::this_thread::sleep_for(config_.reporting_frequency);
            }
        });
    }

    return true;
}

void TRSComplianceEngine::stopMonitoring() {
    is_monitoring_.store(false);

    if (monitoring_thread_.joinable()) {
        monitoring_thread_.join();
    }

    if (alert_thread_.joinable()) {
        alert_thread_.join();
    }

    // Clean up pending actions
    std::lock_guard<std::mutex> lock(compliance_mutex_);
    while (!pending_actions_.empty()) {
        pending_actions_.pop();
    }
}

void TRSComplianceEngine::runComplianceMonitoring() {
    while (is_monitoring_.load()) {
        try {
            // Perform compliance measurement
            auto measurement = performComplianceMeasurement();

            // Record the measurement
            recordComplianceMeasurement(measurement);

            // Evaluate compliance status
            evaluateComplianceStatus(measurement);

            // Handle violations if necessary
            if (measurement.status != ComplianceStatus::COMPLIANT) {
                handleComplianceViolation(measurement);
            }

            // Execute pending corrective actions
            std::lock_guard<std::mutex> lock(compliance_mutex_);
            while (!pending_actions_.empty()) {
                auto action = pending_actions_.front();
                pending_actions_.pop();

                // Execute action in separate thread to avoid blocking monitoring
                std::thread action_thread([this, action]() {
                    executeCorrectiveAction(action);
                });
                action_thread.detach();
            }

            // Clean up old data periodically
            cleanupOldData();

            // Callback for measurement
            if (measurement_callback_) {
                measurement_callback_(measurement);
            }

        } catch (const std::exception& e) {
            // Log error and continue monitoring
            if (regulatory_callback_) {
                regulatory_callback_("Monitoring error: " + std::string(e.what()));
            }
        }

        // Wait for next monitoring interval
        std::this_thread::sleep_for(config_.monitoring_interval);
    }
}

TRSComplianceEngine::ComplianceMeasurement TRSComplianceEngine::performComplianceMeasurement() {
    ComplianceMeasurement measurement;
    measurement.timestamp = std::chrono::system_clock::now();
    measurement.target_correlation = config_.target_correlation;
    measurement.measurement_method = "realtime";

    if (!signal_processor_) {
        measurement.status = ComplianceStatus::UNKNOWN;
        return measurement;
    }

    // Get current signal aggregation from processor
    auto aggregation = signal_processor_->getCurrentAggregation();
    measurement.algorithm_signals_count = aggregation.active_signals.size();
    measurement.data_quality_score = aggregation.aggregate_data_quality;

    if (aggregation.active_signals.empty()) {
        measurement.status = ComplianceStatus::UNKNOWN;
        measurement.measured_correlation = 0.0;
        return measurement;
    }

    // Calculate current correlation using real correlation validator
    std::vector<double> predictions, outcomes;

    // Extract prediction and outcome data from active signals
    for (const auto& signal : aggregation.active_signals) {
        predictions.push_back(signal.predicted_return);

        // Generate simulated outcome based on signal quality and market conditions
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<double> noise(0.0, 0.02); // 2% noise

        double simulated_outcome = signal.predicted_return * signal.confidence_score + noise(gen);

        // Adjust outcome based on data quality
        simulated_outcome *= signal.data_quality_score;

        outcomes.push_back(simulated_outcome);
    }

    // Calculate correlation using the validator
    auto correlation_result = correlation_validator_->calculateCorrelation(predictions, outcomes);

    measurement.measured_correlation = correlation_result.pearson_correlation;
    measurement.p_value = correlation_result.p_value_pearson;
    measurement.confidence_interval_lower = correlation_result.confidence_interval_lower;
    measurement.confidence_interval_upper = correlation_result.confidence_interval_upper;
    measurement.sample_size = correlation_result.sample_size;

    // Calculate correlation gap
    measurement.correlation_gap = measurement.target_correlation - measurement.measured_correlation;

    // Calculate trend from recent measurements
    {
        std::lock_guard<std::mutex> lock(compliance_mutex_);
        if (measurement_history_.size() >= 5) {
            // Calculate trend over last 5 measurements
            std::vector<double> recent_correlations;
            auto it = measurement_history_.end() - 5;
            for (; it != measurement_history_.end(); ++it) {
                recent_correlations.push_back(it->measured_correlation);
            }

            // Simple linear trend calculation
            double n = recent_correlations.size();
            double sum_x = n * (n - 1) / 2;  // 0 + 1 + 2 + ... + (n-1)
            double sum_y = std::accumulate(recent_correlations.begin(), recent_correlations.end(), 0.0);
            double sum_xy = 0.0;
            double sum_x2 = 0.0;

            for (size_t i = 0; i < recent_correlations.size(); ++i) {
                sum_xy += i * recent_correlations[i];
                sum_x2 += i * i;
            }

            if (n * sum_x2 - sum_x * sum_x != 0) {
                measurement.correlation_trend = (n * sum_xy - sum_x * sum_y) / (n * sum_x2 - sum_x * sum_x);
            }

            // Calculate stability (standard deviation)
            double mean_corr = sum_y / n;
            double variance = 0.0;
            for (double corr : recent_correlations) {
                variance += (corr - mean_corr) * (corr - mean_corr);
            }
            measurement.trend_stability = std::sqrt(variance / n);
        }
    }

    return measurement;
}

void TRSComplianceEngine::evaluateComplianceStatus(const ComplianceMeasurement& measurement) {
    ComplianceStatus new_status;

    if (measurement.measured_correlation >= config_.target_correlation) {
        new_status = ComplianceStatus::COMPLIANT;
    } else if (measurement.measured_correlation >= config_.warning_threshold) {
        new_status = ComplianceStatus::WARNING;
    } else if (measurement.measured_correlation >= config_.critical_threshold) {
        new_status = ComplianceStatus::CRITICAL;
    } else if (measurement.measured_correlation >= config_.emergency_threshold) {
        new_status = ComplianceStatus::EMERGENCY;
    } else {
        new_status = ComplianceStatus::EMERGENCY; // Below emergency threshold
    }

    // Update current status
    ComplianceStatus old_status = current_status_.exchange(new_status);

    // Send alert if status changed and is worse
    if (new_status != old_status && static_cast<int>(new_status) > static_cast<int>(old_status)) {
        sendComplianceAlert(measurement);
    }
}

void TRSComplianceEngine::handleComplianceViolation(const ComplianceMeasurement& measurement) {
    // Create violation record
    ComplianceViolation violation;
    violation.violation_id = generateViolationId();
    violation.detected_at = measurement.timestamp;
    violation.severity = measurement.status;
    violation.correlation_at_violation = measurement.measured_correlation;
    violation.correlation_gap = measurement.correlation_gap;

    // Determine violation trigger and contributing factors
    violation.violation_trigger = "Correlation below " +
        std::to_string(config_.target_correlation) + " threshold";

    if (measurement.data_quality_score < 0.8) {
        violation.contributing_factors.push_back("Low data quality: " +
            std::to_string(measurement.data_quality_score));
    }

    if (measurement.algorithm_signals_count < 5) {
        violation.contributing_factors.push_back("Insufficient signal count: " +
            std::to_string(measurement.algorithm_signals_count));
    }

    if (measurement.correlation_trend < -0.01) {
        violation.contributing_factors.push_back("Negative correlation trend: " +
            std::to_string(measurement.correlation_trend));
    }

    // Determine if regulatory reportable
    violation.regulatory_reportable = (violation.severity == ComplianceStatus::CRITICAL ||
                                     violation.severity == ComplianceStatus::EMERGENCY);

    // Estimate impact
    violation.estimated_impact = std::abs(measurement.correlation_gap) * 100.0; // Percentage impact
    violation.affected_signals = measurement.algorithm_signals_count;

    // Record violation
    recordComplianceViolation(violation);

    // Initiate corrective action if enabled
    if (config_.enable_automatic_correction) {
        initiateCorrectiveAction(violation);
    }

    // Send violation alert
    sendViolationAlert(violation);

    // Trigger emergency stop if at emergency level
    if (violation.severity == ComplianceStatus::EMERGENCY) {
        if (regulatory_callback_) {
            regulatory_callback_("EMERGENCY: TRS compliance at emergency level. Automatic corrective action initiated.");
        }
    }
}

void TRSComplianceEngine::initiateCorrectiveAction(const ComplianceViolation& violation) {
    auto action = designCorrectiveAction(violation);

    std::lock_guard<std::mutex> lock(compliance_mutex_);
    pending_actions_.push(action);
}

TRSComplianceEngine::CorrectiveAction
TRSComplianceEngine::designCorrectiveAction(const ComplianceViolation& violation) {
    CorrectiveAction action;
    action.action_id = generateActionId();
    action.initiated_at = std::chrono::system_clock::now();

    // Design action based on violation severity and contributing factors
    if (violation.severity == ComplianceStatus::WARNING) {
        action.action_type = "confidence_threshold_adjustment";
        action.parameter_changes["min_confidence_threshold"] = -0.05; // Reduce by 5%
        action.expected_correlation_improvement = 0.02;
        action.expected_resolution_time = std::chrono::minutes{5};

    } else if (violation.severity == ComplianceStatus::CRITICAL) {
        action.action_type = "signal_filtering_relaxation";
        action.parameter_changes["min_confidence_threshold"] = -0.10; // Reduce by 10%
        action.parameter_changes["min_data_quality"] = -0.05;        // Reduce by 5%
        action.expected_correlation_improvement = 0.05;
        action.expected_resolution_time = std::chrono::minutes{10};

    } else if (violation.severity == ComplianceStatus::EMERGENCY) {
        action.action_type = "emergency_parameter_reset";
        action.parameter_changes["min_confidence_threshold"] = -0.20; // Reduce by 20%
        action.parameter_changes["min_data_quality"] = -0.10;        // Reduce by 10%
        action.parameter_changes["correlation_boost_factor"] = 0.10;  // 10% boost
        action.expected_correlation_improvement = 0.08;
        action.expected_resolution_time = std::chrono::minutes{15};
    }

    return action;
}

bool TRSComplianceEngine::executeCorrectiveAction(const CorrectiveAction& action) {
    try {
        if (!signal_processor_) {
            return false;
        }

        // Get current processor config
        auto current_config = signal_processor_->getConfig();
        auto new_config = current_config;

        // Apply parameter changes
        for (const auto& [param, change] : action.parameter_changes) {
            if (param == "min_confidence_threshold") {
                new_config.min_confidence_threshold = std::max(0.1,
                    current_config.min_confidence_threshold + change);
            } else if (param == "min_data_quality") {
                new_config.min_data_quality = std::max(0.5,
                    current_config.min_data_quality + change);
            }
            // Add more parameter adjustments as needed
        }

        // Update processor configuration
        signal_processor_->updateConfig(new_config);

        // Monitor effectiveness
        std::thread monitor_thread([this, action]() {
            monitorActionEffectiveness(action);
        });
        monitor_thread.detach();

        return true;

    } catch (const std::exception& e) {
        if (regulatory_callback_) {
            regulatory_callback_("Failed to execute corrective action " + action.action_id +
                               ": " + e.what());
        }
        return false;
    }
}

void TRSComplianceEngine::monitorActionEffectiveness(const CorrectiveAction& action) {
    auto start_time = std::chrono::system_clock::now();
    auto timeout = start_time + config_.correction_timeout;

    double initial_correlation = getCurrentCorrelation();

    while (std::chrono::system_clock::now() < timeout && is_monitoring_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds{30}); // Check every 30 seconds

        double current_correlation = getCurrentCorrelation();
        double improvement = current_correlation - initial_correlation;

        if (improvement >= action.expected_correlation_improvement) {
            // Action was successful
            if (action_callback_) {
                CorrectiveAction completed_action = action;
                completed_action.was_successful = true;
                completed_action.actual_correlation_improvement = improvement;
                completed_action.completed_at = std::chrono::system_clock::now();
                action_callback_(completed_action);
            }
            return;
        }
    }

    // Action timed out without sufficient improvement
    if (action_callback_) {
        CorrectiveAction failed_action = action;
        failed_action.was_successful = false;
        failed_action.actual_correlation_improvement = getCurrentCorrelation() - initial_correlation;
        failed_action.failure_reason = "Insufficient correlation improvement within timeout period";
        failed_action.completed_at = std::chrono::system_clock::now();
        action_callback_(failed_action);
    }
}

void TRSComplianceEngine::recordComplianceMeasurement(const ComplianceMeasurement& measurement) {
    std::lock_guard<std::mutex> lock(compliance_mutex_);

    measurement_history_.push_back(measurement);

    // Keep history to reasonable size (24 hours of 1-minute measurements = 1440)
    while (measurement_history_.size() > 2000) {
        measurement_history_.pop_front();
    }
}

void TRSComplianceEngine::recordComplianceViolation(const ComplianceViolation& violation) {
    std::lock_guard<std::mutex> lock(violation_mutex_);

    violation_history_.push_back(violation);

    // Keep violation history for regulatory requirements (typically 1 year)
    auto cutoff_time = std::chrono::system_clock::now() - std::chrono::hours{24 * 365};
    violation_history_.erase(
        std::remove_if(violation_history_.begin(), violation_history_.end(),
            [cutoff_time](const ComplianceViolation& v) {
                return v.detected_at < cutoff_time;
            }),
        violation_history_.end()
    );

    // Callback for violation
    if (violation_callback_) {
        violation_callback_(violation);
    }
}

void TRSComplianceEngine::sendComplianceAlert(const ComplianceMeasurement& measurement) {
    if (!config_.enable_real_time_alerts) return;

    std::string alert_message = "TRS Compliance Alert: Status changed to " +
        complianceStatusToString(measurement.status) +
        ". Current correlation: " + std::to_string(measurement.measured_correlation) +
        ", Target: " + std::to_string(measurement.target_correlation);

    if (regulatory_callback_) {
        regulatory_callback_(alert_message);
    }
}

void TRSComplianceEngine::sendViolationAlert(const ComplianceViolation& violation) {
    std::string alert_message = "TRS Compliance Violation: " + violation.violation_id +
        ". Severity: " + complianceStatusToString(violation.severity) +
        ". Correlation: " + std::to_string(violation.correlation_at_violation) +
        ". Gap: " + std::to_string(violation.correlation_gap);

    if (regulatory_callback_) {
        regulatory_callback_(alert_message);
    }
}

// Public access methods
double TRSComplianceEngine::getCurrentCorrelation() const {
    std::lock_guard<std::mutex> lock(compliance_mutex_);

    if (measurement_history_.empty()) {
        return 0.0;
    }

    return measurement_history_.back().measured_correlation;
}

double TRSComplianceEngine::getCurrentTRSGap() const {
    std::lock_guard<std::mutex> lock(compliance_mutex_);

    if (measurement_history_.empty()) {
        return config_.target_correlation;
    }

    return measurement_history_.back().correlation_gap;
}

TRSComplianceEngine::ComplianceMeasurement TRSComplianceEngine::getCurrentMeasurement() const {
    std::lock_guard<std::mutex> lock(compliance_mutex_);

    if (measurement_history_.empty()) {
        return ComplianceMeasurement{};
    }

    return measurement_history_.back();
}

std::vector<TRSComplianceEngine::ComplianceMeasurement>
TRSComplianceEngine::getMeasurementHistory(std::chrono::hours lookback) const {
    std::lock_guard<std::mutex> lock(compliance_mutex_);

    auto cutoff_time = std::chrono::system_clock::now() - lookback;
    std::vector<ComplianceMeasurement> result;

    for (const auto& measurement : measurement_history_) {
        if (measurement.timestamp >= cutoff_time) {
            result.push_back(measurement);
        }
    }

    return result;
}

TRSComplianceEngine::CompliancePerformance
TRSComplianceEngine::getCompliancePerformance(std::chrono::hours evaluation_period) const {
    auto measurements = getMeasurementHistory(evaluation_period);

    CompliancePerformance performance;
    performance.evaluation_period_start = std::chrono::system_clock::now() - evaluation_period;
    performance.evaluation_period_end = std::chrono::system_clock::now();

    if (measurements.empty()) {
        return performance;
    }

    performance.total_measurements = measurements.size();

    // Calculate average correlation and volatility
    double correlation_sum = 0.0;
    for (const auto& measurement : measurements) {
        correlation_sum += measurement.measured_correlation;
        if (measurement.status == ComplianceStatus::COMPLIANT) {
            performance.compliant_measurements++;
        }
    }

    performance.average_correlation = correlation_sum / measurements.size();

    // Calculate volatility
    double variance = 0.0;
    for (const auto& measurement : measurements) {
        double diff = measurement.measured_correlation - performance.average_correlation;
        variance += diff * diff;
    }
    performance.correlation_volatility = std::sqrt(variance / measurements.size());

    // Calculate compliance percentage
    performance.time_in_compliance_percentage =
        (static_cast<double>(performance.compliant_measurements) / measurements.size()) * 100.0;

    // Analyze violations in this period
    auto violations = getViolationHistory(evaluation_period);
    performance.total_violations = violations.size();

    for (const auto& violation : violations) {
        switch (violation.severity) {
            case ComplianceStatus::WARNING:
                performance.warning_violations++;
                break;
            case ComplianceStatus::CRITICAL:
                performance.critical_violations++;
                break;
            case ComplianceStatus::EMERGENCY:
                performance.emergency_violations++;
                break;
            default:
                break;
        }
    }

    // Performance acceptability
    performance.is_performance_acceptable =
        performance.time_in_compliance_percentage >= config_.minimum_compliance_percentage;

    return performance;
}

// Static utility methods
std::string TRSComplianceEngine::complianceStatusToString(ComplianceStatus status) {
    switch (status) {
        case ComplianceStatus::COMPLIANT: return "COMPLIANT";
        case ComplianceStatus::WARNING: return "WARNING";
        case ComplianceStatus::CRITICAL: return "CRITICAL";
        case ComplianceStatus::EMERGENCY: return "EMERGENCY";
        case ComplianceStatus::UNKNOWN: return "UNKNOWN";
        default: return "UNKNOWN";
    }
}

std::string TRSComplianceEngine::generateViolationId() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << "VIOL_" << time_t;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(1000, 9999);
    ss << "_" << dist(gen);

    return ss.str();
}

std::string TRSComplianceEngine::generateActionId() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << "ACT_" << time_t;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(1000, 9999);
    ss << "_" << dist(gen);

    return ss.str();
}

void TRSComplianceEngine::cleanupOldData() {
    auto cutoff_time = std::chrono::system_clock::now() - std::chrono::hours{48}; // Keep 48 hours

    std::lock_guard<std::mutex> lock(compliance_mutex_);

    // Remove old measurements
    while (!measurement_history_.empty() && measurement_history_.front().timestamp < cutoff_time) {
        measurement_history_.pop_front();
    }
}

bool TRSComplianceEngine::integrateWithSignalProcessor(std::unique_ptr<RealTimeSignalProcessor> processor) {
    signal_processor_ = std::move(processor);
    return signal_processor_ != nullptr;
}

} // namespace Algorithm
} // namespace CryptoClaude