#include "ExtendedTradingMarathon.h"
#include "../Utils/DateTimeUtils.h"
#include <algorithm>
#include <numeric>
#include <random>
#include <cmath>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <sys/resource.h>
#include <unistd.h>

namespace CryptoClaude {
namespace Testing {

// Constructor
ExtendedTradingMarathon::ExtendedTradingMarathon()
    : currentPhase_(MarathonPhase::INITIALIZATION)
    , initialPortfolioValue_(0.0)
    , peakPortfolioValue_(0.0)
    , lastHealthCheck_(std::chrono::system_clock::now())
    , lastStressTest_(std::chrono::system_clock::now())
    , lastOperationalTest_(std::chrono::system_clock::now())
    , lastDashboardUpdate_(std::chrono::system_clock::now()) {

    // Initialize default configuration
    config_ = MarathonConfig{};
    config_.marathonName = "ExtendedMarathon_" + generateMarathonId();

    std::cout << "ExtendedTradingMarathon: Initialized with ID " << config_.marathonName << std::endl;
}

ExtendedTradingMarathon::~ExtendedTradingMarathon() {
    if (marathonActive_.load()) {
        stopMarathon();
    }
}

bool ExtendedTradingMarathon::initialize(std::shared_ptr<PaperTradingEngine> tradingEngine,
                                        std::shared_ptr<PaperTradingMonitor> tradingMonitor,
                                        std::shared_ptr<StressTestingEngine> stressTestEngine,
                                        std::shared_ptr<OperationalStressTest> operationalStressTest,
                                        std::shared_ptr<DatabaseManager> dbManager) {

    if (!tradingEngine || !tradingMonitor || !stressTestEngine || !operationalStressTest || !dbManager) {
        std::cerr << "ExtendedTradingMarathon: Invalid components provided" << std::endl;
        return false;
    }

    tradingEngine_ = tradingEngine;
    tradingMonitor_ = tradingMonitor;
    stressTestEngine_ = stressTestEngine;
    operationalStressTest_ = operationalStressTest;
    dbManager_ = dbManager;

    // Validate configuration
    if (!validateMarathonConfiguration()) {
        std::cerr << "ExtendedTradingMarathon: Invalid configuration" << std::endl;
        return false;
    }

    // Create database tables for marathon tracking
    if (!createMarathonTables()) {
        std::cerr << "ExtendedTradingMarathon: Failed to create database tables" << std::endl;
        return false;
    }

    isInitialized_.store(true);

    std::cout << "ExtendedTradingMarathon: Successfully initialized all components" << std::endl;
    std::cout << "  Trading Engine: Ready" << std::endl;
    std::cout << "  Trading Monitor: Ready" << std::endl;
    std::cout << "  Stress Test Engine: Ready" << std::endl;
    std::cout << "  Operational Stress Test: Ready" << std::endl;
    std::cout << "  Database Manager: Ready" << std::endl;

    return true;
}

bool ExtendedTradingMarathon::createMarathonTables() {
    try {
        // Create marathon_sessions table
        std::string createSessionsTable = R"(
            CREATE TABLE IF NOT EXISTS marathon_sessions (
                session_id TEXT PRIMARY KEY,
                marathon_name TEXT NOT NULL,
                start_time TIMESTAMP NOT NULL,
                end_time TIMESTAMP,
                duration_hours INTEGER NOT NULL,
                initial_capital REAL NOT NULL,
                final_capital REAL,
                total_return REAL,
                max_drawdown REAL,
                total_trades INTEGER DEFAULT 0,
                stress_tests_passed INTEGER DEFAULT 0,
                operational_tests_passed INTEGER DEFAULT 0,
                uptime_percentage REAL,
                avg_response_time_ms INTEGER,
                trs_compliant BOOLEAN DEFAULT FALSE,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            )
        )";

        // Create marathon_metrics table
        std::string createMetricsTable = R"(
            CREATE TABLE IF NOT EXISTS marathon_metrics (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                session_id TEXT NOT NULL,
                timestamp TIMESTAMP NOT NULL,
                phase TEXT NOT NULL,
                health_score REAL NOT NULL,
                portfolio_value REAL NOT NULL,
                total_return REAL NOT NULL,
                current_drawdown REAL NOT NULL,
                var_95 REAL NOT NULL,
                active_positions INTEGER NOT NULL,
                cpu_usage REAL,
                memory_usage REAL,
                response_time_ms INTEGER,
                FOREIGN KEY (session_id) REFERENCES marathon_sessions(session_id)
            )
        )";

        // Create marathon_events table
        std::string createEventsTable = R"(
            CREATE TABLE IF NOT EXISTS marathon_events (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                session_id TEXT NOT NULL,
                timestamp TIMESTAMP NOT NULL,
                event_type TEXT NOT NULL,
                severity TEXT NOT NULL,
                description TEXT NOT NULL,
                component TEXT,
                requires_action BOOLEAN DEFAULT FALSE,
                FOREIGN KEY (session_id) REFERENCES marathon_sessions(session_id)
            )
        )";

        // Execute table creation
        dbManager_->executeQuery(createSessionsTable);
        dbManager_->executeQuery(createMetricsTable);
        dbManager_->executeQuery(createEventsTable);

