#include "EnhancedHttpClient.h"
#include <iostream>
#include <random>
#include <regex>
#include <thread>
#include <algorithm>

namespace CryptoClaude {
namespace Http {

// ================================
// HttpConnection Implementation
// ================================

HttpConnection::HttpConnection(const std::string& host, int port, bool isHttps)
    : host_(host), port_(port), isHttps_(isHttps),
      inUse_(false), requestCount_(0), isHealthy_(true) {
    created_ = std::chrono::system_clock::now();
    lastUsed_ = created_;
    lastHealthCheck_ = created_;
}

HttpConnection::~HttpConnection() = default;

bool HttpConnection::isAvailable() const {
    return !inUse_.load() && isHealthy_;
}

bool HttpConnection::isExpired(std::chrono::minutes maxAge) const {
    auto now = std::chrono::system_clock::now();
    return (now - created_) > maxAge;
}

bool HttpConnection::needsHealthCheck(std::chrono::minutes interval) const {
    auto now = std::chrono::system_clock::now();
    return (now - lastHealthCheck_) > interval;
}

void HttpConnection::markInUse() {
    inUse_ = true;
    lastUsed_ = std::chrono::system_clock::now();
}

void HttpConnection::markAvailable() {
    inUse_ = false;
    incrementRequestCount();
}

void HttpConnection::updateLastUsed() {
    lastUsed_ = std::chrono::system_clock::now();
}

void HttpConnection::incrementRequestCount() {
    ++requestCount_;
}

bool HttpConnection::performHealthCheck() {
    // Simplified health check - in production would test actual connection
    lastHealthCheck_ = std::chrono::system_clock::now();

    // Simulate health check logic
    if (requestCount_ > 1000) {
        isHealthy_ = false; // Connection might be stale
        return false;
    }

    isHealthy_ = true;
    return true;
}

// ================================
// ConnectionPool Implementation
// ================================

ConnectionPool::ConnectionPool(int maxPerHost, int maxTotal)
    : maxConnectionsPerHost_(maxPerHost), maxTotalConnections_(maxTotal),
      connectionMaxAge_(std::chrono::minutes(5)), healthCheckInterval_(std::chrono::minutes(1)),
      totalConnections_(0), activeConnections_(0),
      connectionHits_(0), connectionMisses_(0), connectionsCreated_(0), connectionsDestroyed_(0) {
}

ConnectionPool::~ConnectionPool() {
    closeAllConnections();
}

std::shared_ptr<HttpConnection> ConnectionPool::getConnection(const std::string& host, int port, bool isHttps) {
    std::lock_guard<std::mutex> lock(poolMutex_);

    std::string key = createConnectionKey(host, port, isHttps);
    auto& hostQueue = connections_[key];

    // Try to get an existing available connection
    while (!hostQueue.empty()) {
        auto connection = hostQueue.front();
        hostQueue.pop();

        if (connection->isAvailable() && !connection->isExpired(connectionMaxAge_)) {
            connection->markInUse();
            ++activeConnections_;
            ++connectionHits_;
            return connection;
        } else {
            // Connection expired or unhealthy, destroy it
            ++connectionsDestroyed_;
            --totalConnections_;
        }
    }

    // No available connection, create new one if under limits
    if (totalConnections_ < maxTotalConnections_ &&
        hostQueue.size() < static_cast<size_t>(maxConnectionsPerHost_)) {

        auto newConnection = std::make_shared<HttpConnection>(host, port, isHttps);
        newConnection->markInUse();

        ++totalConnections_;
        ++activeConnections_;
        ++connectionsCreated_;
        ++connectionMisses_;

        return newConnection;
    }

    // Limits reached, return nullptr (caller should handle)
    ++connectionMisses_;
    return nullptr;
}

void ConnectionPool::returnConnection(std::shared_ptr<HttpConnection> connection) {
    if (!connection) return;

    std::lock_guard<std::mutex> lock(poolMutex_);

    connection->markAvailable();
    --activeConnections_;

    if (!connection->isExpired(connectionMaxAge_) && connection->isHealthy()) {
        // Return healthy connection to pool
        std::string key = createConnectionKey(connection->getHost(), connection->getPort(), connection->isHttps());
        connections_[key].push(connection);
    } else {
        // Connection expired or unhealthy, destroy it
        ++connectionsDestroyed_;
        --totalConnections_;
    }
}

std::string ConnectionPool::createConnectionKey(const std::string& host, int port, bool isHttps) const {
    return host + ":" + std::to_string(port) + ":" + (isHttps ? "https" : "http");
}

void ConnectionPool::cleanupExpiredConnections() {
    std::lock_guard<std::mutex> lock(poolMutex_);

    for (auto& [key, queue] : connections_) {
        cleanupExpiredConnectionsForHost(key);
    }
}

void ConnectionPool::cleanupExpiredConnectionsForHost(const std::string& key) {
    auto& queue = connections_[key];
    std::queue<std::shared_ptr<HttpConnection>> cleanQueue;

    while (!queue.empty()) {
        auto connection = queue.front();
        queue.pop();

        if (!connection->isExpired(connectionMaxAge_) && connection->isHealthy()) {
            cleanQueue.push(connection);
        } else {
            ++connectionsDestroyed_;
            --totalConnections_;
        }
    }

    connections_[key] = std::move(cleanQueue);
}

ConnectionPool::PoolStatistics ConnectionPool::getStatistics() const {
    PoolStatistics stats;
    stats.totalConnections = totalConnections_.load();
    stats.activeConnections = activeConnections_.load();
    stats.connectionHits = connectionHits_.load();
    stats.connectionMisses = connectionMisses_.load();
    stats.connectionsCreated = connectionsCreated_.load();
    stats.connectionsDestroyed = connectionsDestroyed_.load();

    int totalAttempts = stats.connectionHits + stats.connectionMisses;
    stats.hitRate = totalAttempts > 0 ? static_cast<double>(stats.connectionHits) / totalAttempts : 0.0;

    return stats;
}

void ConnectionPool::closeAllConnections() {
    std::lock_guard<std::mutex> lock(poolMutex_);
    connections_.clear();
    totalConnections_ = 0;
    activeConnections_ = 0;
}

void ConnectionPool::resetStatistics() {
    connectionHits_ = 0;
    connectionMisses_ = 0;
    connectionsCreated_ = 0;
    connectionsDestroyed_ = 0;
}

void ConnectionPool::performHealthChecks() {
    std::lock_guard<std::mutex> lock(poolMutex_);

    for (auto& [key, queue] : connections_) {
        std::queue<std::shared_ptr<HttpConnection>> healthyConnections;

        while (!queue.empty()) {
            auto connection = queue.front();
            queue.pop();

            if (connection->needsHealthCheck(healthCheckInterval_)) {
                if (connection->performHealthCheck()) {
                    healthyConnections.push(connection);
                } else {
                    --totalConnections_;
                    ++connectionsDestroyed_;
                }
            } else {
                healthyConnections.push(connection);
            }
        }

        queue = std::move(healthyConnections);
    }
}

// ================================
// CircuitBreaker Implementation
// ================================

CircuitBreaker::CircuitBreaker(int failureThreshold, std::chrono::seconds timeout,
                             int successThreshold, double failureRateThreshold)
    : state_(CircuitBreakerState::CLOSED), failureCount_(0), successCount_(0), totalRequests_(0),
      failureThreshold_(failureThreshold), timeout_(timeout), successThreshold_(successThreshold),
      failureRateThreshold_(failureRateThreshold), circuitOpenCount_(0), requestsRejected_(0) {

    stateChangeTime_ = std::chrono::system_clock::now();
}

bool CircuitBreaker::allowRequest() const {
    std::lock_guard<std::mutex> lock(stateMutex_);

    switch (state_) {
        case CircuitBreakerState::CLOSED:
            return true;

        case CircuitBreakerState::OPEN:
            if (canAttemptReset()) {
                // Try to transition to half-open
                const_cast<CircuitBreaker*>(this)->changeState(CircuitBreakerState::HALF_OPEN);
                return true;
            }
            ++requestsRejected_;
            return false;

        case CircuitBreakerState::HALF_OPEN:
            return true;
    }

    return false;
}

void CircuitBreaker::recordSuccess() {
    std::lock_guard<std::mutex> lock(stateMutex_);

    ++successCount_;
    ++totalRequests_;

    if (state_ == CircuitBreakerState::HALF_OPEN) {
        if (successCount_ >= successThreshold_) {
            changeState(CircuitBreakerState::CLOSED);
            failureCount_ = 0;
            successCount_ = 0;
        }
    }
}

void CircuitBreaker::recordFailure() {
    std::lock_guard<std::mutex> lock(stateMutex_);

    ++failureCount_;
    ++totalRequests_;
    lastFailureTime_ = std::chrono::system_clock::now();

    if (state_ == CircuitBreakerState::HALF_OPEN) {
        // Failed during half-open, go back to open
        changeState(CircuitBreakerState::OPEN);
    } else if (state_ == CircuitBreakerState::CLOSED && shouldTripCircuit()) {
        changeState(CircuitBreakerState::OPEN);
        ++circuitOpenCount_;
    }
}

CircuitBreakerState CircuitBreaker::getState() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return state_;
}

void CircuitBreaker::reset() {
    std::lock_guard<std::mutex> lock(stateMutex_);
    failureCount_ = 0;
    successCount_ = 0;
    totalRequests_ = 0;
    changeState(CircuitBreakerState::CLOSED);
}

void CircuitBreaker::changeState(CircuitBreakerState newState) {
    if (state_ != newState) {
        state_ = newState;
        stateChangeTime_ = std::chrono::system_clock::now();
    }
}

bool CircuitBreaker::shouldTripCircuit() const {
    if (failureCount_ >= failureThreshold_) {
        return true;
    }

    if (totalRequests_ >= 10) { // Minimum sample size
        double currentFailureRate = static_cast<double>(failureCount_) / totalRequests_;
        return currentFailureRate >= failureRateThreshold_;
    }

    return false;
}

bool CircuitBreaker::canAttemptReset() const {
    auto now = std::chrono::system_clock::now();
    return (now - stateChangeTime_) >= timeout_;
}

CircuitBreaker::BreakerStatistics CircuitBreaker::getStatistics() const {
    std::lock_guard<std::mutex> lock(stateMutex_);

    BreakerStatistics stats;
    stats.state = state_;
    stats.failureCount = failureCount_.load();
    stats.successCount = successCount_.load();
    stats.totalRequests = totalRequests_.load();
    stats.circuitOpenCount = circuitOpenCount_.load();
    stats.requestsRejected = requestsRejected_.load();
    stats.lastFailureTime = lastFailureTime_;
    stats.stateChangeTime = stateChangeTime_;

    if (stats.totalRequests > 0) {
        stats.failureRate = static_cast<double>(stats.failureCount) / stats.totalRequests;
    }

    return stats;
}

// ================================
// EnhancedHttpClient Implementation
// ================================

EnhancedHttpClient::EnhancedHttpClient(const AdvancedRetryConfig& retryConfig)
    : retryConfig_(retryConfig), defaultTimeout_(std::chrono::seconds(30)),
      userAgent_("CryptoClaude/1.0 Enhanced"), enableLogging_(false),
      enableConnectionPooling_(true), enableCircuitBreaker_(true),
      totalRequests_(0), successfulRequests_(0), failedRequests_(0),
      retriedRequests_(0), circuitBreakerRejections_(0) {

    connectionPool_ = std::make_unique<ConnectionPool>();
}

EnhancedHttpClient::~EnhancedHttpClient() {
    closeAllConnections();
}

void EnhancedHttpClient::setRetryConfig(const RetryConfig& config) {
    // Adapter to convert basic RetryConfig to AdvancedRetryConfig
    retryConfig_.maxRetries = config.maxRetries;
    retryConfig_.baseDelay = config.baseDelay;
    retryConfig_.backoffMultiplier = config.backoffMultiplier;
    retryConfig_.maxDelay = config.maxDelay;
    retryConfig_.retryOnTimeout = config.retryOnTimeout;
    retryConfig_.retryOnServerError = config.retryOnServerError;
    retryConfig_.retryOnClientError = config.retryOnClientError;
    retryConfig_.retryOnStatusCodes = config.retryOnStatusCodes;
}

void EnhancedHttpClient::setAdvancedRetryConfig(const AdvancedRetryConfig& config) {
    retryConfig_ = config;
}

HttpResponse EnhancedHttpClient::execute(const HttpRequest& request) {
    ++totalRequests_;

    if (enableLogging_) {
        logRequest(request);
    }

    // Check circuit breaker
    std::string host = extractHostFromUrl(request.getUrl());
    if (enableCircuitBreaker_) {
        CircuitBreaker* breaker = getCircuitBreaker(host);
        if (breaker && !breaker->allowRequest()) {
            ++circuitBreakerRejections_;
            updateStatistics(false, false, true);
            return HttpResponse(503, "Service temporarily unavailable (Circuit Breaker Open)");
        }
    }

    // Execute request with retry logic
    HttpResponse response = executeWithRetry(request);

    // Update circuit breaker based on result
    if (enableCircuitBreaker_) {
        CircuitBreaker* breaker = getCircuitBreaker(host);
        if (breaker) {
            if (response.isSuccess()) {
                breaker->recordSuccess();
            } else {
                breaker->recordFailure();
            }
        }
    }

    updateStatistics(response.isSuccess(), false, false);

    if (enableLogging_) {
        logResponse(response);
    }

    return response;
}

HttpResponse EnhancedHttpClient::executeWithRetry(const HttpRequest& request) {
    HttpResponse lastResponse(0, "");

    for (int attempt = 0; attempt <= retryConfig_.maxRetries; ++attempt) {
        if (attempt > 0) {
            // Calculate and apply retry delay
            auto delay = calculateRetryDelay(attempt);
            addJitter(delay);
            std::this_thread::sleep_for(delay);

            ++retriedRequests_;

            if (enableLogging_) {
                logRequest(request, attempt + 1);
            }
        }

        // Get connection from pool
        std::shared_ptr<HttpConnection> connection;
        if (enableConnectionPooling_) {
            std::string host = extractHostFromUrl(request.getUrl());
            auto [hostName, port] = parseHostAndPort(request.getUrl());
            bool https = isHttps(request.getUrl());
            connection = getConnection(hostName, port, https);
        }

        // Execute single request
        lastResponse = executeSingleRequest(request, connection);

        // Return connection to pool
        if (connection && enableConnectionPooling_) {
            returnConnection(connection);
        }

        // Check if we should retry
        if (lastResponse.isSuccess() || !shouldRetry(lastResponse, attempt + 1)) {
            break;
        }

        if (enableLogging_) {
            logResponse(lastResponse, attempt > 0);
        }
    }

    return lastResponse;
}

HttpResponse EnhancedHttpClient::executeSingleRequest(const HttpRequest& request,
                                                    std::shared_ptr<HttpConnection> connection) {
    // Simplified HTTP request execution
    // In production, this would use actual HTTP libraries (libcurl, WinINet, etc.)

    try {
        // Simulate network request
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Simulate different response scenarios for testing
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(1, 100);

        int random = dis(gen);

        if (random <= 85) {
            // 85% success rate
            return HttpResponse(200, R"({"status":"success","data":"mock_data"})");
        } else if (random <= 95) {
            // 10% server error (retryable)
            return HttpResponse(503, "Service temporarily unavailable");
        } else {
            // 5% client error (not retryable)
            return HttpResponse(400, "Bad request");
        }

    } catch (const std::exception& e) {
        return HttpResponse(0, "Connection error: " + std::string(e.what()));
    }
}

bool EnhancedHttpClient::shouldRetry(const HttpResponse& response, int attemptNumber) const {
    if (attemptNumber > retryConfig_.maxRetries) {
        return false;
    }

    int statusCode = response.getStatusCode();

    // Check no-retry status codes first
    if (std::find(retryConfig_.noRetryOnStatusCodes.begin(),
                  retryConfig_.noRetryOnStatusCodes.end(), statusCode)
        != retryConfig_.noRetryOnStatusCodes.end()) {
        return false;
    }

    // Check explicit retry status codes
    if (std::find(retryConfig_.retryOnStatusCodes.begin(),
                  retryConfig_.retryOnStatusCodes.end(), statusCode)
        != retryConfig_.retryOnStatusCodes.end()) {
        return true;
    }

    // Check general retry conditions
    if (statusCode == 0 && retryConfig_.retryOnConnectionError) {
        return true;
    }

    if (statusCode >= 500 && retryConfig_.retryOnServerError) {
        return true;
    }

    if (statusCode >= 400 && statusCode < 500 && retryConfig_.retryOnClientError) {
        return true;
    }

    // Custom retry condition
    if (retryConfig_.customRetryCondition) {
        return retryConfig_.customRetryCondition(response, attemptNumber);
    }

    return false;
}

std::chrono::milliseconds EnhancedHttpClient::calculateRetryDelay(int attemptNumber) const {
    if (retryConfig_.customDelayCalculation) {
        return retryConfig_.customDelayCalculation(attemptNumber, retryConfig_.baseDelay);
    }

    // Exponential backoff
    auto delay = retryConfig_.baseDelay;
    for (int i = 1; i < attemptNumber; ++i) {
        delay = std::chrono::milliseconds(static_cast<long long>(delay.count() * retryConfig_.backoffMultiplier));
    }

    return std::min(delay, retryConfig_.maxDelay);
}

void EnhancedHttpClient::addJitter(std::chrono::milliseconds& delay) const {
    if (retryConfig_.jitterFactor > 0.0) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(-retryConfig_.jitterFactor, retryConfig_.jitterFactor);

        double jitter = 1.0 + dis(gen);
        delay = std::chrono::milliseconds(static_cast<long long>(delay.count() * jitter));
    }
}

