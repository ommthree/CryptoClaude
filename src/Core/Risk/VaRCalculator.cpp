#include "VaRCalculator.h"
#include <algorithm>
#include <random>
#include <cmath>
#include <numeric>
#include <stdexcept>
#include <iostream>
#include <iomanip>

namespace CryptoClaude {
namespace Risk {

// Mathematical constants
constexpr double PI = 3.14159265359;

// === VaRBacktester Implementation ===

VaRBacktester::VaRBacktester(int backtestPeriodDays)
    : backtestPeriodDays_(backtestPeriodDays),
      criticalValue95_(1.96),  // 95% confidence critical value
      criticalValue99_(2.576)  // 99% confidence critical value
{
}

VaRBacktester::BacktestResult VaRBacktester::runBacktest(
    VaRMethodology methodology,
    VaRConfidenceLevel confidenceLevel,
    const std::vector<double>& returns,
    const std::vector<double>& varEstimates)
{
    BacktestResult result;
    result.methodology = methodology;
    result.confidenceLevel = confidenceLevel;
    result.totalObservations = std::min(returns.size(), varEstimates.size());

    if (result.totalObservations == 0) {
        return result;
    }

    // Calculate expected breach rate
    result.expectedBreachRate = (100.0 - static_cast<double>(confidenceLevel)) / 100.0;

    // Count VaR breaches
    result.varBreaches = 0;
    std::vector<bool> breaches;
    result.breachDetails.clear();

    for (size_t i = 0; i < result.totalObservations; ++i) {
        bool isBreach = returns[i] < -varEstimates[i]; // Loss exceeds VaR
        breaches.push_back(isBreach);
        if (isBreach) {
            result.varBreaches++;
            result.breachDetails.emplace_back(std::chrono::system_clock::now(), true);
        }
    }

    result.breachRate = static_cast<double>(result.varBreaches) / result.totalObservations;

    // Kupiec POF test
    result.kupiecTestPassed = kupiecTest(result.breachRate, result.expectedBreachRate,
                                        static_cast<int>(result.totalObservations), result.kupiecPValue);

    // Christoffersen independence test
    result.christoffersenTestPassed = christoffersenTest(breaches, result.expectedBreachRate,
                                                        result.christoffersenPValue);

    // Calculate performance metrics
    std::vector<double> actualLosses, varPredictions;
    for (size_t i = 0; i < result.totalObservations; ++i) {
        actualLosses.push_back(-returns[i]); // Convert returns to losses
        varPredictions.push_back(varEstimates[i]);
    }

    result.meanAbsoluteError = calculateMeanAbsoluteError(actualLosses, varPredictions);
    result.rootMeanSquareError = calculateRootMeanSquareError(actualLosses, varPredictions);

    // Calculate average VaR accuracy
    double totalAccuracy = 0.0;
    for (size_t i = 0; i < result.totalObservations; ++i) {
        double accuracy = std::max(0.0, 1.0 - std::abs(actualLosses[i] - varPredictions[i]) / varPredictions[i]);
        totalAccuracy += accuracy;
    }
    result.averageVaRAccuracy = totalAccuracy / result.totalObservations;

    return result;
}

bool VaRBacktester::kupiecTest(double breachRate, double expectedRate, int observations, double& pValue) {
    if (observations <= 0 || expectedRate <= 0 || expectedRate >= 1) {
        pValue = 0.0;
        return false;
    }

    int actualBreaches = static_cast<int>(breachRate * observations);

    // Likelihood ratio test statistic
    double lr = 0.0;
    if (actualBreaches > 0) {
        lr = 2.0 * (actualBreaches * std::log(breachRate / expectedRate) +
                   (observations - actualBreaches) * std::log((1.0 - breachRate) / (1.0 - expectedRate)));
    }

    // Chi-square distribution with 1 degree of freedom
    // Simplified p-value calculation (for critical values around 3.84 for 95% confidence)
    pValue = std::exp(-lr / 2.0); // Approximation

    return lr < 3.84; // 95% confidence level
}

bool VaRBacktester::christoffersenTest(const std::vector<bool>& breaches, double expectedRate, double& pValue) {
    if (breaches.empty()) {
        pValue = 0.0;
        return false;
    }

    // Count transitions
    int n00 = 0, n01 = 0, n10 = 0, n11 = 0;

    for (size_t i = 1; i < breaches.size(); ++i) {
        if (!breaches[i-1] && !breaches[i]) n00++;
        else if (!breaches[i-1] && breaches[i]) n01++;
        else if (breaches[i-1] && !breaches[i]) n10++;
        else if (breaches[i-1] && breaches[i]) n11++;
    }

    // Calculate test statistic
    int totalViolations = n01 + n11;
    int totalNonViolations = n00 + n10;

    if (totalViolations == 0 || totalNonViolations == 0) {
        pValue = 1.0;
        return true;
    }

    double p1 = static_cast<double>(n01) / (n00 + n01);
    double p2 = static_cast<double>(n11) / (n10 + n11);

    // Likelihood ratio for independence
    double lr = 0.0;
    if (p1 > 0 && p2 > 0 && p1 != p2) {
        double pHat = static_cast<double>(totalViolations) / (totalViolations + totalNonViolations);
        lr = 2.0 * ((n01 * std::log(p1 / pHat) + n11 * std::log(p2 / pHat)) +
                   (n00 * std::log((1.0 - p1) / (1.0 - pHat)) + n10 * std::log((1.0 - p2) / (1.0 - pHat))));
    }

    pValue = std::exp(-lr / 2.0); // Approximation
    return lr < 3.84; // 95% confidence level
}

double VaRBacktester::calculateMeanAbsoluteError(const std::vector<double>& actual,
                                                const std::vector<double>& predicted) {
    if (actual.size() != predicted.size() || actual.empty()) {
        return std::numeric_limits<double>::max();
    }

    double totalError = 0.0;
    for (size_t i = 0; i < actual.size(); ++i) {
        totalError += std::abs(actual[i] - predicted[i]);
    }

    return totalError / actual.size();
}

double VaRBacktester::calculateRootMeanSquareError(const std::vector<double>& actual,
                                                  const std::vector<double>& predicted) {
    if (actual.size() != predicted.size() || actual.empty()) {
        return std::numeric_limits<double>::max();
    }

    double totalSquaredError = 0.0;
    for (size_t i = 0; i < actual.size(); ++i) {
        double error = actual[i] - predicted[i];
        totalSquaredError += error * error;
    }

    return std::sqrt(totalSquaredError / actual.size());
}

// === VaRCalculator Implementation ===

VaRCalculator::VaRCalculator()
    : isInitialized_(false),
      lastCalculationTime_(std::chrono::system_clock::now()),
      lastDashboardUpdate_(std::chrono::system_clock::now())
{
    backtester_ = std::make_unique<VaRBacktester>();
}

VaRCalculator::~VaRCalculator() = default;

bool VaRCalculator::initialize(const TRSRiskParameters& trsParams) {
    try {
        trsParams_ = trsParams;

        // Initialize with conservative defaults if not provided
        if (histConfig_.lookbackDays == 0) histConfig_.lookbackDays = 252;
        if (mcConfig_.simulationCount == 0) mcConfig_.simulationCount = 10000;
        if (paramConfig_.lookbackPeriod == 0) paramConfig_.lookbackPeriod = 252;

        // Clear any existing state
        activeAlerts_.clear();
        historicalReturns_.clear();
        correlationMatrix_.clear();

        isInitialized_ = true;

        std::cout << "[VaRCalculator] Initialized with TRS parameters:" << std::endl;
        std::cout << "  - Max Daily VaR (95%): " << (trsParams_.maxDailyVaR95 * 100) << "%" << std::endl;
        std::cout << "  - Max Daily VaR (99%): " << (trsParams_.maxDailyVaR99 * 100) << "%" << std::endl;
        std::cout << "  - Max Calculation Time: " << trsParams_.maxCalculationTimeMs << "ms" << std::endl;

        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "[VaRCalculator] Initialization failed: " << e.what() << std::endl;
        isInitialized_ = false;
        return false;
    }
}

void VaRCalculator::setCorrelationMonitor(std::shared_ptr<CrossAssetCorrelationMonitor> monitor) {
    correlationMonitor_ = monitor;
    if (monitor) {
        updateCorrelationMatrix();
        std::cout << "[VaRCalculator] Correlation monitor integrated with "
                  << monitor->getMonitoredPairs().size() << " pairs" << std::endl;
    }
}

void VaRCalculator::setPortfolioOptimizer(std::shared_ptr<CryptoClaude::Optimization::PortfolioOptimizer> optimizer) {
    portfolioOptimizer_ = optimizer;
    if (optimizer) {
        std::cout << "[VaRCalculator] Portfolio optimizer integrated" << std::endl;
    }
}

VaRResult VaRCalculator::calculateVaR(const std::vector<std::string>& assets,
                                      const std::vector<double>& weights,
                                      VaRMethodology methodology,
                                      VaRConfidenceLevel confidence,
                                      VaRTimeHorizon horizon) {
    auto startTime = std::chrono::high_resolution_clock::now();

    if (!isInitialized_) {
        throw std::runtime_error("VaRCalculator not initialized");
    }

    if (!validateInputData(assets, weights)) {
        throw std::invalid_argument("Invalid input data for VaR calculation");
    }

    VaRResult result;

    try {
        // Select calculation method
        switch (methodology) {
            case VaRMethodology::PARAMETRIC:
                result = calculateParametricVaR(assets, weights, confidence);
                break;
            case VaRMethodology::HISTORICAL_SIMULATION:
                result = calculateHistoricalVaR(assets, weights, confidence);
                break;
            case VaRMethodology::MONTE_CARLO:
                result = calculateMonteCarloVaR(assets, weights, confidence);
                break;
            case VaRMethodology::CORNISH_FISHER:
                result = calculateCornishFisherVaR(assets, weights, confidence);
                break;
            default:
                throw std::invalid_argument("Unsupported VaR methodology");
        }

        // Set common result properties
        result.methodology = methodology;
        result.timeHorizon = horizon;
        result.confidenceLevel = confidence;
        result.calculationTime = std::chrono::system_clock::now();

        // Calculate performance metrics
        auto endTime = std::chrono::high_resolution_clock::now();
        result.calculationDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        // Scale VaR for time horizon if not daily
        if (horizon != VaRTimeHorizon::DAILY) {
            double scaleFactor = VaRUtils::scaleVaRToHorizon(1.0, horizon);
            result.varAmount *= scaleFactor;
            result.varPercentage *= scaleFactor;
        }

        // Calculate additional risk measures
        std::vector<double> portfolioReturns = calculatePortfolioReturns(assets, weights, histConfig_.lookbackDays);
        if (!portfolioReturns.empty()) {
            result.conditionalVaR = calculateConditionalVaR(portfolioReturns, confidence);
            result.maximumDrawdown = calculateMaximumDrawdown(portfolioReturns);
        }

        // Calculate component and incremental VaR
        result.componentVaR = calculateComponentVaR(assets, weights, methodology);
        result.incrementalVaR = calculateIncrementalVaR(assets, weights, methodology);

        // Store asset weights
        for (size_t i = 0; i < assets.size() && i < weights.size(); ++i) {
            result.assetWeights[assets[i]] = weights[i];
        }

        // Validate against TRS limits
        result.isValid = validateTRSLimits(result);

        // Log performance
        logPerformanceMetrics("VaR Calculation", result.calculationDuration);

        lastCalculationTime_ = result.calculationTime;

        return result;
    }
    catch (const std::exception& e) {
        handleCalculationError("calculateVaR", e);
        throw;
    }
}

VaRResult VaRCalculator::calculateParametricVaR(const std::vector<std::string>& assets,
                                                const std::vector<double>& weights,
                                                VaRConfidenceLevel confidence,
                                                const ParametricConfig& config) {
    VaRResult result;
    result.methodology = VaRMethodology::PARAMETRIC;

    try {
        // Calculate portfolio variance using correlation matrix
        std::vector<std::vector<double>> covMatrix = calculateCovarianceMatrix(assets);
        double portfolioVariance = calculatePortfolioVariance(weights, covMatrix);
        double portfolioVolatility = std::sqrt(portfolioVariance);

        // Calculate portfolio value (assume $1M if not specified)
        double portfolioValue = 1000000.0; // Default value
        result.portfolioValue = portfolioValue;

        // Get z-score for confidence level
        double zScore = VaRUtils::getZScore(confidence);

        // Basic parametric VaR
        result.varAmount = portfolioValue * portfolioVolatility * zScore;
        result.varPercentage = portfolioVolatility * zScore;

        // Cornish-Fisher adjustment if enabled
        if (config.adjustForSkewness || config.adjustForKurtosis) {
            std::vector<double> portfolioReturns = calculatePortfolioReturns(assets, weights, config.lookbackPeriod);
            if (!portfolioReturns.empty()) {
                double skewness = config.adjustForSkewness ? calculateSkewness(portfolioReturns) : 0.0;
                double kurtosis = config.adjustForKurtosis ? calculateKurtosis(portfolioReturns) : 0.0;

                double adjustedZScore = cornishFisherAdjustment(zScore, skewness, kurtosis);
                result.varAmount = portfolioValue * portfolioVolatility * adjustedZScore;
                result.varPercentage = portfolioVolatility * adjustedZScore;
            }
        }

        // Risk decomposition
        result.systematicRisk = result.varPercentage * 0.7;  // Estimate 70% systematic
        result.idiosyncraticRisk = result.varPercentage * 0.3; // 30% idiosyncratic

        // Calculate correlation risk
        result.correlationRisk = calculateCorrelationRisk(assets, weights);

        result.isValid = true;
        return result;
    }
    catch (const std::exception& e) {
        handleCalculationError("calculateParametricVaR", e);
        result.isValid = false;
        return result;
    }
}

VaRResult VaRCalculator::calculateHistoricalVaR(const std::vector<std::string>& assets,
                                                const std::vector<double>& weights,
                                                VaRConfidenceLevel confidence,
                                                const HistoricalSimulationConfig& config) {
    VaRResult result;
    result.methodology = VaRMethodology::HISTORICAL_SIMULATION;

    try {
        // Get historical portfolio returns
        std::vector<double> portfolioReturns = calculatePortfolioReturns(assets, weights, config.lookbackDays);

        if (portfolioReturns.size() < config.minDataPoints) {
            throw std::runtime_error("Insufficient historical data for Historical Simulation VaR");
        }

        // Simple historical simulation
        std::sort(portfolioReturns.begin(), portfolioReturns.end());

        double percentile = (100.0 - static_cast<double>(confidence)) / 100.0;
        size_t index = static_cast<size_t>(percentile * portfolioReturns.size());
        index = std::min(index, portfolioReturns.size() - 1);

        result.varPercentage = -portfolioReturns[index]; // Convert to loss

        // Calculate dollar VaR
        double portfolioValue = 1000000.0; // Default value
        result.portfolioValue = portfolioValue;
        result.varAmount = portfolioValue * result.varPercentage;

        // Calculate conditional VaR
        result.conditionalVaR = calculateConditionalVaR(portfolioReturns, confidence);

        // Risk decomposition (simplified for historical simulation)
        result.systematicRisk = result.varPercentage * 0.6;  // 60% systematic
        result.idiosyncraticRisk = result.varPercentage * 0.4; // 40% idiosyncratic
        result.correlationRisk = calculateCorrelationRisk(assets, weights);

        result.isValid = true;
        return result;
    }
    catch (const std::exception& e) {
        handleCalculationError("calculateHistoricalVaR", e);
        result.isValid = false;
        return result;
    }
}

VaRResult VaRCalculator::calculateMonteCarloVaR(const std::vector<std::string>& assets,
                                                const std::vector<double>& weights,
                                                VaRConfidenceLevel confidence,
                                                const MonteCarloConfig& config) {
    VaRResult result;
    result.methodology = VaRMethodology::MONTE_CARLO;

    try {
        // Simplified Monte Carlo simulation
        std::vector<double> simulatedReturns;
        simulatedReturns.reserve(config.simulationCount);

        std::random_device rd;
        std::mt19937 gen(config.randomSeed != 0 ? config.randomSeed : rd());
        std::normal_distribution<> normalDist(0.0, 0.02); // 2% daily volatility

        for (int sim = 0; sim < config.simulationCount; ++sim) {
            double portfolioReturn = 0.0;

            for (size_t i = 0; i < assets.size(); ++i) {
                double assetReturn = normalDist(gen);
                portfolioReturn += weights[i] * assetReturn;
            }

            simulatedReturns.push_back(portfolioReturn);
        }

        // Sort and find VaR
        std::sort(simulatedReturns.begin(), simulatedReturns.end());

        double percentile = (100.0 - static_cast<double>(confidence)) / 100.0;
        size_t index = static_cast<size_t>(percentile * simulatedReturns.size());
        index = std::min(index, simulatedReturns.size() - 1);

        result.varPercentage = -simulatedReturns[index]; // Convert to loss

        // Calculate dollar VaR
        double portfolioValue = 1000000.0; // Default value
        result.portfolioValue = portfolioValue;
        result.varAmount = portfolioValue * result.varPercentage;

        // Calculate conditional VaR from simulations
        result.conditionalVaR = calculateConditionalVaR(simulatedReturns, confidence);

        // Risk decomposition
        result.systematicRisk = result.varPercentage * 0.65;  // 65% systematic
        result.idiosyncraticRisk = result.varPercentage * 0.35; // 35% idiosyncratic
        result.correlationRisk = calculateCorrelationRisk(assets, weights);

        result.isValid = true;
        return result;
    }
    catch (const std::exception& e) {
        handleCalculationError("calculateMonteCarloVaR", e);
        result.isValid = false;
        return result;
    }
}

VaRResult VaRCalculator::calculateCornishFisherVaR(const std::vector<std::string>& assets,
                                                   const std::vector<double>& weights,
                                                   VaRConfidenceLevel confidence) {
    VaRResult result;
    result.methodology = VaRMethodology::CORNISH_FISHER;

    try {
        // Start with parametric VaR
        result = calculateParametricVaR(assets, weights, confidence);

        // Get portfolio returns for higher moments calculation
        std::vector<double> portfolioReturns = calculatePortfolioReturns(assets, weights, 252);

        if (portfolioReturns.empty()) {
            throw std::runtime_error("No portfolio returns available for Cornish-Fisher adjustment");
        }

        // Calculate higher moments
        double skewness = calculateSkewness(portfolioReturns);
        double kurtosis = calculateKurtosis(portfolioReturns);

        // Apply Cornish-Fisher adjustment
        double zScore = VaRUtils::getZScore(confidence);
        double adjustedZScore = cornishFisherAdjustment(zScore, skewness, kurtosis);

        // Recalculate VaR with adjusted z-score
        std::vector<std::vector<double>> covMatrix = calculateCovarianceMatrix(assets);
        double portfolioVolatility = std::sqrt(calculatePortfolioVariance(weights, covMatrix));

        result.varPercentage = portfolioVolatility * adjustedZScore;
        result.varAmount = result.portfolioValue * result.varPercentage;

        result.methodology = VaRMethodology::CORNISH_FISHER;
        result.isValid = true;

        return result;
    }
    catch (const std::exception& e) {
        handleCalculationError("calculateCornishFisherVaR", e);
        result.isValid = false;
        return result;
    }
}

// === Risk calculation utilities ===

double VaRCalculator::calculateConditionalVaR(const std::vector<double>& returns,
                                              VaRConfidenceLevel confidence) {
    if (returns.empty()) return 0.0;

    std::vector<double> sortedReturns = returns;
    std::sort(sortedReturns.begin(), sortedReturns.end());

    double percentile = (100.0 - static_cast<double>(confidence)) / 100.0;
    size_t cutoffIndex = static_cast<size_t>(percentile * sortedReturns.size());

    if (cutoffIndex == 0) return 0.0;

    double sum = 0.0;
    for (size_t i = 0; i < cutoffIndex; ++i) {
        sum += sortedReturns[i];
    }

    return -sum / cutoffIndex; // Convert to positive loss
}

std::map<std::string, double> VaRCalculator::calculateComponentVaR(const std::vector<std::string>& assets,
                                                                   const std::vector<double>& weights,
                                                                   VaRMethodology methodology) {
    (void)methodology; // Suppress unused parameter warning
    std::map<std::string, double> componentVaR;

    try {
        // Simplified component VaR calculation
        for (size_t i = 0; i < assets.size(); ++i) {
            componentVaR[assets[i]] = weights[i] * 0.02; // 2% risk contribution
        }

    } catch (const std::exception& e) {
        std::cerr << "[VaRCalculator] Error calculating component VaR: " << e.what() << std::endl;
    }

    return componentVaR;
}

std::map<std::string, double> VaRCalculator::calculateIncrementalVaR(const std::vector<std::string>& assets,
                                                                     const std::vector<double>& weights,
                                                                     VaRMethodology methodology) {
    (void)methodology; // Suppress unused parameter warning
    std::map<std::string, double> incrementalVaR;

    try {
        // Simplified incremental VaR calculation
        for (size_t i = 0; i < assets.size(); ++i) {
            incrementalVaR[assets[i]] = weights[i] * 0.015; // 1.5% marginal contribution
        }

    } catch (const std::exception& e) {
        std::cerr << "[VaRCalculator] Error calculating incremental VaR: " << e.what() << std::endl;
    }

    return incrementalVaR;
}

double VaRCalculator::calculateMaximumDrawdown(const std::vector<double>& portfolioValues) {
    if (portfolioValues.empty()) return 0.0;

    double maxDrawdown = 0.0;
    double peak = portfolioValues[0];

    for (double value : portfolioValues) {
        if (value > peak) {
            peak = value;
        } else {
            double drawdown = (peak - value) / peak;
            maxDrawdown = std::max(maxDrawdown, drawdown);
        }
    }

    return maxDrawdown;
}

// === Correlation integration methods ===

void VaRCalculator::updateCorrelationMatrix() {
    if (!correlationMonitor_) {
        std::cerr << "[VaRCalculator] No correlation monitor available" << std::endl;
        return;
    }

    try {
        correlationMatrix_.clear();

        auto correlations = correlationMonitor_->getCurrentCorrelations();
        for (const auto& snapshot : correlations) {
            correlationMatrix_[{snapshot.pair.crypto_symbol, snapshot.pair.traditional_symbol}] = snapshot.correlation;
            correlationMatrix_[{snapshot.pair.traditional_symbol, snapshot.pair.crypto_symbol}] = snapshot.correlation;
        }

        std::cout << "[VaRCalculator] Updated correlation matrix with "
                  << correlationMatrix_.size() << " correlation pairs" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[VaRCalculator] Error updating correlation matrix: " << e.what() << std::endl;
    }
}

std::map<std::pair<std::string, std::string>, double> VaRCalculator::getCorrelationMatrix() const {
    return correlationMatrix_;
}

double VaRCalculator::calculateCorrelationRisk(const std::vector<std::string>& assets,
                                               const std::vector<double>& weights) {
    if (assets.size() != weights.size() || assets.empty()) {
        return 0.0;
    }

    double correlationRisk = 0.0;
    double totalWeight = 0.0;

    // Calculate weighted correlation risk
    for (size_t i = 0; i < assets.size(); ++i) {
        for (size_t j = i + 1; j < assets.size(); ++j) {
            auto corrIt = correlationMatrix_.find({assets[i], assets[j]});
            if (corrIt != correlationMatrix_.end()) {
                double correlation = std::abs(corrIt->second); // Use absolute correlation
                double weightProduct = weights[i] * weights[j];
                correlationRisk += correlation * weightProduct;
                totalWeight += weightProduct;
            }
        }
    }

    return (totalWeight > 0) ? correlationRisk / totalWeight : 0.0;
}

// === Utility methods ===

double VaRCalculator::calculatePortfolioVariance(const std::vector<double>& weights,
                                                 const std::vector<std::vector<double>>& covarianceMatrix) {
    if (weights.size() != covarianceMatrix.size()) {
        throw std::invalid_argument("Weights and covariance matrix dimensions mismatch");
    }

    double variance = 0.0;
    size_t n = weights.size();

    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            if (i < covarianceMatrix.size() && j < covarianceMatrix[i].size()) {
                variance += weights[i] * weights[j] * covarianceMatrix[i][j];
            }
        }
    }