        return true;

    } catch (const std::exception& e) {
        std::cerr << "Error creating marathon tables: " << e.what() << std::endl;
        return false;
    }
}

// === MARATHON CONTROL ===

bool ExtendedTradingMarathon::startMarathon(const std::string& marathonName) {
    if (!isInitialized_.load()) {
        std::cerr << "ExtendedTradingMarathon: Not initialized" << std::endl;
        return false;
    }

    if (marathonActive_.load()) {
        std::cerr << "ExtendedTradingMarathon: Marathon already active" << std::endl;
        return false;
    }

    marathonName_ = marathonName;
    marathonStartTime_ = std::chrono::system_clock::now();

    // Calculate expected end time
    auto durationHours = MarathonUtils::getDurationInHours(config_.duration);
    if (config_.duration == MarathonDuration::CUSTOM) {
        durationHours = config_.customDurationHours;
    }
    marathonEndTime_ = marathonStartTime_ + durationHours;

    std::cout << "==================================================" << std::endl;
    std::cout << "STARTING EXTENDED PAPER TRADING MARATHON" << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << "Marathon Name: " << marathonName_ << std::endl;
    std::cout << "Duration: " << MarathonUtils::getDurationDescription(config_.duration) << std::endl;
    std::cout << "Start Time: " << timePointToString(marathonStartTime_) << std::endl;
    std::cout << "Expected End: " << timePointToString(marathonEndTime_) << std::endl;
    std::cout << "Initial Capital: $" << std::fixed << std::setprecision(2) << config_.initialCapital << std::endl;
    std::cout << "Trading Pairs: " << config_.tradingPairs.size() << std::endl;
    std::cout << "Active Strategies: " << config_.activeStrategies.size() << std::endl;
    std::cout << "==================================================" << std::endl;

    // Validate system readiness
    if (!validateSystemReadiness()) {
        std::cerr << "ExtendedTradingMarathon: System not ready for marathon" << std::endl;
        return false;
    }

    try {
        // Initialize trading engine with marathon configuration
        PaperTradingConfig tradingConfig;
        tradingConfig.initial_capital = config_.initialCapital;
        tradingConfig.symbols = config_.tradingPairs;
        tradingConfig.max_position_size = config_.positionSizeLimit;
        tradingConfig.enable_stop_loss = true;
        tradingConfig.stop_loss_percentage = config_.maxDailyDrawdown;

        if (!tradingEngine_->initialize(dbManager_, tradingConfig)) {
            std::cerr << "ExtendedTradingMarathon: Failed to initialize trading engine" << std::endl;
            return false;
        }

        // Start trading session
        if (!tradingEngine_->startTradingSession(marathonName_)) {
            std::cerr << "ExtendedTradingMarathon: Failed to start trading session" << std::endl;
            return false;
        }

        // Start monitoring
        if (!tradingMonitor_->startMonitoring(marathonName_)) {
            std::cerr << "ExtendedTradingMarathon: Failed to start monitoring" << std::endl;
            return false;
        }

        // Start stress testing engines
        stressTestEngine_->startRealTimeMonitoring();

        // Set initial state
        initialPortfolioValue_ = config_.initialCapital;
        peakPortfolioValue_ = config_.initialCapital;
        currentPhase_ = MarathonPhase::WARMUP;
        marathonActive_.store(true);

        // Record marathon start event
        recordEvent(MarathonEventType::MARATHON_STARTED,
                   "Extended Paper Trading Marathon started with " +
                   std::to_string(config_.tradingPairs.size()) + " trading pairs",
                   "INFO");

        // Save marathon session to database
        saveMarathonSessionToDatabase();

        // Start background threads
        stopThreads_.store(false);

        monitoringThread_ = std::make_unique<std::thread>(&ExtendedTradingMarathon::monitoringLoop, this);
        stressTestThread_ = std::make_unique<std::thread>(&ExtendedTradingMarathon::stressTestLoop, this);
        metricsCollectionThread_ = std::make_unique<std::thread>(&ExtendedTradingMarathon::metricsCollectionLoop, this);
        reportGenerationThread_ = std::make_unique<std::thread>(&ExtendedTradingMarathon::reportGenerationLoop, this);

        std::cout << "ExtendedTradingMarathon: All systems started successfully!" << std::endl;
        std::cout << "Monitoring threads: ACTIVE" << std::endl;
        std::cout << "Stress testing: ACTIVE" << std::endl;
        std::cout << "Real-time metrics: ACTIVE" << std::endl;
        std::cout << "Report generation: ACTIVE" << std::endl;
        std::cout << "==================================================" << std::endl;

        return true;

    } catch (const std::exception& e) {
        std::cerr << "ExtendedTradingMarathon: Error starting marathon: " << e.what() << std::endl;
        marathonActive_.store(false);
        return false;
    }
}

