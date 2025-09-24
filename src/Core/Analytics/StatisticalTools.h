#pragma once

#include <vector>
#include <map>
#include <string>
#include <cmath>
#include <algorithm>
#include <numeric>

namespace CryptoClaude {
namespace Analytics {

// Statistical computation results
struct CorrelationMatrix {
    std::vector<std::string> symbols;
    std::vector<std::vector<double>> correlations;
    size_t sampleSize;
    std::string timeFrame;

    double getCorrelation(const std::string& symbol1, const std::string& symbol2) const;
    std::vector<std::pair<std::string, double>> getCorrelationsFor(const std::string& symbol) const;
};

struct RegressionResult {
    double slope;
    double intercept;
    double rSquared;
    double correlation;
    double standardError;
    size_t sampleSize;
    bool isSignificant;

    double predict(double x) const { return slope * x + intercept; }
    double getConfidenceLevel() const { return isSignificant ? 0.95 : 0.0; }
};

struct DistributionStats {
    double mean;
    double median;
    double mode;
    double standardDeviation;
    double variance;
    double skewness;
    double kurtosis;
    double min;
    double max;
    double percentile25;
    double percentile75;
    size_t sampleSize;

    bool isNormalDistribution() const { return std::abs(skewness) < 0.5 && std::abs(kurtosis - 3.0) < 1.0; }
    double getConfidenceInterval(double confidenceLevel) const;
};

// Statistical analysis tools
class StatisticalTools {
public:
    StatisticalTools() = default;
    ~StatisticalTools() = default;

    // Correlation analysis
    static double calculateCorrelation(const std::vector<double>& x, const std::vector<double>& y);
    static CorrelationMatrix calculateCorrelationMatrix(
        const std::map<std::string, std::vector<double>>& data);
    static double calculateSpearmanCorrelation(const std::vector<double>& x, const std::vector<double>& y);

    // Regression analysis
    static RegressionResult calculateLinearRegression(
        const std::vector<double>& x, const std::vector<double>& y);
    static RegressionResult calculatePolynomialRegression(
        const std::vector<double>& x, const std::vector<double>& y, int degree);

    // Distribution analysis
    static DistributionStats calculateDistributionStats(const std::vector<double>& data);
    static double calculatePercentile(const std::vector<double>& data, double percentile);
    static bool testNormality(const std::vector<double>& data, double significanceLevel = 0.05);

    // Time series analysis
    static std::vector<double> calculateMovingAverage(const std::vector<double>& data, int window);
    static std::vector<double> calculateExponentialMovingAverage(
        const std::vector<double>& data, double alpha);
    static double calculateAutoCorrelation(const std::vector<double>& data, int lag);
    static std::vector<double> calculateAutoCorrelationFunction(
        const std::vector<double>& data, int maxLag);

    // Volatility analysis
    static double calculateVolatility(const std::vector<double>& returns, int window = 0);
    static std::vector<double> calculateRollingVolatility(
        const std::vector<double>& returns, int window);
    static double calculateDownsideDeviation(const std::vector<double>& returns, double threshold = 0.0);

    // Statistical tests
    static double calculateTStatistic(const std::vector<double>& sample, double populationMean);
    static bool performTTest(const std::vector<double>& sample1, const std::vector<double>& sample2,
                            double significanceLevel = 0.05);
    static double calculateChiSquareStatistic(const std::vector<double>& observed,
                                             const std::vector<double>& expected);

    // Utility functions
    static std::vector<double> standardize(const std::vector<double>& data);
    static std::vector<double> normalize(const std::vector<double>& data);
    static std::vector<double> calculateReturns(const std::vector<double>& prices);
    static std::vector<double> calculateLogReturns(const std::vector<double>& prices);
    static double calculateSharpeRatio(const std::vector<double>& returns, double riskFreeRate = 0.0);
    static double calculateSortinoRatio(const std::vector<double>& returns, double targetReturn = 0.0);

private:
    static double calculateMean(const std::vector<double>& data);
    static double calculateVariance(const std::vector<double>& data, double mean);
    static double calculateSkewness(const std::vector<double>& data, double mean, double stdDev);
    static double calculateKurtosis(const std::vector<double>& data, double mean, double stdDev);
    static std::vector<double> calculateRanks(const std::vector<double>& data);
};

// Advanced statistical analysis
class AdvancedStatistics {
public:
    // Monte Carlo simulation
    struct MonteCarloResult {
        std::vector<double> simulations;
        DistributionStats statistics;
        double valueAtRisk95;
        double valueAtRisk99;
        double expectedShortfall95;
        double expectedShortfall99;
    };

    static MonteCarloResult runMonteCarloSimulation(
        std::function<double()> simulationFunction, int iterations);

    // Bootstrap analysis
    struct BootstrapResult {
        std::vector<double> bootstrapSamples;
        double originalStatistic;
        double bootstrapMean;
        double bootstrapStdError;
        double confidenceInterval95Lower;
        double confidenceInterval95Upper;
    };

    static BootstrapResult performBootstrap(
        const std::vector<double>& data,
        std::function<double(const std::vector<double>&)> statisticFunction,
        int iterations = 1000);

    // Principal Component Analysis
    struct PCAResult {
        std::vector<std::vector<double>> principalComponents;
        std::vector<double> eigenValues;
        std::vector<double> explainedVarianceRatio;
        double cumulativeVarianceExplained;
        int recommendedComponents;
    };

    static PCAResult performPCA(const std::vector<std::vector<double>>& data,
                               double varianceThreshold = 0.95);

private:
    static std::vector<std::vector<double>> calculateCovarianceMatrix(
        const std::vector<std::vector<double>>& data);
    static std::pair<std::vector<double>, std::vector<std::vector<double>>>
        calculateEigenDecomposition(const std::vector<std::vector<double>>& matrix);
};

} // namespace Analytics
} // namespace CryptoClaude