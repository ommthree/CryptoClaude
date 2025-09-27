# Dynamic Concentration and Pair Calibration Strategy

## Overview

With our expanded 75-coin universe providing 2,775 possible pairs, we need **dynamic concentration management** and **systematic pair calibration** to optimize performance while managing risk. This strategy will be refined through extensive backtesting.

## 1. Dynamic Concentration Framework

### Concentration Levels Based on Algorithm Confidence
```cpp
enum class ConcentrationMode {
    HIGHLY_CONCENTRATED,    // 3-5 pairs (very high sorting confidence)
    CONCENTRATED,           // 8-12 pairs (high sorting confidence)
    MODERATE,              // 15-20 pairs (moderate sorting confidence)
    DIVERSIFIED,           // 25-35 pairs (low sorting confidence)
    HIGHLY_DIVERSIFIED     // 40-50 pairs (very low sorting confidence)
};

struct DynamicConcentrationParams {
    ConcentrationMode mode = ConcentrationMode::MODERATE;
    int minPairs = 3;
    int maxPairs = 50;
    int currentOptimalPairs = 20; // Calibrated through backtesting

    // Algorithm confidence triggers for concentration changes
    double highSortingConfidenceThreshold = 0.85;     // Very confident in rankings
    double moderateSortingConfidenceThreshold = 0.70; // Moderately confident
    double lowSortingConfidenceThreshold = 0.55;      // Low confidence
    double minimumSortingConfidenceThreshold = 0.52;  // Below this = diversify maximally

    // Sorting algorithm performance metrics
    double recentAccuracyWindow = 30;                 // Days to assess recent performance
    double minimumTrackRecord = 100;                  // Minimum predictions needed
    double consistencyThreshold = 0.1;                // Max acceptable accuracy variance
};
```

### Dynamic Concentration Algorithm Based on Sorting Confidence
```cpp
class DynamicConcentrationManager {
public:
    ConcentrationMode determineOptimalConcentration(
        const SortingAlgorithmPerformance& recentPerformance,
        const std::vector<PairSignal>& rankedPairs,
        double signalSeparation
    ) {

        // Calculate algorithm's confidence in its sorting ability
        SortingConfidenceMetrics confidence = calculateSortingConfidence(
            recentPerformance, rankedPairs, signalSeparation
        );

        // Very high confidence in sorting = concentrate on top picks
        if (confidence.overallConfidence > 0.85 && confidence.topTierSeparation > 0.3) {
            return ConcentrationMode::HIGHLY_CONCENTRATED;
        }

        // High confidence in sorting = moderate concentration
        if (confidence.overallConfidence > 0.70 && confidence.rankingConsistency > 0.8) {
            return ConcentrationMode::CONCENTRATED;
        }

        // Moderate confidence = balanced approach
        if (confidence.overallConfidence > 0.55) {
            return ConcentrationMode::MODERATE;
        }

        // Low confidence in sorting = diversify to reduce single-pick risk
        if (confidence.overallConfidence > 0.52) {
            return ConcentrationMode::DIVERSIFIED;
        }

        // Very low confidence = maximum diversification
        return ConcentrationMode::HIGHLY_DIVERSIFIED;
    }

    int calculateOptimalPairCount(ConcentrationMode mode, int availableHighQualityPairs) {
        switch (mode) {
            case ConcentrationMode::HIGHLY_CONCENTRATED:
                return std::min(5, availableHighQualityPairs);
            case ConcentrationMode::CONCENTRATED:
                return std::min(12, availableHighQualityPairs);
            case ConcentrationMode::MODERATE:
                return std::min(20, availableHighQualityPairs);
            case ConcentrationMode::DIVERSIFIED:
                return std::min(35, availableHighQualityPairs);
            case ConcentrationMode::HIGHLY_DIVERSIFIED:
                return std::min(50, availableHighQualityPairs);
        }
        return 20; // Default
    }

private:
    struct SortingConfidenceMetrics {
        double overallConfidence;        // Overall confidence in sorting ability (0-1)
        double topTierSeparation;        // How well separated top pairs are from others
        double rankingConsistency;       // How consistent rankings are over time
        double recentAccuracy;           // Recent prediction accuracy
        double accuracyStability;        // Variance in accuracy over time
        int predictionCount;             // Number of predictions in assessment window

        // Sorting-specific metrics
        double signalStrengthSpread;     // Difference between strongest and weakest signals
        double topQuartileConfidence;    // Confidence in top 25% of ranked pairs
        double bottomQuartileAvoidance;  // Confidence in avoiding worst 25%
    };

    SortingConfidenceMetrics calculateSortingConfidence(
        const SortingAlgorithmPerformance& performance,
        const std::vector<PairSignal>& rankedPairs,
        double signalSeparation
    ) {
        SortingConfidenceMetrics confidence;

        // Recent accuracy performance
        confidence.recentAccuracy = performance.getRecentAccuracy(30); // Last 30 days
        confidence.accuracyStability = performance.getAccuracyVariance(30);
        confidence.predictionCount = performance.getRecentPredictionCount(30);

        // Signal separation analysis
        confidence.signalStrengthSpread = calculateSignalSpread(rankedPairs);
        confidence.topTierSeparation = calculateTopTierSeparation(rankedPairs);

        // Ranking consistency over time
        confidence.rankingConsistency = performance.getRankingStabilityScore(7); // 7-day consistency

        // Top and bottom quartile performance
        confidence.topQuartileConfidence = performance.getQuartileAccuracy(1); // Top 25%
        confidence.bottomQuartileAvoidance = performance.getAvoidanceScore(4); // Bottom 25%

        // Overall confidence calculation weighted by importance
        confidence.overallConfidence =
            (confidence.recentAccuracy * 0.3) +
            (confidence.topQuartileConfidence * 0.25) +
            (confidence.rankingConsistency * 0.2) +
            (confidence.topTierSeparation * 0.15) +
            (confidence.bottomQuartileAvoidance * 0.1);

        // Penalty for insufficient data
        if (confidence.predictionCount < 100) {
            confidence.overallConfidence *= (confidence.predictionCount / 100.0);
        }

        return confidence;
    }

    double calculateSignalSpread(const std::vector<PairSignal>& rankedPairs) {
        if (rankedPairs.empty()) return 0.0;

        double maxSignal = rankedPairs.front().confidence;
        double minSignal = rankedPairs.back().confidence;
        return maxSignal - minSignal;
    }

    double calculateTopTierSeparation(const std::vector<PairSignal>& rankedPairs) {
        if (rankedPairs.size() < 10) return 0.0;

        size_t topTierSize = std::max(5UL, rankedPairs.size() / 4); // Top quartile
        double topTierAvg = 0.0;
        double restAvg = 0.0;

        for (size_t i = 0; i < topTierSize; ++i) {
            topTierAvg += rankedPairs[i].confidence;
        }
        topTierAvg /= topTierSize;

        for (size_t i = topTierSize; i < rankedPairs.size(); ++i) {
            restAvg += rankedPairs[i].confidence;
        }
        restAvg /= (rankedPairs.size() - topTierSize);

        return topTierAvg - restAvg; // Higher separation = more confidence
    }
};
```

