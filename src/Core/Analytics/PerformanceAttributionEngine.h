#pragma once

#include "../Database/Models/PortfolioData.h"
#include "../Database/Models/MarketData.h"
#include "../Database/Models/SentimentData.h"
#include "../Portfolio/PortfolioOptimizer.h"
#include "../../Configuration/Config.h"
#include <vector>
#include <map>
#include <string>
#include <chrono>
#include <optional>
#include <algorithm>
#include <numeric>
#include <cmath>

namespace CryptoClaude {
namespace Analytics {

using namespace CryptoClaude::Database::Models;
using namespace CryptoClaude::Optimization;

// Forward declarations for performance analysis structures
struct PerformanceContribution {
    std::string factor;
    double contribution;           // Absolute contribution to returns
    double contributionPercent;    // Percentage of total returns
    std::string description;
    double confidence;            // Confidence in attribution (0-1)
};

struct FactorAttribution {
    std::string factorName;
    double totalReturn;
    double specificReturn;        // Return from this factor alone
    double interactionReturn;     // Interaction with other factors
    std::vector<PerformanceContribution> contributions;
};

struct AssetAttribution {
    std::string symbol;
    double totalReturn;
    double allocationEffect;      // Return from allocation decision
    double selectionEffect;       // Return from security selection
    double interactionEffect;     // Interaction between allocation and selection
    double leverageEffect;        // Impact of leverage on this position
    double sentimentEffect;       // Impact of sentiment on performance
    double weight;               // Average weight during period
    double activeWeight;         // Weight vs benchmark
};

struct PeriodPerformance {
    std::chrono::system_clock::time_point startDate;
    std::chrono::system_clock::time_point endDate;
    double totalReturn;
    double benchmarkReturn;
    double activeReturn;          // Total return - benchmark return
    double volatility;
    double sharpeRatio;
    double informationRatio;      // Active return / tracking error
    double maxDrawdown;
    double calmarRatio;
    double sortinoRatio;
    int tradingDays;
};

struct DrawdownAnalysis {
    double maxDrawdown;
    double currentDrawdown;
    std::chrono::system_clock::time_point drawdownStart;
    std::chrono::system_clock::time_point drawdownEnd;
    int drawdownDays;
    int recoveryDays;
    bool inDrawdown;
    std::vector<std::pair<std::chrono::system_clock::time_point, double>> drawdownHistory;
};

struct WinLossAnalysis {
    int totalTrades;
    int winningTrades;
    int losingTrades;
    double winRate;
    double averageWin;
    double averageLoss;
    double profitFactor;          // Gross profit / Gross loss
    int longestWinStreak;
    int longestLossStreak;
    int currentStreak;
    bool currentStreakIsWin;
    std::vector<double> tradeReturns;
};

struct RiskAdjustedMetrics {
    double sharpeRatio;
    double sortinoRatio;          // Uses downside deviation instead of total volatility
    double calmarRatio;           // Annual return / Max drawdown
    double treynorRatio;          // Excess return / Beta
    double informationRatio;      // Active return / Tracking error
    double jensenAlpha;           // Risk-adjusted excess return
    double trackingError;         // Standard deviation of active returns
    double downsideDeviation;     // Standard deviation of negative returns
};

class PerformanceAttributionEngine {
private:
    // Performance data storage
    std::vector<PeriodPerformance> performanceHistory_;
    std::map<std::string, std::vector<double>> factorReturns_;
    std::map<std::string, double> benchmarkWeights_;

    // Analysis parameters
    double riskFreeRate_ = 0.02;  // 2% risk-free rate
    int rollingWindowDays_ = 30;   // 30-day rolling window for metrics

    // Market data for benchmarking
    std::map<std::string, std::vector<MarketData>> historicalPrices_;
    std::map<std::string, std::vector<SentimentData>> historicalSentiment_;

public:
    PerformanceAttributionEngine(double riskFreeRate = 0.02, int rollingWindow = 30)
        : riskFreeRate_(riskFreeRate), rollingWindowDays_(rollingWindow) {
        initializeBenchmarks();
    }

    // === CORE PERFORMANCE ATTRIBUTION ===

    // Factor-based performance attribution
    std::vector<FactorAttribution> calculateFactorAttribution(
        const Portfolio& portfolio,
        const std::vector<Position>& positions,
        const std::chrono::system_clock::time_point& startDate,
        const std::chrono::system_clock::time_point& endDate
    );

    // Asset allocation vs security selection analysis
    std::vector<AssetAttribution> calculateAssetAttribution(
        const Portfolio& portfolio,
        const std::vector<Position>& positions,
        const std::map<std::string, double>& benchmarkWeights,
        const std::chrono::system_clock::time_point& startDate,
        const std::chrono::system_clock::time_point& endDate
    );

