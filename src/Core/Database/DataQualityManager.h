#pragma once
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>

namespace CryptoClaude {
namespace Database {

class DatabaseManager;

struct QualityMetric {
    std::string tableName;
    std::string columnName;
    double qualityScore;
    double completenessRatio;
    double accuracyScore;
    int outlierCount;
    int totalRecords;
    long long measurementTimestamp;
    bool remediationApplied;
    std::string remediationDetails;
};

struct DataAnomalyInfo {
    std::string tableName;
    std::string columnName;
    std::string anomalyType;
    int recordCount;
    std::string description;
    double severity; // 0.0 to 1.0
    std::vector<std::string> sampleValues;
};

class DataQualityManager {
public:
    explicit DataQualityManager(DatabaseManager& dbManager);
    ~DataQualityManager() = default;

    // Initialize quality monitoring system
    bool initialize();

    // Run comprehensive data quality assessment
    bool assessDataQuality();

    // Automated remediation procedures
    bool performAutomatedRemediation();

    // Quality assessment for specific tables
    QualityMetric assessTable(const std::string& tableName);
    QualityMetric assessColumn(const std::string& tableName, const std::string& columnName);

    // Anomaly detection
    std::vector<DataAnomalyInfo> detectAnomalies();
    std::vector<DataAnomalyInfo> detectAnomaliesInTable(const std::string& tableName);

    // Remediation operations
    bool fixMissingData(const std::string& tableName, const std::string& columnName);
    bool fixOutliers(const std::string& tableName, const std::string& columnName,
                     double standardDeviationThreshold = 3.0);
    bool interpolateMissingValues(const std::string& tableName, const std::string& columnName);

    // Data validation
    bool validateMarketData();
    bool validateSentimentData();
    bool validatePortfolioData();

    // Quality reporting
    std::vector<QualityMetric> getQualityReport();
    std::vector<QualityMetric> getQualityReportForTable(const std::string& tableName);
    double getOverallQualityScore();

    // Configuration
    void setQualityThreshold(double threshold) { m_qualityThreshold = threshold; }
    void setOutlierThreshold(double threshold) { m_outlierThreshold = threshold; }
    void setCompletenessThreshold(double threshold) { m_completenessThreshold = threshold; }

private:
    DatabaseManager& m_dbManager;
    double m_qualityThreshold;
    double m_outlierThreshold;
    double m_completenessThreshold;

    // Quality assessment helpers
    double calculateCompleteness(const std::string& tableName, const std::string& columnName);
    double calculateAccuracy(const std::string& tableName, const std::string& columnName);
    int countOutliers(const std::string& tableName, const std::string& columnName);
    int getTotalRecordCount(const std::string& tableName);

    // Anomaly detection helpers
    std::vector<DataAnomalyInfo> detectPriceAnomalies();
    std::vector<DataAnomalyInfo> detectVolumeAnomalies();
    std::vector<DataAnomalyInfo> detectSentimentAnomalies();
    std::vector<DataAnomalyInfo> detectTemporalAnomalies();

    // Remediation helpers
    bool interpolateLinear(const std::string& tableName, const std::string& columnName,
                           const std::string& timeColumn);
    bool capOutliers(const std::string& tableName, const std::string& columnName,
                     double lowerBound, double upperBound);
    bool fillWithMedian(const std::string& tableName, const std::string& columnName);

    // Validation helpers
    bool validatePriceData(const std::string& symbol);
    bool validateVolumeData(const std::string& symbol);
    bool validateSentimentRange(const std::string& symbol);
    bool validateTimestampConsistency(const std::string& tableName);

    // Persistence
    bool recordQualityMetric(const QualityMetric& metric);
    std::vector<QualityMetric> loadQualityHistory(const std::string& tableName, int daysBack = 7);
};

} // namespace Database
} // namespace CryptoClaude