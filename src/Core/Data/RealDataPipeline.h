#pragma once

#include "ProductionApiManager.h"
#include "../Database/DatabaseManager.h"
#include "../Database/DataQualityManager.h"
#include "../Database/Models/MarketData.h"
#include "../Database/Models/SentimentData.h"
#include "../Quality/LiveDataQualityValidator.h"
#include "../Analytics/PerformanceTracker.h"
#include <memory>
#include <vector>
#include <map>
#include <string>
#include <chrono>
#include <atomic>
#include <mutex>
#include <thread>
#include <queue>
#include <functional>

namespace CryptoClaude {
namespace Data {

/**
 * Real Data Pipeline for production-ready small-scale data ingestion,
 * validation, and storage with comprehensive quality control and monitoring
 */
class RealDataPipeline {
public:
    RealDataPipeline(ProductionApiManager& apiManager,
                    Database::DatabaseManager& dbManager,
                    Database::DataQualityManager& qualityManager);
    ~RealDataPipeline();

    // === INITIALIZATION AND LIFECYCLE ===

    bool initialize();
    void shutdown();
    bool isInitialized() const { return initialized_; }

    // === PIPELINE CONFIGURATION ===

    struct PipelineConfig {
        // Data sources
        std::vector<std::string> primarySymbols = {"BTC", "ETH", "ADA", "DOT", "SOL"};
        std::vector<std::string> preferredProviders = {"cryptocompare", "newsapi"};

        // Data collection settings
        std::chrono::minutes marketDataInterval = std::chrono::minutes(15);
        std::chrono::hours sentimentDataInterval = std::chrono::hours(4);
        std::chrono::hours newsUpdateInterval = std::chrono::hours(2);

        // Quality thresholds
        double minDataQualityScore = 0.90;
        std::chrono::minutes maxDataAge = std::chrono::minutes(30);
        int maxConsecutiveFailures = 5;

        // Processing limits
        int maxConcurrentRequests = 10;
        int batchSize = 20;
        std::chrono::milliseconds processingTimeout = std::chrono::milliseconds(60000);

        // Storage settings
        bool enableRealTimeStorage = true;
        bool enableDataBackup = true;
        int dataRetentionDays = 365;
    };

    void setPipelineConfig(const PipelineConfig& config);
    PipelineConfig getPipelineConfig() const { return config_; }

    // === REAL-TIME DATA COLLECTION ===

    // Start/stop continuous data collection
    bool startRealTimeCollection();
    bool stopRealTimeCollection();
    bool isCollectionActive() const { return collectionActive_; }

    // Manual data collection triggers
    bool collectMarketDataNow(const std::vector<std::string>& symbols = {});
    bool collectSentimentDataNow(const std::vector<std::string>& symbols = {});
    bool collectNewsDataNow();

    // Historical data backfill
    bool backfillMarketData(const std::string& symbol,
                           const std::chrono::system_clock::time_point& startDate,
                           const std::chrono::system_clock::time_point& endDate);

    bool backfillSentimentData(const std::vector<std::string>& symbols,
                              const std::chrono::system_clock::time_point& startDate,
                              const std::chrono::system_clock::time_point& endDate);

    // === DATA VALIDATION AND QUALITY CONTROL ===

    struct DataValidationResult {
        bool isValid;
        double qualityScore;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
        std::chrono::system_clock::time_point validationTime;
        std::string dataSource;
        int dataPointsValidated;
        int validDataPoints;
        int invalidDataPoints;
    };

    DataValidationResult validateMarketData(const std::vector<Database::Models::MarketData>& data);
    DataValidationResult validateSentimentData(const std::vector<Database::Models::SentimentData>& data);

    // Quality monitoring
    double getCurrentDataQualityScore() const;
    std::map<std::string, double> getProviderQualityScores() const;

    // === PIPELINE MONITORING AND METRICS ===

    struct PipelineMetrics {
        std::string symbol;
        std::chrono::system_clock::time_point lastUpdate;
        std::chrono::system_clock::time_point lastSuccess;

