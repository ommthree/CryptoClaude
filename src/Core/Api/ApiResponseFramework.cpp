#include "ApiResponseFramework.h"
#include "../Json/JsonParser.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cmath>

namespace CryptoClaude {
namespace Api {

// =============================================================================
// AlertManager Implementation
// =============================================================================

void AlertManager::addAlert(AlertType type, const std::string& message, const std::string& source) {
    Alert alert;
    alert.type = type;
    alert.message = message;
    alert.source = source;
    alert.timestamp = std::chrono::system_clock::now();
    alert.id = generateAlertId();

    alerts_.push_back(alert);

    // Log alert based on severity
    switch (type) {
        case AlertType::INFO:
            std::cout << "[INFO] " << source << ": " << message << std::endl;
            break;
        case AlertType::WARNING:
            std::cout << "[WARNING] " << source << ": " << message << std::endl;
            break;
        case AlertType::ERROR:
            std::cerr << "[ERROR] " << source << ": " << message << std::endl;
            break;
        case AlertType::CRITICAL:
            std::cerr << "[CRITICAL] " << source << ": " << message << std::endl;
            break;
    }
}

std::vector<Alert> AlertManager::getRecentAlerts(std::chrono::minutes timeWindow) const {
    auto cutoff = std::chrono::system_clock::now() - timeWindow;
    std::vector<Alert> recent;

    for (const auto& alert : alerts_) {
        if (alert.timestamp >= cutoff) {
            recent.push_back(alert);
        }
    }

    return recent;
}

bool AlertManager::hasCriticalAlerts(std::chrono::minutes timeWindow) const {
    auto recent = getRecentAlerts(timeWindow);
    return std::any_of(recent.begin(), recent.end(),
        [](const Alert& alert) { return alert.type == AlertType::CRITICAL; });
}

void AlertManager::clearOldAlerts(std::chrono::hours maxAge) {
    auto cutoff = std::chrono::system_clock::now() - maxAge;
    alerts_.erase(
        std::remove_if(alerts_.begin(), alerts_.end(),
            [cutoff](const Alert& alert) { return alert.timestamp < cutoff; }),
        alerts_.end()
    );
}

std::string AlertManager::generateAlertId() {
    static int counter = 0;
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    return "alert_" + std::to_string(timestamp) + "_" + std::to_string(++counter);
}

// =============================================================================
// RateLimiter Implementation
// =============================================================================

RateLimiter::RateLimiter(const RateLimitConfig& config, std::shared_ptr<AlertManager> alertManager)
    : config_(config), alertManager_(alertManager) {

    // Initialize with conservative defaults
    currentLimit_ = config_.maxRequestsPerMinute;
    tokens_ = static_cast<double>(currentLimit_);
    lastRefill_ = std::chrono::steady_clock::now();
}

bool RateLimiter::canMakeRequest() {
    std::lock_guard<std::mutex> lock(mutex_);
    refillTokens();

    if (tokens_ >= 1.0) {
        tokens_ -= 1.0;
        recordRequest();
        return true;
    }

    // Rate limited - add alert
    if (alertManager_) {
        alertManager_->addAlert(AlertType::WARNING,
            "Rate limit reached. Current limit: " + std::to_string(currentLimit_) + " requests/minute",
            "RateLimiter");
    }

    return false;
}

std::chrono::milliseconds RateLimiter::getRetryDelay() {
    std::lock_guard<std::mutex> lock(mutex_);
    refillTokens();

    if (tokens_ >= 1.0) {
        return std::chrono::milliseconds(0);
    }

    // Calculate time until next token becomes available
    double tokensNeeded = 1.0 - tokens_;
    double tokensPerMs = static_cast<double>(currentLimit_) / 60000.0; // per millisecond
    auto delayMs = static_cast<long>(tokensNeeded / tokensPerMs);

    return std::chrono::milliseconds(std::max(100L, delayMs)); // Minimum 100ms
}

void RateLimiter::recordFailure() {
    std::lock_guard<std::mutex> lock(mutex_);

    recentFailures_++;

    // Adaptive scaling - be more conservative on failures
    if (config_.enableAdaptiveScaling && recentFailures_ > 2) {
        int newLimit = static_cast<int>(currentLimit_ * config_.adaptiveScaleFactor);
        if (newLimit < config_.minRequestsPerMinute) {
            newLimit = config_.minRequestsPerMinute;
        }

        if (newLimit != currentLimit_) {
            if (alertManager_) {
                alertManager_->addAlert(AlertType::WARNING,
                    "Reducing rate limit due to failures: " + std::to_string(currentLimit_) +
                    " -> " + std::to_string(newLimit) + " requests/minute",
                    "RateLimiter");
            }
            currentLimit_ = newLimit;
        }
    }
}

void RateLimiter::recordSuccess() {
    std::lock_guard<std::mutex> lock(mutex_);

    consecutiveSuccesses_++;

    // Very conservative recovery - only increase after many successes
    if (config_.enableAdaptiveScaling && consecutiveSuccesses_ > 20) {
        int maxAllowed = std::min(config_.maxRequestsPerMinute,
                                  static_cast<int>(currentLimit_ / config_.adaptiveScaleFactor));

        if (currentLimit_ < maxAllowed && recentFailures_ == 0) {
            int newLimit = std::min(currentLimit_ + 1, maxAllowed); // Very gradual increase

            if (alertManager_) {
                alertManager_->addAlert(AlertType::INFO,
                    "Cautiously increasing rate limit: " + std::to_string(currentLimit_) +
                    " -> " + std::to_string(newLimit) + " requests/minute",
                    "RateLimiter");
            }
            currentLimit_ = newLimit;
            consecutiveSuccesses_ = 0; // Reset counter
        }
    }
}

void RateLimiter::refillTokens() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastRefill_).count();

