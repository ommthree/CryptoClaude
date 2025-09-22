#pragma once
#include "../Database/Models/PortfolioData.h"
#include "../MachineLearning/PredictionService.h"
#include <vector>
#include <string>
#include <chrono>

namespace CryptoClaude {
namespace Strategy {

// Strategy signal types
enum class SignalType {
    BUY,
    SELL,
    HOLD,
    CLOSE_LONG,
    CLOSE_SHORT
};

// Trading signal
struct TradingSignal {
    std::string symbol;
    SignalType signalType;
    double targetWeight;        // Target portfolio weight [-1, 1]
    double confidence;          // Signal confidence [0, 1]
    double expectedReturn;      // Expected return for this position
    double riskScore;          // Risk assessment [0, 1]
    std::string reason;        // Human-readable explanation
    std::chrono::system_clock::time_point timestamp;

    TradingSignal()
        : signalType(SignalType::HOLD), targetWeight(0.0),
          confidence(0.0), expectedReturn(0.0), riskScore(0.0),
          timestamp(std::chrono::system_clock::now()) {}
};

// Strategy performance metrics
struct StrategyMetrics {
    std::string strategyName;
    std::chrono::system_clock::time_point startDate;
    std::chrono::system_clock::time_point endDate;

    double totalReturn;
    double annualizedReturn;
    double volatility;
    double sharpeRatio;
    double maxDrawdown;
    double calmarRatio;

    int totalTrades;
    int winningTrades;
    int losingTrades;
    double winRate;
    double averageWin;
    double averageLoss;
    double profitFactor;

    double beta;               // Market beta (if benchmark available)
    double alpha;              // Jensen's alpha
    double informationRatio;

    StrategyMetrics()
        : totalReturn(0.0), annualizedReturn(0.0), volatility(0.0),
          sharpeRatio(0.0), maxDrawdown(0.0), calmarRatio(0.0),
          totalTrades(0), winningTrades(0), losingTrades(0),
          winRate(0.0), averageWin(0.0), averageLoss(0.0),
          profitFactor(0.0), beta(0.0), alpha(0.0), informationRatio(0.0) {}
};

// Abstract strategy interface
class IStrategy {
public:
    virtual ~IStrategy() = default;

    // Strategy identification
    virtual std::string getName() const = 0;
    virtual std::string getDescription() const = 0;
    virtual std::string getVersion() const = 0;

    // Strategy execution
    virtual std::vector<TradingSignal> generateSignals(
        const std::string& currentDate,
        const Database::Models::Portfolio& currentPortfolio,
        const std::vector<MachineLearning::DailyPrediction>& predictions) = 0;

    virtual bool shouldRebalance(
        const Database::Models::Portfolio& portfolio,
        const std::vector<MachineLearning::DailyPrediction>& predictions) = 0;

    // Strategy configuration
    virtual void setParameters(const std::map<std::string, double>& parameters) = 0;
    virtual std::map<std::string, double> getParameters() const = 0;
    virtual std::vector<std::string> getRequiredParameters() const = 0;

    // Risk management
    virtual double calculatePositionSize(
        const std::string& symbol,
        double expectedReturn,
        double volatility,
        const Database::Models::Portfolio& portfolio) = 0;

    virtual bool checkRiskLimits(
        const std::vector<TradingSignal>& signals,
        const Database::Models::Portfolio& portfolio) = 0;

    // Strategy validation
    virtual bool validateConfiguration() const = 0;
    virtual std::vector<std::string> getConfigurationErrors() const = 0;

    // Performance tracking
    virtual void updatePerformanceMetrics(const Database::Models::Portfolio& portfolio) = 0;
    virtual StrategyMetrics getPerformanceMetrics() const = 0;
};

// Strategy factory
class StrategyFactory {
public:
    enum class StrategyType {
        LongShort,
        MeanReversion,
        Momentum,
        PairsTrading,
        RiskParity
    };

    static std::unique_ptr<IStrategy> createStrategy(StrategyType type);
    static std::vector<std::string> getAvailableStrategies();
    static StrategyType getStrategyTypeFromString(const std::string& strategyName);
};

// Strategy parameter optimization
class StrategyOptimizer {
public:
    struct ParameterRange {
        std::string name;
        double minValue;
        double maxValue;
        double stepSize;

        ParameterRange(const std::string& n, double min, double max, double step = 0.1)
            : name(n), minValue(min), maxValue(max), stepSize(step) {}
    };

    struct OptimizationResult {
        std::map<std::string, double> bestParameters;
        StrategyMetrics bestMetrics;
        std::vector<std::map<std::string, double>> allParameters;
        std::vector<StrategyMetrics> allMetrics;
    };

    StrategyOptimizer() = default;

    OptimizationResult optimizeStrategy(
        std::unique_ptr<IStrategy>& strategy,
        const std::vector<ParameterRange>& parameterRanges,
        const std::string& optimizationStartDate,
        const std::string& optimizationEndDate,
        const std::string& metric = "sharpe_ratio");

private:
    std::vector<std::map<std::string, double>> generateParameterGrid(
        const std::vector<ParameterRange>& ranges);

    double extractMetricValue(const StrategyMetrics& metrics, const std::string& metricName);
};

} // namespace Strategy
} // namespace CryptoClaude