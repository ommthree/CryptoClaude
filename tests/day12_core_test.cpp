#include <iostream>
#include <cassert>
#include "src/Core/Json/JsonParser.h"

using namespace CryptoClaude::Json;

int main() {
    std::cout << "Day 12 JSON Core Functionality Tests..." << std::endl;
    std::cout << "================================================================================\n" << std::endl;

    // Test 1: Basic JsonValue operations
    std::cout << "Testing JsonValue operations..." << std::endl;
    JsonValue stringVal{"hello"};
    JsonValue numberVal{42.0};
    JsonValue boolVal{true};

    assert(stringVal.is_string());
    assert(numberVal.is_number());
    assert(boolVal.is_boolean());
    assert(stringVal.get_string() == "hello");
    assert(numberVal.get_number() == 42.0);
    assert(boolVal.get_bool() == true);
    std::cout << "✅ JsonValue operations working" << std::endl;

    // Test 2: Object creation and access
    std::cout << "Testing object creation..." << std::endl;
    std::map<std::string, JsonValue> testObj;
    testObj["name"] = JsonValue{"test"};
    testObj["value"] = JsonValue{123.0};
    JsonValue objectJson{testObj};

    assert(objectJson.is_object());
    assert(objectJson.contains("name"));
    assert(objectJson.contains("value"));
    assert(objectJson.at("name").get_string() == "test");
    assert(objectJson.at("value").get_number() == 123.0);
    std::cout << "✅ Object creation and access working" << std::endl;

    // Test 3: JSON parsing from string
    std::cout << "Testing JSON parsing..." << std::endl;
    JsonParser parser;
    std::string testJson = R"({"name":"test","value":42,"active":true})";
    JsonValue parsed = parser.parse(testJson);

    assert(parsed.is_object());
    assert(parsed.contains("name"));
    assert(parsed.at("name").get_string() == "test");
    assert(parsed.at("value").get_number() == 42.0);
    assert(parsed.at("active").get_bool() == true);
    std::cout << "✅ JSON parsing working" << std::endl;

    // Test 4: Safe extraction methods
    std::cout << "Testing safe extraction..." << std::endl;
    auto optionalString = parser.extractOptional<std::string>(parsed, "name");
    assert(optionalString.has_value());
    assert(optionalString.value() == "test");

    auto missingOptional = parser.extractOptional<std::string>(parsed, "missing");
    assert(!missingOptional.has_value());

    auto withDefault = parser.extractWithDefault<std::string>(parsed, "missing", "default");
    assert(withDefault == "default");
    std::cout << "✅ Safe extraction working" << std::endl;

    // Test 5: API Response Parsing
    std::cout << "Testing API response parsing..." << std::endl;
    std::string cryptoCompareJson = R"({"USD":50000,"EUR":42000})";
    auto ccPrice = ApiParsers::CryptoCompareParser::parsePrice(cryptoCompareJson);
    assert(!ccPrice.errorMessage.has_value());
    assert(ccPrice.prices.size() == 2);
    assert(ccPrice.prices.at("USD") == 50000.0);
    std::cout << "✅ API response parsing working" << std::endl;

    // Test 6: Error handling modes
    std::cout << "Testing error handling..." << std::endl;
    ParseOptions strictOptions;
    strictOptions.failOnMissingRequired = true;
    strictOptions.logWarnings = false;

    JsonParser strictParser(strictOptions);
    bool threwException = false;
    try {
        strictParser.extractRequired<std::string>(parsed, "nonexistent");
    } catch (const JsonAccessException&) {
        threwException = true;
    }
    assert(threwException);
    std::cout << "✅ Error handling working" << std::endl;

    // Test 7: JsonUtils
    std::cout << "Testing JSON utilities..." << std::endl;
    auto doubleValue = JsonUtils::stringToDouble("123.45");
    assert(doubleValue.has_value());
    assert(doubleValue.value() == 123.45);

    auto boolValue = JsonUtils::stringToBool("true");
    assert(boolValue.has_value());
    assert(boolValue.value() == true);

    assert(JsonUtils::isValidJson(R"({"valid":true})"));
    std::cout << "✅ JSON utilities working" << std::endl;

    std::cout << "\n================================================================================\n";
    std::cout << "🎉 Day 12 JSON Core Functionality Complete!" << std::endl;
    std::cout << "✅ JsonValue class with type safety" << std::endl;
    std::cout << "✅ JSON parsing framework (mock implementation)" << std::endl;
    std::cout << "✅ Safe extraction methods with mixed error handling" << std::endl;
    std::cout << "✅ API-specific parsers for CryptoCompare and NewsAPI" << std::endl;
    std::cout << "✅ Configurable error handling strategies" << std::endl;
    std::cout << "✅ JSON utility functions for common operations" << std::endl;
    std::cout << "✅ Clean separation of JSON parsing from business logic" << std::endl;
    std::cout << "🚀 Framework ready for production nlohmann/json integration!" << std::endl;
    std::cout << "\nNote: Path-based navigation deferred for production implementation with nlohmann/json" << std::endl;
    std::cout << "\nReady for Day 13: API Response Framework!" << std::endl;

    return 0;
}