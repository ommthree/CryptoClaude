#pragma once

#include "VaRCalculator.h"
#include "../Database/Models/PortfolioData.h"
#include "../Database/Models/MarketData.h"
#include "../Analytics/CrossAssetCorrelationMonitor.h"
#include "../Portfolio/PortfolioOptimizer.h"
#include "../../Configuration/Config.h"
#include <vector>
#include <map>
#include <string>
#include <functional>
#include <chrono>
#include <memory>
#include <optional>
#include <atomic>
#include <thread>

namespace CryptoClaude {
namespace Risk {

using namespace CryptoClaude::Database::Models;
using namespace CryptoClaude::Analytics;

// Stress testing scenario types
enum class StressScenarioType {
    // Market stress scenarios
    FLASH_CRASH,              // Sudden severe market drop (-20% in minutes)
    LIQUIDITY_CRISIS,         // Extreme bid-ask spread widening
    CORRELATION_BREAKDOWN,    // Correlation matrix becomes unstable
    VOLATILITY_SPIKE,         // Extreme volatility increase (3x+)
    MOMENTUM_REVERSAL,        // Sharp trend reversal
    CRYPTO_WINTER,           // Extended bear market scenario

    // Historical events
    FINANCIAL_CRISIS_2008,    // Lehman Brothers collapse scenario
    COVID_CRASH_2020,        // March 2020 pandemic crash
    LUNA_COLLAPSE_2022,      // Terra LUNA/UST collapse
    FTX_COLLAPSE_2022,       // FTX exchange collapse
    SVB_BANKING_2023,        // Silicon Valley Bank collapse

    // Extreme tail events
    EXCHANGE_HACK,           // Major exchange security breach
    REGULATORY_CRACKDOWN,    // Severe regulatory action
    STABLECOIN_DEPEG,       // Major stablecoin loses peg
    MARKET_MANIPULATION,     // Coordinated manipulation event

    // Custom scenarios
    CUSTOM_SCENARIO          // User-defined stress test
};

// Stress test severity levels
enum class StressSeverity {
    MILD = 1,        // 1-2 standard deviations
    MODERATE = 2,    // 2-3 standard deviations
    SEVERE = 3,      // 3-5 standard deviations
    EXTREME = 4,     // 5+ standard deviations
    CATASTROPHIC = 5 // Historical worst-case events
};

// Stress test time horizons
enum class StressTimeHorizon {
    IMMEDIATE,       // 1-5 minutes (flash events)
    SHORT_TERM,      // 1 hour
    INTRADAY,        // 1 day
    MULTI_DAY,       // 1 week
    EXTENDED         // 1 month
};

// Market condition regimes
enum class MarketRegime {
    BULL_MARKET,     // Rising prices, low volatility
    BEAR_MARKET,     // Falling prices, high volatility
    SIDEWAYS,        // Range-bound, normal volatility
    CRISIS,          // Extreme volatility, correlations spike
    RECOVERY         // Post-crisis stabilization
};

// Stress test result structure
struct StressTestResult {
    StressScenarioType scenario;
    StressSeverity severity;
    StressTimeHorizon timeHorizon;

    // Portfolio impact
    double portfolioValue;           // Current portfolio value
    double stressedValue;           // Value under stress
    double absoluteLoss;            // Dollar loss
    double percentageLoss;          // Percentage loss
    double maxDrawdown;             // Maximum drawdown during stress

    // Risk metrics under stress
    VaRResult stressedVaR;          // VaR under stress conditions
    double stressVaRMultiplier;     // Stress VaR / Normal VaR
    double liquidityAdjustment;     // Liquidity cost adjustment

    // Asset-level impacts
    std::map<std::string, double> assetLosses;       // Loss per asset
    std::map<std::string, double> stressedWeights;   // New portfolio weights
    std::map<std::string, double> liquidityCosts;    // Liquidation costs

    // Correlation impacts
    std::map<std::pair<std::string, std::string>, double> stressedCorrelations;
    double correlationRisk;         // Additional correlation risk

    // Recovery analysis
    std::chrono::seconds estimatedRecoveryTime;
    double recoveryProbability;     // Probability of full recovery
    std::vector<std::string> recommendedActions;

