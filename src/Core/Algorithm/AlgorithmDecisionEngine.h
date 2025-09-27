#pragma once

#include "RealMarketSignalGenerator.h"
#include "RealTimeMarketDataProcessor.h"
#include "../Risk/ProductionRiskManager.h"
#include <memory>
#include <vector>
#include <map>
#include <string>
#include <chrono>

namespace CryptoClaude {
namespace Algorithm {

/**
 * Algorithm Decision Engine - Day 20 Implementation
 * Core decision-making component that integrates signal generation with market data
 * Implements the main algorithm logic flow replacing placeholder implementations
 */
class AlgorithmDecisionEngine {
public:
    // Decision engine configuration
    struct DecisionConfig {
        // Signal filtering parameters
        double min_signal_confidence = 0.7;        // Minimum signal confidence
        double min_data_quality = 0.9;             // Minimum data quality required
        int min_historical_periods = 100;          // Minimum historical data required

        // Risk management parameters
        double max_position_size = 0.15;           // Maximum position size (15% of portfolio)
        double max_sector_exposure = 0.25;         // Maximum exposure to any sector
        double correlation_threshold = 0.8;        // Maximum correlation between positions

        // Decision thresholds
        double strong_buy_threshold = 0.8;         // Signal strength for strong buy
        double buy_threshold = 0.6;                // Signal strength for buy
        double sell_threshold = -0.6;              // Signal strength for sell
        double strong_sell_threshold = -0.8;       // Signal strength for strong sell

        // Algorithm behavior
        bool enable_regime_adaptation = true;       // Adapt to market regimes
        bool enable_correlation_filtering = true;   // Filter correlated positions
        bool enable_volatility_scaling = true;     // Scale positions by volatility
        bool enable_momentum_bias = true;          // Apply momentum bias to decisions

        // Performance tracking
        bool track_decision_outcomes = true;       // Track decision performance
        int outcome_tracking_period_days = 30;    // Period to track outcomes

        DecisionConfig() {}
    };

    // Trading decision output
    struct TradingDecision {
        std::string pair_name;                     // e.g., "BTC/ETH"

        // Decision details
        enum class Action {
            STRONG_BUY,
            BUY,
            HOLD,
            SELL,
            STRONG_SELL,
            NO_ACTION
        } action;

        double recommended_weight;                 // Position weight (0.0 to 1.0)
        double confidence_score;                   // Decision confidence (0.0 to 1.0)

        // Supporting information
        double signal_strength;                    // Raw signal strength
        double risk_adjusted_strength;             // Risk-adjusted signal
        double expected_return;                    // Expected return estimate
        double expected_volatility;                // Expected volatility

        // Decision rationale
        std::vector<std::string> decision_reasons; // Human-readable justification
        std::map<std::string, double> factor_contributions; // Factor weightings

        // Risk assessment
        double position_risk_score;                // Position-level risk (0-1)
        double portfolio_impact_score;             // Portfolio impact (0-1)
        double correlation_risk;                   // Correlation with existing positions

        // Timing information
        std::chrono::system_clock::time_point decision_time;
        std::chrono::hours recommended_holding_period;

        // Decision tracking
        std::string decision_id;                   // Unique decision identifier
        bool is_live_decision;                     // True for live, false for backtest

        TradingDecision() : action(Action::NO_ACTION), recommended_weight(0.0),
                          confidence_score(0.0), signal_strength(0.0),
                          risk_adjusted_strength(0.0), expected_return(0.0),
                          expected_volatility(0.0), position_risk_score(0.5),
                          portfolio_impact_score(0.0), correlation_risk(0.0),
                          recommended_holding_period(std::chrono::hours{24}),
                          is_live_decision(true),
                          decision_time(std::chrono::system_clock::now()) {}
    };

    // Portfolio context for decision making
    struct PortfolioContext {
        double total_portfolio_value;              // Current portfolio value
        double cash_available;                     // Available cash for new positions
        double current_volatility;                 // Portfolio volatility

