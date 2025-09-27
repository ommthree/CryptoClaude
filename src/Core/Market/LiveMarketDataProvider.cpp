#include "LiveMarketDataProvider.h"
#include <iostream>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace CryptoClaude {
namespace Market {

LiveMarketDataProvider::LiveMarketDataProvider()
    : start_time_(std::chrono::system_clock::now()) {
    // Initialize HTTP clients for each potential exchange
    http_clients_["binance"] = std::make_unique<Http::HttpClient>();
    http_clients_["coinbase"] = std::make_unique<Http::HttpClient>();
    http_clients_["kraken"] = std::make_unique<Http::HttpClient>();
}

LiveMarketDataProvider::~LiveMarketDataProvider() {
    if (is_running_.load()) {
        stopDataFeeds();
    }
}

bool LiveMarketDataProvider::addExchangeCredentials(const std::string& exchange,
                                                   const ExchangeCredentials& credentials) {
    if (!validateCredentials(credentials)) {
        std::cerr << "Invalid credentials for exchange: " << exchange << std::endl;
        return false;
    }

    std::lock_guard<std::mutex> lock(connection_mutex_);
    exchange_credentials_[exchange] = credentials;

    // Initialize connection status
    connection_status_[exchange] = ExchangeConnectionStatus{};
    connection_status_[exchange].exchange_name = exchange;

    return true;
}

bool LiveMarketDataProvider::updateExchangeCredentials(const std::string& exchange,
                                                      const ExchangeCredentials& credentials) {
    std::lock_guard<std::mutex> lock(connection_mutex_);

    if (exchange_credentials_.find(exchange) == exchange_credentials_.end()) {
        return false;
    }

    exchange_credentials_[exchange] = credentials;
    return true;
}

std::vector<std::string> LiveMarketDataProvider::getConfiguredExchanges() const {
    std::lock_guard<std::mutex> lock(connection_mutex_);

    std::vector<std::string> exchanges;
    for (const auto& [exchange, _] : exchange_credentials_) {
        exchanges.push_back(exchange);
    }
    return exchanges;
}

bool LiveMarketDataProvider::subscribe(const MarketDataSubscription& subscription) {
    std::lock_guard<std::mutex> lock(data_mutex_);

    // Check if already subscribed
    auto it = std::find_if(active_subscriptions_.begin(), active_subscriptions_.end(),
        [&subscription](const MarketDataSubscription& sub) {
            return sub.symbol == subscription.symbol;
        });

    if (it != active_subscriptions_.end()) {
        // Update existing subscription
        *it = subscription;
    } else {
        // Add new subscription
        active_subscriptions_.push_back(subscription);
    }

    // Initialize aggregated view for this symbol
    aggregated_views_[subscription.symbol] = AggregatedMarketView{};
    aggregated_views_[subscription.symbol].symbol = subscription.symbol;

    return true;
}

bool LiveMarketDataProvider::unsubscribe(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(data_mutex_);

    active_subscriptions_.erase(
        std::remove_if(active_subscriptions_.begin(), active_subscriptions_.end(),
            [&symbol](const MarketDataSubscription& sub) {
                return sub.symbol == symbol;
            }),
        active_subscriptions_.end());

    aggregated_views_.erase(symbol);
    return true;
}

std::vector<LiveMarketDataProvider::MarketDataSubscription>
LiveMarketDataProvider::getActiveSubscriptions() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return active_subscriptions_;
}

bool LiveMarketDataProvider::startDataFeeds() {
    if (is_running_.load()) {
        return false; // Already running
    }

    std::lock_guard<std::mutex> lock(connection_mutex_);

    if (exchange_credentials_.empty()) {
        std::cerr << "No exchange credentials configured" << std::endl;
        return false;
    }

    is_running_.store(true);
    start_time_ = std::chrono::system_clock::now();

    // Start exchange connection threads
    for (const auto& [exchange, credentials] : exchange_credentials_) {
        exchange_threads_[exchange] = std::thread([this, exchange]() {
            if (exchange == "binance") {
                connectToBinance();
            } else if (exchange == "coinbase") {
                connectToCoinbase();
            } else if (exchange == "kraken") {
                connectToKraken();
            }
        });
    }

    // Start aggregation and monitoring threads
    data_aggregation_thread_ = std::thread(&LiveMarketDataProvider::runDataAggregation, this);
    quality_monitoring_thread_ = std::thread(&LiveMarketDataProvider::runQualityMonitoring, this);

    std::cout << "Live market data feeds started for " << exchange_credentials_.size()
              << " exchanges" << std::endl;

    return true;
}

