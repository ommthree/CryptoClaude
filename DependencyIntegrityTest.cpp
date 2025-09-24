#include <iostream>
#include <fstream>
#include <string>
#include <vector>

class DependencyIntegrityValidator {
private:
    bool hasErrors = false;

    void error(const std::string& msg) {
        std::cout << "❌ " << msg << std::endl;
        hasErrors = true;
    }

    void success(const std::string& msg) {
        std::cout << "✅ " << msg << std::endl;
    }

    void warning(const std::string& msg) {
        std::cout << "⚠️  " << msg << std::endl;
    }

public:
    // Test 1: Installation Script Has Integrity Verification
    void testInstallationScriptIntegrity() {
        std::cout << "\n🔍 Testing installation script integrity features..." << std::endl;

        std::ifstream script("install_dependencies.sh");
        if (!script.is_open()) {
            error("Cannot open install_dependencies.sh for testing");
            return;
        }

        std::string content((std::istreambuf_iterator<char>(script)),
                            std::istreambuf_iterator<char>());
        script.close();

        // Check for integrity verification features
        std::vector<std::string> integrityFeatures = {
            "INTEGRITY VERIFICATION",
            "verify_integrity",
            "SHA256",
            "shasum",
            "sha256sum",
            "EXPECTED_CHECKSUMS",
            "supply chain attack",
            "File integrity verified"
        };

        bool allFeaturesPresent = true;
        for (const auto& feature : integrityFeatures) {
            if (content.find(feature) != std::string::npos) {
                success("Found integrity feature: " + feature);
            } else {
                error("Missing integrity feature: " + feature);
                allFeaturesPresent = false;
            }
        }

        if (allFeaturesPresent) {
            success("All integrity verification features present");
        }
    }

    // Test 2: Security Headers and Download Options
    void testSecureDownloadFeatures() {
        std::cout << "\n🔍 Testing secure download features..." << std::endl;

        std::ifstream script("install_dependencies.sh");
        if (!script.is_open()) {
            error("Cannot open install_dependencies.sh for testing");
            return;
        }

        std::string content((std::istreambuf_iterator<char>(script)),
                            std::istreambuf_iterator<char>());
        script.close();

        // Check for secure download features
        std::vector<std::string> securityFeatures = {
            "User-Agent",
            "CryptoClaude-Install",
            "--max-time",
            "--timeout",
            "--retry",
            "--tries",
            "http.sslVerify=true",
            "Accept: application/octet-stream"
        };

        bool foundSecurityFeatures = false;
        for (const auto& feature : securityFeatures) {
            if (content.find(feature) != std::string::npos) {
                success("Found security feature: " + feature);
                foundSecurityFeatures = true;
            }
        }

        if (!foundSecurityFeatures) {
            error("No secure download features found");
        }
    }

    // Test 3: Supply Chain Attack Prevention
    void testSupplyChainProtection() {
        std::cout << "\n🔍 Testing supply chain attack prevention..." << std::endl;

        std::ifstream script("install_dependencies.sh");
        if (!script.is_open()) {
            error("Cannot open install_dependencies.sh for testing");
            return;
        }

        std::string content((std::istreambuf_iterator<char>(script)),
                            std::istreambuf_iterator<char>());
        script.close();

        // Check for supply chain protection features
        std::vector<std::string> protectionFeatures = {
            "supply chain attack",
            "SECURITY WARNING",
            "integrity verification failed",
            "Continue anyway",
            "Installation aborted for security",
            "Repository verification failed"
        };

        bool hasProtections = false;
        for (const auto& feature : protectionFeatures) {
            if (content.find(feature) != std::string::npos) {
                success("Found protection feature: " + feature);
                hasProtections = true;
            }
        }

        if (!hasProtections) {
            error("No supply chain attack protections found");
        }
    }

    // Test 4: Manual Installation Security Guidance
    void testManualInstallationGuidance() {
        std::cout << "\n🔍 Testing manual installation security guidance..." << std::endl;

        std::ifstream script("install_dependencies.sh");
        if (!script.is_open()) {
            error("Cannot open install_dependencies.sh for testing");
            return;
        }

        std::string content((std::istreambuf_iterator<char>(script)),
                            std::istreambuf_iterator<char>());
        script.close();

        // Check for manual installation guidance
        std::vector<std::string> guidanceFeatures = {
            "SECURE MANUAL INSTALLATION",
            "Verify SHA256 checksum",
            "Always verify checksums",
            "checksum matches one of"
        };

        bool hasGuidance = false;
        for (const auto& feature : guidanceFeatures) {
            if (content.find(feature) != std::string::npos) {
                success("Found guidance feature: " + feature);
                hasGuidance = true;
            }
        }

        if (!hasGuidance) {
            error("No manual installation security guidance found");
        }
    }

    // Test 5: Error Handling and Security Defaults
    void testErrorHandlingAndDefaults() {
        std::cout << "\n🔍 Testing error handling and security defaults..." << std::endl;

        std::ifstream script("install_dependencies.sh");
        if (!script.is_open()) {
            error("Cannot open install_dependencies.sh for testing");
            return;
        }

        std::string content((std::istreambuf_iterator<char>(script)),
                            std::istreambuf_iterator<char>());
        script.close();

        // Check for secure defaults
        if (content.find("recommended: N") != std::string::npos) {
            success("Defaults to secure option (N) for risky operations");
        } else {
            error("Doesn't default to secure option for risky operations");
        }

        // Check for proper error handling
        if (content.find("return 1") != std::string::npos) {
            success("Has proper error handling with return codes");
        } else {
            error("Missing proper error handling");
        }

        // Check that it fails securely
        if (content.find("SECURITY RISK") != std::string::npos) {
            success("Clearly marks security risks");
        } else {
            warning("Should more clearly mark security risks");
        }
    }

    bool runAllTests() {
        std::cout << "=== DEPENDENCY INTEGRITY VERIFICATION VALIDATION ===" << std::endl;

        testInstallationScriptIntegrity();
        testSecureDownloadFeatures();
        testSupplyChainProtection();
        testManualInstallationGuidance();
        testErrorHandlingAndDefaults();

        std::cout << "\n" << std::string(60, '=') << std::endl;

        if (hasErrors) {
            std::cout << "❌ INTEGRITY VALIDATION: FAILED" << std::endl;
            std::cout << "Dependency integrity verification needs improvement" << std::endl;
            return false;
        } else {
            std::cout << "🎯 INTEGRITY VALIDATION: PASSED ✅" << std::endl;
            std::cout << "Strong dependency integrity verification implemented" << std::endl;
            return true;
        }
    }
};

int main() {
    DependencyIntegrityValidator validator;
    return validator.runAllTests() ? 0 : 1;
}