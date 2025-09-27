#include "src/Core/Testing/RealCorrelationFramework.h"
#include "src/Core/Database/DatabaseManager.h"
#include <iostream>
#include <iomanip>
#include <memory>

int main() {
    std::cout << "=== REAL CORRELATION FRAMEWORK VALIDATION ===" << std::endl;
    std::cout << "Testing Real Correlation Implementation vs Simulated Values" << std::endl << std::endl;

    try {
        // Initialize real correlation framework (handles database internally)
        CryptoClaude::Testing::RealCorrelationFramework correlationFramework;

        std::cout << "🔍 TESTING PHASE 1: Database Population Validation" << std::endl;

        // Test database connectivity
        auto& dbManager = CryptoClaude::Database::DatabaseManager::getInstance();
        auto query = "SELECT COUNT(*) as record_count FROM market_data";
        auto results = dbManager.executeSelectQuery(query);

        if (!results.empty()) {
            int recordCount = std::stoi(results[0]["record_count"]);
            std::cout << "   Database Records Found: " << recordCount << std::endl;

            if (recordCount < 730) {
                std::cout << "   ⚠️  WARNING: Insufficient historical data" << std::endl;
                std::cout << "   Required: 730+ records (2 years daily data)" << std::endl;
                std::cout << "   Found: " << recordCount << " records" << std::endl;
                std::cout << "   Status: REAL CORRELATION CANNOT BE CALCULATED" << std::endl;
            } else {
                std::cout << "   ✅ Sufficient historical data available" << std::endl;
            }
        }

        std::cout << std::endl << "🔍 TESTING PHASE 2: Real Correlation Calculation" << std::endl;

        // Attempt real correlation calculation
        auto correlationResult = correlationFramework.calculateRealCorrelation();

        std::cout << "📊 REAL CORRELATION RESULTS:" << std::endl;
        std::cout << "   Pearson Correlation: " << std::fixed << std::setprecision(4)
                  << correlationResult.pearsonCorrelation << std::endl;
        std::cout << "   Spearman Correlation: " << correlationResult.spearmanCorrelation << std::endl;
        std::cout << "   Sample Size: " << correlationResult.sampleSize << std::endl;
        std::cout << "   P-Value: " << correlationResult.pValue << std::endl;
        std::cout << "   Statistical Significance: " << (correlationResult.isStatisticallySignificant ? "YES" : "NO") << std::endl;
        std::cout << "   Confidence Interval: [" << correlationResult.confidenceIntervalLower
                  << ", " << correlationResult.confidenceIntervalUpper << "]" << std::endl;

        std::cout << std::endl << "🎯 TESTING PHASE 3: Regulatory Compliance Validation" << std::endl;

        bool passesRegulatory = correlationFramework.passesRegulatoryRequirements(correlationResult);

        std::cout << "📋 TRS COMPLIANCE ASSESSMENT:" << std::endl;
        std::cout << "   Required Correlation: ≥0.85" << std::endl;
        std::cout << "   Achieved Correlation: " << correlationResult.pearsonCorrelation << std::endl;
        std::cout << "   Required Significance: p<0.05" << std::endl;
        std::cout << "   Achieved P-Value: " << correlationResult.pValue << std::endl;
        std::cout << "   Required Sample Size: ≥500" << std::endl;
        std::cout << "   Achieved Sample Size: " << correlationResult.sampleSize << std::endl;

        if (passesRegulatory) {
            std::cout << "   ✅ REGULATORY COMPLIANCE: PASS" << std::endl;
            std::cout << "   Status: Ready for TRS submission" << std::endl;
        } else {
            std::cout << "   ❌ REGULATORY COMPLIANCE: FAIL" << std::endl;
            std::cout << "   Status: NOT ready for TRS submission" << std::endl;
        }

        std::cout << std::endl << "🔍 TESTING PHASE 4: TRS Compliance Report Generation" << std::endl;

        std::string complianceReport = correlationFramework.generateTRSComplianceReport();
        std::cout << complianceReport << std::endl;

        std::cout << std::endl << "🔍 TESTING PHASE 5: Multi-Timeframe Validation" << std::endl;

        auto multiTimeframeResults = correlationFramework.validateMultiTimeframes();

        for (const auto& timeframe : multiTimeframeResults) {
            std::cout << "   " << timeframe.first << " Correlation: "
                     << std::fixed << std::setprecision(4) << timeframe.second.pearsonCorrelation << std::endl;
        }

        std::cout << std::endl << "=== REAL CORRELATION FRAMEWORK VALIDATION COMPLETE ===" << std::endl;

        // Final assessment
        if (correlationResult.pearsonCorrelation > 0.0 && passesRegulatory) {
            std::cout << "🎯 RESULT: REAL CORRELATION FRAMEWORK OPERATIONAL" << std::endl;
            std::cout << "✅ Ready to replace simulated correlation values" << std::endl;
            return 0;
        } else if (correlationResult.pearsonCorrelation > 0.0) {
            std::cout << "⚠️  RESULT: REAL CORRELATION CALCULATED BUT BELOW REQUIREMENTS" << std::endl;
            std::cout << "🔄 Algorithm optimization needed to meet TRS standards" << std::endl;
            return 0;
        } else {
            std::cout << "❌ RESULT: REAL CORRELATION CANNOT BE CALCULATED" << std::endl;
            std::cout << "🚨 Critical gap: Historical data or algorithm implementation missing" << std::endl;
            return 1;
        }

    } catch (const std::exception& e) {
        std::cout << "❌ CRITICAL ERROR: " << e.what() << std::endl;
        std::cout << "🚨 Real correlation framework cannot be initialized" << std::endl;
        std::cout << "   This confirms the agent analysis: infrastructure ready, algorithm missing" << std::endl;
        return 1;
    }
}