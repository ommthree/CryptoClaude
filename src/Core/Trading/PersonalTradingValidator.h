#pragma once

#include "../ML/PersonalMLPipeline.h"
#include "../ML/PersonalMLBenchmarks.h"
#include "../Data/SmartCacheManager.h"
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <memory>

namespace CryptoClaude {
namespace Trading {

using namespace CryptoClaude::ML;
using namespace CryptoClaude::Data;

// Trading signal validation configuration
struct ValidationConfig {
    // Signal validation parameters
    double minConfidenceThreshold;     // Minimum signal confidence
    double maxRiskPerTrade;           // Maximum risk per individual trade
    double maxPortfolioRisk;          // Maximum total portfolio risk
    int maxPositions;                 // Maximum simultaneous positions

    // Performance requirements
    double minWinRate;                // Minimum acceptable win rate
    double minSharpeRatio;            // Minimum Sharpe ratio
    double maxDrawdown;               // Maximum acceptable drawdown

    // Time-based validation
    int validationPeriodDays;         // Period for signal validation
    int minSignalsForValidation;      // Minimum signals needed
    bool requireRecentPerformance;    // Must have recent good performance

    ValidationConfig()
        : minConfidenceThreshold(0.6), maxRiskPerTrade(0.02), maxPortfolioRisk(0.05),
          maxPositions(3), minWinRate(0.45), minSharpeRatio(0.5), maxDrawdown(0.15),
          validationPeriodDays(30), minSignalsForValidation(5), requireRecentPerformance(true) {}
};

// Trading signal with validation metadata
struct ValidatedTradingSignal {
    // Basic signal information
    std::string symbol;
    std::string signal;              // "BUY", "SELL", "HOLD"
    double confidence;               // 0.0 to 1.0
    double expectedReturn;
    double riskScore;

    // Validation results
    bool isValid;                    // Passed all validation tests
    double validationScore;          // Overall validation score (0-100)
    std::vector<std::string> validationTests;    // Tests that passed
    std::vector<std::string> validationWarnings; // Warning messages
    std::vector<std::string> validationFailures; // Failed validations

    // Risk assessment
    double positionSizeRecommendation; // Recommended position size
    double stopLossLevel;              // Recommended stop loss
    double takeProfitLevel;            // Recommended take profit

    // Supporting information
    std::string modelId;               // Model used for prediction
    std::map<std::string, double> featureImportance;
    std::chrono::system_clock::time_point signalTime;
    std::chrono::system_clock::time_point validationTime;

    ValidatedTradingSignal()
        : confidence(0.0), expectedReturn(0.0), riskScore(0.0), isValid(false),
          validationScore(0.0), positionSizeRecommendation(0.0), stopLossLevel(0.0),
          takeProfitLevel(0.0) {
        signalTime = std::chrono::system_clock::now();
        validationTime = signalTime;
    }
};

// Portfolio-level validation result
struct PortfolioValidation {
    bool isValid;
    double totalRisk;                 // Total portfolio risk
    int activePositions;              // Current active positions
    double portfolioCorrelation;      // Inter-asset correlation
    double diversificationScore;      // Portfolio diversification

    std::vector<std::string> riskWarnings;
    std::vector<std::string> suggestions;

    PortfolioValidation() : isValid(false), totalRisk(0.0), activePositions(0),
                           portfolioCorrelation(0.0), diversificationScore(0.0) {}
};

// Signal performance tracking
struct SignalPerformance {
    std::string signalId;
    std::string symbol;
    std::string originalSignal;
    double originalConfidence;
    double actualReturn;
    bool wasCorrect;
    std::chrono::system_clock::time_point signalTime;
    std::chrono::system_clock::time_point resolvedTime;

    SignalPerformance() : originalConfidence(0.0), actualReturn(0.0),
                         wasCorrect(false) {}
};

// Personal trading signal validator
class PersonalTradingValidator {
public:
    PersonalTradingValidator(PersonalMLPipeline& mlPipeline,
                           PersonalMLBenchmarks& benchmarks,
                           SmartCacheManager& cacheManager);
    ~PersonalTradingValidator() = default;

