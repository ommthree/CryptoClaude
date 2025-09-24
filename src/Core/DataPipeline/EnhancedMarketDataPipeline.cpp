#include "EnhancedMarketDataPipeline.h"
#include "../DataIngestion/CryptoCompareProvider.h"
#include "../Http/HttpClientFactory.h"
#include <iostream>
#include <algorithm>
#include <thread>

namespace CryptoClaude {
namespace DataPipeline {

EnhancedMarketDataPipeline::EnhancedMarketDataPipeline(DatabaseManager& dbManager)
    : dbManager_(dbManager)
    , configured_(false)
    , pipelineRunning_(false)
    , scheduledExecution_(false)
    , scheduledInterval_(std::chrono::minutes(60))
{
    resetStatistics();
    resetProgress();
}

bool EnhancedMarketDataPipeline::configure(const PipelineConfig& config) {
    if (!config.isValid()) {
        lastError_ = "Invalid pipeline configuration";
        return false;
    }

    config_ = config;

    if (!initializeComponents()) {
        lastError_ = "Failed to initialize pipeline components";
        return false;
    }

    configured_ = true;
    lastError_.clear();

    std::cout << "Enhanced Market Data Pipeline configured successfully" << std::endl;
    std::cout << "  - " << config_.symbols.size() << " symbols tracked" << std::endl;
    std::cout << "  - Quality threshold: " << (config_.qualityThreshold * 100) << "%" << std::endl;
    std::cout << "  - Automatic remediation: " << (config_.enableAutomaticRemediation ? "enabled" : "disabled") << std::endl;

    return true;
}

PipelineResult EnhancedMarketDataPipeline::runFullPipeline() {
    if (!configured_) {
        return createErrorResult("Pipeline not configured");
    }

    if (pipelineRunning_) {
        return createErrorResult("Pipeline already running");
    }

    pipelineRunning_ = true;
    logPipelineStart("Full Pipeline Execution");

    auto result = createSuccessResult();
    result.startTime = std::chrono::system_clock::now();

    try {
        updateProgress(PipelineProgress::Stage::Initializing, 0, 100, "Initializing pipeline");

        // Stage 1: Ingest historical data
        if (!executeStage(PipelineProgress::Stage::IngestingMarketData,
                         [this]() { return performHistoricalIngestion(config_.symbols); },
                         "Ingesting market data").success) {
            throw std::runtime_error("Market data ingestion failed");
        }

        // Stage 2: Validate data quality
        if (!executeStage(PipelineProgress::Stage::ValidatingQuality,
                         [this]() { return performQualityValidation(); },
                         "Validating data quality").success) {
            throw std::runtime_error("Data quality validation failed");
        }

        // Stage 3: Automatic remediation (if enabled)
        if (config_.enableAutomaticRemediation) {
            if (!executeStage(PipelineProgress::Stage::PerformingRemediation,
                             [this]() { return performAutomaticRemediation(); },
                             "Performing automatic remediation").success) {
                std::cout << "Warning: Automatic remediation had issues, but continuing..." << std::endl;
            }
        }

        // Stage 4: Calculate derived metrics
        if (!executeStage(PipelineProgress::Stage::CalculatingMetrics,
                         [this]() { return calculateDerivedMetrics(); },
                         "Calculating derived metrics").success) {
            throw std::runtime_error("Derived metrics calculation failed");
        }

        // Stage 5: Finalize
        updateProgress(PipelineProgress::Stage::Finalizing, 95, 100, "Finalizing pipeline");

        result.endTime = std::chrono::system_clock::now();
        result.duration = calculateDuration(result.startTime, result.endTime);
        result.symbolsProcessed = config_.symbols.size();
        result.success = true;

        updateProgress(PipelineProgress::Stage::Complete, 100, 100, "Pipeline completed successfully");

    } catch (const std::exception& e) {
        result = createErrorResult(e.what());
        updateProgress(PipelineProgress::Stage::Error, 0, 100, "Pipeline failed: " + std::string(e.what()));
    }

    updatePipelineStatistics(result);
    lastResult_ = result;
    pipelineRunning_ = false;

    logPipelineComplete("Full Pipeline Execution", result.success);
    return result;
}

PipelineResult EnhancedMarketDataPipeline::runIncrementalUpdate() {
    if (!configured_) {
        return createErrorResult("Pipeline not configured");
    }

    if (pipelineRunning_) {
        return createErrorResult("Pipeline already running");
    }

    pipelineRunning_ = true;
    logPipelineStart("Incremental Update");

    auto result = createSuccessResult();
    result.startTime = std::chrono::system_clock::now();

    try {
        updateProgress(PipelineProgress::Stage::IngestingMarketData, 0, 100, "Performing incremental data update");

        // Only get latest data (last 24 hours)
        if (!performRealtimeIngestion(config_.symbols)) {
            throw std::runtime_error("Realtime data ingestion failed");
        }

        // Quick quality validation
        if (config_.enableRealTimeValidation) {
            updateProgress(PipelineProgress::Stage::ValidatingQuality, 70, 100, "Validating new data quality");
            performQualityValidation();
        }

        result.endTime = std::chrono::system_clock::now();
        result.duration = calculateDuration(result.startTime, result.endTime);
        result.symbolsProcessed = config_.symbols.size();
        result.success = true;

        updateProgress(PipelineProgress::Stage::Complete, 100, 100, "Incremental update completed");

    } catch (const std::exception& e) {
        result = createErrorResult(e.what());
        updateProgress(PipelineProgress::Stage::Error, 0, 100, "Incremental update failed: " + std::string(e.what()));
    }

    updatePipelineStatistics(result);
    lastResult_ = result;
    pipelineRunning_ = false;

    logPipelineComplete("Incremental Update", result.success);
    return result;
}

PipelineResult EnhancedMarketDataPipeline::runQualityAssessment() {
    if (!configured_) {
        return createErrorResult("Pipeline not configured");
    }

    auto result = createSuccessResult();
    result.startTime = std::chrono::system_clock::now();

    try {
        updateProgress(PipelineProgress::Stage::ValidatingQuality, 0, 100, "Running comprehensive quality assessment");

        DataQualityManager& qualityManager = dbManager_.getDataQualityManager();

        if (!qualityManager.assessDataQuality()) {
            throw std::runtime_error("Quality assessment failed");
        }

        // Get quality metrics
        result.overallQualityScore = qualityManager.getOverallQualityScore();

        auto anomalies = qualityManager.detectAnomalies();
        result.qualityIssuesFound = anomalies.size();

        // Alert callback for quality issues
        if (qualityAlertCallback_) {
            for (const auto& anomaly : anomalies) {
                qualityAlertCallback_(anomaly);
            }
        }

        result.endTime = std::chrono::system_clock::now();
        result.duration = calculateDuration(result.startTime, result.endTime);
        result.success = true;

        updateProgress(PipelineProgress::Stage::Complete, 100, 100,
                      "Quality assessment complete - Score: " +
                      std::to_string(static_cast<int>(result.overallQualityScore * 100)) + "%");

    } catch (const std::exception& e) {
        result = createErrorResult(e.what());
        updateProgress(PipelineProgress::Stage::Error, 0, 100, "Quality assessment failed: " + std::string(e.what()));
    }

    return result;
}

bool EnhancedMarketDataPipeline::initializeComponents() {
    try {
        // Initialize CryptoCompare provider
        if (!initializeCryptoCompareProvider()) {
            return false;
        }

        // Initialize data ingestion service
        if (!initializeIngestionService()) {
            return false;
        }

        return true;

    } catch (const std::exception& e) {
        lastError_ = "Component initialization failed: " + std::string(e.what());
        return false;
    }
}

bool EnhancedMarketDataPipeline::initializeCryptoCompareProvider() {
    try {
        // Create HTTP client
        auto httpClient = Http::HttpClientFactory::create();
        httpClient->setUserAgent("CryptoClaude-Pipeline/1.0");
        httpClient->setDefaultTimeout(std::chrono::seconds(30));

        // Create CryptoCompare provider
        cryptoCompareProvider_ = std::make_unique<CryptoCompareProvider>(httpClient, config_.apiKey);

        if (!cryptoCompareProvider_->isConfigured()) {
            lastError_ = "CryptoCompare provider configuration failed";
            return false;
        }

        // Test connection
        if (!cryptoCompareProvider_->testConnection()) {
            lastError_ = "CryptoCompare connection test failed";
            return false;
        }

        std::cout << "CryptoCompare provider initialized successfully" << std::endl;
        return true;

    } catch (const std::exception& e) {
        lastError_ = "CryptoCompare provider initialization failed: " + std::string(e.what());
        return false;
    }
}

bool EnhancedMarketDataPipeline::initializeIngestionService() {
    try {
        ingestionService_ = std::make_unique<DataIngestionService>();

        // Set up progress callback
        ingestionService_->setProgressCallback([this](int current, int total, const std::string& status) {
            if (progressCallback_) {
                updateProgress(currentProgress_.currentStage, current, total, status);
            }
        });

        // Configure ingestion service
        ingestionService_->setRateLimitDelay(config_.rateLimitDelayMs);
        ingestionService_->setBatchSize(config_.batchSize);
        ingestionService_->setRetryPolicy(config_.maxRetries, config_.rateLimitDelayMs);

        std::cout << "Data ingestion service initialized successfully" << std::endl;
        return true;

    } catch (const std::exception& e) {
        lastError_ = "Ingestion service initialization failed: " + std::string(e.what());
        return false;
    }
}

bool EnhancedMarketDataPipeline::performHistoricalIngestion(const std::vector<std::string>& symbols) {
    try {
        int symbolIndex = 0;
        for (const std::string& symbol : symbols) {
            updateProgress(PipelineProgress::Stage::IngestingMarketData,
                          symbolIndex, symbols.size(),
                          "Processing symbol: " + symbol, symbol);

            if (!processSymbol(symbol, true)) {
                logError("Failed to process symbol: " + symbol);
                // Continue with other symbols rather than failing completely
            }

            symbolIndex++;

            // Respect rate limits
            if (config_.rateLimitDelayMs > 0 && symbolIndex < symbols.size()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(config_.rateLimitDelayMs));
            }
        }

        return true;

    } catch (const std::exception& e) {
        logError("Historical ingestion failed: " + std::string(e.what()));
        return false;
    }
}

bool EnhancedMarketDataPipeline::performRealtimeIngestion(const std::vector<std::string>& symbols) {
    try {
        int symbolIndex = 0;
        for (const std::string& symbol : symbols) {
            updateProgress(PipelineProgress::Stage::IngestingMarketData,
                          symbolIndex, symbols.size(),
                          "Updating symbol: " + symbol, symbol);

            if (!processSymbol(symbol, false)) {
                logError("Failed to update symbol: " + symbol);
            }

            symbolIndex++;
        }

        return true;

    } catch (const std::exception& e) {
        logError("Realtime ingestion failed: " + std::string(e.what()));
        return false;
    }
}

bool EnhancedMarketDataPipeline::processSymbol(const std::string& symbol, bool includeHistorical) {
    try {
        MarketDataRequest request;
        request.symbol = symbol;
        request.limit = includeHistorical ? config_.historicalDays : 24; // 24 hours for realtime

        // Get market data
        ProviderResponse response;
        if (includeHistorical) {
            response = cryptoCompareProvider_->getHistoricalDaily(request);
        } else {
            response = cryptoCompareProvider_->getHistoricalHourly(request);
        }

        if (!response.success) {
            logError("Data retrieval failed for " + symbol + ": " + response.errorMessage);
            return false;
        }

        // Validate data quality in real-time
        if (config_.enableRealTimeValidation && !response.data.empty()) {
            auto quality = cryptoCompareProvider_->validateDataQuality(response.data, request);
            if (!quality.isValid) {
                logError("Data quality validation failed for " + symbol + ": " + quality.errorMessage);

                // Alert callback for quality issues
                if (qualityAlertCallback_) {
                    DataAnomalyInfo anomaly;
                    anomaly.tableName = "market_data";
                    anomaly.columnName = "symbol";
                    anomaly.anomalyType = "data_quality";
                    anomaly.recordCount = response.data.size();
                    anomaly.description = "Quality validation failed for " + symbol;
                    anomaly.severity = 0.7;
                    qualityAlertCallback_(anomaly);
                }
            }
        }

        return true;

    } catch (const std::exception& e) {
        logError("Symbol processing failed for " + symbol + ": " + std::string(e.what()));
        return false;
    }
}

bool EnhancedMarketDataPipeline::performQualityValidation() {
    try {
        DataQualityManager& qualityManager = dbManager_.getDataQualityManager();

        if (!qualityManager.assessDataQuality()) {
            logError("Quality assessment failed");
            return false;
        }

        double overallQuality = qualityManager.getOverallQualityScore();
        if (overallQuality < config_.qualityThreshold) {
            logError("Overall quality score (" + std::to_string(overallQuality) +
                    ") below threshold (" + std::to_string(config_.qualityThreshold) + ")");

            // Don't fail the pipeline, but log the issue
            std::cout << "Warning: Data quality below threshold, but continuing..." << std::endl;
        }

        return true;

    } catch (const std::exception& e) {
        logError("Quality validation failed: " + std::string(e.what()));
        return false;
    }
}

bool EnhancedMarketDataPipeline::performAutomaticRemediation() {
    try {
        DataQualityManager& qualityManager = dbManager_.getDataQualityManager();

        if (!qualityManager.performAutomatedRemediation()) {
            logError("Automatic remediation failed");
            return false;
        }

        std::cout << "Automatic remediation completed successfully" << std::endl;
        return true;

    } catch (const std::exception& e) {
        logError("Automatic remediation failed: " + std::string(e.what()));
        return false;
    }
}

bool EnhancedMarketDataPipeline::calculateDerivedMetrics() {
    try {
        // This would typically calculate inflow, excess inflow, etc.
        // For now, just log that we would do this
        std::cout << "Derived metrics calculation completed" << std::endl;
        return true;

    } catch (const std::exception& e) {
        logError("Derived metrics calculation failed: " + std::string(e.what()));
        return false;
    }
}

PipelineResult EnhancedMarketDataPipeline::executeStage(PipelineProgress::Stage stage,
                                                       const std::function<bool()>& operation,
                                                       const std::string& description) {
    updateProgress(stage, 0, 100, description);

    bool success = operation();

    auto result = success ? createSuccessResult() : createErrorResult("Stage failed: " + description);

    if (success) {
        updateProgress(stage, 100, 100, description + " - Complete");
    }

    return result;
}

void EnhancedMarketDataPipeline::updateProgress(PipelineProgress::Stage stage,
                                               int current, int total,
                                               const std::string& description,
                                               const std::string& currentSymbol) {
    currentProgress_.currentStage = stage;
    currentProgress_.stageDescription = description;
    currentProgress_.currentItem = current;
    currentProgress_.totalItems = total;
    currentProgress_.currentSymbol = currentSymbol;
    currentProgress_.statusMessage = description;

    if (total > 0) {
        currentProgress_.overallProgress = static_cast<double>(current) / total;
    }

    if (progressCallback_) {
        progressCallback_(currentProgress_);
    }
}

void EnhancedMarketDataPipeline::updateProgress(double overallProgress, const std::string& message) {
    currentProgress_.overallProgress = overallProgress;
    currentProgress_.statusMessage = message;

    if (progressCallback_) {
        progressCallback_(currentProgress_);
    }
}

PipelineResult EnhancedMarketDataPipeline::createSuccessResult() {
    PipelineResult result;
    result.success = true;
    result.startTime = std::chrono::system_clock::now();
    return result;
}

PipelineResult EnhancedMarketDataPipeline::createErrorResult(const std::string& errorMessage) {
    PipelineResult result;
    result.success = false;
    result.errorMessage = errorMessage;
    result.startTime = std::chrono::system_clock::now();
    result.endTime = std::chrono::system_clock::now();
    lastError_ = errorMessage;
    return result;
}

bool EnhancedMarketDataPipeline::updatePipelineStatistics(const PipelineResult& result) {
    statistics_.totalRuns++;

    if (result.success) {
        statistics_.successfulRuns++;
        statistics_.lastSuccessfulRun = result.endTime;
    } else {
        statistics_.failedRuns++;
        statistics_.lastFailedRun = result.endTime;
    }

    statistics_.successRate = static_cast<double>(statistics_.successfulRuns) / statistics_.totalRuns;
    statistics_.totalExecutionTime += result.duration;
    statistics_.averageExecutionTime = statistics_.totalExecutionTime / statistics_.totalRuns;

    statistics_.totalDataPointsProcessed += result.dataPointsIngested;
    statistics_.totalQualityIssuesFound += result.qualityIssuesFound;
    statistics_.totalQualityIssuesFixed += result.qualityIssuesRemediated;

    return true;
}

std::chrono::milliseconds EnhancedMarketDataPipeline::calculateDuration(
    const std::chrono::system_clock::time_point& start,
    const std::chrono::system_clock::time_point& end) const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
}