    if (elapsed > 0) {
        double tokensToAdd = (static_cast<double>(currentLimit_) * elapsed) / 60000.0; // 60000ms = 1 minute
        tokens_ = std::min(static_cast<double>(currentLimit_), tokens_ + tokensToAdd);
        lastRefill_ = now;
    }
}

void RateLimiter::recordRequest() {
    auto now = std::chrono::system_clock::now();
    recentRequests_.push_back(now);

    // Clean up old requests (older than 1 minute)
    auto cutoff = now - std::chrono::minutes(1);
    recentRequests_.erase(
        std::remove_if(recentRequests_.begin(), recentRequests_.end(),
            [cutoff](const std::chrono::system_clock::time_point& tp) { return tp < cutoff; }),
        recentRequests_.end()
    );

    // Reset failure counters periodically
    if (recentRequests_.size() % 10 == 0) {
        recentFailures_ = std::max(0, recentFailures_ - 1); // Gradual decay
    }
}

// =============================================================================
// ResponseProcessor Implementation
// =============================================================================

ResponseProcessor::ResponseProcessor(const DataFreshnessConfig& config, std::shared_ptr<AlertManager> alertManager)
    : config_(config), alertManager_(alertManager) {}

ProcessedResponse ResponseProcessor::processResponse(const Http::HttpResponse& httpResponse,
                                                   ApiProvider provider,
                                                   const std::string& endpoint) {
    ProcessedResponse result{
        httpResponse,                                        // originalResponse
        Json::JsonValue{},                                  // parsedData (empty initially)
        ResponseStatus::SUCCESS,                            // status (will be updated)
        std::nullopt,                                       // errorMessage
        provider,                                           // provider
        endpoint,                                           // endpoint
        std::chrono::system_clock::now(),                  // processedAt
        std::nullopt                                       // dataTimestamp
    };

    // Step 1: Check HTTP status
    if (httpResponse.getStatusCode() >= 200 && httpResponse.getStatusCode() < 300) {
        result.status = ResponseStatus::SUCCESS;
    } else if (httpResponse.getStatusCode() >= 400 && httpResponse.getStatusCode() < 500) {
        result.status = ResponseStatus::CLIENT_ERROR;
        result.errorMessage = "Client error: " + std::to_string(httpResponse.getStatusCode());
    } else if (httpResponse.getStatusCode() >= 500) {
        result.status = ResponseStatus::SERVER_ERROR;
        result.errorMessage = "Server error: " + std::to_string(httpResponse.getStatusCode());
    } else {
        result.status = ResponseStatus::NETWORK_ERROR;
        result.errorMessage = "Network error: " + std::to_string(httpResponse.getStatusCode());
    }

    // Step 2: Parse and validate JSON if successful
    if (result.status == ResponseStatus::SUCCESS) {
        try {
            Json::JsonParser parser;
            result.parsedData = parser.parse(httpResponse.getBody());

            // Step 3: Provider-specific validation
            validateProviderResponse(result);

            // Step 4: Check data freshness
            checkDataFreshness(result);

        } catch (const Json::JsonParseException& e) {
            result.status = ResponseStatus::VALIDATION_ERROR;
            result.errorMessage = "JSON parsing failed: " + std::string(e.what());
        } catch (const std::exception& e) {
            result.status = ResponseStatus::VALIDATION_ERROR;
            result.errorMessage = "Response validation failed: " + std::string(e.what());
        }
    }

    // Step 5: Generate alerts for problematic responses
    generateAlertsForResponse(result);

    return result;
}

