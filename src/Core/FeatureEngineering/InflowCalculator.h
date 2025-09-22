#pragma once
#include "../Database/Models/MarketData.h"
#include <vector>
#include <map>
#include <string>
#include <chrono>

namespace CryptoClaude {
namespace FeatureEngineering {

struct LiquidityMetrics {
    std::string symbol;
    double lambda250;           // Liquidity lambda for past 250 days
    double lambda250_500;       // Liquidity lambda for days 250-500 ago
    double rSquared250;         // R² for the past 250 days regression
    double marketCap;           // Market capitalization
    bool isValid;               // Whether the metrics are reliable

    LiquidityMetrics()
        : lambda250(0.0), lambda250_500(0.0), rSquared250(0.0),
          marketCap(0.0), isValid(false) {}
};

class InflowCalculator {
public:
    InflowCalculator() = default;
    ~InflowCalculator() = default;

    // Primary inflow calculations
    double calculateNetInflow(
        double currentPrice,
        double previousPrice,
        double volume);

    double calculateExcessInflow(
        double netInflow,
        const std::vector<double>& marketInflowsAtSameTime);

    // Daily inflow from hourly data aggregation
    std::map<std::string, double> computeDailyInflowFromHourly(
        const std::vector<Database::Models::MarketData>& hourlyData);

    // Batch processing for database updates
    bool updateNetInflowInDatabase(
        const std::string& tableName = "market_data");

    bool updateExcessInflowInDatabase(
        const std::string& tableName = "market_data");

    // Liquidity impact calculation (λ - lambda)
    LiquidityMetrics calculateLiquidityLambda(
        const std::string& symbol,
        const std::vector<Database::Models::MarketData>& marketData,
        const std::map<std::string, double>& marketCapData);

    std::map<std::string, LiquidityMetrics> calculateLiquidityLambdaBatch(
        const std::map<std::string, std::vector<Database::Models::MarketData>>& marketDataBySymbol,
        const std::map<std::string, double>& marketCapData);

    // Filtering and validation
    std::map<std::string, LiquidityMetrics> filterLiquidityMetrics(
        const std::map<std::string, LiquidityMetrics>& metrics,
        double minRSquared = 0.3,
        double maxLambdaDeviation = 0.5);

    // Regression analysis for lambda calculation
    struct RegressionResult {
        double slope;           // λ (lambda) - the liquidity impact coefficient
        double intercept;       // Regression intercept
        double rSquared;        // R-squared goodness of fit
        double standardError;   // Standard error of the slope
        int dataPoints;         // Number of data points used

        bool isSignificant(double pValueThreshold = 0.05) const;
    };

    RegressionResult performLinearRegression(
        const std::vector<double>& inflowRatios,
        const std::vector<double>& priceChanges);

    // Inflow processing utilities
    std::vector<Database::Models::MarketData> removeFirstRowPerCoin(
        const std::vector<Database::Models::MarketData>& data);

    bool validateInflowData(const std::vector<Database::Models::MarketData>& data);

    std::vector<Database::Models::MarketData> removeStablecoins(
        const std::vector<Database::Models::MarketData>& data);

    // Statistical utilities for inflow analysis
    double calculateInflowVolatility(
        const std::vector<double>& inflows,
        int windowSize = 30);

    double calculateInflowTrend(
        const std::vector<double>& inflows,
        int windowSize = 30);

    std::vector<double> calculateRollingInflowStats(
        const std::vector<Database::Models::MarketData>& data,
        int windowSize = 30);

    // Configuration
    void setLambdaCalculationPeriods(int recent, int historical);
    void setRegressionFilters(double minRSquared, double maxDeviation);
    void setStablecoinList(const std::vector<std::string>& stablecoins);

    // Validation and diagnostics
    struct InflowDiagnostics {
        std::string symbol;
        int totalDataPoints;
        int validInflowPoints;
        double averageInflow;
        double inflowVolatility;
        double maxInflow;
        double minInflow;
        std::vector<std::string> warnings;
    };

    InflowDiagnostics diagnoseInflowData(
        const std::string& symbol,
        const std::vector<Database::Models::MarketData>& data);

    std::map<std::string, InflowDiagnostics> diagnoseInflowDataBatch(
        const std::map<std::string, std::vector<Database::Models::MarketData>>& dataBySymbol);

private:
    // Configuration parameters
    int m_recentPeriod = 250;
    int m_historicalPeriod = 250;
    double m_minRSquared = 0.3;
    double m_maxLambdaDeviation = 0.5;

    std::vector<std::string> m_stablecoins = {
        "USDT", "USDC", "BUSD", "DAI", "TUSD", "FDUSD", "PYUSD"
    };

    // Helper methods
    std::vector<double> extractInflowRatios(
        const std::vector<Database::Models::MarketData>& data,
        double marketCap);

    std::vector<double> extractPriceChanges(
        const std::vector<Database::Models::MarketData>& data);

    bool isStablecoin(const std::string& symbol) const;

    // Statistical computation helpers
    double calculateMean(const std::vector<double>& values);
    double calculateVariance(const std::vector<double>& values);
    double calculateCovariance(const std::vector<double>& x, const std::vector<double>& y);
    double calculateCorrelation(const std::vector<double>& x, const std::vector<double>& y);

    // Data validation helpers
    bool isValidPrice(double price) const;
    bool isValidVolume(double volume) const;
    bool isValidInflow(double inflow) const;

    // Database interaction helpers
    bool updateDatabaseColumn(
        const std::string& tableName,
        const std::string& columnName,
        const std::string& updateExpression);

    // Time series utilities
    std::vector<Database::Models::MarketData> sortByTime(
        const std::vector<Database::Models::MarketData>& data);

    std::map<std::string, std::vector<Database::Models::MarketData>> groupBySymbol(
        const std::vector<Database::Models::MarketData>& data);

    // Error handling and logging
    void logCalculationError(const std::string& operation, const std::string& error);
    void logCalculationWarning(const std::string& operation, const std::string& warning);
};

// Utility class for inflow-based alpha generation
class InflowAlphaGenerator {
public:
    struct AlphaSignal {
        std::string symbol;
        std::string date;
        double predictedInflow;
        double currentInflow;
        double alpha;           // Expected excess return
        double confidence;      // Signal confidence [0,1]
        int rank;               // Relative ranking

        AlphaSignal()
            : predictedInflow(0.0), currentInflow(0.0),
              alpha(0.0), confidence(0.0), rank(0) {}
    };

    InflowAlphaGenerator() = default;

    std::vector<AlphaSignal> generateAlphaSignals(
        const std::map<std::string, double>& predictedInflows,
        const std::map<std::string, double>& currentInflows,
        const std::map<std::string, LiquidityMetrics>& liquidityMetrics);

    std::vector<AlphaSignal> rankSignalsByAlpha(
        const std::vector<AlphaSignal>& signals);

    // Portfolio construction helpers
    std::vector<std::string> selectLongPositions(
        const std::vector<AlphaSignal>& rankedSignals,
        int count = 10);

    std::vector<std::string> selectShortPositions(
        const std::vector<AlphaSignal>& rankedSignals,
        int count = 10);

private:
    double convertInflowToPriceChange(double inflowRatio, double lambda);
    double calculateSignalConfidence(const AlphaSignal& signal, const LiquidityMetrics& metrics);
};

} // namespace FeatureEngineering
} // namespace CryptoClaude