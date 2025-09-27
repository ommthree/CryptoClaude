#pragma once

#include "../Risk/StressTestingEngine.h"
#include "../Database/DatabaseManager.h"
#include "../Utils/HttpClient.h"
#include "../Config/ApiConfigurationManager.h"
#include "../Trading/TradingEngine.h"
#include "../Monitoring/PaperTradingMonitor.h"
#include "../../Configuration/Config.h"
#include <vector>
#include <map>
#include <string>
#include <functional>
#include <chrono>
#include <memory>
#include <atomic>
#include <thread>
#include <future>
#include <queue>

namespace CryptoClaude {
namespace Testing {

using namespace CryptoClaude::Database;
using namespace CryptoClaude::Utils;
using namespace CryptoClaude::Config;
using namespace CryptoClaude::Trading;
using namespace CryptoClaude::Monitoring;

// Operational stress test types
enum class OperationalStressType {
    // API and connectivity failures
    API_EXCHANGE_FAILURE,           // Primary exchange API down
    API_MULTI_EXCHANGE_FAILURE,     // Multiple exchanges down
    API_RATE_LIMIT_EXCEEDED,        // Rate limiting stress
    API_AUTHENTICATION_FAILURE,     // Auth token expiry/rejection
    API_LATENCY_SPIKE,             // High latency responses
    API_DATA_CORRUPTION,           // Malformed API responses

    // Database stress tests
    DATABASE_CONNECTION_FAILURE,    // DB connection lost
    DATABASE_CORRUPTION,           // Data integrity issues
    DATABASE_DEADLOCK,             // Concurrent access issues
    DATABASE_DISK_FULL,            // Storage exhaustion
    DATABASE_SLOW_QUERIES,         // Performance degradation
    DATABASE_BACKUP_FAILURE,       // Backup/restore issues

    // Network stress tests
    NETWORK_PARTITION,             // Network split scenarios
    NETWORK_CONGESTION,            // High latency/packet loss
    NETWORK_BANDWIDTH_LIMIT,       // Bandwidth throttling
    DNS_RESOLUTION_FAILURE,        // DNS lookup failures
    SSL_CERTIFICATE_EXPIRY,        // TLS handshake failures

    // System resource stress
    CPU_EXHAUSTION,                // High CPU load
    MEMORY_EXHAUSTION,             // Memory pressure/OOM
    DISK_IO_SATURATION,            // Disk I/O bottlenecks
    FILE_DESCRIPTOR_EXHAUSTION,    // File handle limits
    THREAD_POOL_EXHAUSTION,        // Threading resource limits

    // Application-level failures
    TRADING_ENGINE_CRASH,          // Core engine failure
    RISK_CALCULATOR_FAILURE,       // Risk system failure
    MONITORING_SYSTEM_FAILURE,     // Monitoring outage
    CONFIGURATION_CORRUPTION,      // Config file issues
    LOG_SYSTEM_FAILURE,           // Logging system down

    // Coordinated failure scenarios
    CASCADE_FAILURE,               // Multiple simultaneous failures
    DISASTER_RECOVERY,             // Full system recovery test
    FAILOVER_STRESS,              // Failover mechanism stress
    LOAD_BALANCER_FAILURE,        // Load balancing failures
    SERVICE_DISCOVERY_FAILURE      // Service mesh issues
};

// Stress test severity for operational tests
enum class OperationalSeverity {
    MINOR = 1,          // Single component degradation
    MODERATE = 2,       // Multiple component issues
    MAJOR = 3,          // Critical system failures
    CATASTROPHIC = 4,   // Multiple simultaneous failures
    DISASTER = 5        // Complete system failure
};

// Recovery test types
enum class RecoveryTestType {
    AUTOMATIC_RECOVERY,     // System self-recovery
    MANUAL_RECOVERY,        // Human intervention required
    GRACEFUL_DEGRADATION,   // Reduced functionality mode
    FAILOVER_RECOVERY,      // Switch to backup systems
    PARTIAL_RECOVERY,       // Some functionality restored
    FULL_RECOVERY          // Complete restoration
};

// Operational stress test result
struct OperationalStressResult {
    OperationalStressType stressType;
    OperationalSeverity severity;
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;
    std::chrono::milliseconds duration;

