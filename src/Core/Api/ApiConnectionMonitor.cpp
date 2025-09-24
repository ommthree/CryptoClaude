#include "ApiConnectionMonitor.h"
#include <algorithm>
#include <numeric>
#include <random>
#include <sstream>
#include <iostream>
#include <iomanip>

namespace CryptoClaude {
namespace Api {

ApiConnectionMonitor::ApiConnectionMonitor(DatabaseManager& dbManager)
    : dbManager_(dbManager), isMonitoring_(false) {

    // Initialize default configuration
    config_.healthCheckInterval = std::chrono::seconds(60);
    config_.connectionTestInterval = std::chrono::seconds(300);
    config_.enableRealTimeDataValidation = true;
    config_.enableAutomatedRemediation = false;
}

ApiConnectionMonitor::~ApiConnectionMonitor() {
    shutdown();
}

bool ApiConnectionMonitor::initialize() {
    try {
        lastHealthCheck_ = std::chrono::system_clock::now();

        // Initialize database tables for monitoring metrics
        std::string createMetricsTable = R"(
            CREATE TABLE IF NOT EXISTS api_connection_metrics (
                provider_id TEXT,
                timestamp INTEGER,
                average_latency REAL,
                success_rate REAL,
                total_requests INTEGER,
                failed_requests INTEGER,
                health_level INTEGER,
                data_quality_score REAL,
                PRIMARY KEY (provider_id, timestamp)
            )
        )";

        std::string createAlertsTable = R"(
            CREATE TABLE IF NOT EXISTS api_health_alerts (
                alert_id TEXT PRIMARY KEY,
                provider_id TEXT,
                trigger_level INTEGER,
                message TEXT,
                triggered_at INTEGER,
                is_active INTEGER,
                requires_action INTEGER
            )
        )";

        if (!dbManager_.executeQuery(createMetricsTable) ||
            !dbManager_.executeQuery(createAlertsTable)) {
            std::cerr << "Failed to initialize monitoring database tables" << std::endl;
            return false;
        }

        std::cout << "ApiConnectionMonitor initialized successfully" << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "ApiConnectionMonitor initialization failed: " << e.what() << std::endl;
        return false;
    }
}

void ApiConnectionMonitor::shutdown() {
    stopRealTimeMonitoring();
}

bool ApiConnectionMonitor::registerProvider(const std::string& providerId,
                                          std::shared_ptr<CryptoCompareProvider> provider) {
    if (!provider) {
        std::cerr << "Cannot register null provider: " << providerId << std::endl;
        return false;
    }

    if (providers_.find(providerId) != providers_.end()) {
        std::cout << "Provider already registered, updating: " << providerId << std::endl;
    }

    providers_[providerId] = provider;
    metrics_[providerId] = ConnectionMetrics(providerId);

    std::cout << "Registered API provider: " << providerId << std::endl;
    return true;
}

bool ApiConnectionMonitor::unregisterProvider(const std::string& providerId) {
    auto it = providers_.find(providerId);
    if (it == providers_.end()) {
        return false;
    }

    providers_.erase(it);
    metrics_.erase(providerId);

    // Remove alerts for this provider
    activeAlerts_.erase(
        std::remove_if(activeAlerts_.begin(), activeAlerts_.end(),
            [&providerId](const HealthAlert& alert) {
                return alert.providerId == providerId;
            }),
        activeAlerts_.end());

    std::cout << "Unregistered API provider: " << providerId << std::endl;
    return true;
}

std::vector<std::string> ApiConnectionMonitor::getRegisteredProviders() const {
    std::vector<std::string> providerIds;
    for (const auto& pair : providers_) {
        providerIds.push_back(pair.first);
    }
    return providerIds;
}

void ApiConnectionMonitor::startRealTimeMonitoring() {
    if (isMonitoring_) {
        return;
    }

    isMonitoring_ = true;
    monitoringThread_ = std::make_unique<std::thread>(&ApiConnectionMonitor::monitoringLoop, this);

    std::cout << "Started real-time API connection monitoring" << std::endl;
}

void ApiConnectionMonitor::stopRealTimeMonitoring() {
    if (!isMonitoring_) {
        return;
    }

    isMonitoring_ = false;
    if (monitoringThread_ && monitoringThread_->joinable()) {
        monitoringThread_->join();
    }

    std::cout << "Stopped real-time API connection monitoring" << std::endl;
}

