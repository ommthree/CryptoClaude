#pragma once

#include "RealTimeDataStreamManager.h"
#include "RealMarketSignalGenerator.h"
#include "RealCorrelationValidator.h"
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
 * Real-Time Signal Processor - Day 22 Implementation
 * Processes live market data streams to generate real-time trading signals
 * Integrates with correlation validation and decision engines
 */
class RealTimeSignalProcessor {
public:
    // Real-time signal with enhanced metadata
    struct LiveTradingSignal {
        std::string signal_id;
        std::string pair_name;
        std::chrono::system_clock::time_point generated_at;

        // Core signal properties
        double signal_strength;              // -1.0 to +1.0
        double confidence_score;             // 0.0 to 1.0
        double predicted_return;             // Expected return
        std::chrono::hours prediction_horizon;

        // Real-time enhancements
        double real_time_correlation;        // Current algorithm correlation
        double market_regime_adjustment;     // Regime-based signal adjustment
        double liquidity_score;              // Market liquidity assessment
        double volatility_adjustment;        // Volatility-based adjustment

        // Data quality context
        double data_quality_score;           // Quality of underlying data
        int participating_exchanges;         // Number of exchanges providing data
        std::chrono::milliseconds data_latency; // Freshness of data

        // Risk assessment
        double risk_score;                   // 0.0 to 1.0 (higher = riskier)
        double portfolio_impact;             // Expected impact on portfolio
        std::vector<std::string> risk_factors; // Identified risk factors

        // Performance tracking
        std::string correlation_status;      // TRS compliance status
        double trs_gap;                     // Gap from TRS target (0.85)
        bool meets_quality_threshold;       // Data quality sufficient

        // Market context
        RealTimeDataStreamManager::MarketRegime current_regime;
        std::vector<std::string> supporting_indicators;
        std::map<std::string, double> technical_indicators;

        LiveTradingSignal() : signal_strength(0), confidence_score(0), predicted_return(0),
                             prediction_horizon(std::chrono::hours{24}), real_time_correlation(0),
                             market_regime_adjustment(0), liquidity_score(0), volatility_adjustment(0),
                             data_quality_score(0), participating_exchanges(0),
                             data_latency(std::chrono::milliseconds{0}), risk_score(0.5),
                             portfolio_impact(0), trs_gap(0), meets_quality_threshold(false) {}
    };

    // Signal processing configuration
    struct ProcessingConfig {
        // Signal generation parameters
        double min_confidence_threshold = 0.6;
        double min_data_quality = 0.8;
        std::chrono::milliseconds max_data_latency{500};
        int min_participating_exchanges = 2;

        // Correlation requirements
        double trs_target_correlation = 0.85;
        double min_acceptable_correlation = 0.70;
        bool require_trs_compliance = true;

        // Market regime adjustments
        double volatile_regime_penalty = 0.2;    // Reduce signal strength in volatile markets
        double illiquid_regime_penalty = 0.3;    // Reduce signal strength in illiquid markets
        double trending_regime_boost = 0.1;      // Boost signal strength in trending markets

        // Processing timing
        std::chrono::milliseconds processing_interval{100}; // 10Hz processing
        std::chrono::seconds signal_timeout{300};           // 5 minute signal timeout
        int max_concurrent_signals = 50;

        // Quality filters
        bool enable_regime_filtering = true;
        bool enable_liquidity_filtering = true;
        bool enable_correlation_filtering = true;
        bool enable_volatility_adjustment = true;

        ProcessingConfig() {}
    };

    // Processing statistics and monitoring
    struct ProcessingStats {
        // Signal generation metrics
        int signals_generated_total = 0;
        int signals_generated_1h = 0;
        int signals_generated_24h = 0;
        double average_signal_confidence = 0.0;

        // Quality metrics
        double average_data_quality = 0.0;
        double average_correlation = 0.0;
        int trs_compliance_violations = 0;
        std::chrono::milliseconds average_processing_latency{0};

        // Performance metrics
        int signals_above_threshold = 0;
        int signals_filtered_quality = 0;
        int signals_filtered_correlation = 0;
        int signals_filtered_regime = 0;

