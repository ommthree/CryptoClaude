#pragma once

#include "../ML/EnsembleMLPredictor.h"
#include "../Risk/ProductionRiskManager.h"
#include "../Trading/OrderManagementSystem.h"
#include "../Strategy/EnhancedSignalProcessor.h"
#include <memory>
#include <vector>
#include <map>
#include <string>
#include <chrono>
#include <atomic>
#include <mutex>
#include <thread>
#include <deque>
#include <functional>
#include <numeric>

namespace CryptoClaude {
namespace Analytics {

/**
 * Advanced Performance Engine - Day 24 Implementation
 * Comprehensive analytics engine for trading system optimization
 * Provides real-time performance analysis and strategy optimization
 */
class AdvancedPerformanceEngine {
public:
    // Advanced performance metrics structure
    struct AdvancedMetrics {
        std::chrono::system_clock::time_point calculation_time;

        // Profitability metrics
        double total_return_percent;
        double annualized_return;
        double monthly_return;
        double weekly_return;
        double daily_return;

        // Risk-adjusted returns
        double sharpe_ratio;
        double sortino_ratio;
        double calmar_ratio;
        double information_ratio;
        double treynor_ratio;

        // Drawdown analysis
        double max_drawdown;
        double current_drawdown;
        double average_drawdown;
        std::chrono::hours max_drawdown_duration;
        std::chrono::hours current_drawdown_duration;

        // Volatility metrics
        double return_volatility;
        double price_volatility;
        double rolling_volatility_30d;
        double volatility_of_volatility;

        // Trading efficiency
        double win_rate;
        double profit_factor;
        double expectancy;
        double average_win;
        double average_loss;
        double largest_win;
        double largest_loss;

        // Trade frequency and timing
        int total_trades;
        int winning_trades;
        int losing_trades;
        double average_trade_duration_hours;
        double trades_per_day;

        // Market correlation
        double market_correlation;
        double beta_to_market;
        double alpha_generation;
        double tracking_error;

        // Advanced statistics
        double skewness;
        double kurtosis;
        double var_95;
        double var_99;
        double cvar_95;
        double cvar_99;

        AdvancedMetrics() : total_return_percent(0), annualized_return(0), monthly_return(0),
                           weekly_return(0), daily_return(0), sharpe_ratio(0), sortino_ratio(0),
                           calmar_ratio(0), information_ratio(0), treynor_ratio(0),
                           max_drawdown(0), current_drawdown(0), average_drawdown(0),
                           max_drawdown_duration(std::chrono::hours{0}),
                           current_drawdown_duration(std::chrono::hours{0}),
                           return_volatility(0), price_volatility(0), rolling_volatility_30d(0),
                           volatility_of_volatility(0), win_rate(0), profit_factor(0),
                           expectancy(0), average_win(0), average_loss(0), largest_win(0),
                           largest_loss(0), total_trades(0), winning_trades(0), losing_trades(0),
                           average_trade_duration_hours(0), trades_per_day(0), market_correlation(0),
                           beta_to_market(0), alpha_generation(0), tracking_error(0),
                           skewness(0), kurtosis(0), var_95(0), var_99(0), cvar_95(0), cvar_99(0) {}
    };

    // Performance attribution analysis
    struct AttributionAnalysis {
        std::chrono::system_clock::time_point analysis_time;

        // Strategy contribution
        std::map<std::string, double> strategy_returns;
        std::map<std::string, double> strategy_sharpe;
        std::map<std::string, double> strategy_allocation;

        // Asset class contribution
        std::map<std::string, double> asset_returns;
        std::map<std::string, double> asset_weights;
        std::map<std::string, double> asset_contribution;

        // Factor analysis
        std::map<std::string, double> factor_exposure;
        std::map<std::string, double> factor_returns;
        std::map<std::string, double> factor_contribution;

        // Time-based attribution
        std::map<std::string, double> monthly_attribution;
        std::map<std::string, double> weekly_attribution;
        std::map<std::string, double> daily_attribution;

        // Risk attribution
        double systematic_risk_contribution;
        double idiosyncratic_risk_contribution;
        double concentration_risk;
        double liquidity_risk;

        AttributionAnalysis() : systematic_risk_contribution(0),
                              idiosyncratic_risk_contribution(0),
                              concentration_risk(0), liquidity_risk(0) {}
    };