    // Stress test metadata
    std::chrono::system_clock::time_point testTime;
    std::chrono::milliseconds calculationDuration;
    bool testValid;
    std::vector<std::string> warnings;

    // Performance during stress
    double worstCaseVaR;           // Worst possible VaR
    double expectedShortfall;      // Expected loss in tail
    double liquidityAtRisk;        // Assets at liquidity risk

    // Model stability
    std::map<VaRMethodology, double> modelReliability; // Model accuracy under stress
    bool requiresPortfolioRebalancing;
    std::vector<std::string> riskMitigationActions;
};

// Historical stress scenario configuration
struct HistoricalStressConfig {
    StressScenarioType scenario;
    std::chrono::system_clock::time_point startDate;
    std::chrono::system_clock::time_point endDate;

    // Key characteristics of the event
    double maxMarketDrop;           // Maximum market decline
    int durationDays;               // Event duration
    double volatilityMultiplier;    // Vol increase factor
    double correlationSpike;        // Correlation increase

    // Recovery characteristics
    int recoveryDays;              // Time to recover
    double recoveryRate;           // Daily recovery rate
    bool hadSecondaryShocks;       // Multiple waves of selling

    std::map<std::string, double> assetSpecificImpacts; // Asset-specific shocks
    std::string description;        // Event description
};

// Custom stress scenario builder
struct CustomStressScenario {
    std::string name;
    std::string description;
    StressSeverity severity;
    StressTimeHorizon timeHorizon;

    // Market shocks
    std::map<std::string, double> assetShocks;          // Price shocks per asset
    std::map<std::string, double> volatilityShocks;     // Vol shocks per asset

    // Correlation matrix changes
    std::map<std::pair<std::string, std::string>, double> correlationShocks;
    double overallCorrelationIncrease = 0.0;

    // Liquidity impacts
    std::map<std::string, double> liquidityShocks;      // Liquidity cost increases
    double marketImpactMultiplier = 1.0;

    // Time evolution
    std::vector<std::pair<int, double>> timeEvolution;  // (minutes, shock_intensity)
    bool hasRecovery = true;

    std::function<double(int)> customShockFunction;     // Custom shock evolution
};

// Real-time stress detection configuration
struct StressDetectionConfig {
    // Detection thresholds
    double flashCrashThreshold = -0.10;         // -10% in short time
    double volatilitySpikeFactor = 2.0;         // 2x normal volatility
    double correlationSpikeThreshold = 0.8;     // Correlations above 0.8
    double liquidityDryupThreshold = 3.0;       // 3x normal spreads

    // Time windows for detection
    std::chrono::minutes flashCrashWindow{15};
    std::chrono::minutes volatilityWindow{60};
    std::chrono::minutes correlationWindow{30};
    std::chrono::minutes liquidityWindow{10};

    // Alert sensitivity
    double alertSensitivity = 0.5;              // 0 = low, 1 = high
    bool enableAutomaticPortfolioProtection = true;
    double maxAutomaticReduction = 0.20;        // Max 20% position reduction

    // Market regime detection
    bool enableRegimeDetection = true;
    std::chrono::hours regimeDetectionWindow{24};

    std::vector<std::string> monitoredAssets;    // Assets to monitor
};

// Portfolio protection actions
enum class ProtectionActionType {
    REDUCE_POSITIONS,        // Reduce risky positions
    INCREASE_CASH,          // Increase cash buffer
    HEDGE_PORTFOLIO,        // Add hedging positions
    STOP_TRADING,           // Halt trading operations
    REBALANCE_CONSERVATIVE, // Switch to conservative allocation
    ACTIVATE_STOP_LOSSES,   // Trigger stop-loss orders
    NOTIFY_OPERATORS        // Alert human operators
};

struct ProtectionAction {
    ProtectionActionType type;
    double magnitude;               // Intensity of action (0-1)
    std::vector<std::string> targetAssets; // Specific assets affected
    std::chrono::minutes timeframe; // Execution timeframe
    std::string description;
    bool requiresConfirmation;      // Needs human approval
    int priority;                   // 1-10 execution priority
};

// Main stress testing engine
class StressTestingEngine {
public:
    StressTestingEngine();
    ~StressTestingEngine();

