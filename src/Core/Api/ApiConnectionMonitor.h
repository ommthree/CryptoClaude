#pragma once

#include "../Data/Providers/CryptoCompareProvider.h"
#include "../Database/DatabaseManager.h"
#include <memory>
#include <vector>
#include <map>
#include <chrono>
#include <functional>
#include <atomic>
#include <thread>
#include <future>

namespace CryptoClaude {
namespace Api {

using namespace CryptoClaude::Data::Providers;
using namespace CryptoClaude::Database;

// Connection health status
enum class ConnectionHealth {
    EXCELLENT,  // < 100ms latency, 100% success rate
    GOOD,       // < 500ms latency, >95% success rate
    FAIR,       // < 1000ms latency, >90% success rate
    POOR,       // < 2000ms latency, >80% success rate
    CRITICAL    // > 2000ms latency or <80% success rate
};

// Connection monitoring metrics
struct ConnectionMetrics {
    std::string providerId;
    std::chrono::system_clock::time_point lastUpdateTime;

    // Latency metrics
    double averageLatency = 0.0;     // milliseconds
    double maxLatency = 0.0;
    double minLatency = 0.0;
    double latencyStdDev = 0.0;

    // Success rate metrics
    int totalRequests = 0;
    int successfulRequests = 0;
    int failedRequests = 0;
    double successRate = 0.0;        // 0.0 to 1.0

    // Rate limiting metrics
    double rateLimitUtilization = 0.0; // 0.0 to 1.0
    int rateLimitHits = 0;
    std::chrono::system_clock::time_point lastRateLimitHit;

    // Data quality metrics
    double dataCompletenessRatio = 0.0;
    int invalidDataPoints = 0;
    int totalDataPoints = 0;

    // Connection health assessment
    ConnectionHealth overallHealth = ConnectionHealth::EXCELLENT;
    std::string healthReason;

    // Recent error information
    std::vector<std::string> recentErrors;
    std::string lastError;
    std::chrono::system_clock::time_point lastErrorTime;

    ConnectionMetrics() = default;
    ConnectionMetrics(const std::string& id) : providerId(id) {}
};

// Health alert configuration
struct HealthAlert {
    std::string alertId;
    std::string providerId;
    ConnectionHealth triggerLevel;
    std::string message;
    std::chrono::system_clock::time_point triggeredAt;
    bool isActive = true;
    bool requiresAction = false;

    std::function<bool()> automaticRemediation; // Optional auto-fix function
    std::vector<std::string> recommendedActions;
};

// Real-time connection monitoring and validation
class ApiConnectionMonitor {
public:
    ApiConnectionMonitor(DatabaseManager& dbManager);
    ~ApiConnectionMonitor();

    // Initialization and configuration
    bool initialize();
    void shutdown();

    // Provider registration and management
    bool registerProvider(const std::string& providerId,
                         std::shared_ptr<CryptoCompareProvider> provider);
    bool unregisterProvider(const std::string& providerId);
    std::vector<std::string> getRegisteredProviders() const;

    // Real-time monitoring controls
    void startRealTimeMonitoring();
    void stopRealTimeMonitoring();
    bool isMonitoringActive() const { return isMonitoring_; }
    void setMonitoringInterval(std::chrono::seconds interval);

    // Health assessment
    ConnectionMetrics getConnectionMetrics(const std::string& providerId) const;
    std::map<std::string, ConnectionMetrics> getAllConnectionMetrics() const;
    ConnectionHealth assessOverallHealth() const;

    // Connection testing
    struct ConnectionTestResult {
        std::string providerId;
        bool connectionSuccessful = false;
        double latency = 0.0; // milliseconds
        std::string errorMessage;
        std::chrono::system_clock::time_point testedAt;

        // Data validation results
        bool dataValid = false;
        int dataPointsReceived = 0;
        double dataQualityScore = 0.0; // 0.0 to 1.0
        std::vector<std::string> validationErrors;
    };

    ConnectionTestResult testConnection(const std::string& providerId);
    std::map<std::string, ConnectionTestResult> testAllConnections();

    // Automated health checks
    void performHealthCheck();
    std::chrono::system_clock::time_point getLastHealthCheckTime() const { return lastHealthCheck_; }