    // Portfolio optimization results
    struct OptimizationResults {
        std::chrono::system_clock::time_point optimization_time;

        // Optimal allocation
        std::map<std::string, double> optimal_weights;
        std::map<std::string, double> current_weights;
        std::map<std::string, double> weight_changes;

        // Expected performance
        double expected_return;
        double expected_volatility;
        double expected_sharpe;
        double expected_max_drawdown;

        // Constraint satisfaction
        bool weight_constraints_met;
        bool risk_constraints_met;
        bool turnover_constraints_met;
        double portfolio_turnover;

        // Optimization quality
        double optimization_score;
        int iterations_used;
        std::chrono::milliseconds computation_time;

        // Risk decomposition
        std::map<std::string, double> risk_contribution;
        double portfolio_var;
        double diversification_ratio;

        OptimizationResults() : expected_return(0), expected_volatility(0),
                               expected_sharpe(0), expected_max_drawdown(0),
                               weight_constraints_met(false), risk_constraints_met(false),
                               turnover_constraints_met(false), portfolio_turnover(0),
                               optimization_score(0), iterations_used(0),
                               computation_time(std::chrono::milliseconds{0}),
                               portfolio_var(0), diversification_ratio(0) {}
    };

    // Real-time dashboard data
    struct DashboardData {
        std::chrono::system_clock::time_point snapshot_time;

        // Current performance
        AdvancedMetrics current_metrics;

        // Live trading status
        double current_portfolio_value;
        double daily_pnl;
        double unrealized_pnl;
        double realized_pnl;

        // Position summary
        int total_positions;
        int long_positions;
        int short_positions;
        double gross_exposure;
        double net_exposure;
        double leverage_ratio;

        // Risk monitoring
        double current_var;
        double stress_test_loss;
        double portfolio_beta;
        Risk::ProductionRiskManager::RealTimeRiskAssessment::RiskLevel risk_status;

        // Market conditions
        double market_volatility;
        double correlation_breakdown;
        std::string market_regime;
        double opportunity_score;

        // System health
        double execution_quality_score;
        std::chrono::milliseconds average_latency;
        double data_quality_percentage;
        int active_strategies;

        DashboardData() : current_portfolio_value(0), daily_pnl(0), unrealized_pnl(0),
                         realized_pnl(0), total_positions(0), long_positions(0),
                         short_positions(0), gross_exposure(0), net_exposure(0),
                         leverage_ratio(0), current_var(0), stress_test_loss(0),
                         portfolio_beta(0), risk_status(Risk::ProductionRiskManager::RealTimeRiskAssessment::RiskLevel::GREEN),
                         market_volatility(0), correlation_breakdown(0), opportunity_score(0),
                         execution_quality_score(0), average_latency(std::chrono::milliseconds{0}),
                         data_quality_percentage(0), active_strategies(0) {}
    };

private:
    // Configuration
    struct EngineConfig {
        std::chrono::minutes calculation_interval{5};        // Recalculate every 5 minutes
        std::chrono::hours lookback_period{24 * 30};         // 30 days lookback
        double risk_free_rate = 0.02;                        // 2% risk-free rate
        double market_return = 0.08;                         // 8% market return

        // Optimization parameters
        int max_optimization_iterations = 1000;
        double convergence_tolerance = 1e-6;
        double max_portfolio_turnover = 0.50;                // 50% max turnover

        // Risk thresholds
        double max_position_weight = 0.20;                   // 20% max single position
        double max_sector_weight = 0.40;                     // 40% max sector exposure
        double target_volatility = 0.15;                     // 15% target volatility

        EngineConfig() {}
    } config_;

    // State management
    std::atomic<bool> is_running_{false};
    mutable std::mutex data_mutex_;
    mutable std::mutex calculation_mutex_;

    // Data storage
    std::deque<AdvancedMetrics> metrics_history_;
    std::deque<AttributionAnalysis> attribution_history_;
    std::deque<OptimizationResults> optimization_history_;
    std::deque<DashboardData> dashboard_history_;

    AdvancedMetrics current_metrics_;
    AttributionAnalysis current_attribution_;
    OptimizationResults current_optimization_;
    DashboardData current_dashboard_;

