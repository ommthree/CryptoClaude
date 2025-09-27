#pragma once

#include "../Market/LiveMarketDataProvider.h"
#include "../Algorithm/TRSComplianceEngine.h"
#include "../Database/DatabaseManager.h"
#include <memory>
#include <vector>
#include <map>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>
#include <functional>

namespace CryptoClaude {
namespace Risk {

/**
 * Production Risk Manager - Day 23 Implementation
 * Real-time risk monitoring and control for live trading environment
 * Enforces position limits, portfolio risk, and regulatory compliance
 */
class ProductionRiskManager {
public:
    // Live account configuration
    struct AccountConfiguration {
        std::string account_id;
        std::string broker_name;

        // Capital limits
        double total_capital;                    // Total account capital
        double available_capital;                // Currently available for trading
        double reserved_capital;                 // Reserved/margin requirements
        double max_daily_loss_limit;            // Maximum daily loss allowed
        double max_drawdown_limit;               // Maximum portfolio drawdown

        // Position limits
        double max_position_size_pct = 0.05;    // Max 5% per position
        double max_portfolio_exposure_pct = 0.25; // Max 25% total exposure
        int max_concurrent_positions = 10;       // Maximum open positions
        double max_sector_concentration = 0.15;  // Max 15% in any sector

        // Risk parameters
        double stop_loss_percentage = 0.03;      // 3% stop loss
        double portfolio_var_limit = 0.02;       // 2% daily VaR limit
        double correlation_limit = 0.30;         // Max correlation exposure

        // Emergency controls
        bool emergency_only_mode = false;        // Emergency mode flag
        std::chrono::hours position_timeout{72}; // Max position holding time

        AccountConfiguration() : total_capital(0), available_capital(0), reserved_capital(0),
                               max_daily_loss_limit(0) {}
    };

    // Live position in production environment
    struct LivePosition {
        std::string position_id;
        std::string symbol;
        std::string account_id;
        std::chrono::system_clock::time_point opened_at;

        // Position details
        double quantity;                         // Position size in base currency
        double entry_price;                     // Average entry price
        double current_price;                   // Current market price
        double position_value_usd;              // Current USD value
        bool is_long;                           // Long or short position

        // Risk metrics
        double unrealized_pnl;                  // Current unrealized P&L
        double unrealized_pnl_pct;              // P&L as percentage
        double stop_loss_price;                 // Stop loss trigger price
        double take_profit_price;               // Take profit target
        double max_adverse_excursion;           // Worst unrealized loss
        double max_favorable_excursion;         // Best unrealized gain

        // Risk controls
        double position_risk_score;             // Composite risk score (0-1)
        double portfolio_impact_pct;            // Impact on total portfolio
        std::vector<std::string> risk_flags;    // Active risk warnings

        // Compliance tracking
        double trs_correlation_contribution;     // Contribution to TRS correlation
        bool is_trs_compliant;                  // TRS compliance status

        LivePosition() : quantity(0), entry_price(0), current_price(0), position_value_usd(0),
                        is_long(true), unrealized_pnl(0), unrealized_pnl_pct(0),
                        stop_loss_price(0), take_profit_price(0), max_adverse_excursion(0),
                        max_favorable_excursion(0), position_risk_score(0),
                        portfolio_impact_pct(0), trs_correlation_contribution(0),
                        is_trs_compliant(true) {}
    };

    // Real-time risk assessment
    struct RealTimeRiskAssessment {
        std::chrono::system_clock::time_point assessment_time;

        // Portfolio risk metrics
        double current_portfolio_value;
        double total_unrealized_pnl;
        double daily_realized_pnl;
        double current_drawdown_pct;
        double portfolio_var_1d;                // 1-day Value at Risk
        double portfolio_leverage;              // Effective leverage ratio

        // Position risk summary
        int open_positions_count;
        double largest_position_pct;
        double average_position_size;
        double concentration_risk_score;

        // Compliance status
        Algorithm::TRSComplianceEngine::ComplianceStatus trs_status;
        double current_algorithm_correlation;
        double trs_gap;                         // Gap to target correlation

        // Risk warnings
        enum class RiskLevel {
            GREEN,      // Normal operations
            YELLOW,     // Caution required
            ORANGE,     // High risk - reduce exposure
            RED         // Emergency - stop trading
        };

        RiskLevel overall_risk_level;
        std::vector<std::string> active_risk_warnings;
        std::vector<std::string> recommended_actions;

        // Market conditions impact
        double market_stress_multiplier;        // Risk adjustment for market conditions
        std::string dominant_market_regime;

