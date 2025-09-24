#pragma once

#include "../Database/Models/PortfolioData.h"
#include "../Database/Models/MarketData.h"
#include "../Database/Models/SentimentData.h"
#include "../Risk/PredictiveRiskEngine.h"  // Now contains CoinExclusionEngine
#include "../Common/TradingTypes.h"
#include "../../Configuration/Config.h"
#include <vector>
#include <map>
#include <string>
#include <chrono>
#include <memory>
#include <optional>

using namespace CryptoClaude::Common;

namespace CryptoClaude {
namespace Trading {

using namespace CryptoClaude::Database::Models;
using namespace CryptoClaude::Risk;

// Forward declarations for trading system components

// Use unified prediction type
using PredictionWithConfidence = PredictionData;

// Use unified target position type
// (Already defined in TradingTypes.h)

struct TradeOrder {
    enum OrderType {
        MARKET_BUY,
        MARKET_SELL,
        LIMIT_BUY,
        LIMIT_SELL,
        STOP_LOSS,
        TAKE_PROFIT
    };

    enum OrderStatus {
        PENDING,
        SUBMITTED,
        PARTIALLY_FILLED,
        FILLED,
        CANCELLED,
        REJECTED,
        EXPIRED
    };

    std::string orderId;
    std::string symbol;
    OrderType type;
    OrderStatus status;
    double quantity;             // Positive for buy, negative for sell
    double price;                // Limit price (0 for market orders)
    double executedQuantity;     // Actually filled quantity
    double averageExecutedPrice; // Average fill price

    std::chrono::system_clock::time_point orderTimestamp;
    std::chrono::system_clock::time_point executionTimestamp;

    // Risk and reasoning
    std::string orderReason;     // Why this order was placed
    double expectedSlippage;     // Expected execution slippage
    double estimatedCost;        // Total estimated transaction cost
    bool isRebalanceOrder;       // Part of portfolio rebalancing
    bool isRiskControlOrder;     // Emergency risk control order
};

struct TradingUniverse {
    std::vector<std::string> eligibleSymbols;
    std::map<std::string, double> liquidityScores;    // 0-1, higher = more liquid
    std::map<std::string, double> modelPerformance;   // Historical R² or Sharpe
    std::map<std::string, double> averageSpreads;     // Bid-ask spreads
    std::map<std::string, double> averageVolumes;     // 30-day average volume
    std::map<std::string, bool> shortingAllowed;      // Can we short this asset

    // Universe filtering criteria
    double minLiquidityScore = 0.6;
    double minModelR2 = 0.15;
    double maxSpreadBps = 50.0;           // Max 50bps spread
    double minVolumeUSD = 10000000.0;     // Min $10M daily volume
    int maxUniverseSize = 100;
    int targetPortfolioSize = 15;         // Target number of positions
};

// Use unified strategy parameters
using PairingRiskLimits = StrategyParameters;

// Use unified strategy parameters
using PairingParameters = StrategyParameters;

class TradingEngine {
private:
    // Core system state
    Portfolio currentPortfolio_;
    std::vector<Position> currentPositions_;
    TradingUniverse tradingUniverse_;
    StrategyParameters params_;

    // Component systems
    std::unique_ptr<CoinExclusionEngine> exclusionEngine_;

    // Trading state
    std::vector<TargetPosition> currentTargets_;
    std::vector<TradeOrder> pendingOrders_;
    std::vector<TradeOrder> orderHistory_;

    // Performance tracking
    std::map<std::string, double> dailyPnL_;
    std::map<std::string, double> positionStopLosses_;
    double currentDrawdown_ = 0.0;
    double maxDrawdownSinceStart_ = 0.0;

    // Market data cache
    std::map<std::string, MarketData> latestMarketData_;
    std::map<std::string, SentimentData> latestSentimentData_;
    std::chrono::system_clock::time_point lastRebalanceTime_;

    // Emergency state
    bool emergencyStop_ = false;
    bool riskOverride_ = false;
    std::vector<std::string> riskAlerts_;

public:
    explicit TradingEngine(const StrategyParameters& params = StrategyParameters{});

    // === CORE TRADING OPERATIONS ===

    // Main trading cycle - call this periodically
    void runTradingCycle();

    // Update market data and recalculate if needed
    void updateMarketData(const std::string& symbol, const MarketData& data);
    void updateSentimentData(const std::string& symbol, const SentimentData& data);

    // Portfolio state management
    void setCurrentPortfolio(const Portfolio& portfolio, const std::vector<Position>& positions);
    Portfolio getCurrentPortfolio() const { return currentPortfolio_; }
    std::vector<Position> getCurrentPositions() const { return currentPositions_; }

    // === PAIRING ALGORITHM (CORE OF STRATEGY) ===

    // Get predictions with confidence from ML models
    std::vector<PredictionData> generatePredictions(
        const std::vector<std::string>& symbols,
        const std::string& predictionModel = "RandomForest"
    );

    // Use unified trading pair type
    // (Already defined in TradingTypes.h)

    std::vector<TradingPair> createTradingPairs(
        const std::vector<PredictionData>& predictions
    );

    // Allocate capital to pairs based on confidence
    std::vector<TradingPair> allocateCapitalToPairs(
        const std::vector<TradingPair>& pairs
    );

    // Convert pairs to target positions for execution
    std::vector<TargetPosition> convertPairsToTargetPositions(
        const std::vector<TradingPair>& pairs
    );

    // === COIN EXCLUSION & FILTERING (PAIRING STRATEGY) ===

