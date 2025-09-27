#pragma once

#include <vector>
#include <string>
#include <map>
#include <memory>
#include <chrono>
#include <functional>
#include "../Database/Models/MarketData.h"

namespace CryptoClaude {
namespace Algorithm {

/**
 * Historical Data Manager - Handles integration of 2+ years of historical data
 * Critical component for real correlation validation and algorithm backtesting
 * Replaces simulated data with actual market history
 */
class HistoricalDataManager {
public:
    // Historical OHLCV structure with enhanced metadata
    struct HistoricalOHLCV {
        std::chrono::system_clock::time_point timestamp;
        std::string symbol;
        double open, high, low, close, volume;
        double market_cap;              // Market capitalization if available
        double volume_usd;              // Volume in USD

        // Data quality metrics
        std::string data_source;        // "binance", "coingecko", etc.
        double quality_score;           // 0.0 to 1.0
        bool is_interpolated;           // True if value was interpolated
        bool has_anomaly;               // True if anomaly detected

        HistoricalOHLCV() : open(0), high(0), low(0), close(0), volume(0),
                           market_cap(0), volume_usd(0), quality_score(1.0),
                           is_interpolated(false), has_anomaly(false) {}
    };

    // Historical sentiment data structure
    struct HistoricalSentiment {
        std::chrono::system_clock::time_point timestamp;
        std::string symbol;
        double sentiment_score;         // -1.0 (bearish) to +1.0 (bullish)
        double confidence_level;        // Confidence in sentiment analysis
        int news_article_count;         // Number of articles analyzed
        int social_mention_count;       // Social media mentions

        // Source breakdown
        double news_sentiment;          // News-based sentiment
        double social_sentiment;        // Social media sentiment
        double analyst_sentiment;       // Analyst/expert sentiment

        std::vector<std::string> news_sources;

        HistoricalSentiment() : sentiment_score(0), confidence_level(0),
                               news_article_count(0), social_mention_count(0),
                               news_sentiment(0), social_sentiment(0), analyst_sentiment(0) {}
    };

    // Data quality report for validation
    struct DataQualityReport {
        std::string symbol;
        std::chrono::system_clock::time_point start_date;
        std::chrono::system_clock::time_point end_date;

        // Completeness metrics
        int total_expected_points;      // Expected number of data points
        int actual_data_points;         // Actual data points available
        double completeness_ratio;      // Actual / Expected

        // Quality metrics
        double average_quality_score;   // Average quality across all points
        int interpolated_points;        // Number of interpolated values
        int anomaly_points;             // Number of detected anomalies

        // Data consistency
        int price_gaps;                 // Number of significant price gaps
        int volume_anomalies;           // Volume anomalies detected
        double data_consistency_score;  // Overall consistency metric

        // Source validation
        std::map<std::string, int> source_coverage; // Coverage by source
        bool meets_minimum_standards;   // True if data meets minimum quality

        std::vector<std::string> quality_issues;    // List of identified issues
        std::vector<std::string> recommendations;   // Improvement recommendations

        DataQualityReport() : total_expected_points(0), actual_data_points(0),
                             completeness_ratio(0), average_quality_score(0),
                             interpolated_points(0), anomaly_points(0),
                             price_gaps(0), volume_anomalies(0),
                             data_consistency_score(0), meets_minimum_standards(false) {}
    };

    // Time range specification
    struct TimeRange {
        std::chrono::system_clock::time_point start;
        std::chrono::system_clock::time_point end;

        TimeRange() = default;
        TimeRange(const std::chrono::system_clock::time_point& s,
                 const std::chrono::system_clock::time_point& e)
            : start(s), end(e) {}

        std::chrono::hours duration() const {
            return std::chrono::duration_cast<std::chrono::hours>(end - start);
        }

        int days() const {
            return std::chrono::duration_cast<std::chrono::hours>(end - start).count() / 24;
        }
    };

    // Time frame enumeration
    enum class TimeFrame {
        MINUTE_1,       // 1-minute bars
        MINUTE_5,       // 5-minute bars
        MINUTE_15,      // 15-minute bars
        HOUR_1,         // 1-hour bars
        HOUR_4,         // 4-hour bars
        DAY_1,          // Daily bars
        WEEK_1,         // Weekly bars
        MONTH_1         // Monthly bars
    };

    // Data source configuration
    struct DataSourceConfig {
        std::string source_name;        // "binance", "coingecko", etc.
        std::string api_key;           // API key if required
        int rate_limit_ms;             // Rate limiting in milliseconds
        int max_retries;               // Maximum retry attempts
        bool is_primary_source;        // True for primary data source
        double reliability_weight;      // Weight for data aggregation

        DataSourceConfig() : rate_limit_ms(1000), max_retries(3),
                           is_primary_source(false), reliability_weight(1.0) {}
    };

private:
    std::vector<DataSourceConfig> data_sources_;
    std::map<std::string, std::vector<HistoricalOHLCV>> price_cache_;
    std::map<std::string, std::vector<HistoricalSentiment>> sentiment_cache_;

