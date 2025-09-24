#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <cmath>
#include <iomanip>
#include <cassert>
#include "src/Core/Database/Models/PortfolioData.h"
#include "src/Configuration/Config.h"

using namespace CryptoClaude::Database::Models;

class Day6RiskAnalyticsTest {
private:
    int testsPassed = 0;
    int testsTotal = 0;

    void assert_test(bool condition, const std::string& testName) {
        testsTotal++;
        if (condition) {
            testsPassed++;
            std::cout << "[✓] " << testName << std::endl;
        } else {
            std::cout << "[✗] " << testName << " FAILED" << std::endl;
        }
    }

    void assert_near(double actual, double expected, double tolerance, const std::string& testName) {
        bool condition = std::abs(actual - expected) <= tolerance;
        testsTotal++;
        if (condition) {
            testsPassed++;
            std::cout << "[✓] " << testName << " (actual: " << actual << ", expected: " << expected << ")" << std::endl;
        } else {
            std::cout << "[✗] " << testName << " FAILED (actual: " << actual << ", expected: " << expected << ", diff: " << std::abs(actual - expected) << ")" << std::endl;
        }
    }

public:
    void runAllTests() {
        std::cout << "\n=== DAY 6 RISK ANALYTICS COMPREHENSIVE TEST SUITE ===" << std::endl;
        std::cout << "Testing all enhanced risk management functionality...\n" << std::endl;

        testPositionRiskMetrics();
        testPortfolioRiskMetrics();
        testRiskReportingFramework();
        testRiskCalculationAccuracy();
        testRiskAlertSystem();
        testRiskScenarios();

        printSummary();
    }

private:
    void testPositionRiskMetrics() {
        std::cout << "\n--- Testing Position-Level Risk Metrics ---" << std::endl;

        // Create a test position
        Position pos("BTC", 1.0, 50000.0, true, 2.0); // 1 BTC at $50k, long, 2x leverage

        // Test initial values
        assert_test(pos.getVolatility() == 0.0, "Initial volatility is zero");
        assert_test(pos.getBeta() == 1.0, "Default beta is 1.0");
        assert_test(pos.getDeltaEquivalent() == 50000.0, "Delta equivalent calculated correctly");
        assert_test(pos.getMaxPriceDrawdown() == 0.0, "Initial drawdown is zero");

        // Test price updates and volatility calculation
        pos.setCurrentPrice(51000.0); // +2% move
        pos.setCurrentPrice(49000.0); // -4% move
        pos.setCurrentPrice(53000.0); // +8% move
        pos.setCurrentPrice(52000.0); // -2% move

        // After price updates, volatility should be calculated
        double volatility = pos.getVolatility();
        assert_test(volatility > 0.0, "Volatility calculated after price updates");
        std::cout << "    Calculated volatility: " << std::fixed << std::setprecision(4) << volatility << std::endl;

        // Test VaR calculation
        double var95 = pos.calculatePositionVaR(0.05, 1); // 95% confidence, 1 day
        double var99 = pos.calculatePositionVaR(0.01, 1); // 99% confidence, 1 day

        assert_test(var95 > 0.0, "VaR 95% calculated successfully");
        assert_test(var99 > var95, "VaR 99% is higher than VaR 95%");
        std::cout << "    Position VaR (95%): $" << std::fixed << std::setprecision(2) << var95 << std::endl;
        std::cout << "    Position VaR (99%): $" << std::fixed << std::setprecision(2) << var99 << std::endl;

        // Test Expected Shortfall
        double es = pos.calculateExpectedShortfall(0.05, 1);
        assert_test(es > var95, "Expected Shortfall is higher than VaR");
        std::cout << "    Expected Shortfall: $" << std::fixed << std::setprecision(2) << es << std::endl;

        // Test time decay risk
        double timeDecayRisk = pos.calculateTimeDecayRisk();
        assert_test(timeDecayRisk >= 0.0, "Time decay risk calculated");
        std::cout << "    Time decay risk: " << std::fixed << std::setprecision(4) << timeDecayRisk << std::endl;

        // Test beta setting
        pos.setBeta(1.5);
        assert_test(pos.getBeta() == 1.5, "Beta can be set correctly");

        // Test drawdown tracking
        pos.setCurrentPrice(45000.0); // Significant drop
        double drawdown = pos.getMaxPriceDrawdown();
        assert_test(drawdown > 0.0, "Maximum drawdown tracked correctly");
        std::cout << "    Maximum drawdown: " << std::fixed << std::setprecision(4) << (drawdown * 100.0) << "%" << std::endl;
    }

