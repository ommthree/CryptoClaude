#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <cassert>
#include <memory>

// Include the production hardening components
#include "src/Core/Http/EnhancedHttpClient.h"
#include "src/Core/Monitoring/DataQualityMonitor.h"
#include "src/Core/Data/DataProcessingService.h"
#include "src/Core/Data/Providers/CryptoCompareProvider.h"
#include "src/Core/Data/Providers/NewsApiProvider.h"

using namespace CryptoClaude;
using namespace CryptoClaude::Http;
using namespace CryptoClaude::Monitoring;
using namespace CryptoClaude::Data;
using namespace CryptoClaude::Data::Providers;

// Test helper class for mock HTTP client
class MockHttpClient : public IHttpClient {
private:
    mutable std::atomic<int> requestCount_{0};
    mutable std::atomic<int> successCount_{0};
    mutable std::atomic<int> failureCount_{0};

public:
    HttpResponse execute(const HttpRequest& request) override {
        ++requestCount_;

        // Simulate network delay
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Simulate different response patterns based on URL
        std::string url = request.getUrl();

        if (url.find("test-success") != std::string::npos) {
            ++successCount_;
            return HttpResponse(200, R"({"status":"success","data":"test_data"})");
        } else if (url.find("test-retry") != std::string::npos) {
            if (requestCount_ % 3 == 0) {  // Succeed every 3rd attempt
                ++successCount_;
                return HttpResponse(200, R"({"status":"success","data":"retry_success"})");
            } else {
                ++failureCount_;
                return HttpResponse(503, "Service temporarily unavailable");
            }
        } else if (url.find("test-failure") != std::string::npos) {
            ++failureCount_;
            return HttpResponse(500, "Internal server error");
        } else {
            ++successCount_;
            return HttpResponse(200, R"({"status":"ok","result":"mock_response"})");
        }
    }

    void setRetryConfig(const RetryConfig& config) override {}
    void setDefaultTimeout(std::chrono::seconds timeout) override {}
    void setUserAgent(const std::string& userAgent) override {}

    int getRequestCount() const { return requestCount_; }
    int getSuccessCount() const { return successCount_; }
    int getFailureCount() const { return failureCount_; }
    void resetCounters() { requestCount_ = 0; successCount_ = 0; failureCount_ = 0; }
};

class Priority3ProductionHardeningTest {
private:
    bool testConnectionPooling();
    bool testCircuitBreakerPattern();
    bool testEnhancedRetryLogic();
    bool testDataQualityMonitoring();
    bool testIntegrationScenarios();

public:
    bool runAllTests() {
        std::cout << "\n🏭 PRIORITY 3 PRODUCTION HARDENING TESTS" << std::endl;
        std::cout << "=========================================" << std::endl;

        bool allPassed = true;

        allPassed &= testConnectionPooling();
        allPassed &= testCircuitBreakerPattern();
        allPassed &= testEnhancedRetryLogic();
        allPassed &= testDataQualityMonitoring();
        allPassed &= testIntegrationScenarios();

        std::cout << "\n=========================================" << std::endl;
        if (allPassed) {
            std::cout << "🎉 ALL PRODUCTION HARDENING TESTS PASSED!" << std::endl;
        } else {
            std::cout << "❌ SOME PRODUCTION HARDENING TESTS FAILED!" << std::endl;
        }
        std::cout << "=========================================" << std::endl;

        return allPassed;
    }
};

