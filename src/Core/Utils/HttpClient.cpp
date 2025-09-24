#include "HttpClient.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <thread>
#include <chrono>

namespace CryptoClaude {
namespace Utils {

// PIMPL implementation to hide platform-specific details
class HttpClient::Impl {
public:
    int timeoutSeconds = 30;
    std::string userAgent = "CryptoClaude/1.0";
    std::map<std::string, std::string> defaultHeaders;
    bool followRedirects = true;
    int maxRetries = 3;
    int initialBackoffMs = 1000;
    ProgressCallback progressCallback;

    Impl() {
        // Initialize any platform-specific resources here
        defaultHeaders["User-Agent"] = userAgent;
    }

    ~Impl() {
        // Cleanup platform-specific resources here
    }
};

HttpClient::HttpClient() : m_impl(std::make_unique<Impl>()) {
}

HttpClient::~HttpClient() = default;

void HttpClient::setTimeout(int timeoutSeconds) {
    m_impl->timeoutSeconds = timeoutSeconds;
}

void HttpClient::setUserAgent(const std::string& userAgent) {
    m_impl->userAgent = userAgent;
    m_impl->defaultHeaders["User-Agent"] = userAgent;
}

void HttpClient::addHeader(const std::string& key, const std::string& value) {
    m_impl->defaultHeaders[key] = value;
}

void HttpClient::setFollowRedirects(bool follow) {
    m_impl->followRedirects = follow;
}

void HttpClient::setRetryPolicy(int maxRetries, int initialBackoffMs) {
    m_impl->maxRetries = maxRetries;
    m_impl->initialBackoffMs = initialBackoffMs;
}

void HttpClient::setProgressCallback(ProgressCallback callback) {
    m_impl->progressCallback = callback;
}

HttpResponse HttpClient::get(const std::string& url) {
    return performRequestWithRetry("GET", url, "");
}

HttpResponse HttpClient::post(const std::string& url, const std::string& body) {
    return performRequestWithRetry("POST", url, body);
}

HttpResponse HttpClient::put(const std::string& url, const std::string& body) {
    return performRequestWithRetry("PUT", url, body);
}

HttpResponse HttpClient::delete_(const std::string& url) {
    return performRequestWithRetry("DELETE", url, "");
}

HttpResponse HttpClient::performRequestWithRetry(const std::string& method,
                                                const std::string& url,
                                                const std::string& body) {
    int attempts = 0;
    while (attempts <= m_impl->maxRetries) {
        HttpResponse response = performRequest(method, url, body);

        // If successful or on last attempt, return the response
        if (response.isSuccess() || attempts == m_impl->maxRetries) {
            return response;
        }

        // Wait before retry with exponential backoff
        int backoffMs = m_impl->initialBackoffMs * (1 << attempts);
        std::this_thread::sleep_for(std::chrono::milliseconds(backoffMs));

        attempts++;
    }

    // This should never be reached, but just in case
    HttpResponse errorResponse;
    errorResponse.statusCode = 0;
    errorResponse.errorMessage = "Max retries exceeded";
    return errorResponse;
}

HttpResponse HttpClient::performRequest(const std::string& method,
                                      const std::string& url,
                                      const std::string& body) {
    // Mock implementation for now - in production this would use curl or platform-specific HTTP
    HttpResponse response;

    // Log the request for debugging
    std::cout << "HTTP " << method << " " << url;
    if (!body.empty()) {
        std::cout << " [body: " << body.length() << " bytes]";
    }
    std::cout << std::endl;

    // Simulate different responses based on URL patterns for testing
    if (url.find("api.cryptocompare.com") != std::string::npos) {
        // Mock CryptoCompare API response
        response.statusCode = 200;
        response.body = R"({
            "Response": "Success",
            "Message": "Mock CryptoCompare response",
            "Data": {
                "USD": 50000,
                "LASTUPDATE": 1640995200
            }
        })";
        response.headers["Content-Type"] = "application/json";
    }
    else if (url.find("newsapi.org") != std::string::npos) {
        // Mock NewsAPI response
        response.statusCode = 200;
        response.body = R"({
            "status": "ok",
            "totalResults": 1,
            "articles": [
                {
                    "source": {"id": null, "name": "Mock News"},
                    "title": "Mock Bitcoin News Article",
                    "description": "This is a mock news article for testing",
                    "url": "https://example.com/mock-article",
                    "publishedAt": "2024-01-01T12:00:00Z",
                    "content": "Mock article content for testing purposes."
                }
            ]
        })";
        response.headers["Content-Type"] = "application/json";
    }
    else if (url.find("test-error") != std::string::npos) {
        // Simulate error for testing
        response.statusCode = 500;
        response.body = "Internal Server Error";
        response.errorMessage = "Simulated server error";
    }
    else if (url.find("test-timeout") != std::string::npos) {
        // Simulate timeout
        response.statusCode = 0;
        response.errorMessage = "Request timeout";
    }
    else {
        // Generic successful response
        response.statusCode = 200;
        response.body = R"({"status": "success", "message": "Mock HTTP response"})";
        response.headers["Content-Type"] = "application/json";
    }

    return response;
}

std::string HttpClient::urlEncode(const std::string& value) {
    std::ostringstream encoded;
    encoded.fill('0');
    encoded << std::hex;

    for (char c : value) {
        // Keep alphanumeric and ".-~_" intact
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded << c;
        } else {
            // Any other characters are percent-encoded
            encoded << std::uppercase;
            encoded << '%' << std::setw(2) << int(static_cast<unsigned char>(c));
            encoded << std::nouppercase;
        }
    }

    return encoded.str();
}

std::string HttpClient::buildQueryString(const std::map<std::string, std::string>& params) {
    if (params.empty()) {
        return "";
    }

    std::ostringstream query;
    bool first = true;

    for (const auto& param : params) {
        if (!first) {
            query << "&";
        }
        query << urlEncode(param.first) << "=" << urlEncode(param.second);
        first = false;
    }

    return query.str();
}

} // namespace Utils
} // namespace CryptoClaude