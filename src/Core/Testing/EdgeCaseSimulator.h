#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <chrono>
#include <functional>
#include <queue>
#include "../Trading/PaperTradingEngine.h"
#include "../Database/DatabaseManager.h"
#include "../Analytics/BacktestingEngine.h"

namespace CryptoClaude {
namespace Testing {

/**
 * Edge case test scenario definition
 */
struct EdgeCaseScenario {
    std::string scenario_id;
    std::string name;
    std::string description;
    std::string category; // "EXCHANGE", "NETWORK", "DATA", "SECURITY", "OPERATIONAL"
    std::string severity; // "LOW", "MEDIUM", "HIGH", "CRITICAL"

    // Test Parameters
    std::chrono::minutes duration;
    std::vector<std::string> affected_symbols;
    std::map<std::string, double> test_parameters;

    // Expected Behavior
    std::vector<std::string> expected_responses;
    double max_acceptable_loss;
    std::chrono::seconds max_recovery_time;

    EdgeCaseScenario() : duration(std::chrono::minutes(10)), max_acceptable_loss(0.05),
                        max_recovery_time(std::chrono::seconds(60)) {}
};

/**
 * Exchange simulation configuration
 */
struct ExchangeSimulation {
    std::string exchange_name;
    bool is_operational;
    double latency_ms; // Network latency
    double uptime_percentage; // Availability
    double order_rejection_rate; // Percentage of orders rejected
    double partial_fill_rate; // Percentage of orders partially filled
    std::vector<std::string> supported_symbols;
    std::map<std::string, double> trading_fees; // Symbol -> fee percentage

    ExchangeSimulation() : is_operational(true), latency_ms(50.0), uptime_percentage(99.9),
                          order_rejection_rate(0.01), partial_fill_rate(0.05) {}
};

/**
 * Network condition simulation
 */
struct NetworkCondition {
    std::string condition_name;
    double packet_loss_rate; // 0.0 to 1.0
    double latency_ms; // Network latency
    double jitter_ms; // Latency variation
    double bandwidth_limit_mbps; // Bandwidth constraint
    bool is_partition; // Complete network partition

    NetworkCondition() : packet_loss_rate(0.0), latency_ms(10.0), jitter_ms(5.0),
                        bandwidth_limit_mbps(100.0), is_partition(false) {}
};

/**
 * Edge case test result
 */
struct EdgeCaseResult {
    std::string scenario_id;
    std::chrono::system_clock::time_point test_start_time;
    std::chrono::system_clock::time_point test_end_time;

    // Test Execution
    bool test_completed;
    std::string failure_reason;
    std::chrono::seconds actual_recovery_time;

    // Performance Impact
    double portfolio_impact; // P&L impact as percentage
    int orders_affected;
    int positions_closed_emergency;
    double max_drawdown_during_test;

    // System Response
    bool emergency_procedures_triggered;
    bool risk_limits_enforced;
    bool failover_activated;
    std::vector<std::string> system_responses;

    // Validation Results
    bool passed_recovery_time_limit;
    bool passed_loss_limit;
    bool passed_system_stability;
    double overall_test_score; // 0-100

    EdgeCaseResult() : test_completed(false), actual_recovery_time(std::chrono::seconds(0)),
                      portfolio_impact(0.0), orders_affected(0), positions_closed_emergency(0),
                      max_drawdown_during_test(0.0), emergency_procedures_triggered(false),
                      risk_limits_enforced(false), failover_activated(false),
                      passed_recovery_time_limit(false), passed_loss_limit(false),
                      passed_system_stability(false), overall_test_score(0.0) {}
};

/**
 * API key rotation test parameters
 */
struct ApiKeyRotationTest {
    std::string test_name;
    std::chrono::minutes rotation_interval;
    bool simulate_key_expiry;
    bool simulate_rate_limit_changes;
    bool test_during_active_trading;
    int concurrent_api_calls;

    ApiKeyRotationTest() : rotation_interval(std::chrono::minutes(30)), simulate_key_expiry(true),
                          simulate_rate_limit_changes(true), test_during_active_trading(true),
                          concurrent_api_calls(10) {}
};

/**
 * Database corruption simulation
 */
struct DatabaseCorruptionTest {
    std::string corruption_type; // "PARTIAL", "COMPLETE", "INDEX_CORRUPTION", "LOCK_TIMEOUT"
    std::vector<std::string> affected_tables;
    double corruption_severity; // 0.1 = 10% of records affected
    bool test_backup_recovery;
    bool test_transaction_rollback;