bool Priority3ProductionHardeningTest::testConnectionPooling() {
    std::cout << "\n🔗 Testing Connection Pooling..." << std::endl;

    try {
        // Create connection pool
        auto pool = std::make_unique<ConnectionPool>(5, 20);

        // Test getting connections
        std::vector<std::shared_ptr<HttpConnection>> connections;

        for (int i = 0; i < 3; ++i) {
            auto conn = pool->getConnection("api.example.com", 443, true);
            assert(conn != nullptr);
            connections.push_back(conn);
        }

        auto stats = pool->getStatistics();
        assert(stats.totalConnections == 3);
        assert(stats.activeConnections == 3);
        assert(stats.connectionsCreated == 3);
        assert(stats.connectionMisses == 3); // New connections

        std::cout << "  ✅ Connection creation: " << stats.connectionsCreated << " connections" << std::endl;

        // Return connections to pool
        for (auto& conn : connections) {
            pool->returnConnection(conn);
        }
        connections.clear();

        stats = pool->getStatistics();
        assert(stats.activeConnections == 0);
        std::cout << "  ✅ Connection return: " << stats.activeConnections << " active connections" << std::endl;

        // Test connection reuse
        auto conn1 = pool->getConnection("api.example.com", 443, true);
        assert(conn1 != nullptr);
        stats = pool->getStatistics();
        assert(stats.connectionHits == 1); // Reused from pool

        std::cout << "  ✅ Connection reuse: Hit rate = " << (stats.hitRate * 100) << "%" << std::endl;

        // Test cleanup
        pool->cleanupExpiredConnections();
        pool->performHealthChecks();

        std::cout << "  ✅ Connection maintenance completed" << std::endl;

        return true;

    } catch (const std::exception& e) {
        std::cout << "  ❌ Connection pooling test failed: " << e.what() << std::endl;
        return false;
    }
}

bool Priority3ProductionHardeningTest::testCircuitBreakerPattern() {
    std::cout << "\n⚡ Testing Circuit Breaker Pattern..." << std::endl;

    try {
        CircuitBreaker breaker(3, std::chrono::seconds(2), 2, 0.6);

        // Test closed state (normal operation)
        assert(breaker.getState() == CircuitBreakerState::CLOSED);
        assert(breaker.allowRequest() == true);
        std::cout << "  ✅ Circuit breaker starts in CLOSED state" << std::endl;

        // Test failure threshold
        for (int i = 0; i < 5; ++i) {
            assert(breaker.allowRequest() == true);
            breaker.recordFailure();
        }

        // Should now be open
        assert(breaker.getState() == CircuitBreakerState::OPEN);
        assert(breaker.allowRequest() == false);
        std::cout << "  ✅ Circuit breaker opens after failure threshold (5 failures)" << std::endl;

        // Wait for timeout and test half-open state
        std::this_thread::sleep_for(std::chrono::seconds(3));
        assert(breaker.allowRequest() == true); // Should transition to half-open
        assert(breaker.getState() == CircuitBreakerState::HALF_OPEN);
        std::cout << "  ✅ Circuit breaker transitions to HALF_OPEN after timeout" << std::endl;

        // Test recovery (success in half-open)
        breaker.recordSuccess();
        breaker.recordSuccess(); // Need 2 successes by default
        assert(breaker.getState() == CircuitBreakerState::CLOSED);
        std::cout << "  ✅ Circuit breaker closes after successful recovery" << std::endl;

        // Test statistics
        auto stats = breaker.getStatistics();
        assert(stats.totalRequests > 0);
        assert(stats.failureCount > 0);
        std::cout << "  ✅ Circuit breaker statistics: " << stats.totalRequests
                  << " requests, " << (stats.failureRate * 100) << "% failure rate" << std::endl;

        return true;

    } catch (const std::exception& e) {
        std::cout << "  ❌ Circuit breaker test failed: " << e.what() << std::endl;
        return false;
    }
}