    // Failure characteristics
    bool testSuccessful;                    // Test executed properly
    bool systemSurvived;                   // System remained operational
    std::chrono::seconds failureDetectionTime; // Time to detect failure
    std::chrono::seconds recoveryTime;      // Time to recover

    // Performance during stress
    double performanceDegradation;          // 0-1 scale performance loss
    double uptimePercentage;               // % uptime during test
    int transactionFailures;               // Failed operations
    int successfulTransactions;            // Successful operations

    // Recovery characteristics
    RecoveryTestType recoveryType;
    bool automaticRecoverySucceeded;
    bool manualInterventionRequired;
    std::vector<std::string> recoveryActions;
    double recoveryCompleteness;           // 0-1 scale

    // Resource utilization during stress
    struct ResourceMetrics {
        double maxCpuUsage;                // Peak CPU usage %
        double maxMemoryUsage;             // Peak memory usage %
        double maxDiskUsage;               // Peak disk usage %
        int maxConnectionCount;            // Peak connection count
        double maxLatency;                 // Peak response latency ms
        int maxQueueDepth;                 // Peak queue depth
    } resourceMetrics;

    // Error analysis
    std::map<std::string, int> errorCounts;      // Error types and counts
    std::vector<std::string> criticalErrors;    // Critical error messages
    std::vector<std::string> recoveryWarnings;  // Recovery warnings

    // System stability metrics
    bool dataIntegrityMaintained;          // No data corruption
    bool consistencyMaintained;           // System state consistent
    bool securityCompromised;             // Security violations
    std::vector<std::string> stabilityIssues;

    // Performance requirements validation (TRS compliance)
    bool meetsApiFailoverTarget;          // <30s API failover
    bool meetsDatabaseRecoveryTarget;     // <60s DB recovery
    bool meetsExtremeEventTarget;         // <2min extreme event recovery
    bool meetsUptimeTarget;              // >99.9% uptime maintained

    // Recommendations
    std::vector<std::string> improvementRecommendations;
    std::vector<std::string> preventiveActions;
    std::vector<std::string> monitoringEnhancements;

    // Test metadata
    std::string testDescription;
    std::map<std::string, std::string> testParameters;
    bool isValid;
    std::vector<std::string> warnings;
};

// API failover configuration
struct ApiFailoverConfig {
    std::vector<std::string> primaryExchanges;
    std::vector<std::string> backupExchanges;
    std::chrono::seconds failoverTimeout{30};      // Max failover time
    std::chrono::seconds healthCheckInterval{5};   // Health check frequency
    int maxRetryAttempts = 3;
    double failoverThreshold = 0.8;                // Trigger threshold
    bool enableAutomaticFailback = true;
    std::chrono::seconds failbackDelay{300};       // 5min failback delay

    // Rate limiting configuration
    int maxRequestsPerSecond = 10;
    int burstLimit = 50;
    std::chrono::seconds rateLimitWindow{60};

    // Circuit breaker configuration
    int circuitBreakerThreshold = 5;               // Failures before opening
    std::chrono::seconds circuitBreakerTimeout{60}; // Recovery timeout
    double circuitBreakerRecoveryRate = 0.5;       // % success to close
};

// Database resilience configuration
struct DatabaseResilienceConfig {
    std::chrono::seconds connectionTimeout{10};
    std::chrono::seconds queryTimeout{30};
    int maxRetryAttempts = 3;
    std::chrono::milliseconds retryDelay{100};

    // Connection pooling
    int maxConnections = 20;
    int minConnections = 5;
    std::chrono::seconds connectionMaxAge{3600};    // 1 hour

    // Backup and recovery
    std::chrono::seconds backupInterval{1800};     // 30 minutes
    bool enableWALMode = true;                      // Write-Ahead Logging
    bool enableAutomaticVacuum = true;

    // Corruption detection
    bool enableChecksumValidation = true;
    std::chrono::seconds integrityCheckInterval{3600}; // 1 hour
    bool enableAutomaticRepair = false;             // Manual repair only
};

// Network resilience configuration
struct NetworkResilienceConfig {
    std::chrono::seconds connectionTimeout{10};
    std::chrono::seconds readTimeout{30};
    int maxRetryAttempts = 3;
    std::chrono::milliseconds baseRetryDelay{500};
    double retryBackoffMultiplier = 2.0;

