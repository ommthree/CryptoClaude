#pragma once

#include "../Database/Models/PortfolioData.h"
#include "../Database/Models/MarketData.h"
#include "../Analytics/CrossAssetCorrelationMonitor.h"
#include "../Portfolio/PortfolioOptimizer.h"
#include "PredictiveRiskEngine.h"
#include "../../Configuration/Config.h"
#include <vector>
#include <map>
#include <string>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <optional>
#include <chrono>
#include <memory>
#include <functional>

namespace CryptoClaude {
namespace Risk {

using namespace CryptoClaude::Database::Models;
using namespace CryptoClaude::Analytics;

// VaR calculation methodologies
enum class VaRMethodology {
    PARAMETRIC,           // Normal distribution assumption
    HISTORICAL_SIMULATION, // Historical returns distribution
    MONTE_CARLO,          // Monte Carlo simulation
    CORNISH_FISHER        // Higher moments adjustment
};

// Risk measure types
enum class RiskMeasureType {
    VALUE_AT_RISK,        // Standard VaR
    CONDITIONAL_VAR,      // Expected Shortfall (CVaR)
    COMPONENT_VAR,        // Risk attribution by component
    INCREMENTAL_VAR,      // Marginal risk contribution
    MAXIMUM_DRAWDOWN,     // Maximum portfolio loss
    TAIL_EXPECTATION      // Average loss beyond VaR
};

// Time horizons for VaR calculation
enum class VaRTimeHorizon {
    INTRADAY_1H,         // 1 hour
    INTRADAY_4H,         // 4 hours
    DAILY,               // 1 day (most common)
    WEEKLY,              // 7 days
    MONTHLY              // 30 days
};

// VaR confidence levels
enum class VaRConfidenceLevel {
    PERCENT_90 = 90,     // 90% confidence
    PERCENT_95 = 95,     // 95% confidence (regulatory standard)
    PERCENT_99 = 99,     // 99% confidence (conservative)
    PERCENT_999 = 999    // 99.9% confidence (extreme)
};

// VaR calculation result structure
struct VaRResult {
    VaRMethodology methodology;
    RiskMeasureType measureType;
    VaRTimeHorizon timeHorizon;
    VaRConfidenceLevel confidenceLevel;

    double portfolioValue;           // Current portfolio value
    double varAmount;                // VaR in dollar terms
    double varPercentage;            // VaR as percentage of portfolio
    double conditionalVaR;           // Expected Shortfall (CVaR)
    double maximumDrawdown;          // Maximum historical drawdown

    std::map<std::string, double> componentVaR;      // Risk contribution by asset
    std::map<std::string, double> incrementalVaR;    // Marginal risk per asset
    std::map<std::string, double> assetWeights;      // Current portfolio weights

    std::chrono::system_clock::time_point calculationTime;
    std::chrono::milliseconds calculationDuration;   // Performance metric

    // Risk decomposition
    double systematicRisk;           // Market risk component
    double idiosyncraticRisk;        // Asset-specific risk component
    double correlationRisk;          // Risk from correlation changes

    // Validation metrics
    double backtestAccuracy;         // Historical VaR accuracy
    int breachCount;                 // Number of VaR breaches
    double kupiecTestPValue;         // Kupiec test p-value

    std::vector<std::string> warnings;
    bool isValid;
};

// Historical simulation configuration
struct HistoricalSimulationConfig {
    int lookbackDays = 252;          // 1 year of trading days
    int minDataPoints = 100;         // Minimum required data points
    bool useWeightedReturns = true;  // Weight recent observations more
    double decayFactor = 0.94;       // Exponential decay factor
    bool adjustForVolatilityClustering = true;
    bool removeOutliers = true;      // Remove extreme outliers
    double outlierThreshold = 3.0;   // Z-score threshold
};

// Monte Carlo simulation configuration
struct MonteCarloConfig {
    int simulationCount = 10000;     // Number of simulations
    int randomSeed = 12345;          // For reproducibility
    bool useAntithetic = true;       // Antithetic variance reduction
    bool useControlVariates = true;  // Control variate technique
    double correlationDecay = 0.95;  // Correlation mean reversion
    bool includeJumpRisk = true;     // Include jump diffusion
    double jumpIntensity = 0.1;      // Jump probability per day
};

// Parametric VaR configuration
struct ParametricConfig {
    bool useEWMA = true;             // Exponentially weighted moving average
    double ewmaDecay = 0.94;         // EWMA decay factor (RiskMetrics)
    bool adjustForSkewness = true;   // Cornish-Fisher adjustment
    bool adjustForKurtosis = true;   // Higher moment adjustment
    int lookbackPeriod = 252;        // Days for parameter estimation
    bool useGARCH = false;           // GARCH volatility modeling
};

// TRS-approved conservative risk parameters
struct TRSRiskParameters {
    // Portfolio constraints (TRS requirements)
    double maxCashBuffer = 0.15;            // 15% maximum cash buffer
    double maxPairAllocation = 0.12;        // 12% maximum per pair
    double minInvestmentLevel = 0.85;       // 85% minimum investment

