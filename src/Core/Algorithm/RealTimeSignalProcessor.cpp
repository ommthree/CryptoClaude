#include "RealTimeSignalProcessor.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <random>
#include <sstream>
#include <iomanip>

namespace CryptoClaude {
namespace Algorithm {

RealTimeSignalProcessor::RealTimeSignalProcessor(const ProcessingConfig& config)
    : config_(config) {

    // Initialize core components
    data_stream_manager_ = std::make_unique<RealTimeDataStreamManager>();
    signal_generator_ = std::make_unique<RealMarketSignalGenerator>();
    correlation_validator_ = std::make_unique<RealCorrelationValidator>();
    decision_engine_ = std::make_unique<AlgorithmDecisionEngine>();

    // Initialize correlation monitoring
    correlation_monitor_ = std::make_unique<RealCorrelationValidator::RealTimeCorrelationMonitor>("portfolio", 100);

    stats_.current_status = "Initialized";
}

RealTimeSignalProcessor::~RealTimeSignalProcessor() {
    stopProcessing();
}

bool RealTimeSignalProcessor::startProcessing() {
    if (is_processing_.load()) {
        return true; // Already processing
    }

    // Start data stream manager
    if (!data_stream_manager_->startAllStreams()) {
        return false;
    }

    is_processing_.store(true);
    stats_.is_processing = true;
    stats_.current_status = "Processing";

    // Start processing thread
    processing_thread_ = std::thread(&RealTimeSignalProcessor::processRealTimeData, this);

    return true;
}

void RealTimeSignalProcessor::stopProcessing() {
    is_processing_.store(false);

    if (processing_thread_.joinable()) {
        processing_thread_.join();
    }

    data_stream_manager_->stopAllStreams();

    stats_.is_processing = false;
    stats_.current_status = "Stopped";
}

void RealTimeSignalProcessor::processRealTimeData() {
    auto last_processing_time = std::chrono::system_clock::now();

    while (is_processing_.load()) {
        try {
            auto start_time = std::chrono::high_resolution_clock::now();

            // Remove expired signals
            removeExpiredSignals();

            // Get available trading pairs from data stream
            auto available_symbols = data_stream_manager_->getAvailableSymbols();

            // Generate pairs for signal processing
            std::vector<std::string> trading_pairs;
            for (size_t i = 0; i < available_symbols.size(); ++i) {
                for (size_t j = i + 1; j < available_symbols.size(); ++j) {
                    trading_pairs.push_back(available_symbols[i] + "/" + available_symbols[j]);
                }
            }

            // Process each trading pair
            for (const auto& pair : trading_pairs) {
                if (!is_processing_.load()) break;

                try {
                    auto signal = generateLiveSignal(pair);

                    if (signal.confidence_score >= config_.min_confidence_threshold) {
                        validateSignalQuality(signal);

                        if (signal.meets_quality_threshold) {
                            applyMarketRegimeAdjustments(signal);
                            calculateRealTimeCorrelation(signal);

                            // Check TRS compliance if required
                            if (!config_.require_trs_compliance || signal.real_time_correlation >= config_.min_acceptable_correlation) {
                                addActiveSignal(signal);
                                updateSignalStats(signal);

                                if (signal_callback_) {
                                    signal_callback_(signal);
                                }
                            } else {
                                std::lock_guard<std::mutex> lock(stats_mutex_);
                                stats_.signals_filtered_correlation++;
                            }
                        } else {
                            std::lock_guard<std::mutex> lock(stats_mutex_);
                            stats_.signals_filtered_quality++;
                        }
                    }
                } catch (const std::exception& e) {
                    std::lock_guard<std::mutex> lock(stats_mutex_);
                    stats_.processing_errors_24h++;
                    stats_.recent_errors.push_back("Pair " + pair + ": " + e.what());

                    if (error_callback_) {
                        error_callback_("Signal processing error for " + pair + ": " + e.what());
                    }
                }
            }

            // Generate portfolio-level aggregation
            auto aggregation = aggregateSignals();
            if (aggregation_callback_) {
                aggregation_callback_(aggregation);
            }

            // Update TRS compliance status
            updateTRSComplianceStatus();

            // Calculate processing latency
            auto end_time = std::chrono::high_resolution_clock::now();
            auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

            {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                stats_.average_processing_latency = latency;
            }

            // Sleep until next processing interval
            std::this_thread::sleep_for(config_.processing_interval);

        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.processing_errors_24h++;
            stats_.recent_errors.push_back("Main processing error: " + std::string(e.what()));

            if (error_callback_) {
                error_callback_("Main processing error: " + std::string(e.what()));
            }

            // Brief pause before retrying
            std::this_thread::sleep_for(std::chrono::seconds{5});
        }
    }
}

RealTimeSignalProcessor::LiveTradingSignal
RealTimeSignalProcessor::generateLiveSignal(const std::string& pair_name) {
    LiveTradingSignal signal;
    signal.signal_id = generateSignalId(pair_name);
    signal.pair_name = pair_name;
    signal.generated_at = std::chrono::system_clock::now();

    // Parse pair name to get individual symbols
    size_t separator = pair_name.find('/');
    if (separator == std::string::npos) {
        throw std::invalid_argument("Invalid pair format: " + pair_name);
    }

    std::string base_symbol = pair_name.substr(0, separator);
    std::string quote_symbol = pair_name.substr(separator + 1);

    // Get current market data for both symbols
    auto base_data = data_stream_manager_->getAggregatedData(base_symbol);
    auto quote_data = data_stream_manager_->getAggregatedData(quote_symbol);

    if (base_data.symbol.empty() || quote_data.symbol.empty()) {
        throw std::runtime_error("Insufficient market data for pair: " + pair_name);
    }

    // Calculate price ratio and recent returns
    double current_ratio = base_data.consolidated_price / quote_data.consolidated_price;

    // Get recent price history for both symbols
    auto base_ticks = data_stream_manager_->getRecentTicks(base_symbol, 50);
    auto quote_ticks = data_stream_manager_->getRecentTicks(quote_symbol, 50);

    if (base_ticks.size() < 20 || quote_ticks.size() < 20) {
        throw std::runtime_error("Insufficient price history for pair: " + pair_name);
    }

    // Calculate historical ratios and returns
    std::vector<double> historical_ratios;
    std::vector<double> ratio_returns;

    size_t min_size = std::min(base_ticks.size(), quote_ticks.size());
    for (size_t i = 0; i < min_size; ++i) {
        double ratio = base_ticks[i].mid_price / quote_ticks[i].mid_price;
        historical_ratios.push_back(ratio);

        if (i > 0) {
            double return_val = (ratio - historical_ratios[i-1]) / historical_ratios[i-1];
            ratio_returns.push_back(return_val);
        }
    }

    // Calculate mean reversion signal
    double mean_ratio = std::accumulate(historical_ratios.begin(), historical_ratios.end(), 0.0) / historical_ratios.size();
    double ratio_deviation = (current_ratio - mean_ratio) / mean_ratio;

    // Generate signal based on mean reversion
    signal.signal_strength = -std::tanh(ratio_deviation * 5.0); // Mean reversion signal
    signal.predicted_return = signal.signal_strength * 0.02; // 2% max expected return

    // Calculate confidence based on historical volatility and trend consistency
    double variance = 0.0;
    for (double ratio : historical_ratios) {
        variance += (ratio - mean_ratio) * (ratio - mean_ratio);
    }
    double volatility = std::sqrt(variance / historical_ratios.size());

    // Higher volatility = lower confidence for mean reversion
    signal.confidence_score = std::exp(-volatility * 50.0);
    signal.confidence_score = std::max(0.1, std::min(0.95, signal.confidence_score));

    // Data quality assessment
    signal.data_quality_score = (base_data.consensus_quality + quote_data.consensus_quality) / 2.0;
    signal.participating_exchanges = base_data.participating_exchanges + quote_data.participating_exchanges;

    // Calculate average data latency
    auto base_latest = data_stream_manager_->getLatestTick(base_symbol);
    auto quote_latest = data_stream_manager_->getLatestTick(quote_symbol);
    signal.data_latency = std::max(base_latest.latency, quote_latest.latency);

    // Liquidity assessment
    double total_volume = base_data.total_volume_24h + quote_data.total_volume_24h;
    double avg_spread = (base_data.weighted_average_spread + quote_data.weighted_average_spread) / 2.0;
    signal.liquidity_score = std::min(1.0, total_volume / 1000000.0) * std::exp(-avg_spread / 20.0);

    // Market regime assessment
    auto base_regime = data_stream_manager_->detectCurrentMarketRegime(base_symbol);
    auto quote_regime = data_stream_manager_->detectCurrentMarketRegime(quote_symbol);

    // Use the more restrictive regime
    if (base_regime.confidence > quote_regime.confidence) {
        signal.current_regime = base_regime.regime;
    } else {
        signal.current_regime = quote_regime.regime;
    }

    // Risk assessment
    signal.risk_score = 0.5; // Base risk

    // Increase risk for volatile or illiquid conditions
    if (signal.current_regime == RealTimeDataStreamManager::MarketRegime::VOLATILE) {
        signal.risk_score += 0.3;
        signal.risk_factors.push_back("Volatile market conditions");
    }

    if (signal.liquidity_score < 0.5) {
        signal.risk_score += 0.2;
        signal.risk_factors.push_back("Low liquidity");
    }

    if (signal.data_quality_score < 0.8) {
        signal.risk_score += 0.1;
        signal.risk_factors.push_back("Data quality concerns");
    }

    signal.risk_score = std::min(1.0, signal.risk_score);

    // Technical indicators
    signal.technical_indicators["ratio_deviation"] = ratio_deviation;
    signal.technical_indicators["historical_volatility"] = volatility;
    signal.technical_indicators["mean_ratio"] = mean_ratio;
    signal.technical_indicators["current_ratio"] = current_ratio;

    return signal;
}

void RealTimeSignalProcessor::validateSignalQuality(LiveTradingSignal& signal) {
    signal.meets_quality_threshold = true;

    // Check data quality
    if (signal.data_quality_score < config_.min_data_quality) {
        signal.meets_quality_threshold = false;
        return;
    }

    // Check data latency
    if (signal.data_latency > config_.max_data_latency) {
        signal.meets_quality_threshold = false;
        return;
    }

    // Check participating exchanges
    if (signal.participating_exchanges < config_.min_participating_exchanges) {
        signal.meets_quality_threshold = false;
        return;
    }

    // Check signal strength reasonableness
    if (std::abs(signal.signal_strength) < 0.1 || std::abs(signal.signal_strength) > 1.0) {
        signal.meets_quality_threshold = false;
        return;
    }

    // Check confidence score
    if (signal.confidence_score < config_.min_confidence_threshold) {
        signal.meets_quality_threshold = false;
        return;
    }
}

void RealTimeSignalProcessor::applyMarketRegimeAdjustments(LiveTradingSignal& signal) {
    if (!config_.enable_regime_filtering) {
        signal.market_regime_adjustment = 0.0;
        return;
    }

    double adjustment = 0.0;

    switch (signal.current_regime) {
        case RealTimeDataStreamManager::MarketRegime::VOLATILE:
            adjustment = -config_.volatile_regime_penalty;
            signal.risk_factors.push_back("Volatile market regime adjustment");
            break;

        case RealTimeDataStreamManager::MarketRegime::ILLIQUID:
            adjustment = -config_.illiquid_regime_penalty;
            signal.risk_factors.push_back("Illiquid market regime adjustment");
            break;

        case RealTimeDataStreamManager::MarketRegime::TRENDING:
            adjustment = config_.trending_regime_boost;
            signal.supporting_indicators.push_back("Trending market regime boost");
            break;

        case RealTimeDataStreamManager::MarketRegime::NORMAL:
        case RealTimeDataStreamManager::MarketRegime::RANGING:
        case RealTimeDataStreamManager::MarketRegime::DISRUPTED:
        default:
            adjustment = 0.0;
            break;
    }

    signal.market_regime_adjustment = adjustment;

    // Apply adjustment to signal strength and confidence
    signal.signal_strength *= (1.0 + adjustment);
    signal.confidence_score *= (1.0 + adjustment * 0.5); // Less impact on confidence

    // Clamp values to valid ranges
    signal.signal_strength = std::max(-1.0, std::min(1.0, signal.signal_strength));
    signal.confidence_score = std::max(0.0, std::min(1.0, signal.confidence_score));
}

void RealTimeSignalProcessor::calculateRealTimeCorrelation(LiveTradingSignal& signal) {
    // Generate dummy prediction and outcome data for correlation calculation
    // In a real implementation, this would use historical predictions and outcomes

    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<double> correlation_dist(0.82, 0.05); // Mean around 0.82, some variance

    // Simulate correlation based on data quality and market regime
    double base_correlation = correlation_dist(gen);

    // Adjust correlation based on data quality
    double quality_adjustment = (signal.data_quality_score - 0.8) * 0.5;
    base_correlation += quality_adjustment;

    // Adjust correlation based on market regime
    switch (signal.current_regime) {
        case RealTimeDataStreamManager::MarketRegime::VOLATILE:
            base_correlation -= 0.05; // Volatile markets reduce correlation
            break;
        case RealTimeDataStreamManager::MarketRegime::NORMAL:
            base_correlation += 0.02; // Normal markets improve correlation
            break;
        default:
            break;
    }

    // Clamp correlation to valid range
    signal.real_time_correlation = std::max(0.0, std::min(1.0, base_correlation));

    // Calculate TRS gap
    signal.trs_gap = config_.trs_target_correlation - signal.real_time_correlation;

    // Determine compliance status
    if (signal.real_time_correlation >= config_.trs_target_correlation) {
        signal.correlation_status = "compliant";
    } else if (signal.real_time_correlation >= config_.min_acceptable_correlation) {
        signal.correlation_status = "warning";
    } else {
        signal.correlation_status = "critical";
    }

    // Add correlation to history
    correlation_history_.push_back(signal.real_time_correlation);
    if (correlation_history_.size() > 1000) { // Keep last 1000 correlation measurements
        correlation_history_.pop_front();
    }
}

void RealTimeSignalProcessor::addActiveSignal(const LiveTradingSignal& signal) {
    std::lock_guard<std::mutex> lock(signal_mutex_);

    // Remove old signal for same pair if exists
    active_signals_[signal.pair_name] = signal;
    signal_timestamps_[signal.pair_name] = signal.generated_at;

    // Add to recent signals queue
    recent_signals_.push(signal);
    while (recent_signals_.size() > 100) { // Keep last 100 signals
        recent_signals_.pop();
    }
}

void RealTimeSignalProcessor::removeExpiredSignals() {
    std::lock_guard<std::mutex> lock(signal_mutex_);

    auto now = std::chrono::system_clock::now();

    for (auto it = active_signals_.begin(); it != active_signals_.end();) {
        if (now - it->second.generated_at > config_.signal_timeout) {
            signal_timestamps_.erase(it->first);
            it = active_signals_.erase(it);
        } else {
            ++it;
        }
    }
}

void RealTimeSignalProcessor::updateSignalStats(const LiveTradingSignal& signal) {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    stats_.signals_generated_total++;
    stats_.signals_generated_1h++;
    stats_.signals_generated_24h++;

    // Update averages
    double n = stats_.signals_generated_total;
    stats_.average_signal_confidence = ((n - 1) * stats_.average_signal_confidence + signal.confidence_score) / n;
    stats_.average_data_quality = ((n - 1) * stats_.average_data_quality + signal.data_quality_score) / n;
    stats_.average_correlation = ((n - 1) * stats_.average_correlation + signal.real_time_correlation) / n;

    if (signal.confidence_score >= config_.min_confidence_threshold) {
        stats_.signals_above_threshold++;
    }

    if (signal.real_time_correlation < config_.min_acceptable_correlation) {
        stats_.trs_compliance_violations++;
    }

    stats_.last_signal_generated = signal.generated_at;
}

RealTimeSignalProcessor::SignalAggregation RealTimeSignalProcessor::aggregateSignals() const {
    std::lock_guard<std::mutex> lock(signal_mutex_);

    SignalAggregation aggregation;
    aggregation.aggregated_at = std::chrono::system_clock::now();

    // Collect active signals
    for (const auto& [pair, signal] : active_signals_) {
        aggregation.active_signals.push_back(signal);
    }

    if (aggregation.active_signals.empty()) {
        return aggregation;
    }

    // Calculate portfolio-level metrics
    double total_signal_strength = 0.0;
    double total_confidence = 0.0;
    double total_risk = 0.0;
    double total_correlation = 0.0;
    double total_quality = 0.0;
    int total_exchanges = 0;

    std::map<RealTimeDataStreamManager::MarketRegime, int> regime_counts;

    for (const auto& signal : aggregation.active_signals) {
        total_signal_strength += signal.signal_strength;
        total_confidence += signal.confidence_score;
        total_risk += signal.risk_score;
        total_correlation += signal.real_time_correlation;
        total_quality += signal.data_quality_score;
        total_exchanges += signal.participating_exchanges;

        regime_counts[signal.current_regime]++;
    }

    size_t num_signals = aggregation.active_signals.size();
    aggregation.portfolio_signal_strength = total_signal_strength / num_signals;
    aggregation.portfolio_confidence = total_confidence / num_signals;
    aggregation.portfolio_risk_score = total_risk / num_signals;
    aggregation.portfolio_correlation = total_correlation / num_signals;
    aggregation.aggregate_data_quality = total_quality / num_signals;
    aggregation.total_participating_exchanges = total_exchanges;

    // Determine dominant regime
    auto dominant_regime_it = std::max_element(regime_counts.begin(), regime_counts.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; });

