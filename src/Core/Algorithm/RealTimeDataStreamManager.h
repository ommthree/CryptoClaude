#pragma once

#include "../Http/HttpClient.h"
#include "../Database/DatabaseManager.h"
#include "HistoricalDataManager.h"
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <chrono>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>

namespace CryptoClaude {
namespace Algorithm {

/**
 * Real-Time Data Stream Manager - Day 22 Implementation
 * Manages live market data streaming from multiple exchanges
 * Provides real-time data ingestion with quality validation and failover
 */
class RealTimeDataStreamManager {
public:
    // Real-time market tick data structure
    struct RealTimeTick {
        std::string symbol;
        std::chrono::system_clock::time_point timestamp;

        // Price data
        double bid_price;
        double ask_price;
        double last_price;
        double mid_price;                    // (bid + ask) / 2

        // Volume data
        double bid_size;
        double ask_size;
        double last_size;
        double volume_24h;

        // Market structure
        double spread;                       // ask - bid
        double spread_bps;                   // spread in basis points

        // Data quality
        std::string exchange;
        double quality_score;                // 0.0 to 1.0
        std::chrono::milliseconds latency;   // Data latency
        bool is_stale;                      // Data freshness flag

        // Market context
        double price_change_24h;
        double price_change_percentage;
        double market_cap;

        RealTimeTick() : bid_price(0), ask_price(0), last_price(0), mid_price(0),
                        bid_size(0), ask_size(0), last_size(0), volume_24h(0),
                        spread(0), spread_bps(0), quality_score(1.0),
                        latency(std::chrono::milliseconds{0}), is_stale(false),
                        price_change_24h(0), price_change_percentage(0), market_cap(0) {}
    };

    // Stream configuration for each exchange
    struct StreamConfig {
        std::string exchange_name;
        std::string websocket_url;
        std::string api_key;
        std::string api_secret;

        // Connection parameters
        int reconnect_attempts = 5;
        std::chrono::seconds reconnect_delay{10};
        std::chrono::seconds ping_interval{30};
        std::chrono::seconds timeout{60};

        // Data parameters
        std::vector<std::string> symbols;
        std::vector<std::string> channels;   // ticker, depth, trades
        int buffer_size = 1000;

        // Quality thresholds
        std::chrono::milliseconds max_latency{500};
        double min_quality_score = 0.8;

        bool is_primary = false;
        double reliability_weight = 1.0;

        StreamConfig() = default;
        StreamConfig(const std::string& name, const std::string& url)
            : exchange_name(name), websocket_url(url) {}
    };

    // Stream status and health monitoring
    struct StreamHealth {
        std::string exchange_name;
        std::chrono::system_clock::time_point last_update;

        // Connection status
        bool is_connected = false;
        bool is_healthy = true;
        int reconnection_count = 0;
        std::chrono::system_clock::time_point last_reconnect;

        // Performance metrics
        std::chrono::milliseconds average_latency{0};
        double message_rate_per_second = 0.0;
        int messages_received_total = 0;
        int messages_processed = 0;
        int messages_dropped = 0;

        // Quality metrics
        double data_quality_score = 1.0;
        int quality_violations = 0;
        std::chrono::system_clock::time_point last_quality_issue;

        // Error tracking
        std::vector<std::string> recent_errors;
        int error_count_24h = 0;

        StreamHealth() = default;
        StreamHealth(const std::string& name) : exchange_name(name) {}
    };

    // Market data aggregation result
    struct AggregatedMarketData {
        std::string symbol;
        std::chrono::system_clock::time_point timestamp;

        // Best bid/offer across exchanges
        double best_bid;
        double best_ask;
        double consolidated_price;           // Volume-weighted or median
        std::string best_bid_exchange;
        std::string best_ask_exchange;

        // Volume aggregation
        double total_volume_24h;
        std::map<std::string, double> volume_by_exchange;

        // Market depth
        double total_bid_depth;
        double total_ask_depth;

        // Quality assessment
        int participating_exchanges;
        double consensus_quality;            // Agreement between exchanges
        double data_freshness_score;        // How recent is the data

        // Price discovery metrics
        double price_spread_across_exchanges;
        double weighted_average_spread;
        bool is_arbitrage_opportunity;

        AggregatedMarketData() : best_bid(0), best_ask(0), consolidated_price(0),
                               total_volume_24h(0), total_bid_depth(0), total_ask_depth(0),
                               participating_exchanges(0), consensus_quality(0), data_freshness_score(0),
                               price_spread_across_exchanges(0), weighted_average_spread(0),
                               is_arbitrage_opportunity(false) {}
    };

private:
    // Configuration and state
    std::vector<StreamConfig> stream_configs_;
    std::map<std::string, StreamHealth> stream_health_;
    std::unique_ptr<Database::DatabaseManager> db_manager_;

    // Threading and synchronization
    std::vector<std::thread> stream_threads_;
    std::atomic<bool> is_running_{false};
    std::mutex data_mutex_;
    std::mutex health_mutex_;

    // Data buffers and processing
    std::map<std::string, std::queue<RealTimeTick>> tick_buffers_;
    std::map<std::string, RealTimeTick> latest_ticks_;

    // Market data aggregation
    std::map<std::string, AggregatedMarketData> aggregated_data_;
    std::chrono::system_clock::time_point last_aggregation_;