    // Threading
    std::thread calculation_thread_;
    std::thread optimization_thread_;
    std::thread dashboard_thread_;

    // Component integrations
    std::unique_ptr<Risk::ProductionRiskManager> risk_manager_;
    std::unique_ptr<Trading::OrderManagementSystem> order_manager_;
    std::unique_ptr<Strategy::EnhancedSignalProcessor> strategy_manager_;
    std::unique_ptr<ML::EnsembleMLPredictor> model_manager_;

    // Callback functions
    std::function<void(const AdvancedMetrics&)> metrics_callback_;
    std::function<void(const AttributionAnalysis&)> attribution_callback_;
    std::function<void(const OptimizationResults&)> optimization_callback_;
    std::function<void(const DashboardData&)> dashboard_callback_;

    // Core calculation methods
    void runCalculationLoop();
    void runOptimizationLoop();
    void runDashboardLoop();

    // Metrics calculation
    AdvancedMetrics calculateAdvancedMetrics();
    AttributionAnalysis performAttributionAnalysis();
    OptimizationResults runPortfolioOptimization();
    DashboardData generateDashboardData();

    // Statistical calculations
    double calculateSharpeRatio(const std::vector<double>& returns);
    double calculateSortinoRatio(const std::vector<double>& returns);
    double calculateMaxDrawdown(const std::vector<double>& values);
    double calculateVolatility(const std::vector<double>& returns);
    double calculateSkewness(const std::vector<double>& returns);
    double calculateKurtosis(const std::vector<double>& returns);
    double calculateVaR(const std::vector<double>& returns, double confidence);
    double calculateCVaR(const std::vector<double>& returns, double confidence);

    // Optimization algorithms
    std::map<std::string, double> optimizePortfolio(const std::map<std::string, std::vector<double>>& returns,
                                                   const std::map<std::string, double>& current_weights);
    double calculatePortfolioRisk(const std::map<std::string, double>& weights,
                                 const std::vector<std::vector<double>>& covariance_matrix);

    // Performance attribution
    void calculateStrategyAttribution();
    void calculateAssetAttribution();
    void calculateFactorAttribution();

    // Utility methods
    std::vector<double> getReturnsTimeSeries(std::chrono::hours period);
    std::vector<double> getPortfolioValues(std::chrono::hours period);
    std::map<std::string, std::vector<double>> getAssetReturns(std::chrono::hours period);

public:
    explicit AdvancedPerformanceEngine(const EngineConfig& config = EngineConfig());
    ~AdvancedPerformanceEngine();

    // Configuration
    void updateConfig(const EngineConfig& config);
    const EngineConfig& getConfig() const { return config_; }

    // Component integration
    bool integrateRiskManager(std::unique_ptr<Risk::ProductionRiskManager> risk_manager);
    bool integrateOrderManager(std::unique_ptr<Trading::OrderManagementSystem> order_manager);
    bool integrateStrategyManager(std::unique_ptr<Strategy::EnhancedSignalProcessor> strategy_manager);
    bool integrateModelManager(std::unique_ptr<ML::EnsembleMLPredictor> model_manager);

    // Engine control
    bool startEngine();
    void stopEngine();
    bool isRunning() const { return is_running_.load(); }

    // Data access
    AdvancedMetrics getCurrentMetrics() const;
    AttributionAnalysis getCurrentAttribution() const;
    OptimizationResults getCurrentOptimization() const;
    DashboardData getCurrentDashboard() const;

    // Historical data
    std::vector<AdvancedMetrics> getMetricsHistory(std::chrono::hours lookback = std::chrono::hours{168}) const;
    std::vector<AttributionAnalysis> getAttributionHistory(std::chrono::hours lookback = std::chrono::hours{168}) const;
    std::vector<OptimizationResults> getOptimizationHistory(std::chrono::hours lookback = std::chrono::hours{168}) const;
    std::vector<DashboardData> getDashboardHistory(std::chrono::hours lookback = std::chrono::hours{168}) const;

    // Manual calculations
    AdvancedMetrics calculateMetricsNow();
    AttributionAnalysis performAttributionNow();
    OptimizationResults runOptimizationNow();
    DashboardData generateDashboardNow();

    // Advanced analytics
    struct TrendAnalysis {
        std::chrono::system_clock::time_point analysis_time;