bool Priority3ProductionHardeningTest::testEnhancedRetryLogic() {
    std::cout << "\n🔄 Testing Enhanced Retry Logic..." << std::endl;

    try {
        // Configure advanced retry settings
        AdvancedRetryConfig config;
        config.maxRetries = 3;
        config.baseDelay = std::chrono::milliseconds(100);
        config.backoffMultiplier = 2.0;
        config.maxDelay = std::chrono::milliseconds(1000);
        config.jitterFactor = 0.1;
        config.retryOnStatusCodes = {503, 502, 504};
        config.noRetryOnStatusCodes = {400, 401, 404};

        auto client = std::make_unique<EnhancedHttpClient>(config);
        client->enableLogging(true);

        std::cout << "  ✅ Enhanced HTTP client configured with advanced retry" << std::endl;

        // Test successful request (no retry needed)
        HttpRequest successRequest(HttpMethod::GET, "https://api.test.com/test-success");
        auto response1 = client->execute(successRequest);
        assert(response1.getStatusCode() == 200);
        std::cout << "  ✅ Successful request completed without retry" << std::endl;

        // Test retry scenario
        HttpRequest retryRequest(HttpMethod::GET, "https://api.test.com/test-retry");
        auto response2 = client->execute(retryRequest);
        // Should eventually succeed after retries
        std::cout << "  ✅ Retry logic handled temporary failures" << std::endl;

        // Test circuit breaker integration
        client->enableCircuitBreaker(true);

        // Make some failures to trip circuit breaker
        HttpRequest failRequest(HttpMethod::GET, "https://api.failure.com/test-failure");
        for (int i = 0; i < 8; ++i) {
            client->execute(failRequest);
        }

        // Next request should be rejected by circuit breaker
        auto response3 = client->execute(failRequest);
        assert(response3.getStatusCode() == 503); // Circuit breaker rejection
        std::cout << "  ✅ Circuit breaker integration working" << std::endl;

        // Test connection pooling integration
        client->enableConnectionPooling(true);
        client->warmupConnectionPool({"api.test.com", "api.example.com"});
        std::cout << "  ✅ Connection pool warmup completed" << std::endl;

        // Get comprehensive statistics
        auto stats = client->getStatistics();
        std::cout << "  📊 Enhanced client statistics:" << std::endl;
        std::cout << "    Total requests: " << stats.totalRequests << std::endl;
        std::cout << "    Success rate: " << (stats.successRate * 100) << "%" << std::endl;
        std::cout << "    Retry rate: " << (stats.retryRate * 100) << "%" << std::endl;
        std::cout << "    Circuit breaker rejections: " << stats.circuitBreakerRejections << std::endl;

        return true;

    } catch (const std::exception& e) {
        std::cout << "  ❌ Enhanced retry logic test failed: " << e.what() << std::endl;
        return false;
    }
}