        // Collection statistics
        int totalCollectionAttempts;
        int successfulCollections;
        int failedCollections;
        double successRate;

        // Data quality metrics
        double averageQualityScore;
        double currentQualityScore;
        int consecutiveFailures;

        // Performance metrics
        std::chrono::milliseconds averageLatency;
        std::chrono::milliseconds lastLatency;

        // Data volume
        int dataPointsCollected;
        int dataPointsStored;
        size_t totalDataSize;
    };

    PipelineMetrics getPipelineMetrics(const std::string& symbol) const;
    std::map<std::string, PipelineMetrics> getAllPipelineMetrics() const;

    struct SystemHealth {
        bool overallHealth;
        double systemQualityScore;
        int activeConnections;
        int healthyProviders;
        int activeSymbols;
        std::chrono::system_clock::time_point lastHealthCheck;
        std::vector<std::string> activeWarnings;
        std::vector<std::string> criticalErrors;
    };

    SystemHealth getSystemHealth();
    void performHealthCheck();

    // === ERROR HANDLING AND RECOVERY ===

    enum class ErrorSeverity {
        INFO,
        WARNING,
        ERROR,
        CRITICAL
    };

    struct PipelineError {
        ErrorSeverity severity;
        std::string component;
        std::string symbol;
        std::string message;
        std::string details;
        std::chrono::system_clock::time_point timestamp;
        int errorCode;
    };

    std::vector<PipelineError> getRecentErrors(int hours = 24) const;
    void clearErrors(ErrorSeverity maxSeverity = ErrorSeverity::WARNING);

    // Error recovery
    bool attemptAutomaticRecovery();
    bool resetSymbolCollection(const std::string& symbol);

    // === DATA ACCESS AND RETRIEVAL ===

    // Real-time data access
    std::vector<Database::Models::MarketData> getLatestMarketData(
        const std::string& symbol,
        int maxPoints = 100);

    std::vector<Database::Models::SentimentData> getLatestSentimentData(
        const std::string& symbol,
        int maxPoints = 50);

    // Data availability checks
    bool isMarketDataFresh(const std::string& symbol,
                          std::chrono::minutes maxAge = std::chrono::minutes(30));
    bool isSentimentDataFresh(const std::string& symbol,
                             std::chrono::hours maxAge = std::chrono::hours(4));

    std::chrono::system_clock::time_point getLastMarketDataUpdate(const std::string& symbol);
    std::chrono::system_clock::time_point getLastSentimentDataUpdate(const std::string& symbol);

    // === CALLBACK AND EVENT SYSTEM ===

    using DataUpdateCallback = std::function<void(const std::string& symbol, const std::string& dataType)>;
    using ErrorCallback = std::function<void(const PipelineError& error)>;
    using QualityCallback = std::function<void(const std::string& symbol, double qualityScore)>;

    void setDataUpdateCallback(DataUpdateCallback callback) { dataUpdateCallback_ = callback; }
    void setErrorCallback(ErrorCallback callback) { errorCallback_ = callback; }
    void setQualityCallback(QualityCallback callback) { qualityCallback_ = callback; }

    // === PERFORMANCE OPTIMIZATION ===

    // Cache management
    void clearDataCache();
    void preloadDataCache(const std::vector<std::string>& symbols);

    // Performance tuning
    void optimizeBatchSizes();
    void adjustCollectionFrequencies();

    // Resource management
    struct ResourceUsage {
        double cpuUsage;
        size_t memoryUsage;
        int networkConnections;
        int databaseConnections;
        double diskIORate;
        double networkIORate;
    };

    ResourceUsage getCurrentResourceUsage();
    void setResourceLimits(double maxCpuUsage, size_t maxMemoryUsage);

private:
    // Core dependencies
    ProductionApiManager& apiManager_;
    Database::DatabaseManager& dbManager_;
    Database::DataQualityManager& qualityManager_;
    std::unique_ptr<Quality::LiveDataQualityValidator> qualityValidator_;
    std::unique_ptr<Analytics::PerformanceTracker> performanceTracker_;