    // Initialization and configuration
    bool initialize(std::shared_ptr<VaRCalculator> varCalculator,
                   std::shared_ptr<CrossAssetCorrelationMonitor> correlationMonitor,
                   std::shared_ptr<CryptoClaude::Optimization::PortfolioOptimizer> optimizer);

    void setStressDetectionConfig(const StressDetectionConfig& config);
    void setTRSParameters(const TRSRiskParameters& params);

    // === MAIN STRESS TESTING METHODS ===

    // Run single stress test scenario
    StressTestResult runStressTest(StressScenarioType scenario,
                                  const std::vector<std::string>& assets,
                                  const std::vector<double>& weights,
                                  StressSeverity severity = StressSeverity::SEVERE);

    // Run comprehensive stress testing suite
    std::map<StressScenarioType, StressTestResult> runComprehensiveStressTests(
        const std::vector<std::string>& assets,
        const std::vector<double>& weights);

    // Run portfolio-level stress tests
    std::map<StressScenarioType, StressTestResult> runPortfolioStressTests(
        const Portfolio& portfolio);

    // === SPECIFIC STRESS SCENARIOS ===

    // Market stress scenarios
    StressTestResult runFlashCrashScenario(const std::vector<std::string>& assets,
                                          const std::vector<double>& weights,
                                          StressSeverity severity = StressSeverity::SEVERE);

    StressTestResult runLiquidityCrisisScenario(const std::vector<std::string>& assets,
                                               const std::vector<double>& weights,
                                               StressSeverity severity = StressSeverity::SEVERE);

    StressTestResult runCorrelationBreakdownScenario(const std::vector<std::string>& assets,
                                                     const std::vector<double>& weights,
                                                     StressSeverity severity = StressSeverity::SEVERE);

    StressTestResult runVolatilitySpike(const std::vector<std::string>& assets,
                                       const std::vector<double>& weights,
                                       StressSeverity severity = StressSeverity::SEVERE);

    // Historical event replications
    StressTestResult run2008FinancialCrisis(const std::vector<std::string>& assets,
                                           const std::vector<double>& weights);

    StressTestResult runCOVID2020Crash(const std::vector<std::string>& assets,
                                      const std::vector<double>& weights);

    StressTestResult runLUNACollapse2022(const std::vector<std::string>& assets,
                                        const std::vector<double>& weights);

    StressTestResult runFTXCollapse2022(const std::vector<std::string>& assets,
                                       const std::vector<double>& weights);

    // Custom scenario testing
    StressTestResult runCustomScenario(const CustomStressScenario& scenario,
                                      const std::vector<std::string>& assets,
                                      const std::vector<double>& weights);

    // === REAL-TIME STRESS DETECTION ===

    struct StressDetectionResult {
        bool stressDetected;
        std::vector<StressScenarioType> detectedScenarios;
        MarketRegime currentRegime;
        double stressIntensity;             // 0-1 scale
        std::chrono::system_clock::time_point detectionTime;

        // Specific stress indicators
        bool flashCrashDetected;
        bool volatilitySpikeDetected;
        bool correlationSpikeDetected;
        bool liquidityCrisisDetected;

        std::vector<std::string> affectedAssets;
        std::vector<ProtectionAction> recommendedActions;

        // Model predictions
        double probabilityOfContinuation;   // Probability stress continues
        std::chrono::minutes estimatedDuration;
        double expectedMaxLoss;

        std::vector<std::string> detectionDetails;
    };

    // Real-time monitoring
    StressDetectionResult detectMarketStress(const std::map<std::string, MarketData>& currentData);
    MarketRegime detectMarketRegime(int lookbackHours = 24);

    // Continuous monitoring thread
    void startRealTimeMonitoring();
    void stopRealTimeMonitoring();
    bool isMonitoringActive() const { return monitoringActive_; }

    // === PORTFOLIO PROTECTION ===

    struct ProtectionResult {
        bool protectionTriggered;
        std::vector<ProtectionAction> executedActions;
        double portfolioReduction;          // Amount reduced
        double cashBufferIncrease;          // Cash buffer added

        std::chrono::system_clock::time_point triggerTime;
        std::chrono::milliseconds responseTime;

