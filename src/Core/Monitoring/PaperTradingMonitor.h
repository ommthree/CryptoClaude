#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <chrono>
#include <atomic>
#include <thread>
#include <mutex>
#include "../Trading/PaperTradingEngine.h"
#include "../Database/DatabaseManager.h"

namespace CryptoClaude {
namespace Monitoring {

/**
 * Real-time performance alert
 */
struct PerformanceAlert {
    std::string alert_id;
    std::chrono::system_clock::time_point timestamp;
    std::string severity; // "LOW", "MEDIUM", "HIGH", "CRITICAL"
    std::string category; // "PERFORMANCE", "RISK", "POSITION", "MARKET"
    std::string message;
    double trigger_value;
    double threshold_value;
    std::string symbol;
    bool acknowledged;

    PerformanceAlert() : trigger_value(0.0), threshold_value(0.0), acknowledged(false) {}
    PerformanceAlert(const std::string& cat, const std::string& sev, const std::string& msg,
                    double trigger, double threshold, const std::string& sym = "")
        : timestamp(std::chrono::system_clock::now()), severity(sev), category(cat),
          message(msg), trigger_value(trigger), threshold_value(threshold),
          symbol(sym), acknowledged(false) {}
};

/**
 * Performance metrics snapshot
 */
struct PerformanceMetrics {
    std::chrono::system_clock::time_point timestamp;

    // Portfolio Metrics
    double total_equity;
    double cash_balance;
    double total_pnl;
    double unrealized_pnl;
    double realized_pnl;
    double daily_return;
    double session_return;

    // Risk Metrics
    double current_drawdown;
    double max_drawdown;
    double volatility;
    double value_at_risk_95;
    double sharpe_ratio;

    // Position Metrics
    int active_positions;
    double total_exposure;
    double largest_position_size;
    std::map<std::string, double> position_pnls;

    // Trading Metrics
    int total_trades_today;
    int winning_trades_today;
    double win_rate_today;
    double average_trade_pnl;

    PerformanceMetrics() : total_equity(0.0), cash_balance(0.0), total_pnl(0.0),
                          unrealized_pnl(0.0), realized_pnl(0.0), daily_return(0.0),
                          session_return(0.0), current_drawdown(0.0), max_drawdown(0.0),
                          volatility(0.0), value_at_risk_95(0.0), sharpe_ratio(0.0),
                          active_positions(0), total_exposure(0.0), largest_position_size(0.0),
                          total_trades_today(0), winning_trades_today(0), win_rate_today(0.0),
                          average_trade_pnl(0.0) {}
};

/**
 * Alert configuration
 */
struct AlertConfig {
    // Drawdown Alerts
    double max_drawdown_warning; // -5%
    double max_drawdown_critical; // -10%

    // P&L Alerts
    double daily_loss_warning; // -2%
    double daily_loss_critical; // -5%

    // Position Alerts
    double max_position_size_warning; // 20%
    double max_position_size_critical; // 30%

    // Risk Alerts
    double var_breach_threshold; // 95% VaR breach
    double volatility_spike_threshold; // 3x normal volatility

    // Trading Frequency
    int max_trades_per_hour_warning; // 50 trades
    int max_trades_per_hour_critical; // 100 trades

    AlertConfig() : max_drawdown_warning(-0.05), max_drawdown_critical(-0.10),
                   daily_loss_warning(-0.02), daily_loss_critical(-0.05),
                   max_position_size_warning(0.20), max_position_size_critical(0.30),
                   var_breach_threshold(0.95), volatility_spike_threshold(3.0),
                   max_trades_per_hour_warning(50), max_trades_per_hour_critical(100) {}
};

/**
 * Dashboard data for real-time display
 */
struct TradingDashboard {
    PerformanceMetrics current_metrics;
    std::vector<PerformanceAlert> active_alerts;
    std::vector<double> equity_curve_1hour;
    std::vector<double> pnl_curve_1hour;
    std::map<std::string, Trading::VirtualPosition> top_positions;
    std::vector<Trading::VirtualOrder> recent_orders;

    // Market Data
    std::map<std::string, Trading::LiveMarketData> current_prices;

    TradingDashboard() = default;
};

/**
 * Real-time paper trading performance monitor
 * Tracks portfolio performance, generates alerts, and provides dashboard data
 */
class PaperTradingMonitor {
public:
    PaperTradingMonitor();
    ~PaperTradingMonitor();

