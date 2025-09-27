#pragma once

#include "RealTimeDataStreamManager.h"
#include "RealTimeSignalProcessor.h"
#include "TRSComplianceEngine.h"
#include "AlgorithmDecisionEngine.h"
#include <memory>
#include <vector>
#include <map>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>

namespace CryptoClaude {
namespace Algorithm {

/**
 * Live Trading Simulator - Day 22 Implementation
 * Realistic real-time trading simulation environment using live market data
 * Provides safe testing of algorithm performance without real capital risk
 */
class LiveTradingSimulator {
public:
    // Simulation configuration
    struct SimulationConfig {
        // Capital and risk parameters
        double initial_capital = 100000.0;      // Starting capital (USD)
        double max_position_size = 0.10;        // Max 10% per position
        double max_portfolio_risk = 0.20;       // Max 20% portfolio risk
        double stop_loss_percentage = 0.05;     // 5% stop loss
        double take_profit_percentage = 0.15;   // 15% take profit

        // Trading parameters
        std::chrono::hours max_position_duration{72};     // 3 days max hold
        double min_signal_confidence = 0.65;              // Minimum confidence to trade
        int max_concurrent_positions = 8;                 // Max positions at once
        bool enable_position_sizing = true;               // Dynamic position sizing

        // Market impact and costs
        double base_transaction_cost_bps = 8.0;           // 0.08% transaction cost
        double slippage_impact_factor = 0.0001;           // Slippage factor
        double market_impact_threshold = 0.01;            // 1% of volume threshold
        bool enable_realistic_execution = true;          // Realistic execution delays

        // Risk management
        bool enable_stop_loss = true;
        bool enable_take_profit = true;
        bool enable_correlation_limits = true;
        double max_correlation_exposure = 0.30;          // Max 30% in correlated positions

        // Simulation mode
        enum class Mode {
            PAPER_TRADING,    // Real-time with live data
            ACCELERATED,      // Faster than real-time
            HISTORICAL_REPLAY // Replay historical period
        } simulation_mode = Mode::PAPER_TRADING;

        double acceleration_factor = 1.0;               // For accelerated mode
    };

    // Position in the simulation
    struct SimulatedPosition {
        std::string position_id;
        std::string pair_name;
        std::chrono::system_clock::time_point opened_at;
        std::chrono::system_clock::time_point closed_at;

        // Entry details
        double entry_price;
        double position_size;                    // In base currency units
        double position_value_usd;               // USD value at entry
        AlgorithmDecisionEngine::DecisionType direction;

        // Signal context
        std::string originating_signal_id;
        double signal_confidence;
        double predicted_return;

        // Current status
        bool is_open;
        double current_price;
        double current_value_usd;
        double unrealized_pnl;
        double unrealized_return_percentage;

        // Risk management
        double stop_loss_price;
        double take_profit_price;
        double max_favorable_excursion;         // Best price during position
        double max_adverse_excursion;           // Worst price during position

        // Execution details
        double executed_price;                  // Actual fill price (vs entry_price)
        std::chrono::milliseconds execution_delay;
        double slippage_cost;
        double transaction_cost;
        double market_impact_cost;

        // Exit details (when closed)
        double exit_price;
        std::string exit_reason;                // "stop_loss", "take_profit", "timeout", "manual"
        double realized_pnl;
        double realized_return_percentage;
        std::chrono::hours holding_period;

        SimulatedPosition() : entry_price(0), position_size(0), position_value_usd(0),
                             direction(AlgorithmDecisionEngine::DecisionType::HOLD),
                             is_open(false), current_price(0), current_value_usd(0),
                             unrealized_pnl(0), unrealized_return_percentage(0),
                             stop_loss_price(0), take_profit_price(0),
                             max_favorable_excursion(0), max_adverse_excursion(0),
                             executed_price(0), execution_delay(std::chrono::milliseconds{0}),
                             slippage_cost(0), transaction_cost(0), market_impact_cost(0),
                             exit_price(0), realized_pnl(0), realized_return_percentage(0),
                             holding_period(std::chrono::hours{0}) {}
    };

