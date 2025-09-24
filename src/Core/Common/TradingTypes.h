#pragma once

#include <string>
#include <chrono>
#include <vector>
#include <map>

namespace CryptoClaude {
namespace Common {

// Unified prediction interface for all components
struct PredictionData {
    std::string symbol;
    double predictedReturn;              // Expected return over prediction horizon
    double confidence;                   // Model confidence (0-1)
    double regressionR2 = 0.0;          // Historical model R² for this asset
    std::string predictionModel = "Unknown"; // Model name
    std::chrono::system_clock::time_point predictionTimestamp;

    // Risk metrics
    double volatilityForecast = 0.3;    // Default 30% annual volatility
    double maxDrawdownRisk = 0.15;      // Default 15% drawdown risk
    double correlationToMarket = 0.0;   // Not used in pairing strategy

    // Validation
    bool isValid() const {
        return !symbol.empty() &&
               confidence >= 0.0 && confidence <= 1.0 &&
               !std::isnan(predictedReturn) && !std::isinf(predictedReturn);
    }
};

// Unified trading pair structure
struct TradingPair {
    std::string longSymbol;      // Highest expected return
    std::string shortSymbol;     // Lowest expected return
    double longExpectedReturn = 0.0;
    double shortExpectedReturn = 0.0;
    double pairConfidence = 0.0; // Combined confidence
    double allocationWeight = 0.0; // How much to allocate to this pair

    // Weights for individual positions
    double longWeight = 0.0;
    double shortWeight = 0.0;

    double calculatePairReturn() const {
        return longExpectedReturn - shortExpectedReturn; // Market-neutral return
    }

    bool isValid() const {
        return !longSymbol.empty() && !shortSymbol.empty() &&
               longSymbol != shortSymbol &&
               pairConfidence >= 0.0 && pairConfidence <= 1.0 &&
               allocationWeight >= 0.0;
    }
};

// Strategy parameters - unified structure
struct StrategyParameters {
    // Core pairing parameters
    int maxPairsToCreate = 20;
    double totalInvestmentRatio = 0.9;  // 90% invested, 10% cash
    double minConfidenceThreshold = 0.3;

    // Allocation parameters
    bool sortByExpectedReturn = true;
    bool pairTopWithBottom = true;
    bool dynamicAllocation = true;

    // Rebalancing
    std::chrono::minutes rebalanceInterval{1440};  // Daily
    double minRebalanceThreshold = 0.02;           // 2% deviation triggers rebalance

    // Transaction costs
    double estimatedTradingFeeBps = 10.0;          // 10bps fees
    double minExpectedBenefitBps = 20.0;           // Min 20bps benefit to trade

    // Risk limits
    double cashBufferPercentage = 0.10;            // Fixed 10% cash buffer
    double maxSinglePairAllocation = 0.15;        // Max 15% per pair
    double portfolioDrawdownStop = 0.15;          // 15% stop loss

    bool validateConfiguration() const {
        return maxPairsToCreate > 0 && maxPairsToCreate <= 50 &&
               totalInvestmentRatio > 0.5 && totalInvestmentRatio <= 0.95 &&
               minConfidenceThreshold >= 0.0 && minConfidenceThreshold <= 1.0 &&
               cashBufferPercentage >= 0.05 && cashBufferPercentage <= 0.3 &&
               maxSinglePairAllocation > 0.0 && maxSinglePairAllocation <= 0.5 &&
               estimatedTradingFeeBps >= 0.0 && estimatedTradingFeeBps <= 100.0 &&
               minExpectedBenefitBps >= 0.0 && minExpectedBenefitBps <= 1000.0 &&
               portfolioDrawdownStop > 0.0 && portfolioDrawdownStop <= 0.5 &&
               rebalanceInterval.count() > 0 &&
               std::abs((totalInvestmentRatio + cashBufferPercentage) - 1.0) <= 0.05; // Allow small rounding
    }