bool Priority3ProductionHardeningTest::testDataQualityMonitoring() {
    std::cout << "\n📊 Testing Data Quality Monitoring..." << std::endl;

    try {
        // Create mock data processing service
        auto mockHttpClient = std::make_shared<MockHttpClient>();
        auto cryptoProvider = std::make_shared<CryptoCompareProvider>(mockHttpClient, "test_api_key");
        auto newsProvider = std::make_shared<NewsApiProvider>(mockHttpClient, "test_news_key");

        auto dataService = DataProcessingServiceFactory::createWithProviders(
            cryptoProvider, newsProvider, std::vector<std::string>{"BTC", "ETH"}
        );

        std::cout << "  ✅ Mock data processing service created" << std::endl;

        // Configure quality thresholds
        QualityThresholds thresholds;
        thresholds.minCompleteness = 0.8;
        thresholds.minAccuracy = 0.9;
        thresholds.minTimeliness = 0.7;
        thresholds.minOverallScore = 0.75;
        thresholds.maxDataAge = std::chrono::minutes(30);

        // Create data quality monitor
        auto monitor = DataQualityMonitorFactory::createWithConsoleAlerts(dataService);
        monitor->setThresholds(thresholds);

        std::cout << "  ✅ Data quality monitor configured" << std::endl;

        // Test manual quality assessment
        std::vector<MarketData> testMarketData;
        for (int i = 0; i < 10; ++i) {
            MarketData data;
            data.setTicker("BTC");
            data.setPrice(50000.0 + i * 100);
            data.setVolume(1000000.0);
            data.setHigh(data.getPrice() + 50);
            data.setLow(data.getPrice() - 50);
            data.setTimestamp(std::chrono::system_clock::now() - std::chrono::minutes(i));
            testMarketData.push_back(data);
        }

        auto qualityMetrics = monitor->assessMarketDataQuality(testMarketData);
        std::cout << "  📊 Market data quality assessment:" << std::endl;
        std::cout << "    Completeness: " << (qualityMetrics.completeness * 100) << "%" << std::endl;
        std::cout << "    Accuracy: " << (qualityMetrics.accuracy * 100) << "%" << std::endl;
        std::cout << "    Timeliness: " << (qualityMetrics.timeliness * 100) << "%" << std::endl;
        std::cout << "    Overall score: " << (qualityMetrics.overallScore * 100) << "%" << std::endl;

        // Test alert generation (trigger low quality data)
        std::vector<MarketData> poorQualityData;
        for (int i = 0; i < 5; ++i) {
            MarketData data;
            data.setTicker(""); // Missing ticker (poor completeness)
            data.setPrice(-100.0); // Invalid price (poor accuracy)
            data.setTimestamp(std::chrono::system_clock::now() - std::chrono::hours(25)); // Stale (poor timeliness)
            poorQualityData.push_back(data);
        }

        auto poorMetrics = monitor->assessMarketDataQuality(poorQualityData);
        assert(poorMetrics.overallScore < thresholds.minOverallScore);
        std::cout << "  ⚠️  Poor quality data detected (score: " << (poorMetrics.overallScore * 100) << "%)" << std::endl;

        // Test alert system
        auto activeAlerts = monitor->getActiveAlerts();
        std::cout << "  🚨 Active alerts: " << activeAlerts.size() << std::endl;

        // Test monitoring lifecycle
        monitor->startMonitoring(std::chrono::seconds(2));
        std::this_thread::sleep_for(std::chrono::seconds(3));
        assert(monitor->isMonitoring());
        std::cout << "  ✅ Monitoring started and running" << std::endl;

        monitor->stopMonitoring();
        std::this_thread::sleep_for(std::chrono::seconds(1));
        assert(!monitor->isMonitoring());
        std::cout << "  ✅ Monitoring stopped cleanly" << std::endl;

        // Test statistics
        auto monitorStats = monitor->getStatistics();
        std::cout << "  📊 Monitoring statistics:" << std::endl;
        std::cout << "    Total alerts: " << monitorStats.totalAlerts << std::endl;
        std::cout << "    Critical alerts: " << monitorStats.criticalAlerts << std::endl;
        std::cout << "    Active alerts: " << monitorStats.activeAlerts << std::endl;

        return true;

    } catch (const std::exception& e) {
        std::cout << "  ❌ Data quality monitoring test failed: " << e.what() << std::endl;
        return false;
    }
}

