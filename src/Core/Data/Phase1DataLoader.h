#pragma once

#include <vector>
#include <string>
#include <chrono>
#include <map>
#include <functional>
#include <memory>

namespace CryptoClaude {
namespace Data {

/**
 * Phase 1 Data Loader for TRS-Approved Tier 1 Universe
 * Implements progressive loading strategy with 2-year historical coverage
 * Conservative approach with comprehensive quality validation
 */
class Phase1DataLoader {
public:
    // Loading priority structure for systematic data acquisition
    struct LoadingSequence {
        // Priority 1: Major pairs foundation (Hours 1-4)
        std::vector<std::string> priority1 = {"BTC", "ETH", "ADA", "DOT", "LINK"};

        // Priority 2: DeFi infrastructure (Hours 5-8)
        std::vector<std::string> priority2 = {"UNI", "AAVE", "COMP", "MKR", "MATIC"};

        // Priority 3: Complete Tier 1 universe (Hours 9-16)
        std::vector<std::string> priority3 = {
            "AVAX", "SOL", "ATOM", "NEAR", "CRV", "SNX",
            "LRC", "OMG", "VET", "FIL"
        };
    };

    // Quality validation metrics for loaded data
    struct DataQualityMetrics {
        double completenessScore;        // % of expected data points
        double consistencyScore;         // Absence of anomalies
        double timelinessScore;          // Recency of data
        double crossValidationScore;     // Agreement between sources
        double overallQualityScore;      // Composite quality metric

        bool isAcceptableQuality() const {
            return overallQualityScore >= 0.98 && completenessScore >= 0.98;
        }
    };

    // Historical data loading configuration
    struct HistoricalDataLoadingPlan {
        int targetDays = 730;                    // 2 years comprehensive coverage
        int dailyApiCallBudget = 200;            // Conservative API usage
        int requestDelaySeconds = 10;            // Anti-rate-limiting delay
        int batchSize = 5;                       // Symbols per batch

        // Quality validation requirements
        double minDataCompleteness = 0.98;       // 98% completeness requirement
        int maxConsecutiveMissingDays = 3;       // Maximum gap tolerance

        // Provider configuration with fallbacks
        std::vector<std::string> fallbackProviders = {
            "CryptoCompare", "AlphaVantage", "CoinGecko", "Binance"
        };

        bool enableAutomaticGapFilling = true;   // Interpolate small gaps
        bool enableQualityScoring = true;        // Real-time quality metrics
    };

    // Loading execution timeline
    struct LoadingTimeline {
        int phase1Hours = 4;                     // Priority 1 symbols
        int phase2Hours = 4;                     // Priority 2 symbols
        int phase3Hours = 8;                     // Priority 3 symbols
        int validationHours = 4;                 // Quality validation
        int totalHours = 20;                     // Complete loading cycle
    };

    // Progress tracking and reporting
    struct LoadingProgress {
        int symbolsCompleted = 0;
        int symbolsTotal = 20;
        double overallProgress = 0.0;
        std::string currentPhase = "Not Started";
        std::chrono::system_clock::time_point startTime;
        std::chrono::system_clock::time_point estimatedCompletion;

        // Phase-specific progress
        bool phase1Complete = false;
        bool phase2Complete = false;
        bool phase3Complete = false;
        bool validationComplete = false;
    };

public:
    Phase1DataLoader();
    ~Phase1DataLoader() = default;

    // Main loading orchestration
    bool startDataLoading();
    bool resumeDataLoading();
    void stopDataLoading();

    // Progress monitoring
    LoadingProgress getLoadingProgress() const;
    DataQualityMetrics getDataQualityMetrics(const std::string& symbol) const;
    std::map<std::string, DataQualityMetrics> getAllQualityMetrics() const;

    // Phase-specific loading methods
    bool loadPriority1Symbols();     // BTC, ETH, ADA, DOT, LINK
    bool loadPriority2Symbols();     // UNI, AAVE, COMP, MKR, MATIC
    bool loadPriority3Symbols();     // Remaining Tier 1 universe
    bool validateLoadedData();       // Comprehensive quality validation

    // Data access and validation
    bool isSymbolDataComplete(const std::string& symbol) const;
    bool isAllDataLoaded() const;
    std::vector<std::string> getLoadedSymbols() const;
    std::vector<std::string> getPendingSymbols() const;

