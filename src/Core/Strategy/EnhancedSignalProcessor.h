#pragma once

#include "IStrategy.h"
#include "../Analytics/StatisticalTools.h"
#include "../Analytics/TechnicalIndicators.h"
#include "../FeatureEngineering/FeatureCalculator.h"
#include "../Database/Models/MarketData.h"
#include "../Database/Models/SentimentData.h"
#include <memory>
#include <vector>
#include <map>
#include <functional>

namespace CryptoClaude {
namespace Strategy {

// Enhanced signal types and structures
enum class SignalQuality {
    VERY_LOW = 0,
    LOW = 1,
    MEDIUM = 2,
    HIGH = 3,
    VERY_HIGH = 4
};

enum class SignalSource {
    TECHNICAL_ANALYSIS,
    FUNDAMENTAL_ANALYSIS,
    SENTIMENT_ANALYSIS,
    MACHINE_LEARNING,
    STATISTICAL_ARBITRAGE,
    CROSS_ASSET_CORRELATION
};

struct EnhancedTradingSignal {
    // Base signal information (extends TradingSignal)
    std::string symbol;
    SignalType signalType;
    double targetWeight;
    double confidence;
    double expectedReturn;
    double riskScore;
    std::string reason;
    std::chrono::system_clock::time_point timestamp;

    // Enhanced signal properties
    SignalSource source;
    SignalQuality quality;
    double statisticalSignificance; // p-value or confidence interval
    double timeHorizon; // Expected holding period in days
    std::vector<std::string> supportingIndicators;
    std::map<std::string, double> metadata; // Additional signal-specific data

    // Risk and performance attribution
    double expectedVolatility;
    double valueAtRisk; // 95% VaR
    double expectedShortfall; // Conditional VaR
    double correlationWithPortfolio;
    double diversificationBenefit;

    // Signal validation and tracking
    std::string signalId; // Unique identifier for tracking
    double currentPnL = 0.0; // Track actual performance
    bool isActive = true;
    std::chrono::system_clock::time_point expirationTime;

    EnhancedTradingSignal() : signalType(SignalType::HOLD), targetWeight(0.0),
                             confidence(0.0), expectedReturn(0.0), riskScore(0.0),
                             source(SignalSource::TECHNICAL_ANALYSIS), quality(SignalQuality::MEDIUM),
                             statisticalSignificance(0.0), timeHorizon(1.0),
                             expectedVolatility(0.0), valueAtRisk(0.0),
                             expectedShortfall(0.0), correlationWithPortfolio(0.0),
                             diversificationBenefit(0.0),
                             timestamp(std::chrono::system_clock::now()) {
        expirationTime = timestamp + std::chrono::hours(24); // Default 24h expiration
    }
};

struct MarketRegime {
    enum class Type {
        BULL_MARKET,
        BEAR_MARKET,
        SIDEWAYS_MARKET,
        HIGH_VOLATILITY,
        LOW_VOLATILITY,
        CRISIS_MODE
    };

    Type regimeType;
    double confidence;
    std::chrono::system_clock::time_point detectedAt;
    double volatilityLevel;
    double trendStrength;
    std::string description;
};

// Advanced signal processing and generation
class EnhancedSignalProcessor {
public:
    EnhancedSignalProcessor();
    ~EnhancedSignalProcessor() = default;

    // Configuration and initialization
    void setFeatureCalculator(std::shared_ptr<FeatureEngineering::FeatureCalculator> calculator);
    void setMinimumSignalQuality(SignalQuality minQuality);
    void setMaximumSignalsPerSymbol(int maxSignals);
    void enableMarketRegimeDetection(bool enable);

    // Core signal generation
    std::vector<EnhancedTradingSignal> generateTechnicalSignals(
        const std::string& symbol,
        const std::vector<Database::Models::MarketData>& marketData,
        int lookbackPeriod = 100);

    std::vector<EnhancedTradingSignal> generateStatisticalSignals(
        const std::string& symbol,
        const std::vector<Database::Models::MarketData>& marketData,
        const std::map<std::string, std::vector<Database::Models::MarketData>>& marketContext);

    std::vector<EnhancedTradingSignal> generateSentimentSignals(
        const std::string& symbol,
        const std::vector<Database::Models::SentimentData>& sentimentData,
        const std::vector<Database::Models::MarketData>& marketData);

    std::vector<EnhancedTradingSignal> generateMLSignals(
        const std::string& symbol,
        const std::vector<FeatureEngineering::FeatureVector>& features,
        const std::vector<MachineLearning::DailyPrediction>& predictions);

    // Multi-asset and cross-correlation signals
    std::vector<EnhancedTradingSignal> generateCrossAssetSignals(
        const std::map<std::string, std::vector<Database::Models::MarketData>>& multiAssetData);

    std::vector<EnhancedTradingSignal> generatePairsTradingSignals(
        const std::string& symbol1,
        const std::string& symbol2,
        const std::vector<Database::Models::MarketData>& data1,
        const std::vector<Database::Models::MarketData>& data2);

    // Signal aggregation and filtering
    std::vector<EnhancedTradingSignal> aggregateSignals(
        const std::vector<std::vector<EnhancedTradingSignal>>& signalSets,
        const std::string& aggregationMethod = "weighted_average");

    std::vector<EnhancedTradingSignal> filterSignalsByQuality(
        const std::vector<EnhancedTradingSignal>& signals,
        SignalQuality minQuality);

    std::vector<EnhancedTradingSignal> filterSignalsByRisk(
        const std::vector<EnhancedTradingSignal>& signals,
        double maxRiskScore,
        double maxVaR);

