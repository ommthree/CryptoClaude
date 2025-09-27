#include "PaperTradingMonitor.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <fstream>

namespace CryptoClaude {
namespace Monitoring {

PaperTradingMonitor::PaperTradingMonitor()
    : session_peak_equity_(0.0)
    , daily_start_equity_(0.0) {
}

PaperTradingMonitor::~PaperTradingMonitor() {
    shutdown();
}

bool PaperTradingMonitor::initialize(std::shared_ptr<Trading::PaperTradingEngine> trading_engine,
                                    std::shared_ptr<Database::DatabaseManager> db_manager) {
    trading_engine_ = trading_engine;
    db_manager_ = db_manager;

    if (!trading_engine_ || !db_manager_) {
        std::cerr << "PaperTradingMonitor: Invalid trading engine or database manager" << std::endl;
        return false;
    }

    // Create monitoring database tables
    if (!createMonitoringTables()) {
        std::cerr << "PaperTradingMonitor: Failed to create monitoring tables" << std::endl;
        return false;
    }

    std::cout << "PaperTradingMonitor: Initialized successfully" << std::endl;
    return true;
}

void PaperTradingMonitor::shutdown() {
    if (monitoring_active_.load()) {
        stopMonitoring();
    }

    stop_monitoring_threads_.store(true);

    if (monitoring_thread_.joinable()) {
        monitoring_thread_.join();
    }
    if (alert_processing_thread_.joinable()) {
        alert_processing_thread_.join();
    }

    std::cout << "PaperTradingMonitor: Shutdown complete" << std::endl;
}

bool PaperTradingMonitor::startMonitoring(const std::string& session_name) {
    if (monitoring_active_.load()) {
        std::cerr << "PaperTradingMonitor: Monitoring already active" << std::endl;
        return false;
    }

    if (!trading_engine_ || !trading_engine_->isSessionActive()) {
        std::cerr << "PaperTradingMonitor: No active trading session to monitor" << std::endl;
        return false;
    }

    current_session_name_ = session_name;
    monitoring_start_time_ = std::chrono::system_clock::now();
    monitoring_active_.store(true);
    stop_monitoring_threads_.store(false);

    // Initialize baseline metrics
    auto initial_snapshot = trading_engine_->getPortfolioSnapshot();
    session_peak_equity_ = initial_snapshot.total_equity;
    daily_start_equity_ = initial_snapshot.total_equity;

    // Start background monitoring threads
    monitoring_thread_ = std::thread(&PaperTradingMonitor::monitoringLoop, this);
    alert_processing_thread_ = std::thread(&PaperTradingMonitor::alertProcessingLoop, this);

    // Calculate and store initial metrics
    calculateAndStoreMetrics();

    std::cout << "PaperTradingMonitor: Started monitoring session '" << session_name
              << "' with initial equity $" << std::fixed << std::setprecision(2)
              << session_peak_equity_ << std::endl;

    return true;
}

void PaperTradingMonitor::stopMonitoring() {
    if (!monitoring_active_.load()) {
        return;
    }

    monitoring_active_.store(false);
    stop_monitoring_threads_.store(true);

    // Final metrics calculation
    calculateAndStoreMetrics();

    std::cout << "PaperTradingMonitor: Stopped monitoring session '" << current_session_name_
              << "'" << std::endl;
}

PerformanceMetrics PaperTradingMonitor::getCurrentMetrics() {
    if (!monitoring_active_.load()) {
        return PerformanceMetrics();
    }

    return calculateCurrentMetricsInternal();
}

TradingDashboard PaperTradingMonitor::getDashboardData() {
    TradingDashboard dashboard;

    if (!monitoring_active_.load() || !trading_engine_) {
        return dashboard;
    }

    // Current metrics
    dashboard.current_metrics = getCurrentMetrics();

    // Active alerts
    std::lock_guard<std::mutex> alerts_lock(alerts_mutex_);
    for (const auto& alert : active_alerts_) {
        if (!alert.acknowledged) {
            dashboard.active_alerts.push_back(alert);
        }
    }

    // Equity and P&L curves (last hour)
    dashboard.equity_curve_1hour = getEquityCurve(1);
    dashboard.pnl_curve_1hour = getPnLCurve(1);

    // Top positions by absolute P&L
    auto positions = trading_engine_->getActivePositions();
    std::sort(positions.begin(), positions.end(),
             [](const Trading::VirtualPosition& a, const Trading::VirtualPosition& b) {
                 return std::abs(a.unrealized_pnl) > std::abs(b.unrealized_pnl);
             });

    for (size_t i = 0; i < std::min(positions.size(), size_t(5)); ++i) {
        dashboard.top_positions[positions[i].symbol] = positions[i];
    }

    // Recent orders (last 10)
    auto order_history = trading_engine_->getOrderHistory();
    size_t recent_count = std::min(order_history.size(), size_t(10));
    for (size_t i = order_history.size() - recent_count; i < order_history.size(); ++i) {
        dashboard.recent_orders.push_back(order_history[i]);
    }

    return dashboard;
}

void PaperTradingMonitor::monitoringLoop() {
    while (!stop_monitoring_threads_.load()) {
        if (monitoring_active_.load()) {
            calculateAndStoreMetrics();
        }

        std::this_thread::sleep_for(std::chrono::seconds(30)); // Update every 30 seconds
    }
}

void PaperTradingMonitor::alertProcessingLoop() {
    while (!stop_monitoring_threads_.load()) {
        if (monitoring_active_.load() && !isAlertSuppressed()) {
            auto metrics = getCurrentMetrics();
            checkAndGenerateAlerts(metrics);
        }

        std::this_thread::sleep_for(std::chrono::seconds(10)); // Check alerts every 10 seconds
    }
}

void PaperTradingMonitor::calculateAndStoreMetrics() {
    auto metrics = calculateCurrentMetricsInternal();

    // Store in history
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    metrics_history_.push_back(metrics);

    // Keep only last 24 hours of data
    auto cutoff_time = std::chrono::system_clock::now() - std::chrono::hours(24);
    metrics_history_.erase(
        std::remove_if(metrics_history_.begin(), metrics_history_.end(),
                      [cutoff_time](const PerformanceMetrics& m) {
                          return m.timestamp < cutoff_time;
                      }),
        metrics_history_.end());

    // Update peak equity
    session_peak_equity_ = std::max(session_peak_equity_, metrics.total_equity);

    // Save to database
    saveMetrics(metrics);
}

PerformanceMetrics PaperTradingMonitor::calculateCurrentMetricsInternal() {
    PerformanceMetrics metrics;
    metrics.timestamp = std::chrono::system_clock::now();

    if (!trading_engine_) {
        return metrics;
    }

    // Get current portfolio snapshot
    auto snapshot = trading_engine_->getPortfolioSnapshot();

    // Basic portfolio metrics
    metrics.total_equity = snapshot.total_equity;
    metrics.cash_balance = snapshot.cash_balance;
    metrics.total_pnl = snapshot.total_pnl;
    metrics.unrealized_pnl = snapshot.unrealized_pnl;
    metrics.realized_pnl = snapshot.realized_pnl;

    // Session return
    if (daily_start_equity_ > 0) {
        metrics.session_return = (metrics.total_equity - daily_start_equity_) / daily_start_equity_;
    }

    // Daily return (simplified - would need day start tracking)
    metrics.daily_return = metrics.session_return; // For now, same as session return

    // Risk metrics
    metrics.current_drawdown = calculateCurrentDrawdown();
    metrics.max_drawdown = std::min(metrics.current_drawdown, -std::abs(metrics.current_drawdown));

    // Calculate volatility if we have enough history
    if (metrics_history_.size() > 10) {
        metrics.volatility = calculateVolatility(1); // Last hour volatility
    }

    // Position metrics
    auto positions = trading_engine_->getActivePositions();
    metrics.active_positions = static_cast<int>(positions.size());

    double total_position_value = 0.0;
    double largest_position = 0.0;

    for (const auto& position : positions) {
        double position_value = std::abs(position.quantity * position.entry_price);
        total_position_value += position_value;
        largest_position = std::max(largest_position, position_value);
        metrics.position_pnls[position.symbol] = position.unrealized_pnl;
    }

    metrics.total_exposure = total_position_value;
    metrics.largest_position_size = (metrics.total_equity > 0) ? largest_position / metrics.total_equity : 0.0;

    // Trading metrics (simplified - would need order tracking)
    metrics.total_trades_today = trading_engine_->getTotalTrades();
    metrics.win_rate_today = trading_engine_->getWinRate();

    return metrics;
}

double PaperTradingMonitor::calculateCurrentDrawdown() {
    if (session_peak_equity_ <= 0) return 0.0;

    auto snapshot = trading_engine_->getPortfolioSnapshot();
    return (snapshot.total_equity - session_peak_equity_) / session_peak_equity_;
}

double PaperTradingMonitor::calculateVolatility(int hours) {
    auto returns = getReturns(hours);
    if (returns.size() < 2) return 0.0;

    return calculateStandardDeviation(returns) * std::sqrt(24); // Annualized
}

std::vector<double> PaperTradingMonitor::getReturns(int hours) {
    std::vector<double> returns;

    std::lock_guard<std::mutex> lock(metrics_mutex_);
    if (metrics_history_.size() < 2) return returns;

    auto cutoff_time = std::chrono::system_clock::now() - std::chrono::hours(hours);

    for (size_t i = 1; i < metrics_history_.size(); ++i) {
        if (metrics_history_[i].timestamp >= cutoff_time) {
            double prev_equity = metrics_history_[i-1].total_equity;
            double curr_equity = metrics_history_[i].total_equity;

            if (prev_equity > 0) {
                returns.push_back((curr_equity - prev_equity) / prev_equity);
            }
        }
    }

    return returns;
}

void PaperTradingMonitor::checkAndGenerateAlerts(const PerformanceMetrics& metrics) {
    generateDrawdownAlert(metrics);
    generatePnLAlert(metrics);
    generatePositionAlert(metrics);
    generateRiskAlert(metrics);
    generateTradingFrequencyAlert();
}

void PaperTradingMonitor::generateDrawdownAlert(const PerformanceMetrics& metrics) {
    if (metrics.current_drawdown <= alert_config_.max_drawdown_critical) {
        PerformanceAlert alert("RISK", "CRITICAL",
                              "CRITICAL: Maximum drawdown limit exceeded",
                              metrics.current_drawdown, alert_config_.max_drawdown_critical);
        addAlert(alert);
    } else if (metrics.current_drawdown <= alert_config_.max_drawdown_warning) {
        PerformanceAlert alert("RISK", "HIGH",
                              "WARNING: Approaching maximum drawdown limit",
                              metrics.current_drawdown, alert_config_.max_drawdown_warning);
        addAlert(alert);
    }
}

void PaperTradingMonitor::generatePnLAlert(const PerformanceMetrics& metrics) {
    if (metrics.daily_return <= alert_config_.daily_loss_critical) {
        PerformanceAlert alert("PERFORMANCE", "CRITICAL",
                              "CRITICAL: Daily loss limit exceeded",
                              metrics.daily_return, alert_config_.daily_loss_critical);
        addAlert(alert);
    } else if (metrics.daily_return <= alert_config_.daily_loss_warning) {
        PerformanceAlert alert("PERFORMANCE", "HIGH",
                              "WARNING: Significant daily loss",
                              metrics.daily_return, alert_config_.daily_loss_warning);
        addAlert(alert);
    }
}

void PaperTradingMonitor::generatePositionAlert(const PerformanceMetrics& metrics) {
    if (metrics.largest_position_size >= alert_config_.max_position_size_critical) {
        PerformanceAlert alert("POSITION", "CRITICAL",
                              "CRITICAL: Position size limit exceeded",
                              metrics.largest_position_size, alert_config_.max_position_size_critical);
        addAlert(alert);
    } else if (metrics.largest_position_size >= alert_config_.max_position_size_warning) {
        PerformanceAlert alert("POSITION", "HIGH",
                              "WARNING: Large position size detected",
                              metrics.largest_position_size, alert_config_.max_position_size_warning);
        addAlert(alert);
    }
}

void PaperTradingMonitor::generateRiskAlert(const PerformanceMetrics& metrics) {
    // Check volatility spike
    if (metrics_history_.size() > 20) { // Need historical data for comparison
        std::vector<double> recent_volatilities;
        auto cutoff_time = std::chrono::system_clock::now() - std::chrono::hours(24);

        std::lock_guard<std::mutex> lock(metrics_mutex_);
        for (const auto& hist_metric : metrics_history_) {
            if (hist_metric.timestamp >= cutoff_time) {
                recent_volatilities.push_back(hist_metric.volatility);
            }
        }

        if (recent_volatilities.size() > 5) {
            double avg_volatility = calculateMean(recent_volatilities);
            if (metrics.volatility > avg_volatility * alert_config_.volatility_spike_threshold) {
                PerformanceAlert alert("RISK", "HIGH",
                                      "WARNING: Volatility spike detected",
                                      metrics.volatility, avg_volatility);
                addAlert(alert);
            }
        }
    }
}

void PaperTradingMonitor::generateTradingFrequencyAlert() {
    // Implementation would track order frequency and generate alerts for excessive trading
    // For now, this is a placeholder
}

void PaperTradingMonitor::addAlert(const PerformanceAlert& alert) {
    std::lock_guard<std::mutex> lock(alerts_mutex_);

    // Check if similar alert already exists and is recent (within 5 minutes)
    auto cutoff_time = std::chrono::system_clock::now() - std::chrono::minutes(5);
    bool similar_alert_exists = false;

    for (const auto& existing_alert : active_alerts_) {
        if (existing_alert.category == alert.category &&
            existing_alert.severity == alert.severity &&
            existing_alert.timestamp >= cutoff_time) {
            similar_alert_exists = true;
            break;
        }
    }

    if (!similar_alert_exists) {
        PerformanceAlert new_alert = alert;
        new_alert.alert_id = generateAlertId();
        active_alerts_.push_back(new_alert);

        // Save to database
        saveAlert(new_alert);

        std::cout << "PaperTradingMonitor: " << new_alert.severity << " alert generated - "
                  << new_alert.message << " (Value: " << std::fixed << std::setprecision(4)
                  << new_alert.trigger_value << ")" << std::endl;
    }
}

std::string PaperTradingMonitor::generateAlertId() {
    return "ALERT_" + std::to_string(alert_counter_.fetch_add(1)) + "_" +
           std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch()).count());
}

