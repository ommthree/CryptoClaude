#pragma once

#include <string>
#include <map>
#include <memory>
#include <functional>
#include <vector>
#include <chrono>
#include <optional>

namespace CryptoClaude {
namespace Http {

// Forward declarations
class HttpResponse;
class HttpRequest;

// HTTP method enumeration
enum class HttpMethod {
    GET,
    POST,
    PUT,
    DELETE,
    PATCH
};

// Authentication types
enum class AuthType {
    NONE,
    API_KEY_HEADER,     // X-Api-Key header (NewsAPI)
    API_KEY_PARAM,      // URL parameter (CryptoCompare)
    BEARER_TOKEN,       // Authorization: Bearer token
    BASIC_AUTH          // Authorization: Basic base64
};

// HTTP response structure
class HttpResponse {
private:
    int statusCode_;
    std::string body_;
    std::map<std::string, std::string> headers_;
    std::chrono::milliseconds responseTime_;
    std::string errorMessage_;
    bool success_;

public:
    HttpResponse(int statusCode, const std::string& body,
                const std::map<std::string, std::string>& headers = {},
                std::chrono::milliseconds responseTime = std::chrono::milliseconds(0));

    // Success/failure status
    bool isSuccess() const { return success_; }
    bool isError() const { return !success_; }

    // Response data accessors
    int getStatusCode() const { return statusCode_; }
    const std::string& getBody() const { return body_; }
    const std::map<std::string, std::string>& getHeaders() const { return headers_; }
    std::chrono::milliseconds getResponseTime() const { return responseTime_; }
    const std::string& getErrorMessage() const { return errorMessage_; }

    // Header utilities
    std::optional<std::string> getHeader(const std::string& key) const;
    bool hasHeader(const std::string& key) const;

    // Status code helpers
    bool isSuccessStatus() const { return statusCode_ >= 200 && statusCode_ < 300; }
    bool isClientError() const { return statusCode_ >= 400 && statusCode_ < 500; }
    bool isServerError() const { return statusCode_ >= 500; }
    bool isRedirect() const { return statusCode_ >= 300 && statusCode_ < 400; }

    // Set error information
    void setError(const std::string& errorMessage);
};

// HTTP request configuration
class HttpRequest {
private:
    HttpMethod method_;
    std::string url_;
    std::map<std::string, std::string> headers_;
    std::map<std::string, std::string> queryParams_;
    std::string body_;
    std::chrono::seconds timeout_;

    // Authentication
    AuthType authType_;
    std::string authKey_;
    std::string authSecret_;  // For HMAC or Basic auth
    std::string authHeaderName_;  // Custom header name if needed

public:
    HttpRequest(HttpMethod method, const std::string& url);

    // Method configuration
    HttpRequest& setMethod(HttpMethod method) { method_ = method; return *this; }
    HttpRequest& setUrl(const std::string& url) { url_ = url; return *this; }
    HttpRequest& setBody(const std::string& body) { body_ = body; return *this; }
    HttpRequest& setTimeout(std::chrono::seconds timeout) { timeout_ = timeout; return *this; }

    // Header management
    HttpRequest& addHeader(const std::string& key, const std::string& value);
    HttpRequest& setHeaders(const std::map<std::string, std::string>& headers);

    // Query parameter management
    HttpRequest& addQueryParam(const std::string& key, const std::string& value);
    HttpRequest& setQueryParams(const std::map<std::string, std::string>& params);

    // Authentication configuration
    HttpRequest& setApiKeyHeader(const std::string& apiKey, const std::string& headerName = "X-Api-Key");
    HttpRequest& setApiKeyParam(const std::string& apiKey, const std::string& paramName = "api_key");
    HttpRequest& setBearerToken(const std::string& token);
    HttpRequest& setBasicAuth(const std::string& username, const std::string& password);

    // Accessors
    HttpMethod getMethod() const { return method_; }
    const std::string& getUrl() const { return url_; }
    const std::map<std::string, std::string>& getHeaders() const { return headers_; }
    const std::map<std::string, std::string>& getQueryParams() const { return queryParams_; }
    const std::string& getBody() const { return body_; }
    std::chrono::seconds getTimeout() const { return timeout_; }

    // Build final URL with query parameters
    std::string buildUrl() const;