    DatabaseCorruptionTest() : corruption_type("PARTIAL"), corruption_severity(0.1),
                              test_backup_recovery(true), test_transaction_rollback(true) {}
};

/**
 * Comprehensive edge case and security testing framework
 * Tests system resilience under extreme conditions and attack scenarios
 */
class EdgeCaseSimulator {
public:
    EdgeCaseSimulator();
    ~EdgeCaseSimulator();

    // Initialization
    bool initialize(std::shared_ptr<Trading::PaperTradingEngine> trading_engine,
                   std::shared_ptr<Database::DatabaseManager> db_manager);
    void shutdown();

    // Test Suite Management
    void loadDefaultEdgeCaseScenarios();
    void addCustomScenario(const EdgeCaseScenario& scenario);
    std::vector<EdgeCaseScenario> getAvailableScenarios() const { return edge_case_scenarios_; }

    // Exchange Outage Testing
    EdgeCaseResult testExchangeOutage(const std::string& exchange_name,
                                     std::chrono::minutes outage_duration,
                                     bool test_failover = true);

    EdgeCaseResult testPartialExchangeFailure(const std::string& exchange_name,
                                             const std::vector<std::string>& affected_symbols,
                                             std::chrono::minutes duration);

    EdgeCaseResult testExchangeLatencySpike(const std::string& exchange_name,
                                           double normal_latency_ms,
                                           double spike_latency_ms,
                                           std::chrono::minutes duration);

    // Network Condition Testing
    EdgeCaseResult testNetworkPartition(std::chrono::minutes partition_duration);
    EdgeCaseResult testHighLatencyConditions(double latency_ms, std::chrono::minutes duration);
    EdgeCaseResult testPacketLoss(double loss_rate, std::chrono::minutes duration);
    EdgeCaseResult testBandwidthConstraints(double bandwidth_mbps, std::chrono::minutes duration);

    // API Security Testing
    EdgeCaseResult testApiKeyRotation(const ApiKeyRotationTest& test_params);
    EdgeCaseResult testApiKeyExpiry(const std::string& exchange_name);
    EdgeCaseResult testRateLimitExceeded(const std::string& exchange_name, int requests_per_second);
    EdgeCaseResult testApiKeyCompromise(const std::string& exchange_name);

    // Database Resilience Testing
    EdgeCaseResult testDatabaseCorruption(const DatabaseCorruptionTest& test_params);
    EdgeCaseResult testDatabaseConnectionLoss(std::chrono::minutes outage_duration);
    EdgeCaseResult testDatabaseLockTimeout();
    EdgeCaseResult testTransactionRollback();

    // Market Data Edge Cases
    EdgeCaseResult testMarketDataOutage(const std::vector<std::string>& symbols,
                                       std::chrono::minutes duration);
    EdgeCaseResult testStaleMarketData(const std::vector<std::string>& symbols,
                                      std::chrono::minutes staleness_duration);
    EdgeCaseResult testCorruptedMarketData(const std::vector<std::string>& symbols);
    EdgeCaseResult testMarketDataSpike(const std::string& symbol, double spike_magnitude);

    // Trading Edge Cases
    EdgeCaseResult testOrderRejectionCascade(const std::vector<std::string>& symbols);
    EdgeCaseResult testPartialFillScenario(const std::string& symbol, double fill_percentage);
    EdgeCaseResult testOrderBookGap(const std::string& symbol, double gap_percentage);
    EdgeCaseResult testLiquidityDrought(const std::vector<std::string>& symbols,
                                       double liquidity_reduction);

    // Emergency Procedures Testing
    EdgeCaseResult testEmergencyShutdown();
    EdgeCaseResult testEmergencyLiquidation(const std::vector<std::string>& symbols);
    EdgeCaseResult testRiskLimitBreach(const std::string& risk_type, double breach_severity);

    // Comprehensive Stress Testing
    std::map<std::string, EdgeCaseResult> runComprehensiveEdgeCasesTesting();
    EdgeCaseResult runConcurrentFailureScenario(const std::vector<EdgeCaseScenario>& scenarios);

    // Failover and Recovery Testing
    EdgeCaseResult testFailoverMechanisms();
    EdgeCaseResult testDisasterRecovery();
    EdgeCaseResult testBackupSystemActivation();

    // Security Stress Testing
    EdgeCaseResult testDDoSResistance(int concurrent_requests, std::chrono::minutes duration);
    EdgeCaseResult testSQLInjectionAttempts();
    EdgeCaseResult testUnauthorizedAccessAttempts();

