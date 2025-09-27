#pragma once

#include "../ML/EnsembleMLPredictor.h"
#include "../Analytics/AdvancedPerformanceEngine.h"
#include "../Risk/ProductionRiskManager.h"
#include <memory>
#include <vector>
#include <map>
#include <string>
#include <chrono>
#include <atomic>
#include <mutex>
#include <thread>
#include <functional>
#include <random>

namespace CryptoClaude {
namespace Strategy {

/**
 * Trading Strategy Optimizer - Day 24 Implementation
 * ML-powered strategy optimization and parameter tuning system
 * Automatically optimizes trading strategies for maximum risk-adjusted returns
 */
class TradingStrategyOptimizer {
public:
    // Strategy parameter definition
    struct ParameterDefinition {
        std::string name;
        std::string type;           // "double", "int", "bool", "string"
        double min_value;           // For numeric parameters
        double max_value;
        double step_size;           // Optimization step size
        std::vector<std::string> discrete_values; // For string/discrete parameters
        double current_value;
        bool is_optimizable;        // Whether this parameter should be optimized

        ParameterDefinition() : min_value(0), max_value(1), step_size(0.1),
                              current_value(0), is_optimizable(true) {}
    };

    // Strategy configuration
    struct StrategyConfig {
        std::string strategy_id;
        std::string strategy_name;
        std::string strategy_type;  // "momentum", "mean_reversion", "ml", "arbitrage"
        std::map<std::string, ParameterDefinition> parameters;

        // Performance targets
        double target_sharpe_ratio = 2.0;
        double target_max_drawdown = 0.10;      // 10% max drawdown
        double target_volatility = 0.15;        // 15% target volatility
        double min_return_threshold = 0.08;     // 8% minimum annual return

        // Constraints
        double max_position_size = 0.20;        // 20% max position size
        double max_leverage = 2.0;              // 2x max leverage
        int max_concurrent_positions = 10;
        std::chrono::minutes min_holding_period{30};
        std::chrono::hours max_holding_period{168}; // 1 week

        StrategyConfig() = default;
    };

    // Optimization result
    struct OptimizationResult {
        std::chrono::system_clock::time_point optimization_time;
        std::string strategy_id;

        // Optimal parameters
        std::map<std::string, double> optimal_parameters;
        std::map<std::string, double> original_parameters;
        std::map<std::string, double> parameter_improvements;

        // Performance metrics
        double optimal_sharpe_ratio;
        double optimal_return;
        double optimal_volatility;
        double optimal_max_drawdown;
        double optimal_profit_factor;
        double optimal_win_rate;

        // Improvement metrics
        double sharpe_improvement;
        double return_improvement;
        double drawdown_improvement;
        double volatility_improvement;

        // Optimization quality
        int iterations_performed;
        int evaluations_performed;
        std::chrono::minutes optimization_duration;
        double convergence_score;
        std::string optimization_method;

        // Statistical significance
        double confidence_level;
        bool is_statistically_significant;
        double p_value;
        int sample_size;

        // Risk assessment
        bool meets_risk_constraints;
        bool meets_return_targets;
        std::vector<std::string> constraint_violations;
        std::string risk_assessment;

        OptimizationResult() : optimal_sharpe_ratio(0), optimal_return(0),
                              optimal_volatility(0), optimal_max_drawdown(0),
                              optimal_profit_factor(0), optimal_win_rate(0),
                              sharpe_improvement(0), return_improvement(0),
                              drawdown_improvement(0), volatility_improvement(0),
                              iterations_performed(0), evaluations_performed(0),
                              optimization_duration(std::chrono::minutes{0}),
                              convergence_score(0), confidence_level(0),
                              is_statistically_significant(false), p_value(1.0),
                              sample_size(0), meets_risk_constraints(false),
                              meets_return_targets(false) {}
    };

    // ML-based parameter suggestions
    struct MLParameterSuggestion {
        std::chrono::system_clock::time_point suggestion_time;
        std::string strategy_id;

        // ML predictions
        std::map<std::string, double> predicted_optimal_parameters;
        std::map<std::string, double> parameter_confidence;
        double expected_performance_improvement;
        std::string ml_model_used;
        double model_accuracy;

        // Feature importance
        std::map<std::string, double> feature_importance;
        std::map<std::string, double> market_condition_weights;

        // Risk assessment
        double predicted_risk_score;
        std::vector<std::string> risk_factors;
        std::string market_regime_prediction;

        MLParameterSuggestion() : expected_performance_improvement(0),
                                model_accuracy(0), predicted_risk_score(0) {}
    };

    // Backtesting framework
    struct BacktestConfig {
        std::chrono::system_clock::time_point start_date;
        std::chrono::system_clock::time_point end_date;
        double initial_capital = 1000000.0;    // $1M initial capital
        double transaction_costs = 0.001;      // 0.1% transaction costs
        bool include_slippage = true;
        double slippage_bps = 5.0;             // 5 bps slippage
        bool use_realistic_fills = true;
        int warm_up_period_days = 30;          // 30-day warm-up period

