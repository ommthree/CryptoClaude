#include "HistoricalDataLoader.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <random>
#include <thread>
#include <chrono>
#include <regex>
#include <json/json.h>

namespace CryptoClaude {
namespace Data {

// HistoricalDataLoader Implementation
HistoricalDataLoader::HistoricalDataLoader(SmartCacheManager& cacheManager,
                                          ApiLimitHandler& apiHandler,
                                          DatabaseManager& dbManager)
    : cacheManager_(cacheManager), apiHandler_(apiHandler), dbManager_(dbManager),
      shutdownRequested_(false) {

    // Set default conservation strategy
    conservationStrategy_.skipWeekendsForDaily = false; // Crypto trades 24/7
    conservationStrategy_.batchSimilarRequests = true;
    conservationStrategy_.usePreferredProvider = true;
    conservationStrategy_.respectRateLimit = true;
    conservationStrategy_.maxUtilizationPercent = 75.0; // Conservative 75%
    conservationStrategy_.offPeakStartHour = std::chrono::hours(2); // 2 AM off-peak
}

HistoricalDataLoader::~HistoricalDataLoader() {
    shutdown();
}

bool HistoricalDataLoader::initialize() {
    std::lock_guard<std::mutex> lock(loaderMutex_);

    shutdownRequested_ = false;

    // Start worker thread
    workerThread_ = std::thread(&HistoricalDataLoader::workerThreadFunction, this);

    std::cout << "HistoricalDataLoader initialized successfully" << std::endl;
    return true;
}

void HistoricalDataLoader::shutdown() {
    {
        std::lock_guard<std::mutex> lock(loaderMutex_);
        shutdownRequested_ = true;
    }

    workerCondition_.notify_all();

    if (workerThread_.joinable()) {
        workerThread_.join();
    }

    // Cancel all active loadings
    for (auto& loading : activeLoadings_) {
        loading.second.status = "cancelled";
    }

    std::cout << "HistoricalDataLoader shutdown completed" << std::endl;
}

std::string HistoricalDataLoader::startPersonalSetupLoading(const PersonalSetupConfig& config) {
    std::lock_guard<std::mutex> lock(loaderMutex_);

    std::string loadingId = generateLoadingId();

    // Create loading progress tracker
    LoadingProgress progress;
    progress.requestId = loadingId;
    progress.symbol = "PORTFOLIO"; // Multi-symbol loading
    progress.status = "pending";
    progress.startTime = std::chrono::system_clock::now();

    // Calculate total chunks needed
    std::vector<HistoricalDataRequest> allRequests;

    for (const auto& symbol : config.symbols) {
        for (const auto& provider : config.providers) {
            for (const auto& timeframe : config.timeframes) {
                HistoricalDataRequest request;
                request.symbol = symbol;
                request.provider = provider;
                request.timeframe = timeframe;
                request.startDate = config.startDate;
                request.endDate = std::chrono::system_clock::now();
                request.priority = 1; // High priority for personal setup
                request.isPermanent = true;

                allRequests.push_back(request);
            }
        }
    }

    // Optimize request order for API conservation
    allRequests = optimizeRequestOrder(allRequests);

    // Create data chunks for progressive loading
    std::vector<DataChunk> allChunks;
    for (const auto& request : allRequests) {
        auto chunks = createDataChunks(request, 60); // 60-day chunks for conservation
        allChunks.insert(allChunks.end(), chunks.begin(), chunks.end());
    }

    progress.totalChunks = allChunks.size();
    progress.completedChunks = 0;
    progress.failedChunks = 0;
    progress.progressPercent = 0.0;

    // Estimate completion time based on rate limits and delays
    auto estimatedSeconds = allChunks.size() * config.delayBetweenRequests.count() / 1000;
    progress.estimatedCompletion = progress.startTime + std::chrono::seconds(estimatedSeconds);

    // Store loading data
    activeLoadings_[loadingId] = progress;
    loadingChunks_[loadingId] = allChunks;

    // Add to work queue
    workQueue_.push(loadingId);
    workerCondition_.notify_one();

    std::cout << "Started personal setup loading with " << allChunks.size()
              << " data chunks for " << config.symbols.size() << " symbols" << std::endl;

    return loadingId;
}

std::string HistoricalDataLoader::loadSymbolData(const std::string& symbol,
                                                const std::string& provider,
                                                const std::string& timeframe,
                                                int days) {
    HistoricalDataRequest request;
    request.symbol = symbol;
    request.provider = provider;
    request.timeframe = timeframe;
    request.endDate = std::chrono::system_clock::now();
    request.startDate = request.endDate - std::chrono::hours(24 * days);
    request.priority = 2; // Medium-high priority
    request.isPermanent = true;

    return loadHistoricalData(request);
}

std::string HistoricalDataLoader::loadHistoricalData(const HistoricalDataRequest& request) {
    std::lock_guard<std::mutex> lock(loaderMutex_);

    std::string loadingId = generateLoadingId();

    // Check if data already exists in cache
    if (cacheManager_.hasHistoricalData(request.symbol, request.provider,
                                       request.timeframe, request.startDate, request.endDate)) {
        std::cout << "Historical data already cached for " << request.symbol
                  << " (" << request.timeframe << ")" << std::endl;
        return loadingId; // Return immediately as data exists
    }

    // Create loading progress
    LoadingProgress progress;
    progress.requestId = loadingId;
    progress.symbol = request.symbol;
    progress.status = "pending";
    progress.startTime = std::chrono::system_clock::now();

    // Create chunks for progressive loading
    auto chunks = createDataChunks(request, 30); // 30-day chunks
    progress.totalChunks = chunks.size();

    activeLoadings_[loadingId] = progress;
    loadingChunks_[loadingId] = chunks;

    // Add to work queue
    workQueue_.push(loadingId);
    workerCondition_.notify_one();

    return loadingId;
}

std::vector<DataChunk> HistoricalDataLoader::createDataChunks(const HistoricalDataRequest& request,
                                                             int chunkSizeDays) const {
    std::vector<DataChunk> chunks;

    auto current = request.startDate;
    auto chunkDuration = std::chrono::hours(24 * chunkSizeDays);

    while (current < request.endDate) {
        DataChunk chunk;
        chunk.symbol = request.symbol;
        chunk.provider = request.provider;
        chunk.timeframe = request.timeframe;
        chunk.startDate = current;
        chunk.endDate = std::min(current + chunkDuration, request.endDate);
        chunk.isComplete = false;
        chunk.retryCount = 0;

        chunks.push_back(chunk);
        current += chunkDuration;
    }

    std::cout << "Created " << chunks.size() << " data chunks for "
              << request.symbol << " (" << request.timeframe << ")" << std::endl;

    return chunks;
}

ValidationResult HistoricalDataLoader::validateHistoricalData(const std::string& symbol,
                                                             const std::string& provider,
                                                             const std::string& timeframe,
                                                             const std::chrono::system_clock::time_point& startDate,
                                                             const std::chrono::system_clock::time_point& endDate) const {
    ValidationResult result;

    // Get cached data
    std::string cachedData = cacheManager_.getHistoricalData(symbol, provider, timeframe, startDate, endDate);

    if (cachedData.empty()) {
        result.isValid = false;
        result.errors.push_back("No cached data found for validation");
        return result;
    }

    // Parse and validate data format
    try {
        Json::Value jsonData;
        Json::Reader reader;

        if (!reader.parse(cachedData, jsonData)) {
            result.errors.push_back("Invalid JSON format");
            result.isValid = false;
            return result;
        }

        // Count data points and check for completeness
        if (jsonData.isArray()) {
            result.totalDataPoints = jsonData.size();

            if (result.totalDataPoints == 0) {
                result.errors.push_back("No data points found");
                result.isValid = false;
                return result;
            }

            // Calculate expected data points based on timeframe
            int expectedPoints = 0;
            auto duration = std::chrono::duration_cast<std::chrono::hours>(endDate - startDate);

            if (timeframe == "1d") {
                expectedPoints = duration.count() / 24;
            } else if (timeframe == "1h") {
                expectedPoints = duration.count();
            } else if (timeframe == "5m") {
                expectedPoints = duration.count() * 12; // 12 five-minute periods per hour
            }

            result.missingDataPoints = std::max(0, expectedPoints - result.totalDataPoints);

            // Calculate data quality score
            double completenessScore = (double)result.totalDataPoints / std::max(1, expectedPoints);
            completenessScore = std::min(1.0, completenessScore); // Cap at 100%

            result.dataQualityScore = completenessScore;

            // Determine if data is valid (80% completeness threshold)
            result.isValid = result.dataQualityScore >= 0.80;

            if (result.dataQualityScore < 0.80) {
                result.warnings.push_back("Data completeness below 80%");
            }

            if (result.dataQualityScore < 0.60) {
                result.errors.push_back("Data completeness critically low");
            }

            // Extract first and last dates
            if (result.totalDataPoints > 0) {
                // This would require parsing the actual data format
                result.firstDate = startDate; // Simplified
                result.lastDate = endDate;    // Simplified
            }
        }
    } catch (const std::exception& e) {
        result.errors.push_back("Exception during validation: " + std::string(e.what()));
        result.isValid = false;
    }

    return result;
}

bool HistoricalDataLoader::checkDataCompleteness(const std::string& symbol,
                                                const std::string& timeframe,
                                                const std::chrono::system_clock::time_point& startDate,
                                                const std::chrono::system_clock::time_point& endDate) const {
    // Check across all providers for completeness
    std::vector<std::string> providers = {"cryptocompare", "alphavantage"};

    for (const auto& provider : providers) {
        auto validation = validateHistoricalData(symbol, provider, timeframe, startDate, endDate);
        if (validation.isValid && validation.dataQualityScore >= 0.90) {
            return true; // Found complete data
        }
    }

    return false; // No complete data found
}

LoadingProgress HistoricalDataLoader::getLoadingProgress(const std::string& loadingId) const {
    std::lock_guard<std::mutex> lock(loaderMutex_);

    auto it = activeLoadings_.find(loadingId);
    if (it != activeLoadings_.end()) {
        return it->second;
    }

    return LoadingProgress(); // Return empty progress if not found
}

bool HistoricalDataLoader::isLoadingComplete(const std::string& loadingId) const {
    auto progress = getLoadingProgress(loadingId);
    return progress.status == "completed" || progress.status == "failed";
}

void HistoricalDataLoader::workerThreadFunction() {
    while (!shutdownRequested_) {
        std::string loadingId;

        {
            std::unique_lock<std::mutex> lock(loaderMutex_);
            workerCondition_.wait(lock, [this] {
                return !workQueue_.empty() || shutdownRequested_;
            });

            if (shutdownRequested_) break;

            if (!workQueue_.empty()) {
                loadingId = workQueue_.front();
                workQueue_.pop();
            }
        }

        if (!loadingId.empty()) {
            processLoadingChunks(loadingId);
        }
    }
}

void HistoricalDataLoader::processLoadingChunks(const std::string& loadingId) {
    std::lock_guard<std::mutex> lock(loaderMutex_);

    auto progressIt = activeLoadings_.find(loadingId);
    auto chunksIt = loadingChunks_.find(loadingId);

    if (progressIt == activeLoadings_.end() || chunksIt == loadingChunks_.end()) {
        std::cerr << "Loading ID not found: " << loadingId << std::endl;
        return;
    }

    LoadingProgress& progress = progressIt->second;
    std::vector<DataChunk>& chunks = chunksIt->second;

    progress.status = "in_progress";

    for (auto& chunk : chunks) {
        if (shutdownRequested_) {
            progress.status = "cancelled";
            break;
        }

        if (chunk.isComplete) continue;

        // Check API limits before making request
        if (!apiHandler_.isProviderAvailable(chunk.provider)) {
            std::cout << "Provider " << chunk.provider << " not available, skipping chunk" << std::endl;
            continue;
        }

        // Apply conservation strategy delays
        if (conservationStrategy_.respectRateLimit) {
            auto delay = apiHandler_.getNextAvailableTime(chunk.provider);
            if (delay.count() > 0) {
                std::this_thread::sleep_for(delay);
            }
        }

        // Execute chunk request
        if (executeChunkRequest(chunk)) {
            progress.completedChunks++;
            progress.bytesLoaded += chunk.data.size();

            // Store chunk data in cache
            if (chunk.isComplete && !chunk.data.empty()) {
                cacheManager_.putHistoricalData(chunk.symbol, chunk.provider,
                                              chunk.timeframe, chunk.data,
                                              chunk.startDate, chunk.endDate);
            }

            // Fire progress callback
            if (progressCallback_) {
                progressCallback_(progress);
            }
        } else {
            progress.failedChunks++;
            chunk.retryCount++;

            if (chunk.retryCount >= 3) {
                std::cout << "Chunk failed after 3 retries: " << chunk.symbol
                          << " (" << formatTimePoint(chunk.startDate) << ")" << std::endl;
            }
        }

        updateLoadingProgress(loadingId);

        // Add delay between requests for API conservation
        if (conservationStrategy_.respectRateLimit) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2000)); // 2 second minimum
        }
    }

    // Mark loading as complete
    if (progress.completedChunks + progress.failedChunks >= progress.totalChunks) {
        if (progress.failedChunks == 0) {
            progress.status = "completed";
            std::cout << "Loading completed successfully: " << loadingId << std::endl;

            if (completionCallback_) {
                completionCallback_(loadingId, true);
            }
        } else {
            progress.status = "completed_with_errors";
            std::cout << "Loading completed with " << progress.failedChunks
                      << " failed chunks: " << loadingId << std::endl;

            if (completionCallback_) {
                completionCallback_(loadingId, false);
            }
        }
    }
}

