#include <iostream>
#include <fstream>
#include <string>

int main() {
    std::cout << "=== DOWNLOAD VERIFICATION TEST ===" << std::endl;

    // Read the installation script
    std::ifstream script("install_dependencies.sh");
    if (!script.is_open()) {
        std::cout << "❌ Cannot open install_dependencies.sh for testing" << std::endl;
        return 1;
    }

    std::string content((std::istreambuf_iterator<char>(script)),
                        std::istreambuf_iterator<char>());
    script.close();

    // Check that fake checksums are removed
    if (content.find("a259cd8cf71063c6d5d2295b1a8e6d1f80c2b0a1b4e8b8b3c5d1f0a2b3c4d5e6") != std::string::npos) {
        std::cout << "❌ Fake checksums still present!" << std::endl;
        return 1;
    }

    if (content.find("EXPECTED_CHECKSUMS") != std::string::npos) {
        std::cout << "❌ EXPECTED_CHECKSUMS still referenced!" << std::endl;
        return 1;
    }

    // Check for new verification approach
    std::vector<std::string> newFeatures = {
        "verify_download",
        "Basic download verification",
        "namespace nlohmann",
        "file_size",
        "basic verification only"
    };

    bool allPresent = true;
    for (const auto& feature : newFeatures) {
        if (content.find(feature) != std::string::npos) {
            std::cout << "✅ Found: " << feature << std::endl;
        } else {
            std::cout << "❌ Missing: " << feature << std::endl;
            allPresent = false;
        }
    }

    if (allPresent) {
        std::cout << "\n🎯 DOWNLOAD VERIFICATION UPDATE: PASSED ✅" << std::endl;
        std::cout << "Fake checksums removed, honest verification implemented" << std::endl;
        return 0;
    } else {
        std::cout << "\n❌ DOWNLOAD VERIFICATION UPDATE: FAILED" << std::endl;
        return 1;
    }
}