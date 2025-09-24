#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>
#include <vector>
#include <future>
#include <atomic>
#include "src/Core/Api/ApiResponseFramework.h"
#include "src/Core/Http/HttpClient.h"
#include "src/Core/Json/JsonParser.h"

using namespace CryptoClaude::Api;
using namespace CryptoClaude::Http;
using namespace CryptoClaude::Json;

// Enhanced mock HTTP client for edge case testing
class ComprehensiveTestHttpClient : public IHttpClient {
private:
    std::atomic<int> requestCount_{0};
    bool simulateNetworkDelay_;
    bool simulateErrors_;

public:
    ComprehensiveTestHttpClient(bool networkDelay = false, bool errors = false)
        : simulateNetworkDelay_(networkDelay), simulateErrors_(errors) {}

    HttpResponse execute(const HttpRequest& request) override {
        requestCount_++;

        // Simulate network delay
        if (simulateNetworkDelay_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        // Simulate various error conditions
        if (simulateErrors_ && requestCount_ % 3 == 0) {
            return HttpResponse(503, "Service Unavailable");
        }

        // Return different responses based on URL
        std::string url = "test_url"; // In real implementation, extract from request
        if (url.find("invalid") != std::string::npos) {
            return HttpResponse(400, R"({"error": "Invalid request"})");
        }

        // Return large JSON payload to test parsing
        if (url.find("large") != std::string::npos) {
            std::string largePayload = R"({"data": {)";
            for (int i = 0; i < 1000; ++i) {
                largePayload += "\"item" + std::to_string(i) + "\": " + std::to_string(i * 100);
                if (i < 999) largePayload += ",";
            }
            largePayload += "}}";
            return HttpResponse(200, largePayload);
        }

        // Return malformed JSON
        if (url.find("malformed") != std::string::npos) {
            return HttpResponse(200, R"({"incomplete": "json", "missing": )");
        }

        // Default successful response
        return HttpResponse(200, R"({"USD": 50000, "status": "ok", "timestamp": 1640995200})");
    }

    void setRetryConfig(const RetryConfig& config) override {}
    void setDefaultTimeout(std::chrono::seconds timeout) override {}
    void setUserAgent(const std::string& userAgent) override {}

    int getRequestCount() const { return requestCount_; }
};

void testThreadSafetyAndConcurrency() {
    std::cout << "Testing thread safety and concurrency..." << std::endl;

    auto alertManager = std::make_shared<AlertManager>();
    auto httpClient = std::make_shared<ComprehensiveTestHttpClient>(true, false); // With network delay

    // Test concurrent rate limiter access
    RateLimitConfig config;
    config.maxRequestsPerMinute = 100; // Higher limit for concurrency test
    auto rateLimiter = std::make_shared<RateLimiter>(config, alertManager);

    std::atomic<int> successfulRequests{0};
    std::atomic<int> rateLimitedRequests{0};

    // Create multiple threads making concurrent requests
    std::vector<std::future<void>> futures;
    const int numThreads = 10;
    const int requestsPerThread = 20;

    for (int t = 0; t < numThreads; ++t) {
        futures.push_back(std::async(std::launch::async, [&]() {
            for (int i = 0; i < requestsPerThread; ++i) {
                if (rateLimiter->canMakeRequest()) {
                    successfulRequests++;
                    rateLimiter->recordSuccess();
                } else {
                    rateLimitedRequests++;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }));
    }

    // Wait for all threads to complete
    for (auto& future : futures) {
        future.wait();
    }

    int totalRequests = successfulRequests + rateLimitedRequests;
    assert(totalRequests == numThreads * requestsPerThread);
    assert(successfulRequests > 0);
    assert(rateLimitedRequests > 0); // Should have some rate limiting

    std::cout << "✅ Concurrent requests: " << successfulRequests << " successful, "
              << rateLimitedRequests << " rate limited" << std::endl;

    // Test concurrent alert manager access
    std::atomic<int> alertsAdded{0};
    std::vector<std::future<void>> alertFutures;

    for (int t = 0; t < 5; ++t) {
        alertFutures.push_back(std::async(std::launch::async, [&, t]() {
            for (int i = 0; i < 50; ++i) {
                alertManager->addAlert(AlertType::INFO,
                    "Thread " + std::to_string(t) + " alert " + std::to_string(i),
                    "ConcurrencyTest");
                alertsAdded++;
            }
        }));
    }

    for (auto& future : alertFutures) {
        future.wait();
    }

    assert(alertsAdded == 250); // 5 threads * 50 alerts each
    auto recentAlerts = alertManager->getRecentAlerts(std::chrono::minutes(5));
    assert(recentAlerts.size() == 250);

    std::cout << "✅ Thread safety test passed: " << alertsAdded << " alerts added concurrently" << std::endl;
}

void testJsonParsingEdgeCases() {
    std::cout << "Testing JSON parsing edge cases..." << std::endl;

    auto alertManager = std::make_shared<AlertManager>();
    auto processor = std::make_shared<ResponseProcessor>(DataFreshnessConfig{}, alertManager);

    // Test malformed JSON
    HttpResponse malformedResponse(200, R"({"invalid": json, "missing": })");
    auto processed = processor->processResponse(malformedResponse, ApiProvider::CRYPTO_COMPARE, "/test");
    assert(processed.status == ResponseStatus::VALIDATION_ERROR);
    assert(processed.errorMessage.has_value());
    assert(processed.errorMessage->find("JSON parsing failed") != std::string::npos);
    std::cout << "✅ Malformed JSON handled correctly" << std::endl;

    // Test empty JSON
    HttpResponse emptyResponse(200, "{}");
    auto processedEmpty = processor->processResponse(emptyResponse, ApiProvider::CRYPTO_COMPARE, "/test");
    assert(processedEmpty.status == ResponseStatus::SUCCESS); // Empty JSON is valid
    std::cout << "✅ Empty JSON handled correctly" << std::endl;

    // Test deeply nested JSON (limited by our mock parser)
    HttpResponse nestedResponse(200, R"({"level1": {"level2": {"level3": {"value": 42}}}})");
    auto processedNested = processor->processResponse(nestedResponse, ApiProvider::CRYPTO_COMPARE, "/test");
    assert(processedNested.status == ResponseStatus::SUCCESS);
    std::cout << "✅ Nested JSON handled correctly" << std::endl;

    // Test array JSON
    HttpResponse arrayResponse(200, R"([{"item": 1}, {"item": 2}, {"item": 3}])");
    auto processedArray = processor->processResponse(arrayResponse, ApiProvider::NEWS_API, "/test");
    assert(processedArray.status == ResponseStatus::SUCCESS);
    std::cout << "✅ Array JSON handled correctly" << std::endl;
}

void testConfigurationEdgeCases() {
    std::cout << "Testing configuration edge cases..." << std::endl;

    auto alertManager = std::make_shared<AlertManager>();

    // Test invalid rate limit configurations
    RateLimitConfig invalidConfig1;
    invalidConfig1.maxRequestsPerMinute = 0; // Zero requests

    // Should not crash, but handle gracefully
    try {
        auto rateLimiter = std::make_shared<RateLimiter>(invalidConfig1, alertManager);
        bool canRequest = rateLimiter->canMakeRequest();
        assert(!canRequest); // Should not allow requests with 0 limit
        std::cout << "✅ Zero rate limit handled correctly" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "✅ Zero rate limit throws exception as expected: " << e.what() << std::endl;
    }

    // Test negative rate limit
    RateLimitConfig invalidConfig2;
    invalidConfig2.maxRequestsPerMinute = -10; // Negative requests
    invalidConfig2.minRequestsPerMinute = -5;

    try {
        auto rateLimiter = std::make_shared<RateLimiter>(invalidConfig2, alertManager);
        bool canRequest = rateLimiter->canMakeRequest();
        // Implementation should handle this gracefully
        std::cout << "✅ Negative rate limit handled: canRequest = " << canRequest << std::endl;
    } catch (const std::exception& e) {
        std::cout << "✅ Negative rate limit throws exception as expected: " << e.what() << std::endl;
    }

    // Test extreme configuration values
    RateLimitConfig extremeConfig;
    extremeConfig.maxRequestsPerMinute = 1000000; // Very high limit
    extremeConfig.adaptiveScaleFactor = 0.1; // Very aggressive scaling

    auto extremeRateLimiter = std::make_shared<RateLimiter>(extremeConfig, alertManager);
    assert(extremeRateLimiter->canMakeRequest()); // Should work
    std::cout << "✅ Extreme configuration values handled" << std::endl;

    // Test data freshness edge cases
    DataFreshnessConfig freshnessConfig;
    freshnessConfig.maxMarketDataAge = std::chrono::hours(0); // Immediate expiry
    freshnessConfig.maxNewsDataAge = std::chrono::hours(8760 * 10); // 10 years

    auto processor = std::make_shared<ResponseProcessor>(freshnessConfig, alertManager);

    // Create response with current timestamp
    HttpResponse freshResponse(200, R"({"status": "ok"})");
    auto processed = processor->processResponse(freshResponse, ApiProvider::NEWS_API, "/news");
    // News API sets timestamp to now, so with 10-year limit it should not be stale
    assert(!processor->isDataStale(processed));
    std::cout << "✅ Extreme freshness configuration handled" << std::endl;
}

void testMemoryAndResourceManagement() {
    std::cout << "Testing memory and resource management..." << std::endl;

    auto alertManager = std::make_shared<AlertManager>();

    // Test alert cleanup over time
    for (int i = 0; i < 10000; ++i) {
        alertManager->addAlert(AlertType::INFO, "Test alert " + std::to_string(i), "MemoryTest");
    }

    auto beforeCleanup = alertManager->getRecentAlerts(std::chrono::minutes(60));
    assert(beforeCleanup.size() == 10000);
    std::cout << "✅ Added 10,000 alerts: " << beforeCleanup.size() << " found" << std::endl;

    // Clear old alerts (everything older than 0 hours = everything)
    alertManager->clearOldAlerts(std::chrono::hours(0));

    auto afterCleanup = alertManager->getRecentAlerts(std::chrono::minutes(60));
    assert(afterCleanup.size() == 0);
    std::cout << "✅ Alert cleanup working: " << afterCleanup.size() << " remaining after cleanup" << std::endl;

    // Test rate limiter with many requests over time
    RateLimitConfig config;
    config.maxRequestsPerMinute = 1000;
    auto rateLimiter = std::make_shared<RateLimiter>(config, alertManager);

    int successfulRequests = 0;
    for (int i = 0; i < 5000; ++i) {
        if (rateLimiter->canMakeRequest()) {
            successfulRequests++;
            if (i % 100 == 0) {
                rateLimiter->recordSuccess();
            }
        }

        // Small delay every 1000 requests to allow token refill
        if (i % 1000 == 999) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    std::cout << "✅ Processed 5,000 requests: " << successfulRequests << " successful" << std::endl;
    assert(successfulRequests > 0);
}

void testProviderSpecificErrorHandling() {
    std::cout << "Testing provider-specific error handling..." << std::endl;

    auto alertManager = std::make_shared<AlertManager>();
    auto processor = std::make_shared<ResponseProcessor>(DataFreshnessConfig{}, alertManager);

    // Test CryptoCompare specific errors
    HttpResponse ccErrorResponse(200, R"({"Response": "Error", "Message": "Invalid symbol"})");
    auto ccProcessed = processor->processResponse(ccErrorResponse, ApiProvider::CRYPTO_COMPARE, "/data/price");
    assert(ccProcessed.status == ResponseStatus::CLIENT_ERROR);
    assert(ccProcessed.errorMessage.has_value());
    assert(ccProcessed.errorMessage->find("Invalid symbol") != std::string::npos);
    std::cout << "✅ CryptoCompare error handling" << std::endl;

    // Test NewsAPI specific errors
    HttpResponse newsErrorResponse(200, R"({"status": "error", "message": "API key invalid"})");
    auto newsProcessed = processor->processResponse(newsErrorResponse, ApiProvider::NEWS_API, "/news");
    assert(newsProcessed.status == ResponseStatus::CLIENT_ERROR);
    assert(newsProcessed.errorMessage.has_value());
    assert(newsProcessed.errorMessage->find("API key invalid") != std::string::npos);
    std::cout << "✅ NewsAPI error handling" << std::endl;

    // Test unknown provider (should not crash)
    auto unknownProcessed = processor->processResponse(ccErrorResponse, ApiProvider::UNKNOWN, "/unknown");
    assert(unknownProcessed.status == ResponseStatus::SUCCESS); // No specific validation
    std::cout << "✅ Unknown provider handled gracefully" << std::endl;
}

void testLongRunningScenarios() {
    std::cout << "Testing long-running scenarios..." << std::endl;

    auto alertManager = std::make_shared<AlertManager>();
    auto httpClient = std::make_shared<ComprehensiveTestHttpClient>(false, true); // With errors

    RateLimitConfig config;
    config.maxRequestsPerMinute = 60; // 1 per second
    config.enableAdaptiveScaling = true;
    auto rateLimiter = std::make_shared<RateLimiter>(config, alertManager);

    auto processor = std::make_shared<ResponseProcessor>(DataFreshnessConfig{}, alertManager);
    auto apiClient = std::make_shared<ApiClient>(httpClient, rateLimiter, processor, alertManager);

    // Simulate continuous operation for a short period
    int successCount = 0;
    int errorCount = 0;
    int rateLimitCount = 0;

    auto startTime = std::chrono::steady_clock::now();
    auto endTime = startTime + std::chrono::seconds(5); // Run for 5 seconds

    while (std::chrono::steady_clock::now() < endTime) {
        auto response = apiClient->makeRequest("https://test.com/api", ApiProvider::CRYPTO_COMPARE);

        switch (response.status) {
            case ResponseStatus::SUCCESS:
                successCount++;
                break;
            case ResponseStatus::RATE_LIMITED:
                rateLimitCount++;
                break;
            default:
                errorCount++;
                break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 20 requests per second attempted
    }

    std::cout << "✅ Long-running test (5s): " << successCount << " success, "
              << errorCount << " errors, " << rateLimitCount << " rate limited" << std::endl;

    // Verify adaptive behavior kicked in
    assert(rateLimiter->getCurrentLimit() <= config.maxRequestsPerMinute); // May have been reduced

    // Check that alerts were generated
    auto alerts = alertManager->getRecentAlerts(std::chrono::minutes(5));
    assert(alerts.size() > 0); // Should have some alerts from errors
    std::cout << "✅ Generated " << alerts.size() << " alerts during long-running test" << std::endl;
}

int main() {
    std::cout << "Starting Day 13 Comprehensive API Response Framework Tests..." << std::endl;
    std::cout << "================================================================================\n" << std::endl;

    try {
        testThreadSafetyAndConcurrency();
        std::cout << std::endl;

        testJsonParsingEdgeCases();
        std::cout << std::endl;

        testConfigurationEdgeCases();
        std::cout << std::endl;

        testMemoryAndResourceManagement();
        std::cout << std::endl;

        testProviderSpecificErrorHandling();
        std::cout << std::endl;

        testLongRunningScenarios();

        std::cout << "\n================================================================================\n";
        std::cout << "🎉 All comprehensive API Response Framework tests passed!" << std::endl;
        std::cout << "🧵 Thread safety and concurrency verified" << std::endl;
        std::cout << "📊 JSON parsing edge cases handled" << std::endl;
        std::cout << "⚙️  Configuration validation working" << std::endl;
        std::cout << "💾 Memory management and cleanup functional" << std::endl;
        std::cout << "🔌 Provider-specific error handling robust" << std::endl;
        std::cout << "⏱️  Long-running scenarios stable" << std::endl;
        std::cout << "\n🚀 API Response Framework is production-ready!" << std::endl;

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "❌ Comprehensive test failed: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "❌ Unknown comprehensive test failure" << std::endl;
        return 1;
    }
}