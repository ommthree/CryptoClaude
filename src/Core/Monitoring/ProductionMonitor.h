#pragma once

#include "../Market/LiveMarketDataProvider.h"
#include "../Risk/ProductionRiskManager.h"
#include "../Trading/OrderManagementSystem.h"
#include "../Algorithm/TRSComplianceEngine.h"
#include <memory>
#include <vector>
#include <map>
#include <string>
#include <chrono>
#include <atomic>
#include <mutex>
#include <thread>
#include <queue>
#include <functional>

namespace CryptoClaude {
namespace Monitoring {

/**
 * Production Monitor - Day 23 Implementation
 * Comprehensive monitoring and alerting system for live trading environment
 * Tracks system health, performance, and critical events
 */
class ProductionMonitor {
public:
    // Alert severity levels
    enum class AlertSeverity {
        INFO,           // Informational messages
        WARNING,        // Potential issues that need attention
        CRITICAL,       // Serious issues requiring immediate action
        EMERGENCY       // System-threatening issues requiring emergency response
    };

    // System component types
    enum class ComponentType {
        MARKET_DATA,
        RISK_MANAGER,
        ORDER_MANAGEMENT,
        TRS_COMPLIANCE,
        DATABASE,
        NETWORK,
        EXCHANGE_CONNECTION,
        ALGORITHM_ENGINE
    };

    // System health status
    enum class HealthStatus {
        HEALTHY,        // All systems operating normally
        DEGRADED,       // Some performance issues but functional
        CRITICAL,       // Major issues affecting operations
        OFFLINE         // System component not responding
    };

    // Monitoring alert
    struct Alert {
        std::string alert_id;
        std::chrono::system_clock::time_point timestamp;
        AlertSeverity severity;
        ComponentType component;
        std::string component_name;

        // Alert content
        std::string title;
        std::string description;
        std::map<std::string, std::string> metadata;

        // Response tracking
        bool is_acknowledged;
        std::chrono::system_clock::time_point acknowledged_at;
        std::string acknowledged_by;
        bool is_resolved;
        std::chrono::system_clock::time_point resolved_at;
        std::string resolution_notes;

        // Escalation
        int escalation_level;
        std::chrono::system_clock::time_point next_escalation_at;

        Alert() : severity(AlertSeverity::INFO), component(ComponentType::MARKET_DATA),
                 is_acknowledged(false), is_resolved(false), escalation_level(0) {}
    };

    // System component health
    struct ComponentHealth {
        ComponentType component_type;
        std::string component_name;
        HealthStatus status;
        std::chrono::system_clock::time_point last_check;

        // Performance metrics
        double cpu_usage_percent;
        double memory_usage_mb;
        std::chrono::milliseconds response_time;
        double error_rate_percent;

        // Component-specific metrics
        std::map<std::string, double> custom_metrics;

        // Status details
        std::string status_message;
        std::vector<std::string> active_issues;
        std::chrono::system_clock::time_point last_healthy_time;

        ComponentHealth() : component_type(ComponentType::MARKET_DATA),
                           status(HealthStatus::HEALTHY),
                           cpu_usage_percent(0), memory_usage_mb(0),
                           response_time(std::chrono::milliseconds{0}),
                           error_rate_percent(0) {}
    };

    // Performance dashboard data
    struct DashboardMetrics {
        std::chrono::system_clock::time_point snapshot_time;

        // Trading performance
        int orders_per_minute;
        int fills_per_minute;
        double average_execution_time_ms;
        double total_pnl_today;
        double current_drawdown_percent;

        // System performance
        double system_cpu_usage;
        double system_memory_usage;
        int active_connections;
        std::chrono::milliseconds average_latency;

        // Risk metrics
        double current_portfolio_value;
        double var_utilization_percent;
        Risk::ProductionRiskManager::RealTimeRiskAssessment::RiskLevel risk_level;
        Algorithm::TRSComplianceEngine::ComplianceStatus trs_status;

        // Market data quality
        double data_quality_score;
        int exchange_connections_active;
        double market_data_latency_ms;

        DashboardMetrics() : orders_per_minute(0), fills_per_minute(0),
                           average_execution_time_ms(0), total_pnl_today(0),
                           current_drawdown_percent(0), system_cpu_usage(0),
                           system_memory_usage(0), active_connections(0),
                           average_latency(std::chrono::milliseconds{0}),
                           current_portfolio_value(0), var_utilization_percent(0),
                           risk_level(Risk::ProductionRiskManager::RealTimeRiskAssessment::RiskLevel::GREEN),
                           trs_status(Algorithm::TRSComplianceEngine::ComplianceStatus::UNKNOWN),
                           data_quality_score(0), exchange_connections_active(0),
                           market_data_latency_ms(0) {}
    };

    // Monitoring thresholds and configuration
    struct MonitoringConfig {
        // Alert thresholds
        std::chrono::milliseconds max_response_time{5000};      // 5 second max response
        double max_error_rate_percent = 5.0;                   // 5% max error rate
        double max_cpu_usage_percent = 80.0;                   // 80% max CPU usage
        double max_memory_usage_mb = 8192.0;                   // 8GB max memory

