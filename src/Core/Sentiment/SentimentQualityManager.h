#pragma once

#include "../Database/DataQualityManager.h"
#include "../Data/Providers/CryptoNewsProvider.h"
#include "../Database/Models/SentimentData.h"
#include <memory>
#include <vector>
#include <map>
#include <functional>

namespace CryptoClaude {
namespace Sentiment {

using namespace CryptoClaude::Database;
using namespace CryptoClaude::Data::Providers;

// Sentiment-specific quality metrics
struct SentimentQualityMetric {
    std::string source;
    std::string symbol;
    std::chrono::system_clock::time_point timestamp;

    // Data completeness
    double dataCompletenessScore; // 0-1, how complete the sentiment data is
    int expectedArticleCount;
    int actualArticleCount;

    // Data accuracy
    double sentimentAccuracyScore; // 0-1, based on validation against market movements
    double predictionAccuracy; // Historical accuracy of sentiment predictions
    double sourceCredibilityScore; // Weighted average of source credibilities

    // Data freshness
    double freshnessScore; // 0-1, how recent the sentiment data is
    std::chrono::minutes averageArticleAge;
    std::chrono::minutes maxAcceptableAge;

    // Data consistency
    double consistencyScore; // 0-1, consistency across different sources
    double sentimentVariance; // Variance in sentiment across sources
    int consensusSourceCount; // Number of sources in agreement

    // Market relevance
    double marketRelevanceScore; // 0-1, relevance to current market conditions
    double volumeCorrelation; // Correlation with trading volume
    double priceCorrelation; // Correlation with price movements
    double socialEngagementScore; // Social media engagement level

    // Overall quality score
    double overallQualityScore; // Weighted average of all metrics

    SentimentQualityMetric() : dataCompletenessScore(0.0), expectedArticleCount(0),
                              actualArticleCount(0), sentimentAccuracyScore(0.0),
                              predictionAccuracy(0.0), sourceCredibilityScore(0.0),
                              freshnessScore(0.0), consistencyScore(0.0),
                              sentimentVariance(0.0), consensusSourceCount(0),
                              marketRelevanceScore(0.0), volumeCorrelation(0.0),
                              priceCorrelation(0.0), socialEngagementScore(0.0),
                              overallQualityScore(0.0) {}
};

// Sentiment anomaly detection
struct SentimentAnomaly {
    std::string anomalyType;
    std::string description;
    std::string affectedSymbol;
    std::string source;
    std::chrono::system_clock::time_point detectedAt;
    double severity; // 0-1, severity of the anomaly
    double confidence; // 0-1, confidence in anomaly detection

    // Anomaly-specific data
    double expectedValue;
    double actualValue;
    double deviationMagnitude;
    std::vector<std::string> relatedSources;
    bool requiresInvestigation;

    SentimentAnomaly() : severity(0.0), confidence(0.0), expectedValue(0.0),
                        actualValue(0.0), deviationMagnitude(0.0),
                        requiresInvestigation(false) {}
};

// Comprehensive sentiment quality management
class SentimentQualityManager {
public:
    SentimentQualityManager(DatabaseManager& dbManager);
    ~SentimentQualityManager() = default;

    // Initialization and configuration
    bool initialize();
    void setQualityThresholds(const std::map<std::string, double>& thresholds);
    void setAnomalyDetectionSensitivity(double sensitivity);
    void enableRealTimeMonitoring(bool enable);

    // Quality assessment
    SentimentQualityMetric assessSentimentQuality(
        const std::string& symbol,
        const std::vector<Database::Models::SentimentData>& sentimentData,
        const std::chrono::system_clock::time_point& assessmentTime = std::chrono::system_clock::now());

    std::vector<SentimentQualityMetric> assessMultiSymbolQuality(
        const std::vector<std::string>& symbols,
        const std::chrono::hours& timeWindow = std::chrono::hours(24));

    double getOverallSentimentQualityScore() const;

    // Anomaly detection
    std::vector<SentimentAnomaly> detectSentimentAnomalies(
        const std::string& symbol,
        const std::vector<Database::Models::SentimentData>& recentData);

