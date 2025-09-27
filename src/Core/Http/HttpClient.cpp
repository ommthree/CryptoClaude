#include "HttpClient.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <thread>
#include <random>
#include <algorithm>
#include <regex>

// Platform-specific includes
#ifdef _WIN32
#include <windows.h>
#include <wininet.h>
#elif __APPLE__ || __linux__
#include <curl/curl.h>
#endif

namespace CryptoClaude {
namespace Http {

#if defined(__APPLE__) || defined(__linux__)
// cURL callback function for writing response data
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t totalSize = size * nmemb;
    userp->append((char*)contents, totalSize);
    return totalSize;
}

// cURL callback function for writing headers
static size_t HeaderCallback(char* buffer, size_t size, size_t nitems, std::map<std::string, std::string>* headers) {
    size_t totalSize = size * nitems;
    std::string header(buffer, totalSize);

    // Remove trailing CRLF
    if (header.size() >= 2 && header.substr(header.size() - 2) == "\r\n") {
        header = header.substr(0, header.size() - 2);
    }

    // Parse header
    size_t colonPos = header.find(':');
    if (colonPos != std::string::npos && colonPos < header.size() - 1) {
        std::string key = header.substr(0, colonPos);
        std::string value = header.substr(colonPos + 1);

        // Trim whitespace
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);

        (*headers)[key] = value;
    }

    return totalSize;
}
#endif

// ================================
// HttpResponse Implementation
// ================================

HttpResponse::HttpResponse(int statusCode, const std::string& body,
                          const std::map<std::string, std::string>& headers,
                          std::chrono::milliseconds responseTime)
    : statusCode_(statusCode), body_(body), headers_(headers),
      responseTime_(responseTime), success_(false) {

    // Determine success based on status code
    success_ = isSuccessStatus();

    if (!success_ && errorMessage_.empty()) {
        errorMessage_ = "HTTP " + std::to_string(statusCode);
    }
}

std::optional<std::string> HttpResponse::getHeader(const std::string& key) const {
    auto it = headers_.find(key);
    if (it != headers_.end()) {
        return it->second;
    }

    // Case-insensitive search
    for (const auto& header : headers_) {
        std::string lowerKey = key;
        std::string lowerHeaderKey = header.first;
        std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), ::tolower);
        std::transform(lowerHeaderKey.begin(), lowerHeaderKey.end(), lowerHeaderKey.begin(), ::tolower);

        if (lowerKey == lowerHeaderKey) {
            return header.second;
        }
    }

    return std::nullopt;
}

bool HttpResponse::hasHeader(const std::string& key) const {
    return getHeader(key).has_value();
}

void HttpResponse::setError(const std::string& errorMessage) {
    errorMessage_ = errorMessage;
    success_ = false;
}

// ================================
// HttpRequest Implementation
// ================================

HttpRequest::HttpRequest(HttpMethod method, const std::string& url)
    : method_(method), url_(url), timeout_(std::chrono::seconds(30)),
      authType_(AuthType::NONE) {

    // Set default headers
    addHeader("User-Agent", "CryptoClaude/1.0");
    addHeader("Accept", "application/json");
    addHeader("Connection", "close");
}

HttpRequest& HttpRequest::addHeader(const std::string& key, const std::string& value) {
    headers_[key] = value;
    return *this;
}

HttpRequest& HttpRequest::setHeaders(const std::map<std::string, std::string>& headers) {
    headers_ = headers;
    return *this;
}

HttpRequest& HttpRequest::addQueryParam(const std::string& key, const std::string& value) {
    queryParams_[key] = value;
    return *this;
}

HttpRequest& HttpRequest::setQueryParams(const std::map<std::string, std::string>& params) {
    queryParams_ = params;
    return *this;
}

HttpRequest& HttpRequest::setApiKeyHeader(const std::string& apiKey, const std::string& headerName) {
    authType_ = AuthType::API_KEY_HEADER;
    authKey_ = apiKey;
    authHeaderName_ = headerName;
    applyAuthentication();
    return *this;
}