    // Callbacks and event handlers
    std::function<void(const RealTimeTick&)> tick_callback_;
    std::function<void(const AggregatedMarketData&)> aggregated_callback_;
    std::function<void(const std::string&, const std::string&)> error_callback_;

    // Connection management
    void startExchangeStream(const StreamConfig& config);
    void maintainConnection(const StreamConfig& config);
    bool reconnectStream(const StreamConfig& config);
    void stopAllStreams();

    // Data processing
    void processIncomingTick(const RealTimeTick& tick);
    void aggregateMarketData();
    void validateDataQuality(RealTimeTick& tick);

    // Health monitoring
    void updateStreamHealth(const std::string& exchange, const StreamHealth& health);
    void monitorStreamHealth();
    bool isStreamHealthy(const std::string& exchange) const;

    // Failover management
    void handleStreamFailure(const std::string& exchange);
    void activateBackupStream(const std::string& symbol);
    std::string selectPrimaryExchange(const std::string& symbol) const;

public:
    RealTimeDataStreamManager();
    ~RealTimeDataStreamManager();

    // Configuration management
    void addStreamConfig(const StreamConfig& config);
    void updateStreamConfig(const std::string& exchange, const StreamConfig& config);
    void removeStreamConfig(const std::string& exchange);

    // Stream control
    bool startAllStreams();
    void pauseStream(const std::string& exchange);
    void resumeStream(const std::string& exchange);

    // Data access
    RealTimeTick getLatestTick(const std::string& symbol) const;
    std::vector<RealTimeTick> getRecentTicks(const std::string& symbol, int count = 10) const;
    AggregatedMarketData getAggregatedData(const std::string& symbol) const;
    std::vector<std::string> getAvailableSymbols() const;

    // Market data queries
    double getCurrentPrice(const std::string& symbol) const;
    double getBestBid(const std::string& symbol) const;
    double getBestAsk(const std::string& symbol) const;
    double getSpread(const std::string& symbol) const;
    double getVolume24h(const std::string& symbol) const;

    // Health and monitoring
    StreamHealth getStreamHealth(const std::string& exchange) const;
    std::map<std::string, StreamHealth> getAllStreamHealth() const;
    bool isSystemHealthy() const;
    double getOverallQualityScore() const;

    // Performance metrics
    struct PerformanceMetrics {
        std::chrono::milliseconds average_latency;
        double total_message_rate;
        int active_connections;
        int healthy_connections;
        double system_uptime_percentage;
        std::chrono::system_clock::time_point last_restart;

        // Data quality
        double average_quality_score;
        int quality_violations_24h;
        double data_completeness_percentage;

        PerformanceMetrics() : average_latency(std::chrono::milliseconds{0}),
                             total_message_rate(0), active_connections(0), healthy_connections(0),
                             system_uptime_percentage(0), average_quality_score(0),
                             quality_violations_24h(0), data_completeness_percentage(0) {}
    };

    PerformanceMetrics getPerformanceMetrics() const;

    // Callback registration
    void setTickCallback(std::function<void(const RealTimeTick&)> callback);
    void setAggregatedDataCallback(std::function<void(const AggregatedMarketData&)> callback);
    void setErrorCallback(std::function<void(const std::string&, const std::string&)> callback);

    // Data persistence
    bool enableDataPersistence(bool enable = true);
    bool saveTicksToDatabase(const std::vector<RealTimeTick>& ticks);
    bool saveAggregatedDataToDatabase(const AggregatedMarketData& data);

    // Market regime detection from real-time data
    enum class MarketRegime {
        NORMAL,      // Normal trading conditions
        VOLATILE,    // High volatility period
        TRENDING,    // Strong directional movement
        RANGING,     // Sideways price action
        ILLIQUID,    // Low liquidity conditions
        DISRUPTED    // Market disruption or anomaly
    };

    struct MarketRegimeInfo {
        MarketRegime regime;
        double confidence;                   // 0.0 to 1.0
        std::string description;
        std::chrono::system_clock::time_point detected_at;
        std::vector<std::string> indicators; // What triggered the detection

        MarketRegimeInfo() : regime(MarketRegime::NORMAL), confidence(0.0),
                           detected_at(std::chrono::system_clock::now()) {}
    };

    MarketRegimeInfo detectCurrentMarketRegime(const std::string& symbol) const;
    std::map<std::string, MarketRegimeInfo> detectAllMarketRegimes() const;

    // Advanced features
    bool enableArbitrageDetection(bool enable = true);
    std::vector<std::pair<std::string, std::string>> detectArbitrageOpportunities() const;

    // Integration with historical data
    bool syncWithHistoricalData(HistoricalDataManager& historical_manager);
    bool validateAgainstHistoricalData(const std::string& symbol) const;

    // Emergency controls
    void emergencyStop();
    void emergencyRestart();
    bool isInEmergencyMode() const;

    // Static utility methods
    static StreamConfig createBinanceConfig(const std::vector<std::string>& symbols);
    static StreamConfig createCoinbaseConfig(const std::vector<std::string>& symbols);
    static StreamConfig createKrakenConfig(const std::vector<std::string>& symbols);
    static double calculateSpreadBps(double bid, double ask, double mid_price);
    static bool isDataStale(const std::chrono::system_clock::time_point& timestamp,
                          std::chrono::milliseconds threshold = std::chrono::milliseconds{1000});
};

} // namespace Algorithm
} // namespace CryptoClaude