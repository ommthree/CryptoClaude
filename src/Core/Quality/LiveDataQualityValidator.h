#pragma once

#include "../Database/DatabaseManager.h"
#include "../Database/Models/MarketData.h"
#include "../Data/Providers/CryptoCompareProvider.h"
#include <vector>
#include <map>
#include <chrono>
#include <memory>
#include <functional>
#include <atomic>
#include <thread>

namespace CryptoClaude {
namespace Quality {

using namespace CryptoClaude::Database;
using namespace CryptoClaude::Database::Models;
using namespace CryptoClaude::Data::Providers;

// Data quality issue severity levels
enum class QualityIssueSeverity {
    INFO,       // Informational, no action needed
    WARNING,    // Potential issue, monitor
    ERROR,      // Data quality issue, needs attention
    CRITICAL    // Critical data quality failure, immediate action required
};

// Types of data quality issues
enum class QualityIssueType {
    MISSING_DATA,           // Expected data not received
    STALE_DATA,            // Data too old
    INVALID_PRICE,         // Price outside reasonable range
    INVALID_VOLUME,        // Volume data invalid
    TIMESTAMP_ERROR,       // Timestamp issues
    DUPLICATE_DATA,        // Duplicate data points
    DATA_INCONSISTENCY,    // Data inconsistent across sources
    RATE_LIMIT_EXCEEDED,   // Provider rate limits hit
    PROVIDER_ERROR,        // Provider-specific errors
    NETWORK_ISSUE,         // Network connectivity problems
    VALIDATION_FAILURE     // Custom validation rule failure
};

// Individual data quality issue
struct DataQualityIssue {
    std::string issueId;
    QualityIssueType type;
    QualityIssueSeverity severity;
    std::string providerId;
    std::string symbol;
    std::chrono::system_clock::time_point detectedAt;
    std::string description;
    std::string details;

    // Issue context
    std::map<std::string, std::string> context;
    std::vector<std::string> affectedDataPoints;

    // Resolution
    bool isResolved = false;
    std::chrono::system_clock::time_point resolvedAt;
    std::string resolutionAction;
    std::string resolvedBy;

    DataQualityIssue() = default;
    DataQualityIssue(QualityIssueType t, QualityIssueSeverity s,
                    const std::string& provider, const std::string& sym,
                    const std::string& desc)
        : type(t), severity(s), providerId(provider), symbol(sym),
          detectedAt(std::chrono::system_clock::now()), description(desc) {
        issueId = generateIssueId();
    }

private:
    std::string generateIssueId() const;
};

// Data quality metrics for a specific symbol/provider combination
struct DataQualityMetrics {
    std::string providerId;
    std::string symbol;
    std::chrono::system_clock::time_point assessmentTime;

    // Completeness metrics
    int expectedDataPoints = 0;
    int receivedDataPoints = 0;
    double completenessRatio = 0.0; // 0.0 to 1.0

    // Accuracy metrics
    int validDataPoints = 0;
    int invalidDataPoints = 0;
    double accuracyRatio = 0.0; // 0.0 to 1.0

    // Timeliness metrics
    std::chrono::minutes averageDataAge;
    std::chrono::minutes maxDataAge;
    double timelinessScore = 0.0; // 0.0 to 1.0

    // Consistency metrics
    double consistencyScore = 0.0; // 0.0 to 1.0 (consistency across providers)
    int inconsistentDataPoints = 0;

    // Overall quality score
    double overallQualityScore = 0.0; // 0.0 to 1.0

    // Issue counts by severity
    int infoIssues = 0;
    int warningIssues = 0;
    int errorIssues = 0;
    int criticalIssues = 0;

    DataQualityMetrics() = default;
    DataQualityMetrics(const std::string& provider, const std::string& sym)
        : providerId(provider), symbol(sym),
          assessmentTime(std::chrono::system_clock::now()) {}

    bool meetsQualityThreshold(double threshold) const {
        return overallQualityScore >= threshold;
    }
};

// Real-time validation rule
struct ValidationRule {
    std::string ruleId;
    std::string ruleName;
    std::string description;
    bool isEnabled = true;
    QualityIssueSeverity severityOnFailure = QualityIssueSeverity::WARNING;

    // Validation function
    std::function<bool(const MarketData&, const std::map<std::string, std::string>&)> validate;

    // Rule configuration
    std::map<std::string, std::string> parameters;
    std::vector<std::string> applicableSymbols; // Empty = all symbols
    std::vector<std::string> applicableProviders; // Empty = all providers

    ValidationRule() = default;
    ValidationRule(const std::string& id, const std::string& name,
                  std::function<bool(const MarketData&, const std::map<std::string, std::string>&)> validator)
        : ruleId(id), ruleName(name), validate(validator) {}
};

// Live data quality validator
class LiveDataQualityValidator {
public:
    LiveDataQualityValidator(DatabaseManager& dbManager);
    ~LiveDataQualityValidator();

