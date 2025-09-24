#include "CryptoCompareProvider.h"
#include "../../Json/JsonHelper.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <thread>

namespace CryptoClaude {
namespace Data {
namespace Providers {

using json = CryptoClaude::Json::json;
using CryptoClaude::Json::JsonHelper;

CryptoCompareProvider::CryptoCompareProvider(std::shared_ptr<IHttpClient> httpClient,
                                           const std::string& apiKey,
                                           const std::string& baseUrl)
    : httpClient_(httpClient), apiKey_(apiKey), baseUrl_(baseUrl),
      enableLogging_(false), requestsThisSecond_(0),
      totalRequests_(0), successfulRequests_(0), failedRequests_(0) {

    lastRequestTime_ = std::chrono::system_clock::now() - std::chrono::seconds(1);

    if (!httpClient_) {
        throw std::invalid_argument("HttpClient cannot be null");
    }
}

ProviderResponse CryptoCompareProvider::getHistoricalDaily(const MarketDataRequest& request) {
    if (!isConfigured()) {
        return createErrorResponse("CryptoCompare provider not configured with API key");
    }

    if (!isValidSymbol(request.symbol)) {
        return createErrorResponse("Invalid symbol: " + request.symbol);
    }

    enforceRateLimit();

    try {
        auto httpRequest = buildHistoricalRequest("histoday", request);

        if (enableLogging_) {
            logRequest(httpRequest);
        }

        auto startTime = std::chrono::high_resolution_clock::now();
        auto httpResponse = httpClient_->execute(httpRequest);
        auto endTime = std::chrono::high_resolution_clock::now();

        auto responseTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        bool success = httpResponse.isSuccess();

        updateStatistics(success, responseTime);

        if (enableLogging_) {
            logResponse(httpResponse, success);
        }

        return parseHistoricalResponse(httpResponse, request);

    } catch (const std::exception& e) {
        updateStatistics(false, std::chrono::milliseconds(0));
        return createErrorResponse("Request failed: " + std::string(e.what()));
    }
}

ProviderResponse CryptoCompareProvider::getHistoricalHourly(const MarketDataRequest& request) {
    if (!isConfigured()) {
        return createErrorResponse("CryptoCompare provider not configured with API key");
    }

    enforceRateLimit();

    try {
        auto httpRequest = buildHistoricalRequest("histohour", request);
        auto httpResponse = httpClient_->execute(httpRequest);

        auto responseTime = std::chrono::milliseconds(100); // Placeholder
        updateStatistics(httpResponse.isSuccess(), responseTime);

        return parseHistoricalResponse(httpResponse, request);

    } catch (const std::exception& e) {
        updateStatistics(false, std::chrono::milliseconds(0));
        return createErrorResponse("Request failed: " + std::string(e.what()));
    }
}

ProviderResponse CryptoCompareProvider::getCurrentPrice(const std::string& symbol, const std::string& toSymbol) {
    if (!isConfigured()) {
        return createErrorResponse("CryptoCompare provider not configured with API key");
    }

    enforceRateLimit();

    try {
        auto httpRequest = buildPriceRequest("price", symbol, toSymbol);
        auto httpResponse = httpClient_->execute(httpRequest);

        updateStatistics(httpResponse.isSuccess(), std::chrono::milliseconds(50));

        return parsePriceResponse(httpResponse, symbol);

    } catch (const std::exception& e) {
        updateStatistics(false, std::chrono::milliseconds(0));
        return createErrorResponse("Request failed: " + std::string(e.what()));
    }
}

ProviderResponse CryptoCompareProvider::getMultiplePrices(const std::vector<std::string>& symbols, const std::string& toSymbol) {
    if (!isConfigured()) {
        return createErrorResponse("CryptoCompare provider not configured with API key");
    }

    if (symbols.empty()) {
        return createErrorResponse("No symbols provided");
    }

    enforceRateLimit();

    try {
        // Build request for multiple symbols
        std::string symbolsStr;
        for (size_t i = 0; i < symbols.size(); ++i) {
            if (i > 0) symbolsStr += ",";
            symbolsStr += symbols[i];
        }

        HttpRequest request(HttpMethod::GET, baseUrl_ + "/pricemulti");
        request.setApiKeyParam(apiKey_)
               .addQueryParam("fsyms", symbolsStr)
               .addQueryParam("tsyms", toSymbol);

        auto httpResponse = httpClient_->execute(request);
        updateStatistics(httpResponse.isSuccess(), std::chrono::milliseconds(100));

        return parseMultiplePriceResponse(httpResponse, symbols);

    } catch (const std::exception& e) {
        updateStatistics(false, std::chrono::milliseconds(0));
        return createErrorResponse("Request failed: " + std::string(e.what()));
    }
}

HttpRequest CryptoCompareProvider::buildHistoricalRequest(const std::string& endpoint, const MarketDataRequest& request) const {
    std::string url = baseUrl_ + "/" + endpoint;

    HttpRequest httpRequest(HttpMethod::GET, url);
    httpRequest.setApiKeyParam(apiKey_)
               .addQueryParam("fsym", request.symbol)
               .addQueryParam("tsym", request.toSymbol)
               .addQueryParam("limit", std::to_string(request.limit))
               .addQueryParam("aggregate", request.aggregate);

    if (!request.exchange.empty()) {
        httpRequest.addQueryParam("e", request.exchange);
    }

    if (request.tryConversion) {
        httpRequest.addQueryParam("tryConversion", "true");
    }

    return httpRequest;
}

HttpRequest CryptoCompareProvider::buildPriceRequest(const std::string& endpoint, const std::string& symbol, const std::string& toSymbol) const {
    std::string url = baseUrl_ + "/" + endpoint;

    HttpRequest httpRequest(HttpMethod::GET, url);
    httpRequest.setApiKeyParam(apiKey_)
               .addQueryParam("fsym", symbol)
               .addQueryParam("tsyms", toSymbol);

    return httpRequest;
}

ProviderResponse CryptoCompareProvider::parseHistoricalResponse(const HttpResponse& httpResponse, const MarketDataRequest& originalRequest) const {
    ProviderResponse response;
    response.timestamp = std::chrono::system_clock::now();
    response.httpStatusCode = httpResponse.getStatusCode();

    if (!httpResponse.isSuccess()) {
        response.success = false;
        response.errorMessage = "HTTP " + std::to_string(httpResponse.getStatusCode()) + ": " + httpResponse.getBody();
        return response;
    }

    try {
        json jsonData;
        if (!JsonHelper::parseString(httpResponse.getBody(), jsonData)) {
            response.success = false;
            response.errorMessage = "Failed to parse JSON response";
            return response;
        }

        std::string apiResponse = JsonHelper::getString(jsonData, "Response", "Success");
        if (apiResponse != "Success") {
            response.success = false;
            response.errorMessage = JsonHelper::getString(jsonData, "Message", "Unknown API error");
            return response;
        }

        if (JsonHelper::hasKey(jsonData, "Data") && JsonHelper::isObject(jsonData, "Data")) {
            const auto& dataObj = jsonData["Data"];
            if (JsonHelper::hasKey(dataObj, "Data") && JsonHelper::isArray(dataObj, "Data")) {
                response.data = createMarketDataListFromJson(originalRequest.symbol, dataObj["Data"]);
            }
        }

        response.success = true;

        // Check for rate limit info
        if (JsonHelper::hasKey(jsonData, "RateLimit") && JsonHelper::isObject(jsonData, "RateLimit")) {
            const auto& rateLimit = jsonData["RateLimit"];
            response.rateLimitRemaining = JsonHelper::getDouble(rateLimit, "CallsLeft", -1.0);
        }

    } catch (const std::exception& e) {
        response.success = false;
        response.errorMessage = "JSON parse error: " + std::string(e.what());
    } catch (const std::exception& e) {
        response.success = false;
        response.errorMessage = "Data parsing error: " + std::string(e.what());
    }

    return response;
}

ProviderResponse CryptoCompareProvider::parsePriceResponse(const HttpResponse& httpResponse, const std::string& symbol) const {
    ProviderResponse response;
    response.timestamp = std::chrono::system_clock::now();
    response.httpStatusCode = httpResponse.getStatusCode();

    if (!httpResponse.isSuccess()) {
        response.success = false;
        response.errorMessage = "HTTP " + std::to_string(httpResponse.getStatusCode());
        return response;
    }

    try {
        json jsonData = json::parse(httpResponse.getBody());

        if (jsonData.contains("USD")) {
            double price = jsonData["USD"].get<double>();

            MarketData marketData(symbol, std::chrono::system_clock::now(),
                                price, 0.0, 0.0); // Price only, no volume data

            response.data.push_back(marketData);
            response.success = true;
        } else {
            response.success = false;
            response.errorMessage = "Price data not found in response";
        }

    } catch (const std::exception& e) {
        response.success = false;
        response.errorMessage = "Price parsing error: " + std::string(e.what());
    }

    return response;
}

ProviderResponse CryptoCompareProvider::parseMultiplePriceResponse(const HttpResponse& httpResponse, const std::vector<std::string>& symbols) const {
    ProviderResponse response;
    response.timestamp = std::chrono::system_clock::now();
    response.httpStatusCode = httpResponse.getStatusCode();

    if (!httpResponse.isSuccess()) {
        response.success = false;
        response.errorMessage = "HTTP " + std::to_string(httpResponse.getStatusCode());
        return response;
    }

    try {
        json jsonData = json::parse(httpResponse.getBody());

        for (const auto& symbol : symbols) {
            if (jsonData.contains(symbol) && jsonData[symbol].contains("USD")) {
                double price = jsonData[symbol]["USD"].get<double>();

                MarketData marketData(symbol, std::chrono::system_clock::now(),
                                    price, 0.0, 0.0);

                response.data.push_back(marketData);
            }
        }

        response.success = !response.data.empty();

        if (!response.success) {
            response.errorMessage = "No valid price data found for any symbol";
        }

    } catch (const std::exception& e) {
        response.success = false;
        response.errorMessage = "Multiple price parsing error: " + std::string(e.what());
    }

    return response;
}

std::vector<MarketData> CryptoCompareProvider::createMarketDataListFromJson(const std::string& symbol, const nlohmann::json& jsonArray) const {
    std::vector<MarketData> result;

    for (const auto& item : jsonArray) {
        try {
            auto timestamp = std::chrono::system_clock::from_time_t(item["time"].get<std::time_t>());
            double close = item["close"].get<double>();
            double volumeFrom = item.value("volumefrom", 0.0);
            double volumeTo = item.value("volumeto", 0.0);

            MarketData marketData(symbol, timestamp, close, volumeFrom, volumeTo);
            result.push_back(marketData);

        } catch (const std::exception& e) {
            // Skip invalid data points but continue processing
            if (enableLogging_) {
                std::cerr << "Skipped invalid data point for " << symbol << ": " << e.what() << std::endl;
            }
        }
    }

    return result;
}

void CryptoCompareProvider::enforceRateLimit() const {
    auto now = std::chrono::system_clock::now();
    auto timeSinceLastRequest = std::chrono::duration_cast<std::chrono::seconds>(now - lastRequestTime_);

    if (timeSinceLastRequest < std::chrono::seconds(1)) {
        if (requestsThisSecond_ >= MAX_REQUESTS_PER_SECOND) {
            auto sleepTime = std::chrono::seconds(1) - timeSinceLastRequest;
            std::this_thread::sleep_for(sleepTime);
            requestsThisSecond_ = 0;
        }
    } else {
        requestsThisSecond_ = 0;
    }

    ++requestsThisSecond_;
    lastRequestTime_ = std::chrono::system_clock::now();
}

void CryptoCompareProvider::updateStatistics(bool success, std::chrono::milliseconds responseTime) const {
    ++totalRequests_;
    if (success) {
        ++successfulRequests_;
    } else {
        ++failedRequests_;
    }
}

CryptoCompareProvider::ProviderStats CryptoCompareProvider::getStatistics() const {
    ProviderStats stats;
    stats.totalRequests = totalRequests_;
    stats.successfulRequests = successfulRequests_;
    stats.failedRequests = failedRequests_;
    stats.successRate = totalRequests_ > 0 ?
        static_cast<double>(successfulRequests_) / totalRequests_ : 0.0;

    return stats;
}

bool CryptoCompareProvider::testConnection() {
    try {
        auto response = getCurrentPrice("BTC", "USD");
        return response.success;
    } catch (...) {
        return false;
    }
}

bool CryptoCompareProvider::isValidSymbol(const std::string& symbol) const {
    if (symbol.empty() || symbol.length() > 10) {
        return false;
    }

    // Check for valid characters (alphanumeric)
    return std::all_of(symbol.begin(), symbol.end(),
        [](char c) { return std::isalnum(c); });
}

ProviderResponse CryptoCompareProvider::createErrorResponse(const std::string& message, int statusCode) const {
    ProviderResponse response;
    response.success = false;
    response.errorMessage = message;
    response.httpStatusCode = statusCode;
    response.timestamp = std::chrono::system_clock::now();
    return response;
}

void CryptoCompareProvider::logRequest(const HttpRequest& request) const {
    if (enableLogging_) {
        std::cout << "[CryptoCompare] Request: " << request.getMethod() << " " << request.buildUrl() << std::endl;
    }
}

void CryptoCompareProvider::logResponse(const HttpResponse& response, bool success) const {
    if (enableLogging_) {
        std::cout << "[CryptoCompare] Response: " << response.getStatusCode()
                  << " (" << (success ? "SUCCESS" : "FAILED") << ")" << std::endl;
    }
}

// Factory implementations
std::unique_ptr<CryptoCompareProvider> CryptoCompareProviderFactory::create(const std::string& apiKey) {
    auto httpClient = std::make_shared<HttpClient>();
    httpClient->setUserAgent("CryptoClaude/1.0");
    return std::make_unique<CryptoCompareProvider>(httpClient, apiKey);
}

std::unique_ptr<CryptoCompareProvider> CryptoCompareProviderFactory::createWithCustomClient(
    std::shared_ptr<IHttpClient> client, const std::string& apiKey) {
    return std::make_unique<CryptoCompareProvider>(client, apiKey);
}

} // namespace Providers
} // namespace Data
} // namespace CryptoClaude