    // MAIN EXCLUSION ENGINE: Filter out unsuitable coins for pairing
    std::vector<std::string> excludeUnsuitableCoins(
        const std::vector<std::string>& candidates
    );

    // Individual exclusion criteria
    bool shouldExcludeCoinLiquidity(const std::string& symbol);
    bool shouldExcludeCoinVolatility(const std::string& symbol);
    bool shouldExcludeCoinModelConfidence(const std::string& symbol);
    bool shouldExcludeCoinTechnical(const std::string& symbol);

    // Build filtered universe for pairing
    void updatePairingUniverse();

    // Legacy methods (kept for compatibility)
    double calculateLiquidityScore(const std::string& symbol);
    double calculateModelPerformance(const std::string& symbol);

    // === REBALANCING & EXECUTION ===

    // Determine if rebalancing is needed
    bool shouldRebalancePortfolio();

    // Calculate required trades for rebalancing
    std::vector<TradeOrder> calculateRebalancingTrades(
        const std::vector<TargetPosition>& targets
    );

    // Transaction cost analysis
    double estimateTransactionCosts(const std::vector<TradeOrder>& trades);
    double calculateExpectedBenefit(const std::vector<TargetPosition>& targets);

    // Execute individual trade orders
    void executeOrder(TradeOrder& order);
    void executeOrderBatch(std::vector<TradeOrder>& orders);

    // === PAIR-FOCUSED RISK MANAGEMENT ===

    // Cash buffer protection (CRITICAL - never breach)
    void checkCashBufferIntegrity();
    bool isCashBufferBreached();

    // Portfolio-level controls
    void checkPortfolioDrawdownLimit();
    double calculateCurrentDrawdown();

    // Pair-level monitoring (treat pairs as units)
    void checkAllPairs();
    bool shouldLiquidatePair(const TradingPair& pair);
    void liquidatePair(const TradingPair& pair);  // Liquidate BOTH sides simultaneously

    // Emergency controls (same logic but pair-aware)
    void activateEmergencyStop(const std::string& reason);
    void closeAllPairs();  // Close all pairs, maintain cash buffer

    // Market-neutral specific risks
    void checkPairCorrelationBreakdown();
    bool isPairBehavingAsExpected(const TradingPair& pair);

    // === CONFIGURATION & CONTROLS ===

    void setStrategyParameters(const StrategyParameters& params);
    void setTradingUniverse(const TradingUniverse& universe) { tradingUniverse_ = universe; }

    StrategyParameters getStrategyParameters() const { return params_; }
    TradingUniverse getTradingUniverse() const { return tradingUniverse_; }

    // Configuration validation
    bool isConfigurationValid() const { return params_.validateConfiguration(); }
    std::vector<std::string> getConfigurationErrors() const { return params_.getConfigurationErrors(); }

    // Emergency controls
    void enableEmergencyStop() { emergencyStop_ = true; }
    void disableEmergencyStop() { emergencyStop_ = false; }
    void enableRiskOverride() { riskOverride_ = true; }
    void disableRiskOverride() { riskOverride_ = false; }

    // === REPORTING & ANALYTICS ===

    struct TradingReport {
        std::vector<TargetPosition> currentTargets;
        std::vector<TradeOrder> recentOrders;
        double portfolioValue;
        double dailyPnL;
        double currentDrawdown;
        double portfolioVolatility;
        double totalLongExposure;
        double totalShortExposure;
        std::vector<std::string> activeAlerts;
        std::chrono::system_clock::time_point reportTimestamp;

        // Performance metrics
        double sharpeRatio;
        double informationRatio;
        double maxDrawdown;
        int activeTradingDays;
        double averageTurnover;
    };

    TradingReport generateTradingReport();

    // Order and execution history
    std::vector<TradeOrder> getOrderHistory(int lastNOrders = 50) const;
    std::vector<TradeOrder> getPendingOrders() const { return pendingOrders_; }

    // Performance analytics
    double calculateSharpeRatio(int lookbackDays = 252);
    double calculateInformationRatio(int lookbackDays = 252);
    std::map<std::string, double> getPositionContributions();

    // === DIAGNOSTICS & TESTING ===

    // System health checks
    bool isSystemHealthy();
    std::vector<std::string> getSystemWarnings();

    // Backtesting and simulation support
    void runBacktest(const std::chrono::system_clock::time_point& startDate,
                    const std::chrono::system_clock::time_point& endDate);

    // Model validation
    void validatePredictionModels();
    std::map<std::string, double> getModelPerformanceMetrics();

private:
    // === INTERNAL HELPER METHODS ===

    // Portfolio analysis
    double calculatePositionWeight(const Position& position);
    std::vector<Position> getPositionsForSymbols(const std::vector<std::string>& symbols);
    Position getPositionForSymbol(const std::string& symbol);

    // Risk calculations
    double calculatePositionRisk(const Position& position);
    double calculateCorrelationMatrix(const std::vector<std::string>& symbols);

    // Order management
    std::string generateOrderId();
    void updateOrderStatus(TradeOrder& order);
    void processFilledOrders();
    void updatePositionFromOrder(const TradeOrder& order);

    // Market data utilities
    bool hasRecentMarketData(const std::string& symbol, int maxAgeMinutes = 10);
    double getCurrentPrice(const std::string& symbol);
    double estimateSlippage(const std::string& symbol, double orderSize);

    // Utility methods
    void logTradingAction(const std::string& action, const std::string& details);
    void sendAlert(const std::string& alertType, const std::string& message);
    void updatePerformanceMetrics();

    // Configuration validation
    void validateConfiguration();
    void initializeDefaultParameters();
};

} // namespace Trading
} // namespace CryptoClaude