        BacktestConfig() {
            auto now = std::chrono::system_clock::now();
            end_date = now - std::chrono::hours{24}; // End 1 day ago
            start_date = end_date - std::chrono::hours{24 * 365}; // 1 year backtest
        }
    };

    struct BacktestResult {
        std::chrono::system_clock::time_point test_time;
        BacktestConfig config_used;

        // Performance metrics
        Analytics::AdvancedPerformanceEngine::AdvancedMetrics performance;

        // Trade statistics
        int total_trades;
        int winning_trades;
        int losing_trades;
        double average_trade_return;
        double best_trade_return;
        double worst_trade_return;
        std::chrono::minutes average_trade_duration;

        // Risk metrics
        double maximum_drawdown_duration_days;
        double value_at_risk_95;
        double expected_shortfall_95;
        double calmar_ratio;
        double sterling_ratio;

        // Efficiency metrics
        double profit_per_trade;
        double trades_per_month;
        double capital_utilization;
        double return_on_risk;

        BacktestResult() : total_trades(0), winning_trades(0), losing_trades(0),
                          average_trade_return(0), best_trade_return(0),
                          worst_trade_return(0), average_trade_duration(std::chrono::minutes{0}),
                          maximum_drawdown_duration_days(0), value_at_risk_95(0),
                          expected_shortfall_95(0), calmar_ratio(0), sterling_ratio(0),
                          profit_per_trade(0), trades_per_month(0),
                          capital_utilization(0), return_on_risk(0) {}
    };

    // Optimization methods
    enum class OptimizationMethod {
        GRID_SEARCH,                // Exhaustive grid search
        RANDOM_SEARCH,             // Random parameter sampling
        GENETIC_ALGORITHM,         // Genetic algorithm optimization
        BAYESIAN_OPTIMIZATION,     // Bayesian optimization
        PARTICLE_SWARM,           // Particle swarm optimization
        SIMULATED_ANNEALING,      // Simulated annealing
        ML_GUIDED                 // ML-guided optimization
    };

private:
    // Configuration
    struct OptimizerConfig {
        OptimizationMethod default_method = OptimizationMethod::BAYESIAN_OPTIMIZATION;
        int max_iterations = 1000;
        int max_evaluations = 5000;
        std::chrono::hours max_optimization_time{6}; // 6-hour limit
        double convergence_threshold = 0.001;
        int parallel_evaluations = 4;          // Number of parallel backtests

        // Genetic algorithm parameters
        int population_size = 50;
        double mutation_rate = 0.1;
        double crossover_rate = 0.8;
        int elite_size = 5;

        // Bayesian optimization parameters
        int acquisition_samples = 1000;
        std::string acquisition_function = "expected_improvement";
        double exploration_factor = 0.1;

        OptimizerConfig() {}
    } config_;

    // State management
    std::atomic<bool> is_optimizing_{false};
    mutable std::mutex optimization_mutex_;
    mutable std::mutex results_mutex_;

    // Data storage
    std::map<std::string, StrategyConfig> strategy_configs_;
    std::map<std::string, OptimizationResult> latest_results_;
    std::map<std::string, std::vector<OptimizationResult>> optimization_history_;
    std::map<std::string, MLParameterSuggestion> ml_suggestions_;

    // Component integrations
    std::unique_ptr<ML::EnsembleMLPredictor> model_manager_;
    std::unique_ptr<Analytics::AdvancedPerformanceEngine> performance_engine_;
    std::unique_ptr<Risk::ProductionRiskManager> risk_manager_;

    // Threading
    std::vector<std::thread> optimization_threads_;

    // Callback functions
    std::function<void(const OptimizationResult&)> optimization_complete_callback_;
    std::function<void(const std::string&, double)> progress_callback_;
    std::function<void(const MLParameterSuggestion&)> ml_suggestion_callback_;

    // Optimization algorithms
    OptimizationResult runGridSearch(const StrategyConfig& config);
    OptimizationResult runRandomSearch(const StrategyConfig& config);
    OptimizationResult runGeneticAlgorithm(const StrategyConfig& config);
    OptimizationResult runBayesianOptimization(const StrategyConfig& config);
    OptimizationResult runParticleSwarm(const StrategyConfig& config);
    OptimizationResult runSimulatedAnnealing(const StrategyConfig& config);
    OptimizationResult runMLGuidedOptimization(const StrategyConfig& config);

    // Backtesting methods
    BacktestResult runBacktest(const StrategyConfig& config, const BacktestConfig& backtest_config);
    double evaluateParameterSet(const StrategyConfig& config, const std::map<std::string, double>& parameters);

    // ML integration methods
    MLParameterSuggestion generateMLSuggestions(const std::string& strategy_id);
    void trainOptimizationModel(const std::string& strategy_id);
    double predictPerformance(const StrategyConfig& config, const std::map<std::string, double>& parameters);

