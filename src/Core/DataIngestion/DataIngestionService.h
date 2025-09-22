#pragma once
#include "IDataProvider.h"
#include "../Database/Repositories/MarketDataRepository.h"
#include "../Database/Repositories/SentimentRepository.h"
#include <memory>
#include <vector>
#include <functional>
#include <chrono>

namespace CryptoClaude {
namespace DataIngestion {

// Progress callback types
using ProgressCallback = std::function<void(int current, int total, const std::string& status)>;
using CompletionCallback = std::function<void(bool success, const std::string& message)>;

class DataIngestionService {
public:
    DataIngestionService();
    ~DataIngestionService() = default;

    // Provider management
    void setMarketDataProvider(std::unique_ptr<IMarketDataProvider> provider);
    void setSentimentDataProvider(std::unique_ptr<ISentimentDataProvider> provider);

    // Repository management
    void setMarketDataRepository(std::shared_ptr<Database::Repositories::MarketDataRepository> repository);
    void setSentimentRepository(std::shared_ptr<Database::Repositories::SentimentRepository> repository);

    // Callback management
    void setProgressCallback(ProgressCallback callback) { m_progressCallback = callback; }
    void setCompletionCallback(CompletionCallback callback) { m_completionCallback = callback; }

    // Market data ingestion
    bool ingestHistoricalMarketData(
        const std::vector<std::string>& symbols,
        int days = 1000,
        bool includeHourlyData = true);

    bool ingestDailyMarketData(
        const std::vector<std::string>& symbols,
        int days = 365);

    bool ingestHourlyMarketData(
        const std::vector<std::string>& symbols,
        int hours = 24 * 30);

    // Market cap data
    bool ingestMarketCapData(int topCoins = 300);

    // Sentiment data ingestion
    bool ingestSentimentData(
        const std::vector<std::string>& symbols,
        const std::string& startDate,
        const std::string& endDate);

    bool ingestNewsSources();

    // Comprehensive data ingestion (full pipeline)
    bool ingestAllData(
        int topCoins = 300,
        int historicalDays = 1000,
        bool includeSentiment = true);

    // Data processing and cleanup
    bool processAndCleanData();
    bool computeDerivedMetrics();
    bool mergeSentimentWithMarketData();

    // Status and monitoring
    bool isIngestionInProgress() const { return m_isIngesting; }
    std::string getCurrentStatus() const { return m_currentStatus; }
    double getProgressPercentage() const { return m_progressPercentage; }
    std::string getLastError() const { return m_lastError; }

    // Configuration
    void setRateLimitDelay(int delayMs) { m_rateLimitDelayMs = delayMs; }
    void setBatchSize(int batchSize) { m_batchSize = batchSize; }
    void setRetryPolicy(int maxRetries, int initialBackoffMs);

    // Validation
    bool validateProviders() const;
    bool validateRepositories() const;
    std::vector<std::string> getValidationErrors() const;

private:
    // Providers
    std::unique_ptr<IMarketDataProvider> m_marketDataProvider;
    std::unique_ptr<ISentimentDataProvider> m_sentimentDataProvider;

    // Repositories
    std::shared_ptr<Database::Repositories::MarketDataRepository> m_marketDataRepository;
    std::shared_ptr<Database::Repositories::SentimentRepository> m_sentimentRepository;

    // Callbacks
    ProgressCallback m_progressCallback;
    CompletionCallback m_completionCallback;

    // Status tracking
    bool m_isIngesting;
    std::string m_currentStatus;
    double m_progressPercentage;
    std::string m_lastError;

    // Configuration
    int m_rateLimitDelayMs;
    int m_batchSize;
    int m_maxRetries;
    int m_initialBackoffMs;

    // Helper methods
    void updateProgress(int current, int total, const std::string& status);
    void setError(const std::string& error);
    void clearError() { m_lastError.clear(); }

    bool ingestMarketDataBatch(
        const std::vector<std::string>& symbols,
        const std::function<std::vector<Database::Models::MarketData>(const std::string&)>& fetchFunction);

    bool ingestSentimentDataBatch(
        const std::vector<std::string>& symbols,
        const std::string& startDate,
        const std::string& endDate);

    // Data validation and cleanup
    bool validateMarketData(const std::vector<Database::Models::MarketData>& data) const;
    bool validateSentimentData(const std::vector<Database::Models::SentimentData>& data) const;

    void removeInvalidData();
    void removeStablecoins();
    void computeNetInflow();
    void computeExcessInflow();
    void computeLiquidityLambda();

    // Retry mechanisms
    template<typename T>
    bool retryOperation(const std::function<T()>& operation, T& result);

    void handleProviderError(const std::string& providerName, const std::string& operation);
    void waitWithBackoff(int attempt);

    // Logging and monitoring
    void logIngestionStart(const std::string& operation);
    void logIngestionComplete(const std::string& operation, bool success);
    void logError(const std::string& error);
    void logProgress(const std::string& status);
};

// Utility class for managing ingestion jobs
class IngestionJob {
public:
    enum class JobType {
        MarketDataDaily,
        MarketDataHourly,
        MarketCap,
        Sentiment,
        NewsSources,
        FullPipeline
    };

    enum class JobStatus {
        Pending,
        Running,
        Completed,
        Failed,
        Cancelled
    };

    IngestionJob(JobType type, const std::string& name);

    JobType getType() const { return m_type; }
    std::string getName() const { return m_name; }
    JobStatus getStatus() const { return m_status; }
    double getProgress() const { return m_progress; }
    std::string getStatusMessage() const { return m_statusMessage; }
    std::chrono::system_clock::time_point getStartTime() const { return m_startTime; }
    std::chrono::system_clock::time_point getEndTime() const { return m_endTime; }
    std::string getError() const { return m_error; }

    void setStatus(JobStatus status);
    void setProgress(double progress);
    void setStatusMessage(const std::string& message);
    void setError(const std::string& error);

private:
    JobType m_type;
    std::string m_name;
    JobStatus m_status;
    double m_progress;
    std::string m_statusMessage;
    std::chrono::system_clock::time_point m_startTime;
    std::chrono::system_clock::time_point m_endTime;
    std::string m_error;
};

} // namespace DataIngestion
} // namespace CryptoClaude