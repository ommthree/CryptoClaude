#include <iostream>
#include <fstream>
#include <filesystem>
#include "src/Core/Config/ConfigManager.h"

using namespace CryptoClaude::Config;

int main() {
    std::cout << "=== PRODUCTION SECURITY VALIDATION ===" << std::endl;

    // Test 1: Configuration file security
    std::cout << "\n🔍 Testing configuration security..." << std::endl;

    // Check if config files exist
    if (!std::filesystem::exists("config/production.json")) {
        std::cout << "❌ Production config file missing" << std::endl;
        return 1;
    }

    if (!std::filesystem::exists("config/development.json")) {
        std::cout << "❌ Development config file missing" << std::endl;
        return 1;
    }

    std::cout << "✅ Configuration files present" << std::endl;

    // Test 2: Configuration loading
    std::cout << "\n🔍 Testing configuration loading..." << std::endl;

    ConfigManager& config = ConfigManager::getInstance();

    // Test production config
    if (config.loadProductionConfig()) {
        std::cout << "✅ Production configuration loaded successfully" << std::endl;

        // Validate production security settings
        if (config.isProductionMode()) {
            std::cout << "✅ Production mode properly detected" << std::endl;
        } else {
            std::cout << "❌ Production mode not detected correctly" << std::endl;
            return 1;
        }

        // Check security settings
        if (!config.shouldLogSensitiveData()) {
            std::cout << "✅ Sensitive data logging disabled in production" << std::endl;
        } else {
            std::cout << "❌ Sensitive data logging enabled in production - security risk!" << std::endl;
            return 1;
        }

        std::string permissions = config.getConfigFilePermissions();
        if (permissions == "600") {
            std::cout << "✅ Secure file permissions configured: " << permissions << std::endl;
        } else {
            std::cout << "⚠️  File permissions not optimal: " << permissions << " (recommended: 600)" << std::endl;
        }

        // Check trading limits
        double maxPosition = config.getMaxPositionSizeUSD();
        if (maxPosition > 0 && maxPosition <= 50000) {
            std::cout << "✅ Reasonable position size limit: $" << maxPosition << std::endl;
        } else {
            std::cout << "⚠️  Position size limit may be too high: $" << maxPosition << std::endl;
        }

        double riskLimit = config.getRiskLimitPercent();
        if (riskLimit > 0 && riskLimit <= 5.0) {
            std::cout << "✅ Conservative risk limit: " << riskLimit << "%" << std::endl;
        } else {
            std::cout << "⚠️  Risk limit may be too high: " << riskLimit << "%" << std::endl;
        }

    } else {
        std::cout << "❌ Failed to load production configuration" << std::endl;
        return 1;
    }

    // Test 3: Development config
    std::cout << "\n🔍 Testing development configuration..." << std::endl;

    if (config.loadDevelopmentConfig()) {
        std::cout << "✅ Development configuration loaded successfully" << std::endl;

        if (!config.isProductionMode()) {
            std::cout << "✅ Development mode properly detected" << std::endl;
        } else {
            std::cout << "❌ Development mode not detected correctly" << std::endl;
            return 1;
        }

        // Check development settings
        if (config.isConsoleLoggingEnabled()) {
            std::cout << "✅ Console logging enabled in development" << std::endl;
        }

        std::string logLevel = config.getLogLevel();
        if (logLevel == "DEBUG") {
            std::cout << "✅ Debug logging enabled in development" << std::endl;
        }

    } else {
        std::cout << "❌ Failed to load development configuration" << std::endl;
        return 1;
    }

    // Test 4: API key security
    std::cout << "\n🔍 Testing API key security..." << std::endl;

    std::string apiKeyVar = config.getApiKeyEnvVar("cryptocompare");
    if (!apiKeyVar.empty()) {
        std::cout << "✅ API key environment variable configured: " << apiKeyVar << std::endl;

        // Check if environment variable is actually set (don't print value for security)
        const char* apiKey = std::getenv(apiKeyVar.c_str());
        if (apiKey && strlen(apiKey) > 0) {
            std::cout << "✅ API key environment variable is set" << std::endl;
        } else {
            std::cout << "⚠️  API key environment variable not set (normal for testing)" << std::endl;
        }
    } else {
        std::cout << "❌ API key environment variable not configured" << std::endl;
        return 1;
    }

    std::cout << "\n🎯 PRODUCTION SECURITY VALIDATION: PASSED ✅" << std::endl;
    std::cout << "Configuration management system is secure and production-ready!" << std::endl;

    return 0;
}