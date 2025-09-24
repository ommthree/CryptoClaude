#include "SentimentQualityManager.h"
#include <algorithm>
#include <numeric>
#include <cmath>

namespace CryptoClaude {
namespace Sentiment {

SentimentQualityManager::SentimentQualityManager(DatabaseManager& dbManager)
    : dbManager_(dbManager), realTimeMonitoringEnabled_(false), anomalyDetectionSensitivity_(0.8) {

    // Initialize default thresholds
    thresholds_.minimumDataCompleteness = 0.8;
    thresholds_.minimumAccuracyScore = 0.7;
    thresholds_.minimumFreshnessScore = 0.9;
    thresholds_.minimumConsistencyScore = 0.6;
    thresholds_.minimumRelevanceScore = 0.5;
    thresholds_.minimumOverallQuality = 0.7;
    thresholds_.anomalySensitivity = 0.8;
    thresholds_.severityThreshold = 0.6;
    thresholds_.minimumConsensusCount = 3;
}

bool SentimentQualityManager::initialize() {
    try {
        // Create sentiment quality monitoring tables
        std::vector<std::string> createTables = {
            R"(CREATE TABLE IF NOT EXISTS sentiment_quality_metrics (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                source TEXT NOT NULL,
                symbol TEXT NOT NULL,
                timestamp INTEGER NOT NULL,
                data_completeness_score REAL,
                sentiment_accuracy_score REAL,
                prediction_accuracy REAL,
                source_credibility_score REAL,
                freshness_score REAL,
                average_article_age INTEGER,
                consistency_score REAL,
                sentiment_variance REAL,
                consensus_source_count INTEGER,
                market_relevance_score REAL,
                volume_correlation REAL,
                price_correlation REAL,
                social_engagement_score REAL,
                overall_quality_score REAL,
                created_at INTEGER DEFAULT (strftime('%s', 'now'))
            ))",

            R"(CREATE TABLE IF NOT EXISTS sentiment_anomalies (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                anomaly_id TEXT UNIQUE NOT NULL,
                anomaly_type TEXT NOT NULL,
                description TEXT,
                affected_symbol TEXT,
                source TEXT,
                detected_at INTEGER NOT NULL,
                severity REAL,
                confidence REAL,
                expected_value REAL,
                actual_value REAL,
                deviation_magnitude REAL,
                requires_investigation INTEGER DEFAULT 0,
                is_resolved INTEGER DEFAULT 0,
                resolved_at INTEGER,
                resolution_notes TEXT
            ))",

            R"(CREATE TABLE IF NOT EXISTS source_quality_profiles (
                source_name TEXT PRIMARY KEY,
                overall_quality REAL,
                reliability_score REAL,
                timeliness REAL,
                accuracy REAL,
                coverage REAL,
                uniqueness REAL,
                articles_analyzed INTEGER DEFAULT 0,
                last_update INTEGER DEFAULT (strftime('%s', 'now'))
            ))"
        };

        for (const auto& tableQuery : createTables) {
            if (!dbManager_.executeQuery(tableQuery)) {
                return false;
            }
        }

        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

void SentimentQualityManager::setQualityThresholds(const std::map<std::string, double>& thresholds) {
    for (const auto& threshold : thresholds) {
        if (threshold.first == "data_completeness") {
            thresholds_.minimumDataCompleteness = threshold.second;
        } else if (threshold.first == "accuracy_score") {
            thresholds_.minimumAccuracyScore = threshold.second;
        } else if (threshold.first == "freshness_score") {
            thresholds_.minimumFreshnessScore = threshold.second;
        } else if (threshold.first == "consistency_score") {
            thresholds_.minimumConsistencyScore = threshold.second;
        } else if (threshold.first == "relevance_score") {
            thresholds_.minimumRelevanceScore = threshold.second;
        } else if (threshold.first == "overall_quality") {
            thresholds_.minimumOverallQuality = threshold.second;
        }
    }
}

void SentimentQualityManager::setAnomalyDetectionSensitivity(double sensitivity) {
    anomalyDetectionSensitivity_ = std::clamp(sensitivity, 0.0, 1.0);
    thresholds_.anomalySensitivity = anomalyDetectionSensitivity_;
}