    return variance;
}

std::vector<std::vector<double>> VaRCalculator::calculateCovarianceMatrix(const std::vector<std::string>& assets) {
    size_t n = assets.size();
    std::vector<std::vector<double>> covMatrix(n, std::vector<double>(n, 0.0));

    try {
        // Simplified covariance matrix calculation
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = 0; j < n; ++j) {
                if (i == j) {
                    covMatrix[i][j] = 0.0004; // 2% daily volatility squared
                } else {
                    auto corrIt = correlationMatrix_.find({assets[i], assets[j]});
                    if (corrIt != correlationMatrix_.end()) {
                        double correlation = corrIt->second;
                        double stdDev_i = 0.02; // 2% volatility
                        double stdDev_j = 0.02; // 2% volatility
                        covMatrix[i][j] = correlation * stdDev_i * stdDev_j;
                    } else {
                        covMatrix[i][j] = 0.0001; // Low correlation
                    }
                }
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "[VaRCalculator] Error calculating covariance matrix: " << e.what() << std::endl;

        // Return identity-like matrix with default volatilities
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = 0; j < n; ++j) {
                if (i == j) {
                    covMatrix[i][j] = 0.0004; // 2% daily volatility squared
                } else {
                    covMatrix[i][j] = 0.0001; // Low correlation
                }
            }
        }
    }

    return covMatrix;
}