bool PaperTradingMonitor::isAlertSuppressed() {
    return std::chrono::system_clock::now() < alert_suppression_end_;
}

double PaperTradingMonitor::calculateMean(const std::vector<double>& values) {
    if (values.empty()) return 0.0;
    return std::accumulate(values.begin(), values.end(), 0.0) / values.size();
}

double PaperTradingMonitor::calculateStandardDeviation(const std::vector<double>& values) {
    if (values.size() < 2) return 0.0;

    double mean = calculateMean(values);
    double variance = 0.0;

    for (double value : values) {
        variance += (value - mean) * (value - mean);
    }
    variance /= (values.size() - 1);

    return std::sqrt(variance);
}

std::vector<double> PaperTradingMonitor::getEquityCurve(int hours) {
    std::vector<double> equity_curve;

    std::lock_guard<std::mutex> lock(metrics_mutex_);
    auto cutoff_time = std::chrono::system_clock::now() - std::chrono::hours(hours);

    for (const auto& metrics : metrics_history_) {
        if (metrics.timestamp >= cutoff_time) {
            equity_curve.push_back(metrics.total_equity);
        }
    }

    return equity_curve;
}

std::vector<double> PaperTradingMonitor::getPnLCurve(int hours) {
    std::vector<double> pnl_curve;

    std::lock_guard<std::mutex> lock(metrics_mutex_);
    auto cutoff_time = std::chrono::system_clock::now() - std::chrono::hours(hours);

    for (const auto& metrics : metrics_history_) {
        if (metrics.timestamp >= cutoff_time) {
            pnl_curve.push_back(metrics.total_pnl);
        }
    }

    return pnl_curve;
}

