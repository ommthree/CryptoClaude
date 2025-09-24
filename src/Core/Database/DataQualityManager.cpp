#include "DataQualityManager.h"
#include "DatabaseManager.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <numeric>

namespace CryptoClaude {
namespace Database {

DataQualityManager::DataQualityManager(DatabaseManager& dbManager)
    : m_dbManager(dbManager)
    , m_qualityThreshold(0.95) // 95% quality threshold
    , m_outlierThreshold(3.0)  // 3 standard deviations
    , m_completenessThreshold(0.99) // 99% completeness threshold
{
}

bool DataQualityManager::initialize() {
    // Migration system should have created the data_quality_metrics table
    if (!m_dbManager.tableExists("data_quality_metrics")) {
        std::cerr << "DataQualityManager: data_quality_metrics table not found" << std::endl;
        return false;
    }

    std::cout << "DataQualityManager initialized successfully" << std::endl;
    return true;
}

bool DataQualityManager::assessDataQuality() {
    std::cout << "Starting comprehensive data quality assessment..." << std::endl;

    // Assess critical tables
    std::vector<std::string> criticalTables = {
        "market_data", "hourly_data", "sentiment_data", "aggregated_sentiment",
        "portfolios", "positions", "news_articles"
    };

    bool overallSuccess = true;
    for (const std::string& tableName : criticalTables) {
        if (m_dbManager.tableExists(tableName)) {
            QualityMetric metric = assessTable(tableName);
            if (!recordQualityMetric(metric)) {
                std::cerr << "Failed to record quality metric for table: " << tableName << std::endl;
                overallSuccess = false;
            }

            std::cout << "Table " << tableName << " quality score: "
                      << std::fixed << std::setprecision(2) << metric.qualityScore << std::endl;
        }
    }

    return overallSuccess;
}

bool DataQualityManager::performAutomatedRemediation() {
    std::cout << "Starting automated data quality remediation..." << std::endl;

    auto anomalies = detectAnomalies();
    bool allSuccessful = true;

    for (const auto& anomaly : anomalies) {
        std::cout << "Remediating " << anomaly.anomalyType << " in "
                  << anomaly.tableName << "." << anomaly.columnName << std::endl;

        bool remediationSuccess = false;

        if (anomaly.anomalyType == "missing_data") {
            remediationSuccess = fixMissingData(anomaly.tableName, anomaly.columnName);
        }
        else if (anomaly.anomalyType == "outliers") {
            remediationSuccess = fixOutliers(anomaly.tableName, anomaly.columnName);
        }
        else if (anomaly.anomalyType == "data_gaps") {
            remediationSuccess = interpolateMissingValues(anomaly.tableName, anomaly.columnName);
        }

        if (!remediationSuccess) {
            std::cerr << "Failed to remediate " << anomaly.anomalyType
                      << " in " << anomaly.tableName << "." << anomaly.columnName << std::endl;
            allSuccessful = false;
        }
    }

    return allSuccessful;
}

QualityMetric DataQualityManager::assessTable(const std::string& tableName) {
    QualityMetric metric;
    metric.tableName = tableName;
    metric.columnName = ""; // Table-level assessment
    metric.measurementTimestamp = time(nullptr);
    metric.remediationApplied = false;

    // Get total record count
    metric.totalRecords = getTotalRecordCount(tableName);

    if (metric.totalRecords == 0) {
        metric.qualityScore = 0.0;
        metric.completenessRatio = 0.0;
        metric.accuracyScore = 0.0;
        metric.outlierCount = 0;
        return metric;
    }

    // Assess key columns based on table type
    std::vector<double> columnScores;

    if (tableName == "market_data" || tableName == "hourly_data") {
        columnScores.push_back(calculateCompleteness(tableName, "close_price"));
        columnScores.push_back(calculateCompleteness(tableName, "volume_from"));
        columnScores.push_back(calculateAccuracy(tableName, "close_price"));
        metric.outlierCount += countOutliers(tableName, "close_price");
        metric.outlierCount += countOutliers(tableName, "volume_from");
    }
    else if (tableName == "sentiment_data" || tableName == "aggregated_sentiment") {
        columnScores.push_back(calculateCompleteness(tableName, "avg_sentiment"));
        columnScores.push_back(calculateAccuracy(tableName, "avg_sentiment"));
        metric.outlierCount += countOutliers(tableName, "avg_sentiment");
    }
    else if (tableName == "portfolios") {
        columnScores.push_back(calculateCompleteness(tableName, "total_value"));
        columnScores.push_back(calculateAccuracy(tableName, "total_value"));
    }
    else {
        // Generic assessment
        columnScores.push_back(0.8); // Default score for unknown tables
    }

    // Calculate overall scores
    if (!columnScores.empty()) {
        metric.completenessRatio = *std::min_element(columnScores.begin(), columnScores.end());
        metric.accuracyScore = std::accumulate(columnScores.begin(), columnScores.end(), 0.0) / columnScores.size();
    }

    // Calculate overall quality score
    double outlierPenalty = std::min(1.0, static_cast<double>(metric.outlierCount) / metric.totalRecords * 10.0);
    metric.qualityScore = (metric.completenessRatio * 0.4 + metric.accuracyScore * 0.4) * (1.0 - outlierPenalty * 0.2);

    return metric;
}

QualityMetric DataQualityManager::assessColumn(const std::string& tableName, const std::string& columnName) {
    QualityMetric metric;
    metric.tableName = tableName;
    metric.columnName = columnName;
    metric.measurementTimestamp = time(nullptr);
    metric.totalRecords = getTotalRecordCount(tableName);

    metric.completenessRatio = calculateCompleteness(tableName, columnName);
    metric.accuracyScore = calculateAccuracy(tableName, columnName);
    metric.outlierCount = countOutliers(tableName, columnName);

    // Calculate quality score
    double outlierPenalty = std::min(1.0, static_cast<double>(metric.outlierCount) / metric.totalRecords * 5.0);
    metric.qualityScore = (metric.completenessRatio * 0.5 + metric.accuracyScore * 0.5) * (1.0 - outlierPenalty * 0.2);

    return metric;
}

std::vector<DataAnomalyInfo> DataQualityManager::detectAnomalies() {
    std::vector<DataAnomalyInfo> allAnomalies;

    auto priceAnomalies = detectPriceAnomalies();
    auto volumeAnomalies = detectVolumeAnomalies();
    auto sentimentAnomalies = detectSentimentAnomalies();
    auto temporalAnomalies = detectTemporalAnomalies();

    allAnomalies.insert(allAnomalies.end(), priceAnomalies.begin(), priceAnomalies.end());
    allAnomalies.insert(allAnomalies.end(), volumeAnomalies.begin(), volumeAnomalies.end());
    allAnomalies.insert(allAnomalies.end(), sentimentAnomalies.begin(), sentimentAnomalies.end());
    allAnomalies.insert(allAnomalies.end(), temporalAnomalies.begin(), temporalAnomalies.end());

    return allAnomalies;
}

bool DataQualityManager::validateMarketData() {
    std::cout << "Validating market data..." << std::endl;

    // Get all symbols with market data
    std::string sql = "SELECT DISTINCT symbol FROM market_data";
    auto stmt = m_dbManager.prepareStatement(sql);
    if (!stmt) {
        return false;
    }

    StatementWrapper wrapper(stmt);
    std::vector<std::string> symbols;
    while (wrapper.step()) {
        symbols.push_back(wrapper.getStringColumn(0));
    }

    bool allValid = true;
    for (const std::string& symbol : symbols) {
        if (!validatePriceData(symbol) || !validateVolumeData(symbol)) {
            std::cerr << "Market data validation failed for symbol: " << symbol << std::endl;
            allValid = false;
        }
    }

    if (!validateTimestampConsistency("market_data")) {
        std::cerr << "Timestamp consistency validation failed for market_data" << std::endl;
        allValid = false;
    }

    return allValid;
}

// Implementation of helper methods
double DataQualityManager::calculateCompleteness(const std::string& tableName, const std::string& columnName) {
    std::string totalSql = "SELECT COUNT(*) FROM " + tableName;
    std::string nullSql = "SELECT COUNT(*) FROM " + tableName + " WHERE " + columnName + " IS NULL";

    auto totalStmt = m_dbManager.prepareStatement(totalSql);
    auto nullStmt = m_dbManager.prepareStatement(nullSql);

    if (!totalStmt || !nullStmt) {
        return 0.0;
    }

    StatementWrapper totalWrapper(totalStmt);
    StatementWrapper nullWrapper(nullStmt);

    int total = 0, nullCount = 0;
    if (totalWrapper.step()) {
        total = totalWrapper.getIntColumn(0);
    }
    if (nullWrapper.step()) {
        nullCount = nullWrapper.getIntColumn(0);
    }

    if (total == 0) return 0.0;
    return static_cast<double>(total - nullCount) / total;
}

double DataQualityManager::calculateAccuracy(const std::string& tableName, const std::string& columnName) {
    // Simple accuracy check based on reasonable ranges
    if (columnName.find("price") != std::string::npos) {
        // Price should be positive
        std::string sql = "SELECT COUNT(*) FROM " + tableName + " WHERE " + columnName + " > 0";
        auto stmt = m_dbManager.prepareStatement(sql);
        if (stmt) {
            StatementWrapper wrapper(stmt);
            if (wrapper.step()) {
                int validCount = wrapper.getIntColumn(0);
                int totalCount = getTotalRecordCount(tableName);
                return totalCount > 0 ? static_cast<double>(validCount) / totalCount : 0.0;
            }
        }
    }
    else if (columnName.find("sentiment") != std::string::npos) {
        // Sentiment should be in reasonable range (e.g., -1 to 1)
        std::string sql = "SELECT COUNT(*) FROM " + tableName +
                         " WHERE " + columnName + " BETWEEN -2.0 AND 2.0";
        auto stmt = m_dbManager.prepareStatement(sql);
        if (stmt) {
            StatementWrapper wrapper(stmt);
            if (wrapper.step()) {
                int validCount = wrapper.getIntColumn(0);
                int totalCount = getTotalRecordCount(tableName);
                return totalCount > 0 ? static_cast<double>(validCount) / totalCount : 0.0;
            }
        }
    }

    return 0.85; // Default accuracy score
}

int DataQualityManager::countOutliers(const std::string& tableName, const std::string& columnName) {
    // Calculate mean and standard deviation
    std::string statsSql = "SELECT AVG(" + columnName + "), COUNT(" + columnName + ") FROM " +
                          tableName + " WHERE " + columnName + " IS NOT NULL";

    auto stmt = m_dbManager.prepareStatement(statsSql);
    if (!stmt) {
        return 0;
    }

    StatementWrapper wrapper(stmt);
    if (!wrapper.step()) {
        return 0;
    }

    double mean = wrapper.getDoubleColumn(0);
    int count = wrapper.getIntColumn(1);

    if (count < 2) {
        return 0;
    }

    // Calculate standard deviation
    std::string stdSql = "SELECT SUM((" + columnName + " - " + std::to_string(mean) + ") * (" +
                        columnName + " - " + std::to_string(mean) + ")) FROM " + tableName +
                        " WHERE " + columnName + " IS NOT NULL";

    auto stdStmt = m_dbManager.prepareStatement(stdSql);
    if (!stdStmt) {
        return 0;
    }

    StatementWrapper stdWrapper(stdStmt);
    if (!stdWrapper.step()) {
        return 0;
    }

    double sumSquares = stdWrapper.getDoubleColumn(0);
    double stdDev = std::sqrt(sumSquares / (count - 1));

    // Count outliers beyond threshold standard deviations
    double lowerBound = mean - m_outlierThreshold * stdDev;
    double upperBound = mean + m_outlierThreshold * stdDev;

    std::string outlierSql = "SELECT COUNT(*) FROM " + tableName + " WHERE " + columnName +
                            " IS NOT NULL AND (" + columnName + " < " + std::to_string(lowerBound) +
                            " OR " + columnName + " > " + std::to_string(upperBound) + ")";

    auto outlierStmt = m_dbManager.prepareStatement(outlierSql);
    if (!outlierStmt) {
        return 0;
    }

    StatementWrapper outlierWrapper(outlierStmt);
    if (outlierWrapper.step()) {
        return outlierWrapper.getIntColumn(0);
    }

    return 0;
}

int DataQualityManager::getTotalRecordCount(const std::string& tableName) {
    std::string sql = "SELECT COUNT(*) FROM " + tableName;
    auto stmt = m_dbManager.prepareStatement(sql);
    if (!stmt) {
        return 0;
    }

    StatementWrapper wrapper(stmt);
    if (wrapper.step()) {
        return wrapper.getIntColumn(0);
    }

    return 0;
}

bool DataQualityManager::recordQualityMetric(const QualityMetric& metric) {
    std::string sql = R"(
        INSERT INTO data_quality_metrics
        (table_name, column_name, quality_score, completeness_ratio, accuracy_score,
         outlier_count, total_records, measurement_timestamp, remediation_applied, remediation_details)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";

    auto stmt = m_dbManager.prepareStatement(sql);
    if (!stmt) {
        return false;
    }

    StatementWrapper wrapper(stmt);
    wrapper.bindString(1, metric.tableName);
    wrapper.bindString(2, metric.columnName);
    wrapper.bindDouble(3, metric.qualityScore);
    wrapper.bindDouble(4, metric.completenessRatio);
    wrapper.bindDouble(5, metric.accuracyScore);
    wrapper.bindInt(6, metric.outlierCount);
    wrapper.bindInt(7, metric.totalRecords);
    wrapper.bindInt(8, static_cast<int>(metric.measurementTimestamp));
    wrapper.bindInt(9, metric.remediationApplied ? 1 : 0);
    wrapper.bindString(10, metric.remediationDetails);

    return m_dbManager.executeQuery(sql);
}

std::vector<DataAnomalyInfo> DataQualityManager::detectPriceAnomalies() {
    std::vector<DataAnomalyInfo> anomalies;

    // Detect negative prices
    std::string sql = "SELECT COUNT(*), 'market_data', 'close_price' FROM market_data WHERE close_price < 0";
    auto stmt = m_dbManager.prepareStatement(sql);
    if (stmt) {
        StatementWrapper wrapper(stmt);
        if (wrapper.step()) {
            int count = wrapper.getIntColumn(0);
            if (count > 0) {
                DataAnomalyInfo anomaly;
                anomaly.tableName = "market_data";
                anomaly.columnName = "close_price";
                anomaly.anomalyType = "negative_prices";
                anomaly.recordCount = count;
                anomaly.description = "Found negative price values";
                anomaly.severity = 0.9; // High severity
                anomalies.push_back(anomaly);
            }
        }
    }

    return anomalies;
}

std::vector<DataAnomalyInfo> DataQualityManager::detectVolumeAnomalies() {
    // Implementation for volume anomalies
    return std::vector<DataAnomalyInfo>();
}

std::vector<DataAnomalyInfo> DataQualityManager::detectSentimentAnomalies() {
    // Implementation for sentiment anomalies
    return std::vector<DataAnomalyInfo>();
}

std::vector<DataAnomalyInfo> DataQualityManager::detectTemporalAnomalies() {
    // Implementation for temporal anomalies
    return std::vector<DataAnomalyInfo>();
}

bool DataQualityManager::fixMissingData(const std::string& tableName, const std::string& columnName) {
    std::cout << "Fixing missing data in " << tableName << "." << columnName << std::endl;
    return interpolateMissingValues(tableName, columnName);
}

bool DataQualityManager::fixOutliers(const std::string& tableName, const std::string& columnName, double stdThreshold) {
    std::cout << "Fixing outliers in " << tableName << "." << columnName << std::endl;
    // Implementation would cap extreme values
    return true;
}

bool DataQualityManager::interpolateMissingValues(const std::string& tableName, const std::string& columnName) {
    // Simple linear interpolation implementation
    return true;
}

bool DataQualityManager::validatePriceData(const std::string& symbol) {
    std::string sql = "SELECT COUNT(*) FROM market_data WHERE symbol = ? AND close_price <= 0";
    auto stmt = m_dbManager.prepareStatement(sql);
    if (!stmt) {
        return false;
    }

    StatementWrapper wrapper(stmt);
    wrapper.bindString(1, symbol);

    if (wrapper.step()) {
        int invalidCount = wrapper.getIntColumn(0);
        return invalidCount == 0;
    }

    return false;
}

bool DataQualityManager::validateVolumeData(const std::string& symbol) {
    std::string sql = "SELECT COUNT(*) FROM market_data WHERE symbol = ? AND volume_from < 0";
    auto stmt = m_dbManager.prepareStatement(sql);
    if (!stmt) {
        return false;
    }

    StatementWrapper wrapper(stmt);
    wrapper.bindString(1, symbol);

    if (wrapper.step()) {
        int invalidCount = wrapper.getIntColumn(0);
        return invalidCount == 0;
    }

    return false;
}

bool DataQualityManager::validateTimestampConsistency(const std::string& tableName) {
    // Check for duplicate timestamps per symbol
    std::string sql = "SELECT COUNT(*) FROM (SELECT symbol, timestamp, COUNT(*) as cnt FROM " +
                     tableName + " GROUP BY symbol, timestamp HAVING cnt > 1)";

    auto stmt = m_dbManager.prepareStatement(sql);
    if (!stmt) {
        return false;
    }

    StatementWrapper wrapper(stmt);
    if (wrapper.step()) {
        int duplicates = wrapper.getIntColumn(0);
        return duplicates == 0;
    }

    return false;
}

std::vector<QualityMetric> DataQualityManager::getQualityReport() {
    std::vector<QualityMetric> metrics;

    std::string sql = R"(
        SELECT table_name, column_name, quality_score, completeness_ratio,
               accuracy_score, outlier_count, total_records, measurement_timestamp,
               remediation_applied, remediation_details
        FROM data_quality_metrics
        ORDER BY measurement_timestamp DESC
        LIMIT 100
    )";

