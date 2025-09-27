#pragma once

#include "../Http/HttpClient.h"
#include "../Algorithm/RealTimeDataStreamManager.h"
#include <memory>
#include <vector>
#include <map>
#include <string>
#include <chrono>
#include <atomic>
#include <mutex>
#include <thread>
#include <queue>
#include <functional>

namespace CryptoClaude {
namespace Market {

/**
 * Live Market Data Provider - Day 23 Implementation
 * Real exchange API integration replacing simulated data feeds
 * Provides actual market data from Binance, Coinbase, Kraken with failover
 */
class LiveMarketDataProvider {
public:
    // Exchange-specific configuration
    struct ExchangeCredentials {
        std::string exchange_name;
        std::string api_key;
        std::string api_secret;
        std::string passphrase;              // For Coinbase Pro
        std::string base_url;
        std::string websocket_url;

        // Rate limiting
        int max_requests_per_second = 10;
        int max_websocket_connections = 5;

        // Security settings
        bool use_testnet = true;             // Start with testnet
        bool require_ssl = true;
        std::chrono::seconds timeout{30};

        ExchangeCredentials() = default;
    };

    // Live market tick from real exchange
    struct LiveMarketTick {
        std::string symbol;
        std::string exchange;
        std::chrono::system_clock::time_point server_timestamp;
        std::chrono::system_clock::time_point local_timestamp;

        // Price data
        double bid_price;
        double ask_price;
        double last_price;
        double mid_price;
        double spread_bps;

        // Volume and liquidity
        double bid_volume;
        double ask_volume;
        double last_volume;
        double daily_volume;

        // Market microstructure
        std::string trade_side;              // "buy", "sell", "unknown"
        int trade_count_1m;                  // Trades in last minute
        double vwap_1h;                      // 1-hour VWAP

        // Data quality metrics
        std::chrono::milliseconds latency;
        double data_quality_score;           // 0.0 to 1.0
        bool is_stale;                       // Data older than threshold
        std::string quality_flags;           // Quality issue indicators

        LiveMarketTick() : bid_price(0), ask_price(0), last_price(0), mid_price(0),
                          spread_bps(0), bid_volume(0), ask_volume(0), last_volume(0),
                          daily_volume(0), trade_count_1m(0), vwap_1h(0),
                          data_quality_score(0), is_stale(true) {}
    };

    // Exchange connection status
    struct ExchangeConnectionStatus {
        std::string exchange_name;
        bool is_connected;
        std::chrono::system_clock::time_point last_heartbeat;

        // Connection quality
        std::chrono::milliseconds avg_latency;
        double uptime_percentage_24h;
        int reconnection_count;

        // Data flow metrics
        int ticks_received_1m;
        int messages_sent_1m;
        double message_loss_rate;

        // Error tracking
        std::string last_error_message;
        std::chrono::system_clock::time_point last_error_time;
        int error_count_1h;

        ExchangeConnectionStatus() : is_connected(false), avg_latency(std::chrono::milliseconds{0}),
                                   uptime_percentage_24h(0), reconnection_count(0),
                                   ticks_received_1m(0), messages_sent_1m(0),
                                   message_loss_rate(0), error_count_1h(0) {}
    };

    // Market data subscription
    struct MarketDataSubscription {
        std::string symbol;
        std::vector<std::string> exchanges;  // Priority order

        // Data types
        bool subscribe_trades = true;
        bool subscribe_orderbook = true;
        bool subscribe_ticker = true;
        bool subscribe_klines = false;

        // Quality requirements
        std::chrono::milliseconds max_latency{100};
        double min_data_quality = 0.8;
        bool require_all_exchanges = false;

        // Callbacks
        std::function<void(const LiveMarketTick&)> tick_callback;
        std::function<void(const std::string&)> error_callback;
    };

    // Aggregated market view across exchanges
    struct AggregatedMarketView {
        std::string symbol;
        std::chrono::system_clock::time_point timestamp;

        // Best bid/ask across exchanges
        double best_bid;
        double best_ask;
        double weighted_mid;
        std::string best_bid_exchange;
        std::string best_ask_exchange;

        // Liquidity aggregation
        double total_bid_volume;
        double total_ask_volume;
        std::map<std::string, double> exchange_volumes;

        // Cross-exchange metrics
        double price_dispersion;             // Price variance across exchanges
        double liquidity_concentration;      // Liquidity distribution
        std::vector<std::string> arbitrage_opportunities;

        // Composite quality score
        double composite_quality_score;
        int active_exchange_count;
        std::chrono::milliseconds max_latency_observed;

        AggregatedMarketView() : best_bid(0), best_ask(0), weighted_mid(0),
                               total_bid_volume(0), total_ask_volume(0),
                               price_dispersion(0), liquidity_concentration(0),
                               composite_quality_score(0), active_exchange_count(0),
                               max_latency_observed(std::chrono::milliseconds{0}) {}
    };

private:
    // Configuration and credentials
    std::map<std::string, ExchangeCredentials> exchange_credentials_;
    std::vector<MarketDataSubscription> active_subscriptions_;

    // HTTP clients for REST API calls
    std::map<std::string, std::unique_ptr<Http::HttpClient>> http_clients_;