    // Keep-alive configuration
    bool enableKeepAlive = true;
    std::chrono::seconds keepAliveInterval{60};
    int maxKeepAliveProbes = 3;

    // DNS configuration
    std::vector<std::string> dnsServers = {"8.8.8.8", "1.1.1.1"};
    std::chrono::seconds dnsTimeout{5};
    int dnsCacheSize = 100;
    std::chrono::seconds dnsCacheTTL{300};

    // Bandwidth management
    int maxBandwidthMbps = 100;
    int emergencyBandwidthMbps = 10;
    bool enableTrafficShaping = true;
};

// System resource monitoring configuration
struct ResourceMonitoringConfig {
    // CPU monitoring
    double cpuWarningThreshold = 0.8;              // 80% CPU usage warning
    double cpuCriticalThreshold = 0.95;            // 95% CPU usage critical
    std::chrono::seconds cpuCheckInterval{10};

    // Memory monitoring
    double memoryWarningThreshold = 0.8;           // 80% memory warning
    double memoryCriticalThreshold = 0.95;         // 95% memory critical
    std::chrono::seconds memoryCheckInterval{10};

    // Disk monitoring
    double diskWarningThreshold = 0.8;             // 80% disk warning
    double diskCriticalThreshold = 0.95;           // 95% disk critical
    std::chrono::seconds diskCheckInterval{30};

    // Network monitoring
    double networkLatencyWarning = 100.0;          // 100ms warning
    double networkLatencyCritical = 500.0;         // 500ms critical
    double packetLossWarning = 0.01;               // 1% packet loss
    double packetLossCritical = 0.05;              // 5% packet loss

    // Process monitoring
    int maxFileDescriptors = 1024;
    int maxThreadCount = 200;
    std::chrono::seconds processCheckInterval{30};
};

// Main operational stress testing engine
class OperationalStressTest {
public:
    OperationalStressTest();
    ~OperationalStressTest();

    // Initialization and configuration
    bool initialize(std::shared_ptr<DatabaseManager> dbManager,
                   std::shared_ptr<ApiConfigurationManager> apiManager,
                   std::shared_ptr<TradingEngine> tradingEngine,
                   std::shared_ptr<PaperTradingMonitor> monitor);

    void setApiFailoverConfig(const ApiFailoverConfig& config);
    void setDatabaseResilienceConfig(const DatabaseResilienceConfig& config);
    void setNetworkResilienceConfig(const NetworkResilienceConfig& config);
    void setResourceMonitoringConfig(const ResourceMonitoringConfig& config);

    // === MAIN STRESS TESTING METHODS ===

    // Run single operational stress test
    OperationalStressResult runOperationalStressTest(OperationalStressType stressType,
                                                     OperationalSeverity severity = OperationalSeverity::MAJOR);

    // Run comprehensive operational test suite
    std::map<OperationalStressType, OperationalStressResult> runComprehensiveOperationalTests();

    // Run coordinated failure scenarios
    OperationalStressResult runCascadeFailureTest(const std::vector<OperationalStressType>& failureSequence);
    OperationalStressResult runDisasterRecoveryTest();

    // === API FAILURE AND RECOVERY TESTING ===

    // Single exchange failure
    OperationalStressResult testExchangeApiFailure(const std::string& exchange,
                                                  std::chrono::seconds failureDuration = std::chrono::seconds(300));

    // Multi-exchange failover
    OperationalStressResult testMultiExchangeFailover(const std::vector<std::string>& failingExchanges,
                                                     std::chrono::seconds failureDuration = std::chrono::seconds(600));

    // Rate limiting stress
    OperationalStressResult testApiRateLimitStress(int requestMultiplier = 10);

    // Authentication failure simulation
    OperationalStressResult testApiAuthenticationFailure(std::chrono::seconds outageTime = std::chrono::seconds(120));

    // High latency simulation
    OperationalStressResult testApiLatencyStress(std::chrono::milliseconds additionalLatency = std::chrono::milliseconds(2000));

    // API response corruption
    OperationalStressResult testApiDataCorruption(double corruptionRate = 0.1);

    // === DATABASE RESILIENCE TESTING ===

    // Connection failure simulation
    OperationalStressResult testDatabaseConnectionFailure(std::chrono::seconds outageTime = std::chrono::seconds(60));

