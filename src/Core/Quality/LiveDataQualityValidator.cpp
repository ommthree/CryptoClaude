#include "LiveDataQualityValidator.h"
#include <algorithm>
#include <numeric>
#include <random>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <cmath>

namespace CryptoClaude {
namespace Quality {

std::string DataQualityIssue::generateIssueId() const {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    std::stringstream ss;
    ss << "DQ_" << std::hex << timestamp << "_" << static_cast<int>(type);
    return ss.str();
}

LiveDataQualityValidator::LiveDataQualityValidator(DatabaseManager& dbManager)
    : dbManager_(dbManager), isValidating_(false) {

    // Initialize default configuration
    config_.minimumCompletenessThreshold = 0.95;
    config_.minimumAccuracyThreshold = 0.98;
    config_.minimumTimelinessThreshold = 0.90;
    config_.minimumConsistencyThreshold = 0.85;
    config_.minimumOverallQualityThreshold = 0.90;

    // Add default validation rules
    addValidationRule(createPriceRangeValidationRule());
    addValidationRule(createVolumeValidationRule());
    addValidationRule(createTimestampValidationRule());
    addValidationRule(createDataFreshnessValidationRule());
}

LiveDataQualityValidator::~LiveDataQualityValidator() {
    shutdown();
}

bool LiveDataQualityValidator::initialize() {
    try {
        if (!initializeDatabaseTables()) {
            std::cerr << "Failed to initialize data quality database tables" << std::endl;
            return false;
        }

        std::cout << "LiveDataQualityValidator initialized successfully" << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "LiveDataQualityValidator initialization failed: " << e.what() << std::endl;
        return false;
    }
}

void LiveDataQualityValidator::shutdown() {
    stopRealTimeValidation();
}

bool LiveDataQualityValidator::initializeDatabaseTables() {
    std::vector<std::string> createTableQueries = {
        R"(
            CREATE TABLE IF NOT EXISTS data_quality_metrics (
                provider_id TEXT,
                symbol TEXT,
                assessment_time INTEGER,
                expected_data_points INTEGER,
                received_data_points INTEGER,
                completeness_ratio REAL,
                valid_data_points INTEGER,
                invalid_data_points INTEGER,
                accuracy_ratio REAL,
                average_data_age INTEGER,
                max_data_age INTEGER,
                timeliness_score REAL,
                consistency_score REAL,
                overall_quality_score REAL,
                info_issues INTEGER DEFAULT 0,
                warning_issues INTEGER DEFAULT 0,
                error_issues INTEGER DEFAULT 0,
                critical_issues INTEGER DEFAULT 0,
                PRIMARY KEY (provider_id, symbol, assessment_time)
            )
        )",
        R"(
            CREATE TABLE IF NOT EXISTS data_quality_issues (
                issue_id TEXT PRIMARY KEY,
                issue_type INTEGER,
                severity INTEGER,
                provider_id TEXT,
                symbol TEXT,
                detected_at INTEGER,
                description TEXT,
                details TEXT,
                context TEXT,
                affected_data_points TEXT,
                is_resolved INTEGER DEFAULT 0,
                resolved_at INTEGER,
                resolution_action TEXT,
                resolved_by TEXT
            )
        )",
        R"(
            CREATE TABLE IF NOT EXISTS validation_rules (
                rule_id TEXT PRIMARY KEY,
                rule_name TEXT,
                description TEXT,
                is_enabled INTEGER DEFAULT 1,
                severity_on_failure INTEGER,
                parameters TEXT,
                applicable_symbols TEXT,
                applicable_providers TEXT
            )
        )"
    };

    for (const auto& query : createTableQueries) {
        if (!dbManager_.executeQuery(query)) {
            std::cerr << "Failed to create data quality table" << std::endl;
            return false;
        }
    }

    return true;
}