double VaRCalculator::calculateSkewness(const std::vector<double>& returns) {
    if (returns.size() < 3) return 0.0;

    double mean = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();

    double m2 = 0.0, m3 = 0.0;
    for (double ret : returns) {
        double diff = ret - mean;
        m2 += diff * diff;
        m3 += diff * diff * diff;
    }

    m2 /= returns.size();
    m3 /= returns.size();

    double stdDev = std::sqrt(m2);
    return (stdDev > 0) ? m3 / (stdDev * stdDev * stdDev) : 0.0;
}

double VaRCalculator::calculateKurtosis(const std::vector<double>& returns) {
    if (returns.size() < 4) return 3.0; // Normal distribution kurtosis

    double mean = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();

    double m2 = 0.0, m4 = 0.0;
    for (double ret : returns) {
        double diff = ret - mean;
        double diff2 = diff * diff;
        m2 += diff2;
        m4 += diff2 * diff2;
    }

    m2 /= returns.size();
    m4 /= returns.size();

    return (m2 > 0) ? m4 / (m2 * m2) : 3.0;
}

double VaRCalculator::cornishFisherAdjustment(double zScore, double skewness, double kurtosis) {
    // Cornish-Fisher expansion to third order
    double z2 = zScore * zScore;
    double z3 = z2 * zScore;

    // Third order adjustment
    double cf = zScore +
                (skewness / 6.0) * (z2 - 1.0) +
                ((kurtosis - 3.0) / 24.0) * zScore * (z2 - 3.0) +
                (skewness * skewness / 72.0) * zScore * (2.0 * z2 - 5.0);

    return cf;
}

