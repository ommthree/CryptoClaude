#include "CryptoCompareProvider.h"
#include "../Utils/Logger.h"
#include <thread>
#include <chrono>
#include <sstream>
#include <algorithm>
#include <ctime>
#include <iomanip>

namespace CryptoClaude {
namespace DataIngestion {

CryptoCompareProvider::CryptoCompareProvider(const std::string& apiKey)
    : m_apiKey(apiKey),
      m_baseUrl("https://api.cryptocompare.com/data/"),
      m_remainingRequests(100),
      m_rateLimitDelayMs(100) {

    m_httpClient = std::make_unique<Utils::HttpClient>();
    m_rateLimitReset = std::chrono::system_clock::now() + std::chrono::minutes(1);
    m_symbolCacheTime = std::chrono::system_clock::now() - std::chrono::hours(1); // Force initial load
}

bool CryptoCompareProvider::isAvailable() const {
    try {
        // Test with a simple API call
        std::map<std::string, std::string> params;
        params["fsym"] = "BTC";
        params["tsym"] = "USD";
        params["limit"] = "1";

        auto response = m_httpClient->get(buildUrl("histoday", params));

        if (response.statusCode == 200 && !response.body.empty()) {
            // Parse response to check for success
            auto json = Utils::JsonHelper::parse(response.body);
            if (json.contains("Response") && json["Response"] == "Success") {
                return true;
            } else if (json.contains("Message")) {
                m_lastError = json["Message"].get<std::string>();
                return false;
            }
        } else if (response.statusCode == 401) {
            m_lastError = "Invalid API key";
            return false;
        } else if (response.statusCode == 429) {
            m_lastError = "Rate limit exceeded";
            return false;
        } else {
            m_lastError = "HTTP Error " + std::to_string(response.statusCode);
            return false;
        }

        return false;
    } catch (const std::exception& e) {
        m_lastError = "Connection test failed: " + std::string(e.what());
        return false;
    }
}

std::vector<Database::Models::MarketData> CryptoCompareProvider::getDailyData(
    const std::string& symbol,
    const std::string& currency,
    int days) {

    std::vector<Database::Models::MarketData> marketData;

    try {
        if (!validateSymbol(symbol) || !validateCurrency(currency) || !validateTimeRange(days)) {
            return marketData;
        }

        waitForRateLimit();

        std::map<std::string, std::string> params;
        params["fsym"] = symbol;
        params["tsym"] = currency;
        params["limit"] = std::to_string(std::min(days, 2000)); // CryptoCompare API limit

        auto json = fetchJson("histoday", params);

        if (json.contains("Response") && json["Response"] == "Success") {
            marketData = parseMarketData(json, symbol);
            logApiRequest("histoday", 200);
        } else {
            handleApiError(json);
        }

    } catch (const std::exception& e) {
        m_lastError = "Failed to fetch daily data: " + std::string(e.what());
        Utils::Logger::error("CryptoCompareProvider::getDailyData - " + m_lastError);
        logApiRequest("histoday", 500);
    }

    return marketData;
}

std::vector<Database::Models::MarketData> CryptoCompareProvider::getHourlyData(
    const std::string& symbol,
    const std::string& currency,
    int hours) {

    std::vector<Database::Models::MarketData> marketData;

    try {
        if (!validateSymbol(symbol) || !validateCurrency(currency)) {
            return marketData;
        }

        waitForRateLimit();

        std::map<std::string, std::string> params;
        params["fsym"] = symbol;
        params["tsym"] = currency;
        params["limit"] = std::to_string(std::min(hours, 2000)); // CryptoCompare API limit

        auto json = fetchJson("histohour", params);

        if (json.contains("Response") && json["Response"] == "Success") {
            marketData = parseMarketData(json, symbol);
            logApiRequest("histohour", 200);
        } else {
            handleApiError(json);
        }

    } catch (const std::exception& e) {
        m_lastError = "Failed to fetch hourly data: " + std::string(e.what());
        Utils::Logger::error("CryptoCompareProvider::getHourlyData - " + m_lastError);
        logApiRequest("histohour", 500);
    }

    return marketData;
}

std::vector<Database::Models::MarketData> CryptoCompareProvider::getDailyDataBatch(
    const std::vector<std::string>& symbols,
    const std::string& currency,
    int days) {

    std::vector<Database::Models::MarketData> allData;

    for (const auto& symbol : symbols) {
        auto symbolData = getDailyData(symbol, currency, days);
        allData.insert(allData.end(), symbolData.begin(), symbolData.end());

        // Add delay between batch requests to respect rate limits
        if (!symbols.empty() && symbol != symbols.back()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(m_rateLimitDelayMs));
        }
    }