    void testPortfolioRiskMetrics() {
        std::cout << "\n--- Testing Portfolio-Level Risk Metrics ---" << std::endl;

        // Create a test portfolio
        Portfolio portfolio("RiskTestStrategy", 100000.0, 3.0); // $100k, 3x max leverage

        // Create multiple positions for concentration/correlation testing
        std::vector<Position> positions;
        positions.emplace_back("BTC", 1.0, 50000.0, true, 2.0);   // $50k position (50% concentration)
        positions.emplace_back("ETH", 10.0, 3000.0, true, 1.5);   // $30k position (30% concentration)
        positions.emplace_back("ADA", 5000.0, 2.0, true, 1.0);    // $10k position (10% concentration)
        positions.emplace_back("DOT", 500.0, 20.0, true, 1.0);    // $10k position (10% concentration)

        // Update positions with some price movement to generate volatility
        for (auto& pos : positions) {
            double basePrice = pos.getEntryPrice();
            pos.setCurrentPrice(basePrice * 1.02); // +2%
            pos.setCurrentPrice(basePrice * 0.97); // -3%
            pos.setCurrentPrice(basePrice * 1.05); // +5%
            pos.setCurrentPrice(basePrice * 1.01); // +1%
        }

        // Update portfolio value history for volatility calculation
        portfolio.setTotalValue(105000.0); // +5%
        portfolio.setTotalValue(98000.0);  // -7%
        portfolio.setTotalValue(108000.0); // +10%
        portfolio.setTotalValue(104000.0); // -4%

        // Test portfolio VaR calculation
        double portfolioVar95 = portfolio.calculatePortfolioVaR(positions, 0.05, 1);
        double portfolioVar99 = portfolio.calculatePortfolioVaR(positions, 0.01, 1);

        assert_test(portfolioVar95 > 0.0, "Portfolio VaR 95% calculated");
        assert_test(portfolioVar99 > portfolioVar95, "Portfolio VaR 99% > VaR 95%");
        std::cout << "    Portfolio VaR (95%): $" << std::fixed << std::setprecision(2) << portfolioVar95 << std::endl;
        std::cout << "    Portfolio VaR (99%): $" << std::fixed << std::setprecision(2) << portfolioVar99 << std::endl;

        // Test Expected Shortfall
        double portfolioES = portfolio.calculatePortfolioExpectedShortfall(positions, 0.05, 1);
        assert_test(portfolioES > portfolioVar95, "Portfolio ES > Portfolio VaR");
        std::cout << "    Portfolio Expected Shortfall: $" << std::fixed << std::setprecision(2) << portfolioES << std::endl;

        // Test concentration risk (should be high due to 50% BTC allocation)
        double concentrationRisk = portfolio.calculateConcentrationRisk(positions);
        assert_test(concentrationRisk > 0.3, "High concentration risk detected"); // HHI should be > 0.3
        std::cout << "    Concentration Risk (HHI): " << std::fixed << std::setprecision(4) << concentrationRisk << std::endl;

        // Test correlation risk
        double correlationRisk = portfolio.calculateCorrelationRisk(positions);
        assert_test(correlationRisk > 0.0, "Correlation risk calculated");
        std::cout << "    Correlation Risk: " << std::fixed << std::setprecision(4) << correlationRisk << std::endl;

        // Test diversification ratio
        portfolio.calculateDiversificationRatio(positions);
        double diversificationRatio = portfolio.getDiversificationRatio();
        assert_test(diversificationRatio > 0.0, "Diversification ratio calculated");
        std::cout << "    Diversification Ratio: " << std::fixed << std::setprecision(4) << diversificationRatio << std::endl;

        // Test sector exposure update
        portfolio.updateSectorExposure(positions);
        auto sectorExposure = portfolio.getSectorExposure();
        assert_test(!sectorExposure.empty(), "Sector exposure calculated");
        for (const auto& sector : sectorExposure) {
            std::cout << "    Sector " << sector.first << ": " << std::fixed << std::setprecision(2) << sector.second << "%" << std::endl;
        }

        // Test comprehensive risk metrics update
        portfolio.updatePortfolioRiskMetrics(positions);
        assert_test(portfolio.getPortfolioVaR() > 0.0, "Comprehensive risk update successful");
    }

