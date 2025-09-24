#pragma once

#include "HttpClient.h"
#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <map>
#include <chrono>
#include <functional>

namespace CryptoClaude {
namespace Http {

// Connection pool for HTTP connections
class HttpConnection {
private:
    std::string host_;
    int port_;
    bool isHttps_;
    std::chrono::system_clock::time_point lastUsed_;
    std::chrono::system_clock::time_point created_;
    std::atomic<bool> inUse_;
    int requestCount_;

    // Connection health
    bool isHealthy_;
    std::chrono::system_clock::time_point lastHealthCheck_;

public:
    HttpConnection(const std::string& host, int port = 443, bool isHttps = true);
    ~HttpConnection();

    // Connection management
    bool isAvailable() const;
    bool isExpired(std::chrono::minutes maxAge = std::chrono::minutes(5)) const;
    bool needsHealthCheck(std::chrono::minutes interval = std::chrono::minutes(1)) const;

    void markInUse();
    void markAvailable();
    void updateLastUsed();
    void incrementRequestCount();

    // Health check
    bool performHealthCheck();
    bool isHealthy() const { return isHealthy_; }

    // Getters
    const std::string& getHost() const { return host_; }
    int getPort() const { return port_; }
    bool isHttps() const { return isHttps_; }
    int getRequestCount() const { return requestCount_; }
    std::chrono::system_clock::time_point getLastUsed() const { return lastUsed_; }
};

// Connection pool implementation
class ConnectionPool {
private:
    mutable std::mutex poolMutex_;
    std::map<std::string, std::queue<std::shared_ptr<HttpConnection>>> connections_;
    std::atomic<int> totalConnections_;
    std::atomic<int> activeConnections_;

    // Configuration
    int maxConnectionsPerHost_;
    int maxTotalConnections_;
    std::chrono::minutes connectionMaxAge_;
    std::chrono::minutes healthCheckInterval_;

    // Statistics
    mutable std::atomic<int> connectionHits_;
    mutable std::atomic<int> connectionMisses_;
    mutable std::atomic<int> connectionsCreated_;
    mutable std::atomic<int> connectionsDestroyed_;

public:
    explicit ConnectionPool(int maxPerHost = 10, int maxTotal = 100);
    ~ConnectionPool();

    // Connection management
    std::shared_ptr<HttpConnection> getConnection(const std::string& host, int port = 443, bool isHttps = true);
    void returnConnection(std::shared_ptr<HttpConnection> connection);

    // Maintenance
    void cleanupExpiredConnections();
    void performHealthChecks();
    void closeAllConnections();

    // Statistics
    struct PoolStatistics {
        int totalConnections = 0;
        int activeConnections = 0;
        int connectionHits = 0;
        int connectionMisses = 0;
        int connectionsCreated = 0;
        int connectionsDestroyed = 0;
        double hitRate = 0.0;
    };

    PoolStatistics getStatistics() const;
    void resetStatistics();

private:
    std::string createConnectionKey(const std::string& host, int port, bool isHttps) const;
    void cleanupExpiredConnectionsForHost(const std::string& key);
};

// Circuit breaker states
enum class CircuitBreakerState {
    CLOSED,     // Normal operation
    OPEN,       // Failing, reject requests
    HALF_OPEN   // Testing if service recovered
};

// Circuit breaker implementation
class CircuitBreaker {
private:
    mutable std::mutex stateMutex_;
    CircuitBreakerState state_;

    // Failure tracking
    std::atomic<int> failureCount_;
    std::atomic<int> successCount_;
    std::atomic<int> totalRequests_;
    std::chrono::system_clock::time_point lastFailureTime_;
    std::chrono::system_clock::time_point stateChangeTime_;

    // Configuration
    int failureThreshold_;              // Failures needed to open circuit
    std::chrono::seconds timeout_;      // Time to wait before trying again
    int successThreshold_;              // Successes needed to close circuit (half-open -> closed)
    double failureRateThreshold_;       // Failure rate to open circuit

    // Statistics
    mutable std::atomic<int> circuitOpenCount_;
    mutable std::atomic<int> requestsRejected_;

public:
    explicit CircuitBreaker(int failureThreshold = 5,
                          std::chrono::seconds timeout = std::chrono::seconds(60),
                          int successThreshold = 3,
                          double failureRateThreshold = 0.5);

    // Circuit breaker operations
    bool allowRequest() const;
    void recordSuccess();
    void recordFailure();
    void reset();

    // State management
    CircuitBreakerState getState() const;
    bool isOpen() const { return getState() == CircuitBreakerState::OPEN; }
    bool isClosed() const { return getState() == CircuitBreakerState::CLOSED; }
    bool isHalfOpen() const { return getState() == CircuitBreakerState::HALF_OPEN; }

    // Statistics
    struct BreakerStatistics {
        CircuitBreakerState state;
        int failureCount = 0;
        int successCount = 0;
        int totalRequests = 0;
        double failureRate = 0.0;
        int circuitOpenCount = 0;
        int requestsRejected = 0;
        std::chrono::system_clock::time_point lastFailureTime;
        std::chrono::system_clock::time_point stateChangeTime;
    };

    BreakerStatistics getStatistics() const;

private:
    void changeState(CircuitBreakerState newState);
    bool shouldTripCircuit() const;
    bool canAttemptReset() const;
};

// Enhanced retry configuration with exponential backoff and jitter
struct AdvancedRetryConfig {
    int maxRetries = 3;
    std::chrono::milliseconds baseDelay = std::chrono::milliseconds(1000);
    double backoffMultiplier = 2.0;
    std::chrono::milliseconds maxDelay = std::chrono::milliseconds(30000);
    double jitterFactor = 0.1;  // Add randomness to prevent thundering herd

