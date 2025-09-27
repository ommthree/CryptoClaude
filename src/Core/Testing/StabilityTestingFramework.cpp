#include "StabilityTestingFramework.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <random>
#include <algorithm>
#include <fstream>
#include "../Database/DatabaseManager.h"

namespace CryptoClaude {
namespace Testing {

StabilityTestingFramework::StabilityTestingFramework(const StabilityConfig& config)
    : config_(config), lastHealthCheck_(std::chrono::system_clock::now()) {

    // Initialize data manager with production API keys
    dataManager_ = std::make_unique<Data::LiveDataManager>(
        getenv("NEWS_API_KEY") ? getenv("NEWS_API_KEY") : "NEWS_API_KEY_NOT_SET",              // NewsAPI
        "L6Y7WECGSWZFHHGX",                              // Alpha Vantage
        getenv("CRYPTONEWS_API_KEY") ? getenv("CRYPTONEWS_API_KEY") : "CRYPTONEWS_KEY_NOT_SET"       // CryptoNews
    );

    // Initialize parameter manager
    paramManager_ = std::make_unique<Config::TunableParametersManager>("config/stability_test.json");

    // Initialize real correlation framework
    realCorrelationFramework_ = std::make_unique<RealCorrelationFramework>();

    // Initialize metrics
    overallMetrics_.startTime = std::chrono::system_clock::now();
    overallMetrics_.totalRequests = 0;
    overallMetrics_.failedRequests = 0;
}

StabilityTestingFramework::~StabilityTestingFramework() {
    stopStabilityTest();
}

bool StabilityTestingFramework::startStabilityTest() {
    if (testRunning_.load()) {
        return false; // Already running
    }

    testRunning_.store(true);
    stopRequested_.store(false);
    overallMetrics_.startTime = std::chrono::system_clock::now();

    std::cout << "🚀 Starting 72-Hour Stability Test..." << std::endl;
    std::cout << "Duration: " << config_.testDuration.count() << " hours" << std::endl;
    std::cout << "Health check interval: " << config_.healthCheckInterval.count() << " seconds" << std::endl;

    // Start monitoring threads
    healthMonitorThread_ = std::make_unique<std::thread>(&StabilityTestingFramework::healthMonitorLoop, this);
    performanceMonitorThread_ = std::make_unique<std::thread>(&StabilityTestingFramework::performanceMonitorLoop, this);
    correlationMonitorThread_ = std::make_unique<std::thread>(&StabilityTestingFramework::correlationMonitorLoop, this);

    return true;
}

void StabilityTestingFramework::stopStabilityTest() {
    if (!testRunning_.load()) {
        return;
    }

    stopRequested_.store(true);
    testRunning_.store(false);

    // Wait for monitoring threads to finish
    if (healthMonitorThread_ && healthMonitorThread_->joinable()) {
        healthMonitorThread_->join();
    }
    if (performanceMonitorThread_ && performanceMonitorThread_->joinable()) {
        performanceMonitorThread_->join();
    }
    if (correlationMonitorThread_ && correlationMonitorThread_->joinable()) {
        correlationMonitorThread_->join();
    }

    overallMetrics_.endTime = std::chrono::system_clock::now();
    calculateOverallMetrics();

    std::cout << "🛑 Stability test stopped." << std::endl;
}

bool StabilityTestingFramework::testApiConnectivity() {
    auto startTime = std::chrono::high_resolution_clock::now();
    TestResult result;
    result.testName = "API Connectivity Test";
    result.timestamp = std::chrono::system_clock::now();

    try {
        bool allConnected = dataManager_->testAllConnections();
        auto healthyProviders = dataManager_->getHealthyProviders();

        result.metrics["healthy_providers"] = static_cast<double>(healthyProviders.size());
        result.metrics["total_providers"] = 4.0;
        result.metrics["success_rate"] = static_cast<double>(healthyProviders.size()) / 4.0;

        result.passed = (healthyProviders.size() >= 3); // At least 3/4 working for redundancy
        if (!result.passed) {
            result.errorMessage = "Insufficient API connectivity: " +
                                std::to_string(healthyProviders.size()) + "/4 providers healthy";
        }

    } catch (const std::exception& e) {
        result.passed = false;
        result.errorMessage = "API connectivity test failed: " + std::string(e.what());
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    result.executionTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    recordTestResult(result);
    return result.passed;
}

bool StabilityTestingFramework::testDataRetrieval() {
    auto startTime = std::chrono::high_resolution_clock::now();
    TestResult result;
    result.testName = "Data Retrieval Test";
    result.timestamp = std::chrono::system_clock::now();

    try {
        std::vector<std::string> testSymbols = {"BTC", "ETH", "ADA", "DOT", "LINK"};

        // Test price data retrieval
        auto priceData = dataManager_->getCurrentPrices(testSymbols);
        result.metrics["price_points_retrieved"] = static_cast<double>(priceData.size());

        // Test news data retrieval
        auto newsData = dataManager_->getLatestNews(testSymbols, 5);
        result.metrics["news_articles_retrieved"] = static_cast<double>(newsData.size());

        result.passed = (priceData.size() > 0 && newsData.size() > 0);
        if (!result.passed) {
            result.errorMessage = "Data retrieval failed: " +
                                std::to_string(priceData.size()) + " price points, " +
                                std::to_string(newsData.size()) + " news articles";
        }

    } catch (const std::exception& e) {
        result.passed = false;
        result.errorMessage = "Data retrieval test failed: " + std::string(e.what());
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    result.executionTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    recordTestResult(result);
    updatePerformanceMetrics("DataRetrieval", result.executionTimeMs, result.passed);
    return result.passed;
}

bool StabilityTestingFramework::testParameterManagement() {
    auto startTime = std::chrono::high_resolution_clock::now();
    TestResult result;
    result.testName = "Parameter Management Test";
    result.timestamp = std::chrono::system_clock::now();

    try {
        // Test parameter reading
        double correlationThreshold = paramManager_->getDouble("algo.correlation_threshold");
        int maxPairs = paramManager_->getInt("algo.max_pairs");

        result.metrics["correlation_threshold"] = correlationThreshold;
        result.metrics["max_pairs"] = static_cast<double>(maxPairs);

        // Test parameter modification
        double originalValue = correlationThreshold;
        bool setSuccess = paramManager_->setDouble("algo.correlation_threshold", 0.88);
        double newValue = paramManager_->getDouble("algo.correlation_threshold");

        // Restore original value
        paramManager_->setDouble("algo.correlation_threshold", originalValue);

        result.passed = (setSuccess && newValue == 0.88 && correlationThreshold >= 0.85);
        if (!result.passed) {
            result.errorMessage = "Parameter management failed: setSuccess=" + std::to_string(setSuccess) +
                                ", correlation=" + std::to_string(correlationThreshold);
        }

    } catch (const std::exception& e) {
        result.passed = false;
        result.errorMessage = "Parameter management test failed: " + std::string(e.what());
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    result.executionTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    recordTestResult(result);
    return result.passed;
}

bool StabilityTestingFramework::testCorrelationCalculation() {
    auto startTime = std::chrono::high_resolution_clock::now();
    TestResult result;
    result.testName = "Correlation Calculation Test";
    result.timestamp = std::chrono::system_clock::now();

    try {
        double correlationThreshold = paramManager_->getDouble("algo.correlation_threshold");
        // Use REAL correlation calculation instead of simulated
        double actualCorrelation = calculateRealCorrelationAccuracy();

        result.metrics["correlation_threshold"] = correlationThreshold;
        result.metrics["actual_correlation"] = actualCorrelation;
        result.metrics["correlation_meets_threshold"] = (actualCorrelation >= 0.85) ? 1.0 : 0.0; // Use TRS requirement

        result.passed = (actualCorrelation >= 0.85); // TRS requirement: >85%
        if (!result.passed) {
            result.errorMessage = "Correlation below TRS threshold: " + std::to_string(actualCorrelation) +
                                " < 0.85 (TRS requirement)";
        }

    } catch (const std::exception& e) {
        result.passed = false;
        result.errorMessage = "Correlation calculation test failed: " + std::string(e.what());
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    result.executionTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    recordTestResult(result);
    return result.passed;
}

bool StabilityTestingFramework::testErrorRecovery() {
    auto startTime = std::chrono::high_resolution_clock::now();
    TestResult result;
    result.testName = "Error Recovery Test";
    result.timestamp = std::chrono::system_clock::now();

    try {
        // Simulate provider failure
        dataManager_->enableProvider("NewsAPI", false);
        bool systemHealthyAfterFailure = dataManager_->isHealthy();

        // Test recovery
        dataManager_->enableProvider("NewsAPI", true);
        std::this_thread::sleep_for(std::chrono::seconds(2)); // Allow recovery time
        bool systemHealthyAfterRecovery = dataManager_->isHealthy();

        result.metrics["healthy_after_failure"] = systemHealthyAfterFailure ? 1.0 : 0.0;
        result.metrics["healthy_after_recovery"] = systemHealthyAfterRecovery ? 1.0 : 0.0;

        result.passed = (systemHealthyAfterFailure && systemHealthyAfterRecovery);
        if (!result.passed) {
            result.errorMessage = "Error recovery failed: healthy_after_failure=" +
                                std::to_string(systemHealthyAfterFailure) +
                                ", healthy_after_recovery=" + std::to_string(systemHealthyAfterRecovery);
        }

    } catch (const std::exception& e) {
        result.passed = false;
        result.errorMessage = "Error recovery test failed: " + std::string(e.what());
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    result.executionTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    recordTestResult(result);
    return result.passed;
}

bool StabilityTestingFramework::testPerformanceUnderLoad() {
    auto startTime = std::chrono::high_resolution_clock::now();
    TestResult result;
    result.testName = "Performance Under Load Test";
    result.timestamp = std::chrono::system_clock::now();

    try {
        std::vector<std::string> testSymbols = {"BTC", "ETH", "ADA", "DOT", "LINK"};
        std::vector<double> responseTimes;

        // Perform multiple rapid requests
        for (int i = 0; i < 10; ++i) {
            auto requestStart = std::chrono::high_resolution_clock::now();
            auto priceData = dataManager_->getCurrentPrices(testSymbols);
            auto requestEnd = std::chrono::high_resolution_clock::now();

            double responseTimeMs = std::chrono::duration<double, std::milli>(requestEnd - requestStart).count();
            responseTimes.push_back(responseTimeMs);
        }

        // Calculate performance metrics
        double avgResponseTime = std::accumulate(responseTimes.begin(), responseTimes.end(), 0.0) / responseTimes.size();
        double maxResponseTime = *std::max_element(responseTimes.begin(), responseTimes.end());

        result.metrics["avg_response_time_ms"] = avgResponseTime;
        result.metrics["max_response_time_ms"] = maxResponseTime;
        result.metrics["requests_completed"] = static_cast<double>(responseTimes.size());

        result.passed = (avgResponseTime < config_.maxResponseTimeMs && maxResponseTime < config_.maxResponseTimeMs * 2);
        if (!result.passed) {
            result.errorMessage = "Performance under load failed: avg=" + std::to_string(avgResponseTime) +
                                "ms, max=" + std::to_string(maxResponseTime) + "ms";
        }

    } catch (const std::exception& e) {
        result.passed = false;
        result.errorMessage = "Performance under load test failed: " + std::string(e.what());
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    result.executionTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    recordTestResult(result);
    return result.passed;
}

void StabilityTestingFramework::healthMonitorLoop() {
    while (testRunning_.load() && !stopRequested_.load()) {
        try {
            bool apiConnectivity = testApiConnectivity();
            bool dataRetrieval = testDataRetrieval();

            if (apiConnectivity && dataRetrieval) {
                setHealthStatus(HealthStatus::HEALTHY);
            } else if (apiConnectivity || dataRetrieval) {
                setHealthStatus(HealthStatus::DEGRADED, "Partial system failure detected");
            } else {
                setHealthStatus(HealthStatus::CRITICAL, "Multiple system failures detected");
                if (config_.enableAutoRecovery) {
                    attemptRecovery();
                }
            }

            lastHealthCheck_ = std::chrono::system_clock::now();

        } catch (const std::exception& e) {
            setHealthStatus(HealthStatus::CRITICAL, "Health monitor exception: " + std::string(e.what()));
        }

        std::this_thread::sleep_for(config_.healthCheckInterval);
    }
}

void StabilityTestingFramework::performanceMonitorLoop() {
    while (testRunning_.load() && !stopRequested_.load()) {
        try {
            testPerformanceUnderLoad();
        } catch (const std::exception& e) {
            std::cerr << "Performance monitor error: " << e.what() << std::endl;
        }

        std::this_thread::sleep_for(config_.performanceCheckInterval);
    }
}

void StabilityTestingFramework::correlationMonitorLoop() {
    while (testRunning_.load() && !stopRequested_.load()) {
        try {
            testCorrelationCalculation();
        } catch (const std::exception& e) {
            std::cerr << "Correlation monitor error: " << e.what() << std::endl;
        }

        std::this_thread::sleep_for(config_.correlationCheckInterval);
    }
}

void StabilityTestingFramework::recordTestResult(const TestResult& result) {
    testResults_.push_back(result);

    // Update overall metrics
    overallMetrics_.totalRequests++;
    if (!result.passed) {
        overallMetrics_.failedRequests++;
    }

    // Print result
    auto timestamp_t = std::chrono::system_clock::to_time_t(result.timestamp);
    std::cout << "[" << std::put_time(std::localtime(&timestamp_t), "%H:%M:%S") << "] "
              << result.testName << ": " << (result.passed ? "✅ PASS" : "❌ FAIL")
              << " (" << std::fixed << std::setprecision(1) << result.executionTimeMs << "ms)";

    if (!result.passed) {
        std::cout << " - " << result.errorMessage;
    }
    std::cout << std::endl;
}

double StabilityTestingFramework::calculateCorrelationAccuracy() {
    // DEPRECATED: This method uses simulated correlation values
    std::cout << "⚠️  WARNING: Using simulated correlation (DEPRECATED)" << std::endl;
    std::cout << "   Use calculateRealCorrelationAccuracy() for production" << std::endl;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<> dist(0.87, 0.02); // Mean 0.87, std dev 0.02

    return std::max(0.80, std::min(0.95, dist(gen)));
}

double StabilityTestingFramework::calculateRealCorrelationAccuracy() {
    // NEW: Real correlation calculation using actual algorithm and historical data
    try {
        if (!realCorrelationFramework_) {
            throw std::runtime_error("Real correlation framework not initialized");
        }

        std::cout << "🔍 Calculating real correlation using historical data..." << std::endl;

        auto result = realCorrelationFramework_->calculateRealCorrelation();

        std::cout << "📊 Real Correlation Results:" << std::endl;
        std::cout << "   Pearson Correlation: " << std::fixed << std::setprecision(4) << result.pearsonCorrelation << std::endl;
        std::cout << "   Sample Size: " << result.sampleSize << std::endl;
        std::cout << "   P-Value: " << result.pValue << std::endl;
        std::cout << "   Statistically Significant: " << (result.isStatisticallySignificant ? "YES" : "NO") << std::endl;

        // Check if meets regulatory requirements
        bool passesRegulatory = realCorrelationFramework_->passesRegulatoryRequirements(result);

        if (!passesRegulatory) {
            std::cout << "❌ REGULATORY COMPLIANCE: FAIL" << std::endl;
            std::cout << "   Required correlation: ≥0.85, Achieved: " << result.pearsonCorrelation << std::endl;
            std::cout << "   Required significance: p<0.05, Achieved: " << result.pValue << std::endl;
        } else {
            std::cout << "✅ REGULATORY COMPLIANCE: PASS" << std::endl;
        }

        return result.pearsonCorrelation;

    } catch (const std::exception& e) {
        std::cout << "❌ ERROR: Real correlation calculation failed: " << e.what() << std::endl;
        std::cout << "   This indicates missing historical data or algorithm implementation" << std::endl;
        std::cout << "   Falling back to simulated value with clear warning" << std::endl;

        // Return 0.0 to indicate failure rather than fake value
        return 0.0;
    }
}

std::string StabilityTestingFramework::getHealthReport() const {
    std::ostringstream report;
    report << "=== Stability Testing Health Report ===" << std::endl;
    report << "Current Status: ";

    switch (currentHealth_.load()) {
        case HealthStatus::HEALTHY:
            report << "✅ HEALTHY";
            break;
        case HealthStatus::DEGRADED:
            report << "⚠️ DEGRADED";
            break;
        case HealthStatus::CRITICAL:
            report << "❌ CRITICAL";
            break;
        case HealthStatus::OFFLINE:
            report << "🔴 OFFLINE";
            break;
    }

    report << std::endl;
    auto healthcheck_t = std::chrono::system_clock::to_time_t(lastHealthCheck_);
    report << "Last Health Check: " << std::put_time(std::localtime(&healthcheck_t), "%Y-%m-%d %H:%M:%S") << std::endl;

    if (!lastHealthError_.empty()) {
        report << "Last Error: " << lastHealthError_ << std::endl;
    }

    report << "Total Tests: " << testResults_.size() << std::endl;
    report << "Failed Tests: " << overallMetrics_.failedRequests << std::endl;
    report << "Success Rate: " << std::fixed << std::setprecision(1)
           << (100.0 * (overallMetrics_.totalRequests - overallMetrics_.failedRequests) / std::max(1, overallMetrics_.totalRequests))
           << "%" << std::endl;

    return report.str();
}

double StabilityTestingFramework::getTestProgress() const {
    if (!testRunning_.load()) {
        return overallMetrics_.endTime > overallMetrics_.startTime ? 100.0 : 0.0;
    }

    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - overallMetrics_.startTime);
    auto totalDuration = std::chrono::duration_cast<std::chrono::seconds>(config_.testDuration);

    return std::min(100.0, (100.0 * elapsed.count()) / totalDuration.count());
}

// StabilityTestRunner Implementation
StabilityTestRunner::StabilityTestRunner() {
    framework_ = std::make_unique<StabilityTestingFramework>();
}

bool StabilityTestRunner::executeQuickStabilityTest(std::chrono::minutes duration) {
    std::cout << "🚀 Starting Quick Stability Test (" << duration.count() << " minutes)..." << std::endl;

    StabilityConfig quickConfig;
    quickConfig.testDuration = std::chrono::duration_cast<std::chrono::hours>(duration);
    quickConfig.healthCheckInterval = std::chrono::seconds(30);   // More frequent for quick test
    quickConfig.performanceCheckInterval = std::chrono::seconds(15);
    quickConfig.correlationCheckInterval = std::chrono::seconds(60);

    framework_->setConfig(quickConfig);
    startTime_ = std::chrono::system_clock::now();

    if (!framework_->startStabilityTest()) {
        std::cout << "❌ Failed to start stability test" << std::endl;
        return false;
    }

    // Monitor progress
    auto endTime = startTime_ + duration;
    while (std::chrono::system_clock::now() < endTime && framework_->isTestRunning()) {
        std::this_thread::sleep_for(std::chrono::seconds(30));
        printProgressReport();
    }

    framework_->stopStabilityTest();
    testCompleted_ = true;

    std::cout << std::endl << "🏁 Quick stability test completed!" << std::endl;
    std::cout << getFinalReport() << std::endl;

    return wasTestSuccessful();
}

void StabilityTestRunner::printProgressReport() const {
    if (!framework_) return;

    double progress = framework_->getTestProgress();
    auto health = framework_->getCurrentHealth();

    std::cout << "\r[" << std::fixed << std::setprecision(1) << progress << "%] ";

    switch (health) {
        case HealthStatus::HEALTHY:
            std::cout << "✅ HEALTHY";
            break;
        case HealthStatus::DEGRADED:
            std::cout << "⚠️ DEGRADED";
            break;
        case HealthStatus::CRITICAL:
            std::cout << "❌ CRITICAL";
            break;
        case HealthStatus::OFFLINE:
            std::cout << "🔴 OFFLINE";
            break;
    }

    std::cout << " - Tests: " << framework_->getTestResults().size();
    std::cout.flush();
}

bool StabilityTestRunner::wasTestSuccessful() const {
    if (!framework_ || !testCompleted_) {
        return false;
    }

    auto metrics = framework_->getOverallMetrics();
    auto health = framework_->getCurrentHealth();

    return (health == HealthStatus::HEALTHY || health == HealthStatus::DEGRADED) &&
           metrics.successRate >= 0.95;  // 95% minimum success rate
}

std::string StabilityTestRunner::getFinalReport() const {
    if (!framework_) {
        return "No framework available for reporting";
    }

    std::ostringstream report;
    report << "=== STABILITY TEST FINAL REPORT ===" << std::endl;

    auto metrics = framework_->getOverallMetrics();
    auto results = framework_->getTestResults();

    report << "Test Duration: " << std::chrono::duration_cast<std::chrono::minutes>(
        metrics.endTime - metrics.startTime).count() << " minutes" << std::endl;
    report << "Total Tests: " << results.size() << std::endl;
    report << "Passed Tests: " << (results.size() - metrics.failedRequests) << std::endl;
    report << "Failed Tests: " << metrics.failedRequests << std::endl;
    report << "Success Rate: " << std::fixed << std::setprecision(1)
           << (100.0 * (results.size() - metrics.failedRequests) / std::max(1, (int)results.size())) << "%" << std::endl;

    report << "Final Status: " << (wasTestSuccessful() ? "✅ SUCCESS" : "❌ FAILED") << std::endl;

    return report.str();
}

void StabilityTestingFramework::setHealthStatus(HealthStatus status, const std::string& error) {
    currentHealth_.store(status);
    if (!error.empty()) {
        lastHealthError_ = error;
    }
}

bool StabilityTestingFramework::attemptRecovery() {
    std::cout << "⚠️  Attempting system recovery..." << std::endl;

    // Perform health check on all providers
    dataManager_->performHealthCheck();

    // Wait for recovery
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Check if recovery was successful
    return dataManager_->isHealthy();
}

void StabilityTestingFramework::calculateOverallMetrics() {
    if (overallMetrics_.totalRequests > 0) {
        overallMetrics_.successRate = static_cast<double>(overallMetrics_.totalRequests - overallMetrics_.failedRequests) /
                                     overallMetrics_.totalRequests;
    } else {
        overallMetrics_.successRate = 1.0;
    }

    // Calculate uptime
    if (overallMetrics_.endTime > overallMetrics_.startTime) {
        auto totalDuration = std::chrono::duration_cast<std::chrono::seconds>(overallMetrics_.endTime - overallMetrics_.startTime);
        overallMetrics_.uptimePercentage = overallMetrics_.successRate; // Simplified calculation
    }
}

void StabilityTestingFramework::updatePerformanceMetrics(const std::string& provider, double responseTime, bool success) {
    // Update provider-specific metrics
    auto& metrics = providerMetrics_[provider];

    if (metrics.totalRequests == 0) {
        metrics.minResponseTimeMs = responseTime;
        metrics.maxResponseTimeMs = responseTime;
        metrics.avgResponseTimeMs = responseTime;
    } else {
        metrics.minResponseTimeMs = std::min(metrics.minResponseTimeMs, responseTime);
        metrics.maxResponseTimeMs = std::max(metrics.maxResponseTimeMs, responseTime);

        // Update average
        metrics.avgResponseTimeMs = (metrics.avgResponseTimeMs * metrics.totalRequests + responseTime) /
                                   (metrics.totalRequests + 1);
    }

    metrics.totalRequests++;
    if (!success) {
        metrics.failedRequests++;
    }

    metrics.successRate = static_cast<double>(metrics.totalRequests - metrics.failedRequests) / metrics.totalRequests;
}

} // namespace Testing
} // namespace CryptoClaude