    // VaR limits
    double maxDailyVaR95 = 0.025;           // 2.5% maximum daily VaR at 95%
    double maxDailyVaR99 = 0.035;           // 3.5% maximum daily VaR at 99%
    double maxWeeklyVaR95 = 0.055;          // 5.5% maximum weekly VaR at 95%
    double maxMonthlyVaR95 = 0.12;          // 12% maximum monthly VaR at 95%

    // Risk monitoring thresholds
    double varBreachThreshold = 0.8;        // 80% of limit triggers alert
    double cvarLimitMultiple = 1.3;         // CVaR should be ≤ 1.3x VaR
    double correlationRiskLimit = 0.4;      // 40% max correlation risk

    // Performance requirements
    int maxCalculationTimeMs = 100;         // <100ms calculation time
    int maxDashboardRefreshMs = 1000;       // <1s dashboard refresh
    double minBacktestAccuracy = 0.90;      // 90% minimum accuracy
};

// VaR backtesting framework
class VaRBacktester {
public:
    struct BacktestResult {
        VaRMethodology methodology;
        VaRConfidenceLevel confidenceLevel;
        int totalObservations;
        int varBreaches;
        double breachRate;
        double expectedBreachRate;

        // Statistical tests
        double kupiecStatistic;          // Kupiec POF test
        double kupiecPValue;
        bool kupiecTestPassed;

        double christoffersenStatistic;   // Independence test
        double christoffersenPValue;
        bool christoffersenTestPassed;

        // Performance metrics
        double meanAbsoluteError;
        double rootMeanSquareError;
        double averageVaRAccuracy;

        std::vector<std::pair<std::chrono::system_clock::time_point, bool>> breachDetails;
    };

    VaRBacktester(int backtestPeriodDays = 252);
    ~VaRBacktester() = default;

    // Run comprehensive backtest
    BacktestResult runBacktest(VaRMethodology methodology,
                              VaRConfidenceLevel confidenceLevel,
                              const std::vector<double>& returns,
                              const std::vector<double>& varEstimates);

    // Individual test methods
    bool kupiecTest(double breachRate, double expectedRate, int observations, double& pValue);
    bool christoffersenTest(const std::vector<bool>& breaches, double expectedRate, double& pValue);

    // Performance metrics
    double calculateMeanAbsoluteError(const std::vector<double>& actual,
                                    const std::vector<double>& predicted);
    double calculateRootMeanSquareError(const std::vector<double>& actual,
                                      const std::vector<double>& predicted);

private:
    int backtestPeriodDays_;
    double criticalValue95_;
    double criticalValue99_;
};

// Main VaR calculation engine
class VaRCalculator {
public:
    VaRCalculator();
    ~VaRCalculator();

    // Initialization and configuration
    bool initialize(const TRSRiskParameters& trsParams = TRSRiskParameters{});
    void setCorrelationMonitor(std::shared_ptr<CrossAssetCorrelationMonitor> monitor);
    void setPortfolioOptimizer(std::shared_ptr<CryptoClaude::Optimization::PortfolioOptimizer> optimizer);

    // === MAIN VAR CALCULATION METHODS ===

    // Calculate VaR using specified methodology
    VaRResult calculateVaR(const std::vector<std::string>& assets,
                          const std::vector<double>& weights,
                          VaRMethodology methodology = VaRMethodology::PARAMETRIC,
                          VaRConfidenceLevel confidence = VaRConfidenceLevel::PERCENT_95,
                          VaRTimeHorizon horizon = VaRTimeHorizon::DAILY);

    // Portfolio-level VaR calculation
    VaRResult calculatePortfolioVaR(const Portfolio& portfolio,
                                   VaRMethodology methodology = VaRMethodology::HISTORICAL_SIMULATION,
                                   VaRConfidenceLevel confidence = VaRConfidenceLevel::PERCENT_95);

    // Multiple methodology comparison
    std::map<VaRMethodology, VaRResult> calculateMultiMethodVaR(
        const std::vector<std::string>& assets,
        const std::vector<double>& weights,
        VaRConfidenceLevel confidence = VaRConfidenceLevel::PERCENT_95);

    // === SPECIFIC VAR METHODOLOGIES ===

    // Parametric VaR (assumes normal distribution)
    VaRResult calculateParametricVaR(const std::vector<std::string>& assets,
                                    const std::vector<double>& weights,
                                    VaRConfidenceLevel confidence,
                                    const ParametricConfig& config = ParametricConfig{});