    // Leverage contribution analysis
    double calculateLeverageContribution(
        const Portfolio& portfolio,
        const std::vector<Position>& positions,
        const std::chrono::system_clock::time_point& startDate,
        const std::chrono::system_clock::time_point& endDate
    );

    // Sentiment impact attribution
    double calculateSentimentAttribution(
        const std::vector<Position>& positions,
        const std::chrono::system_clock::time_point& startDate,
        const std::chrono::system_clock::time_point& endDate
    );

    // === ADVANCED PERFORMANCE METRICS ===

    // Rolling performance metrics
    std::vector<double> calculateRollingSharpeRatio(
        const std::vector<double>& returns,
        int windowSize = 30
    );

    std::vector<double> calculateRollingVolatility(
        const std::vector<double>& returns,
        int windowSize = 30
    );

    // Maximum drawdown analysis with recovery time
    DrawdownAnalysis calculateDrawdownAnalysis(
        const std::vector<double>& portfolioValues,
        const std::vector<std::chrono::system_clock::time_point>& dates
    );

    // Win/loss streak analysis
    WinLossAnalysis calculateWinLossAnalysis(
        const std::vector<Position>& closedPositions
    );

    // Risk-adjusted return metrics (Sortino, Calmar, etc.)
    RiskAdjustedMetrics calculateRiskAdjustedMetrics(
        const std::vector<double>& returns,
        const std::vector<double>& benchmarkReturns = {},
        double beta = 1.0
    );

    // === BENCHMARKING FRAMEWORK ===

    // Market benchmark comparison
    PeriodPerformance calculateBenchmarkComparison(
        const Portfolio& portfolio,
        const std::string& benchmarkSymbol,
        const std::chrono::system_clock::time_point& startDate,
        const std::chrono::system_clock::time_point& endDate
    );

    // Strategy vs benchmark attribution
    std::vector<PerformanceContribution> calculateBenchmarkAttribution(
        const std::vector<double>& portfolioReturns,
        const std::vector<double>& benchmarkReturns,
        const std::vector<std::string>& attributionFactors = {"Selection", "Allocation", "Interaction"}
    );

    // Relative performance tracking
    std::vector<double> calculateActiveReturns(
        const std::vector<double>& portfolioReturns,
        const std::vector<double>& benchmarkReturns
    );

    // Benchmark-adjusted risk metrics
    double calculateTrackingError(
        const std::vector<double>& portfolioReturns,
        const std::vector<double>& benchmarkReturns
    );

    double calculateInformationRatio(
        const std::vector<double>& portfolioReturns,
        const std::vector<double>& benchmarkReturns
    );

    // === COMPREHENSIVE PERFORMANCE REPORTING ===

    struct PerformanceReport {
        PeriodPerformance overallPerformance;
        std::vector<FactorAttribution> factorAttributions;
        std::vector<AssetAttribution> assetAttributions;
        DrawdownAnalysis drawdownAnalysis;
        WinLossAnalysis winLossAnalysis;
        RiskAdjustedMetrics riskMetrics;
        std::vector<PerformanceContribution> topContributors;
        std::vector<PerformanceContribution> topDetractors;
        std::chrono::system_clock::time_point reportDate;
        std::string reportPeriod;
    };

    // Generate comprehensive performance report
    PerformanceReport generateComprehensiveReport(
        const Portfolio& portfolio,
        const std::vector<Position>& positions,
        const std::chrono::system_clock::time_point& startDate,
        const std::chrono::system_clock::time_point& endDate,
        const std::string& benchmarkSymbol = "BTC"
    );

    // === DATA MANAGEMENT ===

    // Update historical data
    void updateHistoricalPrices(const std::string& symbol, const std::vector<MarketData>& prices);
    void updateHistoricalSentiment(const std::string& symbol, const std::vector<SentimentData>& sentiment);

    // Set benchmark weights
    void setBenchmarkWeights(const std::map<std::string, double>& weights);
    void updateBenchmarkWeight(const std::string& symbol, double weight);

    // Performance history management
    void addPerformancePeriod(const PeriodPerformance& performance);
    std::vector<PeriodPerformance> getPerformanceHistory(int periods = -1) const;

    // === CONFIGURATION ===

    void setRiskFreeRate(double rate) { riskFreeRate_ = rate; }
    double getRiskFreeRate() const { return riskFreeRate_; }

    void setRollingWindowDays(int days) { rollingWindowDays_ = days; }
    int getRollingWindowDays() const { return rollingWindowDays_; }

    // === UTILITY METHODS ===