## 2. Pair Calibration Framework

### Pair Quality Scoring System
```cpp
struct PairQualityMetrics {
    double signalStrength;        // ML model confidence for this pair
    double historicalPerformance; // Backtested Sharpe ratio
    double correlationStability;  // How stable the correlation is over time
    double liquidityScore;        // Combined liquidity of both assets
    double sectorDiversification; // Benefit from sector exposure
    double volatilityMatch;       // How well volatilities match for hedging
    double newsEventSensitivity;  // Response to news events

    // Composite quality score (0-1)
    double getCompositeScore() const {
        return (signalStrength * 0.3) +
               (historicalPerformance * 0.25) +
               (correlationStability * 0.15) +
               (liquidityScore * 0.1) +
               (sectorDiversification * 0.1) +
               (volatilityMatch * 0.05) +
               (newsEventSensitivity * 0.05);
    }
};

class PairCalibrationEngine {
public:
    // Extensive backtesting to find optimal pair count ranges
    struct CalibrationResults {
        int optimalPairCountBullMarket;
        int optimalPairCountBearMarket;
        int optimalPairCountSidewaysMarket;
        int optimalPairCountHighVolatility;
        int optimalPairCountLowVolatility;

        std::vector<std::pair<std::string, std::string>> topPerformingPairs;
        std::vector<std::pair<std::string, std::string>> allWeatherPairs;
        std::map<MarketSector, int> optimalSectorConcentration;
    };

    CalibrationResults runExtensiveBacktesting(
        const std::vector<std::string>& universe,
        int backtestDays = 730,
        int calibrationRuns = 1000
    );
};
```

## 3. Extensive Backtesting Framework

### Multi-Dimensional Backtesting Strategy
```cpp
class ExtensivePairBacktester {
public:
    struct BacktestingParameters {
        std::vector<int> pairCountsToTest = {3, 5, 8, 12, 15, 20, 25, 30, 35, 40, 50};
        std::vector<int> rebalanceFrequencies = {1, 7, 14, 30}; // days
        std::vector<double> concentrationThresholds = {0.05, 0.08, 0.10, 0.15, 0.20};

        // Market condition scenarios
        std::vector<std::string> marketRegimes = {
            "bull_market", "bear_market", "sideways", "high_volatility",
            "low_volatility", "crisis", "recovery", "sector_rotation"
        };

        // Performance metrics to optimize
        std::vector<std::string> optimizationTargets = {
            "sharpe_ratio", "max_drawdown", "calmar_ratio",
            "sortino_ratio", "omega_ratio", "risk_adjusted_return"
        };
    };

    struct BacktestResults {
        std::map<int, double> pairCountToPerformance;
        std::map<std::string, int> regimeToOptimalPairCount;
        std::map<std::pair<std::string, std::string>, double> pairToPerformance;

        // Concentration analysis
        double optimalMaxSinglePairWeight;
        double optimalSectorConcentrationLimit;
        int optimalRebalanceFrequency;

        // Dynamic parameters
        std::map<std::string, DynamicConcentrationParams> regimeToConcentrationParams;
    };

    BacktestResults runComprehensiveCalibration();
};
```

