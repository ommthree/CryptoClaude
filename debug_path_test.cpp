#include <iostream>
#include "src/Core/Json/JsonParser.h"

using namespace CryptoClaude::Json;

int main() {
    std::cout << "=== DEBUG PATH NAVIGATION ===" << std::endl;

    JsonParser parser;

    // Create simple test structure
    std::map<std::string, JsonValue> innerObj;
    innerObj["price"] = JsonValue{50000.0};
    std::cout << "Created inner object" << std::endl;

    std::map<std::string, JsonValue> outerObj;
    outerObj["data"] = JsonValue{innerObj};
    std::cout << "Created outer object" << std::endl;

    JsonValue json{outerObj};
    std::cout << "Created JSON value" << std::endl;

    // Test individual components step by step
    std::cout << "\n--- Testing splitPath ---" << std::endl;

    // Let's manually call splitPath to see what happens
    std::cout << "About to call splitPath with 'data'..." << std::endl;
    std::flush(std::cout);

    try {
        // This is a private method, but let's access it via getByPath
        std::cout << "Calling getByPath with empty path..." << std::endl;
        JsonValue emptyResult = parser.getByPath(json, "");
        std::cout << "Empty path worked" << std::endl;

        std::cout << "Calling getByPath with 'data'..." << std::endl;
        std::flush(std::cout);
        JsonValue result = parser.getByPath(json, "data");
        std::cout << "getByPath('data') completed!" << std::endl;

        std::cout << "Result is object: " << result.is_object() << std::endl;

    } catch (const std::exception& e) {
        std::cout << "Exception caught: " << e.what() << std::endl;
    }

    std::cout << "Debug completed" << std::endl;
    return 0;
}