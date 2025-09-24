#pragma once

#include "Providers/CryptoCompareProvider.h"
#include "Providers/NewsApiProvider.h"
#include "../Database/Models/MarketData.h"
#include "../Database/Models/SentimentData.h"
#include <memory>
#include <vector>
#include <string>
#include <chrono>
#include <functional>
#include <thread>
#include <atomic>
#include <map>

namespace CryptoClaude {
namespace Data {

using namespace CryptoClaude::Data::Providers;
using namespace CryptoClaude::Database::Models;

// Processing job configuration
struct ProcessingJob {
    std::string id;
    std::string name;
    std::string description;
    std::chrono::minutes interval;  // How often to run
    bool enabled = true;
    std::chrono::system_clock::time_point lastRun;
    std::chrono::system_clock::time_point nextRun;
    int successCount = 0;
    int failureCount = 0;
    std::string lastError;

    bool isDue() const {
        return std::chrono::system_clock::now() >= nextRun;
    }

    void updateNextRun() {
        lastRun = std::chrono::system_clock::now();
        nextRun = lastRun + interval;
    }
};

// Processing result
struct ProcessingResult {
    bool success = false;
    std::string jobId;
    std::string errorMessage;
    int recordsProcessed = 0;
    std::chrono::milliseconds processingTime = std::chrono::milliseconds(0);
    std::chrono::system_clock::time_point timestamp;

    // Data quality metrics
    int validRecords = 0;
    int invalidRecords = 0;
    int duplicateRecords = 0;
    double qualityScore = 0.0; // 0-1 scale
};

// Data quality assessment
struct DataQualityMetrics {
    double completeness = 0.0;     // % of expected data received
    double accuracy = 0.0;         // % of valid data
    double timeliness = 0.0;       // How fresh is the data
    double consistency = 0.0;      // Internal consistency checks
    double uniqueness = 0.0;       // Duplicate detection

    double overallScore = 0.0;     // Combined quality score
    std::vector<std::string> issues;  // Quality issues found
};

// Main data processing service
class DataProcessingService {
private:
    // Data providers
    std::shared_ptr<CryptoCompareProvider> marketDataProvider_;
    std::shared_ptr<NewsApiProvider> newsProvider_;

    // Configuration
    std::vector<std::string> trackedSymbols_;
    bool enableLogging_;
    bool enableQualityChecks_;

    // Job management
    std::map<std::string, ProcessingJob> jobs_;
    std::atomic<bool> isRunning_;
    std::thread processingThread_;

    // Statistics
    std::atomic<int> totalJobsRun_;
    std::atomic<int> successfulJobs_;
    std::atomic<int> failedJobs_;
    std::chrono::system_clock::time_point serviceStartTime_;

    // Callbacks for processed data
    std::function<void(const std::vector<MarketData>&)> marketDataCallback_;
    std::function<void(const std::vector<SentimentData>&)> sentimentDataCallback_;
    std::function<void(const ProcessingResult&)> jobCompletionCallback_;

public:
    explicit DataProcessingService(std::shared_ptr<CryptoCompareProvider> marketProvider,
                                 std::shared_ptr<NewsApiProvider> newsProvider,
                                 const std::vector<std::string>& symbols = {});

    virtual ~DataProcessingService();

    // Service control
    void start();
    void stop();
    bool isRunning() const { return isRunning_; }

    // Job management
    void addJob(const ProcessingJob& job);
    void removeJob(const std::string& jobId);
    void enableJob(const std::string& jobId, bool enabled = true);
    std::vector<ProcessingJob> getJobs() const;
    ProcessingJob getJob(const std::string& jobId) const;

    // Default jobs setup
    void setupDefaultJobs();
    void addMarketDataJob(std::chrono::minutes interval = std::chrono::minutes(5));
    void addSentimentDataJob(std::chrono::minutes interval = std::chrono::minutes(15));
    void addDataQualityJob(std::chrono::minutes interval = std::chrono::minutes(60));

    // Manual processing
    ProcessingResult processMarketData();
    ProcessingResult processSentimentData();
    ProcessingResult processDataQuality();

    // Configuration
    void setTrackedSymbols(const std::vector<std::string>& symbols) { trackedSymbols_ = symbols; }
    void addTrackedSymbol(const std::string& symbol);
    void removeTrackedSymbol(const std::string& symbol);
    std::vector<std::string> getTrackedSymbols() const { return trackedSymbols_; }

    void enableLogging(bool enable) { enableLogging_ = enable; }
    void enableQualityChecks(bool enable) { enableQualityChecks_ = enable; }

    // Callbacks
    void setMarketDataCallback(std::function<void(const std::vector<MarketData>&)> callback) {
        marketDataCallback_ = callback;
    }