        // Trading thresholds
        std::chrono::milliseconds max_execution_time{2000};     // 2 second max execution
        double max_slippage_bps = 50.0;                        // 50 bps max slippage
        double max_daily_loss_percent = 5.0;                   // 5% max daily loss

        // Alert timing
        std::chrono::minutes alert_cooldown{5};                // 5 minute cooldown between similar alerts
        std::chrono::minutes escalation_interval{15};          // 15 minute escalation interval
        int max_escalation_level = 3;                          // Maximum escalation level

        // Monitoring intervals
        std::chrono::seconds health_check_interval{30};        // Health check every 30 seconds
        std::chrono::seconds metrics_collection_interval{10};   // Collect metrics every 10 seconds
        std::chrono::minutes dashboard_update_interval{1};      // Update dashboard every minute

        MonitoringConfig() = default;
    };

private:
    // Configuration and state
    MonitoringConfig config_;
    std::atomic<bool> is_monitoring_{false};

    // Component integrations
    std::unique_ptr<Market::LiveMarketDataProvider> market_data_provider_;
    std::unique_ptr<Risk::ProductionRiskManager> risk_manager_;
    std::unique_ptr<Trading::OrderManagementSystem> order_manager_;
    std::unique_ptr<Algorithm::TRSComplianceEngine> compliance_engine_;

    // Threading
    std::thread health_monitoring_thread_;
    std::thread metrics_collection_thread_;
    std::thread alert_processing_thread_;
    std::thread dashboard_update_thread_;

    // Data storage
    mutable std::mutex alerts_mutex_;
    mutable std::mutex health_mutex_;
    mutable std::mutex metrics_mutex_;

    std::vector<Alert> active_alerts_;
    std::vector<Alert> alert_history_;
    std::map<ComponentType, ComponentHealth> component_health_;
    std::deque<DashboardMetrics> metrics_history_;
    DashboardMetrics current_dashboard_;

    // Alert management
    std::queue<Alert> pending_alerts_;
    std::map<std::string, std::chrono::system_clock::time_point> alert_cooldowns_;

    // Callback functions
    std::function<void(const Alert&)> alert_callback_;
    std::function<void(const ComponentHealth&)> health_callback_;
    std::function<void(const DashboardMetrics&)> dashboard_callback_;

    // Core monitoring methods
    void runHealthMonitoring();
    void runMetricsCollection();
    void runAlertProcessing();
    void runDashboardUpdate();

    // Health checking methods
    void checkComponentHealth();
    void checkMarketDataHealth();
    void checkRiskManagerHealth();
    void checkOrderManagerHealth();
    void checkTRSComplianceHealth();
    void checkSystemResourceHealth();

    // Metrics collection methods
    void collectTradingMetrics();
    void collectSystemMetrics();
    void collectRiskMetrics();
    void collectMarketDataMetrics();

    // Alert generation and processing
    void generateAlert(AlertSeverity severity, ComponentType component,
                      const std::string& title, const std::string& description,
                      const std::map<std::string, std::string>& metadata = {});
    void processAlert(Alert& alert);
    void escalateAlert(Alert& alert);
    bool shouldSuppressAlert(const Alert& alert);

    // Performance analysis
    void analyzePerformanceTrends();
    void detectAnomalies();

public:
    explicit ProductionMonitor(const MonitoringConfig& config = MonitoringConfig{});
    ~ProductionMonitor();

    // Configuration management
    void updateConfig(const MonitoringConfig& config);
    const MonitoringConfig& getConfig() const { return config_; }

    // Component integration
    bool integrateMarketDataProvider(std::unique_ptr<Market::LiveMarketDataProvider> provider);
    bool integrateRiskManager(std::unique_ptr<Risk::ProductionRiskManager> risk_manager);
    bool integrateOrderManager(std::unique_ptr<Trading::OrderManagementSystem> order_manager);
    bool integrateComplianceEngine(std::unique_ptr<Algorithm::TRSComplianceEngine> compliance_engine);

    // Monitoring control
    bool startMonitoring();
    void stopMonitoring();
    bool isMonitoring() const { return is_monitoring_.load(); }

    // Alert management
    std::vector<Alert> getActiveAlerts() const;
    std::vector<Alert> getAlertHistory(std::chrono::hours lookback = std::chrono::hours{24}) const;
    bool acknowledgeAlert(const std::string& alert_id, const std::string& acknowledged_by);
    bool resolveAlert(const std::string& alert_id, const std::string& resolution_notes);
    void clearResolvedAlerts();

    // Health status
    std::map<ComponentType, ComponentHealth> getComponentHealth() const;
    ComponentHealth getComponentHealth(ComponentType component) const;
    HealthStatus getOverallSystemHealth() const;

    // Dashboard and metrics
    DashboardMetrics getCurrentDashboard() const;
    std::vector<DashboardMetrics> getMetricsHistory(std::chrono::hours lookback = std::chrono::hours{24}) const;

