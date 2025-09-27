#pragma once

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <functional>
#include <map>
#include <atomic>
#include <thread>
#include "../Data/LiveDataManager.h"
#include "../Config/TunableParameters.h"
#include "RealCorrelationFramework.h"

namespace CryptoClaude {
namespace Testing {

// Test result structure
struct TestResult {
    std::string testName;
    bool passed;
    std::string errorMessage;
    std::chrono::system_clock::time_point timestamp;
    double executionTimeMs;
    std::map<std::string, double> metrics;
};

// Performance metrics structure
struct PerformanceMetrics {
    double avgResponseTimeMs;
    double maxResponseTimeMs;
    double minResponseTimeMs;
    int totalRequests;
    int failedRequests;
    double successRate;
    double uptimePercentage;
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;
};

// System health status
enum class HealthStatus {
    HEALTHY,
    DEGRADED,
    CRITICAL,
    OFFLINE
};

// Stability test configuration
struct StabilityConfig {
    std::chrono::hours testDuration{72};                    // 72-hour default
    std::chrono::seconds healthCheckInterval{300};          // 5-minute checks
    std::chrono::seconds performanceCheckInterval{60};      // 1-minute performance
    std::chrono::seconds correlationCheckInterval{3600};    // Hourly correlation

    // Performance thresholds
    double maxResponseTimeMs{5000.0};                       // 5-second max response
    double minSuccessRate{0.99};                           // 99% success rate
    double minUptimePercentage{0.999};                     // 99.9% uptime
    double minCorrelationThreshold{0.85};                  // >85% correlation

    // Recovery settings
    int maxRetries{3};
    std::chrono::seconds retryInterval{30};
    bool enableAutoRecovery{true};
};

// Main stability testing framework
class StabilityTestingFramework {
private:
    std::unique_ptr<Data::LiveDataManager> dataManager_;
    std::unique_ptr<Config::TunableParametersManager> paramManager_;
    std::unique_ptr<RealCorrelationFramework> realCorrelationFramework_;

    StabilityConfig config_;
    std::atomic<bool> testRunning_{false};
    std::atomic<bool> stopRequested_{false};

    // Test results and metrics
    std::vector<TestResult> testResults_;
    PerformanceMetrics overallMetrics_;
    std::map<std::string, PerformanceMetrics> providerMetrics_;

    // Background monitoring threads
    std::unique_ptr<std::thread> healthMonitorThread_;
    std::unique_ptr<std::thread> performanceMonitorThread_;
    std::unique_ptr<std::thread> correlationMonitorThread_;

    // Health status tracking
    std::atomic<HealthStatus> currentHealth_{HealthStatus::HEALTHY};
    std::string lastHealthError_;
    std::chrono::system_clock::time_point lastHealthCheck_;

public:
    explicit StabilityTestingFramework(const StabilityConfig& config = StabilityConfig{});
    ~StabilityTestingFramework();

    // Test execution control
    bool startStabilityTest();
    void stopStabilityTest();
    bool isTestRunning() const { return testRunning_.load(); }

    // Configuration management
    void setConfig(const StabilityConfig& config) { config_ = config; }
    const StabilityConfig& getConfig() const { return config_; }

    // Results and metrics access
    std::vector<TestResult> getTestResults() const { return testResults_; }
    PerformanceMetrics getOverallMetrics() const { return overallMetrics_; }
    std::map<std::string, PerformanceMetrics> getProviderMetrics() const { return providerMetrics_; }

    // Health monitoring
    HealthStatus getCurrentHealth() const { return currentHealth_.load(); }
    std::string getHealthReport() const;
    std::string getLastHealthError() const { return lastHealthError_; }

    // Progress tracking
    double getTestProgress() const;
    std::chrono::seconds getRemainingTime() const;
    std::string getProgressReport() const;

    // Individual test methods
    bool testApiConnectivity();
    bool testDataRetrieval();
    bool testParameterManagement();
    bool testCorrelationCalculation();
    bool testErrorRecovery();
    bool testPerformanceUnderLoad();

private:
    // Background monitoring methods
    void healthMonitorLoop();
    void performanceMonitorLoop();
    void correlationMonitorLoop();

    // Utility methods
    void recordTestResult(const TestResult& result);
    void updatePerformanceMetrics(const std::string& provider, double responseTime, bool success);
    void setHealthStatus(HealthStatus status, const std::string& error = "");

    // Recovery methods
    bool attemptRecovery();
    void restartFailedComponents();

public:
    // Correlation calculation methods for testing (made public for validation)
    double calculateCorrelationAccuracy(); // DEPRECATED - Uses simulated values
    double calculateRealCorrelationAccuracy(); // NEW - Real correlation implementation

private:
    // Metrics calculation
    void calculateOverallMetrics();
};

// 72-Hour Stability Test Runner
class StabilityTestRunner {
private:
    std::unique_ptr<StabilityTestingFramework> framework_;
    std::chrono::system_clock::time_point startTime_;
    bool testCompleted_{false};

public:
    StabilityTestRunner();
    ~StabilityTestRunner() = default;

    // Main test execution
    bool executeFullStabilityTest();
    bool executeQuickStabilityTest(std::chrono::minutes duration = std::chrono::minutes(30));

    // Progress monitoring
    void printProgressReport() const;
    void generateStabilityReport(const std::string& outputPath) const;

    // Results access
    bool isTestCompleted() const { return testCompleted_; }
    bool wasTestSuccessful() const;
    std::string getFinalReport() const;
};

// Stability test utilities
class StabilityTestUtils {
public:
    // Test data generation
    static std::vector<std::string> generateTestSymbols(int count = 10);
    static std::map<std::string, double> generateExpectedPrices();

    // Performance benchmarking
    static double benchmarkResponseTime(std::function<void()> operation, int iterations = 100);
    static bool validateResponseTime(double actualMs, double expectedMs, double tolerancePercent = 10.0);

    // Correlation validation
    static double calculateTestCorrelation(const std::vector<double>& expected,
                                         const std::vector<double>& actual);
    static bool validateCorrelationThreshold(double correlation, double threshold = 0.85);

    // Report generation
    static std::string generateHtmlReport(const PerformanceMetrics& metrics,
                                        const std::vector<TestResult>& results);
    static std::string generateJsonReport(const PerformanceMetrics& metrics,
                                        const std::vector<TestResult>& results);
};

} // namespace Testing
} // namespace CryptoClaude