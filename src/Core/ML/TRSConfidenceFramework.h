#pragma once

#include <vector>
#include <map>
#include <chrono>
#include <functional>
#include "SortingAlgorithmPerformance.h"

namespace CryptoClaude {
namespace ML {

/**
 * TRS-Compliant Confidence Framework for Phase 1 Implementation
 * Implements >85% correlation requirement with comprehensive validation
 * Conservative approach with statistical rigor and out-of-sample testing
 */
class TRSConfidenceFramework {
public:
    // TRS-specific confidence requirements
    struct TRSConfidenceRequirements {
        double minCorrelationThreshold = 0.85;      // TRS >85% requirement
        int minSampleSize = 200;                    // Minimum predictions for significance
        int outOfSampleDays = 180;                  // 6-month holdout period
        double minStatisticalSignificance = 0.95;   // 95% confidence level

        // Enhanced validation requirements
        bool requiresWalkForwardAnalysis = true;    // Rolling validation
        bool requiresRegimeStabilityTest = true;    // Bull/bear/sideways consistency
        bool requiresBootstrapValidation = true;    // Bootstrap confidence intervals
        bool requiresMultiTimeframeTest = true;     // Multiple timeframe validation

        // Performance requirements
        double minAccuracyThreshold = 0.55;         // Minimum 55% accuracy
        double maxVarianceThreshold = 0.15;         // Maximum 15% variance
        int minConsistentPeriods = 12;              // Minimum consistent validation periods
    };

    // Comprehensive confidence metrics for TRS compliance
    struct TRSConfidenceMetrics {
        // Core confidence measures
        double correlationWithOutcomes;             // Primary TRS metric (>85%)
        double predictionAccuracy;                  // Directional accuracy
        double confidenceCalibration;               // Confidence vs outcome alignment
        double rankingStability;                    // Ranking consistency over time

        // Statistical validation results
        double statisticalSignificance;             // P-value for correlation
        double bootstrapConfidenceInterval[2];      // Lower and upper bounds
        double walkForwardConsistency;              // Walk-forward analysis result
        double regimeStabilityScore;                // Performance across market regimes

        // Sample and timing metrics
        int totalSampleSize;                        // Total predictions analyzed
        int outOfSampleSize;                        // Out-of-sample predictions
        std::chrono::system_clock::time_point lastUpdate;
        double validationWindowDays;                // Days of validation data

        // TRS compliance status
        bool meetsTRSRequirement;                   // >85% correlation achieved
        bool passesStatisticalTests;                // All statistical validations pass
        bool sufficientSampleSize;                  // Adequate sample for significance
        bool meetsConsistencyRequirements;          // Consistent across periods

        // Confidence level determination
        enum class TRSConfidenceLevel {
            INSUFFICIENT,    // <85%, does not meet TRS requirement
            MARGINAL,        // 85-87%, meets minimum TRS requirement
            GOOD,           // 87-90%, exceeds TRS requirement
            EXCELLENT,      // 90-93%, strong TRS compliance
            EXCEPTIONAL     // >93%, exceptional TRS compliance
        } confidenceLevel;

        double getOverallTRSScore() const;
        bool isReadyForProduction() const;
        std::vector<std::string> getComplianceIssues() const;
    };

    // Out-of-sample validation framework
    struct OutOfSampleValidator {
        std::vector<PairPrediction> holdoutPredictions;  // Never used in training
        std::vector<double> holdoutOutcomes;             // Actual results for holdout
        std::chrono::system_clock::time_point holdoutStart;
        std::chrono::system_clock::time_point holdoutEnd;

        double calculateHoldoutCorrelation() const;
        double calculateHoldoutAccuracy() const;
        bool validateHoldoutSignificance() const;
        void generateHoldoutReport() const;
    };

    // Walk-forward analysis for temporal stability
    struct WalkForwardAnalyzer {
        struct ValidationWindow {
            std::chrono::system_clock::time_point startDate;
            std::chrono::system_clock::time_point endDate;
            std::vector<PairPrediction> predictions;
            std::vector<double> outcomes;
            double windowCorrelation;
            double windowAccuracy;
            bool windowPassed;
        };

        std::vector<ValidationWindow> validationWindows;
        int windowDays = 90;                        // 3-month windows
        int stepDays = 30;                          // 1-month steps
        double minWindowCorrelation = 0.80;         // Minimum per-window correlation

        bool performWalkForwardAnalysis();
        double calculateOverallConsistency() const;
        std::vector<ValidationWindow> getFailedWindows() const;
        void generateWalkForwardReport() const;
    };

    // Bootstrap confidence interval calculator
    struct BootstrapValidator {
        int bootstrapIterations = 1000;             // Bootstrap sample count
        double confidenceLevel = 0.95;              // 95% confidence intervals
        std::vector<double> bootstrapCorrelations;   // Bootstrap correlation samples

        std::pair<double, double> calculateConfidenceInterval(
            const std::vector<PairPrediction>& predictions,
            const std::vector<double>& outcomes
        );
        bool validateBootstrapSignificance(double observedCorrelation) const;
        void generateBootstrapReport() const;
    };

public:
    TRSConfidenceFramework();
    ~TRSConfidenceFramework() = default;

    // Main TRS confidence assessment
    TRSConfidenceMetrics calculateTRSCompliantConfidence();
    bool validateTRSRequirements();
    void updateConfidenceMetrics(const std::vector<PairPrediction>& newPredictions);

