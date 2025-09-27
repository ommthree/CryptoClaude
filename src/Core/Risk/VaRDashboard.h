#pragma once

#include "VaRCalculator.h"
#include "../Analytics/CrossAssetCorrelationMonitor.h"
#include "../Portfolio/PortfolioOptimizer.h"
// Note: AlertSystem integration available for future enhancement
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <functional>

namespace CryptoClaude {
namespace Risk {

// Real-time risk dashboard for TRS demonstrations and monitoring
class VaRDashboard {
public:
    // Dashboard configuration
    struct DashboardConfig {
        int refreshIntervalMs = 1000;           // 1 second refresh (TRS requirement)
        int maxHistoryPoints = 1440;            // 24 hours of minute data
        bool enableRealTimeAlerts = true;
        bool enablePerformanceMonitoring = true;
        bool enableCorrelationTracking = true;

        // Display preferences
        bool showMultipleMethodologies = true;
        bool showComponentRisk = true;
        bool showBacktestResults = true;
        bool showPerformanceMetrics = true;

        // Alert thresholds
        double warningVaRThreshold = 0.8;       // 80% of limit
        double criticalVaRThreshold = 0.95;     // 95% of limit
    };

    // Risk dashboard state for real-time monitoring
    struct DashboardState {
        // Current VaR metrics
        std::map<VaRMethodology, VaRResult> currentVaRResults;
        VaRResult primaryVaRResult;             // Main VaR calculation

        // Portfolio information
        double portfolioValue;
        std::map<std::string, double> currentWeights;
        int totalPositions;

        // Risk limits and utilization
        double dailyVaRLimit95;
        double dailyVaRLimit99;
        double currentVaRUtilization95;         // % of limit used
        double currentVaRUtilization99;

        // Correlation metrics
        std::vector<Analytics::CorrelationSnapshot> correlationData;
        double averageCorrelation;
        double maxCorrelation;
        double correlationRiskScore;

        // Performance metrics
        std::chrono::milliseconds lastCalculationTime;
        std::chrono::milliseconds averageCalculationTime;
        int calculationsPerformed;
        bool meetsPerformanceTargets;

        // Alert information
        std::vector<VaRCalculator::RiskAlert> activeAlerts;
        int criticalAlerts;
        int warningAlerts;

        // Historical tracking
        std::vector<std::pair<std::chrono::system_clock::time_point, double>> varHistory;
        std::vector<std::pair<std::chrono::system_clock::time_point, double>> portfolioHistory;

        std::chrono::system_clock::time_point lastUpdate;
        bool isOnline;
    };

    // Performance tracking for TRS requirements
    struct PerformanceBenchmark {
        std::string metric;
        double currentValue;
        double targetValue;
        bool meetsTarget;
        std::string unit;
        std::string description;
    };

    explicit VaRDashboard(const DashboardConfig& config = DashboardConfig{});
    ~VaRDashboard();

    // === INITIALIZATION AND CONFIGURATION ===

    bool initialize();
    void setVaRCalculator(std::shared_ptr<VaRCalculator> calculator);
    void setCorrelationMonitor(std::shared_ptr<Analytics::CrossAssetCorrelationMonitor> monitor);
    void setPortfolioOptimizer(std::shared_ptr<CryptoClaude::Optimization::PortfolioOptimizer> optimizer);

    void setConfiguration(const DashboardConfig& config) { config_ = config; }
    DashboardConfig getConfiguration() const { return config_; }

    // === REAL-TIME DASHBOARD OPERATIONS ===

    // Main dashboard update - must complete within 1 second (TRS requirement)
    bool updateDashboard();

    // Get current dashboard state
    DashboardState getCurrentState() const { return currentState_; }

    // Force immediate dashboard refresh
    bool refreshNow();

    // Check if dashboard is meeting performance targets
    bool isPerformingOptimally() const;

    // === RISK MONITORING AND ALERTS ===

    // Get current risk status summary
    struct RiskStatusSummary {
        enum StatusLevel { GREEN, YELLOW, RED, CRITICAL };

        StatusLevel overallStatus;
        double riskScore;                       // 0-100 scale
        std::string statusMessage;

