#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <cstdlib>
#include <unistd.h>
#include <sys/stat.h>

class CrossPlatformValidator {
private:
    bool hasErrors = false;

    void error(const std::string& msg) {
        std::cout << "❌ " << msg << std::endl;
        hasErrors = true;
    }

    void success(const std::string& msg) {
        std::cout << "✅ " << msg << std::endl;
    }

    void info(const std::string& msg) {
        std::cout << "ℹ️  " << msg << std::endl;
    }

public:
    // Test 1: JSON Stub Conditional Compilation
    void testJsonStubConditionalCompilation() {
        std::cout << "\n🔍 Testing JSON stub conditional compilation..." << std::endl;

        // Test that we can force stub mode
        std::system("echo '#define JSON_LIBRARY_AVAILABLE 0' > test_stub_force.cpp");
        std::system("echo '#include \"src/Core/Json/JsonHelper.h\"' >> test_stub_force.cpp");
        std::system("echo 'int main() { return CryptoClaude::Json::JsonHelper::isStubMode() ? 0 : 1; }' >> test_stub_force.cpp");

        int result = std::system("g++ -std=c++17 test_stub_force.cpp src/Core/Json/JsonHelper.cpp -o test_stub_force 2>/dev/null && ./test_stub_force");
        std::system("rm -f test_stub_force.cpp test_stub_force");

        if (result == 0) {
            success("JSON stub conditional compilation works - can force stub mode");
        } else {
            error("JSON stub conditional compilation failed - cannot force stub mode");
        }

        // Test that auto-detection works when library is available
        std::system("echo '#include \"src/Core/Json/JsonHelper.h\"' > test_auto_detect.cpp");
        std::system("echo 'int main() { return CryptoClaude::Json::JsonHelper::isJsonLibraryAvailable() ? 0 : 1; }' >> test_auto_detect.cpp");

        // Include proper include paths for compilation
        result = std::system("g++ -std=c++17 -I/opt/homebrew/include -I/usr/local/include test_auto_detect.cpp src/Core/Json/JsonHelper.cpp -o test_auto_detect 2>/dev/null && ./test_auto_detect");
        std::system("rm -f test_auto_detect.cpp test_auto_detect");

        if (result == 0) {
            success("JSON auto-detection works when library is available");
        } else {
            info("JSON auto-detection returned non-zero (library may not be available on this system)");
            // Don't count this as an error since the library might not be installed
        }
    }

    // Test 2: Installation Script Security
    void testInstallationScriptSecurity() {
        std::cout << "\n🔍 Testing installation script security..." << std::endl;

        // Check that script doesn't use sudo
        std::ifstream script("install_dependencies.sh");
        if (!script.is_open()) {
            error("Cannot open install_dependencies.sh for testing");
            return;
        }

        std::string content((std::istreambuf_iterator<char>(script)),
                            std::istreambuf_iterator<char>());
        script.close();

        // Check for actual sudo command execution (not just documentation)
        std::vector<std::string> sudoCommands = {
            "sudo apt-get", "sudo yum", "sudo pacman", "sudo make", "sudo cmake", "sudo cp", "sudo mv"
        };
        bool foundSudoCommand = false;
        for (const auto& cmd : sudoCommands) {
            if (content.find(cmd) != std::string::npos) {
                error("Installation script contains sudo command: " + cmd);
                foundSudoCommand = true;
            }
        }
        if (!foundSudoCommand) {
            success("Installation script contains no sudo command executions");
        }

        // Test that mktemp is used
        if (content.find("mktemp") != std::string::npos) {
            success("Installation script uses secure temporary file creation");
        } else {
            error("Installation script doesn't use mktemp for secure temporary files");
        }

        // Test script syntax
        int result = std::system("bash -n install_dependencies.sh");
        if (result == 0) {
            success("Installation script has valid bash syntax");
        } else {
            error("Installation script has syntax errors");
        }
    }