    // Utility methods
    std::vector<std::map<std::string, double>> generateParameterGrid(const StrategyConfig& config);
    std::map<std::string, double> generateRandomParameters(const StrategyConfig& config);
    std::map<std::string, double> mutateParameters(const std::map<std::string, double>& parameters,
                                                  const StrategyConfig& config, double mutation_rate);
    double calculateFitnessScore(const BacktestResult& result);
    bool isValidParameterSet(const StrategyConfig& config, const std::map<std::string, double>& parameters);

    // Statistical analysis
    bool isStatisticallySignificant(const OptimizationResult& result);
    double calculateConfidenceLevel(const std::vector<double>& sample1, const std::vector<double>& sample2);

public:
    explicit TradingStrategyOptimizer(const OptimizerConfig& config = OptimizerConfig());
    ~TradingStrategyOptimizer();

    // Configuration management
    void updateConfig(const OptimizerConfig& config);
    const OptimizerConfig& getConfig() const { return config_; }

    // Component integration
    bool integrateModelManager(std::unique_ptr<ML::EnsembleMLPredictor> model_manager);
    bool integratePerformanceEngine(std::unique_ptr<Analytics::AdvancedPerformanceEngine> performance_engine);
    bool integrateRiskManager(std::unique_ptr<Risk::ProductionRiskManager> risk_manager);

    // Strategy management
    bool addStrategy(const StrategyConfig& config);
    bool updateStrategy(const std::string& strategy_id, const StrategyConfig& config);
    bool removeStrategy(const std::string& strategy_id);
    std::vector<std::string> getStrategyIds() const;
    StrategyConfig getStrategyConfig(const std::string& strategy_id) const;

    // Optimization operations
    OptimizationResult optimizeStrategy(const std::string& strategy_id,
                                       OptimizationMethod method = OptimizationMethod::BAYESIAN_OPTIMIZATION);
    std::vector<OptimizationResult> optimizeAllStrategies();
    void optimizeStrategyAsync(const std::string& strategy_id,
                              OptimizationMethod method = OptimizationMethod::BAYESIAN_OPTIMIZATION);

    // Results access
    OptimizationResult getLatestResult(const std::string& strategy_id) const;
    std::vector<OptimizationResult> getOptimizationHistory(const std::string& strategy_id) const;
    std::map<std::string, OptimizationResult> getAllLatestResults() const;

    // ML-based suggestions
    MLParameterSuggestion getMLSuggestions(const std::string& strategy_id);
    void updateMLModel(const std::string& strategy_id);
    double getModelAccuracy(const std::string& strategy_id) const;

    // Backtesting
    BacktestResult backtest(const std::string& strategy_id, const BacktestConfig& config);
    BacktestResult backtestWithParameters(const std::string& strategy_id,
                                         const std::map<std::string, double>& parameters,
                                         const BacktestConfig& config);

    // Analysis and reporting
    struct OptimizationSummary {
        std::chrono::system_clock::time_point summary_time;
        int total_strategies;
        int optimized_strategies;
        int improved_strategies;

        double average_sharpe_improvement;
        double average_return_improvement;
        double average_drawdown_improvement;

        std::string best_performing_strategy;
        double best_sharpe_ratio;
        std::string most_improved_strategy;
        double largest_improvement;

        std::vector<std::string> recommendations;

        OptimizationSummary() : total_strategies(0), optimized_strategies(0),
                               improved_strategies(0), average_sharpe_improvement(0),
                               average_return_improvement(0), average_drawdown_improvement(0),
                               best_sharpe_ratio(0), largest_improvement(0) {}
    };

    OptimizationSummary generateOptimizationSummary() const;

    // Performance comparison
    struct PerformanceComparison {
        std::string strategy_id;
        Analytics::AdvancedPerformanceEngine::AdvancedMetrics before_optimization;
        Analytics::AdvancedPerformanceEngine::AdvancedMetrics after_optimization;
        double improvement_percentage;
        std::string improvement_summary;
    };

    std::vector<PerformanceComparison> comparePerformance() const;

    // Control and monitoring
    bool isOptimizing() const { return is_optimizing_.load(); }
    void stopOptimization();
    double getOptimizationProgress(const std::string& strategy_id) const;

    // Callback registration
    void setOptimizationCompleteCallback(std::function<void(const OptimizationResult&)> callback);
    void setProgressCallback(std::function<void(const std::string&, double)> callback);
    void setMLSuggestionCallback(std::function<void(const MLParameterSuggestion&)> callback);

    // Data export
    bool exportResults(const std::string& filename, const std::string& format = "json") const;
    bool exportBacktestResults(const BacktestResult& result, const std::string& filename) const;
    bool exportOptimizationHistory(const std::string& strategy_id, const std::string& filename) const;

    // Static utility methods
    static std::string optimizationMethodToString(OptimizationMethod method);
    static OptimizationMethod stringToOptimizationMethod(const std::string& method_str);
    static StrategyConfig createDefaultConfig(const std::string& strategy_type);
    static std::vector<ParameterDefinition> getCommonParameters(const std::string& strategy_type);
};

} // namespace Strategy
} // namespace CryptoClaude