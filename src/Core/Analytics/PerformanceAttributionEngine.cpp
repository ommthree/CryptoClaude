#include "PerformanceAttributionEngine.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <sstream>

namespace CryptoClaude {
namespace Analytics {

// === CORE PERFORMANCE ATTRIBUTION IMPLEMENTATIONS ===

std::vector<FactorAttribution> PerformanceAttributionEngine::calculateFactorAttribution(
    const Portfolio& portfolio,
    const std::vector<Position>& positions,
    const std::chrono::system_clock::time_point& startDate,
    const std::chrono::system_clock::time_point& endDate) {

    std::vector<FactorAttribution> attributions;

    if (!validateDateRange(startDate, endDate) || positions.empty()) {
        logAnalysisWarning("Invalid date range or empty positions for factor attribution");
        return attributions;
    }

    try {
        // Market Factor Attribution
        FactorAttribution marketFactor;
        marketFactor.factorName = "Market";
        marketFactor.specificReturn = calculateMarketFactor(positions, startDate, endDate);
        marketFactor.totalReturn = marketFactor.specificReturn;
        marketFactor.interactionReturn = 0.0;

        PerformanceContribution marketContrib;
        marketContrib.factor = "Market Beta";
        marketContrib.contribution = marketFactor.specificReturn;
        marketContrib.contributionPercent = 100.0 * marketFactor.specificReturn / portfolio.getTotalPnL();
        marketContrib.description = "Return attributable to overall market movement";
        marketContrib.confidence = 0.9;
        marketFactor.contributions.push_back(marketContrib);

        attributions.push_back(marketFactor);

        // Size Factor Attribution
        FactorAttribution sizeFactor;
        sizeFactor.factorName = "Size";
        sizeFactor.specificReturn = calculateSizeFactor(positions, startDate, endDate);
        sizeFactor.totalReturn = sizeFactor.specificReturn;
        sizeFactor.interactionReturn = 0.0;

        PerformanceContribution sizeContrib;
        sizeContrib.factor = "Position Size Effect";
        sizeContrib.contribution = sizeFactor.specificReturn;
        sizeContrib.contributionPercent = 100.0 * sizeFactor.specificReturn / portfolio.getTotalPnL();
        sizeContrib.description = "Return from position sizing decisions";
        sizeContrib.confidence = 0.7;
        sizeFactor.contributions.push_back(sizeContrib);

        attributions.push_back(sizeFactor);

        // Momentum Factor Attribution
        FactorAttribution momentumFactor;
        momentumFactor.factorName = "Momentum";
        momentumFactor.specificReturn = calculateMomentumFactor(positions, startDate, endDate);
        momentumFactor.totalReturn = momentumFactor.specificReturn;
        momentumFactor.interactionReturn = 0.0;

        PerformanceContribution momentumContrib;
        momentumContrib.factor = "Price Momentum";
        momentumContrib.contribution = momentumFactor.specificReturn;
        momentumContrib.contributionPercent = 100.0 * momentumFactor.specificReturn / portfolio.getTotalPnL();
        momentumContrib.description = "Return from momentum in asset prices";
        momentumContrib.confidence = 0.6;
        momentumFactor.contributions.push_back(momentumContrib);

        attributions.push_back(momentumFactor);

        // Volatility Factor Attribution
        FactorAttribution volatilityFactor;
        volatilityFactor.factorName = "Volatility";
        volatilityFactor.specificReturn = calculateVolatilityFactor(positions, startDate, endDate);
        volatilityFactor.totalReturn = volatilityFactor.specificReturn;
        volatilityFactor.interactionReturn = 0.0;

        PerformanceContribution volContrib;
        volContrib.factor = "Volatility Exposure";
        volContrib.contribution = volatilityFactor.specificReturn;
        volContrib.contributionPercent = 100.0 * volatilityFactor.specificReturn / portfolio.getTotalPnL();
        volContrib.description = "Return from volatility exposure";
        volContrib.confidence = 0.8;
        volatilityFactor.contributions.push_back(volContrib);

        attributions.push_back(volatilityFactor);

    } catch (const std::exception& e) {
        logAnalysisWarning("Factor attribution calculation failed: " + std::string(e.what()));
    }

    return attributions;
}

std::vector<AssetAttribution> PerformanceAttributionEngine::calculateAssetAttribution(
    const Portfolio& portfolio,
    const std::vector<Position>& positions,
    const std::map<std::string, double>& benchmarkWeights,
    const std::chrono::system_clock::time_point& startDate,
    const std::chrono::system_clock::time_point& endDate) {

    std::vector<AssetAttribution> attributions;

    if (positions.empty()) {
        logAnalysisWarning("No positions provided for asset attribution");
        return attributions;
    }

    try {
        double totalValue = portfolio.getTotalValue();

        for (const auto& position : positions) {
            AssetAttribution attribution;
            attribution.symbol = position.getSymbol();

            // Calculate position weight
            attribution.weight = position.getPositionValue() / totalValue;

            // Get benchmark weight
            double benchmarkWeight = 0.0;
            if (benchmarkWeights.count(position.getSymbol())) {
                benchmarkWeight = benchmarkWeights.at(position.getSymbol());
            }
            attribution.activeWeight = attribution.weight - benchmarkWeight;

            // Calculate position return
            double positionReturn = position.getPnLPercentage() / 100.0;
            attribution.totalReturn = positionReturn;

            // Simplified benchmark return (could be enhanced with historical data)
            double benchmarkReturn = 0.05; // Assume 5% benchmark return

            // Calculate attribution effects
            attribution.allocationEffect = calculateAllocationEffect(
                position.getSymbol(), attribution.weight, benchmarkWeight, benchmarkReturn);

            attribution.selectionEffect = calculateSelectionEffect(
                position.getSymbol(), benchmarkWeight, positionReturn, benchmarkReturn);

            attribution.interactionEffect = calculateInteractionEffect(
                position.getSymbol(), attribution.weight, benchmarkWeight, positionReturn, benchmarkReturn);

            // Calculate leverage effect
            double leverageRatio = position.getLeverageRatio();
            attribution.leverageEffect = (leverageRatio - 1.0) * positionReturn;

            // Calculate sentiment effect (simplified)
            attribution.sentimentEffect = 0.0; // Would require historical sentiment data

            attributions.push_back(attribution);
        }

    } catch (const std::exception& e) {
        logAnalysisWarning("Asset attribution calculation failed: " + std::string(e.what()));
    }

    return attributions;
}

double PerformanceAttributionEngine::calculateLeverageContribution(
    const Portfolio& portfolio,
    const std::vector<Position>& positions,
    const std::chrono::system_clock::time_point& startDate,
    const std::chrono::system_clock::time_point& endDate) {

    if (positions.empty()) {
        return 0.0;
    }

    try {
        double leverageContribution = 0.0;
        double totalValue = portfolio.getTotalValue();

        for (const auto& position : positions) {
            double leverageRatio = position.getLeverageRatio();
            double positionWeight = position.getPositionValue() / totalValue;
            double positionReturn = position.getPnLPercentage() / 100.0;

            // Leverage contribution is the additional return from leverage
            double leverageEffect = (leverageRatio - 1.0) * positionReturn;
            leverageContribution += positionWeight * leverageEffect;
        }

        return leverageContribution;

    } catch (const std::exception& e) {
        logAnalysisWarning("Leverage contribution calculation failed: " + std::string(e.what()));
        return 0.0;
    }
}

double PerformanceAttributionEngine::calculateSentimentAttribution(
    const std::vector<Position>& positions,
    const std::chrono::system_clock::time_point& startDate,
    const std::chrono::system_clock::time_point& endDate) {

    if (positions.empty()) {
        return 0.0;
    }

    try {
        // Simplified sentiment attribution
        // In practice, this would analyze historical sentiment data correlation with returns
        double sentimentContribution = 0.0;

        for (const auto& position : positions) {
            // Placeholder: assume sentiment contributed 1% of position return
            double positionReturn = position.getPnLPercentage() / 100.0;
            sentimentContribution += 0.01 * positionReturn;
        }

        return sentimentContribution / positions.size();

    } catch (const std::exception& e) {
        logAnalysisWarning("Sentiment attribution calculation failed: " + std::string(e.what()));
        return 0.0;
    }
}

// === ADVANCED PERFORMANCE METRICS ===

std::vector<double> PerformanceAttributionEngine::calculateRollingSharpeRatio(
    const std::vector<double>& returns,
    int windowSize) {

    std::vector<double> rollingSharpe;

    if (returns.size() < static_cast<size_t>(windowSize)) {
        logAnalysisWarning("Insufficient data for rolling Sharpe ratio calculation");
        return rollingSharpe;
    }

    for (size_t i = windowSize - 1; i < returns.size(); ++i) {
        std::vector<double> windowReturns(returns.begin() + i - windowSize + 1, returns.begin() + i + 1);

        double meanReturn = PerformanceUtils::calculateMean(windowReturns);
        double volatility = PerformanceUtils::calculateStandardDeviation(windowReturns);

        double excessReturn = meanReturn - (riskFreeRate_ / 365.0); // Daily risk-free rate
        double sharpe = (volatility > 0) ? excessReturn / volatility : 0.0;

        rollingSharpe.push_back(sharpe * std::sqrt(365.0)); // Annualized
    }

    return rollingSharpe;
}

std::vector<double> PerformanceAttributionEngine::calculateRollingVolatility(
    const std::vector<double>& returns,
    int windowSize) {

    std::vector<double> rollingVol;

    if (returns.size() < static_cast<size_t>(windowSize)) {
        return rollingVol;
    }

    for (size_t i = windowSize - 1; i < returns.size(); ++i) {
        std::vector<double> windowReturns(returns.begin() + i - windowSize + 1, returns.begin() + i + 1);
        double volatility = PerformanceUtils::calculateStandardDeviation(windowReturns);
        rollingVol.push_back(volatility * std::sqrt(365.0)); // Annualized
    }

    return rollingVol;
}

DrawdownAnalysis PerformanceAttributionEngine::calculateDrawdownAnalysis(
    const std::vector<double>& portfolioValues,
    const std::vector<std::chrono::system_clock::time_point>& dates) {

    DrawdownAnalysis analysis;

    if (portfolioValues.empty() || portfolioValues.size() != dates.size()) {
        logAnalysisWarning("Invalid data for drawdown analysis");
        return analysis;
    }

    try {
        std::vector<double> cumulativeReturns = calculateCumulativeReturns(calculateReturns(portfolioValues));

        analysis.maxDrawdown = PerformanceUtils::calculateMaxDrawdown(cumulativeReturns);

        // Find current drawdown
        double peak = *std::max_element(cumulativeReturns.begin(), cumulativeReturns.end());
        double current = cumulativeReturns.back();
        analysis.currentDrawdown = (peak - current) / peak;
        analysis.inDrawdown = analysis.currentDrawdown > 0.001; // 0.1% threshold

        // Find drawdown periods
        double runningMax = cumulativeReturns[0];
        bool inDrawdownPeriod = false;

        for (size_t i = 1; i < cumulativeReturns.size(); ++i) {
            if (cumulativeReturns[i] > runningMax) {
                runningMax = cumulativeReturns[i];
                if (inDrawdownPeriod) {
                    // End of drawdown period
                    analysis.drawdownEnd = dates[i];
                    analysis.recoveryDays = static_cast<int>(i) - analysis.drawdownDays;
                    inDrawdownPeriod = false;
                }
            } else {
                double drawdown = (runningMax - cumulativeReturns[i]) / runningMax;
                if (drawdown > 0.01 && !inDrawdownPeriod) { // 1% threshold
                    // Start of drawdown period
                    analysis.drawdownStart = dates[i];
                    analysis.drawdownDays = static_cast<int>(i);
                    inDrawdownPeriod = true;
                }

                analysis.drawdownHistory.push_back({dates[i], drawdown});
            }
        }

    } catch (const std::exception& e) {
        logAnalysisWarning("Drawdown analysis failed: " + std::string(e.what()));
    }

    return analysis;
}

WinLossAnalysis PerformanceAttributionEngine::calculateWinLossAnalysis(
    const std::vector<Position>& closedPositions) {

    WinLossAnalysis analysis;
    // Initialize all fields to avoid garbage values
    analysis.totalTrades = 0;
    analysis.winningTrades = 0;
    analysis.losingTrades = 0;
    analysis.winRate = 0.0;
    analysis.averageWin = 0.0;
    analysis.averageLoss = 0.0;
    analysis.profitFactor = 0.0;
    analysis.longestWinStreak = 0;
    analysis.longestLossStreak = 0;
    analysis.currentStreak = 0;
    analysis.currentStreakIsWin = false;

    if (closedPositions.empty()) {
        return analysis;
    }

    try {
        analysis.totalTrades = static_cast<int>(closedPositions.size());

        std::vector<double> returns;
        for (const auto& position : closedPositions) {
            double positionReturn = position.getPnLPercentage();
            returns.push_back(positionReturn);

            if (positionReturn > 0) {
                analysis.winningTrades++;
            } else {
                analysis.losingTrades++;
            }
        }

        analysis.tradeReturns = returns;
        analysis.winRate = static_cast<double>(analysis.winningTrades) / analysis.totalTrades;

        // Calculate average win/loss
        std::vector<double> wins, losses;
        for (double ret : returns) {
            if (ret > 0) wins.push_back(ret);
            else if (ret < 0) losses.push_back(ret);
        }

        analysis.averageWin = wins.empty() ? 0.0 : PerformanceUtils::calculateMean(wins);
        analysis.averageLoss = losses.empty() ? 0.0 : PerformanceUtils::calculateMean(losses);

        double grossProfit = std::accumulate(wins.begin(), wins.end(), 0.0);
        double grossLoss = std::abs(std::accumulate(losses.begin(), losses.end(), 0.0));
        analysis.profitFactor = (grossLoss > 0) ? grossProfit / grossLoss : 0.0;

        // Calculate streaks
        int currentStreak = 0;
        bool currentIsWin = false;
        analysis.longestWinStreak = 0;
        analysis.longestLossStreak = 0;

        for (size_t i = 0; i < returns.size(); ++i) {
            bool isWin = returns[i] > 0;

            if (i == 0 || isWin == currentIsWin) {
                currentStreak++;
            } else {
                if (currentIsWin) {
                    analysis.longestWinStreak = std::max(analysis.longestWinStreak, currentStreak);
                } else {
                    analysis.longestLossStreak = std::max(analysis.longestLossStreak, currentStreak);
                }
                currentStreak = 1;
                currentIsWin = isWin;
            }
        }

        // Final streak
        if (currentIsWin) {
            analysis.longestWinStreak = std::max(analysis.longestWinStreak, currentStreak);
        } else {
            analysis.longestLossStreak = std::max(analysis.longestLossStreak, currentStreak);
        }

        analysis.currentStreak = currentStreak;
        analysis.currentStreakIsWin = currentIsWin;

    } catch (const std::exception& e) {
        logAnalysisWarning("Win/loss analysis failed: " + std::string(e.what()));
    }

    return analysis;
}

RiskAdjustedMetrics PerformanceAttributionEngine::calculateRiskAdjustedMetrics(
    const std::vector<double>& returns,
    const std::vector<double>& benchmarkReturns,
    double beta) {

    RiskAdjustedMetrics metrics;

    if (returns.empty()) {
        logAnalysisWarning("No returns provided for risk-adjusted metrics");
        return metrics;
    }

    try {
        double meanReturn = PerformanceUtils::calculateMean(returns);
        double volatility = PerformanceUtils::calculateStandardDeviation(returns);
        double excessReturn = meanReturn - riskFreeRate_ / 365.0; // Daily risk-free rate

        // Basic risk-adjusted ratios
        metrics.sharpeRatio = PerformanceUtils::calculateSharpeRatio(excessReturn, volatility);

        metrics.downsideDeviation = calculateDownsideDeviation(returns);
        metrics.sortinoRatio = PerformanceUtils::calculateSortinoRatio(excessReturn, metrics.downsideDeviation);

        std::vector<double> cumulativeReturns = calculateCumulativeReturns(returns);
        double maxDrawdown = PerformanceUtils::calculateMaxDrawdown(cumulativeReturns);
        double annualReturn = meanReturn * 365.0;
        metrics.calmarRatio = PerformanceUtils::calculateCalmarRatio(annualReturn, maxDrawdown);

        metrics.treynorRatio = PerformanceUtils::calculateTreynorRatio(excessReturn, beta);

        // Benchmark-relative metrics
        if (!benchmarkReturns.empty() && benchmarkReturns.size() == returns.size()) {
            std::vector<double> activeReturns = calculateActiveReturns(returns, benchmarkReturns);
            double activeMean = PerformanceUtils::calculateMean(activeReturns);
            metrics.trackingError = PerformanceUtils::calculateStandardDeviation(activeReturns);
            metrics.informationRatio = (metrics.trackingError > 0) ? activeMean / metrics.trackingError : 0.0;

            // Jensen's Alpha
            double benchmarkMean = PerformanceUtils::calculateMean(benchmarkReturns);
            metrics.jensenAlpha = meanReturn - (riskFreeRate_ / 365.0 + beta * (benchmarkMean - riskFreeRate_ / 365.0));
        }

    } catch (const std::exception& e) {
        logAnalysisWarning("Risk-adjusted metrics calculation failed: " + std::string(e.what()));
    }

    return metrics;
}

// === BENCHMARKING FRAMEWORK ===

PeriodPerformance PerformanceAttributionEngine::calculateBenchmarkComparison(
    const Portfolio& portfolio,
    const std::string& benchmarkSymbol,
    const std::chrono::system_clock::time_point& startDate,
    const std::chrono::system_clock::time_point& endDate) {

    PeriodPerformance performance;
    performance.startDate = startDate;
    performance.endDate = endDate;

    if (!validateDateRange(startDate, endDate)) {
        logAnalysisWarning("Invalid date range for benchmark comparison");
        return performance;
    }

    try {
        // Calculate portfolio performance
        performance.totalReturn = portfolio.getTotalPnL() / portfolio.getTotalValue();

        // Simplified benchmark return (would use historical data in practice)
        performance.benchmarkReturn = 0.08; // Assume 8% benchmark return
        performance.activeReturn = performance.totalReturn - performance.benchmarkReturn;

        performance.tradingDays = calculateTradingDays(startDate, endDate);

        // Calculate annualized metrics (simplified)
        double annualizationFactor = 365.0 / performance.tradingDays;
        performance.totalReturn *= annualizationFactor;
        performance.benchmarkReturn *= annualizationFactor;
        performance.activeReturn *= annualizationFactor;

        // Placeholder metrics (would be calculated from historical data)
        performance.volatility = 0.25; // 25% annual volatility
        performance.sharpeRatio = (performance.totalReturn - riskFreeRate_) / performance.volatility;
        performance.maxDrawdown = 0.15; // 15% max drawdown
        performance.calmarRatio = performance.totalReturn / performance.maxDrawdown;
        performance.sortinoRatio = performance.sharpeRatio * 1.2; // Approximation

        performance.informationRatio = performance.activeReturn / (performance.volatility * 0.8); // Approximation

    } catch (const std::exception& e) {
        logAnalysisWarning("Benchmark comparison failed: " + std::string(e.what()));
    }

    return performance;
}

// === COMPREHENSIVE REPORTING ===

PerformanceAttributionEngine::PerformanceReport PerformanceAttributionEngine::generateComprehensiveReport(
    const Portfolio& portfolio,
    const std::vector<Position>& positions,
    const std::chrono::system_clock::time_point& startDate,
    const std::chrono::system_clock::time_point& endDate,
    const std::string& benchmarkSymbol) {

    PerformanceReport report;
    report.reportDate = std::chrono::system_clock::now();
    report.reportPeriod = "Custom Period";

    try {
        // Overall performance
        report.overallPerformance = calculateBenchmarkComparison(portfolio, benchmarkSymbol, startDate, endDate);

        // Factor attributions
        report.factorAttributions = calculateFactorAttribution(portfolio, positions, startDate, endDate);

        // Asset attributions
        report.assetAttributions = calculateAssetAttribution(portfolio, positions, benchmarkWeights_, startDate, endDate);

        // Create portfolio values for analysis (simplified)
        std::vector<double> portfolioValues;
        std::vector<std::chrono::system_clock::time_point> dates;

        // Generate sample data points (in practice, this would come from historical data)
        double currentValue = portfolio.getTotalValue();
        for (int i = 0; i < 30; ++i) {
            portfolioValues.push_back(currentValue * (1.0 + 0.001 * i)); // Sample growth
            dates.push_back(startDate + std::chrono::hours(24 * i));
        }

        // Drawdown analysis
        report.drawdownAnalysis = calculateDrawdownAnalysis(portfolioValues, dates);

        // Win/loss analysis
        report.winLossAnalysis = calculateWinLossAnalysis(positions);

        // Risk-adjusted metrics
        std::vector<double> returns = calculateReturns(portfolioValues);
        report.riskMetrics = calculateRiskAdjustedMetrics(returns);

        // Top contributors and detractors
        for (const auto& assetAttr : report.assetAttributions) {
            PerformanceContribution contrib;
            contrib.factor = assetAttr.symbol;
            contrib.contribution = assetAttr.totalReturn * assetAttr.weight;
            contrib.contributionPercent = contrib.contribution * 100.0;
            contrib.description = "Asset contribution to portfolio return";
            contrib.confidence = 0.8;

            if (contrib.contribution > 0) {
                report.topContributors.push_back(contrib);
            } else {
                report.topDetractors.push_back(contrib);
            }
        }

        // Sort contributors
        std::sort(report.topContributors.begin(), report.topContributors.end(),
                 [](const auto& a, const auto& b) { return a.contribution > b.contribution; });
        std::sort(report.topDetractors.begin(), report.topDetractors.end(),
                 [](const auto& a, const auto& b) { return a.contribution < b.contribution; });

        // Keep only top 5
        if (report.topContributors.size() > 5) report.topContributors.resize(5);
        if (report.topDetractors.size() > 5) report.topDetractors.resize(5);

    } catch (const std::exception& e) {
        logAnalysisWarning("Comprehensive report generation failed: " + std::string(e.what()));
    }

    return report;
}

// === UTILITY IMPLEMENTATIONS ===

double PerformanceAttributionEngine::calculateVolatility(const std::vector<double>& returns) {
    return PerformanceUtils::calculateStandardDeviation(returns);
}

double PerformanceAttributionEngine::calculateDownsideDeviation(const std::vector<double>& returns, double threshold) {
    std::vector<double> downsideReturns;
    for (double ret : returns) {
        if (ret < threshold) {
            downsideReturns.push_back(ret - threshold);
        }
    }

    if (downsideReturns.empty()) return 0.0;

    return PerformanceUtils::calculateStandardDeviation(downsideReturns);
}

std::vector<double> PerformanceAttributionEngine::calculateReturns(const std::vector<double>& prices) {
    std::vector<double> returns;

    for (size_t i = 1; i < prices.size(); ++i) {
        if (prices[i-1] != 0) {
            returns.push_back((prices[i] - prices[i-1]) / prices[i-1]);
        }
    }

    return returns;
}

std::vector<double> PerformanceAttributionEngine::calculateCumulativeReturns(const std::vector<double>& returns) {
    std::vector<double> cumulative;
    double cum = 1.0;

    cumulative.push_back(cum);
    for (double ret : returns) {
        cum *= (1.0 + ret);
        cumulative.push_back(cum);
    }

    return cumulative;
}

std::vector<double> PerformanceAttributionEngine::calculateActiveReturns(
    const std::vector<double>& portfolioReturns,
    const std::vector<double>& benchmarkReturns) {

    std::vector<double> activeReturns;

    size_t minSize = std::min(portfolioReturns.size(), benchmarkReturns.size());
    for (size_t i = 0; i < minSize; ++i) {
        activeReturns.push_back(portfolioReturns[i] - benchmarkReturns[i]);
    }

    return activeReturns;
}

int PerformanceAttributionEngine::calculateTradingDays(
    const std::chrono::system_clock::time_point& start,
    const std::chrono::system_clock::time_point& end) {

    auto duration = end - start;
    int totalDays = static_cast<int>(std::chrono::duration_cast<std::chrono::hours>(duration).count() / 24);
    return static_cast<int>(totalDays * 5.0 / 7.0); // Approximate trading days (5/7 of total)
}

// === PRIVATE METHOD IMPLEMENTATIONS ===

void PerformanceAttributionEngine::initializeBenchmarks() {
    // Initialize default crypto benchmark weights
    benchmarkWeights_["BTC"] = 0.60;
    benchmarkWeights_["ETH"] = 0.25;
    benchmarkWeights_["ADA"] = 0.05;
    benchmarkWeights_["DOT"] = 0.05;
    benchmarkWeights_["LINK"] = 0.05;
}

double PerformanceAttributionEngine::calculateMarketFactor(
    const std::vector<Position>& positions,
    const std::chrono::system_clock::time_point& startDate,
    const std::chrono::system_clock::time_point& endDate) {

    // Simplified market factor calculation
    double marketReturn = 0.0;
    for (const auto& position : positions) {
        marketReturn += position.getPnLPercentage() / 100.0;
    }
    return marketReturn / positions.size() * 0.8; // Assume 80% is market-driven
}

double PerformanceAttributionEngine::calculateSizeFactor(
    const std::vector<Position>& positions,
    const std::chrono::system_clock::time_point& startDate,
    const std::chrono::system_clock::time_point& endDate) {

    // Simplified size factor - larger positions tend to have different returns
    double sizeEffect = 0.0;
    for (const auto& position : positions) {
        double positionSize = position.getPositionValue();
        double sizeWeight = (positionSize > 50000) ? 1.2 : 0.8; // Large vs small
        sizeEffect += position.getPnLPercentage() / 100.0 * sizeWeight;
    }
    return sizeEffect / positions.size() * 0.1; // Assume 10% size effect
}

double PerformanceAttributionEngine::calculateMomentumFactor(
    const std::vector<Position>& positions,
    const std::chrono::system_clock::time_point& startDate,
    const std::chrono::system_clock::time_point& endDate) {

    // Simplified momentum factor
    return 0.02; // Assume 2% momentum contribution
}

double PerformanceAttributionEngine::calculateVolatilityFactor(
    const std::vector<Position>& positions,
    const std::chrono::system_clock::time_point& startDate,
    const std::chrono::system_clock::time_point& endDate) {

    // Simplified volatility factor
    double volEffect = 0.0;
    for (const auto& position : positions) {
        double vol = position.getVolatility();
        volEffect += position.getPnLPercentage() / 100.0 * vol;
    }
    return volEffect / positions.size() * 0.05; // Assume 5% vol effect
}

double PerformanceAttributionEngine::calculateAllocationEffect(
    const std::string& symbol,
    double portfolioWeight,
    double benchmarkWeight,
    double benchmarkReturn) {

    return (portfolioWeight - benchmarkWeight) * benchmarkReturn;
}

double PerformanceAttributionEngine::calculateSelectionEffect(
    const std::string& symbol,
    double benchmarkWeight,
    double assetReturn,
    double benchmarkReturn) {

    return benchmarkWeight * (assetReturn - benchmarkReturn);
}

double PerformanceAttributionEngine::calculateInteractionEffect(
    const std::string& symbol,
    double portfolioWeight,
    double benchmarkWeight,
    double assetReturn,
    double benchmarkReturn) {

    return (portfolioWeight - benchmarkWeight) * (assetReturn - benchmarkReturn);
}

bool PerformanceAttributionEngine::validateDateRange(
    const std::chrono::system_clock::time_point& start,
    const std::chrono::system_clock::time_point& end) {

    return start < end;
}

bool PerformanceAttributionEngine::validateReturnsData(const std::vector<double>& returns) {
    return !returns.empty() && std::all_of(returns.begin(), returns.end(),
                                          [](double ret) { return std::isfinite(ret); });
}

void PerformanceAttributionEngine::logAnalysisWarning(const std::string& warning) {
    std::cerr << "Performance Analysis Warning: " << warning << std::endl;
}

std::string PerformanceAttributionEngine::formatPerformanceNumber(double value, bool isPercent) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(isPercent ? 2 : 4);
    if (isPercent) {
        oss << (value * 100.0) << "%";
    } else {
        oss << value;
    }
    return oss.str();
}

// === PERFORMANCE UTILITIES IMPLEMENTATIONS ===

double PerformanceUtils::calculateMean(const std::vector<double>& values) {
    if (values.empty()) return 0.0;
    return std::accumulate(values.begin(), values.end(), 0.0) / values.size();
}

double PerformanceUtils::calculateStandardDeviation(const std::vector<double>& values) {
    if (values.size() < 2) return 0.0;

    double mean = calculateMean(values);
    double variance = 0.0;

    for (double val : values) {
        variance += std::pow(val - mean, 2);
    }

    variance /= (values.size() - 1); // Sample standard deviation
    return std::sqrt(variance);
}

double PerformanceUtils::calculateSharpeRatio(double excessReturn, double volatility) {
    return (volatility > 0) ? excessReturn / volatility : 0.0;
}

double PerformanceUtils::calculateSortinoRatio(double excessReturn, double downsideDeviation) {
    return (downsideDeviation > 0) ? excessReturn / downsideDeviation : 0.0;
}

double PerformanceUtils::calculateCalmarRatio(double annualReturn, double maxDrawdown) {
    return (maxDrawdown > 0) ? annualReturn / maxDrawdown : 0.0;
}

double PerformanceUtils::calculateTreynorRatio(double excessReturn, double beta) {
    return (beta != 0) ? excessReturn / beta : 0.0;
}

double PerformanceUtils::calculateMaxDrawdown(const std::vector<double>& cumulativeReturns) {
    if (cumulativeReturns.empty()) return 0.0;

    double maxDrawdown = 0.0;
    double peak = cumulativeReturns[0];

    for (double value : cumulativeReturns) {
        if (value > peak) {
            peak = value;
        } else {
            double drawdown = (peak - value) / peak;
            maxDrawdown = std::max(maxDrawdown, drawdown);
        }
    }

    return maxDrawdown;
}

// Additional PerformanceUtils implementations
double PerformanceUtils::calculateMedian(const std::vector<double>& values) {
    if (values.empty()) return 0.0;

    std::vector<double> sorted = values;
    std::sort(sorted.begin(), sorted.end());

    size_t n = sorted.size();
    if (n % 2 == 0) {
        return (sorted[n/2 - 1] + sorted[n/2]) / 2.0;
    } else {
        return sorted[n/2];
    }
}

double PerformanceUtils::calculateSkewness(const std::vector<double>& values) {
    if (values.size() < 3) return 0.0;

    double mean = calculateMean(values);
    double stdDev = calculateStandardDeviation(values);

    if (stdDev == 0.0) return 0.0;

    double skewness = 0.0;
    for (double val : values) {
        skewness += std::pow((val - mean) / stdDev, 3);
    }

    return skewness / values.size();
}

double PerformanceUtils::calculateKurtosis(const std::vector<double>& values) {
    if (values.size() < 4) return 0.0;

    double mean = calculateMean(values);
    double stdDev = calculateStandardDeviation(values);

    if (stdDev == 0.0) return 0.0;

    double kurtosis = 0.0;
    for (double val : values) {
        kurtosis += std::pow((val - mean) / stdDev, 4);
    }

    return (kurtosis / values.size()) - 3.0; // Excess kurtosis
}

double PerformanceUtils::calculateVaR(const std::vector<double>& returns, double confidenceLevel) {
    if (returns.empty()) return 0.0;

    std::vector<double> sorted = returns;
    std::sort(sorted.begin(), sorted.end());

    size_t index = static_cast<size_t>(confidenceLevel * returns.size());
    if (index >= returns.size()) index = returns.size() - 1;

    return -sorted[index]; // VaR is positive for losses
}

double PerformanceUtils::calculateExpectedShortfall(const std::vector<double>& returns, double confidenceLevel) {
    if (returns.empty()) return 0.0;

    std::vector<double> sorted = returns;
    std::sort(sorted.begin(), sorted.end());

    size_t cutoff = static_cast<size_t>(confidenceLevel * returns.size());
    if (cutoff == 0) cutoff = 1;

    double sum = 0.0;
    for (size_t i = 0; i < cutoff; ++i) {
        sum += sorted[i];
    }

    return -sum / cutoff; // Expected shortfall is positive for losses
}

std::vector<double> PerformanceUtils::calculateMovingAverage(const std::vector<double>& values, int window) {
    std::vector<double> ma;
    if (values.size() < static_cast<size_t>(window)) return ma;

    for (size_t i = window - 1; i < values.size(); ++i) {
        double sum = 0.0;
        for (int j = 0; j < window; ++j) {
            sum += values[i - j];
        }
        ma.push_back(sum / window);
    }

    return ma;
}

std::vector<double> PerformanceUtils::calculateExponentialMovingAverage(const std::vector<double>& values, double alpha) {
    std::vector<double> ema;
    if (values.empty()) return ema;

    ema.push_back(values[0]);
    for (size_t i = 1; i < values.size(); ++i) {
        double newEma = alpha * values[i] + (1 - alpha) * ema.back();
        ema.push_back(newEma);
    }

    return ema;
}

double PerformanceUtils::calculateAutoCorrelation(const std::vector<double>& values, int lag) {
    if (values.size() <= static_cast<size_t>(lag)) return 0.0;

    std::vector<double> x(values.begin(), values.end() - lag);
    std::vector<double> y(values.begin() + lag, values.end());

    double meanX = calculateMean(x);
    double meanY = calculateMean(y);

    double numerator = 0.0, denomX = 0.0, denomY = 0.0;

    for (size_t i = 0; i < x.size(); ++i) {
        double dx = x[i] - meanX;
        double dy = y[i] - meanY;

        numerator += dx * dy;
        denomX += dx * dx;
        denomY += dy * dy;
    }

    double denom = std::sqrt(denomX * denomY);
    return (denom > 0) ? numerator / denom : 0.0;
}

double PerformanceUtils::calculateActiveReturn(double portfolioReturn, double benchmarkReturn) {
    return portfolioReturn - benchmarkReturn;
}

double PerformanceUtils::calculateRelativeReturn(double portfolioReturn, double benchmarkReturn) {
    return (benchmarkReturn != 0) ? (portfolioReturn / benchmarkReturn) - 1.0 : 0.0;
}

std::vector<double> PerformanceUtils::calculateRollingCorrelation(
    const std::vector<double>& x,
    const std::vector<double>& y,
    int window) {

    std::vector<double> rollingCorr;
    if (x.size() != y.size() || x.size() < static_cast<size_t>(window)) return rollingCorr;

    for (size_t i = window - 1; i < x.size(); ++i) {
        std::vector<double> xWindow(x.begin() + i - window + 1, x.begin() + i + 1);
        std::vector<double> yWindow(y.begin() + i - window + 1, y.begin() + i + 1);

        double meanX = calculateMean(xWindow);
        double meanY = calculateMean(yWindow);

        double numerator = 0.0, denomX = 0.0, denomY = 0.0;

        for (size_t j = 0; j < xWindow.size(); ++j) {
            double dx = xWindow[j] - meanX;
            double dy = yWindow[j] - meanY;

            numerator += dx * dy;
            denomX += dx * dx;
            denomY += dy * dy;
        }

        double denom = std::sqrt(denomX * denomY);
        rollingCorr.push_back((denom > 0) ? numerator / denom : 0.0);
    }

    return rollingCorr;
}

} // namespace Analytics
} // namespace CryptoClaude