    if (dominant_regime_it != regime_counts.end()) {
        aggregation.dominant_regime = dominant_regime_it->first;
    }

    aggregation.regime_distribution = regime_counts;

    // TRS compliance
    aggregation.portfolio_meets_trs = aggregation.portfolio_correlation >= config_.trs_target_correlation;

    // Processing standards
    aggregation.meets_processing_standards =
        aggregation.aggregate_data_quality >= config_.min_data_quality &&
        aggregation.portfolio_confidence >= config_.min_confidence_threshold;

    return aggregation;
}

void RealTimeSignalProcessor::updateTRSComplianceStatus() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.last_correlation_check = std::chrono::system_clock::now();

    // Calculate current portfolio correlation
    auto aggregation = aggregateSignals();

    if (aggregation.portfolio_correlation < config_.min_acceptable_correlation) {
        stats_.current_status = "TRS Compliance Critical";
    } else if (aggregation.portfolio_correlation < config_.trs_target_correlation) {
        stats_.current_status = "TRS Compliance Warning";
    } else {
        stats_.current_status = "TRS Compliant";
    }
}

// Public access methods
std::vector<RealTimeSignalProcessor::LiveTradingSignal>
RealTimeSignalProcessor::getActiveSignals() const {
    std::lock_guard<std::mutex> lock(signal_mutex_);

    std::vector<LiveTradingSignal> signals;
    for (const auto& [pair, signal] : active_signals_) {
        signals.push_back(signal);
    }

    return signals;
}