void LiveMarketDataProvider::stopDataFeeds() {
    if (!is_running_.load()) {
        return;
    }

    is_running_.store(false);

    // Wait for all threads to complete
    for (auto& [exchange, thread] : exchange_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    if (data_aggregation_thread_.joinable()) {
        data_aggregation_thread_.join();
    }

    if (quality_monitoring_thread_.joinable()) {
        quality_monitoring_thread_.join();
    }

    exchange_threads_.clear();

    std::cout << "Live market data feeds stopped" << std::endl;
}

void LiveMarketDataProvider::connectToBinance() {
    const std::string exchange = "binance";
    auto& status = connection_status_[exchange];

    while (is_running_.load()) {
        try {
            status.is_connected = true;
            status.last_heartbeat = std::chrono::system_clock::now();

            // Simulate WebSocket connection and data reception
            for (const auto& subscription : active_subscriptions_) {
                if (std::find(subscription.exchanges.begin(), subscription.exchanges.end(),
                             exchange) != subscription.exchanges.end()) {

                    // Simulate receiving market tick
                    LiveMarketTick tick;
                    tick.symbol = subscription.symbol;
                    tick.exchange = exchange;
                    tick.server_timestamp = std::chrono::system_clock::now();
                    tick.local_timestamp = std::chrono::system_clock::now();

                    // Generate realistic price data (this would be from WebSocket in production)
                    static double base_price = 40000.0; // Starting BTC price
                    double random_walk = (rand() % 200 - 100) / 10000.0; // ±1% random walk
                    base_price *= (1.0 + random_walk);

                    tick.last_price = base_price;
                    tick.bid_price = base_price - (base_price * 0.0001); // 1 bps spread
                    tick.ask_price = base_price + (base_price * 0.0001);
                    tick.mid_price = (tick.bid_price + tick.ask_price) / 2.0;
                    tick.spread_bps = calculateSpreadBps(tick.bid_price, tick.ask_price);

                    tick.bid_volume = 10.0 + (rand() % 50);
                    tick.ask_volume = 10.0 + (rand() % 50);
                    tick.last_volume = 0.1 + (rand() % 10) / 10.0;
                    tick.daily_volume = 50000.0 + (rand() % 20000);

                    // Quality metrics
                    tick.latency = std::chrono::milliseconds{10 + rand() % 40}; // 10-50ms
                    tick.data_quality_score = 0.95 + (rand() % 5) / 100.0; // 95-99%
                    tick.is_stale = false;

                    // Add to buffer
                    {
                        std::lock_guard<std::mutex> lock(data_mutex_);
                        tick_buffer_.push(tick);
                        if (tick_buffer_.size() > 10000) { // Limit buffer size
                            tick_buffer_.pop();
                        }
                    }

                    status.ticks_received_1m++;
                    status.avg_latency = tick.latency;
                }
            }

            // Update uptime
            auto now = std::chrono::system_clock::now();
            auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);
            status.uptime_percentage_24h = std::min(100.0, uptime.count() / 864.0); // 86400 seconds in 24h

            std::this_thread::sleep_for(std::chrono::milliseconds{100}); // 10 ticks per second

        } catch (const std::exception& e) {
            status.is_connected = false;
            status.last_error_message = e.what();
            status.last_error_time = std::chrono::system_clock::now();
            status.error_count_1h++;

            std::cerr << "Binance connection error: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds{5}); // Retry delay
        }
    }

    status.is_connected = false;
}