    // Error handling and recovery
    std::vector<std::string> getFailedSymbols() const;
    bool retryFailedSymbols();
    void generateLoadingReport() const;

    // Callback registration for progress updates
    void setProgressCallback(std::function<void(const LoadingProgress&)> callback);
    void setQualityCallback(std::function<void(const std::string&, const DataQualityMetrics&)> callback);

private:
    // Internal loading state
    LoadingSequence loadingSequence_;
    HistoricalDataLoadingPlan loadingPlan_;
    LoadingProgress progress_;

    // Quality tracking
    std::map<std::string, DataQualityMetrics> qualityMetrics_;
    std::vector<std::string> failedSymbols_;

    // Callbacks
    std::function<void(const LoadingProgress&)> progressCallback_;
    std::function<void(const std::string&, const DataQualityMetrics&)> qualityCallback_;

    // Internal helper methods
    bool loadSymbolData(const std::string& symbol, int retryCount = 0);
    DataQualityMetrics calculateQualityMetrics(const std::string& symbol) const;
    bool validateSymbolData(const std::string& symbol);
    void updateProgress(const std::string& phase, double phaseProgress);

    // Provider management
    std::string getCurrentProvider() const;
    bool switchToFallbackProvider();
    void resetProviderSelection();

    // Constants
    static constexpr int MAX_RETRY_ATTEMPTS = 3;
    static constexpr double QUALITY_THRESHOLD = 0.98;
    static constexpr int RATE_LIMIT_DELAY_MS = 10000;
};

/**
 * Specialized data loader for TRS Phase 1 validation requirements
 * Implements enhanced validation with statistical significance testing
 */
class TRSPhase1DataValidator {
public:
    // TRS-specific validation requirements
    struct TRSValidationRequirements {
        double minDataCompleteness = 0.98;       // 98% completeness minimum
        int minHistoricalDays = 730;             // 2-year minimum coverage
        double maxGapDays = 3;                   // Maximum consecutive gap
        bool requiresMultiProviderValidation = true; // Cross-provider validation

        // Statistical requirements
        int minNewsArticlesPerWeek = 5;          // News coverage requirement
        int minExchangeListings = 5;             // Exchange availability
        double maxVolatilityAnnualized = 2.0;    // Volatility limit (200%)
        double minLiquidityScore = 0.5;          // Liquidity threshold
    };

    // Validation results structure
    struct ValidationResults {
        bool passesAllRequirements = false;
        std::map<std::string, bool> symbolValidation;
        std::map<std::string, std::vector<std::string>> validationErrors;
        std::map<std::string, Phase1DataLoader::DataQualityMetrics> qualityScores;

        double overallPassRate = 0.0;
        int symbolsPassed = 0;
        int symbolsTotal = 20;

        std::chrono::system_clock::time_point validationTime;
    };

public:
    TRSPhase1DataValidator();
    ~TRSPhase1DataValidator() = default;

    // Main validation methods
    ValidationResults validateTier1Universe();
    bool validateSymbol(const std::string& symbol, const TRSValidationRequirements& requirements);

    // Specific validation checks
    bool validateDataCompleteness(const std::string& symbol) const;
    bool validateHistoricalCoverage(const std::string& symbol) const;
    bool validateDataQuality(const std::string& symbol) const;
    bool validateMarketRequirements(const std::string& symbol) const;

    // Reporting and documentation
    void generateTRSValidationReport(const ValidationResults& results) const;
    void exportValidationData(const std::string& filename) const;

    // Configuration
    void setValidationRequirements(const TRSValidationRequirements& requirements);
    TRSValidationRequirements getValidationRequirements() const;

private:
    TRSValidationRequirements requirements_;
    ValidationResults lastValidationResults_;

    // Internal validation helpers
    Phase1DataLoader::DataQualityMetrics assessDataQuality(const std::string& symbol) const;
    bool checkNewsAvailability(const std::string& symbol) const;
    bool checkExchangeListings(const std::string& symbol) const;
    double calculateVolatilityMetric(const std::string& symbol) const;
    double calculateLiquidityScore(const std::string& symbol) const;
};

} // namespace Data
} // namespace CryptoClaude