bool LiveDataQualityValidator::addValidationRule(const ValidationRule& rule) {
    try {
        // Check if rule already exists
        auto it = std::find_if(validationRules_.begin(), validationRules_.end(),
            [&rule](const ValidationRule& existing) {
                return existing.ruleId == rule.ruleId;
            });

        if (it != validationRules_.end()) {
            // Update existing rule
            *it = rule;
        } else {
            // Add new rule
            validationRules_.push_back(rule);
        }

        std::cout << "Added validation rule: " << rule.ruleName << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Failed to add validation rule " << rule.ruleId << ": " << e.what() << std::endl;
        return false;
    }
}

bool LiveDataQualityValidator::registerDataSource(const std::string& providerId,
                                                 std::shared_ptr<CryptoCompareProvider> provider) {
    if (!provider) {
        std::cerr << "Cannot register null data source: " << providerId << std::endl;
        return false;
    }

    dataSources_[providerId] = provider;
    std::cout << "Registered data source for validation: " << providerId << std::endl;
    return true;
}

DataQualityMetrics LiveDataQualityValidator::validateMarketData(
    const std::string& providerId,
    const std::string& symbol,
    const std::vector<MarketData>& data) {

    DataQualityMetrics metrics(providerId, symbol);

    if (data.empty()) {
        createQualityIssue(QualityIssueType::MISSING_DATA, QualityIssueSeverity::ERROR,
                         providerId, symbol, "No market data received");
        return metrics;
    }

    // Calculate completeness
    metrics.expectedDataPoints = 100; // Expected data points (configurable)
    metrics.receivedDataPoints = static_cast<int>(data.size());
    metrics.completenessRatio = calculateCompletenessRatio(
        metrics.expectedDataPoints, metrics.receivedDataPoints);

    // Validate each data point and calculate accuracy
    int validDataPoints = 0;
    int invalidDataPoints = 0;

    for (const auto& dataPoint : data) {
        bool isValid = true;

        // Apply all validation rules
        for (const auto& rule : validationRules_) {
            if (!rule.isEnabled) continue;

            // Check if rule applies to this symbol/provider
            if (!rule.applicableSymbols.empty() &&
                std::find(rule.applicableSymbols.begin(), rule.applicableSymbols.end(), symbol) ==
                rule.applicableSymbols.end()) {
                continue;
            }

            if (!rule.applicableProviders.empty() &&
                std::find(rule.applicableProviders.begin(), rule.applicableProviders.end(), providerId) ==
                rule.applicableProviders.end()) {
                continue;
            }

            if (!validateSingleDataPoint(dataPoint, rule, providerId)) {
                isValid = false;

                // Create quality issue
                createQualityIssue(QualityIssueType::VALIDATION_FAILURE, rule.severityOnFailure,
                                 providerId, symbol,
                                 "Validation rule failed: " + rule.ruleName,
                                 "Data point failed validation at " +
                                 std::to_string(std::chrono::system_clock::to_time_t(dataPoint.getTimestamp())));

                // Count by severity
                switch (rule.severityOnFailure) {
                    case QualityIssueSeverity::INFO: metrics.infoIssues++; break;
                    case QualityIssueSeverity::WARNING: metrics.warningIssues++; break;
                    case QualityIssueSeverity::ERROR: metrics.errorIssues++; break;
                    case QualityIssueSeverity::CRITICAL: metrics.criticalIssues++; break;
                }
            }
        }

        if (isValid) {
            validDataPoints++;
        } else {
            invalidDataPoints++;
        }
    }

    metrics.validDataPoints = validDataPoints;
    metrics.invalidDataPoints = invalidDataPoints;
    metrics.accuracyRatio = calculateAccuracyRatio(validDataPoints, static_cast<int>(data.size()));

    // Calculate timeliness score
    metrics.timelinessScore = calculateTimelinessScore(data);

    // Calculate data age metrics
    if (!data.empty()) {
        auto now = std::chrono::system_clock::now();
        std::vector<std::chrono::minutes> ages;

        for (const auto& dataPoint : data) {
            auto age = std::chrono::duration_cast<std::chrono::minutes>(
                now - dataPoint.getTimestamp());
            ages.push_back(age);
        }

        metrics.averageDataAge = std::chrono::minutes(
            std::accumulate(ages.begin(), ages.end(), std::chrono::minutes(0)).count() / ages.size());
        metrics.maxDataAge = *std::max_element(ages.begin(), ages.end());
    }

    // Calculate consistency score (placeholder - would need multiple providers)
    metrics.consistencyScore = 1.0; // Default to perfect consistency

    // Calculate overall quality score
    metrics.overallQualityScore = calculateOverallQualityScore(metrics);

    // Store metrics in cache and persist
    qualityMetrics_[providerId + "_" + symbol] = metrics;
    persistQualityMetrics(metrics);

    // Check if metrics meet quality threshold
    if (!metrics.meetsQualityThreshold(config_.minimumOverallQualityThreshold)) {
        createQualityIssue(QualityIssueType::VALIDATION_FAILURE, QualityIssueSeverity::WARNING,
                         providerId, symbol,
                         "Data quality below threshold",
                         "Overall quality score: " + std::to_string(metrics.overallQualityScore) +
                         ", Threshold: " + std::to_string(config_.minimumOverallQualityThreshold));
    }

    // Trigger callback
    if (metricsCallback_) {
        metricsCallback_(metrics);
    }

    return metrics;
}