CircuitBreaker* EnhancedHttpClient::getCircuitBreaker(const std::string& host) {
    std::lock_guard<std::mutex> lock(breakersMutex_);

    auto it = circuitBreakers_.find(host);
    if (it == circuitBreakers_.end()) {
        circuitBreakers_[host] = std::make_unique<CircuitBreaker>();
        return circuitBreakers_[host].get();
    }

    return it->second.get();
}

std::string EnhancedHttpClient::extractHostFromUrl(const std::string& url) const {
    std::regex hostRegex(R"(https?://([^/]+))");
    std::smatch match;

    if (std::regex_search(url, match, hostRegex)) {
        return match[1].str();
    }

    return "unknown";
}

std::shared_ptr<HttpConnection> EnhancedHttpClient::getConnection(const std::string& host, int port, bool isHttps) {
    if (connectionPool_) {
        return connectionPool_->getConnection(host, port, isHttps);
    }
    return nullptr;
}

void EnhancedHttpClient::returnConnection(std::shared_ptr<HttpConnection> connection) {
    if (connectionPool_ && connection) {
        connectionPool_->returnConnection(connection);
    }
}

std::pair<std::string, int> EnhancedHttpClient::parseHostAndPort(const std::string& url) const {
    std::regex hostPortRegex(R"(https?://([^:/]+)(?::(\d+))?)");
    std::smatch match;

    if (std::regex_search(url, match, hostPortRegex)) {
        std::string host = match[1].str();
        int port = 443; // Default HTTPS port

        if (match[2].matched) {
            port = std::stoi(match[2].str());
        } else if (url.find("http://") == 0) {
            port = 80; // Default HTTP port
        }

        return {host, port};
    }

    return {"unknown", 443};
}

