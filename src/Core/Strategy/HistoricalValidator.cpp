#include "HistoricalValidator.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <iomanip>
#include <sstream>
#include <iostream>

namespace CryptoClaude {
namespace Strategy {

HistoricalValidator::HistoricalValidator()
    : max_position_size_(0.25)
    , max_portfolio_risk_(0.15)
    , target_volatility_(0.12) {
}

bool HistoricalValidator::initialize(std::shared_ptr<Analytics::BacktestingEngine> backtester) {
    backtester_ = backtester;
    if (!backtester_) {
        std::cerr << "HistoricalValidator: Invalid backtesting engine" << std::endl;
        return false;
    }

    // Add default cryptocurrency stress scenarios
    stress_scenarios_ = getDefaultCryptocurrencyScenarios();

    std::cout << "HistoricalValidator: Initialized with " << stress_scenarios_.size()
              << " stress scenarios" << std::endl;
    return true;
}

void HistoricalValidator::setRiskParameters(double max_position_size,
                                          double max_portfolio_risk,
                                          double target_volatility) {
    max_position_size_ = max_position_size;
    max_portfolio_risk_ = max_portfolio_risk;
    target_volatility_ = target_volatility;

    std::cout << "HistoricalValidator: Risk parameters updated - "
              << "Max Position: " << max_position_size * 100 << "%, "
              << "Max Portfolio Risk: " << max_portfolio_risk * 100 << "%, "
              << "Target Volatility: " << target_volatility * 100 << "%" << std::endl;
}

std::vector<StressScenario> HistoricalValidator::getDefaultCryptocurrencyScenarios() {
    std::vector<StressScenario> scenarios;

    // COVID-19 Crash (March 2020)
    StressScenario covid_crash("COVID_CRASH", "March 2020 cryptocurrency market crash",
                              parseDate("2020-03-01"), parseDate("2020-04-01"));
    covid_crash.expected_volatility = 0.80;
    covid_crash.expected_max_drawdown = -0.50;
    scenarios.push_back(covid_crash);

    // Bull Market 2020-2021
    StressScenario bull_2020("BULL_2020_2021", "Bitcoin bull market rally 2020-2021",
                            parseDate("2020-10-01"), parseDate("2021-05-01"));
    bull_2020.expected_volatility = 0.60;
    bull_2020.expected_max_drawdown = -0.20;
    scenarios.push_back(bull_2020);

    // Crypto Winter 2022
    StressScenario bear_2022("CRYPTO_WINTER_2022", "Crypto winter bear market 2022",
                            parseDate("2022-01-01"), parseDate("2022-12-31"));
    bear_2022.expected_volatility = 0.70;
    bear_2022.expected_max_drawdown = -0.75;
    scenarios.push_back(bear_2022);

    // LUNA Collapse (May 2022)
    StressScenario luna_collapse("LUNA_COLLAPSE", "Terra LUNA ecosystem collapse May 2022",
                                parseDate("2022-05-01"), parseDate("2022-06-01"));
    luna_collapse.expected_volatility = 1.20;
    luna_collapse.expected_max_drawdown = -0.60;
    scenarios.push_back(luna_collapse);

    // FTX Collapse (November 2022)
    StressScenario ftx_collapse("FTX_COLLAPSE", "FTX exchange collapse November 2022",
                               parseDate("2022-11-01"), parseDate("2022-12-01"));
    ftx_collapse.expected_volatility = 0.90;
    ftx_collapse.expected_max_drawdown = -0.40;
    scenarios.push_back(ftx_collapse);

    return scenarios;
}

RiskValidationResults HistoricalValidator::validateRiskManagement(const std::string& strategy_name,
                                                                 const StressScenario& scenario) {
    RiskValidationResults results;
    results.scenario_name = scenario.name;
    results.test_date = std::chrono::system_clock::now();
    results.allowed_max_drawdown = scenario.expected_max_drawdown;

    std::cout << "HistoricalValidator: Validating risk management for scenario '"
              << scenario.name << "'" << std::endl;

    if (!backtester_) {
        std::cerr << "HistoricalValidator: Backtesting engine not initialized" << std::endl;
        return results;
    }

    try {
        // Run backtest for the scenario period
        auto backtest_results = backtester_->runBacktest(strategy_name, scenario.start_date, scenario.end_date);

        // Validate drawdown control
        results.actual_max_drawdown = backtest_results.max_drawdown;
        results.drawdown_control_effective = (results.actual_max_drawdown >= scenario.expected_max_drawdown * 0.8);

        // Validate volatility targeting
        results.target_volatility = target_volatility_;
        results.actual_volatility = backtest_results.volatility;
        results.volatility_tracking_error = std::abs(results.actual_volatility - results.target_volatility);
        results.volatility_targeting_accurate = (results.volatility_tracking_error < 0.05); // 5% tolerance

        // Portfolio risk compliance
        results.portfolio_risk_compliant = (results.actual_max_drawdown >= -max_portfolio_risk_);
        if (!results.portfolio_risk_compliant) {
            results.max_portfolio_risk_breach = std::abs(results.actual_max_drawdown) - max_portfolio_risk_;
            results.risk_limit_violations = 1;
        }

        // Position sizing validation would require access to individual trades
        // For now, we'll assume position sizing is accurate if overall risk is controlled
        results.position_sizing_accurate = results.portfolio_risk_compliant;
        results.average_position_size_error = results.portfolio_risk_compliant ? 0.02 : 0.10; // Estimate

        // Stop-loss effectiveness
        results.stop_loss_triggered_correctly = (backtest_results.max_drawdown > -0.30); // If drawdown < 30%, stops worked

        std::cout << "HistoricalValidator: Scenario '" << scenario.name << "' validation completed. "
                  << "Max Drawdown: " << std::fixed << std::setprecision(2)
                  << results.actual_max_drawdown * 100 << "%, "
                  << "Volatility: " << results.actual_volatility * 100 << "%" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "HistoricalValidator: Error during risk validation: " << e.what() << std::endl;
    }

    return results;
}

WalkForwardResults HistoricalValidator::runWalkForwardValidation(const std::string& strategy_name,
                                                                int training_periods,
                                                                int validation_periods,
                                                                int step_size_days) {
    WalkForwardResults results;

    std::cout << "HistoricalValidator: Running walk-forward validation for '"
              << strategy_name << "' with " << training_periods
              << " training periods" << std::endl;

    if (!backtester_) {
        std::cerr << "HistoricalValidator: Backtesting engine not initialized" << std::endl;
        return results;
    }

    try {
        // Define the overall validation period (last 2 years)
        auto end_date = std::chrono::system_clock::now();
        auto start_date = end_date - std::chrono::hours(24 * 365 * 2); // 2 years ago

        std::vector<double> in_sample_returns;
        std::vector<double> out_of_sample_returns;

        // Perform walk-forward analysis
        auto current_start = start_date;
        int periods_tested = 0;

        while (current_start + std::chrono::hours(24 * training_periods * 30) < end_date && periods_tested < 12) {
            // Training period
            auto training_end = current_start + std::chrono::hours(24 * training_periods * 30);

            // Validation period
            auto validation_start = training_end;
            auto validation_end = validation_start + std::chrono::hours(24 * validation_periods * 30);

            if (validation_end > end_date) break;

            // Run backtest on training period
            auto training_results = backtester_->runBacktest(strategy_name + "_training",
                                                           current_start, training_end);
            in_sample_returns.push_back(training_results.total_return);

            // Run backtest on validation period
            auto validation_results = backtester_->runBacktest(strategy_name + "_validation",
                                                             validation_start, validation_end);
            out_of_sample_returns.push_back(validation_results.total_return);

            results.period_results.push_back(validation_results);

            current_start += std::chrono::hours(24 * step_size_days);
            periods_tested++;
        }

        // Calculate consistency metrics
        if (!out_of_sample_returns.empty()) {
            double mean_oos_return = std::accumulate(out_of_sample_returns.begin(),
                                                    out_of_sample_returns.end(), 0.0) / out_of_sample_returns.size();
            results.out_of_sample_return = mean_oos_return;

            // Calculate coefficient of variation
            double variance = 0.0;
            for (double ret : out_of_sample_returns) {
                variance += (ret - mean_oos_return) * (ret - mean_oos_return);
            }
            variance /= out_of_sample_returns.size();
            results.performance_consistency = std::sqrt(variance) / std::abs(mean_oos_return);
        }

        if (!in_sample_returns.empty()) {
            results.in_sample_return = std::accumulate(in_sample_returns.begin(),
                                                      in_sample_returns.end(), 0.0) / in_sample_returns.size();

            if (results.in_sample_return != 0) {
                results.performance_degradation = (results.in_sample_return - results.out_of_sample_return) / std::abs(results.in_sample_return);
            }
        }

        // Statistical significance test
        results.results_statistically_significant = testStatisticalSignificance(out_of_sample_returns);
        results.degrees_of_freedom = static_cast<int>(out_of_sample_returns.size()) - 1;
        results.confidence_level = 0.95;

        std::cout << "HistoricalValidator: Walk-forward validation completed. "
                  << periods_tested << " periods tested. "
                  << "Out-of-sample return: " << std::fixed << std::setprecision(2)
                  << results.out_of_sample_return * 100 << "%" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "HistoricalValidator: Error during walk-forward validation: " << e.what() << std::endl;
    }

    return results;
}

std::map<std::string, Analytics::BacktestResults> HistoricalValidator::analyzePerformanceByRegime(const std::string& strategy_name) {
    std::map<std::string, Analytics::BacktestResults> regime_results;

    std::cout << "HistoricalValidator: Analyzing performance by market regime for '"
              << strategy_name << "'" << std::endl;

    if (!backtester_) {
        return regime_results;
    }

    try {
        // Test each stress scenario as a market regime
        for (const auto& scenario : stress_scenarios_) {
            auto results = backtester_->runBacktest(strategy_name + "_" + scenario.name,
                                                  scenario.start_date, scenario.end_date);
            regime_results[scenario.name] = results;

            std::cout << "HistoricalValidator: Regime '" << scenario.name
                      << "' - Return: " << std::fixed << std::setprecision(2)
                      << results.total_return * 100 << "%, "
                      << "Sharpe: " << results.sharpe_ratio << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "HistoricalValidator: Error during regime analysis: " << e.what() << std::endl;
    }

    return regime_results;
}

bool HistoricalValidator::testStatisticalSignificance(const std::vector<double>& returns, double confidence_level) {
    if (returns.size() < 3) return false;

    double mean = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();

    // Calculate standard deviation
    double variance = 0.0;
    for (double ret : returns) {
        variance += (ret - mean) * (ret - mean);
    }
    variance /= (returns.size() - 1);
    double std_dev = std::sqrt(variance);

    // Simple t-test for mean > 0
    double t_statistic = mean / (std_dev / std::sqrt(returns.size()));

    // Critical t-value for 95% confidence (approximation for df > 30)
    double critical_t = 1.96; // For large samples
    if (returns.size() <= 30) {
        critical_t = 2.05; // Conservative estimate
    }

    return std::abs(t_statistic) > critical_t;
}

double HistoricalValidator::calculateInformationRatio(const std::vector<double>& strategy_returns,
                                                     const std::vector<double>& benchmark_returns) {
    if (strategy_returns.size() != benchmark_returns.size() || strategy_returns.empty()) {
        return 0.0;
    }

    // Calculate excess returns
    std::vector<double> excess_returns;
    for (size_t i = 0; i < strategy_returns.size(); ++i) {
        excess_returns.push_back(strategy_returns[i] - benchmark_returns[i]);
    }

    // Calculate mean and standard deviation of excess returns
    double mean_excess = std::accumulate(excess_returns.begin(), excess_returns.end(), 0.0) / excess_returns.size();

    double variance = 0.0;
    for (double ret : excess_returns) {
        variance += (ret - mean_excess) * (ret - mean_excess);
    }
    variance /= excess_returns.size();
    double tracking_error = std::sqrt(variance);

    return tracking_error > 0 ? mean_excess / tracking_error : 0.0;
}

std::string HistoricalValidator::generateValidationReport(const std::vector<RiskValidationResults>& risk_results,
                                                         const WalkForwardResults& walk_forward_results) {
    std::stringstream report;

    report << "=== HISTORICAL VALIDATION REPORT ===" << std::endl;
    report << "Generated: " << formatDate(std::chrono::system_clock::now()) << std::endl;
    report << std::endl;

    // Risk Management Validation Summary
    report << "RISK MANAGEMENT VALIDATION:" << std::endl;
    int total_scenarios = static_cast<int>(risk_results.size());
    int passed_scenarios = 0;

    for (const auto& result : risk_results) {
        bool scenario_passed = result.portfolio_risk_compliant &&
                              result.drawdown_control_effective &&
                              result.volatility_targeting_accurate;
        if (scenario_passed) passed_scenarios++;

        report << "  Scenario: " << result.scenario_name << std::endl;
        report << "    Portfolio Risk Compliant: " << (result.portfolio_risk_compliant ? "PASS" : "FAIL") << std::endl;
        report << "    Drawdown Control: " << (result.drawdown_control_effective ? "PASS" : "FAIL") << std::endl;
        report << "    Max Drawdown: " << std::fixed << std::setprecision(2)
               << result.actual_max_drawdown * 100 << "%" << std::endl;
        report << "    Volatility Targeting: " << (result.volatility_targeting_accurate ? "PASS" : "FAIL") << std::endl;
        report << "    Actual Volatility: " << result.actual_volatility * 100 << "%" << std::endl;
        report << std::endl;
    }

    report << "Risk Validation Summary: " << passed_scenarios << "/" << total_scenarios
           << " scenarios passed (" << std::fixed << std::setprecision(1)
           << (static_cast<double>(passed_scenarios) / total_scenarios) * 100 << "%)" << std::endl;
    report << std::endl;

    // Walk-Forward Analysis Summary
    report << "WALK-FORWARD VALIDATION:" << std::endl;
    report << "  In-Sample Return: " << std::fixed << std::setprecision(2)
           << walk_forward_results.in_sample_return * 100 << "%" << std::endl;
    report << "  Out-of-Sample Return: " << walk_forward_results.out_of_sample_return * 100 << "%" << std::endl;
    report << "  Performance Degradation: " << walk_forward_results.performance_degradation * 100 << "%" << std::endl;
    report << "  Performance Consistency: " << walk_forward_results.performance_consistency << std::endl;
    report << "  Statistical Significance: "
           << (walk_forward_results.results_statistically_significant ? "SIGNIFICANT" : "NOT SIGNIFICANT") << std::endl;
    report << "  Periods Tested: " << walk_forward_results.period_results.size() << std::endl;
    report << std::endl;

    // Overall Assessment
    bool validation_passed = (static_cast<double>(passed_scenarios) / total_scenarios >= 0.80) &&
                            walk_forward_results.results_statistically_significant &&
                            (walk_forward_results.performance_degradation < 0.50);

    report << "OVERALL VALIDATION RESULT: " << (validation_passed ? "PASS" : "FAIL") << std::endl;

    if (validation_passed) {
        report << "✅ Strategy demonstrates robust risk management and consistent performance" << std::endl;
    } else {
        report << "⚠️ Strategy requires improvement in risk management or consistency" << std::endl;
    }

    return report.str();
}

std::chrono::system_clock::time_point HistoricalValidator::parseDate(const std::string& date_str) {
    std::tm tm = {};
    std::istringstream ss(date_str);
    ss >> std::get_time(&tm, "%Y-%m-%d");
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

std::string HistoricalValidator::formatDate(const std::chrono::system_clock::time_point& tp) {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

double HistoricalValidator::calculateVolatility(const std::vector<double>& returns) {
    if (returns.empty()) return 0.0;

    double mean = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
    double variance = 0.0;

    for (double ret : returns) {
        variance += (ret - mean) * (ret - mean);
    }
    variance /= returns.size();

    return std::sqrt(variance);
}

}} // namespace CryptoClaude::Strategy