ApiConnectionMonitor::ConnectionTestResult
ApiConnectionMonitor::testConnection(const std::string& providerId) {
    ConnectionTestResult result;
    result.providerId = providerId;
    result.testedAt = std::chrono::system_clock::now();

    auto providerIt = providers_.find(providerId);
    if (providerIt == providers_.end()) {
        result.errorMessage = "Provider not found: " + providerId;
        return result;
    }

    auto provider = providerIt->second;

    try {
        // Test basic connectivity with latency measurement
        auto startTime = std::chrono::high_resolution_clock::now();

        auto response = provider->getCurrentPrice("BTC", "USD");

        auto endTime = std::chrono::high_resolution_clock::now();
        result.latency = std::chrono::duration<double, std::milli>(endTime - startTime).count();

        if (response.success) {
            result.connectionSuccessful = true;
            result.dataPointsReceived = static_cast<int>(response.data.size());

            // Validate data quality
            if (!response.data.empty()) {
                result.dataValid = validatePriceData(response.data[0]);
                result.dataQualityScore = result.dataValid ? 1.0 : 0.0;
            }
        } else {
            result.errorMessage = response.errorMessage;
        }

    } catch (const std::exception& e) {
        result.errorMessage = "Connection test failed: " + std::string(e.what());
    }

    return result;
}

std::map<std::string, ApiConnectionMonitor::ConnectionTestResult>
ApiConnectionMonitor::testAllConnections() {
    std::map<std::string, ConnectionTestResult> results;

    for (const auto& providerId : getRegisteredProviders()) {
        results[providerId] = testConnection(providerId);
    }

    return results;
}

void ApiConnectionMonitor::performHealthCheck() {
    lastHealthCheck_ = std::chrono::system_clock::now();

    for (const auto& providerId : getRegisteredProviders()) {
        updateConnectionMetrics(providerId);
        checkHealthThresholds(providerId);
    }

    std::cout << "Completed health check for " << providers_.size() << " providers" << std::endl;
}

ConnectionMetrics ApiConnectionMonitor::getConnectionMetrics(const std::string& providerId) const {
    auto it = metrics_.find(providerId);
    if (it != metrics_.end()) {
        return it->second;
    }
    return ConnectionMetrics(providerId);
}

std::map<std::string, ConnectionMetrics> ApiConnectionMonitor::getAllConnectionMetrics() const {
    return metrics_;
}

ConnectionHealth ApiConnectionMonitor::assessOverallHealth() const {
    if (metrics_.empty()) {
        return ConnectionHealth::CRITICAL;
    }

    std::vector<ConnectionHealth> healthLevels;
    for (const auto& pair : metrics_) {
        healthLevels.push_back(pair.second.overallHealth);
    }

    // Determine overall health based on worst provider health
    auto worstHealth = *std::max_element(healthLevels.begin(), healthLevels.end(),
        [](ConnectionHealth a, ConnectionHealth b) {
            return static_cast<int>(a) < static_cast<int>(b);
        });

    return worstHealth;
}

std::vector<HealthAlert> ApiConnectionMonitor::getActiveAlerts() const {
    std::vector<HealthAlert> activeOnly;
    std::copy_if(activeAlerts_.begin(), activeAlerts_.end(),
                std::back_inserter(activeOnly),
                [](const HealthAlert& alert) { return alert.isActive; });
    return activeOnly;
}

ApiConnectionMonitor::HealthReport ApiConnectionMonitor::generateHealthReport() const {
    HealthReport report;
    report.reportTime = std::chrono::system_clock::now();
    report.overallHealth = assessOverallHealth();
    report.totalProviders = static_cast<int>(providers_.size());

    // Aggregate metrics
    std::vector<double> latencies, successRates, dataQualities;
    int healthy = 0, warning = 0, critical = 0;

    for (const auto& pair : metrics_) {
        const auto& metrics = pair.second;
        report.providerDetails[pair.first] = metrics;

        latencies.push_back(metrics.averageLatency);
        successRates.push_back(metrics.successRate);

        // Calculate data quality score
        double dataQuality = metrics.totalDataPoints > 0 ?
            1.0 - (static_cast<double>(metrics.invalidDataPoints) / metrics.totalDataPoints) : 0.0;
        dataQualities.push_back(dataQuality);

        // Count health levels
        switch (metrics.overallHealth) {
            case ConnectionHealth::EXCELLENT:
            case ConnectionHealth::GOOD:
                healthy++;
                break;
            case ConnectionHealth::FAIR:
                warning++;
                break;
            case ConnectionHealth::POOR:
            case ConnectionHealth::CRITICAL:
                critical++;
                report.criticalIssues.push_back(
                    "Provider " + pair.first + ": " + metrics.healthReason);
                break;
        }
    }

    report.healthyProviders = healthy;
    report.warningProviders = warning;
    report.criticalProviders = critical;

    // Calculate averages
    if (!latencies.empty()) {
        report.averageLatency = std::accumulate(latencies.begin(), latencies.end(), 0.0) / latencies.size();
    }
    if (!successRates.empty()) {
        report.averageSuccessRate = std::accumulate(successRates.begin(), successRates.end(), 0.0) / successRates.size();
    }
    if (!dataQualities.empty()) {
        report.averageDataQuality = std::accumulate(dataQualities.begin(), dataQualities.end(), 0.0) / dataQualities.size();
    }

    // Generate recommendations
    if (report.averageLatency > config_.latencyWarningThreshold) {
        report.recommendations.push_back("Consider optimizing API request patterns or enabling connection pooling");
    }
    if (report.averageSuccessRate < config_.successRateWarningThreshold) {
        report.recommendations.push_back("Review API error handling and implement retry mechanisms");
    }
    if (report.averageDataQuality < config_.dataQualityWarningThreshold) {
        report.recommendations.push_back("Investigate data quality issues and enhance validation");
    }

    return report;
}