    // Historical Simulation VaR
    VaRResult calculateHistoricalVaR(const std::vector<std::string>& assets,
                                    const std::vector<double>& weights,
                                    VaRConfidenceLevel confidence,
                                    const HistoricalSimulationConfig& config = HistoricalSimulationConfig{});

    // Monte Carlo VaR
    VaRResult calculateMonteCarloVaR(const std::vector<std::string>& assets,
                                    const std::vector<double>& weights,
                                    VaRConfidenceLevel confidence,
                                    const MonteCarloConfig& config = MonteCarloConfig{});

    // Cornish-Fisher VaR (higher moments)
    VaRResult calculateCornishFisherVaR(const std::vector<std::string>& assets,
                                       const std::vector<double>& weights,
                                       VaRConfidenceLevel confidence);

    // === ADVANCED RISK MEASURES ===

    // Conditional VaR (Expected Shortfall)
    double calculateConditionalVaR(const std::vector<double>& returns,
                                  VaRConfidenceLevel confidence);

    // Component VaR (risk attribution)
    std::map<std::string, double> calculateComponentVaR(const std::vector<std::string>& assets,
                                                        const std::vector<double>& weights,
                                                        VaRMethodology methodology);

    // Incremental VaR (marginal contribution)
    std::map<std::string, double> calculateIncrementalVaR(const std::vector<std::string>& assets,
                                                          const std::vector<double>& weights,
                                                          VaRMethodology methodology);

    // Maximum Drawdown analysis
    double calculateMaximumDrawdown(const std::vector<double>& portfolioValues);

    // === RISK MONITORING AND ALERTS ===

    struct RiskAlert {
        enum AlertLevel { INFO, WARNING, CRITICAL, EMERGENCY };
        enum AlertType {
            VAR_LIMIT_BREACH,
            CORRELATION_SPIKE,
            VOLATILITY_REGIME_CHANGE,
            BACKTEST_FAILURE,
            CONCENTRATION_RISK,
            LIQUIDITY_RISK
        };

        AlertLevel level;
        AlertType type;
        std::string message;
        double severity;        // 0-1 scale
        std::vector<std::string> affectedAssets;
        std::chrono::system_clock::time_point timestamp;
        std::vector<std::string> recommendedActions;
    };

    // Real-time risk monitoring
    std::vector<RiskAlert> checkRiskLimits(const VaRResult& varResult);
    bool isVaRLimitBreached(const VaRResult& varResult);
    std::vector<std::string> generateRiskRecommendations(const VaRResult& varResult);

    // === CORRELATION INTEGRATION ===

    // Update correlation matrix from monitor
    void updateCorrelationMatrix();
    std::map<std::pair<std::string, std::string>, double> getCorrelationMatrix() const;

    // Correlation risk analysis
    double calculateCorrelationRisk(const std::vector<std::string>& assets,
                                   const std::vector<double>& weights);

    // === BACKTESTING AND VALIDATION ===

    // Run VaR backtesting
    VaRBacktester::BacktestResult backtest(VaRMethodology methodology,
                                          VaRConfidenceLevel confidence,
                                          int backtestDays = 252);

    // Validate VaR models
    struct ModelValidationResult {
        VaRMethodology methodology;
        bool isValid;
        double accuracy;
        std::vector<std::string> validationWarnings;
        VaRBacktester::BacktestResult backtestResult;
    };

    std::vector<ModelValidationResult> validateAllModels(int backtestDays = 252);

    // === PERFORMANCE AND UTILITIES ===

    // Performance benchmarking
    struct PerformanceMetrics {
        std::map<VaRMethodology, std::chrono::milliseconds> calculationTimes;
        double averageCalculationTime;
        double maxCalculationTime;
        bool meetsPerformanceTarget;    // <100ms requirement
        int calculationsPerSecond;
    };

    PerformanceMetrics benchmarkPerformance(int iterations = 100);

    // Data management
    void updateMarketData(const std::map<std::string, MarketData>& data);
    void updatePortfolioData(const Portfolio& portfolio);

    // Configuration
    void setHistoricalSimulationConfig(const HistoricalSimulationConfig& config);
    void setMonteCarloConfig(const MonteCarloConfig& config);
    void setParametricConfig(const ParametricConfig& config);
    void setTRSParameters(const TRSRiskParameters& params);

    // Getters
    TRSRiskParameters getTRSParameters() const { return trsParams_; }
    bool isInitialized() const { return isInitialized_; }
    std::chrono::system_clock::time_point getLastCalculation() const { return lastCalculationTime_; }

    // === DASHBOARD AND REPORTING ===

    struct RiskDashboardData {
        // Current VaR metrics
        std::map<VaRMethodology, VaRResult> currentVaR;
        std::vector<RiskAlert> activeAlerts;