        // Performance trends
        double return_trend_7d;
        double return_trend_30d;
        double volatility_trend_7d;
        double volatility_trend_30d;
        double sharpe_trend_7d;
        double sharpe_trend_30d;

        // Risk trends
        double var_trend;
        double drawdown_trend;
        double correlation_trend;

        // Trading efficiency trends
        double win_rate_trend;
        double profit_factor_trend;
        double expectancy_trend;

        // Predictive indicators
        double momentum_score;
        double mean_reversion_score;
        double regime_stability;
        std::string predicted_regime;
        double confidence_level;

        TrendAnalysis() : return_trend_7d(0), return_trend_30d(0), volatility_trend_7d(0),
                         volatility_trend_30d(0), sharpe_trend_7d(0), sharpe_trend_30d(0),
                         var_trend(0), drawdown_trend(0), correlation_trend(0),
                         win_rate_trend(0), profit_factor_trend(0), expectancy_trend(0),
                         momentum_score(0), mean_reversion_score(0), regime_stability(0),
                         confidence_level(0) {}
    };

    TrendAnalysis analyzeTrends() const;

    // Benchmarking
    struct BenchmarkComparison {
        std::chrono::system_clock::time_point comparison_time;

        // Return comparison
        double portfolio_return;
        double benchmark_return;
        double excess_return;
        double tracking_error;
        double information_ratio;

        // Risk comparison
        double portfolio_volatility;
        double benchmark_volatility;
        double portfolio_sharpe;
        double benchmark_sharpe;
        double portfolio_max_dd;
        double benchmark_max_dd;

        // Relative performance
        double upside_capture;
        double downside_capture;
        double batting_average;
        double up_market_capture;
        double down_market_capture;

        BenchmarkComparison() : portfolio_return(0), benchmark_return(0), excess_return(0),
                               tracking_error(0), information_ratio(0), portfolio_volatility(0),
                               benchmark_volatility(0), portfolio_sharpe(0), benchmark_sharpe(0),
                               portfolio_max_dd(0), benchmark_max_dd(0), upside_capture(0),
                               downside_capture(0), batting_average(0), up_market_capture(0),
                               down_market_capture(0) {}
    };

    BenchmarkComparison compareToBenchmark(const std::vector<double>& benchmark_returns) const;

    // Callback registration
    void setMetricsCallback(std::function<void(const AdvancedMetrics&)> callback);
    void setAttributionCallback(std::function<void(const AttributionAnalysis&)> callback);
    void setOptimizationCallback(std::function<void(const OptimizationResults&)> callback);
    void setDashboardCallback(std::function<void(const DashboardData&)> callback);

    // Data export
    bool exportMetrics(const std::string& filename, const std::string& format = "csv",
                      std::chrono::hours lookback = std::chrono::hours{168}) const;
    bool exportAttribution(const std::string& filename, const std::string& format = "json",
                          std::chrono::hours lookback = std::chrono::hours{168}) const;
    bool exportOptimization(const std::string& filename, const std::string& format = "csv",
                           std::chrono::hours lookback = std::chrono::hours{168}) const;
    bool exportDashboard(const std::string& filename, const std::string& format = "json") const;

    // Report generation
    struct ComprehensiveReport {
        std::chrono::system_clock::time_point report_time;
        std::chrono::hours report_period;

        AdvancedMetrics performance_summary;
        AttributionAnalysis attribution_summary;
        OptimizationResults optimization_summary;
        TrendAnalysis trend_summary;
        BenchmarkComparison benchmark_summary;

        std::vector<std::string> key_insights;
        std::vector<std::string> recommendations;
        std::vector<std::string> risk_warnings;

        ComprehensiveReport() : report_period(std::chrono::hours{168}) {}
    };

    ComprehensiveReport generateComprehensiveReport(std::chrono::hours period = std::chrono::hours{168}) const;

    // Static utility methods
    static double calculateCorrelation(const std::vector<double>& x, const std::vector<double>& y);
    static double calculateBeta(const std::vector<double>& returns, const std::vector<double>& market_returns);
    static std::vector<std::vector<double>> calculateCovarianceMatrix(const std::map<std::string, std::vector<double>>& returns);
    static std::string formatPerformanceMetrics(const AdvancedMetrics& metrics);
};

} // namespace Analytics
} // namespace CryptoClaude