        // Risk breakdown
        StatusLevel varStatus;
        StatusLevel correlationStatus;
        StatusLevel performanceStatus;
        StatusLevel backtestStatus;

        std::vector<std::string> keyRisks;
        std::vector<std::string> recommendations;
    };

    RiskStatusSummary getRiskStatusSummary() const;

    // Alert management
    std::vector<VaRCalculator::RiskAlert> getActiveAlerts() const;
    void acknowledgeAlert(size_t alertIndex);
    void clearAllAlerts();

    // === VISUALIZATION AND REPORTING DATA ===

    // Get data for VaR trend charts
    struct VaRTrendData {
        std::vector<std::chrono::system_clock::time_point> timestamps;
        std::vector<double> var95Values;
        std::vector<double> var99Values;
        std::vector<double> actualReturns;
        std::vector<bool> varBreaches;

        double averageVaR95;
        double averageVaR99;
        double breachRate95;
        double breachRate99;
    };

    VaRTrendData getVaRTrendData(std::chrono::hours lookbackPeriod = std::chrono::hours(24)) const;

    // Get correlation heatmap data
    struct CorrelationHeatmapData {
        std::vector<std::string> assetNames;
        std::vector<std::vector<double>> correlationMatrix;
        std::vector<std::pair<std::string, std::string>> highCorrelationPairs;
        double avgCorrelation;
        std::chrono::system_clock::time_point timestamp;
    };

    CorrelationHeatmapData getCorrelationHeatmapData() const;

    // Get risk decomposition data
    struct RiskDecompositionData {
        std::map<std::string, double> componentVaR;
        std::map<std::string, double> incrementalVaR;
        std::map<std::string, double> riskContributions;

        double systematicRisk;
        double idiosyncraticRisk;
        double correlationRisk;

        // Risk attribution by methodology
        std::map<VaRMethodology, double> methodologyComparison;
    };

    RiskDecompositionData getRiskDecompositionData() const;

    // === BACKTESTING AND VALIDATION RESULTS ===

    struct BacktestDashboardData {
        std::map<VaRMethodology, VaRBacktester::BacktestResult> backtestResults;

        // Model comparison metrics
        VaRMethodology bestPerformingModel;
        VaRMethodology worstPerformingModel;

        // Overall validation status
        bool allModelsValid;
        double averageAccuracy;
        int totalBreaches;

        std::chrono::system_clock::time_point lastBacktestDate;
    };

    BacktestDashboardData getBacktestResults() const;

    // Run new backtest and update dashboard
    bool runBacktestUpdate();

    // === PERFORMANCE MONITORING ===

    // Get performance benchmarks vs TRS requirements
    std::vector<PerformanceBenchmark> getPerformanceBenchmarks() const;

    // Detailed performance metrics
    struct DetailedPerformanceMetrics {
        // Calculation performance
        double avgVaRCalculationTimeMs;
        double maxVaRCalculationTimeMs;
        double minVaRCalculationTimeMs;

        // Dashboard performance
        double avgDashboardUpdateTimeMs;
        double maxDashboardUpdateTimeMs;
        bool meetsDashboardTarget;           // <1 second

        // Correlation monitoring performance
        double correlationUpdateFrequencyHz;
        double correlationDataQuality;

        // Memory and resource usage
        double memoryUsageMB;
        double cpuUtilization;

        // Reliability metrics
        double uptimePercentage;
        int errorCount24h;
        int successfulUpdates24h;

        std::chrono::system_clock::time_point lastMeasurement;
    };

    DetailedPerformanceMetrics getDetailedPerformanceMetrics() const;

    // === TRS DEMONSTRATION SUPPORT ===

    // Generate comprehensive TRS demonstration report
    struct TRSDemonstrationReport {
        // Executive summary
        std::string executiveSummary;
        RiskStatusSummary overallStatus;

        // Technical implementation details
        std::vector<std::string> implementedCapabilities;
        std::vector<PerformanceBenchmark> performanceResults;

        // Risk management effectiveness
        BacktestDashboardData validationResults;
        std::map<std::string, double> riskLimitCompliance;

        // Integration with existing systems
        std::string correlationIntegrationStatus;
        std::string portfolioOptimizationStatus;

        // Future enhancements roadmap
        std::vector<std::string> plannedEnhancements;