    // Initialization and configuration
    bool initialize();
    void shutdown();

    // Validation rule management
    bool addValidationRule(const ValidationRule& rule);
    bool removeValidationRule(const std::string& ruleId);
    bool enableValidationRule(const std::string& ruleId, bool enable = true);
    std::vector<ValidationRule> getValidationRules() const;

    // Real-time data validation
    DataQualityMetrics validateMarketData(const std::string& providerId,
                                        const std::string& symbol,
                                        const std::vector<MarketData>& data);

    std::vector<DataQualityIssue> validateDataPoint(const std::string& providerId,
                                                   const MarketData& dataPoint);

    // Batch validation for historical data
    std::map<std::string, DataQualityMetrics> validateBatchData(
        const std::map<std::string, std::vector<MarketData>>& providerData);

    // Cross-provider consistency validation
    std::vector<DataQualityIssue> validateCrossProviderConsistency(
        const std::map<std::string, std::vector<MarketData>>& providerData,
        const std::string& symbol);

    // Quality monitoring
    void startRealTimeValidation();
    void stopRealTimeValidation();
    bool isValidationActive() const { return isValidating_; }

    // Data source registration for monitoring
    bool registerDataSource(const std::string& providerId,
                          std::shared_ptr<CryptoCompareProvider> provider);
    bool unregisterDataSource(const std::string& providerId);

    // Issue management
    std::vector<DataQualityIssue> getActiveIssues() const;
    std::vector<DataQualityIssue> getIssuesByProvider(const std::string& providerId) const;
    std::vector<DataQualityIssue> getIssuesBySeverity(QualityIssueSeverity severity) const;
    bool resolveIssue(const std::string& issueId, const std::string& resolutionAction,
                     const std::string& resolvedBy = "system");

    // Quality metrics and reporting
    DataQualityMetrics getQualityMetrics(const std::string& providerId,
                                       const std::string& symbol) const;
    std::map<std::string, DataQualityMetrics> getAllQualityMetrics() const;

    struct QualityReport {
        std::chrono::system_clock::time_point reportTime;
        int totalDataSources = 0;
        int healthyDataSources = 0;
        int dataSourcesWithIssues = 0;

        double averageQualityScore = 0.0;
        int totalActiveIssues = 0;
        int criticalIssues = 0;
        int errorIssues = 0;
        int warningIssues = 0;

        std::map<std::string, DataQualityMetrics> providerMetrics;
        std::vector<DataQualityIssue> criticalIssuesList;
        std::vector<std::string> recommendations;
    };

    QualityReport generateQualityReport() const;

    // Configuration
    struct ValidationConfig {
        // Quality thresholds
        double minimumCompletenessThreshold = 0.95;   // 95%
        double minimumAccuracyThreshold = 0.98;       // 98%
        double minimumTimelinessThreshold = 0.90;     // 90%
        double minimumConsistencyThreshold = 0.85;    // 85%
        double minimumOverallQualityThreshold = 0.90; // 90%

        // Data age limits
        std::chrono::minutes maxDataAge = std::chrono::minutes(5);
        std::chrono::minutes warningDataAge = std::chrono::minutes(2);

        // Price validation ranges (symbol -> {min, max})
        std::map<std::string, std::pair<double, double>> priceRanges;

        // Volume validation
        double minVolume = 0.0;
        double maxVolumeMultiplier = 1000.0; // Max volume as multiple of average

        // Cross-provider consistency
        double maxPriceDeviationPercent = 5.0; // 5% deviation allowed
        int minProvidersForConsistencyCheck = 2;

        // Validation frequency
        std::chrono::seconds validationInterval = std::chrono::seconds(30);
        bool enableRealTimeValidation = true;
        bool enableCrossProviderValidation = true;

        // Issue management
        std::chrono::hours issueRetentionPeriod = std::chrono::hours(24 * 7); // 1 week
        bool autoResolveInfoIssues = true;
        std::chrono::minutes autoResolveDelay = std::chrono::minutes(30);

        ValidationConfig() {
            // Initialize default price ranges
            priceRanges["BTC"] = {1000.0, 1000000.0};
            priceRanges["ETH"] = {100.0, 50000.0};
            priceRanges["ADA"] = {0.1, 10.0};
            priceRanges["DOT"] = {1.0, 100.0};
            priceRanges["LINK"] = {1.0, 1000.0};
        }
    };

    void setValidationConfig(const ValidationConfig& config);
    ValidationConfig getValidationConfig() const { return config_; }

    // Event callbacks
    using QualityIssueCallback = std::function<void(const DataQualityIssue&)>;
    using QualityMetricsCallback = std::function<void(const DataQualityMetrics&)>;

    void setQualityIssueCallback(QualityIssueCallback callback) { issueCallback_ = callback; }
    void setQualityMetricsCallback(QualityMetricsCallback callback) { metricsCallback_ = callback; }

