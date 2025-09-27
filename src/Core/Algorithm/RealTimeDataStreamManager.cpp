#include "RealTimeDataStreamManager.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <random>
#include <sstream>
#include <iomanip>

namespace CryptoClaude {
namespace Algorithm {

RealTimeDataStreamManager::RealTimeDataStreamManager() {
    // Initialize with default exchange configurations
    addStreamConfig(createBinanceConfig({"BTC", "ETH", "ADA", "DOT"}));
    addStreamConfig(createCoinbaseConfig({"BTC", "ETH", "ADA", "DOT"}));
    addStreamConfig(createKrakenConfig({"BTC", "ETH", "ADA", "DOT"}));

    // Start health monitoring thread
    std::thread health_monitor(&RealTimeDataStreamManager::monitorStreamHealth, this);
    health_monitor.detach();
}

RealTimeDataStreamManager::~RealTimeDataStreamManager() {
    stopAllStreams();
}

void RealTimeDataStreamManager::addStreamConfig(const StreamConfig& config) {
    stream_configs_.push_back(config);
    stream_health_[config.exchange_name] = StreamHealth(config.exchange_name);
}

void RealTimeDataStreamManager::updateStreamConfig(const std::string& exchange, const StreamConfig& config) {
    auto it = std::find_if(stream_configs_.begin(), stream_configs_.end(),
                          [&exchange](const StreamConfig& c) { return c.exchange_name == exchange; });

    if (it != stream_configs_.end()) {
        *it = config;
    }
}

void RealTimeDataStreamManager::removeStreamConfig(const std::string& exchange) {
    stream_configs_.erase(
        std::remove_if(stream_configs_.begin(), stream_configs_.end(),
                      [&exchange](const StreamConfig& c) { return c.exchange_name == exchange; }),
        stream_configs_.end());

    stream_health_.erase(exchange);
}

bool RealTimeDataStreamManager::startAllStreams() {
    if (is_running_.load()) {
        return true; // Already running
    }

    is_running_.store(true);

    // Start streams for each configured exchange
    for (const auto& config : stream_configs_) {
        std::thread stream_thread(&RealTimeDataStreamManager::startExchangeStream, this, config);
        stream_threads_.push_back(std::move(stream_thread));
    }

    // Start aggregation thread
    std::thread aggregation_thread([this]() {
        while (is_running_.load()) {
            aggregateMarketData();
            std::this_thread::sleep_for(std::chrono::milliseconds{100}); // 10Hz aggregation
        }
    });
    aggregation_thread.detach();

    return true;
}

void RealTimeDataStreamManager::stopAllStreams() {
    is_running_.store(false);

    // Wait for all threads to finish
    for (auto& thread : stream_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    stream_threads_.clear();

    // Clear data buffers
    std::lock_guard<std::mutex> lock(data_mutex_);
    tick_buffers_.clear();
    latest_ticks_.clear();
    aggregated_data_.clear();
}

void RealTimeDataStreamManager::startExchangeStream(const StreamConfig& config) {
    while (is_running_.load()) {
        try {
            // Simulate WebSocket connection and data streaming
            // In real implementation, this would establish actual WebSocket connections

            StreamHealth health(config.exchange_name);
            health.is_connected = true;
            health.is_healthy = true;
            health.last_update = std::chrono::system_clock::now();

            std::random_device rd;
            std::mt19937 gen(rd());

            // Simulate market data generation for each symbol
            for (const auto& symbol : config.symbols) {
                if (!is_running_.load()) break;

                // Generate realistic tick data
                RealTimeTick tick;
                tick.symbol = symbol;
                tick.timestamp = std::chrono::system_clock::now();
                tick.exchange = config.exchange_name;

                // Base prices for different symbols
                double base_price = 40000.0; // BTC
                if (symbol == "ETH") base_price = 3000.0;
                else if (symbol == "ADA") base_price = 1.0;
                else if (symbol == "DOT") base_price = 25.0;

                // Add some realistic variation
                std::normal_distribution<double> price_variation(0.0, 0.001); // 0.1% variation
                double price_delta = price_variation(gen);
                double current_price = base_price * (1.0 + price_delta);

                // Generate bid/ask spread
                std::uniform_real_distribution<double> spread_dist(0.0005, 0.002); // 0.05% to 0.2%
                double spread_pct = spread_dist(gen);
                double half_spread = current_price * spread_pct / 2.0;

                tick.mid_price = current_price;
                tick.bid_price = current_price - half_spread;
                tick.ask_price = current_price + half_spread;
                tick.last_price = current_price;

                tick.spread = tick.ask_price - tick.bid_price;
                tick.spread_bps = (tick.spread / tick.mid_price) * 10000.0;

                // Generate volume data
                std::uniform_real_distribution<double> volume_dist(0.1, 10.0);
                tick.bid_size = volume_dist(gen);
                tick.ask_size = volume_dist(gen);
                tick.last_size = volume_dist(gen) / 10.0;
                tick.volume_24h = volume_dist(gen) * 1000.0;

                // Data quality simulation
                std::uniform_real_distribution<double> quality_dist(0.85, 1.0);
                tick.quality_score = quality_dist(gen);

                // Latency simulation based on exchange
                std::uniform_int_distribution<int> latency_dist(10, 100);
                tick.latency = std::chrono::milliseconds(latency_dist(gen));

                if (config.exchange_name == "binance") {
                    tick.latency = std::chrono::milliseconds(latency_dist(gen) / 2); // Lower latency
                    tick.quality_score = std::min(1.0, tick.quality_score + 0.05);   // Higher quality
                }

                tick.is_stale = tick.latency > config.max_latency;

                // Calculate market context
                tick.price_change_24h = current_price * std::uniform_real_distribution<double>(-0.05, 0.05)(gen);
                tick.price_change_percentage = (tick.price_change_24h / current_price) * 100.0;
                tick.market_cap = current_price * 19000000; // Approximate supply

                validateDataQuality(tick);
                processIncomingTick(tick);

                // Update health metrics
                health.messages_received_total++;
                health.messages_processed++;
                health.average_latency = tick.latency;
                health.data_quality_score = tick.quality_score;
                health.last_update = std::chrono::system_clock::now();

                updateStreamHealth(config.exchange_name, health);

                if (tick_callback_) {
                    tick_callback_(tick);
                }

                // Simulate network delay
                std::this_thread::sleep_for(std::chrono::milliseconds(50 + latency_dist(gen) / 5));
            }

            // Update message rate
            health.message_rate_per_second = config.symbols.size() * (1000.0 / 50.0); // Approximate rate

        } catch (const std::exception& e) {
            // Handle connection errors
            StreamHealth health = stream_health_[config.exchange_name];
            health.is_connected = false;
            health.is_healthy = false;
            health.recent_errors.push_back(e.what());
            health.error_count_24h++;
            updateStreamHealth(config.exchange_name, health);

            if (error_callback_) {
                error_callback_(config.exchange_name, e.what());
            }

            // Wait before reconnecting
            std::this_thread::sleep_for(config.reconnect_delay);
        }
    }
}

void RealTimeDataStreamManager::processIncomingTick(const RealTimeTick& tick) {
    std::lock_guard<std::mutex> lock(data_mutex_);

    // Add to buffer
    auto& buffer = tick_buffers_[tick.symbol];
    buffer.push(tick);

    // Maintain buffer size
    while (buffer.size() > 1000) { // Keep last 1000 ticks
        buffer.pop();
    }

    // Update latest tick
    latest_ticks_[tick.symbol] = tick;
}

void RealTimeDataStreamManager::validateDataQuality(RealTimeTick& tick) {
    double quality_penalty = 0.0;

    // Check price consistency
    if (tick.bid_price >= tick.ask_price) {
        quality_penalty += 0.3; // Major penalty for crossed market
    }

    if (tick.last_price < tick.bid_price || tick.last_price > tick.ask_price) {
        quality_penalty += 0.1; // Penalty for last price outside bid/ask
    }

    // Check spread reasonableness
    if (tick.spread_bps > 50.0) { // > 0.5%
        quality_penalty += 0.1;
    }

    // Check latency
    if (tick.latency > std::chrono::milliseconds{200}) {
        quality_penalty += 0.2;
    }

    // Apply penalty
    tick.quality_score = std::max(0.0, tick.quality_score - quality_penalty);

    // Mark as stale if quality is too low
    if (tick.quality_score < 0.5) {
        tick.is_stale = true;
    }
}

void RealTimeDataStreamManager::aggregateMarketData() {
    std::lock_guard<std::mutex> lock(data_mutex_);

    auto now = std::chrono::system_clock::now();

    // Get all unique symbols
    std::set<std::string> all_symbols;
    for (const auto& [symbol, tick] : latest_ticks_) {
        all_symbols.insert(symbol);
    }

    // Aggregate data for each symbol
    for (const auto& symbol : all_symbols) {
        AggregatedMarketData aggregated;
        aggregated.symbol = symbol;
        aggregated.timestamp = now;

        std::vector<RealTimeTick> symbol_ticks;

        // Collect ticks from all exchanges for this symbol
        for (const auto& [tick_symbol, tick] : latest_ticks_) {
            if (tick_symbol == symbol) {
                symbol_ticks.push_back(tick);
            }
        }

        if (symbol_ticks.empty()) continue;

        // Find best bid and ask
        double best_bid = 0.0;
        double best_ask = std::numeric_limits<double>::max();

        for (const auto& tick : symbol_ticks) {
            if (!tick.is_stale && tick.quality_score > 0.7) {
                if (tick.bid_price > best_bid) {
                    best_bid = tick.bid_price;
                    aggregated.best_bid_exchange = tick.exchange;
                }
                if (tick.ask_price < best_ask) {
                    best_ask = tick.ask_price;
                    aggregated.best_ask_exchange = tick.exchange;
                }
            }
        }

        aggregated.best_bid = best_bid;
        aggregated.best_ask = best_ask;

        // Calculate consolidated price (volume-weighted average)
        double total_volume = 0.0;
        double weighted_price = 0.0;

        for (const auto& tick : symbol_ticks) {
            if (!tick.is_stale) {
                double weight = tick.volume_24h * tick.quality_score;
                weighted_price += tick.mid_price * weight;
                total_volume += weight;
                aggregated.total_volume_24h += tick.volume_24h;
                aggregated.volume_by_exchange[tick.exchange] = tick.volume_24h;
            }
        }

        if (total_volume > 0) {
            aggregated.consolidated_price = weighted_price / total_volume;
        } else {
            aggregated.consolidated_price = (best_bid + best_ask) / 2.0;
        }

        // Calculate quality metrics
        aggregated.participating_exchanges = symbol_ticks.size();

        double quality_sum = 0.0;
        for (const auto& tick : symbol_ticks) {
            quality_sum += tick.quality_score;
        }
        aggregated.consensus_quality = quality_sum / symbol_ticks.size();

        // Calculate data freshness
        auto oldest_timestamp = now;
        for (const auto& tick : symbol_ticks) {
            if (tick.timestamp < oldest_timestamp) {
                oldest_timestamp = tick.timestamp;
            }
        }
        auto freshness_duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - oldest_timestamp);
        aggregated.data_freshness_score = 1.0 - (freshness_duration.count() / 5000.0); // 5 second decay

        // Calculate price spread across exchanges
        if (symbol_ticks.size() > 1) {
            double min_price = std::numeric_limits<double>::max();
            double max_price = 0.0;

            for (const auto& tick : symbol_ticks) {
                if (!tick.is_stale) {
                    min_price = std::min(min_price, tick.mid_price);
                    max_price = std::max(max_price, tick.mid_price);
                }
            }

            aggregated.price_spread_across_exchanges = max_price - min_price;
            aggregated.is_arbitrage_opportunity =
                (aggregated.price_spread_across_exchanges / aggregated.consolidated_price) > 0.005; // 0.5%
        }

        // Calculate weighted average spread
        double total_spread_weight = 0.0;
        double weighted_spread = 0.0;

        for (const auto& tick : symbol_ticks) {
            if (!tick.is_stale) {
                double weight = tick.quality_score;
                weighted_spread += tick.spread_bps * weight;
                total_spread_weight += weight;
            }
        }

        if (total_spread_weight > 0) {
            aggregated.weighted_average_spread = weighted_spread / total_spread_weight;
        }

        aggregated_data_[symbol] = aggregated;

        if (aggregated_callback_) {
            aggregated_callback_(aggregated);
        }
    }

    last_aggregation_ = now;
}

void RealTimeDataStreamManager::updateStreamHealth(const std::string& exchange, const StreamHealth& health) {
    std::lock_guard<std::mutex> lock(health_mutex_);
    stream_health_[exchange] = health;
}

void RealTimeDataStreamManager::monitorStreamHealth() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds{10});