    void testRiskReportingFramework() {
        std::cout << "\n--- Testing Risk Reporting Framework ---" << std::endl;

        // Create portfolio and positions for testing
        Portfolio portfolio("ReportTestStrategy", 50000.0, 2.5);
        std::vector<Position> positions;

        // Create a concentrated, high-risk portfolio
        positions.emplace_back("BTC", 1.5, 50000.0, true, 3.0); // Very concentrated position
        positions.emplace_back("ETH", 5.0, 3000.0, true, 2.0);

        // Set high margin utilization
        portfolio.setMarginUsed(40000.0); // 80% of available margin

        // Add price history for volatility
        for (auto& pos : positions) {
            double basePrice = pos.getEntryPrice();
            for (int i = 0; i < 10; ++i) {
                double variation = 0.95 + (0.1 * i / 10.0); // 5% variation
                pos.setCurrentPrice(basePrice * variation);
            }
        }

        // Update portfolio metrics
        portfolio.updatePortfolioRiskMetrics(positions);

        // Create and populate risk report
        RiskReport report;
        // Ensure portfolio has some volatility for VaR calculation
        portfolio.setTotalValue(110000.0);
        portfolio.setTotalValue(95000.0);
        portfolio.setTotalValue(105000.0);

        report.portfolioVaR95 = portfolio.calculatePortfolioVaR(positions, 0.05);
        report.portfolioVaR99 = portfolio.calculatePortfolioVaR(positions, 0.01);
        report.portfolioExpectedShortfall = portfolio.calculatePortfolioExpectedShortfall(positions);
        report.portfolioVolatility = portfolio.getPortfolioVolatility();
        report.concentrationRisk = portfolio.calculateConcentrationRisk(positions);
        report.correlationRisk = portfolio.calculateCorrelationRisk(positions);
        report.diversificationRatio = portfolio.getDiversificationRatio();

        // Populate position risks
        for (const auto& pos : positions) {
            RiskReport::PositionRisk posRisk;
            posRisk.symbol = pos.getSymbol();
            posRisk.positionVaR = pos.calculatePositionVaR();
            posRisk.expectedShortfall = pos.calculateExpectedShortfall();
            posRisk.volatility = pos.getVolatility();
            posRisk.timeDecayRisk = pos.getTimeDecayRisk();
            posRisk.deltaEquivalent = pos.getDeltaEquivalent();
            posRisk.maxDrawdown = pos.getMaxPriceDrawdown();
            report.positionRisks.push_back(posRisk);
        }

        // Test alert generation
        report.generateRiskAlerts(portfolio, positions);

        // Validate report
        assert_test(report.portfolioVaR95 > 0.0, "Risk report VaR populated");
        assert_test(report.portfolioVolatility >= 0.0, "Risk report volatility populated");
        assert_test(!report.positionRisks.empty(), "Position risks populated");
        assert_test(!report.alerts.empty(), "Risk alerts generated");

        std::cout << "    Generated " << report.alerts.size() << " risk alerts" << std::endl;
        std::cout << "    Risk Level: " << report.riskLevel << std::endl;
        std::cout << "    Total Risk Score: " << std::fixed << std::setprecision(2) << report.totalRiskScore << std::endl;

        // Display alerts
        for (const auto& alert : report.alerts) {
            std::cout << "    ALERT: " << alert.alertType << " - " << alert.message
                      << " (Severity: " << std::fixed << std::setprecision(2) << alert.severity << ")" << std::endl;
        }

        // Test risk score calculation
        assert_test(report.totalRiskScore > 0.0, "Risk score calculated");
        assert_test(!report.riskLevel.empty(), "Risk level assigned");
    }