bool ResponseProcessor::isDataStale(const ProcessedResponse& response) {
    if (!response.dataTimestamp.has_value()) {
        return false; // Can't determine staleness without timestamp
    }

    auto age = std::chrono::system_clock::now() - response.dataTimestamp.value();

    // Determine staleness based on data type
    bool isMarketData = (response.endpoint.find("price") != std::string::npos ||
                        response.endpoint.find("ohlcv") != std::string::npos ||
                        response.endpoint.find("ticker") != std::string::npos);

    if (isMarketData) {
        return age > config_.maxMarketDataAge;
    } else {
        return age > config_.maxNewsDataAge;
    }
}

void ResponseProcessor::validateProviderResponse(ProcessedResponse& response) {
    Json::JsonParser parser;

    switch (response.provider) {
        case ApiProvider::CRYPTO_COMPARE: {
            // Check for CryptoCompare error responses
            if (parser.pathExists(response.parsedData, "Response") &&
                parser.getString(response.parsedData, "Response").value_or("") == "Error") {

                response.status = ResponseStatus::CLIENT_ERROR;
                response.errorMessage = parser.getString(response.parsedData, "Message")
                                       .value_or("CryptoCompare API error");
                return;
            }

            // Extract timestamp if available
            if (parser.pathExists(response.parsedData, "LASTUPDATE")) {
                auto timestamp = parser.getDouble(response.parsedData, "LASTUPDATE");
                if (timestamp.has_value()) {
                    response.dataTimestamp = std::chrono::system_clock::from_time_t(
                        static_cast<time_t>(timestamp.value())
                    );
                }
            } else {
                // Fallback: assume data is fresh for testing
                response.dataTimestamp = std::chrono::system_clock::now();
            }
            break;
        }

        case ApiProvider::NEWS_API: {
            // Check NewsAPI status
            auto status = parser.getString(response.parsedData, "status");
            if (status.has_value() && status.value() != "ok") {
                response.status = ResponseStatus::CLIENT_ERROR;
                response.errorMessage = parser.getString(response.parsedData, "message")
                                       .value_or("NewsAPI error");
                return;
            }

            // NewsAPI data is considered fresh (real-time news)
            response.dataTimestamp = std::chrono::system_clock::now();
            break;
        }

        case ApiProvider::UNKNOWN:
        default:
            // Generic validation - just check if we have valid JSON
            break;
    }
}

void ResponseProcessor::checkDataFreshness(ProcessedResponse& response) {
    if (isDataStale(response)) {
        response.status = ResponseStatus::DATA_STALE;

        auto age = std::chrono::system_clock::now() - response.dataTimestamp.value();
        auto ageHours = std::chrono::duration_cast<std::chrono::hours>(age).count();

        response.errorMessage = "Data is stale. Age: " + std::to_string(ageHours) + " hours";
    }
}