    return allData;
}

std::vector<Database::Models::MarketCapData> CryptoCompareProvider::getTopCoinsByMarketCap(int count) {
    std::vector<Database::Models::MarketCapData> marketCapData;

    try {
        waitForRateLimit();

        std::map<std::string, std::string> params;
        params["limit"] = std::to_string(std::min(count, 100)); // CryptoCompare API limit
        params["tsym"] = "USD";

        auto json = fetchJson("top/mktcapfull", params);

        if (json.contains("Response") && json["Response"] == "Success") {
            marketCapData = parseMarketCapData(json);
            logApiRequest("top/mktcapfull", 200);
        } else {
            handleApiError(json);
        }

    } catch (const std::exception& e) {
        m_lastError = "Failed to fetch market cap data: " + std::string(e.what());
        Utils::Logger::error("CryptoCompareProvider::getTopCoinsByMarketCap - " + m_lastError);
        logApiRequest("top/mktcapfull", 500);
    }

    return marketCapData;
}

Database::Models::MarketCapData CryptoCompareProvider::getMarketCap(const std::string& symbol) {
    Database::Models::MarketCapData marketCap;

    try {
        if (!validateSymbol(symbol)) {
            return marketCap;
        }

        waitForRateLimit();

        std::map<std::string, std::string> params;
        params["fsym"] = symbol;
        params["tsym"] = "USD";

        auto json = fetchJson("pricemultifull", params);

        if (json.contains("Response") && json["Response"] == "Success") {
            if (json.contains("RAW") && json["RAW"].contains(symbol) &&
                json["RAW"][symbol].contains("USD")) {
                marketCap = jsonToMarketCap(json["RAW"][symbol]["USD"]);
                marketCap.symbol = symbol;
            }
            logApiRequest("pricemultifull", 200);
        } else {
            handleApiError(json);
        }

    } catch (const std::exception& e) {
        m_lastError = "Failed to fetch market cap for " + symbol + ": " + std::string(e.what());
        Utils::Logger::error("CryptoCompareProvider::getMarketCap - " + m_lastError);
        logApiRequest("pricemultifull", 500);
    }

    return marketCap;
}

std::vector<std::string> CryptoCompareProvider::getSupportedSymbols() {
    // Check cache first
    auto now = std::chrono::system_clock::now();
    if (std::chrono::duration_cast<std::chrono::hours>(now - m_symbolCacheTime).count() < 24 &&
        !m_symbolCache["coins"].empty()) {
        return m_symbolCache["coins"];
    }

    std::vector<std::string> symbols;

    try {
        waitForRateLimit();

        auto json = fetchJson("all/coinlist");

        if (json.contains("Response") && json["Response"] == "Success" && json.contains("Data")) {
            for (const auto& coin : json["Data"].items()) {
                const auto& coinData = coin.value();
                if (coinData.contains("Symbol")) {
                    symbols.push_back(coinData["Symbol"].get<std::string>());
                }
            }

            // Cache the results
            m_symbolCache["coins"] = symbols;
            m_symbolCacheTime = now;

            logApiRequest("all/coinlist", 200);
        } else {
            handleApiError(json);
        }

    } catch (const std::exception& e) {
        m_lastError = "Failed to fetch supported symbols: " + std::string(e.what());
        Utils::Logger::error("CryptoCompareProvider::getSupportedSymbols - " + m_lastError);
        logApiRequest("all/coinlist", 500);
    }

    return symbols;
}

bool CryptoCompareProvider::isSymbolSupported(const std::string& symbol) {
    auto supportedSymbols = getSupportedSymbols();
    return std::find(supportedSymbols.begin(), supportedSymbols.end(), symbol) != supportedSymbols.end();
}

std::chrono::system_clock::time_point CryptoCompareProvider::getEarliestDataDate(const std::string& symbol) {
    // CryptoCompare doesn't provide this info directly, return a reasonable default
    // Bitcoin data starts around 2010, most others around 2013-2015
    if (symbol == "BTC") {
        std::tm tm = {};
        tm.tm_year = 110; // 2010
        tm.tm_mon = 0;    // January
        tm.tm_mday = 1;
        return std::chrono::system_clock::from_time_t(std::mktime(&tm));
    } else {
        std::tm tm = {};
        tm.tm_year = 113; // 2013
        tm.tm_mon = 0;    // January
        tm.tm_mday = 1;
        return std::chrono::system_clock::from_time_t(std::mktime(&tm));
    }
}

std::chrono::system_clock::time_point CryptoCompareProvider::getLatestDataDate(const std::string& symbol) {
    // Return current time as CryptoCompare provides real-time data
    return std::chrono::system_clock::now();
}

void CryptoCompareProvider::setApiKey(const std::string& apiKey) {
    m_apiKey = apiKey;
    clearError();
}

// === PRIVATE HELPER METHODS ===

std::string CryptoCompareProvider::buildUrl(const std::string& endpoint,
                                          const std::map<std::string, std::string>& params) {
    std::string url = m_baseUrl + endpoint;

    bool firstParam = true;
    for (const auto& param : params) {
        url += (firstParam ? "?" : "&") + param.first + "=" + param.second;
        firstParam = false;
    }

    // Add API key if provided
    if (!m_apiKey.empty()) {
        url += (firstParam ? "?" : "&") + std::string("api_key=") + m_apiKey;
    }

    return url;
}

Utils::JsonHelper::Json CryptoCompareProvider::fetchJson(const std::string& endpoint,
                                                        const std::map<std::string, std::string>& params) {
    std::string url = buildUrl(endpoint, params);

    auto response = m_httpClient->get(url);
    updateRateLimit(response);

    if (response.statusCode != 200) {
        throw std::runtime_error("HTTP Error " + std::to_string(response.statusCode) + ": " + response.body);
    }

    return Utils::JsonHelper::parse(response.body);
}

std::vector<Database::Models::MarketData> CryptoCompareProvider::parseMarketData(
    const Utils::JsonHelper::Json& json,
    const std::string& symbol) {

    std::vector<Database::Models::MarketData> marketData;

    if (json.contains("Data") && json["Data"].is_array()) {
        for (const auto& item : json["Data"]) {
            auto data = jsonToMarketData(item, symbol);
            if (data.volume > 0) { // Filter out zero-volume data points
                marketData.push_back(data);
            }
        }
    }

    return marketData;
}

std::vector<Database::Models::MarketCapData> CryptoCompareProvider::parseMarketCapData(
    const Utils::JsonHelper::Json& json) {

    std::vector<Database::Models::MarketCapData> marketCapData;

    if (json.contains("Data") && json["Data"].is_array()) {
        for (const auto& item : json["Data"]) {
            if (item.contains("CoinInfo") && item.contains("RAW") &&
                item["RAW"].contains("USD")) {

                auto marketCap = jsonToMarketCap(item["RAW"]["USD"]);
                marketCap.symbol = item["CoinInfo"]["Name"].get<std::string>();
                marketCap.name = item["CoinInfo"]["FullName"].get<std::string>();

                marketCapData.push_back(marketCap);
            }
        }
    }

    return marketCapData;
}

void CryptoCompareProvider::updateRateLimit(const Utils::HttpResponse& response) {
    // Check rate limit headers if available
    auto it = response.headers.find("X-RateLimit-Remaining");
    if (it != response.headers.end()) {
        try {
            m_remainingRequests = std::stoi(it->second);
        } catch (...) {
            // Ignore parsing errors
        }
    }

    auto resetIt = response.headers.find("X-RateLimit-Reset");
    if (resetIt != response.headers.end()) {
        try {
            auto resetTime = std::stoi(resetIt->second);
            m_rateLimitReset = std::chrono::system_clock::from_time_t(resetTime);
        } catch (...) {
            // Ignore parsing errors
        }
    }
}

void CryptoCompareProvider::handleApiError(const Utils::JsonHelper::Json& json) {
    if (json.contains("Message")) {
        m_lastError = json["Message"].get<std::string>();
    } else if (json.contains("Response") && json["Response"] == "Error") {
        m_lastError = "API returned error response";
    } else {
        m_lastError = "Unknown API error";
    }

    Utils::Logger::warning("CryptoCompareProvider API Error: " + m_lastError);
}

void CryptoCompareProvider::waitForRateLimit() {
    auto now = std::chrono::system_clock::now();

    // Check if we're hitting rate limits
    if (m_remainingRequests <= 5 && now < m_rateLimitReset) {
        auto waitTime = std::chrono::duration_cast<std::chrono::milliseconds>(m_rateLimitReset - now);
        if (waitTime.count() > 0 && waitTime.count() < 60000) { // Don't wait more than 1 minute
            Utils::Logger::info("Rate limit approaching, waiting " +
                               std::to_string(waitTime.count()) + "ms");
            std::this_thread::sleep_for(waitTime);
        }
    }

    // Always add a small delay to be respectful
    if (m_rateLimitDelayMs > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(m_rateLimitDelayMs));
    }
}