bool EnhancedHttpClient::isHttps(const std::string& url) const {
    return url.find("https://") == 0;
}

EnhancedHttpClient::EnhancedClientStatistics EnhancedHttpClient::getStatistics() const {
    EnhancedClientStatistics stats;

    stats.totalRequests = totalRequests_.load();
    stats.successfulRequests = successfulRequests_.load();
    stats.failedRequests = failedRequests_.load();
    stats.retriedRequests = retriedRequests_.load();
    stats.circuitBreakerRejections = circuitBreakerRejections_.load();

    if (stats.totalRequests > 0) {
        stats.successRate = static_cast<double>(stats.successfulRequests) / stats.totalRequests;
        stats.retryRate = static_cast<double>(stats.retriedRequests) / stats.totalRequests;
    }

    if (connectionPool_) {
        stats.connectionPoolStats = connectionPool_->getStatistics();
    }

    // Get circuit breaker stats
    std::lock_guard<std::mutex> lock(breakersMutex_);
    for (const auto& [host, breaker] : circuitBreakers_) {
        stats.circuitBreakerStats[host] = breaker->getStatistics();
    }

    return stats;
}

void EnhancedHttpClient::updateStatistics(bool success, bool wasRetried, bool circuitBreakerRejection) const {
    if (success) {
        ++successfulRequests_;
    } else {
        ++failedRequests_;
    }
}