    // Connection management
    std::atomic<bool> is_running_{false};
    mutable std::mutex connection_mutex_;
    mutable std::mutex data_mutex_;

    // Threading for each exchange
    std::map<std::string, std::thread> exchange_threads_;
    std::thread data_aggregation_thread_;
    std::thread quality_monitoring_thread_;

    // Data storage
    std::map<std::string, ExchangeConnectionStatus> connection_status_;
    std::queue<LiveMarketTick> tick_buffer_;
    std::map<std::string, AggregatedMarketView> aggregated_views_;

    // Performance tracking
    std::chrono::system_clock::time_point start_time_;
    std::map<std::string, std::chrono::milliseconds> exchange_latencies_;

    // Exchange-specific connection handlers
    void connectToBinance();
    void connectToCoinbase();
    void connectToKraken();

    // WebSocket management
    void handleWebSocketMessage(const std::string& exchange, const std::string& message);
    void processTickData(const std::string& exchange, const std::string& raw_data);
    bool reconnectExchange(const std::string& exchange);

    // Data processing and aggregation
    void runDataAggregation();
    void aggregateMarketData();
    void calculateQualityMetrics();
    void detectArbitrageOpportunities();

    // Quality monitoring
    void runQualityMonitoring();
    void validateDataLatency();
    void checkConnectionHealth();
    void handleDataQualityIssues();

public:
    explicit LiveMarketDataProvider();
    ~LiveMarketDataProvider();

    // Configuration management
    bool addExchangeCredentials(const std::string& exchange, const ExchangeCredentials& credentials);
    bool updateExchangeCredentials(const std::string& exchange, const ExchangeCredentials& credentials);
    std::vector<std::string> getConfiguredExchanges() const;

    // Subscription management
    bool subscribe(const MarketDataSubscription& subscription);
    bool unsubscribe(const std::string& symbol);
    std::vector<MarketDataSubscription> getActiveSubscriptions() const;

    // Connection control
    bool startDataFeeds();
    void stopDataFeeds();
    bool isRunning() const { return is_running_.load(); }

    // Data access
    std::vector<LiveMarketTick> getLatestTicks(const std::string& symbol, int count = 10) const;
    AggregatedMarketView getAggregatedView(const std::string& symbol) const;
    std::map<std::string, AggregatedMarketView> getAllAggregatedViews() const;

    // Connection status
    std::vector<ExchangeConnectionStatus> getConnectionStatuses() const;
    ExchangeConnectionStatus getExchangeStatus(const std::string& exchange) const;
    bool isExchangeConnected(const std::string& exchange) const;

    // Performance metrics
    struct PerformanceMetrics {
        std::chrono::milliseconds average_latency;
        int ticks_per_second;
        double data_quality_average;
        int active_connections;
        double uptime_percentage;

        // Per-exchange breakdown
        std::map<std::string, std::chrono::milliseconds> exchange_latencies;
        std::map<std::string, double> exchange_quality_scores;
        std::map<std::string, int> exchange_tick_counts;
    };

    PerformanceMetrics getPerformanceMetrics() const;

    // Market data analysis
    struct MarketConditions {
        std::string dominant_regime;         // "normal", "volatile", "trending", "ranging"
        double market_stress_indicator;      // 0.0 to 1.0
        double liquidity_index;              // Composite liquidity measure
        std::vector<std::string> market_alerts;

        // Cross-exchange analysis
        double price_efficiency;             // How quickly arbitrage closes
        std::map<std::string, double> exchange_premiums; // Price premiums by exchange
        double market_fragmentation;         // Liquidity fragmentation measure
    };

    MarketConditions analyzeMarketConditions() const;

    // Integration with existing systems
    bool integrateWithDataStreamManager(Algorithm::RealTimeDataStreamManager* stream_manager);

    // Data export and historical storage
    bool exportMarketData(const std::string& symbol,
                         const std::chrono::system_clock::time_point& start,
                         const std::chrono::system_clock::time_point& end,
                         const std::string& filename) const;

    // Emergency controls
    void emergencyDisconnectAll();
    void emergencyDisconnectExchange(const std::string& exchange);
    bool switchToTestnet();
    bool switchToLiveTrading();

    // Callback registration
    void setTickCallback(std::function<void(const LiveMarketTick&)> callback);
    void setConnectionCallback(std::function<void(const ExchangeConnectionStatus&)> callback);
    void setQualityCallback(std::function<void(const std::string&, double)> callback);

    // Static utility methods
    static ExchangeCredentials createBinanceCredentials(const std::string& api_key,
                                                        const std::string& api_secret,
                                                        bool use_testnet = true);
    static ExchangeCredentials createCoinbaseCredentials(const std::string& api_key,
                                                         const std::string& api_secret,
                                                         const std::string& passphrase,
                                                         bool use_testnet = true);
    static ExchangeCredentials createKrakenCredentials(const std::string& api_key,
                                                       const std::string& api_secret,
                                                       bool use_testnet = true);
    static bool validateCredentials(const ExchangeCredentials& credentials);
    static double calculateSpreadBps(double bid, double ask);
    static std::chrono::milliseconds calculateLatency(const std::chrono::system_clock::time_point& sent,
                                                     const std::chrono::system_clock::time_point& received);
};

} // namespace Market
} // namespace CryptoClaude