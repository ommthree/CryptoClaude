#include "ApiLimitHandler.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <random>
#include <cmath>
#include <thread>

namespace CryptoClaude {
namespace Data {

// ApiLimitHandler Implementation
ApiLimitHandler::ApiLimitHandler(SmartCacheManager& cacheManager, PersonalUsageTracker& usageTracker)
    : cacheManager_(cacheManager), usageTracker_(usageTracker),
      gracefulDegradationEnabled_(true), emergencyMode_(false),
      requestQueue_([](const ApiRequest& a, const ApiRequest& b) {
          // Higher priority requests come first (lower number = higher priority)
          if (a.priority != b.priority) {
              return a.priority > b.priority;
          }
          // If same priority, earlier scheduled time comes first
          return a.scheduledTime > b.scheduledTime;
      }) {

    // Initialize default degradation strategies
    providerStrategies_["cryptocompare"] = DegradationStrategy::CACHE_FIRST;
    providerStrategies_["newsapi"] = DegradationStrategy::REDUCE_FREQUENCY;
    providerStrategies_["alphavantage"] = DegradationStrategy::PROVIDER_FALLBACK;

    // Initialize fallback sources
    setupDefaultFallbackSources();
}

bool ApiLimitHandler::initialize() {
    std::lock_guard<std::mutex> lock(handlerMutex_);

    updateProviderStatuses();

    std::cout << "ApiLimitHandler initialized with graceful degradation enabled" << std::endl;
    return true;
}

void ApiLimitHandler::shutdown() {
    std::lock_guard<std::mutex> lock(handlerMutex_);

    // Cancel all pending requests
    pendingRequests_.clear();
    while (!requestQueue_.empty()) {
        requestQueue_.pop();
    }

    std::cout << "ApiLimitHandler shutdown completed" << std::endl;
}

std::string ApiLimitHandler::submitRequest(const ApiRequest& request) {
    std::lock_guard<std::mutex> lock(handlerMutex_);

    // Generate unique request ID
    std::string requestId = generateRequestId();

    ApiRequest requestCopy = request;
    requestCopy.requestId = requestId;

    // Check if we're in emergency mode and if this request should be served
    if (emergencyMode_ && !shouldServeInEmergencyMode(requestCopy)) {
        if (requestCopy.errorCallback) {
            requestCopy.errorCallback("Request denied: system in emergency mode");
        }
        return "";
    }

    // Try cache first if allowed
    if (requestCopy.canUseCache) {
        std::string cachedResult = tryCache(requestCopy);
        if (!cachedResult.empty()) {
            if (requestCopy.successCallback) {
                requestCopy.successCallback(cachedResult);
            }
            updateStatistics(requestCopy, cachedResult, true);
            return requestId;
        }
    }

    // Check provider availability
    if (!isProviderAvailable(requestCopy.provider)) {
        // Try fallback providers
        std::string fallbackResult = tryProviderFallback(requestCopy);
        if (!fallbackResult.empty()) {
            if (requestCopy.successCallback) {
                requestCopy.successCallback(fallbackResult);
            }
            updateStatistics(requestCopy, fallbackResult, false);
            return requestId;
        }

        // If no fallback available, apply degradation strategy
        std::string degradedResult = applyDegradationStrategy(requestCopy);
        if (!degradedResult.empty()) {
            if (requestCopy.successCallback) {
                requestCopy.successCallback(degradedResult);
            }
            updateStatistics(requestCopy, degradedResult, false);
            return requestId;
        }
    }

    // Add to queue for processing
    pendingRequests_[requestId] = requestCopy;
    requestQueue_.push(requestCopy);

    return requestId;
}

std::string ApiLimitHandler::requestDataWithFallback(const std::string& provider,
                                                    const std::string& endpoint,
                                                    const std::string& symbol,
                                                    RequestPriority priority,
                                                    bool allowCache) {
    ApiRequestBuilder builder;

    auto request = builder
        .provider(provider)
        .endpoint(endpoint)
        .symbol(symbol)
        .priority(priority)
        .allowCache(allowCache)
        .build();

    // Synchronous request with fallback handling
    std::string result = submitRequest(request);

    if (result.empty()) {
        // Try all available fallback strategies
        if (allowCache) {
            result = tryCache(request);
            if (!result.empty()) return result;
        }

        result = tryProviderFallback(request);
        if (!result.empty()) return result;

        if (gracefulDegradationEnabled_) {
            result = tryDataInterpolation(request);
            if (!result.empty()) return result;

            result = tryStaticFallback(request);
        }
    }

    return result;
}

bool ApiLimitHandler::isProviderAvailable(const std::string& provider) const {
    auto it = providerStatuses_.find(provider);
    if (it == providerStatuses_.end()) return false;

    const auto& status = it->second;

    // Check if temporarily blocked
    if (status.isTemporarilyBlocked) {
        return std::chrono::system_clock::now() > status.blockUntil;
    }

    // Check if we have remaining quota
    return status.remainingDaily > 0 && status.remainingMonthly > 0;
}

std::string ApiLimitHandler::tryCache(const ApiRequest& request) {
    std::string cacheKey = generateCacheKey(request);
    std::string cachedData = cacheManager_.get(cacheKey);

    if (!cachedData.empty()) {
        // Assess quality of cached data
        QualityMetrics quality = assessDataQuality(cachedData, "cache");

        // Accept cached data if it meets minimum quality standards
        if (quality.freshness > 0.3 || emergencyMode_) { // Accept 30% freshness normally, any in emergency
            return cachedData;
        }
    }

    return "";
}

std::string ApiLimitHandler::tryProviderFallback(const ApiRequest& request) {
    auto fallbackSources = getFallbackSources(request.provider);

    // Sort by priority (lower number = higher priority)
    std::sort(fallbackSources.begin(), fallbackSources.end(),
              [](const FallbackSource& a, const FallbackSource& b) {
                  return a.priority < b.priority;
              });

    for (const auto& fallback : fallbackSources) {
        if (!fallback.isAvailable || fallback.sourceType != "provider") continue;

        if (isProviderAvailable(fallback.provider)) {
            // Create new request with fallback provider
            ApiRequest fallbackRequest = request;
            fallbackRequest.provider = fallback.provider;

            std::string result = executeRequest(fallbackRequest);
            if (!result.empty()) {
                return result;
            }
        }
    }

    return "";
}

std::string ApiLimitHandler::tryDataInterpolation(const ApiRequest& request) {
    if (!emergencyConfig_.enableDataInterpolation) return "";

    // Get recent historical data for interpolation
    std::vector<std::string> historicalData;

    // Look for recent cached data points
    for (int i = 1; i <= 24; ++i) { // Check last 24 hours
        auto pastTime = std::chrono::system_clock::now() - std::chrono::hours(i);
        std::string historicalKey = generateHistoricalCacheKey(request, pastTime);
        std::string historicalValue = cacheManager_.get(historicalKey);

        if (!historicalValue.empty()) {
            historicalData.push_back(historicalValue);
        }

        if (historicalData.size() >= 5) break; // Enough data for interpolation
    }

    if (historicalData.size() >= 2) {
        return DegradationUtils::interpolatePriceData(historicalData,
                                                     std::chrono::system_clock::now());
    }

    return "";
}

std::string ApiLimitHandler::tryStaticFallback(const ApiRequest& request) {
    // For price data, use last known value with conservative adjustment
    std::string lastKnownValue = getLastKnownValue(request.symbol, request.endpoint);

    if (!lastKnownValue.empty()) {
        auto lastUpdate = getLastUpdateTime(request.symbol, request.endpoint);
        auto timeSinceUpdate = std::chrono::duration_cast<std::chrono::minutes>(
            std::chrono::system_clock::now() - lastUpdate);

        return DegradationUtils::generateConservativeEstimate(
            request.symbol, lastKnownValue, timeSinceUpdate);
    }

    return "";
}

void ApiLimitHandler::processRequestQueue() {
    std::lock_guard<std::mutex> lock(handlerMutex_);

    updateProviderStatuses();
    checkAndFireAlerts();

    while (!requestQueue_.empty()) {
        ApiRequest request = requestQueue_.top();
        requestQueue_.pop();

        // Check if request is still valid
        auto it = pendingRequests_.find(request.requestId);
        if (it == pendingRequests_.end()) {
            continue; // Request was cancelled
        }

        // Check if provider is now available
        if (isProviderAvailable(request.provider)) {
            std::string result = executeRequest(request);

            if (!result.empty()) {
                if (request.successCallback) {
                    request.successCallback(result);
                }
                updateStatistics(request, result, false);
            } else {
                // Request failed, apply degradation
                std::string degradedResult = applyDegradationStrategy(request);
                if (!degradedResult.empty() && request.successCallback) {
                    request.successCallback(degradedResult);
                }
                updateStatistics(request, degradedResult, false);
            }

            // Move to completed requests
            completedRequests_[request.requestId] = request;
            pendingRequests_.erase(request.requestId);
        } else {
            // Provider still not available, check if we should wait or degrade
            auto now = std::chrono::system_clock::now();
            auto maxDelay = request.createdTime + schedulingOptions_.maxDelay;

            if (now > maxDelay) {
                // Timeout reached, apply degradation
                std::string degradedResult = applyDegradationStrategy(request);
                if (request.successCallback && !degradedResult.empty()) {
                    request.successCallback(degradedResult);
                } else if (request.errorCallback) {
                    request.errorCallback("Request timeout: provider unavailable");
                }

                pendingRequests_.erase(request.requestId);
            } else {
                // Put request back in queue with updated schedule time
                request.scheduledTime = now + getNextAvailableTime(request.provider);
                requestQueue_.push(request);
            }
        }
    }
}

void ApiLimitHandler::updateProviderStatuses() {
    auto personalTiers = PersonalApiConfig::getPersonalApiTiers();

    for (const auto& tier : personalTiers) {
        const std::string& provider = tier.first;
        ApiLimitStatus& status = providerStatuses_[provider];

        status.provider = provider;
        status.dailyLimit = tier.second.dailyLimit;
        status.monthlyLimit = tier.second.monthlyLimit;

        // Get current usage from usage tracker
        auto usageStats = usageTracker_.getUsageStats(provider);
        status.dailyUsed = usageStats.dailyRequests;
        status.monthlyUsed = usageStats.monthlyRequests;
        status.remainingDaily = std::max(0, status.dailyLimit - status.dailyUsed);
        status.remainingMonthly = std::max(0, status.monthlyLimit - status.monthlyUsed);

        if (status.dailyLimit > 0) {
            status.utilizationPercent = (double)status.dailyUsed / status.dailyLimit * 100.0;
        }

        status.lastRequest = usageStats.lastRequest;
        status.dailyResetTime = usageStats.dailyResetTime;
        status.monthlyResetTime = usageStats.monthlyResetTime;

        // Check if temporarily blocked
        if (!usageTracker_.canMakeRequest(provider)) {
            status.isTemporarilyBlocked = true;
            status.blockUntil = std::chrono::system_clock::now() +
                               usageTracker_.getMinDelayForNextRequest(provider);
        } else {
            status.isTemporarilyBlocked = false;
        }
    }
}

std::string ApiLimitHandler::applyDegradationStrategy(const ApiRequest& request) {
    auto strategy = getDegradationStrategy(request.provider);

    switch (strategy) {
        case DegradationStrategy::CACHE_FIRST:
            return tryCache(request);

        case DegradationStrategy::PROVIDER_FALLBACK:
            return tryProviderFallback(request);

        case DegradationStrategy::INTERPOLATE_DATA:
            return tryDataInterpolation(request);

        case DegradationStrategy::STATIC_FALLBACK:
            return tryStaticFallback(request);

        case DegradationStrategy::REDUCE_FREQUENCY:
            // For reduce frequency, we defer the request
            return "";

        case DegradationStrategy::PRIORITIZE_CRITICAL:
            // Only serve if priority is high enough
            if (request.priority <= RequestPriority::HIGH) {
                return tryCache(request);
            }
            return "";

        default:
            return tryCache(request);
    }
}

void ApiLimitHandler::checkAndFireAlerts() {
    for (const auto& statusPair : providerStatuses_) {
        const auto& provider = statusPair.first;
        const auto& status = statusPair.second;

        // Check for approaching limits (80% utilization)
        if (status.utilizationPercent > 80.0 && status.utilizationPercent < 90.0) {
            LimitAlert alert;
            alert.provider = provider;
            alert.alertType = "approaching";
            alert.message = "API limit approaching for " + provider;
            alert.utilizationPercent = status.utilizationPercent;
            alert.alertTime = std::chrono::system_clock::now();

            fireAlert(alert);
        }

        // Check for exceeded limits (100% utilization)
        if (status.remainingDaily <= 0 || status.remainingMonthly <= 0) {
            LimitAlert alert;
            alert.provider = provider;
            alert.alertType = "exceeded";
            alert.message = "API limit exceeded for " + provider;
            alert.utilizationPercent = status.utilizationPercent;
            alert.alertTime = std::chrono::system_clock::now();

            fireAlert(alert);

            // Consider activating emergency mode if multiple providers are exceeded
            int exceededCount = 0;
            for (const auto& sp : providerStatuses_) {
                if (sp.second.remainingDaily <= 0) exceededCount++;
            }

            if (exceededCount >= 2 && !emergencyMode_) {
                activateEmergencyMode("Multiple providers exceeded limits");
            }
        }
    }
}

void ApiLimitHandler::fireAlert(const LimitAlert& alert) {
    recentAlerts_.push_back(alert);

    // Keep only recent alerts (last 48 hours)
    auto cutoff = std::chrono::system_clock::now() - std::chrono::hours(48);
    recentAlerts_.erase(
        std::remove_if(recentAlerts_.begin(), recentAlerts_.end(),
                      [cutoff](const LimitAlert& a) { return a.alertTime < cutoff; }),
        recentAlerts_.end());

    if (alertCallback_) {
        alertCallback_(alert);
    }

    std::cout << "API Limit Alert [" << alert.provider << "]: "
              << alert.message << " (" << alert.utilizationPercent << "%)" << std::endl;
}

void ApiLimitHandler::activateEmergencyMode(const std::string& reason) {
    emergencyMode_ = true;

    LimitAlert alert;
    alert.alertType = "emergency";
    alert.message = "Emergency mode activated: " + reason;
    alert.alertTime = std::chrono::system_clock::now();

    fireAlert(alert);

    std::cout << "EMERGENCY MODE ACTIVATED: " << reason << std::endl;
}

void ApiLimitHandler::deactivateEmergencyMode() {
    emergencyMode_ = false;

    LimitAlert alert;
    alert.alertType = "recovery";
    alert.message = "Emergency mode deactivated";
    alert.alertTime = std::chrono::system_clock::now();

    fireAlert(alert);

    std::cout << "Emergency mode deactivated" << std::endl;
}

bool ApiLimitHandler::shouldServeInEmergencyMode(const ApiRequest& request) const {
    if (request.priority < emergencyConfig_.minPriority) {
        return false;
    }

    if (emergencyConfig_.disableNonCriticalUpdates &&
        request.priority > RequestPriority::HIGH) {
        return false;
    }

    // Check if symbol is in critical list
    if (!emergencyConfig_.criticalSymbols.empty()) {
        auto it = std::find(emergencyConfig_.criticalSymbols.begin(),
                           emergencyConfig_.criticalSymbols.end(),
                           request.symbol);
        return it != emergencyConfig_.criticalSymbols.end();
    }

    return true;
}

ApiLimitHandler::QualityMetrics ApiLimitHandler::assessDataQuality(const std::string& data, const std::string& source) const {
    QualityMetrics metrics;
    metrics.source = source;

    if (source == "cache") {
        // For cached data, quality depends on age
        // This is simplified - in practice, you'd check the cache entry timestamp
        metrics.freshness = 0.8; // Assume 80% freshness for cached data
        metrics.accuracy = 0.95;  // High accuracy for cached data
        metrics.completeness = 1.0; // Complete data
    } else if (source == "interpolated") {
        metrics.freshness = 1.0;  // Current time
        metrics.accuracy = 0.7;   // Lower accuracy for interpolated data
        metrics.completeness = 0.9; // Slightly incomplete
    } else if (source == "static") {
        metrics.freshness = 0.3;  // Low freshness
        metrics.accuracy = 0.6;   // Conservative estimate
        metrics.completeness = 0.8; // Basic data only
    } else {
        // Fresh API data
        metrics.freshness = 1.0;
        metrics.accuracy = 1.0;
        metrics.completeness = 1.0;
    }

    return metrics;
}

void ApiLimitHandler::setupDefaultFallbackSources() {
    // CryptoCompare fallbacks
    FallbackSource ccCache;
    ccCache.sourceId = "cryptocompare_cache";
    ccCache.sourceType = "cache";
    ccCache.reliability = 0.9;
    ccCache.maxAge = std::chrono::minutes(30);
    ccCache.priority = 1;
    fallbackSources_["cryptocompare"].push_back(ccCache);

    FallbackSource ccToAlpha;
    ccToAlpha.sourceId = "cryptocompare_to_alphavantage";
    ccToAlpha.sourceType = "provider";
    ccToAlpha.provider = "alphavantage";
    ccToAlpha.reliability = 0.8;
    ccToAlpha.priority = 2;
    fallbackSources_["cryptocompare"].push_back(ccToAlpha);

    // NewsAPI fallbacks
    FallbackSource newsCache;
    newsCache.sourceId = "newsapi_cache";
    newsCache.sourceType = "cache";
    newsCache.reliability = 0.7;
    newsCache.maxAge = std::chrono::hours(12);
    newsCache.priority = 1;
    fallbackSources_["newsapi"].push_back(newsCache);

    // Alpha Vantage fallbacks
    FallbackSource alphaCache;
    alphaCache.sourceId = "alphavantage_cache";
    alphaCache.sourceType = "cache";
    alphaCache.reliability = 0.9;
    alphaCache.maxAge = std::chrono::minutes(60);
    alphaCache.priority = 1;
    fallbackSources_["alphavantage"].push_back(alphaCache);
}

std::string ApiLimitHandler::generateRequestId() const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);

    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    return std::to_string(timestamp) + "_" + std::to_string(dis(gen));
}

