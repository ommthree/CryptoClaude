#include "RealDataPipeline.h"
#include "../Utils/Logger.h"
#include <algorithm>
#include <thread>
#include <future>
#include <chrono>
#include <numeric>

namespace CryptoClaude {
namespace Data {

RealDataPipeline::RealDataPipeline(ProductionApiManager& apiManager,
                                  Database::DatabaseManager& dbManager,
                                  Database::DataQualityManager& qualityManager)
    : apiManager_(apiManager),
      dbManager_(dbManager),
      qualityManager_(qualityManager),
      initialized_(false),
      collectionActive_(false),
      shutdownRequested_(false) {

    // Initialize default configuration
    config_.primarySymbols = {"BTC", "ETH", "ADA", "DOT", "SOL"};
    config_.preferredProviders = {"cryptocompare", "newsapi"};
    config_.marketDataInterval = std::chrono::minutes(15);
    config_.sentimentDataInterval = std::chrono::hours(4);
    config_.newsUpdateInterval = std::chrono::hours(2);
    config_.minDataQualityScore = 0.90;
    config_.maxDataAge = std::chrono::minutes(30);
    config_.maxConsecutiveFailures = 5;
    config_.maxConcurrentRequests = 10;
    config_.batchSize = 20;
    config_.processingTimeout = std::chrono::milliseconds(60000);
    config_.enableRealTimeStorage = true;
    config_.enableDataBackup = true;
    config_.dataRetentionDays = 365;

    lastQualityCheck_ = std::chrono::system_clock::now();
}

RealDataPipeline::~RealDataPipeline() {
    shutdown();
}

// === INITIALIZATION AND LIFECYCLE ===

bool RealDataPipeline::initialize() {
    if (initialized_) {
        return true;
    }

    try {
        // Initialize quality validator
        if (!initializeQualityValidator()) {
            Utils::Logger::error("Failed to initialize quality validator");
            return false;
        }

        // Initialize performance tracker
        if (!initializePerformanceTracker()) {
            Utils::Logger::error("Failed to initialize performance tracker");
            return false;
        }

        // Create necessary database tables
        if (!createDatabaseTables()) {
            Utils::Logger::error("Failed to create database tables");
            return false;
        }

        // Initialize metrics for primary symbols
        {
            std::lock_guard<std::mutex> lock(metricsMutex_);
            for (const auto& symbol : config_.primarySymbols) {
                PipelineMetrics metrics;
                metrics.symbol = symbol;
                metrics.lastUpdate = std::chrono::system_clock::now() - std::chrono::hours(24);
                metrics.lastSuccess = std::chrono::system_clock::now() - std::chrono::hours(24);
                metrics.totalCollectionAttempts = 0;
                metrics.successfulCollections = 0;
                metrics.failedCollections = 0;
                metrics.successRate = 0.0;
                metrics.averageQualityScore = 0.0;
                metrics.currentQualityScore = 0.0;
                metrics.consecutiveFailures = 0;
                metrics.averageLatency = std::chrono::milliseconds(0);
                metrics.lastLatency = std::chrono::milliseconds(0);
                metrics.dataPointsCollected = 0;
                metrics.dataPointsStored = 0;
                metrics.totalDataSize = 0;

                symbolMetrics_[symbol] = metrics;
            }
        }

        initialized_ = true;
        Utils::Logger::info("RealDataPipeline initialized successfully");
        return true;

    } catch (const std::exception& e) {
        Utils::Logger::error("Exception during RealDataPipeline initialization: " + std::string(e.what()));
        return false;
    }
}

void RealDataPipeline::shutdown() {
    if (!initialized_) {
        return;
    }

    shutdownRequested_ = true;

    // Stop real-time collection
    stopRealTimeCollection();

    // Wait for threads to finish
    if (collectionThread_ && collectionThread_->joinable()) {
        collectionThread_->join();
    }
    if (validationThread_ && validationThread_->joinable()) {
        validationThread_->join();
    }
    if (monitoringThread_ && monitoringThread_->joinable()) {
        monitoringThread_->join();
    }

    // Clear queues and caches
    clearDataRequestQueue();
    clearDataCache();

    initialized_ = false;
    Utils::Logger::info("RealDataPipeline shut down successfully");
}

void RealDataPipeline::setPipelineConfig(const PipelineConfig& config) {
    std::lock_guard<std::mutex> lock(pipelineMutex_);
    config_ = config;
    Utils::Logger::info("Pipeline configuration updated");
}

// === REAL-TIME DATA COLLECTION ===

bool RealDataPipeline::startRealTimeCollection() {
    if (!initialized_) {
        recordError(ErrorSeverity::ERROR, "Pipeline", "", "Cannot start collection: pipeline not initialized");
        return false;
    }

    if (collectionActive_) {
        Utils::Logger::warning("Real-time collection already active");
        return true;
    }

    try {
        shutdownRequested_ = false;
        collectionActive_ = true;

        // Start collection threads
        collectionThread_ = std::make_unique<std::thread>(&RealDataPipeline::collectionThreadMain, this);
        validationThread_ = std::make_unique<std::thread>(&RealDataPipeline::validationThreadMain, this);
        monitoringThread_ = std::make_unique<std::thread>(&RealDataPipeline::monitoringThreadMain, this);

        Utils::Logger::info("Real-time data collection started");
        return true;

    } catch (const std::exception& e) {
        collectionActive_ = false;
        recordError(ErrorSeverity::CRITICAL, "Pipeline", "",
                   "Failed to start real-time collection: " + std::string(e.what()));
        return false;
    }
}

bool RealDataPipeline::stopRealTimeCollection() {
    if (!collectionActive_) {
        return true;
    }

    collectionActive_ = false;
    queueCondition_.notify_all(); // Wake up waiting threads

    Utils::Logger::info("Real-time data collection stopped");
    return true;
}

bool RealDataPipeline::collectMarketDataNow(const std::vector<std::string>& symbols) {
    if (!initialized_) {
        return false;
    }

    auto symbolsToCollect = symbols.empty() ? config_.primarySymbols : symbols;
    int successCount = 0;

    for (const auto& symbol : symbolsToCollect) {
        if (collectMarketDataForSymbol(symbol)) {
            successCount++;
        }
    }

    bool success = successCount > 0;
    Utils::Logger::info("Manual market data collection: " + std::to_string(successCount) + "/" +
                       std::to_string(symbolsToCollect.size()) + " symbols successful");

    return success;
}

bool RealDataPipeline::collectSentimentDataNow(const std::vector<std::string>& symbols) {
    if (!initialized_) {
        return false;
    }

    auto symbolsToCollect = symbols.empty() ? config_.primarySymbols : symbols;
    int successCount = 0;

    for (const auto& symbol : symbolsToCollect) {
        if (collectSentimentDataForSymbol(symbol)) {
            successCount++;
        }
    }

    bool success = successCount > 0;
    Utils::Logger::info("Manual sentiment data collection: " + std::to_string(successCount) + "/" +
                       std::to_string(symbolsToCollect.size()) + " symbols successful");

    return success;
}

bool RealDataPipeline::collectNewsDataNow() {
    if (!initialized_) {
        return false;
    }

    return collectNewsData();
}

// === DATA VALIDATION AND QUALITY CONTROL ===

RealDataPipeline::DataValidationResult RealDataPipeline::validateMarketData(
    const std::vector<Database::Models::MarketData>& data) {

    return performMarketDataValidation(data);
}

RealDataPipeline::DataValidationResult RealDataPipeline::validateSentimentData(
    const std::vector<Database::Models::SentimentData>& data) {

    return performSentimentDataValidation(data);
}

double RealDataPipeline::getCurrentDataQualityScore() const {
    std::lock_guard<std::mutex> lock(metricsMutex_);

    if (symbolMetrics_.empty()) {
        return 0.0;
    }

    double totalScore = 0.0;
    int validSymbols = 0;

    for (const auto& pair : symbolMetrics_) {
        if (pair.second.currentQualityScore > 0.0) {
            totalScore += pair.second.currentQualityScore;
            validSymbols++;
        }
    }

    return validSymbols > 0 ? totalScore / validSymbols : 0.0;
}

std::map<std::string, double> RealDataPipeline::getProviderQualityScores() const {
    std::lock_guard<std::mutex> lock(pipelineMutex_);
    return providerQualityScores_;
}

// === PIPELINE MONITORING AND METRICS ===

RealDataPipeline::PipelineMetrics RealDataPipeline::getPipelineMetrics(const std::string& symbol) const {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    auto it = symbolMetrics_.find(symbol);
    if (it != symbolMetrics_.end()) {
        return it->second;
    }

    // Return empty metrics if symbol not found
    PipelineMetrics metrics;
    metrics.symbol = symbol;
    return metrics;
}

std::map<std::string, RealDataPipeline::PipelineMetrics> RealDataPipeline::getAllPipelineMetrics() const {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    return symbolMetrics_;
}

RealDataPipeline::SystemHealth RealDataPipeline::getSystemHealth() {
    SystemHealth health;
    health.lastHealthCheck = std::chrono::system_clock::now();
    health.overallHealth = true;

    // Check API manager health
    auto apiDiagnostics = apiManager_.performSystemDiagnostics();
    health.activeConnections = apiDiagnostics.totalProviders;
    health.healthyProviders = apiDiagnostics.healthyProviders;

    // Check symbol health
    health.activeSymbols = 0;
    double totalQualityScore = 0.0;
    int validSymbols = 0;

    {
        std::lock_guard<std::mutex> lock(metricsMutex_);
        for (const auto& pair : symbolMetrics_) {
            if (pair.second.consecutiveFailures < config_.maxConsecutiveFailures) {
                health.activeSymbols++;
            }
            if (pair.second.currentQualityScore > 0.0) {
                totalQualityScore += pair.second.currentQualityScore;
                validSymbols++;
            }
        }
    }

    health.systemQualityScore = validSymbols > 0 ? totalQualityScore / validSymbols : 0.0;

    // Overall health assessment
    if (health.healthyProviders == 0) {
        health.overallHealth = false;
        health.criticalErrors.push_back("No healthy API providers available");
    }

    if (health.systemQualityScore < config_.minDataQualityScore) {
        health.overallHealth = false;
        health.activeWarnings.push_back("System quality score below threshold");
    }

    if (health.activeSymbols < config_.primarySymbols.size() / 2) {
        health.overallHealth = false;
        health.activeWarnings.push_back("Less than 50% of symbols are active");
    }

    return health;
}

void RealDataPipeline::performHealthCheck() {
    auto health = getSystemHealth();

    if (!health.overallHealth) {
        recordError(ErrorSeverity::WARNING, "HealthCheck", "",
                   "System health check failed. Quality: " + std::to_string(health.systemQualityScore));

        // Attempt automatic recovery
        attemptAutomaticRecovery();
    }

    Utils::Logger::debug("Health check completed. Overall health: " +
                        std::string(health.overallHealth ? "GOOD" : "POOR") +
                        ", Quality: " + std::to_string(health.systemQualityScore));
}

// === PRIVATE IMPLEMENTATION METHODS ===

bool RealDataPipeline::initializeQualityValidator() {
    try {
        qualityValidator_ = std::make_unique<Quality::LiveDataQualityValidator>(dbManager_);
        return qualityValidator_->initialize();
    } catch (const std::exception& e) {
        Utils::Logger::error("Failed to initialize quality validator: " + std::string(e.what()));
        return false;
    }
}

bool RealDataPipeline::initializePerformanceTracker() {
    try {
        performanceTracker_ = std::make_unique<Analytics::PerformanceTracker>(dbManager_);
        return performanceTracker_->initialize();
    } catch (const std::exception& e) {
        Utils::Logger::error("Failed to initialize performance tracker: " + std::string(e.what()));
        return false;
    }
}

bool RealDataPipeline::createDatabaseTables() {
    // The database tables should already exist from the DatabaseManager initialization
    // This is a placeholder for any additional pipeline-specific tables
    return true;
}

void RealDataPipeline::collectionThreadMain() {
    Utils::Logger::info("Collection thread started");

    auto lastMarketDataCollection = std::chrono::system_clock::now() - config_.marketDataInterval;
    auto lastSentimentDataCollection = std::chrono::system_clock::now() - config_.sentimentDataInterval;
    auto lastNewsCollection = std::chrono::system_clock::now() - config_.newsUpdateInterval;

    while (collectionActive_ && !shutdownRequested_) {
        try {
            auto now = std::chrono::system_clock::now();

            // Check if it's time to collect market data
            if (now - lastMarketDataCollection >= config_.marketDataInterval) {
                for (const auto& symbol : config_.primarySymbols) {
                    addDataRequest(symbol, "market", 1); // High priority
                }
                lastMarketDataCollection = now;
            }

            // Check if it's time to collect sentiment data
            if (now - lastSentimentDataCollection >= config_.sentimentDataInterval) {
                for (const auto& symbol : config_.primarySymbols) {
                    addDataRequest(symbol, "sentiment", 2); // Medium priority
                }
                lastSentimentDataCollection = now;
            }

            // Check if it's time to collect news data
            if (now - lastNewsCollection >= config_.newsUpdateInterval) {
                addDataRequest("ALL", "news", 3); // Lower priority
                lastNewsCollection = now;
            }

            // Process queued requests
            while (!shutdownRequested_) {
                try {
                    auto request = getNextDataRequest();
                    if (request.symbol.empty()) {
                        break; // No more requests in queue
                    }

                    if (request.dataType == "market") {
                        collectMarketDataForSymbol(request.symbol);
                    } else if (request.dataType == "sentiment") {
                        collectSentimentDataForSymbol(request.symbol);
                    } else if (request.dataType == "news") {
                        collectNewsData();
                    }

                } catch (const std::exception& e) {
                    Utils::Logger::warning("Error processing data request: " + std::string(e.what()));
                }
            }

            // Sleep for a short interval before checking again
            std::this_thread::sleep_for(std::chrono::seconds(30));

        } catch (const std::exception& e) {
            recordError(ErrorSeverity::ERROR, "CollectionThread", "",
                       "Collection thread error: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(60)); // Longer sleep on error
        }
    }

    Utils::Logger::info("Collection thread stopped");
}

void RealDataPipeline::validationThreadMain() {
    Utils::Logger::info("Validation thread started");

    while (collectionActive_ && !shutdownRequested_) {
        try {
            // Perform periodic quality checks
            for (const auto& symbol : config_.primarySymbols) {
                if (shutdownRequested_) break;

                checkSymbolHealth(symbol);
                std::this_thread::sleep_for(std::chrono::seconds(10));
            }

            // Sleep before next validation cycle
            std::this_thread::sleep_for(std::chrono::minutes(5));

        } catch (const std::exception& e) {
            recordError(ErrorSeverity::ERROR, "ValidationThread", "",
                       "Validation thread error: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(60));
        }
    }

    Utils::Logger::info("Validation thread stopped");
}

void RealDataPipeline::monitoringThreadMain() {
    Utils::Logger::info("Monitoring thread started");

    while (collectionActive_ && !shutdownRequested_) {
        try {
            // Perform health checks
            performHealthCheck();

            // Clean up old errors
            clearErrors(ErrorSeverity::WARNING);

            // Perform data cleanup if enabled
            if (config_.enableDataBackup) {
                performDataCleanup();
            }

            // Sleep before next monitoring cycle
            std::this_thread::sleep_for(std::chrono::minutes(10));

        } catch (const std::exception& e) {
            recordError(ErrorSeverity::ERROR, "MonitoringThread", "",
                       "Monitoring thread error: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(60));
        }
    }

    Utils::Logger::info("Monitoring thread stopped");
}

bool RealDataPipeline::collectMarketDataForSymbol(const std::string& symbol) {
    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        // Select best provider for market data
        std::string providerId = selectBestProvider("market");
        if (providerId.empty()) {
            recordError(ErrorSeverity::ERROR, "MarketDataCollection", symbol,
                       "No available providers for market data");
            return false;
        }

        auto marketProvider = apiManager_.getMarketDataProvider(providerId);
        if (!marketProvider) {
            recordError(ErrorSeverity::ERROR, "MarketDataCollection", symbol,
                       "Failed to get market data provider: " + providerId);
            return false;
        }

        // Collect latest data (last 24 hours)
        auto marketData = marketProvider->getDailyData(symbol, "USD", 1);

        auto endTime = std::chrono::high_resolution_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        if (marketData.empty()) {
            updatePerformanceMetrics(symbol, false, latency, 0);
            recordError(ErrorSeverity::WARNING, "MarketDataCollection", symbol,
                       "No market data received from provider: " + providerId);
            return false;
        }

        // Validate data quality
        auto validationResult = validateMarketData(marketData);
        if (!validationResult.isValid) {
            updatePerformanceMetrics(symbol, false, latency, marketData.size());
            recordError(ErrorSeverity::WARNING, "MarketDataCollection", symbol,
                       "Market data validation failed: " +
                       (validationResult.errors.empty() ? "Unknown error" : validationResult.errors[0]));
            return false;
        }

        // Store data
        if (!storeMarketData(marketData)) {
            updatePerformanceMetrics(symbol, false, latency, marketData.size());
            recordError(ErrorSeverity::ERROR, "MarketDataCollection", symbol,
                       "Failed to store market data");
            return false;
        }

        // Update metrics and quality scores
        updatePerformanceMetrics(symbol, true, latency, marketData.size());
        updateQualityMetrics(symbol, validationResult.qualityScore);

        // Notify callback
        if (dataUpdateCallback_) {
            dataUpdateCallback_(symbol, "market");
        }

        Utils::Logger::debug("Successfully collected market data for " + symbol +
                           " (" + std::to_string(marketData.size()) + " points, " +
                           std::to_string(latency.count()) + "ms)");
        return true;

    } catch (const std::exception& e) {
        auto endTime = std::chrono::high_resolution_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        updatePerformanceMetrics(symbol, false, latency, 0);
        recordError(ErrorSeverity::ERROR, "MarketDataCollection", symbol,
                   "Exception during market data collection: " + std::string(e.what()));
        return false;
    }
}

bool RealDataPipeline::collectSentimentDataForSymbol(const std::string& symbol) {
    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        // Select best provider for sentiment data
        std::string providerId = selectBestProvider("sentiment");
        if (providerId.empty()) {
            recordError(ErrorSeverity::ERROR, "SentimentDataCollection", symbol,
                       "No available providers for sentiment data");
            return false;
        }

        auto sentimentProvider = apiManager_.getNewsProvider(providerId);
        if (!sentimentProvider) {
            recordError(ErrorSeverity::ERROR, "SentimentDataCollection", symbol,
                       "Failed to get sentiment data provider: " + providerId);
            return false;
        }

        // Calculate date range for sentiment data (last 7 days)
        auto endTime = std::chrono::system_clock::now();
        auto startTime = endTime - std::chrono::hours(24 * 7);

        std::time_t startTimeT = std::chrono::system_clock::to_time_t(startTime);
        std::time_t endTimeT = std::chrono::system_clock::to_time_t(endTime);

        std::ostringstream startStream, endStream;
        startStream << std::put_time(std::gmtime(&startTimeT), "%Y-%m-%d");
        endStream << std::put_time(std::gmtime(&endTimeT), "%Y-%m-%d");

        // Collect sentiment data
        auto sentimentData = sentimentProvider->getSentimentData({symbol},
                                                               startStream.str(),
                                                               endStream.str());

        auto collectEndTime = std::chrono::high_resolution_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(collectEndTime - startTime);

        if (sentimentData.empty()) {
            updatePerformanceMetrics(symbol, false, latency, 0);
            recordError(ErrorSeverity::WARNING, "SentimentDataCollection", symbol,
                       "No sentiment data received from provider: " + providerId);
            return false;
        }

        // Validate data quality
        auto validationResult = validateSentimentData(sentimentData);
        if (!validationResult.isValid) {
            updatePerformanceMetrics(symbol, false, latency, sentimentData.size());
            recordError(ErrorSeverity::WARNING, "SentimentDataCollection", symbol,
                       "Sentiment data validation failed: " +
                       (validationResult.errors.empty() ? "Unknown error" : validationResult.errors[0]));
            return false;
        }

        // Store data
        if (!storeSentimentData(sentimentData)) {
            updatePerformanceMetrics(symbol, false, latency, sentimentData.size());
            recordError(ErrorSeverity::ERROR, "SentimentDataCollection", symbol,
                       "Failed to store sentiment data");
            return false;
        }

        // Update metrics and quality scores
        updatePerformanceMetrics(symbol, true, latency, sentimentData.size());
        updateQualityMetrics(symbol, validationResult.qualityScore);

        // Notify callback
        if (dataUpdateCallback_) {
            dataUpdateCallback_(symbol, "sentiment");
        }

        Utils::Logger::debug("Successfully collected sentiment data for " + symbol +
                           " (" + std::to_string(sentimentData.size()) + " points, " +
                           std::to_string(latency.count()) + "ms)");
        return true;

    } catch (const std::exception& e) {
        auto collectEndTime = std::chrono::high_resolution_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(collectEndTime - startTime);

        updatePerformanceMetrics(symbol, false, latency, 0);
        recordError(ErrorSeverity::ERROR, "SentimentDataCollection", symbol,
                   "Exception during sentiment data collection: " + std::string(e.what()));
        return false;
    }
}

bool RealDataPipeline::collectNewsData() {
    // Implementation for general news data collection
    // This would collect overall crypto market news rather than symbol-specific
    return true; // Placeholder
}

// Additional helper method implementations...

RealDataPipeline::DataValidationResult RealDataPipeline::performMarketDataValidation(
    const std::vector<Database::Models::MarketData>& data) {

    DataValidationResult result;
    result.validationTime = std::chrono::system_clock::now();
    result.dataPointsValidated = data.size();
    result.validDataPoints = 0;
    result.invalidDataPoints = 0;
    result.isValid = true;

    if (qualityValidator_) {
        // Use the quality validator for comprehensive validation
        for (const auto& dataPoint : data) {
            if (qualityValidator_->validateMarketDataPoint(dataPoint)) {
                result.validDataPoints++;
            } else {
                result.invalidDataPoints++;
                result.errors.push_back("Invalid data point for " + dataPoint.symbol);
            }
        }
    } else {
        // Basic validation if quality validator is not available
        for (const auto& dataPoint : data) {
            bool isValid = true;

            if (dataPoint.open <= 0 || dataPoint.high <= 0 ||
                dataPoint.low <= 0 || dataPoint.close <= 0) {
                isValid = false;
                result.errors.push_back("Invalid price data for " + dataPoint.symbol);
            }

            if (dataPoint.high < dataPoint.low) {
                isValid = false;
                result.errors.push_back("High price less than low price for " + dataPoint.symbol);
            }

            if (dataPoint.volume < 0) {
                isValid = false;
                result.errors.push_back("Negative volume for " + dataPoint.symbol);
            }

            if (isValid) {
                result.validDataPoints++;
            } else {
                result.invalidDataPoints++;
            }
        }
    }

    // Calculate quality score
    if (result.dataPointsValidated > 0) {
        result.qualityScore = static_cast<double>(result.validDataPoints) / result.dataPointsValidated;
    } else {
        result.qualityScore = 0.0;
    }

    result.isValid = result.qualityScore >= config_.minDataQualityScore;

    return result;
}

RealDataPipeline::DataValidationResult RealDataPipeline::performSentimentDataValidation(
    const std::vector<Database::Models::SentimentData>& data) {

    DataValidationResult result;
    result.validationTime = std::chrono::system_clock::now();
    result.dataPointsValidated = data.size();
    result.validDataPoints = 0;
    result.invalidDataPoints = 0;
    result.isValid = true;

    // Basic sentiment data validation
    for (const auto& dataPoint : data) {
        bool isValid = true;

        if (dataPoint.sentiment < -1.0 || dataPoint.sentiment > 1.0) {
            isValid = false;
            result.errors.push_back("Sentiment score out of range for " + dataPoint.symbol);
        }

        if (dataPoint.confidence < 0.0 || dataPoint.confidence > 1.0) {
            isValid = false;
            result.errors.push_back("Confidence score out of range for " + dataPoint.symbol);
        }

        if (dataPoint.symbol.empty() || dataPoint.source.empty()) {
            isValid = false;
            result.errors.push_back("Missing required fields for sentiment data");
        }

        if (isValid) {
            result.validDataPoints++;
        } else {
            result.invalidDataPoints++;
        }
    }

    // Calculate quality score
    if (result.dataPointsValidated > 0) {
        result.qualityScore = static_cast<double>(result.validDataPoints) / result.dataPointsValidated;
    } else {
        result.qualityScore = 0.0;
    }

    result.isValid = result.qualityScore >= config_.minDataQualityScore;

    return result;
}

// Remaining implementation methods would follow similar patterns...

void RealDataPipeline::recordError(ErrorSeverity severity, const std::string& component,
                                  const std::string& symbol, const std::string& message,
                                  const std::string& details, int errorCode) {
    PipelineError error;
    error.severity = severity;
    error.component = component;
    error.symbol = symbol;
    error.message = message;
    error.details = details;
    error.timestamp = std::chrono::system_clock::now();
    error.errorCode = errorCode;

    {
        std::lock_guard<std::mutex> lock(metricsMutex_);
        errorHistory_.push_back(error);

        // Keep only recent errors (limit memory usage)
        if (errorHistory_.size() > 1000) {
            errorHistory_.erase(errorHistory_.begin(), errorHistory_.begin() + 100);
        }
    }

    // Notify callback
    if (errorCallback_) {
        errorCallback_(error);
    }

    // Log error based on severity
    std::string logMessage = "[" + component + "] " + symbol + ": " + message;
    switch (severity) {
        case ErrorSeverity::INFO:
            Utils::Logger::info(logMessage);
            break;
        case ErrorSeverity::WARNING:
            Utils::Logger::warning(logMessage);
            break;
        case ErrorSeverity::ERROR:
            Utils::Logger::error(logMessage);
            break;
        case ErrorSeverity::CRITICAL:
            Utils::Logger::error("CRITICAL: " + logMessage);
            break;
    }
}

std::string RealDataPipeline::selectBestProvider(const std::string& dataType) {
    if (dataType == "market") {
        return apiManager_.selectBestProvider("cryptocompare");
    } else if (dataType == "sentiment" || dataType == "news") {
        return apiManager_.selectBestProvider("newsapi");
    }
    return "";
}

bool RealDataPipeline::storeMarketData(const std::vector<Database::Models::MarketData>& data) {
    // Implementation would use the database manager to store market data
    // Placeholder for now
    return true;
}

bool RealDataPipeline::storeSentimentData(const std::vector<Database::Models::SentimentData>& data) {
    // Implementation would use the database manager to store sentiment data
    // Placeholder for now
    return true;
}

void RealDataPipeline::updatePerformanceMetrics(const std::string& symbol, bool success,
                                               std::chrono::milliseconds latency, int dataPoints) {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    auto& metrics = symbolMetrics_[symbol];

    metrics.lastUpdate = std::chrono::system_clock::now();
    metrics.totalCollectionAttempts++;
    metrics.lastLatency = latency;

    if (success) {
        metrics.successfulCollections++;
        metrics.lastSuccess = std::chrono::system_clock::now();
        metrics.consecutiveFailures = 0;
        metrics.dataPointsCollected += dataPoints;
        metrics.dataPointsStored += dataPoints; // Assuming successful storage
    } else {
        metrics.failedCollections++;
        metrics.consecutiveFailures++;
    }

    // Update success rate
    if (metrics.totalCollectionAttempts > 0) {
        metrics.successRate = static_cast<double>(metrics.successfulCollections) /
                             metrics.totalCollectionAttempts;
    }

    // Update average latency
    if (metrics.successfulCollections > 0) {
        auto totalLatency = metrics.averageLatency.count() * (metrics.successfulCollections - 1) +
                           latency.count();
        metrics.averageLatency = std::chrono::milliseconds(totalLatency / metrics.successfulCollections);
    }
}

void RealDataPipeline::updateQualityMetrics(const std::string& symbol, double qualityScore) {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    auto& metrics = symbolMetrics_[symbol];

    metrics.currentQualityScore = qualityScore;

    // Update average quality score
    if (metrics.successfulCollections > 0) {
        double totalQuality = metrics.averageQualityScore * (metrics.successfulCollections - 1) + qualityScore;
        metrics.averageQualityScore = totalQuality / metrics.successfulCollections;
    } else {
        metrics.averageQualityScore = qualityScore;
    }

    // Check quality thresholds
    checkQualityThresholds(symbol);

    // Notify callback
    if (qualityCallback_) {
        qualityCallback_(symbol, qualityScore);
    }
}

void RealDataPipeline::checkQualityThresholds(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    auto& metrics = symbolMetrics_[symbol];

    if (metrics.currentQualityScore < config_.minDataQualityScore) {
        recordError(ErrorSeverity::WARNING, "QualityCheck", symbol,
                   "Quality score below threshold: " + std::to_string(metrics.currentQualityScore));
    }

    if (metrics.consecutiveFailures >= config_.maxConsecutiveFailures) {
        recordError(ErrorSeverity::ERROR, "QualityCheck", symbol,
                   "Too many consecutive failures: " + std::to_string(metrics.consecutiveFailures));
    }
}

void RealDataPipeline::addDataRequest(const std::string& symbol, const std::string& dataType, int priority) {
    std::lock_guard<std::mutex> lock(queueMutex_);

    DataRequest request;
    request.symbol = symbol;
    request.dataType = dataType;
    request.requestTime = std::chrono::system_clock::now();
    request.priority = priority;

    dataRequestQueue_.push(request);
    queueCondition_.notify_one();
}

RealDataPipeline::DataRequest RealDataPipeline::getNextDataRequest() {
    std::unique_lock<std::mutex> lock(queueMutex_);

    if (dataRequestQueue_.empty()) {
        return DataRequest{}; // Return empty request
    }

    auto request = dataRequestQueue_.front();
    dataRequestQueue_.pop();
    return request;
}

void RealDataPipeline::clearDataRequestQueue() {
    std::lock_guard<std::mutex> lock(queueMutex_);
    std::queue<DataRequest> empty;
    dataRequestQueue_.swap(empty);
}

bool RealDataPipeline::checkSymbolHealth(const std::string& symbol) {
    auto metrics = getPipelineMetrics(symbol);

    // Check if symbol is healthy based on recent activity and quality
    auto now = std::chrono::system_clock::now();
    auto timeSinceLastSuccess = now - metrics.lastSuccess;

    bool isHealthy = true;

    if (timeSinceLastSuccess > config_.maxDataAge * 2) {
        isHealthy = false;
        recordError(ErrorSeverity::WARNING, "SymbolHealth", symbol,
                   "Symbol has not been updated recently");
    }

    if (metrics.consecutiveFailures >= config_.maxConsecutiveFailures) {
        isHealthy = false;
        recordError(ErrorSeverity::ERROR, "SymbolHealth", symbol,
                   "Too many consecutive failures for symbol");
    }

    if (metrics.currentQualityScore < config_.minDataQualityScore) {
        isHealthy = false;
        recordError(ErrorSeverity::WARNING, "SymbolHealth", symbol,
                   "Quality score below threshold");
    }

    return isHealthy;
}

bool RealDataPipeline::attemptAutomaticRecovery() {
    Utils::Logger::info("Attempting automatic recovery");

    bool recoverySuccessful = false;

    // Try to restart failed symbol collections
    {
        std::lock_guard<std::mutex> lock(metricsMutex_);
        for (auto& pair : symbolMetrics_) {
            if (pair.second.consecutiveFailures >= config_.maxConsecutiveFailures) {
                Utils::Logger::info("Attempting recovery for symbol: " + pair.first);

                // Reset consecutive failures to give it another chance
                pair.second.consecutiveFailures = 0;

                // Add immediate data request
                addDataRequest(pair.first, "market", 0); // Highest priority

                recoverySuccessful = true;
            }
        }
    }

    return recoverySuccessful;
}

std::vector<RealDataPipeline::PipelineError> RealDataPipeline::getRecentErrors(int hours) const {
    std::lock_guard<std::mutex> lock(metricsMutex_);

    auto cutoffTime = std::chrono::system_clock::now() - std::chrono::hours(hours);
    std::vector<PipelineError> recentErrors;

    for (const auto& error : errorHistory_) {
        if (error.timestamp >= cutoffTime) {
            recentErrors.push_back(error);
        }
    }

    return recentErrors;
}

void RealDataPipeline::clearErrors(ErrorSeverity maxSeverity) {
    std::lock_guard<std::mutex> lock(metricsMutex_);

    errorHistory_.erase(
        std::remove_if(errorHistory_.begin(), errorHistory_.end(),
                      [maxSeverity](const PipelineError& error) {
                          return error.severity <= maxSeverity;
                      }),
        errorHistory_.end()
    );
}

void RealDataPipeline::clearDataCache() {
    // Implementation would clear any internal data caches
    // Placeholder for now
}

void RealDataPipeline::performDataCleanup() {
    // Implementation would clean up old data based on retention policies
    // Placeholder for now
}

} // namespace Data
} // namespace CryptoClaude