        // Historical performance
        std::vector<std::pair<std::chrono::system_clock::time_point, double>> varHistory;
        std::vector<std::pair<std::chrono::system_clock::time_point, double>> portfolioValueHistory;

        // Risk decomposition
        std::map<std::string, double> riskContributions;
        std::map<std::string, double> correlationRisks;

        // Model validation status
        std::map<VaRMethodology, bool> modelValidationStatus;
        std::map<VaRMethodology, double> backtestAccuracy;

        // Performance metrics
        PerformanceMetrics performanceMetrics;

        std::chrono::system_clock::time_point lastUpdate;
    };

    RiskDashboardData getDashboardData();
    bool updateDashboard();  // <1s performance requirement

private:
    // Configuration
    TRSRiskParameters trsParams_;
    HistoricalSimulationConfig histConfig_;
    MonteCarloConfig mcConfig_;
    ParametricConfig paramConfig_;

    // Components
    std::shared_ptr<CrossAssetCorrelationMonitor> correlationMonitor_;
    std::shared_ptr<CryptoClaude::Optimization::PortfolioOptimizer> portfolioOptimizer_;
    std::unique_ptr<VaRBacktester> backtester_;

    // Data storage
    std::map<std::string, std::vector<double>> historicalReturns_;
    std::map<std::string, MarketData> currentMarketData_;
    std::map<std::pair<std::string, std::string>, double> correlationMatrix_;
    Portfolio currentPortfolio_;

    // Risk monitoring
    std::vector<RiskAlert> activeAlerts_;
    RiskDashboardData dashboardData_;

    // State management
    bool isInitialized_;
    std::chrono::system_clock::time_point lastCalculationTime_;
    std::chrono::system_clock::time_point lastDashboardUpdate_;

    // === INTERNAL CALCULATION METHODS ===

    // Statistical utilities
    double calculatePortfolioVariance(const std::vector<double>& weights,
                                     const std::vector<std::vector<double>>& covarianceMatrix);
    std::vector<std::vector<double>> calculateCovarianceMatrix(const std::vector<std::string>& assets);
    double calculateSkewness(const std::vector<double>& returns);
    double calculateKurtosis(const std::vector<double>& returns);

    // Distribution utilities
    double normalInverseCDF(double probability);
    double cornishFisherAdjustment(double zScore, double skewness, double kurtosis);

    // Historical data management
    std::vector<double> getAssetReturns(const std::string& asset, int lookbackDays);
    std::vector<double> calculatePortfolioReturns(const std::vector<std::string>& assets,
                                                  const std::vector<double>& weights,
                                                  int lookbackDays);

    // Monte Carlo simulation helpers
    std::vector<std::vector<double>> generateCorrelatedRandomNumbers(
        const std::vector<std::vector<double>>& choleskyCovMatrix,
        int simulations);
    std::vector<std::vector<double>> choleskyDecomposition(
        const std::vector<std::vector<double>>& matrix);

    // Risk limit validation
    bool validateTRSLimits(const VaRResult& result);
    RiskAlert createRiskAlert(RiskAlert::AlertLevel level,
                             RiskAlert::AlertType type,
                             const std::string& message,
                             const std::vector<std::string>& assets = {});

    // Performance optimization
    void optimizeCalculationPerformance();
    void cacheFrequentCalculations();

    // Data validation
    bool validateInputData(const std::vector<std::string>& assets,
                          const std::vector<double>& weights);
    bool hasValidHistoricalData(const std::string& asset, int requiredDays);

    // Error handling
    void handleCalculationError(const std::string& method, const std::exception& e);
    void logPerformanceMetrics(const std::string& method,
                              std::chrono::milliseconds duration);
};

// Utility functions for VaR analysis
namespace VaRUtils {
    // Confidence level utilities
    double getZScore(VaRConfidenceLevel confidence);
    double getConfidenceValue(VaRConfidenceLevel confidence);

    // Time horizon scaling
    double scaleVaRToHorizon(double dailyVaR, VaRTimeHorizon horizon);
    int getHorizonDays(VaRTimeHorizon horizon);

    // Risk measure conversions
    double varToCovar(double var, const std::vector<double>& returns,
                     VaRConfidenceLevel confidence);
    double covarToVar(double cvar, double tailProbability);

    // Portfolio utilities
    double calculateDiversificationRatio(const std::vector<double>& weights,
                                        const std::vector<double>& volatilities,
                                        const std::vector<std::vector<double>>& correlations);

    // Statistical tests
    struct StatisticalTestResult {
        double statistic;
        double pValue;
        bool testPassed;
        std::string interpretation;
    };

    StatisticalTestResult kolmogorovSmirnovTest(const std::vector<double>& sample1,
                                               const std::vector<double>& sample2);
    StatisticalTestResult jarqueBeraTest(const std::vector<double>& returns);
}

} // namespace Risk
} // namespace CryptoClaude