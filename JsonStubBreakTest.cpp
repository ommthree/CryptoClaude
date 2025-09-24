#include <iostream>

// Test to demonstrate the JSON stub logic flaw
// Force older compiler path by undefining __has_include
#ifdef __has_include
#undef __has_include
#endif

// This should force stub mode, but won't due to || 1 logic error
#define JSON_LIBRARY_AVAILABLE 0
#include "src/Core/Json/JsonHelper.h"

using namespace CryptoClaude::Json;

int main() {
    std::cout << "=== JSON STUB LOGIC ERROR PROOF ===" << std::endl;

    // This should be in stub mode but isn't due to || 1 error
    if (JsonHelper::isJsonLibraryAvailable()) {
        std::cout << "❌ CRITICAL FAILURE: Logic error allows real JSON library despite forced stub!" << std::endl;
        std::cout << "The || 1 condition in line 24 makes fallback ALWAYS true!" << std::endl;
        return 1;
    }

    std::cout << "✅ Stub mode working (this won't print due to logic error)" << std::endl;
    return 0;
}