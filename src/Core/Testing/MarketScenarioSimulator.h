#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <chrono>
#include <random>
#include "../Analytics/BacktestingEngine.h"
#include "../Trading/PaperTradingEngine.h"

namespace CryptoClaude {
namespace Testing {

/**
 * Market scenario parameters for simulation
 */
struct ScenarioParameters {
    std::string scenario_name;
    std::string description;

    // Price Movement Parameters
    double trend_strength; // Daily trend (positive = bullish, negative = bearish)
    double volatility_multiplier; // Multiplier of normal volatility
    double mean_reversion_strength; // How quickly prices revert to trend

    // Market Microstructure
    double bid_ask_spread_multiplier; // Spread expansion during stress
    double liquidity_multiplier; // Available liquidity (0.1 = 10% of normal)
    double slippage_multiplier; // Market impact amplification

    // Event Parameters
    bool has_flash_crash; // Sudden large drop
    double flash_crash_magnitude; // Percentage drop
    std::chrono::minutes flash_crash_duration; // Recovery time

    bool has_gap_opening; // Weekend/overnight gaps
    double gap_magnitude; // Percentage gap

    bool has_correlation_breakdown; // Assets become highly correlated
    double stress_correlation; // Correlation during stress (0.9 = highly correlated)

    ScenarioParameters() : trend_strength(0.0), volatility_multiplier(1.0),
                          mean_reversion_strength(0.1), bid_ask_spread_multiplier(1.0),
                          liquidity_multiplier(1.0), slippage_multiplier(1.0),
                          has_flash_crash(false), flash_crash_magnitude(-0.1),
                          flash_crash_duration(std::chrono::minutes(10)),
                          has_gap_opening(false), gap_magnitude(0.02),
                          has_correlation_breakdown(false), stress_correlation(0.9) {}
};

/**
 * Simulated market event
 */
struct MarketEvent {
    std::chrono::system_clock::time_point timestamp;
    std::string event_type; // "FLASH_CRASH", "GAP_OPENING", "LIQUIDITY_CRISIS", "CORRELATION_SPIKE"
    std::string description;
    std::vector<std::string> affected_symbols;
    std::map<std::string, double> impact_parameters;

    MarketEvent() = default;
    MarketEvent(const std::chrono::system_clock::time_point& ts, const std::string& type,
               const std::string& desc, const std::vector<std::string>& symbols)
        : timestamp(ts), event_type(type), description(desc), affected_symbols(symbols) {}
};

/**
 * Scenario simulation results
 */
struct ScenarioResults {
    std::string scenario_name;
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;

    // Strategy Performance Under Stress
    double strategy_return;
    double strategy_volatility;
    double max_drawdown;
    double var_95;
    double stress_test_score; // 0-100, higher is better

    // Market Behavior Validation
    bool volatility_target_achieved;
    bool correlation_target_achieved;
    bool liquidity_stress_simulated;

    // Event Execution
    std::vector<MarketEvent> executed_events;
    int successful_events;
    int failed_events;

    // Recovery Analysis
    std::chrono::minutes recovery_time;
    double post_stress_performance;

    ScenarioResults() : strategy_return(0.0), strategy_volatility(0.0), max_drawdown(0.0),
                       var_95(0.0), stress_test_score(0.0), volatility_target_achieved(false),
                       correlation_target_achieved(false), liquidity_stress_simulated(false),
                       successful_events(0), failed_events(0),
                       recovery_time(std::chrono::minutes(0)), post_stress_performance(0.0) {}
};

/**
 * Comprehensive market scenario simulator for cryptocurrency stress testing
 * Simulates bull markets, bear markets, flash crashes, and extreme volatility events
 */
class MarketScenarioSimulator {
public:
    MarketScenarioSimulator();
    ~MarketScenarioSimulator() = default;

    // Initialization
    bool initialize(std::shared_ptr<Analytics::BacktestingEngine> backtester,
                   std::shared_ptr<Trading::PaperTradingEngine> paper_trader);

    // Predefined Cryptocurrency Scenarios
    ScenarioParameters getBullMarket2020Scenario();
    ScenarioParameters getBearMarket2022Scenario();
    ScenarioParameters getCovidCrashScenario();
    ScenarioParameters getLunaCollapseScenario();
    ScenarioParameters getFlashCrashScenario();
    ScenarioParameters getHighVolatilityScenario();

    // Custom Scenario Creation
    ScenarioParameters createCustomScenario(const std::string& name,
                                           double trend_strength,
                                           double volatility_multiplier);

    // Scenario Execution
    ScenarioResults runScenarioSimulation(const ScenarioParameters& scenario,
                                         const std::string& strategy_name,
                                         std::chrono::hours duration = std::chrono::hours(168)); // 1 week default

    // Multi-Scenario Stress Testing
    std::map<std::string, ScenarioResults> runComprehensiveStressTesting(const std::string& strategy_name);

    // Market Data Generation
    std::vector<Analytics::MarketDataPoint> generateScenarioMarketData(
        const ScenarioParameters& scenario,
        const std::string& symbol,
        const std::chrono::system_clock::time_point& start_time,
        std::chrono::hours duration);

