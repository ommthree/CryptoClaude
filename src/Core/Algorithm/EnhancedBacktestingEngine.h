#pragma once

#include "HistoricalDataManager.h"
#include "RealCorrelationValidator.h"
#include "RealMarketSignalGenerator.h"
#include "AlgorithmDecisionEngine.h"
#include <memory>
#include <vector>
#include <map>
#include <chrono>
#include <functional>

namespace CryptoClaude {
namespace Algorithm {

/**
 * Enhanced Backtesting Engine - Day 21 Implementation
 * Comprehensive backtesting framework with real historical data integration
 * Provides rigorous validation of algorithm performance and correlation tracking
 * Critical for TRS compliance and algorithm validation
 */
class EnhancedBacktestingEngine {
public:
    // Enhanced backtest configuration with real data requirements
    struct EnhancedBacktestConfig {
        // Test period and data requirements
        std::chrono::system_clock::time_point start_date;
        std::chrono::system_clock::time_point end_date;
        std::vector<std::string> asset_pairs;
        HistoricalDataManager::TimeFrame data_frequency = HistoricalDataManager::TimeFrame::DAY_1;

        // Algorithm parameters
        std::chrono::hours prediction_horizon{24};
        std::chrono::hours rebalancing_frequency{24};
        double minimum_confidence_threshold = 0.6;
        int maximum_positions = 10;
        double position_size_limit = 0.1; // 10% max per position

        // Transaction costs and market impact
        double transaction_cost_bps = 5.0;
        double slippage_bps = 2.0;
        double market_impact_coefficient = 0.001;

        // Risk management
        double maximum_drawdown_limit = 0.20; // 20% max drawdown
        double stop_loss_threshold = 0.05;    // 5% stop loss
        double take_profit_threshold = 0.15;  // 15% take profit

        // Correlation tracking requirements
        double target_correlation = 0.85;     // TRS target
        double minimum_correlation = 0.70;    // Minimum acceptable
        int correlation_measurement_window = 30; // Days for correlation calculation

        // Data quality requirements
        double minimum_data_quality = 0.95;
        double maximum_data_gap_percentage = 0.05;
        bool require_all_pairs = true;

        // Walk-forward validation
        bool enable_walk_forward = true;
        std::chrono::days training_window{180};
        std::chrono::days testing_window{30};
        std::chrono::days step_size{7};

        EnhancedBacktestConfig() = default;
    };

    // Comprehensive backtest results with detailed analytics
    struct EnhancedBacktestResults {
        std::string backtest_id;
        std::chrono::system_clock::time_point execution_time;
        EnhancedBacktestConfig configuration;

        // Core performance metrics
        double total_return = 0.0;
        double annualized_return = 0.0;
        double volatility = 0.0;
        double sharpe_ratio = 0.0;
        double sortino_ratio = 0.0;
        double calmar_ratio = 0.0;
        double maximum_drawdown = 0.0;
        double maximum_drawdown_duration_days = 0.0;

        // Advanced risk metrics
        double value_at_risk_95 = 0.0;        // 95% VaR
        double conditional_var_95 = 0.0;      // Expected shortfall
        double beta_to_market = 0.0;          // Market beta
        double alpha = 0.0;                   // Risk-adjusted excess return
        double information_ratio = 0.0;       // Active return / tracking error
        double tracking_error = 0.0;

        // Algorithm-specific performance
        double prediction_accuracy = 0.0;                    // % correct direction predictions
        double magnitude_accuracy = 0.0;                     // Accuracy of return magnitude predictions
        double correlation_to_predictions = 0.0;             // Correlation between predictions and outcomes
        double average_prediction_confidence = 0.0;          // Average confidence score
        double correlation_stability = 0.0;                  // Stability of correlation over time

        // Trade statistics
        int total_trades = 0;
        int winning_trades = 0;
        int losing_trades = 0;
        double win_rate = 0.0;
        double average_trade_return = 0.0;
        double average_winning_trade = 0.0;
        double average_losing_trade = 0.0;
        double profit_factor = 0.0;            // Gross profit / Gross loss
        double expectancy = 0.0;               // Expected value per trade

        // Temporal performance analysis
        std::vector<std::pair<std::chrono::system_clock::time_point, double>> equity_curve;
        std::vector<std::pair<std::chrono::system_clock::time_point, double>> correlation_timeline;
        std::vector<std::pair<std::chrono::system_clock::time_point, double>> drawdown_timeline;
        std::vector<std::pair<std::chrono::system_clock::time_point, double>> volatility_timeline;

        // Market regime analysis
        std::map<std::string, double> performance_by_regime; // Bull, Bear, Sideways, Volatile
        std::map<std::string, int> trades_by_regime;
        std::map<std::string, double> correlation_by_regime;

        // Asset pair performance breakdown
        std::map<std::string, double> returns_by_pair;
        std::map<std::string, int> trades_by_pair;
        std::map<std::string, double> correlation_by_pair;
        std::map<std::string, double> accuracy_by_pair;