bool HistoricalDataLoader::executeChunkRequest(DataChunk& chunk) {
    try {
        // Use ApiLimitHandler to make the request with fallback
        std::string endpoint = "histoday"; // CryptoCompare historical daily endpoint

        if (chunk.timeframe == "1h") {
            endpoint = "histohour";
        } else if (chunk.timeframe == "5m") {
            endpoint = "histominute";
        }

        std::string result = apiHandler_.requestDataWithFallback(
            chunk.provider, endpoint, chunk.symbol, RequestPriority::MEDIUM, true);

        if (!result.empty()) {
            chunk.data = result;
            chunk.isComplete = true;
            statistics_.totalRequestsCompleted++;
            statistics_.apiCallsUsed++;

            return true;
        } else {
            statistics_.totalRequestsFailed++;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception in chunk request: " << e.what() << std::endl;
        statistics_.totalRequestsFailed++;
        return false;
    }
}

void HistoricalDataLoader::updateLoadingProgress(const std::string& loadingId) {
    auto& progress = activeLoadings_[loadingId];

    if (progress.totalChunks > 0) {
        progress.progressPercent = (double)progress.completedChunks / progress.totalChunks * 100.0;
    }

    // Update estimated completion based on current progress
    if (progress.completedChunks > 0) {
        auto elapsed = std::chrono::system_clock::now() - progress.startTime;
        auto avgTimePerChunk = elapsed / progress.completedChunks;
        auto remainingChunks = progress.totalChunks - progress.completedChunks;
        progress.estimatedCompletion = std::chrono::system_clock::now() +
                                      (avgTimePerChunk * remainingChunks);
    }

    // Save progress to database for persistence
    saveLoadingProgress(progress);
}

std::vector<HistoricalDataRequest> HistoricalDataLoader::optimizeRequestOrder(
    const std::vector<HistoricalDataRequest>& requests) const {

    std::vector<HistoricalDataRequest> optimized = requests;

    // Sort by priority first, then by provider preference
    std::sort(optimized.begin(), optimized.end(),
              [](const HistoricalDataRequest& a, const HistoricalDataRequest& b) {
                  if (a.priority != b.priority) {
                      return a.priority < b.priority; // Lower number = higher priority
                  }
                  // Group by provider to minimize switching
                  return a.provider < b.provider;
              });

    return optimized;
}

std::string HistoricalDataLoader::generateLoadingId() const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);

    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    return "load_" + std::to_string(timestamp) + "_" + std::to_string(dis(gen));
}

