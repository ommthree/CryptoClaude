#include "EnhancedSignalProcessor.h"
#include "../Analytics/StatisticalTools.h"
#include <algorithm>
#include <cmath>
#include <random>

namespace CryptoClaude {
namespace Strategy {

EnhancedSignalProcessor::EnhancedSignalProcessor()
    : minimumSignalQuality_(SignalQuality::LOW),
      maximumSignalsPerSymbol_(50),
      marketRegimeDetectionEnabled_(false),
      volatilityAdjustmentEnabled_(true) {

    // Initialize default thresholds
    signalThresholds_[SignalQuality::EXCELLENT] = 0.95;
    signalThresholds_[SignalQuality::GOOD] = 0.80;
    signalThresholds_[SignalQuality::MEDIUM] = 0.60;
    signalThresholds_[SignalQuality::LOW] = 0.40;
}

void EnhancedSignalProcessor::setMinimumSignalQuality(SignalQuality quality) {
    minimumSignalQuality_ = quality;
}

void EnhancedSignalProcessor::setMaximumSignalsPerSymbol(int maxSignals) {
    maximumSignalsPerSymbol_ = maxSignals;
}

void EnhancedSignalProcessor::enableMarketRegimeDetection(bool enable) {
    marketRegimeDetectionEnabled_ = enable;
}

void EnhancedSignalProcessor::enableVolatilityAdjustment(bool enable) {
    volatilityAdjustmentEnabled_ = enable;
}

std::vector<EnhancedTradingSignal> EnhancedSignalProcessor::generateSignals(
    const std::string& symbol,
    const std::vector<MarketData>& marketData) {

    std::vector<EnhancedTradingSignal> signals;

    if (marketData.size() < 20) {
        // Insufficient data for signal generation
        return signals;
    }

    // Extract price data
    std::vector<double> prices;
    std::vector<double> volumes;

    for (const auto& data : marketData) {
        prices.push_back(data.getPrice());
        volumes.push_back(data.getVolume());
    }

    // Generate technical signals
    auto technicalSignals = generateTechnicalSignals(symbol, prices, volumes);
    signals.insert(signals.end(), technicalSignals.begin(), technicalSignals.end());

    // Generate statistical signals
    auto statisticalSignals = generateStatisticalSignals(symbol, prices);
    signals.insert(signals.end(), statisticalSignals.begin(), statisticalSignals.end());

    // Apply market regime detection if enabled
    if (marketRegimeDetectionEnabled_) {
        auto currentRegime = detectMarketRegime(prices);
        adjustSignalsForRegime(signals, currentRegime);
    }

    // Apply volatility adjustment if enabled
    if (volatilityAdjustmentEnabled_) {
        double volatility = calculateVolatility(prices);
        adjustSignalsForVolatility(signals, volatility);
    }

    // Filter signals by quality threshold
    filterSignalsByQuality(signals);

    // Limit number of signals per symbol
    limitSignalsPerSymbol(signals);

    return signals;
}

std::vector<EnhancedTradingSignal> EnhancedSignalProcessor::generateTechnicalSignals(
    const std::string& symbol,
    const std::vector<double>& prices,
    const std::vector<double>& volumes) {

    std::vector<EnhancedTradingSignal> signals;

    // Moving average crossover signal
    if (prices.size() >= 50) {
        auto shortMA = calculateSMA(prices, 10);
        auto longMA = calculateSMA(prices, 20);

        if (!shortMA.empty() && !longMA.empty()) {
            double shortValue = shortMA.back();
            double longValue = longMA.back();

            if (shortValue > longValue) {
                EnhancedTradingSignal signal;
                signal.symbol = symbol;
                signal.signalType = SignalType::BUY;
                signal.source = SignalSource::TECHNICAL;
                signal.strength = calculateSignalStrength(shortValue, longValue);
                signal.confidence = 0.75;
                signal.quality = determineSignalQuality(signal.confidence);
                signal.timeHorizon = 24.0; // 24 hours
                signal.generatedAt = std::chrono::system_clock::now();
                signal.signalId = generateSignalId();
                signals.push_back(signal);
            }
        }
    }

    // Volume-based signal
    if (volumes.size() >= 10) {
        double avgVolume = std::accumulate(volumes.end() - 10, volumes.end(), 0.0) / 10.0;
        double currentVolume = volumes.back();

        if (currentVolume > avgVolume * 1.5) { // 50% above average
            EnhancedTradingSignal signal;
            signal.symbol = symbol;
            signal.signalType = SignalType::BUY;
            signal.source = SignalSource::TECHNICAL;
            signal.strength = std::min(1.0, currentVolume / avgVolume / 2.0);
            signal.confidence = 0.60;
            signal.quality = determineSignalQuality(signal.confidence);
            signal.timeHorizon = 12.0; // 12 hours
            signal.generatedAt = std::chrono::system_clock::now();
            signal.signalId = generateSignalId();
            signals.push_back(signal);
        }
    }

    return signals;
}

std::vector<EnhancedTradingSignal> EnhancedSignalProcessor::generateStatisticalSignals(
    const std::string& symbol,
    const std::vector<double>& prices) {

    std::vector<EnhancedTradingSignal> signals;

    if (prices.size() < 30) {
        return signals;
    }

    // Mean reversion signal
    double mean = std::accumulate(prices.end() - 20, prices.end(), 0.0) / 20.0;
    double currentPrice = prices.back();
    double deviation = std::abs(currentPrice - mean) / mean;

    if (deviation > 0.05) { // 5% deviation from mean
        EnhancedTradingSignal signal;
        signal.symbol = symbol;
        signal.signalType = (currentPrice < mean) ? SignalType::BUY : SignalType::SELL;
        signal.source = SignalSource::STATISTICAL;
        signal.strength = std::min(1.0, deviation * 10.0); // Scale deviation to strength
        signal.confidence = 0.65;
        signal.quality = determineSignalQuality(signal.confidence);
        signal.timeHorizon = 6.0; // 6 hours
        signal.generatedAt = std::chrono::system_clock::now();
        signal.signalId = generateSignalId();
        signals.push_back(signal);
    }

    return signals;
}

MarketRegime EnhancedSignalProcessor::detectMarketRegime(const std::vector<double>& prices) const {
    if (prices.size() < 20) {
        return MarketRegime::SIDEWAYS;
    }

    // Calculate trend over last 20 periods
    std::vector<double> recentPrices(prices.end() - 20, prices.end());
    double firstPrice = recentPrices.front();
    double lastPrice = recentPrices.back();
    double trendPercent = (lastPrice - firstPrice) / firstPrice;

    // Calculate volatility
    double volatility = calculateVolatility(recentPrices);

    // Determine regime based on trend and volatility
    if (std::abs(trendPercent) < 0.02) { // Less than 2% trend
        return MarketRegime::SIDEWAYS;
    } else if (volatility > 0.05) { // High volatility
        return MarketRegime::VOLATILE;
    } else if (trendPercent > 0.02) {
        return MarketRegime::TRENDING_UP;
    } else {
        return MarketRegime::TRENDING_DOWN;
    }
}

void EnhancedSignalProcessor::adjustSignalsForRegime(std::vector<EnhancedTradingSignal>& signals,
                                                    MarketRegime regime) {
    for (auto& signal : signals) {
        switch (regime) {
            case MarketRegime::VOLATILE:
                // Reduce confidence in volatile markets
                signal.confidence *= 0.8;
                break;
            case MarketRegime::SIDEWAYS:
                // Favor mean reversion signals in sideways markets
                if (signal.source == SignalSource::STATISTICAL) {
                    signal.confidence *= 1.2;
                }
                break;
            case MarketRegime::TRENDING_UP:
            case MarketRegime::TRENDING_DOWN:
                // Favor trend-following signals in trending markets
                if (signal.source == SignalSource::TECHNICAL) {
                    signal.confidence *= 1.1;
                }
                break;
        }

        // Update quality based on adjusted confidence
        signal.quality = determineSignalQuality(signal.confidence);
    }
}

void EnhancedSignalProcessor::adjustSignalsForVolatility(std::vector<EnhancedTradingSignal>& signals,
                                                        double volatility) {
    for (auto& signal : signals) {
        // Adjust time horizon based on volatility
        if (volatility > 0.05) {
            signal.timeHorizon *= 0.5; // Shorter time horizon in high volatility
        } else if (volatility < 0.02) {
            signal.timeHorizon *= 1.5; // Longer time horizon in low volatility
        }

        // Adjust value at risk based on volatility
        signal.valueAtRisk = signal.strength * volatility * 100; // Simple VaR calculation
    }
}

void EnhancedSignalProcessor::filterSignalsByQuality(std::vector<EnhancedTradingSignal>& signals) {
    double minimumThreshold = signalThresholds_[minimumSignalQuality_];

    signals.erase(
        std::remove_if(signals.begin(), signals.end(),
            [this, minimumThreshold](const EnhancedTradingSignal& signal) {
                return signal.confidence < minimumThreshold;
            }),
        signals.end());
}

void EnhancedSignalProcessor::limitSignalsPerSymbol(std::vector<EnhancedTradingSignal>& signals) {
    if (signals.size() <= static_cast<size_t>(maximumSignalsPerSymbol_)) {
        return;
    }

    // Sort by confidence (descending) and keep top signals
    std::sort(signals.begin(), signals.end(),
        [](const EnhancedTradingSignal& a, const EnhancedTradingSignal& b) {
            return a.confidence > b.confidence;
        });

    signals.resize(maximumSignalsPerSymbol_);
}

SignalQuality EnhancedSignalProcessor::determineSignalQuality(double confidence) const {
    if (confidence >= signalThresholds_.at(SignalQuality::EXCELLENT)) {
        return SignalQuality::EXCELLENT;
    } else if (confidence >= signalThresholds_.at(SignalQuality::GOOD)) {
        return SignalQuality::GOOD;
    } else if (confidence >= signalThresholds_.at(SignalQuality::MEDIUM)) {
        return SignalQuality::MEDIUM;
    } else {
        return SignalQuality::LOW;
    }
}

double EnhancedSignalProcessor::calculateSignalStrength(double shortValue, double longValue) const {
    double ratio = shortValue / longValue;
    return std::tanh((ratio - 1.0) * 10.0); // Sigmoid-like function
}

double EnhancedSignalProcessor::calculateVolatility(const std::vector<double>& prices) const {
    if (prices.size() < 2) {
        return 0.0;
    }

    std::vector<double> returns;
    for (size_t i = 1; i < prices.size(); ++i) {
        double logReturn = std::log(prices[i] / prices[i-1]);
        returns.push_back(logReturn);
    }

    double mean = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
    double variance = 0.0;

    for (double ret : returns) {
        variance += (ret - mean) * (ret - mean);
    }

    variance /= returns.size();
    return std::sqrt(variance);
}

std::vector<double> EnhancedSignalProcessor::calculateSMA(const std::vector<double>& prices, int period) const {
    std::vector<double> sma;

    if (prices.size() < static_cast<size_t>(period)) {
        return sma;
    }

    for (size_t i = period - 1; i < prices.size(); ++i) {
        double sum = 0.0;
        for (int j = 0; j < period; ++j) {
            sum += prices[i - j];
        }
        sma.push_back(sum / period);
    }

    return sma;
}

std::string EnhancedSignalProcessor::generateSignalId() const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);

    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    return "SIG_" + std::to_string(timestamp) + "_" + std::to_string(dis(gen));
}

} // namespace Strategy
} // namespace CryptoClaude