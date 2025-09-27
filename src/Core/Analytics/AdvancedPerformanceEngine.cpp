#include "AdvancedPerformanceEngine.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <random>
#include <chrono>

namespace CryptoClaude {
namespace Analytics {

AdvancedPerformanceEngine::AdvancedPerformanceEngine(const EngineConfig& config)
    : config_(config) {
    // Initialize data structures
    metrics_history_.clear();
    attribution_history_.clear();
    optimization_history_.clear();
    dashboard_history_.clear();
}

AdvancedPerformanceEngine::~AdvancedPerformanceEngine() {
    stopEngine();
}

void AdvancedPerformanceEngine::updateConfig(const EngineConfig& config) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    config_ = config;
}

bool AdvancedPerformanceEngine::integrateRiskManager(std::unique_ptr<Risk::ProductionRiskManager> risk_manager) {
    if (!risk_manager) return false;

    std::lock_guard<std::mutex> lock(data_mutex_);
    risk_manager_ = std::move(risk_manager);
    return true;
}

bool AdvancedPerformanceEngine::integrateOrderManager(std::unique_ptr<Trading::OrderManagementSystem> order_manager) {
    if (!order_manager) return false;

    std::lock_guard<std::mutex> lock(data_mutex_);
    order_manager_ = std::move(order_manager);
    return true;
}

bool AdvancedPerformanceEngine::integrateStrategyManager(std::unique_ptr<Strategy::StrategyManager> strategy_manager) {
    if (!strategy_manager) return false;

    std::lock_guard<std::mutex> lock(data_mutex_);
    strategy_manager_ = std::move(strategy_manager);
    return true;
}

bool AdvancedPerformanceEngine::integrateModelManager(std::unique_ptr<ML::ModelManager> model_manager) {
    if (!model_manager) return false;

    std::lock_guard<std::mutex> lock(data_mutex_);
    model_manager_ = std::move(model_manager);
    return true;
}

bool AdvancedPerformanceEngine::startEngine() {
    if (is_running_.load()) return false;

    is_running_.store(true);

    // Start calculation threads
    calculation_thread_ = std::thread(&AdvancedPerformanceEngine::runCalculationLoop, this);
    optimization_thread_ = std::thread(&AdvancedPerformanceEngine::runOptimizationLoop, this);
    dashboard_thread_ = std::thread(&AdvancedPerformanceEngine::runDashboardLoop, this);

    return true;
}

void AdvancedPerformanceEngine::stopEngine() {
    is_running_.store(false);

    if (calculation_thread_.joinable()) {
        calculation_thread_.join();
    }
    if (optimization_thread_.joinable()) {
        optimization_thread_.join();
    }
    if (dashboard_thread_.joinable()) {
        dashboard_thread_.join();
    }
}

void AdvancedPerformanceEngine::runCalculationLoop() {
    while (is_running_.load()) {
        try {
            // Calculate advanced metrics
            auto metrics = calculateAdvancedMetrics();
            auto attribution = performAttributionAnalysis();

            // Update current data
            {
                std::lock_guard<std::mutex> lock(data_mutex_);
                current_metrics_ = metrics;
                current_attribution_ = attribution;

                // Add to history
                metrics_history_.push_back(metrics);
                attribution_history_.push_back(attribution);

                // Maintain history size (keep last 1000 entries)
                if (metrics_history_.size() > 1000) {
                    metrics_history_.pop_front();
                }
                if (attribution_history_.size() > 1000) {
                    attribution_history_.pop_front();
                }
            }

            // Trigger callbacks
            if (metrics_callback_) {
                metrics_callback_(metrics);
            }
            if (attribution_callback_) {
                attribution_callback_(attribution);
            }

        } catch (const std::exception& e) {
            // Log error and continue
        }

        // Sleep until next calculation
        std::this_thread::sleep_for(config_.calculation_interval);
    }
}

void AdvancedPerformanceEngine::runOptimizationLoop() {
    while (is_running_.load()) {
        try {
            // Run portfolio optimization
            auto optimization = runPortfolioOptimization();

            // Update current data
            {
                std::lock_guard<std::mutex> lock(data_mutex_);
                current_optimization_ = optimization;

                // Add to history
                optimization_history_.push_back(optimization);

                // Maintain history size
                if (optimization_history_.size() > 500) {
                    optimization_history_.pop_front();
                }
            }

            // Trigger callback
            if (optimization_callback_) {
                optimization_callback_(optimization);
            }

        } catch (const std::exception& e) {
            // Log error and continue
        }

        // Sleep until next optimization (run less frequently)
        std::this_thread::sleep_for(std::chrono::minutes(15));
    }
}

void AdvancedPerformanceEngine::runDashboardLoop() {
    while (is_running_.load()) {
        try {
            // Generate dashboard data
            auto dashboard = generateDashboardData();

            // Update current data
            {
                std::lock_guard<std::mutex> lock(data_mutex_);
                current_dashboard_ = dashboard;

                // Add to history
                dashboard_history_.push_back(dashboard);

                // Maintain history size
                if (dashboard_history_.size() > 2000) {
                    dashboard_history_.pop_front();
                }
            }

            // Trigger callback
            if (dashboard_callback_) {
                dashboard_callback_(dashboard);
            }

        } catch (const std::exception& e) {
            // Log error and continue
        }

        // Sleep until next dashboard update
        std::this_thread::sleep_for(config_.dashboard_update_interval);
    }
}

AdvancedPerformanceEngine::AdvancedMetrics AdvancedPerformanceEngine::calculateAdvancedMetrics() {
    std::lock_guard<std::mutex> lock(calculation_mutex_);

    AdvancedMetrics metrics;
    metrics.calculation_time = std::chrono::system_clock::now();

    // Get historical data
    auto returns = getReturnsTimeSeries(config_.lookback_period);
    auto portfolio_values = getPortfolioValues(config_.lookback_period);

    if (returns.empty() || portfolio_values.empty()) {
        return metrics; // Return empty metrics if no data
    }

    // Calculate basic return metrics
    double total_return = 0.0;
    if (portfolio_values.size() >= 2) {
        total_return = (portfolio_values.back() - portfolio_values.front()) / portfolio_values.front();
    }
    metrics.total_return_percent = total_return * 100.0;

    // Annualized return
    double days = static_cast<double>(config_.lookback_period.count()) / 24.0;
    metrics.annualized_return = (std::pow(1.0 + total_return, 365.0 / days) - 1.0) * 100.0;

    // Calculate risk-adjusted returns
    metrics.sharpe_ratio = calculateSharpeRatio(returns);
    metrics.sortino_ratio = calculateSortinoRatio(returns);

    // Drawdown analysis
    metrics.max_drawdown = calculateMaxDrawdown(portfolio_values) * 100.0;

    // Calculate Calmar ratio
    if (metrics.max_drawdown != 0) {
        metrics.calmar_ratio = metrics.annualized_return / std::abs(metrics.max_drawdown);
    }

    // Volatility metrics
    metrics.return_volatility = calculateVolatility(returns) * 100.0;
    metrics.rolling_volatility_30d = calculateVolatility(
        std::vector<double>(returns.end() - std::min(720, static_cast<int>(returns.size())), returns.end())
    ) * 100.0;

    // Trading efficiency metrics
    int winning_trades = 0, losing_trades = 0;
    double total_wins = 0, total_losses = 0;
    double largest_win = 0, largest_loss = 0;

    for (double ret : returns) {
        if (ret > 0) {
            winning_trades++;
            total_wins += ret;
            largest_win = std::max(largest_win, ret);
        } else if (ret < 0) {
            losing_trades++;
            total_losses += ret;
            largest_loss = std::min(largest_loss, ret);
        }
    }

    metrics.total_trades = static_cast<int>(returns.size());
    metrics.winning_trades = winning_trades;
    metrics.losing_trades = losing_trades;

    if (metrics.total_trades > 0) {
        metrics.win_rate = (static_cast<double>(winning_trades) / metrics.total_trades) * 100.0;
    }

    if (winning_trades > 0) {
        metrics.average_win = (total_wins / winning_trades) * 100.0;
    }
    if (losing_trades > 0) {
        metrics.average_loss = (total_losses / losing_trades) * 100.0;
    }

    metrics.largest_win = largest_win * 100.0;
    metrics.largest_loss = largest_loss * 100.0;

    // Profit factor
    if (std::abs(total_losses) > 0) {
        metrics.profit_factor = total_wins / std::abs(total_losses);
    }

    // Expectancy
    if (metrics.total_trades > 0) {
        double avg_win_prob = static_cast<double>(winning_trades) / metrics.total_trades;
        double avg_loss_prob = static_cast<double>(losing_trades) / metrics.total_trades;
        metrics.expectancy = (avg_win_prob * metrics.average_win + avg_loss_prob * metrics.average_loss);
    }

    // Advanced statistics
    metrics.skewness = calculateSkewness(returns);
    metrics.kurtosis = calculateKurtosis(returns);
    metrics.var_95 = calculateVaR(returns, 0.95) * 100.0;
    metrics.var_99 = calculateVaR(returns, 0.99) * 100.0;
    metrics.cvar_95 = calculateCVaR(returns, 0.95) * 100.0;
    metrics.cvar_99 = calculateCVaR(returns, 0.99) * 100.0;

    return metrics;
}

AdvancedPerformanceEngine::AttributionAnalysis AdvancedPerformanceEngine::performAttributionAnalysis() {
    AttributionAnalysis attribution;
    attribution.analysis_time = std::chrono::system_clock::now();

    // Simulate strategy attribution
    std::vector<std::string> strategies = {"MomentumStrategy", "MeanReversionStrategy", "MLStrategy", "ArbitrageStrategy"};

    double total_attribution = 0.0;
    for (const auto& strategy : strategies) {
        double strategy_return = 0.02 + (std::rand() % 1000) / 10000.0; // 2-12% return simulation
        double strategy_allocation = 0.25; // Equal allocation simulation

        attribution.strategy_returns[strategy] = strategy_return * 100.0;
        attribution.strategy_allocation[strategy] = strategy_allocation;
        attribution.strategy_sharpe[strategy] = 1.2 + (std::rand() % 100) / 100.0; // 1.2-2.2 Sharpe simulation

        total_attribution += strategy_return * strategy_allocation;
    }

    // Asset class attribution
    std::vector<std::string> asset_classes = {"BTC", "ETH", "Altcoins", "Stablecoins"};
    for (const auto& asset : asset_classes) {
        attribution.asset_returns[asset] = (std::rand() % 2000 - 1000) / 10000.0 * 100.0; // -10% to +10%
        attribution.asset_weights[asset] = 0.25; // Equal weights simulation
        attribution.asset_contribution[asset] = attribution.asset_returns[asset] * attribution.asset_weights[asset];
    }

    // Risk attribution
    attribution.systematic_risk_contribution = 0.6; // 60% systematic risk
    attribution.idiosyncratic_risk_contribution = 0.4; // 40% idiosyncratic risk
    attribution.concentration_risk = 0.15; // 15% concentration risk
    attribution.liquidity_risk = 0.05; // 5% liquidity risk

    return attribution;
}

AdvancedPerformanceEngine::OptimizationResults AdvancedPerformanceEngine::runPortfolioOptimization() {
    auto start_time = std::chrono::high_resolution_clock::now();

    OptimizationResults results;
    results.optimization_time = std::chrono::system_clock::now();

    // Get asset returns for optimization
    auto asset_returns = getAssetReturns(config_.lookbook_period);

    if (asset_returns.empty()) {
        return results; // Return empty results if no data
    }

    // Current portfolio weights (simulation)
    std::map<std::string, double> current_weights;
    for (const auto& [asset, returns] : asset_returns) {
        current_weights[asset] = 1.0 / asset_returns.size(); // Equal weights as starting point
    }

    // Run optimization
    auto optimal_weights = optimizePortfolio(asset_returns, current_weights);

    // Calculate optimization results
    results.optimal_weights = optimal_weights;
    results.current_weights = current_weights;

    // Calculate weight changes and turnover
    double total_turnover = 0.0;
    for (const auto& [asset, optimal_weight] : optimal_weights) {
        double current_weight = current_weights.count(asset) ? current_weights[asset] : 0.0;
        double weight_change = optimal_weight - current_weight;

        results.weight_changes[asset] = weight_change;
        total_turnover += std::abs(weight_change);
    }
    results.portfolio_turnover = total_turnover;

    // Expected performance metrics (simplified calculations)
    results.expected_return = 0.08 + (std::rand() % 400) / 10000.0; // 8-12% expected return
    results.expected_volatility = 0.12 + (std::rand() % 800) / 10000.0; // 12-20% expected volatility

    if (results.expected_volatility > 0) {
        results.expected_sharpe = (results.expected_return - config_.risk_free_rate) / results.expected_volatility;
    }

    results.expected_max_drawdown = results.expected_volatility * 2.5; // Rough estimate

    // Constraint checks
    results.turnover_constraints_met = results.portfolio_turnover <= config_.max_portfolio_turnover;
    results.weight_constraints_met = true;
    results.risk_constraints_met = results.expected_volatility <= config_.target_volatility;

    for (const auto& [asset, weight] : optimal_weights) {
        if (weight > config_.max_position_weight) {
            results.weight_constraints_met = false;
            break;
        }
    }

    // Optimization quality metrics
    results.optimization_score = results.expected_sharpe * (results.weight_constraints_met ? 1.0 : 0.5);
    results.iterations_used = 100 + (std::rand() % 400); // 100-500 iterations simulation

    auto end_time = std::chrono::high_resolution_clock::now();
    results.computation_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    return results;
}

AdvancedPerformanceEngine::DashboardData AdvancedPerformanceEngine::generateDashboardData() {
    DashboardData dashboard;
    dashboard.snapshot_time = std::chrono::system_clock::now();

    // Get current metrics
    dashboard.current_metrics = current_metrics_;

    // Portfolio status simulation
    dashboard.current_portfolio_value = 1000000.0 + (std::rand() % 200000 - 100000); // $900K - $1.1M
    dashboard.daily_pnl = (std::rand() % 10000 - 5000) / 100.0; // -$50 to +$50
    dashboard.unrealized_pnl = (std::rand() % 20000 - 10000) / 100.0; // -$100 to +$100
    dashboard.realized_pnl = dashboard.daily_pnl - dashboard.unrealized_pnl;

    // Position summary
    dashboard.total_positions = 8 + (std::rand() % 12); // 8-20 positions
    dashboard.long_positions = dashboard.total_positions * 0.6; // 60% long
    dashboard.short_positions = dashboard.total_positions - dashboard.long_positions;
    dashboard.gross_exposure = 1.0 + (std::rand() % 500) / 1000.0; // 100%-150% gross exposure
    dashboard.net_exposure = 0.6 + (std::rand() % 400) / 1000.0; // 60%-100% net exposure
    dashboard.leverage_ratio = dashboard.gross_exposure;

    // Risk metrics
    dashboard.current_var = dashboard.current_portfolio_value * 0.02; // 2% VaR
    dashboard.stress_test_loss = dashboard.current_portfolio_value * 0.15; // 15% stress loss
    dashboard.portfolio_beta = 0.8 + (std::rand() % 400) / 1000.0; // 0.8-1.2 beta

    // Market conditions
    dashboard.market_volatility = 0.15 + (std::rand() % 200) / 1000.0; // 15%-35% volatility
    dashboard.correlation_breakdown = 0.3 + (std::rand() % 400) / 1000.0; // 30%-70% correlation
    dashboard.opportunity_score = 50 + (std::rand() % 500) / 10.0; // 50-100 opportunity score

    // System health
    dashboard.execution_quality_score = 85 + (std::rand() % 150) / 10.0; // 85-100 quality score
    dashboard.average_latency = std::chrono::milliseconds(5 + std::rand() % 45); // 5-50ms latency
    dashboard.data_quality_percentage = 95 + (std::rand() % 50) / 10.0; // 95-100% data quality
    dashboard.active_strategies = 4 + (std::rand() % 4); // 4-8 active strategies

    return dashboard;
}

// Statistical calculation methods
double AdvancedPerformanceEngine::calculateSharpeRatio(const std::vector<double>& returns) {
    if (returns.empty()) return 0.0;

    double mean_return = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
    double volatility = calculateVolatility(returns);

    if (volatility == 0.0) return 0.0;

    return (mean_return * 252 - config_.risk_free_rate) / (volatility * std::sqrt(252));
}

double AdvancedPerformanceEngine::calculateSortinoRatio(const std::vector<double>& returns) {
    if (returns.empty()) return 0.0;

    double mean_return = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();

    // Calculate downside deviation
    double downside_variance = 0.0;
    int downside_count = 0;

    for (double ret : returns) {
        if (ret < 0) {
            downside_variance += ret * ret;
            downside_count++;
        }
    }

    if (downside_count == 0) return std::numeric_limits<double>::infinity();

    double downside_deviation = std::sqrt(downside_variance / downside_count);

    if (downside_deviation == 0.0) return 0.0;

    return (mean_return * 252 - config_.risk_free_rate) / (downside_deviation * std::sqrt(252));
}

double AdvancedPerformanceEngine::calculateMaxDrawdown(const std::vector<double>& values) {
    if (values.empty()) return 0.0;

    double max_drawdown = 0.0;
    double peak = values[0];

    for (double value : values) {
        if (value > peak) {
            peak = value;
        }

        double drawdown = (peak - value) / peak;
        max_drawdown = std::max(max_drawdown, drawdown);
    }

    return max_drawdown;
}

double AdvancedPerformanceEngine::calculateVolatility(const std::vector<double>& returns) {
    if (returns.size() < 2) return 0.0;

    double mean = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();

    double variance = 0.0;
    for (double ret : returns) {
        variance += (ret - mean) * (ret - mean);
    }
    variance /= (returns.size() - 1);

    return std::sqrt(variance);
}

double AdvancedPerformanceEngine::calculateSkewness(const std::vector<double>& returns) {
    if (returns.size() < 3) return 0.0;

    double mean = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
    double variance = calculateVolatility(returns);
    variance *= variance;

    if (variance == 0.0) return 0.0;

    double skewness = 0.0;
    for (double ret : returns) {
        skewness += std::pow((ret - mean), 3);
    }

    skewness /= returns.size();
    skewness /= std::pow(variance, 1.5);

    return skewness;
}

double AdvancedPerformanceEngine::calculateKurtosis(const std::vector<double>& returns) {
    if (returns.size() < 4) return 0.0;

    double mean = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
    double variance = calculateVolatility(returns);
    variance *= variance;

    if (variance == 0.0) return 0.0;

    double kurtosis = 0.0;
    for (double ret : returns) {
        kurtosis += std::pow((ret - mean), 4);
    }

    kurtosis /= returns.size();
    kurtosis /= (variance * variance);
    kurtosis -= 3.0; // Excess kurtosis

    return kurtosis;
}

double AdvancedPerformanceEngine::calculateVaR(const std::vector<double>& returns, double confidence) {
    if (returns.empty()) return 0.0;

    std::vector<double> sorted_returns = returns;
    std::sort(sorted_returns.begin(), sorted_returns.end());

    size_t index = static_cast<size_t>((1.0 - confidence) * sorted_returns.size());
    if (index >= sorted_returns.size()) index = sorted_returns.size() - 1;

    return -sorted_returns[index]; // Return as positive value
}

double AdvancedPerformanceEngine::calculateCVaR(const std::vector<double>& returns, double confidence) {
    if (returns.empty()) return 0.0;

    std::vector<double> sorted_returns = returns;
    std::sort(sorted_returns.begin(), sorted_returns.end());

    size_t cutoff_index = static_cast<size_t>((1.0 - confidence) * sorted_returns.size());
    if (cutoff_index >= sorted_returns.size()) cutoff_index = sorted_returns.size() - 1;

    double cvar = 0.0;
    for (size_t i = 0; i <= cutoff_index; ++i) {
        cvar += sorted_returns[i];
    }

    if (cutoff_index > 0) {
        cvar /= (cutoff_index + 1);
    }

    return -cvar; // Return as positive value
}

// Utility methods for data access
std::vector<double> AdvancedPerformanceEngine::getReturnsTimeSeries(std::chrono::hours period) {
    // Simulate returns time series
    std::vector<double> returns;
    int num_periods = static_cast<int>(period.count() / 24); // Daily returns

    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<> dis(0.001, 0.02); // 0.1% daily return, 2% volatility

    for (int i = 0; i < num_periods; ++i) {
        returns.push_back(dis(gen));
    }

    return returns;
}

std::vector<double> AdvancedPerformanceEngine::getPortfolioValues(std::chrono::hours period) {
    auto returns = getReturnsTimeSeries(period);
    std::vector<double> values;

    double initial_value = 1000000.0; // $1M starting value
    values.push_back(initial_value);

    for (double ret : returns) {
        double new_value = values.back() * (1.0 + ret);
        values.push_back(new_value);
    }

    return values;
}

std::map<std::string, std::vector<double>> AdvancedPerformanceEngine::getAssetReturns(std::chrono::hours period) {
    std::map<std::string, std::vector<double>> asset_returns;
    std::vector<std::string> assets = {"BTC", "ETH", "BNB", "ADA", "DOT"};

    std::random_device rd;
    std::mt19937 gen(rd());

    int num_periods = static_cast<int>(period.count() / 24);

    for (const auto& asset : assets) {
        std::normal_distribution<> dis(0.0005, 0.025 + (std::rand() % 200) / 10000.0); // Varying volatility

        std::vector<double> returns;
        for (int i = 0; i < num_periods; ++i) {
            returns.push_back(dis(gen));
        }
        asset_returns[asset] = returns;
    }

    return asset_returns;
}

// Portfolio optimization
std::map<std::string, double> AdvancedPerformanceEngine::optimizePortfolio(
    const std::map<std::string, std::vector<double>>& returns,
    const std::map<std::string, double>& current_weights) {

    // Simplified mean-variance optimization
    std::map<std::string, double> optimal_weights;

    // Calculate expected returns for each asset
    std::map<std::string, double> expected_returns;
    for (const auto& [asset, asset_returns] : returns) {
        double mean_return = std::accumulate(asset_returns.begin(), asset_returns.end(), 0.0) / asset_returns.size();
        expected_returns[asset] = mean_return * 252; // Annualized
    }

    // Simple optimization: weight by Sharpe ratio
    double total_sharpe = 0.0;
    std::map<std::string, double> asset_sharpe;

    for (const auto& [asset, asset_returns] : returns) {
        double sharpe = calculateSharpeRatio(asset_returns);
        sharpe = std::max(sharpe, 0.1); // Minimum Sharpe ratio
        asset_sharpe[asset] = sharpe;
        total_sharpe += sharpe;
    }

    // Allocate weights based on Sharpe ratios
    for (const auto& [asset, sharpe] : asset_sharpe) {
        double weight = sharpe / total_sharpe;

        // Apply constraints
        weight = std::min(weight, config_.max_position_weight);
        weight = std::max(weight, 0.01); // Minimum 1% allocation

        optimal_weights[asset] = weight;
    }

    // Normalize weights to sum to 1.0
    double total_weight = 0.0;
    for (const auto& [asset, weight] : optimal_weights) {
        total_weight += weight;
    }

    if (total_weight > 0) {
        for (auto& [asset, weight] : optimal_weights) {
            weight /= total_weight;
        }
    }

    return optimal_weights;
}

// Public interface methods
AdvancedPerformanceEngine::AdvancedMetrics AdvancedPerformanceEngine::getCurrentMetrics() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return current_metrics_;
}

