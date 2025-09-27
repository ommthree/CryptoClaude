#include "SortingAlgorithmPerformance.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <iostream>
#include <fstream>

namespace CryptoClaude {
namespace ML {

// PairPrediction method implementations
double RankingPrediction::getSpearmanCorrelation() const {
    if (!outcomesKnown || rankedPairs.size() != actualReturns.size()) {
        return 0.0;
    }

    // Calculate Spearman correlation between predicted ranking and actual performance
    std::vector<size_t> predictedRanks(rankedPairs.size());
    std::vector<size_t> actualRanks(actualReturns.size());

    // Create rank vectors
    std::iota(predictedRanks.begin(), predictedRanks.end(), 0);

    // Sort indices by actual returns (descending)
    std::iota(actualRanks.begin(), actualRanks.end(), 0);
    std::sort(actualRanks.begin(), actualRanks.end(),
              [this](size_t i, size_t j) { return actualReturns[i] > actualReturns[j]; });

    // Calculate Spearman correlation
    double n = rankedPairs.size();
    double sumDiffSquared = 0.0;

    for (size_t i = 0; i < n; ++i) {
        double diff = static_cast<double>(predictedRanks[i]) - static_cast<double>(actualRanks[i]);
        sumDiffSquared += diff * diff;
    }

    return 1.0 - (6.0 * sumDiffSquared) / (n * (n * n - 1.0));
}

double RankingPrediction::getTopQuartileAccuracy() const {
    if (!outcomesKnown || rankedPairs.empty()) {
        return 0.0;
    }

    size_t quartileSize = rankedPairs.size() / 4;
    if (quartileSize == 0) quartileSize = 1;

    // Check how many of the top quartile predictions actually performed well
    double correctPredictions = 0.0;
    for (size_t i = 0; i < quartileSize && i < actualReturns.size(); ++i) {
        if (actualReturns[i] > 0) { // Positive return
            correctPredictions++;
        }
    }

    return correctPredictions / static_cast<double>(quartileSize);
}

double RankingPrediction::getBottomQuartileAvoidance() const {
    if (!outcomesKnown || rankedPairs.empty()) {
        return 0.0;
    }

    size_t quartileSize = rankedPairs.size() / 4;
    if (quartileSize == 0) quartileSize = 1;

    // Check how many of the bottom quartile predictions actually performed poorly
    size_t startIdx = rankedPairs.size() - quartileSize;
    double correctAvoidance = 0.0;

    for (size_t i = startIdx; i < rankedPairs.size() && i < actualReturns.size(); ++i) {
        if (actualReturns[i] <= 0) { // Negative or zero return
            correctAvoidance++;
        }
    }

    return correctAvoidance / static_cast<double>(quartileSize);
}

// SortingAlgorithmPerformance implementation
SortingAlgorithmPerformance::SortingAlgorithmPerformance() {
    lastCacheUpdate_ = std::chrono::system_clock::now() - std::chrono::hours(24);
    std::cout << "[SortingAlgorithmPerformance] Initialized performance tracking system\n";
    std::cout << "[SortingAlgorithmPerformance] Target: >85% confidence correlation (TRS requirement)\n";
}

void SortingAlgorithmPerformance::recordPairPrediction(const PairPrediction& prediction) {
    pairPredictions_.push_back(prediction);

    // Maintain maximum retention period
    cleanOldPredictions();

    std::cout << "[SortingAlgorithmPerformance] Recorded prediction for "
              << prediction.longSymbol << "/" << prediction.shortSymbol
              << " (confidence: " << (prediction.confidence * 100) << "%)\n";
}

void SortingAlgorithmPerformance::recordRankingPrediction(const RankingPrediction& ranking) {
    rankingPredictions_.push_back(ranking);

    // Maintain maximum retention period
    auto cutoffTime = std::chrono::system_clock::now() - std::chrono::days(MAX_RETENTION_DAYS);

    rankingPredictions_.erase(
        std::remove_if(rankingPredictions_.begin(), rankingPredictions_.end(),
                      [cutoffTime](const RankingPrediction& r) {
                          return r.rankingTime < cutoffTime;
                      }),
        rankingPredictions_.end()
    );

    std::cout << "[SortingAlgorithmPerformance] Recorded ranking prediction with "
              << ranking.rankedPairs.size() << " pairs\n";
}

void SortingAlgorithmPerformance::updatePredictionOutcome(
    const std::string& longSymbol, const std::string& shortSymbol,
    double actualReturn, std::chrono::system_clock::time_point outcomeTime) {

    bool updated = false;

    for (auto& prediction : pairPredictions_) {
        if (prediction.longSymbol == longSymbol && prediction.shortSymbol == shortSymbol && !prediction.outcomeKnown) {
            prediction.actualReturn = actualReturn;
            prediction.outcomeKnown = true;
            prediction.outcomeTime = outcomeTime;
            updated = true;

            std::cout << "[SortingAlgorithmPerformance] Updated outcome for "
                      << longSymbol << "/" << shortSymbol
                      << " (predicted: " << (prediction.predictedReturn * 100) << "%, "
                      << "actual: " << (actualReturn * 100) << "%)\n";
            break;
        }
    }

    if (updated) {
        updateCache();
    }
}

void SortingAlgorithmPerformance::updateRankingOutcomes(
    const std::vector<double>& actualReturns,
    std::chrono::system_clock::time_point outcomeTime) {

    if (!rankingPredictions_.empty()) {
        auto& latestRanking = rankingPredictions_.back();
        if (!latestRanking.outcomesKnown && actualReturns.size() == latestRanking.rankedPairs.size()) {
            latestRanking.actualReturns = actualReturns;
            latestRanking.outcomesKnown = true;

            std::cout << "[SortingAlgorithmPerformance] Updated ranking outcomes for "
                      << actualReturns.size() << " pairs\n";

            updateCache();
        }
    }
}

double SortingAlgorithmPerformance::getRecentAccuracy(int days) const {
    auto recentPredictions = getRecentPredictions(days);
    return calculateAccuracyForPeriod(recentPredictions);
}

double SortingAlgorithmPerformance::getAccuracyVariance(int days) const {
    auto recentPredictions = getRecentPredictions(days);
    return calculateVarianceForPeriod(recentPredictions);
}

int SortingAlgorithmPerformance::getRecentPredictionCount(int days) const {
    auto recentPredictions = getRecentPredictions(days);
    return static_cast<int>(recentPredictions.size());
}

double SortingAlgorithmPerformance::getRankingStabilityScore(int days) const {
    auto recentRankings = getRecentRankings(days);

    if (recentRankings.size() < 2) {
        return 0.0;
    }

    // Calculate stability as consistency of rankings over time
    double totalCorrelation = 0.0;
    int comparisons = 0;

    for (size_t i = 1; i < recentRankings.size(); ++i) {
        double correlation = calculateSpearmanCorrelation(
            recentRankings[i-1].confidenceScores,
            recentRankings[i].confidenceScores
        );
        totalCorrelation += correlation;
        comparisons++;
    }

    return comparisons > 0 ? totalCorrelation / comparisons : 0.0;
}

double SortingAlgorithmPerformance::getQuartileAccuracy(int quartile) const {
    auto recentRankings = getRecentRankings(30); // 30-day window

    if (recentRankings.empty()) {
        return 0.0;
    }

    double totalAccuracy = 0.0;
    int validRankings = 0;

    for (const auto& ranking : recentRankings) {
        if (ranking.outcomesKnown) {
            if (quartile == 1) {
                totalAccuracy += ranking.getTopQuartileAccuracy();
            } else if (quartile == 4) {
                totalAccuracy += ranking.getBottomQuartileAvoidance();
            }
            validRankings++;
        }
    }

    return validRankings > 0 ? totalAccuracy / validRankings : 0.0;
}

double SortingAlgorithmPerformance::getAverageSignalSeparation(int days) const {
    auto recentRankings = getRecentRankings(days);

    if (recentRankings.empty()) {
        return 0.0;
    }

    double totalSeparation = 0.0;
    int validRankings = 0;

    for (const auto& ranking : recentRankings) {
        if (ranking.outcomesKnown && ranking.actualReturns.size() >= 4) {
            // Calculate separation between top and bottom quartiles
            size_t quartileSize = ranking.actualReturns.size() / 4;

            double topQuartileAvg = 0.0;
            double bottomQuartileAvg = 0.0;

            // Top quartile
            for (size_t i = 0; i < quartileSize; ++i) {
                topQuartileAvg += ranking.actualReturns[i];
            }
            topQuartileAvg /= quartileSize;

            // Bottom quartile
            size_t bottomStart = ranking.actualReturns.size() - quartileSize;
            for (size_t i = bottomStart; i < ranking.actualReturns.size(); ++i) {
                bottomQuartileAvg += ranking.actualReturns[i];
            }
            bottomQuartileAvg /= quartileSize;

            totalSeparation += (topQuartileAvg - bottomQuartileAvg);
            validRankings++;
        }
    }

    return validRankings > 0 ? totalSeparation / validRankings : 0.0;
}

SortingAlgorithmPerformance::ConfidenceMetrics SortingAlgorithmPerformance::getCurrentConfidenceMetrics() const {
    ConfidenceMetrics metrics;

    // Calculate short-term accuracy (30 days)
    metrics.shortTermAccuracy = getRecentAccuracy(30);

    // Calculate ranking consistency (7 days)
    metrics.rankingConsistency = getRankingStabilityScore(7);

    // Calculate signal separation (30 days)
    metrics.signalSeparation = getAverageSignalSeparation(30);

    // Calculate top-tier reliability
    metrics.topTierReliability = getQuartileAccuracy(1); // Top quartile

    // Sample size
    metrics.sampleSize = getRecentPredictionCount(30);

    // Calculate overall confidence with TRS-specific weighting
    metrics.overallConfidence = calculateTRSCompliantConfidence(metrics);

    // Determine confidence level
    if (metrics.overallConfidence >= 0.85) {
        metrics.level = ConfidenceMetrics::Level::VERY_HIGH;
    } else if (metrics.overallConfidence >= 0.75) {
        metrics.level = ConfidenceMetrics::Level::HIGH;
    } else if (metrics.overallConfidence >= 0.60) {
        metrics.level = ConfidenceMetrics::Level::MODERATE;
    } else if (metrics.overallConfidence >= 0.45) {
        metrics.level = ConfidenceMetrics::Level::LOW;
    } else {
        metrics.level = ConfidenceMetrics::Level::VERY_LOW;
    }

    return metrics;
}

double SortingAlgorithmPerformance::calculateTRSCompliantConfidence(const ConfidenceMetrics& baseMetrics) const {
    // TRS-specific confidence calculation emphasizing correlation with outcomes
    double trsConfidence = 0.0;

    // Weight factors for TRS compliance
    const double ACCURACY_WEIGHT = 0.30;         // 30% weight on accuracy
    const double CONSISTENCY_WEIGHT = 0.25;      // 25% weight on consistency
    const double SEPARATION_WEIGHT = 0.20;       // 20% weight on signal separation
    const double RELIABILITY_WEIGHT = 0.15;      // 15% weight on top-tier reliability
    const double SAMPLE_SIZE_WEIGHT = 0.10;      // 10% weight on sample adequacy

    // Calculate weighted confidence
    trsConfidence += baseMetrics.shortTermAccuracy * ACCURACY_WEIGHT;
    trsConfidence += baseMetrics.rankingConsistency * CONSISTENCY_WEIGHT;
    trsConfidence += std::min(1.0, baseMetrics.signalSeparation / 0.1) * SEPARATION_WEIGHT; // Normalize signal separation
    trsConfidence += baseMetrics.topTierReliability * RELIABILITY_WEIGHT;

    // Sample size adjustment
    double sampleAdequacy = std::min(1.0, static_cast<double>(baseMetrics.sampleSize) / 200.0); // Target 200+ samples
    trsConfidence += sampleAdequacy * SAMPLE_SIZE_WEIGHT;

    // Apply TRS-specific adjustments
    if (baseMetrics.sampleSize < MIN_SAMPLE_SIZE) {
        trsConfidence *= 0.7; // Penalty for insufficient samples
    }

    return std::max(0.0, std::min(1.0, trsConfidence));
}

std::vector<SortingAlgorithmPerformance::PairPrediction>
SortingAlgorithmPerformance::getRecentPredictions(int days) const {
    auto cutoffTime = std::chrono::system_clock::now() - std::chrono::days(days);

    std::vector<PairPrediction> recent;
    for (const auto& prediction : pairPredictions_) {
        if (prediction.predictionTime >= cutoffTime) {
            recent.push_back(prediction);
        }
    }

    return recent;
}

std::vector<SortingAlgorithmPerformance::RankingPrediction>
SortingAlgorithmPerformance::getRecentRankings(int days) const {
    auto cutoffTime = std::chrono::system_clock::now() - std::chrono::days(days);

    std::vector<RankingPrediction> recent;
    for (const auto& ranking : rankingPredictions_) {
        if (ranking.rankingTime >= cutoffTime) {
            recent.push_back(ranking);
        }
    }

    return recent;
}

double SortingAlgorithmPerformance::calculateAccuracyForPeriod(
    const std::vector<PairPrediction>& predictions) const {

    if (predictions.empty()) {
        return 0.0;
    }

    int correctPredictions = 0;
    int totalPredictions = 0;

    for (const auto& prediction : predictions) {
        if (prediction.outcomeKnown) {
            if (prediction.wasCorrectDirection()) {
                correctPredictions++;
            }
            totalPredictions++;
        }
    }

    return totalPredictions > 0 ? static_cast<double>(correctPredictions) / totalPredictions : 0.0;
}

double SortingAlgorithmPerformance::calculateVarianceForPeriod(
    const std::vector<PairPrediction>& predictions) const {

    std::vector<double> accuracies;

    // Group predictions by week and calculate weekly accuracy
    auto now = std::chrono::system_clock::now();
    for (int week = 0; week < 4; ++week) {
        auto weekStart = now - std::chrono::days((week + 1) * 7);
        auto weekEnd = now - std::chrono::days(week * 7);

        int correct = 0, total = 0;
        for (const auto& prediction : predictions) {
            if (prediction.outcomeKnown &&
                prediction.predictionTime >= weekStart &&
                prediction.predictionTime < weekEnd) {
                if (prediction.wasCorrectDirection()) {
                    correct++;
                }
                total++;
            }
        }

        if (total > 0) {
            accuracies.push_back(static_cast<double>(correct) / total);
        }
    }

    if (accuracies.size() < 2) {
        return 0.0;
    }

    // Calculate variance
    double mean = std::accumulate(accuracies.begin(), accuracies.end(), 0.0) / accuracies.size();
    double variance = 0.0;

    for (double accuracy : accuracies) {
        variance += (accuracy - mean) * (accuracy - mean);
    }

    return variance / (accuracies.size() - 1);
}

double SortingAlgorithmPerformance::calculateSpearmanCorrelation(
    const std::vector<double>& predicted, const std::vector<double>& actual) const {

    if (predicted.size() != actual.size() || predicted.empty()) {
        return 0.0;
    }

    size_t n = predicted.size();

    // Create rank vectors
    std::vector<size_t> predictedRanks(n), actualRanks(n);
    std::iota(predictedRanks.begin(), predictedRanks.end(), 0);
    std::iota(actualRanks.begin(), actualRanks.end(), 0);

    // Sort by values (descending)
    std::sort(predictedRanks.begin(), predictedRanks.end(),
              [&predicted](size_t i, size_t j) { return predicted[i] > predicted[j]; });
    std::sort(actualRanks.begin(), actualRanks.end(),
              [&actual](size_t i, size_t j) { return actual[i] > actual[j]; });

    // Calculate Spearman correlation
    double sumDiffSquared = 0.0;
    for (size_t i = 0; i < n; ++i) {
        double diff = static_cast<double>(predictedRanks[i]) - static_cast<double>(actualRanks[i]);
        sumDiffSquared += diff * diff;
    }

    return 1.0 - (6.0 * sumDiffSquared) / (static_cast<double>(n) * (n * n - 1.0));
}

void SortingAlgorithmPerformance::updateCache() const {
    lastCacheUpdate_ = std::chrono::system_clock::now();
    accuracyCache_.clear();

    // Pre-calculate commonly requested metrics
    accuracyCache_[7] = getRecentAccuracy(7);
    accuracyCache_[30] = getRecentAccuracy(30);
    accuracyCache_[90] = getRecentAccuracy(90);
}

void SortingAlgorithmPerformance::cleanOldPredictions() {
    auto cutoffTime = std::chrono::system_clock::now() - std::chrono::days(MAX_RETENTION_DAYS);

    pairPredictions_.erase(
        std::remove_if(pairPredictions_.begin(), pairPredictions_.end(),
                      [cutoffTime](const PairPrediction& p) {
                          return p.predictionTime < cutoffTime;
                      }),
        pairPredictions_.end()
    );
}

void SortingAlgorithmPerformance::printPerformanceSummary() const {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "ALGORITHM PERFORMANCE SUMMARY\n";
    std::cout << std::string(60, '=') << "\n";

    auto metrics = getCurrentConfidenceMetrics();

    std::cout << "Overall Performance:\n";
    std::cout << "- Confidence Level: " << metrics.overallConfidence * 100 << "%\n";
    std::cout << "- TRS Requirement (85%): " << (metrics.overallConfidence >= 0.85 ? "✓ MET" : "✗ NOT MET") << "\n";
    std::cout << "- Confidence Rating: ";

    switch (metrics.level) {
        case ConfidenceMetrics::Level::VERY_HIGH: std::cout << "VERY HIGH"; break;
        case ConfidenceMetrics::Level::HIGH: std::cout << "HIGH"; break;
        case ConfidenceMetrics::Level::MODERATE: std::cout << "MODERATE"; break;
        case ConfidenceMetrics::Level::LOW: std::cout << "LOW"; break;
        case ConfidenceMetrics::Level::VERY_LOW: std::cout << "VERY LOW"; break;
    }
    std::cout << "\n\n";

    std::cout << "Detailed Metrics:\n";
    std::cout << "- Short-term accuracy (30d): " << (metrics.shortTermAccuracy * 100) << "%\n";
    std::cout << "- Ranking consistency (7d): " << (metrics.rankingConsistency * 100) << "%\n";
    std::cout << "- Signal separation: " << (metrics.signalSeparation * 100) << "%\n";
    std::cout << "- Top-tier reliability: " << (metrics.topTierReliability * 100) << "%\n";
    std::cout << "- Sample size: " << metrics.sampleSize << " predictions\n";

    std::cout << "\nRecent Performance (Days):\n";
    std::cout << "- 7-day accuracy: " << (getRecentAccuracy(7) * 100) << "%\n";
    std::cout << "- 30-day accuracy: " << (getRecentAccuracy(30) * 100) << "%\n";
    std::cout << "- 90-day accuracy: " << (getRecentAccuracy(90) * 100) << "%\n";

    std::cout << std::string(60, '=') << "\n\n";
}

// ConcentrationConfidenceCalculator implementation
int ConcentrationConfidenceCalculator::calculateOptimalPairCount(
    const SortingAlgorithmPerformance::ConfidenceMetrics& metrics, int maxAvailablePairs) {

    int optimalCount = ConcentrationThresholds::MODERATE_PAIRS; // Default

    if (metrics.overallConfidence >= ConcentrationThresholds::VERY_HIGH_CONFIDENCE) {
        optimalCount = ConcentrationThresholds::VERY_HIGH_PAIRS;
    } else if (metrics.overallConfidence >= ConcentrationThresholds::HIGH_CONFIDENCE) {
        optimalCount = ConcentrationThresholds::HIGH_PAIRS;
    } else if (metrics.overallConfidence >= ConcentrationThresholds::MODERATE_CONFIDENCE) {
        optimalCount = ConcentrationThresholds::MODERATE_PAIRS;
    } else if (metrics.overallConfidence >= ConcentrationThresholds::LOW_CONFIDENCE) {
        optimalCount = ConcentrationThresholds::LOW_PAIRS;
    } else {
        optimalCount = ConcentrationThresholds::VERY_LOW_PAIRS;
    }

    // Ensure we don't exceed available pairs and meet TRS minimum of 8
    return std::max(8, std::min(optimalCount, maxAvailablePairs));
}

bool ConcentrationConfidenceCalculator::shouldConcentrate(
    const SortingAlgorithmPerformance::ConfidenceMetrics& metrics, double concentrationThreshold) {

    return metrics.overallConfidence >= concentrationThreshold &&
           metrics.sampleSize >= 50 && // Sufficient sample size
           metrics.rankingConsistency >= 0.6; // Consistent performance
}

double ConcentrationConfidenceCalculator::calculateConcentrationRatio(
    const SortingAlgorithmPerformance::ConfidenceMetrics& metrics) {

    // Higher confidence = more concentration (lower ratio = more concentrated)
    if (metrics.overallConfidence >= ConcentrationThresholds::VERY_HIGH_CONFIDENCE) {
        return 0.2; // Top 20% of pairs
    } else if (metrics.overallConfidence >= ConcentrationThresholds::HIGH_CONFIDENCE) {
        return 0.3; // Top 30% of pairs
    } else if (metrics.overallConfidence >= ConcentrationThresholds::MODERATE_CONFIDENCE) {
        return 0.5; // Top 50% of pairs
    } else {
        return 0.8; // Top 80% of pairs (less concentration)
    }
}

} // namespace ML
} // namespace CryptoClaude