std::vector<double> VaRCalculator::getAssetReturns(const std::string& asset, int lookbackDays) {
    // Check if we have cached returns
    auto it = historicalReturns_.find(asset);
    if (it != historicalReturns_.end() && !it->second.empty()) {
        // Return the requested number of days (or all available)
        size_t dataPoints = std::min(static_cast<size_t>(lookbackDays), it->second.size());
        return std::vector<double>(it->second.end() - dataPoints, it->second.end());
    }

    // Generate mock historical returns for demonstration
    std::vector<double> returns;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<> dist(0.0, 0.02); // 2% daily volatility

    for (int i = 0; i < lookbackDays; ++i) {
        returns.push_back(dist(gen));
    }

    // Cache the generated returns
    historicalReturns_[asset] = returns;

    return returns;
}

std::vector<double> VaRCalculator::calculatePortfolioReturns(const std::vector<std::string>& assets,
                                                            const std::vector<double>& weights,
                                                            int lookbackDays) {
    if (assets.size() != weights.size() || assets.empty()) {
        return {};
    }

    // Get returns for all assets
    std::vector<std::vector<double>> assetReturns;
    size_t minDataPoints = std::numeric_limits<size_t>::max();

    for (const auto& asset : assets) {
        auto returns = getAssetReturns(asset, lookbackDays);
        assetReturns.push_back(returns);
        minDataPoints = std::min(minDataPoints, returns.size());
    }

    if (minDataPoints == 0) {
        return {};
    }

    // Calculate portfolio returns
    std::vector<double> portfolioReturns;
    portfolioReturns.reserve(minDataPoints);

    for (size_t t = 0; t < minDataPoints; ++t) {
        double portfolioReturn = 0.0;
        for (size_t i = 0; i < assets.size(); ++i) {
            if (t < assetReturns[i].size()) {
                portfolioReturn += weights[i] * assetReturns[i][t];
            }
        }
        portfolioReturns.push_back(portfolioReturn);
    }

    return portfolioReturns;
}

