#pragma once

#include "SmartCacheManager.h"
#include "ApiLimitHandler.h"
#include "../Config/PersonalApiConfig.h"
#include "../Database/DatabaseManager.h"
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <memory>
#include <functional>
#include <atomic>
#include <thread>

namespace CryptoClaude {
namespace Data {

using namespace CryptoClaude::Config;
using namespace CryptoClaude::Database;

// Historical data request specification
struct HistoricalDataRequest {
    std::string symbol;
    std::string provider;
    std::string timeframe; // "1d", "1h", "5m", etc.
    std::chrono::system_clock::time_point startDate;
    std::chrono::system_clock::time_point endDate;
    int priority; // 1-5, 1 being highest priority
    bool isPermanent; // Store permanently in cache

    HistoricalDataRequest() : priority(3), isPermanent(true) {}
};

// Historical data chunk for progressive loading
struct DataChunk {
    std::string symbol;
    std::string provider;
    std::string timeframe;
    std::chrono::system_clock::time_point startDate;
    std::chrono::system_clock::time_point endDate;
    std::string data;
    bool isComplete;
    int retryCount;

    DataChunk() : isComplete(false), retryCount(0) {}
};

// Loading progress tracking
struct LoadingProgress {
    std::string requestId;
    std::string symbol;
    std::string status; // "pending", "in_progress", "completed", "failed", "paused"
    int totalChunks;
    int completedChunks;
    int failedChunks;
    double progressPercent;
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point estimatedCompletion;
    std::string lastError;
    size_t bytesLoaded;

    LoadingProgress() : totalChunks(0), completedChunks(0), failedChunks(0),
                       progressPercent(0.0), bytesLoaded(0) {}
};

// Data validation result
struct ValidationResult {
    bool isValid;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    int totalDataPoints;
    int missingDataPoints;
    int duplicateDataPoints;
    double dataQualityScore; // 0.0 to 1.0
    std::chrono::system_clock::time_point firstDate;
    std::chrono::system_clock::time_point lastDate;

    ValidationResult() : isValid(false), totalDataPoints(0), missingDataPoints(0),
                        duplicateDataPoints(0), dataQualityScore(0.0) {}
};

// Historical data loader with progressive loading and API conservation
class HistoricalDataLoader {
public:
    HistoricalDataLoader(SmartCacheManager& cacheManager,
                        ApiLimitHandler& apiHandler,
                        DatabaseManager& dbManager);
    ~HistoricalDataLoader();

    // Initialization and shutdown
    bool initialize();
    void shutdown();

    // One-time historical data loading for personal trading setup
    struct PersonalSetupConfig {
        std::vector<std::string> symbols;          // Target symbols
        std::vector<std::string> providers;        // Preferred providers in order
        std::vector<std::string> timeframes;       // Required timeframes
        std::chrono::system_clock::time_point startDate; // How far back to load
        int maxDailyApiCalls;                      // Daily API call budget
        int maxSimultaneousLoads;                  // Concurrent loading limit
        bool conservativeMode;                     // Extra conservative API usage
        std::chrono::milliseconds delayBetweenRequests; // Minimum delay

        PersonalSetupConfig() {
            symbols = {"BTC", "ETH", "ADA", "DOT", "LINK"};
            providers = {"cryptocompare", "alphavantage"};
            timeframes = {"1d", "1h"};

            // Load 2 years of data by default
            startDate = std::chrono::system_clock::now() - std::chrono::hours(24 * 730);

            maxDailyApiCalls = 100; // Conservative daily budget
            maxSimultaneousLoads = 1; // One at a time for personal use
            conservativeMode = true;
            delayBetweenRequests = std::chrono::milliseconds(15000); // 15 seconds
        }
    };

    // Main loading functions
    std::string startPersonalSetupLoading(const PersonalSetupConfig& config);
    bool pauseLoading(const std::string& loadingId = "");
    bool resumeLoading(const std::string& loadingId = "");
    bool cancelLoading(const std::string& loadingId = "");

