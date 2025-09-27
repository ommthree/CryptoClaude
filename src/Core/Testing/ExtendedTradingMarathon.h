#pragma once

#include "../Trading/PaperTradingEngine.h"
#include "../Monitoring/PaperTradingMonitor.h"
#include "../Risk/StressTestingEngine.h"
#include "OperationalStressTest.h"
#include "../Database/DatabaseManager.h"
#include "../Strategy/EnhancedSignalProcessor.h"
#include "../Portfolio/PortfolioOptimizer.h"
#include "../../Configuration/Config.h"
#include <vector>
#include <map>
#include <string>
#include <chrono>
#include <memory>
#include <atomic>
#include <thread>
#include <future>
#include <queue>
#include <functional>

namespace CryptoClaude {
namespace Testing {

using namespace CryptoClaude::Trading;
using namespace CryptoClaude::Monitoring;
using namespace CryptoClaude::Risk;
using namespace CryptoClaude::Database;
using namespace CryptoClaude::Strategy;
using namespace CryptoClaude::Optimization;

// Marathon duration options
enum class MarathonDuration {
    HOURS_48,       // 48 hour marathon
    HOURS_72,       // 72 hour marathon
    HOURS_96,       // 96 hour marathon (4 days)
    CUSTOM          // Custom duration
};

// Marathon testing phases
enum class MarathonPhase {
    INITIALIZATION, // Setup and validation
    WARMUP,        // Initial trading warmup (first 2 hours)
    NORMAL_OPS,    // Normal operation (bulk of marathon)
    STRESS_TEST,   // Stress testing during operation
    COOLDOWN,      // Final wind-down phase
    ANALYSIS,      // Post-marathon analysis
    COMPLETED      // Marathon completed
};

// System health levels during marathon
enum class MarathonHealthLevel {
    EXCELLENT,     // >95% health score
    GOOD,         // 80-95% health score
    FAIR,         // 65-80% health score
    POOR,         // 50-65% health score
    CRITICAL      // <50% health score
};

// Marathon configuration
struct MarathonConfig {
    MarathonDuration duration;
    std::chrono::hours customDurationHours{48};

    // Trading configuration
    double initialCapital = 1000000.0;      // $1M starting capital
    std::vector<std::string> tradingPairs = {
        "BTC/USD", "ETH/USD", "ADA/USD", "DOT/USD", "LINK/USD",
        "UNI/USD", "AAVE/USD", "SUSHI/USD", "MATIC/USD", "SOL/USD"
    };

    // Strategy configuration
    bool enableMultipleStrategies = true;
    std::vector<std::string> activeStrategies = {
        "MeanReversion", "MomentumBreakout", "StatisticalArbitrage", "PairTrading"
    };

    // Risk management
    double maxDailyDrawdown = 0.05;         // 5% max daily drawdown
    double maxTotalDrawdown = 0.15;         // 15% max total drawdown
    double positionSizeLimit = 0.10;        // 10% max position size
    double portfolioVaRLimit = 0.025;       // 2.5% daily VaR limit

    // Stress testing integration
    bool enableStressTestingDuringMarathon = true;
    std::chrono::hours stressTestInterval{12}; // Every 12 hours
    std::vector<StressScenarioType> stressScenarios = {
        StressScenarioType::FLASH_CRASH,
        StressScenarioType::VOLATILITY_SPIKE,
        StressScenarioType::LIQUIDITY_CRISIS
    };

    // Operational stress testing
    bool enableOperationalStressTests = true;
    std::chrono::hours operationalTestInterval{8}; // Every 8 hours
    std::vector<OperationalStressType> operationalTests = {
        OperationalStressType::API_EXCHANGE_FAILURE,
        OperationalStressType::DATABASE_CONNECTION_FAILURE,
        OperationalStressType::NETWORK_CONGESTION
    };

    // Performance targets (TRS requirements)
    double targetUptimePercentage = 0.999;  // >99.9% uptime
    std::chrono::milliseconds maxLatencyMs{500}; // <500ms response time
    double targetPerformanceDegradation = 0.20; // <20% degradation under stress
    std::chrono::seconds maxApiFailoverTime{30}; // <30s API failover
    std::chrono::seconds maxDatabaseRecoveryTime{60}; // <60s DB recovery
    std::chrono::minutes maxExtremeEventRecovery{2}; // <2min extreme events

    // Monitoring and alerting
    std::chrono::minutes metricsCollectionInterval{1}; // Every minute
    std::chrono::minutes healthCheckInterval{5};       // Every 5 minutes
    std::chrono::minutes reportGenerationInterval{60}; // Every hour
    bool enableRealTimeAlerts = true;
    bool enableAutomaticRecovery = true;