void EnhancedHttpClient::logRequest(const HttpRequest& request, int attempt) const {
    if (enableLogging_) {
        std::string attemptStr = attempt > 1 ? " (Attempt " + std::to_string(attempt) + ")" : "";
        std::cout << "[EnhancedHttpClient] " << request.buildUrl() << attemptStr << std::endl;
    }
}

void EnhancedHttpClient::logResponse(const HttpResponse& response, bool isRetry) const {
    if (enableLogging_) {
        std::string retryStr = isRetry ? " (Retry)" : "";
        std::cout << "[EnhancedHttpClient] Response: " << response.getStatusCode() << retryStr << std::endl;
    }
}

void EnhancedHttpClient::closeAllConnections() {
    if (connectionPool_) {
        connectionPool_->closeAllConnections();
    }
}

void EnhancedHttpClient::performMaintenanceCheck() {
    if (connectionPool_) {
        connectionPool_->cleanupExpiredConnections();
        connectionPool_->performHealthChecks();
    }
}

void EnhancedHttpClient::resetCircuitBreaker(const std::string& host) {
    std::lock_guard<std::mutex> lock(breakersMutex_);
    auto it = circuitBreakers_.find(host);
    if (it != circuitBreakers_.end()) {
        it->second->reset();
    }
}

void EnhancedHttpClient::resetAllCircuitBreakers() {
    std::lock_guard<std::mutex> lock(breakersMutex_);
    for (auto& [host, breaker] : circuitBreakers_) {
        breaker->reset();
    }
}