std::string ApiLimitHandler::generateCacheKey(const ApiRequest& request) const {
    std::ostringstream oss;
    oss << request.dataType << ":" << request.provider << ":" << request.symbol;

    // Add relevant parameters to make key unique
    for (const auto& param : request.parameters) {
        oss << ":" << param.first << "=" << param.second;
    }

    return oss.str();
}

// ApiRequestBuilder Implementation
ApiRequestBuilder::ApiRequestBuilder() {
    request_.requestId = "";
    request_.priority = RequestPriority::MEDIUM;
    request_.canUseCache = true;
    request_.retryCount = 0;
}

ApiRequestBuilder& ApiRequestBuilder::provider(const std::string& provider) {
    request_.provider = provider;
    return *this;
}

ApiRequestBuilder& ApiRequestBuilder::endpoint(const std::string& endpoint) {
    request_.endpoint = endpoint;
    return *this;
}

ApiRequestBuilder& ApiRequestBuilder::symbol(const std::string& symbol) {
    request_.symbol = symbol;
    return *this;
}

ApiRequestBuilder& ApiRequestBuilder::priority(RequestPriority priority) {
    request_.priority = priority;
    return *this;
}

ApiRequestBuilder& ApiRequestBuilder::allowCache(bool allow) {
    request_.canUseCache = allow;
    return *this;
}