    // Configuration and Control
    void setTestEnvironmentMode(bool enabled) { test_mode_enabled_ = enabled; }
    void setMaxAcceptableLoss(double max_loss) { global_max_loss_ = max_loss; }
    void setMaxRecoveryTime(std::chrono::seconds max_time) { global_max_recovery_time_ = max_time; }

    // Reporting and Analysis
    std::string generateEdgeCaseReport(const EdgeCaseResult& result);
    std::string generateComprehensiveTestReport(const std::map<std::string, EdgeCaseResult>& results);
    double calculateSystemResilienceScore(const std::map<std::string, EdgeCaseResult>& results);

    // Test Data Management
    bool exportTestResults(const std::string& filename);
    bool saveTestConfiguration(const std::string& config_name);
    bool loadTestConfiguration(const std::string& config_name);

private:
    std::shared_ptr<Trading::PaperTradingEngine> trading_engine_;
    std::shared_ptr<Database::DatabaseManager> db_manager_;

    // Test Configuration
    bool test_mode_enabled_;
    double global_max_loss_;
    std::chrono::seconds global_max_recovery_time_;

    // Test Scenarios
    std::vector<EdgeCaseScenario> edge_case_scenarios_;
    std::map<std::string, ExchangeSimulation> exchange_simulations_;
    std::map<std::string, NetworkCondition> network_conditions_;

    // Test Execution State
    std::atomic<bool> test_in_progress_{false};
    std::string current_test_scenario_;
    std::chrono::system_clock::time_point test_start_time_;

    // Monitoring and Recovery
    std::queue<std::string> recovery_actions_;
    std::vector<std::function<void()>> emergency_procedures_;

    // Exchange Simulation Methods
    void simulateExchangeOutage(const std::string& exchange_name, bool is_down);
    void simulateExchangeLatency(const std::string& exchange_name, double latency_ms);
    void simulateOrderRejections(const std::string& exchange_name, double rejection_rate);

    // Network Simulation Methods
    void simulateNetworkPartition(bool is_partitioned);
    void simulateNetworkLatency(double latency_ms);
    void simulatePacketLoss(double loss_rate);

    // Database Simulation Methods
    void simulateDatabaseCorruption(const std::string& corruption_type, double severity);
    void simulateDatabaseOutage(bool is_down);
    void simulateConnectionTimeouts();

    // API Security Simulation
    void simulateApiKeyRotation(const std::string& exchange_name);
    void simulateApiKeyExpiry(const std::string& exchange_name);
    void simulateRateLimitExceeded(const std::string& exchange_name);

    // Market Data Simulation
    void simulateMarketDataOutage(const std::vector<std::string>& symbols, bool is_down);
    void simulateStaleData(const std::vector<std::string>& symbols);
    void simulateCorruptedData(const std::vector<std::string>& symbols);

    // Recovery Mechanisms
    bool attemptSystemRecovery();
    void activateFailoverSystems();
    void executeEmergencyProcedures();
    void validateSystemStability();

    // Monitoring and Metrics
    EdgeCaseResult monitorTestExecution(const EdgeCaseScenario& scenario,
                                       const std::function<void()>& test_function);

    double calculatePortfolioImpact();
    std::chrono::seconds measureRecoveryTime(const std::chrono::system_clock::time_point& failure_time);
    bool verifySystemIntegrity();

    // Test Result Analysis
    bool evaluateTestSuccess(const EdgeCaseResult& result, const EdgeCaseScenario& scenario);
    double calculateTestScore(const EdgeCaseResult& result);

    // Utility Functions
    EdgeCaseScenario createExchangeOutageScenario(const std::string& exchange_name,
                                                 std::chrono::minutes duration);

    EdgeCaseScenario createNetworkFailureScenario(const std::string& failure_type,
                                                  std::chrono::minutes duration);

    EdgeCaseScenario createDatabaseFailureScenario(const std::string& failure_type);

    std::string generateTestId();
    std::string timePointToString(const std::chrono::system_clock::time_point& tp);
    void logTestEvent(const std::string& event_type, const std::string& description);

    // Safety Mechanisms
    void enableTestSafetyLimits();
    void disableTestSafetyLimits();
    bool checkTestSafetyLimits(const EdgeCaseResult& current_result);

    // Database Operations for Test Results
    bool createEdgeCaseTestingTables();
    bool saveEdgeCaseResult(const EdgeCaseResult& result);
    std::vector<EdgeCaseResult> loadTestHistory(const std::string& scenario_type);
};

}} // namespace CryptoClaude::Testing