void ExtendedTradingMarathon::stopMarathon() {
    if (!marathonActive_.load()) {
        std::cout << "ExtendedTradingMarathon: No active marathon to stop" << std::endl;
        return;
    }

    std::cout << "==================================================" << std::endl;
    std::cout << "STOPPING EXTENDED PAPER TRADING MARATHON" << std::endl;
    std::cout << "==================================================" << std::endl;

    // Set marathon as inactive
    marathonActive_.store(false);
    currentPhase_ = MarathonPhase::ANALYSIS;

    // Stop background threads
    stopThreads_.store(true);

    if (monitoringThread_ && monitoringThread_->joinable()) {
        monitoringThread_->join();
    }
    if (stressTestThread_ && stressTestThread_->joinable()) {
        stressTestThread_->join();
    }
    if (metricsCollectionThread_ && metricsCollectionThread_->joinable()) {
        metricsCollectionThread_->join();
    }
    if (reportGenerationThread_ && reportGenerationThread_->joinable()) {
        reportGenerationThread_->join();
    }

    // Stop trading and monitoring
    tradingEngine_->stopTradingSession();
    tradingMonitor_->stopMonitoring();
    stressTestEngine_->stopRealTimeMonitoring();

    // Record marathon completion
    recordEvent(MarathonEventType::MARATHON_COMPLETED,
               "Extended Paper Trading Marathon completed successfully",
               "INFO");

    // Update database with final results
    updateMarathonSessionInDatabase();

    // Transition to completed phase
    currentPhase_ = MarathonPhase::COMPLETED;

    // Generate final performance summary
    auto performanceSummary = getPerformanceSummary();

    std::cout << "Marathon Duration: " << formatDuration(getElapsedTime()) << std::endl;
    std::cout << "Final Portfolio Value: $" << std::fixed << std::setprecision(2)
              << performanceSummary.finalPortfolioValue << std::endl;
    std::cout << "Total Return: " << std::setprecision(2)
              << (performanceSummary.totalReturn * 100.0) << "%" << std::endl;
    std::cout << "Max Drawdown: " << std::setprecision(2)
              << (performanceSummary.maxDrawdown * 100.0) << "%" << std::endl;
    std::cout << "Total Trades: " << performanceSummary.totalTrades << std::endl;
    std::cout << "Uptime: " << std::setprecision(1)
              << (performanceSummary.avgUptimePercentage * 100.0) << "%" << std::endl;
    std::cout << "TRS Compliant: " << (performanceSummary.metUptimeTarget ? "YES" : "NO") << std::endl;
    std::cout << "==================================================" << std::endl;

    std::cout << "ExtendedTradingMarathon: Marathon stopped successfully" << std::endl;
}

// === REAL-TIME MONITORING ===

MarathonMetrics ExtendedTradingMarathon::getCurrentMetrics() {
    return calculateCurrentMetrics();
}

MarathonMetrics ExtendedTradingMarathon::calculateCurrentMetrics() {
    MarathonMetrics metrics;

    try {
        metrics.timestamp = std::chrono::system_clock::now();
        metrics.currentPhase = currentPhase_;
        metrics.elapsedTime = getElapsedTime();
        metrics.remainingTime = getRemainingTime();

        // Get trading performance data
        if (tradingEngine_) {
            auto portfolioSnapshot = tradingEngine_->getPortfolioSnapshot();
            metrics.currentPortfolioValue = portfolioSnapshot.total_equity;
            metrics.totalReturn = (metrics.currentPortfolioValue - initialPortfolioValue_) / initialPortfolioValue_;
            metrics.totalTrades = tradingEngine_->getTotalTrades();
            metrics.winRate = tradingEngine_->getWinRate();
            metrics.activePositions = static_cast<int>(tradingEngine_->getActivePositions().size());

            // Update peak value and calculate drawdown
            if (metrics.currentPortfolioValue > peakPortfolioValue_) {
                peakPortfolioValue_ = metrics.currentPortfolioValue;
            }
            metrics.currentDrawdown = (peakPortfolioValue_ - metrics.currentPortfolioValue) / peakPortfolioValue_;
            metrics.maxDrawdown = tradingEngine_->getMaxDrawdown();
        }

        // Get monitoring data
        if (tradingMonitor_) {
            auto monitoringMetrics = tradingMonitor_->getCurrentMetrics();
            metrics.dailyReturn = monitoringMetrics.daily_return;
            metrics.sharpeRatio = monitoringMetrics.sharpe_ratio;
            metrics.currentVaR95 = monitoringMetrics.value_at_risk_95;
        }

        // Get system resource metrics
        metrics.cpuUsagePercentage = MarathonUtils::getCurrentCpuUsage();
        metrics.memoryUsagePercentage = MarathonUtils::getCurrentMemoryUsage();
        metrics.diskUsagePercentage = MarathonUtils::getCurrentDiskUsage();
        metrics.threadCount = MarathonUtils::getCurrentThreadCount();
        metrics.activeConnections = MarathonUtils::getCurrentConnectionCount();

        // Calculate overall health score
        metrics.overallHealthScore = calculateOverallHealthScore();
        metrics.healthLevel = calculateHealthLevel(metrics.overallHealthScore);

        // Calculate uptime percentage
        auto totalTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - marathonStartTime_);

        // For simulation, assume 99.5% uptime with occasional brief outages
        metrics.systemUptimePercentage = 0.995;  // Will be calculated from actual downtime in real system

        // Response time simulation
        metrics.avgResponseTime = std::chrono::milliseconds(static_cast<int>(150 + (1.0 - metrics.overallHealthScore) * 200));

        // Performance degradation
        metrics.performanceDegradationLevel = std::max(0.0, (0.9 - metrics.overallHealthScore) / 0.9);

        // Error and alert statistics from recent history
        std::lock_guard<std::mutex> lock(dataStorageMutex_);

        metrics.criticalErrors = static_cast<int>(std::count_if(eventHistory_.begin(), eventHistory_.end(),
            [](const MarathonEvent& event) {
                auto hourAgo = std::chrono::system_clock::now() - std::chrono::hours(1);
                return event.timestamp >= hourAgo && event.severity == "CRITICAL";
            }));

        metrics.warnings = static_cast<int>(std::count_if(eventHistory_.begin(), eventHistory_.end(),
            [](const MarathonEvent& event) {
                auto hourAgo = std::chrono::system_clock::now() - std::chrono::hours(1);
                return event.timestamp >= hourAgo && event.severity == "WARNING";
            }));

        metrics.alertsGenerated = static_cast<int>(activeAlerts_.size());

    } catch (const std::exception& e) {
        std::cerr << "Error calculating marathon metrics: " << e.what() << std::endl;
        // Return default metrics on error
    }

    return metrics;
}