ApiRequest ApiRequestBuilder::build() {
    return request_;
}

ApiRequest ApiRequestBuilder::createPriceRequest(const std::string& symbol, const std::string& provider) {
    return ApiRequestBuilder()
        .provider(provider.empty() ? "cryptocompare" : provider)
        .endpoint("price")
        .symbol(symbol)
        .priority(RequestPriority::HIGH)
        .allowCache(true)
        .build();
}

ApiRequest ApiRequestBuilder::createHistoricalRequest(const std::string& symbol, int days, const std::string& provider) {
    return ApiRequestBuilder()
        .provider(provider.empty() ? "cryptocompare" : provider)
        .endpoint("histoday")
        .symbol(symbol)
        .priority(RequestPriority::MEDIUM)
        .parameter("limit", std::to_string(days))
        .allowCache(true)
        .build();
}

// DegradationUtils Implementation
std::string DegradationUtils::interpolatePriceData(const std::vector<std::string>& knownDataPoints,
                                                   const std::chrono::system_clock::time_point& targetTime) {
    // Simple linear interpolation for demonstration
    // In practice, this would parse the data points and perform proper interpolation
    if (knownDataPoints.size() < 2) {
        return knownDataPoints.empty() ? "" : knownDataPoints[0];
    }

    // Return the most recent data point for simplicity
    return knownDataPoints[0];
}

std::string DegradationUtils::generateConservativeEstimate(const std::string& symbol,
                                                          const std::string& lastKnownValue,
                                                          std::chrono::minutes timeSinceLastUpdate) {
    // For conservative estimates, assume minimal change
    // This is a simplified implementation
    return lastKnownValue; // No change assumed for safety
}

} // namespace Data
} // namespace CryptoClaude