AdvancedPerformanceEngine::AttributionAnalysis AdvancedPerformanceEngine::getCurrentAttribution() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return current_attribution_;
}

AdvancedPerformanceEngine::OptimizationResults AdvancedPerformanceEngine::getCurrentOptimization() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return current_optimization_;
}

AdvancedPerformanceEngine::DashboardData AdvancedPerformanceEngine::getCurrentDashboard() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return current_dashboard_;
}

// Manual calculation methods
AdvancedPerformanceEngine::AdvancedMetrics AdvancedPerformanceEngine::calculateMetricsNow() {
    return calculateAdvancedMetrics();
}

AdvancedPerformanceEngine::AttributionAnalysis AdvancedPerformanceEngine::performAttributionNow() {
    return performAttributionAnalysis();
}

AdvancedPerformanceEngine::OptimizationResults AdvancedPerformanceEngine::runOptimizationNow() {
    return runPortfolioOptimization();
}

AdvancedPerformanceEngine::DashboardData AdvancedPerformanceEngine::generateDashboardNow() {
    return generateDashboardData();
}

// Callback registration
void AdvancedPerformanceEngine::setMetricsCallback(std::function<void(const AdvancedMetrics&)> callback) {
    metrics_callback_ = callback;
}

void AdvancedPerformanceEngine::setAttributionCallback(std::function<void(const AttributionAnalysis&)> callback) {
    attribution_callback_ = callback;
}

