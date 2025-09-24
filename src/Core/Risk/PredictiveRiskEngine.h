#pragma once

#include "../Database/Models/PortfolioData.h"
#include "../Database/Models/MarketData.h"
#include "../Database/Models/SentimentData.h"
#include "../../Configuration/Config.h"
#include <vector>
#include <map>
#include <string>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <optional>
#include <chrono>
#include <memory>
#include <set>

namespace CryptoClaude {
namespace Risk {

using namespace CryptoClaude::Database::Models;

// Structures for coin exclusion framework - designed for long-short pairing strategy

struct CoinExclusionAlert {
    enum AlertLevel {
        INFO,           // Informational
        WARNING,        // Coin becoming risky
        CRITICAL,       // Exclude from pairing immediately
        EMERGENCY       // Market-wide issue
    };

    enum ExclusionReason {
        LIQUIDITY_CRISIS,      // Volume too low, spreads too wide
        EXTREME_VOLATILITY,    // Volatility beyond acceptable bounds
        MODEL_CONFIDENCE_LOW,  // Our model has low confidence
        TECHNICAL_BREAKDOWN,   // Chart patterns suggest major move
        NEWS_EVENT_RISK,       // Major announcement/regulatory issue
        CORRELATION_ANOMALY    // Not behaving as expected vs market
    };

    AlertLevel level;
    ExclusionReason reason;
    std::string symbol;                 // Specific coin to exclude
    std::string message;
    double exclusionScore;              // 0-1, higher = more reason to exclude
    std::vector<std::string> suggestedActions;
    std::chrono::system_clock::time_point alertTimestamp;
    bool shouldExclude;                 // Final recommendation
};

struct CoinExclusionCriteria {
    // Liquidity thresholds for pairing
    double minDailyVolumeUSD = 100e6;         // $100M minimum daily volume
    double maxBidAskSpread = 0.005;           // 0.5% max bid-ask spread
    int minExchangeCount = 3;                 // Available on at least 3 major exchanges

    // Volatility thresholds
    double maxDailyVolatility = 0.25;         // 25% max daily price change
    double maxVolatilitySpike = 3.0;          // 3x normal volatility spike

    // Model confidence thresholds
    double minModelConfidence = 0.30;         // 30% min confidence to include coin
    double lowConfidenceThreshold = 0.40;     // Below this = reduced allocation

    // Technical analysis thresholds
    double maxRSIExtreme = 85.0;              // RSI above 85 = overbought exclusion
    double minRSIExtreme = 15.0;              // RSI below 15 = oversold exclusion
};

struct CoinExclusionMetrics {
    std::string symbol;
    bool isExcluded = false;
    double exclusionScore = 0.0;              // 0-1, higher = more reason to exclude
    std::vector<CoinExclusionAlert::ExclusionReason> activeReasons;
    std::chrono::system_clock::time_point lastAssessment;
};

class CoinExclusionEngine {
private:
    // Core data for exclusion decisions
    std::map<std::string, std::vector<double>> priceHistory_;
    std::map<std::string, std::vector<double>> volumeHistory_;
    std::map<std::string, std::vector<double>> volatilityHistory_;
    std::map<std::string, std::vector<SentimentData>> sentimentHistory_;
    std::map<std::string, MarketData> latestMarketData_;

    // Exclusion configuration and state
    CoinExclusionCriteria exclusionCriteria_;
    std::vector<CoinExclusionAlert> activeExclusions_;
    std::set<std::string> currentlyExcludedCoins_;
    std::map<std::string, CoinExclusionMetrics> exclusionMetrics_;

    // Lookback windows for analysis
    int liquidityLookbackDays_ = 7;            // Days to analyze liquidity
    int volatilityLookbackDays_ = 30;          // Days to analyze volatility
    int modelConfidenceLookbackDays_ = 14;     // Days to assess model performance
    int technicalLookbackDays_ = 20;           // Days for technical analysis

public:
    explicit CoinExclusionEngine() {
        initializeDefaultCriteria();
    }

    // === COIN EXCLUSION FRAMEWORK (MAIN PURPOSE) ===

    // Primary exclusion method: should this coin be excluded from pairing?
    bool shouldExcludeCoin(const std::string& symbol);

    // Individual exclusion criteria
    CoinExclusionAlert checkLiquidityExclusion(const std::string& symbol);
    CoinExclusionAlert checkVolatilityExclusion(const std::string& symbol);
    CoinExclusionAlert checkModelConfidenceExclusion(const std::string& symbol);
    CoinExclusionAlert checkTechnicalExclusion(const std::string& symbol);
    CoinExclusionAlert checkNewsEventExclusion(const std::string& symbol);

    // Batch exclusion processing
    std::vector<std::string> filterExcludedCoins(const std::vector<std::string>& candidates);
    std::map<std::string, CoinExclusionAlert> assessAllCoins(const std::vector<std::string>& coins);