std::string HistoricalDataLoader::formatTimePoint(const std::chrono::system_clock::time_point& tp) const {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

bool HistoricalDataLoader::saveLoadingProgress(const LoadingProgress& progress) {
    // In practice, this would save to database
    // For now, just log the progress
    std::cout << "Progress [" << progress.requestId << "]: "
              << progress.progressPercent << "% ("
              << progress.completedChunks << "/" << progress.totalChunks << ")" << std::endl;
    return true;
}

HistoricalDataLoader::LoadingStatistics HistoricalDataLoader::getLoadingStatistics() const {
    std::lock_guard<std::mutex> lock(loaderMutex_);
    return statistics_;
}

void HistoricalDataLoader::resetStatistics() {
    std::lock_guard<std::mutex> lock(loaderMutex_);
    statistics_ = LoadingStatistics();
}

// HistoricalDataLoaderFactory Implementation
std::unique_ptr<HistoricalDataLoader> HistoricalDataLoaderFactory::create(
    SmartCacheManager& cacheManager,
    ApiLimitHandler& apiHandler,
    DatabaseManager& dbManager,
    LoadingStrategy strategy) {

    auto loader = std::make_unique<HistoricalDataLoader>(cacheManager, apiHandler, dbManager);

    auto conservationStrategy = getConservationForStrategy(strategy);
    loader->setConservationStrategy(conservationStrategy);

    return loader;
}

HistoricalDataLoader::PersonalSetupConfig HistoricalDataLoaderFactory::getConfigForStrategy(LoadingStrategy strategy) {
    HistoricalDataLoader::PersonalSetupConfig config;

    switch (strategy) {
        case LoadingStrategy::CONSERVATIVE:
            config.maxDailyApiCalls = 50;
            config.maxSimultaneousLoads = 1;
            config.delayBetweenRequests = std::chrono::milliseconds(30000); // 30 seconds
            config.conservativeMode = true;
            break;

        case LoadingStrategy::BALANCED:
            config.maxDailyApiCalls = 100;
            config.maxSimultaneousLoads = 1;
            config.delayBetweenRequests = std::chrono::milliseconds(15000); // 15 seconds
            config.conservativeMode = true;
            break;

        case LoadingStrategy::AGGRESSIVE:
            config.maxDailyApiCalls = 200;
            config.maxSimultaneousLoads = 2;
            config.delayBetweenRequests = std::chrono::milliseconds(5000); // 5 seconds
            config.conservativeMode = false;
            break;

        case LoadingStrategy::EMERGENCY:
            config.symbols = {"BTC", "ETH"}; // Only critical symbols
            config.timeframes = {"1d"}; // Only daily data
            config.maxDailyApiCalls = 20;
            config.maxSimultaneousLoads = 1;
            config.delayBetweenRequests = std::chrono::milliseconds(60000); // 1 minute
            config.conservativeMode = true;
            break;
    }

    return config;
}

HistoricalDataLoader::ConservationStrategy HistoricalDataLoaderFactory::getConservationForStrategy(LoadingStrategy strategy) {
    HistoricalDataLoader::ConservationStrategy conservation;

    switch (strategy) {
        case LoadingStrategy::CONSERVATIVE:
            conservation.maxUtilizationPercent = 60.0;
            conservation.respectRateLimit = true;
            conservation.batchSimilarRequests = true;
            conservation.usePreferredProvider = true;
            break;

        case LoadingStrategy::BALANCED:
            conservation.maxUtilizationPercent = 75.0;
            conservation.respectRateLimit = true;
            conservation.batchSimilarRequests = true;
            conservation.usePreferredProvider = true;
            break;

        case LoadingStrategy::AGGRESSIVE:
            conservation.maxUtilizationPercent = 90.0;
            conservation.respectRateLimit = true;
            conservation.batchSimilarRequests = false; // Less batching for speed
            conservation.usePreferredProvider = false; // Use any available
            break;

        case LoadingStrategy::EMERGENCY:
            conservation.maxUtilizationPercent = 40.0; // Very conservative
            conservation.respectRateLimit = true;
            conservation.batchSimilarRequests = true;
            conservation.usePreferredProvider = true;
            break;
    }

    return conservation;
}

} // namespace Data
} // namespace CryptoClaude