    // Portfolio state at any point in time
    struct PortfolioSnapshot {
        std::chrono::system_clock::time_point timestamp;

        // Capital and P&L
        double initial_capital;
        double current_capital;                  // Available capital
        double invested_capital;                 // Capital in positions
        double total_portfolio_value;            // Current + invested
        double total_pnl;                       // Realized + unrealized P&L
        double total_return_percentage;

        // Position summary
        int open_positions;
        int closed_positions_today;
        double largest_position_percentage;     // Largest single position
        double portfolio_concentration;         // Concentration risk metric

        // Risk metrics
        double portfolio_var_95;                // 95% Value at Risk
        double current_drawdown;                // Current drawdown from peak
        double max_drawdown;                    // Maximum drawdown to date
        double portfolio_beta;                  // Portfolio beta vs market

        // Performance metrics
        double sharpe_ratio_ytd;               // Year-to-date Sharpe ratio
        double win_rate;                       // Percentage of winning trades
        double profit_factor;                  // Gross profit / gross loss
        double average_holding_period_hours;

        // TRS compliance
        double current_algorithm_correlation;
        TRSComplianceEngine::ComplianceStatus trs_status;
        double trs_gap;

        PortfolioSnapshot() : initial_capital(0), current_capital(0), invested_capital(0),
                             total_portfolio_value(0), total_pnl(0), total_return_percentage(0),
                             open_positions(0), closed_positions_today(0),
                             largest_position_percentage(0), portfolio_concentration(0),
                             portfolio_var_95(0), current_drawdown(0), max_drawdown(0),
                             portfolio_beta(0), sharpe_ratio_ytd(0), win_rate(0),
                             profit_factor(0), average_holding_period_hours(0),
                             current_algorithm_correlation(0),
                             trs_status(TRSComplianceEngine::ComplianceStatus::UNKNOWN),
                             trs_gap(0) {}
    };

    // Trading event for logging and analysis
    struct TradingEvent {
        std::string event_id;
        std::chrono::system_clock::time_point timestamp;
        std::string event_type;                 // "signal", "position_open", "position_close", etc.

        // Event details
        std::string pair_name;
        std::string signal_id;
        std::string position_id;

        // Market context
        double market_price;
        RealTimeDataStreamManager::MarketRegime market_regime;
        double data_quality_score;

        // Decision context
        std::string decision_rationale;
        double confidence_score;
        std::vector<std::string> risk_factors;

        // Execution details
        bool was_executed;
        std::string execution_result;
        std::chrono::milliseconds execution_latency;

        TradingEvent() : market_price(0), market_regime(RealTimeDataStreamManager::MarketRegime::NORMAL),
                        data_quality_score(0), confidence_score(0), was_executed(false),
                        execution_latency(std::chrono::milliseconds{0}) {}
    };

private:
    // Core components
    SimulationConfig config_;
    std::unique_ptr<RealTimeDataStreamManager> data_manager_;
    std::unique_ptr<RealTimeSignalProcessor> signal_processor_;
    std::unique_ptr<TRSComplianceEngine> compliance_engine_;
    std::unique_ptr<AlgorithmDecisionEngine> decision_engine_;

    // Simulation state
    std::atomic<bool> is_running_{false};
    mutable std::mutex portfolio_mutex_;
    mutable std::mutex position_mutex_;
    mutable std::mutex event_mutex_;

    // Threading
    std::thread simulation_thread_;
    std::thread monitoring_thread_;

    // Portfolio and positions
    PortfolioSnapshot current_portfolio_;
    std::map<std::string, SimulatedPosition> open_positions_;
    std::vector<SimulatedPosition> closed_positions_;
    std::queue<TradingEvent> trading_events_;