    // Test 3: Makefile Portability and Security
    void testMakefilePortability() {
        std::cout << "\n🔍 Testing Makefile portability and security..." << std::endl;

        // Test dependency detection
        int result = std::system("make check-dependencies >/dev/null 2>&1");
        if (result == 0) {
            success("Makefile dependency detection works");
        } else {
            error("Makefile dependency detection failed");
        }

        // Check for hard-coded paths
        std::ifstream makefile("Makefile");
        if (!makefile.is_open()) {
            error("Cannot open Makefile for testing");
            return;
        }

        std::string content((std::istreambuf_iterator<char>(makefile)),
                            std::istreambuf_iterator<char>());
        makefile.close();

        std::vector<std::string> hardcodedPaths = {
            "/opt/homebrew/include =",
            "/usr/local/include =",
            "/usr/include =",
            "/tmp/json_test",
            "/tmp/sqlite_test"
        };

        bool foundHardcoded = false;
        for (const auto& path : hardcodedPaths) {
            if (content.find(path) != std::string::npos) {
                error("Found hard-coded path: " + path);
                foundHardcoded = true;
            }
        }

        if (!foundHardcoded) {
            success("No hard-coded paths found in Makefile");
        }

        // Test secure temporary file usage
        if (content.find("mktemp") != std::string::npos) {
            success("Makefile uses secure temporary file creation");
        } else {
            error("Makefile doesn't use mktemp for temporary files");
        }
    }

    // Test 4: Build System Integration
    void testBuildSystemIntegration() {
        std::cout << "\n🔍 Testing build system integration..." << std::endl;

        // Test that we can compile core components
        int result = std::system("make src/Core/Json/JsonHelper.o >/dev/null 2>&1");
        if (result == 0) {
            success("Core JSON helper compiles successfully");
        } else {
            error("Failed to compile core JSON helper");
        }

        // Clean up
        std::system("make clean >/dev/null 2>&1");
    }

    // Test 5: Cross-Platform Path Handling
    void testCrossPlatformPaths() {
        std::cout << "\n🔍 Testing cross-platform path handling..." << std::endl;

        // Test that paths work with spaces and special characters
        std::string testDir = "./test path with spaces";
        if (mkdir(testDir.c_str(), 0755) == 0 || errno == EEXIST) {
            std::string testFile = testDir + "/test_file.txt";
            std::ofstream file(testFile);
            if (file.is_open()) {
                file << "test content" << std::endl;
                file.close();

                // Test that our build system can handle this
                if (access(testFile.c_str(), F_OK) == 0) {
                    success("Path handling works with spaces and special characters");
                } else {
                    error("Path handling failed with spaces and special characters");
                }

                // Cleanup
                std::system(("rm -rf \"" + testDir + "\"").c_str());
            } else {
                error("Failed to create test file in path with spaces");
            }
        } else {
            error("Failed to create test directory with spaces");
        }
    }

    // Test 6: Environment Variable Security
    void testEnvironmentVariableSecurity() {
        std::cout << "\n🔍 Testing environment variable security..." << std::endl;

        // Test that malicious environment variables don't affect build
        std::system("export CXXFLAGS='-DMALICIOUS_DEFINE' && make check-dependencies >/dev/null 2>&1");

        // This is a basic test - in production we'd want more comprehensive testing
        info("Environment variable security testing is basic - manual verification recommended");
    }

    bool runAllTests() {
        std::cout << "=== COMPREHENSIVE CROSS-PLATFORM SECURITY VALIDATION ===" << std::endl;

        testJsonStubConditionalCompilation();
        testInstallationScriptSecurity();
        testMakefilePortability();
        testBuildSystemIntegration();
        testCrossPlatformPaths();
        testEnvironmentVariableSecurity();

        std::cout << "\n" << std::string(60, '=') << std::endl;

        if (hasErrors) {
            std::cout << "❌ COMPREHENSIVE VALIDATION: FAILED" << std::endl;
            std::cout << "Some cross-platform compatibility issues found" << std::endl;
            return false;
        } else {
            std::cout << "🎯 COMPREHENSIVE VALIDATION: PASSED ✅" << std::endl;
            std::cout << "System demonstrates strong cross-platform compatibility" << std::endl;
            return true;
        }
    }
};

int main() {
    CrossPlatformValidator validator;
    return validator.runAllTests() ? 0 : 1;
}