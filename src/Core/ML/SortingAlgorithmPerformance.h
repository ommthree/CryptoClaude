#pragma once

#include <vector>
#include <map>
#include <chrono>
#include <deque>

namespace CryptoClaude {
namespace ML {

/**
 * Tracks and analyzes the performance of our sorting/ranking algorithm
 * to determine confidence levels for dynamic concentration decisions
 */

struct PairPrediction {
    std::string longSymbol;
    std::string shortSymbol;
    double predictedReturn;
    double confidence;
    std::chrono::system_clock::time_point predictionTime;

    // Actual outcome (filled after position close)
    double actualReturn = 0.0;
    bool outcomeKnown = false;
    std::chrono::system_clock::time_point outcomeTime;

    // Performance metrics
    bool wasCorrectDirection() const {
        return outcomeKnown && ((predictedReturn > 0) == (actualReturn > 0));
    }

    double getAbsoluteError() const {
        return outcomeKnown ? std::abs(predictedReturn - actualReturn) : 0.0;
    }
};

struct RankingPrediction {
    std::vector<std::pair<std::string, std::string>> rankedPairs;
    std::vector<double> confidenceScores;
    std::chrono::system_clock::time_point rankingTime;

    // Track actual performance of ranked pairs
    std::vector<double> actualReturns;
    bool outcomesKnown = false;

    // Ranking quality metrics
    double getSpearmanCorrelation() const;  // Correlation between predicted and actual rankings
    double getTopQuartileAccuracy() const;  // How well did top 25% perform
    double getBottomQuartileAvoidance() const; // How well did we avoid worst 25%
};

class SortingAlgorithmPerformance {
public:
    SortingAlgorithmPerformance();
    ~SortingAlgorithmPerformance() = default;

    // Record predictions
    void recordPairPrediction(const PairPrediction& prediction);
    void recordRankingPrediction(const RankingPrediction& ranking);

    // Update with actual outcomes
    void updatePredictionOutcome(const std::string& longSymbol, const std::string& shortSymbol,
                               double actualReturn, std::chrono::system_clock::time_point outcomeTime);
    void updateRankingOutcomes(const std::vector<double>& actualReturns,
                             std::chrono::system_clock::time_point outcomeTime);

    // Performance analysis for confidence calculation
    double getRecentAccuracy(int days = 30) const;
    double getAccuracyVariance(int days = 30) const;
    int getRecentPredictionCount(int days = 30) const;

    // Ranking-specific performance
    double getRankingStabilityScore(int days = 7) const;
    double getQuartileAccuracy(int quartile) const; // 1=top 25%, 4=bottom 25%
    double getAvoidanceScore(int quartile) const;   // How well we avoided bad quartiles

    // Signal separation analysis
    double getAverageSignalSeparation(int days = 30) const;
    double getTopTierConsistency(int days = 30) const;

    // Confidence metrics for dynamic concentration
    struct ConfidenceMetrics {
        double overallConfidence;
        double shortTermAccuracy;
        double rankingConsistency;
        double signalSeparation;
        double topTierReliability;
        int sampleSize;

        // Derived confidence level
        enum Level { VERY_LOW, LOW, MODERATE, HIGH, VERY_HIGH } level;
    };

    ConfidenceMetrics getCurrentConfidenceMetrics() const;

    // Historical performance tracking
    std::map<std::string, double> getPerformanceByTimeframe() const; // "1d", "7d", "30d", etc.
    std::vector<double> getAccuracyTimeSeries(int days = 90) const;

    // Performance attribution
    std::map<std::string, double> getPerformanceBySector() const;
    std::map<std::pair<std::string, std::string>, double> getPerformanceByPair() const;

    // Debugging and analysis
    void printPerformanceSummary() const;
    void exportPerformanceData(const std::string& filename) const;

private:
    std::deque<PairPrediction> pairPredictions_;
    std::deque<RankingPrediction> rankingPredictions_;

    // Performance cache for efficiency
    mutable std::map<int, double> accuracyCache_;
    mutable std::chrono::system_clock::time_point lastCacheUpdate_;

    // Helper methods
public:
    std::vector<PairPrediction> getRecentPredictions(int days) const;
    std::vector<RankingPrediction> getRecentRankings(int days) const;

private:

    void updateCache() const;
    void cleanOldPredictions(); // Remove predictions older than retention period

    // Statistical helpers
    double calculateSpearmanCorrelation(const std::vector<double>& predicted,
                                       const std::vector<double>& actual) const;
    double calculateAccuracyForPeriod(const std::vector<PairPrediction>& predictions) const;
    double calculateVarianceForPeriod(const std::vector<PairPrediction>& predictions) const;

    // Constants
    static constexpr int MAX_RETENTION_DAYS = 365;
    static constexpr int MIN_SAMPLE_SIZE = 10;
    static constexpr double CACHE_VALIDITY_HOURS = 1.0;
};

// Specialized confidence calculator for dynamic concentration decisions
class ConcentrationConfidenceCalculator {
public:
    static int calculateOptimalPairCount(const SortingAlgorithmPerformance::ConfidenceMetrics& metrics,
                                       int maxAvailablePairs);

    static bool shouldConcentrate(const SortingAlgorithmPerformance::ConfidenceMetrics& metrics,
                                double concentrationThreshold = 0.75);

    static double calculateConcentrationRatio(const SortingAlgorithmPerformance::ConfidenceMetrics& metrics);

private:
    // Concentration decision thresholds
    struct ConcentrationThresholds {
        static constexpr double VERY_HIGH_CONFIDENCE = 0.85;
        static constexpr double HIGH_CONFIDENCE = 0.70;
        static constexpr double MODERATE_CONFIDENCE = 0.55;
        static constexpr double LOW_CONFIDENCE = 0.40;

        static constexpr int VERY_HIGH_PAIRS = 5;
        static constexpr int HIGH_PAIRS = 12;
        static constexpr int MODERATE_PAIRS = 20;
        static constexpr int LOW_PAIRS = 35;
        static constexpr int VERY_LOW_PAIRS = 50;
    };
};

} // namespace ML
} // namespace CryptoClaude