    // Data corruption testing
    OperationalStressResult testDatabaseCorruption();

    // Deadlock stress testing
    OperationalStressResult testDatabaseDeadlocks(int concurrentConnections = 20);

    // Disk space exhaustion
    OperationalStressResult testDatabaseDiskExhaustion();

    // Query performance degradation
    OperationalStressResult testDatabaseSlowQueries(double slowdownFactor = 10.0);

    // Backup and recovery testing
    OperationalStressResult testDatabaseBackupRecovery();

    // === NETWORK STRESS TESTING ===

    // Network partition simulation
    OperationalStressResult testNetworkPartition(std::chrono::seconds partitionDuration = std::chrono::seconds(300));

    // Network congestion simulation
    OperationalStressResult testNetworkCongestion(double packetLossRate = 0.05,
                                                 std::chrono::milliseconds additionalLatency = std::chrono::milliseconds(500));

    // Bandwidth limitation
    OperationalStressResult testBandwidthLimitation(int maxBandwidthKbps = 1000);

    // DNS resolution failure
    OperationalStressResult testDnsResolutionFailure(std::chrono::seconds outageTime = std::chrono::seconds(180));

    // SSL certificate issues
    OperationalStressResult testSslCertificateFailure();

    // === SYSTEM RESOURCE STRESS TESTING ===

    // CPU exhaustion testing
    OperationalStressResult testCpuExhaustion(double targetCpuUsage = 0.95,
                                             std::chrono::seconds duration = std::chrono::seconds(300));

    // Memory pressure testing
    OperationalStressResult testMemoryExhaustion(double targetMemoryUsage = 0.90,
                                                std::chrono::seconds duration = std::chrono::seconds(300));

    // Disk I/O saturation
    OperationalStressResult testDiskIoSaturation(int iopsTarget = 1000,
                                               std::chrono::seconds duration = std::chrono::seconds(300));

    // File descriptor exhaustion
    OperationalStressResult testFileDescriptorExhaustion();

    // Thread pool exhaustion
    OperationalStressResult testThreadPoolExhaustion(int threadTarget = 500);

    // === APPLICATION-LEVEL FAILURE TESTING ===

    // Trading engine crash simulation
    OperationalStressResult testTradingEngineCrash(bool gracefulShutdown = false);

    // Risk calculator failure
    OperationalStressResult testRiskCalculatorFailure(std::chrono::seconds outageTime = std::chrono::seconds(120));

    // Monitoring system failure
    OperationalStressResult testMonitoringSystemFailure();

    // Configuration corruption
    OperationalStressResult testConfigurationCorruption();

    // === RECOVERY AND RESILIENCE VALIDATION ===

    // Automatic recovery testing
    struct AutoRecoveryTest {
        OperationalStressType failureType;
        bool recoverySuccessful;
        std::chrono::seconds recoveryTime;
        std::vector<std::string> recoverySteps;
        bool requiresManualIntervention;
        double systemHealthAfterRecovery; // 0-1 scale
    };

    AutoRecoveryTest testAutomaticRecovery(OperationalStressType failureType);

    // Graceful degradation testing
    struct GracefulDegradationTest {
        OperationalStressType stressType;
        double remainingFunctionality;      // 0-1 scale
        std::vector<std::string> disabledFeatures;
        std::vector<std::string> maintainedFeatures;
        bool criticalFunctionsPreserved;
        std::chrono::seconds degradationResponseTime;
    };

    GracefulDegradationTest testGracefulDegradation(OperationalStressType stressType);

    // Failover mechanism testing
    struct FailoverTest {
        std::string failingComponent;
        std::string backupComponent;
        std::chrono::seconds failoverTime;
        bool failoverSuccessful;
        bool dataConsistencyMaintained;
        double performanceImpact;          // 0-1 scale
        std::vector<std::string> failoverActions;
    };

    FailoverTest testFailoverMechanism(const std::string& component);

    // === CONTINUOUS MONITORING AND ALERTING ===

    struct OperationalHealthMetrics {
        // System health indicators
        double overallSystemHealth;        // 0-1 composite score
        double apiHealthScore;             // API system health
        double databaseHealthScore;        // Database health
        double networkHealthScore;         // Network health
        double resourceHealthScore;        // Resource utilization health