HttpRequest& HttpRequest::setApiKeyParam(const std::string& apiKey, const std::string& paramName) {
    authType_ = AuthType::API_KEY_PARAM;
    authKey_ = apiKey;
    authHeaderName_ = paramName; // Reuse for param name
    applyAuthentication();
    return *this;
}

HttpRequest& HttpRequest::setBearerToken(const std::string& token) {
    authType_ = AuthType::BEARER_TOKEN;
    authKey_ = token;
    applyAuthentication();
    return *this;
}

HttpRequest& HttpRequest::setBasicAuth(const std::string& username, const std::string& password) {
    authType_ = AuthType::BASIC_AUTH;
    authKey_ = username;
    authSecret_ = password;
    applyAuthentication();
    return *this;
}

void HttpRequest::applyAuthentication() {
    switch (authType_) {
        case AuthType::API_KEY_HEADER: {
            std::string headerName = authHeaderName_.empty() ? "X-Api-Key" : authHeaderName_;
            headers_[headerName] = authKey_;
            break;
        }
        case AuthType::API_KEY_PARAM: {
            std::string paramName = authHeaderName_.empty() ? "api_key" : authHeaderName_;
            queryParams_[paramName] = authKey_;
            break;
        }
        case AuthType::BEARER_TOKEN:
            headers_["Authorization"] = "Bearer " + authKey_;
            break;
        case AuthType::BASIC_AUTH: {
            // Basic auth requires base64 encoding
            std::string credentials = authKey_ + ":" + authSecret_;
            // Simple base64 implementation (for production, use proper library)
            std::string encoded = "Basic " + credentials; // Simplified - need proper base64
            headers_["Authorization"] = encoded;
            break;
        }
        case AuthType::NONE:
        default:
            break;
    }
}

std::string HttpRequest::buildUrl() const {
    if (queryParams_.empty()) {
        return url_;
    }

    std::ostringstream oss;
    oss << url_;

    // Check if URL already has query parameters
    if (url_.find('?') == std::string::npos) {
        oss << "?";
    } else {
        oss << "&";
    }

    bool first = true;
    for (const auto& param : queryParams_) {
        if (!first) {
            oss << "&";
        }
        oss << urlEncode(param.first) << "=" << urlEncode(param.second);
        first = false;
    }

    return oss.str();
}

std::map<std::string, std::string> HttpRequest::buildHeaders() const {
    auto finalHeaders = headers_;

    // Content-Type for POST/PUT requests with body
    if (!body_.empty() && (method_ == HttpMethod::POST || method_ == HttpMethod::PUT)) {
        if (finalHeaders.find("Content-Type") == finalHeaders.end()) {
            finalHeaders["Content-Type"] = "application/json";
        }
    }

    return finalHeaders;
}

std::string HttpRequest::urlEncode(const std::string& str) const {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : str) {
        // Keep alphanumeric and specific symbols
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << '%' << std::setw(2) << int((unsigned char)c);
        }
    }

    return escaped.str();
}

// ================================
// HttpClient Implementation
// ================================

HttpClient::HttpClient(const RetryConfig& retryConfig)
    : retryConfig_(retryConfig), defaultTimeout_(std::chrono::seconds(30)),
      userAgent_("CryptoClaude/1.0"), enableLogging_(false) {

    // For production, would initialize platform-specific HTTP libraries here
    // Currently using mock implementation for Day 11 testing
}

HttpResponse HttpClient::execute(const HttpRequest& request) {
    if (enableLogging_) {
        logRequest(request);
    }

    auto startTime = std::chrono::steady_clock::now();
    HttpResponse response = executeWithRetry(request);
    auto endTime = std::chrono::steady_clock::now();

    auto responseTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    // Update statistics
    std::string host = extractHost(request.getUrl());
    updateStatistics(host, responseTime);

    if (enableLogging_) {
        logResponse(response);
    }

    return response;
}

