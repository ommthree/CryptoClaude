#pragma once
#include "IStrategy.h"
#include "../MachineLearning/PredictionService.h"
#include "../Database/Repositories/MarketDataRepository.h"
#include <memory>
#include <vector>
#include <functional>

namespace CryptoClaude {
namespace Strategy {

// Backtest configuration
struct BacktestConfig {
    std::string startDate;
    std::string endDate;
    double initialCapital;
    double transactionCostBps;      // Transaction costs in basis points
    double marginRate;              // Interest rate for margin/short positions
    int rebalanceFrequency;         // Days between rebalances
    bool useOutOfSampleData;        // Whether to split data for out-of-sample testing
    double outOfSampleRatio;        // Ratio of data reserved for out-of-sample testing

    BacktestConfig()
        : initialCapital(1000000.0), transactionCostBps(5.0), marginRate(0.03),
          rebalanceFrequency(1), useOutOfSampleData(true), outOfSampleRatio(0.3) {}
};

// Backtest result for a single period
struct BacktestPeriodResult {
    std::string date;
    Database::Models::Portfolio portfolio;
    std::vector<TradingSignal> signals;
    double portfolioReturn;
    double cumulativeReturn;
    double drawdown;
    double transactionCosts;
    double marginCosts;

    BacktestPeriodResult() : portfolioReturn(0.0), cumulativeReturn(0.0),
                           drawdown(0.0), transactionCosts(0.0), marginCosts(0.0) {}
};

// Comprehensive backtest results
struct BacktestResult {
    BacktestConfig config;
    StrategyMetrics metrics;
    std::vector<BacktestPeriodResult> periodResults;

    // Performance comparison
    std::vector<double> benchmarkReturns;
    std::vector<double> strategyReturns;

    // Risk analysis
    double maxDrawdown;
    double valueAtRisk95;           // 95% VaR
    double conditionalVaR95;        // 95% CVaR (Expected Shortfall)

    // Trade analysis
    struct TradeStatistics {
        int totalTrades;
        int winningTrades;
        int losingTrades;
        double winRate;
        double averageWin;
        double averageLoss;
        double largestWin;
        double largestLoss;
        double profitFactor;
        double averageHoldingPeriod;
    } tradeStats;

    BacktestResult() : maxDrawdown(0.0), valueAtRisk95(0.0), conditionalVaR95(0.0) {}
};

class BacktestEngine {
public:
    BacktestEngine();
    ~BacktestEngine() = default;

    // Core components setup
    void setStrategy(std::unique_ptr<IStrategy> strategy);
    void setPredictionService(std::shared_ptr<MachineLearning::PredictionService> predictionService);
    void setMarketDataRepository(std::shared_ptr<Database::Repositories::MarketDataRepository> repository);

    // Backtesting execution
    BacktestResult runBacktest(const BacktestConfig& config);
    BacktestResult runWalkForwardBacktest(
        const BacktestConfig& config,
        int trainPeriodDays,
        int testPeriodDays);

    // Out-of-sample testing
    BacktestResult runOutOfSampleTest(const BacktestConfig& config);

    // Monte Carlo simulation
    std::vector<BacktestResult> runMonteCarloSimulation(
        const BacktestConfig& config,
        int simulations,
        double returnVolatilityScale = 1.0);

    // Benchmark comparison
    BacktestResult runWithBenchmark(
        const BacktestConfig& config,
        const std::string& benchmarkSymbol = "BTC");

    // Progress callbacks
    using ProgressCallback = std::function<void(int current, int total, const std::string& status)>;
    void setProgressCallback(ProgressCallback callback) { m_progressCallback = callback; }

    // Configuration
    void setSlippageModel(const std::string& model) { m_slippageModel = model; }
    void setLiquidityConstraints(bool enabled) { m_useLiquidityConstraints = enabled; }
    void setCommissionModel(const std::string& model) { m_commissionModel = model; }

    // Results analysis
    void exportBacktestResults(const BacktestResult& result, const std::string& filename);
    void exportPerformanceChart(const BacktestResult& result, const std::string& filename);
    void exportTradeAnalysis(const BacktestResult& result, const std::string& filename);

    // Error handling
    std::string getLastError() const { return m_lastError; }
    bool hasError() const { return !m_lastError.empty(); }
    void clearError() { m_lastError.clear(); }

private:
    // Core components
    std::unique_ptr<IStrategy> m_strategy;
    std::shared_ptr<MachineLearning::PredictionService> m_predictionService;
    std::shared_ptr<Database::Repositories::MarketDataRepository> m_marketDataRepository;

