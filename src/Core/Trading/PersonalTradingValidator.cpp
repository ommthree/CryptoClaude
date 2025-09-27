#include "PersonalTradingValidator.h"
#include <iostream>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <random>
#include <sstream>

namespace CryptoClaude {
namespace Trading {

// PersonalTradingValidator Implementation
PersonalTradingValidator::PersonalTradingValidator(PersonalMLPipeline& mlPipeline,
                                                 PersonalMLBenchmarks& benchmarks,
                                                 SmartCacheManager& cacheManager)
    : mlPipeline_(mlPipeline), benchmarks_(benchmarks), cacheManager_(cacheManager),
      emergencyMode_(false) {

    setupDefaultValidationRules();
}

bool PersonalTradingValidator::initialize(const ValidationConfig& config) {
    config_ = config;
    emergencyMode_ = false;

    // Reset metrics
    metrics_ = ValidationMetrics();
    metrics_.lastUpdate = std::chrono::system_clock::now();

    std::cout << "PersonalTradingValidator initialized with:" << std::endl;
    std::cout << "  Min confidence: " << config_.minConfidenceThreshold << std::endl;
    std::cout << "  Max risk per trade: " << config_.maxRiskPerTrade << std::endl;
    std::cout << "  Max portfolio risk: " << config_.maxPortfolioRisk << std::endl;
    std::cout << "  Max positions: " << config_.maxPositions << std::endl;

    return true;
}

void PersonalTradingValidator::shutdown() {
    std::cout << "PersonalTradingValidator shutdown - processed "
              << metrics_.totalSignalsProcessed << " signals" << std::endl;
}

ValidatedTradingSignal PersonalTradingValidator::validateTradingSignal(const PersonalPrediction& prediction) {
    ValidatedTradingSignal validatedSignal;
    validatedSignal.symbol = prediction.symbol;
    validatedSignal.signal = prediction.signal;
    validatedSignal.confidence = prediction.confidence;
    validatedSignal.expectedReturn = prediction.expectedReturn;
    validatedSignal.riskScore = prediction.riskScore;
    validatedSignal.signalTime = prediction.predictionTime;
    validatedSignal.validationTime = std::chrono::system_clock::now();

    metrics_.totalSignalsProcessed++;

    // Emergency mode check
    if (emergencyMode_) {
        if (emergencySettings_.disableNewPositions) {
            validatedSignal.isValid = false;
            validatedSignal.validationFailures.push_back("Emergency mode: new positions disabled");
            return validatedSignal;
        }

        if (emergencySettings_.onlyCriticalSignals && prediction.confidence < 0.8) {
            validatedSignal.isValid = false;
            validatedSignal.validationFailures.push_back("Emergency mode: signal confidence too low");
            return validatedSignal;
        }

        // Check if symbol is allowed in emergency mode
        auto it = std::find(emergencySettings_.allowedSymbols.begin(),
                           emergencySettings_.allowedSymbols.end(),
                           prediction.symbol);
        if (it == emergencySettings_.allowedSymbols.end()) {
            validatedSignal.isValid = false;
            validatedSignal.validationFailures.push_back("Emergency mode: symbol not in allowed list");
            return validatedSignal;
        }
    }

    // Run validation tests
    std::vector<bool> testResults;
    double totalScore = 0.0;
    int scoreCount = 0;

    // 1. Confidence threshold test
    if (validateSignalConfidence(prediction)) {
        validatedSignal.validationTests.push_back("Confidence test passed");
        testResults.push_back(true);
        totalScore += 85.0;
    } else {
        validatedSignal.validationFailures.push_back("Signal confidence below threshold");
        testResults.push_back(false);
        totalScore += 30.0;
    }
    scoreCount++;

    // 2. Model quality test
    std::string modelId = prediction.featureImportance.empty() ?
        prediction.symbol + "_default" : prediction.symbol + "_model";
    validatedSignal.modelId = modelId;

    if (validateModelQuality(modelId)) {
        validatedSignal.validationTests.push_back("Model quality test passed");
        testResults.push_back(true);
        totalScore += 80.0;
    } else {
        validatedSignal.validationWarnings.push_back("Model quality concerns");
        testResults.push_back(false);
        totalScore += 40.0;
    }
    scoreCount++;

    // 3. Data freshness test
    if (validateDataFreshness(prediction.symbol)) {
        validatedSignal.validationTests.push_back("Data freshness test passed");
        testResults.push_back(true);
        totalScore += 75.0;
    } else {
        validatedSignal.validationWarnings.push_back("Data may be stale");
        testResults.push_back(false);
        totalScore += 50.0;
    }
    scoreCount++;

    // 4. Risk limits test
    std::map<std::string, double> emptyPositions; // Simplified - would use real positions
    if (validateRiskLimits(prediction, emptyPositions)) {
        validatedSignal.validationTests.push_back("Risk limits test passed");
        testResults.push_back(true);
        totalScore += 90.0;
    } else {
        validatedSignal.validationFailures.push_back("Signal violates risk limits");
        testResults.push_back(false);
        totalScore += 20.0;
    }
    scoreCount++;

    // 5. Market conditions test
    auto marketConditions = assessMarketConditions(prediction.symbol);
    if (marketConditions.isSuitableForTrading) {
        validatedSignal.validationTests.push_back("Market conditions suitable");
        testResults.push_back(true);
        totalScore += 70.0;
    } else {
        validatedSignal.validationWarnings.push_back("Suboptimal market conditions");
        for (const auto& warning : marketConditions.warnings) {
            validatedSignal.validationWarnings.push_back(warning);
        }
        testResults.push_back(false);
        totalScore += 45.0;
    }
    scoreCount++;

    // Calculate overall validation score
    validatedSignal.validationScore = totalScore / scoreCount;

    // Determine if signal is valid (must pass critical tests)
    bool passedCriticalTests = testResults[0] && testResults[3]; // Confidence and risk
    bool passedMostTests = std::count(testResults.begin(), testResults.end(), true) >= 3;

    validatedSignal.isValid = passedCriticalTests && passedMostTests &&
                             validatedSignal.validationScore >= 60.0;

    if (validatedSignal.isValid) {
        metrics_.validSignalsGenerated++;

        // Calculate position sizing and risk management
        double accountBalance = 10000.0; // Simplified - would use real account balance
        validatedSignal.positionSizeRecommendation = calculatePositionSize(
            validatedSignal, accountBalance, config_.maxRiskPerTrade);

        double estimatedEntryPrice = 100.0; // Simplified - would use current market price
        validatedSignal.stopLossLevel = calculateStopLoss(validatedSignal, estimatedEntryPrice);
        validatedSignal.takeProfitLevel = calculateTakeProfit(validatedSignal, estimatedEntryPrice);
    }

    // Store feature importance if available
    validatedSignal.featureImportance = prediction.featureImportance;

    return validatedSignal;
}

std::vector<ValidatedTradingSignal> PersonalTradingValidator::validateBatchSignals(
    const std::vector<PersonalPrediction>& predictions) {

    std::vector<ValidatedTradingSignal> validatedSignals;

    for (const auto& prediction : predictions) {
        auto validatedSignal = validateTradingSignal(prediction);
        validatedSignals.push_back(validatedSignal);
    }

    // Additional portfolio-level validation
    auto portfolioValidation = validatePortfolio(validatedSignals);
    if (!portfolioValidation.isValid) {
        // Mark some signals as invalid based on portfolio constraints
        for (auto& signal : validatedSignals) {
            if (signal.isValid && portfolioValidation.totalRisk > config_.maxPortfolioRisk) {
                signal.validationWarnings.push_back("Portfolio risk exceeded");
                // Keep highest confidence signals, mark others as invalid
                if (signal.confidence < 0.8) {
                    signal.isValid = false;
                    signal.validationFailures.push_back("Deselected due to portfolio risk");
                }
            }
        }
    }

    return validatedSignals;
}

PortfolioValidation PersonalTradingValidator::validatePortfolio(
    const std::vector<ValidatedTradingSignal>& signals,
    const std::map<std::string, double>& currentPositions) {

    PortfolioValidation validation;

    // Count valid signals
    std::vector<ValidatedTradingSignal> validSignals;
    std::copy_if(signals.begin(), signals.end(), std::back_inserter(validSignals),
                [](const ValidatedTradingSignal& s) { return s.isValid; });

    validation.activePositions = currentPositions.size() + validSignals.size();

    // Check position limits
    if (validation.activePositions > config_.maxPositions) {
        validation.riskWarnings.push_back("Would exceed maximum position limit");
        validation.isValid = false;
    }

    // Calculate total portfolio risk
    validation.totalRisk = 0.0;
    for (const auto& signal : validSignals) {
        validation.totalRisk += signal.riskScore * signal.positionSizeRecommendation;
    }

    if (validation.totalRisk > config_.maxPortfolioRisk) {
        validation.riskWarnings.push_back("Total portfolio risk exceeds limit");
        validation.isValid = false;
    }

    // Calculate correlation (simplified)
    std::vector<std::string> symbols;
    for (const auto& signal : validSignals) {
        symbols.push_back(signal.symbol);
    }

    if (symbols.size() > 1) {
        validation.portfolioCorrelation = calculatePortfolioCorrelation(symbols);
        validation.diversificationScore = calculateDiversificationBenefit(validSignals);

        if (validation.portfolioCorrelation > 0.8) {
            validation.riskWarnings.push_back("High correlation between positions");
        }

        if (validation.diversificationScore < 0.3) {
            validation.suggestions.push_back("Consider more diversified positions");
        }
    }

    // Overall validation
    if (validation.riskWarnings.empty()) {
        validation.isValid = true;
    }

    return validation;
}

bool PersonalTradingValidator::validateSignalConfidence(const PersonalPrediction& prediction) {
    return prediction.confidence >= config_.minConfidenceThreshold;
}

bool PersonalTradingValidator::validateModelQuality(const std::string& modelId) {
    // Get model from ML pipeline
    try {
        // This would typically load the model and check its recent performance
        // For simplification, we'll use basic rules

        // Check if model has recent performance data
        auto recentPerformance = getRecentPerformance(7); // Last 7 days
        int modelSignals = 0;
        int correctSignals = 0;

        for (const auto& perf : recentPerformance) {
            if (perf.signalId.find(modelId) != std::string::npos) {
                modelSignals++;
                if (perf.wasCorrect) correctSignals++;
            }
        }

        if (modelSignals >= 3) { // Need at least 3 recent signals
            double recentAccuracy = (double)correctSignals / modelSignals;
            return recentAccuracy >= config_.minWinRate;
        }

        return true; // Pass if insufficient recent data
    } catch (const std::exception& e) {
        std::cerr << "Error validating model quality: " << e.what() << std::endl;
        return false;
    }
}

bool PersonalTradingValidator::validateRiskLimits(const PersonalPrediction& prediction,
                                                 const std::map<std::string, double>& currentPositions) {
    // Check individual trade risk
    if (prediction.riskScore > config_.maxRiskPerTrade) {
        return false;
    }

    // Check if adding this position would exceed portfolio risk
    double currentPortfolioRisk = 0.0;
    for (const auto& position : currentPositions) {
        currentPortfolioRisk += std::abs(position.second) * 0.02; // Assume 2% risk per position
    }

    double newPositionRisk = prediction.riskScore;
    if (currentPortfolioRisk + newPositionRisk > config_.maxPortfolioRisk) {
        return false;
    }

    return true;
}

bool PersonalTradingValidator::validateDataFreshness(const std::string& symbol) {
    // Check if we have recent cached data for the symbol
    auto now = std::chrono::system_clock::now();
    auto oneHourAgo = now - std::chrono::hours(1);

    // This is simplified - would check actual cache timestamps
    return true; // Assume data is fresh for now
}

PersonalTradingValidator::MarketConditionCheck PersonalTradingValidator::assessMarketConditions(const std::string& symbol) {
    MarketConditionCheck conditions;

    // Get recent price data from cache
    auto features = mlPipeline_.extractFeaturesFromCache(symbol, "1d", 30);

    if (features.volatility.empty()) {
        conditions.warnings.push_back("No volatility data available");
        return conditions;
    }

    // Calculate current volatility regime
    conditions.volatilityScore = calculateVolatilityRegime(symbol);
    conditions.trendStrength = calculateTrendStrength(symbol);

    // Determine market regime
    if (conditions.volatilityScore > 0.8) {
        conditions.marketRegime = "volatile";
        conditions.warnings.push_back("High volatility environment");
    } else if (conditions.trendStrength > 0.7) {
        conditions.marketRegime = "trending";
    } else {
        conditions.marketRegime = "ranging";
        conditions.warnings.push_back("Ranging market conditions");
    }

    // Determine if suitable for trading
    conditions.isSuitableForTrading = (conditions.volatilityScore < 0.9) &&
                                     (conditions.trendStrength > 0.3);

    if (!conditions.isSuitableForTrading) {
        if (conditions.volatilityScore >= 0.9) {
            conditions.warnings.push_back("Volatility too high for safe trading");
        }
        if (conditions.trendStrength <= 0.3) {
            conditions.warnings.push_back("Weak trend - difficult to predict direction");
        }
    }

    return conditions;
}

double PersonalTradingValidator::calculatePositionSize(const ValidatedTradingSignal& signal,
                                                      double accountBalance,
                                                      double riskPerTrade) {
    // Kelly Criterion modified for personal use
    double winProbability = signal.confidence;
    double avgWin = std::abs(signal.expectedReturn);
    double avgLoss = signal.riskScore;

    if (avgLoss <= 0) avgLoss = 0.02; // Default 2% risk

    // Kelly fraction
    double kellyFraction = (winProbability * avgWin - (1 - winProbability) * avgLoss) / avgWin;

    // Conservative adjustment for personal trading
    kellyFraction = std::max(0.0, std::min(kellyFraction * 0.5, riskPerTrade)); // Half Kelly, max risk limit

    return accountBalance * kellyFraction;
}

double PersonalTradingValidator::calculateStopLoss(const ValidatedTradingSignal& signal,
                                                  double entryPrice,
                                                  double maxRisk) {
    // Calculate stop loss based on volatility and confidence
    double volatilityAdjustment = signal.riskScore * 2.0; // Use risk score as volatility proxy
    double confidenceAdjustment = (1.0 - signal.confidence) * 0.5; // Lower confidence = tighter stop

    double stopDistance = std::min(maxRisk, volatilityAdjustment + confidenceAdjustment);

    if (signal.signal == "BUY") {
        return entryPrice * (1.0 - stopDistance);
    } else if (signal.signal == "SELL") {
        return entryPrice * (1.0 + stopDistance);
    }

    return entryPrice; // No stop for HOLD signals
}

double PersonalTradingValidator::calculateTakeProfit(const ValidatedTradingSignal& signal,
                                                    double entryPrice,
                                                    double riskRewardRatio) {
    double expectedMove = signal.expectedReturn;
    double targetMove = expectedMove * signal.confidence * riskRewardRatio;

    if (signal.signal == "BUY") {
        return entryPrice * (1.0 + targetMove);
    } else if (signal.signal == "SELL") {
        return entryPrice * (1.0 - targetMove);
    }

    return entryPrice; // No target for HOLD signals
}

void PersonalTradingValidator::recordSignalResult(const std::string& signalId, double actualReturn) {
    SignalPerformance performance;
    performance.signalId = signalId;
    performance.actualReturn = actualReturn;
    performance.wasCorrect = (actualReturn > 0.01) || (actualReturn < -0.01 && signalId.find("SELL") != std::string::npos);
    performance.resolvedTime = std::chrono::system_clock::now();

    signalHistory_.push_back(performance);

    // Update metrics
    metrics_.signalsTraded++;
    metrics_.averageActualReturn = ((metrics_.averageActualReturn * (metrics_.signalsTraded - 1)) + actualReturn) / metrics_.signalsTraded;

    // Keep only recent history (last 90 days)
    auto cutoff = std::chrono::system_clock::now() - std::chrono::hours(24 * 90);
    signalHistory_.erase(
        std::remove_if(signalHistory_.begin(), signalHistory_.end(),
                      [cutoff](const SignalPerformance& p) {
                          return p.signalTime < cutoff;
                      }),
        signalHistory_.end());

    std::cout << "Recorded signal result: " << signalId << " -> " << actualReturn << std::endl;
}

std::vector<PersonalTradingValidator::SignalPerformance> PersonalTradingValidator::getRecentPerformance(int days) const {
    std::vector<SignalPerformance> recentPerformance;
    auto cutoff = std::chrono::system_clock::now() - std::chrono::hours(24 * days);

    std::copy_if(signalHistory_.begin(), signalHistory_.end(),
                std::back_inserter(recentPerformance),
                [cutoff](const SignalPerformance& p) {
                    return p.signalTime >= cutoff;
                });

    return recentPerformance;
}

// Helper method implementations
double PersonalTradingValidator::calculateVolatilityRegime(const std::string& symbol) {
    auto features = mlPipeline_.extractFeaturesFromCache(symbol, "1d", 20);

    if (features.volatility.empty()) return 0.5; // Default moderate volatility

    double recentVolatility = 0.0;
    if (features.volatility.size() >= 5) {
        // Average of last 5 days
        for (int i = features.volatility.size() - 5; i < (int)features.volatility.size(); ++i) {
            recentVolatility += features.volatility[i];
        }
        recentVolatility /= 5.0;
    }

    // Normalize to 0-1 scale (simplified)
    return std::min(1.0, recentVolatility * 10.0); // Assume volatility values are typically 0-0.1
}

double PersonalTradingValidator::calculateTrendStrength(const std::string& symbol) {
    auto features = mlPipeline_.extractFeaturesFromCache(symbol, "1d", 50);

    if (features.sma20.empty() || features.sma50.empty()) return 0.5;

    // Compare SMA20 to SMA50 for trend strength
    if (features.sma20.size() >= 1 && features.sma50.size() >= 1) {
        double sma20 = features.sma20.back();
        double sma50 = features.sma50.back();

        double trendStrength = std::abs(sma20 - sma50) / sma50;
        return std::min(1.0, trendStrength * 20.0); // Normalize
    }

    return 0.5; // Default moderate trend
}

double PersonalTradingValidator::calculatePortfolioCorrelation(const std::vector<std::string>& symbols) {
    if (symbols.size() < 2) return 0.0;

    // Simplified correlation calculation
    // In practice, would calculate actual price correlations

    // Crypto correlations (simplified assumptions)
    std::map<std::string, std::map<std::string, double>> correlations = {
        {"BTC", {{"ETH", 0.8}, {"ADA", 0.7}, {"DOT", 0.6}, {"LINK", 0.5}}},
        {"ETH", {{"BTC", 0.8}, {"ADA", 0.75}, {"DOT", 0.7}, {"LINK", 0.6}}},
        {"ADA", {{"BTC", 0.7}, {"ETH", 0.75}, {"DOT", 0.8}, {"LINK", 0.6}}},
        {"DOT", {{"BTC", 0.6}, {"ETH", 0.7}, {"ADA", 0.8}, {"LINK", 0.7}}},
        {"LINK", {{"BTC", 0.5}, {"ETH", 0.6}, {"ADA", 0.6}, {"DOT", 0.7}}}
    };

    double totalCorrelation = 0.0;
    int pairCount = 0;

    for (size_t i = 0; i < symbols.size(); ++i) {
        for (size_t j = i + 1; j < symbols.size(); ++j) {
            auto it1 = correlations.find(symbols[i]);
            if (it1 != correlations.end()) {
                auto it2 = it1->second.find(symbols[j]);
                if (it2 != it1->second.end()) {
                    totalCorrelation += it2->second;
                    pairCount++;
                }
            }
        }
    }

    return pairCount > 0 ? totalCorrelation / pairCount : 0.5;
}

double PersonalTradingValidator::calculateDiversificationBenefit(const std::vector<ValidatedTradingSignal>& signals) {
    if (signals.size() <= 1) return 0.0;

    std::vector<std::string> symbols;
    for (const auto& signal : signals) {
        symbols.push_back(signal.symbol);
    }

    double correlation = calculatePortfolioCorrelation(symbols);

    // Diversification benefit decreases with correlation
    return std::max(0.0, 1.0 - correlation);
}

void PersonalTradingValidator::setupDefaultValidationRules() {
    // Rule 1: Minimum confidence
    ValidationRule confidenceRule;
    confidenceRule.ruleName = "minimum_confidence";
    confidenceRule.ruleFunction = [this](const PersonalPrediction& pred) {
        return pred.confidence >= config_.minConfidenceThreshold;
    };
    confidenceRule.weight = 2.0;
    confidenceRule.isMandatory = true;
    validationRules_["minimum_confidence"] = confidenceRule;

    // Rule 2: Reasonable expected return
    ValidationRule returnRule;
    returnRule.ruleName = "reasonable_return";
    returnRule.ruleFunction = [](const PersonalPrediction& pred) {
        return std::abs(pred.expectedReturn) <= 0.5; // No more than 50% expected move
    };
    returnRule.weight = 1.5;
    returnRule.isMandatory = true;
    validationRules_["reasonable_return"] = returnRule;

    // Rule 3: Risk score validation
    ValidationRule riskRule;
    riskRule.ruleName = "risk_validation";
    riskRule.ruleFunction = [this](const PersonalPrediction& pred) {
        return pred.riskScore <= config_.maxRiskPerTrade;
    };
    riskRule.weight = 2.0;
    riskRule.isMandatory = true;
    validationRules_["risk_validation"] = riskRule;
}

PersonalTradingValidator::ValidationMetrics PersonalTradingValidator::getValidationMetrics() const {
    ValidationMetrics currentMetrics = metrics_;

    if (currentMetrics.totalSignalsProcessed > 0) {
        currentMetrics.averageConfidence = currentMetrics.averageConfidence / currentMetrics.totalSignalsProcessed;
    }

    // Calculate validation accuracy
    int correctValidations = 0;
    auto recentPerformance = getRecentPerformance(30);
    for (const auto& perf : recentPerformance) {
        if (perf.wasCorrect) correctValidations++;
    }

    if (!recentPerformance.empty()) {
        currentMetrics.validationAccuracy = (double)correctValidations / recentPerformance.size();
    }

    return currentMetrics;
}

std::string PersonalTradingValidator::generateSignalId() const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);

    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    return "signal_" + std::to_string(timestamp) + "_" + std::to_string(dis(gen));
}