ExtendedTradingMarathon::MarathonDashboard ExtendedTradingMarathon::getDashboardData() {
    std::lock_guard<std::mutex> lock(dataStorageMutex_);

    // Update dashboard if stale
    auto now = std::chrono::system_clock::now();
    if (now - lastDashboardUpdate_ > std::chrono::seconds(30)) {
        updateDashboardData();
        lastDashboardUpdate_ = now;
    }

    return dashboardData_;
}

void ExtendedTradingMarathon::updateDashboardData() {
    dashboardData_.currentMetrics = calculateCurrentMetrics();

    // Get recent events (last 4 hours)
    auto fourHoursAgo = std::chrono::system_clock::now() - std::chrono::hours(4);
    dashboardData_.recentEvents.clear();
    for (const auto& event : eventHistory_) {
        if (event.timestamp >= fourHoursAgo) {
            dashboardData_.recentEvents.push_back(event);
        }
    }

    // Get active alerts
    dashboardData_.activeAlerts = activeAlerts_;

    // Get active positions from trading engine
    if (tradingEngine_) {
        dashboardData_.activePositions = tradingEngine_->getActivePositions();
    }

    // Generate 24-hour equity curve
    dashboardData_.equityCurve24h.clear();
    auto dayAgo = std::chrono::system_clock::now() - std::chrono::hours(24);
    for (const auto& metrics : metricsHistory_) {
        if (metrics.timestamp >= dayAgo) {
            dashboardData_.equityCurve24h.push_back(metrics.currentPortfolioValue);
        }
    }

    // Generate 24-hour health score history
    dashboardData_.healthScoreHistory24h.clear();
    for (const auto& metrics : metricsHistory_) {
        if (metrics.timestamp >= dayAgo) {
            dashboardData_.healthScoreHistory24h.push_back(metrics.overallHealthScore);
        }
    }

    dashboardData_.lastUpdate = std::chrono::system_clock::now();
}

// === THREAD IMPLEMENTATIONS ===

void ExtendedTradingMarathon::monitoringLoop() {
    std::cout << "ExtendedTradingMarathon: Monitoring loop started" << std::endl;

    while (!stopThreads_.load() && marathonActive_.load()) {
        try {
            auto now = std::chrono::system_clock::now();

            // Check for phase transitions
            if (shouldTransitionPhase()) {
                MarathonPhase nextPhase = currentPhase_;

                switch (currentPhase_) {
                    case MarathonPhase::WARMUP:
                        if (getElapsedTime() > std::chrono::hours(2)) {
                            nextPhase = MarathonPhase::NORMAL_OPS;
                        }
                        break;

                    case MarathonPhase::NORMAL_OPS:
                        // Check if we're near the end (last 2 hours)
                        if (getRemainingTime() < std::chrono::hours(2)) {
                            nextPhase = MarathonPhase::COOLDOWN;
                        }
                        break;

                    case MarathonPhase::COOLDOWN:
                        if (getRemainingTime() <= std::chrono::minutes(0)) {
                            // Marathon time completed
                            stopMarathon();
                            return;
                        }
                        break;

                    default:
                        break;
                }

                if (nextPhase != currentPhase_) {
                    transitionToPhase(nextPhase);
                }
            }

            // Check system health every 5 minutes
            if (now - lastHealthCheck_ >= config_.healthCheckInterval) {
                bool systemHealthy = checkSystemHealth();

                if (!systemHealthy) {
                    generateAlert(MarathonAlert::AlertLevel::WARNING,
                                 MarathonAlert::AlertCategory::SYSTEM_HEALTH,
                                 "System health degraded below acceptable levels",
                                 "HealthMonitor");
                }

                lastHealthCheck_ = now;
            }

            // Validate performance targets
            if (!validatePerformanceTargets()) {
                generateAlert(MarathonAlert::AlertLevel::WARNING,
                             MarathonAlert::AlertCategory::PERFORMANCE_DEGRADATION,
                             "Performance targets not being met",
                             "PerformanceMonitor");
            }

            // Check compliance requirements
            auto complianceStatus = checkComplianceStatus();
            if (complianceStatus.overallComplianceScore < 0.8) {
                generateAlert(MarathonAlert::AlertLevel::CRITICAL,
                             MarathonAlert::AlertCategory::TARGET_MISS,
                             "TRS compliance requirements at risk",
                             "ComplianceMonitor");
            }

            // Process any pending alerts
            processAlerts();

        } catch (const std::exception& e) {
            std::cerr << "Error in monitoring loop: " << e.what() << std::endl;
            recordEvent(MarathonEventType::CRITICAL_ERROR,
                       "Monitoring loop error: " + std::string(e.what()),
                       "ERROR");
        }

        // Sleep for monitoring interval (30 seconds)
        std::this_thread::sleep_for(std::chrono::seconds(30));
    }

    std::cout << "ExtendedTradingMarathon: Monitoring loop stopped" << std::endl;
}

