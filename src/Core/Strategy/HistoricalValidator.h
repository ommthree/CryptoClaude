#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <chrono>
#include "../Analytics/BacktestingEngine.h"
#include "../Risk/PredictiveRiskEngine.h"

namespace CryptoClaude {
namespace Strategy {

/**
 * Historical stress test scenario
 */
struct StressScenario {
    std::string name;
    std::string description;
    std::chrono::system_clock::time_point start_date;
    std::chrono::system_clock::time_point end_date;
    double expected_volatility;
    double expected_max_drawdown;
    std::map<std::string, double> scenario_parameters;

    StressScenario() = default;
    StressScenario(const std::string& n, const std::string& desc,
                  const std::chrono::system_clock::time_point& start,
                  const std::chrono::system_clock::time_point& end)
        : name(n), description(desc), start_date(start), end_date(end) {}
};

/**
 * Risk validation results for a specific period
 */
struct RiskValidationResults {
    std::string scenario_name;
    std::chrono::system_clock::time_point test_date;

    // Position Sizing Validation
    bool position_sizing_accurate;
    double average_position_size_error;
    int position_size_violations;

    // Risk Limit Compliance
    bool portfolio_risk_compliant;
    double max_portfolio_risk_breach;
    int risk_limit_violations;

    // Drawdown Control
    bool drawdown_control_effective;
    double actual_max_drawdown;
    double allowed_max_drawdown;
    bool stop_loss_triggered_correctly;

    // Volatility Management
    bool volatility_targeting_accurate;
    double target_volatility;
    double actual_volatility;
    double volatility_tracking_error;

    // Correlation Risk
    bool correlation_limits_enforced;
    double max_correlation_breach;
    std::map<std::string, double> asset_correlations;

    RiskValidationResults() : position_sizing_accurate(false), portfolio_risk_compliant(false),
                             drawdown_control_effective(false), volatility_targeting_accurate(false),
                             correlation_limits_enforced(false), average_position_size_error(0.0),
                             position_size_violations(0), max_portfolio_risk_breach(0.0),
                             risk_limit_violations(0), actual_max_drawdown(0.0),
                             allowed_max_drawdown(0.0), stop_loss_triggered_correctly(false),
                             target_volatility(0.0), actual_volatility(0.0),
                             volatility_tracking_error(0.0), max_correlation_breach(0.0) {}
};

/**
 * Walk-forward validation results
 */
struct WalkForwardResults {
    std::vector<Analytics::BacktestResults> period_results;

    // Consistency Metrics
    double performance_consistency; // Coefficient of variation of returns
    double risk_consistency; // Consistency of risk metrics
    double parameter_stability; // How stable optimal parameters are

    // Out-of-Sample Performance
    double in_sample_return;
    double out_of_sample_return;
    double performance_degradation; // (in_sample - out_of_sample) / in_sample

    // Statistical Significance
    bool results_statistically_significant;
    double confidence_level;
    int degrees_of_freedom;

    WalkForwardResults() : performance_consistency(0.0), risk_consistency(0.0),
                          parameter_stability(0.0), in_sample_return(0.0),
                          out_of_sample_return(0.0), performance_degradation(0.0),
                          results_statistically_significant(false), confidence_level(0.0),
                          degrees_of_freedom(0) {}
};

/**
 * Comprehensive historical validation system for trading strategies
 * Validates risk management, position sizing, and strategy robustness across market conditions
 */
class HistoricalValidator {
public:
    HistoricalValidator();
    ~HistoricalValidator() = default;

    // Initialization
    bool initialize(std::shared_ptr<Analytics::BacktestingEngine> backtester);
    void setRiskParameters(double max_position_size = 0.25,
                          double max_portfolio_risk = 0.15,
                          double target_volatility = 0.12);

    // Stress Testing Scenarios
    void addStressScenario(const StressScenario& scenario);
    std::vector<StressScenario> getDefaultCryptocurrencyScenarios();

    // Risk Management Validation
    RiskValidationResults validateRiskManagement(const std::string& strategy_name,
                                                const StressScenario& scenario);

    // Position Sizing Validation
    bool validatePositionSizing(const std::vector<Analytics::TradingSignal>& signals,
                               const std::vector<Analytics::MarketDataPoint>& market_data,
                               RiskValidationResults& results);

    // Drawdown Control Validation
    bool validateDrawdownControl(const std::vector<Analytics::BacktestTrade>& trades,
                                const std::vector<double>& equity_curve,
                                RiskValidationResults& results);

    // Walk-Forward Analysis
    WalkForwardResults runWalkForwardValidation(const std::string& strategy_name,
                                               int training_periods = 6,
                                               int validation_periods = 1,
                                               int step_size_days = 30);

    // Parameter Stability Testing
    std::map<std::string, double> testParameterSensitivity(const std::string& strategy_name,
                                                          const std::map<std::string, std::vector<double>>& parameter_ranges);

    // Market Regime Analysis
    std::map<std::string, Analytics::BacktestResults> analyzePerformanceByRegime(const std::string& strategy_name);

    // Comprehensive Validation Report
    std::string generateValidationReport(const std::vector<RiskValidationResults>& risk_results,
                                        const WalkForwardResults& walk_forward_results);

    // Statistical Tests
    bool testStatisticalSignificance(const std::vector<double>& returns, double confidence_level = 0.05);
    double calculateInformationRatio(const std::vector<double>& strategy_returns,
                                    const std::vector<double>& benchmark_returns);

private:
    std::shared_ptr<Analytics::BacktestingEngine> backtester_;

    // Risk Parameters
    double max_position_size_;
    double max_portfolio_risk_;
    double target_volatility_;

    // Stress Scenarios
    std::vector<StressScenario> stress_scenarios_;

    // Market Regime Detection
    enum class MarketRegime {
        BULL_MARKET,
        BEAR_MARKET,
        HIGH_VOLATILITY,
        LOW_VOLATILITY,
        TRENDING,
        RANGING
    };

    MarketRegime detectMarketRegime(const std::vector<Analytics::MarketDataPoint>& market_data,
                                   const std::chrono::system_clock::time_point& start_time,
                                   const std::chrono::system_clock::time_point& end_time);

    // Risk Calculations
    double calculatePortfolioRisk(const std::vector<Analytics::TradingSignal>& signals,
                                 const std::vector<Analytics::MarketDataPoint>& market_data);

    double calculatePositionRisk(const Analytics::TradingSignal& signal,
                                const std::vector<Analytics::MarketDataPoint>& market_data);

    // Statistical Utilities
    double calculateCorrelation(const std::vector<double>& x, const std::vector<double>& y);
    double calculateVolatility(const std::vector<double>& returns);
    std::vector<double> calculateRollingReturns(const std::vector<double>& prices, int window);

    // Scenario Generation
    std::chrono::system_clock::time_point parseDate(const std::string& date_str);
    std::string formatDate(const std::chrono::system_clock::time_point& tp);

    // Validation Utilities
    bool checkPositionSizeCompliance(double position_size, double max_allowed);
    bool checkRiskLimitCompliance(double portfolio_risk, double risk_limit);
    bool checkDrawdownCompliance(double drawdown, double max_allowed_drawdown);
};

}} // namespace CryptoClaude::Strategy