    // Built-in validation rules
    static ValidationRule createPriceRangeValidationRule();
    static ValidationRule createVolumeValidationRule();
    static ValidationRule createTimestampValidationRule();
    static ValidationRule createDataFreshnessValidationRule();
    static ValidationRule createDuplicateDataValidationRule();

private:
    DatabaseManager& dbManager_;
    ValidationConfig config_;

    // Data sources and providers
    std::map<std::string, std::shared_ptr<CryptoCompareProvider>> dataSources_;

    // Validation rules
    std::vector<ValidationRule> validationRules_;

    // Quality tracking
    mutable std::map<std::string, DataQualityMetrics> qualityMetrics_;
    std::vector<DataQualityIssue> activeIssues_;

    // Real-time validation
    std::atomic<bool> isValidating_{false};
    std::unique_ptr<std::thread> validationThread_;

    // Callbacks
    QualityIssueCallback issueCallback_;
    QualityMetricsCallback metricsCallback_;

    // Validation logic
    void validationLoop();
    bool validateSingleDataPoint(const MarketData& data, const ValidationRule& rule,
                               const std::string& providerId);

    // Quality calculation methods
    double calculateCompletenessRatio(int expected, int received) const;
    double calculateAccuracyRatio(int valid, int total) const;
    double calculateTimelinessScore(const std::vector<MarketData>& data) const;
    double calculateConsistencyScore(const std::vector<MarketData>& data,
                                   const std::string& symbol) const;
    double calculateOverallQualityScore(const DataQualityMetrics& metrics) const;

    // Issue detection and management
    void detectDataQualityIssues(const std::string& providerId, const std::string& symbol,
                                const std::vector<MarketData>& data);
    void createQualityIssue(QualityIssueType type, QualityIssueSeverity severity,
                          const std::string& providerId, const std::string& symbol,
                          const std::string& description, const std::string& details = "");

    // Cross-provider validation
    bool validatePriceConsistency(const std::map<std::string, double>& providerPrices,
                                const std::string& symbol) const;
    double calculatePriceDeviation(const std::vector<double>& prices) const;

    // Database operations
    bool initializeDatabaseTables();
    void persistQualityMetrics(const DataQualityMetrics& metrics);
    void persistQualityIssue(const DataQualityIssue& issue);
    std::vector<DataQualityMetrics> loadHistoricalMetrics(const std::string& providerId,
                                                         const std::string& symbol,
                                                         int hoursBack = 24) const;

    // Validation rule implementations
    bool validatePriceRange(const MarketData& data, const std::map<std::string, std::string>& params);
    bool validateVolume(const MarketData& data, const std::map<std::string, std::string>& params);
    bool validateTimestamp(const MarketData& data, const std::map<std::string, std::string>& params);
    bool validateDataFreshness(const MarketData& data, const std::map<std::string, std::string>& params);

    // Utility functions
    std::string generateUniqueId() const;
    std::string qualityIssueTypeToString(QualityIssueType type) const;
    std::string qualityIssueSeverityToString(QualityIssueSeverity severity) const;
    QualityIssueSeverity stringToQualityIssueSeverity(const std::string& severity) const;
    QualityIssueType stringToQualityIssueType(const std::string& type) const;
};

// Data quality alert system
class DataQualityAlerter {
public:
    DataQualityAlerter(LiveDataQualityValidator& validator);

    struct AlertRule {
        std::string alertId;
        std::string alertName;
        QualityIssueSeverity triggerSeverity = QualityIssueSeverity::ERROR;
        double qualityThreshold = 0.90;
        int maxIssueCount = 5;
        std::chrono::minutes alertCooldown = std::chrono::minutes(30);

        // Alert targets
        std::vector<std::string> emailRecipients;
        std::string webhookUrl;
        bool enableSlackNotification = false;

        // Alert conditions
        std::vector<std::string> monitoredProviders; // Empty = all
        std::vector<std::string> monitoredSymbols;   // Empty = all
        std::vector<QualityIssueType> monitoredIssueTypes; // Empty = all

        bool isActive = true;
        std::chrono::system_clock::time_point lastTriggered;
    };

    bool addAlertRule(const AlertRule& rule);
    bool removeAlertRule(const std::string& alertId);
    std::vector<AlertRule> getAlertRules() const;

    void checkAlertConditions();
    void enableRealTimeAlerts(bool enable = true);

private:
    LiveDataQualityValidator& validator_;
    std::vector<AlertRule> alertRules_;
    std::atomic<bool> alertingEnabled_{false};
    std::unique_ptr<std::thread> alertThread_;

    void alertingLoop();
    bool shouldTriggerAlert(const AlertRule& rule, const DataQualityIssue& issue);
    void sendAlert(const AlertRule& rule, const DataQualityIssue& issue);
};

} // namespace Quality
} // namespace CryptoClaude