#pragma once
#include "IStrategy.h"
#include <map>
#include <set>

namespace CryptoClaude {
namespace Strategy {

class LongShortStrategy : public IStrategy {
public:
    LongShortStrategy();
    ~LongShortStrategy() override = default;

    // IStrategy interface implementation
    std::string getName() const override { return "Long-Short Strategy"; }
    std::string getDescription() const override {
        return "Market-neutral long-short strategy based on sentiment and inflow predictions";
    }
    std::string getVersion() const override { return "1.0"; }

    std::vector<TradingSignal> generateSignals(
        const std::string& currentDate,
        const Database::Models::Portfolio& currentPortfolio,
        const std::vector<MachineLearning::DailyPrediction>& predictions) override;

    bool shouldRebalance(
        const Database::Models::Portfolio& portfolio,
        const std::vector<MachineLearning::DailyPrediction>& predictions) override;

    void setParameters(const std::map<std::string, double>& parameters) override;
    std::map<std::string, double> getParameters() const override;
    std::vector<std::string> getRequiredParameters() const override;

    double calculatePositionSize(
        const std::string& symbol,
        double expectedReturn,
        double volatility,
        const Database::Models::Portfolio& portfolio) override;

    bool checkRiskLimits(
        const std::vector<TradingSignal>& signals,
        const Database::Models::Portfolio& portfolio) override;

    bool validateConfiguration() const override;
    std::vector<std::string> getConfigurationErrors() const override;

    void updatePerformanceMetrics(const Database::Models::Portfolio& portfolio) override;
    StrategyMetrics getPerformanceMetrics() const override;

    // LongShortStrategy specific methods
    void setLongCount(int count) { m_longPositions = count; }
    void setShortCount(int count) { m_shortPositions = count; }
    void setMinConfidenceThreshold(double threshold) { m_minConfidenceThreshold = threshold; }
    void setMaxPositionSize(double maxSize) { m_maxPositionSize = maxSize; }
    void setRebalanceThreshold(double threshold) { m_rebalanceThreshold = threshold; }

    // Risk management
    void setMaxDrawdownLimit(double limit) { m_maxDrawdownLimit = limit; }
    void setMaxPortfolioRisk(double risk) { m_maxPortfolioRisk = risk; }
    void setStopLossThreshold(double threshold) { m_stopLossThreshold = threshold; }

    // Transaction cost modeling
    void setTransactionCosts(double basisPoints) { m_transactionCostBps = basisPoints; }
    double calculateTransactionCost(double notionalAmount) const;

private:
    // Strategy parameters
    int m_longPositions;
    int m_shortPositions;
    double m_minConfidenceThreshold;
    double m_maxPositionSize;
    double m_rebalanceThreshold;

    // Risk management parameters
    double m_maxDrawdownLimit;
    double m_maxPortfolioRisk;
    double m_stopLossThreshold;
    double m_transactionCostBps;

    // Performance tracking
    mutable StrategyMetrics m_metrics;
    std::vector<double> m_portfolioValues;
    std::vector<std::chrono::system_clock::time_point> m_valueDates;
    double m_highWaterMark;

    // Position tracking
    std::map<std::string, double> m_currentWeights;
    std::map<std::string, double> m_targetWeights;

    // Helper methods
    std::vector<MachineLearning::DailyPrediction> filterPredictions(
        const std::vector<MachineLearning::DailyPrediction>& predictions) const;

    std::vector<std::string> selectLongPositions(
        const std::vector<MachineLearning::DailyPrediction>& rankedPredictions) const;

    std::vector<std::string> selectShortPositions(
        const std::vector<MachineLearning::DailyPrediction>& rankedPredictions) const;

    std::vector<TradingSignal> generateRebalanceSignals(
        const std::vector<std::string>& longSymbols,
        const std::vector<std::string>& shortSymbols,
        const Database::Models::Portfolio& currentPortfolio,
        const std::vector<MachineLearning::DailyPrediction>& predictions) const;

    // Risk calculations
    double calculatePortfolioRisk(
        const std::vector<TradingSignal>& signals,
        const Database::Models::Portfolio& portfolio) const;

    double calculatePositionRisk(const std::string& symbol, double weight) const;

    bool isWithinRiskLimits(double portfolioRisk) const;

    // Performance calculations
    void updateMetricsFromPortfolio(const Database::Models::Portfolio& portfolio);
    double calculateDrawdown(double currentValue) const;
    double calculateSharpeRatio() const;
    double calculateVolatility() const;

    // Utility methods
    TradingSignal createTradingSignal(
        const std::string& symbol,
        SignalType signalType,
        double targetWeight,
        const MachineLearning::DailyPrediction& prediction) const;

    double normalizeWeight(double rawWeight, int totalPositions) const;
    bool isSignificantWeightChange(const std::string& symbol, double newWeight) const;

    // Validation helpers
    bool validatePositionCounts() const;
    bool validateRiskParameters() const;
    bool validateThresholds() const;
};

// Strategy utilities
class LongShortStrategyUtils {
public:
    // Portfolio construction helpers
    static std::map<std::string, double> calculateOptimalWeights(
        const std::vector<MachineLearning::DailyPrediction>& predictions,
        int longCount,
        int shortCount);

    // Risk management utilities
    static double calculatePortfolioBeta(
        const Database::Models::Portfolio& portfolio,
        const std::vector<Database::Models::MarketData>& marketData);

    static double calculateTrackingError(
        const std::vector<double>& portfolioReturns,
        const std::vector<double>& benchmarkReturns);

    // Transaction cost analysis
    static double calculateTurnover(
        const std::map<std::string, double>& currentWeights,
        const std::map<std::string, double>& targetWeights);

    static double estimateTransactionCosts(
        const std::vector<TradingSignal>& signals,
        double costBasisPoints);

    // Performance attribution
    struct AttributionResult {
        double selectionReturn;
        double allocationReturn;
        double interactionReturn;
        double totalActiveReturn;
    };

    static AttributionResult performAttribution(
        const Database::Models::Portfolio& portfolio,
        const std::vector<Database::Models::MarketData>& benchmarkData);

private:
    static std::vector<double> calculateReturns(const std::vector<double>& prices);
    static double calculateCorrelation(const std::vector<double>& x, const std::vector<double>& y);
};

} // namespace Strategy
} // namespace CryptoClaude