    // Event Injection
    void injectMarketEvent(const MarketEvent& event,
                          std::vector<Analytics::MarketDataPoint>& market_data);

    // Validation and Scoring
    double calculateStressTestScore(const ScenarioResults& results);
    bool validateScenarioExecution(const ScenarioParameters& scenario,
                                  const ScenarioResults& results);

    // Reporting
    std::string generateScenarioReport(const ScenarioResults& results);
    std::string generateComprehensiveStressReport(
        const std::map<std::string, ScenarioResults>& all_results);

    // Configuration
    void setRandomSeed(unsigned int seed) { random_generator_.seed(seed); }
    void setBaseVolatility(double daily_volatility) { base_daily_volatility_ = daily_volatility; }

private:
    std::shared_ptr<Analytics::BacktestingEngine> backtester_;
    std::shared_ptr<Trading::PaperTradingEngine> paper_trader_;

    // Random number generation
    std::mt19937 random_generator_;
    std::normal_distribution<double> normal_dist_;
    std::uniform_real_distribution<double> uniform_dist_;

    // Market parameters
    double base_daily_volatility_;
    std::map<std::string, double> base_prices_; // Starting prices for symbols

    // Scenario execution state
    std::vector<MarketEvent> scheduled_events_;

    // Market Data Generation Methods
    double generatePriceReturn(const ScenarioParameters& scenario,
                              double current_price,
                              double trend_price,
                              std::chrono::minutes elapsed_time);

    double calculateTrendPrice(double initial_price,
                              double trend_strength,
                              std::chrono::minutes elapsed_time);

    std::vector<Analytics::MarketDataPoint> applyVolatilityRegime(
        const std::vector<Analytics::MarketDataPoint>& base_data,
        double volatility_multiplier);

    std::vector<Analytics::MarketDataPoint> applyLiquidityStress(
        const std::vector<Analytics::MarketDataPoint>& base_data,
        double liquidity_multiplier);

    // Event Generation
    std::vector<MarketEvent> generateScenarioEvents(const ScenarioParameters& scenario,
                                                    const std::chrono::system_clock::time_point& start_time,
                                                    std::chrono::hours duration);

    MarketEvent createFlashCrashEvent(const std::chrono::system_clock::time_point& timestamp,
                                     double magnitude,
                                     std::chrono::minutes duration);

    MarketEvent createGapOpeningEvent(const std::chrono::system_clock::time_point& timestamp,
                                     double gap_magnitude);

    MarketEvent createCorrelationBreakdownEvent(const std::chrono::system_clock::time_point& timestamp,
                                               double target_correlation);

    // Event Injection Implementation
    void injectFlashCrash(const MarketEvent& event,
                         std::vector<Analytics::MarketDataPoint>& market_data);

    void injectGapOpening(const MarketEvent& event,
                         std::vector<Analytics::MarketDataPoint>& market_data);

    void injectCorrelationBreakdown(const MarketEvent& event,
                                   std::vector<Analytics::MarketDataPoint>& market_data);

    // Market Behavior Simulation
    std::vector<Analytics::MarketDataPoint> simulateWeekendGaps(
        const std::vector<Analytics::MarketDataPoint>& weekday_data,
        double gap_probability = 0.3);

    std::vector<Analytics::MarketDataPoint> simulateExchangeOutages(
        const std::vector<Analytics::MarketDataPoint>& normal_data,
        double outage_probability = 0.05);

    // Correlation and Cross-Asset Effects
    std::map<std::string, std::vector<Analytics::MarketDataPoint>> applyCorrelationStress(
        const std::map<std::string, std::vector<Analytics::MarketDataPoint>>& symbol_data,
        double target_correlation);

    // Liquidity and Market Impact Modeling
    double calculateMarketImpact(double order_size, double available_liquidity, double base_impact);
    double simulateSlippage(double base_slippage, double slippage_multiplier, double order_size);

    // Validation Methods
    bool validateVolatilityRealized(const std::vector<Analytics::MarketDataPoint>& data,
                                   double target_volatility, double tolerance = 0.2);

    bool validateCorrelationRealized(const std::map<std::string, std::vector<Analytics::MarketDataPoint>>& data,
                                    double target_correlation, double tolerance = 0.1);

    // Statistical Utilities
    double calculateRealizedVolatility(const std::vector<Analytics::MarketDataPoint>& data);
    double calculateCorrelation(const std::vector<double>& returns1, const std::vector<double>& returns2);
    std::vector<double> extractReturns(const std::vector<Analytics::MarketDataPoint>& data);

    // Utility Functions
    std::chrono::system_clock::time_point addBusinessDays(
        const std::chrono::system_clock::time_point& start, int business_days);

    bool isWeekend(const std::chrono::system_clock::time_point& timestamp);
    bool isMarketHours(const std::chrono::system_clock::time_point& timestamp);

    std::string timePointToString(const std::chrono::system_clock::time_point& tp);
    double roundToTickSize(double price, double tick_size = 0.01);
};

}} // namespace CryptoClaude::Testing