    // Data collection and analysis
    bool enableComprehensiveLogging = true;
    bool enablePerformanceMetrics = true;
    bool enableDetailedTradeAnalysis = true;
    std::string marathonName = "ExtendedMarathon_Default";
    std::string dataOutputPath = "/tmp/marathon_data";
};

// Real-time marathon metrics
struct MarathonMetrics {
    std::chrono::system_clock::time_point timestamp;
    MarathonPhase currentPhase;
    std::chrono::milliseconds elapsedTime;
    std::chrono::milliseconds remainingTime;

    // System health metrics
    MarathonHealthLevel healthLevel;
    double overallHealthScore;        // 0-1 scale
    double systemUptimePercentage;    // Actual uptime achieved
    std::chrono::milliseconds avgResponseTime;
    double performanceDegradationLevel;

    // Trading performance metrics
    double currentPortfolioValue;
    double totalReturn;               // Overall return since start
    double dailyReturn;              // Current day return
    double maxDrawdown;              // Maximum drawdown experienced
    double currentDrawdown;          // Current drawdown level
    int totalTrades;                 // Total trades executed
    double winRate;                  // Win rate percentage
    double sharpeRatio;              // Risk-adjusted return

    // Risk metrics
    double currentVaR95;             // Current 95% VaR
    double stressTestVaR;           // VaR under stress conditions
    int activePositions;            // Number of active positions
    double portfolioConcentration;  // Portfolio concentration risk
    double correlationRisk;         // Correlation risk level

    // Operational metrics
    int successfulApiCalls;         // Successful API calls
    int failedApiCalls;            // Failed API calls
    int databaseQueries;           // Database queries executed
    int databaseErrors;            // Database errors encountered
    std::chrono::milliseconds avgApiLatency; // Average API response time
    std::chrono::milliseconds avgDbLatency;  // Average DB response time

    // Stress test results
    int stressTestsCompleted;       // Number of stress tests completed
    int stressTestsPassed;         // Number of stress tests passed
    double avgStressRecoveryTime;  // Average stress recovery time (seconds)
    int operationalTestsCompleted; // Number of operational tests completed
    int operationalTestsPassed;    // Number of operational tests passed

    // System resource utilization
    double cpuUsagePercentage;     // Current CPU usage
    double memoryUsagePercentage;  // Current memory usage
    double diskUsagePercentage;    // Current disk usage
    int activeConnections;         // Active network connections
    int threadCount;               // Active thread count

    // Error and alert statistics
    int criticalErrors;            // Critical errors encountered
    int warnings;                  // Warnings generated
    int alertsGenerated;          // Total alerts generated
    int alertsResolved;           // Total alerts resolved
    int manualInterventions;      // Manual interventions required

    MarathonMetrics() {
        timestamp = std::chrono::system_clock::now();
        currentPhase = MarathonPhase::INITIALIZATION;
        elapsedTime = std::chrono::milliseconds(0);
        remainingTime = std::chrono::milliseconds(0);
        healthLevel = MarathonHealthLevel::GOOD;
        overallHealthScore = 0.85;
        systemUptimePercentage = 1.0;
        avgResponseTime = std::chrono::milliseconds(100);
        performanceDegradationLevel = 0.0;
        currentPortfolioValue = 0.0;
        totalReturn = 0.0;
        dailyReturn = 0.0;
        maxDrawdown = 0.0;
        currentDrawdown = 0.0;
        totalTrades = 0;
        winRate = 0.0;
        sharpeRatio = 0.0;
        currentVaR95 = 0.0;
        stressTestVaR = 0.0;
        activePositions = 0;
        portfolioConcentration = 0.0;
        correlationRisk = 0.0;
        successfulApiCalls = 0;
        failedApiCalls = 0;
        databaseQueries = 0;
        databaseErrors = 0;
        avgApiLatency = std::chrono::milliseconds(0);
        avgDbLatency = std::chrono::milliseconds(0);
        stressTestsCompleted = 0;
        stressTestsPassed = 0;
        avgStressRecoveryTime = 0.0;
        operationalTestsCompleted = 0;
        operationalTestsPassed = 0;
        cpuUsagePercentage = 0.0;
        memoryUsagePercentage = 0.0;
        diskUsagePercentage = 0.0;
        activeConnections = 0;
        threadCount = 0;
        criticalErrors = 0;
        warnings = 0;
        alertsGenerated = 0;
        alertsResolved = 0;
        manualInterventions = 0;
    }
};

// Marathon event types
enum class MarathonEventType {
    MARATHON_STARTED,
    PHASE_CHANGED,
    STRESS_TEST_EXECUTED,
    OPERATIONAL_TEST_EXECUTED,
    HEALTH_DEGRADATION,
    PERFORMANCE_TARGET_MISSED,
    CRITICAL_ERROR,
    RECOVERY_SUCCESSFUL,
    MANUAL_INTERVENTION,
    MARATHON_COMPLETED,
    EMERGENCY_STOP
};

// Marathon event record
struct MarathonEvent {
    std::chrono::system_clock::time_point timestamp;
    MarathonEventType eventType;
    std::string description;
    std::string severity;          // "INFO", "WARNING", "ERROR", "CRITICAL"
    std::map<std::string, std::string> eventData;
    bool requiresAction;
    std::vector<std::string> recommendedActions;