std::vector<DataQualityIssue> LiveDataQualityValidator::validateDataPoint(
    const std::string& providerId, const MarketData& dataPoint) {

    std::vector<DataQualityIssue> issues;

    for (const auto& rule : validationRules_) {
        if (!rule.isEnabled) continue;

        // Check if rule applies to this symbol/provider
        const std::string& symbol = dataPoint.getSymbol();

        if (!rule.applicableSymbols.empty() &&
            std::find(rule.applicableSymbols.begin(), rule.applicableSymbols.end(), symbol) ==
            rule.applicableSymbols.end()) {
            continue;
        }

        if (!rule.applicableProviders.empty() &&
            std::find(rule.applicableProviders.begin(), rule.applicableProviders.end(), providerId) ==
            rule.applicableProviders.end()) {
            continue;
        }

        if (!validateSingleDataPoint(dataPoint, rule, providerId)) {
            DataQualityIssue issue(QualityIssueType::VALIDATION_FAILURE, rule.severityOnFailure,
                                 providerId, symbol,
                                 "Validation rule failed: " + rule.ruleName);

            issue.details = "Data point validation failed at " +
                          std::to_string(std::chrono::system_clock::to_time_t(dataPoint.getTimestamp()));
            issue.context["rule_id"] = rule.ruleId;
            issue.context["price"] = std::to_string(dataPoint.getPrice());
            issue.context["volume"] = std::to_string(dataPoint.getVolume());

            issues.push_back(issue);
        }
    }

    return issues;
}

void LiveDataQualityValidator::startRealTimeValidation() {
    if (isValidating_) {
        return;
    }

    isValidating_ = true;
    validationThread_ = std::make_unique<std::thread>(&LiveDataQualityValidator::validationLoop, this);

    std::cout << "Started real-time data quality validation" << std::endl;
}

void LiveDataQualityValidator::stopRealTimeValidation() {
    if (!isValidating_) {
        return;
    }

    isValidating_ = false;
    if (validationThread_ && validationThread_->joinable()) {
        validationThread_->join();
    }

    std::cout << "Stopped real-time data quality validation" << std::endl;
}

