#pragma once

#include "../Database/DatabaseManager.h"
#include "../Database/Models/MarketData.h"
#include "../DataPipeline/EnhancedMarketDataPipeline.h"
#include "RealMarketSignalGenerator.h"
#include <memory>
#include <vector>
#include <map>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace CryptoClaude {
namespace Algorithm {

/**
 * Real-Time Market Data Processor - Day 20 Implementation
 * Provides real market data integration for algorithm signal generation
 * Critical component for eliminating placeholder/synthetic data usage
 */
class RealTimeMarketDataProcessor {
public:
    // Real-time data update configuration
    struct ProcessorConfig {
        // Data refresh intervals
        std::chrono::seconds real_time_update_interval{30};    // 30-second real-time updates
        std::chrono::minutes technical_update_interval{5};      // 5-minute technical analysis updates
        std::chrono::hours daily_update_interval{1};           // Hourly data consistency checks

        // Data requirements
        int min_historical_periods = 200;                      // Minimum history for technical analysis
        int max_cache_age_minutes = 10;                        // Maximum cache age for real-time data
        double data_quality_threshold = 0.95;                  // Minimum data quality for signal generation

        // Processing parameters
        int max_concurrent_symbols = 50;                       // Maximum symbols to process simultaneously
        int batch_size = 10;                                   // Batch size for database operations
        bool enable_parallel_processing = true;                // Enable multi-threaded processing

        // Real-time features
        bool enable_streaming_data = false;                     // Enable WebSocket streaming (future)
        bool enable_cache_warming = true;                       // Pre-load frequently used data
        bool enable_quality_monitoring = true;                 // Continuous data quality monitoring

        ProcessorConfig() {}
    };

    // Market data snapshot for algorithm consumption
    struct AlgorithmMarketData {
        std::string symbol;
        std::chrono::system_clock::time_point timestamp;

        // Current price data
        double current_price;
        double price_change_24h;
        double price_change_percentage_24h;
        double volume_24h;
        double volume_change_24h;

        // Technical analysis data
        struct TechnicalData {
            double rsi_14;
            double rsi_30;

            struct MACD {
                double macd_line;
                double signal_line;
                double histogram;
            } macd;

            struct BollingerBands {
                double upper;
                double middle;
                double lower;
                double percentage_b;  // Position within bands
            } bollinger;

            struct MovingAverages {
                double sma_20;
                double sma_50;
                double sma_200;
                double ema_12;
                double ema_26;
            } moving_averages;

            // Momentum indicators
            double stochastic_k;
            double stochastic_d;
            double williams_r;
        } technical;

        // Volume analysis
        struct VolumeData {
            double volume_weighted_average_price;
            double on_balance_volume;
            double volume_profile_high;
            double volume_profile_low;
            double volume_ratio;  // Current vs average
        } volume;

        // Market structure
        struct MarketStructure {
            double bid_ask_spread;
            double market_cap;
            double circulating_supply;
            double orderbook_depth;
        } market_structure;

        // Quality metrics
        struct DataQuality {
            double completeness_score;      // 0-1, percentage of non-null data
            double freshness_score;         // 0-1, how recent the data is
            double consistency_score;       // 0-1, consistency with historical patterns
            double overall_quality;         // 0-1, composite quality score
            std::vector<std::string> quality_issues;
        } quality;

        AlgorithmMarketData() : current_price(0), price_change_24h(0), price_change_percentage_24h(0),
                               volume_24h(0), volume_change_24h(0) {
            // Initialize technical data with neutral values
            technical.rsi_14 = 50.0;
            technical.rsi_30 = 50.0;
            technical.macd = {0, 0, 0};
            technical.bollinger = {0, 0, 0, 0.5};
            technical.moving_averages = {0, 0, 0, 0, 0};
            technical.stochastic_k = 50.0;
            technical.stochastic_d = 50.0;
            technical.williams_r = -50.0;

            // Initialize quality scores
            quality.completeness_score = 0.0;
            quality.freshness_score = 0.0;
            quality.consistency_score = 0.0;
            quality.overall_quality = 0.0;
        }
    };

