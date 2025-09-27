#pragma once

#include "../Config/ApiConfigurationManager.h"
#include "../DataIngestion/IDataProvider.h"
#include "../DataIngestion/CryptoCompareProvider.h"
#include "../DataIngestion/NewsApiProvider.h"
#include "../Utils/HttpClient.h"
#include "../Security/EncryptionManager.h"
#include <memory>
#include <map>
#include <mutex>
#include <atomic>
#include <future>
#include <queue>
#include <chrono>

namespace CryptoClaude {
namespace Data {

using namespace Config;
using namespace DataIngestion;

/**
 * Production-grade API Manager for secure API key management,
 * rate limiting, failover, and real-time connectivity monitoring
 */
class ProductionApiManager {
public:
    explicit ProductionApiManager(ApiConfigurationManager& configManager);
    ~ProductionApiManager();

    // === INITIALIZATION AND LIFECYCLE ===

    bool initialize();
    void shutdown();
    bool isInitialized() const { return initialized_; }

    // === API PROVIDER MANAGEMENT ===

    // Provider registration and lifecycle
    bool registerApiProvider(const std::string& providerId,
                           std::unique_ptr<IDataProvider> provider);
    bool unregisterApiProvider(const std::string& providerId);

    std::vector<std::string> getRegisteredProviders() const;
    std::vector<std::string> getActiveProviders() const;
    bool isProviderActive(const std::string& providerId) const;

    // Provider health and connectivity
    struct ProviderHealth {
        std::string providerId;
        bool isConnected;
        bool isHealthy;
        double latencyMs;
        double successRate; // Last 100 requests
        std::chrono::system_clock::time_point lastHealthCheck;
        std::string lastError;
        int consecutiveFailures;
    };

    ProviderHealth getProviderHealth(const std::string& providerId) const;
    std::map<std::string, ProviderHealth> getAllProviderHealth() const;

    // Manual health checks
    bool performHealthCheck(const std::string& providerId);
    void performAllHealthChecks();

    // === SECURE API KEY MANAGEMENT ===

    // Encrypted API key storage
    bool setApiKey(const std::string& providerId, const std::string& apiKey);
    bool setApiCredentials(const std::string& providerId,
                          const std::string& apiKey,
                          const std::string& apiSecret = "",
                          const std::string& passphrase = "");

    std::string getApiKey(const std::string& providerId) const;
    bool hasValidApiKey(const std::string& providerId) const;

    // Key validation and testing
    bool validateApiKey(const std::string& providerId);
    std::map<std::string, bool> validateAllApiKeys();

    // === RATE LIMITING AND THROTTLING ===

    struct RateLimitStatus {
        int currentRequests;
        int maxRequestsPerSecond;
        int maxRequestsPerMinute;
        int remainingRequests;
        std::chrono::system_clock::time_point resetTime;
        bool isThrottled;
        std::chrono::milliseconds nextAllowedRequest;
    };

    RateLimitStatus getRateLimitStatus(const std::string& providerId) const;
    bool canMakeRequest(const std::string& providerId) const;

    // Request throttling
    std::future<bool> scheduleRequest(const std::string& providerId,
                                    std::function<bool()> requestFunction);

    // === BASIC CONNECTIVITY TESTING ===

    struct ConnectivityTest {
        std::string providerId;
        bool connectionSuccessful;
        double latencyMs;
        int httpStatusCode;
        std::string responseTime;
        std::string errorMessage;
        std::chrono::system_clock::time_point testTime;
    };

    ConnectivityTest testBasicConnectivity(const std::string& providerId);
    std::map<std::string, ConnectivityTest> testAllConnectivity();

    // Continuous connectivity monitoring
    void enableContinuousMonitoring(bool enable = true);
    bool isContinuousMonitoringEnabled() const { return continuousMonitoring_; }

    // === DATA PROVIDER ACCESS ===

    // Get provider instances (with automatic failover)
    template<typename T>
    std::shared_ptr<T> getProvider(const std::string& providerId) const;

    std::shared_ptr<IDataProvider> getDataProvider(const std::string& providerId) const;
    std::shared_ptr<IMarketDataProvider> getMarketDataProvider(const std::string& providerId) const;
    std::shared_ptr<INewsProvider> getNewsProvider(const std::string& providerId) const;

    // Provider selection with failover
    std::string selectBestProvider(const std::string& providerType = "") const;
    std::vector<std::string> getProvidersByPriority(const std::string& providerType = "") const;

    // === FAILOVER AND REDUNDANCY ===

    struct FailoverConfig {
        bool enableFailover = true;
        int maxFailoverAttempts = 3;
        std::chrono::minutes failoverCooldown = std::chrono::minutes(5);
        double healthThreshold = 0.80; // Minimum success rate for health
        int maxConsecutiveFailures = 5;
    };

    void setFailoverConfig(const FailoverConfig& config);
    FailoverConfig getFailoverConfig() const { return failoverConfig_; }

    bool triggerFailover(const std::string& failingProviderId);
    std::string getFailoverProvider(const std::string& originalProviderId) const;