    std::vector<std::string> getConfigurationErrors() const {
        std::vector<std::string> errors;

        if (maxPairsToCreate <= 0 || maxPairsToCreate > 50)
            errors.push_back("maxPairsToCreate must be between 1 and 50");
        if (totalInvestmentRatio <= 0.5 || totalInvestmentRatio > 0.95)
            errors.push_back("totalInvestmentRatio must be between 0.5 and 0.95");
        if (minConfidenceThreshold < 0.0 || minConfidenceThreshold > 1.0)
            errors.push_back("minConfidenceThreshold must be between 0.0 and 1.0");
        if (cashBufferPercentage < 0.05 || cashBufferPercentage > 0.3)
            errors.push_back("cashBufferPercentage must be between 0.05 and 0.3");
        if (maxSinglePairAllocation <= 0.0 || maxSinglePairAllocation > 0.5)
            errors.push_back("maxSinglePairAllocation must be between 0.0 and 0.5");
        if (estimatedTradingFeeBps < 0.0 || estimatedTradingFeeBps > 100.0)
            errors.push_back("estimatedTradingFeeBps must be between 0.0 and 100.0");
        if (minExpectedBenefitBps < 0.0 || minExpectedBenefitBps > 1000.0)
            errors.push_back("minExpectedBenefitBps must be between 0.0 and 1000.0");
        if (portfolioDrawdownStop <= 0.0 || portfolioDrawdownStop > 0.5)
            errors.push_back("portfolioDrawdownStop must be between 0.0 and 0.5");
        if (rebalanceInterval.count() <= 0)
            errors.push_back("rebalanceInterval must be positive");
        if (std::abs((totalInvestmentRatio + cashBufferPercentage) - 1.0) > 0.05)
            errors.push_back("totalInvestmentRatio + cashBufferPercentage must sum to approximately 1.0");

        return errors;
    }
};

// Target position for execution
struct TargetPosition {
    std::string symbol;
    double targetWeight = 0.0;           // Target portfolio weight (-1 to +1, negative = short)
    double currentWeight = 0.0;          // Current portfolio weight
    double confidence = 0.0;             // Confidence in this target
    double expectedReturn = 0.0;         // Expected return driving this target
    double riskAdjustedSize = 0.0;      // Position size after risk adjustments
    bool isLongPosition = false;
    bool isShortPosition = false;

    // Risk controls
    double stopLossLevel = 0.0;          // Stop-loss price level
    double maxLeverageUsed = 1.0;        // Leverage applied (always 1.0 for our strategy)
    double concentrationRisk = 0.0;      // Concentration risk score

    bool isValid() const {
        return !symbol.empty() &&
               std::abs(targetWeight) <= 1.0 &&
               confidence >= 0.0 && confidence <= 1.0 &&
               (isLongPosition || isShortPosition) && !(isLongPosition && isShortPosition);
    }
};

// Signal types for strategy interface
enum class SignalType {
    BUY,
    SELL,
    HOLD,
    CLOSE
};

struct TradingSignal {
    std::string symbol;
    SignalType signalType;
    double targetWeight;                 // Target portfolio weight
    double confidence;                   // Signal confidence
    std::string reasoning;               // Human-readable reasoning
    std::chrono::system_clock::time_point timestamp;

    bool isValid() const {
        return !symbol.empty() &&
               std::abs(targetWeight) <= 1.0 &&
               confidence >= 0.0 && confidence <= 1.0;
    }
};

// Performance metrics
struct StrategyMetrics {
    double totalReturn = 0.0;
    double sharpeRatio = 0.0;
    double maxDrawdown = 0.0;
    double volatility = 0.0;
    double informationRatio = 0.0;
    int activeTradingDays = 0;
    double averageTurnover = 0.0;
    std::chrono::system_clock::time_point lastUpdate;

    void reset() {
        totalReturn = 0.0;
        sharpeRatio = 0.0;
        maxDrawdown = 0.0;
        volatility = 0.0;
        informationRatio = 0.0;
        activeTradingDays = 0;
        averageTurnover = 0.0;
        lastUpdate = std::chrono::system_clock::now();
    }
};

} // namespace Common
} // namespace CryptoClaude