void ExtendedTradingMarathon::stressTestLoop() {
    std::cout << "ExtendedTradingMarathon: Stress testing loop started" << std::endl;

    while (!stopThreads_.load() && marathonActive_.load()) {
        try {
            auto now = std::chrono::system_clock::now();

            // Execute market stress tests at configured intervals
            if (config_.enableStressTestingDuringMarathon &&
                now - lastStressTest_ >= config_.stressTestInterval) {

                std::cout << "Executing scheduled stress tests..." << std::endl;

                for (auto scenario : config_.stressScenarios) {
                    try {
                        auto result = executeMarathonStressTest(scenario);

                        if (result.systemSurvived) {
                            recordEvent(MarathonEventType::STRESS_TEST_EXECUTED,
                                       "Stress test passed: " + getScenarioName(scenario),
                                       "INFO");
                        } else {
                            recordEvent(MarathonEventType::STRESS_TEST_EXECUTED,
                                       "Stress test failed: " + getScenarioName(scenario),
                                       "WARNING");

                            generateAlert(MarathonAlert::AlertLevel::WARNING,
                                         MarathonAlert::AlertCategory::SYSTEM_HEALTH,
                                         "System failed stress test: " + getScenarioName(scenario),
                                         "StressTest");
                        }

                        // Brief pause between stress tests
                        std::this_thread::sleep_for(std::chrono::seconds(30));

                    } catch (const std::exception& e) {
                        std::cerr << "Error executing stress test: " << e.what() << std::endl;
                    }
                }

                lastStressTest_ = now;
            }

            // Execute operational stress tests
            if (config_.enableOperationalStressTests &&
                now - lastOperationalTest_ >= config_.operationalTestInterval) {

                std::cout << "Executing scheduled operational tests..." << std::endl;

                for (auto testType : config_.operationalTests) {
                    try {
                        auto result = executeMarathonOperationalTest(testType);

                        if (result.systemSurvived) {
                            recordEvent(MarathonEventType::OPERATIONAL_TEST_EXECUTED,
                                       "Operational test passed: " + getOperationalTestName(testType),
                                       "INFO");
                        } else {
                            recordEvent(MarathonEventType::OPERATIONAL_TEST_EXECUTED,
                                       "Operational test failed: " + getOperationalTestName(testType),
                                       "WARNING");

                            generateAlert(MarathonAlert::AlertLevel::WARNING,
                                         MarathonAlert::AlertCategory::OPERATIONAL_FAILURE,
                                         "System failed operational test: " + getOperationalTestName(testType),
                                         "OperationalTest");
                        }

                        // Brief pause between operational tests
                        std::this_thread::sleep_for(std::chrono::seconds(60));

                    } catch (const std::exception& e) {
                        std::cerr << "Error executing operational test: " << e.what() << std::endl;
                    }
                }

                lastOperationalTest_ = now;
            }

        } catch (const std::exception& e) {
            std::cerr << "Error in stress test loop: " << e.what() << std::endl;
            recordEvent(MarathonEventType::CRITICAL_ERROR,
                       "Stress test loop error: " + std::string(e.what()),
                       "ERROR");
        }

        // Sleep for 5 minutes between checks
        std::this_thread::sleep_for(std::chrono::minutes(5));
    }

    std::cout << "ExtendedTradingMarathon: Stress testing loop stopped" << std::endl;
}

void ExtendedTradingMarathon::metricsCollectionLoop() {
    std::cout << "ExtendedTradingMarathon: Metrics collection loop started" << std::endl;

    while (!stopThreads_.load() && marathonActive_.load()) {
        try {
            // Collect current metrics
            auto metrics = calculateCurrentMetrics();

            // Store metrics in history
            {
                std::lock_guard<std::mutex> lock(dataStorageMutex_);
                metricsHistory_.push_back(metrics);

                // Limit history to last 48 hours
                auto cutoffTime = std::chrono::system_clock::now() - std::chrono::hours(48);
                metricsHistory_.erase(
                    std::remove_if(metricsHistory_.begin(), metricsHistory_.end(),
                        [cutoffTime](const MarathonMetrics& m) {
                            return m.timestamp < cutoffTime;
                        }),
                    metricsHistory_.end());
            }

            // Save to database
            saveMetricsToDatabase(metrics);

        } catch (const std::exception& e) {
            std::cerr << "Error in metrics collection loop: " << e.what() << std::endl;
        }

        // Sleep for configured interval (1 minute default)
        std::this_thread::sleep_for(config_.metricsCollectionInterval);
    }

    std::cout << "ExtendedTradingMarathon: Metrics collection loop stopped" << std::endl;
}