CircuitBreakerState EnhancedHttpClient::getCircuitBreakerState(const std::string& host) const {
    std::lock_guard<std::mutex> lock(breakersMutex_);
    auto it = circuitBreakers_.find(host);
    if (it != circuitBreakers_.end()) {
        return it->second->getState();
    }
    return CircuitBreakerState::CLOSED;
}

void EnhancedHttpClient::warmupConnectionPool(const std::vector<std::string>& hosts) {
    if (!enableConnectionPooling_ || !connectionPool_) return;

    for (const auto& host : hosts) {
        try {
            auto [hostname, port] = parseHostAndPort("https://" + host);
            auto connection = connectionPool_->getConnection(hostname, port, true);
            if (connection) {
                connectionPool_->returnConnection(connection);
            }
        } catch (const std::exception& e) {
            if (enableLogging_) {
                std::cout << "[EnhancedHttpClient] Failed to warmup " << host << ": " << e.what() << std::endl;
            }
        }
    }
}

void EnhancedHttpClient::resetStatistics() {
    totalRequests_ = 0;
    successfulRequests_ = 0;
    failedRequests_ = 0;
    retriedRequests_ = 0;
    circuitBreakerRejections_ = 0;

    if (connectionPool_) {
        connectionPool_->resetStatistics();
    }
}