    std::vector<SentimentAnomaly> detectCrossSymbolAnomalies(
        const std::map<std::string, std::vector<Database::Models::SentimentData>>& multiSymbolData);

    std::vector<SentimentAnomaly> detectSourceAnomalies(
        const std::map<std::string, std::vector<CryptoNewsArticle>>& sourceData);

    // Quality remediation
    struct RemediationAction {
        std::string actionType;
        std::string description;
        std::string targetSymbol;
        std::string targetSource;
        std::function<bool()> executeAction;
        double expectedImprovement; // Expected quality score improvement
        bool isAutomated; // Whether action can be executed automatically
    };

    std::vector<RemediationAction> generateRemediationActions(
        const std::vector<SentimentAnomaly>& anomalies);

    bool executeAutomatedRemediation(const std::vector<RemediationAction>& actions);

    // Source quality management
    struct SourceQualityProfile {
        std::string sourceName;
        double overallQuality; // 0-1
        double reliabilityScore; // Historical reliability
        double timeliness; // How quickly they report news
        double accuracy; // Historical prediction accuracy
        double coverage; // Market coverage breadth
        double uniqueness; // How unique their content is
        int articlesAnalyzed;
        std::chrono::system_clock::time_point lastUpdate;
    };

    std::vector<SourceQualityProfile> getSourceQualityProfiles() const;
    void updateSourceQuality(const std::string& source, const SourceQualityProfile& profile);
    std::vector<std::string> getRecommendedSources(const std::string& symbol) const;

    // Quality monitoring and reporting
    struct QualityReport {
        std::chrono::system_clock::time_point reportTime;
        double overallQuality;
        int totalSymbolsMonitored;
        int totalSourcesMonitored;
        int activeAnomalies;
        int remediationActionsExecuted;

        std::map<std::string, double> symbolQualityScores;
        std::map<std::string, double> sourceQualityScores;
        std::vector<SentimentAnomaly> criticalAnomalies;
        std::string qualityTrend; // "IMPROVING", "STABLE", "DECLINING"
    };

    QualityReport generateQualityReport() const;
    void scheduledQualityAssessment(); // Called by scheduler/timer
    std::vector<QualityReport> getHistoricalReports(int daysBack = 7) const;

    // Validation and backtesting
    struct ValidationResult {
        std::string symbol;
        std::chrono::system_clock::time_point validationPeriodStart;
        std::chrono::system_clock::time_point validationPeriodEnd;

        double sentimentPredictionAccuracy;
        double priceMovementCorrelation;
        double volumeMovementCorrelation;
        double falsePositiveRate;
        double falseNegativeRate;

        std::map<std::string, double> sourceAccuracies;
        std::vector<std::string> recommendationForImprovement;
    };

    ValidationResult validateSentimentQuality(
        const std::string& symbol,
        const std::chrono::system_clock::time_point& startTime,
        const std::chrono::system_clock::time_point& endTime);

    // Advanced analytics
    struct SentimentQualityTrend {
        std::string symbol;
        std::vector<std::pair<std::chrono::system_clock::time_point, double>> qualityTimeline;
        double currentTrendDirection; // -1 to 1, negative = declining quality
        std::string primaryQualityDriver; // What's driving quality changes
        std::vector<std::string> recommendedActions;
    };

    std::vector<SentimentQualityTrend> analyzeSentimentQualityTrends(
        const std::vector<std::string>& symbols,
        int daysBack = 30);

    // Integration with existing DataQualityManager
    void integrateWithDataQualityManager(std::shared_ptr<DataQualityManager> dataQualityManager);
    std::vector<DataAnomalyInfo> convertSentimentAnomaliesToDataAnomalies(
        const std::vector<SentimentAnomaly>& sentimentAnomalies) const;

    // Configuration and thresholds
    struct QualityThresholds {
        double minimumDataCompleteness = 0.8;
        double minimumAccuracyScore = 0.7;
        double minimumFreshnessScore = 0.9;
        double minimumConsistencyScore = 0.6;
        double minimumRelevanceScore = 0.5;
        double minimumOverallQuality = 0.7;

        // Anomaly detection thresholds
        double anomalySensitivity = 0.8;
        double severityThreshold = 0.6;
        int minimumConsensusCount = 3;
    };