    // Component validators
    OutOfSampleValidator& getOutOfSampleValidator() { return outOfSampleValidator_; }
    WalkForwardAnalyzer& getWalkForwardAnalyzer() { return walkForwardAnalyzer_; }
    BootstrapValidator& getBootstrapValidator() { return bootstrapValidator_; }

    // Configuration and requirements
    void setTRSRequirements(const TRSConfidenceRequirements& requirements);
    TRSConfidenceRequirements getTRSRequirements() const;

    // Real-time monitoring
    void enableRealTimeMonitoring(std::chrono::minutes updateFrequency);
    void disableRealTimeMonitoring();
    bool isMonitoringActive() const;

    // Callbacks for confidence updates
    void setConfidenceUpdateCallback(std::function<void(const TRSConfidenceMetrics&)> callback);
    void setTRSComplianceCallback(std::function<void(bool)> callback);

    // Reporting and analysis
    void generateTRSComplianceReport() const;
    void exportConfidenceData(const std::string& filename) const;
    void printConfidenceSummary() const;

    // Integration with existing performance tracker
    void integrateWithPerformanceTracker(SortingAlgorithmPerformance& performanceTracker);

private:
    // Internal state
    TRSConfidenceRequirements requirements_;
    TRSConfidenceMetrics lastMetrics_;

    // Component validators
    OutOfSampleValidator outOfSampleValidator_;
    WalkForwardAnalyzer walkForwardAnalyzer_;
    BootstrapValidator bootstrapValidator_;

    // Real-time monitoring
    bool monitoringActive_ = false;
    std::chrono::minutes updateFrequency_{60};
    std::function<void(const TRSConfidenceMetrics&)> confidenceCallback_;
    std::function<void(bool)> complianceCallback_;

    // Integration
    SortingAlgorithmPerformance* performanceTracker_ = nullptr;

    // Internal calculation methods
    double calculateCorrelationWithOutcomes(const std::vector<PairPrediction>& predictions) const;
    double calculateConfidenceCalibration(const std::vector<PairPrediction>& predictions) const;
    double assessRegimeStability() const;
    double calculateStatisticalSignificance(double correlation, int sampleSize) const;

    // Helper methods
    std::vector<PairPrediction> getValidPredictions() const;
    std::vector<PairPrediction> getOutOfSamplePredictions() const;
    bool performStatisticalTests(const TRSConfidenceMetrics& metrics) const;
    void updateComplianceStatus(TRSConfidenceMetrics& metrics) const;

    // Constants
    static constexpr double TRS_CORRELATION_THRESHOLD = 0.85;
    static constexpr double CRITICAL_CORRELATION_THRESHOLD = 0.93;
    static constexpr int MIN_TRS_SAMPLE_SIZE = 200;
};

/**
 * Real-time confidence monitoring system for continuous TRS compliance
 * Provides alerts and automatic adjustments based on confidence levels
 */
class TRSConfidenceMonitor {
public:
    // Monitoring configuration
    struct MonitoringConfig {
        std::chrono::minutes updateFrequency{30};   // 30-minute updates
        double warningThreshold = 0.87;             // Warning below 87%
        double criticalThreshold = 0.85;            // Critical at TRS threshold

        bool enableAlerts = true;                   // Enable alert notifications
        bool enableAutoAdjustment = true;           // Auto-adjust concentration
        bool enableEmergencyShutdown = true;        // Emergency protocols

        // Alert thresholds
        int consecutiveWarnings = 3;                // Warnings before action
        int consecutiveCritical = 1;                // Critical alerts before emergency
    };

    // Alert system
    struct ConfidenceAlert {
        enum class Severity { INFO, WARNING, CRITICAL, EMERGENCY } severity;
        std::string message;
        double currentConfidence;
        std::chrono::system_clock::time_point timestamp;
        bool acknowledged = false;

        std::string getSeverityString() const;
    };

public:
    TRSConfidenceMonitor(TRSConfidenceFramework& framework);
    ~TRSConfidenceMonitor() = default;

    // Monitoring control
    void startMonitoring();
    void stopMonitoring();
    bool isMonitoring() const { return monitoring_; }

    // Configuration
    void setMonitoringConfig(const MonitoringConfig& config);
    MonitoringConfig getMonitoringConfig() const;

    // Alert management
    std::vector<ConfidenceAlert> getActiveAlerts() const;
    std::vector<ConfidenceAlert> getUnacknowledgedAlerts() const;
    void acknowledgeAlert(size_t alertIndex);
    void clearAllAlerts();

    // Emergency protocols
    bool requiresEmergencyAction() const;
    void triggerEmergencyProtocols();
    void resetEmergencyStatus();

private:
    TRSConfidenceFramework& framework_;
    MonitoringConfig config_;
    bool monitoring_ = false;

    // Alert tracking
    std::vector<ConfidenceAlert> alerts_;
    int consecutiveWarnings_ = 0;
    int consecutiveCritical_ = 0;
    bool emergencyTriggered_ = false;

    // Monitoring loop
    void monitoringLoop();
    void checkConfidenceLevels();
    void generateAlert(ConfidenceAlert::Severity severity, const std::string& message, double confidence);

    // Auto-adjustment logic
    void performAutoAdjustment(double currentConfidence);
};

} // namespace ML
} // namespace CryptoClaude