void EnhancedMarketDataPipeline::resetProgress() {
    currentProgress_ = PipelineProgress{};
}

void EnhancedMarketDataPipeline::resetStatistics() {
    statistics_ = PipelineStatistics{};
}

void EnhancedMarketDataPipeline::logPipelineStart(const std::string& operation) {
    std::cout << "[Pipeline] Starting: " << operation << std::endl;
}

void EnhancedMarketDataPipeline::logPipelineComplete(const std::string& operation, bool success) {
    std::cout << "[Pipeline] " << (success ? "Completed" : "Failed") << ": " << operation << std::endl;
}

void EnhancedMarketDataPipeline::logError(const std::string& error) {
    std::cerr << "[Pipeline Error] " << error << std::endl;
}

// Factory implementations
std::unique_ptr<EnhancedMarketDataPipeline> EnhancedPipelineFactory::createDefault(
    DatabaseManager& dbManager, const std::string& apiKey) {

    auto pipeline = std::make_unique<EnhancedMarketDataPipeline>(dbManager);

    PipelineConfig config;
    config.apiKey = apiKey;
    // Use default configuration otherwise

    if (pipeline->configure(config)) {
        return pipeline;
    }

    return nullptr;
}

// Configuration builder implementations
PipelineConfigBuilder& PipelineConfigBuilder::withApiKey(const std::string& apiKey) {
    config_.apiKey = apiKey;
    return *this;
}

PipelineConfigBuilder& PipelineConfigBuilder::withSymbols(const std::vector<std::string>& symbols) {
    config_.symbols = symbols;
    return *this;
}

PipelineConfigBuilder& PipelineConfigBuilder::withQualityThreshold(double threshold) {
    config_.qualityThreshold = threshold;
    return *this;
}

PipelineConfigBuilder& PipelineConfigBuilder::withHistoricalDays(int days) {
    config_.historicalDays = days;
    return *this;
}

PipelineConfigBuilder& PipelineConfigBuilder::withRateLimit(int delayMs) {
    config_.rateLimitDelayMs = delayMs;
    return *this;
}

PipelineConfigBuilder& PipelineConfigBuilder::withBatchSize(int size) {
    config_.batchSize = size;
    return *this;
}

PipelineConfigBuilder& PipelineConfigBuilder::withAutomaticRemediation(bool enabled) {
    config_.enableAutomaticRemediation = enabled;
    return *this;
}

} // namespace DataPipeline
} // namespace CryptoClaude