    // Alert management
    std::vector<HealthAlert> getActiveAlerts() const;
    std::vector<HealthAlert> getAlertsForProvider(const std::string& providerId) const;
    bool acknowledgeAlert(const std::string& alertId);
    void clearAllAlerts();

    // Remediation and recovery
    struct RemediationAction {
        std::string actionId;
        std::string providerId;
        std::string actionType; // "RECONNECT", "RATE_LIMIT_BACKOFF", "CONFIG_REFRESH", etc.
        std::string description;
        std::function<bool(const std::string&)> executeAction;
        bool isAutomated = false;
        double successProbability = 0.0; // 0.0 to 1.0
    };

    std::vector<RemediationAction> suggestRemediationActions(const std::string& providerId);
    bool executeAutomatedRemediation(const std::string& providerId);
    std::map<std::string, bool> executeRemediationAction(const std::string& actionId);

    // Performance optimization
    struct PerformanceRecommendation {
        std::string providerId;
        std::string recommendationType; // "RATE_LIMIT", "BATCH_SIZE", "TIMEOUT", etc.
        std::string description;
        std::map<std::string, std::string> suggestedSettings;
        double expectedImprovement = 0.0; // Expected latency reduction or success rate improvement
    };

    std::vector<PerformanceRecommendation> getPerformanceRecommendations() const;
    bool applyPerformanceOptimizations(const std::string& providerId);

    // Data quality validation
    struct DataQualityAssessment {
        std::string providerId;
        std::string symbol;
        std::chrono::system_clock::time_point assessmentTime;

        // Data completeness
        int expectedDataPoints = 0;
        int receivedDataPoints = 0;
        double completenessRatio = 0.0;

        // Data accuracy
        bool pricesRealistic = true;
        bool timestampsValid = true;
        bool volumeDataPresent = true;

        // Data freshness
        std::chrono::minutes dataAge;
        bool dataFresh = true;

        // Overall quality score
        double overallQualityScore = 0.0; // 0.0 to 1.0
        std::vector<std::string> qualityIssues;
    };

    DataQualityAssessment validateDataQuality(const std::string& providerId,
                                            const std::string& symbol,
                                            const std::vector<MarketData>& data);

    // Monitoring configuration
    struct MonitoringConfig {
        std::chrono::seconds healthCheckInterval = std::chrono::seconds(60);
        std::chrono::seconds connectionTestInterval = std::chrono::seconds(300);

        // Alert thresholds
        double latencyWarningThreshold = 500.0;    // milliseconds
        double latencyCriticalThreshold = 2000.0;  // milliseconds
        double successRateWarningThreshold = 0.95; // 95%
        double successRateCriticalThreshold = 0.80; // 80%

        // Data quality thresholds
        double dataQualityWarningThreshold = 0.90;  // 90%
        double dataQualityCriticalThreshold = 0.70; // 70%

        // Monitoring scope
        std::vector<std::string> symbolsToMonitor = {"BTC", "ETH", "ADA", "DOT", "LINK"};
        bool enableRealTimeDataValidation = true;
        bool enableAutomatedRemediation = false;

        // Logging and storage
        bool logHealthMetrics = true;
        std::chrono::hours metricsRetentionPeriod = std::chrono::hours(24 * 7); // 1 week
    };

    void setMonitoringConfig(const MonitoringConfig& config);
    MonitoringConfig getMonitoringConfig() const { return config_; }

    // Reporting and analytics
    struct HealthReport {
        std::chrono::system_clock::time_point reportTime;
        ConnectionHealth overallHealth;

        int totalProviders = 0;
        int healthyProviders = 0;
        int warningProviders = 0;
        int criticalProviders = 0;

        double averageLatency = 0.0;
        double averageSuccessRate = 0.0;
        double averageDataQuality = 0.0;

        std::vector<std::string> criticalIssues;
        std::vector<std::string> warnings;
        std::vector<std::string> recommendations;

        std::map<std::string, ConnectionMetrics> providerDetails;
    };

    HealthReport generateHealthReport() const;
    std::vector<HealthReport> getHistoricalHealthReports(int hoursBack = 24) const;

    // Event callbacks for external integration
    using HealthChangeCallback = std::function<void(const std::string&, ConnectionHealth, ConnectionHealth)>;
    using AlertCallback = std::function<void(const HealthAlert&)>;
    using MetricsCallback = std::function<void(const ConnectionMetrics&)>;