        // Performance metrics
        std::chrono::milliseconds avgResponseTime;
        double successRate;                // 0-1 success rate
        int activeConnections;
        double throughputTps;              // Transactions per second

        // Resource utilization
        double currentCpuUsage;
        double currentMemoryUsage;
        double currentDiskUsage;
        double currentNetworkLatency;

        // Error rates
        double apiErrorRate;
        double databaseErrorRate;
        double networkErrorRate;

        std::chrono::system_clock::time_point lastUpdate;
        std::vector<std::string> currentAlerts;
        std::vector<std::string> healthWarnings;
    };

    OperationalHealthMetrics getOperationalHealthMetrics();

    // Real-time monitoring
    void startContinuousMonitoring();
    void stopContinuousMonitoring();
    bool isMonitoringActive() const { return monitoringActive_.load(); }

    // Alert system
    struct OperationalAlert {
        enum AlertLevel { INFO, WARNING, CRITICAL, EMERGENCY };

        AlertLevel level;
        OperationalStressType component;
        std::string message;
        std::chrono::system_clock::time_point timestamp;
        bool acknowledged;
        std::vector<std::string> recommendedActions;

        // Alert metrics
        double severity;                   // 0-1 severity score
        bool requiresImmediateAction;
        std::chrono::seconds estimatedImpactDuration;
    };

    std::vector<OperationalAlert> getActiveAlerts();
    void acknowledgeAlert(const std::string& alertId);

    // === PERFORMANCE BENCHMARKING ===

    struct OperationalPerformanceMetrics {
        // Test execution performance
        std::map<OperationalStressType, std::chrono::milliseconds> testExecutionTimes;
        std::chrono::milliseconds averageTestTime;
        std::chrono::milliseconds maxTestTime;

        // Recovery performance
        std::map<OperationalStressType, std::chrono::seconds> recoveryTimes;
        std::chrono::seconds averageRecoveryTime;
        std::chrono::seconds maxRecoveryTime;

        // System performance under stress
        double averagePerformanceDegradation;
        double maxPerformanceDegradation;
        double averageUptimePercentage;
        double minUptimePercentage;

        // Compliance metrics
        bool meetsApiFailoverRequirements;    // <30s
        bool meetsDatabaseRecoveryRequirements; // <60s
        bool meetsExtremeEventRequirements;   // <2min
        bool meetsUptimeRequirements;         // >99.9%

        std::chrono::system_clock::time_point lastBenchmark;
    };

    OperationalPerformanceMetrics benchmarkOperationalPerformance(int iterations = 10);

    // === DASHBOARD AND REPORTING ===

    struct OperationalDashboardData {
        // Current system status
        OperationalHealthMetrics currentHealth;
        std::vector<OperationalAlert> activeAlerts;

        // Recent test results
        std::map<OperationalStressType, OperationalStressResult> recentTests;
        std::chrono::system_clock::time_point lastTestSuite;

        // Performance trends
        std::vector<std::pair<std::chrono::system_clock::time_point, double>> healthHistory;
        std::vector<std::pair<std::chrono::system_clock::time_point, double>> uptimeHistory;

        // Failure analysis
        std::map<OperationalStressType, int> failureFrequency;
        std::map<std::string, int> commonErrorPatterns;

        // Recovery effectiveness
        std::map<RecoveryTestType, double> recoverySuccessRates;
        std::vector<std::string> recoveryImprovements;

        // Compliance status
        OperationalPerformanceMetrics performanceMetrics;
        bool systemMeetsResilienceRequirements;
        std::vector<std::string> complianceGaps;

        std::chrono::system_clock::time_point lastUpdate;
        std::vector<std::string> systemRecommendations;
    };

    OperationalDashboardData getDashboardData();
    bool updateDashboard();

    // Reporting
    std::string generateOperationalStressReport(const std::map<OperationalStressType, OperationalStressResult>& results);
    void exportStressResults(const std::string& filename,
                            const std::map<OperationalStressType, OperationalStressResult>& results);

    // === STATE MANAGEMENT ===

    bool isInitialized() const { return isInitialized_; }
    std::chrono::system_clock::time_point getLastStressTest() const { return lastStressTestTime_; }

private:
    // Configuration
    ApiFailoverConfig apiConfig_;
    DatabaseResilienceConfig dbConfig_;
    NetworkResilienceConfig networkConfig_;
    ResourceMonitoringConfig resourceConfig_;