        // Data quality impact analysis
        double data_quality_score = 0.0;
        std::map<std::string, double> data_quality_by_symbol;
        double correlation_data_quality_impact = 0.0;

        // TRS compliance tracking
        bool meets_trs_requirements = false;
        double average_correlation_over_period = 0.0;
        int periods_above_target_correlation = 0;
        int total_periods_measured = 0;
        double correlation_compliance_percentage = 0.0;

        // Walk-forward validation results
        struct WalkForwardPeriod {
            std::chrono::system_clock::time_point training_start;
            std::chrono::system_clock::time_point training_end;
            std::chrono::system_clock::time_point testing_start;
            std::chrono::system_clock::time_point testing_end;
            double period_return;
            double period_correlation;
            double period_accuracy;
            int period_trades;
        };
        std::vector<WalkForwardPeriod> walk_forward_results;

        // Statistical significance testing
        double performance_p_value = 1.0;     // Statistical significance of performance
        double correlation_p_value = 1.0;     // Statistical significance of correlation
        bool performance_statistically_significant = false;
        bool correlation_statistically_significant = false;

        // Benchmark comparison
        struct BenchmarkComparison {
            std::string benchmark_name;
            double benchmark_return;
            double excess_return;
            double tracking_error;
            double information_ratio;
            bool outperformed;
        };
        std::vector<BenchmarkComparison> benchmark_comparisons;

        EnhancedBacktestResults() = default;
    };

    // Individual trade record with comprehensive details
    struct TradeRecord {
        std::string trade_id;
        std::string pair_name;

        // Entry details
        std::chrono::system_clock::time_point entry_time;
        double entry_price;
        double predicted_direction;           // -1, 0, +1
        double predicted_magnitude;           // Expected return
        double entry_confidence;
        double position_size;
        std::string entry_reason;            // Human-readable reason

        // Market context at entry
        std::string market_regime;
        double market_volatility;
        double rsi_at_entry;
        double macd_at_entry;
        double correlation_at_entry;

        // Exit details
        std::chrono::system_clock::time_point exit_time;
        double exit_price;
        std::string exit_reason;             // "target", "stop_loss", "time_limit", "rebalance"

        // Performance
        double actual_return;
        double trade_pnl;
        std::chrono::hours trade_duration;
        bool direction_correct;
        double magnitude_error;

        // Costs
        double transaction_costs;
        double slippage_costs;
        double market_impact_costs;
        double total_costs;

        // Post-trade analysis
        double correlation_during_trade;
        double max_favorable_excursion;      // Best price during trade
        double max_adverse_excursion;        // Worst price during trade

        TradeRecord() : predicted_direction(0), predicted_magnitude(0), entry_confidence(0),
                       position_size(0), market_volatility(0), rsi_at_entry(50), macd_at_entry(0),
                       correlation_at_entry(0), actual_return(0), trade_pnl(0),
                       trade_duration(std::chrono::hours{0}), direction_correct(false),
                       magnitude_error(0), transaction_costs(0), slippage_costs(0),
                       market_impact_costs(0), total_costs(0), correlation_during_trade(0),
                       max_favorable_excursion(0), max_adverse_excursion(0) {}
    };

private:
    std::unique_ptr<HistoricalDataManager> data_manager_;
    std::unique_ptr<RealCorrelationValidator> correlation_validator_;
    std::unique_ptr<RealMarketSignalGenerator> signal_generator_;
    std::unique_ptr<AlgorithmDecisionEngine> decision_engine_;

    // Backtesting state management
    struct BacktestState {
        std::chrono::system_clock::time_point current_time;
        double current_equity;
        double peak_equity;
        double current_drawdown;
        std::map<std::string, double> current_positions;
        std::vector<TradeRecord> trade_history;

        // Correlation tracking
        std::deque<std::pair<std::chrono::system_clock::time_point, double>> correlation_history;
        RealCorrelationValidator::RealTimeCorrelationMonitor correlation_monitor;

        BacktestState(const std::string& monitor_name)
            : current_equity(100000.0), peak_equity(100000.0), current_drawdown(0.0),
              correlation_monitor(monitor_name, 100) {}
    };

    // Backtesting execution methods
    EnhancedBacktestResults runSingleBacktest(const EnhancedBacktestConfig& config);
    std::vector<EnhancedBacktestResults::WalkForwardPeriod> runWalkForwardValidation(const EnhancedBacktestConfig& config);

    // Market data processing
    bool validateDataQuality(const std::map<std::string, std::vector<HistoricalDataManager::HistoricalOHLCV>>& data,
                            const EnhancedBacktestConfig& config);
    std::map<std::string, std::vector<HistoricalDataManager::HistoricalOHLCV>> loadBacktestData(const EnhancedBacktestConfig& config);

