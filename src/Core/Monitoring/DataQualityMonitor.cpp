#include "DataQualityMonitor.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <random>
#include <iomanip>

namespace CryptoClaude {
namespace Monitoring {

// ================================
// Alert Handler Implementations
// ================================

void ConsoleAlertHandler::handleAlert(const DataQualityAlert& alert) {
    std::cout << "\n🚨 DATA QUALITY ALERT 🚨" << std::endl;
    std::cout << "ID: " << alert.id << std::endl;
    std::cout << "Type: " << alert.getTypeString() << std::endl;
    std::cout << "Severity: " << alert.getSeverityString() << std::endl;
    std::cout << "Title: " << alert.title << std::endl;
    std::cout << "Description: " << alert.description << std::endl;
    std::cout << "Source: " << alert.source << std::endl;

    auto time_t = std::chrono::system_clock::to_time_t(alert.timestamp);
    std::cout << "Timestamp: " << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << std::endl;

    if (!alert.metadata.empty()) {
        std::cout << "Additional Info:" << std::endl;
        for (const auto& [key, value] : alert.metadata) {
            std::cout << "  " << key << ": " << value << std::endl;
        }
    }
    std::cout << "================================" << std::endl;
}

void EmailAlertHandler::handleAlert(const DataQualityAlert& alert) {
    // Mock email implementation - in production would use actual SMTP
    std::cout << "[EmailHandler] Sending alert to " << recipients_.size() << " recipients" << std::endl;
    std::cout << "Subject: [CryptoClaude] " << alert.getSeverityString()
              << " Alert: " << alert.title << std::endl;

    // Simulate email sending
    for (const auto& recipient : recipients_) {
        std::cout << "  -> " << recipient << " (via " << smtpServer_ << ")" << std::endl;
    }
}

void LogFileAlertHandler::handleAlert(const DataQualityAlert& alert) {
    std::lock_guard<std::mutex> lock(fileMutex_);

    try {
        std::ofstream logFile(logFilePath_, std::ios::app);
        if (logFile.is_open()) {
            auto time_t = std::chrono::system_clock::to_time_t(alert.timestamp);
            logFile << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S")
                   << " [" << alert.getSeverityString() << "] "
                   << "[" << alert.getTypeString() << "] "
                   << alert.title << " - " << alert.description
                   << " (Source: " << alert.source << ")" << std::endl;

            for (const auto& [key, value] : alert.metadata) {
                logFile << "    " << key << "=" << value << std::endl;
            }
            logFile << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to write alert to log file: " << e.what() << std::endl;
    }
}

// ================================
// DataQualityMonitor Implementation
// ================================

DataQualityMonitor::DataQualityMonitor(std::shared_ptr<DataProcessingService> dataService,
                                     const QualityThresholds& thresholds)
    : dataService_(dataService), thresholds_(thresholds), isMonitoring_(false),
      monitoringInterval_(std::chrono::seconds(60)), totalAlerts_(0), criticalAlerts_(0) {

    if (!dataService_) {
        throw std::invalid_argument("DataProcessingService cannot be null");
    }

    lastAssessment_ = std::chrono::system_clock::now();
}

DataQualityMonitor::~DataQualityMonitor() {
    stopMonitoring();
}

void DataQualityMonitor::startMonitoring(std::chrono::seconds interval) {
    if (isMonitoring_) {
        return; // Already monitoring
    }

    monitoringInterval_ = interval;
    isMonitoring_ = true;

    monitoringThread_ = std::thread(&DataQualityMonitor::monitoringLoop, this);

    generateAlert(AlertType::SYSTEM_HEALTH, AlertSeverity::INFO,
                 "Data Quality Monitoring Started",
                 "Data quality monitoring has been activated with " + std::to_string(interval.count()) + "s interval",
                 "DataQualityMonitor");
}

void DataQualityMonitor::stopMonitoring() {
    if (!isMonitoring_) {
        return;
    }

    isMonitoring_ = false;

    if (monitoringThread_.joinable()) {
        monitoringThread_.join();
    }

    generateAlert(AlertType::SYSTEM_HEALTH, AlertSeverity::INFO,
                 "Data Quality Monitoring Stopped",
                 "Data quality monitoring has been deactivated",
                 "DataQualityMonitor");
}

void DataQualityMonitor::addAlertHandler(std::shared_ptr<IAlertHandler> handler) {
    if (handler) {
        alertHandlers_.push_back(handler);
        std::cout << "Added alert handler: " << handler->getHandlerName() << std::endl;
    }
}

void DataQualityMonitor::removeAlertHandler(const std::string& handlerName) {
    auto it = std::remove_if(alertHandlers_.begin(), alertHandlers_.end(),
        [&handlerName](const std::weak_ptr<IAlertHandler>& handler) {
            auto locked = handler.lock();
            return !locked || locked->getHandlerName() == handlerName;
        });
    alertHandlers_.erase(it, alertHandlers_.end());
}

std::vector<std::string> DataQualityMonitor::getAlertHandlers() const {
    std::vector<std::string> names;
    for (const auto& handler : alertHandlers_) {
        if (handler) {
            names.push_back(handler->getHandlerName());
        }
    }
    return names;
}

void DataQualityMonitor::performQualityCheck() {
    try {
        checkDataQuality();
        checkDataFreshness();
        checkSystemHealth();

        lastAssessment_ = std::chrono::system_clock::now();

    } catch (const std::exception& e) {
        generateAlert(AlertType::SYSTEM_HEALTH, AlertSeverity::ERROR,
                     "Quality Check Failed",
                     "Manual quality check failed: " + std::string(e.what()),
                     "DataQualityMonitor");
    }
}

DataQualityMetrics DataQualityMonitor::assessMarketDataQuality(const std::vector<MarketData>& data) {
    DataQualityMetrics metrics;

    if (data.empty()) {
        metrics.overallScore = 0.0;
        metrics.issues.push_back("No market data available");
        return metrics;
    }

    // Calculate completeness (non-null data points)
    int validCount = 0;
    int totalExpected = data.size() * 5; // 5 main fields per record

    for (const auto& record : data) {
        if (!record.getTicker().empty()) ++validCount;
        if (record.getPrice() > 0) ++validCount;
        if (record.getVolume() >= 0) ++validCount;
        if (record.getHigh() >= record.getLow()) ++validCount;
        if (record.getTimestamp() != std::chrono::system_clock::time_point{}) ++validCount;
    }

    metrics.completeness = static_cast<double>(validCount) / totalExpected;

    // Calculate accuracy (reasonable values)
    int accurateCount = 0;
    for (const auto& record : data) {
        bool accurate = true;
        if (record.getPrice() <= 0 || record.getPrice() > 1000000) accurate = false; // Reasonable price range
        if (record.getVolume() < 0) accurate = false;
        if (record.getHigh() < record.getLow()) accurate = false;

        if (accurate) ++accurateCount;
    }

    metrics.accuracy = static_cast<double>(accurateCount) / data.size();

    // Calculate timeliness (recent data)
    auto now = std::chrono::system_clock::now();
    int freshCount = 0;
    for (const auto& record : data) {
        auto age = std::chrono::duration_cast<std::chrono::hours>(now - record.getTimestamp());
        if (age <= std::chrono::hours(24)) ++freshCount; // Data less than 24 hours old
    }

    metrics.timeliness = data.empty() ? 0.0 : static_cast<double>(freshCount) / data.size();

    // Calculate consistency (no duplicate timestamps)
    std::set<std::chrono::system_clock::time_point> timestamps;
    for (const auto& record : data) {
        timestamps.insert(record.getTimestamp());
    }
    metrics.consistency = data.empty() ? 1.0 : static_cast<double>(timestamps.size()) / data.size();

    // Calculate uniqueness (no exact duplicates)
    std::set<std::string> uniqueRecords;
    for (const auto& record : data) {
        std::string key = record.getTicker() + "_" + std::to_string(record.getPrice());
        uniqueRecords.insert(key);
    }
    metrics.uniqueness = data.empty() ? 1.0 : static_cast<double>(uniqueRecords.size()) / data.size();

    // Calculate overall score
    metrics.overallScore = (metrics.completeness + metrics.accuracy + metrics.timeliness +
                           metrics.consistency + metrics.uniqueness) / 5.0;

    // Identify issues
    if (metrics.completeness < thresholds_.minCompleteness) {
        metrics.issues.push_back("Data completeness below threshold: " +
                               std::to_string(metrics.completeness * 100) + "%");
    }
    if (metrics.accuracy < thresholds_.minAccuracy) {
        metrics.issues.push_back("Data accuracy below threshold: " +
                               std::to_string(metrics.accuracy * 100) + "%");
    }
    if (metrics.timeliness < thresholds_.minTimeliness) {
        metrics.issues.push_back("Data freshness below threshold: " +
                               std::to_string(metrics.timeliness * 100) + "%");
    }

    return metrics;
}

DataQualityMetrics DataQualityMonitor::assessSentimentDataQuality(const std::vector<SentimentData>& data) {
    DataQualityMetrics metrics;

    if (data.empty()) {
        metrics.overallScore = 0.0;
        metrics.issues.push_back("No sentiment data available");
        return metrics;
    }

    // Calculate completeness
    int validCount = 0;
    int totalExpected = data.size() * 3; // 3 main fields per record

    for (const auto& record : data) {
        if (!record.getTicker().empty()) ++validCount;
        if (record.getAvgSentiment() >= -1.0 && record.getAvgSentiment() <= 1.0) ++validCount;
        if (record.getArticleCount() >= 0) ++validCount;
    }

    metrics.completeness = static_cast<double>(validCount) / totalExpected;

    // Calculate accuracy (sentiment in valid range)
    int accurateCount = 0;
    for (const auto& record : data) {
        bool accurate = true;
        if (record.getAvgSentiment() < -1.0 || record.getAvgSentiment() > 1.0) accurate = false;
        if (record.getArticleCount() < 0) accurate = false;

        if (accurate) ++accurateCount;
    }

    metrics.accuracy = static_cast<double>(accurateCount) / data.size();

    // Calculate timeliness
    auto now = std::chrono::system_clock::now();
    int freshCount = 0;
    for (const auto& record : data) {
        auto age = std::chrono::duration_cast<std::chrono::hours>(now - record.getTimestamp());
        if (age <= std::chrono::hours(12)) ++freshCount; // Sentiment data should be more recent
    }

    metrics.timeliness = data.empty() ? 0.0 : static_cast<double>(freshCount) / data.size();

    // Calculate consistency and uniqueness
    std::set<std::string> uniqueRecords;
    for (const auto& record : data) {
        std::string key = record.getTicker() + "_" + std::to_string(record.getAvgSentiment());
        uniqueRecords.insert(key);
    }
    metrics.consistency = 1.0; // Simplified for sentiment data
    metrics.uniqueness = data.empty() ? 1.0 : static_cast<double>(uniqueRecords.size()) / data.size();

    // Calculate overall score
    metrics.overallScore = (metrics.completeness + metrics.accuracy + metrics.timeliness +
                           metrics.consistency + metrics.uniqueness) / 5.0;

    return metrics;
}

void DataQualityMonitor::acknowledgeAlert(const std::string& alertId, const std::string& acknowledgedBy) {
    std::lock_guard<std::mutex> lock(alertsMutex_);

    auto it = std::find_if(activeAlerts_.begin(), activeAlerts_.end(),
        [&alertId](const DataQualityAlert& alert) { return alert.id == alertId; });

    if (it != activeAlerts_.end()) {
        it->acknowledged = true;
        it->acknowledgedBy = acknowledgedBy;
        it->acknowledgedAt = std::chrono::system_clock::now();

        // Move to history
        alertHistory_.push_back(*it);
        activeAlerts_.erase(it);

        std::cout << "Alert " << alertId << " acknowledged by " << acknowledgedBy << std::endl;
    }
}

void DataQualityMonitor::clearAllAlerts() {
    std::lock_guard<std::mutex> lock(alertsMutex_);

    // Move all active alerts to history as acknowledged
    for (auto& alert : activeAlerts_) {
        alert.acknowledged = true;
        alert.acknowledgedBy = "system";
        alert.acknowledgedAt = std::chrono::system_clock::now();
        alertHistory_.push_back(alert);
    }

    activeAlerts_.clear();
    std::cout << "All alerts cleared" << std::endl;
}

std::vector<DataQualityAlert> DataQualityMonitor::getActiveAlerts() const {
    std::lock_guard<std::mutex> lock(alertsMutex_);
    return activeAlerts_;
}

std::vector<DataQualityAlert> DataQualityMonitor::getAlertHistory(std::chrono::hours lookback) const {
    std::lock_guard<std::mutex> lock(alertsMutex_);

    auto cutoff = std::chrono::system_clock::now() - lookback;
    std::vector<DataQualityAlert> recent;

    for (const auto& alert : alertHistory_) {
        if (alert.timestamp >= cutoff) {
            recent.push_back(alert);
        }
    }

    return recent;
}

DataQualityMonitor::MonitoringStatistics DataQualityMonitor::getStatistics() const {
    MonitoringStatistics stats;
    stats.totalAlerts = totalAlerts_.load();
    stats.criticalAlerts = criticalAlerts_.load();
    stats.lastAssessment = lastAssessment_;

    std::lock_guard<std::mutex> alertsLock(alertsMutex_);
    stats.activeAlerts = activeAlerts_.size();

    // Count alerts by type and severity
    for (const auto& alert : activeAlerts_) {
        stats.alertsByType[alert.type]++;
        stats.alertsBySeverity[alert.severity]++;
    }

    std::lock_guard<std::mutex> metricsLock(metricsMutex_);
    stats.currentMetrics = lastMetrics_;

    return stats;
}

void DataQualityMonitor::resetStatistics() {
    totalAlerts_ = 0;
    criticalAlerts_ = 0;
    std::lock_guard<std::mutex> lock(metricsMutex_);
    lastMetrics_.clear();
}

bool DataQualityMonitor::isHealthy() const {
    std::lock_guard<std::mutex> lock(alertsMutex_);

    // Check if there are any critical alerts
    for (const auto& alert : activeAlerts_) {
        if (alert.severity == AlertSeverity::CRITICAL) {
            return false;
        }
    }

    // Check if monitoring is running when it should be
    return isMonitoring_ || activeAlerts_.size() < 10;
}

std::vector<std::string> DataQualityMonitor::getHealthIssues() const {
    std::vector<std::string> issues;

    if (!isMonitoring_) {
        issues.push_back("Data quality monitoring is not active");
    }

    std::lock_guard<std::mutex> lock(alertsMutex_);

    if (activeAlerts_.size() > 10) {
        issues.push_back("High number of active alerts: " + std::to_string(activeAlerts_.size()));
    }

    int criticalCount = 0;
    for (const auto& alert : activeAlerts_) {
        if (alert.severity == AlertSeverity::CRITICAL) {
            ++criticalCount;
        }
    }

    if (criticalCount > 0) {
        issues.push_back("Critical alerts present: " + std::to_string(criticalCount));
    }

    return issues;
}

void DataQualityMonitor::monitoringLoop() {
    while (isMonitoring_) {
        try {
            performQualityCheck();
            cleanupOldAlerts();
        } catch (const std::exception& e) {
            generateAlert(AlertType::SYSTEM_HEALTH, AlertSeverity::ERROR,
                         "Monitoring Loop Error",
                         "Error in monitoring loop: " + std::string(e.what()),
                         "DataQualityMonitor");
        }

        // Sleep with interruption check
        auto sleepEnd = std::chrono::steady_clock::now() + monitoringInterval_;
        while (std::chrono::steady_clock::now() < sleepEnd && isMonitoring_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

void DataQualityMonitor::checkDataQuality() {
    if (!dataService_ || !dataService_->isRunning()) {
        generateAlert(AlertType::SYSTEM_HEALTH, AlertSeverity::WARNING,
                     "Data Service Not Running",
                     "Data processing service is not active",
                     "DataProcessingService");
        return;
    }

    // Get current data quality metrics from service
    auto stats = dataService_->getStatistics();
    if (stats.totalJobsRun == 0) {
        generateAlert(AlertType::DATA_QUALITY, AlertSeverity::INFO,
                     "No Processing Jobs Run",
                     "Data processing service has not processed any jobs yet",
                     "DataProcessingService");
        return;
    }

    // Check success rate
    if (stats.successRate < 0.8) { // Less than 80% success rate
        std::map<std::string, std::string> metadata;
        metadata["success_rate"] = std::to_string(stats.successRate);
        metadata["total_jobs"] = std::to_string(stats.totalJobsRun);
        metadata["failed_jobs"] = std::to_string(stats.failedJobs);

        generateAlert(AlertType::DATA_QUALITY, AlertSeverity::WARNING,
                     "Low Data Processing Success Rate",
                     "Data processing success rate is below 80%: " + std::to_string(stats.successRate * 100) + "%",
                     "DataProcessingService", metadata);
    }
}

void DataQualityMonitor::checkDataFreshness() {
    auto stats = dataService_->getStatistics();
    auto now = std::chrono::system_clock::now();

    // Check if last processing was too long ago
    auto timeSinceLastProcessing = now - stats.lastProcessing;
    if (timeSinceLastProcessing > thresholds_.maxDataAge) {
        std::map<std::string, std::string> metadata;
        auto minutes = std::chrono::duration_cast<std::chrono::minutes>(timeSinceLastProcessing).count();
        metadata["minutes_since_last_processing"] = std::to_string(minutes);
        metadata["threshold_minutes"] = std::to_string(thresholds_.maxDataAge.count());

        generateAlert(AlertType::DATA_FRESHNESS, AlertSeverity::ERROR,
                     "Stale Data Detected",
                     "No data processing activity for " + std::to_string(minutes) + " minutes",
                     "DataProcessingService", metadata);
    }
}

void DataQualityMonitor::checkSystemHealth() {
    if (!dataService_->isHealthy()) {
        auto healthIssues = dataService_->getHealthIssues();
        std::string description = "Data service health issues detected: ";
        for (size_t i = 0; i < healthIssues.size(); ++i) {
            if (i > 0) description += ", ";
            description += healthIssues[i];
        }

        generateAlert(AlertType::SYSTEM_HEALTH, AlertSeverity::ERROR,
                     "Data Service Health Issues",
                     description,
                     "DataProcessingService");
    }
}

void DataQualityMonitor::generateAlert(AlertType type, AlertSeverity severity, const std::string& title,
                                     const std::string& description, const std::string& source,
                                     const std::map<std::string, std::string>& metadata) {
    DataQualityAlert alert;
    alert.id = generateAlertId();
    alert.type = type;
    alert.severity = severity;
    alert.title = title;
    alert.description = description;
    alert.source = source;
    alert.metadata = metadata;
    alert.timestamp = std::chrono::system_clock::now();

    if (shouldSuppressAlert(alert)) {
        return;
    }

    processAlert(alert);
}

void DataQualityMonitor::processAlert(const DataQualityAlert& alert) {
    // Update statistics
    ++totalAlerts_;
    if (alert.severity == AlertSeverity::CRITICAL) {
        ++criticalAlerts_;
    }

    // Add to active alerts
    {
        std::lock_guard<std::mutex> lock(alertsMutex_);
        activeAlerts_.push_back(alert);
    }

    // Update suppression tracking
    std::string suppressionKey = alert.getTypeString() + "_" + alert.source;
    lastAlertTime_[suppressionKey] = alert.timestamp;

    // Send to all handlers
    for (const auto& handler : alertHandlers_) {
        if (handler) {
            try {
                handler->handleAlert(alert);
            } catch (const std::exception& e) {
                std::cerr << "Alert handler error: " << e.what() << std::endl;
            }
        }
    }
}

bool DataQualityMonitor::shouldSuppressAlert(const DataQualityAlert& alert) const {
    // Rate limiting
    {
        std::lock_guard<std::mutex> lock(alertsMutex_);
        if (activeAlerts_.size() >= static_cast<size_t>(thresholds_.maxAlertsPerHour)) {
            return true;
        }
    }

    // Cooldown check
    std::string suppressionKey = alert.getTypeString() + "_" + alert.source;
    auto it = lastAlertTime_.find(suppressionKey);
    if (it != lastAlertTime_.end()) {
        auto timeSinceLast = alert.timestamp - it->second;
        if (timeSinceLast < thresholds_.alertCooldown) {
            return true;
        }
    }

    return false;
}

void DataQualityMonitor::cleanupOldAlerts() {
    std::lock_guard<std::mutex> lock(alertsMutex_);

    auto cutoff = std::chrono::system_clock::now() - std::chrono::hours(24);

    // Remove old alerts from history
    auto it = std::remove_if(alertHistory_.begin(), alertHistory_.end(),
        [cutoff](const DataQualityAlert& alert) {
            return alert.timestamp < cutoff;
        });
    alertHistory_.erase(it, alertHistory_.end());
}

std::string DataQualityMonitor::generateAlertId() const {
    static std::atomic<int> counter{0};
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    return "ALERT_" + std::to_string(timestamp) + "_" + std::to_string(++counter);
}

// Factory implementations
std::unique_ptr<DataQualityMonitor> DataQualityMonitorFactory::create(
    std::shared_ptr<DataProcessingService> dataService,
    const QualityThresholds& thresholds) {
    return std::make_unique<DataQualityMonitor>(dataService, thresholds);
}

std::unique_ptr<DataQualityMonitor> DataQualityMonitorFactory::createWithConsoleAlerts(
    std::shared_ptr<DataProcessingService> dataService) {
    auto monitor = std::make_unique<DataQualityMonitor>(dataService);
    monitor->addAlertHandler(std::make_shared<ConsoleAlertHandler>());
    return monitor;
}

std::unique_ptr<DataQualityMonitor> DataQualityMonitorFactory::createWithEmailAlerts(
    std::shared_ptr<DataProcessingService> dataService,
    const std::vector<std::string>& emailRecipients) {
    auto monitor = std::make_unique<DataQualityMonitor>(dataService);
    monitor->addAlertHandler(std::make_shared<ConsoleAlertHandler>());
    monitor->addAlertHandler(std::make_shared<EmailAlertHandler>(emailRecipients));
    return monitor;
}

std::unique_ptr<DataQualityMonitor> DataQualityMonitorFactory::createForProduction(
    std::shared_ptr<DataProcessingService> dataService,
    const std::vector<std::string>& emailRecipients,
    const std::string& logFilePath) {
    auto monitor = std::make_unique<DataQualityMonitor>(dataService);
    monitor->addAlertHandler(std::make_shared<EmailAlertHandler>(emailRecipients));
    monitor->addAlertHandler(std::make_shared<LogFileAlertHandler>(logFilePath));
    return monitor;
}

std::unique_ptr<DataQualityMonitor> DataQualityMonitorFactory::createForTesting(
    std::shared_ptr<DataProcessingService> dataService) {
    QualityThresholds testThresholds;
    testThresholds.minOverallScore = 0.5; // Lower thresholds for testing
    testThresholds.maxDataAge = std::chrono::minutes(60);

    auto monitor = std::make_unique<DataQualityMonitor>(dataService, testThresholds);
    monitor->addAlertHandler(std::make_shared<ConsoleAlertHandler>());
    return monitor;
}

} // namespace Monitoring
} // namespace CryptoClaude