    // Performance tracking
    std::deque<PortfolioSnapshot> portfolio_history_;
    double peak_portfolio_value_;
    std::chrono::system_clock::time_point simulation_start_time_;

    // Execution simulation
    struct PendingOrder {
        std::string order_id;
        std::string pair_name;
        AlgorithmDecisionEngine::DecisionType direction;
        double target_price;
        double quantity;
        std::chrono::system_clock::time_point submitted_at;
        std::chrono::milliseconds expected_execution_delay;
    };

    std::queue<PendingOrder> pending_orders_;

    // Event callbacks
    std::function<void(const SimulatedPosition&)> position_callback_;
    std::function<void(const PortfolioSnapshot&)> portfolio_callback_;
    std::function<void(const TradingEvent&)> event_callback_;

    // Core simulation methods
    void runSimulation();
    void processSignals();
    void updatePositions();
    void updatePortfolio();
    void executeRiskManagement();

    // Position management
    bool openPosition(const RealTimeSignalProcessor::LiveTradingSignal& signal);
    bool closePosition(const std::string& position_id, const std::string& reason);
    void updatePositionPnL(SimulatedPosition& position);

    // Execution simulation
    PendingOrder createOrder(const RealTimeSignalProcessor::LiveTradingSignal& signal);
    bool executeOrder(const PendingOrder& order);
    double calculateExecutionPrice(const PendingOrder& order);
    double calculateSlippage(const PendingOrder& order, double market_price);
    double calculateMarketImpact(const PendingOrder& order);

    // Risk management
    bool passesRiskChecks(const RealTimeSignalProcessor::LiveTradingSignal& signal);
    double calculatePositionSize(const RealTimeSignalProcessor::LiveTradingSignal& signal);
    double calculatePortfolioRisk();
    void checkStopLossLevels();
    void checkTakeProfitLevels();

    // Performance calculation
    void calculatePerformanceMetrics();
    double calculatePortfolioBeta();
    double calculateSharpeRatio();

    // Event logging
    void logTradingEvent(const TradingEvent& event);
    TradingEvent createSignalEvent(const RealTimeSignalProcessor::LiveTradingSignal& signal);
    TradingEvent createPositionEvent(const SimulatedPosition& position, const std::string& action);

public:
    explicit LiveTradingSimulator(const SimulationConfig& config = SimulationConfig{});
    ~LiveTradingSimulator();

    // Simulation control
    bool startSimulation();
    void stopSimulation();
    void pauseSimulation();
    void resumeSimulation();
    bool isRunning() const { return is_running_.load(); }

    // Configuration management
    void updateConfig(const SimulationConfig& new_config);
    const SimulationConfig& getConfig() const { return config_; }

    // Component integration
    bool integrateDataManager(std::unique_ptr<RealTimeDataStreamManager> data_manager);
    bool integrateSignalProcessor(std::unique_ptr<RealTimeSignalProcessor> signal_processor);
    bool integrateComplianceEngine(std::unique_ptr<TRSComplianceEngine> compliance_engine);
    bool integrateDecisionEngine(std::unique_ptr<AlgorithmDecisionEngine> decision_engine);

    // Portfolio access
    PortfolioSnapshot getCurrentPortfolio() const;
    std::vector<PortfolioSnapshot> getPortfolioHistory(std::chrono::hours lookback = std::chrono::hours{24}) const;
    double getCurrentReturn() const;
    double getCurrentDrawdown() const;

    // Position access
    std::vector<SimulatedPosition> getOpenPositions() const;
    std::vector<SimulatedPosition> getClosedPositions(std::chrono::hours lookback = std::chrono::hours{24}) const;
    SimulatedPosition getPosition(const std::string& position_id) const;
    int getOpenPositionCount() const;

    // Trading event access
    std::vector<TradingEvent> getTradingEvents(std::chrono::hours lookback = std::chrono::hours{1}) const;
    std::vector<TradingEvent> getEventsForPair(const std::string& pair_name) const;

