#include <iostream>
#include <cassert>
#include "src/Core/Json/JsonParser.h"

using namespace CryptoClaude::Json;

int main() {
    std::cout << "Testing nested object access..." << std::endl;

    JsonParser parser;

    // Create deeply nested structure
    std::map<std::string, JsonValue> level3;
    level3["value"] = JsonValue{42.0};
    level3["name"] = JsonValue{"deep"};

    std::map<std::string, JsonValue> level2;
    level2["level3"] = JsonValue{level3};
    level2["price"] = JsonValue{100.0};

    std::map<std::string, JsonValue> level1;
    level1["level2"] = JsonValue{level2};
    level1["status"] = JsonValue{"active"};

    JsonValue json{level1};

    // Test single level access
    bool exists1 = parser.pathExists(json, "status");
    assert(exists1);
    std::cout << "✅ Single level path exists" << std::endl;

    // Test two level access
    bool exists2 = parser.pathExists(json, "level2");
    assert(exists2);
    std::cout << "✅ Second level path exists" << std::endl;

    bool exists2b = parser.pathExists(json, "level2.price");
    assert(exists2b);
    std::cout << "✅ Two level dot notation works" << std::endl;

    // Test three level access
    bool exists3 = parser.pathExists(json, "level2.level3.value");
    assert(exists3);
    std::cout << "✅ Three level dot notation works" << std::endl;

    // Test value extraction
    auto status = parser.getString(json, "status");
    assert(status.has_value());
    assert(status.value() == "active");
    std::cout << "✅ Single level extraction works" << std::endl;

    auto price = parser.getDouble(json, "level2.price");
    assert(price.has_value());
    assert(price.value() == 100.0);
    std::cout << "✅ Two level extraction works" << std::endl;

    auto deepValue = parser.getDouble(json, "level2.level3.value");
    assert(deepValue.has_value());
    assert(deepValue.value() == 42.0);
    std::cout << "✅ Three level extraction works" << std::endl;

    auto deepName = parser.getString(json, "level2.level3.name");
    assert(deepName.has_value());
    assert(deepName.value() == "deep");
    std::cout << "✅ Deep string extraction works" << std::endl;

    // Test non-existent paths
    bool nonExistent = parser.pathExists(json, "nonexistent");
    assert(!nonExistent);
    std::cout << "✅ Non-existent path correctly returns false" << std::endl;

    bool nonExistentDeep = parser.pathExists(json, "level2.nonexistent.value");
    assert(!nonExistentDeep);
    std::cout << "✅ Non-existent deep path correctly returns false" << std::endl;

    // Test optional extraction of non-existent
    auto missing = parser.getString(json, "level2.missing.field");
    assert(!missing.has_value());
    std::cout << "✅ Optional extraction of missing path returns nullopt" << std::endl;

    std::cout << "\n🎉 All nested object access tests passed!" << std::endl;
    return 0;
}