#include "src/Core/Http/HttpClient.h"
#include "src/Core/Json/JsonHelper.h"
#include <iostream>

int main() {
    std::cout << "=== API DEBUG TEST ===" << std::endl;

    CryptoClaude::Http::HttpClient client;

    // Test Alpha Vantage
    std::cout << "\n=== Testing Alpha Vantage ===" << std::endl;
    CryptoClaude::Http::HttpRequest avRequest(CryptoClaude::Http::HttpMethod::GET,
        "https://www.alphavantage.co/query");
    avRequest.addQueryParam("function", "CURRENCY_EXCHANGE_RATE")
             .addQueryParam("from_currency", "BTC")
             .addQueryParam("to_currency", "USD")
             .addQueryParam("apikey", "L6Y7WECGSWZFHHGX");

    // Test NewsAPI
    std::cout << "\n=== Testing NewsAPI ===" << std::endl;
    CryptoClaude::Http::HttpRequest newsRequest(CryptoClaude::Http::HttpMethod::GET,
        "https://newsapi.org/v2/everything");
    newsRequest.addQueryParam("q", "bitcoin")
               .addQueryParam("pageSize", "1")
               .addQueryParam("apiKey", getenv("NEWS_API_KEY") ? getenv("NEWS_API_KEY") : "NEWS_API_KEY_NOT_SET");

    auto newsResponse = client.execute(newsRequest);
    std::cout << "Status: " << newsResponse.getStatusCode() << std::endl;
    std::cout << "Response: " << newsResponse.getBody().substr(0, 200) << "..." << std::endl;

    if (newsResponse.isSuccess()) {
        auto json = CryptoClaude::Json::JsonHelper::parse(newsResponse.getBody());
        std::cout << "Contains 'articles': " << json.contains("articles") << std::endl;
        if (json.contains("articles") && json["articles"].is_array()) {
            std::cout << "Articles count: " << json["articles"].size() << std::endl;
        }
        if (json.contains("status")) {
            std::cout << "Status: " << CryptoClaude::Json::JsonHelper::getString(json, "status") << std::endl;
        }
    }

    auto avResponse = client.execute(avRequest);
    std::cout << "Status: " << avResponse.getStatusCode() << std::endl;
    std::cout << "Response: " << avResponse.getBody().substr(0, 200) << "..." << std::endl;

    if (avResponse.isSuccess()) {
        auto json = CryptoClaude::Json::JsonHelper::parse(avResponse.getBody());
        std::cout << "Contains 'Realtime Currency Exchange Rate': " << json.contains("Realtime Currency Exchange Rate") << std::endl;
        if (json.contains("Error Message")) {
            std::cout << "Error: " << CryptoClaude::Json::JsonHelper::getString(json, "Error Message") << std::endl;
        }
    }

    // Test CryptoNews
    std::cout << "\n=== Testing CryptoNews ===" << std::endl;
    CryptoClaude::Http::HttpRequest cnRequest(CryptoClaude::Http::HttpMethod::GET,
        "https://cryptonews-api.com/api/v1");
    cnRequest.addQueryParam("tickers", "BTC")
             .addQueryParam("items", "1")
             .addQueryParam("page", "1")
             .addQueryParam("token", getenv("CRYPTONEWS_API_KEY") ? getenv("CRYPTONEWS_API_KEY") : "CRYPTONEWS_KEY_NOT_SET");

    auto cnResponse = client.execute(cnRequest);
    std::cout << "Status: " << cnResponse.getStatusCode() << std::endl;
    std::cout << "Response: " << cnResponse.getBody().substr(0, 200) << "..." << std::endl;

    if (cnResponse.isSuccess()) {
        auto json = CryptoClaude::Json::JsonHelper::parse(cnResponse.getBody());
        std::cout << "Contains 'data': " << json.contains("data") << std::endl;
        std::cout << "Data is array: " << (json.contains("data") && json["data"].is_array()) << std::endl;
        if (json.contains("data") && json["data"].is_array()) {
            std::cout << "Array size: " << json["data"].size() << std::endl;
            std::cout << "Array empty: " << json["data"].empty() << std::endl;
        }
    }

    return 0;
}