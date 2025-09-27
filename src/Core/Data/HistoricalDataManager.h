#pragma once

#include "../Http/HttpClient.h"
#include "../Database/Models/MarketData.h"
#include "../Database/DatabaseManager.h"
#include "Providers/CryptoCompareProvider.h"
#include <vector>
#include <string>
#include <memory>
#include <map>
#include <chrono>
#include <atomic>
#include <mutex>
#include <future>

namespace CryptoClaude {
namespace Data {

/**
 * Historical Data Manager - Day 28 Production Calibration
 * Manages comprehensive historical data loading for VaR model recalibration
 * Target: Load 2+ years of data for 50+ cryptocurrencies with 98%+ completeness
 */
class HistoricalDataManager {
public:
    // Historical data loading configuration
    struct HistoricalDataConfig {
        // Data coverage requirements
        int historical_days = 730;                    // 2+ years of historical data
        double min_completeness_tier1 = 0.98;         // 98%+ completeness for top 20 coins
        double min_completeness_extended = 0.90;      // 90%+ completeness for extended universe

        // Loading performance parameters
        int max_concurrent_requests = 5;              // Concurrent API requests
        int rate_limit_requests_per_minute = 50;      // CryptoCompare rate limits
        int request_retry_attempts = 3;               // Retry failed requests
        std::chrono::milliseconds request_delay{2000}; // Delay between requests

        // Data quality parameters
        double max_price_change_per_day = 10.0;       // 10x max daily price change (outlier detection)
        double min_volume_threshold = 1000.0;         // Minimum daily volume threshold
        bool enable_data_validation = true;           // Enable comprehensive validation
        bool enable_gap_filling = true;               // Enable data gap interpolation

        HistoricalDataConfig() = default;
    };

    // Data quality metrics
    struct DataQualityMetrics {
        std::string symbol;
        int total_days_requested;
        int days_loaded_successfully;
        int days_with_gaps;
        int outliers_detected;
        double completeness_percentage;
        double quality_score;                          // Overall quality score 0-100

        std::vector<std::string> quality_issues;      // List of quality issues found
        std::chrono::system_clock::time_point last_updated;

        DataQualityMetrics() : total_days_requested(0), days_loaded_successfully(0),
                              days_with_gaps(0), outliers_detected(0),
                              completeness_percentage(0.0), quality_score(0.0) {}
    };

    // Historical data loading result
    struct HistoricalLoadResult {
        bool success;
        std::string error_message;

        // Loading statistics
        int total_symbols_requested;
        int symbols_loaded_successfully;
        int total_data_points_loaded;
        std::chrono::milliseconds total_loading_time;

        // Quality metrics by symbol
        std::map<std::string, DataQualityMetrics> symbol_metrics;

        // Overall metrics
        double overall_completeness;
        double overall_quality_score;
        int total_api_calls_made;

        HistoricalLoadResult() : success(false), total_symbols_requested(0),
                                symbols_loaded_successfully(0), total_data_points_loaded(0),
                                total_loading_time(0), overall_completeness(0.0),
                                overall_quality_score(0.0), total_api_calls_made(0) {}
    };

    // Cryptocurrency universe definition
    struct CryptocurrencyUniverse {
        // Tier 1: Top 20 cryptocurrencies (98%+ completeness required)
        std::vector<std::string> tier1_symbols = {
            "BTC", "ETH", "BNB", "XRP", "ADA", "SOL", "DOGE", "DOT", "MATIC", "SHIB",
            "AVAX", "UNI", "LINK", "ATOM", "NEAR", "APE", "MANA", "SAND", "CRV", "AAVE"
        };

        // Extended Universe: Additional 30 cryptocurrencies (90%+ completeness required)
        std::vector<std::string> extended_symbols = {
            "LTC", "BCH", "ETC", "XLM", "ALGO", "VET", "FIL", "THETA", "ICP", "FLOW",
            "EGLD", "XTZ", "MINA", "ROSE", "FTM", "ONE", "HBAR", "WAVES", "ZIL", "ENJ",
            "BAT", "COMP", "MKR", "SNX", "YFI", "SUSHI", "1INCH", "REN", "KNC", "LRC"
        };

        std::vector<std::string> getAllSymbols() const {
            std::vector<std::string> all_symbols = tier1_symbols;
            all_symbols.insert(all_symbols.end(), extended_symbols.begin(), extended_symbols.end());
            return all_symbols;
        }

        bool isTier1Symbol(const std::string& symbol) const {
            return std::find(tier1_symbols.begin(), tier1_symbols.end(), symbol) != tier1_symbols.end();
        }
    };

public:
    explicit HistoricalDataManager(
        std::shared_ptr<Database::DatabaseManager> db_manager,
        std::shared_ptr<Providers::ICryptoCompareProvider> crypto_provider,
        const HistoricalDataConfig& config = HistoricalDataConfig()
    );

