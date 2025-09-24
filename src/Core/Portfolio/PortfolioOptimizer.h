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

namespace CryptoClaude {
namespace Optimization {

using namespace CryptoClaude::Database::Models;

// Forward declarations for optimization results
struct AllocationResult {
    std::string symbol;
    double targetWeight;
    double currentWeight;
    double rebalanceAmount;
    double expectedReturn;
    double riskContribution;
    std::string rationale;
};

struct OptimizationResult {
    std::vector<AllocationResult> allocations;
    double expectedPortfolioReturn;
    double expectedPortfolioRisk;
    double sharpeRatio;
    double diversificationRatio;
    std::string optimizationMethod;
    std::chrono::system_clock::time_point timestamp;
    bool requiresRebalancing;
    double totalRebalanceCost;
    std::vector<std::string> warnings;
};

// Rebalancing trigger conditions
struct RebalancingTrigger {
    enum TriggerType {
        THRESHOLD_BASED,    // Weight deviation exceeds threshold
        TIME_BASED,         // Regular schedule (daily, weekly)
        RISK_BASED,         // Risk metrics exceed limits
        COST_OPTIMIZED,     // Cost-benefit analysis
        SENTIMENT_DRIVEN,   // Market sentiment changes
        VOLATILITY_REGIME   // Volatility regime change
    };

    TriggerType type;
    double threshold;
    std::string description;
    bool isTriggered;
    double severity; // 0-1 scale
};

// Allocation constraints
struct AllocationConstraints {
    double minPositionWeight = 0.01;      // Minimum 1% allocation
    double maxPositionWeight = 0.40;      // Maximum 40% allocation
    double maxLeverage = 3.0;             // Maximum portfolio leverage
    double maxTurnover = 0.20;            // Maximum 20% turnover per rebalance
    double minCashBuffer = 0.05;          // Minimum 5% cash buffer
    double maxConcentrationRisk = 0.50;   // Maximum concentration (HHI)
    double transactionCostRate = 0.001;   // 0.1% transaction cost
    bool allowShortPositions = false;
    std::map<std::string, double> symbolMaxWeights; // Symbol-specific limits
};

class PortfolioOptimizer {
private:
    AllocationConstraints constraints_;
    std::vector<RebalancingTrigger> triggers_;

    // Risk and return estimation
    std::map<std::string, double> expectedReturns_;
    std::map<std::string, double> volatilities_;
    std::map<std::pair<std::string, std::string>, double> correlations_;

    // Market data for optimization
    std::map<std::string, MarketData> marketData_;
    std::map<std::string, SentimentData> sentimentData_;

public:
    explicit PortfolioOptimizer(const AllocationConstraints& constraints = AllocationConstraints{})
        : constraints_(constraints) {
        initializeDefaultTriggers();
    }

    // === MAIN OPTIMIZATION METHODS ===

    // Modern Portfolio Theory optimization
    OptimizationResult optimizePortfolio_MPT(
        const Portfolio& portfolio,
        const std::vector<Position>& currentPositions,
        const std::vector<std::string>& availableAssets,
        double targetReturn = 0.0,
        bool minimizeRisk = true
    );

    // Risk Parity allocation
    OptimizationResult optimizePortfolio_RiskParity(
        const Portfolio& portfolio,
        const std::vector<Position>& currentPositions,
        const std::vector<std::string>& availableAssets
    );

    // Equal weight allocation with risk adjustment
    OptimizationResult optimizePortfolio_EqualWeight(
        const Portfolio& portfolio,
        const std::vector<Position>& currentPositions,
        const std::vector<std::string>& availableAssets,
        bool riskAdjusted = true
    );

    // Volatility-based position sizing
    OptimizationResult optimizePortfolio_VolatilityWeighted(
        const Portfolio& portfolio,
        const std::vector<Position>& currentPositions,
        const std::vector<std::string>& availableAssets,
        bool inverseVolatility = true
    );

    // Sentiment-weighted allocation
    OptimizationResult optimizePortfolio_SentimentWeighted(
        const Portfolio& portfolio,
        const std::vector<Position>& currentPositions,
        const std::vector<std::string>& availableAssets,
        double sentimentWeight = 0.3
    );