### Calibration Methodology
```cpp
class PairRangeOptimizer {
public:
    // Phase 1: Broad pair universe screening (all 2,775 combinations)
    std::vector<std::pair<std::string, std::string>> screenPairUniverse(
        const std::vector<std::string>& coins,
        double minCorrelationThreshold = -0.8,  // For long-short
        double maxCorrelationThreshold = 0.5,   // Not too correlated
        double minLiquidityScore = 0.3
    );

    // Phase 2: Intensive backtesting on filtered pairs (top 200-500 pairs)
    std::map<std::pair<std::string, std::string>, PairQualityMetrics>
    calibrateFilteredPairs(
        const std::vector<std::pair<std::string, std::string>>& candidatePairs,
        int backtestDays = 730
    );

    // Phase 3: Dynamic concentration optimization
    DynamicConcentrationParams optimizeConcentrationParameters(
        const std::map<std::pair<std::string, std::string>, PairQualityMetrics>& pairQualities
    );

    // Phase 4: Market regime-specific optimization
    std::map<std::string, std::vector<std::pair<std::string, std::string>>>
    optimizePairsForRegimes(
        const std::map<std::pair<std::string, std::string>, PairQualityMetrics>& pairQualities
    );
};
```

## 4. Implementation Strategy for Day 16-17

### Day 16: Data Loading + Initial Pair Screening
```cpp
struct Day16Implementation {
    // 1. Load historical data for all 75 coins
    bool loadHistoricalDataForUniverse();

    // 2. Calculate correlation matrices for different timeframes
    std::map<int, CorrelationMatrix> calculateRollingCorrelations(
        std::vector<int> windows = {30, 60, 90, 180, 365}
    );

    // 3. Screen for viable pair candidates (reduce 2,775 to ~500)
    std::vector<std::pair<std::string, std::string>> screenViablePairs();

    // 4. Initial quality scoring for screened pairs
    std::map<std::pair<std::string, std::string>, double> calculateInitialQualityScores();
};
```

### Day 17: Extensive Backtesting + Calibration
```cpp
struct Day17Implementation {
    // 1. Run comprehensive pair backtesting
    BacktestResults runExtensiveBacktesting(
        const std::vector<std::pair<std::string, std::string>>& candidatePairs
    );

    // 2. Calibrate optimal pair count ranges
    std::map<std::string, int> calibrateOptimalPairCounts();

    // 3. Determine dynamic concentration parameters
    DynamicConcentrationParams calibrateConcentrationTriggers();

    // 4. Generate market regime-specific pair rankings
    std::map<std::string, std::vector<std::pair<std::string, std::string>>>
    generateRegimeSpecificRankings();
};
```

## 5. Expected Calibration Outcomes

### Pair Count Optimization Results (Expected)
```cpp
struct ExpectedCalibrationResults {
    // Market regime specific optimal counts
    int bullMarketOptimal = 15;        // Moderate concentration in trending markets
    int bearMarketOptimal = 25;        // Higher diversification in declining markets
    int sidewaysMarketOptimal = 30;    // Maximum diversification in choppy markets
    int highVolatilityOptimal = 35;    // High diversification when volatility spikes
    int crisisOptimal = 8;             // High concentration on highest quality pairs

    // Dynamic ranges
    int minPairsAnyCondition = 3;      // Emergency minimum
    int maxPairsAnyCondition = 50;     // Practical maximum for execution

    // Concentration limits (per pair)
    double maxSinglePairWeight = 0.15; // 15% maximum per pair
    double maxSectorWeight = 0.40;     // 40% maximum per sector
};
```

### Performance Validation Framework
```cpp
class CalibrationValidator {
public:
    // Out-of-sample validation
    bool validateCalibrationOutOfSample(
        const CalibrationResults& results,
        int outOfSampleDays = 180
    );

    // Walk-forward analysis
    std::vector<double> walkForwardValidation(
        const DynamicConcentrationParams& params,
        int windowDays = 90,
        int stepDays = 30
    );

    // Stress testing calibrated parameters
    std::map<std::string, double> stressTestCalibration(
        const CalibrationResults& results
    );
};
```

## 6. Integration with Existing Framework

### Portfolio Optimizer Integration
```cpp
// Enhanced portfolio optimizer with dynamic concentration
class EnhancedPortfolioOptimizer : public PortfolioOptimizer {
private:
    DynamicConcentrationManager concentrationManager_;
    PairCalibrationEngine calibrationEngine_;

public:
    // Override pair selection with calibrated approach
    std::vector<std::pair<std::string, std::string>> selectOptimalPairs(
        const MarketConditions& conditions,
        const std::vector<PairSignal>& availableSignals
    ) override;

    // Dynamic concentration adjustment
    void adjustConcentrationForMarketConditions(
        const MarketConditions& conditions
    );
};
```

This comprehensive framework ensures that our expanded 75-coin universe is optimally utilized through **systematic backtesting** and **dynamic concentration management**, with pair counts and concentration levels **calibrated specifically for different market conditions** rather than using static parameters.