#include "CrossAssetCorrelationMonitor.h"
#include "../Database/DatabaseManager.h"
#include "../Http/HttpClient.h"
#include <algorithm>
#include <random>
#include <thread>
#include <iostream>

namespace CryptoClaude {
namespace Analytics {

// TraditionalAssetIntegration Implementation
TraditionalAssetIntegration::TraditionalAssetIntegration() {
    // Initialize with empty cache
}

TraditionalAssetIntegration::~TraditionalAssetIntegration() = default;

bool TraditionalAssetIntegration::updateTraditionalAssetData() {
    bool overall_success = true;

    for (const std::string& symbol : supported_assets_) {
        if (!fetchAssetDataFromAPI(symbol)) {
            overall_success = false;
            std::cerr << "Failed to update data for " << symbol << std::endl;
        }
    }

    return overall_success;
}

std::vector<TraditionalAssetDataPoint> TraditionalAssetIntegration::getAssetData(
    const std::string& symbol, int days) {

    auto it = asset_data_cache_.find(symbol);
    if (it == asset_data_cache_.end()) {
        return {};
    }

    auto& all_data = it->second;
    if (all_data.empty()) return {};

    // Return most recent 'days' worth of data
    int start_index = std::max(0, static_cast<int>(all_data.size()) - days);
    return std::vector<TraditionalAssetDataPoint>(
        all_data.begin() + start_index, all_data.end());
}

std::map<std::string, std::vector<double>> TraditionalAssetIntegration::getReturnsData(
    const std::vector<std::string>& symbols, int days) {

    std::map<std::string, std::vector<double>> returns_data;

    for (const std::string& symbol : symbols) {
        auto asset_data = getAssetData(symbol, days + 1); // Need +1 for return calculation
        if (asset_data.size() > 1) {
            returns_data[symbol] = calculateReturns(asset_data);
        }
    }

    return returns_data;
}

std::vector<std::string> TraditionalAssetIntegration::getSupportedAssets() const {
    return supported_assets_;
}

bool TraditionalAssetIntegration::isAssetSupported(const std::string& symbol) const {
    return std::find(supported_assets_.begin(), supported_assets_.end(), symbol) != supported_assets_.end();
}

std::chrono::system_clock::time_point TraditionalAssetIntegration::getLastUpdate(
    const std::string& symbol) const {
    auto it = last_update_times_.find(symbol);
    return it != last_update_times_.end() ? it->second : std::chrono::system_clock::time_point{};
}

double TraditionalAssetIntegration::getDataQuality(const std::string& symbol) const {
    auto it = asset_data_cache_.find(symbol);
    if (it == asset_data_cache_.end() || it->second.empty()) return 0.0;

    // Simple quality metric based on data completeness and freshness
    double completeness = std::min(1.0, static_cast<double>(it->second.size()) / 30.0);
    double freshness = isDataStale(symbol, 120) ? 0.5 : 1.0; // 2 hour staleness penalty
    return completeness * freshness;
}

bool TraditionalAssetIntegration::isDataStale(const std::string& symbol, int max_age_minutes) const {
    auto last_update = getLastUpdate(symbol);
    if (last_update == std::chrono::system_clock::time_point{}) return true;

    auto now = std::chrono::system_clock::now();
    auto age = std::chrono::duration_cast<std::chrono::minutes>(now - last_update);
    return age.count() > max_age_minutes;
}

bool TraditionalAssetIntegration::fetchAssetDataFromAPI(const std::string& symbol) {
    // For demonstration, generate synthetic but realistic data
    // In production, this would integrate with real traditional asset APIs

    std::random_device rd;
    std::mt19937 gen(rd());

    // Generate realistic synthetic data based on symbol
    std::vector<TraditionalAssetDataPoint> new_data;
    auto now = std::chrono::system_clock::now();

    double base_price = 100.0;
    double volatility = 0.02;

    if (symbol == "SPX") { base_price = 4500.0; volatility = 0.015; }
    else if (symbol == "GLD") { base_price = 180.0; volatility = 0.018; }
    else if (symbol == "DXY") { base_price = 103.0; volatility = 0.008; }
    else if (symbol == "VIX") { base_price = 18.0; volatility = 0.25; }
    else if (symbol == "TLT") { base_price = 95.0; volatility = 0.012; }
    else if (symbol == "HYG") { base_price = 82.0; volatility = 0.010; }

    std::normal_distribution<double> price_dist(0.0, volatility);

    // Generate 30 days of synthetic data
    for (int i = 30; i >= 0; --i) {
        TraditionalAssetDataPoint data_point;
        data_point.symbol = symbol;
        data_point.timestamp = now - std::chrono::hours(24 * i);

        // Random walk for price
        double return_val = price_dist(gen);
        data_point.price = base_price * std::exp(return_val);
        data_point.change_24h = return_val;
        data_point.volume = 1000000 + (gen() % 2000000); // Synthetic volume
        data_point.source = "synthetic_feed";

        new_data.push_back(data_point);
        base_price = data_point.price;
    }

    // Update cache
    asset_data_cache_[symbol] = new_data;
    last_update_times_[symbol] = now;

    return true;
}

std::vector<double> TraditionalAssetIntegration::calculateReturns(
    const std::vector<TraditionalAssetDataPoint>& data) {

    if (data.size() < 2) return {};

    std::vector<double> returns;
    returns.reserve(data.size() - 1);

    for (size_t i = 1; i < data.size(); ++i) {
        if (data[i-1].price > 0) {
            double return_val = (data[i].price - data[i-1].price) / data[i-1].price;
            returns.push_back(return_val);
        } else {
            returns.push_back(0.0);
        }
    }

    return returns;
}

// CorrelationStressDetector Implementation
CorrelationStressDetector::CorrelationStressDetector(double spike_threshold, double regime_threshold)
    : spike_threshold_(spike_threshold), regime_threshold_(regime_threshold) {
}

CorrelationStressDetector::~CorrelationStressDetector() = default;

std::vector<MarketStressSignal> CorrelationStressDetector::detectStressSignals(
    const std::vector<CorrelationSnapshot>& correlations) {

    std::vector<MarketStressSignal> signals;

    for (const auto& correlation : correlations) {
        // Check for correlation spikes
        if (correlation.correlation_spike) {
            MarketStressSignal signal = createStressSignal(
                "correlation_spike",
                {correlation.pair.crypto_symbol + "/" + correlation.pair.traditional_symbol},
                std::abs(correlation.z_score) / 5.0 // Normalize to 0-1
            );
            signal.portfolio_risk_multiplier = 1.0 + (std::abs(correlation.z_score) * 0.1);
            signals.push_back(signal);
        }

        // Check for regime changes
        if (correlation.regime_change) {
            MarketStressSignal signal = createStressSignal(
                "regime_change",
                {correlation.pair.crypto_symbol + "/" + correlation.pair.traditional_symbol},
                std::abs(correlation.correlation_change_24h) / 0.5 // Normalize large changes
            );
            signal.portfolio_risk_multiplier = 1.0 + (std::abs(correlation.correlation_change_24h) * 0.2);
            signals.push_back(signal);
        }
    }

    return signals;
}

bool CorrelationStressDetector::isCorrelationSpike(
    const CorrelationSnapshot& current, const CorrelationSnapshot& previous) {

    double correlation_change = std::abs(current.correlation - previous.correlation);
    return correlation_change > 0.3; // Significant correlation change threshold
}

bool CorrelationStressDetector::isRegimeChange(
    const std::vector<CorrelationSnapshot>& correlation_history) {

    if (correlation_history.size() < 10) return false;

    // Compare recent vs historical average correlation
    double recent_avg = 0.0;
    double historical_avg = 0.0;

    int recent_days = 5;
    for (int i = 0; i < recent_days && i < correlation_history.size(); ++i) {
        recent_avg += correlation_history[correlation_history.size() - 1 - i].correlation;
    }
    recent_avg /= std::min(recent_days, static_cast<int>(correlation_history.size()));

    for (const auto& snapshot : correlation_history) {
        historical_avg += snapshot.correlation;
    }
    historical_avg /= correlation_history.size();

    return std::abs(recent_avg - historical_avg) > regime_threshold_;
}

double CorrelationStressDetector::calculateMarketStress(
    const std::vector<CorrelationSnapshot>& correlations) {

    if (correlations.empty()) return 0.0;

    double stress_sum = 0.0;
    int stress_count = 0;

    for (const auto& correlation : correlations) {
        if (correlation.correlation_spike || correlation.regime_change) {
            stress_sum += std::abs(correlation.z_score) / 5.0; // Normalize
            stress_count++;
        }
    }

    // Return average stress level, capped at 1.0
    return stress_count > 0 ? std::min(1.0, stress_sum / stress_count) : 0.0;
}

std::vector<std::string> CorrelationStressDetector::generateRiskRecommendations(
    const std::vector<MarketStressSignal>& signals) {

    std::vector<std::string> recommendations;

    if (signals.empty()) {
        recommendations.push_back("Normal market conditions - maintain current risk posture");
        return recommendations;
    }

    double avg_stress = 0.0;
    for (const auto& signal : signals) {
        avg_stress += signal.stress_magnitude;
    }
    avg_stress /= signals.size();

    if (avg_stress > 0.7) {
        recommendations.push_back("High stress detected - consider reducing position sizes by 30%");
        recommendations.push_back("Increase diversification across uncorrelated assets");
        recommendations.push_back("Consider increasing cash allocation temporarily");
    } else if (avg_stress > 0.4) {
        recommendations.push_back("Moderate stress detected - monitor positions closely");
        recommendations.push_back("Consider reducing leverage on highly correlated positions");
    } else {
        recommendations.push_back("Low stress conditions - maintain current strategy");
    }

    return recommendations;
}

double CorrelationStressDetector::calculateCorrelationZScore(
    double current, const std::vector<double>& history) {

    if (history.size() < 5) return 0.0;

    double mean = 0.0;
    for (double val : history) {
        mean += val;
    }
    mean /= history.size();

    double variance = 0.0;
    for (double val : history) {
        variance += (val - mean) * (val - mean);
    }
    variance /= (history.size() - 1);
    double std_dev = std::sqrt(variance);

    return std_dev > 0 ? (current - mean) / std_dev : 0.0;
}

MarketStressSignal CorrelationStressDetector::createStressSignal(
    const std::string& type, const std::vector<std::string>& pairs, double magnitude) {

    MarketStressSignal signal;
    signal.timestamp = std::chrono::system_clock::now();
    signal.stress_type = type;
    signal.affected_pairs = pairs;
    signal.stress_magnitude = std::min(1.0, magnitude);
    signal.portfolio_risk_multiplier = 1.0;

    if (type == "correlation_spike") {
        signal.description = "Sudden correlation change detected in " + pairs[0];
        signal.recommended_actions = {"Monitor position closely", "Consider risk reduction"};
    } else if (type == "regime_change") {
        signal.description = "Correlation regime change detected in " + pairs[0];
        signal.recommended_actions = {"Review portfolio allocation", "Assess diversification"};
    }

    return signal;
}

// CrossAssetCorrelationMonitor Implementation
CrossAssetCorrelationMonitor::CrossAssetCorrelationMonitor()
    : monitoring_active_(false), current_market_stress_(0.0) {

    traditional_assets_ = std::make_unique<TraditionalAssetIntegration>();
    stress_detector_ = std::make_unique<CorrelationStressDetector>();
}

CrossAssetCorrelationMonitor::~CrossAssetCorrelationMonitor() = default;

bool CrossAssetCorrelationMonitor::initialize() {
    // Initialize default cross-asset pairs
    initializeDefaultPairs();

    // Initialize traditional asset data
    if (!traditional_assets_->updateTraditionalAssetData()) {
        std::cerr << "Warning: Failed to initialize traditional asset data" << std::endl;
    }

    monitoring_active_ = true;
    last_update_ = std::chrono::system_clock::now();

    // Initialize performance metrics
    performance_metrics_.update_frequency_hz = 0.0;
    performance_metrics_.avg_calculation_time_ms = 0.0;
    performance_metrics_.successful_updates_24h = 0;
    performance_metrics_.failed_updates_24h = 0;
    performance_metrics_.data_quality_score = 0.8;

    return true;
}

void CrossAssetCorrelationMonitor::addCrossAssetPair(
    const std::string& crypto, const std::string& traditional, const std::string& description) {

    monitored_pairs_.emplace_back(crypto, traditional, description);
}

void CrossAssetCorrelationMonitor::removeCrossAssetPair(
    const std::string& crypto, const std::string& traditional) {

    monitored_pairs_.erase(
        std::remove_if(monitored_pairs_.begin(), monitored_pairs_.end(),
            [&](const CrossAssetPair& pair) {
                return pair.crypto_symbol == crypto && pair.traditional_symbol == traditional;
            }),
        monitored_pairs_.end());
}

bool CrossAssetCorrelationMonitor::updateCorrelations() {
    if (!monitoring_active_) return false;

    auto start_time = std::chrono::high_resolution_clock::now();
    bool success = true;

    try {
        // Update traditional asset data
        traditional_assets_->updateTraditionalAssetData();

        // Calculate correlations for all pairs
        for (const auto& pair : monitored_pairs_) {
            try {
                auto correlation = calculatePairCorrelation(pair);
                updateCorrelationHistory(correlation);
            } catch (const std::exception& e) {
                std::cerr << "Failed to calculate correlation for "
                         << pair.crypto_symbol << "/" << pair.traditional_symbol
                         << ": " << e.what() << std::endl;
                success = false;
            }
        }

        // Update stress signals and market stress
        updateStressSignals();
        calculateMarketStress();

        last_update_ = std::chrono::system_clock::now();

        // Update performance metrics
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        performance_metrics_.avg_calculation_time_ms = duration.count() / 1000.0;

        if (success) {
            performance_metrics_.successful_updates_24h++;
        } else {
            performance_metrics_.failed_updates_24h++;
        }

    } catch (const std::exception& e) {
        std::cerr << "Error updating correlations: " << e.what() << std::endl;
        success = false;
        performance_metrics_.failed_updates_24h++;
    }

    return success;
}

std::vector<CorrelationSnapshot> CrossAssetCorrelationMonitor::getCurrentCorrelations() const {
    std::vector<CorrelationSnapshot> current_correlations;

    for (const auto& pair_history : correlation_history_) {
        if (!pair_history.second.empty()) {
            current_correlations.push_back(pair_history.second.back());
        }
    }

    return current_correlations;
}

std::vector<CorrelationSnapshot> CrossAssetCorrelationMonitor::getCorrelationHistory(
    const CrossAssetPair& pair, int days) const {

    std::string pair_key = pair.crypto_symbol + "/" + pair.traditional_symbol;
    auto it = correlation_history_.find(pair_key);

    if (it == correlation_history_.end()) return {};

    const auto& history = it->second;
    if (history.empty()) return {};

    // Return most recent 'days' worth of data
    int start_index = std::max(0, static_cast<int>(history.size()) - days);
    return std::vector<CorrelationSnapshot>(history.begin() + start_index, history.end());
}

std::vector<MarketStressSignal> CrossAssetCorrelationMonitor::getCurrentStressSignals() const {
    return current_stress_signals_;
}

double CrossAssetCorrelationMonitor::getCurrentMarketStress() const {
    return current_market_stress_;
}

CorrelationSnapshot CrossAssetCorrelationMonitor::calculateCorrelation(const CrossAssetPair& pair) {
    return calculatePairCorrelation(pair);
}

std::map<std::string, double> CrossAssetCorrelationMonitor::getCryptoTraditionalCorrelations(
    const std::string& crypto_symbol) {

    std::map<std::string, double> correlations;

    for (const auto& pair : monitored_pairs_) {
        if (pair.crypto_symbol == crypto_symbol) {
            try {
                auto correlation = calculatePairCorrelation(pair);
                correlations[pair.traditional_symbol] = correlation.correlation;
            } catch (const std::exception& e) {
                std::cerr << "Failed to calculate correlation: " << e.what() << std::endl;
            }
        }
    }

    return correlations;
}

double CrossAssetCorrelationMonitor::getPortfolioRiskAdjustment() const {
    double max_adjustment = 1.0;

    for (const auto& signal : current_stress_signals_) {
        max_adjustment = std::max(max_adjustment, signal.portfolio_risk_multiplier);
    }

    return max_adjustment;
}

std::vector<std::string> CrossAssetCorrelationMonitor::getRiskRecommendations() const {
    return stress_detector_->generateRiskRecommendations(current_stress_signals_);
}

CrossAssetCorrelationMonitor::PerformanceMetrics CrossAssetCorrelationMonitor::getPerformanceMetrics() const {
    return performance_metrics_;
}

void CrossAssetCorrelationMonitor::initializeDefaultPairs() {
    monitored_pairs_ = CrossAssetUtils::DEFAULT_PAIRS;
}

std::vector<double> CrossAssetCorrelationMonitor::getCryptoReturns(
    const std::string& symbol, int days) {

    // In production, this would fetch from DatabaseManager
    // For now, generate synthetic crypto returns
    std::vector<double> returns;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<double> dist(0.0, 0.04); // 4% daily vol

    for (int i = 0; i < days; ++i) {
        returns.push_back(dist(gen));
    }

    return returns;
}

std::vector<double> CrossAssetCorrelationMonitor::getTraditionalReturns(
    const std::string& symbol, int days) {

    auto returns_data = traditional_assets_->getReturnsData({symbol}, days);
    auto it = returns_data.find(symbol);
    return it != returns_data.end() ? it->second : std::vector<double>{};
}

CorrelationSnapshot CrossAssetCorrelationMonitor::calculatePairCorrelation(const CrossAssetPair& pair) {
    CorrelationSnapshot snapshot;
    snapshot.pair = pair;
    snapshot.timestamp = std::chrono::system_clock::now();

    // Get returns data
    auto crypto_returns = getCryptoReturns(pair.crypto_symbol, 30);
    auto traditional_returns = getTraditionalReturns(pair.traditional_symbol, 30);

    if (!validateCorrelationData(crypto_returns, traditional_returns)) {
        throw std::runtime_error("Invalid correlation data for pair: "
                                + pair.crypto_symbol + "/" + pair.traditional_symbol);
    }

    // Calculate correlation at different timeframes
    snapshot.correlation_30d = StatisticalTools::calculateCorrelation(crypto_returns, traditional_returns);

    // Calculate shorter timeframes
    if (crypto_returns.size() >= 7 && traditional_returns.size() >= 7) {
        std::vector<double> crypto_7d(crypto_returns.end() - 7, crypto_returns.end());
        std::vector<double> traditional_7d(traditional_returns.end() - 7, traditional_returns.end());
        snapshot.correlation_7d = StatisticalTools::calculateCorrelation(crypto_7d, traditional_7d);
    }

    if (crypto_returns.size() >= 3 && traditional_returns.size() >= 3) {
        std::vector<double> crypto_3d(crypto_returns.end() - 3, crypto_returns.end());
        std::vector<double> traditional_3d(traditional_returns.end() - 3, traditional_returns.end());
        snapshot.correlation_3d = StatisticalTools::calculateCorrelation(crypto_3d, traditional_3d);
    }

    // Use 30-day correlation as primary
    snapshot.correlation = snapshot.correlation_30d;
    snapshot.sample_size = std::min(crypto_returns.size(), traditional_returns.size());
    snapshot.p_value = 0.05; // Simplified p-value calculation
    snapshot.is_significant = std::abs(snapshot.correlation) > 0.3; // Simple significance threshold

    // Calculate 24h correlation change
    std::string pair_key = pair.crypto_symbol + "/" + pair.traditional_symbol;
    auto history_it = correlation_history_.find(pair_key);
    if (history_it != correlation_history_.end() && !history_it->second.empty()) {
        auto& previous = history_it->second.back();
        snapshot.correlation_change_24h = snapshot.correlation - previous.correlation;

        // Calculate z-score for stress detection
        std::vector<double> correlation_values;
        for (const auto& hist_snapshot : history_it->second) {
            correlation_values.push_back(hist_snapshot.correlation);
        }
        snapshot.z_score = stress_detector_->calculateCorrelationZScore(
            snapshot.correlation, correlation_values);
    }

    // Determine stress indicators
    snapshot.correlation_spike = std::abs(snapshot.z_score) > 2.0;
    snapshot.regime_change = std::abs(snapshot.correlation_change_24h) > 0.3;

    return snapshot;
}

void CrossAssetCorrelationMonitor::updateCorrelationHistory(const CorrelationSnapshot& snapshot) {
    std::string pair_key = snapshot.pair.crypto_symbol + "/" + snapshot.pair.traditional_symbol;
    auto& history = correlation_history_[pair_key];

    history.push_back(snapshot);

    // Keep only recent history (e.g., 100 days)
    if (history.size() > 100) {
        history.erase(history.begin());
    }
}

void CrossAssetCorrelationMonitor::updateStressSignals() {
    auto current_correlations = getCurrentCorrelations();
    current_stress_signals_ = stress_detector_->detectStressSignals(current_correlations);
}

void CrossAssetCorrelationMonitor::calculateMarketStress() {
    auto current_correlations = getCurrentCorrelations();
    current_market_stress_ = stress_detector_->calculateMarketStress(current_correlations);
}

bool CrossAssetCorrelationMonitor::validateCorrelationData(
    const std::vector<double>& crypto_returns, const std::vector<double>& traditional_returns) {

    if (crypto_returns.empty() || traditional_returns.empty()) return false;
    if (crypto_returns.size() != traditional_returns.size()) return false;
    if (crypto_returns.size() < 5) return false; // Minimum data points

    // Check for valid numbers
    for (double val : crypto_returns) {
        if (!std::isfinite(val)) return false;
    }
    for (double val : traditional_returns) {
        if (!std::isfinite(val)) return false;
    }

    return true;
}

// CrossAssetUtils Implementation
namespace CrossAssetUtils {

std::string interpretCorrelation(double correlation) {
    if (correlation > 0.7) return "Strong Positive";
    if (correlation > 0.3) return "Moderate Positive";
    if (correlation > 0.1) return "Weak Positive";
    if (correlation > -0.1) return "No Correlation";
    if (correlation > -0.3) return "Weak Negative";
    if (correlation > -0.7) return "Moderate Negative";
    return "Strong Negative";
}

std::string getMarketRegime(double btc_spx_corr, double btc_vix_corr) {
    if (btc_spx_corr > 0.5 && btc_vix_corr > 0.3) {
        return "Risk-Off (High Correlation)";
    } else if (btc_spx_corr < -0.3) {
        return "Crypto Decoupling";
    } else if (std::abs(btc_spx_corr) < 0.2) {
        return "Normal Regime";
    } else {
        return "Mixed Signals";
    }
}

std::vector<std::string> generateCorrelationInsights(
    const std::vector<CorrelationSnapshot>& correlations) {

    std::vector<std::string> insights;

    if (correlations.empty()) {
        insights.push_back("No correlation data available");
        return insights;
    }

    // Find highest correlations
    double max_correlation = -1.0;
    std::string max_pair;

    for (const auto& corr : correlations) {
        if (std::abs(corr.correlation) > std::abs(max_correlation)) {
            max_correlation = corr.correlation;
            max_pair = corr.pair.crypto_symbol + "/" + corr.pair.traditional_symbol;
        }
    }

    insights.push_back("Highest correlation: " + max_pair + " (" +
                      std::to_string(max_correlation) + ")");

    // Check for stress signals
    int stress_count = 0;
    for (const auto& corr : correlations) {
        if (corr.correlation_spike || corr.regime_change) {
            stress_count++;
        }
    }

    if (stress_count > 0) {
        insights.push_back("Market stress detected in " + std::to_string(stress_count) + " pairs");
    } else {
        insights.push_back("Correlations stable - normal market conditions");
    }

    return insights;
}

double calculateCorrelationRiskAdjustment(const std::vector<CorrelationSnapshot>& correlations) {
    if (correlations.empty()) return 1.0;

    double max_risk_adjustment = 1.0;

    for (const auto& corr : correlations) {
        if (corr.correlation_spike || corr.regime_change) {
            double risk_multiplier = 1.0 + (std::abs(corr.z_score) * 0.1);
            max_risk_adjustment = std::max(max_risk_adjustment, risk_multiplier);
        }
    }

    return std::min(2.0, max_risk_adjustment); // Cap at 2x risk adjustment
}

std::map<std::string, double> getAssetCorrelationWeights(
    const std::vector<CorrelationSnapshot>& correlations,
    const std::vector<std::string>& portfolio_assets) {

    std::map<std::string, double> weights;

    // Initialize equal weights
    double equal_weight = 1.0 / portfolio_assets.size();
    for (const std::string& asset : portfolio_assets) {
        weights[asset] = equal_weight;
    }

    // Adjust weights based on correlations
    for (const auto& corr : correlations) {
        auto it = std::find(portfolio_assets.begin(), portfolio_assets.end(),
                          corr.pair.crypto_symbol);
        if (it != portfolio_assets.end()) {
            // Reduce weight if high correlation with traditional assets (concentration risk)
            if (std::abs(corr.correlation) > 0.7) {
                weights[corr.pair.crypto_symbol] *= 0.8; // 20% reduction
            }
        }
    }

    // Normalize weights to sum to 1.0
    double total_weight = 0.0;
    for (const auto& weight : weights) {
        total_weight += weight.second;
    }

    if (total_weight > 0) {
        for (auto& weight : weights) {
            weight.second /= total_weight;
        }
    }

    return weights;
}

} // namespace CrossAssetUtils

} // namespace Analytics
} // namespace CryptoClaude