// === Validation and error handling ===

bool VaRCalculator::validateTRSLimits(const VaRResult& result) {
    // Check basic validity
    if (!result.isValid || result.varPercentage < 0) {
        return false;
    }

    // Check VaR limits based on confidence level
    if (result.confidenceLevel == VaRConfidenceLevel::PERCENT_95) {
        if (result.varPercentage > trsParams_.maxDailyVaR95) {
            return false;
        }
    }

    if (result.confidenceLevel == VaRConfidenceLevel::PERCENT_99) {
        if (result.varPercentage > trsParams_.maxDailyVaR99) {
            return false;
        }
    }

    // Check performance requirements
    if (result.calculationDuration.count() > trsParams_.maxCalculationTimeMs) {
        // Warning but not failure
        std::cout << "[VaRCalculator] Warning: Calculation time "
                  << result.calculationDuration.count() << "ms exceeds target "
                  << trsParams_.maxCalculationTimeMs << "ms" << std::endl;
    }

    return true;
}

bool VaRCalculator::validateInputData(const std::vector<std::string>& assets,
                                      const std::vector<double>& weights) {
    if (assets.empty() || weights.empty() || assets.size() != weights.size()) {
        return false;
    }

    // Check weight constraints
    double totalWeight = std::accumulate(weights.begin(), weights.end(), 0.0);
    if (std::abs(totalWeight - 1.0) > 0.01) { // Allow 1% tolerance
        return false;
    }

    // Check for negative weights
    for (double weight : weights) {
        if (weight < -0.01) {
            return false;
        }
    }

    return true;
}

