#include "ProductionRiskManager.h"
#include <iostream>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <random>
#include <sstream>
#include <iomanip>
#include <fstream>

namespace CryptoClaude {
namespace Risk {

ProductionRiskManager::ProductionRiskManager(const AccountConfiguration& config)
    : account_config_(config),
      portfolio_peak_value_(config.total_capital),
      last_peak_time_(std::chrono::system_clock::now()) {

    // Initialize risk assessment
    current_risk_assessment_.current_portfolio_value = config.available_capital;
    current_risk_assessment_.assessment_time = std::chrono::system_clock::now();
}

ProductionRiskManager::~ProductionRiskManager() {
    if (is_monitoring_.load()) {
        stopRiskMonitoring();
    }
}

void ProductionRiskManager::updateAccountConfiguration(const AccountConfiguration& config) {
    std::lock_guard<std::mutex> lock(risk_mutex_);
    account_config_ = config;

    // Update peak value if capital increased
    if (config.total_capital > portfolio_peak_value_) {
        portfolio_peak_value_ = config.total_capital;
        last_peak_time_ = std::chrono::system_clock::now();
    }
}

bool ProductionRiskManager::integrateMarketDataProvider(
    std::unique_ptr<Market::LiveMarketDataProvider> provider) {
    if (!provider) {
        return false;
    }

    market_data_provider_ = std::move(provider);
    return true;
}

bool ProductionRiskManager::integrateComplianceEngine(
    std::unique_ptr<Algorithm::TRSComplianceEngine> engine) {
    if (!engine) {
        return false;
    }

    compliance_engine_ = std::move(engine);
    return true;
}

bool ProductionRiskManager::integrateDatabaseManager(
    std::unique_ptr<Database::DatabaseManager> db_manager) {
    if (!db_manager) {
        return false;
    }

    database_manager_ = std::move(db_manager);
    return true;
}

bool ProductionRiskManager::startRiskMonitoring() {
    if (is_monitoring_.load()) {
        return false; // Already running
    }

    if (!market_data_provider_ || !compliance_engine_) {
        std::cerr << "Required components not integrated for risk monitoring" << std::endl;
        return false;
    }

    is_monitoring_.store(true);

    // Start monitoring threads
    risk_monitoring_thread_ = std::thread(&ProductionRiskManager::runRiskMonitoring, this);
    position_monitoring_thread_ = std::thread(&ProductionRiskManager::runPositionMonitoring, this);
    compliance_monitoring_thread_ = std::thread(&ProductionRiskManager::runComplianceMonitoring, this);

    std::cout << "Production risk monitoring started for account: " << account_config_.account_id << std::endl;
    return true;
}

void ProductionRiskManager::stopRiskMonitoring() {
    if (!is_monitoring_.load()) {
        return;
    }

    is_monitoring_.store(false);

    // Wait for threads to complete
    if (risk_monitoring_thread_.joinable()) {
        risk_monitoring_thread_.join();
    }

    if (position_monitoring_thread_.joinable()) {
        position_monitoring_thread_.join();
    }

    if (compliance_monitoring_thread_.joinable()) {
        compliance_monitoring_thread_.join();
    }

    std::cout << "Production risk monitoring stopped" << std::endl;
}

bool ProductionRiskManager::addPosition(const LivePosition& position) {
    std::lock_guard<std::mutex> lock(positions_mutex_);

    if (active_positions_.find(position.position_id) != active_positions_.end()) {
        return false; // Position already exists
    }

    active_positions_[position.position_id] = position;
    return true;
}

bool ProductionRiskManager::updatePosition(const std::string& position_id,
                                          const LivePosition& updated_position) {
    std::lock_guard<std::mutex> lock(positions_mutex_);

    auto it = active_positions_.find(position_id);
    if (it == active_positions_.end()) {
        return false; // Position not found
    }

    it->second = updated_position;
    return true;
}

bool ProductionRiskManager::removePosition(const std::string& position_id) {
    std::lock_guard<std::mutex> lock(positions_mutex_);

    auto it = active_positions_.find(position_id);
    if (it == active_positions_.end()) {
        return false; // Position not found
    }

    active_positions_.erase(it);
    return true;
}

std::vector<ProductionRiskManager::LivePosition>
ProductionRiskManager::getActivePositions() const {
    std::lock_guard<std::mutex> lock(positions_mutex_);

    std::vector<LivePosition> positions;
    for (const auto& [id, position] : active_positions_) {
        positions.push_back(position);
    }
    return positions;
}

ProductionRiskManager::LivePosition
ProductionRiskManager::getPosition(const std::string& position_id) const {
    std::lock_guard<std::mutex> lock(positions_mutex_);

    auto it = active_positions_.find(position_id);
    return (it != active_positions_.end()) ? it->second : LivePosition{};
}

ProductionRiskManager::TradeRiskCheck
ProductionRiskManager::evaluateProposedTrade(const std::string& symbol,
                                             double quantity,
                                             double estimated_price,
                                             bool is_long) const {
    TradeRiskCheck check;
    check.is_approved = false;

    // Calculate proposed position value
    double position_value = std::abs(quantity) * estimated_price;
    double position_pct = position_value / account_config_.total_capital;

    // Check position size limit
    if (position_pct > account_config_.max_position_size_pct) {
        check.rejection_reason = "Position size exceeds limit";
        check.max_allowed_quantity = (account_config_.max_position_size_pct * account_config_.total_capital) / estimated_price;
        return check;
    }

    // Check available capital
    if (position_value > account_config_.available_capital) {
        check.rejection_reason = "Insufficient available capital";
        check.max_allowed_quantity = account_config_.available_capital / estimated_price;
        return check;
    }

    // Check maximum positions limit
    {
        std::lock_guard<std::mutex> lock(positions_mutex_);
        if (static_cast<int>(active_positions_.size()) >= account_config_.max_concurrent_positions) {
            check.rejection_reason = "Maximum concurrent positions reached";
            return check;
        }
    }

    // Check portfolio exposure limit
    double current_exposure = 0;
    {
        std::lock_guard<std::mutex> lock(positions_mutex_);
        for (const auto& [id, position] : active_positions_) {
            current_exposure += position.position_value_usd;
        }
    }

    double total_exposure_pct = (current_exposure + position_value) / account_config_.total_capital;
    if (total_exposure_pct > account_config_.max_portfolio_exposure_pct) {
        check.rejection_reason = "Portfolio exposure limit exceeded";
        double max_additional = account_config_.max_portfolio_exposure_pct * account_config_.total_capital - current_exposure;
        check.max_allowed_quantity = std::max(0.0, max_additional / estimated_price);
        return check;
    }

    // Emergency stop check
    if (emergency_stop_triggered_.load()) {
        check.rejection_reason = "Emergency stop is active";
        return check;
    }

    // All checks passed
    check.is_approved = true;
    check.max_allowed_quantity = quantity;
    check.estimated_portfolio_impact = position_pct;
    check.confidence_score = 0.95; // High confidence if all checks pass

    return check;
}

void ProductionRiskManager::runRiskMonitoring() {
    while (is_monitoring_.load()) {
        try {
            updateRealTimeRiskAssessment();
            calculatePortfolioRisk();
            checkRiskLimits();
            detectRiskViolations();

            // Store historical assessment
            {
                std::lock_guard<std::mutex> lock(risk_mutex_);
                risk_assessment_history_.push_back(current_risk_assessment_);

                // Limit history size
                if (risk_assessment_history_.size() > 2880) { // 24 hours at 30-second intervals
                    risk_assessment_history_.pop_front();
                }
            }

            std::this_thread::sleep_for(std::chrono::seconds{30}); // Update every 30 seconds

        } catch (const std::exception& e) {
            std::cerr << "Risk monitoring error: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds{5});
        }
    }
}

void ProductionRiskManager::runPositionMonitoring() {
    while (is_monitoring_.load()) {
        try {
            updatePositionMetrics();
            checkStopLossLevels();
            checkPositionTimeouts();
            calculatePositionRiskScores();

            std::this_thread::sleep_for(std::chrono::seconds{10}); // Update every 10 seconds

        } catch (const std::exception& e) {
            std::cerr << "Position monitoring error: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds{5});
        }
    }
}

void ProductionRiskManager::runComplianceMonitoring() {
    while (is_monitoring_.load()) {
        try {
            monitorTRSCompliance();
            checkRegulatoryRequirements();

            std::this_thread::sleep_for(std::chrono::minutes{1}); // Update every minute

        } catch (const std::exception& e) {
            std::cerr << "Compliance monitoring error: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds{10});
        }
    }
}

void ProductionRiskManager::updateRealTimeRiskAssessment() {
    std::lock_guard<std::mutex> lock(risk_mutex_);

    current_risk_assessment_.assessment_time = std::chrono::system_clock::now();

    // Calculate portfolio metrics
    double total_position_value = 0;
    double total_unrealized_pnl = 0;
    double largest_position = 0;
    int position_count = 0;

    {
        std::lock_guard<std::mutex> pos_lock(positions_mutex_);
        for (const auto& [id, position] : active_positions_) {
            total_position_value += position.position_value_usd;
            total_unrealized_pnl += position.unrealized_pnl;
            largest_position = std::max(largest_position, position.position_value_usd);
            position_count++;
        }
    }

    current_risk_assessment_.current_portfolio_value = account_config_.available_capital + total_position_value;
    current_risk_assessment_.total_unrealized_pnl = total_unrealized_pnl;
    current_risk_assessment_.open_positions_count = position_count;

    if (current_risk_assessment_.current_portfolio_value > 0) {
        current_risk_assessment_.largest_position_pct =
            (largest_position / current_risk_assessment_.current_portfolio_value) * 100.0;
    }

    // Update peak tracking and drawdown
    if (current_risk_assessment_.current_portfolio_value > portfolio_peak_value_) {
        portfolio_peak_value_ = current_risk_assessment_.current_portfolio_value;
        last_peak_time_ = std::chrono::system_clock::now();
    }

    current_risk_assessment_.current_drawdown_pct =
        ((portfolio_peak_value_ - current_risk_assessment_.current_portfolio_value) / portfolio_peak_value_) * 100.0;

    // Calculate effective leverage
    if (account_config_.available_capital > 0) {
        current_risk_assessment_.portfolio_leverage = total_position_value / account_config_.available_capital;
    }

    // Assess overall risk level
    assessOverallRiskLevel();
}

void ProductionRiskManager::assessOverallRiskLevel() {
    current_risk_assessment_.active_risk_warnings.clear();
    current_risk_assessment_.recommended_actions.clear();

    RealTimeRiskAssessment::RiskLevel risk_level = RealTimeRiskAssessment::RiskLevel::GREEN;

    // Check drawdown
    if (current_risk_assessment_.current_drawdown_pct > account_config_.max_drawdown_limit * 50) {
        risk_level = std::max(risk_level, RealTimeRiskAssessment::RiskLevel::YELLOW);
        current_risk_assessment_.active_risk_warnings.push_back("Moderate drawdown detected");
    }

    if (current_risk_assessment_.current_drawdown_pct > account_config_.max_drawdown_limit * 80) {
        risk_level = std::max(risk_level, RealTimeRiskAssessment::RiskLevel::ORANGE);
        current_risk_assessment_.active_risk_warnings.push_back("High drawdown - approaching limit");
        current_risk_assessment_.recommended_actions.push_back("Consider reducing position sizes");
    }

    if (current_risk_assessment_.current_drawdown_pct > account_config_.max_drawdown_limit) {
        risk_level = RealTimeRiskAssessment::RiskLevel::RED;
        current_risk_assessment_.active_risk_warnings.push_back("Drawdown limit exceeded");
        current_risk_assessment_.recommended_actions.push_back("Emergency position reduction required");
    }

    // Check position concentration
    if (current_risk_assessment_.largest_position_pct > account_config_.max_position_size_pct * 80 * 100) {
        risk_level = std::max(risk_level, RealTimeRiskAssessment::RiskLevel::YELLOW);
        current_risk_assessment_.active_risk_warnings.push_back("High position concentration");
    }

    // Check leverage
    if (current_risk_assessment_.portfolio_leverage > 2.0) {
        risk_level = std::max(risk_level, RealTimeRiskAssessment::RiskLevel::ORANGE);
        current_risk_assessment_.active_risk_warnings.push_back("High portfolio leverage");
        current_risk_assessment_.recommended_actions.push_back("Reduce leverage by closing positions");
    }

    current_risk_assessment_.overall_risk_level = risk_level;
}

void ProductionRiskManager::calculatePortfolioRisk() {
    // Calculate portfolio VaR using simplified Monte Carlo approach
    std::vector<double> position_values;
    std::vector<double> position_volatilities;

    {
        std::lock_guard<std::mutex> lock(positions_mutex_);
        for (const auto& [id, position] : active_positions_) {
            position_values.push_back(position.position_value_usd);
            // Estimate volatility as 2% daily for simplicity
            position_volatilities.push_back(0.02);
        }
    }

    if (position_values.empty()) {
        current_risk_assessment_.portfolio_var_1d = 0;
        return;
    }

    // Simple VaR calculation: 2.33 * portfolio_value * assumed_volatility for 99% confidence
    double total_portfolio = std::accumulate(position_values.begin(), position_values.end(), 0.0);
    double portfolio_volatility = 0.015; // Assumed 1.5% daily portfolio volatility
    current_risk_assessment_.portfolio_var_1d = total_portfolio * portfolio_volatility * 2.33;
}

void ProductionRiskManager::checkRiskLimits() {
    // Check VaR limit
    if (current_risk_assessment_.portfolio_var_1d > account_config_.portfolio_var_limit * account_config_.total_capital) {
        RiskViolation violation;
        violation.violation_id = generateViolationId();
        violation.detected_at = std::chrono::system_clock::now();
        violation.violation_type = "portfolio_var_limit";
        violation.description = "Portfolio VaR exceeds limit";
        violation.current_value = current_risk_assessment_.portfolio_var_1d;
        violation.limit_value = account_config_.portfolio_var_limit * account_config_.total_capital;
        violation.severity_score = 0.8;

        handleRiskViolation(violation);
    }

    // Check drawdown limit
    if (current_risk_assessment_.current_drawdown_pct > account_config_.max_drawdown_limit) {
        RiskViolation violation;
        violation.violation_id = generateViolationId();
        violation.detected_at = std::chrono::system_clock::now();
        violation.violation_type = "max_drawdown_limit";
        violation.description = "Maximum drawdown limit exceeded";
        violation.current_value = current_risk_assessment_.current_drawdown_pct;
        violation.limit_value = account_config_.max_drawdown_limit;
        violation.severity_score = 0.9;

        handleRiskViolation(violation);
    }
}

void ProductionRiskManager::detectRiskViolations() {
    // This method would contain additional violation detection logic
    // For now, we rely on the checks in checkRiskLimits()
}

void ProductionRiskManager::handleRiskViolation(const RiskViolation& violation) {
    {
        std::lock_guard<std::mutex> lock(violations_mutex_);
        active_violations_.push_back(violation);
    }

    // Execute automated response
    executeAutomatedResponse(violation);

    // Trigger callback if registered
    if (violation_callback_) {
        violation_callback_(violation);
    }

    std::cout << "Risk violation detected: " << violation.description
              << " (Severity: " << violation.severity_score << ")" << std::endl;
}

void ProductionRiskManager::executeAutomatedResponse(const RiskViolation& violation) {
    if (violation.violation_type == "max_drawdown_limit" && violation.severity_score > 0.8) {
        triggerEmergencyStop("Maximum drawdown limit exceeded");
    } else if (violation.violation_type == "portfolio_var_limit") {
        // Could implement position size reduction here
        std::cout << "VaR limit exceeded - consider reducing positions" << std::endl;
    }
}

void ProductionRiskManager::updatePositionMetrics() {
    if (!market_data_provider_) {
        return;
    }

    std::lock_guard<std::mutex> lock(positions_mutex_);

    for (auto& [id, position] : active_positions_) {
        // Get current market data for this position
        auto market_view = market_data_provider_->getAggregatedView(position.symbol);

        if (market_view.weighted_mid > 0) {
            position.current_price = market_view.weighted_mid;
            position.position_value_usd = std::abs(position.quantity) * position.current_price;

            // Calculate unrealized P&L
            double price_diff = position.current_price - position.entry_price;
            if (!position.is_long) {
                price_diff = -price_diff; // Invert for short positions
            }

            position.unrealized_pnl = position.quantity * price_diff;
            if (position.entry_price > 0) {
                position.unrealized_pnl_pct = (price_diff / position.entry_price) * 100.0;
            }

            // Update max adverse/favorable excursions
            position.max_adverse_excursion = std::min(position.max_adverse_excursion, position.unrealized_pnl);
            position.max_favorable_excursion = std::max(position.max_favorable_excursion, position.unrealized_pnl);

            // Calculate portfolio impact
            if (current_risk_assessment_.current_portfolio_value > 0) {
                position.portfolio_impact_pct =
                    (position.position_value_usd / current_risk_assessment_.current_portfolio_value) * 100.0;
            }
        }
    }
}

void ProductionRiskManager::checkStopLossLevels() {
    std::lock_guard<std::mutex> lock(positions_mutex_);

    for (const auto& [id, position] : active_positions_) {
        if (position.stop_loss_price <= 0) {
            continue; // No stop loss set
        }

        bool stop_triggered = false;

        if (position.is_long && position.current_price <= position.stop_loss_price) {
            stop_triggered = true;
        } else if (!position.is_long && position.current_price >= position.stop_loss_price) {
            stop_triggered = true;
        }

        if (stop_triggered) {
            std::cout << "Stop loss triggered for position " << id
                      << " at price " << position.current_price << std::endl;

            // In a real implementation, this would trigger an order to close the position
            // For now, just log the event
        }
    }
}

void ProductionRiskManager::monitorTRSCompliance() {
    if (!compliance_engine_) {
        return;
    }

    auto compliance_status = compliance_engine_->getCurrentStatus();
    auto current_correlation = compliance_engine_->getCurrentCorrelation();
    auto trs_gap = compliance_engine_->getCurrentTRSGap();

    {
        std::lock_guard<std::mutex> lock(risk_mutex_);
        current_risk_assessment_.trs_status = compliance_status;
        current_risk_assessment_.current_algorithm_correlation = current_correlation;
        current_risk_assessment_.trs_gap = trs_gap;
    }

    // Check for TRS violations
    if (compliance_status == Algorithm::TRSComplianceEngine::ComplianceStatus::CRITICAL ||
        compliance_status == Algorithm::TRSComplianceEngine::ComplianceStatus::EMERGENCY) {

        RiskViolation violation;
        violation.violation_id = generateViolationId();
        violation.detected_at = std::chrono::system_clock::now();
        violation.violation_type = "trs_compliance";
        violation.description = "TRS compliance violation detected";
        violation.current_value = current_correlation;
        violation.limit_value = 0.85; // TRS target
        violation.severity_score = (compliance_status == Algorithm::TRSComplianceEngine::ComplianceStatus::EMERGENCY) ? 1.0 : 0.7;

        handleRiskViolation(violation);
    }
}

void ProductionRiskManager::triggerEmergencyStop(const std::string& reason) {
    if (emergency_stop_triggered_.load()) {
        return; // Already triggered
    }

    emergency_stop_triggered_.store(true);

    std::cout << "EMERGENCY STOP TRIGGERED: " << reason << std::endl;

    // Execute emergency procedures
    executeEmergencyPositionReduction();

    // Trigger emergency callback
    if (emergency_callback_) {
        emergency_callback_(reason);
    }
}

void ProductionRiskManager::executeEmergencyPositionReduction() {
    // In production, this would close all positions or reduce them to safe levels
    std::cout << "Executing emergency position reduction procedures..." << std::endl;

    // For demonstration, just log what would happen
    std::lock_guard<std::mutex> lock(positions_mutex_);
    for (const auto& [id, position] : active_positions_) {
        std::cout << "Would close position " << id << " (" << position.symbol
                  << ") with current P&L: " << position.unrealized_pnl << std::endl;
    }
}

// Static utility method implementations
std::string ProductionRiskManager::riskLevelToString(RealTimeRiskAssessment::RiskLevel level) {
    switch (level) {
        case RealTimeRiskAssessment::RiskLevel::GREEN: return "GREEN";
        case RealTimeRiskAssessment::RiskLevel::YELLOW: return "YELLOW";
        case RealTimeRiskAssessment::RiskLevel::ORANGE: return "ORANGE";
        case RealTimeRiskAssessment::RiskLevel::RED: return "RED";
        default: return "UNKNOWN";
    }
}

std::string ProductionRiskManager::generateViolationId() {
    static int violation_counter = 0;
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::ostringstream oss;
    oss << "VIOL_" << time_t << "_" << (++violation_counter);
    return oss.str();
}

double ProductionRiskManager::calculatePositionRisk(const LivePosition& position, double portfolio_value) {
    if (portfolio_value <= 0) return 0.0;

    // Simple risk score based on position size and unrealized loss
    double size_risk = (position.position_value_usd / portfolio_value);
    double pnl_risk = std::max(0.0, -position.unrealized_pnl_pct / 100.0); // Negative P&L as risk

    return std::min(1.0, size_risk + pnl_risk);
}

// Additional method stubs for completeness
ProductionRiskManager::RealTimeRiskAssessment
ProductionRiskManager::getCurrentRiskAssessment() const {
    std::lock_guard<std::mutex> lock(risk_mutex_);
    return current_risk_assessment_;
}

std::vector<ProductionRiskManager::RiskViolation>
ProductionRiskManager::getActiveViolations() const {
    std::lock_guard<std::mutex> lock(violations_mutex_);
    return active_violations_;
}

void ProductionRiskManager::setViolationCallback(std::function<void(const RiskViolation&)> callback) {
    violation_callback_ = callback;
}

void ProductionRiskManager::setAssessmentCallback(std::function<void(const RealTimeRiskAssessment&)> callback) {
    assessment_callback_ = callback;
}

void ProductionRiskManager::setEmergencyCallback(std::function<void(const std::string&)> callback) {
    emergency_callback_ = callback;
}

} // namespace Risk
} // namespace CryptoClaude