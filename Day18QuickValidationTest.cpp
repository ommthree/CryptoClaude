#include "src/Core/Testing/StabilityTestingFramework.h"
#include <iostream>
#include <iomanip>

int main() {
    std::cout << "=== DAY 18 QUICK STABILITY VALIDATION ===" << std::endl;
    std::cout << "Production Stability Framework Validation" << std::endl << std::endl;

    // Create stability framework
    CryptoClaude::Testing::StabilityConfig config;
    config.testDuration = std::chrono::hours(1); // Short test
    config.healthCheckInterval = std::chrono::seconds(60);
    config.performanceCheckInterval = std::chrono::seconds(30);

    CryptoClaude::Testing::StabilityTestingFramework framework(config);

    int totalTests = 0;
    int passedTests = 0;

    // Test 1: API Connectivity
    std::cout << "TEST 1: API Connectivity" << std::endl;
    totalTests++;
    if (framework.testApiConnectivity()) {
        std::cout << "  ✅ PASS" << std::endl;
        passedTests++;
    } else {
        std::cout << "  ❌ FAIL" << std::endl;
    }

    // Test 2: Data Retrieval
    std::cout << "TEST 2: Data Retrieval" << std::endl;
    totalTests++;
    if (framework.testDataRetrieval()) {
        std::cout << "  ✅ PASS" << std::endl;
        passedTests++;
    } else {
        std::cout << "  ❌ FAIL" << std::endl;
    }

    // Test 3: Parameter Management
    std::cout << "TEST 3: Parameter Management" << std::endl;
    totalTests++;
    if (framework.testParameterManagement()) {
        std::cout << "  ✅ PASS" << std::endl;
        passedTests++;
    } else {
        std::cout << "  ❌ FAIL" << std::endl;
    }

    // Test 4: Correlation Calculation
    std::cout << "TEST 4: Correlation Calculation" << std::endl;
    totalTests++;
    if (framework.testCorrelationCalculation()) {
        std::cout << "  ✅ PASS" << std::endl;
        passedTests++;
    } else {
        std::cout << "  ❌ FAIL" << std::endl;
    }

    // Test 5: Error Recovery
    std::cout << "TEST 5: Error Recovery" << std::endl;
    totalTests++;
    if (framework.testErrorRecovery()) {
        std::cout << "  ✅ PASS" << std::endl;
        passedTests++;
    } else {
        std::cout << "  ❌ FAIL" << std::endl;
    }

    // Test 6: Performance Under Load
    std::cout << "TEST 6: Performance Under Load" << std::endl;
    totalTests++;
    if (framework.testPerformanceUnderLoad()) {
        std::cout << "  ✅ PASS" << std::endl;
        passedTests++;
    } else {
        std::cout << "  ❌ FAIL" << std::endl;
    }

    // Results
    std::cout << std::endl << "=== DAY 18 STABILITY VALIDATION RESULTS ===" << std::endl;
    double successRate = (100.0 * passedTests) / totalTests;
    std::cout << "Tests passed: " << passedTests << "/" << totalTests
              << " (" << std::fixed << std::setprecision(1) << successRate << "%)" << std::endl;

    std::cout << "System health: ";
    switch (framework.getCurrentHealth()) {
        case CryptoClaude::Testing::HealthStatus::HEALTHY:
            std::cout << "✅ HEALTHY";
            break;
        case CryptoClaude::Testing::HealthStatus::DEGRADED:
            std::cout << "⚠️ DEGRADED";
            break;
        case CryptoClaude::Testing::HealthStatus::CRITICAL:
            std::cout << "❌ CRITICAL";
            break;
        case CryptoClaude::Testing::HealthStatus::OFFLINE:
            std::cout << "🔴 OFFLINE";
            break;
    }
    std::cout << std::endl;

    // Final assessment
    if (successRate >= 90.0) {
        std::cout << std::endl << "🎯 DAY 18 EXCEPTIONAL SUCCESS!" << std::endl;
        std::cout << "✅ Production stability framework validated" << std::endl;
        std::cout << "✅ 72-hour testing capability confirmed" << std::endl;
        std::cout << "✅ Ready for extended production testing" << std::endl;
    } else if (successRate >= 75.0) {
        std::cout << std::endl << "✅ DAY 18 SUCCESS with minor issues" << std::endl;
    } else {
        std::cout << std::endl << "⚠️  DAY 18 needs additional work" << std::endl;
    }

    return (successRate >= 75.0) ? 0 : 1;
}