void SentimentQualityManager::enableRealTimeMonitoring(bool enable) {
    realTimeMonitoringEnabled_ = enable;
}

SentimentQualityMetric SentimentQualityManager::assessSentimentQuality(
    const std::string& symbol,
    const std::vector<Database::Models::SentimentData>& sentimentData,
    const std::chrono::system_clock::time_point& assessmentTime) {

    SentimentQualityMetric metric;
    metric.source = "aggregated";
    metric.symbol = symbol;
    metric.timestamp = assessmentTime;

    if (sentimentData.empty()) {
        return metric; // Return empty metric for no data
    }

    // Calculate data completeness
    metric.expectedArticleCount = 100; // Expected articles per assessment period
    metric.actualArticleCount = static_cast<int>(sentimentData.size());
    metric.dataCompletenessScore = calculateDataCompleteness(sentimentData, symbol);

    // Calculate accuracy score
    metric.sentimentAccuracyScore = calculateAccuracyScore(sentimentData, symbol);

    // Calculate prediction accuracy (simplified - would need historical comparison)
    metric.predictionAccuracy = metric.sentimentAccuracyScore * 0.9; // Approximation

    // Calculate source credibility (average of all sources)
    double totalCredibility = 0.0;
    for (const auto& data : sentimentData) {
        totalCredibility += 0.8; // Default credibility score
    }
    metric.sourceCredibilityScore = sentimentData.empty() ? 0.0 : totalCredibility / sentimentData.size();

    // Calculate freshness score
    metric.freshnessScore = calculateFreshnessScore(sentimentData);

    // Calculate average article age
    if (!sentimentData.empty()) {
        auto now = std::chrono::system_clock::now();
        std::chrono::minutes totalAge{0};

        for (const auto& data : sentimentData) {
            auto age = std::chrono::duration_cast<std::chrono::minutes>(now - data.getTimestamp());
            totalAge += age;
        }

        metric.averageArticleAge = totalAge / static_cast<int>(sentimentData.size());
        metric.maxAcceptableAge = std::chrono::minutes(120); // 2 hours
    }

    // Calculate consistency score
    metric.consistencyScore = calculateConsistencyScore(sentimentData);

    // Calculate sentiment variance
    if (sentimentData.size() > 1) {
        std::vector<double> sentiments;
        for (const auto& data : sentimentData) {
            sentiments.push_back(data.getSentimentScore());
        }

        double mean = std::accumulate(sentiments.begin(), sentiments.end(), 0.0) / sentiments.size();
        double variance = 0.0;
        for (double sentiment : sentiments) {
            variance += (sentiment - mean) * (sentiment - mean);
        }
        metric.sentimentVariance = variance / sentiments.size();
    }

    // Calculate consensus source count (simplified)
    metric.consensusSourceCount = static_cast<int>(sentimentData.size() * 0.7); // Assume 70% consensus

    // Calculate market relevance score
    metric.marketRelevanceScore = calculateRelevanceScore(sentimentData, symbol);

    // Placeholder correlations (would need market data for real calculation)
    metric.volumeCorrelation = 0.6;
    metric.priceCorrelation = 0.5;
    metric.socialEngagementScore = 0.7;

    // Calculate overall quality score
    metric.overallQualityScore = calculateOverallQualityScore(metric);

    // Persist metric to database
    persistQualityMetric(metric);

    return metric;
}

std::vector<SentimentQualityMetric> SentimentQualityManager::assessMultiSymbolQuality(
    const std::vector<std::string>& symbols,
    const std::chrono::hours& timeWindow) {

    std::vector<SentimentQualityMetric> metrics;

    for (const auto& symbol : symbols) {
        // In a real implementation, we would fetch sentiment data for each symbol
        std::vector<Database::Models::SentimentData> sentimentData; // Placeholder
        auto metric = assessSentimentQuality(symbol, sentimentData);
        metrics.push_back(metric);
    }

    return metrics;
}

double SentimentQualityManager::getOverallSentimentQualityScore() const {
    // Calculate average quality score across all recent assessments
    // This would query the database in a real implementation
    return 0.75; // Placeholder value
}

