#pragma once

#include "CoreTradingAlgorithm.h"
#include "../Database/DatabaseManager.h"
#include "../Analytics/TechnicalIndicators.h"
#include "../Sentiment/SentimentQualityManager.h"
#include <memory>
#include <vector>
#include <chrono>

namespace CryptoClaude {
namespace Algorithm {

// Forward declarations
class RealCorrelationValidator;

/**
 * Real Market Signal Generator - Day 20 Implementation
 * Replaces placeholder/hash-based signals with real market data processing
 * Core component for eliminating "algorithm implementation void"
 */
class RealMarketSignalGenerator {
public:
    // Real market data structure for signal generation
    struct RealMarketSnapshot {
        std::string symbol;
        double current_price;
        double price_24h_ago;
        double price_7d_ago;
        double volume_24h;
        double volume_7d_avg;
        double market_cap;
        std::chrono::system_clock::time_point timestamp;

        // Technical indicator values
        double rsi_14;
        double macd_signal;
        double macd_histogram;
        double bb_upper, bb_middle, bb_lower;
        double ema_20, ema_50;

        // Market structure
        double bid_ask_spread;
        double orderbook_depth;

        RealMarketSnapshot() : current_price(0), price_24h_ago(0), price_7d_ago(0),
                              volume_24h(0), volume_7d_avg(0), market_cap(0),
                              rsi_14(50), macd_signal(0), macd_histogram(0),
                              bb_upper(0), bb_middle(0), bb_lower(0),
                              ema_20(0), ema_50(0), bid_ask_spread(0), orderbook_depth(0) {}
    };

    // Real signal generation result
    struct RealTradingSignal {
        std::string pair_name;                    // e.g., "BTC/ETH"
        double signal_strength;                   // -1.0 to +1.0
        double confidence_score;                  // 0.0 to 1.0
        double predicted_return;                  // Expected return over horizon
        std::chrono::hours prediction_horizon;    // Signal validity period

        // Real market justification
        std::vector<std::string> signal_reasons;  // Human-readable justification
        std::map<std::string, double> indicator_contributions;

        // Risk assessment
        double predicted_volatility;
        double risk_score;                        // 0.0 to 1.0 (higher = riskier)

        // Validation tracking
        std::string signal_id;
        std::chrono::system_clock::time_point generated_at;
        bool is_live_signal;                      // True = real-time, False = backtest

        RealTradingSignal() : signal_strength(0), confidence_score(0), predicted_return(0),
                             prediction_horizon(std::chrono::hours{24}), predicted_volatility(0),
                             risk_score(0.5), is_live_signal(true),
                             generated_at(std::chrono::system_clock::now()) {}
    };

    // Configuration for real signal generation
    struct RealSignalConfig {
        // Minimum data requirements
        int min_historical_days = 90;             // Minimum history for reliable signals
        double min_volume_threshold = 100000.0;   // Minimum 24h volume (USD)
        double min_market_cap = 10000000.0;       // Minimum market cap (USD)

        // Technical analysis parameters
        int rsi_period = 14;
        int macd_fast = 12;
        int macd_slow = 26;
        int macd_signal = 9;
        int bb_period = 20;
        double bb_std_dev = 2.0;

        // Signal thresholds
        double rsi_oversold = 30.0;
        double rsi_overbought = 70.0;
        double min_confidence = 0.6;              // Minimum confidence for signal
        double max_risk_score = 0.8;              // Maximum allowed risk

        // Cross-pair analysis
        bool enable_pair_correlation = true;
        double max_pair_correlation = 0.8;        // Max correlation for pair trading
        int correlation_lookback_days = 60;

        RealSignalConfig() {}
    };

private:
    RealSignalConfig config_;
    std::unique_ptr<Database::DatabaseManager> db_manager_;
    std::unique_ptr<Analytics::TechnicalIndicators> technical_analyzer_;
    std::unique_ptr<Sentiment::SentimentQualityManager> sentiment_analyzer_;

    // Real market data cache
    std::map<std::string, std::vector<RealMarketSnapshot>> market_data_cache_;
    std::chrono::system_clock::time_point last_cache_update_;

    // Real market data processing
    RealMarketSnapshot loadRealMarketData(const std::string& symbol);
    std::vector<RealMarketSnapshot> loadHistoricalData(const std::string& symbol, int days);

    // Real technical analysis (replaces hash-based placeholders)
    double calculateRealRSI(const std::vector<double>& prices, int period);
    std::tuple<double, double, double> calculateRealMACD(const std::vector<double>& prices,
                                                        int fast, int slow, int signal);
    std::tuple<double, double, double> calculateRealBollingerBands(const std::vector<double>& prices,
                                                                  int period, double std_dev);
    double calculateRealEMA(const std::vector<double>& prices, int period);