void ResponseProcessor::generateAlertsForResponse(const ProcessedResponse& response) {
    if (!alertManager_) return;

    std::string source = "ResponseProcessor:" + std::to_string(static_cast<int>(response.provider));

    switch (response.status) {
        case ResponseStatus::SUCCESS:
            // No alert needed for success
            break;

        case ResponseStatus::PARTIAL_SUCCESS:
            alertManager_->addAlert(AlertType::WARNING,
                "Partial response from " + response.endpoint + ": " +
                response.errorMessage.value_or("Unknown issue"), source);
            break;

        case ResponseStatus::DATA_STALE:
            alertManager_->addAlert(AlertType::WARNING,
                "Stale data from " + response.endpoint + ": " +
                response.errorMessage.value_or("Data too old"), source);
            break;

        case ResponseStatus::RETRY_NEEDED:
        case ResponseStatus::RATE_LIMITED:
            alertManager_->addAlert(AlertType::INFO,
                "Temporary issue with " + response.endpoint + ": " +
                response.errorMessage.value_or("Retry needed"), source);
            break;

        case ResponseStatus::CLIENT_ERROR:
        case ResponseStatus::VALIDATION_ERROR:
            alertManager_->addAlert(AlertType::ERROR,
                "Client/Validation error from " + response.endpoint + ": " +
                response.errorMessage.value_or("Request error"), source);
            break;

        case ResponseStatus::SERVER_ERROR:
        case ResponseStatus::NETWORK_ERROR:
        case ResponseStatus::FATAL_ERROR:
            alertManager_->addAlert(AlertType::CRITICAL,
                "Critical error from " + response.endpoint + ": " +
                response.errorMessage.value_or("Server/Network error"), source);
            break;
    }
}

// =============================================================================
// ApiClient Implementation
// =============================================================================

ApiClient::ApiClient(std::shared_ptr<Http::IHttpClient> httpClient,
                     std::shared_ptr<RateLimiter> rateLimiter,
                     std::shared_ptr<ResponseProcessor> processor,
                     std::shared_ptr<AlertManager> alertManager)
    : httpClient_(httpClient), rateLimiter_(rateLimiter),
      processor_(processor), alertManager_(alertManager) {}

ApiResponse ApiClient::makeRequest(const std::string& url, ApiProvider provider) {
    ApiResponse response;
    response.provider = provider;
    response.url = url;
    response.requestedAt = std::chrono::system_clock::now();

    // Check if we should stop due to critical alerts
    if (alertManager_ && alertManager_->hasCriticalAlerts(std::chrono::minutes(5))) {
        response.status = ResponseStatus::FATAL_ERROR;
        response.errorMessage = "System stopped due to critical alerts";
        return response;
    }

    // Attempt request with retries
    for (int attempt = 1; attempt <= 3; ++attempt) {
        // Check rate limiting
        if (!rateLimiter_->canMakeRequest()) {
            auto delay = rateLimiter_->getRetryDelay();

            if (attempt == 1 && delay.count() < 5000) { // Only wait up to 5 seconds
                std::this_thread::sleep_for(delay);
                continue;
            } else {
                response.status = ResponseStatus::RATE_LIMITED;
                response.errorMessage = "Rate limited, retry delay too long: " +
                                       std::to_string(delay.count()) + "ms";
                return response;
            }
        }

        // Make the HTTP request
        Http::HttpRequest request(Http::HttpMethod::GET, url);
        Http::HttpResponse httpResponse = httpClient_->execute(request);

        // Process the response
        ProcessedResponse processed = processor_->processResponse(httpResponse, provider, url);

        // Map processed response to API response
        response.status = processed.status;
        response.errorMessage = processed.errorMessage;
        response.processedData = processed.parsedData;
        response.httpStatusCode = httpResponse.getStatusCode();
        response.responseTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - response.requestedAt);

        // Decide whether to retry based on status
        bool shouldRetry = false;
        switch (processed.status) {
            case ResponseStatus::SUCCESS:
            case ResponseStatus::PARTIAL_SUCCESS:
                rateLimiter_->recordSuccess();
                return response; // Success - return immediately

            case ResponseStatus::DATA_STALE:
                // Stale data is still usable, but log warning
                rateLimiter_->recordSuccess(); // Not a failure of the API itself
                return response;

            case ResponseStatus::RATE_LIMITED:
                shouldRetry = (attempt < 3); // Retry rate limits
                break;

            case ResponseStatus::SERVER_ERROR:
            case ResponseStatus::NETWORK_ERROR:
                shouldRetry = (attempt < 3); // Retry server/network errors
                rateLimiter_->recordFailure();
                break;

            case ResponseStatus::CLIENT_ERROR:
            case ResponseStatus::VALIDATION_ERROR:
                rateLimiter_->recordFailure();
                return response; // Don't retry client errors

            case ResponseStatus::FATAL_ERROR:
                rateLimiter_->recordFailure();
                return response; // Don't retry fatal errors

            case ResponseStatus::RETRY_NEEDED:
                shouldRetry = (attempt < 3);
                break;
        }

        if (!shouldRetry) {
            break;
        }

        // Exponential backoff for retries
        auto backoffDelay = std::chrono::milliseconds(1000 * attempt); // 1s, 2s, 3s
        std::this_thread::sleep_for(backoffDelay);
    }

    // All retries exhausted - check if we should alert and stop
    if (response.status == ResponseStatus::SERVER_ERROR ||
        response.status == ResponseStatus::NETWORK_ERROR ||
        response.status == ResponseStatus::FATAL_ERROR) {

        if (alertManager_) {
            alertManager_->addAlert(AlertType::CRITICAL,
                "API failure after retries - system should stop: " + url,
                "ApiClient");
        }
    }

    return response;
}

