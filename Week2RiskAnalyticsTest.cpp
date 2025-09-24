#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <chrono>
#include "src/Core/Database/Models/PortfolioData.h"

using namespace CryptoClaude::Database::Models;

void testVaRCalculations() {
    std::cout << "Testing VaR calculations..." << std::endl;

    Portfolio portfolio;
    portfolio.setTotalValue(100000.0);

    // Create sample positions
    std::vector<Position> positions;

    Position btc("BTC", 1.0, 40000.0, true, 2.0); // symbol, qty, entry_price, is_long, leverage
    btc.setCurrentPrice(45000.0);
    positions.push_back(btc);

    Position eth("ETH", 10.0, 2500.0, true, 1.5);
    eth.setCurrentPrice(3000.0);
    positions.push_back(eth);

    // First add some value history to calculate volatility
    portfolio.setTotalValue(95000.0);
    portfolio.setTotalValue(102000.0);
    portfolio.setTotalValue(98000.0);
    portfolio.setTotalValue(105000.0);
    portfolio.setTotalValue(100000.0); // Final value

    // Debug: Check portfolio volatility
    double volatility = portfolio.getPortfolioVolatility();
    std::cout << "Portfolio volatility: " << volatility << std::endl;

    // Test VaR calculation
    double var95 = portfolio.calculatePortfolioVaR(positions, 0.05, 1); // 95% confidence, 1 day
    std::cout << "Calculated VaR (95%): $" << var95 << std::endl;
    assert(var95 > 0.0);
    std::cout << "✅ VaR (95%, 1-day): $" << var95 << std::endl;

    double var99 = portfolio.calculatePortfolioVaR(positions, 0.01, 1); // 99% confidence, 1 day
    assert(var99 > var95); // 99% VaR should be higher than 95%
    std::cout << "✅ VaR (99%, 1-day): $" << var99 << std::endl;

    // Test Expected Shortfall
    double es = portfolio.calculatePortfolioExpectedShortfall(positions, 0.05, 1);
    assert(es > var95); // ES should be higher than VaR
    std::cout << "✅ Expected Shortfall: $" << es << std::endl;
}

void testCorrelationAnalysis() {
    std::cout << "Testing correlation analysis..." << std::endl;

    Portfolio portfolio;
    std::vector<Position> positions;

    // Create diversified positions
    Position btc("BTC", 1.0, 40000.0, true, 2.0);
    btc.setCurrentPrice(45000.0);

    Position eth("ETH", 10.0, 2500.0, true, 1.8);
    eth.setCurrentPrice(3000.0);

    Position ada("ADA", 1000.0, 1.0, true, 1.2);
    ada.setCurrentPrice(1.5);

    positions.push_back(btc);
    positions.push_back(eth);
    positions.push_back(ada);

    // Test concentration risk
    double concentrationRisk = portfolio.calculateConcentrationRisk(positions);
    assert(concentrationRisk > 0.0 && concentrationRisk <= 1.0);
    std::cout << "✅ Concentration Risk (HHI): " << concentrationRisk << std::endl;

    // Test correlation risk
    double correlationRisk = portfolio.calculateCorrelationRisk(positions);
    assert(correlationRisk >= 0.0);
    std::cout << "✅ Correlation Risk: " << correlationRisk << std::endl;

    // Test diversification ratio
    portfolio.calculateDiversificationRatio(positions);
    double diversificationRatio = portfolio.getDiversificationRatio();
    assert(diversificationRatio > 0.0);
    std::cout << "✅ Diversification Ratio: " << diversificationRatio << std::endl;
}

void testPerformanceMetrics() {
    std::cout << "Testing performance metrics..." << std::endl;

    // Test BacktestResult performance metrics
    BacktestResult result("Long-Short Strategy", 100000.0);
    result.setFinalValue(120000.0);
    result.setSharpeRatio(1.8);
    result.setMaxDrawdown(0.15);
    result.setTotalTrades(150);
    result.setWinRate(0.65);
    result.setMaxLeverageUsed(2.5);
    result.setAvgLeverage(1.8);

    assert(std::abs(result.getTotalReturn() - 20.0) < 0.001); // Should calculate 20.0% return
    assert(result.getSharpeRatio() == 1.8);
    assert(result.getMaxDrawdown() == 0.15);
    assert(result.getWinRate() == 0.65);

    std::cout << "✅ Performance Metrics:" << std::endl;
    std::cout << "   - Total Return: " << result.getTotalReturn() << "%" << std::endl;
    std::cout << "   - Sharpe Ratio: " << result.getSharpeRatio() << std::endl;
    std::cout << "   - Max Drawdown: " << result.getMaxDrawdown() * 100 << "%" << std::endl;
    std::cout << "   - Win Rate: " << result.getWinRate() * 100 << "%" << std::endl;
    std::cout << "   - Max Leverage: " << result.getMaxLeverageUsed() << "x" << std::endl;
}