        std::chrono::system_clock::time_point reportTimestamp;
    };

    TRSDemonstrationReport generateTRSReport() const;

    // Export dashboard data for TRS presentation
    std::string exportDashboardDataJSON() const;
    std::string exportPerformanceReportCSV() const;

    // === CONFIGURATION AND UTILITIES ===

    // Dashboard lifecycle management
    bool startRealTimeMonitoring();
    bool stopRealTimeMonitoring();
    bool isRealTimeActive() const { return realTimeActive_; }

    // Manual calculation triggers
    bool triggerVaRRecalculation();
    bool triggerCorrelationUpdate();
    bool triggerBacktestRun();

    // Dashboard health check
    bool performHealthCheck();
    std::vector<std::string> getDiagnosticMessages() const;

private:
    // Configuration and components
    DashboardConfig config_;
    std::shared_ptr<VaRCalculator> varCalculator_;
    std::shared_ptr<Analytics::CrossAssetCorrelationMonitor> correlationMonitor_;
    std::shared_ptr<CryptoClaude::Optimization::PortfolioOptimizer> portfolioOptimizer_;

    // Current state
    DashboardState currentState_;
    DetailedPerformanceMetrics performanceMetrics_;

    // Real-time monitoring
    bool realTimeActive_;
    std::chrono::system_clock::time_point lastUpdate_;
    std::chrono::system_clock::time_point lastPerformanceCheck_;

    // Historical data storage
    std::vector<std::pair<std::chrono::system_clock::time_point, DashboardState>> stateHistory_;
    size_t maxHistorySize_;

    // Performance tracking
    std::vector<std::chrono::milliseconds> recentUpdateTimes_;
    std::vector<std::chrono::milliseconds> recentCalculationTimes_;

    // Alert management
    mutable std::vector<VaRCalculator::RiskAlert> acknowledgedAlerts_;

    // === INTERNAL METHODS ===

    // Data collection and processing
    bool collectVaRData();
    bool collectCorrelationData();
    bool collectPortfolioData();
    bool collectPerformanceData();

    // State management
    void updateCurrentState();
    void addToHistory();
    void cleanupOldHistory();

    // Performance monitoring
    void trackUpdatePerformance(std::chrono::milliseconds updateTime);
    void updatePerformanceMetrics();
    bool checkPerformanceTargets() const;

    // Alert processing
    void processNewAlerts();
    RiskStatusSummary::StatusLevel calculateOverallStatus() const;

    // Utility methods
    std::string formatTimestamp(const std::chrono::system_clock::time_point& timestamp) const;
    std::string formatDuration(std::chrono::milliseconds duration) const;
    std::string formatPercentage(double value) const;

    // Validation helpers
    bool validateDashboardData() const;
    bool validatePerformanceMetrics() const;

    // JSON serialization helpers
    std::string serializeState(const DashboardState& state) const;
    std::string serializePerformanceMetrics() const;
    std::string serializeBacktestResults() const;
};

// Utility functions for dashboard operations
namespace DashboardUtils {
    // Risk level color mapping for UI
    enum class RiskColor { GREEN, YELLOW, ORANGE, RED };

    RiskColor getRiskColor(double utilizationPercentage);
    std::string getRiskColorName(RiskColor color);

    // Performance status indicators
    std::string getPerformanceStatusIcon(bool meetsTarget);
    std::string formatPerformanceMetric(double value, const std::string& unit);

    // Time series data utilities
    std::vector<double> calculateMovingAverage(const std::vector<double>& data, int window);
    std::pair<double, double> calculateTrend(const std::vector<std::pair<std::chrono::system_clock::time_point, double>>& timeSeries);

    // Statistical summaries
    struct StatisticalSummary {
        double mean;
        double median;
        double standardDeviation;
        double minimum;
        double maximum;
        double percentile95;
        double percentile99;
    };

    StatisticalSummary calculateSummary(const std::vector<double>& data);

    // Alert formatting
    std::string formatAlert(const VaRCalculator::RiskAlert& alert);
    std::string getAlertSeverityText(VaRCalculator::RiskAlert::AlertLevel level);
}

} // namespace Risk
} // namespace CryptoClaude