        RealTimeRiskAssessment() : current_portfolio_value(0), total_unrealized_pnl(0),
                                  daily_realized_pnl(0), current_drawdown_pct(0),
                                  portfolio_var_1d(0), portfolio_leverage(0),
                                  open_positions_count(0), largest_position_pct(0),
                                  average_position_size(0), concentration_risk_score(0),
                                  trs_status(Algorithm::TRSComplianceEngine::ComplianceStatus::UNKNOWN),
                                  current_algorithm_correlation(0), trs_gap(0),
                                  overall_risk_level(RiskLevel::GREEN), market_stress_multiplier(1.0) {}
    };

    // Risk violation event
    struct RiskViolation {
        std::string violation_id;
        std::chrono::system_clock::time_point detected_at;
        std::string violation_type;             // "position_limit", "portfolio_risk", "trs_compliance", etc.

        // Violation details
        std::string description;
        double severity_score;                  // 0.0 to 1.0
        std::string affected_position_id;       // If position-specific
        std::string affected_account_id;

        // Current values vs limits
        double current_value;
        double limit_value;
        double violation_magnitude;

        // Automated response
        std::vector<std::string> automated_actions_taken;
        bool requires_manual_intervention;
        std::chrono::system_clock::time_point action_deadline;

        // Resolution tracking
        bool is_resolved;
        std::chrono::system_clock::time_point resolved_at;
        std::string resolution_method;

        RiskViolation() : severity_score(0), current_value(0), limit_value(0),
                         violation_magnitude(0), requires_manual_intervention(false),
                         is_resolved(false) {}
    };

private:
    // Core configuration
    AccountConfiguration account_config_;
    std::unique_ptr<Market::LiveMarketDataProvider> market_data_provider_;
    std::unique_ptr<Algorithm::TRSComplianceEngine> compliance_engine_;
    std::unique_ptr<Database::DatabaseManager> database_manager_;

    // Risk monitoring state
    std::atomic<bool> is_monitoring_{false};
    std::atomic<bool> emergency_stop_triggered_{false};
    mutable std::mutex positions_mutex_;
    mutable std::mutex risk_mutex_;
    mutable std::mutex violations_mutex_;

    // Threading
    std::thread risk_monitoring_thread_;
    std::thread position_monitoring_thread_;
    std::thread compliance_monitoring_thread_;

    // Live data
    std::map<std::string, LivePosition> active_positions_;
    std::vector<RiskViolation> active_violations_;
    RealTimeRiskAssessment current_risk_assessment_;

    // Historical tracking
    std::deque<RealTimeRiskAssessment> risk_assessment_history_;
    double portfolio_peak_value_;
    std::chrono::system_clock::time_point last_peak_time_;

    // Callback functions
    std::function<void(const RiskViolation&)> violation_callback_;
    std::function<void(const RealTimeRiskAssessment&)> assessment_callback_;
    std::function<void(const std::string&)> emergency_callback_;

    // Core monitoring methods
    void runRiskMonitoring();
    void runPositionMonitoring();
    void runComplianceMonitoring();

    // Risk assessment methods
    void updateRealTimeRiskAssessment();
    void calculatePortfolioRisk();
    void assessPositionRisks();
    void checkRiskLimits();

    // Position monitoring
    void updatePositionMetrics();
    void checkStopLossLevels();
    void checkPositionTimeouts();
    void calculatePositionRiskScores();

    // Compliance monitoring
    void monitorTRSCompliance();
    void checkRegulatoryRequirements();

    // Violation handling
    void detectRiskViolations();
    void handleRiskViolation(const RiskViolation& violation);
    void executeAutomatedResponse(const RiskViolation& violation);

    // Emergency procedures
    void triggerEmergencyStop(const std::string& reason);
    void executeEmergencyPositionReduction();

public:
    explicit ProductionRiskManager(const AccountConfiguration& config);
    ~ProductionRiskManager();

    // Configuration management
    void updateAccountConfiguration(const AccountConfiguration& config);
    const AccountConfiguration& getAccountConfiguration() const { return account_config_; }

    // Integration
    bool integrateMarketDataProvider(std::unique_ptr<Market::LiveMarketDataProvider> provider);
    bool integrateComplianceEngine(std::unique_ptr<Algorithm::TRSComplianceEngine> engine);
    bool integrateDatabaseManager(std::unique_ptr<Database::DatabaseManager> db_manager);

    // Risk monitoring control
    bool startRiskMonitoring();
    void stopRiskMonitoring();
    bool isMonitoring() const { return is_monitoring_.load(); }

    // Position management
    bool addPosition(const LivePosition& position);
    bool updatePosition(const std::string& position_id, const LivePosition& updated_position);
    bool removePosition(const std::string& position_id);
    std::vector<LivePosition> getActivePositions() const;
    LivePosition getPosition(const std::string& position_id) const;