        bool requiresHumanConfirmation;
        std::vector<std::string> pendingActions;
        std::string protectionReason;

        // Impact assessment
        double expectedLossReduction;       // Expected loss prevented
        double liquidityCost;              // Cost of protection
        double opportunityCost;            // Potential upside lost
    };

    ProtectionResult executeAutomaticPortfolioProtection(const StressDetectionResult& stress);
    std::vector<ProtectionAction> generateProtectionRecommendations(const StressTestResult& stressResult);

    // === STRESS-ADJUSTED VAR INTEGRATION ===

    // Calculate stress-adjusted VaR
    VaRResult calculateStressAdjustedVaR(const std::vector<std::string>& assets,
                                        const std::vector<double>& weights,
                                        StressScenarioType scenario,
                                        VaRMethodology methodology = VaRMethodology::HISTORICAL_SIMULATION);

    // Multi-scenario stress VaR
    std::map<StressScenarioType, VaRResult> calculateMultiScenarioStressVaR(
        const std::vector<std::string>& assets,
        const std::vector<double>& weights);

    // Dynamic stress VaR (adjusts based on current market conditions)
    VaRResult calculateDynamicStressVaR(const std::vector<std::string>& assets,
                                       const std::vector<double>& weights,
                                       const MarketRegime& regime);

    // === PERFORMANCE AND VALIDATION ===

    struct StressTestPerformanceMetrics {
        std::map<StressScenarioType, std::chrono::milliseconds> scenarioCalculationTimes;
        std::chrono::milliseconds averageCalculationTime;
        std::chrono::milliseconds maxCalculationTime;
        std::chrono::milliseconds detectionResponseTime;

        bool meetsPerformanceTargets;       // <500ms stress calculations
        int scenariosPerSecond;

        // Detection accuracy
        double falsePositiveRate;
        double falseNegativeRate;
        double detectionAccuracy;

        std::chrono::system_clock::time_point lastBenchmark;
    };

    StressTestPerformanceMetrics benchmarkPerformance(int iterations = 50);

    // Historical validation
    struct ValidationResult {
        StressScenarioType scenario;
        double historicalAccuracy;          // How well it predicted actual events
        double averageError;                // Average prediction error
        std::vector<std::string> validationWarnings;
        bool isValidForProduction;
    };

    std::vector<ValidationResult> validateStressScenarios(int backtestDays = 252);

    // === CONFIGURATION AND MANAGEMENT ===

    // Scenario management
    void addCustomScenario(const CustomStressScenario& scenario);
    void removeCustomScenario(const std::string& scenarioName);
    std::vector<CustomStressScenario> getCustomScenarios() const;

    // Historical scenario configuration
    void configureHistoricalScenario(const HistoricalStressConfig& config);
    std::map<StressScenarioType, HistoricalStressConfig> getHistoricalConfigurations() const;

    // Data updates
    void updateMarketData(const std::map<std::string, MarketData>& data);
    void updatePortfolioData(const Portfolio& portfolio);

    // State management
    bool isInitialized() const { return isInitialized_; }
    std::chrono::system_clock::time_point getLastStressTest() const { return lastStressTestTime_; }
    StressDetectionResult getLastDetectionResult() const { return lastDetectionResult_; }

    // === DASHBOARD AND REPORTING ===

    struct StressDashboardData {
        // Current stress status
        StressDetectionResult currentStressLevel;
        MarketRegime currentRegime;

        // Recent stress test results
        std::map<StressScenarioType, StressTestResult> recentStressTests;
        std::chrono::system_clock::time_point lastTestSuite;

        // Historical performance
        std::vector<std::pair<std::chrono::system_clock::time_point, double>> stressHistory;
        std::vector<std::pair<std::chrono::system_clock::time_point, MarketRegime>> regimeHistory;

        // Protection status
        bool protectionActive;
        std::vector<ProtectionAction> activeProtections;
        ProtectionResult lastProtectionResult;

        // Model performance
        StressTestPerformanceMetrics performanceMetrics;
        std::vector<ValidationResult> modelValidation;

        // Risk assessment
        double currentStressScore;          // 0-1 overall stress level
        std::vector<std::string> riskWarnings;
        std::vector<std::string> recommendedActions;

