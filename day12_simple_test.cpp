#include <iostream>
#include "src/Core/Json/JsonParser.h"

using namespace CryptoClaude::Json;

int main() {
    std::cout << "Simple JSON test..." << std::endl;

    JsonParser parser;

    // Create test structure manually
    std::map<std::string, JsonValue> innerObj;
    innerObj["price"] = JsonValue{50000.0};

    std::map<std::string, JsonValue> outerObj;
    outerObj["data"] = JsonValue{innerObj};

    JsonValue json{outerObj};

    std::cout << "Created JSON structure" << std::endl;

    // Test path existence
    std::cout << "About to test path existence..." << std::endl;
    bool exists = parser.pathExists(json, "data");
    std::cout << "Path 'data' exists: " << (exists ? "true" : "false") << std::endl;

    // Test nested path
    bool nestedExists = parser.pathExists(json, "data.price");
    std::cout << "Path 'data.price' exists: " << (nestedExists ? "true" : "false") << std::endl;

    std::cout << "Test completed!" << std::endl;
    return 0;
}