    // Market regime detection and adaptation
    MarketRegime detectMarketRegime(
        const std::vector<Database::Models::MarketData>& marketData,
        int lookbackPeriod = 252);

    void adaptSignalParametersToRegime(const MarketRegime& regime);

    // Signal validation and backtesting
    struct SignalBacktestResult {
        std::string signalId;
        double actualReturn;
        double predictedReturn;
        double accuracy;
        double sharpeRatio;
        double maxDrawdown;
        int winRate;
        std::vector<double> dailyReturns;
    };

    std::vector<SignalBacktestResult> backtestSignals(
        const std::vector<EnhancedTradingSignal>& signals,
        const std::vector<Database::Models::MarketData>& marketData,
        int backtestPeriod = 30);

    // Signal portfolio optimization
    struct OptimizedSignalPortfolio {
        std::vector<EnhancedTradingSignal> selectedSignals;
        std::vector<double> optimalWeights;
        double expectedReturn;
        double expectedVolatility;
        double expectedSharpe;
        Analytics::CorrelationMatrix signalCorrelations;
    };

    OptimizedSignalPortfolio optimizeSignalPortfolio(
        const std::vector<EnhancedTradingSignal>& signals,
        double targetVolatility = 0.15,
        double maxConcentration = 0.1);

    // Real-time signal monitoring and updates
    void updateSignalPerformance(const std::string& signalId,
                                const std::vector<Database::Models::MarketData>& recentData);

    std::vector<EnhancedTradingSignal> getActiveSignals() const;
    std::vector<EnhancedTradingSignal> getExpiredSignals() const;
    void cleanupExpiredSignals();

    // Signal analytics and reporting
    struct SignalAnalytics {
        int totalSignalsGenerated;
        int activeSignals;
        int expiredSignals;
        double averageSignalAccuracy;
        double averageHoldingPeriod;
        std::map<SignalSource, double> sourcePerformance;
        std::map<SignalQuality, int> qualityDistribution;
        double overallPortfolioContribution;
    };

    SignalAnalytics generateSignalAnalytics() const;

private:
    // Configuration
    std::shared_ptr<FeatureEngineering::FeatureCalculator> featureCalculator_;
    SignalQuality minimumSignalQuality_;
    int maximumSignalsPerSymbol_;
    bool marketRegimeDetectionEnabled_;

    // Signal tracking
    std::vector<EnhancedTradingSignal> activeSignals_;
    std::vector<SignalBacktestResult> historicalPerformance_;
    MarketRegime currentMarketRegime_;

    // Signal generation helpers
    EnhancedTradingSignal createTechnicalSignal(
        const std::string& symbol,
        const Analytics::TechnicalIndicators::TrendAnalysis& analysis,
        const Analytics::RSI& rsi,
        const Analytics::MACD& macd,
        const Analytics::BollingerBands& bollinger);

    EnhancedTradingSignal createMeanReversionSignal(
        const std::string& symbol,
        const std::vector<Database::Models::MarketData>& data,
        const Analytics::DistributionStats& stats);

    EnhancedTradingSignal createMomentumSignal(
        const std::string& symbol,
        const std::vector<Database::Models::MarketData>& data,
        double momentum,
        double significance);

    EnhancedTradingSignal createArbitrageSignal(
        const std::string& symbol1,
        const std::string& symbol2,
        double spread,
        double meanSpread,
        double spreadVolatility);

    // Signal quality assessment
    SignalQuality assessSignalQuality(const EnhancedTradingSignal& signal,
                                     const std::vector<Database::Models::MarketData>& historicalData);

    double calculateSignalSignificance(const EnhancedTradingSignal& signal,
                                      const std::vector<double>& returns);

    // Risk calculations
    double calculateSignalVaR(const EnhancedTradingSignal& signal,
                             const std::vector<double>& returns,
                             double confidenceLevel = 0.95);

    double calculateSignalExpectedShortfall(const EnhancedTradingSignal& signal,
                                           const std::vector<double>& returns,
                                           double confidenceLevel = 0.95);

    // Utility functions
    std::string generateSignalId(const std::string& symbol, SignalSource source);
    std::vector<double> extractPrices(const std::vector<Database::Models::MarketData>& data);
    std::vector<double> extractVolumes(const std::vector<Database::Models::MarketData>& data);
    std::vector<double> calculateReturns(const std::vector<double>& prices);
};

// Signal ensemble methods
class SignalEnsemble {
public:
    SignalEnsemble() = default;

    // Ensemble signal combination
    struct EnsembleWeights {
        std::map<SignalSource, double> sourceWeights;
        std::map<SignalQuality, double> qualityWeights;
        double temporalDecay; // Weight decay over time
    };

    static EnhancedTradingSignal combineSignals(
        const std::vector<EnhancedTradingSignal>& signals,
        const EnsembleWeights& weights);

    static EnsembleWeights optimizeEnsembleWeights(
        const std::vector<std::vector<EnhancedTradingSignal>>& historicalSignals,
        const std::vector<std::vector<double>>& historicalReturns);

    // Signal voting and consensus
    static EnhancedTradingSignal voteOnSignals(
        const std::vector<EnhancedTradingSignal>& signals,
        const std::string& votingMethod = "confidence_weighted");

    // Signal stability analysis
    struct StabilityMetrics {
        double signalConsistency;
        double temporalStability;
        double crossValidationScore;
        double robustnessScore;
    };

    static StabilityMetrics analyzeSignalStability(
        const std::vector<EnhancedTradingSignal>& signals,
        int validationPeriods = 5);
};

} // namespace Strategy
} // namespace CryptoClaude