        std::chrono::system_clock::time_point lastUpdate;
    };

    StressDashboardData getDashboardData();
    bool updateDashboard();  // <2s performance requirement

    // Reporting
    std::string generateStressTestReport(const std::map<StressScenarioType, StressTestResult>& results);
    void exportStressTestResults(const std::string& filename,
                                const std::map<StressScenarioType, StressTestResult>& results);

private:
    // Configuration
    StressDetectionConfig detectionConfig_;
    TRSRiskParameters trsParams_;
    std::map<StressScenarioType, HistoricalStressConfig> historicalConfigs_;
    std::vector<CustomStressScenario> customScenarios_;

    // Components
    std::shared_ptr<VaRCalculator> varCalculator_;
    std::shared_ptr<CrossAssetCorrelationMonitor> correlationMonitor_;
    std::shared_ptr<CryptoClaude::Optimization::PortfolioOptimizer> portfolioOptimizer_;

    // Data storage
    std::map<std::string, std::vector<double>> historicalReturns_;
    std::map<std::string, MarketData> currentMarketData_;
    Portfolio currentPortfolio_;

    // Real-time monitoring
    std::atomic<bool> monitoringActive_;
    std::unique_ptr<std::thread> monitoringThread_;
    StressDetectionResult lastDetectionResult_;
    MarketRegime currentRegime_;

    // Dashboard and state
    StressDashboardData dashboardData_;
    std::chrono::system_clock::time_point lastStressTestTime_;
    std::chrono::system_clock::time_point lastDashboardUpdate_;

    // State management
    bool isInitialized_;

    // === INTERNAL CALCULATION METHODS ===

    // Scenario implementation helpers
    std::map<std::string, double> generateFlashCrashShocks(const std::vector<std::string>& assets,
                                                          StressSeverity severity);
    std::map<std::string, double> generateLiquidityShocks(const std::vector<std::string>& assets,
                                                         StressSeverity severity);
    std::map<std::pair<std::string, std::string>, double> generateCorrelationShocks(
        const std::vector<std::string>& assets, StressSeverity severity);
    std::map<std::string, double> generateVolatilityShocks(const std::vector<std::string>& assets,
                                                          StressSeverity severity);

    // Historical scenario calibration
    void calibrateHistoricalScenario(StressScenarioType scenario);
    std::map<std::string, double> getHistoricalShocks(StressScenarioType scenario,
                                                     const std::vector<std::string>& assets);

    // Stress evolution modeling
    std::vector<double> modelStressEvolution(const std::map<std::string, double>& initialShocks,
                                           StressTimeHorizon horizon,
                                           int timeSteps = 100);

    // Detection algorithms
    bool detectFlashCrash(const std::map<std::string, MarketData>& data);
    bool detectVolatilitySpike(const std::map<std::string, MarketData>& data);
    bool detectCorrelationSpike(const std::map<std::string, MarketData>& data);
    bool detectLiquidityCrisis(const std::map<std::string, MarketData>& data);

    // Real-time monitoring implementation
    void monitoringLoop();
    void processStressDetection(const StressDetectionResult& detection);

    // Portfolio protection implementation
    std::vector<ProtectionAction> determineProtectionActions(const StressDetectionResult& stress);
    bool executeProtectionAction(const ProtectionAction& action);

    // Stress VaR calculations
    VaRResult applyStressToVaR(const VaRResult& baseVaR,
                              const std::map<std::string, double>& assetShocks,
                              const std::map<std::pair<std::string, std::string>, double>& correlationShocks);

    // Utility methods
    double calculateSeverityMultiplier(StressSeverity severity);
    std::chrono::seconds estimateRecoveryTime(StressScenarioType scenario, StressSeverity severity);
    double calculateRecoveryProbability(const StressTestResult& result);

    // Portfolio and scenario calculation methods
    double calculatePortfolioValue(const std::vector<std::string>& assets, const std::vector<double>& weights);
    StressTestResult runGenericStressScenario(StressScenarioType scenario, const std::vector<std::string>& assets,
                                             const std::vector<double>& weights, StressSeverity severity);
    StressTestResult runHistoricalScenario(const HistoricalStressConfig& config, const std::vector<std::string>& assets,
                                          const std::vector<double>& weights);
    std::string getScenarioName(StressScenarioType scenario);