    // Maximum diversification optimization
    OptimizationResult optimizePortfolio_MaxDiversification(
        const Portfolio& portfolio,
        const std::vector<Position>& currentPositions,
        const std::vector<std::string>& availableAssets
    );

    // === REBALANCING INTELLIGENCE ===

    // Check if rebalancing is needed
    bool shouldRebalance(
        const Portfolio& portfolio,
        const std::vector<Position>& currentPositions,
        const OptimizationResult& targetAllocation
    );

    // Generate rebalancing plan with cost analysis
    OptimizationResult generateRebalancingPlan(
        const Portfolio& portfolio,
        const std::vector<Position>& currentPositions,
        const OptimizationResult& targetAllocation,
        bool minimizeCosts = true
    );

    // Execute rebalancing with constraint validation
    std::vector<std::string> validateRebalancingPlan(
        const Portfolio& portfolio,
        const OptimizationResult& rebalancingPlan
    );

    // === CONSTRAINT AND RISK MANAGEMENT ===

    // Apply allocation constraints
    OptimizationResult applyConstraints(
        const OptimizationResult& unconstrained,
        const Portfolio& portfolio
    );

    // Calculate portfolio risk metrics for allocation
    double calculatePortfolioRisk(const std::vector<AllocationResult>& allocations);
    double calculateExpectedReturn(const std::vector<AllocationResult>& allocations);
    double calculateSharpeRatio(const std::vector<AllocationResult>& allocations);

    // === DATA INPUT AND MANAGEMENT ===

    // Update market data for optimization
    void updateMarketData(const std::string& symbol, const MarketData& data);
    void updateSentimentData(const std::string& symbol, const SentimentData& data);

    // Estimate expected returns and risk parameters
    void estimateReturnsAndRisk(const std::vector<std::string>& symbols, int lookbackDays = 30);

    // Update correlation matrix
    void updateCorrelationMatrix(const std::vector<std::string>& symbols);

    // === CONFIGURATION AND UTILITIES ===

    void setConstraints(const AllocationConstraints& constraints) { constraints_ = constraints; }
    const AllocationConstraints& getConstraints() const { return constraints_; }

    void addRebalancingTrigger(const RebalancingTrigger& trigger);
    void updateTriggerThresholds(RebalancingTrigger::TriggerType type, double newThreshold);

    // Performance analytics for optimization methods
    struct OptimizationPerformance {
        std::string method;
        double backtestReturn;
        double backtestVolatility;
        double maxDrawdown;
        double sharpeRatio;
        int rebalancingFrequency;
        double averageTransactionCosts;
    };

    OptimizationPerformance backtestOptimization(
        const std::string& method,
        const std::vector<Position>& historicalPositions,
        int backtestDays = 90
    );

private:
    // === INTERNAL OPTIMIZATION ALGORITHMS ===

    // Core MPT implementation
    std::vector<double> solveMPTOptimization(
        const std::vector<std::string>& assets,
        double targetReturn,
        bool minimizeRisk
    );

    // Risk parity weight calculation
    std::vector<double> calculateRiskParityWeights(const std::vector<std::string>& assets);

    // Volatility scaling for position sizing
    std::vector<double> calculateVolatilityScaling(
        const std::vector<std::string>& assets,
        bool inverse = true
    );

    // Sentiment score integration
    std::vector<double> integrateSentimentScores(
        const std::vector<std::string>& assets,
        const std::vector<double>& baseWeights,
        double sentimentWeight
    );

    // Maximum diversification ratio optimization
    std::vector<double> maximizeDiversificationRatio(const std::vector<std::string>& assets);

    // === REBALANCING LOGIC ===

    // Threshold-based rebalancing check
    bool checkThresholdTriggers(
        const std::vector<Position>& current,
        const OptimizationResult& target
    );

    // Cost-aware rebalancing optimization
    OptimizationResult optimizeRebalancingCosts(
        const OptimizationResult& idealAllocation,
        const std::vector<Position>& currentPositions
    );

