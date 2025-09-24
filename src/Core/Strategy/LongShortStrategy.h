#pragma once
#include "IStrategy.h"
#include "../Common/TradingTypes.h"
#include <map>
#include <set>

using namespace CryptoClaude::Common;

namespace CryptoClaude {
namespace Strategy {

class LongShortStrategy : public IStrategy {
public:
    LongShortStrategy();
    ~LongShortStrategy() override = default;

    // IStrategy interface implementation
    std::string getName() const override { return "Long-Short Strategy"; }
    std::string getDescription() const override {
        return "Market-neutral long-short pairing strategy with expected return ranking and coin exclusion";
    }
    std::string getVersion() const override { return "1.0"; }

    std::vector<TradingSignal> generateSignals(
        const std::string& currentDate,
        const Database::Models::Portfolio& currentPortfolio,
        const std::vector<PredictionData>& predictions) override;

    bool shouldRebalance(
        const Database::Models::Portfolio& portfolio,
        const std::vector<PredictionData>& predictions) override;

    void setParameters(const std::map<std::string, double>& parameters) override;
    void setParameters(const StrategyParameters& parameters);
    std::map<std::string, double> getParameters() const override;
    StrategyParameters getStrategyParameters() const;
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

    // Long-Short Pairing Strategy specific methods
    void setMaxPairs(int pairs) { m_maxTradingPairs = pairs; }
    void setCashBufferRatio(double ratio) { m_cashBufferRatio = ratio; }
    void setMinConfidenceThreshold(double threshold) { m_minConfidenceThreshold = threshold; }
    void setMaxPairAllocation(double maxAllocation) { m_maxPairAllocation = maxAllocation; }
    void setRebalanceThreshold(double threshold) { m_rebalanceThreshold = threshold; }

    // Risk management
    void setMaxDrawdownLimit(double limit) { m_maxDrawdownLimit = limit; }
    void setMaxPortfolioRisk(double risk) { m_maxPortfolioRisk = risk; }
    void setStopLossThreshold(double threshold) { m_stopLossThreshold = threshold; }

    // Transaction cost modeling
    void setTransactionCosts(double basisPoints) { m_transactionCostBps = basisPoints; }
    double calculateTransactionCost(double notionalAmount) const;

public:
    // Make TradingPair accessible for integration
    using TradingPairType = TradingPair;

private:
    // Unified strategy parameters
    StrategyParameters m_parameters;

    // Performance tracking
    mutable StrategyMetrics m_metrics;
    std::vector<double> m_portfolioValues;
    std::vector<std::chrono::system_clock::time_point> m_valueDates;
    double m_highWaterMark;

    // Pair tracking using unified types
    std::vector<TradingPair> m_currentPairs;
    std::vector<TradingPair> m_targetPairs;
    std::map<std::string, double> m_currentWeights;
    std::map<std::string, double> m_targetWeights;

    // Helper methods using unified types
    std::vector<PredictionData> filterPredictions(
        const std::vector<PredictionData>& predictions) const;

    // Core pairing algorithm - rank by expected return and pair top-bottom
    std::vector<TradingPair> createTradingPairs(
        const std::vector<PredictionData>& predictions) const;

    // Filter predictions based on coin exclusion criteria
    std::vector<PredictionData> filterExcludedCoins(
        const std::vector<PredictionData>& predictions) const;

    // Allocate capital to pairs based on confidence
    std::vector<TradingPair> allocateCapitalToPairs(
        const std::vector<TradingPair>& pairs) const;

    std::vector<TradingSignal> generateRebalanceSignals(
        const std::vector<TradingPair>& targetPairs,
        const Database::Models::Portfolio& currentPortfolio) const;

    // Convert pairs to individual position targets
    std::vector<TradingSignal> convertPairsToSignals(
        const std::vector<TradingPair>& pairs,
        const Database::Models::Portfolio& currentPortfolio) const;

    // Pair-specific risk calculations
    double calculatePairRisk(const TradingPair& pair) const;
    bool isPairWithinLimits(const TradingPair& pair) const;
    double calculateTotalPairExposure(const std::vector<TradingPair>& pairs) const;

    // Cash buffer protection (critical - never breach)
    bool isCashBufferPreserved(const std::vector<TradingPair>& pairs) const;
    double calculateRequiredCash(const std::vector<TradingPair>& pairs) const;

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

    // Pair utilities
    double calculatePairReturn(const TradingPair& pair) const;
    double calculatePairConfidence(const TradingPair& pair) const;
    bool isSignificantPairChange(const TradingPair& currentPair, const TradingPair& targetPair) const;

    // Weight normalization for pairing approach
    std::vector<TradingPair> normalizePairWeights(
        const std::vector<TradingPair>& pairs, double totalInvestmentRatio) const;

    // Validation helpers for pairing strategy
    bool validatePairingParameters() const;
    bool validateCashBuffer() const;
    bool validatePairLimits() const;
    bool validateAllPairs(const std::vector<TradingPair>& pairs) const;
};

// Pairing Strategy utilities
class LongShortPairingUtils {
public:
    // Pair construction helpers
    static std::vector<TradingPair> createOptimalPairs(
        const std::vector<PredictionData>& predictions,
        int maxPairs);

    // Expected return ranking and pairing
    static std::vector<PredictionData> rankByExpectedReturn(
        const std::vector<PredictionData>& predictions);

    static std::vector<TradingPair> pairTopWithBottom(
        const std::vector<PredictionData>& rankedPredictions,
        int maxPairs);

    // Pair-specific risk management
    static double calculatePairCorrelation(
        const std::string& longSymbol,
        const std::string& shortSymbol,
        const std::vector<Database::Models::MarketData>& marketData);

    static bool isPairMarketNeutral(
        const TradingPair& pair,
        double correlationThreshold = 0.3);

    // Pair-based transaction cost analysis
    static double calculatePairTurnover(
        const std::vector<TradingPair>& currentPairs,
        const std::vector<TradingPair>& targetPairs);

    static double estimatePairTransactionCosts(
        const std::vector<TradingPair>& pairs,
        double costBasisPoints);

    // Pair performance attribution
    struct PairAttributionResult {
        double longReturn;
        double shortReturn;
        double pairReturn;              // Long - Short (market-neutral return)
        double pairContribution;
        std::string longSymbol;
        std::string shortSymbol;
    };

    static std::vector<PairAttributionResult> performPairAttribution(
        const std::vector<TradingPair>& pairs,
        const Database::Models::Portfolio& portfolio);

    // Cash buffer analysis
    static bool validateCashBuffer(
        const std::vector<TradingPair>& pairs,
        double totalCapital,
        double requiredCashRatio);

private:
    static std::vector<double> calculateReturns(const std::vector<double>& prices);
    static double calculateCorrelation(const std::vector<double>& x, const std::vector<double>& y);
};

// Compatibility alias for existing code
using LongShortStrategyUtils = LongShortPairingUtils;

} // namespace Strategy
} // namespace CryptoClaude