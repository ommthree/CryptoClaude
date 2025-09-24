#pragma once

#include "../DataIngestion/DataIngestionService.h"
#include "../Database/DataQualityManager.h"
#include "../Database/MigrationManager.h"
#include "../Data/Providers/CryptoCompareProvider.h"
#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <functional>

namespace CryptoClaude {
namespace DataPipeline {

using namespace CryptoClaude::DataIngestion;
using namespace CryptoClaude::Database;
using namespace CryptoClaude::Data::Providers;

// Enhanced pipeline configuration
struct PipelineConfig {
    // Data ingestion settings
    std::vector<std::string> symbols = {"BTC", "ETH", "ADA", "DOT", "LINK", "UNI", "AAVE", "SUSHI", "COMP", "MKR"};
    int historicalDays = 365;
    bool includeHourlyData = true;

    // Quality control settings
    double qualityThreshold = 0.95;
    bool enableAutomaticRemediation = true;
    bool enableRealTimeValidation = true;

    // CryptoCompare API settings
    std::string apiKey;
    int rateLimitDelayMs = 1000;
    int batchSize = 10;
    int maxRetries = 3;

    // Scheduling settings
    std::chrono::minutes ingestionInterval = std::chrono::minutes(60); // Hourly by default
    bool enableScheduledIngestion = false;

    bool isValid() const {
        return !apiKey.empty() && !symbols.empty() && qualityThreshold > 0.0 && qualityThreshold <= 1.0;
    }

    std::vector<std::string> getValidationErrors() const {
        std::vector<std::string> errors;
        if (apiKey.empty()) errors.push_back("CryptoCompare API key is required");
        if (symbols.empty()) errors.push_back("Symbol list cannot be empty");
        if (qualityThreshold <= 0.0 || qualityThreshold > 1.0) errors.push_back("Quality threshold must be between 0.0 and 1.0");
        if (rateLimitDelayMs < 100) errors.push_back("Rate limit delay must be at least 100ms");
        return errors;
    }
};

// Pipeline execution result
struct PipelineResult {
    bool success = false;
    std::string errorMessage;

    // Ingestion statistics
    int symbolsProcessed = 0;
    int dataPointsIngested = 0;
    int dataPointsRejected = 0;

    // Quality statistics
    double overallQualityScore = 0.0;
    int qualityIssuesFound = 0;
    int qualityIssuesRemediated = 0;

    // Timing information
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;
    std::chrono::milliseconds duration = std::chrono::milliseconds(0);

    // Performance metrics
    double averageIngestionRate = 0.0; // data points per second
    std::chrono::milliseconds averageResponseTime = std::chrono::milliseconds(0);
};

// Progress tracking for real-time updates
struct PipelineProgress {
    enum class Stage {
        Initializing,
        IngestingMarketData,
        ValidatingQuality,
        PerformingRemediation,
        CalculatingMetrics,
        Finalizing,
        Complete,
        Error
    };

    Stage currentStage = Stage::Initializing;
    std::string stageDescription;
    int currentItem = 0;
    int totalItems = 0;
    double overallProgress = 0.0; // 0.0 to 1.0
    std::string currentSymbol;
    std::string statusMessage;
};

// Callback types
using ProgressCallback = std::function<void(const PipelineProgress&)>;
using QualityAlertCallback = std::function<void(const DataAnomalyInfo&)>;

// Enhanced Market Data Pipeline
class EnhancedMarketDataPipeline {
public:
    explicit EnhancedMarketDataPipeline(DatabaseManager& dbManager);
    ~EnhancedMarketDataPipeline() = default;

    // Configuration
    bool configure(const PipelineConfig& config);
    bool isConfigured() const { return configured_; }
    PipelineConfig getConfig() const { return config_; }

    // Pipeline execution
    PipelineResult runFullPipeline();
    PipelineResult runIncrementalUpdate();
    PipelineResult runQualityAssessment();
    PipelineResult runDataRemediation();

    // Specific pipeline operations
    PipelineResult ingestHistoricalData(const std::vector<std::string>& symbols);
    PipelineResult ingestRealtimeData(const std::vector<std::string>& symbols);
    PipelineResult validateAndRemediateData();

    // Callback management
    void setProgressCallback(ProgressCallback callback) { progressCallback_ = callback; }
    void setQualityAlertCallback(QualityAlertCallback callback) { qualityAlertCallback_ = callback; }

    // Pipeline status and monitoring
    bool isPipelineRunning() const { return pipelineRunning_; }
    PipelineProgress getCurrentProgress() const { return currentProgress_; }
    PipelineResult getLastResult() const { return lastResult_; }
    std::string getLastError() const { return lastError_; }

    // Statistics and metrics
    struct PipelineStatistics {
        int totalRuns = 0;
        int successfulRuns = 0;
        int failedRuns = 0;
        double successRate = 0.0;

        int totalDataPointsProcessed = 0;
        int totalQualityIssuesFound = 0;
        int totalQualityIssuesFixed = 0;

        std::chrono::milliseconds totalExecutionTime = std::chrono::milliseconds(0);
        std::chrono::milliseconds averageExecutionTime = std::chrono::milliseconds(0);