    void testRiskCalculationAccuracy() {
        std::cout << "\n--- Testing Risk Calculation Accuracy ---" << std::endl;

        // Create position with known volatility scenario
        Position pos("TEST", 100.0, 100.0, true, 1.0); // 100 shares at $100

        // Create specific price sequence for predictable volatility
        std::vector<double> prices = {100.0, 102.0, 98.0, 104.0, 96.0, 103.0, 99.0, 105.0};

        for (double price : prices) {
            pos.setCurrentPrice(price);
        }

        double calculatedVol = pos.getVolatility();
        std::cout << "    Calculated annualized volatility: " << std::fixed << std::setprecision(4) << calculatedVol << std::endl;

        // Test VaR scaling with different confidence levels
        double var90 = pos.calculatePositionVaR(0.10, 1);
        double var95 = pos.calculatePositionVaR(0.05, 1);
        double var99 = pos.calculatePositionVaR(0.01, 1);

        assert_test(var99 > var95, "VaR increases with confidence level (99% > 95%)");
        assert_test(var95 > var90, "VaR increases with confidence level (95% > 90%)");

        std::cout << "    VaR 90%: $" << std::fixed << std::setprecision(2) << var90 << std::endl;
        std::cout << "    VaR 95%: $" << std::fixed << std::setprecision(2) << var95 << std::endl;
        std::cout << "    VaR 99%: $" << std::fixed << std::setprecision(2) << var99 << std::endl;

        // Test time horizon scaling
        double var1day = pos.calculatePositionVaR(0.05, 1);
        double var10day = pos.calculatePositionVaR(0.05, 10);

        assert_test(var10day > var1day, "VaR scales with time horizon");
        double expectedRatio = std::sqrt(10.0);
        double actualRatio = var10day / var1day;
        assert_near(actualRatio, expectedRatio, 0.1, "VaR time scaling follows sqrt rule");

        std::cout << "    VaR 1-day: $" << std::fixed << std::setprecision(2) << var1day << std::endl;
        std::cout << "    VaR 10-day: $" << std::fixed << std::setprecision(2) << var10day << std::endl;
        std::cout << "    Scaling ratio: " << std::fixed << std::setprecision(2) << actualRatio
                  << " (expected: " << std::fixed << std::setprecision(2) << expectedRatio << ")" << std::endl;
    }

    void testRiskAlertSystem() {
        std::cout << "\n--- Testing Risk Alert System ---" << std::endl;

        Portfolio portfolio("AlertTestStrategy", 100000.0, 3.0);
        std::vector<Position> positions;

        // Create scenarios that should trigger alerts

        // 1. High concentration risk (>50% in single position)
        positions.emplace_back("BTC", 2.0, 50000.0, true, 1.0); // $100k position = 100% concentration

        // 2. High margin utilization
        portfolio.setMarginUsed(280000.0); // >90% of 300k available margin

        // 3. High volatility position
        Position volatilePos("VOLATILE", 1000.0, 10.0, true, 1.0);
        for (int i = 0; i < 20; ++i) {
            double price = 10.0 * (0.5 + (rand() % 100) / 100.0); // High random variation
            volatilePos.setCurrentPrice(price);
        }
        positions.push_back(volatilePos);

        // 4. Stop-loss triggered position
        Position stopLossPos("STOPLOSS", 100.0, 20.0, true, 1.0);
        stopLossPos.setCurrentPrice(18.0); // Below stop-loss
        positions.push_back(stopLossPos);

        // Update portfolio metrics
        portfolio.updatePortfolioRiskMetrics(positions);

        // Generate risk report with alerts
        RiskReport report;
        report.concentrationRisk = portfolio.calculateConcentrationRisk(positions);
        report.portfolioVaR95 = portfolio.calculatePortfolioVaR(positions, 0.05);
        report.generateRiskAlerts(portfolio, positions);

        // Validate alerts were generated
        assert_test(!report.alerts.empty(), "Risk alerts generated for high-risk portfolio");

        bool hasConcentrationAlert = false;
        bool hasMarginAlert = false;
        bool hasVolatilityAlert = false;
        bool hasStopLossAlert = false;

        for (const auto& alert : report.alerts) {
            if (alert.alertType == "CONCENTRATION_RISK") hasConcentrationAlert = true;
            if (alert.alertType == "HIGH_MARGIN_USAGE") hasMarginAlert = true;
            if (alert.alertType == "HIGH_VOLATILITY") hasVolatilityAlert = true;
            if (alert.alertType == "STOP_LOSS_TRIGGERED") hasStopLossAlert = true;

            std::cout << "    ALERT: " << alert.alertType << " - Severity: "
                      << std::fixed << std::setprecision(2) << alert.severity << std::endl;
        }

        assert_test(hasConcentrationAlert, "Concentration risk alert triggered");
        assert_test(hasMarginAlert, "High margin usage alert triggered");
        assert_test(hasStopLossAlert, "Stop-loss alert triggered");

        // Test risk level classification
        assert_test(report.totalRiskScore > 50.0, "High risk score calculated");
        assert_test(report.riskLevel == "HIGH" || report.riskLevel == "CRITICAL", "Appropriate risk level assigned");

        std::cout << "    Final Risk Score: " << std::fixed << std::setprecision(2) << report.totalRiskScore << std::endl;
        std::cout << "    Risk Classification: " << report.riskLevel << std::endl;
    }

