#pragma once

#include "../Data/DataProcessingService.h"
#include "../Database/Models/MarketData.h"
#include "../Database/Models/SentimentData.h"
#include <memory>
#include <vector>
#include <string>
#include <chrono>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>
#include <map>
#include <queue>

namespace CryptoClaude {
namespace Monitoring {

using namespace CryptoClaude::Data;
using namespace CryptoClaude::Database::Models;

// Alert severity levels
enum class AlertSeverity {
    INFO,
    WARNING,
    ERROR,
    CRITICAL
};

// Alert types
enum class AlertType {
    DATA_QUALITY,
    DATA_FRESHNESS,
    DATA_COMPLETENESS,
    DATA_ACCURACY,
    SYSTEM_HEALTH,
    PROCESSING_ERROR
};

// Data quality alert
struct DataQualityAlert {
    std::string id;
    AlertType type;
    AlertSeverity severity;
    std::string title;
    std::string description;
    std::chrono::system_clock::time_point timestamp;
    std::string source;  // Which data source triggered the alert
    std::map<std::string, std::string> metadata;
    bool acknowledged = false;
    std::string acknowledgedBy;
    std::chrono::system_clock::time_point acknowledgedAt;

    bool isActive() const {
        return !acknowledged;
    }

    std::string getSeverityString() const {
        switch (severity) {
            case AlertSeverity::INFO: return "INFO";
            case AlertSeverity::WARNING: return "WARNING";
            case AlertSeverity::ERROR: return "ERROR";
            case AlertSeverity::CRITICAL: return "CRITICAL";
            default: return "UNKNOWN";
        }
    }

    std::string getTypeString() const {
        switch (type) {
            case AlertType::DATA_QUALITY: return "DATA_QUALITY";
            case AlertType::DATA_FRESHNESS: return "DATA_FRESHNESS";
            case AlertType::DATA_COMPLETENESS: return "DATA_COMPLETENESS";
            case AlertType::DATA_ACCURACY: return "DATA_ACCURACY";
            case AlertType::SYSTEM_HEALTH: return "SYSTEM_HEALTH";
            case AlertType::PROCESSING_ERROR: return "PROCESSING_ERROR";
            default: return "UNKNOWN";
        }
    }
};

// Quality monitoring thresholds
struct QualityThresholds {
    double minCompleteness = 0.8;      // 80% data completeness required
    double minAccuracy = 0.9;          // 90% data accuracy required
    double minTimeliness = 0.7;        // 70% data freshness required
    double minConsistency = 0.8;       // 80% data consistency required
    double minOverallScore = 0.75;     // 75% overall quality score required

    std::chrono::minutes maxDataAge = std::chrono::minutes(30);  // Max age before stale alert
    int minDataPoints = 10;            // Minimum data points for quality assessment
    double maxErrorRate = 0.1;         // Maximum 10% error rate

    // Alert suppression
    std::chrono::minutes alertCooldown = std::chrono::minutes(15);  // Min time between similar alerts
    int maxAlertsPerHour = 20;         // Rate limiting for alerts
};

// Alert handler interface
class IAlertHandler {
public:
    virtual ~IAlertHandler() = default;
    virtual void handleAlert(const DataQualityAlert& alert) = 0;
    virtual std::string getHandlerName() const = 0;
};

// Console alert handler
class ConsoleAlertHandler : public IAlertHandler {
public:
    void handleAlert(const DataQualityAlert& alert) override;
    std::string getHandlerName() const override { return "ConsoleHandler"; }
};

// Email alert handler (mock implementation)
class EmailAlertHandler : public IAlertHandler {
private:
    std::vector<std::string> recipients_;
    std::string smtpServer_;

public:
    explicit EmailAlertHandler(const std::vector<std::string>& recipients,
                             const std::string& smtpServer = "localhost")
        : recipients_(recipients), smtpServer_(smtpServer) {}

    void handleAlert(const DataQualityAlert& alert) override;
    std::string getHandlerName() const override { return "EmailHandler"; }

    void addRecipient(const std::string& email) { recipients_.push_back(email); }
    void setSmtpServer(const std::string& server) { smtpServer_ = server; }
};

// Log file alert handler
class LogFileAlertHandler : public IAlertHandler {
private:
    std::string logFilePath_;
    std::mutex fileMutex_;

public:
    explicit LogFileAlertHandler(const std::string& logFilePath)
        : logFilePath_(logFilePath) {}

    void handleAlert(const DataQualityAlert& alert) override;
    std::string getHandlerName() const override { return "LogFileHandler"; }
};

// Main data quality monitor
class DataQualityMonitor {
private:
    std::shared_ptr<DataProcessingService> dataService_;
    QualityThresholds thresholds_;
    std::vector<std::shared_ptr<IAlertHandler>> alertHandlers_;

    // Monitoring state
    std::atomic<bool> isMonitoring_;
    std::thread monitoringThread_;
    std::chrono::seconds monitoringInterval_;

    // Alert management
    mutable std::mutex alertsMutex_;
    std::vector<DataQualityAlert> activeAlerts_;
    std::vector<DataQualityAlert> alertHistory_;
    std::map<std::string, std::chrono::system_clock::time_point> lastAlertTime_;
    std::atomic<int> totalAlerts_;
    std::atomic<int> criticalAlerts_;

