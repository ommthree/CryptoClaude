#include "SystemMonitor.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <fstream>

#ifdef __APPLE__
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <sys/statvfs.h>
#elif __linux__
#include <sys/statvfs.h>
#include <unistd.h>
#endif

namespace CryptoClaude {
namespace Monitoring {

SystemMonitor::SystemMonitor()
    : monitoring_active_(false)
    , check_interval_seconds_(60)
    , start_time_(std::chrono::system_clock::now())
    , cpu_warning_threshold_(70.0)
    , cpu_critical_threshold_(90.0)
    , memory_warning_mb_(400)
    , memory_critical_mb_(500)
    , disk_warning_mb_(1000) {
}

SystemMonitor::~SystemMonitor() {
    stopMonitoring();
}

bool SystemMonitor::startMonitoring() {
    if (monitoring_active_.load()) {
        return false; // Already monitoring
    }

    monitoring_active_ = true;
    monitor_thread_ = std::make_unique<std::thread>(&SystemMonitor::monitoringLoop, this);

    std::cout << "System monitoring started" << std::endl;
    return true;
}

void SystemMonitor::stopMonitoring() {
    if (!monitoring_active_.load()) {
        return;
    }

    monitoring_active_ = false;
    if (monitor_thread_ && monitor_thread_->joinable()) {
        monitor_thread_->join();
    }

    std::cout << "System monitoring stopped" << std::endl;
}

bool SystemMonitor::isMonitoring() const {
    return monitoring_active_.load();
}

void SystemMonitor::setCheckInterval(int seconds) {
    check_interval_seconds_ = std::max(10, seconds); // Minimum 10 seconds
}

void SystemMonitor::setCpuThresholds(double warning, double critical) {
    cpu_warning_threshold_ = warning;
    cpu_critical_threshold_ = critical;
}

void SystemMonitor::setMemoryThresholds(size_t warning_mb, size_t critical_mb) {
    memory_warning_mb_ = warning_mb;
    memory_critical_mb_ = critical_mb;
}

void SystemMonitor::setDiskThreshold(size_t warning_mb) {
    disk_warning_mb_ = warning_mb;
}

SystemMetrics SystemMonitor::getCurrentMetrics() const {
    return collectSystemMetrics();
}

std::vector<SystemMetrics> SystemMonitor::getMetricsHistory(int last_n_entries) const {
    if (metrics_history_.empty()) {
        return {};
    }

    int start_idx = std::max(0, static_cast<int>(metrics_history_.size()) - last_n_entries);
    return std::vector<SystemMetrics>(
        metrics_history_.begin() + start_idx,
        metrics_history_.end()
    );
}

bool SystemMonitor::isSystemHealthy() const {
    // Check if there are any critical alerts
    for (const auto& alert : active_alerts_) {
        if (alert.severity == PerformanceAlert::CRITICAL && !alert.resolved) {
            return false;
        }
    }
    return true;
}

std::vector<PerformanceAlert> SystemMonitor::getActiveAlerts() const {
    std::vector<PerformanceAlert> active;
    for (const auto& alert : active_alerts_) {
        if (!alert.resolved) {
            active.push_back(alert);
        }
    }
    return active;
}

std::vector<PerformanceAlert> SystemMonitor::getCriticalAlerts() const {
    std::vector<PerformanceAlert> critical;
    for (const auto& alert : active_alerts_) {
        if (alert.severity == PerformanceAlert::CRITICAL && !alert.resolved) {
            critical.push_back(alert);
        }
    }
    return critical;
}

double SystemMonitor::getAverageCpuUsage(int minutes) const {
    if (metrics_history_.empty()) return 0.0;

    auto cutoff_time = std::chrono::system_clock::now() - std::chrono::minutes(minutes);
    double total = 0.0;
    int count = 0;

    for (const auto& metric : metrics_history_) {
        if (metric.timestamp >= cutoff_time) {
            total += metric.cpu_usage_percent;
            count++;
        }
    }

    return count > 0 ? total / count : 0.0;
}

size_t SystemMonitor::getAverageMemoryUsage(int minutes) const {
    if (metrics_history_.empty()) return 0;

    auto cutoff_time = std::chrono::system_clock::now() - std::chrono::minutes(minutes);
    size_t total = 0;
    int count = 0;

    for (const auto& metric : metrics_history_) {
        if (metric.timestamp >= cutoff_time) {
            total += metric.memory_usage_mb;
            count++;
        }
    }

    return count > 0 ? total / count : 0;
}

double SystemMonitor::getSystemUptime() const {
    auto now = std::chrono::system_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);
    return uptime.count() / 3600.0; // Convert to hours
}

size_t SystemMonitor::getActiveAlertCount() const {
    size_t count = 0;
    for (const auto& alert : active_alerts_) {
        if (!alert.resolved) {
            count++;
        }
    }
    return count;
}

std::string SystemMonitor::generateHealthReport() const {
    std::ostringstream report;
    auto metrics = getCurrentMetrics();

    report << "=== SYSTEM HEALTH REPORT ===" << std::endl;
    report << "Timestamp: " << std::chrono::duration_cast<std::chrono::seconds>(
        metrics.timestamp.time_since_epoch()).count() << std::endl;
    report << "Overall Health: " << (isSystemHealthy() ? "HEALTHY" : "UNHEALTHY") << std::endl;
    report << "System Uptime: " << getSystemUptime() << " hours" << std::endl;
    report << std::endl;

    report << "Performance Metrics:" << std::endl;
    report << "  CPU Usage: " << metrics.cpu_usage_percent << "%" << std::endl;
    report << "  Memory Usage: " << metrics.memory_usage_mb << " MB" << std::endl;
    report << "  Disk Usage: " << metrics.disk_usage_mb << " MB" << std::endl;
    report << "  Database Connections: " << metrics.database_connections << std::endl;
    report << "  API Requests/Min: " << metrics.api_requests_per_minute << std::endl;
    report << std::endl;

    auto active_alerts = getActiveAlerts();
    report << "Active Alerts: " << active_alerts.size() << std::endl;
    for (const auto& alert : active_alerts) {
        std::string severity_str = (alert.severity == PerformanceAlert::CRITICAL) ? "CRITICAL" :
                                  (alert.severity == PerformanceAlert::WARNING) ? "WARNING" : "INFO";
        report << "  [" << severity_str << "] " << alert.component << ": " << alert.message << std::endl;
    }

    return report.str();
}

std::string SystemMonitor::generatePerformanceReport() const {
    std::ostringstream report;

    report << "=== PERFORMANCE ANALYSIS REPORT ===" << std::endl;
    report << "Analysis Period: Last 10 minutes" << std::endl;
    report << std::endl;

    report << "Average Performance (10 min):" << std::endl;
    report << "  CPU Usage: " << getAverageCpuUsage(10) << "%" << std::endl;
    report << "  Memory Usage: " << getAverageMemoryUsage(10) << " MB" << std::endl;
    report << std::endl;

    report << "Thresholds:" << std::endl;
    report << "  CPU Warning: " << cpu_warning_threshold_ << "%" << std::endl;
    report << "  CPU Critical: " << cpu_critical_threshold_ << "%" << std::endl;
    report << "  Memory Warning: " << memory_warning_mb_ << " MB" << std::endl;
    report << "  Memory Critical: " << memory_critical_mb_ << " MB" << std::endl;
    report << std::endl;

    report << "Recent Metrics History: " << metrics_history_.size() << " entries" << std::endl;

    return report.str();
}

void SystemMonitor::monitoringLoop() {
    while (monitoring_active_.load()) {
        try {
            auto metrics = collectSystemMetrics();

            // Store metrics
            metrics_history_.push_back(metrics);

            // Keep only last 1000 entries to prevent memory growth
            if (metrics_history_.size() > 1000) {
                metrics_history_.erase(metrics_history_.begin());
            }

            // Check thresholds and generate alerts
            checkThresholds(metrics);

            // Sleep for the configured interval
            std::this_thread::sleep_for(std::chrono::seconds(check_interval_seconds_));

        } catch (const std::exception& e) {
            std::cerr << "Error in monitoring loop: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(10)); // Short sleep on error
        }
    }
}

SystemMetrics SystemMonitor::collectSystemMetrics() const {
    SystemMetrics metrics;
    metrics.timestamp = std::chrono::system_clock::now();

    metrics.cpu_usage_percent = getCpuUsage();
    metrics.memory_usage_mb = getMemoryUsage();
    metrics.disk_usage_mb = getDiskUsage();
    metrics.database_connections = getDatabaseConnectionCount();
    metrics.api_requests_per_minute = getApiRequestRate();
    metrics.system_uptime_hours = getSystemUptime();

    // Determine if system is healthy
    metrics.is_healthy = (metrics.cpu_usage_percent < cpu_critical_threshold_ &&
                         metrics.memory_usage_mb < memory_critical_mb_);

    if (metrics.is_healthy) {
        metrics.status_message = "System operating normally";
    } else {
        metrics.status_message = "System performance degraded - check alerts";
    }

    return metrics;
}

double SystemMonitor::getCpuUsage() const {
#ifdef __APPLE__
    // macOS CPU usage monitoring (simplified)
    host_cpu_load_info_data_t cpu_info;
    mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
    if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO,
                       (host_info_t)&cpu_info, &count) == KERN_SUCCESS) {
        // Simplified calculation - in production would track delta
        return 15.0; // Placeholder for demo
    }