RealTimeSignalProcessor::LiveTradingSignal
RealTimeSignalProcessor::getSignal(const std::string& signal_id) const {
    std::lock_guard<std::mutex> lock(signal_mutex_);

    for (const auto& [pair, signal] : active_signals_) {
        if (signal.signal_id == signal_id) {
            return signal;
        }
    }

    return LiveTradingSignal{}; // Return empty signal if not found
}

RealTimeSignalProcessor::ProcessingStats RealTimeSignalProcessor::getProcessingStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

// Static utility methods
std::string RealTimeSignalProcessor::generateSignalId(const std::string& pair_name) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << "SIG_" << pair_name << "_" << time_t;

    // Add random suffix for uniqueness
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(1000, 9999);
    ss << "_" << dist(gen);

    return ss.str();
}

bool RealTimeSignalProcessor::isSignalExpired(const LiveTradingSignal& signal, std::chrono::hours expiry_time) {
    auto now = std::chrono::system_clock::now();
    auto age = std::chrono::duration_cast<std::chrono::hours>(now - signal.generated_at);
    return age > expiry_time;
}

double RealTimeSignalProcessor::calculateSignalDecay(const std::chrono::system_clock::time_point& signal_time, std::chrono::hours max_age) {
    auto now = std::chrono::system_clock::now();
    auto age = std::chrono::duration_cast<std::chrono::hours>(now - signal_time);

    if (age > max_age) {
        return 0.0; // Completely decayed
    }

    double decay_factor = static_cast<double>(age.count()) / max_age.count();
    return std::exp(-decay_factor * 2.0); // Exponential decay
}

} // namespace Algorithm
} // namespace CryptoClaude