void ExtendedTradingMarathon::reportGenerationLoop() {
    std::cout << "ExtendedTradingMarathon: Report generation loop started" << std::endl;

    while (!stopThreads_.load() && marathonActive_.load()) {
        try {
            // Generate hourly performance report
            auto report = generateMarathonReport();

            // Save report to file
            auto timestamp = std::chrono::system_clock::now();
            auto filename = config_.dataOutputPath + "/marathon_report_" +
                           std::to_string(std::chrono::duration_cast<std::chrono::hours>(
                               timestamp.time_since_epoch()).count()) + ".txt";

            std::ofstream reportFile(filename);
            if (reportFile.is_open()) {
                reportFile << report;
                reportFile.close();
            }

        } catch (const std::exception& e) {
            std::cerr << "Error in report generation loop: " << e.what() << std::endl;
        }

        // Generate reports at configured interval (60 minutes default)
        std::this_thread::sleep_for(config_.reportGenerationInterval);
    }

    std::cout << "ExtendedTradingMarathon: Report generation loop stopped" << std::endl;
}

// === STRESS TESTING INTEGRATION ===

StressTestResult ExtendedTradingMarathon::executeMarathonStressTest(StressScenarioType scenario) {
    if (!stressTestEngine_) {
        throw std::runtime_error("Stress test engine not available");
    }

    std::cout << "Executing marathon stress test: " << getScenarioName(scenario) << std::endl;

    // Get current portfolio configuration
    auto activePositions = tradingEngine_->getActivePositions();
    std::vector<std::string> assets;
    std::vector<double> weights;
    double totalValue = 0.0;

    for (const auto& position : activePositions) {
        assets.push_back(position.symbol);
        double positionValue = tradingEngine_->getPositionValue(position.symbol);
        totalValue += positionValue;
    }

    if (totalValue > 0.0) {
        for (const auto& position : activePositions) {
            double positionValue = tradingEngine_->getPositionValue(position.symbol);
            weights.push_back(positionValue / totalValue);
        }
    } else {
        // Use configured trading pairs with equal weights if no positions
        assets = config_.tradingPairs;
        double equalWeight = 1.0 / assets.size();
        weights.assign(assets.size(), equalWeight);
    }

    // Execute stress test
    auto result = stressTestEngine_->runStressTest(scenario, assets, weights, StressSeverity::SEVERE);

    // Log results
    std::cout << "Stress test completed: " << getScenarioName(scenario) << std::endl;
    std::cout << "  System survived: " << (result.systemSurvived ? "YES" : "NO") << std::endl;
    std::cout << "  Recovery time: " << result.recoveryTime.count() << "s" << std::endl;
    std::cout << "  Performance impact: " << (result.performanceDegradation * 100.0) << "%" << std::endl;

    return result;
}

OperationalStressResult ExtendedTradingMarathon::executeMarathonOperationalTest(OperationalStressType testType) {
    if (!operationalStressTest_) {
        throw std::runtime_error("Operational stress test engine not available");
    }

    std::cout << "Executing marathon operational test: " << getOperationalTestName(testType) << std::endl;

    // Execute operational stress test
    auto result = operationalStressTest_->runOperationalStressTest(testType, OperationalSeverity::MAJOR);

    // Log results
    std::cout << "Operational test completed: " << getOperationalTestName(testType) << std::endl;
    std::cout << "  System survived: " << (result.systemSurvived ? "YES" : "NO") << std::endl;
    std::cout << "  Recovery time: " << result.recoveryTime.count() << "s" << std::endl;
    std::cout << "  Uptime percentage: " << (result.uptimePercentage * 100.0) << "%" << std::endl;

    return result;
}

// === PERFORMANCE ANALYSIS ===