    // Initialization
    bool initialize(const ValidationConfig& config);
    void shutdown();

    // Main validation functions
    ValidatedTradingSignal validateTradingSignal(const PersonalPrediction& prediction);
    std::vector<ValidatedTradingSignal> validateBatchSignals(
        const std::vector<PersonalPrediction>& predictions);

    // Portfolio-level validation
    PortfolioValidation validatePortfolio(const std::vector<ValidatedTradingSignal>& signals,
                                         const std::map<std::string, double>& currentPositions = {});

    // Signal filtering and ranking
    std::vector<ValidatedTradingSignal> filterValidSignals(
        const std::vector<ValidatedTradingSignal>& signals);

    std::vector<ValidatedTradingSignal> rankSignalsByQuality(
        const std::vector<ValidatedTradingSignal>& signals);

    // Performance tracking
    void recordSignalResult(const std::string& signalId, double actualReturn);
    std::vector<SignalPerformance> getRecentPerformance(int days = 30) const;

    // Validation components
    bool validateSignalConfidence(const PersonalPrediction& prediction);
    bool validateModelQuality(const std::string& modelId);
    bool validateRiskLimits(const PersonalPrediction& prediction,
                           const std::map<std::string, double>& currentPositions);
    bool validateDataFreshness(const std::string& symbol);

    // Risk management
    double calculatePositionSize(const ValidatedTradingSignal& signal,
                               double accountBalance,
                               double riskPerTrade = 0.02);

    double calculateStopLoss(const ValidatedTradingSignal& signal,
                           double entryPrice,
                           double maxRisk = 0.03);

    double calculateTakeProfit(const ValidatedTradingSignal& signal,
                             double entryPrice,
                             double riskRewardRatio = 2.0);

    // Advanced validation features
    struct MarketConditionCheck {
        bool isSuitableForTrading;
        double volatilityScore;       // Market volatility assessment
        double trendStrength;         // Trend strength indicator
        std::string marketRegime;     // "trending", "ranging", "volatile"
        std::vector<std::string> warnings;

        MarketConditionCheck() : isSuitableForTrading(false), volatilityScore(0.0),
                                trendStrength(0.0), marketRegime("unknown") {}
    };

    MarketConditionCheck assessMarketConditions(const std::string& symbol);

    // Correlation and diversification analysis
    double calculatePortfolioCorrelation(const std::vector<std::string>& symbols);
    double calculateDiversificationBenefit(const std::vector<ValidatedTradingSignal>& signals);

    // Real-time validation monitoring
    struct ValidationMetrics {
        int totalSignalsProcessed;
        int validSignalsGenerated;
        int signalsTraded;
        double averageConfidence;
        double averageActualReturn;
        double validationAccuracy;    // How often validation was correct
        std::chrono::system_clock::time_point lastUpdate;

        ValidationMetrics() : totalSignalsProcessed(0), validSignalsGenerated(0),
                             signalsTraded(0), averageConfidence(0.0),
                             averageActualReturn(0.0), validationAccuracy(0.0) {}
    };

    ValidationMetrics getValidationMetrics() const;
    void resetValidationMetrics();

    // Configuration management
    void updateValidationConfig(const ValidationConfig& config);
    ValidationConfig getValidationConfig() const { return config_; }

    // Emergency controls
    void enableEmergencyMode(bool enable = true);
    bool isInEmergencyMode() const { return emergencyMode_; }

    struct EmergencySettings {
        double maxRiskOverride;       // Override risk limits
        bool disableNewPositions;    // Stop opening new positions
        bool onlyCriticalSignals;    // Only highest confidence signals
        std::vector<std::string> allowedSymbols; // Restrict to specific symbols

        EmergencySettings() : maxRiskOverride(0.01), disableNewPositions(false),
                             onlyCriticalSignals(true) {
            allowedSymbols = {"BTC", "ETH"};
        }
    };

    void setEmergencySettings(const EmergencySettings& settings);