bool PaperTradingMonitor::createMonitoringTables() {
    if (!db_manager_) return false;

    try {
        // Performance metrics table
        std::string create_metrics_table = R"(
            CREATE TABLE IF NOT EXISTS monitoring_metrics (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                session_name TEXT NOT NULL,
                timestamp TEXT NOT NULL,
                total_equity REAL NOT NULL,
                cash_balance REAL NOT NULL,
                total_pnl REAL NOT NULL,
                unrealized_pnl REAL NOT NULL,
                realized_pnl REAL NOT NULL,
                daily_return REAL NOT NULL,
                session_return REAL NOT NULL,
                current_drawdown REAL NOT NULL,
                max_drawdown REAL NOT NULL,
                volatility REAL NOT NULL,
                active_positions INTEGER NOT NULL,
                total_exposure REAL NOT NULL,
                largest_position_size REAL NOT NULL
            )
        )";

        // Alerts table
        std::string create_alerts_table = R"(
            CREATE TABLE IF NOT EXISTS monitoring_alerts (
                alert_id TEXT PRIMARY KEY,
                session_name TEXT NOT NULL,
                timestamp TEXT NOT NULL,
                severity TEXT NOT NULL,
                category TEXT NOT NULL,
                message TEXT NOT NULL,
                trigger_value REAL NOT NULL,
                threshold_value REAL NOT NULL,
                symbol TEXT,
                acknowledged BOOLEAN DEFAULT FALSE
            )
        )";

        db_manager_->executeQuery(create_metrics_table);
        db_manager_->executeQuery(create_alerts_table);

        return true;

    } catch (const std::exception& e) {
        std::cerr << "PaperTradingMonitor: Error creating tables: " << e.what() << std::endl;
        return false;
    }
}