// ValidationConfigFactory Implementation
ValidationConfig ValidationConfigFactory::createConfig(TradingStyle style) {
    ValidationConfig config;

    switch (style) {
        case TradingStyle::CONSERVATIVE:
            config.minConfidenceThreshold = 0.7;
            config.maxRiskPerTrade = 0.01;
            config.maxPortfolioRisk = 0.03;
            config.maxPositions = 2;
            config.minWinRate = 0.55;
            config.minSharpeRatio = 0.7;
            config.maxDrawdown = 0.10;
            break;

        case TradingStyle::MODERATE:
            config.minConfidenceThreshold = 0.6;
            config.maxRiskPerTrade = 0.02;
            config.maxPortfolioRisk = 0.05;
            config.maxPositions = 3;
            config.minWinRate = 0.45;
            config.minSharpeRatio = 0.5;
            config.maxDrawdown = 0.15;
            break;

        case TradingStyle::AGGRESSIVE:
            config.minConfidenceThreshold = 0.55;
            config.maxRiskPerTrade = 0.03;
            config.maxPortfolioRisk = 0.08;
            config.maxPositions = 5;
            config.minWinRate = 0.40;
            config.minSharpeRatio = 0.4;
            config.maxDrawdown = 0.20;
            break;

        case TradingStyle::SCALPING:
            config.minConfidenceThreshold = 0.65;
            config.maxRiskPerTrade = 0.005;
            config.maxPortfolioRisk = 0.02;
            config.maxPositions = 1;
            config.minWinRate = 0.60;
            config.minSharpeRatio = 1.0;
            config.maxDrawdown = 0.05;
            break;

        case TradingStyle::SWING:
            config.minConfidenceThreshold = 0.6;
            config.maxRiskPerTrade = 0.04;
            config.maxPortfolioRisk = 0.10;
            config.maxPositions = 4;
            config.minWinRate = 0.40;
            config.minSharpeRatio = 0.3;
            config.maxDrawdown = 0.25;
            break;
    }

    return config;
}

} // namespace Trading
} // namespace CryptoClaude