    // Backtesting validation
    struct BacktestValidation {
        std::string period;
        int totalSignals;
        int profitableSignals;
        double totalReturn;
        double sharpeRatio;
        double maxDrawdown;
        double winRate;
        bool passedValidation;

        BacktestValidation() : totalSignals(0), profitableSignals(0),
                              totalReturn(0.0), sharpeRatio(0.0), maxDrawdown(0.0),
                              winRate(0.0), passedValidation(false) {}
    };

    BacktestValidation runSignalBacktest(const std::string& symbol, int days = 90);

    // Signal quality scoring
    struct QualityScore {
        double dataQuality;           // Quality of underlying data
        double modelPerformance;      // Recent model performance
        double signalClarity;         // How clear/strong the signal is
        double riskAdjustment;        // Risk-adjusted score
        double overallScore;          // Combined quality score

        QualityScore() : dataQuality(0.0), modelPerformance(0.0),
                        signalClarity(0.0), riskAdjustment(0.0), overallScore(0.0) {}
    };

    QualityScore calculateSignalQuality(const PersonalPrediction& prediction);

    // Automated validation rules
    struct ValidationRule {
        std::string ruleName;
        std::function<bool(const PersonalPrediction&)> ruleFunction;
        double weight;                // Importance weight
        bool isMandatory;            // Must pass for valid signal

        ValidationRule() : weight(1.0), isMandatory(false) {}
    };

    void addValidationRule(const ValidationRule& rule);
    void removeValidationRule(const std::string& ruleName);
    std::vector<std::string> getActiveValidationRules() const;

private:
    PersonalMLPipeline& mlPipeline_;
    PersonalMLBenchmarks& benchmarks_;
    SmartCacheManager& cacheManager_;

    ValidationConfig config_;
    EmergencySettings emergencySettings_;
    bool emergencyMode_;

    // Performance tracking
    std::vector<SignalPerformance> signalHistory_;
    ValidationMetrics metrics_;

    // Validation rules
    std::map<std::string, ValidationRule> validationRules_;

    // Internal validation methods
    double validateAgainstBenchmarks(const std::string& modelId);
    bool checkRecentModelPerformance(const std::string& modelId);
    double assessSignalStrength(const PersonalPrediction& prediction);

    // Risk calculation helpers
    double calculateVaR(const std::vector<std::string>& symbols, double confidence = 0.95);
    double calculateExpectedShortfall(const std::vector<std::string>& symbols);

    // Market condition helpers
    double calculateVolatilityRegime(const std::string& symbol);
    double calculateTrendStrength(const std::string& symbol);

    // Utility functions
    std::string generateSignalId() const;
    bool isMarketOpen() const; // Crypto markets are always open, but useful for other checks
    double getCorrelation(const std::string& symbol1, const std::string& symbol2);

    // Default validation rules
    void setupDefaultValidationRules();
};

// Factory for creating validation configurations
class ValidationConfigFactory {
public:
    enum class TradingStyle {
        CONSERVATIVE,     // Very safe, low risk
        MODERATE,        // Balanced approach
        AGGRESSIVE,      // Higher risk/reward
        SCALPING,        // Very short term
        SWING           // Medium term positions
    };

    static ValidationConfig createConfig(TradingStyle style);
    static PersonalTradingValidator::EmergencySettings getEmergencySettings(TradingStyle style);
};

// Utility functions for signal validation
class SignalValidationUtils {
public:
    // Signal strength assessment
    static double calculateSignalStrength(const PersonalPrediction& prediction);

    // Risk assessment utilities
    static double estimateMaxLoss(const PersonalPrediction& prediction, double positionSize);
    static double calculateCorrelationRisk(const std::vector<std::string>& symbols);

    // Performance analysis
    static double calculateRiskAdjustedReturn(const std::vector<SignalPerformance>& performance);

    // Market timing utilities
    static bool isGoodTradingTime();
    static double getMarketVolatilityFactor(const std::string& symbol);
};

} // namespace Trading
} // namespace CryptoClaude