void LiveMarketDataProvider::connectToCoinbase() {
    const std::string exchange = "coinbase";
    auto& status = connection_status_[exchange];

    while (is_running_.load()) {
        try {
            status.is_connected = true;
            status.last_heartbeat = std::chrono::system_clock::now();

            // Similar implementation to Binance with slight variations
            for (const auto& subscription : active_subscriptions_) {
                if (std::find(subscription.exchanges.begin(), subscription.exchanges.end(),
                             exchange) != subscription.exchanges.end()) {

                    LiveMarketTick tick;
                    tick.symbol = subscription.symbol;
                    tick.exchange = exchange;
                    tick.server_timestamp = std::chrono::system_clock::now();
                    tick.local_timestamp = std::chrono::system_clock::now();

                    // Coinbase typically has slightly different pricing
                    static double coinbase_base_price = 40050.0; // Slight premium
                    double random_walk = (rand() % 200 - 100) / 10000.0;
                    coinbase_base_price *= (1.0 + random_walk);

                    tick.last_price = coinbase_base_price;
                    tick.bid_price = coinbase_base_price - (coinbase_base_price * 0.00012); // Slightly wider spread
                    tick.ask_price = coinbase_base_price + (coinbase_base_price * 0.00012);
                    tick.mid_price = (tick.bid_price + tick.ask_price) / 2.0;
                    tick.spread_bps = calculateSpreadBps(tick.bid_price, tick.ask_price);

                    tick.bid_volume = 8.0 + (rand() % 40); // Slightly less volume
                    tick.ask_volume = 8.0 + (rand() % 40);
                    tick.last_volume = 0.08 + (rand() % 8) / 10.0;
                    tick.daily_volume = 45000.0 + (rand() % 15000);

                    tick.latency = std::chrono::milliseconds{15 + rand() % 35}; // 15-50ms
                    tick.data_quality_score = 0.94 + (rand() % 5) / 100.0; // 94-98%
                    tick.is_stale = false;

                    {
                        std::lock_guard<std::mutex> lock(data_mutex_);
                        tick_buffer_.push(tick);
                        if (tick_buffer_.size() > 10000) {
                            tick_buffer_.pop();
                        }
                    }

                    status.ticks_received_1m++;
                    status.avg_latency = tick.latency;
                }
            }

            auto now = std::chrono::system_clock::now();
            auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);
            status.uptime_percentage_24h = std::min(100.0, uptime.count() / 864.0);

            std::this_thread::sleep_for(std::chrono::milliseconds{110}); // Slightly slower

        } catch (const std::exception& e) {
            status.is_connected = false;
            status.last_error_message = e.what();
            status.last_error_time = std::chrono::system_clock::now();
            status.error_count_1h++;

            std::cerr << "Coinbase connection error: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds{5});
        }
    }

    status.is_connected = false;
}

void LiveMarketDataProvider::connectToKraken() {
    const std::string exchange = "kraken";
    auto& status = connection_status_[exchange];

    while (is_running_.load()) {
        try {
            status.is_connected = true;
            status.last_heartbeat = std::chrono::system_clock::now();

            for (const auto& subscription : active_subscriptions_) {
                if (std::find(subscription.exchanges.begin(), subscription.exchanges.end(),
                             exchange) != subscription.exchanges.end()) {

                    LiveMarketTick tick;
                    tick.symbol = subscription.symbol;
                    tick.exchange = exchange;
                    tick.server_timestamp = std::chrono::system_clock::now();
                    tick.local_timestamp = std::chrono::system_clock::now();

                    // Kraken often has different pricing characteristics
                    static double kraken_base_price = 39980.0; // Slight discount
                    double random_walk = (rand() % 200 - 100) / 10000.0;
                    kraken_base_price *= (1.0 + random_walk);

                    tick.last_price = kraken_base_price;
                    tick.bid_price = kraken_base_price - (kraken_base_price * 0.00015); // Wider spread
                    tick.ask_price = kraken_base_price + (kraken_base_price * 0.00015);
                    tick.mid_price = (tick.bid_price + tick.ask_price) / 2.0;
                    tick.spread_bps = calculateSpreadBps(tick.bid_price, tick.ask_price);

                    tick.bid_volume = 6.0 + (rand() % 30); // Less volume
                    tick.ask_volume = 6.0 + (rand() % 30);
                    tick.last_volume = 0.05 + (rand() % 5) / 10.0;
                    tick.daily_volume = 40000.0 + (rand() % 12000);

                    tick.latency = std::chrono::milliseconds{20 + rand() % 60}; // Higher latency
                    tick.data_quality_score = 0.92 + (rand() % 6) / 100.0; // 92-97%
                    tick.is_stale = false;

                    {
                        std::lock_guard<std::mutex> lock(data_mutex_);
                        tick_buffer_.push(tick);
                        if (tick_buffer_.size() > 10000) {
                            tick_buffer_.pop();
                        }
                    }

                    status.ticks_received_1m++;
                    status.avg_latency = tick.latency;
                }
            }

            auto now = std::chrono::system_clock::now();
            auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);
            status.uptime_percentage_24h = std::min(100.0, uptime.count() / 864.0);

            std::this_thread::sleep_for(std::chrono::milliseconds{130}); // Slowest

        } catch (const std::exception& e) {
            status.is_connected = false;
            status.last_error_message = e.what();
            status.last_error_time = std::chrono::system_clock::now();
            status.error_count_1h++;

            std::cerr << "Kraken connection error: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds{5});
        }
    }

    status.is_connected = false;
}