    void setHealthChangeCallback(HealthChangeCallback callback) { healthChangeCallback_ = callback; }
    void setAlertCallback(AlertCallback callback) { alertCallback_ = callback; }
    void setMetricsCallback(MetricsCallback callback) { metricsCallback_ = callback; }

private:
    DatabaseManager& dbManager_;
    std::map<std::string, std::shared_ptr<CryptoCompareProvider>> providers_;
    mutable std::map<std::string, ConnectionMetrics> metrics_;
    std::vector<HealthAlert> activeAlerts_;
    MonitoringConfig config_;

    // Monitoring thread management
    std::atomic<bool> isMonitoring_{false};
    std::unique_ptr<std::thread> monitoringThread_;
    std::chrono::system_clock::time_point lastHealthCheck_;

    // Callbacks
    HealthChangeCallback healthChangeCallback_;
    AlertCallback alertCallback_;
    MetricsCallback metricsCallback_;

    // Internal monitoring methods
    void monitoringLoop();
    void updateConnectionMetrics(const std::string& providerId);
    void checkHealthThresholds(const std::string& providerId);
    void generateHealthAlert(const std::string& providerId,
                           ConnectionHealth health,
                           const std::string& reason);

    // Metrics calculation helpers
    double calculateAverageLatency(const std::string& providerId) const;
    double calculateSuccessRate(const std::string& providerId) const;
    ConnectionHealth determineHealthLevel(const ConnectionMetrics& metrics) const;

    // Data quality validation helpers
    bool validatePriceData(const MarketData& data) const;
    bool validateTimestamp(const std::chrono::system_clock::time_point& timestamp) const;
    bool validateVolumeData(double volume) const;

    // Remediation helpers
    bool attemptReconnection(const std::string& providerId);
    bool adjustRateLimiting(const std::string& providerId);
    bool refreshConfiguration(const std::string& providerId);

    // Database persistence
    void persistMetrics(const std::string& providerId, const ConnectionMetrics& metrics);
    void persistHealthReport(const HealthReport& report);
    std::vector<ConnectionMetrics> loadHistoricalMetrics(const std::string& providerId, int hoursBack) const;
};

// Utility class for API connection testing and validation
class ApiConnectionTester {
public:
    explicit ApiConnectionTester(std::shared_ptr<CryptoCompareProvider> provider);

    // Connection validation tests
    struct ValidationTestSuite {
        std::string suiteName;
        std::vector<std::function<bool()>> tests;
        int passedTests = 0;
        int totalTests = 0;
        std::vector<std::string> failureReasons;
        double executionTime = 0.0; // seconds
    };

    ValidationTestSuite runConnectivityTests();
    ValidationTestSuite runDataValidationTests();
    ValidationTestSuite runPerformanceTests();
    ValidationTestSuite runRateLimitTests();

    // Comprehensive test execution
    struct ComprehensiveTestResult {
        bool overallSuccess = false;
        int totalTestsPassed = 0;
        int totalTestsFailed = 0;
        double totalExecutionTime = 0.0; // seconds

        std::vector<ValidationTestSuite> testSuites;
        std::vector<std::string> criticalFailures;
        std::vector<std::string> recommendations;
        ConnectionHealth recommendedHealthLevel;
    };

    ComprehensiveTestResult runComprehensiveTests();

    // Load testing for production readiness
    struct LoadTestResult {
        int totalRequests = 0;
        int successfulRequests = 0;
        double averageLatency = 0.0;
        double maxLatency = 0.0;
        double minLatency = 0.0;
        int rateLimitHits = 0;
        std::vector<std::string> errors;
        bool passedLoadTest = false;
    };

    LoadTestResult runLoadTest(int requestCount = 100, int concurrentRequests = 5);

private:
    std::shared_ptr<CryptoCompareProvider> provider_;

    // Individual test implementations
    bool testBasicConnectivity();
    bool testAuthentication();
    bool testDataRetrieval();
    bool testErrorHandling();
    bool testRateLimiting();
    bool testDataQuality();
    bool testLatencyPerformance();
    bool testConcurrentRequests();

    // Test utilities
    double measureLatency(std::function<void()> operation);
    bool validateMarketData(const MarketData& data);
    std::string generateTestSymbol();
};

} // namespace Api
} // namespace CryptoClaude