void testPortfolioVolatilityCalculation() {
    std::cout << "Testing portfolio volatility calculation..." << std::endl;

    Portfolio portfolio;

    // Simulate value history for volatility calculation
    std::vector<double> values = {100000, 102000, 98000, 101000, 99000, 103000, 97000, 105000};

    for (double value : values) {
        portfolio.setTotalValue(value); // This should update value history and calculate volatility
    }

    double volatility = portfolio.getPortfolioVolatility();
    assert(volatility > 0.0);
    std::cout << "✅ Portfolio Volatility (annualized): " << volatility * 100 << "%" << std::endl;
}

void testComprehensiveRiskUpdate() {
    std::cout << "Testing comprehensive risk metrics update..." << std::endl;

    Portfolio portfolio;

    // Add volatility history first
    portfolio.setTotalValue(95000.0);
    portfolio.setTotalValue(102000.0);
    portfolio.setTotalValue(98000.0);
    portfolio.setTotalValue(105000.0);
    portfolio.setTotalValue(100000.0); // Final value

    std::vector<Position> positions;
    Position btc("BTC", 1.0, 40000.0, true, 2.5);
    btc.setCurrentPrice(45000.0);

    Position eth("ETH", 15.0, 2800.0, true, 2.0);
    eth.setCurrentPrice(3000.0);

    positions.push_back(btc);
    positions.push_back(eth);

    // Test comprehensive risk update
    portfolio.updatePortfolioRiskMetrics(positions);

    std::cout << "Debug - VaR: " << portfolio.getPortfolioVaR() << std::endl;
    std::cout << "Debug - ES: " << portfolio.getPortfolioExpectedShortfall() << std::endl;
    std::cout << "Debug - Concentration: " << portfolio.getConcentrationRisk() << std::endl;
    std::cout << "Debug - Correlation: " << portfolio.getCorrelationRisk() << std::endl;

    assert(portfolio.getPortfolioVaR() > 0.0);
    assert(portfolio.getPortfolioExpectedShortfall() > 0.0);
    assert(portfolio.getConcentrationRisk() > 0.0);
    assert(portfolio.getCorrelationRisk() >= 0.0);

    std::cout << "✅ Comprehensive Risk Metrics Updated:" << std::endl;
    std::cout << "   - VaR: $" << portfolio.getPortfolioVaR() << std::endl;
    std::cout << "   - Expected Shortfall: $" << portfolio.getPortfolioExpectedShortfall() << std::endl;
    std::cout << "   - Concentration Risk: " << portfolio.getConcentrationRisk() << std::endl;
    std::cout << "   - Correlation Risk: " << portfolio.getCorrelationRisk() << std::endl;
    std::cout << "   - Diversification Ratio: " << portfolio.getDiversificationRatio() << std::endl;
}

int main() {
    std::cout << "Week 2 Risk Analytics Testing" << std::endl;
    std::cout << "=============================" << std::endl;
    std::cout << std::endl;

    try {
        testVaRCalculations();
        std::cout << std::endl;

        testCorrelationAnalysis();
        std::cout << std::endl;

        testPerformanceMetrics();
        std::cout << std::endl;

        testPortfolioVolatilityCalculation();
        std::cout << std::endl;

        testComprehensiveRiskUpdate();
        std::cout << std::endl;

        std::cout << "=============================" << std::endl;
        std::cout << "🎉 WEEK 2 RISK ANALYTICS VERIFIED!" << std::endl;
        std::cout << "✅ VaR calculations: IMPLEMENTED" << std::endl;
        std::cout << "✅ Correlation analysis: IMPLEMENTED" << std::endl;
        std::cout << "✅ Concentration risk: IMPLEMENTED" << std::endl;
        std::cout << "✅ Performance metrics: IMPLEMENTED" << std::endl;
        std::cout << "✅ Portfolio volatility: IMPLEMENTED" << std::endl;
        std::cout << "✅ Comprehensive risk updates: IMPLEMENTED" << std::endl;
        std::cout << std::endl;
        std::cout << "🚀 Week 2 objectives appear to be complete!" << std::endl;

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "❌ Week 2 risk analytics test failed: " << e.what() << std::endl;
        return 1;
    }
}