void LiveMarketDataProvider::runDataAggregation() {
    while (is_running_.load()) {
        try {
            aggregateMarketData();
            calculateQualityMetrics();
            detectArbitrageOpportunities();

            std::this_thread::sleep_for(std::chrono::milliseconds{500}); // Aggregate every 500ms

        } catch (const std::exception& e) {
            std::cerr << "Data aggregation error: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds{1});
        }
    }
}

void LiveMarketDataProvider::aggregateMarketData() {
    std::lock_guard<std::mutex> lock(data_mutex_);

    // Group ticks by symbol
    std::map<std::string, std::vector<LiveMarketTick>> symbol_ticks;

    // Process all ticks in buffer
    while (!tick_buffer_.empty()) {
        auto tick = tick_buffer_.front();
        tick_buffer_.pop();
        symbol_ticks[tick.symbol].push_back(tick);
    }

    // Create aggregated views
    for (const auto& [symbol, ticks] : symbol_ticks) {
        if (ticks.empty()) continue;

        auto& view = aggregated_views_[symbol];
        view.symbol = symbol;
        view.timestamp = std::chrono::system_clock::now();

        // Find best bid/ask across exchanges
        view.best_bid = 0;
        view.best_ask = std::numeric_limits<double>::max();

        double total_volume = 0;
        double weighted_price_sum = 0;
        view.active_exchange_count = 0;

        std::map<std::string, double> exchange_bids;
        std::map<std::string, double> exchange_asks;

        for (const auto& tick : ticks) {
            // Best bid (highest)
            if (tick.bid_price > view.best_bid) {
                view.best_bid = tick.bid_price;
                view.best_bid_exchange = tick.exchange;
            }

            // Best ask (lowest)
            if (tick.ask_price < view.best_ask) {
                view.best_ask = tick.ask_price;
                view.best_ask_exchange = tick.exchange;
            }

            // Volume aggregation
            view.total_bid_volume += tick.bid_volume;
            view.total_ask_volume += tick.ask_volume;
            view.exchange_volumes[tick.exchange] = tick.bid_volume + tick.ask_volume;

            // Weighted mid calculation
            total_volume += tick.last_volume;
            weighted_price_sum += tick.mid_price * tick.last_volume;

            exchange_bids[tick.exchange] = tick.bid_price;
            exchange_asks[tick.exchange] = tick.ask_price;

            view.active_exchange_count++;
        }

        view.weighted_mid = (total_volume > 0) ? weighted_price_sum / total_volume :
                           (view.best_bid + view.best_ask) / 2.0;

        // Calculate price dispersion
        std::vector<double> mid_prices;
        for (const auto& tick : ticks) {
            mid_prices.push_back(tick.mid_price);
        }

        if (mid_prices.size() > 1) {
            double mean = std::accumulate(mid_prices.begin(), mid_prices.end(), 0.0) / mid_prices.size();
            double variance = 0;
            for (double price : mid_prices) {
                variance += (price - mean) * (price - mean);
            }
            view.price_dispersion = std::sqrt(variance / mid_prices.size()) / mean;
        }

        // Quality score composite
        double quality_sum = 0;
        for (const auto& tick : ticks) {
            quality_sum += tick.data_quality_score;
        }
        view.composite_quality_score = quality_sum / ticks.size();
    }
}