    // Performance analysis
    struct PerformanceAnalysis {
        // Return metrics
        double total_return;
        double annualized_return;
        double volatility;
        double sharpe_ratio;
        double sortino_ratio;
        double calmar_ratio;

        // Risk metrics
        double max_drawdown;
        double value_at_risk_95;
        double expected_shortfall;
        double beta;

        // Trading metrics
        int total_trades;
        double win_rate;
        double profit_factor;
        double average_trade_return;
        std::chrono::hours average_holding_period;

        // Algorithm performance
        double prediction_accuracy;
        double signal_correlation;
        double trs_compliance_percentage;

        PerformanceAnalysis() : total_return(0), annualized_return(0), volatility(0),
                               sharpe_ratio(0), sortino_ratio(0), calmar_ratio(0),
                               max_drawdown(0), value_at_risk_95(0), expected_shortfall(0),
                               beta(0), total_trades(0), win_rate(0), profit_factor(0),
                               average_trade_return(0), average_holding_period(std::chrono::hours{0}),
                               prediction_accuracy(0), signal_correlation(0),
                               trs_compliance_percentage(0) {}
    };

    PerformanceAnalysis getPerformanceAnalysis() const;

    // Risk monitoring
    struct RiskReport {
        std::chrono::system_clock::time_point report_time;

        // Position risk
        double largest_position_risk;
        double portfolio_concentration;
        std::vector<std::string> risk_warnings;

        // Market risk
        double portfolio_var;
        double stress_test_loss;
        RealTimeDataStreamManager::MarketRegime dominant_regime;

        // Compliance risk
        double trs_gap;
        TRSComplianceEngine::ComplianceStatus compliance_status;
        std::vector<std::string> compliance_issues;

        bool requires_immediate_action;
    };

    RiskReport generateRiskReport() const;

    // Manual intervention
    bool manualClosePosition(const std::string& position_id, const std::string& reason = "manual");
    bool manualCloseAllPositions(const std::string& reason = "manual_shutdown");
    bool adjustPositionSize(const std::string& position_id, double new_size);
    void setEmergencyStop(bool enable = true);

    // Simulation modes
    bool switchToAcceleratedMode(double acceleration_factor);
    bool switchToPaperTradingMode();
    bool replayHistoricalPeriod(const std::chrono::system_clock::time_point& start,
                               const std::chrono::system_clock::time_point& end);

    // Data export and reporting
    bool exportPositionHistory(const std::string& filename, const std::string& format = "csv") const;
    bool exportPortfolioHistory(const std::string& filename, const std::string& format = "csv") const;
    bool exportTradingEvents(const std::string& filename, const std::string& format = "csv") const;

    // Callback registration
    void setPositionCallback(std::function<void(const SimulatedPosition&)> callback);
    void setPortfolioCallback(std::function<void(const PortfolioSnapshot&)> callback);
    void setEventCallback(std::function<void(const TradingEvent&)> callback);

    // Stress testing
    struct StressTestScenario {
        std::string scenario_name;
        double market_shock_percentage;          // Price shock magnitude
        std::chrono::hours shock_duration;       // Duration of shock
        double correlation_breakdown_factor;    // Factor by which correlations break down
    };

    struct StressTestResult {
        StressTestScenario scenario;
        double portfolio_loss;
        double max_drawdown_during_shock;
        int positions_stopped_out;
        bool trs_compliance_maintained;
        std::chrono::minutes recovery_time;
    };

    std::vector<StressTestResult> runStressTests(const std::vector<StressTestScenario>& scenarios) const;

    // Static utilities
    static std::string generatePositionId(const std::string& pair_name);
    static std::string generateEventId();
    static double calculateReturnPercentage(double initial_value, double final_value);
    static std::chrono::hours calculateHoldingPeriod(const std::chrono::system_clock::time_point& open_time,
                                                    const std::chrono::system_clock::time_point& close_time);
};

} // namespace Algorithm
} // namespace CryptoClaude