#include <iostream>

// CRITICAL TEST: Force stub implementation by defining JSON_LIBRARY_AVAILABLE as 0 BEFORE including
#define JSON_LIBRARY_AVAILABLE 0
#include "src/Core/Json/JsonHelper.h"

using namespace CryptoClaude::Json;

int main() {
    std::cout << "=== JSON STUB CONDITIONAL COMPILATION VALIDATION ===" << std::endl;

    // Verify we're actually in stub mode
    if (JsonHelper::isJsonLibraryAvailable()) {
        std::cout << "❌ CRITICAL FAILURE: Still using real JSON library despite override!" << std::endl;
        std::cout << "The conditional compilation fix is NOT working!" << std::endl;
        return 1;
    }

    if (!JsonHelper::isStubMode()) {
        std::cout << "❌ CRITICAL FAILURE: Not detected as stub mode!" << std::endl;
        return 1;
    }

    std::cout << "✅ Conditional compilation working - in stub mode" << std::endl;

    json j;

    // Test 1: operator[] should throw consistently
    std::cout << "\nTesting operator[] behavior in FORCED stub mode..." << std::endl;
    try {
        json result = j["key"];
        std::cout << "❌ operator[] did NOT throw - stub implementation failed!" << std::endl;
        return 1;
    } catch (const std::runtime_error& e) {
        std::cout << "✅ operator[] threw correctly: " << e.what() << std::endl;
    }

    // Test 2: contains() should throw consistently
    std::cout << "\nTesting contains() behavior in FORCED stub mode..." << std::endl;
    try {
        bool result = j.contains("key");
        std::cout << "❌ contains() did NOT throw - stub implementation failed!" << std::endl;
        return 1;
    } catch (const std::runtime_error& e) {
        std::cout << "✅ contains() threw correctly: " << e.what() << std::endl;
    }

    // Test 3: parse() should throw consistently
    std::cout << "\nTesting parse() behavior in FORCED stub mode..." << std::endl;
    try {
        json parsed = json::parse("{}");
        std::cout << "❌ parse() did NOT throw - stub implementation failed!" << std::endl;
        return 1;
    } catch (const std::runtime_error& e) {
        std::cout << "✅ parse() threw correctly: " << e.what() << std::endl;
    }

    // Test 4: JsonHelper methods should handle exceptions gracefully in stub mode
    std::cout << "\nTesting JsonHelper graceful handling in FORCED stub mode..." << std::endl;
    bool hasKey = JsonHelper::hasKey(j, "test");
    if (hasKey) {
        std::cout << "❌ JsonHelper::hasKey returned true when should be false in stub mode" << std::endl;
        return 1;
    }
    std::cout << "✅ JsonHelper::hasKey handled stub exception gracefully" << std::endl;

    // Test 5: Verify stub methods actually execute (not dead code)
    std::cout << "\nTesting that stub implementation is actually reachable..." << std::endl;
    try {
        j.dump();
        std::cout << "❌ dump() should have thrown in stub mode!" << std::endl;
        return 1;
    } catch (const std::runtime_error& e) {
        std::cout << "✅ dump() threw in stub mode: " << e.what() << std::endl;
    }

    std::cout << "\n🎯 JSON STUB CONDITIONAL COMPILATION VALIDATION: PASSED ✅" << std::endl;
    std::cout << "Stub implementation is now properly reachable and functional!" << std::endl;
    std::cout << "The validator's critical issue has been resolved." << std::endl;

    return 0;
}