    // Current exclusion status
    std::set<std::string> getCurrentExclusions() const { return currentlyExcludedCoins_; }
    std::vector<CoinExclusionAlert> getActiveExclusionAlerts() const { return activeExclusions_; }

    // === UTILITY & ANALYSIS METHODS FOR EXCLUSION ===

    // Calculate current liquidity metrics
    double calculateLiquidityScore(const std::string& symbol, int lookbackDays = 7);
    double calculateBidAskSpread(const std::string& symbol);
    int countActiveExchanges(const std::string& symbol);

    // Calculate volatility metrics
    double calculateRealizedVolatility(const std::string& symbol, int lookbackDays = 30);
    double calculateVolatilitySpike(const std::string& symbol);
    bool isVolatilityExtreme(const std::string& symbol);

    // Technical analysis helpers
    double calculateRSI(const std::string& symbol, int period = 14);
    bool isTechnicallyOverbought(const std::string& symbol);
    bool isTechnicallyOversold(const std::string& symbol);

    // Model confidence assessment
    double assessModelConfidence(const std::string& symbol, int lookbackDays = 14);
    bool hasRecentModelFailures(const std::string& symbol);

    // News and sentiment analysis
    bool hasRecentNegativeNews(const std::string& symbol);
    double calculateSentimentInstability(const std::string& symbol, int lookbackDays = 7);

    // === DATA MANAGEMENT & UPDATES ===

    // Update market and sentiment data
    void updateMarketData(const std::string& symbol, const MarketData& data);
    void updateSentimentData(const std::string& symbol, const SentimentData& data);

    // Batch data updates for efficiency
    void updateMultipleMarketData(const std::map<std::string, MarketData>& dataMap);
    void updateMultipleSentimentData(const std::map<std::string, SentimentData>& sentimentMap);

    // Data validation
    bool hasValidMarketData(const std::string& symbol, int minDataPoints = 30);
    bool hasRecentData(const std::string& symbol, int maxAgeHours = 24);

    // === CONFIGURATION & UTILITIES ===

    // Configure exclusion criteria
    void setExclusionCriteria(const CoinExclusionCriteria& criteria) { exclusionCriteria_ = criteria; }
    CoinExclusionCriteria getExclusionCriteria() const { return exclusionCriteria_; }

    // Configure lookback windows
    void setLiquidityLookbackDays(int days) { liquidityLookbackDays_ = days; }
    void setVolatilityLookbackDays(int days) { volatilityLookbackDays_ = days; }
    void setModelConfidenceLookbackDays(int days) { modelConfidenceLookbackDays_ = days; }
    void setTechnicalLookbackDays(int days) { technicalLookbackDays_ = days; }

    // Exclusion diagnostics
    struct ExclusionDiagnostics {
        int totalCoinsAssessed;
        int coinsExcluded;
        std::map<CoinExclusionAlert::ExclusionReason, int> exclusionReasonCounts;
        std::chrono::system_clock::time_point lastAssessment;
        std::vector<std::string> diagnosticWarnings;
    };

    ExclusionDiagnostics getDiagnostics() const;

private:
    // === INTERNAL HELPER METHODS ===

    // Statistical calculations
    std::vector<double> calculateReturns(const std::vector<double>& prices);
    double calculateStandardDeviation(const std::vector<double>& data);
    double calculateMean(const std::vector<double>& data);
    double calculatePercentile(const std::vector<double>& data, double percentile);

    // Technical indicator calculations
    std::vector<double> calculateMovingAverage(const std::vector<double>& data, int period);
    std::vector<double> calculateRSIValues(const std::vector<double>& prices, int period = 14);

    // Data processing helpers
    std::vector<double> getRecentPrices(const std::string& symbol, int days);
    std::vector<double> getRecentVolumes(const std::string& symbol, int days);
    std::vector<double> getRecentVolatilities(const std::string& symbol, int days);

    // Exclusion logic helpers
    double calculateExclusionScore(const std::string& symbol, const std::vector<CoinExclusionAlert::ExclusionReason>& reasons);
    void updateExclusionMetrics(const std::string& symbol, const CoinExclusionMetrics& metrics);
    void cleanupOldExclusions(int maxAgeHours = 24);

    // Initialization
    void initializeDefaultCriteria();

    // Data validation
    bool validatePriceData(const std::vector<double>& prices);
    bool hasMinimumDataPoints(const std::string& symbol, int minPoints);

    // Logging and error handling
    void logExclusionWarning(const std::string& symbol, const std::string& warning);
    void handleExclusionError(const std::string& symbol, const std::string& error);

    // Clear exclusion state
    void clearAllExclusions();
    void clearExclusionForSymbol(const std::string& symbol);
};

// Compatibility typedef for existing code
using PredictiveRiskEngine = CoinExclusionEngine;

} // namespace Risk
} // namespace CryptoClaude