std::vector<SentimentAnomaly> SentimentQualityManager::detectSentimentAnomalies(
    const std::string& symbol,
    const std::vector<Database::Models::SentimentData>& recentData) {

    std::vector<SentimentAnomaly> anomalies;

    if (recentData.size() < 5) {
        return anomalies; // Need at least 5 data points for anomaly detection
    }

    // Detect sentiment spikes
    auto spikeAnomaly = detectSentimentSpike(recentData, symbol);
    if (spikeAnomaly.confidence > thresholds_.severityThreshold) {
        anomalies.push_back(spikeAnomaly);
    }

    // Detect data absence
    auto absenceAnomaly = detectSentimentAbsence(recentData, symbol);
    if (absenceAnomaly.confidence > thresholds_.severityThreshold) {
        anomalies.push_back(absenceAnomaly);
    }

    // Detect stale data
    auto staleAnomaly = detectStaleData(recentData);
    if (staleAnomaly.confidence > thresholds_.severityThreshold) {
        anomalies.push_back(staleAnomaly);
    }

    // Persist anomalies to database
    for (const auto& anomaly : anomalies) {
        persistSentimentAnomaly(anomaly);
    }

    return anomalies;
}

double SentimentQualityManager::calculateDataCompleteness(
    const std::vector<Database::Models::SentimentData>& data,
    const std::string& symbol) const {

    if (data.empty()) {
        return 0.0;
    }

    // Simple completeness calculation based on expected vs actual data points
    int expectedDataPoints = 100; // Expected number of sentiment data points
    int actualDataPoints = static_cast<int>(data.size());

    return std::min(1.0, static_cast<double>(actualDataPoints) / expectedDataPoints);
}

double SentimentQualityManager::calculateAccuracyScore(
    const std::vector<Database::Models::SentimentData>& sentimentData,
    const std::string& symbol) const {

    if (sentimentData.empty()) {
        return 0.0;
    }

    // Simplified accuracy calculation
    // In a real implementation, this would compare sentiment predictions with actual market movements
    int validSentiments = 0;

    for (const auto& data : sentimentData) {
        double sentiment = data.getSentimentScore();
        if (sentiment >= -1.0 && sentiment <= 1.0) { // Valid sentiment range
            validSentiments++;
        }
    }

    return static_cast<double>(validSentiments) / sentimentData.size();
}

double SentimentQualityManager::calculateFreshnessScore(
    const std::vector<Database::Models::SentimentData>& data) const {

    if (data.empty()) {
        return 0.0;
    }

    auto now = std::chrono::system_clock::now();
    int freshDataPoints = 0;
    auto freshnessThreshold = std::chrono::minutes(30); // 30 minutes threshold

    for (const auto& dataPoint : data) {
        auto age = std::chrono::duration_cast<std::chrono::minutes>(now - dataPoint.getTimestamp());
        if (age <= freshnessThreshold) {
            freshDataPoints++;
        }
    }

    return static_cast<double>(freshDataPoints) / data.size();
}

double SentimentQualityManager::calculateConsistencyScore(
    const std::vector<Database::Models::SentimentData>& data) const {

    if (data.size() < 2) {
        return 1.0; // Perfect consistency with single or no data point
    }

    std::vector<double> sentiments;
    for (const auto& dataPoint : data) {
        sentiments.push_back(dataPoint.getSentimentScore());
    }

    // Calculate coefficient of variation (lower is more consistent)
    double mean = std::accumulate(sentiments.begin(), sentiments.end(), 0.0) / sentiments.size();

    if (mean == 0.0) {
        return 1.0; // Perfect consistency at zero
    }

    double variance = 0.0;
    for (double sentiment : sentiments) {
        variance += (sentiment - mean) * (sentiment - mean);
    }
    variance /= sentiments.size();
    double stdDev = std::sqrt(variance);

    double coefficientOfVariation = std::abs(stdDev / mean);

    // Convert to consistency score (higher is better)
    return std::max(0.0, 1.0 - coefficientOfVariation);
}