    MarathonEvent(MarathonEventType type, const std::string& desc, const std::string& sev = "INFO")
        : timestamp(std::chrono::system_clock::now())
        , eventType(type)
        , description(desc)
        , severity(sev)
        , requiresAction(false) {}
};

// Marathon alert types
struct MarathonAlert {
    enum AlertLevel { INFO, WARNING, CRITICAL, EMERGENCY };
    enum AlertCategory {
        SYSTEM_HEALTH,
        TRADING_PERFORMANCE,
        RISK_MANAGEMENT,
        OPERATIONAL_FAILURE,
        PERFORMANCE_DEGRADATION,
        TARGET_MISS
    };

    AlertLevel level;
    AlertCategory category;
    std::chrono::system_clock::time_point timestamp;
    std::string message;
    std::string component;
    double severity;               // 0-1 scale
    bool acknowledged;
    bool resolved;
    std::vector<std::string> actionsTaken;
    std::chrono::system_clock::time_point resolvedTime;

    MarathonAlert(AlertLevel lvl, AlertCategory cat, const std::string& msg, const std::string& comp)
        : level(lvl), category(cat), timestamp(std::chrono::system_clock::now())
        , message(msg), component(comp), severity(0.5), acknowledged(false), resolved(false) {}
};

// Main Extended Paper Trading Marathon system
class ExtendedTradingMarathon {
public:
    ExtendedTradingMarathon();
    ~ExtendedTradingMarathon();

    // Initialization and configuration
    bool initialize(std::shared_ptr<PaperTradingEngine> tradingEngine,
                   std::shared_ptr<PaperTradingMonitor> tradingMonitor,
                   std::shared_ptr<StressTestingEngine> stressTestEngine,
                   std::shared_ptr<OperationalStressTest> operationalStressTest,
                   std::shared_ptr<DatabaseManager> dbManager);

    void setMarathonConfig(const MarathonConfig& config);
    MarathonConfig getMarathonConfig() const { return config_; }

    // === MARATHON CONTROL ===

    // Start the extended paper trading marathon
    bool startMarathon(const std::string& marathonName);

    // Stop the marathon (graceful shutdown)
    void stopMarathon();

    // Emergency stop (immediate halt)
    void emergencyStop();

    // Pause/resume marathon
    void pauseMarathon();
    void resumeMarathon();

    // Marathon status
    bool isMarathonActive() const { return marathonActive_.load(); }
    bool isMarathonPaused() const { return marathonPaused_.load(); }
    MarathonPhase getCurrentPhase() const { return currentPhase_; }
    std::string getMarathonName() const { return marathonName_; }

    // === REAL-TIME MONITORING ===

    // Get current marathon metrics
    MarathonMetrics getCurrentMetrics();

    // Get marathon dashboard data
    struct MarathonDashboard {
        MarathonMetrics currentMetrics;
        std::vector<MarathonEvent> recentEvents;
        std::vector<MarathonAlert> activeAlerts;
        std::vector<VirtualPosition> activePositions;
        std::vector<double> equityCurve24h;
        std::vector<double> healthScoreHistory24h;
        std::map<std::string, double> performanceByStrategy;
        std::map<StressScenarioType, StressTestResult> recentStressTests;
        std::map<OperationalStressType, OperationalStressResult> recentOperationalTests;
        std::chrono::system_clock::time_point lastUpdate;
    };

    MarathonDashboard getDashboardData();

    // Event and alert management
    std::vector<MarathonEvent> getMarathonEvents(int hours = 24);
    std::vector<MarathonAlert> getActiveAlerts();
    void acknowledgeAlert(const std::string& alertId);

    // === PERFORMANCE ANALYSIS ===

    // Marathon performance summary
    struct MarathonPerformanceSummary {
        std::chrono::system_clock::time_point startTime;
        std::chrono::system_clock::time_point endTime;
        std::chrono::milliseconds totalDuration;

