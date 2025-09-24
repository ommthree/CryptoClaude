#include "TechnicalIndicators.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <stdexcept>

namespace CryptoClaude {
namespace Analytics {

// RSI Implementation
RSIResult TechnicalIndicators::calculateRSI(const std::vector<double>& prices, int period) {
    RSIResult result;
    result.period = period;

    if (prices.size() < static_cast<size_t>(period + 1)) {
        throw std::invalid_argument("Insufficient data points for RSI calculation");
    }

    std::vector<double> gains, losses;

    // Calculate price changes
    for (size_t i = 1; i < prices.size(); ++i) {
        double change = prices[i] - prices[i-1];
        gains.push_back(change > 0 ? change : 0);
        losses.push_back(change < 0 ? -change : 0);
    }

    // Calculate initial average gain and loss
    double avgGain = std::accumulate(gains.begin(), gains.begin() + period, 0.0) / period;
    double avgLoss = std::accumulate(losses.begin(), losses.begin() + period, 0.0) / period;

    // Calculate RSI values
    for (size_t i = period; i < gains.size(); ++i) {
        if (i > period) {
            // Wilder's smoothing
            avgGain = (avgGain * (period - 1) + gains[i]) / period;
            avgLoss = (avgLoss * (period - 1) + losses[i]) / period;
        }

        double rs = avgLoss == 0 ? 100 : avgGain / avgLoss;
        double rsi = 100 - (100 / (1 + rs));
        result.values.push_back(rsi);
    }

    return result;
}

// Simple Moving Average Implementation
SMAResult TechnicalIndicators::calculateSMA(const std::vector<double>& prices, int period) {
    SMAResult result;
    result.period = period;

    if (prices.size() < static_cast<size_t>(period)) {
        throw std::invalid_argument("Insufficient data points for SMA calculation");
    }

    for (size_t i = period - 1; i < prices.size(); ++i) {
        double sum = 0;
        for (int j = 0; j < period; ++j) {
            sum += prices[i - j];
        }
        result.values.push_back(sum / period);
    }

    return result;
}

// Exponential Moving Average Implementation
EMAResult TechnicalIndicators::calculateEMA(const std::vector<double>& prices, int period) {
    EMAResult result;
    result.period = period;

    if (prices.empty()) {
        throw std::invalid_argument("Empty price data for EMA calculation");
    }

    double multiplier = 2.0 / (period + 1);
    double ema = prices[0]; // Start with first price
    result.values.push_back(ema);

    for (size_t i = 1; i < prices.size(); ++i) {
        ema = (prices[i] * multiplier) + (ema * (1 - multiplier));
        result.values.push_back(ema);
    }

    return result;
}

// Bollinger Bands Implementation
BollingerBands TechnicalIndicators::calculateBollingerBands(const std::vector<double>& prices, int period, double standardDeviations) {
    BollingerBands result;
    result.period = period;
    result.standardDeviations = standardDeviations;

    if (prices.size() < static_cast<size_t>(period)) {
        throw std::invalid_argument("Insufficient data points for Bollinger Bands calculation");
    }

    // Calculate SMA as middle band
    auto sma = calculateSMA(prices, period);
    result.middleBand = sma.values;

    // Calculate standard deviation for each period
    for (size_t i = period - 1; i < prices.size(); ++i) {
        double mean = result.middleBand[i - (period - 1)];
        double variance = 0;

        for (int j = 0; j < period; ++j) {
            double diff = prices[i - j] - mean;
            variance += diff * diff;
        }
        variance /= period;
        double stdDev = std::sqrt(variance);

        result.upperBand.push_back(mean + (standardDeviations * stdDev));
        result.lowerBand.push_back(mean - (standardDeviations * stdDev));

        // Calculate %B (position within bands)
        double percentB = (prices[i] - result.lowerBand.back()) /
                         (result.upperBand.back() - result.lowerBand.back());
        result.percentB.push_back(percentB);

        // Calculate bandwidth
        double bandwidth = (result.upperBand.back() - result.lowerBand.back()) / mean;
        result.bandwidth.push_back(bandwidth);
    }

    return result;
}

// MACD Implementation
MACDResult TechnicalIndicators::calculateMACD(const std::vector<double>& prices, int fastPeriod, int slowPeriod, int signalPeriod) {
    MACDResult result;
    result.fastPeriod = fastPeriod;
    result.slowPeriod = slowPeriod;
    result.signalPeriod = signalPeriod;

    if (prices.size() < static_cast<size_t>(slowPeriod)) {
        throw std::invalid_argument("Insufficient data points for MACD calculation");
    }

    // Calculate fast and slow EMAs
    auto fastEMA = calculateEMA(prices, fastPeriod);
    auto slowEMA = calculateEMA(prices, slowPeriod);

    // Calculate MACD line (fast EMA - slow EMA)
    size_t startIndex = slowPeriod - 1;
    for (size_t i = startIndex; i < fastEMA.values.size(); ++i) {
        double macdValue = fastEMA.values[i] - slowEMA.values[i - startIndex];
        result.macdLine.push_back(macdValue);
    }

    // Calculate signal line (EMA of MACD line)
    if (result.macdLine.size() >= static_cast<size_t>(signalPeriod)) {
        auto signalEMA = calculateEMA(result.macdLine, signalPeriod);
        result.signalLine = signalEMA.values;

        // Calculate histogram (MACD - Signal)
        size_t signalStartIndex = signalPeriod - 1;
        for (size_t i = signalStartIndex; i < result.macdLine.size(); ++i) {
            double histogram = result.macdLine[i] - result.signalLine[i - signalStartIndex];
            result.histogram.push_back(histogram);
        }
    }

    return result;
}

// Stochastic Oscillator Implementation
StochasticResult TechnicalIndicators::calculateStochastic(const std::vector<double>& high,
                                                        const std::vector<double>& low,
                                                        const std::vector<double>& close,
                                                        int kPeriod, int dPeriod) {
    StochasticResult result;
    result.kPeriod = kPeriod;
    result.dPeriod = dPeriod;

    if (high.size() != low.size() || high.size() != close.size()) {
        throw std::invalid_argument("High, low, and close vectors must be the same size");
    }

    if (high.size() < static_cast<size_t>(kPeriod)) {
        throw std::invalid_argument("Insufficient data points for Stochastic calculation");
    }

    // Calculate %K
    for (size_t i = kPeriod - 1; i < close.size(); ++i) {
        double highestHigh = *std::max_element(high.begin() + i - kPeriod + 1, high.begin() + i + 1);
        double lowestLow = *std::min_element(low.begin() + i - kPeriod + 1, low.begin() + i + 1);

        double k = ((close[i] - lowestLow) / (highestHigh - lowestLow)) * 100;
        result.kPercent.push_back(k);
    }

    // Calculate %D (SMA of %K)
    if (result.kPercent.size() >= static_cast<size_t>(dPeriod)) {
        for (size_t i = dPeriod - 1; i < result.kPercent.size(); ++i) {
            double sum = 0;
            for (int j = 0; j < dPeriod; ++j) {
                sum += result.kPercent[i - j];
            }
            result.dPercent.push_back(sum / dPeriod);
        }
    }

    return result;
}

// Williams %R Implementation
WilliamsRResult TechnicalIndicators::calculateWilliamsR(const std::vector<double>& high,
                                                       const std::vector<double>& low,
                                                       const std::vector<double>& close,
                                                       int period) {
    WilliamsRResult result;
    result.period = period;

    if (high.size() != low.size() || high.size() != close.size()) {
        throw std::invalid_argument("High, low, and close vectors must be the same size");
    }

    if (high.size() < static_cast<size_t>(period)) {
        throw std::invalid_argument("Insufficient data points for Williams %R calculation");
    }

    for (size_t i = period - 1; i < close.size(); ++i) {
        double highestHigh = *std::max_element(high.begin() + i - period + 1, high.begin() + i + 1);
        double lowestLow = *std::min_element(low.begin() + i - period + 1, low.begin() + i + 1);

        double williamsR = ((highestHigh - close[i]) / (highestHigh - lowestLow)) * -100;
        result.values.push_back(williamsR);
    }

    return result;
}

// Average True Range Implementation
ATRResult TechnicalIndicators::calculateATR(const std::vector<double>& high,
                                          const std::vector<double>& low,
                                          const std::vector<double>& close,
                                          int period) {
    ATRResult result;
    result.period = period;

    if (high.size() != low.size() || high.size() != close.size()) {
        throw std::invalid_argument("High, low, and close vectors must be the same size");
    }

    if (high.size() < static_cast<size_t>(period + 1)) {
        throw std::invalid_argument("Insufficient data points for ATR calculation");
    }

    std::vector<double> trueRanges;

    // Calculate True Range for each period
    for (size_t i = 1; i < close.size(); ++i) {
        double tr1 = high[i] - low[i];
        double tr2 = std::abs(high[i] - close[i-1]);
        double tr3 = std::abs(low[i] - close[i-1]);

        double trueRange = std::max({tr1, tr2, tr3});
        trueRanges.push_back(trueRange);
    }

    // Calculate ATR using EMA of True Range
    if (trueRanges.size() >= static_cast<size_t>(period)) {
        auto atrEMA = calculateEMA(trueRanges, period);
        result.values = atrEMA.values;
    }

    return result;
}

} // namespace Analytics
} // namespace CryptoClaude