        if (!is_running_.load()) break;

        std::lock_guard<std::mutex> lock(health_mutex_);

        for (auto& [exchange, health] : stream_health_) {
            auto now = std::chrono::system_clock::now();
            auto time_since_update = std::chrono::duration_cast<std::chrono::seconds>(now - health.last_update);

            // Check if stream is stale
            if (time_since_update > std::chrono::seconds{30}) {
                health.is_healthy = false;
                if (error_callback_) {
                    error_callback_(exchange, "Stream appears to be stale");
                }
            }

            // Update overall health
            if (health.error_count_24h > 10) {
                health.is_healthy = false;
            }

            if (health.data_quality_score < 0.7) {
                health.is_healthy = false;
            }
        }
    }
}

// Data access methods
RealTimeDataStreamManager::RealTimeTick
RealTimeDataStreamManager::getLatestTick(const std::string& symbol) const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    auto it = latest_ticks_.find(symbol);
    return (it != latest_ticks_.end()) ? it->second : RealTimeTick{};
}

std::vector<RealTimeDataStreamManager::RealTimeTick>
RealTimeDataStreamManager::getRecentTicks(const std::string& symbol, int count) const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    std::vector<RealTimeTick> result;

    auto it = tick_buffers_.find(symbol);
    if (it != tick_buffers_.end()) {
        auto buffer = it->second; // Copy the queue

        while (!buffer.empty() && result.size() < count) {
            result.insert(result.begin(), buffer.front());
            buffer.pop();
        }
    }

    return result;
}