    // Build headers with authentication
    std::map<std::string, std::string> buildHeaders() const;

private:
    void applyAuthentication();
    std::string urlEncode(const std::string& str) const;
};

// Retry configuration
struct RetryConfig {
    int maxRetries = 3;
    std::chrono::milliseconds baseDelay = std::chrono::milliseconds(1000);  // 1 second base
    double backoffMultiplier = 2.0;  // Exponential backoff
    std::chrono::milliseconds maxDelay = std::chrono::milliseconds(30000);  // 30 second max

    // Conditions for retry
    bool retryOnTimeout = true;
    bool retryOnServerError = true;  // 5xx errors
    bool retryOnClientError = false; // 4xx errors (usually auth/validation issues)
    std::vector<int> retryOnStatusCodes = {429, 502, 503, 504};  // Rate limit, bad gateway, etc.
};

// HTTP client interface for dependency injection and testing
class IHttpClient {
public:
    virtual ~IHttpClient() = default;
    virtual HttpResponse execute(const HttpRequest& request) = 0;
    virtual void setRetryConfig(const RetryConfig& config) = 0;
    virtual void setDefaultTimeout(std::chrono::seconds timeout) = 0;
    virtual void setUserAgent(const std::string& userAgent) = 0;
};

// Main HTTP client implementation
class HttpClient : public IHttpClient {
private:
    RetryConfig retryConfig_;
    std::chrono::seconds defaultTimeout_;
    std::string userAgent_;
    bool enableLogging_;

    // Statistics for monitoring
    mutable std::map<std::string, int> requestCounts_;
    mutable std::map<std::string, std::chrono::milliseconds> averageResponseTimes_;

public:
    explicit HttpClient(const RetryConfig& retryConfig = RetryConfig{});
    virtual ~HttpClient() = default;

    // Configuration
    void setRetryConfig(const RetryConfig& config) override { retryConfig_ = config; }
    void setDefaultTimeout(std::chrono::seconds timeout) override { defaultTimeout_ = timeout; }
    void setTimeout(std::chrono::seconds timeout) { setDefaultTimeout(timeout); } // Compatibility alias
    void setUserAgent(const std::string& userAgent) override { userAgent_ = userAgent; }
    void enableLogging(bool enable) { enableLogging_ = enable; }

    // Main execution method
    HttpResponse execute(const HttpRequest& request) override;

    // Convenience methods for common operations
    HttpResponse get(const std::string& url,
                    const std::map<std::string, std::string>& headers = {});
    HttpResponse post(const std::string& url, const std::string& body,
                     const std::map<std::string, std::string>& headers = {});

    // Specialized methods for our APIs
    HttpResponse getCryptoCompareData(const std::string& endpoint, const std::string& apiKey,
                                     const std::map<std::string, std::string>& params = {});
    HttpResponse getNewsApiData(const std::string& endpoint, const std::string& apiKey,
                               const std::map<std::string, std::string>& params = {});

    // Statistics and monitoring
    std::map<std::string, int> getRequestCounts() const { return requestCounts_; }
    std::map<std::string, std::chrono::milliseconds> getAverageResponseTimes() const { return averageResponseTimes_; }
    void clearStatistics();

    // API call bucketing utilities (for smart batching consideration)
    struct BucketConfig {
        std::chrono::milliseconds bucketInterval = std::chrono::milliseconds(60000);  // 1 minute buckets
        int maxCallsPerBucket = 100;  // Rate limit consideration
        bool enableBucketing = false;  // Future feature flag
    };

    void setBucketConfig(const BucketConfig& config) { bucketConfig_ = config; }

private:
    BucketConfig bucketConfig_;

    // Core implementation methods
    HttpResponse executeWithRetry(const HttpRequest& request);
    HttpResponse executeSingleRequest(const HttpRequest& request);
    bool shouldRetry(const HttpResponse& response, int attemptNumber);
    std::chrono::milliseconds calculateRetryDelay(int attemptNumber);

    // Platform-specific HTTP implementation
    HttpResponse performHttpCall(const HttpRequest& request);

    // Logging and statistics
    void logRequest(const HttpRequest& request) const;
    void logResponse(const HttpResponse& response) const;
    void updateStatistics(const std::string& host, std::chrono::milliseconds responseTime) const;

    // URL utilities
    std::string extractHost(const std::string& url) const;
};

// Factory for creating HTTP clients (useful for testing)
class HttpClientFactory {
public:
    static std::unique_ptr<IHttpClient> create(const RetryConfig& config = RetryConfig{});
    static std::unique_ptr<IHttpClient> createMock();  // For unit tests
};

} // namespace Http
} // namespace CryptoClaude