    // Processing status and monitoring
    struct ProcessingStatus {
        bool is_running;
        std::chrono::system_clock::time_point last_update;
        int symbols_processed;
        int symbols_pending;
        int symbols_with_errors;
        double average_processing_time_ms;
        std::map<std::string, std::string> symbol_status;   // symbol -> status message
        std::vector<std::string> recent_errors;

        double overall_data_quality;
        int cache_hit_rate_percentage;
        std::chrono::seconds time_since_last_update;
    };

private:
    ProcessorConfig config_;
    std::unique_ptr<Database::DatabaseManager> db_manager_;
    std::unique_ptr<DataPipeline::EnhancedMarketDataPipeline> data_pipeline_;

    // Real-time processing state
    std::atomic<bool> processing_active_;
    std::thread processing_thread_;
    std::mutex data_mutex_;
    std::condition_variable data_condition_;

    // Data cache for algorithm consumption
    std::map<std::string, AlgorithmMarketData> market_data_cache_;
    std::map<std::string, std::chrono::system_clock::time_point> cache_timestamps_;

    // Historical data cache
    std::map<std::string, std::vector<Database::Models::MarketData>> historical_cache_;

    // Processing metrics
    ProcessingStatus current_status_;
    std::mutex status_mutex_;

    // Data processing methods
    AlgorithmMarketData processRawMarketData(const std::string& symbol);
    bool updateTechnicalAnalysis(AlgorithmMarketData& data, const std::vector<Database::Models::MarketData>& history);
    bool calculateVolumeMetrics(AlgorithmMarketData& data, const std::vector<Database::Models::MarketData>& history);
    bool assessDataQuality(AlgorithmMarketData& data);

    // Technical analysis calculations
    double calculateRSI(const std::vector<double>& prices, int period);
    AlgorithmMarketData::TechnicalData::MACD calculateMACD(const std::vector<double>& prices, int fast, int slow, int signal);
    AlgorithmMarketData::TechnicalData::BollingerBands calculateBollingerBands(const std::vector<double>& prices, int period, double std_dev);
    AlgorithmMarketData::TechnicalData::MovingAverages calculateMovingAverages(const std::vector<double>& prices);

    // Volume analysis calculations
    double calculateVWAP(const std::vector<Database::Models::MarketData>& data);
    double calculateOnBalanceVolume(const std::vector<Database::Models::MarketData>& data);

    // Data management
    std::vector<Database::Models::MarketData> loadHistoricalData(const std::string& symbol, int periods);
    bool updateDataCache(const std::string& symbol);
    bool isCacheValid(const std::string& symbol) const;
    void cleanupExpiredCache();

    // Processing thread management
    void processingLoop();
    void updateProcessingStatus();

public:
    explicit RealTimeMarketDataProcessor(const ProcessorConfig& config = ProcessorConfig());
    ~RealTimeMarketDataProcessor();

    // Lifecycle management
    bool initialize();
    bool start();
    void stop();
    bool isRunning() const;

    // Algorithm data interface - primary interface for signal generation
    AlgorithmMarketData getAlgorithmMarketData(const std::string& symbol);
    std::vector<AlgorithmMarketData> getAlgorithmMarketData(const std::vector<std::string>& symbols);

    // Pair data interface
    struct PairMarketData {
        AlgorithmMarketData base;
        AlgorithmMarketData quote;

        // Pair-specific metrics
        double correlation_30d;
        double correlation_90d;
        double relative_strength;
        double spread_current;
        double spread_average;
        double spread_zscore;

        // Combined quality score
        double pair_quality_score;
    };

    PairMarketData getPairMarketData(const std::string& pair);  // e.g., "BTC/ETH"

    // Historical data interface
    std::vector<Database::Models::MarketData> getHistoricalData(const std::string& symbol, int periods);
    std::vector<AlgorithmMarketData> getHistoricalAlgorithmData(const std::string& symbol, int periods);