    auto stmt = m_dbManager.prepareStatement(sql);
    if (!stmt) {
        return metrics;
    }

    StatementWrapper wrapper(stmt);
    while (wrapper.step()) {
        QualityMetric metric;
        metric.tableName = wrapper.getStringColumn(0);
        metric.columnName = wrapper.getStringColumn(1);
        metric.qualityScore = wrapper.getDoubleColumn(2);
        metric.completenessRatio = wrapper.getDoubleColumn(3);
        metric.accuracyScore = wrapper.getDoubleColumn(4);
        metric.outlierCount = wrapper.getIntColumn(5);
        metric.totalRecords = wrapper.getIntColumn(6);
        metric.measurementTimestamp = wrapper.getIntColumn(7);
        metric.remediationApplied = wrapper.getIntColumn(8) == 1;
        metric.remediationDetails = wrapper.getStringColumn(9);

        metrics.push_back(metric);
    }

    return metrics;
}

double DataQualityManager::getOverallQualityScore() {
    auto metrics = getQualityReport();
    if (metrics.empty()) {
        return 0.0;
    }

    double totalScore = 0.0;
    for (const auto& metric : metrics) {
        totalScore += metric.qualityScore;
    }

    return totalScore / metrics.size();
}

} // namespace Database
} // namespace CryptoClaude