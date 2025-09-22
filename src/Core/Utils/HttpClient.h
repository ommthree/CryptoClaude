#pragma once
#include <string>
#include <map>
#include <vector>
#include <functional>

namespace CryptoClaude {
namespace Utils {

struct HttpResponse {
    int statusCode;
    std::string body;
    std::map<std::string, std::string> headers;
    std::string errorMessage;

    bool isSuccess() const { return statusCode >= 200 && statusCode < 300; }
};

class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    // Basic HTTP operations
    HttpResponse get(const std::string& url);
    HttpResponse post(const std::string& url, const std::string& body);
    HttpResponse put(const std::string& url, const std::string& body);
    HttpResponse delete_(const std::string& url);

    // Configuration
    void setTimeout(int timeoutSeconds);
    void setUserAgent(const std::string& userAgent);
    void addHeader(const std::string& key, const std::string& value);
    void setFollowRedirects(bool follow);

    // Retry mechanism
    void setRetryPolicy(int maxRetries, int initialBackoffMs);

    // Progress callback for long downloads
    using ProgressCallback = std::function<void(size_t downloaded, size_t total)>;
    void setProgressCallback(ProgressCallback callback);

    // Utility methods
    static std::string urlEncode(const std::string& value);
    static std::string buildQueryString(const std::map<std::string, std::string>& params);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;

    HttpResponse performRequest(const std::string& method,
                               const std::string& url,
                               const std::string& body = "");

    HttpResponse performRequestWithRetry(const std::string& method,
                                        const std::string& url,
                                        const std::string& body = "");
};

} // namespace Utils
} // namespace CryptoClaude