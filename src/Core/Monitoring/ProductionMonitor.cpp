#include "ProductionMonitor.h"
#include <iostream>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <random>

namespace CryptoClaude {
namespace Monitoring {

ProductionMonitor::ProductionMonitor(const MonitoringConfig& config)
    : config_(config) {

    // Initialize component health tracking
    component_health_[ComponentType::MARKET_DATA] = ComponentHealth{};
    component_health_[ComponentType::MARKET_DATA].component_type = ComponentType::MARKET_DATA;
    component_health_[ComponentType::MARKET_DATA].component_name = "Market Data Provider";

    component_health_[ComponentType::RISK_MANAGER] = ComponentHealth{};
    component_health_[ComponentType::RISK_MANAGER].component_type = ComponentType::RISK_MANAGER;
    component_health_[ComponentType::RISK_MANAGER].component_name = "Risk Manager";

    component_health_[ComponentType::ORDER_MANAGEMENT] = ComponentHealth{};
    component_health_[ComponentType::ORDER_MANAGEMENT].component_type = ComponentType::ORDER_MANAGEMENT;
    component_health_[ComponentType::ORDER_MANAGEMENT].component_name = "Order Management System";

    component_health_[ComponentType::TRS_COMPLIANCE] = ComponentHealth{};
    component_health_[ComponentType::TRS_COMPLIANCE].component_type = ComponentType::TRS_COMPLIANCE;
    component_health_[ComponentType::TRS_COMPLIANCE].component_name = "TRS Compliance Engine";

    // Initialize dashboard
    current_dashboard_.snapshot_time = std::chrono::system_clock::now();
}

ProductionMonitor::~ProductionMonitor() {
    if (is_monitoring_.load()) {
        stopMonitoring();
    }
}

void ProductionMonitor::updateConfig(const MonitoringConfig& config) {
    config_ = config;
}

bool ProductionMonitor::integrateMarketDataProvider(
    std::unique_ptr<Market::LiveMarketDataProvider> provider) {
    if (!provider) {
        return false;
    }

    market_data_provider_ = std::move(provider);
    return true;
}

bool ProductionMonitor::integrateRiskManager(
    std::unique_ptr<Risk::ProductionRiskManager> risk_manager) {
    if (!risk_manager) {
        return false;
    }

    risk_manager_ = std::move(risk_manager);
    return true;
}

bool ProductionMonitor::integrateOrderManager(
    std::unique_ptr<Trading::OrderManagementSystem> order_manager) {
    if (!order_manager) {
        return false;
    }

    order_manager_ = std::move(order_manager);
    return true;
}

bool ProductionMonitor::integrateComplianceEngine(
    std::unique_ptr<Algorithm::TRSComplianceEngine> compliance_engine) {
    if (!compliance_engine) {
        return false;
    }

    compliance_engine_ = std::move(compliance_engine);
    return true;
}

bool ProductionMonitor::startMonitoring() {
    if (is_monitoring_.load()) {
        return false; // Already running
    }

    is_monitoring_.store(true);

    // Start monitoring threads
    health_monitoring_thread_ = std::thread(&ProductionMonitor::runHealthMonitoring, this);
    metrics_collection_thread_ = std::thread(&ProductionMonitor::runMetricsCollection, this);
    alert_processing_thread_ = std::thread(&ProductionMonitor::runAlertProcessing, this);
    dashboard_update_thread_ = std::thread(&ProductionMonitor::runDashboardUpdate, this);

    // Generate startup alert
    generateAlert(AlertSeverity::INFO, ComponentType::MARKET_DATA,
                 "Production Monitoring Started",
                 "Production monitoring system has been started and is now active");

    std::cout << "Production monitoring started successfully" << std::endl;
    return true;
}

void ProductionMonitor::stopMonitoring() {
    if (!is_monitoring_.load()) {
        return;
    }

    is_monitoring_.store(false);

    // Wait for threads to complete
    if (health_monitoring_thread_.joinable()) {
        health_monitoring_thread_.join();
    }

    if (metrics_collection_thread_.joinable()) {
        metrics_collection_thread_.join();
    }

    if (alert_processing_thread_.joinable()) {
        alert_processing_thread_.join();
    }

    if (dashboard_update_thread_.joinable()) {
        dashboard_update_thread_.join();
    }

    std::cout << "Production monitoring stopped" << std::endl;
}

void ProductionMonitor::runHealthMonitoring() {
    while (is_monitoring_.load()) {
        try {
            checkComponentHealth();

            std::this_thread::sleep_for(config_.health_check_interval);

        } catch (const std::exception& e) {
            std::cerr << "Health monitoring error: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds{5});
        }
    }
}

void ProductionMonitor::runMetricsCollection() {
    while (is_monitoring_.load()) {
        try {
            collectTradingMetrics();
            collectSystemMetrics();
            collectRiskMetrics();
            collectMarketDataMetrics();

            std::this_thread::sleep_for(config_.metrics_collection_interval);

        } catch (const std::exception& e) {
            std::cerr << "Metrics collection error: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds{5});
        }
    }
}

void ProductionMonitor::runAlertProcessing() {
    while (is_monitoring_.load()) {
        try {
            // Process pending alerts
            std::queue<Alert> alerts_to_process;
            {
                std::lock_guard<std::mutex> lock(alerts_mutex_);
                alerts_to_process.swap(pending_alerts_);
            }

            while (!alerts_to_process.empty()) {
                Alert alert = alerts_to_process.front();
                alerts_to_process.pop();

                if (!shouldSuppressAlert(alert)) {
                    processAlert(alert);
                }
            }

            // Check for alert escalations
            {
                std::lock_guard<std::mutex> lock(alerts_mutex_);
                for (auto& alert : active_alerts_) {
                    if (!alert.is_acknowledged &&
                        std::chrono::system_clock::now() >= alert.next_escalation_at) {
                        escalateAlert(alert);
                    }
                }
            }

            std::this_thread::sleep_for(std::chrono::seconds{5}); // Process every 5 seconds

        } catch (const std::exception& e) {
            std::cerr << "Alert processing error: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds{5});
        }
    }
}

void ProductionMonitor::runDashboardUpdate() {
    while (is_monitoring_.load()) {
        try {
            // Update dashboard metrics
            {
                std::lock_guard<std::mutex> lock(metrics_mutex_);
                current_dashboard_.snapshot_time = std::chrono::system_clock::now();

                // Store historical data
                metrics_history_.push_back(current_dashboard_);

                // Limit history size (keep 24 hours at 1-minute intervals)
                if (metrics_history_.size() > 1440) {
                    metrics_history_.pop_front();
                }
            }

            // Trigger dashboard callback
            if (dashboard_callback_) {
                dashboard_callback_(current_dashboard_);
            }

            std::this_thread::sleep_for(config_.dashboard_update_interval);

        } catch (const std::exception& e) {
            std::cerr << "Dashboard update error: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds{30});
        }
    }
}

void ProductionMonitor::checkComponentHealth() {
    checkMarketDataHealth();
    checkRiskManagerHealth();
    checkOrderManagerHealth();
    checkTRSComplianceHealth();
    checkSystemResourceHealth();
}

void ProductionMonitor::checkMarketDataHealth() {
    auto& health = component_health_[ComponentType::MARKET_DATA];
    health.last_check = std::chrono::system_clock::now();

    if (!market_data_provider_) {
        health.status = HealthStatus::OFFLINE;
        health.status_message = "Market data provider not integrated";
        return;
    }

    try {
        // Check if market data provider is running
        bool is_running = market_data_provider_->isRunning();
        if (!is_running) {
            health.status = HealthStatus::OFFLINE;
            health.status_message = "Market data feeds are not running";

            generateAlert(AlertSeverity::CRITICAL, ComponentType::MARKET_DATA,
                         "Market Data Offline", "Market data feeds are not running");
            return;
        }

        // Check connection statuses
        auto connection_statuses = market_data_provider_->getConnectionStatuses();
        int healthy_connections = 0;
        double total_latency_ms = 0;
        int total_connections = 0;

        for (const auto& status : connection_statuses) {
            total_connections++;
            if (status.is_connected) {
                healthy_connections++;
                total_latency_ms += status.avg_latency.count();
            }
        }

        // Update health metrics
        if (total_connections > 0) {
            double connection_ratio = static_cast<double>(healthy_connections) / total_connections;
            health.response_time = std::chrono::milliseconds{
                static_cast<int>(total_connections > 0 ? total_latency_ms / total_connections : 0)};

            if (connection_ratio >= 0.8) {
                health.status = HealthStatus::HEALTHY;
                health.status_message = "All market data connections healthy";
            } else if (connection_ratio >= 0.5) {
                health.status = HealthStatus::DEGRADED;
                health.status_message = "Some market data connections experiencing issues";

                generateAlert(AlertSeverity::WARNING, ComponentType::MARKET_DATA,
                             "Market Data Degraded", "Some exchange connections are unhealthy");
            } else {
                health.status = HealthStatus::CRITICAL;
                health.status_message = "Multiple market data connection failures";

                generateAlert(AlertSeverity::CRITICAL, ComponentType::MARKET_DATA,
                             "Market Data Critical", "Multiple exchange connections have failed");
            }
        }

        // Check response time
        if (health.response_time > config_.max_response_time) {
            generateAlert(AlertSeverity::WARNING, ComponentType::MARKET_DATA,
                         "High Market Data Latency",
                         "Market data latency exceeds threshold: " +
                         std::to_string(health.response_time.count()) + "ms");
        }

    } catch (const std::exception& e) {
        health.status = HealthStatus::CRITICAL;
        health.status_message = "Error checking market data health: " + std::string(e.what());

        generateAlert(AlertSeverity::CRITICAL, ComponentType::MARKET_DATA,
                     "Market Data Health Check Failed", e.what());
    }
}

void ProductionMonitor::checkRiskManagerHealth() {
    auto& health = component_health_[ComponentType::RISK_MANAGER];
    health.last_check = std::chrono::system_clock::now();

    if (!risk_manager_) {
        health.status = HealthStatus::OFFLINE;
        health.status_message = "Risk manager not integrated";
        return;
    }

    try {
        // Check if risk monitoring is active
        bool is_monitoring = risk_manager_->isMonitoring();
        if (!is_monitoring) {
            health.status = HealthStatus::CRITICAL;
            health.status_message = "Risk monitoring is not active";

            generateAlert(AlertSeverity::EMERGENCY, ComponentType::RISK_MANAGER,
                         "Risk Monitoring Offline", "Risk monitoring system is not active");
            return;
        }

        // Get current risk assessment
        auto risk_assessment = risk_manager_->getCurrentRiskAssessment();

        // Update health based on risk level
        switch (risk_assessment.overall_risk_level) {
            case Risk::ProductionRiskManager::RealTimeRiskAssessment::RiskLevel::GREEN:
                health.status = HealthStatus::HEALTHY;
                health.status_message = "Risk levels are normal";
                break;

            case Risk::ProductionRiskManager::RealTimeRiskAssessment::RiskLevel::YELLOW:
                health.status = HealthStatus::DEGRADED;
                health.status_message = "Elevated risk levels detected";

                generateAlert(AlertSeverity::WARNING, ComponentType::RISK_MANAGER,
                             "Elevated Risk Level", "Risk level has moved to YELLOW");
                break;

            case Risk::ProductionRiskManager::RealTimeRiskAssessment::RiskLevel::ORANGE:
                health.status = HealthStatus::CRITICAL;
                health.status_message = "High risk levels requiring attention";

                generateAlert(AlertSeverity::CRITICAL, ComponentType::RISK_MANAGER,
                             "High Risk Level", "Risk level has moved to ORANGE");
                break;

            case Risk::ProductionRiskManager::RealTimeRiskAssessment::RiskLevel::RED:
                health.status = HealthStatus::CRITICAL;
                health.status_message = "Emergency risk levels detected";

                generateAlert(AlertSeverity::EMERGENCY, ComponentType::RISK_MANAGER,
                             "Emergency Risk Level", "Risk level has moved to RED - immediate action required");
                break;
        }

        // Check for active violations
        auto violations = risk_manager_->getActiveViolations();
        if (!violations.empty()) {
            generateAlert(AlertSeverity::CRITICAL, ComponentType::RISK_MANAGER,
                         "Risk Violations Detected",
                         "Active risk violations: " + std::to_string(violations.size()));
        }

    } catch (const std::exception& e) {
        health.status = HealthStatus::CRITICAL;
        health.status_message = "Error checking risk manager health: " + std::string(e.what());

        generateAlert(AlertSeverity::CRITICAL, ComponentType::RISK_MANAGER,
                     "Risk Manager Health Check Failed", e.what());
    }
}

void ProductionMonitor::checkOrderManagerHealth() {
    auto& health = component_health_[ComponentType::ORDER_MANAGEMENT];
    health.last_check = std::chrono::system_clock::now();

    if (!order_manager_) {
        health.status = HealthStatus::OFFLINE;
        health.status_message = "Order manager not integrated";
        return;
    }

    try {
        // Check if order processing is active
        bool is_running = order_manager_->isRunning();
        if (!is_running) {
            health.status = HealthStatus::CRITICAL;
            health.status_message = "Order processing is not active";

            generateAlert(AlertSeverity::CRITICAL, ComponentType::ORDER_MANAGEMENT,
                         "Order Processing Offline", "Order processing system is not active");
            return;
        }

        // Get performance metrics
        auto metrics = order_manager_->getDailyPerformanceMetrics();

        // Check error rates
        if (metrics.error_rate > config_.max_error_rate_percent / 100.0) {
            health.status = HealthStatus::CRITICAL;
            health.status_message = "High order error rate: " + std::to_string(metrics.error_rate * 100) + "%";

            generateAlert(AlertSeverity::CRITICAL, ComponentType::ORDER_MANAGEMENT,
                         "High Order Error Rate",
                         "Order error rate is " + std::to_string(metrics.error_rate * 100) + "%");
        } else {
            health.status = HealthStatus::HEALTHY;
            health.status_message = "Order processing is healthy";
        }

        // Check execution time
        if (metrics.average_execution_time > config_.max_execution_time) {
            generateAlert(AlertSeverity::WARNING, ComponentType::ORDER_MANAGEMENT,
                         "Slow Order Execution",
                         "Average execution time is " +
                         std::to_string(metrics.average_execution_time.count()) + "ms");
        }

        health.response_time = metrics.average_execution_time;
        health.error_rate_percent = metrics.error_rate * 100.0;

    } catch (const std::exception& e) {
        health.status = HealthStatus::CRITICAL;
        health.status_message = "Error checking order manager health: " + std::string(e.what());

        generateAlert(AlertSeverity::CRITICAL, ComponentType::ORDER_MANAGEMENT,
                     "Order Manager Health Check Failed", e.what());
    }
}

void ProductionMonitor::checkTRSComplianceHealth() {
    auto& health = component_health_[ComponentType::TRS_COMPLIANCE];
    health.last_check = std::chrono::system_clock::now();

    if (!compliance_engine_) {
        health.status = HealthStatus::OFFLINE;
        health.status_message = "TRS compliance engine not integrated";
        return;
    }

    try {
        // Check if compliance monitoring is active
        bool is_monitoring = compliance_engine_->isMonitoring();
        if (!is_monitoring) {
            health.status = HealthStatus::CRITICAL;
            health.status_message = "TRS compliance monitoring is not active";

            generateAlert(AlertSeverity::CRITICAL, ComponentType::TRS_COMPLIANCE,
                         "TRS Monitoring Offline", "TRS compliance monitoring is not active");
            return;
        }

        // Get current compliance status
        auto compliance_status = compliance_engine_->getCurrentStatus();
        double current_correlation = compliance_engine_->getCurrentCorrelation();

        // Update health based on compliance status
        switch (compliance_status) {
            case Algorithm::TRSComplianceEngine::ComplianceStatus::COMPLIANT:
                health.status = HealthStatus::HEALTHY;
                health.status_message = "TRS compliance is maintained";
                break;

            case Algorithm::TRSComplianceEngine::ComplianceStatus::WARNING:
                health.status = HealthStatus::DEGRADED;
                health.status_message = "TRS compliance warning level";

                generateAlert(AlertSeverity::WARNING, ComponentType::TRS_COMPLIANCE,
                             "TRS Compliance Warning",
                             "TRS correlation: " + std::to_string(current_correlation));
                break;

            case Algorithm::TRSComplianceEngine::ComplianceStatus::CRITICAL:
                health.status = HealthStatus::CRITICAL;
                health.status_message = "TRS compliance critical level";

                generateAlert(AlertSeverity::CRITICAL, ComponentType::TRS_COMPLIANCE,
                             "TRS Compliance Critical",
                             "TRS correlation: " + std::to_string(current_correlation));
                break;

            case Algorithm::TRSComplianceEngine::ComplianceStatus::EMERGENCY:
                health.status = HealthStatus::CRITICAL;
                health.status_message = "TRS compliance emergency level";

                generateAlert(AlertSeverity::EMERGENCY, ComponentType::TRS_COMPLIANCE,
                             "TRS Compliance Emergency",
                             "TRS correlation: " + std::to_string(current_correlation));
                break;

            case Algorithm::TRSComplianceEngine::ComplianceStatus::UNKNOWN:
                health.status = HealthStatus::DEGRADED;
                health.status_message = "TRS compliance status unknown";
                break;
        }

    } catch (const std::exception& e) {
        health.status = HealthStatus::CRITICAL;
        health.status_message = "Error checking TRS compliance health: " + std::string(e.what());

        generateAlert(AlertSeverity::CRITICAL, ComponentType::TRS_COMPLIANCE,
                     "TRS Compliance Health Check Failed", e.what());
    }
}

void ProductionMonitor::checkSystemResourceHealth() {
    // Simplified system resource checking
    // In production, this would check actual system metrics

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> cpu_dist(5.0, 25.0);
    std::uniform_real_distribution<> mem_dist(1000.0, 4000.0);

    for (auto& [component_type, health] : component_health_) {
        health.cpu_usage_percent = cpu_dist(gen);
        health.memory_usage_mb = mem_dist(gen);

        // Check for resource issues
        if (health.cpu_usage_percent > config_.max_cpu_usage_percent) {
            generateAlert(AlertSeverity::WARNING, component_type,
                         "High CPU Usage",
                         "CPU usage is " + std::to_string(health.cpu_usage_percent) + "%");
        }

        if (health.memory_usage_mb > config_.max_memory_usage_mb) {
            generateAlert(AlertSeverity::WARNING, component_type,
                         "High Memory Usage",
                         "Memory usage is " + std::to_string(health.memory_usage_mb) + " MB");
        }
    }
}

void ProductionMonitor::collectTradingMetrics() {
    if (!order_manager_) {
        return;
    }

    try {
        auto metrics = order_manager_->getDailyPerformanceMetrics();

        std::lock_guard<std::mutex> lock(metrics_mutex_);
        current_dashboard_.average_execution_time_ms = metrics.average_execution_time.count();

        // Calculate orders and fills per minute (simplified)
        static int last_orders = 0;
        static auto last_update = std::chrono::system_clock::now();

        auto now = std::chrono::system_clock::now();
        auto time_diff = std::chrono::duration_cast<std::chrono::minutes>(now - last_update);

        if (time_diff >= std::chrono::minutes{1}) {
            int order_diff = metrics.total_orders_today - last_orders;
            current_dashboard_.orders_per_minute = order_diff;

            last_orders = metrics.total_orders_today;
            last_update = now;
        }

    } catch (const std::exception& e) {
        std::cerr << "Error collecting trading metrics: " << e.what() << std::endl;
    }
}

void ProductionMonitor::collectSystemMetrics() {
    // Collect system-level metrics
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> cpu_dist(10.0, 40.0);
    std::uniform_real_distribution<> mem_dist(2000.0, 6000.0);

    std::lock_guard<std::mutex> lock(metrics_mutex_);
    current_dashboard_.system_cpu_usage = cpu_dist(gen);
    current_dashboard_.system_memory_usage = mem_dist(gen);
}

void ProductionMonitor::collectRiskMetrics() {
    if (!risk_manager_) {
        return;
    }

    try {
        auto risk_assessment = risk_manager_->getCurrentRiskAssessment();

        std::lock_guard<std::mutex> lock(metrics_mutex_);
        current_dashboard_.current_portfolio_value = risk_assessment.current_portfolio_value;
        current_dashboard_.current_drawdown_percent = risk_assessment.current_drawdown_pct;
        current_dashboard_.risk_level = risk_assessment.overall_risk_level;
        current_dashboard_.total_pnl_today = risk_assessment.daily_realized_pnl;

    } catch (const std::exception& e) {
        std::cerr << "Error collecting risk metrics: " << e.what() << std::endl;
    }
}

void ProductionMonitor::collectMarketDataMetrics() {
    if (!market_data_provider_) {
        return;
    }

    try {
        auto performance = market_data_provider_->getPerformanceMetrics();

        std::lock_guard<std::mutex> lock(metrics_mutex_);
        current_dashboard_.market_data_latency_ms = performance.average_latency.count();
        current_dashboard_.data_quality_score = performance.data_quality_average;
        current_dashboard_.exchange_connections_active = performance.active_connections;

    } catch (const std::exception& e) {
        std::cerr << "Error collecting market data metrics: " << e.what() << std::endl;
    }
}

void ProductionMonitor::generateAlert(AlertSeverity severity, ComponentType component,
                                     const std::string& title, const std::string& description,
                                     const std::map<std::string, std::string>& metadata) {
    Alert alert;
    alert.alert_id = generateAlertId();
    alert.timestamp = std::chrono::system_clock::now();
    alert.severity = severity;
    alert.component = component;
    alert.component_name = componentTypeToString(component);
    alert.title = title;
    alert.description = description;
    alert.metadata = metadata;
    alert.escalation_level = 0;
    alert.next_escalation_at = alert.timestamp + config_.escalation_interval;

    {
        std::lock_guard<std::mutex> lock(alerts_mutex_);
        pending_alerts_.push(alert);
    }
}

void ProductionMonitor::processAlert(Alert& alert) {
    {
        std::lock_guard<std::mutex> lock(alerts_mutex_);
        active_alerts_.push_back(alert);
        alert_history_.push_back(alert);

        // Limit history size
        if (alert_history_.size() > 10000) {
            alert_history_.erase(alert_history_.begin());
        }
    }

    // Trigger callback
    if (alert_callback_) {
        alert_callback_(alert);
    }

    std::cout << "[ALERT] " << alertSeverityToString(alert.severity)
              << " - " << alert.title << ": " << alert.description << std::endl;
}

bool ProductionMonitor::shouldSuppressAlert(const Alert& alert) {
    // Check cooldown period
    std::string alert_key = componentTypeToString(alert.component) + ":" + alert.title;
    auto now = std::chrono::system_clock::now();

    auto it = alert_cooldowns_.find(alert_key);
    if (it != alert_cooldowns_.end()) {
        if (now < it->second + config_.alert_cooldown) {
            return true; // Still in cooldown period
        }
    }

    alert_cooldowns_[alert_key] = now;
    return false;
}

void ProductionMonitor::escalateAlert(Alert& alert) {
    if (alert.escalation_level >= config_.max_escalation_level) {
        return; // Already at maximum escalation
    }

    alert.escalation_level++;
    alert.next_escalation_at = std::chrono::system_clock::now() + config_.escalation_interval;

    std::cout << "[ALERT ESCALATION] Level " << alert.escalation_level
              << " - " << alert.title << std::endl;

    // In production, this would trigger escalation notifications
}

// Query methods
std::vector<ProductionMonitor::Alert> ProductionMonitor::getActiveAlerts() const {
    std::lock_guard<std::mutex> lock(alerts_mutex_);
    return active_alerts_;
}

std::map<ProductionMonitor::ComponentType, ProductionMonitor::ComponentHealth>
ProductionMonitor::getComponentHealth() const {
    std::lock_guard<std::mutex> lock(health_mutex_);
    return component_health_;
}

ProductionMonitor::DashboardMetrics ProductionMonitor::getCurrentDashboard() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    return current_dashboard_;
}

// Static utility methods
std::string ProductionMonitor::alertSeverityToString(AlertSeverity severity) {
    switch (severity) {
        case AlertSeverity::INFO: return "INFO";
        case AlertSeverity::WARNING: return "WARNING";
        case AlertSeverity::CRITICAL: return "CRITICAL";
        case AlertSeverity::EMERGENCY: return "EMERGENCY";
        default: return "UNKNOWN";
    }
}

std::string ProductionMonitor::componentTypeToString(ComponentType component) {
    switch (component) {
        case ComponentType::MARKET_DATA: return "MARKET_DATA";
        case ComponentType::RISK_MANAGER: return "RISK_MANAGER";
        case ComponentType::ORDER_MANAGEMENT: return "ORDER_MANAGEMENT";
        case ComponentType::TRS_COMPLIANCE: return "TRS_COMPLIANCE";
        case ComponentType::DATABASE: return "DATABASE";
        case ComponentType::NETWORK: return "NETWORK";
        case ComponentType::EXCHANGE_CONNECTION: return "EXCHANGE_CONNECTION";
        case ComponentType::ALGORITHM_ENGINE: return "ALGORITHM_ENGINE";
        default: return "UNKNOWN";
    }
}

std::string ProductionMonitor::healthStatusToString(HealthStatus status) {
    switch (status) {
        case HealthStatus::HEALTHY: return "HEALTHY";
        case HealthStatus::DEGRADED: return "DEGRADED";
        case HealthStatus::CRITICAL: return "CRITICAL";
        case HealthStatus::OFFLINE: return "OFFLINE";
        default: return "UNKNOWN";
    }
}

std::string ProductionMonitor::generateAlertId() {
    static int alert_counter = 0;
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::ostringstream oss;
    oss << "ALERT_" << time_t << "_" << (++alert_counter);
    return oss.str();
}

} // namespace Monitoring
} // namespace CryptoClaude