        // Real-time status
        bool is_processing = false;
        std::chrono::system_clock::time_point last_signal_generated;
        std::chrono::system_clock::time_point last_correlation_check;
        std::string current_status;

        // Error tracking
        int processing_errors_24h = 0;
        std::vector<std::string> recent_errors;
    };

    // Signal aggregation for portfolio-level decisions
    struct SignalAggregation {
        std::chrono::system_clock::time_point aggregated_at;

        // Portfolio-level signals
        std::vector<LiveTradingSignal> active_signals;
        double portfolio_signal_strength;    // Aggregate signal strength
        double portfolio_confidence;         // Aggregate confidence
        double portfolio_risk_score;         // Aggregate risk

        // Correlation analysis
        double portfolio_correlation;        // Current portfolio correlation
        bool portfolio_meets_trs;           // Portfolio TRS compliance
        double correlation_stability;       // Correlation stability metric

        // Market regime impact
        std::map<RealTimeDataStreamManager::MarketRegime, int> regime_distribution;
        RealTimeDataStreamManager::MarketRegime dominant_regime;

        // Quality assessment
        double aggregate_data_quality;
        int total_participating_exchanges;
        bool meets_processing_standards;

        SignalAggregation() : portfolio_signal_strength(0), portfolio_confidence(0),
                             portfolio_risk_score(0.5), portfolio_correlation(0),
                             portfolio_meets_trs(false), correlation_stability(0),
                             aggregate_data_quality(0), total_participating_exchanges(0),
                             meets_processing_standards(false) {}
    };

private:
    // Core components
    std::unique_ptr<RealTimeDataStreamManager> data_stream_manager_;
    std::unique_ptr<RealMarketSignalGenerator> signal_generator_;
    std::unique_ptr<RealCorrelationValidator> correlation_validator_;
    std::unique_ptr<AlgorithmDecisionEngine> decision_engine_;

    // Configuration and state
    ProcessingConfig config_;
    ProcessingStats stats_;
    std::atomic<bool> is_processing_{false};

    // Threading and synchronization
    std::thread processing_thread_;
    std::mutex signal_mutex_;
    std::mutex stats_mutex_;

    // Signal storage and management
    std::map<std::string, LiveTradingSignal> active_signals_;
    std::queue<LiveTradingSignal> recent_signals_;
    std::map<std::string, std::chrono::system_clock::time_point> signal_timestamps_;

    // Correlation tracking for real-time validation
    std::unique_ptr<RealCorrelationValidator::RealTimeCorrelationMonitor> correlation_monitor_;
    std::deque<double> correlation_history_;

    // Event callbacks
    std::function<void(const LiveTradingSignal&)> signal_callback_;
    std::function<void(const SignalAggregation&)> aggregation_callback_;
    std::function<void(const std::string&)> error_callback_;

    // Core processing methods
    void processRealTimeData();
    LiveTradingSignal generateLiveSignal(const std::string& pair_name);
    void validateSignalQuality(LiveTradingSignal& signal);
    void applyMarketRegimeAdjustments(LiveTradingSignal& signal);
    void calculateRealTimeCorrelation(LiveTradingSignal& signal);

    // Signal management
    void addActiveSignal(const LiveTradingSignal& signal);
    void removeExpiredSignals();
    void updateSignalStats(const LiveTradingSignal& signal);

    // Aggregation and portfolio analysis
    SignalAggregation aggregateSignals() const;
    double calculatePortfolioCorrelation() const;
    void assessPortfolioRisk(SignalAggregation& aggregation) const;

    // Quality and compliance monitoring
    bool meetsDataQualityStandards(const LiveTradingSignal& signal) const;
    bool meetsCorrelationRequirements(const LiveTradingSignal& signal) const;
    void updateTRSComplianceStatus();

public:
    explicit RealTimeSignalProcessor(const ProcessingConfig& config = ProcessingConfig());
    ~RealTimeSignalProcessor();

    // Processing control
    bool startProcessing();
    void stopProcessing();
    void pauseProcessing();
    void resumeProcessing();
    bool isProcessing() const { return is_processing_.load(); }

