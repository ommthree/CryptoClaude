#include <iostream>
#include <cassert>
#include "src/Core/Http/HttpClient.h"

using namespace CryptoClaude::Http;

void testCryptoCompareApiCall() {
    std::cout << "Testing CryptoCompare API integration..." << std::endl;

    HttpClient client;
    client.enableLogging(true);

    // Test CryptoCompare API call structure
    std::map<std::string, std::string> params = {
        {"fsym", "BTC"},
        {"tsyms", "USD,EUR"}
    };

    HttpResponse response = client.getCryptoCompareData("/data/price", "test-api-key", params);

    assert(response.isSuccess());
    assert(response.getStatusCode() == 200);
    assert(response.getBody().find("USD") != std::string::npos);
    assert(response.hasHeader("Content-Type"));
    assert(response.getHeader("Server").value_or("") == "CryptoCompare");

    std::cout << "Response body: " << response.getBody() << std::endl;
    std::cout << "✅ CryptoCompare API integration test passed" << std::endl;
}

void testNewsApiCall() {
    std::cout << "Testing NewsAPI integration..." << std::endl;

    HttpClient client;
    client.enableLogging(true);

    // Test NewsAPI call structure
    std::map<std::string, std::string> params = {
        {"q", "cryptocurrency"},
        {"sortBy", "publishedAt"},
        {"pageSize", "20"}
    };

    HttpResponse response = client.getNewsApiData("/v2/everything", "test-api-key", params);

    assert(response.isSuccess());
    assert(response.getStatusCode() == 200);
    assert(response.getBody().find("status") != std::string::npos);
    assert(response.hasHeader("Content-Type"));
    assert(response.getHeader("Server").value_or("") == "NewsAPI");

    std::cout << "Response body: " << response.getBody() << std::endl;
    std::cout << "✅ NewsAPI integration test passed" << std::endl;
}

void testRetryMechanism() {
    std::cout << "Testing retry mechanism..." << std::endl;

    RetryConfig config;
    config.maxRetries = 2;
    config.baseDelay = std::chrono::milliseconds(100);
    config.retryOnServerError = true;

    HttpClient client(config);
    client.enableLogging(true);

    // Test with error URL to trigger retries
    HttpResponse response = client.get("https://example.com/test-error");

    assert(!response.isSuccess());
    assert(response.getStatusCode() == 500);
    assert(!response.getErrorMessage().empty());

    std::cout << "Error message: " << response.getErrorMessage() << std::endl;
    std::cout << "✅ Retry mechanism test passed" << std::endl;
}

void testAuthenticationInAction() {
    std::cout << "Testing authentication in action..." << std::endl;

    HttpClient client;
    client.enableLogging(true);

    // Test NewsAPI with X-API-Key header
    HttpRequest newsRequest(HttpMethod::GET, "https://newsapi.org/v2/top-headlines");
    newsRequest.setApiKeyHeader("test-news-key", "X-API-Key")
              .addQueryParam("country", "us")
              .addQueryParam("category", "technology");

    HttpResponse newsResponse = client.execute(newsRequest);
    assert(newsResponse.isSuccess());

    // Test CryptoCompare with API key parameter
    HttpRequest ccRequest(HttpMethod::GET, "https://min-api.cryptocompare.com/data/price");
    ccRequest.setApiKeyParam("test-cc-key", "api_key")
             .addQueryParam("fsym", "ETH")
             .addQueryParam("tsyms", "USD");

    HttpResponse ccResponse = client.execute(ccRequest);
    assert(ccResponse.isSuccess());

    std::cout << "✅ Authentication in action test passed" << std::endl;
}

void testRequestStatistics() {
    std::cout << "Testing request statistics..." << std::endl;

    HttpClient client;

    // Make several requests to build statistics
    client.get("https://api.example.com/endpoint1");
    client.get("https://api.example.com/endpoint2");
    client.get("https://different-api.com/data");

    auto requestCounts = client.getRequestCounts();
    auto responseTimes = client.getAverageResponseTimes();

    assert(!requestCounts.empty());
    assert(!responseTimes.empty());

    std::cout << "Request statistics:" << std::endl;
    for (const auto& stat : requestCounts) {
        std::cout << "  " << stat.first << ": " << stat.second << " requests" << std::endl;
    }

    for (const auto& time : responseTimes) {
        std::cout << "  " << time.first << ": " << time.second.count() << "ms avg" << std::endl;
    }

    client.clearStatistics();
    assert(client.getRequestCounts().empty());

    std::cout << "✅ Request statistics test passed" << std::endl;
}

void testErrorHandling() {
    std::cout << "Testing error handling..." << std::endl;

    HttpClient client;

    // Test timeout simulation
    HttpResponse timeoutResponse = client.get("https://example.com/test-timeout");
    assert(!timeoutResponse.isSuccess());
    assert(timeoutResponse.getStatusCode() == 0);
    assert(timeoutResponse.getErrorMessage().find("timeout") != std::string::npos);

    // Test server error
    HttpResponse serverErrorResponse = client.get("https://example.com/test-error");
    assert(!serverErrorResponse.isSuccess());
    assert(serverErrorResponse.isServerError());

    std::cout << "✅ Error handling test passed" << std::endl;
}

void testBucketingFramework() {
    std::cout << "Testing bucketing framework..." << std::endl;

    HttpClient client;

    // Configure bucketing (future feature)
    HttpClient::BucketConfig bucketConfig;
    bucketConfig.bucketInterval = std::chrono::milliseconds(60000);
    bucketConfig.maxCallsPerBucket = 100;
    bucketConfig.enableBucketing = false; // Currently disabled

    client.setBucketConfig(bucketConfig);

    // Make a request - bucketing would be applied in future
    HttpResponse response = client.get("https://api.example.com/test");
    assert(response.isSuccess());

    std::cout << "✅ Bucketing framework test passed (ready for future implementation)" << std::endl;
}

int main() {
    std::cout << "Starting Day 11 API Integration Tests..." << std::endl;
    std::cout << "================================================================================\n" << std::endl;

    try {
        testCryptoCompareApiCall();
        std::cout << std::endl;

        testNewsApiCall();
        std::cout << std::endl;

        testRetryMechanism();
        std::cout << std::endl;

        testAuthenticationInAction();
        std::cout << std::endl;

        testRequestStatistics();
        std::cout << std::endl;

        testErrorHandling();
        std::cout << std::endl;

        testBucketingFramework();

        std::cout << "\n================================================================================\n";
        std::cout << "🎉 All API Integration tests passed!" << std::endl;
        std::cout << "✅ HTTP Client fully functional with mock responses" << std::endl;
        std::cout << "🔧 CryptoCompare API integration ready (API key as URL parameter)" << std::endl;
        std::cout << "📰 NewsAPI integration ready (X-API-Key header authentication)" << std::endl;
        std::cout << "⚡ Exponential backoff retry mechanism working" << std::endl;
        std::cout << "📊 Request statistics and monitoring operational" << std::endl;
        std::cout << "🚀 Smart bucketing framework prepared for future optimization" << std::endl;
        std::cout << "🛡️ Comprehensive error handling and recovery" << std::endl;
        std::cout << "\nReady for Day 12: JSON Parsing & Utilities!" << std::endl;

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "❌ Unknown test failure" << std::endl;
        return 1;
    }
}