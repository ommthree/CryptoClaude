#include "StressTestingEngine.h"
#include "../Utils/DateTimeUtils.h"
#include <algorithm>
#include <numeric>
#include <random>
#include <cmath>
#include <iostream>
#include <fstream>
#include <sstream>

namespace CryptoClaude {
namespace Risk {

// Constructor
StressTestingEngine::StressTestingEngine()
    : isInitialized_(false)
    , monitoringActive_(false)
    , currentRegime_(MarketRegime::SIDEWAYS)
    , lastStressTestTime_(std::chrono::system_clock::now())
    , lastDashboardUpdate_(std::chrono::system_clock::now()) {

    // Initialize historical scenarios with default configurations
    initializeHistoricalScenarios();
}

StressTestingEngine::~StressTestingEngine() {
    stopRealTimeMonitoring();
}

bool StressTestingEngine::initialize(std::shared_ptr<VaRCalculator> varCalculator,
                                   std::shared_ptr<CrossAssetCorrelationMonitor> correlationMonitor,
                                   std::shared_ptr<CryptoClaude::Optimization::PortfolioOptimizer> optimizer) {
    if (!varCalculator || !correlationMonitor || !optimizer) {
        std::cerr << "StressTestingEngine: Invalid components provided" << std::endl;
        return false;
    }

    varCalculator_ = varCalculator;
    correlationMonitor_ = correlationMonitor;
    portfolioOptimizer_ = optimizer;

    // Initialize default configurations
    detectionConfig_ = StressDetectionConfig{};
    trsParams_ = TRSRiskParameters{};

    // Setup dashboard data
    dashboardData_ = StressDashboardData{};

    isInitialized_ = true;

    std::cout << "StressTestingEngine: Initialized successfully with integrated VaR framework" << std::endl;
    return true;
}

void StressTestingEngine::initializeHistoricalScenarios() {
    // 2008 Financial Crisis configuration
    HistoricalStressConfig crisis2008;
    crisis2008.scenario = StressScenarioType::FINANCIAL_CRISIS_2008;
    crisis2008.maxMarketDrop = -0.54;  // 54% drop
    crisis2008.durationDays = 517;     // Lehman to market bottom
    crisis2008.volatilityMultiplier = 3.5;
    crisis2008.correlationSpike = 0.85;
    crisis2008.recoveryDays = 1825;    // 5 years to full recovery
    crisis2008.recoveryRate = 0.001;
    crisis2008.hadSecondaryShocks = true;
    crisis2008.description = "Lehman Brothers collapse and global financial crisis";
    historicalConfigs_[StressScenarioType::FINANCIAL_CRISIS_2008] = crisis2008;

    // COVID-19 2020 Crash configuration
    HistoricalStressConfig covid2020;
    covid2020.scenario = StressScenarioType::COVID_CRASH_2020;
    covid2020.maxMarketDrop = -0.34;   // 34% drop in 33 days
    covid2020.durationDays = 33;
    covid2020.volatilityMultiplier = 4.2;
    covid2020.correlationSpike = 0.92;  // Everything moved together
    covid2020.recoveryDays = 148;       // V-shaped recovery
    covid2020.recoveryRate = 0.007;
    covid2020.hadSecondaryShocks = false;
    covid2020.description = "COVID-19 pandemic market crash";
    historicalConfigs_[StressScenarioType::COVID_CRASH_2020] = covid2020;

    // LUNA Collapse 2022 configuration
    HistoricalStressConfig luna2022;
    luna2022.scenario = StressScenarioType::LUNA_COLLAPSE_2022;
    luna2022.maxMarketDrop = -0.85;    // 85% crypto market drop
    luna2022.durationDays = 7;         // Rapid collapse
    luna2022.volatilityMultiplier = 6.0;
    luna2022.correlationSpike = 0.88;
    luna2022.recoveryDays = 180;       // Partial recovery only
    luna2022.recoveryRate = 0.003;
    luna2022.hadSecondaryShocks = true;
    luna2022.assetSpecificImpacts = {
        {"LUNA", -0.9999}, {"UST", -0.99}, {"BTC", -0.45}, {"ETH", -0.55}
    };
    luna2022.description = "Terra LUNA/UST algorithmic stablecoin collapse";
    historicalConfigs_[StressScenarioType::LUNA_COLLAPSE_2022] = luna2022;

    // FTX Collapse 2022 configuration
    HistoricalStressConfig ftx2022;
    ftx2022.scenario = StressScenarioType::FTX_COLLAPSE_2022;
    ftx2022.maxMarketDrop = -0.25;     // 25% crypto market drop
    ftx2022.durationDays = 14;         // Two-week crisis
    ftx2022.volatilityMultiplier = 3.8;
    ftx2022.correlationSpike = 0.82;
    ftx2022.recoveryDays = 90;
    ftx2022.recoveryRate = 0.005;
    ftx2022.hadSecondaryShocks = true;
    ftx2022.assetSpecificImpacts = {
        {"FTT", -0.95}, {"SOL", -0.65}, {"BTC", -0.20}, {"ETH", -0.25}
    };
    ftx2022.description = "FTX exchange collapse and contagion";
    historicalConfigs_[StressScenarioType::FTX_COLLAPSE_2022] = ftx2022;
}

// === MAIN STRESS TESTING METHODS ===

StressTestResult StressTestingEngine::runStressTest(StressScenarioType scenario,
                                                   const std::vector<std::string>& assets,
                                                   const std::vector<double>& weights,
                                                   StressSeverity severity) {
    if (!isInitialized_) {
        return createErrorResult(scenario, "StressTestingEngine not initialized");
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    validateStressTestInputs(assets, weights);

    StressTestResult result;
    result.scenario = scenario;
    result.severity = severity;
    result.testTime = std::chrono::system_clock::now();
    result.testValid = true;

    try {
        // Calculate current portfolio value
        result.portfolioValue = calculatePortfolioValue(assets, weights);

        // Generate appropriate stress scenario
        switch (scenario) {
            case StressScenarioType::FLASH_CRASH:
                return runFlashCrashScenario(assets, weights, severity);

            case StressScenarioType::LIQUIDITY_CRISIS:
                return runLiquidityCrisisScenario(assets, weights, severity);

            case StressScenarioType::CORRELATION_BREAKDOWN:
                return runCorrelationBreakdownScenario(assets, weights, severity);

            case StressScenarioType::VOLATILITY_SPIKE:
                return runVolatilitySpike(assets, weights, severity);

            case StressScenarioType::FINANCIAL_CRISIS_2008:
                return run2008FinancialCrisis(assets, weights);

            case StressScenarioType::COVID_CRASH_2020:
                return runCOVID2020Crash(assets, weights);

            case StressScenarioType::LUNA_COLLAPSE_2022:
                return runLUNACollapse2022(assets, weights);

            case StressScenarioType::FTX_COLLAPSE_2022:
                return runFTXCollapse2022(assets, weights);

            default:
                // Run generic stress test
                return runGenericStressScenario(scenario, assets, weights, severity);
        }

    } catch (const std::exception& e) {
        handleStressTestError("StressTest_" + std::to_string(static_cast<int>(scenario)), e);
        return createErrorResult(scenario, e.what());
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    result.calculationDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    lastStressTestTime_ = std::chrono::system_clock::now();

    return result;
}

std::map<StressScenarioType, StressTestResult> StressTestingEngine::runComprehensiveStressTests(
    const std::vector<std::string>& assets,
    const std::vector<double>& weights) {

    if (!isInitialized_) {
        std::map<StressScenarioType, StressTestResult> errorResults;
        errorResults[StressScenarioType::FLASH_CRASH] = createErrorResult(
            StressScenarioType::FLASH_CRASH, "StressTestingEngine not initialized");
        return errorResults;
    }

    std::map<StressScenarioType, StressTestResult> results;
    auto startTime = std::chrono::high_resolution_clock::now();

    std::cout << "Running comprehensive stress test suite on " << assets.size() << " assets..." << std::endl;

    // Core market stress scenarios
    std::vector<StressScenarioType> coreScenarios = {
        StressScenarioType::FLASH_CRASH,
        StressScenarioType::LIQUIDITY_CRISIS,
        StressScenarioType::CORRELATION_BREAKDOWN,
        StressScenarioType::VOLATILITY_SPIKE,
        StressScenarioType::MOMENTUM_REVERSAL,
        StressScenarioType::CRYPTO_WINTER
    };

    // Historical event scenarios
    std::vector<StressScenarioType> historicalScenarios = {
        StressScenarioType::FINANCIAL_CRISIS_2008,
        StressScenarioType::COVID_CRASH_2020,
        StressScenarioType::LUNA_COLLAPSE_2022,
        StressScenarioType::FTX_COLLAPSE_2022
    };

    // Extreme tail events
    std::vector<StressScenarioType> extremeScenarios = {
        StressScenarioType::EXCHANGE_HACK,
        StressScenarioType::REGULATORY_CRACKDOWN,
        StressScenarioType::STABLECOIN_DEPEG,
        StressScenarioType::MARKET_MANIPULATION
    };

    // Run all scenarios with different severity levels
    for (auto scenario : coreScenarios) {
        try {
            results[scenario] = runStressTest(scenario, assets, weights, StressSeverity::SEVERE);
            std::cout << "Completed " << getScenarioName(scenario) << " stress test" << std::endl;
        } catch (const std::exception& e) {
            results[scenario] = createErrorResult(scenario, e.what());
        }
    }

    for (auto scenario : historicalScenarios) {
        try {
            results[scenario] = runStressTest(scenario, assets, weights, StressSeverity::SEVERE);
            std::cout << "Completed " << getScenarioName(scenario) << " historical stress test" << std::endl;
        } catch (const std::exception& e) {
            results[scenario] = createErrorResult(scenario, e.what());
        }
    }

    for (auto scenario : extremeScenarios) {
        try {
            results[scenario] = runStressTest(scenario, assets, weights, StressSeverity::EXTREME);
            std::cout << "Completed " << getScenarioName(scenario) << " extreme stress test" << std::endl;
        } catch (const std::exception& e) {
            results[scenario] = createErrorResult(scenario, e.what());
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    std::cout << "Comprehensive stress test suite completed in " << totalDuration.count() << "ms" << std::endl;
    std::cout << "Total scenarios tested: " << results.size() << std::endl;

    return results;
}

// === SPECIFIC STRESS SCENARIOS ===

StressTestResult StressTestingEngine::runFlashCrashScenario(const std::vector<std::string>& assets,
                                                           const std::vector<double>& weights,
                                                           StressSeverity severity) {
    auto startTime = std::chrono::high_resolution_clock::now();

    StressTestResult result;
    result.scenario = StressScenarioType::FLASH_CRASH;
    result.severity = severity;
    result.timeHorizon = StressTimeHorizon::IMMEDIATE;
    result.testTime = std::chrono::system_clock::now();

    // Generate flash crash shocks (sudden severe price drops)
    auto assetShocks = generateFlashCrashShocks(assets, severity);
    auto correlationShocks = generateCorrelationShocks(assets, severity);
    auto liquidityShocks = generateLiquidityShocks(assets, severity);

    // Calculate portfolio impact
    result.portfolioValue = calculatePortfolioValue(assets, weights);

    double portfolioLoss = 0.0;
    for (size_t i = 0; i < assets.size(); ++i) {
        double assetValue = result.portfolioValue * weights[i];
        double shockLoss = assetValue * std::abs(assetShocks[assets[i]]);
        double liquidityLoss = assetValue * liquidityShocks[assets[i]];

        result.assetLosses[assets[i]] = shockLoss + liquidityLoss;
        portfolioLoss += result.assetLosses[assets[i]];
    }

    result.stressedValue = result.portfolioValue - portfolioLoss;
    result.absoluteLoss = portfolioLoss;
    result.percentageLoss = portfolioLoss / result.portfolioValue;
    result.maxDrawdown = result.percentageLoss;  // Flash crash is immediate

    // Calculate stress-adjusted VaR
    result.stressedVaR = calculateStressAdjustedVaR(assets, weights,
                                                   StressScenarioType::FLASH_CRASH,
                                                   VaRMethodology::HISTORICAL_SIMULATION);

    // Calculate VaR multiplier
    VaRResult normalVaR = varCalculator_->calculateVaR(assets, weights);
    result.stressVaRMultiplier = result.stressedVaR.varAmount / normalVaR.varAmount;

    // Flash crash recovery analysis
    result.estimatedRecoveryTime = std::chrono::seconds(3600 * 24 * 7); // 1 week typical
    result.recoveryProbability = 0.85; // 85% chance of recovery

    result.recommendedActions = {
        "Immediately halt algorithmic trading",
        "Activate circuit breakers",
        "Increase cash reserves",
        "Contact exchanges for trade cancellation if erroneous",
        "Review position sizing limits"
    };

    // Risk mitigation actions
    result.riskMitigationActions = {
        "Implement volatility-based position sizing",
        "Add real-time anomaly detection",
        "Establish maximum single-trade impact limits",
        "Create emergency liquidity facilities"
    };

    auto endTime = std::chrono::high_resolution_clock::now();
    result.calculationDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    result.testValid = true;

    // Performance validation - should be <500ms
    if (result.calculationDuration.count() > 500) {
        result.warnings.push_back("Calculation exceeded 500ms performance target");
    }

    return result;
}

StressTestResult StressTestingEngine::runLiquidityCrisisScenario(const std::vector<std::string>& assets,
                                                                const std::vector<double>& weights,
                                                                StressSeverity severity) {
    auto startTime = std::chrono::high_resolution_clock::now();

    StressTestResult result;
    result.scenario = StressScenarioType::LIQUIDITY_CRISIS;
    result.severity = severity;
    result.timeHorizon = StressTimeHorizon::SHORT_TERM;
    result.testTime = std::chrono::system_clock::now();

    // Generate liquidity crisis conditions
    auto liquidityShocks = generateLiquidityShocks(assets, severity);
    auto assetShocks = generateMarketStressShocks(assets, severity, 0.3); // Moderate price impact

    result.portfolioValue = calculatePortfolioValue(assets, weights);

    double totalLiquidityCost = 0.0;
    double totalMarketImpact = 0.0;

    for (size_t i = 0; i < assets.size(); ++i) {
        double position = result.portfolioValue * weights[i];

        // Liquidity cost (bid-ask spread widening)
        double liquidityCost = position * liquidityShocks[assets[i]];
        result.liquidityCosts[assets[i]] = liquidityCost;
        totalLiquidityCost += liquidityCost;

        // Market impact cost
        double marketImpact = position * std::abs(assetShocks[assets[i]]);
        totalMarketImpact += marketImpact;

        result.assetLosses[assets[i]] = liquidityCost + marketImpact;
    }

    double totalLoss = totalLiquidityCost + totalMarketImpact;
    result.stressedValue = result.portfolioValue - totalLoss;
    result.absoluteLoss = totalLoss;
    result.percentageLoss = totalLoss / result.portfolioValue;
    result.liquidityAdjustment = totalLiquidityCost / result.portfolioValue;

    // Calculate assets at risk of liquidity problems
    result.liquidityAtRisk = 0.0;
    for (size_t i = 0; i < assets.size(); ++i) {
        if (liquidityShocks[assets[i]] > 0.05) { // >5% liquidity cost
            result.liquidityAtRisk += result.portfolioValue * weights[i];
        }
    }

    // Stress VaR calculation
    result.stressedVaR = calculateStressAdjustedVaR(assets, weights,
                                                   StressScenarioType::LIQUIDITY_CRISIS,
                                                   VaRMethodology::MONTE_CARLO);

    VaRResult normalVaR = varCalculator_->calculateVaR(assets, weights);
    result.stressVaRMultiplier = result.stressedVaR.varAmount / normalVaR.varAmount;

    // Recovery characteristics
    result.estimatedRecoveryTime = std::chrono::seconds(3600 * 24 * 3); // 3 days typical
    result.recoveryProbability = 0.75;

    result.recommendedActions = {
        "Reduce position sizes in illiquid assets",
        "Increase cash buffer for liquidity needs",
        "Diversify across multiple exchanges",
        "Implement liquidity stress testing",
        "Consider market-making partnerships"
    };

    auto endTime = std::chrono::high_resolution_clock::now();
    result.calculationDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    result.testValid = true;

    return result;
}

StressTestResult StressTestingEngine::runCorrelationBreakdownScenario(const std::vector<std::string>& assets,
                                                                     const std::vector<double>& weights,
                                                                     StressSeverity severity) {
    auto startTime = std::chrono::high_resolution_clock::now();

    StressTestResult result;
    result.scenario = StressScenarioType::CORRELATION_BREAKDOWN;
    result.severity = severity;
    result.timeHorizon = StressTimeHorizon::MULTI_DAY;
    result.testTime = std::chrono::system_clock::now();

    result.portfolioValue = calculatePortfolioValue(assets, weights);

    // Get current correlation matrix
    auto baseCorrelations = correlationMonitor_->getCorrelationMatrix();

    // Generate correlation breakdown (correlations spike to extreme levels)
    auto correlationShocks = generateCorrelationShocks(assets, severity);
    result.stressedCorrelations = correlationShocks;

    // Calculate correlation risk
    result.correlationRisk = calculateCorrelationRisk(assets, weights, correlationShocks);

    // Generate asset price shocks that reflect correlation breakdown
    auto assetShocks = generateCorrelationBasedShocks(assets, correlationShocks, severity);

    double portfolioLoss = 0.0;
    for (size_t i = 0; i < assets.size(); ++i) {
        double assetValue = result.portfolioValue * weights[i];
        double loss = assetValue * std::abs(assetShocks[assets[i]]);
        result.assetLosses[assets[i]] = loss;
        portfolioLoss += loss;
    }

    // Add correlation-specific risk premium
    double correlationRiskPremium = portfolioLoss * 0.25; // 25% additional risk
    portfolioLoss += correlationRiskPremium;

    result.stressedValue = result.portfolioValue - portfolioLoss;
    result.absoluteLoss = portfolioLoss;
    result.percentageLoss = portfolioLoss / result.portfolioValue;

    // Calculate diversification failure
    double expectedDiversificationBenefit = calculateDiversificationBenefit(assets, weights, baseCorrelations);
    double actualDiversificationBenefit = calculateDiversificationBenefit(assets, weights, correlationShocks);
    double diversificationLoss = expectedDiversificationBenefit - actualDiversificationBenefit;

    result.maxDrawdown = result.percentageLoss + (diversificationLoss * 0.1);

    // Stress VaR calculation
    result.stressedVaR = calculateStressAdjustedVaR(assets, weights,
                                                   StressScenarioType::CORRELATION_BREAKDOWN,
                                                   VaRMethodology::MONTE_CARLO);

    VaRResult normalVaR = varCalculator_->calculateVaR(assets, weights);
    result.stressVaRMultiplier = result.stressedVaR.varAmount / normalVaR.varAmount;

    result.estimatedRecoveryTime = std::chrono::seconds(3600 * 24 * 14); // 2 weeks
    result.recoveryProbability = 0.65;

    result.recommendedActions = {
        "Rebalance portfolio to reduce concentration",
        "Increase position in uncorrelated assets",
        "Consider alternative risk models",
        "Implement dynamic correlation monitoring",
        "Review portfolio construction methodology"
    };

    auto endTime = std::chrono::high_resolution_clock::now();
    result.calculationDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    result.testValid = true;

    return result;
}

StressTestResult StressTestingEngine::runVolatilitySpike(const std::vector<std::string>& assets,
                                                        const std::vector<double>& weights,
                                                        StressSeverity severity) {
    auto startTime = std::chrono::high_resolution_clock::now();

    StressTestResult result;
    result.scenario = StressScenarioType::VOLATILITY_SPIKE;
    result.severity = severity;
    result.timeHorizon = StressTimeHorizon::INTRADAY;
    result.testTime = std::chrono::system_clock::now();

    result.portfolioValue = calculatePortfolioValue(assets, weights);

    // Generate volatility shocks
    auto volatilityShocks = generateVolatilityShocks(assets, severity);
    auto assetShocks = generateVolatilityBasedPriceShocks(assets, volatilityShocks, severity);

    double portfolioLoss = 0.0;
    double maxVolatilityIncrease = 0.0;

    for (size_t i = 0; i < assets.size(); ++i) {
        double assetValue = result.portfolioValue * weights[i];
        double volatilityFactor = volatilityShocks[assets[i]];
        maxVolatilityIncrease = std::max(maxVolatilityIncrease, volatilityFactor);

        // Calculate loss based on volatility-adjusted shock
        double loss = assetValue * std::abs(assetShocks[assets[i]]);
        result.assetLosses[assets[i]] = loss;
        portfolioLoss += loss;
    }

    result.stressedValue = result.portfolioValue - portfolioLoss;
    result.absoluteLoss = portfolioLoss;
    result.percentageLoss = portfolioLoss / result.portfolioValue;

    // For volatility spikes, the max drawdown includes uncertainty premium
    result.maxDrawdown = result.percentageLoss * (1.0 + maxVolatilityIncrease * 0.5);

    // Stress VaR calculation (volatility spikes affect all VaR models)
    result.stressedVaR = calculateStressAdjustedVaR(assets, weights,
                                                   StressScenarioType::VOLATILITY_SPIKE,
                                                   VaRMethodology::PARAMETRIC);

    VaRResult normalVaR = varCalculator_->calculateVaR(assets, weights);
    result.stressVaRMultiplier = result.stressedVaR.varAmount / normalVaR.varAmount;

    result.estimatedRecoveryTime = std::chrono::seconds(3600 * 6); // 6 hours typical
    result.recoveryProbability = 0.80;

    result.recommendedActions = {
        "Reduce leverage immediately",
        "Implement volatility-based position sizing",
        "Increase hedging positions",
        "Monitor options markets for fear signals",
        "Consider volatility targeting strategies"
    };

    result.riskMitigationActions = {
        "Implement real-time volatility monitoring",
        "Add volatility circuit breakers",
        "Use volatility-adjusted risk limits",
        "Consider volatility derivatives for hedging"
    };

    auto endTime = std::chrono::high_resolution_clock::now();
    result.calculationDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    result.testValid = true;

    return result;
}

// === HISTORICAL EVENT STRESS TESTS ===

StressTestResult StressTestingEngine::run2008FinancialCrisis(const std::vector<std::string>& assets,
                                                            const std::vector<double>& weights) {
    auto config = historicalConfigs_[StressScenarioType::FINANCIAL_CRISIS_2008];
    return runHistoricalScenario(config, assets, weights);
}

StressTestResult StressTestingEngine::runCOVID2020Crash(const std::vector<std::string>& assets,
                                                       const std::vector<double>& weights) {
    auto config = historicalConfigs_[StressScenarioType::COVID_CRASH_2020];
    return runHistoricalScenario(config, assets, weights);
}

StressTestResult StressTestingEngine::runLUNACollapse2022(const std::vector<std::string>& assets,
                                                         const std::vector<double>& weights) {
    auto config = historicalConfigs_[StressScenarioType::LUNA_COLLAPSE_2022];
    return runHistoricalScenario(config, assets, weights);
}

StressTestResult StressTestingEngine::runFTXCollapse2022(const std::vector<std::string>& assets,
                                                        const std::vector<double>& weights) {
    auto config = historicalConfigs_[StressScenarioType::FTX_COLLAPSE_2022];
    return runHistoricalScenario(config, assets, weights);
}

// === STRESS-ADJUSTED VAR INTEGRATION ===

VaRResult StressTestingEngine::calculateStressAdjustedVaR(const std::vector<std::string>& assets,
                                                         const std::vector<double>& weights,
                                                         StressScenarioType scenario,
                                                         VaRMethodology methodology) {
    if (!varCalculator_) {
        throw std::runtime_error("VaRCalculator not available");
    }

    // Calculate base VaR
    VaRResult baseVaR = varCalculator_->calculateVaR(assets, weights, methodology);

    // Generate stress scenario shocks
    auto assetShocks = getHistoricalShocks(scenario, assets);
    auto correlationShocks = generateCorrelationShocks(assets, StressSeverity::SEVERE);

    // Apply stress adjustments to VaR
    VaRResult stressedVaR = applyStressToVaR(baseVaR, assetShocks, correlationShocks);

    // Add stress-specific adjustments
    double stressMultiplier = getStressVaRMultiplier(scenario);
    stressedVaR.varAmount *= stressMultiplier;
    stressedVaR.conditionalVaR *= stressMultiplier * 1.2; // CVaR increases more

    // Update metadata
    stressedVaR.warnings.push_back("Stress-adjusted VaR for " + getScenarioName(scenario));

    return stressedVaR;
}

std::map<StressScenarioType, VaRResult> StressTestingEngine::calculateMultiScenarioStressVaR(
    const std::vector<std::string>& assets,
    const std::vector<double>& weights) {

    std::map<StressScenarioType, VaRResult> results;

    std::vector<StressScenarioType> scenarios = {
        StressScenarioType::FLASH_CRASH,
        StressScenarioType::LIQUIDITY_CRISIS,
        StressScenarioType::CORRELATION_BREAKDOWN,
        StressScenarioType::VOLATILITY_SPIKE,
        StressScenarioType::FINANCIAL_CRISIS_2008,
        StressScenarioType::COVID_CRASH_2020,
        StressScenarioType::LUNA_COLLAPSE_2022,
        StressScenarioType::FTX_COLLAPSE_2022
    };

    for (auto scenario : scenarios) {
        try {
            results[scenario] = calculateStressAdjustedVaR(assets, weights, scenario);
        } catch (const std::exception& e) {
            // Create error VaR result
            VaRResult errorResult;
            errorResult.isValid = false;
            errorResult.warnings.push_back("Error calculating stress VaR: " + std::string(e.what()));
            results[scenario] = errorResult;
        }
    }

    return results;
}

// === REAL-TIME STRESS DETECTION ===

StressDetectionResult StressTestingEngine::detectMarketStress(const std::map<std::string, MarketData>& currentData) {
    StressDetectionResult result;
    result.stressDetected = false;
    result.detectionTime = std::chrono::system_clock::now();
    result.stressIntensity = 0.0;
    result.currentRegime = currentRegime_;

    // Update current market data
    currentMarketData_ = currentData;

    // Run detection algorithms
    result.flashCrashDetected = detectFlashCrash(currentData);
    result.volatilitySpikeDetected = detectVolatilitySpike(currentData);
    result.correlationSpikeDetected = detectCorrelationSpike(currentData);
    result.liquidityCrisisDetected = detectLiquidityCrisis(currentData);

    // Determine overall stress level
    int stressIndicators = 0;
    if (result.flashCrashDetected) stressIndicators++;
    if (result.volatilitySpikeDetected) stressIndicators++;
    if (result.correlationSpikeDetected) stressIndicators++;
    if (result.liquidityCrisisDetected) stressIndicators++;

    if (stressIndicators > 0) {
        result.stressDetected = true;
        result.stressIntensity = std::min(1.0, static_cast<double>(stressIndicators) / 4.0);

        // Identify detected scenarios
        if (result.flashCrashDetected) {
            result.detectedScenarios.push_back(StressScenarioType::FLASH_CRASH);
        }
        if (result.liquidityCrisisDetected) {
            result.detectedScenarios.push_back(StressScenarioType::LIQUIDITY_CRISIS);
        }
        if (result.correlationSpikeDetected) {
            result.detectedScenarios.push_back(StressScenarioType::CORRELATION_BREAKDOWN);
        }
        if (result.volatilitySpikeDetected) {
            result.detectedScenarios.push_back(StressScenarioType::VOLATILITY_SPIKE);
        }

        // Generate protection recommendations
        result.recommendedActions = generateProtectionRecommendationsFromDetection(result);

        // Estimate continuation probability and duration
        result.probabilityOfContinuation = estimateContinuationProbability(result);
        result.estimatedDuration = estimateStressDuration(result);
        result.expectedMaxLoss = estimateMaxLoss(result);
    }

    lastDetectionResult_ = result;
    return result;
}

void StressTestingEngine::startRealTimeMonitoring() {
    if (monitoringActive_.load()) {
        std::cout << "Real-time monitoring already active" << std::endl;
        return;
    }

    monitoringActive_.store(true);
    monitoringThread_ = std::make_unique<std::thread>(&StressTestingEngine::monitoringLoop, this);

    std::cout << "Started real-time stress monitoring" << std::endl;
}

void StressTestingEngine::stopRealTimeMonitoring() {
    if (!monitoringActive_.load()) {
        return;
    }

    monitoringActive_.store(false);
    if (monitoringThread_ && monitoringThread_->joinable()) {
        monitoringThread_->join();
    }
    monitoringThread_.reset();

    std::cout << "Stopped real-time stress monitoring" << std::endl;
}

void StressTestingEngine::monitoringLoop() {
    while (monitoringActive_.load()) {
        try {
            if (!currentMarketData_.empty()) {
                auto detection = detectMarketStress(currentMarketData_);

                if (detection.stressDetected) {
                    processStressDetection(detection);

                    if (detectionConfig_.enableAutomaticPortfolioProtection) {
                        auto protection = executeAutomaticPortfolioProtection(detection);
                        dashboardData_.lastProtectionResult = protection;
                        dashboardData_.protectionActive = protection.protectionTriggered;
                    }
                }
            }

            // Update dashboard
            updateDashboard();

            // Sleep for monitoring interval (1 second)
            std::this_thread::sleep_for(std::chrono::seconds(1));

        } catch (const std::exception& e) {
            std::cerr << "Error in monitoring loop: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(5)); // Wait longer on error
        }
    }
}

// === PORTFOLIO PROTECTION ===

ProtectionResult StressTestingEngine::executeAutomaticPortfolioProtection(const StressDetectionResult& stress) {
    ProtectionResult result;
    result.protectionTriggered = false;
    result.triggerTime = std::chrono::system_clock::now();
    result.protectionReason = "Stress detected: intensity " + std::to_string(stress.stressIntensity);

    if (!detectionConfig_.enableAutomaticPortfolioProtection) {
        result.requiresHumanConfirmation = true;
        result.pendingActions.push_back("Manual review required - automatic protection disabled");
        return result;
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        // Determine protection actions based on stress type and intensity
        auto actions = determineProtectionActions(stress);

        // Execute actions in priority order
        std::sort(actions.begin(), actions.end(),
                 [](const ProtectionAction& a, const ProtectionAction& b) {
                     return a.priority > b.priority;
                 });

        for (const auto& action : actions) {
            if (action.requiresConfirmation) {
                result.pendingActions.push_back(action.description);
                result.requiresHumanConfirmation = true;
                continue;
            }

            bool success = executeProtectionAction(action);
            if (success) {
                result.executedActions.push_back(action);
                result.protectionTriggered = true;

                // Update portfolio metrics
                if (action.type == ProtectionActionType::REDUCE_POSITIONS) {
                    result.portfolioReduction += action.magnitude;
                } else if (action.type == ProtectionActionType::INCREASE_CASH) {
                    result.cashBufferIncrease += action.magnitude;
                }
            }
        }

        // Calculate protection effectiveness
        if (result.protectionTriggered) {
            result.expectedLossReduction = estimateLossReduction(result.executedActions, stress);
            result.liquidityCost = estimateLiquidityCost(result.executedActions);
            result.opportunityCost = estimateOpportunityCost(result.executedActions);
        }

    } catch (const std::exception& e) {
        result.pendingActions.push_back("Protection error: " + std::string(e.what()));
        result.requiresHumanConfirmation = true;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    result.responseTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    // Response time should be <2 seconds
    if (result.responseTime.count() > 2000) {
        result.pendingActions.push_back("Response time exceeded 2s target");
    }

    return result;
}

// === DASHBOARD AND REPORTING ===

StressDashboardData StressTestingEngine::getDashboardData() {
    updateDashboard();
    return dashboardData_;
}

bool StressTestingEngine::updateDashboard() {
    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        // Update current stress level
        dashboardData_.currentStressLevel = lastDetectionResult_;
        dashboardData_.currentRegime = currentRegime_;

        // Update performance metrics
        dashboardData_.performanceMetrics = calculatePerformanceMetrics();

        // Update recent test results (keep last 24 hours)
        cleanupOldTestResults();

        // Update risk assessment
        dashboardData_.currentStressScore = calculateOverallStressScore();
        dashboardData_.riskWarnings = generateCurrentRiskWarnings();
        dashboardData_.recommendedActions = generateCurrentRecommendations();

        dashboardData_.lastUpdate = std::chrono::system_clock::now();

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        // Dashboard update should be <1s
        return duration.count() < 1000;

    } catch (const std::exception& e) {
        std::cerr << "Error updating stress testing dashboard: " << e.what() << std::endl;
        return false;
    }
}

// === PERFORMANCE AND VALIDATION ===

StressTestPerformanceMetrics StressTestingEngine::benchmarkPerformance(int iterations) {
    StressTestPerformanceMetrics metrics;
    auto startTime = std::chrono::high_resolution_clock::now();

    std::cout << "Benchmarking stress testing performance with " << iterations << " iterations..." << std::endl;

    // Create test portfolio
    std::vector<std::string> testAssets = {"BTC", "ETH", "ADA", "DOT", "LINK"};
    std::vector<double> testWeights = {0.3, 0.25, 0.2, 0.15, 0.1};

    // Test scenarios
    std::vector<StressScenarioType> testScenarios = {
        StressScenarioType::FLASH_CRASH,
        StressScenarioType::LIQUIDITY_CRISIS,
        StressScenarioType::CORRELATION_BREAKDOWN,
        StressScenarioType::VOLATILITY_SPIKE
    };

    for (auto scenario : testScenarios) {
        auto scenarioStartTime = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < iterations; ++i) {
            try {
                auto result = runStressTest(scenario, testAssets, testWeights, StressSeverity::SEVERE);
                // Record time but don't store results
            } catch (const std::exception& e) {
                std::cerr << "Benchmark error: " << e.what() << std::endl;
            }
        }

        auto scenarioEndTime = std::chrono::high_resolution_clock::now();
        auto scenarioDuration = std::chrono::duration_cast<std::chrono::milliseconds>(scenarioEndTime - scenarioStartTime);

        metrics.scenarioCalculationTimes[scenario] = scenarioDuration / iterations;
    }

    // Calculate aggregated metrics
    auto totalTimes = 0.0;
    auto maxTime = 0.0;

    for (const auto& pair : metrics.scenarioCalculationTimes) {
        auto time = pair.second.count();
        totalTimes += time;
        maxTime = std::max(maxTime, static_cast<double>(time));
    }

    metrics.averageCalculationTime = std::chrono::milliseconds(static_cast<int>(totalTimes / metrics.scenarioCalculationTimes.size()));
    metrics.maxCalculationTime = std::chrono::milliseconds(static_cast<int>(maxTime));

    // Performance targets: <500ms for stress calculations
    metrics.meetsPerformanceTargets = (metrics.maxCalculationTime.count() < 500);

    if (metrics.averageCalculationTime.count() > 0) {
        metrics.scenariosPerSecond = 1000 / metrics.averageCalculationTime.count();
    }

    metrics.lastBenchmark = std::chrono::system_clock::now();

    std::cout << "Stress testing benchmark completed:" << std::endl;
    std::cout << "Average calculation time: " << metrics.averageCalculationTime.count() << "ms" << std::endl;
    std::cout << "Maximum calculation time: " << metrics.maxCalculationTime.count() << "ms" << std::endl;
    std::cout << "Scenarios per second: " << metrics.scenariosPerSecond << std::endl;
    std::cout << "Meets performance targets: " << (metrics.meetsPerformanceTargets ? "YES" : "NO") << std::endl;

    return metrics;
}

// === UTILITY METHODS ===

double StressTestingEngine::calculatePortfolioValue(const std::vector<std::string>& assets,
                                                   const std::vector<double>& weights) {
    // For demo purposes, use a fixed portfolio value
    // In production, this would fetch current market prices and calculate actual value
    return 1000000.0; // $1M portfolio
}

std::map<std::string, double> StressTestingEngine::generateFlashCrashShocks(const std::vector<std::string>& assets,
                                                                           StressSeverity severity) {
    std::map<std::string, double> shocks;

    // Flash crash shocks: sudden severe drops
    double baseDrop = -0.20; // -20% base drop
    double severityMultiplier = static_cast<double>(severity) / 3.0;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<> dist(baseDrop * severityMultiplier, 0.05);

    for (const auto& asset : assets) {
        shocks[asset] = std::max(-0.50, dist(gen)); // Cap at -50%
    }

    return shocks;
}

std::map<std::string, double> StressTestingEngine::generateLiquidityShocks(const std::vector<std::string>& assets,
                                                                          StressSeverity severity) {
    std::map<std::string, double> shocks;

    // Liquidity shocks represent increased transaction costs
    double baseCost = 0.02; // 2% base cost
    double severityMultiplier = static_cast<double>(severity) / 2.0;

    for (const auto& asset : assets) {
        // Different assets have different liquidity characteristics
        double assetLiquidityFactor = getAssetLiquidityFactor(asset);
        shocks[asset] = baseCost * severityMultiplier * assetLiquidityFactor;
    }

    return shocks;
}

std::string StressTestingEngine::getScenarioName(StressScenarioType scenario) {
    switch (scenario) {
        case StressScenarioType::FLASH_CRASH: return "Flash Crash";
        case StressScenarioType::LIQUIDITY_CRISIS: return "Liquidity Crisis";
        case StressScenarioType::CORRELATION_BREAKDOWN: return "Correlation Breakdown";
        case StressScenarioType::VOLATILITY_SPIKE: return "Volatility Spike";
        case StressScenarioType::FINANCIAL_CRISIS_2008: return "2008 Financial Crisis";
        case StressScenarioType::COVID_CRASH_2020: return "COVID-19 2020 Crash";
        case StressScenarioType::LUNA_COLLAPSE_2022: return "LUNA Collapse 2022";
        case StressScenarioType::FTX_COLLAPSE_2022: return "FTX Collapse 2022";
        default: return "Unknown Scenario";
    }
}

StressTestResult StressTestingEngine::createErrorResult(StressScenarioType scenario, const std::string& error) {
    StressTestResult result;
    result.scenario = scenario;
    result.testValid = false;
    result.warnings.push_back(error);
    result.testTime = std::chrono::system_clock::now();
    result.calculationDuration = std::chrono::milliseconds(0);
    return result;
}

void StressTestingEngine::validateStressTestInputs(const std::vector<std::string>& assets,
                                                  const std::vector<double>& weights) {
    if (assets.empty() || weights.empty()) {
        throw std::invalid_argument("Assets and weights cannot be empty");
    }

    if (assets.size() != weights.size()) {
        throw std::invalid_argument("Assets and weights must have same size");
    }

    double totalWeight = std::accumulate(weights.begin(), weights.end(), 0.0);
    if (std::abs(totalWeight - 1.0) > 0.01) {
        throw std::invalid_argument("Weights must sum to 1.0");
    }
}

} // namespace Risk
} // namespace CryptoClaude