bool ApiClient::isHealthy() const {
    // Consider the client healthy if no critical alerts in the last 5 minutes
    if (alertManager_) {
        return !alertManager_->hasCriticalAlerts(std::chrono::minutes(5));
    }
    return true;
}

std::vector<Alert> ApiClient::getRecentAlerts() const {
    if (alertManager_) {
        return alertManager_->getRecentAlerts(std::chrono::minutes(30));
    }
    return {};
}

// =============================================================================
// Provider-Specific Adapters
// =============================================================================

CryptoCompareAdapter::CryptoCompareAdapter(std::shared_ptr<ApiClient> client, const std::string& apiKey)
    : client_(client), apiKey_(apiKey) {}

ApiResponse CryptoCompareAdapter::getCurrentPrice(const std::string& fromSymbol, const std::string& toSymbol) {
    std::string url = "https://min-api.cryptocompare.com/data/price?fsym=" + fromSymbol +
                     "&tsyms=" + toSymbol + "&api_key=" + apiKey_;
    return client_->makeRequest(url, ApiProvider::CRYPTO_COMPARE);
}

ApiResponse CryptoCompareAdapter::getHistoricalData(const std::string& symbol, int days) {
    std::string url = "https://min-api.cryptocompare.com/data/v2/histoday?fsym=" + symbol +
                     "&tsym=USD&limit=" + std::to_string(days) + "&api_key=" + apiKey_;
    return client_->makeRequest(url, ApiProvider::CRYPTO_COMPARE);
}

NewsApiAdapter::NewsApiAdapter(std::shared_ptr<ApiClient> client, const std::string& apiKey)
    : client_(client), apiKey_(apiKey) {}

ApiResponse NewsApiAdapter::getTopHeadlines(const std::string& category, const std::string& country) {
    std::string url = "https://newsapi.org/v2/top-headlines?category=" + category +
                     "&country=" + country + "&apiKey=" + apiKey_;
    return client_->makeRequest(url, ApiProvider::NEWS_API);
}

ApiResponse NewsApiAdapter::searchNews(const std::string& query, const std::string& sortBy) {
    std::string url = "https://newsapi.org/v2/everything?q=" + query +
                     "&sortBy=" + sortBy + "&apiKey=" + apiKey_;
    return client_->makeRequest(url, ApiProvider::NEWS_API);
}

} // namespace Api
} // namespace CryptoClaude