RealTimeDataStreamManager::AggregatedMarketData
RealTimeDataStreamManager::getAggregatedData(const std::string& symbol) const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    auto it = aggregated_data_.find(symbol);
    return (it != aggregated_data_.end()) ? it->second : AggregatedMarketData{};
}

double RealTimeDataStreamManager::getCurrentPrice(const std::string& symbol) const {
    auto aggregated = getAggregatedData(symbol);
    return aggregated.consolidated_price;
}

double RealTimeDataStreamManager::getBestBid(const std::string& symbol) const {
    auto aggregated = getAggregatedData(symbol);
    return aggregated.best_bid;
}

double RealTimeDataStreamManager::getBestAsk(const std::string& symbol) const {
    auto aggregated = getAggregatedData(symbol);
    return aggregated.best_ask;
}

// Market regime detection
RealTimeDataStreamManager::MarketRegimeInfo
RealTimeDataStreamManager::detectCurrentMarketRegime(const std::string& symbol) const {
    MarketRegimeInfo info;
    info.detected_at = std::chrono::system_clock::now();

    auto recent_ticks = getRecentTicks(symbol, 100);
    if (recent_ticks.size() < 50) {
        info.regime = MarketRegime::NORMAL;
        info.confidence = 0.5;
        info.description = "Insufficient data for regime detection";
        return info;
    }

    // Calculate volatility
    std::vector<double> returns;
    for (size_t i = 1; i < recent_ticks.size(); ++i) {
        double return_val = (recent_ticks[i].mid_price - recent_ticks[i-1].mid_price) / recent_ticks[i-1].mid_price;
        returns.push_back(return_val);
    }

    double mean_return = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
    double variance = 0.0;
    for (double ret : returns) {
        variance += (ret - mean_return) * (ret - mean_return);
    }
    double volatility = std::sqrt(variance / returns.size());

    // Calculate trend strength
    double price_start = recent_ticks[0].mid_price;
    double price_end = recent_ticks.back().mid_price;
    double trend_strength = std::abs(price_end - price_start) / price_start;

    // Calculate liquidity (average bid-ask spread)
    double avg_spread = 0.0;
    for (const auto& tick : recent_ticks) {
        avg_spread += tick.spread_bps;
    }
    avg_spread /= recent_ticks.size();

    // Regime classification
    if (volatility > 0.02) { // 2% volatility threshold
        info.regime = MarketRegime::VOLATILE;
        info.confidence = std::min(1.0, volatility * 50.0);
        info.description = "High volatility detected";
        info.indicators.push_back("Volatility: " + std::to_string(volatility * 100.0) + "%");
    } else if (trend_strength > 0.015) { // 1.5% trend threshold
        info.regime = MarketRegime::TRENDING;
        info.confidence = std::min(1.0, trend_strength * 67.0);
        info.description = "Strong directional movement";
        info.indicators.push_back("Trend strength: " + std::to_string(trend_strength * 100.0) + "%");
    } else if (avg_spread > 30.0) { // 0.3% spread threshold
        info.regime = MarketRegime::ILLIQUID;
        info.confidence = std::min(1.0, (avg_spread - 10.0) / 40.0);
        info.description = "Low liquidity conditions";
        info.indicators.push_back("Average spread: " + std::to_string(avg_spread) + " bps");
    } else if (volatility < 0.005 && trend_strength < 0.005) {
        info.regime = MarketRegime::RANGING;
        info.confidence = 1.0 - std::max(volatility, trend_strength) * 200.0;
        info.description = "Sideways price action";
        info.indicators.push_back("Low volatility and trend");
    } else {
        info.regime = MarketRegime::NORMAL;
        info.confidence = 0.8;
        info.description = "Normal trading conditions";
    }

    return info;
}