    // Data quality validation
    DataQualityReport validatePriceData(const std::vector<HistoricalOHLCV>& data);
    void detectAndFlagAnomalies(std::vector<HistoricalOHLCV>& data);
    void interpolateMissingData(std::vector<HistoricalOHLCV>& data);

    // Multi-source data aggregation
    std::vector<HistoricalOHLCV> aggregateMultiSourceData(
        const std::vector<std::vector<HistoricalOHLCV>>& source_data);

    // Data fetching from individual sources
    std::vector<HistoricalOHLCV> fetchFromBinance(
        const std::string& symbol, const TimeRange& range, TimeFrame frame);
    std::vector<HistoricalOHLCV> fetchFromCoinGecko(
        const std::string& symbol, const TimeRange& range, TimeFrame frame);
    std::vector<HistoricalOHLCV> fetchFromCryptoCompare(
        const std::string& symbol, const TimeRange& range, TimeFrame frame);

public:
    explicit HistoricalDataManager();
    ~HistoricalDataManager() = default;

    // Configuration management
    void addDataSource(const DataSourceConfig& config);
    void configureDataSources(const std::vector<DataSourceConfig>& sources);

    // Main data retrieval interface
    std::vector<HistoricalOHLCV> loadHistoricalPrices(
        const std::string& symbol,
        const TimeRange& range,
        TimeFrame frequency = TimeFrame::DAY_1
    );

    std::vector<HistoricalSentiment> loadHistoricalSentiment(
        const std::string& symbol,
        const TimeRange& range
    );

    // Multi-symbol data loading for pairs
    std::map<std::string, std::vector<HistoricalOHLCV>> loadMultiSymbolPrices(
        const std::vector<std::string>& symbols,
        const TimeRange& range,
        TimeFrame frequency = TimeFrame::DAY_1
    );

    // Data quality validation
    DataQualityReport validateHistoricalData(
        const std::string& symbol,
        const TimeRange& range,
        TimeFrame frequency = TimeFrame::DAY_1
    );

    // Batch data quality validation
    std::map<std::string, DataQualityReport> validateMultiSymbolData(
        const std::vector<std::string>& symbols,
        const TimeRange& range,
        TimeFrame frequency = TimeFrame::DAY_1
    );

    // Data preprocessing utilities
    std::vector<HistoricalOHLCV> preprocessPriceData(
        const std::vector<HistoricalOHLCV>& raw_data,
        bool remove_anomalies = true,
        bool interpolate_gaps = true
    );

    // Market regime identification from historical data
    enum class MarketRegime {
        BULL,           // Sustained upward trend
        BEAR,           // Sustained downward trend
        SIDEWAYS,       // Range-bound trading
        VOLATILE,       // High volatility, unclear direction
        TRANSITION      // Regime change period
    };

    struct RegimeIdentification {
        std::chrono::system_clock::time_point timestamp;
        MarketRegime regime;
        double confidence;              // Confidence in regime classification
        double trend_strength;          // Strength of the trend
        std::chrono::hours regime_duration; // How long this regime has lasted
    };

    std::vector<RegimeIdentification> identifyHistoricalRegimes(
        const std::string& symbol,
        const std::vector<HistoricalOHLCV>& price_data,
        std::chrono::hours lookback_window = std::chrono::hours{24*30}
    );

    // Temporal alignment utilities for multi-asset data
    struct AlignedData {
        std::chrono::system_clock::time_point timestamp;
        std::map<std::string, HistoricalOHLCV> prices;      // Price data by symbol
        std::map<std::string, HistoricalSentiment> sentiment; // Sentiment by symbol
        bool is_complete;                                    // True if all symbols have data
    };

    std::vector<AlignedData> alignMultiAssetData(
        const std::map<std::string, std::vector<HistoricalOHLCV>>& price_data,
        const std::map<std::string, std::vector<HistoricalSentiment>>& sentiment_data
    );

    // Data export and persistence
    bool exportHistoricalData(
        const std::string& symbol,
        const std::vector<HistoricalOHLCV>& data,
        const std::string& filename,
        const std::string& format = "csv"
    );

    bool importHistoricalData(
        const std::string& symbol,
        std::vector<HistoricalOHLCV>& data,
        const std::string& filename,
        const std::string& format = "csv"
    );

    // Performance and caching
    void clearCache();
    void preloadData(
        const std::vector<std::string>& symbols,
        const TimeRange& range,
        TimeFrame frequency = TimeFrame::DAY_1
    );

    // Statistics and reporting
    struct DataCoverageReport {
        std::map<std::string, int> symbol_coverage_days;    // Days of data per symbol
        std::map<std::string, DataQualityReport> quality_reports;
        double overall_completeness;                        // Overall data completeness
        int total_symbols;                                 // Number of symbols analyzed
        int symbols_meeting_standards;                     // Symbols meeting min standards

        // Coverage statistics
        std::chrono::system_clock::time_point earliest_date;
        std::chrono::system_clock::time_point latest_date;
        int minimum_coverage_days;
        int maximum_coverage_days;
        double average_coverage_days;
    };