void LiveDataQualityValidator::validationLoop() {
    while (isValidating_) {
        try {
            // Validate data from all registered sources
            for (const auto& sourceEntry : dataSources_) {
                const std::string& providerId = sourceEntry.first;
                auto provider = sourceEntry.second;

                // Test with default symbols
                std::vector<std::string> testSymbols = {"BTC", "ETH", "ADA"};

                for (const auto& symbol : testSymbols) {
                    try {
                        auto response = provider->getCurrentPrice(symbol, "USD");
                        if (response.success && !response.data.empty()) {
                            validateMarketData(providerId, symbol, response.data);
                        } else {
                            createQualityIssue(QualityIssueType::PROVIDER_ERROR,
                                             QualityIssueSeverity::ERROR,
                                             providerId, symbol,
                                             "Failed to retrieve market data",
                                             response.errorMessage);
                        }
                    } catch (const std::exception& e) {
                        createQualityIssue(QualityIssueType::NETWORK_ISSUE,
                                         QualityIssueSeverity::ERROR,
                                         providerId, symbol,
                                         "Network error during data retrieval",
                                         e.what());
                    }
                }
            }

            // Wait for next validation interval
            std::this_thread::sleep_for(config_.validationInterval);

        } catch (const std::exception& e) {
            std::cerr << "Validation loop error: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(10)); // Backoff on error
        }
    }
}

bool LiveDataQualityValidator::validateSingleDataPoint(const MarketData& data,
                                                     const ValidationRule& rule,
                                                     const std::string& providerId) {
    try {
        if (rule.validate) {
            return rule.validate(data, rule.parameters);
        }
        return true; // Default to valid if no validation function

    } catch (const std::exception& e) {
        std::cerr << "Validation rule " << rule.ruleId << " failed: " << e.what() << std::endl;
        return false;
    }
}

double LiveDataQualityValidator::calculateCompletenessRatio(int expected, int received) const {
    if (expected <= 0) return 1.0;
    return std::min(1.0, static_cast<double>(received) / expected);
}

double LiveDataQualityValidator::calculateAccuracyRatio(int valid, int total) const {
    if (total <= 0) return 1.0;
    return static_cast<double>(valid) / total;
}

double LiveDataQualityValidator::calculateTimelinessScore(const std::vector<MarketData>& data) const {
    if (data.empty()) return 0.0;

    auto now = std::chrono::system_clock::now();
    int freshDataPoints = 0;

    for (const auto& dataPoint : data) {
        auto age = std::chrono::duration_cast<std::chrono::minutes>(
            now - dataPoint.getTimestamp());

        if (age <= config_.maxDataAge) {
            freshDataPoints++;
        }
    }

    return static_cast<double>(freshDataPoints) / data.size();
}

double LiveDataQualityValidator::calculateOverallQualityScore(const DataQualityMetrics& metrics) const {
    // Weighted average of quality components
    const double completenessWeight = 0.25;
    const double accuracyWeight = 0.35;
    const double timelinessWeight = 0.25;
    const double consistencyWeight = 0.15;

    return (metrics.completenessRatio * completenessWeight) +
           (metrics.accuracyRatio * accuracyWeight) +
           (metrics.timelinessScore * timelinessWeight) +
           (metrics.consistencyScore * consistencyWeight);
}

void LiveDataQualityValidator::createQualityIssue(QualityIssueType type,
                                                 QualityIssueSeverity severity,
                                                 const std::string& providerId,
                                                 const std::string& symbol,
                                                 const std::string& description,
                                                 const std::string& details) {
    DataQualityIssue issue(type, severity, providerId, symbol, description);
    issue.details = details;

    // Add to active issues
    activeIssues_.push_back(issue);

    // Persist to database
    persistQualityIssue(issue);

    // Trigger callback
    if (issueCallback_) {
        issueCallback_(issue);
    }

    std::cout << "Created data quality issue: " << qualityIssueSeverityToString(severity)
              << " - " << description << " (" << providerId << "/" << symbol << ")" << std::endl;
}

std::vector<DataQualityIssue> LiveDataQualityValidator::getActiveIssues() const {
    std::vector<DataQualityIssue> active;
    std::copy_if(activeIssues_.begin(), activeIssues_.end(),
                std::back_inserter(active),
                [](const DataQualityIssue& issue) { return !issue.isResolved; });
    return active;
}