Database::Models::MarketData CryptoCompareProvider::jsonToMarketData(const Utils::JsonHelper::Json& item,
                                                                     const std::string& symbol) {
    Database::Models::MarketData data;

    data.symbol = symbol;
    data.timestamp = std::chrono::system_clock::from_time_t(item.value("time", 0));
    data.open = item.value("open", 0.0);
    data.high = item.value("high", 0.0);
    data.low = item.value("low", 0.0);
    data.close = item.value("close", 0.0);
    data.volume = item.value("volumeto", 0.0); // Volume in quote currency
    data.source = "cryptocompare";

    return data;
}

Database::Models::MarketCapData CryptoCompareProvider::jsonToMarketCap(const Utils::JsonHelper::Json& item) {
    Database::Models::MarketCapData marketCap;

    marketCap.price = item.value("PRICE", 0.0);
    marketCap.marketCap = item.value("MKTCAP", 0.0);
    marketCap.volume24h = item.value("VOLUME24HOUR", 0.0);
    marketCap.change24h = item.value("CHANGE24HOUR", 0.0);
    marketCap.changePct24h = item.value("CHANGEPCT24HOUR", 0.0);
    marketCap.supply = item.value("SUPPLY", 0.0);
    marketCap.maxSupply = item.value("TOTALTOPTIERVOLUME24H", 0.0); // Approximation
    marketCap.timestamp = std::chrono::system_clock::from_time_t(item.value("LASTUPDATE", 0));
    marketCap.source = "cryptocompare";

    return marketCap;
}