HttpResponse HttpClient::executeWithRetry(const HttpRequest& request) {
    HttpResponse lastResponse(0, "");

    for (int attempt = 0; attempt <= retryConfig_.maxRetries; ++attempt) {
        lastResponse = executeSingleRequest(request);

        if (lastResponse.isSuccess() || !shouldRetry(lastResponse, attempt)) {
            break;
        }

        if (attempt < retryConfig_.maxRetries) {
            auto delay = calculateRetryDelay(attempt);
            if (enableLogging_) {
                std::cout << "Retrying request in " << delay.count() << "ms (attempt "
                         << (attempt + 1) << "/" << retryConfig_.maxRetries << ")" << std::endl;
            }
            std::this_thread::sleep_for(delay);
        }
    }

    return lastResponse;
}

HttpResponse HttpClient::executeSingleRequest(const HttpRequest& request) {
    // Delegate to platform-specific implementation
    return performHttpCall(request);
}

bool HttpClient::shouldRetry(const HttpResponse& response, int attemptNumber) {
    if (attemptNumber >= retryConfig_.maxRetries) {
        return false;
    }

    int statusCode = response.getStatusCode();

    // Check specific status codes
    for (int retryCode : retryConfig_.retryOnStatusCodes) {
        if (statusCode == retryCode) {
            return true;
        }
    }

    // Check error categories
    if (retryConfig_.retryOnServerError && response.isServerError()) {
        return true;
    }

    if (retryConfig_.retryOnClientError && response.isClientError()) {
        return true;
    }

    // Check for timeout (statusCode 0 typically indicates network error)
    if (retryConfig_.retryOnTimeout && statusCode == 0) {
        return true;
    }

    return false;
}

std::chrono::milliseconds HttpClient::calculateRetryDelay(int attemptNumber) {
    auto delay = std::chrono::duration_cast<std::chrono::milliseconds>(
        retryConfig_.baseDelay * std::pow(retryConfig_.backoffMultiplier, attemptNumber)
    );

    // Add jitter to avoid thundering herd
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> jitter(0.8, 1.2);
    delay = std::chrono::milliseconds(static_cast<long long>(delay.count() * jitter(gen)));

    return std::min(delay, retryConfig_.maxDelay);
}

// Convenience methods
HttpResponse HttpClient::get(const std::string& url,
                            const std::map<std::string, std::string>& headers) {
    HttpRequest request(HttpMethod::GET, url);
    request.setHeaders(headers);
    return execute(request);
}

HttpResponse HttpClient::post(const std::string& url, const std::string& body,
                             const std::map<std::string, std::string>& headers) {
    HttpRequest request(HttpMethod::POST, url);
    request.setBody(body).setHeaders(headers);
    return execute(request);
}

// Specialized API methods
HttpResponse HttpClient::getCryptoCompareData(const std::string& endpoint, const std::string& apiKey,
                                             const std::map<std::string, std::string>& params) {
    std::string baseUrl = "https://min-api.cryptocompare.com";
    HttpRequest request(HttpMethod::GET, baseUrl + endpoint);

    // CryptoCompare uses API key as URL parameter
    request.setApiKeyParam(apiKey, "api_key");

    // Add additional parameters
    for (const auto& param : params) {
        request.addQueryParam(param.first, param.second);
    }

    return execute(request);
}

HttpResponse HttpClient::getNewsApiData(const std::string& endpoint, const std::string& apiKey,
                                       const std::map<std::string, std::string>& params) {
    std::string baseUrl = "https://newsapi.org";
    HttpRequest request(HttpMethod::GET, baseUrl + endpoint);

    // NewsAPI uses API key in header
    request.setApiKeyHeader(apiKey, "X-API-Key");

    // Add additional parameters
    for (const auto& param : params) {
        request.addQueryParam(param.first, param.second);
    }

    return execute(request);
}

// Statistics management
void HttpClient::clearStatistics() {
    requestCounts_.clear();
    averageResponseTimes_.clear();
}

void HttpClient::updateStatistics(const std::string& host, std::chrono::milliseconds responseTime) const {
    requestCounts_[host]++;

    // Update rolling average
    if (averageResponseTimes_.find(host) == averageResponseTimes_.end()) {
        averageResponseTimes_[host] = responseTime;
    } else {
        auto currentAvg = averageResponseTimes_[host];
        auto count = requestCounts_.at(host);
        averageResponseTimes_[host] = std::chrono::milliseconds(
            (currentAvg.count() * (count - 1) + responseTime.count()) / count
        );
    }
}