void AdvancedPerformanceEngine::setOptimizationCallback(std::function<void(const OptimizationResults&)> callback) {
    optimization_callback_ = callback;
}

void AdvancedPerformanceEngine::setDashboardCallback(std::function<void(const DashboardData&)> callback) {
    dashboard_callback_ = callback;
}

// Static utility methods
double AdvancedPerformanceEngine::calculateCorrelation(const std::vector<double>& x, const std::vector<double>& y) {
    if (x.size() != y.size() || x.empty()) return 0.0;

    double mean_x = std::accumulate(x.begin(), x.end(), 0.0) / x.size();
    double mean_y = std::accumulate(y.begin(), y.end(), 0.0) / y.size();

    double numerator = 0.0;
    double sum_sq_x = 0.0;
    double sum_sq_y = 0.0;

    for (size_t i = 0; i < x.size(); ++i) {
        double dx = x[i] - mean_x;
        double dy = y[i] - mean_y;

        numerator += dx * dy;
        sum_sq_x += dx * dx;
        sum_sq_y += dy * dy;
    }

    double denominator = std::sqrt(sum_sq_x * sum_sq_y);
    if (denominator == 0.0) return 0.0;

    return numerator / denominator;
}

double AdvancedPerformanceEngine::calculateBeta(const std::vector<double>& returns, const std::vector<double>& market_returns) {
    if (returns.size() != market_returns.size() || returns.empty()) return 1.0;

    double correlation = calculateCorrelation(returns, market_returns);
    double portfolio_vol = calculateVolatility(returns);
    double market_vol = calculateVolatility(market_returns);

    if (market_vol == 0.0) return 1.0;

    return correlation * (portfolio_vol / market_vol);
}

std::string AdvancedPerformanceEngine::formatPerformanceMetrics(const AdvancedMetrics& metrics) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);

    oss << "=== PERFORMANCE METRICS ===\n";
    oss << "Total Return: " << metrics.total_return_percent << "%\n";
    oss << "Annualized Return: " << metrics.annualized_return << "%\n";
    oss << "Sharpe Ratio: " << metrics.sharpe_ratio << "\n";
    oss << "Sortino Ratio: " << metrics.sortino_ratio << "\n";
    oss << "Max Drawdown: " << metrics.max_drawdown << "%\n";
    oss << "Volatility: " << metrics.return_volatility << "%\n";
    oss << "Win Rate: " << metrics.win_rate << "%\n";
    oss << "Profit Factor: " << metrics.profit_factor << "\n";
    oss << "VaR (95%): " << metrics.var_95 << "%\n";
    oss << "CVaR (95%): " << metrics.cvar_95 << "%\n";

    return oss.str();
}

} // namespace Analytics
} // namespace CryptoClaude