double SentimentQualityManager::calculateRelevanceScore(
    const std::vector<Database::Models::SentimentData>& data,
    const std::string& symbol) const {

    if (data.empty()) {
        return 0.0;
    }

    // Simplified relevance calculation
    // In a real implementation, this would analyze content relevance to the specific symbol
    int relevantDataPoints = 0;

    for (const auto& dataPoint : data) {
        // Check if the data point mentions the symbol or related terms
        if (dataPoint.getSymbol() == symbol) {
            relevantDataPoints++;
        }
    }

    return static_cast<double>(relevantDataPoints) / data.size();
}

double SentimentQualityManager::calculateOverallQualityScore(const SentimentQualityMetric& metric) const {
    // Weighted average of quality components
    double weights[] = {0.2, 0.25, 0.15, 0.15, 0.15, 0.1}; // Completeness, Accuracy, Freshness, Consistency, Relevance, Credibility
    double scores[] = {
        metric.dataCompletenessScore,
        metric.sentimentAccuracyScore,
        metric.freshnessScore,
        metric.consistencyScore,
        metric.marketRelevanceScore,
        metric.sourceCredibilityScore
    };

    double weightedSum = 0.0;
    for (int i = 0; i < 6; ++i) {
        weightedSum += weights[i] * scores[i];
    }

    return weightedSum;
}

SentimentAnomaly SentimentQualityManager::detectSentimentSpike(
    const std::vector<Database::Models::SentimentData>& data,
    const std::string& symbol) const {

    SentimentAnomaly anomaly;
    anomaly.anomalyType = "SENTIMENT_SPIKE";
    anomaly.affectedSymbol = symbol;
    anomaly.detectedAt = std::chrono::system_clock::now();

    if (data.size() < 5) {
        return anomaly;
    }

    // Calculate recent average and current value
    std::vector<double> recentSentiments;
    for (const auto& dataPoint : data) {
        recentSentiments.push_back(dataPoint.getSentimentScore());
    }

    double currentSentiment = recentSentiments.back();
    double avgSentiment = std::accumulate(recentSentiments.begin(), recentSentiments.end() - 1, 0.0) / (recentSentiments.size() - 1);

    double deviation = std::abs(currentSentiment - avgSentiment);

    if (deviation > 0.5) { // Significant deviation threshold
        anomaly.severity = std::min(1.0, deviation);
        anomaly.confidence = anomalyDetectionSensitivity_;
        anomaly.expectedValue = avgSentiment;
        anomaly.actualValue = currentSentiment;
        anomaly.deviationMagnitude = deviation;
        anomaly.description = "Significant sentiment spike detected";
        anomaly.requiresInvestigation = (anomaly.severity > 0.7);
    }

    return anomaly;
}

SentimentAnomaly SentimentQualityManager::detectSentimentAbsence(
    const std::vector<Database::Models::SentimentData>& data,
    const std::string& symbol) const {

    SentimentAnomaly anomaly;
    anomaly.anomalyType = "SENTIMENT_ABSENCE";
    anomaly.affectedSymbol = symbol;
    anomaly.detectedAt = std::chrono::system_clock::now();

    auto now = std::chrono::system_clock::now();
    auto threshold = std::chrono::minutes(60); // 1 hour threshold

    if (!data.empty()) {
        auto lastDataTime = data.back().getTimestamp();
        auto timeSinceLastData = std::chrono::duration_cast<std::chrono::minutes>(now - lastDataTime);

        if (timeSinceLastData > threshold) {
            anomaly.severity = std::min(1.0, static_cast<double>(timeSinceLastData.count()) / (threshold.count() * 2));
            anomaly.confidence = anomalyDetectionSensitivity_;
            anomaly.description = "Sentiment data absence detected";
            anomaly.requiresInvestigation = true;
        }
    } else {
        // No data at all
        anomaly.severity = 1.0;
        anomaly.confidence = 1.0;
        anomaly.description = "No sentiment data available";
        anomaly.requiresInvestigation = true;
    }

    return anomaly;
}

