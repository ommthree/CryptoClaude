#pragma once

#include <vector>
#include <string>
#include <map>
#include <set>
#include <chrono>
#include "../Config/ExpandedCoinUniverse.h"

namespace CryptoClaude {
namespace Strategy {

/**
 * Phase 1 Pair Screening Framework for TRS-Approved Tier 1 Universe
 * Reduces ~190 possible pairs to viable candidates for backtesting
 * Conservative approach with comprehensive quality and correlation analysis
 */
class Phase1PairScreening {
public:
    // Pair candidate structure with comprehensive metrics
    struct PairCandidate {
        std::string longSymbol;
        std::string shortSymbol;
        Config::ExpandedCoinUniverse::MarketSector longSector;
        Config::ExpandedCoinUniverse::MarketSector shortSector;

        // Screening metrics
        double correlationScore;        // Correlation between assets
        double liquidityScore;          // Combined liquidity metric
        double volatilityMatch;         // Volatility similarity score
        double sectorDiversification;   // Sector diversity benefit
        double marketCapBalance;        // Market cap similarity
        double tradingVolumeScore;      // Combined trading volume

        // Quality metrics
        double dataQualityScore;        // Historical data quality
        double newsAvailabilityScore;   // News coverage quality
        double exchangeListingScore;    // Exchange availability

        // Composite scores
        double overallQualityScore;     // Combined quality metric
        double tradingViabilityScore;   // Trading suitability
        double riskAdjustedScore;       // Risk-adjusted potential

        // Classification
        enum class Tier { TIER1_PREMIUM, TIER1_STANDARD, TIER2_BACKUP, REJECTED } tier;
        bool passesMinimumCriteria() const;
        bool recommendedForBacktesting() const;
    };

    // Screening criteria for TRS Phase 1 compliance
    struct ScreeningCriteria {
        // Correlation requirements
        double minCorrelation = 0.3;           // Minimum correlation for pair formation
        double maxCorrelation = 0.8;           // Maximum correlation (avoid redundancy)
        double optimalCorrelation = 0.6;       // Target correlation for best pairs

        // Liquidity requirements
        double minLiquidityScore = 0.5;        // Minimum liquidity threshold
        double minCombinedVolume = 200000000;  // $200M combined daily volume

        // Quality requirements
        double minDataQuality = 0.95;          // Minimum data quality score
        double minNewsAvailability = 0.4;      // Minimum news coverage
        int minExchangeListings = 8;           // Combined exchange listings

        // Diversification requirements
        bool requireSectorDiversification = true;  // Prefer cross-sector pairs
        double maxSectorConcentration = 0.25;      // Max 25% pairs from one sector

        // Risk management
        double maxVolatilityRatio = 3.0;       // Max volatility ratio between assets
        double minMarketCapRatio = 0.1;        // Min market cap ratio (smaller/larger)

        // Targeting parameters
        int targetPairCount = 50;              // Target viable pairs for backtesting
        int maxPairCount = 80;                 // Maximum pairs to evaluate
        int minPairCount = 20;                 // Minimum pairs required
    };

    // Screening results and analytics
    struct ScreeningResults {
        std::vector<PairCandidate> tier1Premium;    // Best 15-20 pairs
        std::vector<PairCandidate> tier1Standard;   // Good 20-30 pairs
        std::vector<PairCandidate> tier2Backup;     // Backup 15-20 pairs
        std::vector<PairCandidate> rejected;        // Rejected pairs

        // Summary statistics
        int totalPairsEvaluated = 0;
        int viablePairsFound = 0;
        double averageQualityScore = 0.0;
        double averageCorrelation = 0.0;

        // Sector distribution
        std::map<Config::ExpandedCoinUniverse::MarketSector, int> sectorDistribution;

        // Quality metrics
        double passRate = 0.0;
        bool meetsTargetCount = false;
        bool meetsTRSRequirements = false;

        std::chrono::system_clock::time_point screeningTime;
    };

    // Sector diversification analysis
    struct SectorAnalysis {
        std::map<Config::ExpandedCoinUniverse::MarketSector, std::vector<std::string>> sectorCoins;
        std::map<std::pair<Config::ExpandedCoinUniverse::MarketSector, Config::ExpandedCoinUniverse::MarketSector>, int> crossSectorPairs;

        double sectorDiversificationScore;
        bool meetsDiversificationRequirements;

        std::vector<std::string> recommendations;
    };

public:
    Phase1PairScreening();
    ~Phase1PairScreening() = default;

    // Main screening orchestration
    ScreeningResults screenTier1Universe();
    ScreeningResults screenTier1Universe(const ScreeningCriteria& customCriteria);

    // Individual screening components
    std::vector<PairCandidate> generateAllPossiblePairs();
    std::vector<PairCandidate> applyQualityFilters(const std::vector<PairCandidate>& candidates);
    std::vector<PairCandidate> applyCorrelationFilters(const std::vector<PairCandidate>& candidates);
    std::vector<PairCandidate> applySectorDiversification(const std::vector<PairCandidate>& candidates);
    std::vector<PairCandidate> rankAndSelectBestPairs(const std::vector<PairCandidate>& candidates);

    // Sector analysis
    SectorAnalysis analyzeSectorDistribution(const std::vector<PairCandidate>& pairs);
    bool validateSectorDiversification(const std::vector<PairCandidate>& pairs);

