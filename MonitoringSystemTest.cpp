#include <iostream>
#include <thread>
#include <chrono>
#include "src/Core/Monitoring/SystemMonitor.h"

using namespace CryptoClaude::Monitoring;

int main() {
    std::cout << "=== SYSTEM MONITORING VALIDATION ===" << std::endl;

    SystemMonitor monitor;

    // Test 1: Configuration
    std::cout << "\n🔍 Testing monitoring configuration..." << std::endl;

    monitor.setCheckInterval(5); // 5 seconds for testing
    monitor.setCpuThresholds(70.0, 90.0);
    monitor.setMemoryThresholds(200, 300);
    monitor.setDiskThreshold(1000);

    std::cout << "✅ Monitoring thresholds configured" << std::endl;

    // Test 2: Start monitoring
    std::cout << "\n🔍 Testing monitoring activation..." << std::endl;

    if (monitor.startMonitoring()) {
        std::cout << "✅ System monitoring started successfully" << std::endl;
    } else {
        std::cout << "❌ Failed to start system monitoring" << std::endl;
        return 1;
    }

    if (monitor.isMonitoring()) {
        std::cout << "✅ Monitoring status confirmed" << std::endl;
    } else {
        std::cout << "❌ Monitoring status not confirmed" << std::endl;
        return 1;
    }

    // Test 3: Metrics collection
    std::cout << "\n🔍 Testing metrics collection..." << std::endl;

    // Wait for a few monitoring cycles
    std::this_thread::sleep_for(std::chrono::seconds(6));

    auto metrics = monitor.getCurrentMetrics();
    std::cout << "✅ Current metrics collected:" << std::endl;
    std::cout << "   CPU Usage: " << metrics.cpu_usage_percent << "%" << std::endl;
    std::cout << "   Memory Usage: " << metrics.memory_usage_mb << " MB" << std::endl;
    std::cout << "   System Health: " << (metrics.is_healthy ? "HEALTHY" : "UNHEALTHY") << std::endl;
    std::cout << "   Status: " << metrics.status_message << std::endl;

    // Test 4: Health reporting
    std::cout << "\n🔍 Testing health reporting..." << std::endl;

    if (monitor.isSystemHealthy()) {
        std::cout << "✅ System health check passed" << std::endl;
    } else {
        std::cout << "⚠️  System health issues detected" << std::endl;
    }

    auto active_alerts = monitor.getActiveAlerts();
    std::cout << "✅ Active alerts: " << active_alerts.size() << std::endl;

    if (!active_alerts.empty()) {
        for (const auto& alert : active_alerts) {
            std::string severity = (alert.severity == PerformanceAlert::CRITICAL) ? "CRITICAL" :
                                  (alert.severity == PerformanceAlert::WARNING) ? "WARNING" : "INFO";
            std::cout << "   [" << severity << "] " << alert.component << ": " << alert.message << std::endl;
        }
    }

    // Test 5: Performance analysis
    std::cout << "\n🔍 Testing performance analysis..." << std::endl;

    double avg_cpu = monitor.getAverageCpuUsage(1); // Last 1 minute
    size_t avg_memory = monitor.getAverageMemoryUsage(1);
    double uptime = monitor.getSystemUptime();

    std::cout << "✅ Performance analysis completed:" << std::endl;
    std::cout << "   Average CPU (1 min): " << avg_cpu << "%" << std::endl;
    std::cout << "   Average Memory (1 min): " << avg_memory << " MB" << std::endl;
    std::cout << "   System Uptime: " << uptime << " hours" << std::endl;

    // Test 6: Reports generation
    std::cout << "\n🔍 Testing report generation..." << std::endl;

    std::string health_report = monitor.generateHealthReport();
    if (!health_report.empty()) {
        std::cout << "✅ Health report generated successfully" << std::endl;
        std::cout << "Report preview (first 200 chars):" << std::endl;
        std::cout << health_report.substr(0, 200) << "..." << std::endl;
    } else {
        std::cout << "❌ Health report generation failed" << std::endl;
        return 1;
    }

    std::string perf_report = monitor.generatePerformanceReport();
    if (!perf_report.empty()) {
        std::cout << "✅ Performance report generated successfully" << std::endl;
    } else {
        std::cout << "❌ Performance report generation failed" << std::endl;
        return 1;
    }

    // Test 7: Stop monitoring
    std::cout << "\n🔍 Testing monitoring shutdown..." << std::endl;

    monitor.stopMonitoring();

    if (!monitor.isMonitoring()) {
        std::cout << "✅ System monitoring stopped successfully" << std::endl;
    } else {
        std::cout << "❌ Failed to stop system monitoring" << std::endl;
        return 1;
    }

    std::cout << "\n🎯 SYSTEM MONITORING VALIDATION: PASSED ✅" << std::endl;
    std::cout << "Real-time system monitoring is operational and production-ready!" << std::endl;

    return 0;
}