        // Trading performance
        double finalPortfolioValue;
        double totalReturn;
        double annualizedReturn;
        double maxDrawdown;
        double volatility;
        double sharpeRatio;
        double winRate;
        int totalTrades;

        // System performance
        double avgUptimePercentage;
        std::chrono::milliseconds avgResponseTime;
        double avgHealthScore;
        int totalStressTests;
        int passedStressTests;
        int totalOperationalTests;
        int passedOperationalTests;

        // TRS compliance
        bool metUptimeTarget;          // >99.9%
        bool metLatencyTarget;         // <500ms
        bool metApiFailoverTarget;     // <30s
        bool metDbRecoveryTarget;      // <60s
        bool metExtremeEventTarget;    // <2min
        bool metPerformanceDegTarget;  // <20%

        // Issues and interventions
        int criticalErrors;
        int manualInterventions;
        std::vector<std::string> majorIssues;
        std::vector<std::string> recommendations;

        MarathonPerformanceSummary() {
            finalPortfolioValue = 0.0;
            totalReturn = 0.0;
            annualizedReturn = 0.0;
            maxDrawdown = 0.0;
            volatility = 0.0;
            sharpeRatio = 0.0;
            winRate = 0.0;
            totalTrades = 0;
            avgUptimePercentage = 0.0;
            avgHealthScore = 0.0;
            totalStressTests = 0;
            passedStressTests = 0;
            totalOperationalTests = 0;
            passedOperationalTests = 0;
            metUptimeTarget = false;
            metLatencyTarget = false;
            metApiFailoverTarget = false;
            metDbRecoveryTarget = false;
            metExtremeEventTarget = false;
            metPerformanceDegTarget = false;
            criticalErrors = 0;
            manualInterventions = 0;
        }
    };

    MarathonPerformanceSummary getPerformanceSummary();

    // Real-time compliance checking
    struct ComplianceStatus {
        bool uptimeCompliant;          // Current uptime vs target
        bool latencyCompliant;         // Current latency vs target
        bool performanceCompliant;     // Performance degradation vs target
        bool riskCompliant;           // Risk metrics within limits
        double overallComplianceScore; // 0-1 scale

        std::vector<std::string> complianceGaps;
        std::vector<std::string> recommendations;
    };

    ComplianceStatus checkComplianceStatus();

    // === STRESS TESTING INTEGRATION ===

    // Execute stress test during marathon
    StressTestResult executeMarathonStressTest(StressScenarioType scenario);

    // Execute operational stress test during marathon
    OperationalStressResult executeMarathonOperationalTest(OperationalStressType testType);

    // Schedule automatic stress tests
    void schedulePeriodicStressTests();

    // === REPORTING AND ANALYSIS ===

    // Generate comprehensive marathon report
    std::string generateMarathonReport();

    // Generate TRS compliance report
    std::string generateTRSComplianceReport();

    // Export marathon data
    bool exportMarathonData(const std::string& outputPath);

    // Real-time performance attribution
    std::map<std::string, double> getPerformanceAttribution();

    // Risk analytics during marathon
    struct MarathonRiskAnalytics {
        double currentVaR95;
        double currentVaR99;
        double stressAdjustedVaR;
        double portfolioConcentration;
        double correlationRisk;
        double liquidity AtRisk;
        std::map<std::string, double> positionRiskContributions;
        std::vector<std::string> riskWarnings;
    };

    MarathonRiskAnalytics getRiskAnalytics();

    // === EMERGENCY PROCEDURES ===

    // Emergency response system
    struct EmergencyResponse {
        MarathonEventType triggerEvent;
        std::string emergencyType;
        std::vector<std::string> immediateActions;
        std::vector<std::string> followUpActions;
        bool requiresManualApproval;
        std::chrono::seconds responseTimeSeconds;
        bool successful;
    };

    EmergencyResponse triggerEmergencyResponse(MarathonEventType eventType);

    // System recovery procedures
    bool executeSystemRecovery();
    bool validateSystemHealth();

    // === STATE MANAGEMENT ===

    bool isInitialized() const { return isInitialized_; }
    std::chrono::system_clock::time_point getMarathonStartTime() const { return marathonStartTime_; }
    std::chrono::milliseconds getElapsedTime();
    std::chrono::milliseconds getRemainingTime();

private:
    // Configuration
    MarathonConfig config_;

    // Components
    std::shared_ptr<PaperTradingEngine> tradingEngine_;
    std::shared_ptr<PaperTradingMonitor> tradingMonitor_;
    std::shared_ptr<StressTestingEngine> stressTestEngine_;
    std::shared_ptr<OperationalStressTest> operationalStressTest_;
    std::shared_ptr<DatabaseManager> dbManager_;