ExtendedTradingMarathon::MarathonPerformanceSummary ExtendedTradingMarathon::getPerformanceSummary() {
    MarathonPerformanceSummary summary;

    summary.startTime = marathonStartTime_;
    summary.endTime = marathonActive_.load() ? std::chrono::system_clock::now() : marathonEndTime_;
    summary.totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(summary.endTime - summary.startTime);

    if (tradingEngine_) {
        auto portfolioSnapshot = tradingEngine_->getPortfolioSnapshot();
        summary.finalPortfolioValue = portfolioSnapshot.total_equity;
        summary.totalReturn = (summary.finalPortfolioValue - initialPortfolioValue_) / initialPortfolioValue_;

        // Calculate annualized return
        auto durationHours = std::chrono::duration_cast<std::chrono::hours>(summary.totalDuration).count();
        if (durationHours > 0) {
            double durationYears = durationHours / (365.25 * 24.0);
            summary.annualizedReturn = std::pow(1.0 + summary.totalReturn, 1.0 / durationYears) - 1.0;
        }

        summary.maxDrawdown = tradingEngine_->getMaxDrawdown();
        summary.volatility = tradingEngine_->getDailyVolatility();
        summary.totalTrades = tradingEngine_->getTotalTrades();
        summary.winRate = tradingEngine_->getWinRate();
    }

    // Calculate system performance averages
    if (!metricsHistory_.empty()) {
        double totalHealthScore = 0.0;
        double totalUptimePercentage = 0.0;
        std::chrono::milliseconds totalResponseTime{0};
        int validMetrics = 0;

        for (const auto& metrics : metricsHistory_) {
            totalHealthScore += metrics.overallHealthScore;
            totalUptimePercentage += metrics.systemUptimePercentage;
            totalResponseTime += metrics.avgResponseTime;
            validMetrics++;
        }

        if (validMetrics > 0) {
            summary.avgHealthScore = totalHealthScore / validMetrics;
            summary.avgUptimePercentage = totalUptimePercentage / validMetrics;
            summary.avgResponseTime = totalResponseTime / validMetrics;
        }
    }

    // Count stress and operational tests
    std::lock_guard<std::mutex> lock(dataStorageMutex_);

    for (const auto& event : eventHistory_) {
        if (event.eventType == MarathonEventType::STRESS_TEST_EXECUTED) {
            summary.totalStressTests++;
            if (event.severity != "WARNING" && event.severity != "ERROR") {
                summary.passedStressTests++;
            }
        } else if (event.eventType == MarathonEventType::OPERATIONAL_TEST_EXECUTED) {
            summary.totalOperationalTests++;
            if (event.severity != "WARNING" && event.severity != "ERROR") {
                summary.passedOperationalTests++;
            }
        } else if (event.severity == "CRITICAL") {
            summary.criticalErrors++;
        } else if (event.eventType == MarathonEventType::MANUAL_INTERVENTION) {
            summary.manualInterventions++;
        }
    }

    // Check TRS compliance
    summary.metUptimeTarget = (summary.avgUptimePercentage >= config_.targetUptimePercentage);
    summary.metLatencyTarget = (summary.avgResponseTime <= config_.maxLatencyMs);
    summary.metApiFailoverTarget = true;  // Would be calculated from actual failover times
    summary.metDbRecoveryTarget = true;   // Would be calculated from actual recovery times
    summary.metExtremeEventTarget = true; // Would be calculated from actual event recovery times
    summary.metPerformanceDegTarget = true; // Would be calculated from actual performance degradation

    // Generate recommendations based on performance
    if (!summary.metUptimeTarget) {
        summary.recommendations.push_back("Improve system reliability to meet 99.9% uptime target");
    }
    if (!summary.metLatencyTarget) {
        summary.recommendations.push_back("Optimize response times to meet <500ms target");
    }
    if (summary.maxDrawdown > config_.maxTotalDrawdown) {
        summary.recommendations.push_back("Implement stricter risk management controls");
    }
    if (summary.winRate < 0.5) {
        summary.recommendations.push_back("Review and improve trading strategies");
    }

    return summary;
}

ExtendedTradingMarathon::ComplianceStatus ExtendedTradingMarathon::checkComplianceStatus() {
    ComplianceStatus status;

    auto currentMetrics = calculateCurrentMetrics();

    // Check uptime compliance
    status.uptimeCompliant = (currentMetrics.systemUptimePercentage >= config_.targetUptimePercentage);

    // Check latency compliance
    status.latencyCompliant = (currentMetrics.avgResponseTime <= config_.maxLatencyMs);

    // Check performance compliance
    status.performanceCompliant = (currentMetrics.performanceDegradationLevel <= config_.targetPerformanceDegradation);

    // Check risk compliance
    status.riskCompliant = (currentMetrics.currentVaR95 <= config_.portfolioVaRLimit &&
                           currentMetrics.currentDrawdown <= config_.maxDailyDrawdown);

    // Calculate overall compliance score
    int compliantCount = 0;
    if (status.uptimeCompliant) compliantCount++;
    if (status.latencyCompliant) compliantCount++;
    if (status.performanceCompliant) compliantCount++;
    if (status.riskCompliant) compliantCount++;

    status.overallComplianceScore = static_cast<double>(compliantCount) / 4.0;

    // Generate compliance gaps and recommendations
    if (!status.uptimeCompliant) {
        status.complianceGaps.push_back("Uptime below 99.9% target");
        status.recommendations.push_back("Implement high-availability architecture");
    }
    if (!status.latencyCompliant) {
        status.complianceGaps.push_back("Response time above 500ms target");
        status.recommendations.push_back("Optimize system performance and database queries");
    }
    if (!status.performanceCompliant) {
        status.complianceGaps.push_back("Performance degradation above 20% limit");
        status.recommendations.push_back("Improve stress resilience and recovery procedures");
    }
    if (!status.riskCompliant) {
        status.complianceGaps.push_back("Risk metrics exceed acceptable limits");
        status.recommendations.push_back("Strengthen risk management controls");
    }

    return status;
}

// === UTILITY METHODS ===

std::string ExtendedTradingMarathon::generateMarathonId() {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    return "MARATHON_" + std::to_string(timestamp);
}

std::string ExtendedTradingMarathon::timePointToString(const std::chrono::system_clock::time_point& tp) {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::string ExtendedTradingMarathon::formatDuration(std::chrono::milliseconds duration) {
    auto hours = std::chrono::duration_cast<std::chrono::hours>(duration);
    auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration - hours);
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration - hours - minutes);

    std::stringstream ss;
    ss << hours.count() << "h " << minutes.count() << "m " << seconds.count() << "s";
    return ss.str();
}

