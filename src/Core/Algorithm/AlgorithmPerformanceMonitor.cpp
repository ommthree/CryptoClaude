#include "AlgorithmPerformanceMonitor.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <stdexcept>

namespace CryptoClaude {
namespace Algorithm {

AlgorithmPerformanceMonitor::AlgorithmPerformanceMonitor(const MonitoringConfig& config)
    : config_(config), monitoring_active_(false) {

    current_metrics_.measurement_start = std::chrono::system_clock::now();
    current_metrics_.last_update = std::chrono::system_clock::now();
}

AlgorithmPerformanceMonitor::~AlgorithmPerformanceMonitor() {
    stop();
}

bool AlgorithmPerformanceMonitor::initialize() {
    try {
        // Initialize database manager for performance data storage
        db_manager_ = std::make_unique<Database::DatabaseManager>();

        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

bool AlgorithmPerformanceMonitor::start() {
    if (monitoring_active_.load()) {
        return true; // Already running
    }

    if (!db_manager_) {
        if (!initialize()) {
            return false;
        }
    }

    monitoring_active_ = true;

    // Start monitoring thread
    monitoring_thread_ = std::thread(&AlgorithmPerformanceMonitor::monitoringLoop, this);

    return true;
}

void AlgorithmPerformanceMonitor::stop() {
    monitoring_active_ = false;

    if (monitoring_thread_.joinable()) {
        monitoring_thread_.join();
    }
}

bool AlgorithmPerformanceMonitor::isRunning() const {
    return monitoring_active_.load();
}

void AlgorithmPerformanceMonitor::recordDecision(const AlgorithmDecisionEngine::TradingDecision& decision) {
    std::lock_guard<std::mutex> lock(data_mutex_);

    DecisionOutcome outcome;
    outcome.decision_id = decision.decision_id;
    outcome.pair_name = decision.pair_name;
    outcome.action = decision.action;
    outcome.predicted_return = decision.expected_return;
    outcome.confidence_score = decision.confidence_score;
    outcome.recommended_weight = decision.recommended_weight;
    outcome.decision_time = decision.decision_time;

    // Initialize with prediction - actual return will be updated later
    outcome.actual_return = 0.0; // Will be updated when outcome is known
    outcome.prediction_error = 0.0;
    outcome.was_profitable = false;

    decision_outcomes_.push_back(outcome);

    // Keep outcomes manageable
    if (decision_outcomes_.size() > config_.max_decision_tracking) {
        decision_outcomes_.erase(decision_outcomes_.begin());
    }

    // Update daily decision count
    current_metrics_.daily_decisions_made++;
}

void AlgorithmPerformanceMonitor::recordDecisionOutcome(const std::string& decision_id, double actual_return) {
    std::lock_guard<std::mutex> lock(data_mutex_);

    // Find the corresponding decision
    auto it = std::find_if(decision_outcomes_.begin(), decision_outcomes_.end(),
                          [&decision_id](const DecisionOutcome& outcome) {
                              return outcome.decision_id == decision_id;
                          });

    if (it != decision_outcomes_.end()) {
        it->actual_return = actual_return;
        it->prediction_error = std::abs(actual_return - it->predicted_return);
        it->was_profitable = actual_return > 0.0;
        it->outcome_time = std::chrono::system_clock::now();
        it->return_contribution = actual_return * it->recommended_weight;

        // Calculate risk-adjusted return (simplified Sharpe ratio)
        if (it->predicted_return != 0) {
            it->risk_adjusted_return = actual_return / std::abs(it->predicted_return);
        }

        // Update daily profitable decisions count
        if (it->was_profitable) {
            current_metrics_.daily_profitable_decisions++;
        }

        // Save to database if configured
        if (config_.enable_performance_logging) {
            saveDecisionOutcome(*it);
        }
    }
}

AlgorithmPerformanceMonitor::RealTimeMetrics AlgorithmPerformanceMonitor::getCurrentMetrics() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return current_metrics_;
}

double AlgorithmPerformanceMonitor::getCurrentCorrelation() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return current_metrics_.current_correlation;
}

bool AlgorithmPerformanceMonitor::meetsTargetCorrelation() const {
    return getCurrentCorrelation() >= config_.target_correlation;
}

bool AlgorithmPerformanceMonitor::meetsMinimumCorrelation() const {
    return getCurrentCorrelation() >= config_.min_acceptable_correlation;
}

void AlgorithmPerformanceMonitor::updateRealTimeMetrics() {
    std::lock_guard<std::mutex> lock(data_mutex_);

    auto now = std::chrono::system_clock::now();
    current_metrics_.last_update = now;

    // Filter decisions with known outcomes for correlation calculation
    std::vector<double> predictions, outcomes;
    std::vector<double> returns;
    int profitable_count = 0;

    for (const auto& decision_outcome : decision_outcomes_) {
        if (decision_outcome.outcome_time != std::chrono::system_clock::time_point{}) {
            // Outcome is known
            predictions.push_back(decision_outcome.predicted_return);
            outcomes.push_back(decision_outcome.actual_return);
            returns.push_back(decision_outcome.actual_return);

            if (decision_outcome.was_profitable) {
                profitable_count++;
            }
        }
    }

    // Update basic statistics
    current_metrics_.total_decisions_tracked = static_cast<int>(decision_outcomes_.size());
    current_metrics_.profitable_decisions = profitable_count;

    if (current_metrics_.total_decisions_tracked > 0) {
        current_metrics_.hit_rate = static_cast<double>(profitable_count) / current_metrics_.total_decisions_tracked;
    }

    // Calculate correlation if we have enough data
    if (predictions.size() >= config_.min_decisions_for_correlation) {
        current_metrics_.current_correlation = calculatePearsonCorrelation(predictions, outcomes);
        current_metrics_.rolling_correlation = current_metrics_.current_correlation; // Simplified
    }

    // Calculate performance metrics
    if (!returns.empty()) {
        current_metrics_.rolling_return = std::accumulate(returns.begin(), returns.end(), 0.0);
        current_metrics_.daily_return = current_metrics_.rolling_return; // Simplified

        current_metrics_.current_sharpe_ratio = calculateSharpeRatio(returns);

        // Calculate rolling volatility
        if (returns.size() > 1) {
            double mean_return = current_metrics_.rolling_return / returns.size();
            double variance = 0.0;
            for (double ret : returns) {
                variance += (ret - mean_return) * (ret - mean_return);
            }
            current_metrics_.rolling_volatility = std::sqrt(variance / (returns.size() - 1));
        }

        // Calculate maximum drawdown
        std::vector<double> cumulative_returns(returns.size());
        std::partial_sum(returns.begin(), returns.end(), cumulative_returns.begin());
        current_metrics_.rolling_max_drawdown = calculateMaxDrawdown(cumulative_returns);
    }

    // Calculate accuracy
    if (!predictions.empty()) {
        double correct_predictions = 0.0;
        for (size_t i = 0; i < predictions.size(); i++) {
            // Consider prediction correct if sign matches
            if ((predictions[i] > 0 && outcomes[i] > 0) ||
                (predictions[i] < 0 && outcomes[i] < 0) ||
                (std::abs(predictions[i]) < 0.001 && std::abs(outcomes[i]) < 0.001)) {
                correct_predictions++;
            }
        }
        current_metrics_.current_accuracy = correct_predictions / predictions.size();
        current_metrics_.daily_accuracy = current_metrics_.current_accuracy; // Simplified
    }
}

double AlgorithmPerformanceMonitor::calculatePearsonCorrelation(const std::vector<double>& predictions,
                                                              const std::vector<double>& outcomes) {
    if (predictions.size() != outcomes.size() || predictions.empty()) {
        return 0.0;
    }

    size_t n = predictions.size();

    // Calculate means
    double pred_mean = std::accumulate(predictions.begin(), predictions.end(), 0.0) / n;
    double outcome_mean = std::accumulate(outcomes.begin(), outcomes.end(), 0.0) / n;

    // Calculate correlation coefficient
    double numerator = 0.0;
    double pred_var = 0.0;
    double outcome_var = 0.0;

    for (size_t i = 0; i < n; i++) {
        double pred_diff = predictions[i] - pred_mean;
        double outcome_diff = outcomes[i] - outcome_mean;

        numerator += pred_diff * outcome_diff;
        pred_var += pred_diff * pred_diff;
        outcome_var += outcome_diff * outcome_diff;
    }

    if (pred_var == 0.0 || outcome_var == 0.0) {
        return 0.0;
    }

    return numerator / std::sqrt(pred_var * outcome_var);
}

double AlgorithmPerformanceMonitor::calculateSharpeRatio(const std::vector<double>& returns) {
    if (returns.size() < 2) {
        return 0.0;
    }

    double mean_return = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();

    double variance = 0.0;
    for (double ret : returns) {
        variance += (ret - mean_return) * (ret - mean_return);
    }
    double volatility = std::sqrt(variance / (returns.size() - 1));

    if (volatility == 0.0) {
        return 0.0;
    }

    // Assuming risk-free rate is 0 for simplicity
    return mean_return / volatility;
}

double AlgorithmPerformanceMonitor::calculateMaxDrawdown(const std::vector<double>& cumulative_returns) {
    if (cumulative_returns.empty()) {
        return 0.0;
    }

    double max_drawdown = 0.0;
    double peak = cumulative_returns[0];

    for (double value : cumulative_returns) {
        if (value > peak) {
            peak = value;
        }

        double drawdown = (peak - value) / peak;
        max_drawdown = std::max(max_drawdown, drawdown);
    }

    return max_drawdown;
}

void AlgorithmPerformanceMonitor::checkPerformanceThresholds() {
    std::lock_guard<std::mutex> lock(data_mutex_);

    // Check correlation thresholds
    if (current_metrics_.current_correlation < config_.target_correlation) {
        if (current_metrics_.current_correlation < config_.min_acceptable_correlation) {
            generateAlert(PerformanceAlert::AlertType::CORRELATION_BELOW_MINIMUM,
                         PerformanceAlert::Severity::CRITICAL,
                         "Algorithm correlation below minimum acceptable level",
                         {{"correlation", current_metrics_.current_correlation},
                          {"minimum_required", config_.min_acceptable_correlation}});
        } else {
            generateAlert(PerformanceAlert::AlertType::CORRELATION_BELOW_TARGET,
                         PerformanceAlert::Severity::WARNING,
                         "Algorithm correlation below target level",
                         {{"correlation", current_metrics_.current_correlation},
                          {"target", config_.target_correlation}});
        }
    }

    // Check performance degradation
    if (current_metrics_.current_accuracy < config_.performance_alert_threshold) {
        generateAlert(PerformanceAlert::AlertType::PERFORMANCE_DEGRADATION,
                     PerformanceAlert::Severity::WARNING,
                     "Algorithm accuracy below acceptable level",
                     {{"accuracy", current_metrics_.current_accuracy},
                      {"threshold", config_.performance_alert_threshold}});
    }

    // Check high prediction error
    if (!decision_outcomes_.empty()) {
        std::vector<double> recent_errors;
        auto recent_cutoff = std::chrono::system_clock::now() - std::chrono::hours(24);

        for (const auto& outcome : decision_outcomes_) {
            if (outcome.outcome_time > recent_cutoff && outcome.prediction_error > 0) {
                recent_errors.push_back(outcome.prediction_error);
            }
        }

        if (!recent_errors.empty()) {
            double mean_error = std::accumulate(recent_errors.begin(), recent_errors.end(), 0.0) / recent_errors.size();

            if (mean_error > 0.1) { // 10% average prediction error threshold
                generateAlert(PerformanceAlert::AlertType::HIGH_PREDICTION_ERROR,
                             PerformanceAlert::Severity::WARNING,
                             "High prediction error detected",
                             {{"mean_error", mean_error},
                              {"sample_size", static_cast<double>(recent_errors.size())}});
            }
        }
    }
}

void AlgorithmPerformanceMonitor::generateAlert(PerformanceAlert::AlertType type,
                                               PerformanceAlert::Severity severity,
                                               const std::string& message,
                                               const std::map<std::string, double>& metrics) {
    PerformanceAlert alert;
    alert.type = type;
    alert.severity = severity;
    alert.message = message;
    alert.metrics = metrics;
    alert.alert_time = std::chrono::system_clock::now();

    // Generate detailed message
    std::string details;
    for (const auto& metric : metrics) {
        details += metric.first + ": " + std::to_string(metric.second) + "\n";
    }
    alert.details = details;

    recent_alerts_.push_back(alert);

    // Keep alerts manageable
    if (recent_alerts_.size() > 50) {
        recent_alerts_.erase(recent_alerts_.begin());
    }
}

void AlgorithmPerformanceMonitor::monitoringLoop() {
    while (monitoring_active_.load()) {
        try {
            // Update real-time metrics
            updateRealTimeMetrics();

            // Check performance thresholds
            if (config_.enable_real_time_alerts) {
                checkPerformanceThresholds();
            }

            // Clean up old data
            cleanupOldData();

            // Sleep for the configured interval
            std::this_thread::sleep_for(config_.real_time_interval);

        } catch (const std::exception& e) {
            // Log error and continue
            std::lock_guard<std::mutex> lock(data_mutex_);
            generateAlert(PerformanceAlert::AlertType::SYSTEM_PERFORMANCE_ISSUE,
                         PerformanceAlert::Severity::WARNING,
                         "Monitoring loop error: " + std::string(e.what()));
        }
    }
}

void AlgorithmPerformanceMonitor::cleanupOldData() {
    std::lock_guard<std::mutex> lock(data_mutex_);

    auto cutoff_time = std::chrono::system_clock::now() -
                       std::chrono::hours(config_.rolling_window_hours);

    // Remove old decision outcomes
    decision_outcomes_.erase(
        std::remove_if(decision_outcomes_.begin(), decision_outcomes_.end(),
                      [cutoff_time](const DecisionOutcome& outcome) {
                          return outcome.decision_time < cutoff_time;
                      }),
        decision_outcomes_.end());

    // Remove old alerts
    recent_alerts_.erase(
        std::remove_if(recent_alerts_.begin(), recent_alerts_.end(),
                      [cutoff_time](const PerformanceAlert& alert) {
                          return alert.alert_time < cutoff_time;
                      }),
        recent_alerts_.end());
}

bool AlgorithmPerformanceMonitor::saveDecisionOutcome(const DecisionOutcome& outcome) {
    if (!db_manager_) {
        return false;
    }

    try {
        // In a full implementation, this would save to database
        // For Day 20, we'll just return success
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

std::vector<AlgorithmPerformanceMonitor::PerformanceAlert>
AlgorithmPerformanceMonitor::getRecentAlerts(int max_alerts) const {
    std::lock_guard<std::mutex> lock(data_mutex_);

    std::vector<PerformanceAlert> alerts = recent_alerts_;

    // Sort by time (most recent first)
    std::sort(alerts.begin(), alerts.end(),
              [](const PerformanceAlert& a, const PerformanceAlert& b) {
                  return a.alert_time > b.alert_time;
              });

    // Limit to requested count
    if (alerts.size() > static_cast<size_t>(max_alerts)) {
        alerts.resize(max_alerts);
    }

    return alerts;
}

std::vector<AlgorithmPerformanceMonitor::DecisionOutcome>
AlgorithmPerformanceMonitor::getDecisionOutcomes(int max_outcomes) const {
    std::lock_guard<std::mutex> lock(data_mutex_);

    std::vector<DecisionOutcome> outcomes = decision_outcomes_;

    // Sort by decision time (most recent first)
    std::sort(outcomes.begin(), outcomes.end(),
              [](const DecisionOutcome& a, const DecisionOutcome& b) {
                  return a.decision_time > b.decision_time;
              });

    // Limit to requested count
    if (outcomes.size() > static_cast<size_t>(max_outcomes)) {
        outcomes.resize(max_outcomes);
    }

    return outcomes;
}

AlgorithmPerformanceMonitor::PerformanceReport
AlgorithmPerformanceMonitor::generatePerformanceReport(
    const std::chrono::system_clock::time_point& start_time,
    const std::chrono::system_clock::time_point& end_time) const {

    PerformanceReport report;
    report.report_time = std::chrono::system_clock::now();
    report.period_start = start_time;
    report.period_end = end_time;

    {
        std::lock_guard<std::mutex> lock(data_mutex_);

        // Copy current metrics
        report.overall_metrics = current_metrics_;

        // Filter decision outcomes to the specified period
        std::vector<DecisionOutcome> period_outcomes;
        for (const auto& outcome : decision_outcomes_) {
            if (outcome.decision_time >= start_time && outcome.decision_time <= end_time) {
                period_outcomes.push_back(outcome);
            }
        }

        // Analyze performance by action type
        std::map<AlgorithmDecisionEngine::TradingDecision::Action, std::vector<double>> action_returns;

        for (const auto& outcome : period_outcomes) {
            if (outcome.outcome_time != std::chrono::system_clock::time_point{}) {
                action_returns[outcome.action].push_back(outcome.actual_return);
            }
        }

        // Calculate average returns by action
        for (const auto& action_pair : action_returns) {
            if (!action_pair.second.empty()) {
                double avg_return = std::accumulate(action_pair.second.begin(),
                                                   action_pair.second.end(), 0.0) / action_pair.second.size();
                report.action_performance[action_pair.first] = avg_return;
            }
        }

        // Generate basic recommendations
        if (report.overall_metrics.current_correlation < config_.target_correlation) {
            report.performance_recommendations.push_back("Improve signal generation to increase correlation");
        }

        if (report.overall_metrics.hit_rate < 0.6) {
            report.performance_recommendations.push_back("Review decision thresholds to improve hit rate");
        }
    }

    return report;
}

AlgorithmPerformanceMonitor::PerformanceReport
AlgorithmPerformanceMonitor::generateDailyReport() const {
    auto now = std::chrono::system_clock::now();
    auto start_of_day = now - std::chrono::hours(24);
    return generatePerformanceReport(start_of_day, now);
}

std::vector<AlgorithmPerformanceMonitor::OptimizationRecommendation>
AlgorithmPerformanceMonitor::generateOptimizationRecommendations() const {
    std::vector<OptimizationRecommendation> recommendations;

    std::lock_guard<std::mutex> lock(data_mutex_);

    // Correlation-based recommendations
    if (current_metrics_.current_correlation < config_.target_correlation) {
        OptimizationRecommendation rec;
        rec.category = "Signal Generation";
        rec.recommendation = "Improve signal quality and accuracy to increase correlation with market outcomes";
        rec.potential_impact = config_.target_correlation - current_metrics_.current_correlation;
        rec.implementation_priority = "High";
        rec.required_actions = {
            "Analyze prediction errors to identify systematic biases",
            "Improve feature engineering and technical analysis",
            "Increase data quality requirements",
            "Calibrate signal confidence thresholds"
        };
        recommendations.push_back(rec);
    }

    // Accuracy-based recommendations
    if (current_metrics_.current_accuracy < 0.65) {
        OptimizationRecommendation rec;
        rec.category = "Decision Making";
        rec.recommendation = "Improve decision accuracy through better threshold calibration";
        rec.potential_impact = 0.15; // Estimated 15% accuracy improvement
        rec.implementation_priority = "Medium";
        rec.required_actions = {
            "Analyze false positive and false negative rates",
            "Adjust buy/sell thresholds based on historical performance",
            "Implement dynamic threshold adjustment"
        };
        recommendations.push_back(rec);
    }

    return recommendations;
}

// Real-Time Correlation Tracker Implementation
void RealTimeCorrelationTracker::addPredictionOutcomePair(double prediction, double outcome) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    prediction_outcome_pairs_.push_back({prediction, outcome});

    // Keep a reasonable number of pairs for calculation
    if (prediction_outcome_pairs_.size() > 500) {
        prediction_outcome_pairs_.erase(prediction_outcome_pairs_.begin());
    }
}

RealTimeCorrelationTracker::CorrelationUpdate
RealTimeCorrelationTracker::getCurrentCorrelation() const {
    std::lock_guard<std::mutex> lock(data_mutex_);

    CorrelationUpdate update;
    update.update_time = std::chrono::system_clock::now();
    update.sample_size = static_cast<int>(prediction_outcome_pairs_.size());

    if (prediction_outcome_pairs_.size() < 3) {
        update.correlation_value = 0.0;
        update.p_value = 1.0;
        update.statistically_significant = false;
        return update;
    }

    update.correlation_value = calculatePearsonCorrelation();
    update.p_value = calculatePValue(update.correlation_value, update.sample_size);
    update.statistically_significant = update.p_value < 0.05;

    return update;
}

bool RealTimeCorrelationTracker::meetsTRSRequirements(double minimum_correlation,
                                                     double significance_level) const {
    auto update = getCurrentCorrelation();
    return update.correlation_value >= minimum_correlation &&
           update.p_value <= significance_level &&
           update.sample_size >= 30; // Minimum sample size for reliability
}

double RealTimeCorrelationTracker::calculatePearsonCorrelation() const {
    if (prediction_outcome_pairs_.size() < 2) {
        return 0.0;
    }

    size_t n = prediction_outcome_pairs_.size();

    // Extract predictions and outcomes
    std::vector<double> predictions, outcomes;
    predictions.reserve(n);
    outcomes.reserve(n);

    for (const auto& pair : prediction_outcome_pairs_) {
        predictions.push_back(pair.first);
        outcomes.push_back(pair.second);
    }

    // Calculate means
    double pred_mean = std::accumulate(predictions.begin(), predictions.end(), 0.0) / n;
    double outcome_mean = std::accumulate(outcomes.begin(), outcomes.end(), 0.0) / n;

    // Calculate correlation
    double numerator = 0.0;
    double pred_var = 0.0;
    double outcome_var = 0.0;

    for (size_t i = 0; i < n; i++) {
        double pred_diff = predictions[i] - pred_mean;
        double outcome_diff = outcomes[i] - outcome_mean;

        numerator += pred_diff * outcome_diff;
        pred_var += pred_diff * pred_diff;
        outcome_var += outcome_diff * outcome_diff;
    }

    if (pred_var == 0.0 || outcome_var == 0.0) {
        return 0.0;
    }

    return numerator / std::sqrt(pred_var * outcome_var);
}

double RealTimeCorrelationTracker::calculatePValue(double correlation, int sample_size) const {
    if (sample_size < 3) {
        return 1.0;
    }

    // Simplified t-test for correlation significance
    double t_stat = correlation * std::sqrt((sample_size - 2) / (1 - correlation * correlation));

    // Very simplified p-value approximation
    // In practice, would use proper statistical libraries
    return std::max(0.001, 1.0 - std::abs(t_stat) / 3.0);
}

double AlgorithmPerformanceMonitor::calculateStatisticalSignificance(double correlation, int sample_size) {
    // Calculate statistical significance (p-value) for correlation coefficient
    if (sample_size < 3) {
        return 1.0; // Not significant with insufficient data
    }

    // Calculate t-statistic for correlation significance - TRS compliance
    double t_stat = correlation * std::sqrt((sample_size - 2) / (1 - correlation * correlation + 1e-10));

    // Simplified p-value calculation (two-tailed test)
    double abs_t = std::abs(t_stat);

    // Approximate p-value using normal distribution approximation
    if (abs_t >= 2.58) return 0.01;      // Very significant
    if (abs_t >= 1.96) return 0.05;      // Significant for TRS compliance
    if (abs_t >= 1.64) return 0.10;      // Marginally significant

    // Linear approximation for other values
    return std::max(0.001, 0.5 - (abs_t / 4.0));
}

} // namespace Algorithm
} // namespace CryptoClaude