    // Initialization
    bool initialize(std::shared_ptr<Trading::PaperTradingEngine> trading_engine,
                   std::shared_ptr<Database::DatabaseManager> db_manager);
    void shutdown();

    // Monitoring Control
    bool startMonitoring(const std::string& session_name);
    void stopMonitoring();
    bool isMonitoring() const { return monitoring_active_.load(); }

    // Configuration
    void setAlertConfig(const AlertConfig& config) { alert_config_ = config; }
    AlertConfig getAlertConfig() const { return alert_config_; }

    // Real-time Metrics
    PerformanceMetrics getCurrentMetrics();
    TradingDashboard getDashboardData();
    std::vector<PerformanceAlert> getActiveAlerts();
    std::vector<PerformanceAlert> getRecentAlerts(int hours = 24);

    // Alert Management
    void acknowledgeAlert(const std::string& alert_id);
    void clearAcknowledgedAlerts();
    void suppressAlertsForMinutes(int minutes);

    // Performance Analysis
    double calculateCurrentDrawdown();
    double calculateDailyReturn();
    double calculateVolatility(int hours = 24);
    double calculateValueAtRisk(double confidence_level = 0.95);
    double calculateSharpeRatio();

    // Risk Monitoring
    std::vector<std::string> identifyHighRiskPositions();
    bool checkPortfolioRiskLimits();
    double calculatePortfolioConcentration();

    // Reporting
    std::string generateAlertSummary();
    std::string generatePerformanceReport();
    bool exportMonitoringData(const std::string& filename);

    // Historical Data
    std::vector<PerformanceMetrics> getPerformanceHistory(int hours = 24);
    std::vector<double> getEquityCurve(int hours = 24);
    std::vector<double> getPnLCurve(int hours = 24);

private:
    std::shared_ptr<Trading::PaperTradingEngine> trading_engine_;
    std::shared_ptr<Database::DatabaseManager> db_manager_;

    // Monitoring State
    std::atomic<bool> monitoring_active_{false};
    std::string current_session_name_;
    std::chrono::system_clock::time_point monitoring_start_time_;

    // Configuration
    AlertConfig alert_config_;

    // Alert Management
    std::vector<PerformanceAlert> active_alerts_;
    std::mutex alerts_mutex_;
    std::atomic<int> alert_counter_{0};
    std::chrono::system_clock::time_point alert_suppression_end_;

    // Performance Tracking
    std::vector<PerformanceMetrics> metrics_history_;
    std::mutex metrics_mutex_;
    double session_peak_equity_;
    double daily_start_equity_;

    // Background Processing
    std::thread monitoring_thread_;
    std::thread alert_processing_thread_;
    std::atomic<bool> stop_monitoring_threads_{false};

    // Background Processing Methods
    void monitoringLoop();
    void alertProcessingLoop();

    // Metrics Calculation
    void calculateAndStoreMetrics();
    PerformanceMetrics calculateCurrentMetricsInternal();

    // Alert Generation
    void checkAndGenerateAlerts(const PerformanceMetrics& metrics);
    void generateDrawdownAlert(const PerformanceMetrics& metrics);
    void generatePnLAlert(const PerformanceMetrics& metrics);
    void generatePositionAlert(const PerformanceMetrics& metrics);
    void generateRiskAlert(const PerformanceMetrics& metrics);
    void generateTradingFrequencyAlert();

    // Alert Utilities
    void addAlert(const PerformanceAlert& alert);
    std::string generateAlertId();
    bool isAlertSuppressed();

    // Statistical Calculations
    std::vector<double> getReturns(int hours);
    double calculateMean(const std::vector<double>& values);
    double calculateStandardDeviation(const std::vector<double>& values);
    double calculatePercentile(const std::vector<double>& values, double percentile);

    // Database Operations
    bool createMonitoringTables();
    bool saveMetrics(const PerformanceMetrics& metrics);
    bool saveAlert(const PerformanceAlert& alert);
    std::vector<PerformanceMetrics> loadMetricsHistory(int hours);

    // Utility Functions
    std::string timePointToString(const std::chrono::system_clock::time_point& tp);
    std::string formatCurrency(double amount);
    std::string formatPercentage(double value);
};

}} // namespace CryptoClaude::Monitoring