    // === CORE HISTORICAL DATA LOADING ===

    // Load comprehensive historical data for all cryptocurrencies
    HistoricalLoadResult loadComprehensiveHistoricalData();

    // Load historical data for specific symbol list
    HistoricalLoadResult loadHistoricalDataForSymbols(const std::vector<std::string>& symbols);

    // Load single symbol historical data with comprehensive validation
    DataQualityMetrics loadSingleSymbolHistoricalData(const std::string& symbol);

    // === DATA QUALITY AND VALIDATION ===

    // Validate loaded historical data quality
    DataQualityMetrics validateHistoricalDataQuality(const std::string& symbol);

    // Generate comprehensive data quality report
    std::map<std::string, DataQualityMetrics> generateDataQualityReport();

    // Fix data gaps using interpolation and external sources
    bool fixDataGaps(const std::string& symbol);

    // === CORRELATION DATA POPULATION ===

    // Calculate and populate pairwise correlations
    struct CorrelationMatrix {
        std::map<std::pair<std::string, std::string>, double> correlations;
        std::map<std::pair<std::string, std::string>, std::vector<double>> rolling_correlations_30d;
        std::map<std::pair<std::string, std::string>, std::vector<double>> rolling_correlations_90d;
        std::map<std::pair<std::string, std::string>, std::vector<double>> rolling_correlations_180d;

        // Traditional asset correlations
        std::map<std::string, double> sp500_correlations;     // S&P 500 correlations
        std::map<std::string, double> gold_correlations;      // Gold correlations
        std::map<std::string, double> dollar_correlations;    // USD Index correlations

        std::chrono::system_clock::time_point calculation_time;
        int total_pairs_calculated;
        double calculation_time_ms;
    };

    CorrelationMatrix calculateComprehensiveCorrelations();
    bool populateCorrelationDatabase(const CorrelationMatrix& correlations);

    // === DATABASE OPTIMIZATION ===

    // Optimize database schema for historical data
    bool optimizeDatabaseForHistoricalData();

    // Create indexes for time-series queries
    bool createTimeSeriesIndexes();

    // Implement data compression for large datasets
    bool implementDataCompression();

    // === PERFORMANCE MONITORING ===

    struct LoadingPerformanceMetrics {
        std::chrono::milliseconds avg_request_time;
        std::chrono::milliseconds total_loading_time;
        int requests_per_second;
        int concurrent_requests_peak;
        double memory_usage_mb;
        double database_size_mb;

        std::map<std::string, std::chrono::milliseconds> symbol_loading_times;
    };

    LoadingPerformanceMetrics getLoadingPerformanceMetrics() const;

    // === CONFIGURATION AND UTILITIES ===

    void updateConfig(const HistoricalDataConfig& config);
    const HistoricalDataConfig& getConfig() const { return config_; }

    // Get cryptocurrency universe definition
    const CryptocurrencyUniverse& getUniverseDefinition() const { return universe_; }

    // Progress callback for loading operations
    using ProgressCallback = std::function<void(const std::string& symbol, int progress_percent, const std::string& status)>;
    void setProgressCallback(ProgressCallback callback) { progress_callback_ = callback; }

private:
    // Configuration and dependencies
    HistoricalDataConfig config_;
    std::shared_ptr<Database::DatabaseManager> db_manager_;
    std::shared_ptr<Providers::ICryptoCompareProvider> crypto_provider_;
    CryptocurrencyUniverse universe_;

    // Progress tracking
    ProgressCallback progress_callback_;
    std::atomic<int> completed_symbols_{0};
    std::atomic<int> total_symbols_{0};

    // Performance tracking
    mutable std::mutex metrics_mutex_;
    LoadingPerformanceMetrics performance_metrics_;
    std::chrono::system_clock::time_point loading_start_time_;

    // Concurrency control
    std::mutex rate_limit_mutex_;
    std::chrono::system_clock::time_point last_request_time_;
    std::atomic<int> concurrent_requests_{0};

    // Helper methods
    bool isWithinRateLimit();
    void recordAPICall();
    std::vector<Database::Models::MarketData> requestHistoricalData(const std::string& symbol, int days);
    bool validateDataPoint(const Database::Models::MarketData& data_point);
    double calculateQualityScore(const DataQualityMetrics& metrics);
    bool interpolateDataGaps(std::vector<Database::Models::MarketData>& data);

    // Database helpers
    bool storeHistoricalData(const std::string& symbol, const std::vector<Database::Models::MarketData>& data);
    std::vector<Database::Models::MarketData> loadStoredHistoricalData(const std::string& symbol);

    // Correlation calculation helpers
    double calculatePearsonCorrelation(const std::vector<double>& returns1, const std::vector<double>& returns2);
    std::vector<double> calculateRollingCorrelation(const std::vector<double>& returns1,
                                                    const std::vector<double>& returns2,
                                                    int window_size);
};

}} // namespace CryptoClaude::Data