    // Performance reports
    struct PerformanceReport {
        std::chrono::system_clock::time_point report_start;
        std::chrono::system_clock::time_point report_end;

        // Availability metrics
        double uptime_percentage;
        std::chrono::minutes total_downtime;
        int number_of_outages;

        // Performance metrics
        std::chrono::milliseconds average_response_time;
        std::chrono::milliseconds p95_response_time;
        std::chrono::milliseconds p99_response_time;

        // Trading performance
        int total_orders;
        double order_success_rate;
        double average_execution_time;
        double total_slippage_cost;

        // Alert statistics
        int total_alerts;
        int critical_alerts;
        int emergency_alerts;
        std::chrono::minutes average_resolution_time;

        // Resource utilization
        double peak_cpu_usage;
        double peak_memory_usage;
        double average_cpu_usage;
        double average_memory_usage;

        PerformanceReport() : uptime_percentage(0), total_downtime(std::chrono::minutes{0}),
                             number_of_outages(0), average_response_time(std::chrono::milliseconds{0}),
                             p95_response_time(std::chrono::milliseconds{0}),
                             p99_response_time(std::chrono::milliseconds{0}),
                             total_orders(0), order_success_rate(0), average_execution_time(0),
                             total_slippage_cost(0), total_alerts(0), critical_alerts(0),
                             emergency_alerts(0), average_resolution_time(std::chrono::minutes{0}),
                             peak_cpu_usage(0), peak_memory_usage(0), average_cpu_usage(0),
                             average_memory_usage(0) {}
    };

    PerformanceReport generatePerformanceReport(std::chrono::hours period = std::chrono::hours{24}) const;

    // Custom monitoring
    bool addCustomMetric(ComponentType component, const std::string& metric_name, double value);
    bool setCustomThreshold(ComponentType component, const std::string& metric_name,
                           double warning_threshold, double critical_threshold);

    // Notification channels
    struct NotificationChannel {
        std::string channel_name;
        std::string channel_type;           // "email", "slack", "webhook", "sms"
        std::string endpoint;               // Email address, webhook URL, etc.
        std::vector<AlertSeverity> severity_filter; // Which severities to send
        bool is_enabled;

        NotificationChannel() : is_enabled(true) {}
    };

    bool addNotificationChannel(const NotificationChannel& channel);
    bool removeNotificationChannel(const std::string& channel_name);
    void testNotificationChannel(const std::string& channel_name);

    // Emergency procedures
    void triggerEmergencyShutdown(const std::string& reason);
    void activateDisasterRecoveryMode();
    bool isEmergencyModeActive() const;

    // System diagnostics
    struct SystemDiagnostics {
        std::chrono::system_clock::time_point diagnostic_time;

        // System information
        std::string hostname;
        std::string os_version;
        double system_load_1m;
        double system_load_5m;
        double system_load_15m;

        // Memory breakdown
        double total_memory_mb;
        double available_memory_mb;
        double used_memory_mb;
        double cached_memory_mb;

        // Disk usage
        double disk_usage_percent;
        double disk_available_gb;

        // Network statistics
        double network_bytes_sent;
        double network_bytes_received;
        int network_connections_active;

        // Application-specific
        std::map<std::string, double> thread_pool_utilization;
        std::map<std::string, int> queue_depths;
        std::map<std::string, double> connection_pool_usage;

        SystemDiagnostics() : system_load_1m(0), system_load_5m(0), system_load_15m(0),
                             total_memory_mb(0), available_memory_mb(0), used_memory_mb(0),
                             cached_memory_mb(0), disk_usage_percent(0), disk_available_gb(0),
                             network_bytes_sent(0), network_bytes_received(0),
                             network_connections_active(0) {}
    };

    SystemDiagnostics runSystemDiagnostics() const;

    // Callback registration
    void setAlertCallback(std::function<void(const Alert&)> callback);
    void setHealthCallback(std::function<void(const ComponentHealth&)> callback);
    void setDashboardCallback(std::function<void(const DashboardMetrics&)> callback);

    // Data export
    bool exportAlerts(const std::string& filename, const std::string& format = "json",
                     std::chrono::hours lookback = std::chrono::hours{168}) const; // 1 week
    bool exportMetrics(const std::string& filename, const std::string& format = "csv",
                      std::chrono::hours lookback = std::chrono::hours{24}) const;
    bool exportPerformanceReport(const PerformanceReport& report, const std::string& filename) const;

    // Static utility methods
    static std::string alertSeverityToString(AlertSeverity severity);
    static AlertSeverity stringToAlertSeverity(const std::string& severity_str);
    static std::string componentTypeToString(ComponentType component);
    static ComponentType stringToComponentType(const std::string& component_str);
    static std::string healthStatusToString(HealthStatus status);
    static HealthStatus stringToHealthStatus(const std::string& status_str);
    static std::string generateAlertId();
};

} // namespace Monitoring
} // namespace CryptoClaude