    // Trading simulation
    void processTimeStep(BacktestState& state, const EnhancedBacktestConfig& config,
                        const std::map<std::string, HistoricalDataManager::HistoricalOHLCV>& current_data);
    void executeTradeDecisions(BacktestState& state, const EnhancedBacktestConfig& config,
                              const std::vector<AlgorithmDecisionEngine::TradingDecision>& decisions,
                              const std::map<std::string, HistoricalDataManager::HistoricalOHLCV>& current_data);
    void updatePositions(BacktestState& state, const EnhancedBacktestConfig& config,
                        const std::map<std::string, HistoricalDataManager::HistoricalOHLCV>& current_data);

    // Performance calculation
    void calculatePerformanceMetrics(EnhancedBacktestResults& results, const BacktestState& state);
    void calculateRiskMetrics(EnhancedBacktestResults& results, const BacktestState& state);
    void calculateCorrelationMetrics(EnhancedBacktestResults& results, const BacktestState& state);

    // Market regime identification
    std::string identifyMarketRegime(const std::vector<HistoricalDataManager::HistoricalOHLCV>& price_data,
                                   const std::chrono::system_clock::time_point& current_time);

    // Cost calculation
    double calculateTransactionCosts(double position_size, double price, const EnhancedBacktestConfig& config);
    double calculateMarketImpact(double position_size, double volume, const EnhancedBacktestConfig& config);

    // Statistical analysis
    double calculateStatisticalSignificance(const std::vector<double>& returns);
    void performBenchmarkComparison(EnhancedBacktestResults& results, const EnhancedBacktestConfig& config);

public:
    EnhancedBacktestingEngine();
    ~EnhancedBacktestingEngine() = default;

    // Main backtesting interface
    EnhancedBacktestResults runEnhancedBacktest(const EnhancedBacktestConfig& config);

    // Batch backtesting for parameter optimization
    std::vector<EnhancedBacktestResults> runParameterSweep(
        const EnhancedBacktestConfig& base_config,
        const std::map<std::string, std::vector<double>>& parameter_ranges);

    // Monte Carlo simulation for robustness testing
    struct MonteCarloConfig {
        int num_simulations = 1000;
        double price_noise_level = 0.01;     // 1% random noise
        double volume_noise_level = 0.05;    // 5% volume noise
        bool bootstrap_residuals = true;
        int bootstrap_block_size = 10;
    };

    struct MonteCarloResults {
        std::vector<EnhancedBacktestResults> simulation_results;
        double mean_return;
        double return_std_dev;
        double return_percentile_5;
        double return_percentile_95;
        double probability_of_profit;
        double probability_of_meeting_trs;
        double worst_case_drawdown;
        double best_case_return;
    };

    MonteCarloResults runMonteCarloValidation(const EnhancedBacktestConfig& config,
                                            const MonteCarloConfig& mc_config = MonteCarloConfig{});

    // Performance attribution analysis
    struct PerformanceAttribution {
        std::map<std::string, double> pair_contributions;      // Return contribution by pair
        std::map<std::string, double> signal_contributions;    // Return contribution by signal type
        std::map<std::string, double> regime_contributions;    // Return contribution by market regime
        std::map<std::string, double> time_contributions;      // Return contribution by time period

        double market_timing_contribution;   // Value from market timing
        double pair_selection_contribution;  // Value from pair selection
        double signal_quality_contribution;  // Value from signal quality
        double cost_drag;                    // Performance impact of costs
    };

    PerformanceAttribution analyzePerformanceAttribution(const EnhancedBacktestResults& results);

    // Sensitivity analysis
    struct SensitivityAnalysis {
        std::map<std::string, double> parameter_sensitivities; // Sensitivity to each parameter
        std::map<std::string, double> data_sensitivities;      // Sensitivity to data quality issues
        double correlation_target_sensitivity;                 // Sensitivity to TRS target changes
        double cost_sensitivity;                              // Sensitivity to transaction costs
    };

    SensitivityAnalysis performSensitivityAnalysis(const EnhancedBacktestConfig& base_config);

    // Real-time validation support
    struct RealTimeValidationResult {
        bool validation_passed;
        double current_correlation;
        double correlation_p_value;
        std::vector<std::string> validation_issues;
        std::vector<std::string> recommendations;
        std::chrono::system_clock::time_point validation_time;
    };

    RealTimeValidationResult validateAgainstBacktest(
        const EnhancedBacktestResults& backtest_results,
        const std::vector<TradeRecord>& recent_trades,
        int validation_window_days = 30);

    // Reporting and export capabilities
    bool exportBacktestResults(const EnhancedBacktestResults& results,
                              const std::string& filename,
                              const std::string& format = "json");
    bool exportTradeHistory(const std::vector<TradeRecord>& trades,
                           const std::string& filename,
                           const std::string& format = "csv");

    // Configuration and utility methods
    static EnhancedBacktestConfig createStandardConfig(
        const std::chrono::system_clock::time_point& start,
        const std::chrono::system_clock::time_point& end,
        const std::vector<std::string>& pairs);

    static std::vector<std::string> getRecommendedBenchmarks();
    static double calculateAnnualizedReturn(double total_return, int days);
    static double calculateSharpeRatio(double return_rate, double volatility, double risk_free_rate = 0.0);
};

} // namespace Algorithm
} // namespace CryptoClaude