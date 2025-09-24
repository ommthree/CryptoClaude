#include "PortfolioOptimizer.h"
#include <random>
#include <iostream>
#include <iomanip>

namespace CryptoClaude {
namespace Optimization {

// === MODERN PORTFOLIO THEORY OPTIMIZATION ===

OptimizationResult PortfolioOptimizer::optimizePortfolio_MPT(
    const Portfolio& portfolio,
    const std::vector<Position>& currentPositions,
    const std::vector<std::string>& availableAssets,
    double targetReturn,
    bool minimizeRisk) {

    OptimizationResult result;
    result.optimizationMethod = "Modern Portfolio Theory";
    result.timestamp = std::chrono::system_clock::now();

    if (!validateInputData(availableAssets)) {
        logOptimizationWarning("Invalid input data for MPT optimization", result);
        return result;
    }

    try {
        // Calculate current allocation
        auto currentAllocation = getCurrentAllocation(portfolio, currentPositions);

        // Solve MPT optimization
        auto optimalWeights = solveMPTOptimization(availableAssets, targetReturn, minimizeRisk);

        // Build allocation results
        for (size_t i = 0; i < availableAssets.size(); ++i) {
            AllocationResult allocation;
            allocation.symbol = availableAssets[i];
            allocation.targetWeight = optimalWeights[i];

            // Find current weight
            allocation.currentWeight = 0.0;
            for (const auto& current : currentAllocation) {
                if (current.symbol == availableAssets[i]) {
                    allocation.currentWeight = current.currentWeight;
                    break;
                }
            }

            allocation.rebalanceAmount = (allocation.targetWeight - allocation.currentWeight)
                                       * portfolio.getTotalValue();

            allocation.expectedReturn = expectedReturns_[availableAssets[i]];
            allocation.riskContribution = calculateRiskContribution(availableAssets[i], {allocation});
            allocation.rationale = "MPT optimal allocation based on risk-return optimization";

            if (std::abs(allocation.targetWeight) > 0.001) { // Only include meaningful allocations
                result.allocations.push_back(allocation);
            }
        }

        // Calculate portfolio metrics
        result.expectedPortfolioReturn = calculateExpectedReturn(result.allocations);
        result.expectedPortfolioRisk = calculatePortfolioRisk(result.allocations);
        result.sharpeRatio = calculateSharpeRatio(result.allocations);
        result.diversificationRatio = calculateDiversificationRatio(result.allocations);

        // Apply constraints
        result = applyConstraints(result, portfolio);

        // Check if rebalancing is needed
        result.requiresRebalancing = shouldRebalance(portfolio, currentPositions, result);

        if (result.requiresRebalancing) {
            // Calculate total rebalancing cost
            result.totalRebalanceCost = 0.0;
            for (const auto& allocation : result.allocations) {
                result.totalRebalanceCost += calculateTransactionCost(
                    std::abs(allocation.rebalanceAmount), allocation.symbol);
            }
        }

    } catch (const std::exception& e) {
        logOptimizationWarning("MPT optimization failed: " + std::string(e.what()), result);
    }

    return result;
}

// === RISK PARITY OPTIMIZATION ===

OptimizationResult PortfolioOptimizer::optimizePortfolio_RiskParity(
    const Portfolio& portfolio,
    const std::vector<Position>& currentPositions,
    const std::vector<std::string>& availableAssets) {

    OptimizationResult result;
    result.optimizationMethod = "Risk Parity";
    result.timestamp = std::chrono::system_clock::now();

    if (!validateInputData(availableAssets)) {
        logOptimizationWarning("Invalid input data for Risk Parity optimization", result);
        return result;
    }

    try {
        // Calculate risk parity weights
        auto riskParityWeights = calculateRiskParityWeights(availableAssets);
        auto currentAllocation = getCurrentAllocation(portfolio, currentPositions);

        // Build allocation results
        for (size_t i = 0; i < availableAssets.size(); ++i) {
            AllocationResult allocation;
            allocation.symbol = availableAssets[i];
            allocation.targetWeight = riskParityWeights[i];

            // Find current weight
            allocation.currentWeight = 0.0;
            for (const auto& current : currentAllocation) {
                if (current.symbol == availableAssets[i]) {
                    allocation.currentWeight = current.currentWeight;
                    break;
                }
            }

            allocation.rebalanceAmount = (allocation.targetWeight - allocation.currentWeight)
                                       * portfolio.getTotalValue();

            allocation.expectedReturn = expectedReturns_[availableAssets[i]];
            allocation.riskContribution = 1.0 / availableAssets.size(); // Equal risk contribution
            allocation.rationale = "Risk Parity allocation for equal risk contribution";

            if (allocation.targetWeight > 0.001) {
                result.allocations.push_back(allocation);
            }
        }

        // Calculate portfolio metrics
        result.expectedPortfolioReturn = calculateExpectedReturn(result.allocations);
        result.expectedPortfolioRisk = calculatePortfolioRisk(result.allocations);
        result.sharpeRatio = calculateSharpeRatio(result.allocations);
        result.diversificationRatio = calculateDiversificationRatio(result.allocations);

        // Apply constraints and check rebalancing
        result = applyConstraints(result, portfolio);
        result.requiresRebalancing = shouldRebalance(portfolio, currentPositions, result);

        if (result.requiresRebalancing) {
            result.totalRebalanceCost = 0.0;
            for (const auto& allocation : result.allocations) {
                result.totalRebalanceCost += calculateTransactionCost(
                    std::abs(allocation.rebalanceAmount), allocation.symbol);
            }
        }

    } catch (const std::exception& e) {
        logOptimizationWarning("Risk Parity optimization failed: " + std::string(e.what()), result);
    }

    return result;
}

// === EQUAL WEIGHT WITH RISK ADJUSTMENT ===

OptimizationResult PortfolioOptimizer::optimizePortfolio_EqualWeight(
    const Portfolio& portfolio,
    const std::vector<Position>& currentPositions,
    const std::vector<std::string>& availableAssets,
    bool riskAdjusted) {

    OptimizationResult result;
    result.optimizationMethod = riskAdjusted ? "Equal Weight (Risk Adjusted)" : "Equal Weight";
    result.timestamp = std::chrono::system_clock::now();

    if (availableAssets.empty()) {
        logOptimizationWarning("No available assets for allocation", result);
        return result;
    }

    try {
        auto currentAllocation = getCurrentAllocation(portfolio, currentPositions);

        // Calculate base equal weights
        double baseWeight = 1.0 / availableAssets.size();
        std::vector<double> weights(availableAssets.size(), baseWeight);

        // Apply risk adjustment if requested
        if (riskAdjusted) {
            auto volatilityScaling = calculateVolatilityScaling(availableAssets, true);

            // Use volatility scaling directly (already normalized in method)
            weights = volatilityScaling;
        }

        // Build allocation results
        for (size_t i = 0; i < availableAssets.size(); ++i) {
            AllocationResult allocation;
            allocation.symbol = availableAssets[i];
            allocation.targetWeight = weights[i];

            // Find current weight
            allocation.currentWeight = 0.0;
            for (const auto& current : currentAllocation) {
                if (current.symbol == availableAssets[i]) {
                    allocation.currentWeight = current.currentWeight;
                    break;
                }
            }

            allocation.rebalanceAmount = (allocation.targetWeight - allocation.currentWeight)
                                       * portfolio.getTotalValue();

            allocation.expectedReturn = expectedReturns_[availableAssets[i]];
            allocation.riskContribution = calculateRiskContribution(availableAssets[i], {allocation});
            allocation.rationale = riskAdjusted ?
                "Equal weight allocation adjusted for volatility differences" :
                "Simple equal weight allocation across all assets";

            result.allocations.push_back(allocation);
        }

        // Calculate portfolio metrics
        result.expectedPortfolioReturn = calculateExpectedReturn(result.allocations);
        result.expectedPortfolioRisk = calculatePortfolioRisk(result.allocations);
        result.sharpeRatio = calculateSharpeRatio(result.allocations);
        result.diversificationRatio = calculateDiversificationRatio(result.allocations);

        // Apply constraints and check rebalancing
        result = applyConstraints(result, portfolio);
        result.requiresRebalancing = shouldRebalance(portfolio, currentPositions, result);

    } catch (const std::exception& e) {
        logOptimizationWarning("Equal Weight optimization failed: " + std::string(e.what()), result);
    }

    return result;
}

// === VOLATILITY-WEIGHTED ALLOCATION ===

OptimizationResult PortfolioOptimizer::optimizePortfolio_VolatilityWeighted(
    const Portfolio& portfolio,
    const std::vector<Position>& currentPositions,
    const std::vector<std::string>& availableAssets,
    bool inverseVolatility) {

    OptimizationResult result;
    result.optimizationMethod = inverseVolatility ?
        "Inverse Volatility Weighted" : "Volatility Weighted";
    result.timestamp = std::chrono::system_clock::now();

    if (!validateInputData(availableAssets)) {
        logOptimizationWarning("Invalid input data for volatility weighting", result);
        return result;
    }

    try {
        auto currentAllocation = getCurrentAllocation(portfolio, currentPositions);
        auto volatilityWeights = calculateVolatilityScaling(availableAssets, inverseVolatility);

        // Build allocation results
        for (size_t i = 0; i < availableAssets.size(); ++i) {
            AllocationResult allocation;
            allocation.symbol = availableAssets[i];
            allocation.targetWeight = volatilityWeights[i];

            // Find current weight
            allocation.currentWeight = 0.0;
            for (const auto& current : currentAllocation) {
                if (current.symbol == availableAssets[i]) {
                    allocation.currentWeight = current.currentWeight;
                    break;
                }
            }

            allocation.rebalanceAmount = (allocation.targetWeight - allocation.currentWeight)
                                       * portfolio.getTotalValue();

            allocation.expectedReturn = expectedReturns_[availableAssets[i]];
            allocation.riskContribution = calculateRiskContribution(availableAssets[i], {allocation});

            allocation.rationale = inverseVolatility ?
                "Allocation inversely proportional to volatility for risk balancing" :
                "Allocation proportional to volatility for momentum capture";

            if (allocation.targetWeight > 0.001) {
                result.allocations.push_back(allocation);
            }
        }

        // Calculate portfolio metrics
        result.expectedPortfolioReturn = calculateExpectedReturn(result.allocations);
        result.expectedPortfolioRisk = calculatePortfolioRisk(result.allocations);
        result.sharpeRatio = calculateSharpeRatio(result.allocations);
        result.diversificationRatio = calculateDiversificationRatio(result.allocations);

        // Apply constraints and check rebalancing
        result = applyConstraints(result, portfolio);
        result.requiresRebalancing = shouldRebalance(portfolio, currentPositions, result);

    } catch (const std::exception& e) {
        logOptimizationWarning("Volatility weighted optimization failed: " + std::string(e.what()), result);
    }

    return result;
}

// === SENTIMENT-WEIGHTED ALLOCATION ===

OptimizationResult PortfolioOptimizer::optimizePortfolio_SentimentWeighted(
    const Portfolio& portfolio,
    const std::vector<Position>& currentPositions,
    const std::vector<std::string>& availableAssets,
    double sentimentWeight) {

    OptimizationResult result;
    result.optimizationMethod = "Sentiment Weighted";
    result.timestamp = std::chrono::system_clock::now();

    if (!validateInputData(availableAssets)) {
        logOptimizationWarning("Invalid input data for sentiment weighting", result);
        return result;
    }

    try {
        // Start with equal weight base allocation
        std::vector<double> baseWeights(availableAssets.size(), 1.0 / availableAssets.size());

        // Apply sentiment adjustment
        auto sentimentAdjustedWeights = integrateSentimentScores(
            availableAssets, baseWeights, sentimentWeight);

        auto currentAllocation = getCurrentAllocation(portfolio, currentPositions);

        // Build allocation results
        for (size_t i = 0; i < availableAssets.size(); ++i) {
            AllocationResult allocation;
            allocation.symbol = availableAssets[i];
            allocation.targetWeight = sentimentAdjustedWeights[i];

            // Find current weight
            allocation.currentWeight = 0.0;
            for (const auto& current : currentAllocation) {
                if (current.symbol == availableAssets[i]) {
                    allocation.currentWeight = current.currentWeight;
                    break;
                }
            }

            allocation.rebalanceAmount = (allocation.targetWeight - allocation.currentWeight)
                                       * portfolio.getTotalValue();

            allocation.expectedReturn = expectedReturns_[availableAssets[i]];
            allocation.riskContribution = calculateRiskContribution(availableAssets[i], {allocation});

            // Get sentiment score for rationale
            double sentimentScore = 0.0;
            if (sentimentData_.count(availableAssets[i])) {
                sentimentScore = sentimentData_[availableAssets[i]].getAvgSentiment();
            }

            allocation.rationale = "Allocation adjusted by sentiment score: " +
                                 std::to_string(sentimentScore) + " (weight: " +
                                 std::to_string(sentimentWeight) + ")";

            if (allocation.targetWeight > 0.001) {
                result.allocations.push_back(allocation);
            }
        }

        // Calculate portfolio metrics
        result.expectedPortfolioReturn = calculateExpectedReturn(result.allocations);
        result.expectedPortfolioRisk = calculatePortfolioRisk(result.allocations);
        result.sharpeRatio = calculateSharpeRatio(result.allocations);
        result.diversificationRatio = calculateDiversificationRatio(result.allocations);

        // Apply constraints and check rebalancing
        result = applyConstraints(result, portfolio);
        result.requiresRebalancing = shouldRebalance(portfolio, currentPositions, result);

    } catch (const std::exception& e) {
        logOptimizationWarning("Sentiment weighted optimization failed: " + std::string(e.what()), result);
    }

    return result;
}

// === MAXIMUM DIVERSIFICATION OPTIMIZATION ===

OptimizationResult PortfolioOptimizer::optimizePortfolio_MaxDiversification(
    const Portfolio& portfolio,
    const std::vector<Position>& currentPositions,
    const std::vector<std::string>& availableAssets) {

    OptimizationResult result;
    result.optimizationMethod = "Maximum Diversification";
    result.timestamp = std::chrono::system_clock::now();

    if (!validateInputData(availableAssets)) {
        logOptimizationWarning("Invalid input data for maximum diversification", result);
        return result;
    }

    try {
        auto maxDiversificationWeights = maximizeDiversificationRatio(availableAssets);
        auto currentAllocation = getCurrentAllocation(portfolio, currentPositions);

        // Build allocation results
        for (size_t i = 0; i < availableAssets.size(); ++i) {
            AllocationResult allocation;
            allocation.symbol = availableAssets[i];
            allocation.targetWeight = maxDiversificationWeights[i];

            // Find current weight
            allocation.currentWeight = 0.0;
            for (const auto& current : currentAllocation) {
                if (current.symbol == availableAssets[i]) {
                    allocation.currentWeight = current.currentWeight;
                    break;
                }
            }

            allocation.rebalanceAmount = (allocation.targetWeight - allocation.currentWeight)
                                       * portfolio.getTotalValue();

            allocation.expectedReturn = expectedReturns_[availableAssets[i]];
            allocation.riskContribution = calculateRiskContribution(availableAssets[i], {allocation});
            allocation.rationale = "Maximum diversification allocation to optimize diversification ratio";

            if (allocation.targetWeight > 0.001) {
                result.allocations.push_back(allocation);
            }
        }

        // Calculate portfolio metrics
        result.expectedPortfolioReturn = calculateExpectedReturn(result.allocations);
        result.expectedPortfolioRisk = calculatePortfolioRisk(result.allocations);
        result.sharpeRatio = calculateSharpeRatio(result.allocations);
        result.diversificationRatio = calculateDiversificationRatio(result.allocations);

        // Apply constraints and check rebalancing
        result = applyConstraints(result, portfolio);
        result.requiresRebalancing = shouldRebalance(portfolio, currentPositions, result);

    } catch (const std::exception& e) {
        logOptimizationWarning("Maximum diversification optimization failed: " + std::string(e.what()), result);
    }

    return result;
}

// === REBALANCING LOGIC IMPLEMENTATION ===

bool PortfolioOptimizer::shouldRebalance(
    const Portfolio& portfolio,
    const std::vector<Position>& currentPositions,
    const OptimizationResult& targetAllocation) {

    // Check threshold-based triggers
    if (checkThresholdTriggers(currentPositions, targetAllocation)) {
        return true;
    }

    // Check risk-based triggers
    if (checkRiskTriggers(portfolio, currentPositions)) {
        return true;
    }

    // Check if benefits outweigh costs
    double potentialBenefit = 0.0;
    double rebalancingCost = 0.0;

    for (const auto& allocation : targetAllocation.allocations) {
        if (std::abs(allocation.rebalanceAmount) > 0) {
            potentialBenefit += std::abs(allocation.rebalanceAmount) * 0.001; // Estimated benefit
            rebalancingCost += calculateTransactionCost(
                std::abs(allocation.rebalanceAmount), allocation.symbol);
        }
    }

    return potentialBenefit > rebalancingCost * 2.0; // Benefit must be 2x cost
}

OptimizationResult PortfolioOptimizer::generateRebalancingPlan(
    const Portfolio& portfolio,
    const std::vector<Position>& currentPositions,
    const OptimizationResult& targetAllocation,
    bool minimizeCosts) {

    OptimizationResult rebalancingPlan = targetAllocation;
    rebalancingPlan.optimizationMethod += " (Rebalancing Plan)";

    if (minimizeCosts) {
        rebalancingPlan = optimizeRebalancingCosts(targetAllocation, currentPositions);
    }

    // Validate the rebalancing plan
    auto validationErrors = validateRebalancingPlan(portfolio, rebalancingPlan);
    for (const auto& error : validationErrors) {
        rebalancingPlan.warnings.push_back(error);
    }

    return rebalancingPlan;
}

std::vector<std::string> PortfolioOptimizer::validateRebalancingPlan(
    const Portfolio& portfolio,
    const OptimizationResult& rebalancingPlan) {

    std::vector<std::string> warnings;

    // Check leverage constraints
    double totalLeverage = 0.0;
    for (const auto& allocation : rebalancingPlan.allocations) {
        totalLeverage += std::abs(allocation.targetWeight);
    }

    if (totalLeverage > constraints_.maxLeverage) {
        warnings.push_back("Rebalancing plan exceeds maximum leverage constraint");
    }

    // Check margin requirements
    double totalMarginRequired = 0.0;
    for (const auto& allocation : rebalancingPlan.allocations) {
        if (allocation.targetWeight > 0) {
            totalMarginRequired += allocation.targetWeight * portfolio.getTotalValue() / 2.0; // Simplified margin calc
        }
    }

    if (totalMarginRequired > portfolio.getAvailableMargin()) {
        warnings.push_back("Rebalancing plan requires more margin than available");
    }

    // Check position size constraints
    for (const auto& allocation : rebalancingPlan.allocations) {
        if (allocation.targetWeight > constraints_.maxPositionWeight) {
            warnings.push_back("Position " + allocation.symbol + " exceeds maximum weight constraint");
        }
        if (allocation.targetWeight > 0 && allocation.targetWeight < constraints_.minPositionWeight) {
            warnings.push_back("Position " + allocation.symbol + " below minimum weight constraint");
        }
    }

    return warnings;
}

// === CONSTRAINT APPLICATION ===

OptimizationResult PortfolioOptimizer::applyConstraints(
    const OptimizationResult& unconstrained,
    const Portfolio& portfolio) {

    OptimizationResult constrained = unconstrained;

    // Apply position weight constraints
    for (auto& allocation : constrained.allocations) {
        // Apply maximum position weight
        if (allocation.targetWeight > constraints_.maxPositionWeight) {
            allocation.targetWeight = constraints_.maxPositionWeight;
            constrained.warnings.push_back(
                "Capped " + allocation.symbol + " weight to maximum constraint");
        }

        // Apply minimum position weight (or zero out small positions)
        if (allocation.targetWeight > 0 && allocation.targetWeight < constraints_.minPositionWeight) {
            if (allocation.targetWeight < constraints_.minPositionWeight / 2.0) {
                allocation.targetWeight = 0.0; // Zero out very small positions
                constrained.warnings.push_back(
                    "Zeroed out " + allocation.symbol + " due to minimum weight constraint");
            } else {
                allocation.targetWeight = constraints_.minPositionWeight;
                constrained.warnings.push_back(
                    "Increased " + allocation.symbol + " weight to minimum constraint");
            }
        }

        // Apply symbol-specific constraints
        if (constraints_.symbolMaxWeights.count(allocation.symbol)) {
            double symbolMax = constraints_.symbolMaxWeights.at(allocation.symbol);
            if (allocation.targetWeight > symbolMax) {
                allocation.targetWeight = symbolMax;
                constrained.warnings.push_back(
                    "Applied symbol-specific weight constraint for " + allocation.symbol);
            }
        }
    }

    // Normalize weights to sum to 1.0 (accounting for cash buffer)
    double totalWeight = 0.0;
    for (const auto& allocation : constrained.allocations) {
        totalWeight += allocation.targetWeight;
    }

    double targetTotalWeight = 1.0 - constraints_.minCashBuffer;
    if (totalWeight > 0.0 && totalWeight > targetTotalWeight) {
        double scaleFactor = targetTotalWeight / totalWeight;
        for (auto& allocation : constrained.allocations) {
            allocation.targetWeight *= scaleFactor;
        }
        constrained.warnings.push_back("Scaled down allocations to maintain cash buffer");
    } else if (totalWeight > 0.0 && totalWeight < targetTotalWeight) {
        // Scale up if total is too low
        double scaleFactor = targetTotalWeight / totalWeight;
        for (auto& allocation : constrained.allocations) {
            allocation.targetWeight *= scaleFactor;
        }
        constrained.warnings.push_back("Scaled up allocations to target weight");
    }

    // Recalculate rebalance amounts after constraint application
    for (auto& allocation : constrained.allocations) {
        allocation.rebalanceAmount = (allocation.targetWeight - allocation.currentWeight)
                                   * portfolio.getTotalValue();
    }

    // Recalculate portfolio metrics after constraints
    constrained.expectedPortfolioReturn = calculateExpectedReturn(constrained.allocations);
    constrained.expectedPortfolioRisk = calculatePortfolioRisk(constrained.allocations);
    constrained.sharpeRatio = calculateSharpeRatio(constrained.allocations);
    constrained.diversificationRatio = calculateDiversificationRatio(constrained.allocations);

    return constrained;
}

// === UTILITY IMPLEMENTATIONS ===

std::vector<AllocationResult> PortfolioOptimizer::getCurrentAllocation(
    const Portfolio& portfolio,
    const std::vector<Position>& positions) {

    std::vector<AllocationResult> currentAllocation;
    double totalValue = portfolio.getTotalValue();

    if (totalValue <= 0) {
        return currentAllocation;
    }

    for (const auto& position : positions) {
        AllocationResult current;
        current.symbol = position.getSymbol();
        current.currentWeight = position.getPositionValue() / totalValue;
        current.targetWeight = current.currentWeight; // Initialize with current
        current.rebalanceAmount = 0.0;
        current.expectedReturn = expectedReturns_.count(position.getSymbol()) ?
            expectedReturns_[position.getSymbol()] : 0.0;
        current.riskContribution = 0.0; // Will be calculated later
        current.rationale = "Current allocation";

        currentAllocation.push_back(current);
    }

    return currentAllocation;
}

double PortfolioOptimizer::calculateTransactionCost(double rebalanceAmount, const std::string& symbol) {
    return std::abs(rebalanceAmount) * constraints_.transactionCostRate;
}

bool PortfolioOptimizer::validateInputData(const std::vector<std::string>& assets) {
    if (assets.empty()) {
        return false;
    }

    // Check if we have expected returns data
    for (const auto& asset : assets) {
        if (expectedReturns_.find(asset) == expectedReturns_.end()) {
            expectedReturns_[asset] = 0.08; // Default 8% expected return
        }
        if (volatilities_.find(asset) == volatilities_.end()) {
            volatilities_[asset] = 0.25; // Default 25% volatility
        }
    }

    return true;
}

void PortfolioOptimizer::logOptimizationWarning(const std::string& warning, OptimizationResult& result) {
    result.warnings.push_back(warning);
    std::cerr << "Portfolio Optimization Warning: " << warning << std::endl;
}

// === PLACEHOLDER IMPLEMENTATIONS FOR COMPLEX ALGORITHMS ===

std::vector<double> PortfolioOptimizer::solveMPTOptimization(
    const std::vector<std::string>& assets,
    double targetReturn,
    bool minimizeRisk) {

    // Simplified MPT implementation
    // In practice, this would use quadratic programming
    std::vector<double> weights(assets.size());

    if (minimizeRisk) {
        // Inverse volatility weighting as simplified risk minimization
        double totalInverseVol = 0.0;
        for (const auto& asset : assets) {
            totalInverseVol += 1.0 / volatilities_[asset];
        }

        for (size_t i = 0; i < assets.size(); ++i) {
            weights[i] = (1.0 / volatilities_[assets[i]]) / totalInverseVol;
        }
    } else {
        // Equal weight as simplified return maximization
        double equalWeight = 1.0 / assets.size();
        std::fill(weights.begin(), weights.end(), equalWeight);
    }

    return weights;
}

std::vector<double> PortfolioOptimizer::calculateRiskParityWeights(const std::vector<std::string>& assets) {
    // Simplified risk parity - inverse volatility weighting
    std::vector<double> weights(assets.size());
    double totalInverseVol = 0.0;

    for (const auto& asset : assets) {
        totalInverseVol += 1.0 / std::sqrt(volatilities_[asset]);
    }

    for (size_t i = 0; i < assets.size(); ++i) {
        weights[i] = (1.0 / std::sqrt(volatilities_[assets[i]])) / totalInverseVol;
    }

    return weights;
}

std::vector<double> PortfolioOptimizer::calculateVolatilityScaling(
    const std::vector<std::string>& assets,
    bool inverse) {

    std::vector<double> weights(assets.size());
    double totalWeight = 0.0;

    for (size_t i = 0; i < assets.size(); ++i) {
        double vol = volatilities_.count(assets[i]) ? volatilities_[assets[i]] : 0.25;
        if (vol <= 0.0) vol = 0.25; // Ensure positive volatility

        weights[i] = inverse ? (1.0 / vol) : vol;
        totalWeight += weights[i];
    }

    // Normalize only if totalWeight is positive
    if (totalWeight > 0.0) {
        for (auto& weight : weights) {
            weight /= totalWeight;
        }
    } else {
        // Fallback to equal weights
        double equalWeight = 1.0 / assets.size();
        std::fill(weights.begin(), weights.end(), equalWeight);
    }

    return weights;
}

std::vector<double> PortfolioOptimizer::integrateSentimentScores(
    const std::vector<std::string>& assets,
    const std::vector<double>& baseWeights,
    double sentimentWeight) {

    std::vector<double> adjustedWeights = baseWeights;

    for (size_t i = 0; i < assets.size(); ++i) {
        if (sentimentData_.count(assets[i])) {
            double sentimentScore = sentimentData_[assets[i]].getAvgSentiment();
            double sentimentAdjustment = 1.0 + (sentimentScore * sentimentWeight);
            adjustedWeights[i] *= sentimentAdjustment;
        }
    }

    // Normalize
    double totalWeight = std::accumulate(adjustedWeights.begin(), adjustedWeights.end(), 0.0);
    for (auto& weight : adjustedWeights) {
        weight /= totalWeight;
    }

    return adjustedWeights;
}

std::vector<double> PortfolioOptimizer::maximizeDiversificationRatio(const std::vector<std::string>& assets) {
    // Simplified maximum diversification - equal weight adjusted by correlation
    std::vector<double> weights(assets.size(), 1.0 / assets.size());

    // In practice, this would solve an optimization problem to maximize:
    // (sum of weighted volatilities) / (portfolio volatility)

    return weights;
}

// === HELPER CALCULATIONS ===

double PortfolioOptimizer::calculatePortfolioRisk(const std::vector<AllocationResult>& allocations) {
    double portfolioVariance = 0.0;

    // Simplified portfolio risk calculation
    for (const auto& allocation : allocations) {
        if (volatilities_.count(allocation.symbol)) {
            double assetVol = volatilities_[allocation.symbol];
            portfolioVariance += allocation.targetWeight * allocation.targetWeight * assetVol * assetVol;
        }
    }

    // Add correlation effects (simplified)
    // In practice, this would use the full covariance matrix

    return std::sqrt(portfolioVariance);
}

double PortfolioOptimizer::calculateExpectedReturn(const std::vector<AllocationResult>& allocations) {
    double expectedReturn = 0.0;

    for (const auto& allocation : allocations) {
        expectedReturn += allocation.targetWeight * allocation.expectedReturn;
    }

    return expectedReturn;
}

double PortfolioOptimizer::calculateSharpeRatio(const std::vector<AllocationResult>& allocations) {
    double expectedReturn = calculateExpectedReturn(allocations);
    double portfolioRisk = calculatePortfolioRisk(allocations);

    if (portfolioRisk <= 0) {
        return 0.0;
    }

    double riskFreeRate = 0.02; // Assume 2% risk-free rate
    return (expectedReturn - riskFreeRate) / portfolioRisk;
}

double PortfolioOptimizer::calculateRiskContribution(
    const std::string& asset,
    const std::vector<AllocationResult>& allocation) {

    // Simplified risk contribution calculation
    for (const auto& alloc : allocation) {
        if (alloc.symbol == asset) {
            return alloc.targetWeight; // Simplified - proportional to weight
        }
    }

    return 0.0;
}

double PortfolioOptimizer::calculateDiversificationRatio(const std::vector<AllocationResult>& allocation) {
    // Simplified diversification ratio
    double weightedAvgVol = 0.0;
    double portfolioVol = calculatePortfolioRisk(allocation);

    for (const auto& alloc : allocation) {
        if (volatilities_.count(alloc.symbol)) {
            weightedAvgVol += alloc.targetWeight * volatilities_[alloc.symbol];
        }
    }

    if (portfolioVol <= 0) {
        return 1.0;
    }

    return weightedAvgVol / portfolioVol;
}

// === TRIGGER CHECKING ===

bool PortfolioOptimizer::checkThresholdTriggers(
    const std::vector<Position>& current,
    const OptimizationResult& target) {

    // Check if any position deviates more than threshold from target
    for (const auto& targetAllocation : target.allocations) {
        double currentWeight = 0.0;

        // Find current weight for this symbol
        for (const auto& position : current) {
            if (position.getSymbol() == targetAllocation.symbol) {
                // Calculate current weight (simplified)
                currentWeight = 0.1; // Placeholder
                break;
            }
        }

        double deviation = std::abs(targetAllocation.targetWeight - currentWeight);
        if (deviation > 0.05) { // 5% threshold
            return true;
        }
    }

    return false;
}

bool PortfolioOptimizer::checkRiskTriggers(const Portfolio& portfolio, const std::vector<Position>& positions) {
    // Check if portfolio risk exceeds thresholds
    double portfolioVol = portfolio.getPortfolioVolatility();
    if (portfolioVol > 0.30) { // 30% volatility threshold
        return true;
    }

    // Check margin utilization
    if (portfolio.getMarginUtilization() > 0.80) { // 80% margin utilization
        return true;
    }

    return false;
}

OptimizationResult PortfolioOptimizer::optimizeRebalancingCosts(
    const OptimizationResult& idealAllocation,
    const std::vector<Position>& currentPositions) {

    // Simplified cost optimization - reduce smaller rebalances
    OptimizationResult costOptimized = idealAllocation;

    for (auto& allocation : costOptimized.allocations) {
        double transactionCost = calculateTransactionCost(allocation.rebalanceAmount, allocation.symbol);
        double potentialBenefit = std::abs(allocation.rebalanceAmount) * 0.001; // Estimated benefit

        // Skip rebalancing if cost exceeds benefit
        if (transactionCost > potentialBenefit) {
            allocation.rebalanceAmount = 0.0;
            allocation.targetWeight = allocation.currentWeight;
        }
    }

    return costOptimized;
}

// === DATA MANAGEMENT IMPLEMENTATIONS ===

void PortfolioOptimizer::updateMarketData(const std::string& symbol, const MarketData& data) {
    marketData_[symbol] = data;

    // Update expected returns and volatilities based on market data
    // For now, use simplified estimates
    expectedReturns_[symbol] = 0.08; // Default 8% expected return
    volatilities_[symbol] = 0.25;    // Default 25% volatility

    // In practice, these would be calculated from historical price data
    // and technical indicators in the MarketData
}

void PortfolioOptimizer::updateSentimentData(const std::string& symbol, const SentimentData& data) {
    sentimentData_[symbol] = data;
}

void PortfolioOptimizer::estimateReturnsAndRisk(const std::vector<std::string>& symbols, int lookbackDays) {
    // Simplified implementation
    for (const auto& symbol : symbols) {
        if (expectedReturns_.find(symbol) == expectedReturns_.end()) {
            expectedReturns_[symbol] = 0.08; // Default 8% expected return
        }
        if (volatilities_.find(symbol) == volatilities_.end()) {
            volatilities_[symbol] = 0.25; // Default 25% volatility
        }
    }
}

void PortfolioOptimizer::updateCorrelationMatrix(const std::vector<std::string>& symbols) {
    // Simplified correlation matrix - assume moderate positive correlation
    for (size_t i = 0; i < symbols.size(); ++i) {
        for (size_t j = 0; j < symbols.size(); ++j) {
            if (i == j) {
                correlations_[{symbols[i], symbols[j]}] = 1.0; // Self-correlation
            } else {
                correlations_[{symbols[i], symbols[j]}] = 0.6; // Default moderate correlation
            }
        }
    }
}

void PortfolioOptimizer::addRebalancingTrigger(const RebalancingTrigger& trigger) {
    triggers_.push_back(trigger);
}

void PortfolioOptimizer::updateTriggerThresholds(RebalancingTrigger::TriggerType type, double newThreshold) {
    for (auto& trigger : triggers_) {
        if (trigger.type == type) {
            trigger.threshold = newThreshold;
        }
    }
}

} // namespace Optimization
} // namespace CryptoClaude