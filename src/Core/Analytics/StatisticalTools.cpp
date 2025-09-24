#include "StatisticalTools.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <random>
#include <functional>
#include <stdexcept>

namespace CryptoClaude {
namespace Analytics {

// CorrelationMatrix implementation
double CorrelationMatrix::getCorrelation(const std::string& symbol1, const std::string& symbol2) const {
    auto it1 = std::find(symbols.begin(), symbols.end(), symbol1);
    auto it2 = std::find(symbols.begin(), symbols.end(), symbol2);

    if (it1 == symbols.end() || it2 == symbols.end()) {
        throw std::invalid_argument("Symbol not found in correlation matrix");
    }

    size_t idx1 = std::distance(symbols.begin(), it1);
    size_t idx2 = std::distance(symbols.begin(), it2);

    return correlations[idx1][idx2];
}

std::vector<std::pair<std::string, double>> CorrelationMatrix::getCorrelationsFor(const std::string& symbol) const {
    auto it = std::find(symbols.begin(), symbols.end(), symbol);
    if (it == symbols.end()) {
        throw std::invalid_argument("Symbol not found in correlation matrix");
    }

    size_t idx = std::distance(symbols.begin(), it);
    std::vector<std::pair<std::string, double>> result;

    for (size_t i = 0; i < symbols.size(); ++i) {
        if (i != idx) {
            result.emplace_back(symbols[i], correlations[idx][i]);
        }
    }

    return result;
}

// DistributionStats implementation
double DistributionStats::getConfidenceInterval(double confidenceLevel) const {
    if (confidenceLevel < 0.0 || confidenceLevel > 1.0) {
        throw std::invalid_argument("Confidence level must be between 0 and 1");
    }

    // For normal distribution approximation
    double zScore = 1.96; // 95% confidence
    if (confidenceLevel == 0.99) zScore = 2.576;
    else if (confidenceLevel == 0.90) zScore = 1.645;

    return zScore * (standardDeviation / std::sqrt(sampleSize));
}

// StatisticalTools implementation
double StatisticalTools::calculateCorrelation(const std::vector<double>& x, const std::vector<double>& y) {
    if (x.size() != y.size() || x.empty()) {
        throw std::invalid_argument("Vectors must be non-empty and of equal size");
    }

    double meanX = calculateMean(x);
    double meanY = calculateMean(y);

    double numerator = 0.0;
    double sumXSquared = 0.0;
    double sumYSquared = 0.0;

    for (size_t i = 0; i < x.size(); ++i) {
        double diffX = x[i] - meanX;
        double diffY = y[i] - meanY;

        numerator += diffX * diffY;
        sumXSquared += diffX * diffX;
        sumYSquared += diffY * diffY;
    }

    double denominator = std::sqrt(sumXSquared * sumYSquared);
    return (denominator == 0.0) ? 0.0 : numerator / denominator;
}

CorrelationMatrix StatisticalTools::calculateCorrelationMatrix(
    const std::map<std::string, std::vector<double>>& data) {

    CorrelationMatrix result;
    result.symbols.reserve(data.size());

    for (const auto& pair : data) {
        result.symbols.push_back(pair.first);
    }

    size_t n = result.symbols.size();
    result.correlations.resize(n, std::vector<double>(n));
    result.sampleSize = data.empty() ? 0 : data.begin()->second.size();

    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            if (i == j) {
                result.correlations[i][j] = 1.0;
            } else {
                result.correlations[i][j] = calculateCorrelation(
                    data.at(result.symbols[i]), data.at(result.symbols[j]));
            }
        }
    }

    return result;
}

double StatisticalTools::calculateSpearmanCorrelation(const std::vector<double>& x, const std::vector<double>& y) {
    if (x.size() != y.size() || x.empty()) {
        throw std::invalid_argument("Vectors must be non-empty and of equal size");
    }

    std::vector<double> ranksX = calculateRanks(x);
    std::vector<double> ranksY = calculateRanks(y);

    return calculateCorrelation(ranksX, ranksY);
}

RegressionResult StatisticalTools::calculateLinearRegression(
    const std::vector<double>& x, const std::vector<double>& y) {

    if (x.size() != y.size() || x.empty()) {
        throw std::invalid_argument("Vectors must be non-empty and of equal size");
    }

    RegressionResult result;
    result.sampleSize = x.size();

    double meanX = calculateMean(x);
    double meanY = calculateMean(y);

    double numerator = 0.0;
    double denominator = 0.0;

    for (size_t i = 0; i < x.size(); ++i) {
        double diffX = x[i] - meanX;
        numerator += diffX * (y[i] - meanY);
        denominator += diffX * diffX;
    }

    result.slope = (denominator == 0.0) ? 0.0 : numerator / denominator;
    result.intercept = meanY - result.slope * meanX;
    result.correlation = calculateCorrelation(x, y);
    result.rSquared = result.correlation * result.correlation;

    // Calculate standard error
    double sumSquaredResiduals = 0.0;
    for (size_t i = 0; i < x.size(); ++i) {
        double predicted = result.predict(x[i]);
        double residual = y[i] - predicted;
        sumSquaredResiduals += residual * residual;
    }

    result.standardError = std::sqrt(sumSquaredResiduals / (x.size() - 2));
    result.isSignificant = std::abs(result.correlation) > (2.0 / std::sqrt(x.size()));

    return result;
}