    // Components
    std::shared_ptr<DatabaseManager> dbManager_;
    std::shared_ptr<ApiConfigurationManager> apiManager_;
    std::shared_ptr<TradingEngine> tradingEngine_;
    std::shared_ptr<PaperTradingMonitor> monitor_;

    // Monitoring and state
    std::atomic<bool> monitoringActive_;
    std::unique_ptr<std::thread> monitoringThread_;
    OperationalDashboardData dashboardData_;
    std::vector<OperationalAlert> activeAlerts_;

    // Test state
    std::chrono::system_clock::time_point lastStressTestTime_;
    std::chrono::system_clock::time_point lastDashboardUpdate_;
    bool isInitialized_;

    // === INTERNAL IMPLEMENTATION METHODS ===

    // Failure simulation
    void simulateApiFailure(const std::string& exchange, std::chrono::seconds duration);
    void simulateDatabaseFailure(std::chrono::seconds duration);
    void simulateNetworkFailure(std::chrono::seconds duration);
    void simulateResourceExhaustion(const std::string& resource, double targetUsage, std::chrono::seconds duration);

    // Recovery testing
    bool triggerAutomaticRecovery(OperationalStressType failureType);
    std::vector<std::string> executeManualRecovery(OperationalStressType failureType);

    // System monitoring
    void monitoringLoop();
    OperationalHealthMetrics calculateHealthMetrics();
    std::vector<OperationalAlert> detectSystemIssues();

    // Performance measurement
    double measurePerformanceDegradation();
    double measureUptimePercentage(std::chrono::system_clock::time_point start,
                                  std::chrono::system_clock::time_point end);

    // Utility methods
    OperationalStressResult createErrorResult(OperationalStressType stressType, const std::string& error);
    void logOperationalMetrics(OperationalStressType stressType, std::chrono::milliseconds duration);
    void handleOperationalError(const std::string& test, const std::exception& e);

    // Validation
    void validateTestInputs(OperationalStressType stressType);
    bool validateRecoveryRequirements(const OperationalStressResult& result);

    // Initialization
    void initializeDefaultConfigurations();

    // Component-specific testing
    bool testDatabaseConnection();
    bool testApiConnectivity(const std::string& exchange);
    bool testNetworkReachability(const std::string& host);
    double measureSystemResourceUsage(const std::string& resource);

    // Alert management
    void createAlert(OperationalAlert::AlertLevel level,
                    OperationalStressType component,
                    const std::string& message);
    void updateAlertStatus();
    void cleanupResolvedAlerts();
};

// Utility functions for operational stress testing
namespace OperationalStressUtils {
    // Stress type utilities
    std::string getStressTypeName(OperationalStressType stressType);
    std::string getSeverityDescription(OperationalSeverity severity);

    // Recovery time calculations
    std::chrono::seconds getExpectedRecoveryTime(OperationalStressType stressType);
    double getRecoveryDifficulty(OperationalStressType stressType);

    // Performance impact estimation
    double estimatePerformanceImpact(OperationalStressType stressType, OperationalSeverity severity);
    double estimateUptimeImpact(OperationalStressType stressType, std::chrono::seconds duration);

    // Resource utilization utilities
    double getCpuUsage();
    double getMemoryUsage();
    double getDiskUsage();
    std::chrono::milliseconds getNetworkLatency(const std::string& host);

    // System health scoring
    double calculateHealthScore(const std::map<std::string, double>& metrics);
    bool isSystemHealthy(double healthScore, double threshold = 0.8);

    // Recovery validation
    bool validateApiRecovery(const std::string& exchange);
    bool validateDatabaseRecovery();
    bool validateNetworkRecovery();

    // Load testing utilities
    struct LoadTestResult {
        int totalRequests;
        int successfulRequests;
        int failedRequests;
        std::chrono::milliseconds averageResponseTime;
        std::chrono::milliseconds maxResponseTime;
        double requestsPerSecond;
        std::vector<std::string> errors;
    };

    LoadTestResult executeLoadTest(const std::string& endpoint, int requestCount, int concurrency);
}

} // namespace Testing
} // namespace CryptoClaude