#include <iostream>
#include <fstream>
#include <string>
#include <vector>

int main() {
    std::cout << "=== MAKEFILE TEMPORARY FILE SECURITY VALIDATION ===" << std::endl;

    // Read the Makefile
    std::ifstream makefile("Makefile");
    if (!makefile.is_open()) {
        std::cout << "❌ Could not open Makefile for validation" << std::endl;
        return 1;
    }

    std::string content((std::istreambuf_iterator<char>(makefile)),
                        std::istreambuf_iterator<char>());
    makefile.close();

    // Check for insecure temporary file patterns
    std::vector<std::string> insecurePatterns = {
        "/tmp/sqlite_test",
        "/tmp/json_test",
        "> /tmp/",
        "-o /tmp/"
    };

    std::cout << "\n🔍 Checking for insecure temporary file usage..." << std::endl;
    bool foundInsecure = false;
    for (const auto& pattern : insecurePatterns) {
        if (content.find(pattern) != std::string::npos) {
            std::cout << "❌ Found insecure temp file pattern: " << pattern << std::endl;
            foundInsecure = true;
        }
    }

    if (!foundInsecure) {
        std::cout << "✅ No insecure temporary file patterns found" << std::endl;
    }

    // Check for secure temporary file features
    std::vector<std::string> secureFeatures = {
        "mktemp -d",
        "mktemp -u",
        "SECURE JSON FUNCTIONALITY TEST",
        "NO PREDICTABLE TEMP FILES",
        "rm -rf \"$$",
        "TEST_DIR=$$(mktemp"
    };

    std::cout << "\n🔍 Checking for secure temporary file features..." << std::endl;
    bool allSecurePresent = true;
    for (const auto& feature : secureFeatures) {
        if (content.find(feature) != std::string::npos) {
            std::cout << "✅ Found: " << feature << std::endl;
        } else {
            std::cout << "❌ Missing: " << feature << std::endl;
            allSecurePresent = false;
        }
    }

    // Check for proper cleanup
    std::vector<std::string> cleanupPatterns = {
        "rm -rf",
        "exit $$TEST_RESULT"
    };

    std::cout << "\n🔍 Checking for proper cleanup patterns..." << std::endl;
    bool hasCleanup = false;
    for (const auto& pattern : cleanupPatterns) {
        if (content.find(pattern) != std::string::npos) {
            std::cout << "✅ Found cleanup pattern: " << pattern << std::endl;
            hasCleanup = true;
        }
    }

    if (!foundInsecure && allSecurePresent && hasCleanup) {
        std::cout << "\n🎯 MAKEFILE SECURITY VALIDATION: PASSED ✅" << std::endl;
        std::cout << "Makefile now uses secure temporary file handling!" << std::endl;
        return 0;
    } else {
        std::cout << "\n❌ MAKEFILE SECURITY VALIDATION: FAILED" << std::endl;
        return 1;
    }
}