    // Configuration and validation
    void setScreeningCriteria(const ScreeningCriteria& criteria);
    ScreeningCriteria getScreeningCriteria() const;
    bool validateScreeningResults(const ScreeningResults& results);

    // Reporting and analytics
    void generateScreeningReport(const ScreeningResults& results);
    void exportPairCandidates(const std::string& filename, const ScreeningResults& results);
    void printScreeningSummary(const ScreeningResults& results);

    // Quality assessment methods
    double calculateCorrelationScore(const std::string& symbol1, const std::string& symbol2);
    double calculateLiquidityScore(const std::string& symbol1, const std::string& symbol2);
    double calculateDataQualityScore(const std::string& symbol1, const std::string& symbol2);
    double calculateTradingViabilityScore(const PairCandidate& candidate);

private:
    ScreeningCriteria criteria_;
    ScreeningResults lastResults_;

    // Caching for performance
    std::map<std::pair<std::string, std::string>, double> correlationCache_;
    std::map<std::string, double> liquidityCache_;
    std::map<std::string, double> qualityCache_;

    // Internal helper methods
    PairCandidate createPairCandidate(const std::string& longSymbol, const std::string& shortSymbol);
    void calculateAllPairMetrics(PairCandidate& candidate);
    bool meetsMinimumCriteria(const PairCandidate& candidate);

    void classifyPairTier(PairCandidate& candidate);
    void updateScreeningStatistics(ScreeningResults& results);

    // Statistical helper methods
    double calculateVolatilityMatch(const std::string& symbol1, const std::string& symbol2);
    double calculateMarketCapBalance(const std::string& symbol1, const std::string& symbol2);
    double calculateSectorDiversificationScore(Config::ExpandedCoinUniverse::MarketSector sector1,
                                             Config::ExpandedCoinUniverse::MarketSector sector2);

    // Constants
    static constexpr double QUALITY_WEIGHT = 0.3;
    static constexpr double CORRELATION_WEIGHT = 0.25;
    static constexpr double LIQUIDITY_WEIGHT = 0.2;
    static constexpr double DIVERSIFICATION_WEIGHT = 0.15;
    static constexpr double VIABILITY_WEIGHT = 0.1;
};

/**
 * Advanced pair quality analyzer for TRS compliance
 * Implements sophisticated filtering and ranking algorithms
 */
class TRSPairQualityAnalyzer {
public:
    // TRS-specific quality requirements
    struct TRSQualityRequirements {
        double minConfidenceThreshold = 0.85;     // TRS >85% correlation requirement
        int minHistoricalDays = 730;              // 2-year data requirement
        double minStatisticalSignificance = 0.95; // 95% confidence intervals

        // Enhanced quality metrics
        bool requiresMultiTimeframeAnalysis = true; // Multiple timeframe validation
        bool requiresRegimeStabilityTesting = true; // Bull/bear/sideways consistency
        bool requiresLiquidityStressTesting = true; // Liquidity stress scenarios

        // Performance requirements
        double minBacktestSharpeRatio = 0.5;      // Minimum risk-adjusted performance
        double maxDrawdownThreshold = 0.15;       // Maximum 15% drawdown
        int minTradingDaysPerYear = 200;           // Minimum trading activity
    };

    // Comprehensive quality assessment results
    struct QualityAssessmentResults {
        std::map<std::string, double> pairQualityScores;
        std::map<std::string, bool> trsCompliance;
        std::map<std::string, std::vector<std::string>> qualityIssues;

        double overallQualityScore;
        int pairsPassingTRS;
        int totalPairsEvaluated;

        bool meetsMinimumStandards;
        std::vector<std::string> recommendations;
    };

public:
    TRSPairQualityAnalyzer();
    ~TRSPairQualityAnalyzer() = default;

    // Main quality analysis methods
    QualityAssessmentResults analyzePairQuality(const std::vector<Phase1PairScreening::PairCandidate>& pairs);
    bool validateTRSCompliance(const Phase1PairScreening::PairCandidate& pair);

    // Individual quality checks
    bool checkDataSufficiency(const std::string& symbol1, const std::string& symbol2);
    bool checkStatisticalSignificance(const std::string& symbol1, const std::string& symbol2);
    bool checkRegimeStability(const std::string& symbol1, const std::string& symbol2);
    bool checkLiquidityAdequacy(const std::string& symbol1, const std::string& symbol2);

    // Advanced analytics
    double calculateConfidenceLevel(const std::string& symbol1, const std::string& symbol2);
    double assessBacktestingPotential(const Phase1PairScreening::PairCandidate& pair);
    std::vector<std::string> identifyQualityIssues(const Phase1PairScreening::PairCandidate& pair);

    // Configuration
    void setTRSRequirements(const TRSQualityRequirements& requirements);
    TRSQualityRequirements getTRSRequirements() const;

    // Reporting
    void generateTRSQualityReport(const QualityAssessmentResults& results);

private:
    TRSQualityRequirements requirements_;
    QualityAssessmentResults lastResults_;

    // Internal analysis methods
    double calculateMultiTimeframeStability(const std::string& symbol1, const std::string& symbol2);
    double assessRegimeConsistency(const std::string& symbol1, const std::string& symbol2);
    double evaluateLiquidityStress(const std::string& symbol1, const std::string& symbol2);
};

} // namespace Strategy
} // namespace CryptoClaude