    void setSentimentDataCallback(std::function<void(const std::vector<SentimentData>&)> callback) {
        sentimentDataCallback_ = callback;
    }

    void setJobCompletionCallback(std::function<void(const ProcessingResult&)> callback) {
        jobCompletionCallback_ = callback;
    }

    // Statistics and monitoring
    struct ServiceStatistics {
        std::chrono::system_clock::time_point startTime;
        std::chrono::duration<double> uptime;
        int totalJobsRun = 0;
        int successfulJobs = 0;
        int failedJobs = 0;
        double successRate = 0.0;
        int activeJobs = 0;
        std::chrono::system_clock::time_point lastProcessing;
        DataQualityMetrics overallDataQuality;
    };

    ServiceStatistics getStatistics() const;
    void resetStatistics();

    // Health check
    bool isHealthy() const;
    std::vector<std::string> getHealthIssues() const;

    // Data quality assessment
    DataQualityMetrics assessMarketDataQuality(const std::vector<MarketData>& data) const;
    DataQualityMetrics assessSentimentDataQuality(const std::vector<SentimentData>& data) const;

private:
    // Main processing loop
    void processingLoop();

    // Job execution
    ProcessingResult executeJob(const ProcessingJob& job);
    void updateJobStatus(const std::string& jobId, bool success, const std::string& error = "");

    // Data processing helpers
    std::vector<MarketData> fetchMarketData();
    std::vector<SentimentData> fetchSentimentData();
    void validateAndCleanMarketData(std::vector<MarketData>& data) const;
    void validateAndCleanSentimentData(std::vector<SentimentData>& data) const;

    // Quality checks
    bool isValidMarketData(const MarketData& data) const;
    bool isValidSentimentData(const SentimentData& data) const;
    void removeDuplicateMarketData(std::vector<MarketData>& data) const;
    void removeDuplicateSentimentData(std::vector<SentimentData>& data) const;

    // Logging and monitoring
    void logInfo(const std::string& message) const;
    void logError(const std::string& message) const;
    void updateStatistics(bool success);
    void notifyJobCompletion(const ProcessingResult& result);

    // Utility methods
    std::string generateJobId(const std::string& prefix) const;
    ProcessingResult createErrorResult(const std::string& jobId, const std::string& error) const;
    ProcessingResult createSuccessResult(const std::string& jobId, int recordsProcessed,
                                       std::chrono::milliseconds processingTime) const;
};

// Factory for creating data processing services
class DataProcessingServiceFactory {
public:
    static std::unique_ptr<DataProcessingService> create(
        const std::string& marketDataApiKey,
        const std::string& newsApiKey,
        const std::vector<std::string>& symbols = {"BTC", "ETH", "ADA", "DOT", "LINK"}
    );

    static std::unique_ptr<DataProcessingService> createWithProviders(
        std::shared_ptr<CryptoCompareProvider> marketProvider,
        std::shared_ptr<NewsApiProvider> newsProvider,
        const std::vector<std::string>& symbols = {}
    );

    static std::unique_ptr<DataProcessingService> createForTesting();
};

// Configuration helper
struct DataProcessingConfig {
    // API configuration
    std::string marketDataApiKey;
    std::string newsApiKey;

    // Processing intervals
    std::chrono::minutes marketDataInterval = std::chrono::minutes(5);
    std::chrono::minutes sentimentDataInterval = std::chrono::minutes(15);
    std::chrono::minutes qualityCheckInterval = std::chrono::minutes(60);

    // Symbols to track
    std::vector<std::string> symbols = {"BTC", "ETH", "ADA", "DOT", "LINK", "UNI", "AAVE", "SUSHI"};

    // Quality settings
    bool enableQualityChecks = true;
    bool enableLogging = true;
    bool autoStart = true;

    // Thresholds
    double minQualityScore = 0.7;    // Minimum acceptable quality score
    int maxRetries = 3;              // Max retries for failed jobs
    std::chrono::seconds jobTimeout = std::chrono::seconds(120);  // Job timeout

    bool isValid() const {
        return !marketDataApiKey.empty() || !newsApiKey.empty();  // At least one API key required
    }

    std::vector<std::string> getConfigurationErrors() const {
        std::vector<std::string> errors;
        if (marketDataApiKey.empty() && newsApiKey.empty()) {
            errors.push_back("At least one API key (market data or news) is required");
        }
        if (symbols.empty()) {
            errors.push_back("At least one symbol must be configured for tracking");
        }
        if (minQualityScore < 0.0 || minQualityScore > 1.0) {
            errors.push_back("Quality score must be between 0.0 and 1.0");
        }
        return errors;
    }
};

} // namespace Data
} // namespace CryptoClaude