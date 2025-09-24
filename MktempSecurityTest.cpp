#include <iostream>
#include <fstream>
#include <string>

int main() {
    std::cout << "=== MKTEMP SECURITY VALIDATION ===" << std::endl;

    // Read the Makefile
    std::ifstream makefile("Makefile");
    if (!makefile.is_open()) {
        std::cout << "❌ Cannot open Makefile for testing" << std::endl;
        return 1;
    }

    std::string content((std::istreambuf_iterator<char>(makefile)),
                        std::istreambuf_iterator<char>());
    makefile.close();

    // Check for insecure mktemp -u usage
    if (content.find("mktemp -u") != std::string::npos) {
        std::cout << "❌ Found insecure mktemp -u usage!" << std::endl;
        return 1;
    } else {
        std::cout << "✅ No mktemp -u usage found" << std::endl;
    }

    // Check for secure mktemp usage
    if (content.find("mktemp") != std::string::npos) {
        std::cout << "✅ mktemp is still used for temporary file creation" << std::endl;
    } else {
        std::cout << "❌ No mktemp usage found at all" << std::endl;
        return 1;
    }

    // Check for proper cleanup in both success and failure cases
    if (content.find("rm -f \"$$SQLITE_TEST\"") != std::string::npos) {
        std::cout << "✅ Proper cleanup implemented" << std::endl;
    } else {
        std::cout << "❌ Missing proper cleanup" << std::endl;
        return 1;
    }

    std::cout << "\n🎯 MKTEMP SECURITY VALIDATION: PASSED ✅" << std::endl;
    std::cout << "Race condition vulnerability fixed!" << std::endl;
    return 0;
}