// Static configuration methods
RealTimeDataStreamManager::StreamConfig
RealTimeDataStreamManager::createBinanceConfig(const std::vector<std::string>& symbols) {
    StreamConfig config("binance", "wss://stream.binance.com:9443/ws");
    config.symbols = symbols;
    config.channels = {"ticker", "depth"};
    config.is_primary = true;
    config.reliability_weight = 1.0;
    config.max_latency = std::chrono::milliseconds{100}; // Lower latency for Binance
    config.min_quality_score = 0.9;
    return config;
}

RealTimeDataStreamManager::StreamConfig
RealTimeDataStreamManager::createCoinbaseConfig(const std::vector<std::string>& symbols) {
    StreamConfig config("coinbase", "wss://ws-feed.pro.coinbase.com");
    config.symbols = symbols;
    config.channels = {"ticker", "level2"};
    config.reliability_weight = 0.9;
    config.max_latency = std::chrono::milliseconds{150};
    config.min_quality_score = 0.85;
    return config;
}

RealTimeDataStreamManager::StreamConfig
RealTimeDataStreamManager::createKrakenConfig(const std::vector<std::string>& symbols) {
    StreamConfig config("kraken", "wss://ws.kraken.com");
    config.symbols = symbols;
    config.channels = {"ticker", "book"};
    config.reliability_weight = 0.8;
    config.max_latency = std::chrono::milliseconds{200};
    config.min_quality_score = 0.8;
    return config;
}