    // Retry conditions
    bool retryOnTimeout = true;
    bool retryOnServerError = true;
    bool retryOnClientError = false;
    bool retryOnConnectionError = true;
    std::vector<int> retryOnStatusCodes = {429, 502, 503, 504};
    std::vector<int> noRetryOnStatusCodes = {400, 401, 403, 404};

    // Advanced retry logic
    std::function<bool(const HttpResponse&, int)> customRetryCondition;
    std::function<std::chrono::milliseconds(int, std::chrono::milliseconds)> customDelayCalculation;
};

// Enhanced HTTP client with connection pooling and circuit breaker
class EnhancedHttpClient : public IHttpClient {
private:
    std::unique_ptr<ConnectionPool> connectionPool_;
    std::map<std::string, std::unique_ptr<CircuitBreaker>> circuitBreakers_; // Per-host circuit breakers
    mutable std::mutex breakersMutex_;

    AdvancedRetryConfig retryConfig_;
    std::chrono::seconds defaultTimeout_;
    std::string userAgent_;
    bool enableLogging_;
    bool enableConnectionPooling_;
    bool enableCircuitBreaker_;

    // Statistics
    mutable std::atomic<int> totalRequests_;
    mutable std::atomic<int> successfulRequests_;
    mutable std::atomic<int> failedRequests_;
    mutable std::atomic<int> retriedRequests_;
    mutable std::atomic<int> circuitBreakerRejections_;

public:
    explicit EnhancedHttpClient(const AdvancedRetryConfig& retryConfig = AdvancedRetryConfig{});
    virtual ~EnhancedHttpClient();

    // Configuration
    void setRetryConfig(const RetryConfig& config) override; // Adapter for base interface
    void setAdvancedRetryConfig(const AdvancedRetryConfig& config);
    void setDefaultTimeout(std::chrono::seconds timeout) override { defaultTimeout_ = timeout; }
    void setUserAgent(const std::string& userAgent) override { userAgent_ = userAgent; }
    void enableLogging(bool enable) { enableLogging_ = enable; }
    void enableConnectionPooling(bool enable) { enableConnectionPooling_ = enable; }
    void enableCircuitBreaker(bool enable) { enableCircuitBreaker_ = enable; }

    // Main execution method with enhanced features
    HttpResponse execute(const HttpRequest& request) override;

    // Convenience methods (inherited from base)
    HttpResponse get(const std::string& url, const std::map<std::string, std::string>& headers = {});
    HttpResponse post(const std::string& url, const std::string& body,
                     const std::map<std::string, std::string>& headers = {});

    // Connection pool management
    void warmupConnectionPool(const std::vector<std::string>& hosts);
    void closeAllConnections();
    void performMaintenanceCheck();

    // Circuit breaker management
    void resetCircuitBreaker(const std::string& host);
    void resetAllCircuitBreakers();
    CircuitBreakerState getCircuitBreakerState(const std::string& host) const;

    // Statistics and monitoring
    struct EnhancedClientStatistics {
        int totalRequests = 0;
        int successfulRequests = 0;
        int failedRequests = 0;
        int retriedRequests = 0;
        int circuitBreakerRejections = 0;
        double successRate = 0.0;
        double retryRate = 0.0;

        ConnectionPool::PoolStatistics connectionPoolStats;
        std::map<std::string, CircuitBreaker::BreakerStatistics> circuitBreakerStats;
    };

    EnhancedClientStatistics getStatistics() const;
    void resetStatistics();

    // Health monitoring
    struct HealthStatus {
        bool isHealthy = true;
        std::vector<std::string> issues;
        std::map<std::string, CircuitBreakerState> hostStates;
        ConnectionPool::PoolStatistics poolHealth;
    };

    HealthStatus getHealthStatus() const;
    bool isHealthy() const;

private:
    // Core request execution
    HttpResponse executeWithRetry(const HttpRequest& request);
    HttpResponse executeSingleRequest(const HttpRequest& request, std::shared_ptr<HttpConnection> connection = nullptr);

    // Circuit breaker management
    CircuitBreaker* getCircuitBreaker(const std::string& host);
    std::string extractHostFromUrl(const std::string& url) const;

    // Retry logic
    bool shouldRetry(const HttpResponse& response, int attemptNumber) const;
    std::chrono::milliseconds calculateRetryDelay(int attemptNumber) const;
    void addJitter(std::chrono::milliseconds& delay) const;

    // Connection management
    std::shared_ptr<HttpConnection> getConnection(const std::string& host, int port, bool isHttps);
    void returnConnection(std::shared_ptr<HttpConnection> connection);

    // Logging and monitoring
    void logRequest(const HttpRequest& request, int attempt = 1) const;
    void logResponse(const HttpResponse& response, bool isRetry = false) const;
    void updateStatistics(bool success, bool wasRetried, bool circuitBreakerRejection) const;

    // Utility methods
    std::pair<std::string, int> parseHostAndPort(const std::string& url) const;
    bool isHttps(const std::string& url) const;
};

// Factory for creating enhanced HTTP clients
class EnhancedHttpClientFactory {
public:
    static std::unique_ptr<EnhancedHttpClient> createDefault();
    static std::unique_ptr<EnhancedHttpClient> createHighPerformance(); // Larger connection pools, aggressive retry
    static std::unique_ptr<EnhancedHttpClient> createResilient();        // Conservative settings, high reliability
    static std::unique_ptr<EnhancedHttpClient> createForTesting();       // Mock implementation for tests
};

} // namespace Http
} // namespace CryptoClaude