        // Current positions
        struct Position {
            std::string pair_name;
            double weight;
            double entry_price;
            double current_value;
            double unrealized_pnl;
            std::chrono::system_clock::time_point entry_time;
        };

        std::vector<Position> current_positions;

        // Risk metrics
        double portfolio_var;                      // Value at Risk
        double portfolio_correlation;              // Average position correlation
        std::map<std::string, double> sector_exposures; // Sector exposure mapping

        // Performance metrics
        double recent_returns_7d;                  // 7-day returns
        double recent_returns_30d;                 // 30-day returns
        double max_drawdown;                       // Maximum drawdown

        PortfolioContext() : total_portfolio_value(100000.0), cash_available(20000.0),
                           current_volatility(0.15), portfolio_var(0.05),
                           portfolio_correlation(0.3), recent_returns_7d(0.0),
                           recent_returns_30d(0.0), max_drawdown(0.0) {}
    };

    // Decision batch processing
    struct DecisionBatch {
        std::vector<TradingDecision> decisions;
        PortfolioContext portfolio_context;
        std::chrono::system_clock::time_point batch_time;

        // Batch-level analytics
        double total_recommended_exposure;         // Total position size recommended
        int strong_signals_count;                  // Count of strong buy/sell signals
        double portfolio_risk_change;              // Expected portfolio risk change
        double expected_portfolio_return;          // Expected portfolio return

        // Risk management
        bool batch_approved;                       // Risk management approval
        std::vector<std::string> risk_warnings;   // Risk management warnings

        DecisionBatch() : batch_time(std::chrono::system_clock::now()),
                        total_recommended_exposure(0.0), strong_signals_count(0),
                        portfolio_risk_change(0.0), expected_portfolio_return(0.0),
                        batch_approved(false) {}
    };

private:
    DecisionConfig config_;
    std::unique_ptr<RealMarketSignalGenerator> signal_generator_;
    std::unique_ptr<RealTimeMarketDataProcessor> data_processor_;

    // Decision history and tracking
    std::vector<TradingDecision> decision_history_;
    std::map<std::string, double> factor_performance_; // Track factor performance

    // Portfolio tracking
    PortfolioContext current_portfolio_;
    std::map<std::string, std::chrono::system_clock::time_point> last_decision_times_;

    // Risk management
    double calculatePositionRisk(const std::string& pair, double position_size,
                                const RealTimeMarketDataProcessor::AlgorithmMarketData& data);
    double calculateCorrelationRisk(const std::string& pair,
                                   const PortfolioContext& portfolio);
    double calculatePortfolioImpact(const TradingDecision& decision,
                                   const PortfolioContext& portfolio);

    // Decision logic components
    TradingDecision::Action determineAction(double signal_strength, double confidence);
    double calculateRecommendedWeight(double signal_strength, double risk_score,
                                     const PortfolioContext& portfolio);
    double applyRiskAdjustment(double raw_signal, const RealTimeMarketDataProcessor::AlgorithmMarketData& data);

    // Market regime adaptation
    void adaptToMarketRegime(TradingDecision& decision,
                            RealTimeMarketDataProcessor::MarketRegime regime);

    // Decision validation
    bool validateDecision(const TradingDecision& decision,
                         const PortfolioContext& portfolio) const;
    void generateDecisionRationale(TradingDecision& decision,
                                  const RealMarketSignalGenerator::RealTradingSignal& signal,
                                  const RealTimeMarketDataProcessor::AlgorithmMarketData& data);

public:
    explicit AlgorithmDecisionEngine(const DecisionConfig& config = DecisionConfig());
    ~AlgorithmDecisionEngine() = default;

    // Lifecycle management
    bool initialize();
    bool isInitialized() const;

    // Main decision interface
    TradingDecision makeDecision(const std::string& pair);
    std::vector<TradingDecision> makeDecisions(const std::vector<std::string>& pairs);
    DecisionBatch makeBatchDecision(const std::vector<std::string>& pairs);

