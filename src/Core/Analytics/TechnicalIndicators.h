#pragma once

#include <vector>
#include <string>
#include <map>
#include <memory>

namespace CryptoClaude {
namespace Analytics {

// Technical indicator calculation results
struct IndicatorResult {
    std::vector<double> values;
    std::vector<std::string> timestamps;
    std::string indicatorName;
    std::map<std::string, double> parameters;
    bool isValid = true;

    double getCurrentValue() const { return values.empty() ? 0.0 : values.back(); }
    double getValueAt(size_t index) const { return index < values.size() ? values[index] : 0.0; }
    size_t size() const { return values.size(); }
};

struct BollingerBands {
    std::vector<double> upperBand;
    std::vector<double> middleBand; // Moving average
    std::vector<double> lowerBand;
    std::vector<double> percentB;   // Position within bands
    std::vector<double> bandwidth;  // Band width indicator

    double getCurrentPercentB() const { return percentB.empty() ? 0.5 : percentB.back(); }
    bool isCurrentlyOverbought(double threshold = 0.8) const {
        return getCurrentPercentB() > threshold;
    }
    bool isCurrentlyOversold(double threshold = 0.2) const {
        return getCurrentPercentB() < threshold;
    }
};

struct MACD {
    std::vector<double> macdLine;
    std::vector<double> signalLine;
    std::vector<double> histogram;

    enum class Signal { BUY, SELL, HOLD };
    Signal getCurrentSignal() const;
    bool isBullishDivergence(const std::vector<double>& prices) const;
    bool isBearishDivergence(const std::vector<double>& prices) const;
};

struct RSI {
    std::vector<double> values;
    int period;

    double getCurrentValue() const { return values.empty() ? 50.0 : values.back(); }
    bool isOverbought(double threshold = 70.0) const { return getCurrentValue() > threshold; }
    bool isOversold(double threshold = 30.0) const { return getCurrentValue() < threshold; }
    bool isDivergent(const std::vector<double>& prices) const;
};

// Technical indicator calculator
class TechnicalIndicators {
public:
    TechnicalIndicators() = default;
    ~TechnicalIndicators() = default;

    // Moving averages
    static IndicatorResult calculateSMA(const std::vector<double>& prices, int period);
    static IndicatorResult calculateEMA(const std::vector<double>& prices, int period);
    static IndicatorResult calculateWMA(const std::vector<double>& prices, int period);
    static IndicatorResult calculateDEMA(const std::vector<double>& prices, int period);
    static IndicatorResult calculateTRIMA(const std::vector<double>& prices, int period);

    // Momentum indicators
    static RSI calculateRSI(const std::vector<double>& prices, int period = 14);
    static MACD calculateMACD(const std::vector<double>& prices, int fastPeriod = 12,
                             int slowPeriod = 26, int signalPeriod = 9);
    static IndicatorResult calculateStochastic(const std::vector<double>& high,
                                              const std::vector<double>& low,
                                              const std::vector<double>& close,
                                              int kPeriod = 14, int dPeriod = 3);
    static IndicatorResult calculateWilliamsR(const std::vector<double>& high,
                                             const std::vector<double>& low,
                                             const std::vector<double>& close,
                                             int period = 14);

    // Volatility indicators
    static BollingerBands calculateBollingerBands(const std::vector<double>& prices,
                                                  int period = 20, double stdDevMultiple = 2.0);
    static IndicatorResult calculateATR(const std::vector<double>& high,
                                       const std::vector<double>& low,
                                       const std::vector<double>& close,
                                       int period = 14);
    static IndicatorResult calculateVIX(const std::vector<double>& prices, int period = 21);

    // Volume indicators
    static IndicatorResult calculateOBV(const std::vector<double>& prices,
                                       const std::vector<double>& volume);
    static IndicatorResult calculateVolumeWeightedPrice(const std::vector<double>& prices,
                                                       const std::vector<double>& volume,
                                                       int period = 20);
    static IndicatorResult calculateMoneyFlowIndex(const std::vector<double>& high,
                                                  const std::vector<double>& low,
                                                  const std::vector<double>& close,
                                                  const std::vector<double>& volume,
                                                  int period = 14);