    // Configuration
    std::string m_slippageModel;
    std::string m_commissionModel;
    bool m_useLiquidityConstraints;
    ProgressCallback m_progressCallback;

    // State
    std::string m_lastError;

    // Core backtesting logic
    BacktestPeriodResult simulateTradingDay(
        const std::string& currentDate,
        Database::Models::Portfolio& portfolio,
        const BacktestConfig& config);

    std::vector<MachineLearning::DailyPrediction> getPredictionsForDate(
        const std::string& date);

    std::vector<Database::Models::MarketData> getMarketDataForDate(
        const std::string& date);

    // Portfolio simulation
    void executeSignals(
        Database::Models::Portfolio& portfolio,
        const std::vector<TradingSignal>& signals,
        const std::vector<Database::Models::MarketData>& marketData,
        const BacktestConfig& config,
        BacktestPeriodResult& result);

    void updatePortfolioValue(
        Database::Models::Portfolio& portfolio,
        const std::vector<Database::Models::MarketData>& marketData);

    // Cost calculations
    double calculateTransactionCosts(
        const std::vector<TradingSignal>& signals,
        double transactionCostBps);

    double calculateMarginCosts(
        const Database::Models::Portfolio& portfolio,
        double marginRate,
        int daysSinceLastRebalance);

    double calculateSlippage(
        const std::string& symbol,
        double quantity,
        double price);

    // Risk calculations
    double calculateValueAtRisk(
        const std::vector<double>& returns,
        double confidenceLevel);

    double calculateConditionalVaR(
        const std::vector<double>& returns,
        double confidenceLevel);

    // Performance analysis
    StrategyMetrics calculateStrategyMetrics(
        const std::vector<BacktestPeriodResult>& results);

    BacktestResult::TradeStatistics calculateTradeStatistics(
        const std::vector<BacktestPeriodResult>& results);

    // Benchmark calculations
    std::vector<double> calculateBenchmarkReturns(
        const std::string& benchmarkSymbol,
        const std::string& startDate,
        const std::string& endDate);

    // Utility methods
    std::vector<std::string> generateTradingDates(
        const std::string& startDate,
        const std::string& endDate,
        int rebalanceFrequency);

    Database::Models::Portfolio createInitialPortfolio(double initialCapital);

    bool isRebalanceDay(
        const std::string& currentDate,
        const std::string& lastRebalanceDate,
        int rebalanceFrequency);

    // Data validation
    bool validateBacktestConfig(const BacktestConfig& config);
    bool validateDataAvailability(const std::string& startDate, const std::string& endDate);

    // Progress reporting
    void updateProgress(int current, int total, const std::string& status);

    // Error handling
    void setError(const std::string& error);
};

// Backtest analysis utilities
class BacktestAnalyzer {
public:
    // Statistical analysis
    static double calculateInformationRatio(
        const std::vector<double>& strategyReturns,
        const std::vector<double>& benchmarkReturns);

    static double calculateMaximumDrawdownDuration(const std::vector<BacktestPeriodResult>& results);

    static double calculateCalmarRatio(double annualizedReturn, double maxDrawdown);

    // Performance attribution
    struct PerformanceAttribution {
        double selectionEffect;
        double timingEffect;
        double interactionEffect;
        double totalActiveReturn;
    };

    static PerformanceAttribution analyzePerformanceAttribution(
        const BacktestResult& result);

    // Risk-adjusted metrics
    static double calculateSortinoRatio(
        const std::vector<double>& returns,
        double riskFreeRate = 0.0);

    static double calculateOmegaRatio(
        const std::vector<double>& returns,
        double threshold = 0.0);

    // Stability analysis
    static std::vector<double> calculateRollingMetrics(
        const std::vector<BacktestPeriodResult>& results,
        int windowSize,
        const std::string& metric);

    // Monte Carlo analysis
    static std::map<std::string, double> analyzeMonteCarloResults(
        const std::vector<BacktestResult>& results);

private:
    static std::vector<double> extractReturns(const std::vector<BacktestPeriodResult>& results);
    static std::vector<double> extractDrawdowns(const std::vector<BacktestPeriodResult>& results);
};

} // namespace Strategy
} // namespace CryptoClaude