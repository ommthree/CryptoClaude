#include <iostream>
#include <fstream>
#include <string>
#include <vector>

int main() {
    std::cout << "=== INSTALLATION SCRIPT SUDO SECURITY VALIDATION ===" << std::endl;

    // Read the installation script
    std::ifstream script("install_dependencies.sh");
    if (!script.is_open()) {
        std::cout << "❌ Could not open install_dependencies.sh for validation" << std::endl;
        return 1;
    }

    std::string content((std::istreambuf_iterator<char>(script)),
                        std::istreambuf_iterator<char>());
    script.close();

    // Check for sudo usage
    std::vector<std::string> sudoPatterns = {
        "sudo apt-get",
        "sudo yum",
        "sudo pacman",
        "sudo make install",
        "sudo cmake",
        "sudo cp",
        "sudo mv",
        "sudo ln"
    };

    std::cout << "\n🔍 Checking for sudo commands..." << std::endl;
    bool foundSudo = false;
    for (const auto& pattern : sudoPatterns) {
        if (content.find(pattern) != std::string::npos) {
            std::cout << "❌ Found sudo command: " << pattern << std::endl;
            foundSudo = true;
        }
    }

    if (!foundSudo) {
        std::cout << "✅ No sudo commands found in script" << std::endl;
    }

    // Check for security improvements
    std::vector<std::string> securityFeatures = {
        "SECURE USER-LEVEL INSTALLATION",
        "NO SUDO REQUIRED",
        "mktemp -d",
        "Security Notice",
        "MANUAL INSTALLATION REQUIRED",
        "user-level only"
    };

    std::cout << "\n🔍 Checking for security improvements..." << std::endl;
    bool allSecurityPresent = true;
    for (const auto& feature : securityFeatures) {
        if (content.find(feature) != std::string::npos) {
            std::cout << "✅ Found: " << feature << std::endl;
        } else {
            std::cout << "❌ Missing: " << feature << std::endl;
            allSecurityPresent = false;
        }
    }

    // Check for secure temporary file usage
    std::vector<std::string> tempFilePatterns = {
        "/tmp/",
        "tmp/"
    };

    std::cout << "\n🔍 Checking for insecure temporary file usage..." << std::endl;
    bool foundInsecureTemp = false;
    for (const auto& pattern : tempFilePatterns) {
        size_t pos = 0;
        while ((pos = content.find(pattern, pos)) != std::string::npos) {
            // Check if this is within a secure mktemp context or documentation
            size_t lineStart = content.rfind('\n', pos);
            size_t lineEnd = content.find('\n', pos);
            std::string line = content.substr(lineStart + 1, lineEnd - lineStart - 1);

            // Allow if it's in documentation or mktemp usage
            if (line.find("echo") == std::string::npos &&
                line.find("mktemp") == std::string::npos &&
                line.find("#") != 0) {
                std::cout << "❌ Found insecure temp file usage: " << line << std::endl;
                foundInsecureTemp = true;
            }
            pos += pattern.length();
        }
    }

    if (!foundInsecureTemp) {
        std::cout << "✅ No insecure temporary file usage found" << std::endl;
    }

    if (!foundSudo && allSecurityPresent && !foundInsecureTemp) {
        std::cout << "\n🎯 SUDO SECURITY VALIDATION: PASSED ✅" << std::endl;
        std::cout << "Installation script is now secure - no sudo privilege escalation!" << std::endl;
        return 0;
    } else {
        std::cout << "\n❌ SUDO SECURITY VALIDATION: FAILED" << std::endl;
        return 1;
    }
}