#elif __linux__
    // Linux CPU usage from /proc/stat (simplified)
    std::ifstream file("/proc/stat");
    if (file.is_open()) {
        std::string line;
        std::getline(file, line);
        // Parse CPU usage - simplified for demo
        return 20.0; // Placeholder for demo
    }
#endif

    // Fallback - simulate reasonable CPU usage
    return 15.0 + (rand() % 20); // 15-35% simulated usage
}

size_t SystemMonitor::getMemoryUsage() const {
#ifdef __APPLE__
    // macOS memory usage
    vm_size_t page_size;
    vm_statistics64_data_t vm_stat;
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;

    host_page_size(mach_host_self(), &page_size);
    if (host_statistics64(mach_host_self(), HOST_VM_INFO64,
                         (host_info64_t)&vm_stat, &count) == KERN_SUCCESS) {
        // Calculate used memory in MB
        size_t used_memory = (vm_stat.active_count + vm_stat.inactive_count +
                             vm_stat.wire_count) * page_size / (1024 * 1024);
        return std::min(used_memory, static_cast<size_t>(512)); // Cap for demo
    }
#elif __linux__
    // Linux memory usage from /proc/meminfo
    std::ifstream file("/proc/meminfo");
    if (file.is_open()) {
        // Parse memory usage - simplified for demo
        return 128; // Placeholder for demo
    }
#endif

    // Fallback - simulate reasonable memory usage
    return 100 + (rand() % 50); // 100-150 MB simulated usage
}