EnhancedHttpClient::HealthStatus EnhancedHttpClient::getHealthStatus() const {
    HealthStatus status;
    status.isHealthy = true;

    // Check connection pool health
    if (connectionPool_) {
        status.poolHealth = connectionPool_->getStatistics();
        if (status.poolHealth.hitRate < 0.3 && status.poolHealth.totalConnections > 5) {
            status.isHealthy = false;
            status.issues.push_back("Poor connection pool performance (hit rate: " +
                                  std::to_string(status.poolHealth.hitRate) + ")");
        }
    }

    // Check circuit breakers
    std::lock_guard<std::mutex> lock(breakersMutex_);
    for (const auto& [host, breaker] : circuitBreakers_) {
        CircuitBreakerState state = breaker->getState();
        status.hostStates[host] = state;

        if (state == CircuitBreakerState::OPEN) {
            status.isHealthy = false;
            status.issues.push_back("Circuit breaker open for host: " + host);
        }
    }

    return status;
}

bool EnhancedHttpClient::isHealthy() const {
    return getHealthStatus().isHealthy;
}

// Factory implementations
std::unique_ptr<EnhancedHttpClient> EnhancedHttpClientFactory::createDefault() {
    return std::make_unique<EnhancedHttpClient>();
}

std::unique_ptr<EnhancedHttpClient> EnhancedHttpClientFactory::createHighPerformance() {
    AdvancedRetryConfig config;
    config.maxRetries = 2; // Fewer retries for speed
    config.baseDelay = std::chrono::milliseconds(500); // Shorter delays
    return std::make_unique<EnhancedHttpClient>(config);
}

std::unique_ptr<EnhancedHttpClient> EnhancedHttpClientFactory::createResilient() {
    AdvancedRetryConfig config;
    config.maxRetries = 5; // More retries for reliability
    config.baseDelay = std::chrono::milliseconds(2000); // Longer delays
    config.jitterFactor = 0.2; // More jitter to prevent thundering herd
    return std::make_unique<EnhancedHttpClient>(config);
}

} // namespace Http
} // namespace CryptoClaude