void LiveMarketDataProvider::calculateQualityMetrics() {
    // Calculate various quality metrics for monitoring
    for (auto& [exchange, status] : connection_status_) {
        if (status.ticks_received_1m > 0) {
            status.message_loss_rate = std::max(0.0,
                1.0 - (static_cast<double>(status.ticks_received_1m) / 600.0)); // Expected 600 per minute
        }
    }
}

void LiveMarketDataProvider::detectArbitrageOpportunities() {
    for (auto& [symbol, view] : aggregated_views_) {
        view.arbitrage_opportunities.clear();

        if (view.best_bid > 0 && view.best_ask > 0 &&
            view.best_bid_exchange != view.best_ask_exchange) {

            double spread_bps = ((view.best_bid - view.best_ask) / view.best_ask) * 10000;

            if (spread_bps > 5.0) { // Arbitrage opportunity > 5 bps
                std::ostringstream arb;
                arb << "Buy " << view.best_ask_exchange << " @ " << std::fixed << std::setprecision(2)
                    << view.best_ask << ", Sell " << view.best_bid_exchange << " @ " << view.best_bid
                    << " (+" << spread_bps << " bps)";
                view.arbitrage_opportunities.push_back(arb.str());
            }
        }
    }
}

void LiveMarketDataProvider::runQualityMonitoring() {
    while (is_running_.load()) {
        try {
            validateDataLatency();
            checkConnectionHealth();
            handleDataQualityIssues();

            std::this_thread::sleep_for(std::chrono::seconds{5}); // Monitor every 5 seconds

        } catch (const std::exception& e) {
            std::cerr << "Quality monitoring error: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds{5});
        }
    }
}

void LiveMarketDataProvider::validateDataLatency() {
    auto now = std::chrono::system_clock::now();

    for (auto& [exchange, status] : connection_status_) {
        if (status.is_connected) {
            auto time_since_heartbeat = std::chrono::duration_cast<std::chrono::seconds>(
                now - status.last_heartbeat);

            if (time_since_heartbeat > std::chrono::seconds{30}) {
                status.is_connected = false;
                status.last_error_message = "Heartbeat timeout";
                status.last_error_time = now;
            }
        }
    }
}

void LiveMarketDataProvider::checkConnectionHealth() {
    for (auto& [exchange, status] : connection_status_) {
        // Reset minute counters periodically
        static auto last_reset = std::chrono::system_clock::now();
        auto now = std::chrono::system_clock::now();

        if (std::chrono::duration_cast<std::chrono::minutes>(now - last_reset) >= std::chrono::minutes{1}) {
            status.ticks_received_1m = 0;
            status.messages_sent_1m = 0;
            last_reset = now;
        }
    }
}

void LiveMarketDataProvider::handleDataQualityIssues() {
    // Identify and handle data quality issues
    for (const auto& [exchange, status] : connection_status_) {
        if (!status.is_connected) {
            // Could trigger reconnection logic here
            continue;
        }

        if (status.avg_latency > std::chrono::milliseconds{200}) {
            // High latency warning
            std::cout << "Warning: High latency detected for " << exchange
                      << ": " << status.avg_latency.count() << "ms" << std::endl;
        }

        if (status.message_loss_rate > 0.1) {
            // High message loss warning
            std::cout << "Warning: High message loss for " << exchange
                      << ": " << (status.message_loss_rate * 100) << "%" << std::endl;
        }
    }
}

// Static utility methods implementation
LiveMarketDataProvider::ExchangeCredentials
LiveMarketDataProvider::createBinanceCredentials(const std::string& api_key,
                                                 const std::string& api_secret,
                                                 bool use_testnet) {
    ExchangeCredentials creds;
    creds.exchange_name = "binance";
    creds.api_key = api_key;
    creds.api_secret = api_secret;
    creds.base_url = use_testnet ? "https://testnet.binance.vision" : "https://api.binance.com";
    creds.websocket_url = use_testnet ? "wss://testnet.binance.vision/ws/" : "wss://stream.binance.com:9443/ws/";
    creds.use_testnet = use_testnet;
    creds.max_requests_per_second = 20;
    return creds;
}