    // Data quality and monitoring
    ProcessingStatus getProcessingStatus() const;
    std::map<std::string, double> getDataQualityScores() const;
    std::vector<std::string> getSymbolsWithLowQuality(double threshold = 0.8) const;

    // Configuration management
    void updateConfiguration(const ProcessorConfig& new_config);
    const ProcessorConfig& getConfiguration() const { return config_; }

    // Cache management
    void warmupCache(const std::vector<std::string>& symbols);
    void clearCache();
    void clearSymbolCache(const std::string& symbol);
    double getCacheHitRate() const;

    // Integration with signal generator
    bool validateDataForSignalGeneration(const std::string& symbol) const;
    std::vector<std::string> getReadySymbols() const;  // Symbols ready for signal generation

    // Database integration
    bool saveProcessedData(const AlgorithmMarketData& data);
    bool updateDataPipeline();

    // Performance monitoring
    struct PerformanceMetrics {
        std::chrono::milliseconds average_processing_time;
        int data_points_processed_per_second;
        double memory_usage_mb;
        int active_threads;
        std::chrono::system_clock::time_point last_performance_check;
    };

    PerformanceMetrics getPerformanceMetrics() const;

    // Real-time data validation
    struct ValidationResult {
        bool is_valid;
        double quality_score;
        std::vector<std::string> issues;
        std::chrono::system_clock::time_point validated_at;
    };

    ValidationResult validateRealTimeData(const std::string& symbol) const;

    // Market regime detection support
    enum class MarketRegime {
        BULL_MARKET,
        BEAR_MARKET,
        SIDEWAYS_MARKET,
        HIGH_VOLATILITY,
        LOW_VOLATILITY,
        CRISIS_MODE,
        UNKNOWN
    };

    MarketRegime detectMarketRegime(const std::string& symbol) const;
    std::map<std::string, MarketRegime> detectMarketRegimes(const std::vector<std::string>& symbols) const;

    // Data export for backtesting
    bool exportDataForBacktesting(const std::string& symbol,
                                 const std::chrono::system_clock::time_point& start_date,
                                 const std::chrono::system_clock::time_point& end_date,
                                 const std::string& output_path);
};

/**
 * Market Data Quality Validator - Ensures data meets algorithm requirements
 * Critical component for preventing algorithm failures due to poor data quality
 */
class MarketDataQualityValidator {
public:
    // Quality assessment criteria
    struct QualityStandards {
        double min_completeness = 0.95;        // 95% data completeness required
        double max_staleness_minutes = 5.0;    // Data must be less than 5 minutes old
        double min_price_sanity = 0.01;        // Minimum price for sanity check
        double max_price_change = 0.5;         // Maximum 50% price change per period
        double min_volume_threshold = 1000.0;  // Minimum daily volume

        // Technical indicator validity
        bool require_valid_rsi = true;
        bool require_valid_macd = true;
        bool require_valid_bollinger = true;

        // Consistency checks
        bool enable_cross_validation = true;    // Cross-validate with multiple sources
        bool enable_anomaly_detection = true;   // Detect statistical anomalies
        double anomaly_z_score_threshold = 3.0; // Z-score threshold for anomalies

        QualityStandards() {}
    };

    static RealTimeMarketDataProcessor::AlgorithmMarketData::DataQuality
    assessDataQuality(const RealTimeMarketDataProcessor::AlgorithmMarketData& data,
                     const QualityStandards& standards = QualityStandards());

    static bool isDataSuitableForAlgorithm(const RealTimeMarketDataProcessor::AlgorithmMarketData& data,
                                          const QualityStandards& standards = QualityStandards());

    static std::vector<std::string> identifyQualityIssues(const RealTimeMarketDataProcessor::AlgorithmMarketData& data,
                                                         const QualityStandards& standards = QualityStandards());

    // Cross-validation with external sources
    static bool crossValidatePrice(const std::string& symbol, double price, double tolerance = 0.02);
    static bool detectAnomalousData(const std::vector<double>& values, double z_threshold = 3.0);
};

} // namespace Algorithm
} // namespace CryptoClaude