    // Configuration and state
    PipelineConfig config_;
    std::atomic<bool> initialized_;
    std::atomic<bool> collectionActive_;
    std::atomic<bool> shutdownRequested_;

    // Threading and concurrency
    std::unique_ptr<std::thread> collectionThread_;
    std::unique_ptr<std::thread> validationThread_;
    std::unique_ptr<std::thread> monitoringThread_;
    std::mutex pipelineMutex_;

    // Data processing queue
    struct DataRequest {
        std::string symbol;
        std::string dataType; // "market", "sentiment", "news"
        std::chrono::system_clock::time_point requestTime;
        int priority; // Lower number = higher priority
    };

    std::queue<DataRequest> dataRequestQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCondition_;

    // Metrics and monitoring
    mutable std::map<std::string, PipelineMetrics> symbolMetrics_;
    mutable std::vector<PipelineError> errorHistory_;
    mutable std::mutex metricsMutex_;

    // Quality tracking
    mutable std::map<std::string, double> providerQualityScores_;
    mutable std::chrono::system_clock::time_point lastQualityCheck_;

    // Callbacks
    DataUpdateCallback dataUpdateCallback_;
    ErrorCallback errorCallback_;
    QualityCallback qualityCallback_;

    // === PRIVATE IMPLEMENTATION METHODS ===

    // Initialization helpers
    bool initializeQualityValidator();
    bool initializePerformanceTracker();
    bool createDatabaseTables();

    // Collection thread implementations
    void collectionThreadMain();
    void validationThreadMain();
    void monitoringThreadMain();

    // Data collection workers
    bool collectMarketDataForSymbol(const std::string& symbol);
    bool collectSentimentDataForSymbol(const std::string& symbol);
    bool collectNewsData();

    // Data processing helpers
    bool processMarketData(const std::string& symbol,
                          const std::vector<Database::Models::MarketData>& data);
    bool processSentimentData(const std::string& symbol,
                             const std::vector<Database::Models::SentimentData>& data);

    // Validation implementations
    DataValidationResult performMarketDataValidation(
        const std::vector<Database::Models::MarketData>& data);
    DataValidationResult performSentimentDataValidation(
        const std::vector<Database::Models::SentimentData>& data);

    // Storage operations
    bool storeMarketData(const std::vector<Database::Models::MarketData>& data);
    bool storeSentimentData(const std::vector<Database::Models::SentimentData>& data);

    // Quality monitoring
    void updateQualityMetrics(const std::string& symbol, double qualityScore);
    void checkQualityThresholds(const std::string& symbol);

    // Error handling
    void recordError(ErrorSeverity severity, const std::string& component,
                    const std::string& symbol, const std::string& message,
                    const std::string& details = "", int errorCode = 0);
    void notifyError(const PipelineError& error);

    // Performance tracking
    void updatePerformanceMetrics(const std::string& symbol, bool success,
                                 std::chrono::milliseconds latency, int dataPoints);

    // Queue management
    void addDataRequest(const std::string& symbol, const std::string& dataType, int priority = 5);
    DataRequest getNextDataRequest();
    void clearDataRequestQueue();

    // Provider management
    std::string selectBestProvider(const std::string& dataType);
    void updateProviderQuality(const std::string& providerId, double qualityScore);

    // Health monitoring
    bool checkSymbolHealth(const std::string& symbol);
    bool checkProviderHealth();
    bool checkSystemResources();

    // Recovery operations
    bool recoverFromProviderFailure(const std::string& providerId);
    bool recoverFromQualityDegradation(const std::string& symbol);
    bool recoverFromSystemOverload();

    // Data lifecycle management
    void performDataCleanup();
    void performDataBackup();
    void optimizeDataStorage();
};

} // namespace Data
} // namespace CryptoClaude