    // Portfolio management
    void updatePortfolioContext(const PortfolioContext& portfolio);
    const PortfolioContext& getPortfolioContext() const { return current_portfolio_; }

    // Decision tracking and analysis
    void recordDecisionOutcome(const std::string& decision_id, double actual_return);
    std::vector<TradingDecision> getDecisionHistory(int days = 30) const;

    // Performance analysis
    struct DecisionPerformance {
        int total_decisions;
        int profitable_decisions;
        double hit_rate;                          // Percentage of profitable decisions
        double average_return;                    // Average return per decision
        double risk_adjusted_return;              // Sharpe ratio equivalent
        std::map<TradingDecision::Action, double> action_performance;
        std::map<std::string, double> factor_contribution;
    };

    DecisionPerformance analyzeDecisionPerformance() const;

    // Algorithm calibration
    void calibrateThresholds(const std::vector<std::string>& pairs, int calibration_days);
    void updateFactorWeights(const std::map<std::string, double>& new_weights);

    // Configuration management
    void updateConfiguration(const DecisionConfig& new_config);
    const DecisionConfig& getConfiguration() const { return config_; }

    // Real-time monitoring
    struct EngineStatus {
        bool is_operational;
        std::chrono::system_clock::time_point last_decision_time;
        int decisions_made_today;
        double current_algorithm_correlation;     // Real correlation tracking
        std::vector<std::string> operational_issues;

        // Component status
        bool signal_generator_healthy;
        bool data_processor_healthy;
        double data_quality_average;
    };

    EngineStatus getEngineStatus() const;

    // Backtesting interface
    struct BacktestResult {
        std::vector<TradingDecision> decisions;
        std::vector<double> actual_outcomes;
        double total_return;
        double volatility;
        double sharpe_ratio;
        double max_drawdown;
        double win_rate;
        int total_trades;
        DecisionPerformance performance_analysis;
    };

    BacktestResult runBacktest(const std::vector<std::string>& pairs,
                              const std::chrono::system_clock::time_point& start_date,
                              const std::chrono::system_clock::time_point& end_date);

    // Real correlation validation
    double getCurrentAlgorithmCorrelation() const;
    bool meetsTargetCorrelation(double target = 0.85) const;

    // Risk management integration
    bool checkRiskLimits(const DecisionBatch& batch) const;
    DecisionBatch applyRiskManagement(const DecisionBatch& raw_batch);

    // Market regime detection and adaptation
    void updateMarketRegime(const std::map<std::string, RealTimeMarketDataProcessor::MarketRegime>& regimes);

    // Decision explanation and transparency
    struct DecisionExplanation {
        std::string pair_name;
        std::string decision_summary;             // One-line decision summary
        std::vector<std::string> supporting_factors; // Key supporting factors
        std::vector<std::string> risk_considerations; // Risk factors considered
        std::map<std::string, double> numerical_breakdown; // Numerical factor breakdown
        double confidence_justification;          // Why this confidence level
    };

    DecisionExplanation explainDecision(const TradingDecision& decision) const;
};

/**
 * Decision Engine Factory - Creates configured engines for different use cases
 */
class DecisionEngineFactory {
public:
    // Create standard decision engine
    static std::unique_ptr<AlgorithmDecisionEngine> createStandard();

    // Create conservative decision engine (lower risk)
    static std::unique_ptr<AlgorithmDecisionEngine> createConservative();

    // Create aggressive decision engine (higher risk)
    static std::unique_ptr<AlgorithmDecisionEngine> createAggressive();

    // Create backtesting engine
    static std::unique_ptr<AlgorithmDecisionEngine> createForBacktesting();

    // Create engine with custom configuration
    static std::unique_ptr<AlgorithmDecisionEngine> createCustom(const AlgorithmDecisionEngine::DecisionConfig& config);
};

} // namespace Algorithm
} // namespace CryptoClaude