LiveMarketDataProvider::ExchangeCredentials
LiveMarketDataProvider::createCoinbaseCredentials(const std::string& api_key,
                                                  const std::string& api_secret,
                                                  const std::string& passphrase,
                                                  bool use_testnet) {
    ExchangeCredentials creds;
    creds.exchange_name = "coinbase";
    creds.api_key = api_key;
    creds.api_secret = api_secret;
    creds.passphrase = passphrase;
    creds.base_url = use_testnet ? "https://api-public.sandbox.pro.coinbase.com" : "https://api.pro.coinbase.com";
    creds.websocket_url = use_testnet ? "wss://ws-feed-public.sandbox.pro.coinbase.com" : "wss://ws-feed.pro.coinbase.com";
    creds.use_testnet = use_testnet;
    creds.max_requests_per_second = 10;
    return creds;
}

LiveMarketDataProvider::ExchangeCredentials
LiveMarketDataProvider::createKrakenCredentials(const std::string& api_key,
                                                const std::string& api_secret,
                                                bool use_testnet) {
    ExchangeCredentials creds;
    creds.exchange_name = "kraken";
    creds.api_key = api_key;
    creds.api_secret = api_secret;
    creds.base_url = "https://api.kraken.com";
    creds.websocket_url = "wss://ws.kraken.com";
    creds.use_testnet = use_testnet;
    creds.max_requests_per_second = 15;
    return creds;
}

bool LiveMarketDataProvider::validateCredentials(const ExchangeCredentials& credentials) {
    return !credentials.exchange_name.empty() &&
           !credentials.api_key.empty() &&
           !credentials.api_secret.empty() &&
           !credentials.base_url.empty() &&
           !credentials.websocket_url.empty();
}

double LiveMarketDataProvider::calculateSpreadBps(double bid, double ask) {
    if (bid <= 0 || ask <= 0 || ask <= bid) return 0.0;
    return ((ask - bid) / ((ask + bid) / 2.0)) * 10000.0;
}

std::chrono::milliseconds
LiveMarketDataProvider::calculateLatency(const std::chrono::system_clock::time_point& sent,
                                         const std::chrono::system_clock::time_point& received) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(received - sent);
}

// Additional method implementations for completeness...
std::vector<LiveMarketDataProvider::LiveMarketTick>
LiveMarketDataProvider::getLatestTicks(const std::string& symbol, int count) const {
    std::vector<LiveMarketTick> result;
    // Implementation would retrieve from historical storage
    return result;
}

LiveMarketDataProvider::AggregatedMarketView
LiveMarketDataProvider::getAggregatedView(const std::string& symbol) const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    auto it = aggregated_views_.find(symbol);
    return (it != aggregated_views_.end()) ? it->second : AggregatedMarketView{};
}

std::vector<LiveMarketDataProvider::ExchangeConnectionStatus>
LiveMarketDataProvider::getConnectionStatuses() const {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    std::vector<ExchangeConnectionStatus> statuses;
    for (const auto& [exchange, status] : connection_status_) {
        statuses.push_back(status);
    }
    return statuses;
}

LiveMarketDataProvider::PerformanceMetrics
LiveMarketDataProvider::getPerformanceMetrics() const {
    PerformanceMetrics metrics;

    // Calculate composite metrics from exchange data
    std::vector<std::chrono::milliseconds> latencies;
    std::vector<double> quality_scores;
    int total_ticks = 0;
    int connected_count = 0;

    for (const auto& [exchange, status] : connection_status_) {
        if (status.is_connected) {
            connected_count++;
            latencies.push_back(status.avg_latency);
            total_ticks += status.ticks_received_1m;
            metrics.exchange_latencies[exchange] = status.avg_latency;
            metrics.exchange_tick_counts[exchange] = status.ticks_received_1m;
        }
    }

    if (!latencies.empty()) {
        auto total_latency = std::accumulate(latencies.begin(), latencies.end(),
                                           std::chrono::milliseconds{0});
        metrics.average_latency = total_latency / latencies.size();
    }

    metrics.ticks_per_second = total_ticks / 60; // Convert per minute to per second
    metrics.active_connections = connected_count;

    return metrics;
}

} // namespace Market
} // namespace CryptoClaude