    // Trend indicators
    static IndicatorResult calculateADX(const std::vector<double>& high,
                                       const std::vector<double>& low,
                                       const std::vector<double>& close,
                                       int period = 14);
    static IndicatorResult calculateAroon(const std::vector<double>& high,
                                         const std::vector<double>& low,
                                         int period = 25);
    static IndicatorResult calculateParabolicSAR(const std::vector<double>& high,
                                                 const std::vector<double>& low,
                                                 double acceleration = 0.02,
                                                 double maxAcceleration = 0.2);

    // Support and Resistance
    static std::vector<double> calculatePivotPoints(double high, double low, double close);
    static std::vector<std::pair<double, int>> findSupportLevels(
        const std::vector<double>& prices, int lookback = 20, double tolerance = 0.01);
    static std::vector<std::pair<double, int>> findResistanceLevels(
        const std::vector<double>& prices, int lookback = 20, double tolerance = 0.01);

    // Pattern recognition
    struct CandlestickPattern {
        std::string patternName;
        size_t index;
        double strength; // 0.0 to 1.0
        bool isBullish;
        std::string description;
    };

    static std::vector<CandlestickPattern> detectCandlestickPatterns(
        const std::vector<double>& open,
        const std::vector<double>& high,
        const std::vector<double>& low,
        const std::vector<double>& close);

    // Composite indicators
    struct TrendAnalysis {
        double trendStrength; // -1.0 (strong down) to 1.0 (strong up)
        std::string trendDirection; // "Up", "Down", "Sideways"
        double momentum; // Current momentum
        double volatility; // Current volatility
        std::vector<std::string> signals; // Active trading signals
        double confidenceLevel; // Overall confidence in analysis
    };

    static TrendAnalysis performTrendAnalysis(const std::vector<double>& high,
                                            const std::vector<double>& low,
                                            const std::vector<double>& close,
                                            const std::vector<double>& volume);

    // Multi-timeframe analysis
    struct MultiTimeframeSignal {
        std::map<std::string, double> timeframeSignals; // "1h", "4h", "1d", etc.
        double overallSignal;
        double confidence;
        std::string recommendation; // "BUY", "SELL", "HOLD"
    };

    static MultiTimeframeSignal analyzeMultipleTimeframes(
        const std::map<std::string, std::vector<double>>& timeframePrices);

private:
    // Helper functions
    static std::vector<double> calculateTrueRange(const std::vector<double>& high,
                                                 const std::vector<double>& low,
                                                 const std::vector<double>& close);
    static std::vector<double> calculateTypicalPrice(const std::vector<double>& high,
                                                    const std::vector<double>& low,
                                                    const std::vector<double>& close);
    static double calculateStandardDeviation(const std::vector<double>& data, int period, size_t endIndex);
    static bool isLocalMinimum(const std::vector<double>& data, size_t index, int lookback);
    static bool isLocalMaximum(const std::vector<double>& data, size_t index, int lookback);

    // Pattern recognition helpers
    static bool isDoji(double open, double high, double low, double close);
    static bool isHammer(double open, double high, double low, double close);
    static bool isHangingMan(double open, double high, double low, double close);
    static bool isEngulfing(double open1, double close1, double open2, double close2);
};

// Indicator optimization and backtesting
class IndicatorOptimizer {
public:
    struct ParameterRange {
        std::string name;
        int minValue;
        int maxValue;
        int stepSize;
    };

    struct OptimizationResult {
        std::map<std::string, int> bestParameters;
        double bestPerformance;
        std::vector<std::map<std::string, int>> allParameters;
        std::vector<double> allPerformances;
    };

    // Optimize single indicator parameters
    static OptimizationResult optimizeRSI(const std::vector<double>& prices,
                                         const std::vector<double>& returns,
                                         const ParameterRange& periodRange);

    static OptimizationResult optimizeMACD(const std::vector<double>& prices,
                                          const std::vector<double>& returns,
                                          const ParameterRange& fastRange,
                                          const ParameterRange& slowRange,
                                          const ParameterRange& signalRange);

    static OptimizationResult optimizeBollingerBands(const std::vector<double>& prices,
                                                     const std::vector<double>& returns,
                                                     const ParameterRange& periodRange);

private:
    static double calculateIndicatorPerformance(const std::vector<double>& signals,
                                               const std::vector<double>& returns);
};

} // namespace Analytics
} // namespace CryptoClaude