bool PaperTradingMonitor::saveMetrics(const PerformanceMetrics& metrics) {
    if (!db_manager_) return false;

    try {
        std::string query = R"(
            INSERT INTO monitoring_metrics (
                session_name, timestamp, total_equity, cash_balance, total_pnl,
                unrealized_pnl, realized_pnl, daily_return, session_return,
                current_drawdown, max_drawdown, volatility, active_positions,
                total_exposure, largest_position_size
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        )";

        std::vector<std::string> params = {
            current_session_name_,
            timePointToString(metrics.timestamp),
            std::to_string(metrics.total_equity),
            std::to_string(metrics.cash_balance),
            std::to_string(metrics.total_pnl),
            std::to_string(metrics.unrealized_pnl),
            std::to_string(metrics.realized_pnl),
            std::to_string(metrics.daily_return),
            std::to_string(metrics.session_return),
            std::to_string(metrics.current_drawdown),
            std::to_string(metrics.max_drawdown),
            std::to_string(metrics.volatility),
            std::to_string(metrics.active_positions),
            std::to_string(metrics.total_exposure),
            std::to_string(metrics.largest_position_size)
        };
        return db_manager_->executeParameterizedQuery(query, params);

    } catch (const std::exception& e) {
        std::cerr << "PaperTradingMonitor: Error saving metrics: " << e.what() << std::endl;
        return false;
    }
}