size_t SystemMonitor::getDiskUsage() const {
    struct statvfs stat;
    if (statvfs(".", &stat) == 0) {
        size_t available_mb = (stat.f_bavail * stat.f_frsize) / (1024 * 1024);
        // Return used space (simplified - assume we want to track available space)
        return available_mb < disk_warning_mb_ ? disk_warning_mb_ - available_mb : 0;
    }

    return 100; // Fallback
}

size_t SystemMonitor::getDatabaseConnectionCount() const {
    // In real implementation, would query actual database connection pool
    return 2 + (rand() % 3); // Simulate 2-5 connections
}

size_t SystemMonitor::getApiRequestRate() const {
    // In real implementation, would track actual API request rate
    return 10 + (rand() % 20); // Simulate 10-30 requests per minute
}

void SystemMonitor::checkThresholds(const SystemMetrics& metrics) {
    // CPU threshold checks
    if (metrics.cpu_usage_percent >= cpu_critical_threshold_) {
        generateAlert(PerformanceAlert::CRITICAL, "CPU",
                     "CPU usage critical: " + std::to_string(metrics.cpu_usage_percent) + "%");
    } else if (metrics.cpu_usage_percent >= cpu_warning_threshold_) {
        generateAlert(PerformanceAlert::WARNING, "CPU",
                     "CPU usage high: " + std::to_string(metrics.cpu_usage_percent) + "%");
    } else {
        resolveAlert("CPU", "high_usage");
    }

    // Memory threshold checks
    if (metrics.memory_usage_mb >= memory_critical_mb_) {
        generateAlert(PerformanceAlert::CRITICAL, "Memory",
                     "Memory usage critical: " + std::to_string(metrics.memory_usage_mb) + " MB");
    } else if (metrics.memory_usage_mb >= memory_warning_mb_) {
        generateAlert(PerformanceAlert::WARNING, "Memory",
                     "Memory usage high: " + std::to_string(metrics.memory_usage_mb) + " MB");
    } else {
        resolveAlert("Memory", "high_usage");
    }
}

void SystemMonitor::generateAlert(PerformanceAlert::Severity severity,
                                 const std::string& component,
                                 const std::string& message) {
    // Check if similar alert already exists
    for (auto& alert : active_alerts_) {
        if (alert.component == component && alert.message == message && !alert.resolved) {
            return; // Don't duplicate alerts
        }
    }

    PerformanceAlert alert;
    alert.severity = severity;
    alert.component = component;
    alert.message = message;
    alert.timestamp = std::chrono::system_clock::now();
    alert.resolved = false;

    active_alerts_.push_back(alert);

    std::string severity_str = (severity == PerformanceAlert::CRITICAL) ? "CRITICAL" :
                              (severity == PerformanceAlert::WARNING) ? "WARNING" : "INFO";
    std::cout << "[ALERT " << severity_str << "] " << component << ": " << message << std::endl;
}

void SystemMonitor::resolveAlert(const std::string& component, const std::string& condition) {
    for (auto& alert : active_alerts_) {
        if (alert.component == component && !alert.resolved) {
            alert.resolved = true;
        }
    }
}

} // namespace Monitoring
} // namespace CryptoClaude