void ApiConnectionMonitor::monitoringLoop() {
    while (isMonitoring_) {
        try {
            performHealthCheck();

            // Wait for next monitoring interval
            std::this_thread::sleep_for(config_.healthCheckInterval);

        } catch (const std::exception& e) {
            std::cerr << "Monitoring loop error: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(10)); // Backoff on error
        }
    }
}

void ApiConnectionMonitor::updateConnectionMetrics(const std::string& providerId) {
    auto providerIt = providers_.find(providerId);
    if (providerIt == providers_.end()) {
        return;
    }

    auto provider = providerIt->second;
    auto& metrics = metrics_[providerId];

    try {
        // Get current provider statistics
        auto stats = provider->getStatistics();

        // Update basic metrics
        metrics.totalRequests = stats.totalRequests;
        metrics.successfulRequests = stats.successfulRequests;
        metrics.failedRequests = stats.failedRequests;
        metrics.successRate = stats.successRate;
        metrics.lastUpdateTime = std::chrono::system_clock::now();

        // Test current latency
        auto testResult = testConnection(providerId);
        if (testResult.connectionSuccessful) {
            metrics.averageLatency = testResult.latency;

            if (metrics.minLatency == 0.0 || testResult.latency < metrics.minLatency) {
                metrics.minLatency = testResult.latency;
            }
            if (testResult.latency > metrics.maxLatency) {
                metrics.maxLatency = testResult.latency;
            }
        } else {
            metrics.lastError = testResult.errorMessage;
            metrics.lastErrorTime = testResult.testedAt;
            metrics.recentErrors.push_back(testResult.errorMessage);

            // Keep only recent errors (last 10)
            if (metrics.recentErrors.size() > 10) {
                metrics.recentErrors.erase(metrics.recentErrors.begin());
            }
        }

        // Determine overall health
        metrics.overallHealth = determineHealthLevel(metrics);

        // Trigger callbacks if health changed
        if (healthChangeCallback_) {
            // Note: In a real implementation, we'd track previous health to detect changes
            healthChangeCallback_(providerId, metrics.overallHealth, metrics.overallHealth);
        }

        // Persist metrics
        persistMetrics(providerId, metrics);

    } catch (const std::exception& e) {
        std::cerr << "Error updating metrics for provider " << providerId << ": " << e.what() << std::endl;
    }
}

void ApiConnectionMonitor::checkHealthThresholds(const std::string& providerId) {
    auto& metrics = metrics_[providerId];

    // Check latency thresholds
    if (metrics.averageLatency > config_.latencyCriticalThreshold) {
        generateHealthAlert(providerId, ConnectionHealth::CRITICAL,
                          "High latency: " + std::to_string(metrics.averageLatency) + "ms");
    } else if (metrics.averageLatency > config_.latencyWarningThreshold) {
        generateHealthAlert(providerId, ConnectionHealth::POOR,
                          "Warning latency: " + std::to_string(metrics.averageLatency) + "ms");
    }

    // Check success rate thresholds
    if (metrics.successRate < config_.successRateCriticalThreshold) {
        generateHealthAlert(providerId, ConnectionHealth::CRITICAL,
                          "Low success rate: " + std::to_string(metrics.successRate * 100) + "%");
    } else if (metrics.successRate < config_.successRateWarningThreshold) {
        generateHealthAlert(providerId, ConnectionHealth::POOR,
                          "Warning success rate: " + std::to_string(metrics.successRate * 100) + "%");
    }
}

void ApiConnectionMonitor::generateHealthAlert(const std::string& providerId,
                                             ConnectionHealth health,
                                             const std::string& reason) {
    HealthAlert alert;
    alert.alertId = providerId + "_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    alert.providerId = providerId;
    alert.triggerLevel = health;
    alert.message = reason;
    alert.triggeredAt = std::chrono::system_clock::now();
    alert.requiresAction = (health == ConnectionHealth::CRITICAL);

    activeAlerts_.push_back(alert);

    if (alertCallback_) {
        alertCallback_(alert);
    }

    std::cout << "Generated health alert for " << providerId << ": " << reason << std::endl;
}