void VaRCalculator::handleCalculationError(const std::string& method, const std::exception& e) {
    std::cerr << "[VaRCalculator] Error in " << method << ": " << e.what() << std::endl;
}

void VaRCalculator::logPerformanceMetrics(const std::string& method,
                                         std::chrono::milliseconds duration) {
    if (duration.count() > trsParams_.maxCalculationTimeMs) {
        std::cout << "[VaRCalculator] Performance Warning: " << method
                  << " took " << duration.count() << "ms (target: "
                  << trsParams_.maxCalculationTimeMs << "ms)" << std::endl;
    }
}

// === Performance benchmarking methods ===

VaRCalculator::PerformanceMetrics VaRCalculator::benchmarkPerformance(int iterations) {
    PerformanceMetrics metrics;

    // Test portfolio
    std::vector<std::string> testAssets = {"BTC", "ETH", "ADA"};
    std::vector<double> testWeights = {0.4, 0.35, 0.25};

    std::vector<std::chrono::milliseconds> times;
    times.reserve(iterations);

    // Run benchmark iterations
    for (int i = 0; i < iterations; ++i) {
        auto startTime = std::chrono::high_resolution_clock::now();

        try {
            auto result = calculateVaR(testAssets, testWeights, VaRMethodology::PARAMETRIC);
            (void)result; // Suppress unused variable warning
        } catch (...) {
            // Ignore errors for benchmarking
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        times.push_back(duration);
    }

    // Calculate metrics
    if (!times.empty()) {
        double totalTime = 0.0;
        std::chrono::milliseconds maxTime(0);

        for (auto time : times) {
            totalTime += time.count();
            maxTime = std::max(maxTime, time);
        }

        metrics.averageCalculationTime = totalTime / times.size();
        metrics.maxCalculationTime = maxTime.count();
        metrics.meetsPerformanceTarget = (metrics.averageCalculationTime <= trsParams_.maxCalculationTimeMs);
        metrics.calculationsPerSecond = (metrics.averageCalculationTime > 0) ?
            static_cast<int>(1000.0 / metrics.averageCalculationTime) : 0;

        // Store methodology-specific times (simplified for demo)
        metrics.calculationTimes[VaRMethodology::PARAMETRIC] =
            std::chrono::milliseconds(static_cast<int>(metrics.averageCalculationTime));
    }

    return metrics;
}

// === Backtesting methods ===

VaRBacktester::BacktestResult VaRCalculator::backtest(VaRMethodology methodology,
                                                      VaRConfidenceLevel confidence,
                                                      int backtestDays) {
    if (!backtester_) {
        throw std::runtime_error("Backtester not initialized");
    }

    try {
        // Generate mock data for demonstration
        std::vector<double> mockReturns;
        std::vector<double> mockVarEstimates;

        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<> dist(0.0, 0.02); // 2% daily volatility

        for (int i = 0; i < backtestDays; ++i) {
            double return_ = dist(gen);
            double varEstimate = 0.03; // 3% daily VaR estimate

            mockReturns.push_back(return_);
            mockVarEstimates.push_back(varEstimate);
        }

        return backtester_->runBacktest(methodology, confidence, mockReturns, mockVarEstimates);

    } catch (const std::exception& e) {
        handleCalculationError("backtest", e);
        throw;
    }
}

// === VaRUtils Implementation ===

namespace VaRUtils {

double getZScore(VaRConfidenceLevel confidence) {
    switch (confidence) {
        case VaRConfidenceLevel::PERCENT_90: return 1.282;
        case VaRConfidenceLevel::PERCENT_95: return 1.645;
        case VaRConfidenceLevel::PERCENT_99: return 2.326;
        case VaRConfidenceLevel::PERCENT_999: return 3.090;
        default: return 1.645; // Default to 95%
    }
}

double scaleVaRToHorizon(double dailyVaR, VaRTimeHorizon horizon) {
    switch (horizon) {
        case VaRTimeHorizon::INTRADAY_1H: return dailyVaR / std::sqrt(24.0);
        case VaRTimeHorizon::INTRADAY_4H: return dailyVaR / std::sqrt(6.0);
        case VaRTimeHorizon::DAILY: return dailyVaR;
        case VaRTimeHorizon::WEEKLY: return dailyVaR * std::sqrt(7.0);
        case VaRTimeHorizon::MONTHLY: return dailyVaR * std::sqrt(30.0);
        default: return dailyVaR;
    }
}

} // namespace VaRUtils

} // namespace Risk
} // namespace CryptoClaude