    // Configuration management
    void updateConfig(const ProcessingConfig& new_config);
    const ProcessingConfig& getConfig() const { return config_; }

    // Signal access
    std::vector<LiveTradingSignal> getActiveSignals() const;
    LiveTradingSignal getSignal(const std::string& signal_id) const;
    std::vector<LiveTradingSignal> getSignalsForPair(const std::string& pair_name) const;
    std::vector<LiveTradingSignal> getRecentSignals(int count = 10) const;

    // Signal filtering and querying
    std::vector<LiveTradingSignal> getSignalsAboveConfidence(double min_confidence) const;
    std::vector<LiveTradingSignal> getSignalsForRegime(RealTimeDataStreamManager::MarketRegime regime) const;
    std::vector<LiveTradingSignal> getTRSCompliantSignals() const;

    // Portfolio-level analysis
    SignalAggregation getCurrentAggregation() const;
    double getCurrentPortfolioCorrelation() const;
    bool isPortfolioTRSCompliant() const;

    // Performance and monitoring
    ProcessingStats getProcessingStats() const;
    double getCurrentTRSGap() const;
    std::vector<std::string> getActiveRiskFactors() const;

    // Real-time correlation monitoring
    struct CorrelationMonitoringResult {
        double current_correlation;
        double correlation_trend;           // Trend over last hour
        double correlation_stability;       // Standard deviation
        bool is_statistically_significant;
        std::chrono::system_clock::time_point last_update;

        // TRS compliance
        bool meets_trs_target;
        double trs_gap;
        std::string compliance_status;
    };

    CorrelationMonitoringResult getCorrelationMonitoring() const;
    std::vector<double> getCorrelationHistory(int hours = 24) const;

    // Market regime analysis
    struct RealtimeMarketAnalysis {
        std::map<std::string, RealTimeDataStreamManager::MarketRegimeInfo> current_regimes;
        RealTimeDataStreamManager::MarketRegime dominant_regime;
        double regime_stability;            // How stable current regime is
        std::chrono::system_clock::time_point analysis_time;

        // Impact on signals
        double regime_signal_impact;        // How much regime affects signals
        std::vector<std::string> regime_warnings;
        bool favorable_for_trading;
    };

    RealtimeMarketAnalysis getCurrentMarketAnalysis() const;

    // Advanced signal processing features
    struct SignalValidationResult {
        std::string signal_id;
        bool passed_validation;
        std::vector<std::string> validation_issues;
        std::vector<std::string> quality_warnings;
        double validation_score;            // 0.0 to 1.0
        std::chrono::system_clock::time_point validated_at;
    };

    SignalValidationResult validateSignal(const std::string& signal_id) const;
    std::vector<SignalValidationResult> validateAllActiveSignals() const;

    // Callback registration
    void setSignalCallback(std::function<void(const LiveTradingSignal&)> callback);
    void setAggregationCallback(std::function<void(const SignalAggregation&)> callback);
    void setErrorCallback(std::function<void(const std::string&)> callback);

    // Integration interfaces
    bool integrateWithDataStream(std::unique_ptr<RealTimeDataStreamManager> data_manager);
    bool integrateWithSignalGenerator(std::unique_ptr<RealMarketSignalGenerator> signal_gen);
    bool integrateWithCorrelationValidator(std::unique_ptr<RealCorrelationValidator> correlation_val);

    // Performance optimization
    void optimizeProcessingParameters();
    void enableHighFrequencyMode(bool enable = true);
    void setProcessingPriority(int priority); // Thread priority adjustment

    // Emergency controls
    void emergencyStopSignalGeneration();
    void emergencyRestartProcessing();
    bool isInEmergencyMode() const;

    // Static utility methods
    static double calculateSignalDecay(const std::chrono::system_clock::time_point& signal_time,
                                     std::chrono::hours max_age = std::chrono::hours{4});
    static bool isSignalExpired(const LiveTradingSignal& signal,
                               std::chrono::hours expiry_time = std::chrono::hours{6});
    static std::string generateSignalId(const std::string& pair_name);
};

} // namespace Algorithm
} // namespace CryptoClaude