// Logging methods
void HttpClient::logRequest(const HttpRequest& request) const {
    std::cout << "[HTTP] " << (request.getMethod() == HttpMethod::GET ? "GET" : "POST")
              << " " << request.buildUrl() << std::endl;
}

void HttpClient::logResponse(const HttpResponse& response) const {
    std::cout << "[HTTP] Response: " << response.getStatusCode()
              << " (" << response.getResponseTime().count() << "ms)" << std::endl;
}

std::string HttpClient::extractHost(const std::string& url) const {
    std::regex hostRegex(R"(https?://([^/]+))");
    std::smatch match;
    if (std::regex_search(url, match, hostRegex) && match.size() > 1) {
        return match[1].str();
    }
    return "unknown";
}

// Platform-specific HTTP implementation
HttpResponse HttpClient::performHttpCall(const HttpRequest& request) {
#if defined(__APPLE__) || defined(__linux__)
    // Real cURL implementation
    auto start_time = std::chrono::steady_clock::now();

    CURL* curl = curl_easy_init();
    if (!curl) {
        HttpResponse response(0, "");
        response.setError("Failed to initialize cURL");
        return response;
    }

    std::string responseBody;
    std::map<std::string, std::string> responseHeaders;
    char errorBuffer[CURL_ERROR_SIZE];

    // Build URL and headers
    std::string url = request.buildUrl();
    auto headers = request.buildHeaders();

    if (enableLogging_) {
        std::cout << "[HTTP] Real cURL call: " << url << std::endl;
    }

    // Set basic cURL options
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, HeaderCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &responseHeaders);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(request.getTimeout().count()));
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3L);

    // Set User-Agent
    if (!userAgent_.empty()) {
        curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent_.c_str());
    }

    // Set headers
    struct curl_slist* headerList = nullptr;
    for (const auto& header : headers) {
        std::string headerStr = header.first + ": " + header.second;
        headerList = curl_slist_append(headerList, headerStr.c_str());
    }
    if (headerList) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
    }

    // Set method and body
    switch (request.getMethod()) {
        case HttpMethod::GET:
            // Default is GET
            break;
        case HttpMethod::POST:
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            if (!request.getBody().empty()) {
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.getBody().c_str());
            }
            break;
        case HttpMethod::PUT:
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
            if (!request.getBody().empty()) {
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.getBody().c_str());
            }
            break;
        case HttpMethod::DELETE:
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
            break;
        case HttpMethod::PATCH:
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
            if (!request.getBody().empty()) {
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.getBody().c_str());
            }
            break;
    }

    // Perform the request
    CURLcode res = curl_easy_perform(curl);

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // Get response code
    long responseCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);

    // Clean up
    if (headerList) {
        curl_slist_free_all(headerList);
    }
    curl_easy_cleanup(curl);

    // Handle cURL errors
    if (res != CURLE_OK) {
        HttpResponse response(0, "");
        std::string errorMsg = "cURL error: ";
        errorMsg += (strlen(errorBuffer) > 0) ? errorBuffer : curl_easy_strerror(res);
        response.setError(errorMsg);
        return response;
    }

    return HttpResponse(static_cast<int>(responseCode), responseBody, responseHeaders, duration);

#else
    // Fallback for unsupported platforms
    HttpResponse response(500, "");
    response.setError("HTTP client not implemented for this platform");
    return response;
#endif
}

// ================================
// HttpClientFactory Implementation
// ================================

std::unique_ptr<IHttpClient> HttpClientFactory::create(const RetryConfig& config) {
    return std::make_unique<HttpClient>(config);
}

std::unique_ptr<IHttpClient> HttpClientFactory::createMock() {
    // TODO: Implement mock client for testing
    return std::make_unique<HttpClient>();
}

} // namespace Http
} // namespace CryptoClaude