    // Marathon state
    std::atomic<bool> isInitialized_{false};
    std::atomic<bool> marathonActive_{false};
    std::atomic<bool> marathonPaused_{false};
    MarathonPhase currentPhase_;
    std::string marathonName_;
    std::chrono::system_clock::time_point marathonStartTime_;
    std::chrono::system_clock::time_point marathonEndTime_;

    // Monitoring and data collection
    std::unique_ptr<std::thread> monitoringThread_;
    std::unique_ptr<std::thread> stressTestThread_;
    std::unique_ptr<std::thread> metricsCollectionThread_;
    std::unique_ptr<std::thread> reportGenerationThread_;
    std::atomic<bool> stopThreads_{false};

    // Data storage
    std::vector<MarathonMetrics> metricsHistory_;
    std::vector<MarathonEvent> eventHistory_;
    std::vector<MarathonAlert> activeAlerts_;
    std::mutex dataStorageMutex_;

    // Performance tracking
    double initialPortfolioValue_;
    double peakPortfolioValue_;
    std::chrono::system_clock::time_point lastHealthCheck_;
    std::chrono::system_clock::time_point lastStressTest_;
    std::chrono::system_clock::time_point lastOperationalTest_;

    // Dashboard data
    MarathonDashboard dashboardData_;
    std::chrono::system_clock::time_point lastDashboardUpdate_;

    // === INTERNAL METHODS ===

    // Thread implementations
    void monitoringLoop();
    void stressTestLoop();
    void metricsCollectionLoop();
    void reportGenerationLoop();

    // Phase management
    void transitionToPhase(MarathonPhase newPhase);
    bool shouldTransitionPhase();
    void executePhaseActions(MarathonPhase phase);

    // Metrics calculation
    MarathonMetrics calculateCurrentMetrics();
    MarathonHealthLevel calculateHealthLevel(double healthScore);
    double calculateOverallHealthScore();

    // Event and alert management
    void recordEvent(MarathonEventType eventType, const std::string& description,
                    const std::string& severity = "INFO");
    void generateAlert(MarathonAlert::AlertLevel level, MarathonAlert::AlertCategory category,
                      const std::string& message, const std::string& component);
    void processAlerts();
    void cleanupResolvedAlerts();

    // System health monitoring
    bool checkSystemHealth();
    bool validatePerformanceTargets();
    bool checkComplianceRequirements();

    // Emergency procedures
    void handleCriticalError(const std::string& error);
    void executeAutomaticRecovery();

    // Data management
    void saveMetricsToDatabase(const MarathonMetrics& metrics);
    void saveEventToDatabase(const MarathonEvent& event);
    void cleanupOldData();

    // Utility functions
    std::string generateMarathonId();
    std::string formatDuration(std::chrono::milliseconds duration);
    std::string getCurrentPhaseDescription();

    // Validation methods
    bool validateMarathonConfiguration();
    bool validateSystemReadiness();
};

// Utility functions for extended trading marathon
namespace MarathonUtils {
    // Duration utilities
    std::chrono::hours getDurationInHours(MarathonDuration duration);
    std::string getDurationDescription(MarathonDuration duration);

    // Health level utilities
    std::string getHealthLevelDescription(MarathonHealthLevel level);
    std::string getHealthLevelColor(MarathonHealthLevel level);

    // Phase utilities
    std::string getPhaseDescription(MarathonPhase phase);
    std::chrono::minutes getPhaseEstimatedDuration(MarathonPhase phase, MarathonDuration totalDuration);

    // Performance calculations
    double calculateAnnualizedReturn(double totalReturn, std::chrono::milliseconds duration);
    double calculateSharpeRatio(const std::vector<double>& returns, double riskFreeRate = 0.02);
    double calculateMaxDrawdown(const std::vector<double>& equityCurve);
    double calculateVolatility(const std::vector<double>& returns);

    // Compliance validation
    bool validateTRSCompliance(const MarathonPerformanceSummary& summary);
    double calculateComplianceScore(const ComplianceStatus& status);

    // System resource monitoring
    double getCurrentCpuUsage();
    double getCurrentMemoryUsage();
    double getCurrentDiskUsage();
    int getCurrentThreadCount();
    int getCurrentConnectionCount();

    // Report generation helpers
    std::string generateExecutiveSummary(const MarathonPerformanceSummary& summary);
    std::string generateTechnicalSummary(const MarathonPerformanceSummary& summary);
    std::string generateComplianceSummary(const ComplianceStatus& status);
}

} // namespace Testing
} // namespace CryptoClaude