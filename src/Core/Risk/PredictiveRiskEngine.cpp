#include "PredictiveRiskEngine.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <stdexcept>

namespace CryptoClaude {
namespace Risk {

// === COIN EXCLUSION FRAMEWORK IMPLEMENTATION ===

bool CoinExclusionEngine::shouldExcludeCoin(const std::string& symbol) {
    try {
        // Check all exclusion criteria
        auto liquidityAlert = checkLiquidityExclusion(symbol);
        auto volatilityAlert = checkVolatilityExclusion(symbol);
        auto confidenceAlert = checkModelConfidenceExclusion(symbol);
        auto technicalAlert = checkTechnicalExclusion(symbol);
        auto newsAlert = checkNewsEventExclusion(symbol);

        // Collect all reasons for exclusion
        std::vector<CoinExclusionAlert::ExclusionReason> exclusionReasons;
        double totalExclusionScore = 0.0;

        if (liquidityAlert.shouldExclude) {
            exclusionReasons.push_back(liquidityAlert.reason);
            totalExclusionScore += liquidityAlert.exclusionScore;
        }

        if (volatilityAlert.shouldExclude) {
            exclusionReasons.push_back(volatilityAlert.reason);
            totalExclusionScore += volatilityAlert.exclusionScore;
        }

        if (confidenceAlert.shouldExclude) {
            exclusionReasons.push_back(confidenceAlert.reason);
            totalExclusionScore += confidenceAlert.exclusionScore;
        }

        if (technicalAlert.shouldExclude) {
            exclusionReasons.push_back(technicalAlert.reason);
            totalExclusionScore += technicalAlert.exclusionScore;
        }

        if (newsAlert.shouldExclude) {
            exclusionReasons.push_back(newsAlert.reason);
            totalExclusionScore += newsAlert.exclusionScore;
        }

        // Update exclusion metrics
        CoinExclusionMetrics metrics;
        metrics.symbol = symbol;
        metrics.isExcluded = !exclusionReasons.empty();
        metrics.exclusionScore = std::min(1.0, totalExclusionScore); // Cap at 1.0
        metrics.activeReasons = exclusionReasons;
        metrics.lastAssessment = std::chrono::system_clock::now();

        updateExclusionMetrics(symbol, metrics);

        // Update active exclusions list
        if (metrics.isExcluded) {
            currentlyExcludedCoins_.insert(symbol);

            // Add most severe alert to active list
            CoinExclusionAlert mostSevere = liquidityAlert;
            if (volatilityAlert.exclusionScore > mostSevere.exclusionScore) mostSevere = volatilityAlert;
            if (confidenceAlert.exclusionScore > mostSevere.exclusionScore) mostSevere = confidenceAlert;
            if (technicalAlert.exclusionScore > mostSevere.exclusionScore) mostSevere = technicalAlert;
            if (newsAlert.exclusionScore > mostSevere.exclusionScore) mostSevere = newsAlert;

            activeExclusions_.push_back(mostSevere);
        } else {
            currentlyExcludedCoins_.erase(symbol);
        }

        return metrics.isExcluded;

    } catch (const std::exception& e) {
        handleExclusionError(symbol, e.what());
        return true; // Err on the side of caution - exclude if error
    }
}

CoinExclusionAlert CoinExclusionEngine::checkLiquidityExclusion(const std::string& symbol) {
    CoinExclusionAlert alert;
    alert.symbol = symbol;
    alert.reason = CoinExclusionAlert::LIQUIDITY_CRISIS;
    alert.alertTimestamp = std::chrono::system_clock::now();
    alert.shouldExclude = false;
    alert.exclusionScore = 0.0;

    // For real-time trading, be more lenient with data requirements
    auto marketIt = latestMarketData_.find(symbol);
    if (marketIt == latestMarketData_.end()) {
        alert.level = CoinExclusionAlert::WARNING;
        alert.message = "No market data available for " + symbol;
        alert.exclusionScore = 0.8;
        alert.shouldExclude = true;
        return alert;
    }

    // If we have at least one recent data point, proceed with assessment
    if (!hasRecentData(symbol, 24)) {
        alert.level = CoinExclusionAlert::WARNING;
        alert.message = "Market data too old for " + symbol;
        alert.exclusionScore = 0.5;
        alert.shouldExclude = true;
        return alert;
    }

    // Calculate liquidity score (higher = more liquid)
    double liquidityScore = calculateLiquidityScore(symbol, liquidityLookbackDays_);
    double bidAskSpread = calculateBidAskSpread(symbol);
    int exchangeCount = countActiveExchanges(symbol);

    // Check against thresholds
    double totalVolume = 0.0;
    auto recentVolumes = getRecentVolumes(symbol, liquidityLookbackDays_);
    if (!recentVolumes.empty()) {
        totalVolume = std::accumulate(recentVolumes.begin(), recentVolumes.end(), 0.0) / recentVolumes.size();
    }

    bool volumeTooLow = totalVolume < exclusionCriteria_.minDailyVolumeUSD;
    bool spreadTooWide = bidAskSpread > exclusionCriteria_.maxBidAskSpread;
    bool tooFewExchanges = exchangeCount < exclusionCriteria_.minExchangeCount;

    if (volumeTooLow || spreadTooWide || tooFewExchanges) {
        alert.shouldExclude = true;
        alert.level = CoinExclusionAlert::CRITICAL;

        // Calculate exclusion score based on severity
        double volumeRatio = totalVolume / exclusionCriteria_.minDailyVolumeUSD;
        double spreadRatio = bidAskSpread / exclusionCriteria_.maxBidAskSpread;
        double exchangeRatio = static_cast<double>(exchangeCount) / exclusionCriteria_.minExchangeCount;

        alert.exclusionScore = std::max({
            1.0 - volumeRatio,
            spreadRatio - 1.0,
            1.0 - exchangeRatio
        });
        alert.exclusionScore = std::clamp(alert.exclusionScore, 0.0, 1.0);

        alert.message = "Liquidity crisis detected: ";
        if (volumeTooLow) alert.message += "volume too low ($" + std::to_string(totalVolume/1e6) + "M < $" + std::to_string(exclusionCriteria_.minDailyVolumeUSD/1e6) + "M) ";
        if (spreadTooWide) alert.message += "spread too wide (" + std::to_string(bidAskSpread*100) + "% > " + std::to_string(exclusionCriteria_.maxBidAskSpread*100) + "%) ";
        if (tooFewExchanges) alert.message += "too few exchanges (" + std::to_string(exchangeCount) + " < " + std::to_string(exclusionCriteria_.minExchangeCount) + ")";

        alert.suggestedActions.push_back("Exclude from pairing until liquidity improves");
        alert.suggestedActions.push_back("Monitor for 24h before reconsidering");
    } else {
        alert.level = CoinExclusionAlert::INFO;
        alert.message = "Liquidity metrics within acceptable range";
    }

    return alert;
}

CoinExclusionAlert CoinExclusionEngine::checkVolatilityExclusion(const std::string& symbol) {
    CoinExclusionAlert alert;
    alert.symbol = symbol;
    alert.reason = CoinExclusionAlert::EXTREME_VOLATILITY;
    alert.alertTimestamp = std::chrono::system_clock::now();
    alert.shouldExclude = false;
    alert.exclusionScore = 0.0;

    // Check if we have market data
    auto marketIt = latestMarketData_.find(symbol);
    if (marketIt == latestMarketData_.end()) {
        alert.level = CoinExclusionAlert::WARNING;
        alert.message = "No market data available for " + symbol;
        alert.exclusionScore = 0.8;
        alert.shouldExclude = true;
        return alert;
    }

    // For new coins with limited history, assume normal volatility unless evidence otherwise
    auto priceHistIt = priceHistory_.find(symbol);
    if (priceHistIt == priceHistory_.end() || priceHistIt->second.size() < 5) {
        alert.level = CoinExclusionAlert::INFO;
        alert.message = "Limited data - assuming normal volatility";
        alert.exclusionScore = 0.1;
        alert.shouldExclude = false;
        return alert;
    }

    // Calculate realized volatility
    double realizedVol = calculateRealizedVolatility(symbol, volatilityLookbackDays_);
    double volatilitySpike = calculateVolatilitySpike(symbol);

    // Check against thresholds
    bool volTooHigh = realizedVol > exclusionCriteria_.maxDailyVolatility;
    bool spikeDetected = volatilitySpike > exclusionCriteria_.maxVolatilitySpike;

    if (volTooHigh || spikeDetected) {
        alert.shouldExclude = true;
        alert.level = CoinExclusionAlert::CRITICAL;

        // Calculate exclusion score
        double volRatio = realizedVol / exclusionCriteria_.maxDailyVolatility;
        double spikeRatio = volatilitySpike / exclusionCriteria_.maxVolatilitySpike;

        alert.exclusionScore = std::max(volRatio - 1.0, spikeRatio - 1.0);
        alert.exclusionScore = std::clamp(alert.exclusionScore, 0.0, 1.0);

        alert.message = "Extreme volatility detected: ";
        if (volTooHigh) alert.message += "daily volatility " + std::to_string(realizedVol*100) + "% > " + std::to_string(exclusionCriteria_.maxDailyVolatility*100) + "% ";
        if (spikeDetected) alert.message += "volatility spike " + std::to_string(volatilitySpike) + "x > " + std::to_string(exclusionCriteria_.maxVolatilitySpike) + "x";

        alert.suggestedActions.push_back("Exclude from pairing during high volatility period");
        alert.suggestedActions.push_back("Wait for volatility normalization");
    } else {
        alert.level = CoinExclusionAlert::INFO;
        alert.message = "Volatility within acceptable range (" + std::to_string(realizedVol*100) + "% daily)";
    }

    return alert;
}

CoinExclusionAlert CoinExclusionEngine::checkModelConfidenceExclusion(const std::string& symbol) {
    CoinExclusionAlert alert;
    alert.symbol = symbol;
    alert.reason = CoinExclusionAlert::MODEL_CONFIDENCE_LOW;
    alert.alertTimestamp = std::chrono::system_clock::now();
    alert.shouldExclude = false;
    alert.exclusionScore = 0.0;

    // Assess model confidence
    double modelConfidence = assessModelConfidence(symbol, modelConfidenceLookbackDays_);
    bool hasRecentFailures = hasRecentModelFailures(symbol);

    // Check against thresholds
    bool confidenceTooLow = modelConfidence < exclusionCriteria_.minModelConfidence;

    if (confidenceTooLow || hasRecentFailures) {
        alert.shouldExclude = true;
        alert.level = CoinExclusionAlert::WARNING;

        // Calculate exclusion score
        double confidenceRatio = modelConfidence / exclusionCriteria_.minModelConfidence;
        alert.exclusionScore = 1.0 - confidenceRatio;
        if (hasRecentFailures) alert.exclusionScore += 0.3; // Additional penalty for failures
        alert.exclusionScore = std::clamp(alert.exclusionScore, 0.0, 1.0);

        alert.message = "Low model confidence: " + std::to_string(modelConfidence*100) + "% < " + std::to_string(exclusionCriteria_.minModelConfidence*100) + "%";
        if (hasRecentFailures) alert.message += " with recent prediction failures";

        alert.suggestedActions.push_back("Exclude until model confidence improves");
        alert.suggestedActions.push_back("Retrain model with additional data");
    } else {
        alert.level = CoinExclusionAlert::INFO;
        alert.message = "Model confidence acceptable (" + std::to_string(modelConfidence*100) + "%)";
    }

    return alert;
}

CoinExclusionAlert CoinExclusionEngine::checkTechnicalExclusion(const std::string& symbol) {
    CoinExclusionAlert alert;
    alert.symbol = symbol;
    alert.reason = CoinExclusionAlert::TECHNICAL_BREAKDOWN;
    alert.alertTimestamp = std::chrono::system_clock::now();
    alert.shouldExclude = false;
    alert.exclusionScore = 0.0;

    if (!hasValidMarketData(symbol, technicalLookbackDays_)) {
        alert.level = CoinExclusionAlert::INFO;
        alert.message = "Insufficient data for technical analysis";
        return alert;
    }

    // Calculate technical indicators
    double rsi = calculateRSI(symbol, 14);
    bool overbought = isTechnicallyOverbought(symbol);
    bool oversold = isTechnicallyOversold(symbol);

    // Check for extreme conditions
    bool rsiExtreme = rsi > exclusionCriteria_.maxRSIExtreme || rsi < exclusionCriteria_.minRSIExtreme;

    if (rsiExtreme || overbought || oversold) {
        alert.shouldExclude = true;
        alert.level = CoinExclusionAlert::WARNING;

        // Calculate exclusion score based on RSI extremity
        if (rsi > exclusionCriteria_.maxRSIExtreme) {
            alert.exclusionScore = (rsi - exclusionCriteria_.maxRSIExtreme) / (100.0 - exclusionCriteria_.maxRSIExtreme);
        } else if (rsi < exclusionCriteria_.minRSIExtreme) {
            alert.exclusionScore = (exclusionCriteria_.minRSIExtreme - rsi) / exclusionCriteria_.minRSIExtreme;
        }
        alert.exclusionScore = std::clamp(alert.exclusionScore, 0.0, 1.0);

        alert.message = "Technical extremes detected: RSI=" + std::to_string(rsi);
        if (overbought) alert.message += " (overbought)";
        if (oversold) alert.message += " (oversold)";

        alert.suggestedActions.push_back("Exclude during technical extreme");
        alert.suggestedActions.push_back("Wait for RSI normalization (30-70 range)");
    } else {
        alert.level = CoinExclusionAlert::INFO;
        alert.message = "Technical indicators within normal range (RSI=" + std::to_string(rsi) + ")";
    }

    return alert;
}

CoinExclusionAlert CoinExclusionEngine::checkNewsEventExclusion(const std::string& symbol) {
    CoinExclusionAlert alert;
    alert.symbol = symbol;
    alert.reason = CoinExclusionAlert::NEWS_EVENT_RISK;
    alert.alertTimestamp = std::chrono::system_clock::now();
    alert.shouldExclude = false;
    alert.exclusionScore = 0.0;

    // Check for recent negative news or sentiment instability
    bool hasNegativeNews = hasRecentNegativeNews(symbol);
    double sentimentInstability = calculateSentimentInstability(symbol, 7);

    if (hasNegativeNews || sentimentInstability > 2.0) { // 2 standard deviations
        alert.shouldExclude = true;
        alert.level = CoinExclusionAlert::WARNING;

        alert.exclusionScore = std::min(1.0, sentimentInstability / 3.0); // Scale to 0-1
        if (hasNegativeNews) alert.exclusionScore = std::max(alert.exclusionScore, 0.5);

        alert.message = "News event risk detected";
        if (hasNegativeNews) alert.message += ": recent negative news";
        if (sentimentInstability > 2.0) alert.message += ": sentiment instability (" + std::to_string(sentimentInstability) + " std devs)";

        alert.suggestedActions.push_back("Exclude during news-driven volatility");
        alert.suggestedActions.push_back("Monitor sentiment for 24-48 hours");
    } else {
        alert.level = CoinExclusionAlert::INFO;
        alert.message = "No significant news event risk detected";
    }

    return alert;
}

// === BATCH EXCLUSION PROCESSING ===

std::vector<std::string> CoinExclusionEngine::filterExcludedCoins(const std::vector<std::string>& candidates) {
    std::vector<std::string> filtered;

    for (const auto& symbol : candidates) {
        if (!shouldExcludeCoin(symbol)) {
            filtered.push_back(symbol);
        }
    }

    return filtered;
}

std::map<std::string, CoinExclusionAlert> CoinExclusionEngine::assessAllCoins(const std::vector<std::string>& coins) {
    std::map<std::string, CoinExclusionAlert> assessments;

    // Clear previous assessments
    cleanupOldExclusions();
    activeExclusions_.clear();

    for (const auto& coin : coins) {
        // Run full assessment for each coin
        bool excluded = shouldExcludeCoin(coin);

        // Find the most severe alert for this coin
        CoinExclusionAlert alert;
        alert.symbol = coin;
        alert.alertTimestamp = std::chrono::system_clock::now();
        alert.shouldExclude = excluded;

        if (excluded) {
            // Get the exclusion metrics to find the primary reason
            auto it = exclusionMetrics_.find(coin);
            if (it != exclusionMetrics_.end() && !it->second.activeReasons.empty()) {
                alert.reason = it->second.activeReasons[0]; // Primary reason
                alert.exclusionScore = it->second.exclusionScore;
                alert.level = (alert.exclusionScore > 0.7) ? CoinExclusionAlert::CRITICAL :
                             (alert.exclusionScore > 0.4) ? CoinExclusionAlert::WARNING :
                             CoinExclusionAlert::INFO;
                alert.message = "Excluded for " + std::to_string(it->second.activeReasons.size()) + " reasons";
            }
        } else {
            alert.level = CoinExclusionAlert::INFO;
            alert.message = "Passed all exclusion criteria";
            alert.exclusionScore = 0.0;
        }

        assessments[coin] = alert;
    }

    return assessments;
}

// === UTILITY METHODS IMPLEMENTATION ===

double CoinExclusionEngine::calculateLiquidityScore(const std::string& symbol, int lookbackDays) {
    auto volumes = getRecentVolumes(symbol, lookbackDays);
    if (volumes.empty()) return 0.0;

    // Calculate average volume and consistency
    double avgVolume = std::accumulate(volumes.begin(), volumes.end(), 0.0) / volumes.size();

    // Volume consistency (lower std dev = more consistent = better liquidity)
    double volumeStdDev = calculateStandardDeviation(volumes);
    double consistencyScore = std::exp(-volumeStdDev / avgVolume); // 0-1 score

    // Normalize volume to 0-1 scale (using 1B as max)
    double volumeScore = std::min(1.0, avgVolume / 1e9);

    return (volumeScore + consistencyScore) / 2.0;
}

double CoinExclusionEngine::calculateBidAskSpread(const std::string& symbol) {
    auto it = latestMarketData_.find(symbol);
    if (it == latestMarketData_.end()) return 0.01; // Default 1% if no data

    const auto& data = it->second;
    // Since MarketData doesn't have direct bid/ask, estimate spread from volume
    // In production, this would use actual order book data
    double totalVolume = data.getTotalVolume();
    if (totalVolume > 100e6) {
        return 0.001; // 0.1% for high volume
    } else if (totalVolume > 50e6) {
        return 0.002; // 0.2% for medium volume
    } else {
        return 0.005; // 0.5% for low volume
    }

    return 0.01; // Default 1%
}

int CoinExclusionEngine::countActiveExchanges(const std::string& symbol) {
    // In real implementation, this would query multiple exchanges
    // For now, simulate based on volume (higher volume = more exchanges)
    auto volumes = getRecentVolumes(symbol, 7);
    if (volumes.empty()) return 1;

    double avgVolume = std::accumulate(volumes.begin(), volumes.end(), 0.0) / volumes.size();

    // Estimate exchange count based on volume thresholds
    if (avgVolume > 500e6) return 5; // $500M+ = major exchanges
    if (avgVolume > 100e6) return 4; // $100M+ = most exchanges
    if (avgVolume > 50e6) return 3;  // $50M+ = some exchanges
    if (avgVolume > 10e6) return 2;  // $10M+ = few exchanges
    return 1; // < $10M = minimal exchanges
}

double CoinExclusionEngine::calculateRealizedVolatility(const std::string& symbol, int lookbackDays) {
    auto prices = getRecentPrices(symbol, lookbackDays);
    if (prices.size() < 2) {
        // Enhanced default volatility based on coin characteristics
        if (symbol == "BTC") return 0.6;        // Bitcoin: ~60% annual vol
        else if (symbol == "ETH") return 0.8;   // Ethereum: ~80% annual vol
        else if (symbol == "USDT" || symbol == "USDC" || symbol == "BUSD") return 0.05; // Stablecoins
        else return 0.9; // Other altcoins: ~90% annual volatility
    }

    auto returns = calculateReturns(prices);
    if (returns.empty()) return 0.9; // Default high volatility

    double dailyVol = calculateStandardDeviation(returns);
    return dailyVol * std::sqrt(252); // Annualized volatility
}

double CoinExclusionEngine::calculateVolatilitySpike(const std::string& symbol) {
    auto volatilities = getRecentVolatilities(symbol, 30);
    if (volatilities.size() < 7) return 0.0;

    // Compare recent volatility (last 7 days) to historical average
    double recentVol = std::accumulate(volatilities.end() - 7, volatilities.end(), 0.0) / 7.0;
    double historicalVol = std::accumulate(volatilities.begin(), volatilities.end() - 7, 0.0) / (volatilities.size() - 7);

    return (historicalVol > 0) ? recentVol / historicalVol : 0.0;
}

bool CoinExclusionEngine::isVolatilityExtreme(const std::string& symbol) {
    double realizedVol = calculateRealizedVolatility(symbol, 30);
    double spike = calculateVolatilitySpike(symbol);

    return realizedVol > exclusionCriteria_.maxDailyVolatility ||
           spike > exclusionCriteria_.maxVolatilitySpike;
}

double CoinExclusionEngine::calculateRSI(const std::string& symbol, int period) {
    auto prices = getRecentPrices(symbol, period + 10); // Extra data for calculation
    if (prices.size() < period + 1) return 50.0; // Neutral RSI if insufficient data

    auto rsiValues = calculateRSIValues(prices, period);
    return rsiValues.empty() ? 50.0 : rsiValues.back();
}

bool CoinExclusionEngine::isTechnicallyOverbought(const std::string& symbol) {
    double rsi = calculateRSI(symbol, 14);
    return rsi > exclusionCriteria_.maxRSIExtreme;
}

bool CoinExclusionEngine::isTechnicallyOversold(const std::string& symbol) {
    double rsi = calculateRSI(symbol, 14);
    return rsi < exclusionCriteria_.minRSIExtreme;
}

double CoinExclusionEngine::assessModelConfidence(const std::string& symbol, int lookbackDays) {
    // Enhanced model confidence assessment based on multiple factors
    double volatility = calculateRealizedVolatility(symbol, lookbackDays);
    auto prices = getRecentPrices(symbol, lookbackDays);
    auto volumes = getRecentVolumes(symbol, lookbackDays);

    // Base confidence from volatility (stable coins = higher confidence)
    double volConfidence = 1.0 / (1.0 + volatility * 2.0); // Scale factor

    // Data quality factor (more data = higher confidence)
    double dataQuality = std::min(1.0, static_cast<double>(prices.size()) / lookbackDays);

    // Volume consistency factor (consistent volume = higher confidence)
    double volumeConfidence = 0.5; // Default
    if (volumes.size() > 5) {
        double avgVolume = std::accumulate(volumes.begin(), volumes.end(), 0.0) / volumes.size();
        double volStdev = calculateStandardDeviation(volumes);
        double coeffVar = (avgVolume > 0) ? volStdev / avgVolume : 1.0;
        volumeConfidence = 1.0 / (1.0 + coeffVar); // Lower CV = higher confidence
    }

    // Market tier factor (major coins = higher confidence)
    double tierFactor = 0.6; // Default for altcoins
    if (symbol == "BTC") tierFactor = 1.0;
    else if (symbol == "ETH") tierFactor = 0.95;
    else if (symbol == "BNB" || symbol == "XRP" || symbol == "ADA") tierFactor = 0.85;

    // Combine factors with weights
    double confidence = (volConfidence * 0.4 + volumeConfidence * 0.3 + dataQuality * 0.2 + tierFactor * 0.1);

    return std::clamp(confidence, 0.1, 0.95); // Keep in reasonable range
}

bool CoinExclusionEngine::hasRecentModelFailures(const std::string& symbol) {
    // In real implementation, this would check actual model prediction accuracy
    // For now, simulate based on volatility spikes (high spikes = model failures)
    double spike = calculateVolatilitySpike(symbol);
    return spike > 2.0; // 2x normal volatility indicates model struggles
}

bool CoinExclusionEngine::hasRecentNegativeNews(const std::string& symbol) {
    // Check recent sentiment data for negative trends
    auto it = sentimentHistory_.find(symbol);
    if (it == sentimentHistory_.end() || it->second.empty()) return false;

    // Look at most recent sentiment data
    const auto& recent = it->second.back();
    double sentiment = recent.getSentimentScore();
    return sentiment < -0.3; // Significantly negative
}

double CoinExclusionEngine::calculateSentimentInstability(const std::string& symbol, int lookbackDays) {
    auto it = sentimentHistory_.find(symbol);
    if (it == sentimentHistory_.end() || it->second.size() < lookbackDays) return 0.0;

    // Extract recent sentiment scores
    std::vector<double> scores;
    for (auto rit = it->second.rbegin(); rit != it->second.rend() && scores.size() < lookbackDays; ++rit) {
        scores.push_back(rit->getSentimentScore());
    }

    if (scores.size() < 2) return 0.0;

    // Calculate standard deviation of sentiment scores
    return calculateStandardDeviation(scores);
}

// === DATA MANAGEMENT ===

void CoinExclusionEngine::updateMarketData(const std::string& symbol, const MarketData& data) {
    latestMarketData_[symbol] = data;

    // Update price history with efficient memory management
    priceHistory_[symbol].push_back(data.getClose());
    const size_t MAX_PRICE_HISTORY = 250;  // Reduced for better performance
    if (priceHistory_[symbol].size() > MAX_PRICE_HISTORY) {
        // Remove multiple elements at once for efficiency
        priceHistory_[symbol].erase(priceHistory_[symbol].begin(),
                                   priceHistory_[symbol].begin() + (priceHistory_[symbol].size() - MAX_PRICE_HISTORY));
    }

    // Update volume history with efficient memory management
    volumeHistory_[symbol].push_back(data.getTotalVolume());
    const size_t MAX_VOLUME_HISTORY = 250;
    if (volumeHistory_[symbol].size() > MAX_VOLUME_HISTORY) {
        volumeHistory_[symbol].erase(volumeHistory_[symbol].begin(),
                                    volumeHistory_[symbol].begin() + (volumeHistory_[symbol].size() - MAX_VOLUME_HISTORY));
    }

    // Periodic cleanup to prevent memory leaks
    static int updateCount = 0;
    if (++updateCount % 100 == 0) { // Every 100 updates
        cleanupOldExclusions(24); // Clean data older than 24 hours
    }
}

void CoinExclusionEngine::updateSentimentData(const std::string& symbol, const SentimentData& data) {
    sentimentHistory_[symbol].push_back(data);
    const size_t MAX_SENTIMENT_HISTORY = 30; // Reduced for memory efficiency
    if (sentimentHistory_[symbol].size() > MAX_SENTIMENT_HISTORY) {
        // Batch removal for efficiency
        sentimentHistory_[symbol].erase(sentimentHistory_[symbol].begin(),
                                       sentimentHistory_[symbol].begin() + (sentimentHistory_[symbol].size() - MAX_SENTIMENT_HISTORY));
    }
}

void CoinExclusionEngine::updateMultipleMarketData(const std::map<std::string, MarketData>& dataMap) {
    for (const auto& [symbol, data] : dataMap) {
        updateMarketData(symbol, data);
    }
}

void CoinExclusionEngine::updateMultipleSentimentData(const std::map<std::string, SentimentData>& sentimentMap) {
    for (const auto& [symbol, data] : sentimentMap) {
        updateSentimentData(symbol, data);
    }
}

bool CoinExclusionEngine::hasValidMarketData(const std::string& symbol, int minDataPoints) {
    return hasMinimumDataPoints(symbol, minDataPoints) && hasRecentData(symbol, 24);
}

bool CoinExclusionEngine::hasRecentData(const std::string& symbol, int maxAgeHours) {
    auto it = latestMarketData_.find(symbol);
    if (it == latestMarketData_.end()) return false;

    auto now = std::chrono::system_clock::now();
    auto dataAge = std::chrono::duration_cast<std::chrono::hours>(now - it->second.getTimestamp());

    return dataAge.count() <= maxAgeHours;
}

// === CONFIGURATION ===

CoinExclusionEngine::ExclusionDiagnostics CoinExclusionEngine::getDiagnostics() const {
    ExclusionDiagnostics diagnostics;
    diagnostics.totalCoinsAssessed = exclusionMetrics_.size();
    diagnostics.coinsExcluded = currentlyExcludedCoins_.size();
    diagnostics.lastAssessment = std::chrono::system_clock::now();

    // Count exclusion reasons
    for (const auto& [symbol, metrics] : exclusionMetrics_) {
        for (const auto& reason : metrics.activeReasons) {
            diagnostics.exclusionReasonCounts[reason]++;
        }
    }

    // Add warnings for unusual conditions
    if (diagnostics.coinsExcluded > diagnostics.totalCoinsAssessed * 0.5) {
        diagnostics.diagnosticWarnings.push_back("High exclusion rate: over 50% of coins excluded");
    }

    return diagnostics;
}

// === INTERNAL HELPER METHODS ===

std::vector<double> CoinExclusionEngine::calculateReturns(const std::vector<double>& prices) {
    std::vector<double> returns;
    for (size_t i = 1; i < prices.size(); ++i) {
        if (prices[i-1] > 0) {
            returns.push_back((prices[i] - prices[i-1]) / prices[i-1]);
        }
    }
    return returns;
}

double CoinExclusionEngine::calculateStandardDeviation(const std::vector<double>& data) {
    if (data.size() < 2) return 0.0;

    double mean = calculateMean(data);
    double sumSquaredDiffs = 0.0;

    for (double value : data) {
        double diff = value - mean;
        sumSquaredDiffs += diff * diff;
    }

    return std::sqrt(sumSquaredDiffs / (data.size() - 1));
}

double CoinExclusionEngine::calculateMean(const std::vector<double>& data) {
    if (data.empty()) return 0.0;
    return std::accumulate(data.begin(), data.end(), 0.0) / data.size();
}

double CoinExclusionEngine::calculatePercentile(const std::vector<double>& data, double percentile) {
    if (data.empty()) return 0.0;

    std::vector<double> sorted = data;
    std::sort(sorted.begin(), sorted.end());

    double index = percentile * (sorted.size() - 1);
    size_t lowerIndex = static_cast<size_t>(std::floor(index));
    size_t upperIndex = static_cast<size_t>(std::ceil(index));

    if (lowerIndex == upperIndex) {
        return sorted[lowerIndex];
    }

    double weight = index - lowerIndex;
    return sorted[lowerIndex] * (1.0 - weight) + sorted[upperIndex] * weight;
}

std::vector<double> CoinExclusionEngine::calculateMovingAverage(const std::vector<double>& data, int period) {
    std::vector<double> ma;
    if (data.size() < period) return ma;

    for (size_t i = period - 1; i < data.size(); ++i) {
        double sum = 0.0;
        for (int j = 0; j < period; ++j) {
            sum += data[i - j];
        }
        ma.push_back(sum / period);
    }

    return ma;
}

std::vector<double> CoinExclusionEngine::calculateRSIValues(const std::vector<double>& prices, int period) {
    std::vector<double> rsi;
    if (prices.size() < period + 1) return rsi;

    auto returns = calculateReturns(prices);

    // Calculate initial averages
    double avgGain = 0.0, avgLoss = 0.0;
    for (int i = 0; i < period; ++i) {
        if (returns[i] > 0) avgGain += returns[i];
        else avgLoss -= returns[i];
    }
    avgGain /= period;
    avgLoss /= period;

    // Calculate RSI for each subsequent period
    for (size_t i = period; i < returns.size(); ++i) {
        double gain = returns[i] > 0 ? returns[i] : 0.0;
        double loss = returns[i] < 0 ? -returns[i] : 0.0;

        avgGain = ((avgGain * (period - 1)) + gain) / period;
        avgLoss = ((avgLoss * (period - 1)) + loss) / period;

        double rs = (avgLoss > 0) ? avgGain / avgLoss : 100.0;
        double rsiValue = 100.0 - (100.0 / (1.0 + rs));

        rsi.push_back(rsiValue);
    }

    return rsi;
}

std::vector<double> CoinExclusionEngine::getRecentPrices(const std::string& symbol, int days) {
    auto it = priceHistory_.find(symbol);
    if (it == priceHistory_.end()) return {};

    const auto& prices = it->second;
    if (prices.size() <= days) return prices;

    return std::vector<double>(prices.end() - days, prices.end());
}

std::vector<double> CoinExclusionEngine::getRecentVolumes(const std::string& symbol, int days) {
    auto it = volumeHistory_.find(symbol);
    if (it == volumeHistory_.end()) return {};

    const auto& volumes = it->second;
    if (volumes.size() <= days) return volumes;

    return std::vector<double>(volumes.end() - days, volumes.end());
}

std::vector<double> CoinExclusionEngine::getRecentVolatilities(const std::string& symbol, int days) {
    auto prices = getRecentPrices(symbol, days + 10); // Extra data for volatility calculation
    if (prices.size() < 10) return {};

    std::vector<double> volatilities;

    // Calculate rolling 7-day volatility
    for (size_t i = 7; i < prices.size(); ++i) {
        std::vector<double> window(prices.begin() + i - 7, prices.begin() + i + 1);
        auto returns = calculateReturns(window);
        volatilities.push_back(calculateStandardDeviation(returns));
    }

    return volatilities;
}

double CoinExclusionEngine::calculateExclusionScore(const std::string& symbol, const std::vector<CoinExclusionAlert::ExclusionReason>& reasons) {
    double score = 0.0;

    // Weight different exclusion reasons
    for (const auto& reason : reasons) {
        switch (reason) {
            case CoinExclusionAlert::LIQUIDITY_CRISIS:
                score += 0.4; // High weight for liquidity
                break;
            case CoinExclusionAlert::EXTREME_VOLATILITY:
                score += 0.3; // High weight for volatility
                break;
            case CoinExclusionAlert::MODEL_CONFIDENCE_LOW:
                score += 0.2; // Medium weight for model confidence
                break;
            case CoinExclusionAlert::TECHNICAL_BREAKDOWN:
                score += 0.1; // Lower weight for technical
                break;
            case CoinExclusionAlert::NEWS_EVENT_RISK:
                score += 0.2; // Medium weight for news
                break;
            case CoinExclusionAlert::CORRELATION_ANOMALY:
                score += 0.1; // Lower weight for correlation
                break;
        }
    }

    return std::clamp(score, 0.0, 1.0);
}

void CoinExclusionEngine::updateExclusionMetrics(const std::string& symbol, const CoinExclusionMetrics& metrics) {
    exclusionMetrics_[symbol] = metrics;
}

void CoinExclusionEngine::cleanupOldExclusions(int maxAgeHours) {
    auto now = std::chrono::system_clock::now();
    auto cutoff = now - std::chrono::hours(maxAgeHours);

    // Remove old exclusion metrics
    auto it = exclusionMetrics_.begin();
    while (it != exclusionMetrics_.end()) {
        if (it->second.lastAssessment < cutoff) {
            currentlyExcludedCoins_.erase(it->first);
            it = exclusionMetrics_.erase(it);
        } else {
            ++it;
        }
    }

    // Clean up active exclusions
    auto alertIt = std::remove_if(activeExclusions_.begin(), activeExclusions_.end(),
        [cutoff](const CoinExclusionAlert& alert) {
            return alert.alertTimestamp < cutoff;
        });
    activeExclusions_.erase(alertIt, activeExclusions_.end());

    // Clean up old market data (keep only recent data)
    auto dataIt = latestMarketData_.begin();
    while (dataIt != latestMarketData_.end()) {
        if (dataIt->second.getTimestamp() < cutoff) {
            // Also remove from history maps
            priceHistory_.erase(dataIt->first);
            volumeHistory_.erase(dataIt->first);
            volatilityHistory_.erase(dataIt->first);
            sentimentHistory_.erase(dataIt->first);
            dataIt = latestMarketData_.erase(dataIt);
        } else {
            ++dataIt;
        }
    }

    // Prevent activeExclusions from growing too large
    const size_t MAX_ACTIVE_EXCLUSIONS = 1000;
    if (activeExclusions_.size() > MAX_ACTIVE_EXCLUSIONS) {
        // Keep only the most recent alerts
        std::sort(activeExclusions_.begin(), activeExclusions_.end(),
                 [](const CoinExclusionAlert& a, const CoinExclusionAlert& b) {
                     return a.alertTimestamp > b.alertTimestamp;
                 });
        activeExclusions_.resize(MAX_ACTIVE_EXCLUSIONS);
    }
}

void CoinExclusionEngine::initializeDefaultCriteria() {
    exclusionCriteria_ = CoinExclusionCriteria{}; // Use default values
}

bool CoinExclusionEngine::validatePriceData(const std::vector<double>& prices) {
    if (prices.empty()) return false;

    // Check for non-positive prices
    for (double price : prices) {
        if (price <= 0) return false;
    }

    // Check for extreme price movements (> 10x in single period)
    for (size_t i = 1; i < prices.size(); ++i) {
        double ratio = prices[i] / prices[i-1];
        if (ratio > 10.0 || ratio < 0.1) return false;
    }

    return true;
}

bool CoinExclusionEngine::hasMinimumDataPoints(const std::string& symbol, int minPoints) {
    auto it = priceHistory_.find(symbol);
    return it != priceHistory_.end() && it->second.size() >= minPoints;
}

void CoinExclusionEngine::logExclusionWarning(const std::string& symbol, const std::string& warning) {
    // In real implementation, this would log to a proper logging system
    // For now, we'll store it in diagnostics
}

void CoinExclusionEngine::handleExclusionError(const std::string& symbol, const std::string& error) {
    logExclusionWarning(symbol, "ERROR: " + error);

    // Add emergency exclusion for safety
    currentlyExcludedCoins_.insert(symbol);

    CoinExclusionAlert errorAlert;
    errorAlert.symbol = symbol;
    errorAlert.level = CoinExclusionAlert::EMERGENCY;
    errorAlert.reason = CoinExclusionAlert::CORRELATION_ANOMALY; // Generic error reason
    errorAlert.message = "Emergency exclusion due to error: " + error;
    errorAlert.shouldExclude = true;
    errorAlert.exclusionScore = 1.0;
    errorAlert.alertTimestamp = std::chrono::system_clock::now();

    activeExclusions_.push_back(errorAlert);
}

void CoinExclusionEngine::clearAllExclusions() {
    currentlyExcludedCoins_.clear();
    activeExclusions_.clear();
    exclusionMetrics_.clear();
}

void CoinExclusionEngine::clearExclusionForSymbol(const std::string& symbol) {
    currentlyExcludedCoins_.erase(symbol);
    exclusionMetrics_.erase(symbol);

    activeExclusions_.erase(
        std::remove_if(activeExclusions_.begin(), activeExclusions_.end(),
            [&symbol](const CoinExclusionAlert& alert) {
                return alert.symbol == symbol;
            }),
        activeExclusions_.end()
    );
}

} // namespace Risk
} // namespace CryptoClaude