    // Pre-trade risk checks
    struct TradeRiskCheck {
        bool is_approved;
        std::string rejection_reason;
        double max_allowed_quantity;
        double estimated_portfolio_impact;
        std::vector<std::string> risk_warnings;
        double confidence_score;
    };

    TradeRiskCheck evaluateProposedTrade(const std::string& symbol,
                                        double quantity,
                                        double estimated_price,
                                        bool is_long) const;

    // Risk assessment access
    RealTimeRiskAssessment getCurrentRiskAssessment() const;
    std::vector<RealTimeRiskAssessment> getRiskAssessmentHistory(std::chrono::hours lookback = std::chrono::hours{24}) const;

    // Violation management
    std::vector<RiskViolation> getActiveViolations() const;
    std::vector<RiskViolation> getViolationHistory(std::chrono::hours lookback = std::chrono::hours{168}) const; // 1 week
    bool acknowledgeViolation(const std::string& violation_id, const std::string& acknowledgment_note);

    // Portfolio analytics
    struct PortfolioAnalytics {
        // Return metrics
        double total_return_pct;
        double daily_return_pct;
        double annualized_return_pct;
        double volatility;
        double sharpe_ratio;

        // Risk metrics
        double max_drawdown_pct;
        double current_drawdown_pct;
        double var_1d_pct;
        double var_5d_pct;
        double expected_shortfall;

        // Efficiency metrics
        double information_ratio;
        double calmar_ratio;
        double sortino_ratio;
        double omega_ratio;

        // Position metrics
        double average_holding_period_hours;
        double position_turnover;
        double concentration_herfindahl;

        PortfolioAnalytics() : total_return_pct(0), daily_return_pct(0), annualized_return_pct(0),
                              volatility(0), sharpe_ratio(0), max_drawdown_pct(0),
                              current_drawdown_pct(0), var_1d_pct(0), var_5d_pct(0),
                              expected_shortfall(0), information_ratio(0), calmar_ratio(0),
                              sortino_ratio(0), omega_ratio(0), average_holding_period_hours(0),
                              position_turnover(0), concentration_herfindahl(0) {}
    };

    PortfolioAnalytics calculatePortfolioAnalytics() const;

    // Risk reporting
    struct RiskReport {
        std::chrono::system_clock::time_point report_time;
        RealTimeRiskAssessment current_assessment;
        std::vector<RiskViolation> recent_violations;
        PortfolioAnalytics portfolio_analytics;

        // Executive summary
        std::string risk_status_summary;
        std::vector<std::string> key_risks;
        std::vector<std::string> recommended_actions;

        // Forward-looking
        std::vector<std::string> emerging_risks;
        double stress_test_loss_estimate;
    };

    RiskReport generateRiskReport() const;
    bool exportRiskReport(const RiskReport& report, const std::string& filename, const std::string& format = "json") const;

    // Emergency controls
    void triggerManualEmergencyStop(const std::string& reason);
    void clearEmergencyStop(const std::string& authorization_code);
    bool isEmergencyStopActive() const { return emergency_stop_triggered_.load(); }

    // Position risk controls
    bool forceClosePosition(const std::string& position_id, const std::string& reason);
    bool reducePosition(const std::string& position_id, double new_quantity, const std::string& reason);
    bool setStopLoss(const std::string& position_id, double stop_price);

    // Callback registration
    void setViolationCallback(std::function<void(const RiskViolation&)> callback);
    void setAssessmentCallback(std::function<void(const RealTimeRiskAssessment&)> callback);
    void setEmergencyCallback(std::function<void(const std::string&)> callback);

    // Stress testing
    struct StressTestScenario {
        std::string scenario_name;
        std::map<std::string, double> price_shocks;     // Symbol -> shock percentage
        double correlation_breakdown_factor = 2.0;      // Factor by which correlations increase
        std::chrono::hours scenario_duration{24};       // Duration to apply shock
    };

    struct StressTestResult {
        StressTestScenario scenario;
        double portfolio_loss_pct;
        double peak_drawdown_pct;
        int positions_stopped_out;
        bool would_violate_risk_limits;
        std::vector<std::string> limit_violations;
        std::chrono::minutes estimated_recovery_time;
    };

    std::vector<StressTestResult> runStressTests(const std::vector<StressTestScenario>& scenarios) const;

    // Static utility methods
    static std::string riskLevelToString(RealTimeRiskAssessment::RiskLevel level);
    static RealTimeRiskAssessment::RiskLevel stringToRiskLevel(const std::string& level_str);
    static double calculatePositionRisk(const LivePosition& position, double portfolio_value);
    static double calculatePortfolioConcentration(const std::vector<LivePosition>& positions);
    static std::string generateViolationId();
};

} // namespace Risk
} // namespace CryptoClaude