    void testRiskScenarios() {
        std::cout << "\n--- Testing Risk Calculation Scenarios ---" << std::endl;

        // Scenario 1: Well-diversified, low-risk portfolio
        {
            Portfolio lowRiskPortfolio("LowRisk", 100000.0, 2.0);
            std::vector<Position> positions;

            // Equal-weight diversified positions
            positions.emplace_back("BTC", 0.5, 50000.0, true, 1.2);   // $25k
            positions.emplace_back("ETH", 8.33, 3000.0, true, 1.2);   // $25k
            positions.emplace_back("ADA", 12500.0, 2.0, true, 1.2);   // $25k
            positions.emplace_back("DOT", 1250.0, 20.0, true, 1.2);   // $25k

            lowRiskPortfolio.updatePortfolioRiskMetrics(positions);

            double concentration = lowRiskPortfolio.calculateConcentrationRisk(positions);
            assert_test(concentration < 0.5, "Low concentration risk for diversified portfolio");
            std::cout << "    Low-risk portfolio concentration: " << std::fixed << std::setprecision(4) << concentration << std::endl;
        }

        // Scenario 2: High-leverage, concentrated portfolio
        {
            Portfolio highRiskPortfolio("HighRisk", 50000.0, 3.0);
            std::vector<Position> positions;

            // Single large leveraged position
            positions.emplace_back("BTC", 3.0, 50000.0, true, 3.0); // $150k position on $50k capital

            highRiskPortfolio.setMarginUsed(100000.0); // High margin usage
            highRiskPortfolio.updatePortfolioRiskMetrics(positions);

            double concentration = highRiskPortfolio.calculateConcentrationRisk(positions);
            assert_test(concentration > 0.8, "High concentration risk for single-asset portfolio");
            std::cout << "    High-risk portfolio concentration: " << std::fixed << std::setprecision(4) << concentration << std::endl;

            // Test margin utilization
            double marginUtilization = highRiskPortfolio.getMarginUtilization();
            assert_test(marginUtilization > 0.5, "High margin utilization detected");
            std::cout << "    Margin utilization: " << std::fixed << std::setprecision(2) << (marginUtilization * 100.0) << "%" << std::endl;
        }

        // Scenario 3: Stress test with extreme values
        {
            Position extremePos("EXTREME", 1.0, 10000.0, true, 1.0);

            // Simulate extreme price movements
            std::vector<double> extremePrices = {10000, 12000, 8000, 15000, 5000, 18000, 3000, 20000};
            for (double price : extremePrices) {
                extremePos.setCurrentPrice(price);
            }

            double extremeVol = extremePos.getVolatility();
            double extremeVaR = extremePos.calculatePositionVaR(0.05, 1);

            assert_test(extremeVol > 1.0, "Extreme volatility detected"); // >100% annualized
            assert_test(extremeVaR > 1000.0, "High VaR for volatile position");

            std::cout << "    Extreme volatility: " << std::fixed << std::setprecision(2) << (extremeVol * 100.0) << "%" << std::endl;
            std::cout << "    Extreme VaR: $" << std::fixed << std::setprecision(2) << extremeVaR << std::endl;
        }
    }

    void printSummary() {
        std::cout << "\n=== TEST SUMMARY ===" << std::endl;
        std::cout << "Tests Passed: " << testsPassed << "/" << testsTotal << std::endl;
        std::cout << "Success Rate: " << std::fixed << std::setprecision(1)
                  << (100.0 * testsPassed / testsTotal) << "%" << std::endl;

        if (testsPassed == testsTotal) {
            std::cout << "\n🎉 ALL TESTS PASSED! Day 6 Risk Analytics Implementation Verified! 🎉" << std::endl;
        } else {
            std::cout << "\n⚠️  Some tests failed. Review implementation." << std::endl;
        }
    }
};

int main() {
    std::cout << "CryptoClaude Day 6 Risk Analytics Testing Suite" << std::endl;
    std::cout << "===============================================" << std::endl;

    Day6RiskAnalyticsTest tester;
    tester.runAllTests();

    return 0;
}