    // Market stress shock generators
    std::map<std::string, double> generateMarketStressShocks(const std::vector<std::string>& assets, StressSeverity severity, double baseShock);
    std::map<std::string, double> generateCorrelationBasedShocks(const std::vector<std::string>& assets,
                                                                const std::map<std::pair<std::string, std::string>, double>& correlationShocks,
                                                                StressSeverity severity);
    std::map<std::string, double> generateVolatilityBasedPriceShocks(const std::vector<std::string>& assets,
                                                                    const std::map<std::string, double>& volatilityShocks,
                                                                    StressSeverity severity);

    // Risk calculation methods
    double calculateCorrelationRisk(const std::vector<std::string>& assets, const std::vector<double>& weights,
                                   const std::map<std::pair<std::string, std::string>, double>& correlationShocks);
    double calculateDiversificationBenefit(const std::vector<std::string>& assets, const std::vector<double>& weights,
                                          const std::map<std::pair<std::string, std::string>, double>& correlations);
    double getStressVaRMultiplier(StressScenarioType scenario);
    double getAssetLiquidityFactor(const std::string& asset);

    // Initialization
    void initializeHistoricalScenarios();

    // Protection and detection methods
    std::vector<ProtectionAction> generateProtectionRecommendationsFromDetection(const StressDetectionResult& detection);
    double estimateContinuationProbability(const StressDetectionResult& detection);
    std::chrono::minutes estimateStressDuration(const StressDetectionResult& detection);
    double estimateMaxLoss(const StressDetectionResult& detection);

    // Performance and dashboard methods
    StressTestPerformanceMetrics calculatePerformanceMetrics();
    void cleanupOldTestResults();
    double calculateOverallStressScore();
    std::vector<std::string> generateCurrentRiskWarnings();
    std::vector<std::string> generateCurrentRecommendations();

    // Protection action execution
    double estimateLossReduction(const std::vector<ProtectionAction>& actions, const StressDetectionResult& stress);
    double estimateLiquidityCost(const std::vector<ProtectionAction>& actions);
    double estimateOpportunityCost(const std::vector<ProtectionAction>& actions);

    // Validation and performance
    void validateStressTestInputs(const std::vector<std::string>& assets,
                                 const std::vector<double>& weights);
    void logStressTestMetrics(StressScenarioType scenario,
                            std::chrono::milliseconds duration);

    // Error handling
    void handleStressTestError(const std::string& scenario, const std::exception& e);
    StressTestResult createErrorResult(StressScenarioType scenario, const std::string& error);
};

// Utility functions for stress testing
namespace StressTestUtils {
    // Severity calculations
    double getSeverityFactor(StressSeverity severity);
    std::string getSeverityDescription(StressSeverity severity);

    // Time horizon utilities
    std::chrono::minutes getTimeHorizonDuration(StressTimeHorizon horizon);
    int getTimeHorizonSteps(StressTimeHorizon horizon);

    // Historical event data
    struct HistoricalEventData {
        std::string name;
        std::chrono::system_clock::time_point startDate;
        std::chrono::system_clock::time_point endDate;
        double maxMarketDrop;
        double volatilityIncrease;
        int recoveryDays;
        std::map<std::string, double> assetImpacts;
    };

    HistoricalEventData getEventData(StressScenarioType scenario);
    std::vector<HistoricalEventData> getAllHistoricalEvents();

    // Correlation utilities
    std::vector<std::vector<double>> createStressedCorrelationMatrix(
        const std::vector<std::vector<double>>& baseMatrix,
        StressSeverity severity);

    double calculateCorrelationStability(const std::vector<std::vector<double>>& matrix);

    // Statistical utilities
    double calculateTailRisk(const std::vector<double>& returns, double percentile);
    double calculateMaximumLoss(const std::vector<double>& portfolioValues);

    // Risk measure conversions
    double convertVaRToStressVaR(double baseVaR, StressScenarioType scenario, StressSeverity severity);
    double estimateLiquidityImpact(double portfolioValue, double marketImpact);
}

} // namespace Risk
} // namespace CryptoClaude