        std::chrono::system_clock::time_point lastSuccessfulRun;
        std::chrono::system_clock::time_point lastFailedRun;
    };

    PipelineStatistics getStatistics() const { return statistics_; }
    void resetStatistics();

    // Health check
    bool performHealthCheck();
    std::vector<std::string> getHealthCheckResults() const;

    // Advanced features
    bool enableScheduledExecution(std::chrono::minutes interval);
    void disableScheduledExecution();
    bool isScheduledExecutionEnabled() const { return scheduledExecution_; }

    // Data quality integration
    std::vector<QualityMetric> getQualityMetrics() const;
    std::vector<DataAnomalyInfo> getDetectedAnomalies() const;
    double getOverallDataQuality() const;

private:
    // Core components
    DatabaseManager& dbManager_;
    std::unique_ptr<DataIngestionService> ingestionService_;
    std::unique_ptr<CryptoCompareProvider> cryptoCompareProvider_;

    // Configuration and state
    PipelineConfig config_;
    bool configured_;
    bool pipelineRunning_;
    std::string lastError_;

    // Progress tracking
    PipelineProgress currentProgress_;
    PipelineResult lastResult_;
    PipelineStatistics statistics_;

    // Callbacks
    ProgressCallback progressCallback_;
    QualityAlertCallback qualityAlertCallback_;

    // Scheduled execution
    bool scheduledExecution_;
    std::chrono::minutes scheduledInterval_;
    std::chrono::system_clock::time_point lastScheduledRun_;

    // Initialization and setup
    bool initializeComponents();
    bool initializeCryptoCompareProvider();
    bool initializeIngestionService();
    bool validateConfiguration() const;

    // Pipeline execution helpers
    PipelineResult executeStage(PipelineProgress::Stage stage,
                               const std::function<bool()>& operation,
                               const std::string& description);

    void updateProgress(PipelineProgress::Stage stage,
                       int current, int total,
                       const std::string& description,
                       const std::string& currentSymbol = "");

    void updateProgress(double overallProgress, const std::string& message);

    // Data ingestion operations
    bool performHistoricalIngestion(const std::vector<std::string>& symbols);
    bool performRealtimeIngestion(const std::vector<std::string>& symbols);
    bool processSymbol(const std::string& symbol, bool includeHistorical);

    // Quality control operations
    bool performQualityValidation();
    bool performAutomaticRemediation();
    bool processQualityIssues(const std::vector<DataAnomalyInfo>& anomalies);

    // Metrics calculation
    bool calculateDerivedMetrics();
    bool updatePipelineStatistics(const PipelineResult& result);

    // Error handling and logging
    PipelineResult createErrorResult(const std::string& errorMessage);
    PipelineResult createSuccessResult();
    void logPipelineStart(const std::string& operation);
    void logPipelineComplete(const std::string& operation, bool success);
    void logError(const std::string& error);
    void logQualityIssue(const DataAnomalyInfo& anomaly);

    // Validation helpers
    bool validateSymbolList(const std::vector<std::string>& symbols) const;
    bool isSymbolSupported(const std::string& symbol) const;

    // Utility methods
    std::vector<std::string> getConfiguredSymbols() const { return config_.symbols; }
    std::chrono::milliseconds calculateDuration(
        const std::chrono::system_clock::time_point& start,
        const std::chrono::system_clock::time_point& end) const;

    void resetProgress();
    void markComplete(bool success);
};

// Pipeline factory for different use cases
class EnhancedPipelineFactory {
public:
    // Create pipeline with default configuration
    static std::unique_ptr<EnhancedMarketDataPipeline> createDefault(DatabaseManager& dbManager,
                                                                    const std::string& apiKey);

    // Create pipeline for specific use cases
    static std::unique_ptr<EnhancedMarketDataPipeline> createForTesting(DatabaseManager& dbManager);
    static std::unique_ptr<EnhancedMarketDataPipeline> createForProduction(DatabaseManager& dbManager,
                                                                           const PipelineConfig& config);

    // Create pipeline with specific symbol sets
    static std::unique_ptr<EnhancedMarketDataPipeline> createForTopCoins(DatabaseManager& dbManager,
                                                                         const std::string& apiKey,
                                                                         int topN = 50);
};

// Pipeline configuration builder for ease of use
class PipelineConfigBuilder {
public:
    PipelineConfigBuilder() = default;

    PipelineConfigBuilder& withApiKey(const std::string& apiKey);
    PipelineConfigBuilder& withSymbols(const std::vector<std::string>& symbols);
    PipelineConfigBuilder& withQualityThreshold(double threshold);
    PipelineConfigBuilder& withHistoricalDays(int days);
    PipelineConfigBuilder& withRateLimit(int delayMs);
    PipelineConfigBuilder& withBatchSize(int size);
    PipelineConfigBuilder& withAutomaticRemediation(bool enabled);
    PipelineConfigBuilder& withScheduledExecution(std::chrono::minutes interval);

    PipelineConfig build() const { return config_; }

private:
    PipelineConfig config_;
};

} // namespace DataPipeline
} // namespace CryptoClaude