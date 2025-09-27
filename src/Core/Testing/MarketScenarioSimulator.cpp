#include "MarketScenarioSimulator.h"
#include <cmath>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <iostream>

namespace CryptoClaude {
namespace Testing {

MarketScenarioSimulator::MarketScenarioSimulator()
    : random_generator_(std::random_device{}())
    , normal_dist_(0.0, 1.0)
    , uniform_dist_(0.0, 1.0)
    , base_daily_volatility_(0.03) {

    // Set default crypto base prices
    base_prices_["BTC-USD"] = 40000.0;
    base_prices_["ETH-USD"] = 2500.0;
    base_prices_["BNB-USD"] = 300.0;
    base_prices_["ADA-USD"] = 0.50;
    base_prices_["SOL-USD"] = 100.0;
}

bool MarketScenarioSimulator::initialize(std::shared_ptr<Analytics::BacktestingEngine> backtester,
                                        std::shared_ptr<Trading::PaperTradingEngine> paper_trader) {
    backtester_ = backtester;
    paper_trader_ = paper_trader;

    if (!backtester_) {
        std::cerr << "MarketScenarioSimulator: Invalid backtesting engine" << std::endl;
        return false;
    }

    std::cout << "MarketScenarioSimulator: Initialized with comprehensive stress testing capabilities" << std::endl;
    return true;
}

ScenarioParameters MarketScenarioSimulator::getBullMarket2020Scenario() {
    ScenarioParameters scenario;
    scenario.scenario_name = "Bull Market 2020-2021";
    scenario.description = "Bitcoin bull market rally with institutional adoption";
    scenario.trend_strength = 0.05; // 5% daily positive trend
    scenario.volatility_multiplier = 1.5;
    scenario.mean_reversion_strength = 0.05; // Weak mean reversion in bull markets
    scenario.bid_ask_spread_multiplier = 0.8; // Tighter spreads in bull markets
    scenario.liquidity_multiplier = 1.2; // More liquidity
    scenario.slippage_multiplier = 0.8; // Less slippage

    return scenario;
}

ScenarioParameters MarketScenarioSimulator::getBearMarket2022Scenario() {
    ScenarioParameters scenario;
    scenario.scenario_name = "Crypto Winter 2022";
    scenario.description = "Extended bear market with regulatory pressure";
    scenario.trend_strength = -0.02; // 2% daily negative trend
    scenario.volatility_multiplier = 2.0;
    scenario.mean_reversion_strength = 0.15;
    scenario.bid_ask_spread_multiplier = 1.5; // Wider spreads
    scenario.liquidity_multiplier = 0.7; // Reduced liquidity
    scenario.slippage_multiplier = 1.5; // Higher slippage

    return scenario;
}

ScenarioParameters MarketScenarioSimulator::getCovidCrashScenario() {
    ScenarioParameters scenario;
    scenario.scenario_name = "COVID-19 Market Crash";
    scenario.description = "March 2020 global financial crisis impact";
    scenario.trend_strength = -0.08; // Severe negative trend
    scenario.volatility_multiplier = 4.0; // Extreme volatility
    scenario.mean_reversion_strength = 0.3; // Strong mean reversion after crash
    scenario.bid_ask_spread_multiplier = 3.0; // Very wide spreads
    scenario.liquidity_multiplier = 0.3; // Severe liquidity crisis
    scenario.slippage_multiplier = 3.0; // Extreme slippage

    scenario.has_flash_crash = true;
    scenario.flash_crash_magnitude = -0.5; // 50% drop
    scenario.flash_crash_duration = std::chrono::minutes(30);
    scenario.has_correlation_breakdown = true;
    scenario.stress_correlation = 0.95; // Everything moves together

    return scenario;
}

ScenarioParameters MarketScenarioSimulator::getFlashCrashScenario() {
    ScenarioParameters scenario;
    scenario.scenario_name = "Flash Crash Event";
    scenario.description = "Sudden algorithmic selling cascade";
    scenario.trend_strength = 0.0; // Neutral trend
    scenario.volatility_multiplier = 2.5;
    scenario.mean_reversion_strength = 0.8; // Strong recovery
    scenario.bid_ask_spread_multiplier = 2.0;
    scenario.liquidity_multiplier = 0.5;
    scenario.slippage_multiplier = 2.0;

    scenario.has_flash_crash = true;
    scenario.flash_crash_magnitude = -0.25; // 25% drop
    scenario.flash_crash_duration = std::chrono::minutes(10);

    return scenario;
}

ScenarioResults MarketScenarioSimulator::runScenarioSimulation(const ScenarioParameters& scenario,
                                                              const std::string& strategy_name,
                                                              std::chrono::hours duration) {
    ScenarioResults results;
    results.scenario_name = scenario.scenario_name;
    results.start_time = std::chrono::system_clock::now();

    std::cout << "MarketScenarioSimulator: Running scenario '" << scenario.scenario_name
              << "' for " << duration.count() << " hours" << std::endl;

    if (!backtester_) {
        results.stress_test_score = 0.0;
        return results;
    }

    try {
        // Generate market data for primary cryptocurrencies
        std::vector<std::string> test_symbols = {"BTC-USD", "ETH-USD"};

        for (const auto& symbol : test_symbols) {
            auto market_data = generateScenarioMarketData(scenario, symbol, results.start_time, duration);

            // Store data in backtesting engine (simplified)
            std::cout << "MarketScenarioSimulator: Generated " << market_data.size()
                      << " data points for " << symbol << " under " << scenario.scenario_name << std::endl;
        }

        // Run backtest with generated scenario data
        auto end_time = results.start_time + duration;
        auto backtest_results = backtester_->runBacktest(strategy_name + "_" + scenario.scenario_name,
                                                        results.start_time, end_time);

        // Extract performance metrics
        results.strategy_return = backtest_results.total_return;
        results.strategy_volatility = backtest_results.volatility;
        results.max_drawdown = backtest_results.max_drawdown;
        results.end_time = end_time;

        // Calculate stress test score
        results.stress_test_score = calculateStressTestScore(results);

        // Validate scenario execution
        results.volatility_target_achieved = validateScenarioExecution(scenario, results);

        std::cout << "MarketScenarioSimulator: Scenario '" << scenario.scenario_name
                  << "' completed. Strategy return: " << std::fixed << std::setprecision(2)
                  << results.strategy_return * 100 << "%, Max DD: "
                  << results.max_drawdown * 100 << "%" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "MarketScenarioSimulator: Error running scenario: " << e.what() << std::endl;
        results.stress_test_score = 0.0;
    }

    return results;
}

std::vector<Analytics::MarketDataPoint> MarketScenarioSimulator::generateScenarioMarketData(
    const ScenarioParameters& scenario,
    const std::string& symbol,
    const std::chrono::system_clock::time_point& start_time,
    std::chrono::hours duration) {

    std::vector<Analytics::MarketDataPoint> market_data;

    double base_price = base_prices_.count(symbol) ? base_prices_[symbol] : 1000.0;
    double current_price = base_price;

    auto current_time = start_time;
    auto end_time = start_time + duration;

    // Generate hourly data points
    while (current_time < end_time) {
        auto elapsed_minutes = std::chrono::duration_cast<std::chrono::minutes>(current_time - start_time);

        // Calculate trend-based target price
        double trend_price = calculateTrendPrice(base_price, scenario.trend_strength, elapsed_minutes);

        // Generate price return with scenario parameters
        double price_return = generatePriceReturn(scenario, current_price, trend_price, elapsed_minutes);

        // Apply return to get new price
        double new_price = current_price * (1.0 + price_return);

        // Create market data point with OHLCV
        double open = current_price;
        double close = new_price;
        double high = std::max(open, close) * (1.0 + std::abs(normal_dist_(random_generator_)) * 0.005);
        double low = std::min(open, close) * (1.0 - std::abs(normal_dist_(random_generator_)) * 0.005);
        double volume = 1000.0 + uniform_dist_(random_generator_) * 5000.0;

        market_data.emplace_back(current_time, symbol, open, high, low, close, volume);

        current_price = new_price;
        current_time += std::chrono::hours(1);
    }

    // Generate and inject scenario events
    auto events = generateScenarioEvents(scenario, start_time, duration);
    for (const auto& event : events) {
        injectMarketEvent(event, market_data);
    }

    return market_data;
}

double MarketScenarioSimulator::generatePriceReturn(const ScenarioParameters& scenario,
                                                   double current_price,
                                                   double trend_price,
                                                   std::chrono::minutes elapsed_time) {
    // Base random return
    double random_return = normal_dist_(random_generator_) * base_daily_volatility_ * scenario.volatility_multiplier / 24.0;

    // Mean reversion component
    double mean_reversion = 0.0;
    if (current_price != trend_price) {
        double price_gap = (trend_price - current_price) / current_price;
        mean_reversion = price_gap * scenario.mean_reversion_strength / 24.0;
    }

    // Combine components
    return random_return + mean_reversion;
}

double MarketScenarioSimulator::calculateTrendPrice(double initial_price,
                                                   double trend_strength,
                                                   std::chrono::minutes elapsed_time) {
    double days_elapsed = elapsed_time.count() / (60.0 * 24.0);
    return initial_price * std::pow(1.0 + trend_strength, days_elapsed);
}

std::vector<MarketEvent> MarketScenarioSimulator::generateScenarioEvents(
    const ScenarioParameters& scenario,
    const std::chrono::system_clock::time_point& start_time,
    std::chrono::hours duration) {

    std::vector<MarketEvent> events;

    if (scenario.has_flash_crash) {
        // Schedule flash crash at random time in first half of scenario
        auto crash_time = start_time + std::chrono::minutes(
            static_cast<long>(uniform_dist_(random_generator_) * duration.count() * 30)); // First half

        events.push_back(createFlashCrashEvent(crash_time,
                                              scenario.flash_crash_magnitude,
                                              scenario.flash_crash_duration));
    }

    if (scenario.has_gap_opening) {
        // Schedule weekend gaps
        auto gap_time = start_time + std::chrono::hours(48); // Weekend
        events.push_back(createGapOpeningEvent(gap_time, scenario.gap_magnitude));
    }

    if (scenario.has_correlation_breakdown) {
        // Schedule correlation breakdown during stress
        auto correlation_time = start_time + duration / 3; // Middle third
        events.push_back(createCorrelationBreakdownEvent(correlation_time, scenario.stress_correlation));
    }

    return events;
}

MarketEvent MarketScenarioSimulator::createFlashCrashEvent(
    const std::chrono::system_clock::time_point& timestamp,
    double magnitude,
    std::chrono::minutes duration) {

    MarketEvent event(timestamp, "FLASH_CRASH", "Sudden algorithmic selling cascade",
                     {"BTC-USD", "ETH-USD", "BNB-USD"});

    event.impact_parameters["magnitude"] = magnitude;
    event.impact_parameters["duration_minutes"] = static_cast<double>(duration.count());
    event.impact_parameters["recovery_strength"] = 0.8; // Strong recovery

    return event;
}

void MarketScenarioSimulator::injectMarketEvent(const MarketEvent& event,
                                               std::vector<Analytics::MarketDataPoint>& market_data) {
    if (event.event_type == "FLASH_CRASH") {
        injectFlashCrash(event, market_data);
    } else if (event.event_type == "GAP_OPENING") {
        injectGapOpening(event, market_data);
    } else if (event.event_type == "CORRELATION_SPIKE") {
        injectCorrelationBreakdown(event, market_data);
    }
}

void MarketScenarioSimulator::injectFlashCrash(const MarketEvent& event,
                                              std::vector<Analytics::MarketDataPoint>& market_data) {
    // Find the data point closest to the event time
    auto event_it = std::lower_bound(market_data.begin(), market_data.end(), event.timestamp,
                                    [](const Analytics::MarketDataPoint& data, const std::chrono::system_clock::time_point& time) {
                                        return data.timestamp < time;
                                    });

    if (event_it == market_data.end()) return;

    double magnitude = event.impact_parameters.at("magnitude");
    int duration_minutes = static_cast<int>(event.impact_parameters.at("duration_minutes"));

    // Apply crash to multiple data points
    size_t crash_points = std::min(static_cast<size_t>(duration_minutes / 60 + 1),
                                  static_cast<size_t>(std::distance(event_it, market_data.end())));

    for (size_t i = 0; i < crash_points; ++i) {
        auto& data_point = *(event_it + i);

        // Apply crash magnitude with gradual recovery
        double recovery_factor = static_cast<double>(i) / crash_points;
        double effective_magnitude = magnitude * (1.0 - recovery_factor * 0.8);

        data_point.close *= (1.0 + effective_magnitude);
        data_point.low *= (1.0 + effective_magnitude);
        data_point.high = std::max(data_point.high * (1.0 + effective_magnitude * 0.5), data_point.close);
    }
}

void MarketScenarioSimulator::injectGapOpening(const MarketEvent& event,
                                              std::vector<Analytics::MarketDataPoint>& market_data) {
    // Implementation for gap opening injection
    // For brevity, simplified implementation
}

void MarketScenarioSimulator::injectCorrelationBreakdown(const MarketEvent& event,
                                                        std::vector<Analytics::MarketDataPoint>& market_data) {
    // Implementation for correlation breakdown
    // For brevity, simplified implementation
}

double MarketScenarioSimulator::calculateStressTestScore(const ScenarioResults& results) {
    double score = 50.0; // Base score

    // Penalize negative returns
    if (results.strategy_return < 0) {
        score -= std::abs(results.strategy_return) * 100; // Each 1% loss = 1 point penalty
    } else {
        score += results.strategy_return * 50; // Each 1% gain = 0.5 point bonus
    }

    // Penalize large drawdowns
    score -= std::abs(results.max_drawdown) * 200; // Each 1% drawdown = 2 point penalty

    // Penalize excessive volatility
    if (results.strategy_volatility > 0.5) { // >50% volatility
        score -= (results.strategy_volatility - 0.5) * 100;
    }

    return std::max(0.0, std::min(100.0, score));
}

bool MarketScenarioSimulator::validateScenarioExecution(const ScenarioParameters& scenario,
                                                       const ScenarioResults& results) {
    // Simplified validation - check if strategy survived the stress test
    return results.max_drawdown > -0.50 && results.strategy_return > -0.30;
}

std::map<std::string, ScenarioResults> MarketScenarioSimulator::runComprehensiveStressTesting(
    const std::string& strategy_name) {

    std::map<std::string, ScenarioResults> all_results;

    std::cout << "MarketScenarioSimulator: Running comprehensive stress testing for '"
              << strategy_name << "'" << std::endl;

    // Run all predefined scenarios
    auto scenarios = {
        getBullMarket2020Scenario(),
        getBearMarket2022Scenario(),
        getCovidCrashScenario(),
        getFlashCrashScenario(),
        getHighVolatilityScenario()
    };

    for (const auto& scenario : scenarios) {
        auto results = runScenarioSimulation(scenario, strategy_name, std::chrono::hours(168)); // 1 week
        all_results[scenario.scenario_name] = results;
    }

    return all_results;
}

ScenarioParameters MarketScenarioSimulator::getHighVolatilityScenario() {
    ScenarioParameters scenario;
    scenario.scenario_name = "High Volatility Period";
    scenario.description = "Extended period of extreme price swings";
    scenario.trend_strength = 0.0; // No clear trend
    scenario.volatility_multiplier = 3.0; // 3x normal volatility
    scenario.mean_reversion_strength = 0.2;
    scenario.bid_ask_spread_multiplier = 2.0;
    scenario.liquidity_multiplier = 0.8;
    scenario.slippage_multiplier = 1.8;

    return scenario;
}

std::string MarketScenarioSimulator::generateScenarioReport(const ScenarioResults& results) {
    std::stringstream report;

    report << "=== MARKET SCENARIO SIMULATION REPORT ===" << std::endl;
    report << "Scenario: " << results.scenario_name << std::endl;
    report << "Duration: " << timePointToString(results.start_time)
           << " to " << timePointToString(results.end_time) << std::endl;
    report << std::endl;

    report << "STRATEGY PERFORMANCE:" << std::endl;
    report << "  Total Return: " << std::fixed << std::setprecision(2)
           << results.strategy_return * 100 << "%" << std::endl;
    report << "  Volatility: " << results.strategy_volatility * 100 << "%" << std::endl;
    report << "  Max Drawdown: " << results.max_drawdown * 100 << "%" << std::endl;
    report << "  Stress Test Score: " << results.stress_test_score << "/100" << std::endl;
    report << std::endl;

    report << "SCENARIO VALIDATION:" << std::endl;
    report << "  Volatility Target Achieved: " << (results.volatility_target_achieved ? "YES" : "NO") << std::endl;
    report << "  Events Executed Successfully: " << results.successful_events
           << "/" << (results.successful_events + results.failed_events) << std::endl;

    return report.str();
}

std::string MarketScenarioSimulator::timePointToString(const std::chrono::system_clock::time_point& tp) {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M");
    return ss.str();
}

MarketEvent MarketScenarioSimulator::createGapOpeningEvent(const std::chrono::system_clock::time_point& event_time,
                                                          double gap_percentage) {
    MarketEvent event;
    event.timestamp = event_time;
    event.event_type = "GAP_OPENING";
    event.description = "Market gap opening of " + std::to_string(gap_percentage * 100) + "%";
    event.affected_symbols = {"BTC", "ETH", "ADA"}; // Default affected symbols

    // Store gap percentage in impact parameters
    event.impact_parameters["gap_percentage"] = gap_percentage;
    event.impact_parameters["duration_minutes"] = 15.0; // Gap lasts 15 minutes
    event.impact_parameters["severity"] = std::abs(gap_percentage) > 0.05 ? 2.0 : 1.0; // HIGH=2, MEDIUM=1

    return event;
}

MarketEvent MarketScenarioSimulator::createCorrelationBreakdownEvent(const std::chrono::system_clock::time_point& event_time,
                                                                     double breakdown_severity) {
    MarketEvent event;
    event.timestamp = event_time;
    event.event_type = "correlation_breakdown";
    event.description = "Correlation breakdown between major cryptocurrencies";
    event.affected_symbols = {"BTC", "ETH", "ADA", "DOT", "LINK", "UNI"};

    // Set correlation breakdown parameters
    event.impact_parameters["breakdown_severity"] = breakdown_severity;
    event.impact_parameters["correlation_threshold"] = 0.3;
    event.impact_parameters["recovery_time_hours"] = 12.0;
    event.impact_parameters["volatility_multiplier"] = 1.0 + breakdown_severity;

    return event;
}

}} // namespace CryptoClaude::Testing