SentimentAnomaly SentimentQualityManager::detectStaleData(
    const std::vector<Database::Models::SentimentData>& data) const {

    SentimentAnomaly anomaly;
    anomaly.anomalyType = "STALE_DATA";
    anomaly.detectedAt = std::chrono::system_clock::now();

    if (data.empty()) {
        return anomaly;
    }

    auto now = std::chrono::system_clock::now();
    auto staleThreshold = std::chrono::hours(2); // 2 hours threshold

    int staleDataPoints = 0;
    for (const auto& dataPoint : data) {
        auto age = std::chrono::duration_cast<std::chrono::hours>(now - dataPoint.getTimestamp());
        if (age > staleThreshold) {
            staleDataPoints++;
        }
    }

    double staleRatio = static_cast<double>(staleDataPoints) / data.size();

    if (staleRatio > 0.3) { // More than 30% stale data
        anomaly.severity = staleRatio;
        anomaly.confidence = anomalyDetectionSensitivity_;
        anomaly.description = "High proportion of stale sentiment data";
        anomaly.requiresInvestigation = (staleRatio > 0.5);
    }

    return anomaly;
}

void SentimentQualityManager::persistQualityMetric(const SentimentQualityMetric& metric) {
    try {
        std::string query = R"(
            INSERT INTO sentiment_quality_metrics
            (source, symbol, timestamp, data_completeness_score, sentiment_accuracy_score,
             prediction_accuracy, source_credibility_score, freshness_score, average_article_age,
             consistency_score, sentiment_variance, consensus_source_count, market_relevance_score,
             volume_correlation, price_correlation, social_engagement_score, overall_quality_score)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        )";

        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(metric.timestamp.time_since_epoch()).count();

        // In a real implementation, we would use proper parameterized queries
        // For now, construct the query string directly
        std::stringstream ss;
        ss << "INSERT INTO sentiment_quality_metrics (source, symbol, timestamp, data_completeness_score, "
           << "sentiment_accuracy_score, prediction_accuracy, source_credibility_score, freshness_score, "
           << "average_article_age, consistency_score, sentiment_variance, consensus_source_count, "
           << "market_relevance_score, volume_correlation, price_correlation, social_engagement_score, "
           << "overall_quality_score) VALUES ('" << metric.source << "', '" << metric.symbol << "', "
           << timestamp << ", " << metric.dataCompletenessScore << ", " << metric.sentimentAccuracyScore
           << ", " << metric.predictionAccuracy << ", " << metric.sourceCredibilityScore << ", "
           << metric.freshnessScore << ", " << metric.averageArticleAge.count() << ", "
           << metric.consistencyScore << ", " << metric.sentimentVariance << ", "
           << metric.consensusSourceCount << ", " << metric.marketRelevanceScore << ", "
           << metric.volumeCorrelation << ", " << metric.priceCorrelation << ", "
           << metric.socialEngagementScore << ", " << metric.overallQualityScore << ")";

        dbManager_.executeQuery(ss.str());
    } catch (const std::exception& e) {
        // Handle error silently for now
    }
}

void SentimentQualityManager::persistSentimentAnomaly(const SentimentAnomaly& anomaly) {
    try {
        std::stringstream ss;
        ss << "INSERT INTO sentiment_anomalies (anomaly_id, anomaly_type, description, affected_symbol, "
           << "source, detected_at, severity, confidence, expected_value, actual_value, deviation_magnitude, "
           << "requires_investigation) VALUES ('" << generateAnomalyId(anomaly) << "', '"
           << anomaly.anomalyType << "', '" << anomaly.description << "', '" << anomaly.affectedSymbol
           << "', '" << anomaly.source << "', "
           << std::chrono::duration_cast<std::chrono::seconds>(anomaly.detectedAt.time_since_epoch()).count()
           << ", " << anomaly.severity << ", " << anomaly.confidence << ", " << anomaly.expectedValue
           << ", " << anomaly.actualValue << ", " << anomaly.deviationMagnitude << ", "
           << (anomaly.requiresInvestigation ? 1 : 0) << ")";

        dbManager_.executeQuery(ss.str());
    } catch (const std::exception& e) {
        // Handle error silently for now
    }
}

std::string SentimentQualityManager::generateAnomalyId(const SentimentAnomaly& anomaly) const {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    return "ANOM_" + std::to_string(timestamp) + "_" + anomaly.anomalyType;
}

} // namespace Sentiment
} // namespace CryptoClaude