LiveDataQualityValidator::QualityReport LiveDataQualityValidator::generateQualityReport() const {
    QualityReport report;
    report.reportTime = std::chrono::system_clock::now();
    report.totalDataSources = static_cast<int>(dataSources_.size());

    // Aggregate metrics
    std::vector<double> qualityScores;
    int sourcesWithIssues = 0;

    for (const auto& metricEntry : qualityMetrics_) {
        const auto& metrics = metricEntry.second;
        report.providerMetrics[metricEntry.first] = metrics;

        qualityScores.push_back(metrics.overallQualityScore);

        if (metrics.criticalIssues > 0 || metrics.errorIssues > 0 || metrics.warningIssues > 0) {
            sourcesWithIssues++;
        }
    }

    report.healthyDataSources = report.totalDataSources - sourcesWithIssues;
    report.dataSourcesWithIssues = sourcesWithIssues;

    // Calculate average quality score
    if (!qualityScores.empty()) {
        report.averageQualityScore = std::accumulate(qualityScores.begin(), qualityScores.end(), 0.0) /
                                   qualityScores.size();
    }

    // Count issues by severity
    for (const auto& issue : activeIssues_) {
        if (!issue.isResolved) {
            report.totalActiveIssues++;

            switch (issue.severity) {
                case QualityIssueSeverity::INFO:
                    break;
                case QualityIssueSeverity::WARNING:
                    report.warningIssues++;
                    break;
                case QualityIssueSeverity::ERROR:
                    report.errorIssues++;
                    break;
                case QualityIssueSeverity::CRITICAL:
                    report.criticalIssues++;
                    report.criticalIssuesList.push_back(issue);
                    break;
            }
        }
    }

    // Generate recommendations
    if (report.averageQualityScore < 0.90) {
        report.recommendations.push_back("Review data provider configurations for quality improvements");
    }
    if (report.criticalIssues > 0) {
        report.recommendations.push_back("Address critical data quality issues immediately");
    }
    if (sourcesWithIssues > report.totalDataSources / 2) {
        report.recommendations.push_back("Multiple data sources showing issues - investigate network or configuration problems");
    }

    return report;
}

void LiveDataQualityValidator::persistQualityMetrics(const DataQualityMetrics& metrics) {
    try {
        std::string query = R"(
            INSERT OR REPLACE INTO data_quality_metrics
            (provider_id, symbol, assessment_time, expected_data_points, received_data_points,
             completeness_ratio, valid_data_points, invalid_data_points, accuracy_ratio,
             average_data_age, max_data_age, timeliness_score, consistency_score,
             overall_quality_score, info_issues, warning_issues, error_issues, critical_issues)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        )";

        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
            metrics.assessmentTime.time_since_epoch()).count();

        dbManager_.executeParameterizedQuery(query, {
            metrics.providerId,
            metrics.symbol,
            std::to_string(timestamp),
            std::to_string(metrics.expectedDataPoints),
            std::to_string(metrics.receivedDataPoints),
            std::to_string(metrics.completenessRatio),
            std::to_string(metrics.validDataPoints),
            std::to_string(metrics.invalidDataPoints),
            std::to_string(metrics.accuracyRatio),
            std::to_string(metrics.averageDataAge.count()),
            std::to_string(metrics.maxDataAge.count()),
            std::to_string(metrics.timelinessScore),
            std::to_string(metrics.consistencyScore),
            std::to_string(metrics.overallQualityScore),
            std::to_string(metrics.infoIssues),
            std::to_string(metrics.warningIssues),
            std::to_string(metrics.errorIssues),
            std::to_string(metrics.criticalIssues)
        });

    } catch (const std::exception& e) {
        std::cerr << "Failed to persist quality metrics: " << e.what() << std::endl;
    }
}