bool PaperTradingMonitor::saveAlert(const PerformanceAlert& alert) {
    if (!db_manager_) return false;

    try {
        std::string query = R"(
            INSERT INTO monitoring_alerts (
                alert_id, session_name, timestamp, severity, category, message,
                trigger_value, threshold_value, symbol, acknowledged
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        )";

        std::vector<std::string> params = {
            alert.alert_id,
            current_session_name_,
            timePointToString(alert.timestamp),
            alert.severity,
            alert.category,
            alert.message,
            std::to_string(alert.trigger_value),
            std::to_string(alert.threshold_value),
            alert.symbol,
            alert.acknowledged ? "1" : "0"
        };
        return db_manager_->executeParameterizedQuery(query, params);

    } catch (const std::exception& e) {
        std::cerr << "PaperTradingMonitor: Error saving alert: " << e.what() << std::endl;
        return false;
    }
}

std::string PaperTradingMonitor::timePointToString(const std::chrono::system_clock::time_point& tp) {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::string PaperTradingMonitor::formatCurrency(double amount) {
    std::stringstream ss;
    ss << "$" << std::fixed << std::setprecision(2) << amount;
    return ss.str();
}

std::string PaperTradingMonitor::formatPercentage(double value) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << (value * 100) << "%";
    return ss.str();
}

std::string PaperTradingMonitor::generatePerformanceReport() {
    std::stringstream report;

    auto metrics = getCurrentMetrics();

    report << "=== PAPER TRADING PERFORMANCE MONITOR REPORT ===" << std::endl;
    report << "Session: " << current_session_name_ << std::endl;
    report << "Timestamp: " << timePointToString(metrics.timestamp) << std::endl;
    report << std::endl;

    report << "PORTFOLIO METRICS:" << std::endl;
    report << "  Total Equity: " << formatCurrency(metrics.total_equity) << std::endl;
    report << "  Cash Balance: " << formatCurrency(metrics.cash_balance) << std::endl;
    report << "  Total P&L: " << formatCurrency(metrics.total_pnl) << std::endl;
    report << "  Unrealized P&L: " << formatCurrency(metrics.unrealized_pnl) << std::endl;
    report << "  Realized P&L: " << formatCurrency(metrics.realized_pnl) << std::endl;
    report << "  Session Return: " << formatPercentage(metrics.session_return) << std::endl;
    report << std::endl;

    report << "RISK METRICS:" << std::endl;
    report << "  Current Drawdown: " << formatPercentage(metrics.current_drawdown) << std::endl;
    report << "  Max Drawdown: " << formatPercentage(metrics.max_drawdown) << std::endl;
    report << "  Volatility: " << formatPercentage(metrics.volatility) << std::endl;
    report << std::endl;

    report << "POSITION METRICS:" << std::endl;
    report << "  Active Positions: " << metrics.active_positions << std::endl;
    report << "  Total Exposure: " << formatCurrency(metrics.total_exposure) << std::endl;
    report << "  Largest Position Size: " << formatPercentage(metrics.largest_position_size) << std::endl;
    report << std::endl;

    // Active alerts
    std::lock_guard<std::mutex> lock(alerts_mutex_);
    int unacknowledged_alerts = 0;
    for (const auto& alert : active_alerts_) {
        if (!alert.acknowledged) unacknowledged_alerts++;
    }

    report << "ALERTS:" << std::endl;
    report << "  Active Alerts: " << unacknowledged_alerts << std::endl;

    return report.str();
}

}} // namespace CryptoClaude::Monitoring