bool CryptoCompareProvider::validateSymbol(const std::string& symbol) const {
    if (symbol.empty() || symbol.length() > 10) {
        m_lastError = "Invalid symbol: " + symbol;
        return false;
    }

    // Basic validation - only alphanumeric characters
    for (char c : symbol) {
        if (!std::isalnum(c)) {
            m_lastError = "Invalid symbol format: " + symbol;
            return false;
        }
    }

    return true;
}

bool CryptoCompareProvider::validateCurrency(const std::string& currency) const {
    if (currency.empty() || currency.length() > 10) {
        m_lastError = "Invalid currency: " + currency;
        return false;
    }

    // Basic validation - only alphanumeric characters
    for (char c : currency) {
        if (!std::isalnum(c)) {
            m_lastError = "Invalid currency format: " + currency;
            return false;
        }
    }

    return true;
}

bool CryptoCompareProvider::validateTimeRange(int days) const {
    if (days <= 0 || days > 2000) {
        m_lastError = "Invalid time range: " + std::to_string(days) + " days";
        return false;
    }
    return true;
}

void CryptoCompareProvider::logApiRequest(const std::string& endpoint, int responseCode) const {
    std::string logMessage = "CryptoCompare API " + endpoint + " - Status: " + std::to_string(responseCode);
    if (responseCode == 200) {
        Utils::Logger::debug(logMessage);
    } else {
        Utils::Logger::warning(logMessage);
    }
}

} // namespace DataIngestion
} // namespace CryptoClaude