    // Risk-based rebalancing triggers
    bool checkRiskTriggers(const Portfolio& portfolio, const std::vector<Position>& positions);

    // === UTILITY FUNCTIONS ===

    // Convert positions to current weights
    std::vector<AllocationResult> getCurrentAllocation(
        const Portfolio& portfolio,
        const std::vector<Position>& positions
    );

    // Calculate transaction costs
    double calculateTransactionCost(double rebalanceAmount, const std::string& symbol);

    // Validate weight constraints
    bool validateWeightConstraints(const std::vector<double>& weights);

    // Initialize default rebalancing triggers
    void initializeDefaultTriggers();

    // Matrix operations for optimization
    std::vector<std::vector<double>> calculateCovarianceMatrix(const std::vector<std::string>& assets);
    std::vector<double> solveQuadraticProgram(
        const std::vector<std::vector<double>>& Q,
        const std::vector<double>& c,
        const std::vector<std::vector<double>>& A,
        const std::vector<double>& b
    );

    // Risk contribution calculation
    double calculateRiskContribution(
        const std::string& asset,
        const std::vector<AllocationResult>& allocation
    );

    // Diversification metrics
    double calculateDiversificationRatio(const std::vector<AllocationResult>& allocation);
    double calculateEffectiveNumberOfAssets(const std::vector<AllocationResult>& allocation);

    // Error handling and validation
    bool validateInputData(const std::vector<std::string>& assets);
    void logOptimizationWarning(const std::string& warning, OptimizationResult& result);
};

// === IMPLEMENTATION OF KEY METHODS ===

inline void PortfolioOptimizer::initializeDefaultTriggers() {
    // Threshold-based rebalancing: 5% deviation from target
    triggers_.push_back({
        RebalancingTrigger::THRESHOLD_BASED,
        0.05,
        "Weight deviation exceeds 5% from target allocation",
        false,
        0.0
    });

    // Risk-based rebalancing: Portfolio risk exceeds limits
    triggers_.push_back({
        RebalancingTrigger::RISK_BASED,
        0.20, // 20% portfolio risk threshold
        "Portfolio risk metrics exceed acceptable limits",
        false,
        0.0
    });

    // Time-based rebalancing: Weekly review
    triggers_.push_back({
        RebalancingTrigger::TIME_BASED,
        7.0, // 7 days
        "Weekly scheduled rebalancing review",
        false,
        0.0
    });

    // Cost-optimized rebalancing: Only when benefits > costs
    triggers_.push_back({
        RebalancingTrigger::COST_OPTIMIZED,
        0.002, // 0.2% minimum benefit
        "Rebalancing benefits exceed transaction costs",
        false,
        0.0
    });
}

// === HELPER STRUCTURES FOR COMPLEX OPTIMIZATIONS ===

struct OptimizationContext {
    Portfolio portfolio;
    std::vector<Position> positions;
    std::vector<std::string> availableAssets;
    std::map<std::string, double> priceData;
    std::map<std::string, double> sentimentScores;
    AllocationConstraints constraints;
    std::chrono::system_clock::time_point timestamp;
};

// Advanced optimization strategies
class AdvancedOptimizationStrategies {
public:
    // Black-Litterman model integration
    static OptimizationResult blackLittermanOptimization(
        const OptimizationContext& context,
        const std::vector<std::pair<std::string, double>>& views // Investor views
    );

    // Dynamic risk budgeting
    static OptimizationResult dynamicRiskBudgeting(
        const OptimizationContext& context,
        const std::map<std::string, double>& riskBudgets
    );

    // Regime-aware allocation
    static OptimizationResult regimeAwareAllocation(
        const OptimizationContext& context,
        const std::string& currentRegime // "bull", "bear", "sideways"
    );

    // Factor-based portfolio construction
    static OptimizationResult factorBasedAllocation(
        const OptimizationContext& context,
        const std::vector<std::string>& factors // "momentum", "value", "quality"
    );
};

} // namespace Optimization
} // namespace CryptoClaude