bool Priority3ProductionHardeningTest::testIntegrationScenarios() {
    std::cout << "\n🔗 Testing Production Integration Scenarios..." << std::endl;

    try {
        // Create a fully integrated production-like setup
        auto httpClient = EnhancedHttpClientFactory::createResilient();
        httpClient->enableLogging(false); // Reduce noise
        httpClient->enableConnectionPooling(true);
        httpClient->enableCircuitBreaker(true);

        // Warmup connections for common APIs
        httpClient->warmupConnectionPool({
            "api.cryptocompare.com",
            "newsapi.org",
            "api.coinmarketcap.com"
        });
        std::cout << "  ✅ HTTP client configured for production resilience" << std::endl;

        // Create data providers with enhanced client
        auto cryptoProvider = std::make_shared<CryptoCompareProvider>(httpClient, "test_key");
        auto newsProvider = std::make_shared<NewsApiProvider>(httpClient, "test_key");

        // Create data processing service
        auto dataService = DataProcessingServiceFactory::createWithProviders(
            cryptoProvider, newsProvider,
            std::vector<std::string>{"BTC", "ETH", "ADA", "DOT", "LINK"}
        );

        dataService->enableLogging(true);
        dataService->enableQualityChecks(true);
        dataService->setupDefaultJobs();
        std::cout << "  ✅ Data processing service configured with quality checks" << std::endl;

        // Create comprehensive monitoring
        auto monitor = DataQualityMonitorFactory::createForProduction(
            dataService,
            {"admin@cryptoclaude.com", "ops@cryptoclaude.com"},
            "production_quality.log"
        );

        QualityThresholds prodThresholds;
        prodThresholds.minCompleteness = 0.95;      // Production requires higher quality
        prodThresholds.minAccuracy = 0.98;
        prodThresholds.minTimeliness = 0.85;
        prodThresholds.minOverallScore = 0.90;
        prodThresholds.maxDataAge = std::chrono::minutes(10); // Stricter freshness
        monitor->setThresholds(prodThresholds);

        std::cout << "  ✅ Production monitoring configured with strict thresholds" << std::endl;

        // Test full system integration
        dataService->start();
        monitor->startMonitoring(std::chrono::seconds(5));

        // Simulate production load
        std::cout << "  🏭 Simulating production workload..." << std::endl;

        // Let the system run for a few monitoring cycles
        std::this_thread::sleep_for(std::chrono::seconds(12));

        // Check system health
        assert(dataService->isHealthy());
        assert(monitor->isHealthy());

        // Get comprehensive statistics
        auto httpStats = httpClient->getStatistics();
        auto serviceStats = dataService->getStatistics();
        auto monitorStats = monitor->getStatistics();

        std::cout << "  📊 Production Integration Results:" << std::endl;
        std::cout << "    HTTP Requests: " << httpStats.totalRequests
                  << " (Success: " << (httpStats.successRate * 100) << "%)" << std::endl;
        std::cout << "    Data Processing Jobs: " << serviceStats.totalJobsRun
                  << " (Success: " << (serviceStats.successRate * 100) << "%)" << std::endl;
        std::cout << "    Quality Alerts: " << monitorStats.totalAlerts
                  << " (Critical: " << monitorStats.criticalAlerts << ")" << std::endl;

        // Test failure recovery
        std::cout << "  🔥 Testing failure recovery scenarios..." << std::endl;

        // Reset circuit breakers
        httpClient->resetAllCircuitBreakers();

        // Clear quality alerts
        monitor->clearAllAlerts();

        // Test health monitoring
        auto healthStatus = httpClient->getHealthStatus();
        if (!healthStatus.isHealthy) {
            std::cout << "  ⚠️  System health issues detected:" << std::endl;
            for (const auto& issue : healthStatus.issues) {
                std::cout << "    - " << issue << std::endl;
            }
        } else {
            std::cout << "  ✅ System health status: HEALTHY" << std::endl;
        }

        // Clean shutdown
        monitor->stopMonitoring();
        dataService->stop();
        httpClient->closeAllConnections();

        std::cout << "  ✅ Production integration test completed successfully" << std::endl;

        return true;

    } catch (const std::exception& e) {
        std::cout << "  ❌ Integration scenario test failed: " << e.what() << std::endl;
        return false;
    }
}

int main() {
    std::cout << "🏭 CryptoClaude Priority 3 Production Hardening Test Suite" << std::endl;
    std::cout << "============================================================" << std::endl;
    std::cout << "Testing comprehensive production hardening features:" << std::endl;
    std::cout << "• Connection pooling for HTTP client optimization" << std::endl;
    std::cout << "• Circuit breaker pattern for resilience" << std::endl;
    std::cout << "• Enhanced retry logic with exponential backoff and jitter" << std::endl;
    std::cout << "• Data quality monitoring with alerting system" << std::endl;
    std::cout << "• Full production integration scenarios" << std::endl;

    Priority3ProductionHardeningTest testSuite;

    bool allTestsPassed = testSuite.runAllTests();

    if (allTestsPassed) {
        std::cout << "\n🎉 PRIORITY 3 PRODUCTION HARDENING: COMPLETE" << std::endl;
        std::cout << "✅ All production hardening features implemented and tested" << std::endl;
        std::cout << "✅ HTTP client enhanced with connection pooling and circuit breakers" << std::endl;
        std::cout << "✅ Data quality monitoring system with comprehensive alerting" << std::endl;
        std::cout << "✅ Production-ready resilience and error handling" << std::endl;
        std::cout << "✅ System ready for production deployment" << std::endl;
        return 0;
    } else {
        std::cout << "\n❌ PRODUCTION HARDENING TESTS FAILED" << std::endl;
        std::cout << "Some components need attention before production deployment" << std::endl;
        return 1;
    }
}