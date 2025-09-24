#include "TradingEngine.h"
#include "../Common/TradingTypes.h"
#include <iostream>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <random>
#include <iomanip>
#include <sstream>

namespace CryptoClaude {
namespace Trading {

TradingEngine::TradingEngine(const StrategyParameters& params)
    : params_(params), exclusionEngine_(std::make_unique<CoinExclusionEngine>()) {

    initializeDefaultParameters();

    // Validate configuration before proceeding
    if (!params_.validateConfiguration()) {
        auto errors = params_.getConfigurationErrors();
        std::string errorMsg = "TradingEngine configuration invalid: ";
        for (const auto& error : errors) {
            errorMsg += error + "; ";
        }
        throw std::invalid_argument(errorMsg);
    }

    validateConfiguration();

    lastRebalanceTime_ = std::chrono::system_clock::now() - params_.rebalanceInterval;

    std::cout << "🔥 CryptoClaude Trading Engine Initialized" << std::endl;
    std::cout << "📊 Max pairs: " << params_.maxPairsToCreate << std::endl;
    std::cout << "⚡ Rebalance interval: " << params_.rebalanceInterval.count() << " minutes" << std::endl;
    std::cout << "🎯 Cash buffer: " << params_.cashBufferPercentage * 100 << "%" << std::endl;
}

// === CORE TRADING OPERATIONS ===

void TradingEngine::runTradingCycle() {
    if (emergencyStop_) {
        logTradingAction("EMERGENCY_STOP", "Trading halted due to emergency conditions");
        return;
    }

    try {
        logTradingAction("CYCLE_START", "Beginning trading cycle");

        // 1. Update trading universe and check system health
        updatePairingUniverse();

        if (!isSystemHealthy()) {
            logTradingAction("SYSTEM_UNHEALTHY", "System health check failed, skipping cycle");
            return;
        }

        // 2. Check if rebalancing is needed
        if (!shouldRebalancePortfolio()) {
            logTradingAction("NO_REBALANCE", "No rebalancing needed at this time");
            // Risk monitoring handled in exclusion engine
            return;
        }

        // 3. Generate predictions with confidence
        std::vector<std::string> symbols = tradingUniverse_.eligibleSymbols;
        std::vector<PredictionWithConfidence> predictions = generatePredictions(symbols);

        if (predictions.empty()) {
            logTradingAction("NO_PREDICTIONS", "No valid predictions available");
            return;
        }

        // 4. Filter predictions using coin exclusion engine
        std::vector<std::string> eligibleSymbols;
        for (const auto& pred : predictions) {
            eligibleSymbols.push_back(pred.symbol);
        }
        std::vector<std::string> filteredSymbols = excludeUnsuitableCoins(eligibleSymbols);

        // Filter predictions to only include suitable coins
        std::vector<PredictionData> filteredPredictions;
        for (const auto& pred : predictions) {
            if (std::find(filteredSymbols.begin(), filteredSymbols.end(), pred.symbol) != filteredSymbols.end()) {
                filteredPredictions.push_back(pred);
            }
        }

        // 5. Create trading pairs from filtered predictions
        std::vector<TradingPair> tradingPairs = createTradingPairs(filteredPredictions);

        // 6. Allocate capital to pairs based on confidence
        std::vector<TradingPair> allocatedPairs = allocateCapitalToPairs(tradingPairs);

        // 7. Convert pairs to target positions
        std::vector<TargetPosition> targets = convertPairsToTargetPositions(allocatedPairs);
        currentTargets_ = targets;

        // 8. Calculate required trades for rebalancing
        std::vector<TradeOrder> rebalanceOrders = calculateRebalancingTrades(targets);

        // 9. Analyze transaction costs vs expected benefits
        double totalCosts = estimateTransactionCosts(rebalanceOrders);
        double expectedBenefit = calculateExpectedBenefit(targets);

        if (expectedBenefit < totalCosts + params_.minExpectedBenefitBps / 10000.0) {
            logTradingAction("INSUFFICIENT_BENEFIT",
                "Expected benefit (" + std::to_string(expectedBenefit * 10000) + " bps) " +
                "insufficient vs costs (" + std::to_string(totalCosts * 10000) + " bps)");
            return;
        }

        // 10. Execute trades if beneficial
        if (!rebalanceOrders.empty()) {
            logTradingAction("EXECUTING_REBALANCE",
                "Executing " + std::to_string(rebalanceOrders.size()) + " rebalance orders");
            executeOrderBatch(rebalanceOrders);
            lastRebalanceTime_ = std::chrono::system_clock::now();
        }

        // 11. Monitor risk through exclusion engine
        // Risk monitoring integrated into exclusion checks

        logTradingAction("CYCLE_COMPLETE", "Trading cycle completed successfully");

    } catch (const std::exception& e) {
        logTradingAction("CYCLE_ERROR", "Trading cycle error: " + std::string(e.what()));

        // Activate emergency stop if too many errors
        static int errorCount = 0;
        if (++errorCount > 3) {
            activateEmergencyStop("Too many consecutive trading cycle errors");
        }
    }
}

void TradingEngine::updateMarketData(const std::string& symbol, const MarketData& data) {
    latestMarketData_[symbol] = data;

    // Update exclusion engine with new data
    if (exclusionEngine_) {
        exclusionEngine_->updateMarketData(symbol, data);
    }

    // Check for immediate risk events
    if (data.hasValidPrice()) {
        for (const auto& position : currentPositions_) {
            if (position.getSymbol() == symbol) {
                // Position risk monitored through exclusion engine
            }
        }
    }
}

void TradingEngine::updateSentimentData(const std::string& symbol, const SentimentData& data) {
    latestSentimentData_[symbol] = data;

    // Update exclusion engine with sentiment data
    if (exclusionEngine_) {
        exclusionEngine_->updateSentimentData(symbol, data);
    }
}

void TradingEngine::setCurrentPortfolio(const Portfolio& portfolio,
                                       const std::vector<Position>& positions) {
    currentPortfolio_ = portfolio;
    currentPositions_ = positions;

    // Position management integrated with exclusion engine

    logTradingAction("PORTFOLIO_UPDATE",
        "Updated portfolio: " + std::to_string(positions.size()) + " positions, " +
        "value: $" + std::to_string(portfolio.getTotalValue()));
}

// === PREDICTION & TARGET GENERATION ===

std::vector<PredictionData> TradingEngine::generatePredictions(
    const std::vector<std::string>& symbols,
    const std::string& predictionModel) {

    std::vector<PredictionData> predictions;

    for (const std::string& symbol : symbols) {
        // Skip if we don't have recent market data
        if (!hasRecentMarketData(symbol, 30)) {
            continue;
        }

        try {
            PredictionData prediction;
            prediction.symbol = symbol;
            prediction.predictionModel = predictionModel;
            prediction.predictionTimestamp = std::chrono::system_clock::now();

            // Simplified Random Forest simulation (in production, integrate with actual RF model)
            // Generate prediction based on recent price action, sentiment, and technical factors

            auto marketIt = latestMarketData_.find(symbol);
            if (marketIt == latestMarketData_.end()) continue;

            const MarketData& marketData = marketIt->second;

            // Simulate Random Forest prediction based on multiple factors
            std::random_device rd;
            std::mt19937 gen(rd());

            // Base prediction from trend and momentum
            double trendFactor = 0.0;
            if (marketData.hasValidPrice()) {
                // Simulate trend analysis (in production: use actual technical indicators)
                std::normal_distribution<> trendDist(0.02, 0.15);  // Slight positive bias
                trendFactor = trendDist(gen);
            }

            // Sentiment factor
            double sentimentFactor = 0.0;
            auto sentimentIt = latestSentimentData_.find(symbol);
            if (sentimentIt != latestSentimentData_.end()) {
                const SentimentData& sentiment = sentimentIt->second;
                if (sentiment.hasSignificantSentiment()) {
                    sentimentFactor = sentiment.getAvgSentiment() * 0.05; // 5% max impact
                }
            }

            // Volume factor
            double volumeFactor = 0.0;
            if (marketData.hasValidVolume()) {
                // Higher volume = more reliable signal
                std::uniform_real_distribution<> volDist(-0.02, 0.02);
                volumeFactor = volDist(gen);
            }

            // Combine factors for prediction
            prediction.predictedReturn = trendFactor + sentimentFactor + volumeFactor;

            // Model confidence based on data quality and historical performance
            double confidence = 0.5;  // Base confidence

            // Increase confidence with good sentiment coverage
            if (sentimentIt != latestSentimentData_.end() &&
                sentimentIt->second.hasSignificantSentiment()) {
                confidence += 0.2;
            }

            // Increase confidence with good volume
            if (marketData.hasValidVolume() && marketData.getTotalVolume() > 1000000) {
                confidence += 0.2;
            }

            // Historical model performance (simulated - in production use actual backtesting results)
            auto perfIt = tradingUniverse_.modelPerformance.find(symbol);
            if (perfIt != tradingUniverse_.modelPerformance.end()) {
                confidence = std::min(0.95, confidence + perfIt->second * 0.3);
            }

            prediction.confidence = std::max(0.1, std::min(0.95, confidence));

            // Get risk metrics from exclusion engine
            if (exclusionEngine_) {
                try {
                    prediction.volatilityForecast = exclusionEngine_->calculateRealizedVolatility(symbol);
                    prediction.maxDrawdownRisk = 0.15;  // Default for pairing strategy

                    // Market correlation not needed for pairing strategy
                    prediction.correlationToMarket = 0.0;

                } catch (const std::exception& e) {
                    // Default risk values
                    prediction.volatilityForecast = 0.3;
                    prediction.maxDrawdownRisk = 0.15;
                    prediction.correlationToMarket = 0.0;
                }
            }

            // Historical regression R² (simulated - in production use actual model validation)
            std::uniform_real_distribution<> r2Dist(0.15, 0.45);
            prediction.regressionR2 = r2Dist(gen);

            predictions.push_back(prediction);

        } catch (const std::exception& e) {
            logTradingAction("PREDICTION_ERROR",
                "Failed to generate prediction for " + symbol + ": " + e.what());
        }
    }

    // Sort predictions by expected return for ranking
    std::sort(predictions.begin(), predictions.end(),
        [](const PredictionData& a, const PredictionData& b) {
            return a.predictedReturn > b.predictedReturn;
        });

    logTradingAction("PREDICTIONS_GENERATED",
        "Generated " + std::to_string(predictions.size()) + " predictions");

    return predictions;
}

// PAIRING STRATEGY CORE METHODS (UPDATED FOR PAIRING APPROACH)

std::vector<std::string> TradingEngine::excludeUnsuitableCoins(
    const std::vector<std::string>& candidates) {

    if (!exclusionEngine_) {
        return candidates;  // No filtering if no exclusion engine
    }

    return exclusionEngine_->filterExcludedCoins(candidates);
}

std::vector<TradingPair> TradingEngine::createTradingPairs(
    const std::vector<PredictionData>& predictions) {

    std::vector<TradingPair> pairs;

    if (predictions.size() < 2) {
        return pairs;  // Need at least 2 coins to create a pair
    }

    // Sort predictions by expected return (highest to lowest)
    std::vector<PredictionData> sortedPredictions = predictions;
    std::sort(sortedPredictions.begin(), sortedPredictions.end(),
        [](const PredictionData& a, const PredictionData& b) {
            return a.predictedReturn > b.predictedReturn;
        });

    // Create pairs: highest return with lowest return
    int maxPairs = std::min(params_.maxPairsToCreate, static_cast<int>(sortedPredictions.size() / 2));

    for (int i = 0; i < maxPairs; ++i) {
        int longIdx = i;  // Top of the list (highest expected return)
        int shortIdx = sortedPredictions.size() - 1 - i;  // Bottom of the list (lowest expected return)

        if (longIdx >= shortIdx) break;  // Avoid pairing same coin or crossing over

        TradingPair pair;
        pair.longSymbol = sortedPredictions[longIdx].symbol;
        pair.shortSymbol = sortedPredictions[shortIdx].symbol;
        pair.longExpectedReturn = sortedPredictions[longIdx].predictedReturn;
        pair.shortExpectedReturn = sortedPredictions[shortIdx].predictedReturn;

        // Combined confidence is average of both coins
        pair.pairConfidence = (sortedPredictions[longIdx].confidence + sortedPredictions[shortIdx].confidence) / 2.0;

        pairs.push_back(pair);
    }

    logTradingAction("PAIRS_CREATED",
        "Created " + std::to_string(pairs.size()) + " trading pairs from " +
        std::to_string(predictions.size()) + " predictions");

    return pairs;
}

std::vector<TradingPair> TradingEngine::allocateCapitalToPairs(
    const std::vector<TradingPair>& pairs) {

    std::vector<TradingPair> allocatedPairs = pairs;

    if (pairs.empty()) {
        return allocatedPairs;
    }

    // Available capital for investment (excluding cash buffer)
    double availableCapital = params_.totalInvestmentRatio;

    // Calculate average confidence
    double avgConfidence = 0.0;
    for (const auto& pair : pairs) {
        avgConfidence += pair.pairConfidence;
    }
    avgConfidence /= pairs.size();

    if (avgConfidence >= 0.80) {
        // High confidence: concentrate allocation in top pairs
        for (size_t i = 0; i < allocatedPairs.size(); ++i) {
            if (i < 2) {  // Top 2 pairs get more allocation
                allocatedPairs[i].allocationWeight = availableCapital * 0.4;  // 40% each for top 2
            } else {
                double remainingCapital = availableCapital * 0.2;  // 20% for remaining pairs
                allocatedPairs[i].allocationWeight = remainingCapital / (allocatedPairs.size() - 2);
            }
        }
    } else {
        // Medium/Low confidence: equal weight distribution
        double equalWeight = availableCapital / pairs.size();
        for (auto& pair : allocatedPairs) {
            pair.allocationWeight = std::min(equalWeight, params_.maxSinglePairAllocation);
        }
    }

    logTradingAction("CAPITAL_ALLOCATED",
        "Allocated capital across " + std::to_string(allocatedPairs.size()) +
        " pairs (avg confidence: " + std::to_string(avgConfidence) + ")");

    return allocatedPairs;
}

std::vector<TargetPosition> TradingEngine::convertPairsToTargetPositions(
    const std::vector<TradingPair>& pairs) {

    std::vector<TargetPosition> targets;

    for (const auto& pair : pairs) {
        // Create long position
        TargetPosition longTarget;
        longTarget.symbol = pair.longSymbol;
        longTarget.targetWeight = pair.allocationWeight;
        longTarget.currentWeight = 0.0;  // Will be updated later
        longTarget.confidence = pair.pairConfidence;
        longTarget.expectedReturn = pair.longExpectedReturn;
        longTarget.riskAdjustedSize = pair.allocationWeight;
        longTarget.isLongPosition = true;
        longTarget.isShortPosition = false;
        longTarget.stopLossLevel = getCurrentPrice(pair.longSymbol) * (1.0 - params_.portfolioDrawdownStop * 0.5);
        longTarget.maxLeverageUsed = 1.0;
        longTarget.concentrationRisk = pair.allocationWeight / params_.maxSinglePairAllocation;

        // Create short position
        TargetPosition shortTarget;
        shortTarget.symbol = pair.shortSymbol;
        shortTarget.targetWeight = -pair.allocationWeight;  // Negative for short
        shortTarget.currentWeight = 0.0;
        shortTarget.confidence = pair.pairConfidence;
        shortTarget.expectedReturn = pair.shortExpectedReturn;
        shortTarget.riskAdjustedSize = -pair.allocationWeight;
        shortTarget.isLongPosition = false;
        shortTarget.isShortPosition = true;
        shortTarget.stopLossLevel = getCurrentPrice(pair.shortSymbol) * (1.0 + params_.portfolioDrawdownStop * 0.5);
        shortTarget.maxLeverageUsed = 1.0;
        shortTarget.concentrationRisk = pair.allocationWeight / params_.maxSinglePairAllocation;

        targets.push_back(longTarget);
        targets.push_back(shortTarget);
    }

    // Update current weights for existing positions
    for (auto& target : targets) {
        for (const auto& position : currentPositions_) {
            if (position.getSymbol() == target.symbol && position.getQuantity() != 0.0) {
                target.currentWeight = calculatePositionWeight(position);
                break;
            }
        }
    }

    logTradingAction("TARGETS_GENERATED",
        "Generated " + std::to_string(targets.size()) + " target positions from " +
        std::to_string(pairs.size()) + " pairs");

    return targets;
}

// COIN EXCLUSION METHODS

bool TradingEngine::shouldExcludeCoinLiquidity(const std::string& symbol) {
    if (!exclusionEngine_) return false;
    auto alert = exclusionEngine_->checkLiquidityExclusion(symbol);
    return alert.shouldExclude;
}

bool TradingEngine::shouldExcludeCoinVolatility(const std::string& symbol) {
    if (!exclusionEngine_) return false;
    auto alert = exclusionEngine_->checkVolatilityExclusion(symbol);
    return alert.shouldExclude;
}

bool TradingEngine::shouldExcludeCoinModelConfidence(const std::string& symbol) {
    if (!exclusionEngine_) return false;
    auto alert = exclusionEngine_->checkModelConfidenceExclusion(symbol);
    return alert.shouldExclude;
}

bool TradingEngine::shouldExcludeCoinTechnical(const std::string& symbol) {
    if (!exclusionEngine_) return false;
    auto alert = exclusionEngine_->checkTechnicalExclusion(symbol);
    return alert.shouldExclude;
}

void TradingEngine::updatePairingUniverse() {
    // Apply coin exclusion filtering
    if (exclusionEngine_) {
        tradingUniverse_.eligibleSymbols = exclusionEngine_->filterExcludedCoins(tradingUniverse_.eligibleSymbols);

        logTradingAction("PAIRING_UNIVERSE_UPDATED",
            "Universe filtered to " + std::to_string(tradingUniverse_.eligibleSymbols.size()) +
            " symbols after exclusion");
    }

    // Update liquidity scores and model performance for eligible symbols
    for (const std::string& symbol : tradingUniverse_.eligibleSymbols) {
        tradingUniverse_.liquidityScores[symbol] = calculateLiquidityScore(symbol);
        tradingUniverse_.modelPerformance[symbol] = calculateModelPerformance(symbol);

        // Simulate spread and volume data
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> spreadDist(5.0, 30.0);  // 5-30bps
        std::uniform_real_distribution<> volumeDist(5000000.0, 100000000.0);  // $5M-$100M

        tradingUniverse_.averageSpreads[symbol] = spreadDist(gen);
        tradingUniverse_.averageVolumes[symbol] = volumeDist(gen);
        tradingUniverse_.shortingAllowed[symbol] = true;  // Most crypto can be shorted
    }

    // Limit universe size
    if (tradingUniverse_.eligibleSymbols.size() > tradingUniverse_.maxUniverseSize) {
        // Keep the most liquid and best-performing assets
        std::sort(tradingUniverse_.eligibleSymbols.begin(),
                 tradingUniverse_.eligibleSymbols.end(),
                 [this](const std::string& a, const std::string& b) {
                     return tradingUniverse_.liquidityScores[a] * tradingUniverse_.modelPerformance[a] >
                            tradingUniverse_.liquidityScores[b] * tradingUniverse_.modelPerformance[b];
                 });

        tradingUniverse_.eligibleSymbols.resize(tradingUniverse_.maxUniverseSize);
    }

    logTradingAction("UNIVERSE_UPDATED",
        "Trading universe updated: " + std::to_string(tradingUniverse_.eligibleSymbols.size()) + " eligible symbols");
}


double TradingEngine::calculateLiquidityScore(const std::string& symbol) {
    // Simplified liquidity scoring based on available data
    double score = 0.5;  // Base score

    auto marketIt = latestMarketData_.find(symbol);
    if (marketIt != latestMarketData_.end()) {
        const MarketData& data = marketIt->second;

        // Volume component
        if (data.hasValidVolume()) {
            double volume = data.getTotalVolume();
            if (volume > 50000000) score += 0.3;      // $50M+ = high liquidity
            else if (volume > 10000000) score += 0.2; // $10M+ = medium liquidity
            else if (volume > 1000000) score += 0.1;  // $1M+ = low liquidity
        }

        // Price stability component (less volatility = more liquid)
        if (data.hasValidPrice()) {
            // In production, calculate recent volatility
            // For now, give higher scores to major coins
            if (symbol == "BTC" || symbol == "ETH") score += 0.2;
            else if (symbol == "BNB" || symbol == "XRP" || symbol == "ADA") score += 0.1;
        }
    }

    return std::min(1.0, score);
}

double TradingEngine::calculateModelPerformance(const std::string& symbol) {
    // Simplified model performance calculation
    // In production, this would be based on actual backtesting results

    double performance = 0.2;  // Base R² score

    // Major coins typically have better model performance
    if (symbol == "BTC") performance = 0.35;
    else if (symbol == "ETH") performance = 0.32;
    else if (symbol == "BNB" || symbol == "XRP") performance = 0.28;
    else if (symbol == "ADA" || symbol == "SOL" || symbol == "DOT") performance = 0.25;
    else {
        // Random variation for other coins
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dist(0.15, 0.30);
        performance = dist(gen);
    }

    return performance;
}

// === REBALANCING & EXECUTION ===

bool TradingEngine::shouldRebalancePortfolio() {
    auto now = std::chrono::system_clock::now();

    // Check time-based rebalancing
    if (now - lastRebalanceTime_ < params_.rebalanceInterval) {
        return false;
    }

    // Check if any current positions deviate significantly from targets
    if (!currentTargets_.empty()) {
        for (const auto& target : currentTargets_) {
            double deviation = std::abs(target.targetWeight - target.currentWeight);
            if (deviation > params_.minRebalanceThreshold) {
                return true;
            }
        }
    }

    // Check for emergency rebalancing conditions
    if (currentDrawdown_ > params_.portfolioDrawdownStop * 0.8) {  // 80% of max drawdown
        return true;
    }

    return false;
}

std::vector<TradeOrder> TradingEngine::calculateRebalancingTrades(
    const std::vector<TargetPosition>& targets) {

    std::vector<TradeOrder> orders;
    double portfolioValue = currentPortfolio_.getTotalValue();

    if (portfolioValue <= 0) {
        logTradingAction("NO_PORTFOLIO_VALUE", "Portfolio value is zero or negative");
        return orders;
    }

    for (const auto& target : targets) {
        // Find current weight for this symbol
        double currentWeight = 0.0;
        for (const auto& position : currentPositions_) {
            if (position.getSymbol() == target.symbol && position.getQuantity() != 0.0) {
                currentWeight = calculatePositionWeight(position);
                break;
            }
        }

        double weightDifference = target.targetWeight - currentWeight;

        // Skip if difference is too small
        if (std::abs(weightDifference) < params_.minRebalanceThreshold) {
            continue;
        }

        // Calculate trade quantity
        double tradeValueUSD = weightDifference * portfolioValue;
        double currentPrice = getCurrentPrice(target.symbol);

        if (currentPrice <= 0) {
            logTradingAction("NO_PRICE", "No valid price for " + target.symbol);
            continue;
        }

        double tradeQuantity = tradeValueUSD / currentPrice;

        // Create trade order
        TradeOrder order;
        order.orderId = generateOrderId();
        order.symbol = target.symbol;
        order.quantity = tradeQuantity;
        order.price = 0.0;  // Market order
        order.type = tradeQuantity > 0 ? TradeOrder::MARKET_BUY : TradeOrder::MARKET_SELL;
        order.status = TradeOrder::PENDING;
        order.orderTimestamp = std::chrono::system_clock::now();
        order.isRebalanceOrder = true;
        order.orderReason = "Portfolio rebalancing - target weight: " +
                           std::to_string(target.targetWeight * 100) + "%";

        // Estimate execution costs
        order.expectedSlippage = estimateSlippage(target.symbol, std::abs(tradeQuantity));
        order.estimatedCost = std::abs(tradeValueUSD) *
                             (params_.estimatedTradingFeeBps / 10000.0 + order.expectedSlippage);

        orders.push_back(order);
    }

    // Close positions not in target portfolio
    for (const auto& position : currentPositions_) {
        if (position.getQuantity() == 0) continue;

        // Check if this position is in our targets
        bool inTargets = false;
        for (const auto& target : targets) {
            if (target.symbol == position.getSymbol()) {
                inTargets = true;
                break;
            }
        }

        if (!inTargets) {
            // Create order to close this position
            TradeOrder order;
            order.orderId = generateOrderId();
            order.symbol = position.getSymbol();
            order.quantity = -position.getQuantity();  // Opposite of current position
            order.price = 0.0;  // Market order
            order.type = position.getQuantity() > 0 ? TradeOrder::MARKET_SELL : TradeOrder::MARKET_BUY;
            order.status = TradeOrder::PENDING;
            order.orderTimestamp = std::chrono::system_clock::now();
            order.isRebalanceOrder = true;
            order.orderReason = "Closing position - not in target portfolio";

            double tradeValueUSD = std::abs(position.getQuantity()) * getCurrentPrice(position.getSymbol());
            order.expectedSlippage = estimateSlippage(position.getSymbol(), std::abs(position.getQuantity()));
            order.estimatedCost = tradeValueUSD *
                                 (params_.estimatedTradingFeeBps / 10000.0 + order.expectedSlippage);

            orders.push_back(order);
        }
    }

    return orders;
}

double TradingEngine::estimateTransactionCosts(const std::vector<TradeOrder>& trades) {
    double totalCosts = 0.0;

    for (const auto& trade : trades) {
        totalCosts += trade.estimatedCost;
    }

    return totalCosts / currentPortfolio_.getTotalValue();  // Return as fraction of portfolio
}

double TradingEngine::calculateExpectedBenefit(const std::vector<TargetPosition>& targets) {
    double expectedBenefit = 0.0;
    double portfolioValue = currentPortfolio_.getTotalValue();

    for (const auto& target : targets) {
        Position pos = getPositionForSymbol(target.symbol);
        double currentWeight = calculatePositionWeight(pos);
        double weightDifference = target.targetWeight - currentWeight;

        // Expected benefit = weight change * expected return * confidence
        double positionBenefit = std::abs(weightDifference) * target.expectedReturn * target.confidence;
        expectedBenefit += positionBenefit;
    }

    return expectedBenefit;
}

void TradingEngine::executeOrder(TradeOrder& order) {
    try {
        logTradingAction("EXECUTING_ORDER",
            "Executing " + order.symbol + " order: " + std::to_string(order.quantity));

        order.status = TradeOrder::SUBMITTED;

        // Simulate order execution (in production, integrate with exchange API)
        double currentPrice = getCurrentPrice(order.symbol);
        if (currentPrice <= 0) {
            order.status = TradeOrder::REJECTED;
            order.orderReason += " - No valid price available";
            return;
        }

        // Simulate execution with slippage
        double slippage = estimateSlippage(order.symbol, std::abs(order.quantity));
        double executionPrice = order.type == TradeOrder::MARKET_BUY ?
            currentPrice * (1.0 + slippage) : currentPrice * (1.0 - slippage);

        order.executedQuantity = order.quantity;
        order.averageExecutedPrice = executionPrice;
        order.status = TradeOrder::FILLED;
        order.executionTimestamp = std::chrono::system_clock::now();

        // Update position
        updatePositionFromOrder(order);

        logTradingAction("ORDER_FILLED",
            order.symbol + " order filled: " + std::to_string(order.executedQuantity) +
            " at $" + std::to_string(order.averageExecutedPrice));

    } catch (const std::exception& e) {
        order.status = TradeOrder::REJECTED;
        order.orderReason += " - Execution failed: " + std::string(e.what());
        logTradingAction("ORDER_FAILED", order.symbol + " order failed: " + e.what());
    }
}

void TradingEngine::executeOrderBatch(std::vector<TradeOrder>& orders) {
    logTradingAction("BATCH_EXECUTION_START",
        "Executing batch of " + std::to_string(orders.size()) + " orders");

    for (auto& order : orders) {
        executeOrder(order);
        pendingOrders_.push_back(order);
    }

    // Move completed orders to history
    processFilledOrders();

    logTradingAction("BATCH_EXECUTION_COMPLETE",
        "Batch execution completed");
}

// === RISK MANAGEMENT ===

} // namespace Trading
} // namespace CryptoClaude