void LiveDataQualityValidator::persistQualityIssue(const DataQualityIssue& issue) {
    try {
        std::string query = R"(
            INSERT OR REPLACE INTO data_quality_issues
            (issue_id, issue_type, severity, provider_id, symbol, detected_at,
             description, details, is_resolved)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
        )";

        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
            issue.detectedAt.time_since_epoch()).count();

        dbManager_.executeParameterizedQuery(query, {
            issue.issueId,
            std::to_string(static_cast<int>(issue.type)),
            std::to_string(static_cast<int>(issue.severity)),
            issue.providerId,
            issue.symbol,
            std::to_string(timestamp),
            issue.description,
            issue.details,
            issue.isResolved ? "1" : "0"
        });

    } catch (const std::exception& e) {
        std::cerr << "Failed to persist quality issue: " << e.what() << std::endl;
    }
}

std::string LiveDataQualityValidator::qualityIssueSeverityToString(QualityIssueSeverity severity) const {
    switch (severity) {
        case QualityIssueSeverity::INFO: return "INFO";
        case QualityIssueSeverity::WARNING: return "WARNING";
        case QualityIssueSeverity::ERROR: return "ERROR";
        case QualityIssueSeverity::CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

// Built-in validation rules
ValidationRule LiveDataQualityValidator::createPriceRangeValidationRule() {
    ValidationRule rule("price_range", "Price Range Validation",
        [](const MarketData& data, const std::map<std::string, std::string>& params) {
            const std::string& symbol = data.getSymbol();
            double price = data.getPrice();

            if (price <= 0) return false;

            // Default reasonable ranges
            if (symbol == "BTC" && (price < 1000.0 || price > 1000000.0)) return false;
            if (symbol == "ETH" && (price < 100.0 || price > 50000.0)) return false;
            if (symbol == "ADA" && (price < 0.1 || price > 10.0)) return false;

            return true;
        });

    rule.description = "Validates that price is within reasonable range for the symbol";
    rule.severityOnFailure = QualityIssueSeverity::ERROR;
    return rule;
}

ValidationRule LiveDataQualityValidator::createTimestampValidationRule() {
    ValidationRule rule("timestamp", "Timestamp Validation",
        [](const MarketData& data, const std::map<std::string, std::string>& params) {
            auto now = std::chrono::system_clock::now();
            auto dataTime = data.getTimestamp();

            // Check if timestamp is not in the future or too old (more than 1 hour)
            auto age = std::chrono::duration_cast<std::chrono::hours>(now - dataTime);
            auto futureCheck = std::chrono::duration_cast<std::chrono::minutes>(dataTime - now);

            return age <= std::chrono::hours(1) && futureCheck <= std::chrono::minutes(5);
        });

    rule.description = "Validates that timestamp is reasonable (not too old or in future)";
    rule.severityOnFailure = QualityIssueSeverity::WARNING;
    return rule;
}

ValidationRule LiveDataQualityValidator::createDataFreshnessValidationRule() {
    ValidationRule rule("data_freshness", "Data Freshness Validation",
        [](const MarketData& data, const std::map<std::string, std::string>& params) {
            auto now = std::chrono::system_clock::now();
            auto dataTime = data.getTimestamp();
            auto age = std::chrono::duration_cast<std::chrono::minutes>(now - dataTime);

            return age <= std::chrono::minutes(10); // Data should be less than 10 minutes old
        });

    rule.description = "Validates that data is fresh (less than 10 minutes old)";
    rule.severityOnFailure = QualityIssueSeverity::WARNING;
    return rule;
}

ValidationRule LiveDataQualityValidator::createVolumeValidationRule() {
    ValidationRule rule("volume", "Volume Validation",
        [](const MarketData& data, const std::map<std::string, std::string>& params) {
            double volume = data.getVolume();
            return volume >= 0.0; // Volume should be non-negative
        });

    rule.description = "Validates that volume is non-negative";
    rule.severityOnFailure = QualityIssueSeverity::WARNING;
    return rule;
}

} // namespace Quality
} // namespace CryptoClaude