DistributionStats StatisticalTools::calculateDistributionStats(const std::vector<double>& data) {
    if (data.empty()) {
        throw std::invalid_argument("Data vector cannot be empty");
    }

    DistributionStats stats;
    stats.sampleSize = data.size();

    // Sort data for percentile calculations
    std::vector<double> sortedData = data;
    std::sort(sortedData.begin(), sortedData.end());

    stats.min = sortedData.front();
    stats.max = sortedData.back();
    stats.mean = calculateMean(data);
    stats.median = calculatePercentile(sortedData, 0.5);
    stats.percentile25 = calculatePercentile(sortedData, 0.25);
    stats.percentile75 = calculatePercentile(sortedData, 0.75);

    stats.variance = calculateVariance(data, stats.mean);
    stats.standardDeviation = std::sqrt(stats.variance);
    stats.skewness = calculateSkewness(data, stats.mean, stats.standardDeviation);
    stats.kurtosis = calculateKurtosis(data, stats.mean, stats.standardDeviation);

    // Calculate mode (most frequent value - simplified)
    std::map<double, int> frequency;
    for (double value : data) {
        frequency[value]++;
    }

    int maxFreq = 0;
    for (const auto& pair : frequency) {
        if (pair.second > maxFreq) {
            maxFreq = pair.second;
            stats.mode = pair.first;
        }
    }

    return stats;
}

double StatisticalTools::calculatePercentile(const std::vector<double>& data, double percentile) {
    if (data.empty() || percentile < 0.0 || percentile > 1.0) {
        throw std::invalid_argument("Invalid data or percentile value");
    }

    std::vector<double> sortedData = data;
    std::sort(sortedData.begin(), sortedData.end());

    double index = percentile * (sortedData.size() - 1);
    size_t lower = static_cast<size_t>(std::floor(index));
    size_t upper = static_cast<size_t>(std::ceil(index));

    if (lower == upper) {
        return sortedData[lower];
    }

    double weight = index - lower;
    return sortedData[lower] * (1.0 - weight) + sortedData[upper] * weight;
}

std::vector<double> StatisticalTools::calculateMovingAverage(const std::vector<double>& data, int window) {
    if (data.empty() || window <= 0 || window > static_cast<int>(data.size())) {
        throw std::invalid_argument("Invalid data or window size");
    }

    std::vector<double> result;
    result.reserve(data.size() - window + 1);

    for (size_t i = window - 1; i < data.size(); ++i) {
        double sum = 0.0;
        for (int j = 0; j < window; ++j) {
            sum += data[i - j];
        }
        result.push_back(sum / window);
    }

    return result;
}

double StatisticalTools::calculateVolatility(const std::vector<double>& returns, int window) {
    if (returns.empty()) {
        return 0.0;
    }

    std::vector<double> dataToAnalyze = returns;
    if (window > 0 && window < static_cast<int>(returns.size())) {
        dataToAnalyze = std::vector<double>(returns.end() - window, returns.end());
    }

    double mean = calculateMean(dataToAnalyze);
    double variance = calculateVariance(dataToAnalyze, mean);

    return std::sqrt(variance) * std::sqrt(252); // Annualized volatility
}

double StatisticalTools::calculateSharpeRatio(const std::vector<double>& returns, double riskFreeRate) {
    if (returns.empty()) {
        return 0.0;
    }

    double meanReturn = calculateMean(returns);
    double volatility = calculateVolatility(returns);

    return volatility == 0.0 ? 0.0 : (meanReturn - riskFreeRate) / volatility;
}

double StatisticalTools::calculateSortinoRatio(const std::vector<double>& returns, double targetReturn) {
    if (returns.empty()) {
        return 0.0;
    }

    double meanReturn = calculateMean(returns);
    double downsideDeviation = calculateDownsideDeviation(returns, targetReturn);

    return downsideDeviation == 0.0 ? 0.0 : (meanReturn - targetReturn) / downsideDeviation;
}

double StatisticalTools::calculateDownsideDeviation(const std::vector<double>& returns, double threshold) {
    double sum = 0.0;
    int count = 0;

    for (double ret : returns) {
        if (ret < threshold) {
            double diff = ret - threshold;
            sum += diff * diff;
            count++;
        }
    }

    return count == 0 ? 0.0 : std::sqrt(sum / count);
}

// Private utility functions
double StatisticalTools::calculateMean(const std::vector<double>& data) {
    if (data.empty()) return 0.0;
    return std::accumulate(data.begin(), data.end(), 0.0) / data.size();
}

double StatisticalTools::calculateVariance(const std::vector<double>& data, double mean) {
    if (data.size() <= 1) return 0.0;

    double sum = 0.0;
    for (double value : data) {
        double diff = value - mean;
        sum += diff * diff;
    }

    return sum / (data.size() - 1);
}

double StatisticalTools::calculateSkewness(const std::vector<double>& data, double mean, double stdDev) {
    if (data.size() <= 2 || stdDev == 0.0) return 0.0;

    double sum = 0.0;
    for (double value : data) {
        double standardized = (value - mean) / stdDev;
        sum += standardized * standardized * standardized;
    }

    return sum / data.size();
}

double StatisticalTools::calculateKurtosis(const std::vector<double>& data, double mean, double stdDev) {
    if (data.size() <= 3 || stdDev == 0.0) return 3.0; // Normal distribution kurtosis

    double sum = 0.0;
    for (double value : data) {
        double standardized = (value - mean) / stdDev;
        sum += standardized * standardized * standardized * standardized;
    }

    return sum / data.size();
}

std::vector<double> StatisticalTools::calculateRanks(const std::vector<double>& data) {
    std::vector<std::pair<double, size_t>> indexed_data;
    for (size_t i = 0; i < data.size(); ++i) {
        indexed_data.emplace_back(data[i], i);
    }

    std::sort(indexed_data.begin(), indexed_data.end());

    std::vector<double> ranks(data.size());
    for (size_t i = 0; i < indexed_data.size(); ++i) {
        ranks[indexed_data[i].second] = static_cast<double>(i + 1);
    }

    return ranks;
}

} // namespace Analytics
} // namespace CryptoClaude