ConnectionHealth ApiConnectionMonitor::determineHealthLevel(const ConnectionMetrics& metrics) const {
    // Critical conditions
    if (metrics.successRate < 0.80 || metrics.averageLatency > 2000.0) {
        return ConnectionHealth::CRITICAL;
    }

    // Poor conditions
    if (metrics.successRate < 0.90 || metrics.averageLatency > 1000.0) {
        return ConnectionHealth::POOR;
    }

    // Fair conditions
    if (metrics.successRate < 0.95 || metrics.averageLatency > 500.0) {
        return ConnectionHealth::FAIR;
    }

    // Good conditions
    if (metrics.successRate < 1.0 || metrics.averageLatency > 100.0) {
        return ConnectionHealth::GOOD;
    }

    // Excellent conditions
    return ConnectionHealth::EXCELLENT;
}

bool ApiConnectionMonitor::validatePriceData(const MarketData& data) const {
    // Basic validation checks
    if (data.getPrice() <= 0.0) {
        return false;
    }

    // Check for reasonable price ranges (example for BTC: $1,000 - $1,000,000)
    if (data.getSymbol() == "BTC" && (data.getPrice() < 1000.0 || data.getPrice() > 1000000.0)) {
        return false;
    }

    // Validate timestamp is not too old or in the future
    auto now = std::chrono::system_clock::now();
    auto dataTime = data.getTimestamp();
    auto age = std::chrono::duration_cast<std::chrono::minutes>(now - dataTime);

    if (age > std::chrono::hours(1) || age < std::chrono::seconds(0)) {
        return false;
    }

    return true;
}

void ApiConnectionMonitor::persistMetrics(const std::string& providerId, const ConnectionMetrics& metrics) {
    try {
        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
            metrics.lastUpdateTime.time_since_epoch()).count();

        double dataQualityScore = metrics.totalDataPoints > 0 ?
            1.0 - (static_cast<double>(metrics.invalidDataPoints) / metrics.totalDataPoints) : 1.0;

        std::stringstream query;
        query << "INSERT OR REPLACE INTO api_connection_metrics "
              << "(provider_id, timestamp, average_latency, success_rate, total_requests, failed_requests, health_level, data_quality_score) "
              << "VALUES ('" << providerId << "', " << timestamp << ", "
              << metrics.averageLatency << ", " << metrics.successRate << ", "
              << metrics.totalRequests << ", " << metrics.failedRequests << ", "
              << static_cast<int>(metrics.overallHealth) << ", " << dataQualityScore << ")";

        if (!dbManager_.executeQuery(query.str())) {
            std::cerr << "Failed to persist metrics for provider: " << providerId << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "Error persisting metrics: " << e.what() << std::endl;
    }
}

// ApiConnectionTester implementation
ApiConnectionTester::ApiConnectionTester(std::shared_ptr<CryptoCompareProvider> provider)
    : provider_(provider) {}

ApiConnectionTester::ValidationTestSuite ApiConnectionTester::runConnectivityTests() {
    ValidationTestSuite suite;
    suite.suiteName = "Connectivity Tests";

    auto startTime = std::chrono::high_resolution_clock::now();

    // Test basic connectivity
    suite.tests.push_back([this]() { return testBasicConnectivity(); });

    // Test authentication
    suite.tests.push_back([this]() { return testAuthentication(); });

    // Test error handling
    suite.tests.push_back([this]() { return testErrorHandling(); });

    // Execute tests
    for (const auto& test : suite.tests) {
        try {
            if (test()) {
                suite.passedTests++;
            } else {
                suite.failureReasons.push_back("Connectivity test failed");
            }
        } catch (const std::exception& e) {
            suite.failureReasons.push_back("Test exception: " + std::string(e.what()));
        }
        suite.totalTests++;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    suite.executionTime = std::chrono::duration<double>(endTime - startTime).count();

    return suite;
}

bool ApiConnectionTester::testBasicConnectivity() {
    try {
        auto response = provider_->getCurrentPrice("BTC", "USD");
        return response.success && !response.data.empty();
    } catch (...) {
        return false;
    }
}

bool ApiConnectionTester::testAuthentication() {
    if (!provider_->isConfigured()) {
        return false; // No API key configured
    }

    try {
        auto response = provider_->getCurrentPrice("BTC", "USD");
        // If we get a response, authentication worked (or API doesn't require auth)
        return response.success || response.errorMessage.find("authentication") == std::string::npos;
    } catch (...) {
        return false;
    }
}

bool ApiConnectionTester::testErrorHandling() {
    try {
        // Test with invalid symbol to trigger error handling
        auto response = provider_->getCurrentPrice("INVALID_SYMBOL", "USD");
        // Error handling works if we get a proper error response instead of exception
        return !response.success && !response.errorMessage.empty();
    } catch (...) {
        return false; // Exception thrown instead of proper error handling
    }
}

} // namespace Api
} // namespace CryptoClaude