    // Individual data loading
    std::string loadHistoricalData(const HistoricalDataRequest& request);
    std::string loadSymbolData(const std::string& symbol,
                              const std::string& provider,
                              const std::string& timeframe,
                              int days = 730); // 2 years default

    // Progressive loading with chunking
    std::vector<DataChunk> createDataChunks(const HistoricalDataRequest& request,
                                           int chunkSizeDays = 30) const;
    bool loadDataChunk(DataChunk& chunk);

    // Progress monitoring
    LoadingProgress getLoadingProgress(const std::string& loadingId) const;
    std::vector<LoadingProgress> getAllLoadingProgress() const;
    bool isLoadingComplete(const std::string& loadingId) const;

    // Data validation and completeness
    ValidationResult validateHistoricalData(const std::string& symbol,
                                           const std::string& provider,
                                           const std::string& timeframe,
                                           const std::chrono::system_clock::time_point& startDate,
                                           const std::chrono::system_clock::time_point& endDate) const;

    bool checkDataCompleteness(const std::string& symbol,
                              const std::string& timeframe,
                              const std::chrono::system_clock::time_point& startDate,
                              const std::chrono::system_clock::time_point& endDate) const;

    std::vector<std::chrono::system_clock::time_point> findMissingDataPoints(
        const std::string& symbol,
        const std::string& timeframe,
        const std::chrono::system_clock::time_point& startDate,
        const std::chrono::system_clock::time_point& endDate) const;

    // API conservation strategies
    struct ConservationStrategy {
        bool skipWeekendsForDaily;     // Skip weekends for daily crypto data
        bool batchSimilarRequests;     // Batch similar timeframe requests
        bool usePreferredProvider;     // Always try preferred provider first
        bool respectRateLimit;         // Always respect rate limits
        double maxUtilizationPercent;  // Never exceed this API utilization
        std::chrono::hours offPeakStartHour; // Prefer off-peak loading

        ConservationStrategy() : skipWeekendsForDaily(false), batchSimilarRequests(true),
                                usePreferredProvider(true), respectRateLimit(true),
                                maxUtilizationPercent(80.0), offPeakStartHour(std::chrono::hours(2)) {}
    };

    void setConservationStrategy(const ConservationStrategy& strategy);
    ConservationStrategy getConservationStrategy() const { return conservationStrategy_; }

    // Smart scheduling for API conservation
    std::chrono::system_clock::time_point calculateOptimalLoadTime(
        const HistoricalDataRequest& request) const;

    bool isOptimalLoadingTime() const;
    std::vector<HistoricalDataRequest> optimizeRequestOrder(
        const std::vector<HistoricalDataRequest>& requests) const;

    // Data quality assessment
    double assessDataQuality(const std::string& data,
                            const std::string& symbol,
                            const std::string& timeframe) const;

    // Backup and recovery
    bool createLoadingCheckpoint(const std::string& loadingId);
    bool restoreFromCheckpoint(const std::string& loadingId);
    std::vector<std::string> getAvailableCheckpoints() const;

    // Statistics and reporting
    struct LoadingStatistics {
        int totalRequestsSubmitted;
        int totalRequestsCompleted;
        int totalRequestsFailed;
        int totalDataPointsLoaded;
        size_t totalBytesLoaded;
        int apiCallsUsed;
        std::chrono::seconds totalLoadingTime;
        double averageLoadingRate; // Data points per second
        std::map<std::string, int> providerUsage; // Calls per provider
        std::map<std::string, int> symbolProgress; // Progress per symbol

        LoadingStatistics() : totalRequestsSubmitted(0), totalRequestsCompleted(0),
                             totalRequestsFailed(0), totalDataPointsLoaded(0),
                             totalBytesLoaded(0), apiCallsUsed(0),
                             totalLoadingTime(0), averageLoadingRate(0.0) {}
    };

    LoadingStatistics getLoadingStatistics() const;
    void resetStatistics();

    // Event callbacks for monitoring
    using ProgressCallback = std::function<void(const LoadingProgress&)>;
    using CompletionCallback = std::function<void(const std::string&, bool)>; // loadingId, success
    using ErrorCallback = std::function<void(const std::string&, const std::string&)>; // loadingId, error