    void setQualityThresholds(const QualityThresholds& thresholds);
    QualityThresholds getQualityThresholds() const;

private:
    DatabaseManager& dbManager_;
    std::shared_ptr<DataQualityManager> dataQualityManager_;
    QualityThresholds thresholds_;
    bool realTimeMonitoringEnabled_;
    double anomalyDetectionSensitivity_;

    // Quality tracking
    mutable std::map<std::string, SourceQualityProfile> sourceProfiles_;
    mutable std::vector<SentimentAnomaly> activeAnomalies_;
    mutable std::vector<QualityReport> historicalReports_;

    // Quality assessment helpers
    double calculateDataCompleteness(const std::vector<Database::Models::SentimentData>& data,
                                    const std::string& symbol) const;
    double calculateAccuracyScore(const std::vector<Database::Models::SentimentData>& sentimentData,
                                 const std::string& symbol) const;
    double calculateFreshnessScore(const std::vector<Database::Models::SentimentData>& data) const;
    double calculateConsistencyScore(const std::vector<Database::Models::SentimentData>& data) const;
    double calculateRelevanceScore(const std::vector<Database::Models::SentimentData>& data,
                                  const std::string& symbol) const;

    // Anomaly detection helpers
    SentimentAnomaly detectSentimentSpike(const std::vector<Database::Models::SentimentData>& data,
                                         const std::string& symbol) const;
    SentimentAnomaly detectSentimentAbsence(const std::vector<Database::Models::SentimentData>& data,
                                           const std::string& symbol) const;
    SentimentAnomaly detectSourceInconsistency(
        const std::map<std::string, std::vector<Database::Models::SentimentData>>& sourceData) const;
    SentimentAnomaly detectStaleData(const std::vector<Database::Models::SentimentData>& data) const;

    // Remediation helpers
    bool recalibrateSource(const std::string& source);
    bool requestFreshData(const std::string& symbol);
    bool adjustQualityFilters(const std::string& source);
    bool blacklistUnreliableSource(const std::string& source);

    // Validation helpers
    double calculatePredictionAccuracy(const std::vector<Database::Models::SentimentData>& sentimentData,
                                      const std::string& symbol,
                                      const std::chrono::system_clock::time_point& startTime,
                                      const std::chrono::system_clock::time_point& endTime) const;

    // Utility functions
    std::vector<Database::Models::SentimentData> getHistoricalSentimentData(
        const std::string& symbol,
        const std::chrono::system_clock::time_point& startTime,
        const std::chrono::system_clock::time_point& endTime) const;
    double calculateWeightedAverage(const std::vector<double>& values,
                                   const std::vector<double>& weights) const;
    std::string generateAnomalyId(const SentimentAnomaly& anomaly) const;
    void logQualityEvent(const std::string& eventType, const std::string& details) const;
};

// Sentiment quality validator for integration testing
class SentimentQualityValidator {
public:
    SentimentQualityValidator(SentimentQualityManager& qualityManager);

    // Validation test suites
    struct ValidationTestSuite {
        std::string suiteName;
        std::vector<std::function<bool()>> tests;
        int passedTests = 0;
        int totalTests = 0;
        std::vector<std::string> failureReasons;
    };

    ValidationTestSuite runCompletenessTests();
    ValidationTestSuite runAccuracyTests();
    ValidationTestSuite runConsistencyTests();
    ValidationTestSuite runAnomalyDetectionTests();

    // Integration validation
    bool validateIntegrationWithDataPipeline();
    bool validateIntegrationWithDatabaseManager();
    bool validateRemediationActions();

    // Performance validation
    struct PerformanceMetrics {
        double averageQualityAssessmentTime;
        double averageAnomalyDetectionTime;
        double memoryUsage;
        int concurrentAssessmentsSupported;
    };

    PerformanceMetrics measurePerformance();

private:
    SentimentQualityManager& qualityManager_;

    // Test data generation
    std::vector<Database::Models::SentimentData> generateTestSentimentData(
        const std::string& symbol, int count, bool includeAnomalies = false);
    std::vector<SentimentAnomaly> generateTestAnomalies();
};

} // namespace Sentiment
} // namespace CryptoClaude