    // Statistical calculations
    double calculateVolatility(const std::vector<double>& returns);
    double calculateDownsideDeviation(const std::vector<double>& returns, double threshold = 0.0);
    double calculateBeta(const std::vector<double>& portfolioReturns, const std::vector<double>& marketReturns);
    double calculateCorrelation(const std::vector<double>& x, const std::vector<double>& y);

    // Return calculations
    std::vector<double> calculateReturns(const std::vector<double>& prices);
    std::vector<double> calculateCumulativeReturns(const std::vector<double>& returns);

    // Date and time utilities
    std::vector<std::chrono::system_clock::time_point> generateDateRange(
        const std::chrono::system_clock::time_point& start,
        const std::chrono::system_clock::time_point& end,
        int intervalDays = 1
    );

    int calculateTradingDays(
        const std::chrono::system_clock::time_point& start,
        const std::chrono::system_clock::time_point& end
    );

private:
    // === INTERNAL CALCULATION METHODS ===

    // Initialize default benchmarks
    void initializeBenchmarks();

    // Factor return calculations
    double calculateMarketFactor(
        const std::vector<Position>& positions,
        const std::chrono::system_clock::time_point& startDate,
        const std::chrono::system_clock::time_point& endDate
    );

    double calculateSizeFactor(
        const std::vector<Position>& positions,
        const std::chrono::system_clock::time_point& startDate,
        const std::chrono::system_clock::time_point& endDate
    );

    double calculateMomentumFactor(
        const std::vector<Position>& positions,
        const std::chrono::system_clock::time_point& startDate,
        const std::chrono::system_clock::time_point& endDate
    );

    double calculateVolatilityFactor(
        const std::vector<Position>& positions,
        const std::chrono::system_clock::time_point& startDate,
        const std::chrono::system_clock::time_point& endDate
    );

    // Attribution calculations
    double calculateAllocationEffect(
        const std::string& symbol,
        double portfolioWeight,
        double benchmarkWeight,
        double benchmarkReturn
    );

    double calculateSelectionEffect(
        const std::string& symbol,
        double benchmarkWeight,
        double assetReturn,
        double benchmarkReturn
    );

    double calculateInteractionEffect(
        const std::string& symbol,
        double portfolioWeight,
        double benchmarkWeight,
        double assetReturn,
        double benchmarkReturn
    );

    // Performance analysis helpers
    std::pair<int, int> findDrawdownPeriod(
        const std::vector<double>& values,
        int peakIndex
    );

    std::vector<int> findPeaks(const std::vector<double>& values, double threshold = 0.01);
    std::vector<int> findTroughs(const std::vector<double>& values, double threshold = 0.01);

    // Data validation
    bool validateDateRange(
        const std::chrono::system_clock::time_point& start,
        const std::chrono::system_clock::time_point& end
    );

    bool validateReturnsData(const std::vector<double>& returns);

    // Error handling
    void logAnalysisWarning(const std::string& warning);
    std::string formatPerformanceNumber(double value, bool isPercent = false);
};

// === PERFORMANCE ANALYSIS UTILITIES ===

class PerformanceUtils {
public:
    // Statistical functions
    static double calculateMean(const std::vector<double>& values);
    static double calculateMedian(const std::vector<double>& values);
    static double calculateStandardDeviation(const std::vector<double>& values);
    static double calculateSkewness(const std::vector<double>& values);
    static double calculateKurtosis(const std::vector<double>& values);

    // Performance ratios
    static double calculateSharpeRatio(double excessReturn, double volatility);
    static double calculateSortinoRatio(double excessReturn, double downsideDeviation);
    static double calculateCalmarRatio(double annualReturn, double maxDrawdown);
    static double calculateTreynorRatio(double excessReturn, double beta);

    // Risk metrics
    static double calculateVaR(const std::vector<double>& returns, double confidenceLevel = 0.05);
    static double calculateExpectedShortfall(const std::vector<double>& returns, double confidenceLevel = 0.05);
    static double calculateMaxDrawdown(const std::vector<double>& cumulativeReturns);

    // Time series analysis
    static std::vector<double> calculateMovingAverage(const std::vector<double>& values, int window);
    static std::vector<double> calculateExponentialMovingAverage(const std::vector<double>& values, double alpha);
    static double calculateAutoCorrelation(const std::vector<double>& values, int lag);

    // Benchmarking utilities
    static double calculateActiveReturn(double portfolioReturn, double benchmarkReturn);
    static double calculateRelativeReturn(double portfolioReturn, double benchmarkReturn);
    static std::vector<double> calculateRollingCorrelation(
        const std::vector<double>& x,
        const std::vector<double>& y,
        int window
    );
};

} // namespace Analytics
} // namespace CryptoClaude