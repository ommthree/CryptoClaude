#include "ProductionApiManager.h"
#include "../Security/EncryptionManager.h"
#include "../Utils/Logger.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <thread>
#include <future>

namespace CryptoClaude {
namespace Data {

ProductionApiManager::ProductionApiManager(ApiConfigurationManager& configManager)
    : configManager_(configManager),
      initialized_(false),
      continuousMonitoring_(false),
      shutdownRequested_(false) {

    // Initialize default failover configuration
    failoverConfig_.enableFailover = true;
    failoverConfig_.maxFailoverAttempts = 3;
    failoverConfig_.failoverCooldown = std::chrono::minutes(5);
    failoverConfig_.healthThreshold = 0.80;
    failoverConfig_.maxConsecutiveFailures = 5;
}

ProductionApiManager::~ProductionApiManager() {
    shutdown();
}

// === INITIALIZATION AND LIFECYCLE ===

bool ProductionApiManager::initialize() {
    if (initialized_) {
        return true;
    }

    try {
        // Initialize encryption manager
        if (!initializeEncryption()) {
            Utils::Logger::error("Failed to initialize encryption manager");
            return false;
        }

        // Initialize providers from configuration
        if (!initializeProviders()) {
            Utils::Logger::error("Failed to initialize API providers");
            return false;
        }

        // Initialize rate limiters
        if (!initializeRateLimiters()) {
            Utils::Logger::error("Failed to initialize rate limiters");
            return false;
        }

        // Start monitoring if enabled
        if (!initializeMonitoring()) {
            Utils::Logger::error("Failed to initialize monitoring");
            return false;
        }

        initialized_ = true;
        Utils::Logger::info("ProductionApiManager initialized successfully");
        return true;

    } catch (const std::exception& e) {
        Utils::Logger::error("Exception during ProductionApiManager initialization: " + std::string(e.what()));
        return false;
    }
}

void ProductionApiManager::shutdown() {
    if (!initialized_) {
        return;
    }

    shutdownRequested_ = true;

    // Stop continuous monitoring
    if (monitoringThread_ && monitoringThread_->joinable()) {
        monitoringThread_->join();
    }

    // Clear providers
    {
        std::lock_guard<std::mutex> lock(providerMutex_);
        providers_.clear();
        providerCache_.clear();
    }

    // Clear rate limiters
    {
        std::lock_guard<std::mutex> lock(rateLimitMutex_);
        rateLimiters_.clear();
    }

    // Clear health status
    {
        std::lock_guard<std::mutex> lock(healthMutex_);
        healthStatus_.clear();
        performanceMetrics_.clear();
    }

    initialized_ = false;
    Utils::Logger::info("ProductionApiManager shut down successfully");
}

// === API PROVIDER MANAGEMENT ===

bool ProductionApiManager::registerApiProvider(const std::string& providerId,
                                              std::unique_ptr<IDataProvider> provider) {
    if (!initialized_) {
        Utils::Logger::error("ProductionApiManager not initialized");
        return false;
    }

    if (!provider) {
        Utils::Logger::error("Cannot register null provider: " + providerId);
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(providerMutex_);
        providers_[providerId] = std::move(provider);
        providerCache_.clear(); // Clear cache to force refresh
    }

    // Initialize health status
    {
        std::lock_guard<std::mutex> lock(healthMutex_);
        ProviderHealth health;
        health.providerId = providerId;
        health.isConnected = false;
        health.isHealthy = false;
        health.latencyMs = 0.0;
        health.successRate = 0.0;
        health.lastHealthCheck = std::chrono::system_clock::now();
        health.consecutiveFailures = 0;
        healthStatus_[providerId] = health;

        // Initialize performance metrics
        PerformanceMetrics metrics;
        metrics.providerId = providerId;
        metrics.totalRequests = 0;
        metrics.successfulRequests = 0;
        metrics.failedRequests = 0;
        metrics.averageLatencyMs = 0.0;
        metrics.maxLatencyMs = 0.0;
        metrics.minLatencyMs = std::numeric_limits<double>::max();
        metrics.successRate = 0.0;
        performanceMetrics_[providerId] = metrics;
    }

    // Initialize rate limiter
    {
        std::lock_guard<std::mutex> lock(rateLimitMutex_);
        auto rateLimiter = std::make_unique<RateLimiter>();
        auto config = configManager_.getApiProviderConfig(providerId);
        rateLimiter->maxRequestsPerSecond = config.maxRequestsPerSecond;
        rateLimiter->maxRequestsPerMinute = config.maxRequestsPerMinute;
        rateLimiter->lastCleanup = std::chrono::system_clock::now();
        rateLimiters_[providerId] = std::move(rateLimiter);
    }

    Utils::Logger::info("Registered API provider: " + providerId);
    return true;
}

bool ProductionApiManager::unregisterApiProvider(const std::string& providerId) {
    {
        std::lock_guard<std::mutex> lock(providerMutex_);
        auto it = providers_.find(providerId);
        if (it == providers_.end()) {
            return false;
        }
        providers_.erase(it);
        providerCache_.clear();
    }

    // Clean up health status and metrics
    {
        std::lock_guard<std::mutex> lock(healthMutex_);
        healthStatus_.erase(providerId);
        performanceMetrics_.erase(providerId);
    }

    // Clean up rate limiter
    {
        std::lock_guard<std::mutex> lock(rateLimitMutex_);
        rateLimiters_.erase(providerId);
    }

    Utils::Logger::info("Unregistered API provider: " + providerId);
    return true;
}

std::vector<std::string> ProductionApiManager::getRegisteredProviders() const {
    std::lock_guard<std::mutex> lock(providerMutex_);
    std::vector<std::string> providers;
    for (const auto& pair : providers_) {
        providers.push_back(pair.first);
    }
    return providers;
}

std::vector<std::string> ProductionApiManager::getActiveProviders() const {
    std::lock_guard<std::mutex> lock(healthMutex_);
    std::vector<std::string> activeProviders;
    for (const auto& pair : healthStatus_) {
        if (pair.second.isConnected && pair.second.isHealthy) {
            activeProviders.push_back(pair.first);
        }
    }
    return activeProviders;
}

bool ProductionApiManager::isProviderActive(const std::string& providerId) const {
    std::lock_guard<std::mutex> lock(healthMutex_);
    auto it = healthStatus_.find(providerId);
    if (it == healthStatus_.end()) {
        return false;
    }
    return it->second.isConnected && it->second.isHealthy;
}

// === PROVIDER HEALTH AND CONNECTIVITY ===

ProductionApiManager::ProviderHealth ProductionApiManager::getProviderHealth(const std::string& providerId) const {
    std::lock_guard<std::mutex> lock(healthMutex_);
    auto it = healthStatus_.find(providerId);
    if (it != healthStatus_.end()) {
        return it->second;
    }

    // Return default unhealthy status
    ProviderHealth health;
    health.providerId = providerId;
    health.isConnected = false;
    health.isHealthy = false;
    return health;
}

std::map<std::string, ProductionApiManager::ProviderHealth> ProductionApiManager::getAllProviderHealth() const {
    std::lock_guard<std::mutex> lock(healthMutex_);
    return healthStatus_;
}

bool ProductionApiManager::performHealthCheck(const std::string& providerId) {
    return performSingleHealthCheck(providerId);
}

void ProductionApiManager::performAllHealthChecks() {
    auto providers = getRegisteredProviders();
    for (const auto& providerId : providers) {
        performSingleHealthCheck(providerId);
    }
}

// === SECURE API KEY MANAGEMENT ===

bool ProductionApiManager::setApiKey(const std::string& providerId, const std::string& apiKey) {
    if (apiKey.empty()) {
        Utils::Logger::error("Cannot set empty API key for provider: " + providerId);
        return false;
    }

    try {
        std::string encryptedKey = encryptApiKey(apiKey);
        return configManager_.setConfig(
            "api.key." + providerId,
            encryptedKey,
            SecurityLevel::SECRET,
            "Encrypted API key for " + providerId,
            "system"
        );
    } catch (const std::exception& e) {
        Utils::Logger::error("Failed to set API key for " + providerId + ": " + e.what());
        return false;
    }
}

bool ProductionApiManager::setApiCredentials(const std::string& providerId,
                                           const std::string& apiKey,
                                           const std::string& apiSecret,
                                           const std::string& passphrase) {
    bool success = true;

    // Set API key
    if (!apiKey.empty()) {
        success &= setApiKey(providerId, apiKey);
    }

    // Set API secret if provided
    if (!apiSecret.empty()) {
        try {
            std::string encryptedSecret = encryptApiKey(apiSecret);
            success &= configManager_.setConfig(
                "api.secret." + providerId,
                encryptedSecret,
                SecurityLevel::SECRET,
                "Encrypted API secret for " + providerId,
                "system"
            );
        } catch (const std::exception& e) {
            Utils::Logger::error("Failed to set API secret for " + providerId + ": " + e.what());
            success = false;
        }
    }

    // Set passphrase if provided
    if (!passphrase.empty()) {
        try {
            std::string encryptedPassphrase = encryptApiKey(passphrase);
            success &= configManager_.setConfig(
                "api.passphrase." + providerId,
                encryptedPassphrase,
                SecurityLevel::SECRET,
                "Encrypted passphrase for " + providerId,
                "system"
            );
        } catch (const std::exception& e) {
            Utils::Logger::error("Failed to set passphrase for " + providerId + ": " + e.what());
            success = false;
        }
    }

    return success;
}

std::string ProductionApiManager::getApiKey(const std::string& providerId) const {
    try {
        std::string encryptedKey = configManager_.getConfig("api.key." + providerId);
        if (encryptedKey.empty()) {
            return "";
        }
        return decryptApiKey(encryptedKey);
    } catch (const std::exception& e) {
        Utils::Logger::error("Failed to get API key for " + providerId + ": " + e.what());
        return "";
    }
}

bool ProductionApiManager::hasValidApiKey(const std::string& providerId) const {
    std::string apiKey = getApiKey(providerId);
    return !apiKey.empty();
}

bool ProductionApiManager::validateApiKey(const std::string& providerId) {
    if (!hasValidApiKey(providerId)) {
        Utils::Logger::warning("No API key found for provider: " + providerId);
        return false;
    }

    // Test the API key with a simple connectivity check
    auto testResult = testBasicConnectivity(providerId);
    return testResult.connectionSuccessful;
}

std::map<std::string, bool> ProductionApiManager::validateAllApiKeys() {
    std::map<std::string, bool> results;
    auto providers = getRegisteredProviders();

    for (const auto& providerId : providers) {
        results[providerId] = validateApiKey(providerId);
    }

    return results;
}

// === RATE LIMITING AND THROTTLING ===

ProductionApiManager::RateLimitStatus ProductionApiManager::getRateLimitStatus(const std::string& providerId) const {
    std::lock_guard<std::mutex> lock(rateLimitMutex_);

    RateLimitStatus status;
    auto it = rateLimiters_.find(providerId);
    if (it == rateLimiters_.end()) {
        status.isThrottled = true;
        status.nextAllowedRequest = std::chrono::milliseconds(1000);
        return status;
    }

    auto& limiter = *it->second;
    std::lock_guard<std::mutex> limiterLock(limiter.mutex);

    auto now = std::chrono::system_clock::now();

    // Clean up old requests
    while (!limiter.requests.empty() &&
           std::chrono::duration_cast<std::chrono::seconds>(now - limiter.requests.front()).count() >= 60) {
        limiter.requests.pop();
    }

    status.currentRequests = limiter.requests.size();
    status.maxRequestsPerSecond = limiter.maxRequestsPerSecond;
    status.maxRequestsPerMinute = limiter.maxRequestsPerMinute;
    status.remainingRequests = std::max(0, limiter.maxRequestsPerMinute - status.currentRequests);

    // Check if we're being throttled
    if (status.currentRequests >= limiter.maxRequestsPerMinute) {
        status.isThrottled = true;
        status.nextAllowedRequest = std::chrono::milliseconds(60000); // Wait for minute reset
    } else {
        // Check per-second limit
        int recentRequests = 0;
        auto oneSecondAgo = now - std::chrono::seconds(1);
        auto tempQueue = limiter.requests;
        while (!tempQueue.empty() && tempQueue.back() > oneSecondAgo) {
            recentRequests++;
            tempQueue.pop();
        }

        if (recentRequests >= limiter.maxRequestsPerSecond) {
            status.isThrottled = true;
            status.nextAllowedRequest = std::chrono::milliseconds(1000 / limiter.maxRequestsPerSecond);
        } else {
            status.isThrottled = false;
            status.nextAllowedRequest = std::chrono::milliseconds(0);
        }
    }

    return status;
}

bool ProductionApiManager::canMakeRequest(const std::string& providerId) const {
    auto status = getRateLimitStatus(providerId);
    return !status.isThrottled;
}

// === BASIC CONNECTIVITY TESTING ===

ProductionApiManager::ConnectivityTest ProductionApiManager::testBasicConnectivity(const std::string& providerId) {
    ConnectivityTest test;
    test.providerId = providerId;
    test.testTime = std::chrono::system_clock::now();

    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        auto provider = getDataProvider(providerId);
        if (!provider) {
            test.connectionSuccessful = false;
            test.errorMessage = "Provider not found";
            return test;
        }

        // Test basic availability
        test.connectionSuccessful = provider->isAvailable();
        if (!test.connectionSuccessful) {
            test.errorMessage = provider->getLastError();
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        test.latencyMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

        test.httpStatusCode = test.connectionSuccessful ? 200 : 500;

    } catch (const std::exception& e) {
        test.connectionSuccessful = false;
        test.errorMessage = e.what();
        test.httpStatusCode = 500;

        auto endTime = std::chrono::high_resolution_clock::now();
        test.latencyMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    }

    // Update provider health based on test results
    updateProviderHealth(providerId, test.connectionSuccessful, test.latencyMs, test.errorMessage);

    return test;
}

std::map<std::string, ProductionApiManager::ConnectivityTest> ProductionApiManager::testAllConnectivity() {
    std::map<std::string, ConnectivityTest> results;
    auto providers = getRegisteredProviders();

    for (const auto& providerId : providers) {
        results[providerId] = testBasicConnectivity(providerId);
    }

    return results;
}

void ProductionApiManager::enableContinuousMonitoring(bool enable) {
    if (enable && !continuousMonitoring_) {
        continuousMonitoring_ = true;
        shutdownRequested_ = false;
        monitoringThread_ = std::make_unique<std::thread>(&ProductionApiManager::performContinuousHealthChecks, this);
        Utils::Logger::info("Continuous monitoring enabled");
    } else if (!enable && continuousMonitoring_) {
        continuousMonitoring_ = false;
        shutdownRequested_ = true;
        if (monitoringThread_ && monitoringThread_->joinable()) {
            monitoringThread_->join();
        }
        Utils::Logger::info("Continuous monitoring disabled");
    }
}

// === DATA PROVIDER ACCESS ===

std::shared_ptr<IDataProvider> ProductionApiManager::getDataProvider(const std::string& providerId) const {
    std::lock_guard<std::mutex> lock(providerMutex_);

    // Check cache first
    auto cacheIt = providerCache_.find(providerId);
    if (cacheIt != providerCache_.end()) {
        return cacheIt->second;
    }

    // Find in providers
    auto it = providers_.find(providerId);
    if (it != providers_.end()) {
        auto sharedProvider = std::shared_ptr<IDataProvider>(it->second.get(), [](IDataProvider*) {});
        providerCache_[providerId] = sharedProvider;
        return sharedProvider;
    }

    return nullptr;
}

std::shared_ptr<IMarketDataProvider> ProductionApiManager::getMarketDataProvider(const std::string& providerId) const {
    auto provider = getDataProvider(providerId);
    return std::dynamic_pointer_cast<IMarketDataProvider>(provider);
}

std::shared_ptr<INewsProvider> ProductionApiManager::getNewsProvider(const std::string& providerId) const {
    auto provider = getDataProvider(providerId);
    return std::dynamic_pointer_cast<INewsProvider>(provider);
}

std::string ProductionApiManager::selectBestProvider(const std::string& providerType) const {
    auto providers = getProvidersByPriority(providerType);
    if (providers.empty()) {
        return "";
    }

    // Return the first healthy provider
    for (const auto& providerId : providers) {
        if (isProviderActive(providerId)) {
            return providerId;
        }
    }

    // If no healthy providers, return the highest priority one
    return providers[0];
}

std::vector<std::string> ProductionApiManager::getProvidersByPriority(const std::string& providerType) const {
    std::vector<std::pair<std::string, int>> providerPriorities;

    auto allProviders = getRegisteredProviders();
    for (const auto& providerId : allProviders) {
        auto config = configManager_.getApiProviderConfig(providerId);
        if (providerType.empty() || config.providerType == providerType) {
            providerPriorities.emplace_back(providerId, config.priority);
        }
    }

    // Sort by priority (lower numbers = higher priority)
    std::sort(providerPriorities.begin(), providerPriorities.end(),
              [](const auto& a, const auto& b) { return a.second < b.second; });

    std::vector<std::string> result;
    for (const auto& pair : providerPriorities) {
        result.push_back(pair.first);
    }

    return result;
}

// === PRIVATE HELPER METHODS ===

bool ProductionApiManager::initializeEncryption() {
    try {
        encryptionManager_ = std::make_unique<Security::EncryptionManager>();
        return encryptionManager_->initialize();
    } catch (const std::exception& e) {
        Utils::Logger::error("Failed to initialize encryption manager: " + std::string(e.what()));
        return false;
    }
}

bool ProductionApiManager::initializeProviders() {
    // Get all configured API providers
    auto providerConfigs = configManager_.getAllApiProviders(true); // enabled only

    for (const auto& config : providerConfigs) {
        try {
            std::unique_ptr<IDataProvider> provider;

            // Create provider based on type
            if (config.providerType == "cryptocompare") {
                auto cryptoCompareProvider = std::make_unique<CryptoCompareProvider>(config.apiKey);
                provider = std::move(cryptoCompareProvider);
            } else if (config.providerType == "newsapi") {
                auto newsProvider = std::make_unique<NewsApiProvider>(config.apiKey);
                provider = std::move(newsProvider);
            }
            // Add other provider types as needed

            if (provider) {
                registerApiProvider(config.providerId, std::move(provider));
            } else {
                Utils::Logger::warning("Unknown provider type: " + config.providerType);
            }

        } catch (const std::exception& e) {
            Utils::Logger::error("Failed to initialize provider " + config.providerId + ": " + e.what());
            continue;
        }
    }

    return true;
}

bool ProductionApiManager::initializeRateLimiters() {
    // Rate limiters are created when providers are registered
    return true;
}

bool ProductionApiManager::initializeMonitoring() {
    // Start continuous monitoring if enabled in configuration
    bool enableMonitoring = configManager_.getGlobalConfig().enableHealthMonitoring;
    if (enableMonitoring) {
        enableContinuousMonitoring(true);
    }
    return true;
}

void ProductionApiManager::updateProviderHealth(const std::string& providerId, bool success,
                                              double latencyMs, const std::string& error) {
    std::lock_guard<std::mutex> lock(healthMutex_);

    auto it = healthStatus_.find(providerId);
    if (it == healthStatus_.end()) {
        return; // Provider not found
    }

    auto& health = it->second;
    health.lastHealthCheck = std::chrono::system_clock::now();
    health.latencyMs = latencyMs;

    if (success) {
        health.isConnected = true;
        health.consecutiveFailures = 0;
        health.lastError.clear();

        // Update success rate (simple rolling average)
        health.successRate = 0.9 * health.successRate + 0.1 * 1.0;
        health.isHealthy = health.successRate >= failoverConfig_.healthThreshold;

    } else {
        health.consecutiveFailures++;
        health.lastError = error;

        // Update success rate
        health.successRate = 0.9 * health.successRate + 0.1 * 0.0;
        health.isHealthy = health.successRate >= failoverConfig_.healthThreshold;

        // Mark as disconnected if too many consecutive failures
        if (health.consecutiveFailures >= failoverConfig_.maxConsecutiveFailures) {
            health.isConnected = false;
        }
    }

    // Notify callbacks if health status changed
    if (healthCallback_) {
        healthCallback_(providerId, success ? "healthy" : "unhealthy");
    }
}

bool ProductionApiManager::checkRateLimit(const std::string& providerId) const {
    auto status = getRateLimitStatus(providerId);
    return !status.isThrottled;
}

void ProductionApiManager::recordRequest(const std::string& providerId) const {
    std::lock_guard<std::mutex> lock(rateLimitMutex_);
    auto it = rateLimiters_.find(providerId);
    if (it != rateLimiters_.end()) {
        auto& limiter = *it->second;
        std::lock_guard<std::mutex> limiterLock(limiter.mutex);
        limiter.requests.push(std::chrono::system_clock::now());
    }
}

void ProductionApiManager::performContinuousHealthChecks() {
    while (!shutdownRequested_ && continuousMonitoring_) {
        auto providers = getRegisteredProviders();
        for (const auto& providerId : providers) {
            if (shutdownRequested_) break;
            performSingleHealthCheck(providerId);
        }

        // Wait for next check interval
        std::this_thread::sleep_for(std::chrono::seconds(60));
    }
}

bool ProductionApiManager::performSingleHealthCheck(const std::string& providerId) {
    try {
        auto test = testBasicConnectivity(providerId);
        return test.connectionSuccessful;
    } catch (const std::exception& e) {
        updateProviderHealth(providerId, false, 0.0, e.what());
        return false;
    }
}

void ProductionApiManager::recordRequestMetrics(const std::string& providerId, bool success,
                                               double latencyMs, int httpStatusCode) {
    std::lock_guard<std::mutex> lock(healthMutex_);
    auto it = performanceMetrics_.find(providerId);
    if (it == performanceMetrics_.end()) {
        return;
    }

    auto& metrics = it->second;
    metrics.totalRequests++;
    metrics.lastRequest = std::chrono::system_clock::now();
    metrics.httpStatusCodes[httpStatusCode]++;

    if (success) {
        metrics.successfulRequests++;
    } else {
        metrics.failedRequests++;
    }

    // Update latency statistics
    if (latencyMs > 0) {
        if (metrics.totalRequests == 1) {
            metrics.averageLatencyMs = latencyMs;
            metrics.minLatencyMs = latencyMs;
            metrics.maxLatencyMs = latencyMs;
        } else {
            // Running average
            metrics.averageLatencyMs = (metrics.averageLatencyMs * (metrics.totalRequests - 1) + latencyMs) / metrics.totalRequests;
            metrics.minLatencyMs = std::min(metrics.minLatencyMs, latencyMs);
            metrics.maxLatencyMs = std::max(metrics.maxLatencyMs, latencyMs);
        }
    }

    // Update success rate
    metrics.successRate = static_cast<double>(metrics.successfulRequests) / metrics.totalRequests;
}

std::string ProductionApiManager::encryptApiKey(const std::string& apiKey) const {
    if (!encryptionManager_) {
        throw std::runtime_error("Encryption manager not initialized");
    }
    return encryptionManager_->encrypt(apiKey);
}

std::string ProductionApiManager::decryptApiKey(const std::string& encryptedKey) const {
    if (!encryptionManager_) {
        throw std::runtime_error("Encryption manager not initialized");
    }
    return encryptionManager_->decrypt(encryptedKey);
}

ProductionApiManager::SystemDiagnostics ProductionApiManager::performSystemDiagnostics() {
    SystemDiagnostics diagnostics;
    diagnostics.diagnosticsTime = std::chrono::system_clock::now();

    auto allProviders = getRegisteredProviders();
    diagnostics.totalProviders = allProviders.size();

    int healthyCount = 0;
    int activeCount = 0;
    double totalLatency = 0.0;
    double totalSuccessRate = 0.0;

    for (const auto& providerId : allProviders) {
        auto health = getProviderHealth(providerId);
        if (health.isHealthy) {
            healthyCount++;
            totalLatency += health.latencyMs;
            totalSuccessRate += health.successRate;
        }
        if (health.isConnected) {
            activeCount++;
        }

        // Check for warnings and errors
        if (!health.isConnected) {
            diagnostics.errors.push_back("Provider disconnected: " + providerId);
        } else if (!health.isHealthy) {
            diagnostics.warnings.push_back("Provider unhealthy: " + providerId + " (" + health.lastError + ")");
        }
    }

    diagnostics.healthyProviders = healthyCount;
    diagnostics.activeProviders = activeCount;
    diagnostics.overallHealth = (healthyCount > 0) && (healthyCount >= allProviders.size() / 2);

    if (healthyCount > 0) {
        diagnostics.systemLatencyMs = totalLatency / healthyCount;
        diagnostics.systemSuccessRate = totalSuccessRate / healthyCount;
    }

    return diagnostics;
}

} // namespace Data
} // namespace CryptoClaude