    // === REQUEST EXECUTION WITH ERROR HANDLING ===

    struct RequestOptions {
        int maxRetries = 3;
        std::chrono::milliseconds timeout = std::chrono::milliseconds(30000);
        bool enableFailover = true;
        bool respectRateLimit = true;
        std::chrono::milliseconds retryDelay = std::chrono::milliseconds(1000);
        double retryBackoffMultiplier = 2.0;
    };

    template<typename T>
    struct RequestResult {
        bool success;
        T data;
        std::string providerId; // Which provider was actually used
        std::string errorMessage;
        double latencyMs;
        int httpStatusCode;
        std::chrono::system_clock::time_point requestTime;
    };

    // Execute request with automatic retries and failover
    template<typename T>
    RequestResult<T> executeRequest(
        const std::string& preferredProviderId,
        std::function<T(std::shared_ptr<IDataProvider>)> requestFunction,
        const RequestOptions& options = RequestOptions{}
    );

    // === PERFORMANCE MONITORING ===

    struct PerformanceMetrics {
        std::string providerId;
        int totalRequests;
        int successfulRequests;
        int failedRequests;
        double averageLatencyMs;
        double maxLatencyMs;
        double minLatencyMs;
        double successRate;
        std::chrono::system_clock::time_point lastRequest;
        std::map<int, int> httpStatusCodes; // Status code -> count
    };

    PerformanceMetrics getPerformanceMetrics(const std::string& providerId) const;
    std::map<std::string, PerformanceMetrics> getAllPerformanceMetrics() const;

    void resetPerformanceMetrics(const std::string& providerId = "");

    // === CONFIGURATION MANAGEMENT ===

    // Runtime configuration updates
    bool updateProviderConfig(const std::string& providerId, const ApiProviderConfig& config);
    ApiProviderConfig getProviderConfig(const std::string& providerId) const;

    // Global API settings
    void setGlobalTimeout(std::chrono::milliseconds timeout);
    void setGlobalRateLimit(int maxRequestsPerSecond);
    void enableGlobalRetries(bool enable);

    // === DIAGNOSTICS AND TROUBLESHOOTING ===

    struct SystemDiagnostics {
        bool overallHealth;
        int totalProviders;
        int healthyProviders;
        int activeProviders;
        double systemLatencyMs;
        double systemSuccessRate;
        std::vector<std::string> warnings;
        std::vector<std::string> errors;
        std::chrono::system_clock::time_point diagnosticsTime;
    };

    SystemDiagnostics performSystemDiagnostics();

    // Detailed provider diagnostics
    std::string generateDiagnosticsReport() const;
    bool exportDiagnosticsToFile(const std::string& filePath) const;

    // === EVENT CALLBACKS ===

    using ProviderEventCallback = std::function<void(const std::string&, const std::string&)>;

    void setProviderHealthCallback(ProviderEventCallback callback) {
        healthCallback_ = callback;
    }
    void setProviderFailureCallback(ProviderEventCallback callback) {
        failureCallback_ = callback;
    }

private:
    // Core dependencies
    ApiConfigurationManager& configManager_;
    std::unique_ptr<Security::EncryptionManager> encryptionManager_;

    // Provider management
    std::map<std::string, std::unique_ptr<IDataProvider>> providers_;
    mutable std::map<std::string, std::shared_ptr<IDataProvider>> providerCache_;
    mutable std::mutex providerMutex_;

    // Health monitoring
    mutable std::map<std::string, ProviderHealth> healthStatus_;
    mutable std::map<std::string, PerformanceMetrics> performanceMetrics_;
    mutable std::mutex healthMutex_;

    // Rate limiting
    struct RateLimiter {
        std::queue<std::chrono::system_clock::time_point> requests;
        std::mutex mutex;
        int maxRequestsPerSecond;
        int maxRequestsPerMinute;
        std::chrono::system_clock::time_point lastCleanup;
    };

    mutable std::map<std::string, std::unique_ptr<RateLimiter>> rateLimiters_;
    mutable std::mutex rateLimitMutex_;

    // Configuration and state
    std::atomic<bool> initialized_;
    std::atomic<bool> continuousMonitoring_;
    FailoverConfig failoverConfig_;

    // Background tasks
    std::unique_ptr<std::thread> monitoringThread_;
    std::atomic<bool> shutdownRequested_;

    // Event callbacks
    ProviderEventCallback healthCallback_;
    ProviderEventCallback failureCallback_;

    // === PRIVATE HELPER METHODS ===

    // Initialization helpers
    bool initializeEncryption();
    bool initializeProviders();
    bool initializeRateLimiters();
    bool initializeMonitoring();

    // Provider management helpers
    bool validateProviderConfiguration(const std::string& providerId);
    void updateProviderHealth(const std::string& providerId, bool success,
                            double latencyMs, const std::string& error = "");

    // Rate limiting implementation
    bool checkRateLimit(const std::string& providerId) const;
    void recordRequest(const std::string& providerId) const;
    void cleanupRateLimit(const std::string& providerId) const;