    DataCoverageReport generateCoverageReport(
        const std::vector<std::string>& symbols,
        const TimeRange& target_range
    );

    // Utility functions
    static TimeRange getRecommendedTimeRange();                    // 2+ years back from today
    static std::vector<std::string> getRequiredSymbols();         // Standard symbol list
    static TimeFrame stringToTimeFrame(const std::string& tf);
    static std::string timeFrameToString(TimeFrame tf);

    // TRS compliance helpers
    struct TRSDataRequirements {
        int minimum_days_coverage;      // Minimum days of data required
        double minimum_quality_score;   // Minimum data quality required
        double maximum_gap_percentage;  // Maximum allowable data gaps
        std::vector<std::string> required_symbols; // Required symbols for TRS

        TRSDataRequirements() : minimum_days_coverage(730), minimum_quality_score(0.85),
                               maximum_gap_percentage(0.05) {} // 5% max gaps
    };

    bool validateTRSDataRequirements(
        const std::vector<std::string>& symbols,
        const TimeRange& range,
        const TRSDataRequirements& requirements = TRSDataRequirements{}
    );
};

/**
 * Historical Backtesting Engine - Core component for real correlation validation
 * Processes historical data to validate algorithm predictions against actual outcomes
 */
class HistoricalBacktestingEngine {
public:
    struct BacktestConfiguration {
        HistoricalDataManager::TimeRange test_period;
        std::vector<std::string> asset_pairs;
        std::chrono::hours prediction_horizon;  // How far ahead to predict
        std::chrono::hours rebalancing_frequency; // How often to make new predictions

        // Algorithm parameters
        double minimum_confidence_threshold;     // Min confidence for acting on predictions
        int maximum_positions;                  // Max number of concurrent positions

        // Transaction costs
        double transaction_cost_bps;            // Transaction cost in basis points
        double slippage_bps;                   // Slippage estimate in basis points

        BacktestConfiguration() : prediction_horizon(std::chrono::hours{24}),
                                rebalancing_frequency(std::chrono::hours{24}),
                                minimum_confidence_threshold(0.6),
                                maximum_positions(10),
                                transaction_cost_bps(5.0), slippage_bps(2.0) {}
    };

    struct BacktestResults {
        // Performance metrics
        double total_return;                    // Total return over test period
        double annualized_return;              // Annualized return
        double volatility;                     // Return volatility
        double sharpe_ratio;                   // Risk-adjusted return
        double maximum_drawdown;               // Maximum peak-to-trough decline
        double calmar_ratio;                   // Return/max drawdown

        // Algorithm-specific metrics
        double prediction_accuracy;            // Percentage of correct predictions
        double correlation_to_predictions;     // Correlation between predictions and outcomes
        double average_confidence;             // Average prediction confidence

        // Trade statistics
        int total_trades;                      // Number of trades executed
        int winning_trades;                    // Number of profitable trades
        double win_rate;                       // Percentage of winning trades
        double average_trade_return;           // Average return per trade

        // Temporal analysis
        std::vector<std::pair<std::chrono::system_clock::time_point, double>> equity_curve;
        std::vector<std::pair<std::chrono::system_clock::time_point, double>> correlation_over_time;

        BacktestResults() : total_return(0), annualized_return(0), volatility(0),
                           sharpe_ratio(0), maximum_drawdown(0), calmar_ratio(0),
                           prediction_accuracy(0), correlation_to_predictions(0),
                           average_confidence(0), total_trades(0), winning_trades(0),
                           win_rate(0), average_trade_return(0) {}
    };

private:
    std::unique_ptr<HistoricalDataManager> data_manager_;

public:
    explicit HistoricalBacktestingEngine();
    ~HistoricalBacktestingEngine() = default;

    // Main backtesting interface
    BacktestResults runBacktest(
        class CoreTradingAlgorithm& algorithm,
        const BacktestConfiguration& config
    );

    // Walk-forward validation for out-of-sample testing
    std::vector<BacktestResults> walkForwardValidation(
        class CoreTradingAlgorithm& algorithm,
        const HistoricalDataManager::TimeRange& full_period,
        std::chrono::hours training_window = std::chrono::hours{24*180},
        std::chrono::hours testing_window = std::chrono::hours{24*30}
    );

    // Performance attribution analysis
    struct PerformanceAttribution {
        std::map<std::string, double> pair_contributions;    // Return contribution by pair
        std::map<std::string, double> feature_contributions; // Return contribution by feature
        std::map<std::string, double> regime_contributions;  // Return by market regime

        double alpha;                           // Risk-adjusted excess return
        double beta;                           // Market beta
        double information_ratio;              // Active return / tracking error
    };

    PerformanceAttribution analyzePerformanceAttribution(
        const BacktestResults& results,
        const std::vector<std::string>& benchmark_symbols = {"BTC", "ETH"}
    );
};

} // namespace Algorithm
} // namespace CryptoClaude