    void setProgressCallback(ProgressCallback callback) { progressCallback_ = callback; }
    void setCompletionCallback(CompletionCallback callback) { completionCallback_ = callback; }
    void setErrorCallback(ErrorCallback callback) { errorCallback_ = callback; }

    // Advanced features
    bool canLoadWithCurrentLimits(const std::vector<HistoricalDataRequest>& requests) const;
    std::chrono::hours estimateLoadingTime(const std::vector<HistoricalDataRequest>& requests) const;

    // Data gap filling
    bool fillDataGaps(const std::string& symbol,
                     const std::string& timeframe,
                     const std::chrono::system_clock::time_point& startDate,
                     const std::chrono::system_clock::time_point& endDate);

private:
    SmartCacheManager& cacheManager_;
    ApiLimitHandler& apiHandler_;
    DatabaseManager& dbManager_;

    // Configuration
    ConservationStrategy conservationStrategy_;

    // Active loading tracking
    std::map<std::string, LoadingProgress> activeLoadings_;
    std::map<std::string, std::vector<DataChunk>> loadingChunks_;
    std::map<std::string, std::thread> loadingThreads_;

    // Statistics
    LoadingStatistics statistics_;

    // Callbacks
    ProgressCallback progressCallback_;
    CompletionCallback completionCallback_;
    ErrorCallback errorCallback_;

    // Thread safety
    mutable std::mutex loaderMutex_;
    std::atomic<bool> shutdownRequested_;

    // Worker thread management
    std::thread workerThread_;
    std::condition_variable workerCondition_;
    std::queue<std::string> workQueue_;

    // Private methods
    void workerThreadFunction();
    std::string generateLoadingId() const;

    // Data chunk processing
    void processLoadingChunks(const std::string& loadingId);
    bool executeChunkRequest(DataChunk& chunk);
    void updateLoadingProgress(const std::string& loadingId);

    // API optimization
    std::string selectBestProvider(const std::string& symbol, const std::string& timeframe) const;
    bool shouldDelayForRateLimit(const std::string& provider) const;
    void waitForOptimalTiming(const HistoricalDataRequest& request) const;

    // Data processing
    std::string processRawData(const std::string& rawData,
                              const std::string& symbol,
                              const std::string& timeframe) const;

    bool validateDataFormat(const std::string& data,
                           const std::string& symbol,
                           const std::string& timeframe) const;

    // Persistence
    bool saveLoadingProgress(const LoadingProgress& progress);
    LoadingProgress loadLoadingProgress(const std::string& loadingId) const;
    bool saveDataChunks(const std::string& loadingId, const std::vector<DataChunk>& chunks);

    // Utility functions
    std::vector<std::chrono::system_clock::time_point> generateDateRange(
        const std::chrono::system_clock::time_point& start,
        const std::chrono::system_clock::time_point& end,
        const std::string& timeframe) const;

    std::string formatTimePoint(const std::chrono::system_clock::time_point& tp) const;
    std::chrono::system_clock::time_point parseTimePoint(const std::string& timeStr) const;

    // Error handling
    void handleLoadingError(const std::string& loadingId, const std::string& error);
    bool shouldRetryChunk(const DataChunk& chunk) const;
};

// Factory for creating optimized historical data loaders
class HistoricalDataLoaderFactory {
public:
    enum class LoadingStrategy {
        CONSERVATIVE,    // Minimal API usage, maximum delays
        BALANCED,       // Balanced approach for personal use
        AGGRESSIVE,     // Faster loading, higher API usage
        EMERGENCY       // Only load critical data
    };

    static std::unique_ptr<HistoricalDataLoader> create(
        SmartCacheManager& cacheManager,
        ApiLimitHandler& apiHandler,
        DatabaseManager& dbManager,
        LoadingStrategy strategy = LoadingStrategy::BALANCED);

    static HistoricalDataLoader::PersonalSetupConfig getConfigForStrategy(LoadingStrategy strategy);
    static HistoricalDataLoader::ConservationStrategy getConservationForStrategy(LoadingStrategy strategy);
};

} // namespace Data
} // namespace CryptoClaude