    // Health monitoring
    void performContinuousHealthChecks();
    bool performSingleHealthCheck(const std::string& providerId);
    void notifyHealthChange(const std::string& providerId, bool healthy);

    // Request execution helpers
    template<typename T>
    RequestResult<T> executeWithRetry(
        const std::string& providerId,
        std::function<T(std::shared_ptr<IDataProvider>)> requestFunction,
        const RequestOptions& options
    );

    // Performance tracking
    void recordRequestMetrics(const std::string& providerId, bool success,
                            double latencyMs, int httpStatusCode);

    // Failover implementation
    void markProviderFailed(const std::string& providerId);
    void markProviderRecovered(const std::string& providerId);
    std::vector<std::string> getFailoverCandidates(const std::string& originalProviderId) const;

    // Encryption helpers
    std::string encryptApiKey(const std::string& apiKey) const;
    std::string decryptApiKey(const std::string& encryptedKey) const;

    // Diagnostics helpers
    std::vector<std::string> validateConfiguration() const;
    std::vector<std::string> checkSystemHealth() const;
};

// === TEMPLATE IMPLEMENTATIONS ===

template<typename T>
std::shared_ptr<T> ProductionApiManager::getProvider(const std::string& providerId) const {
    std::lock_guard<std::mutex> lock(providerMutex_);

    auto it = providers_.find(providerId);
    if (it != providers_.end()) {
        return std::dynamic_pointer_cast<T>(it->second);
    }

    return nullptr;
}

template<typename T>
ProductionApiManager::RequestResult<T> ProductionApiManager::executeRequest(
    const std::string& preferredProviderId,
    std::function<T(std::shared_ptr<IDataProvider>)> requestFunction,
    const RequestOptions& options) {

    auto startTime = std::chrono::high_resolution_clock::now();
    RequestResult<T> result;
    result.requestTime = std::chrono::system_clock::now();
    result.success = false;

    std::vector<std::string> providersToTry;
    providersToTry.push_back(preferredProviderId);

    if (options.enableFailover) {
        auto failoverProviders = getFailoverCandidates(preferredProviderId);
        providersToTry.insert(providersToTry.end(),
                            failoverProviders.begin(), failoverProviders.end());
    }

    for (const auto& providerId : providersToTry) {
        if (!isProviderActive(providerId)) {
            continue;
        }

        if (options.respectRateLimit && !canMakeRequest(providerId)) {
            // Wait for rate limit if this is the preferred provider
            if (providerId == preferredProviderId) {
                auto rateLimitStatus = getRateLimitStatus(providerId);
                std::this_thread::sleep_for(rateLimitStatus.nextAllowedRequest);
            } else {
                continue; // Skip to next provider
            }
        }

        try {
            result = executeWithRetry(providerId, requestFunction, options);
            if (result.success) {
                result.providerId = providerId;
                break;
            }
        } catch (const std::exception& e) {
            result.errorMessage = e.what();
            updateProviderHealth(providerId, false, 0.0, e.what());
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    result.latencyMs = std::chrono::duration<double, std::milli>(
        endTime - startTime).count();

    return result;
}

template<typename T>
ProductionApiManager::RequestResult<T> ProductionApiManager::executeWithRetry(
    const std::string& providerId,
    std::function<T(std::shared_ptr<IDataProvider>)> requestFunction,
    const RequestOptions& options) {

    RequestResult<T> result;
    result.providerId = providerId;
    result.requestTime = std::chrono::system_clock::now();

    auto provider = getDataProvider(providerId);
    if (!provider) {
        result.success = false;
        result.errorMessage = "Provider not found: " + providerId;
        return result;
    }

    std::chrono::milliseconds currentDelay = options.retryDelay;

    for (int attempt = 0; attempt <= options.maxRetries; ++attempt) {
        auto startTime = std::chrono::high_resolution_clock::now();

        try {
            recordRequest(providerId);
            result.data = requestFunction(provider);
            result.success = true;

            auto endTime = std::chrono::high_resolution_clock::now();
            result.latencyMs = std::chrono::duration<double, std::milli>(
                endTime - startTime).count();

            recordRequestMetrics(providerId, true, result.latencyMs, 200);
            updateProviderHealth(providerId, true, result.latencyMs);

            break;

        } catch (const std::exception& e) {
            result.errorMessage = e.what();
            result.success = false;

            auto endTime = std::chrono::high_resolution_clock::now();
            result.latencyMs = std::chrono::duration<double, std::milli>(
                endTime - startTime).count();

            recordRequestMetrics(providerId, false, result.latencyMs, 500);
            updateProviderHealth(providerId, false, result.latencyMs, e.what());

            if (attempt < options.maxRetries) {
                std::this_thread::sleep_for(currentDelay);
                currentDelay = std::chrono::milliseconds(
                    static_cast<int>(currentDelay.count() * options.retryBackoffMultiplier));
            }
        }
    }

    return result;
}

} // namespace Data
} // namespace CryptoClaude