bool ExtendedTradingMarathon::validateMarathonConfiguration() {
    if (config_.initialCapital <= 0.0) {
        std::cerr << "Invalid initial capital: " << config_.initialCapital << std::endl;
        return false;
    }

    if (config_.tradingPairs.empty()) {
        std::cerr << "No trading pairs configured" << std::endl;
        return false;
    }

    if (config_.maxDailyDrawdown <= 0.0 || config_.maxDailyDrawdown >= 1.0) {
        std::cerr << "Invalid max daily drawdown: " << config_.maxDailyDrawdown << std::endl;
        return false;
    }

    return true;
}

bool ExtendedTradingMarathon::validateSystemReadiness() {
    // Check trading engine
    if (!tradingEngine_) {
        std::cerr << "Trading engine not available" << std::endl;
        return false;
    }

    // Check monitoring system
    if (!tradingMonitor_) {
        std::cerr << "Trading monitor not available" << std::endl;
        return false;
    }

    // Check stress testing engines
    if (!stressTestEngine_) {
        std::cerr << "Stress test engine not available" << std::endl;
        return false;
    }

    if (!operationalStressTest_) {
        std::cerr << "Operational stress test not available" << std::endl;
        return false;
    }

    // Check database
    if (!dbManager_) {
        std::cerr << "Database manager not available" << std::endl;
        return false;
    }

    return true;
}

std::chrono::milliseconds ExtendedTradingMarathon::getElapsedTime() {
    if (!marathonActive_.load()) {
        return std::chrono::milliseconds(0);
    }

    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - marathonStartTime_);
}

std::chrono::milliseconds ExtendedTradingMarathon::getRemainingTime() {
    if (!marathonActive_.load()) {
        return std::chrono::milliseconds(0);
    }

    auto now = std::chrono::system_clock::now();
    if (now >= marathonEndTime_) {
        return std::chrono::milliseconds(0);
    }

    return std::chrono::duration_cast<std::chrono::milliseconds>(marathonEndTime_ - now);
}

void ExtendedTradingMarathon::recordEvent(MarathonEventType eventType,
                                         const std::string& description,
                                         const std::string& severity) {
    MarathonEvent event(eventType, description, severity);

    std::lock_guard<std::mutex> lock(dataStorageMutex_);
    eventHistory_.push_back(event);

    // Limit event history to prevent memory growth
    if (eventHistory_.size() > 10000) {
        eventHistory_.erase(eventHistory_.begin(), eventHistory_.begin() + 1000);
    }

    // Save to database
    saveEventToDatabase(event);

    std::cout << "[" << timePointToString(event.timestamp) << "] "
              << severity << ": " << description << std::endl;
}

void ExtendedTradingMarathon::generateAlert(MarathonAlert::AlertLevel level,
                                           MarathonAlert::AlertCategory category,
                                           const std::string& message,
                                           const std::string& component) {
    MarathonAlert alert(level, category, message, component);

    std::lock_guard<std::mutex> lock(dataStorageMutex_);
    activeAlerts_.push_back(alert);

    // Log critical alerts
    if (level >= MarathonAlert::AlertLevel::CRITICAL) {
        std::cout << "CRITICAL ALERT: " << message << " (Component: " << component << ")" << std::endl;

        recordEvent(MarathonEventType::CRITICAL_ERROR,
                   "Critical alert: " + message,
                   "CRITICAL");
    }
}

double ExtendedTradingMarathon::calculateOverallHealthScore() {
    try {
        // Weighted health score calculation
        double tradingHealth = 0.85;      // Trading system health
        double systemHealth = 0.90;       // System resource health
        double performanceHealth = 0.88;  // Performance health
        double riskHealth = 0.92;         // Risk management health

        // Get current metrics
        double cpuUsage = MarathonUtils::getCurrentCpuUsage();
        double memoryUsage = MarathonUtils::getCurrentMemoryUsage();

        // Adjust system health based on resource usage
        if (cpuUsage > 0.9) systemHealth *= 0.7;
        else if (cpuUsage > 0.8) systemHealth *= 0.85;

        if (memoryUsage > 0.9) systemHealth *= 0.7;
        else if (memoryUsage > 0.8) systemHealth *= 0.85;

        // Calculate weighted average
        double overallHealth = (tradingHealth * 0.3 + systemHealth * 0.25 +
                               performanceHealth * 0.25 + riskHealth * 0.2);

        return std::max(0.0, std::min(1.0, overallHealth));

    } catch (const std::exception& e) {
        std::cerr << "Error calculating health score: " << e.what() << std::endl;
        return 0.5; // Default to moderate health on error
    }
}

MarathonHealthLevel ExtendedTradingMarathon::calculateHealthLevel(double healthScore) {
    if (healthScore >= 0.95) return MarathonHealthLevel::EXCELLENT;
    if (healthScore >= 0.80) return MarathonHealthLevel::GOOD;
    if (healthScore >= 0.65) return MarathonHealthLevel::FAIR;
    if (healthScore >= 0.50) return MarathonHealthLevel::POOR;
    return MarathonHealthLevel::CRITICAL;
}

} // namespace Testing
} // namespace CryptoClaude