// === Additional method implementations ===

namespace CryptoClaude {
namespace Analytics {

// Data Management Methods
void PerformanceAttributionEngine::updateHistoricalPrices(const std::string& symbol, const std::vector<MarketData>& prices) {
    historicalPrices_[symbol] = prices;
}

void PerformanceAttributionEngine::updateHistoricalSentiment(const std::string& symbol, const std::vector<SentimentData>& sentiment) {
    historicalSentiment_[symbol] = sentiment;
}

void PerformanceAttributionEngine::setBenchmarkWeights(const std::map<std::string, double>& weights) {
    benchmarkWeights_ = weights;
}

void PerformanceAttributionEngine::updateBenchmarkWeight(const std::string& symbol, double weight) {
    benchmarkWeights_[symbol] = weight;
}

void PerformanceAttributionEngine::addPerformancePeriod(const PeriodPerformance& performance) {
    performanceHistory_.push_back(performance);
}

std::vector<PeriodPerformance> PerformanceAttributionEngine::getPerformanceHistory(int periods) const {
    if (periods < 0 || periods >= static_cast<int>(performanceHistory_.size())) {
        return performanceHistory_;
    }

    return std::vector<PeriodPerformance>(
        performanceHistory_.end() - periods,
        performanceHistory_.end()
    );
}

// Additional utility methods
std::vector<PerformanceContribution> PerformanceAttributionEngine::calculateBenchmarkAttribution(
    const std::vector<double>& portfolioReturns,
    const std::vector<double>& benchmarkReturns,
    const std::vector<std::string>& attributionFactors) {

    std::vector<PerformanceContribution> contributions;

    if (portfolioReturns.size() != benchmarkReturns.size() || portfolioReturns.empty()) {
        logAnalysisWarning("Mismatched return data for benchmark attribution");
        return contributions;
    }

    try {
        double totalActiveReturn = 0.0;
        for (size_t i = 0; i < portfolioReturns.size(); ++i) {
            totalActiveReturn += portfolioReturns[i] - benchmarkReturns[i];
        }
        totalActiveReturn /= portfolioReturns.size();

        for (const std::string& factor : attributionFactors) {
            PerformanceContribution contrib;
            contrib.factor = factor;
            contrib.description = "Attribution from " + factor + " decisions";
            contrib.confidence = 0.7;

            if (factor == "Selection") {
                contrib.contribution = totalActiveReturn * 0.6; // 60% from selection
            } else if (factor == "Allocation") {
                contrib.contribution = totalActiveReturn * 0.3; // 30% from allocation
            } else if (factor == "Interaction") {
                contrib.contribution = totalActiveReturn * 0.1; // 10% from interaction
            } else {
                contrib.contribution = 0.0;
            }

            contrib.contributionPercent = (totalActiveReturn != 0) ?
                100.0 * contrib.contribution / totalActiveReturn : 0.0;

            contributions.push_back(contrib);
        }

    } catch (const std::exception& e) {
        logAnalysisWarning("Benchmark attribution calculation failed: " + std::string(e.what()));
    }

    return contributions;
}

double PerformanceAttributionEngine::calculateTrackingError(
    const std::vector<double>& portfolioReturns,
    const std::vector<double>& benchmarkReturns) {

    std::vector<double> activeReturns = calculateActiveReturns(portfolioReturns, benchmarkReturns);
    return PerformanceUtils::calculateStandardDeviation(activeReturns);
}

double PerformanceAttributionEngine::calculateInformationRatio(
    const std::vector<double>& portfolioReturns,
    const std::vector<double>& benchmarkReturns) {

    std::vector<double> activeReturns = calculateActiveReturns(portfolioReturns, benchmarkReturns);
    double meanActiveReturn = PerformanceUtils::calculateMean(activeReturns);
    double trackingError = PerformanceUtils::calculateStandardDeviation(activeReturns);

    return (trackingError > 0) ? meanActiveReturn / trackingError : 0.0;
}

double PerformanceAttributionEngine::calculateBeta(
    const std::vector<double>& portfolioReturns,
    const std::vector<double>& marketReturns) {

    if (portfolioReturns.size() != marketReturns.size() || portfolioReturns.empty()) {
        return 1.0; // Default beta
    }

    double portfolioMean = PerformanceUtils::calculateMean(portfolioReturns);
    double marketMean = PerformanceUtils::calculateMean(marketReturns);

    double covariance = 0.0;
    double marketVariance = 0.0;

    for (size_t i = 0; i < portfolioReturns.size(); ++i) {
        double portfolioDev = portfolioReturns[i] - portfolioMean;
        double marketDev = marketReturns[i] - marketMean;

        covariance += portfolioDev * marketDev;
        marketVariance += marketDev * marketDev;
    }

    covariance /= (portfolioReturns.size() - 1);
    marketVariance /= (portfolioReturns.size() - 1);

    return (marketVariance > 0) ? covariance / marketVariance : 1.0;
}

double PerformanceAttributionEngine::calculateCorrelation(
    const std::vector<double>& x,
    const std::vector<double>& y) {

    if (x.size() != y.size() || x.empty()) return 0.0;

    double meanX = PerformanceUtils::calculateMean(x);
    double meanY = PerformanceUtils::calculateMean(y);

    double numerator = 0.0;
    double denomX = 0.0;
    double denomY = 0.0;

    for (size_t i = 0; i < x.size(); ++i) {
        double dx = x[i] - meanX;
        double dy = y[i] - meanY;

        numerator += dx * dy;
        denomX += dx * dx;
        denomY += dy * dy;
    }

    double denom = std::sqrt(denomX * denomY);
    return (denom > 0) ? numerator / denom : 0.0;
}

std::vector<std::chrono::system_clock::time_point> PerformanceAttributionEngine::generateDateRange(
    const std::chrono::system_clock::time_point& start,
    const std::chrono::system_clock::time_point& end,
    int intervalDays) {

    std::vector<std::chrono::system_clock::time_point> dates;

    auto current = start;
    while (current <= end) {
        dates.push_back(current);
        current += std::chrono::hours(24 * intervalDays);
    }

    return dates;
}

} // namespace Analytics
} // namespace CryptoClaude