double RealTimeDataStreamManager::calculateSpreadBps(double bid, double ask, double mid_price) {
    if (mid_price <= 0) return 0.0;
    return ((ask - bid) / mid_price) * 10000.0;
}

bool RealTimeDataStreamManager::isDataStale(const std::chrono::system_clock::time_point& timestamp,
                                           std::chrono::milliseconds threshold) {
    auto now = std::chrono::system_clock::now();
    auto age = std::chrono::duration_cast<std::chrono::milliseconds>(now - timestamp);
    return age > threshold;
}

void RealTimeDataStreamManager::setTickCallback(std::function<void(const RealTimeTick&)> callback) {
    tick_callback_ = callback;
}

void RealTimeDataStreamManager::setAggregatedDataCallback(std::function<void(const AggregatedMarketData&)> callback) {
    aggregated_callback_ = callback;
}

void RealTimeDataStreamManager::setErrorCallback(std::function<void(const std::string&, const std::string&)> callback) {
    error_callback_ = callback;
}

RealTimeDataStreamManager::PerformanceMetrics
RealTimeDataStreamManager::getPerformanceMetrics() const {
    PerformanceMetrics metrics;

    std::lock_guard<std::mutex> lock(health_mutex_);

    int total_connections = 0;
    int healthy_connections = 0;
    double total_latency_ms = 0.0;
    double total_quality = 0.0;
    double total_message_rate = 0.0;

    for (const auto& [exchange, health] : stream_health_) {
        if (health.is_connected) {
            total_connections++;
            total_latency_ms += health.average_latency.count();
            total_message_rate += health.message_rate_per_second;

            if (health.is_healthy) {
                healthy_connections++;
            }
        }

        total_quality += health.data_quality_score;
    }

    if (total_connections > 0) {
        metrics.average_latency = std::chrono::milliseconds(static_cast<int>(total_latency_ms / total_connections));
        metrics.active_connections = total_connections;
        metrics.healthy_connections = healthy_connections;
        metrics.total_message_rate = total_message_rate;
        metrics.system_uptime_percentage = (static_cast<double>(healthy_connections) / total_connections) * 100.0;
    }

    if (!stream_health_.empty()) {
        metrics.average_quality_score = total_quality / stream_health_.size();
    }

    return metrics;
}

} // namespace Algorithm
} // namespace CryptoClaude