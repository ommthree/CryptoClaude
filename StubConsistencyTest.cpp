#include <iostream>

// Force stub implementation by defining JSON_LIBRARY_AVAILABLE as 0
#define JSON_LIBRARY_AVAILABLE 0
#include "src/Core/Json/JsonHelper.h"

using namespace CryptoClaude::Json;

int main() {
    std::cout << "=== TESTING STUB CONSISTENCY FIX ===" << std::endl;

    json j;

    // Test 1: operator[] should now throw consistently
    std::cout << "Testing operator[] behavior (should throw)..." << std::endl;
    try {
        json result = j["key"];
        std::cout << "❌ operator[] did NOT throw - consistency bug still exists!" << std::endl;
        return 1;
    } catch (const std::runtime_error& e) {
        std::cout << "✅ operator[] threw correctly: " << e.what() << std::endl;
    }

    // Test 2: contains() should throw consistently
    std::cout << "\nTesting contains() behavior (should throw)..." << std::endl;
    try {
        bool result = j.contains("key");
        std::cout << "❌ contains() did NOT throw - consistency bug still exists!" << std::endl;
        return 1;
    } catch (const std::runtime_error& e) {
        std::cout << "✅ contains() threw correctly: " << e.what() << std::endl;
    }

    // Test 3: parse() should throw consistently
    std::cout << "\nTesting parse() behavior (should throw)..." << std::endl;
    try {
        json parsed = json::parse("{}");
        std::cout << "❌ parse() did NOT throw!" << std::endl;
        return 1;
    } catch (const std::runtime_error& e) {
        std::cout << "✅ parse() threw correctly: " << e.what() << std::endl;
    }

    // Test 4: JsonHelper methods should handle exceptions gracefully
    std::cout << "\nTesting JsonHelper graceful handling..." << std::endl;
    bool hasKey = JsonHelper::hasKey(j, "test");
    if (hasKey) {
        std::cout << "❌ JsonHelper::hasKey returned true when should be false" << std::endl;
        return 1;
    }
    std::cout << "✅ JsonHelper::hasKey handled exception gracefully" << std::endl;

    std::cout << "\n🎯 STUB CONSISTENCY VALIDATION: PASSED ✅" << std::endl;
    std::cout << "All stub methods now throw consistently - no more silent failures!" << std::endl;

    return 0;
}