    // Real signal generation logic
    RealTradingSignal generateTechnicalSignal(const std::string& pair,
                                             const RealMarketSnapshot& base_data,
                                             const RealMarketSnapshot& quote_data);

    RealTradingSignal generateMomentumSignal(const std::string& pair,
                                           const std::vector<RealMarketSnapshot>& base_history,
                                           const std::vector<RealMarketSnapshot>& quote_history);

    RealTradingSignal generateMeanReversionSignal(const std::string& pair,
                                                 const std::vector<RealMarketSnapshot>& base_history,
                                                 const std::vector<RealMarketSnapshot>& quote_history);

    // Real correlation calculation using RealCorrelationValidator
    double calculateRealCorrelation(const std::vector<double>& returns1,
                                   const std::vector<double>& returns2);

    // Integration with RealCorrelationValidator for TRS compliance
    std::unique_ptr<class RealCorrelationValidator> correlation_validator_;

    // Signal validation
    bool validateSignalQuality(const RealTradingSignal& signal);
    double calculateSignalConfidence(const std::string& pair,
                                   const RealMarketSnapshot& base_data,
                                   const RealMarketSnapshot& quote_data);

public:
    explicit RealMarketSignalGenerator(const RealSignalConfig& config = RealSignalConfig());
    ~RealMarketSignalGenerator() = default;

    // Main real signal generation interface
    std::vector<RealTradingSignal> generateRealTradingSignals(const std::vector<std::string>& pairs);

    // Single pair real signal
    RealTradingSignal generateRealPairSignal(const std::string& pair);

    // Real market data interface
    bool updateMarketDataCache(const std::vector<std::string>& symbols);
    std::vector<std::string> getAvailableSymbols() const;
    RealMarketSnapshot getCurrentMarketSnapshot(const std::string& symbol);

    // Real signal validation and backtesting
    struct RealBacktestResult {
        std::string pair_name;
        std::vector<RealTradingSignal> generated_signals;
        std::vector<double> actual_returns;
        double correlation_coefficient;           // Real correlation vs predicted
        double hit_rate;                         // % of profitable signals
        double average_return;                   // Average actual return
        double sharpe_ratio;                     // Risk-adjusted performance
        int total_signals;
        int profitable_signals;
    };

    RealBacktestResult backtestRealSignals(const std::string& pair,
                                          const std::chrono::system_clock::time_point& start_date,
                                          const std::chrono::system_clock::time_point& end_date);

    // Configuration management
    void updateConfiguration(const RealSignalConfig& new_config);
    const RealSignalConfig& getConfiguration() const { return config_; }

    // Real-time monitoring
    struct SignalPerformanceMetrics {
        double live_correlation;                  // Real-time correlation tracking
        double current_accuracy;                  // Recent signal accuracy
        int signals_generated_24h;               // Signals in last 24h
        double average_confidence;               // Average signal confidence
        std::map<std::string, double> pair_performance; // Per-pair performance
    };

    SignalPerformanceMetrics getPerformanceMetrics() const;

    // Market regime adaptation
    void adaptToMarketRegime(const std::string& regime_type, double regime_confidence);

    // Database integration for signal tracking
    bool saveSignalToDB(const RealTradingSignal& signal);
    bool updateSignalOutcome(const std::string& signal_id, double actual_return);
    std::vector<RealTradingSignal> loadHistoricalSignals(const std::string& pair, int days);

    // Real correlation validation
    double calculateCurrentAlgorithmCorrelation() const;
    bool meetsTargetCorrelation(double target_correlation = 0.85) const;
};

/**
 * Real Market Data Validator - Ensures signal generation uses only real data
 * Critical for eliminating the "simulated correlation fraud risk"
 */
class RealMarketDataValidator {
public:
    // Data quality assessment
    struct DataQualityReport {
        std::string symbol;
        int total_records;
        int valid_records;
        double completeness_percentage;          // % of non-null/non-zero data
        double quality_score;                    // 0-100 overall quality
        std::vector<std::string> quality_issues; // Identified problems

        bool meets_minimum_standard() const { return quality_score >= 95.0 && completeness_percentage >= 98.0; }
    };

    // Validate real market data
    static DataQualityReport validateMarketData(const std::vector<RealMarketSignalGenerator::RealMarketSnapshot>& data);

    // Detect synthetic/simulated data patterns
    static bool detectSyntheticData(const std::vector<double>& values);

    // Statistical validation of data authenticity
    static bool validateDataAuthenticity(const std::vector<double>& prices,
                                        const std::vector<double>& volumes,
                                        double significance_level = 0.05);

    // Cross-reference with external data sources
    static bool crossValidateWithExternalSource(const std::string& symbol,
                                               const RealMarketSignalGenerator::RealMarketSnapshot& data);
};

} // namespace Algorithm
} // namespace CryptoClaude