    // Quality metrics cache
    mutable std::mutex metricsMutex_;
    std::map<std::string, DataQualityMetrics> lastMetrics_;
    std::chrono::system_clock::time_point lastAssessment_;

public:
    explicit DataQualityMonitor(std::shared_ptr<DataProcessingService> dataService,
                              const QualityThresholds& thresholds = QualityThresholds{});
    virtual ~DataQualityMonitor();

    // Control methods
    void startMonitoring(std::chrono::seconds interval = std::chrono::seconds(60));
    void stopMonitoring();
    bool isMonitoring() const { return isMonitoring_; }

    // Alert handler management
    void addAlertHandler(std::shared_ptr<IAlertHandler> handler);
    void removeAlertHandler(const std::string& handlerName);
    std::vector<std::string> getAlertHandlers() const;

    // Configuration
    void setThresholds(const QualityThresholds& thresholds) { thresholds_ = thresholds; }
    QualityThresholds getThresholds() const { return thresholds_; }

    // Manual quality checks
    void performQualityCheck();
    DataQualityMetrics assessMarketDataQuality(const std::vector<MarketData>& data);
    DataQualityMetrics assessSentimentDataQuality(const std::vector<SentimentData>& data);

    // Alert management
    void acknowledgeAlert(const std::string& alertId, const std::string& acknowledgedBy = "system");
    void clearAllAlerts();
    std::vector<DataQualityAlert> getActiveAlerts() const;
    std::vector<DataQualityAlert> getAlertHistory(std::chrono::hours lookback = std::chrono::hours(24)) const;

    // Statistics
    struct MonitoringStatistics {
        int totalAlerts = 0;
        int criticalAlerts = 0;
        int activeAlerts = 0;
        std::chrono::system_clock::time_point lastAssessment;
        std::map<std::string, DataQualityMetrics> currentMetrics;
        std::map<AlertType, int> alertsByType;
        std::map<AlertSeverity, int> alertsBySeverity;
    };

    MonitoringStatistics getStatistics() const;
    void resetStatistics();

    // Health check
    bool isHealthy() const;
    std::vector<std::string> getHealthIssues() const;

private:
    // Core monitoring logic
    void monitoringLoop();
    void checkDataQuality();
    void checkDataFreshness();
    void checkSystemHealth();

    // Quality assessment
    DataQualityMetrics calculateOverallQuality(const std::vector<MarketData>& marketData,
                                              const std::vector<SentimentData>& sentimentData);

    // Alert generation and management
    void generateAlert(AlertType type, AlertSeverity severity, const std::string& title,
                      const std::string& description, const std::string& source,
                      const std::map<std::string, std::string>& metadata = {});

    void processAlert(const DataQualityAlert& alert);
    bool shouldSuppressAlert(const DataQualityAlert& alert) const;
    void cleanupOldAlerts();

    // Utility methods
    std::string generateAlertId() const;
    bool isDataStale(std::chrono::system_clock::time_point timestamp) const;
    double calculateTrendScore(const std::vector<double>& values) const;
    void updateMetricsCache(const std::string& source, const DataQualityMetrics& metrics);

    // Validation helpers
    bool validateMarketData(const MarketData& data) const;
    bool validateSentimentData(const SentimentData& data) const;
    double calculateDataCompleteness(const std::vector<MarketData>& data) const;
    double calculateDataAccuracy(const std::vector<MarketData>& data) const;
};

// Factory for creating monitors
class DataQualityMonitorFactory {
public:
    static std::unique_ptr<DataQualityMonitor> create(
        std::shared_ptr<DataProcessingService> dataService,
        const QualityThresholds& thresholds = QualityThresholds{});

    static std::unique_ptr<DataQualityMonitor> createWithConsoleAlerts(
        std::shared_ptr<DataProcessingService> dataService);

    static std::unique_ptr<DataQualityMonitor> createWithEmailAlerts(
        std::shared_ptr<DataProcessingService> dataService,
        const std::vector<std::string>& emailRecipients);

    static std::unique_ptr<DataQualityMonitor> createForProduction(
        std::shared_ptr<DataProcessingService> dataService,
        const std::vector<std::string>& emailRecipients,
        const std::string& logFilePath);

    static std::unique_ptr<DataQualityMonitor> createForTesting(
        std::shared_ptr<DataProcessingService> dataService);
};

// Configuration helper
struct DataQualityMonitorConfig {
    QualityThresholds thresholds;
    std::chrono::seconds monitoringInterval = std::chrono::seconds(60);
    bool enableConsoleAlerts = true;
    bool enableEmailAlerts = false;
    bool enableLogFileAlerts = true;
    std::vector<std::string> emailRecipients;
    std::string logFilePath = "data_quality.log";
    std::string smtpServer = "localhost";

    bool isValid() const {
        return monitoringInterval > std::chrono::seconds(10) &&
               (!enableEmailAlerts || !emailRecipients.empty());
    }

    std::vector<std::string> getConfigurationErrors() const {
        std::vector<std::string> errors;
        if (monitoringInterval <= std::chrono::seconds(10)) {
            errors.push_back("Monitoring interval must be greater than 10 seconds");
        }
        if (enableEmailAlerts && emailRecipients.empty()) {
            errors.push_back("Email alerts enabled but no recipients specified");
        }
        if (thresholds.minOverallScore < 0.0 || thresholds.minOverallScore > 1.0) {
            errors.push_back("Quality score thresholds must be between 0.0 and 1.0");
        }
        return errors;
    }
};

} // namespace Monitoring
} // namespace CryptoClaude