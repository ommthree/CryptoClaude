#pragma once

#include <string>
#include <chrono>
#include <vector>
#include <thread>
#include <atomic>
#include <memory>

namespace CryptoClaude {
namespace Monitoring {

struct SystemMetrics {
    std::chrono::system_clock::time_point timestamp;
    double cpu_usage_percent;
    size_t memory_usage_mb;
    size_t disk_usage_mb;
    size_t database_connections;
    size_t api_requests_per_minute;
    double system_uptime_hours;
    bool is_healthy;
    std::string status_message;
};

struct PerformanceAlert {
    enum Severity { INFO, WARNING, CRITICAL };

    Severity severity;
    std::string component;
    std::string message;
    std::chrono::system_clock::time_point timestamp;
    bool resolved;
};

class SystemMonitor {
private:
    std::atomic<bool> monitoring_active_;
    std::unique_ptr<std::thread> monitor_thread_;
    std::vector<SystemMetrics> metrics_history_;
    std::vector<PerformanceAlert> active_alerts_;

    int check_interval_seconds_;
    std::chrono::system_clock::time_point start_time_;

    // Thresholds
    double cpu_warning_threshold_;
    double cpu_critical_threshold_;
    size_t memory_warning_mb_;
    size_t memory_critical_mb_;
    size_t disk_warning_mb_;

public:
    SystemMonitor();
    ~SystemMonitor();

    // Control methods
    bool startMonitoring();
    void stopMonitoring();
    bool isMonitoring() const;

    // Configuration
    void setCheckInterval(int seconds);
    void setCpuThresholds(double warning, double critical);
    void setMemoryThresholds(size_t warning_mb, size_t critical_mb);
    void setDiskThreshold(size_t warning_mb);

    // Metrics collection
    SystemMetrics getCurrentMetrics() const;
    std::vector<SystemMetrics> getMetricsHistory(int last_n_entries = 100) const;

    // Health checking
    bool isSystemHealthy() const;
    std::vector<PerformanceAlert> getActiveAlerts() const;
    std::vector<PerformanceAlert> getCriticalAlerts() const;

    // Performance analysis
    double getAverageCpuUsage(int minutes = 10) const;
    size_t getAverageMemoryUsage(int minutes = 10) const;
    double getSystemUptime() const;

    // Alert management
    void acknowledgeAlert(size_t alert_id);
    void clearResolvedAlerts();
    size_t getActiveAlertCount() const;

    // Reporting
    std::string generateHealthReport() const;
    std::string generatePerformanceReport() const;

private:
    void monitoringLoop();
    SystemMetrics collectSystemMetrics() const;

    // System resource monitoring
    double getCpuUsage() const;
    size_t getMemoryUsage() const;
    size_t getDiskUsage() const;
    size_t getDatabaseConnectionCount() const;
    size_t getApiRequestRate() const;

    // Alert generation
    void checkThresholds(const SystemMetrics& metrics);
    void generateAlert(PerformanceAlert::Severity severity,
                      const std::string& component,
                      const std::string& message);
    void resolveAlert(const std::string& component, const std::string& condition);
};

} // namespace Monitoring
} // namespace CryptoClaude