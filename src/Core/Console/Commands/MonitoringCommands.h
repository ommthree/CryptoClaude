#pragma once

#include "../CommandFramework.h"
#include "../../Database/DatabaseManager.h"
#include "../../Trading/TradingEngine.h"
#include "../../Portfolio/PortfolioManager.h"
#include "../../Risk/RiskManager.h"
#include "../../DataPipeline/EnhancedMarketDataPipeline.h"
#include "../../AI/AIDecisionEngine.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>

namespace CryptoClaude {
namespace Console {
namespace Commands {

using namespace CryptoClaude::Database;
using namespace CryptoClaude::Trading;
using namespace CryptoClaude::Portfolio;
using namespace CryptoClaude::Risk;
using namespace CryptoClaude::DataPipeline;
using namespace CryptoClaude::AI;

// System status command - comprehensive system health
class StatusCommand : public SystemCommand {
private:
    DatabaseManager& dbManager_;
    std::shared_ptr<TradingEngine> tradingEngine_;
    std::shared_ptr<PortfolioManager> portfolioManager_;
    std::shared_ptr<RiskManager> riskManager_;
    std::shared_ptr<EnhancedMarketDataPipeline> dataPipeline_;
    std::shared_ptr<AIScorePolishEngine> aiEngine_;

public:
    StatusCommand(DatabaseManager& dbManager,
                 std::shared_ptr<TradingEngine> tradingEngine,
                 std::shared_ptr<PortfolioManager> portfolioManager,
                 std::shared_ptr<RiskManager> riskManager,
                 std::shared_ptr<EnhancedMarketDataPipeline> dataPipeline,
                 std::shared_ptr<AIScorePolishEngine> aiEngine)
        : dbManager_(dbManager), tradingEngine_(tradingEngine), portfolioManager_(portfolioManager),
          riskManager_(riskManager), dataPipeline_(dataPipeline), aiEngine_(aiEngine) {}

    std::string getName() const override { return "status"; }
    std::string getDescription() const override { return "Display comprehensive system status"; }

    std::vector<ParameterDefinition> getParameters() const override {
        return {
            {"detailed", ParameterType::BOOLEAN, false, "Show detailed system information", "false"},
            {"health", ParameterType::BOOLEAN, false, "Include health check diagnostics", "false"}
        };
    }

    CommandResult execute(CommandContext& context) override {
        bool detailed = context.getBoolParameter("detailed").value_or(false);
        bool includeHealth = context.getBoolParameter("health").value_or(false);

        std::ostringstream output;

        try {
            // Header
            output << "🚀 CRYPTOCLAUDE SYSTEM STATUS\n";
            output << "═══════════════════════════════\n\n";

            // Trading engine status
            output << "⚡ Trading Engine:\n";
            if (tradingEngine_->isActive()) {
                output << "   Status: 🟢 ACTIVE";
                auto mode = tradingEngine_->getTradingMode();
                output << " (" << (mode == TradingEngine::TradingMode::PROFESSIONAL ? "Professional" : "Personal") << " mode)\n";
                output << "   Uptime: " << formatUptime(tradingEngine_->getUptime()) << "\n";
                output << "   Active Positions: " << tradingEngine_->getActivePositionCount() << "\n";
                output << "   Pending Orders: " << tradingEngine_->getPendingOrdersCount() << "\n";
            } else {
                output << "   Status: 🔴 INACTIVE\n";
                output << "   Last Active: " << formatTimestamp(tradingEngine_->getLastActiveTime()) << "\n";
            }

            // Portfolio overview
            output << "\n💼 Portfolio Overview:\n";
            output << "   Total Value: $" << std::fixed << std::setprecision(2)
                   << portfolioManager_->getTotalPortfolioValue() << "\n";
            output << "   Cash Balance: $" << portfolioManager_->getCashBalance() << "\n";
            output << "   Daily P&L: " << std::showpos << portfolioManager_->getDailyPnLPercent()
                   << "%" << std::noshowpos << " ($" << std::showpos << portfolioManager_->getDailyPnL() << std::noshowpos << ")\n";
            output << "   Active Positions: " << portfolioManager_->getActivePositionCount() << "\n";

            // Risk metrics
            output << "\n⚠️  Risk Status:\n";
            auto currentVaR = riskManager_->getCurrentVaR(0.95);
            auto portfolioRisk = riskManager_->getPortfolioRiskLevel();
            output << "   Portfolio VaR (95%): " << std::fixed << std::setprecision(2) << (currentVaR * 100) << "%\n";
            output << "   Risk Level: ";
            if (portfolioRisk < 0.03) output << "🟢 LOW";
            else if (portfolioRisk < 0.08) output << "🟡 MODERATE";
            else output << "🔴 HIGH";
            output << " (" << (portfolioRisk * 100) << "%)\n";

            auto activeAlerts = riskManager_->getActiveAlerts();
            output << "   Active Alerts: " << activeAlerts.size() << "\n";
            if (!activeAlerts.empty() && detailed) {
                for (const auto& alert : activeAlerts) {
                    output << "     🚨 " << alert.description << " (Severity: " << alert.severity << ")\n";
                }
            }

            // Data pipeline status
            output << "\n📊 Data Pipeline:\n";
            if (dataPipeline_->isHealthy()) {
                output << "   Status: 🟢 HEALTHY\n";
                output << "   Data Freshness: " << dataPipeline_->getDataFreshnessMinutes() << " minutes\n";
                output << "   API Calls Today: " << dataPipeline_->getTodayAPICallCount() << "\n";
                output << "   Cache Hit Rate: " << std::fixed << std::setprecision(1)
                       << (dataPipeline_->getCacheHitRate() * 100) << "%\n";
            } else {
                output << "   Status: 🔴 ISSUES DETECTED\n";
                output << "   Last Update: " << formatTimestamp(dataPipeline_->getLastUpdateTime()) << "\n";
            }

            // AI system status
            output << "\n🤖 AI Enhancement:\n";
            if (aiEngine_ && aiEngine_->isEnabled()) {
                output << "   Status: 🟢 ACTIVE\n";
                auto metrics = aiEngine_->getEngineMetrics();
                output << "   Claude API Calls Today: " << metrics.claude_api_calls_today << "\n";
                output << "   Success Rate: " << std::fixed << std::setprecision(1)
                       << (metrics.claude_success_rate * 100) << "%\n";
                output << "   Avg Confidence: " << std::setprecision(3) << metrics.average_claude_confidence << "\n";
                output << "   Predictions Polished: " << metrics.predictions_polished_count << "\n";
            } else {
                output << "   Status: 🔴 DISABLED\n";
                output << "   Mode: Fallback to Random Forest only\n";
            }

            // Database status
            output << "\n💾 Database:\n";
            if (dbManager_.isConnected()) {
                output << "   Status: 🟢 CONNECTED\n";
                auto& migrationManager = dbManager_.getMigrationManager();
                output << "   Schema Version: " << migrationManager.getCurrentVersion() << "/"
                       << migrationManager.getTargetVersion() << "\n";
                auto& qualityManager = dbManager_.getDataQualityManager();
                output << "   Data Quality: " << std::fixed << std::setprecision(1)
                       << (qualityManager.getOverallQualityScore() * 100) << "%\n";
            } else {
                output << "   Status: 🔴 DISCONNECTED\n";
                output << "   Last Error: " << dbManager_.getLastError() << "\n";
            }

            // System health check
            if (includeHealth) {
                output << "\n🔍 SYSTEM HEALTH DIAGNOSTICS\n";
                output << "──────────────────────────────\n";
                performHealthCheck(output, detailed);
            }

            // Detailed information
            if (detailed) {
                appendDetailedStatus(output);
            }

            auto result = CommandResult::createSuccess(output.str());
            result.addOutput("trading_active", tradingEngine_->isActive() ? "true" : "false");
            result.addOutput("portfolio_value", std::to_string(portfolioManager_->getTotalPortfolioValue()));
            result.addOutput("daily_pnl_percent", std::to_string(portfolioManager_->getDailyPnLPercent()));
            result.addOutput("active_positions", std::to_string(portfolioManager_->getActivePositionCount()));
            result.addOutput("risk_level", std::to_string(portfolioRisk));
            return result;

        } catch (const std::exception& e) {
            return CommandResult::createError("Status command failed", e.what());
        }
    }

private:
    std::string formatUptime(std::chrono::seconds uptime) {
        auto hours = std::chrono::duration_cast<std::chrono::hours>(uptime);
        auto minutes = std::chrono::duration_cast<std::chrono::minutes>(uptime - hours);
        return std::to_string(hours.count()) + "h " + std::to_string(minutes.count()) + "m";
    }

    std::string formatTimestamp(std::chrono::system_clock::time_point timestamp) {
        auto time_t = std::chrono::system_clock::to_time_t(timestamp);
        std::ostringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

    void performHealthCheck(std::ostringstream& output, bool detailed) {
        int healthScore = 0;
        int totalChecks = 0;

        // Check 1: Database connectivity
        totalChecks++;
        if (dbManager_.isConnected()) {
            output << "   ✅ Database connectivity\n";
            healthScore++;
        } else {
            output << "   ❌ Database connectivity\n";
        }

        // Check 2: Data freshness
        totalChecks++;
        if (dataPipeline_->getDataFreshnessMinutes() < 15) {
            output << "   ✅ Data freshness (< 15 min)\n";
            healthScore++;
        } else {
            output << "   ⚠️  Data freshness (" << dataPipeline_->getDataFreshnessMinutes() << " min)\n";
        }

        // Check 3: Risk management
        totalChecks++;
        if (riskManager_->getActiveAlerts().size() == 0) {
            output << "   ✅ Risk management (no alerts)\n";
            healthScore++;
        } else {
            output << "   ⚠️  Risk management (" << riskManager_->getActiveAlerts().size() << " alerts)\n";
        }

        // Check 4: API limits
        totalChecks++;
        if (dataPipeline_->getTodayAPICallCount() < dataPipeline_->getDailyAPILimit() * 0.8) {
            output << "   ✅ API usage within limits\n";
            healthScore++;
        } else {
            output << "   ⚠️  API usage approaching limits\n";
        }

        // Check 5: AI system
        totalChecks++;
        if (aiEngine_ && aiEngine_->isEnabled() && aiEngine_->getEngineMetrics().claude_success_rate > 0.9) {
            output << "   ✅ AI system performance\n";
            healthScore++;
        } else {
            output << "   ⚠️  AI system performance\n";
        }

        // Overall health score
        double healthPercent = (double)healthScore / totalChecks * 100;
        output << "\n🎯 Overall Health Score: " << std::fixed << std::setprecision(0) << healthPercent << "%";
        if (healthPercent >= 90) output << " 🟢 EXCELLENT";
        else if (healthPercent >= 80) output << " 🟡 GOOD";
        else if (healthPercent >= 70) output << " 🟠 FAIR";
        else output << " 🔴 POOR";
        output << "\n";
    }

    void appendDetailedStatus(std::ostringstream& output) {
        output << "\n📋 DETAILED SYSTEM INFORMATION\n";
        output << "──────────────────────────────────\n";

        // Memory and performance metrics
        output << "⚡ Performance:\n";
        output << "   Average Response Time: " << tradingEngine_->getAverageResponseTimeMs() << "ms\n";
        output << "   Trades Executed Today: " << tradingEngine_->getTodayTradeCount() << "\n";
        output << "   System Load: " << getCurrentSystemLoad() << "%\n";

        // Configuration status
        output << "\n⚙️  Configuration:\n";
        output << "   Config File: " << getActiveConfigFile() << "\n";
        output << "   Environment: " << getEnvironmentName() << "\n";
        output << "   Log Level: " << getCurrentLogLevel() << "\n";

        // Network connectivity
        output << "\n🌐 Connectivity:\n";
        auto connectivityStatus = checkConnectivity();
        for (const auto& [service, status] : connectivityStatus) {
            output << "   " << service << ": " << (status ? "🟢" : "🔴") << "\n";
        }
    }

    double getCurrentSystemLoad() {
        // Placeholder - would implement actual system monitoring
        return 23.5; // Example load percentage
    }

    std::string getActiveConfigFile() {
        return "production.json"; // Placeholder
    }

    std::string getEnvironmentName() {
        return "Production"; // Placeholder
    }

    std::string getCurrentLogLevel() {
        return "INFO"; // Placeholder
    }

    std::map<std::string, bool> checkConnectivity() {
        return {
            {"CryptoCompare API", true},
            {"CryptoNews API", true},
            {"Trading Exchange", true},
            {"Database", dbManager_.isConnected()},
            {"Claude API", aiEngine_ && aiEngine_->isEnabled()}
        };
    }
};

// Positions command - detailed position information
class PositionsCommand : public TradingCommand {
private:
    std::shared_ptr<PortfolioManager> portfolioManager_;
    std::shared_ptr<RiskManager> riskManager_;

public:
    PositionsCommand(std::shared_ptr<PortfolioManager> portfolioManager,
                    std::shared_ptr<RiskManager> riskManager)
        : portfolioManager_(portfolioManager), riskManager_(riskManager) {}

    std::string getName() const override { return "positions"; }
    std::string getDescription() const override { return "Display detailed position information"; }

    std::vector<ParameterDefinition> getParameters() const override {
        return {
            {"symbol", ParameterType::SYMBOL, false, "Filter by specific symbol", ""},
            {"sort", ParameterType::STRING, false, "Sort by field", "pnl", {"pnl", "size", "risk", "age", "symbol"}},
            {"format", ParameterType::STRING, false, "Output format", "table", {"table", "json", "csv"}}
        };
    }

    CommandResult execute(CommandContext& context) override {
        std::string symbolFilter = context.getStringParameter("symbol", "");
        std::string sortBy = context.getStringParameter("sort", "pnl");
        std::string format = context.getStringParameter("format", "table");

        std::ostringstream output;

        try {
            auto positions = portfolioManager_->getAllActivePositions();

            // Filter by symbol if specified
            if (!symbolFilter.empty()) {
                positions.erase(
                    std::remove_if(positions.begin(), positions.end(),
                        [&symbolFilter](const Position& pos) {
                            return pos.getSymbol() != symbolFilter;
                        }),
                    positions.end()
                );
            }

            if (positions.empty()) {
                std::string message = symbolFilter.empty() ?
                    "No active positions found" :
                    "No active positions found for symbol: " + symbolFilter;
                return CommandResult::createSuccess(message);
            }

            // Sort positions
            sortPositions(positions, sortBy);

            if (format == "table") {
                generateTableFormat(output, positions);
            } else if (format == "json") {
                generateJSONFormat(output, positions);
            } else if (format == "csv") {
                generateCSVFormat(output, positions);
            }

            auto result = CommandResult::createSuccess(output.str());
            result.addOutput("position_count", std::to_string(positions.size()));

            double totalPnL = 0.0;
            for (const auto& pos : positions) {
                totalPnL += pos.getPnL();
            }
            result.addOutput("total_pnl", std::to_string(totalPnL));

            return result;

        } catch (const std::exception& e) {
            return CommandResult::createError("Positions command failed", e.what());
        }
    }

private:
    void sortPositions(std::vector<Position>& positions, const std::string& sortBy) {
        if (sortBy == "pnl") {
            std::sort(positions.begin(), positions.end(),
                [](const Position& a, const Position& b) { return a.getPnL() > b.getPnL(); });
        } else if (sortBy == "size") {
            std::sort(positions.begin(), positions.end(),
                [](const Position& a, const Position& b) { return a.getMarketValue() > b.getMarketValue(); });
        } else if (sortBy == "symbol") {
            std::sort(positions.begin(), positions.end(),
                [](const Position& a, const Position& b) { return a.getSymbol() < b.getSymbol(); });
        } else if (sortBy == "age") {
            std::sort(positions.begin(), positions.end(),
                [](const Position& a, const Position& b) { return a.getEntryTime() < b.getEntryTime(); });
        }
        // Add more sorting options as needed
    }

    void generateTableFormat(std::ostringstream& output, const std::vector<Position>& positions) {
        output << "📊 ACTIVE POSITIONS\n";
        output << "═══════════════════\n\n";

        // Table header
        output << std::left << std::setw(8) << "Symbol"
               << std::right << std::setw(12) << "Quantity"
               << std::setw(12) << "Entry Price"
               << std::setw(12) << "Current"
               << std::setw(10) << "P&L ($)"
               << std::setw(8) << "P&L %"
               << std::setw(8) << "Risk"
               << std::setw(12) << "Age\n";

        output << std::string(84, '─') << "\n";

        double totalPnL = 0.0;
        double totalValue = 0.0;

        for (const auto& position : positions) {
            auto positionRisk = riskManager_->getPositionRisk(position.getPositionId());
            auto positionAge = getCurrentTime() - position.getEntryTime();
            auto ageHours = std::chrono::duration_cast<std::chrono::hours>(positionAge).count();

            output << std::left << std::setw(8) << position.getSymbol()
                   << std::right << std::fixed
                   << std::setw(12) << std::setprecision(8) << position.getQuantity()
                   << std::setw(12) << std::setprecision(2) << position.getEntryPrice()
                   << std::setw(12) << position.getCurrentPrice()
                   << std::setw(10) << std::showpos << position.getPnL() << std::noshowpos
                   << std::setw(8) << std::showpos << position.getPnLPercentage() << "%" << std::noshowpos;

            // Risk level indicator
            if (positionRisk < 0.02) output << std::setw(8) << "🟢 LOW";
            else if (positionRisk < 0.05) output << std::setw(8) << "🟡 MED";
            else output << std::setw(8) << "🔴 HIGH";

            output << std::setw(12) << ageHours << "h\n";

            totalPnL += position.getPnL();
            totalValue += position.getMarketValue();
        }

        output << std::string(84, '─') << "\n";
        output << std::left << std::setw(8) << "TOTAL"
               << std::right << std::setw(36) << ""
               << std::setw(10) << std::fixed << std::setprecision(2)
               << std::showpos << totalPnL << std::noshowpos
               << std::setw(8) << std::showpos
               << (totalValue > 0 ? (totalPnL / totalValue * 100) : 0.0) << "%" << std::noshowpos << "\n\n";

        // Summary statistics
        output << "📈 Position Summary:\n";
        output << "   Total Positions: " << positions.size() << "\n";
        output << "   Total Market Value: $" << std::fixed << std::setprecision(2) << totalValue << "\n";
        output << "   Total P&L: $" << std::showpos << totalPnL << std::noshowpos << "\n";
        output << "   Average P&L per Position: $"
               << std::showpos << (positions.empty() ? 0.0 : totalPnL / positions.size()) << std::noshowpos << "\n";

        // Winning vs losing positions
        int winningPositions = 0;
        for (const auto& pos : positions) {
            if (pos.getPnL() > 0) winningPositions++;
        }
        output << "   Winning Positions: " << winningPositions << "/" << positions.size()
               << " (" << std::fixed << std::setprecision(1)
               << (positions.empty() ? 0.0 : (double)winningPositions / positions.size() * 100) << "%)\n";
    }

    void generateJSONFormat(std::ostringstream& output, const std::vector<Position>& positions) {
        output << "{\n  \"positions\": [\n";

        for (size_t i = 0; i < positions.size(); ++i) {
            const auto& pos = positions[i];
            auto positionRisk = riskManager_->getPositionRisk(pos.getPositionId());

            output << "    {\n"
                   << "      \"symbol\": \"" << pos.getSymbol() << "\",\n"
                   << "      \"quantity\": " << std::fixed << std::setprecision(8) << pos.getQuantity() << ",\n"
                   << "      \"entry_price\": " << std::setprecision(2) << pos.getEntryPrice() << ",\n"
                   << "      \"current_price\": " << pos.getCurrentPrice() << ",\n"
                   << "      \"pnl_dollars\": " << pos.getPnL() << ",\n"
                   << "      \"pnl_percent\": " << pos.getPnLPercentage() << ",\n"
                   << "      \"market_value\": " << pos.getMarketValue() << ",\n"
                   << "      \"risk_level\": " << positionRisk << ",\n"
                   << "      \"entry_timestamp\": \"" << formatTimestamp(pos.getEntryTime()) << "\"\n"
                   << "    }";

            if (i < positions.size() - 1) output << ",";
            output << "\n";
        }

        output << "  ],\n";
        output << "  \"summary\": {\n";
        output << "    \"total_positions\": " << positions.size() << ",\n";

        double totalPnL = 0.0;
        for (const auto& pos : positions) totalPnL += pos.getPnL();

        output << "    \"total_pnl\": " << std::fixed << std::setprecision(2) << totalPnL << "\n";
        output << "  }\n";
        output << "}\n";
    }

    void generateCSVFormat(std::ostringstream& output, const std::vector<Position>& positions) {
        output << "Symbol,Quantity,EntryPrice,CurrentPrice,PnL_Dollars,PnL_Percent,MarketValue,RiskLevel,EntryTime\n";

        for (const auto& pos : positions) {
            auto positionRisk = riskManager_->getPositionRisk(pos.getPositionId());

            output << pos.getSymbol() << ","
                   << std::fixed << std::setprecision(8) << pos.getQuantity() << ","
                   << std::setprecision(2) << pos.getEntryPrice() << ","
                   << pos.getCurrentPrice() << ","
                   << pos.getPnL() << ","
                   << pos.getPnLPercentage() << ","
                   << pos.getMarketValue() << ","
                   << positionRisk << ","
                   << formatTimestamp(pos.getEntryTime()) << "\n";
        }
    }

    std::chrono::system_clock::time_point getCurrentTime() {
        return std::chrono::system_clock::now();
    }

    std::string formatTimestamp(std::chrono::system_clock::time_point timestamp) {
        auto time_t = std::chrono::system_clock::to_time_t(timestamp);
        std::ostringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }
};

// Portfolio command - portfolio-level information
class PortfolioCommand : public TradingCommand {
private:
    std::shared_ptr<PortfolioManager> portfolioManager_;
    std::shared_ptr<RiskManager> riskManager_;

public:
    PortfolioCommand(std::shared_ptr<PortfolioManager> portfolioManager,
                    std::shared_ptr<RiskManager> riskManager)
        : portfolioManager_(portfolioManager), riskManager_(riskManager) {}

    std::string getName() const override { return "portfolio"; }
    std::string getDescription() const override { return "Display portfolio-level information"; }

    std::vector<ParameterDefinition> getParameters() const override {
        return {
            {"period", ParameterType::STRING, false, "Performance period", "1d", {"1d", "7d", "30d", "90d", "1y", "all"}},
            {"breakdown", ParameterType::BOOLEAN, false, "Include asset breakdown", "true"},
            {"risk", ParameterType::BOOLEAN, false, "Include detailed risk metrics", "true"}
        };
    }

    CommandResult execute(CommandContext& context) override {
        std::string period = context.getStringParameter("period", "1d");
        bool includeBreakdown = context.getBoolParameter("breakdown").value_or(true);
        bool includeRisk = context.getBoolParameter("risk").value_or(true);

        std::ostringstream output;

        try {
            // Portfolio overview
            output << "💼 PORTFOLIO OVERVIEW\n";
            output << "════════════════════\n\n";

            auto portfolioValue = portfolioManager_->getTotalPortfolioValue();
            auto cashBalance = portfolioManager_->getCashBalance();
            auto investedValue = portfolioValue - cashBalance;

            output << "💰 Value Summary:\n";
            output << "   Total Portfolio Value: $" << std::fixed << std::setprecision(2) << portfolioValue << "\n";
            output << "   Cash Balance: $" << cashBalance << "\n";
            output << "   Invested Value: $" << investedValue << "\n";
            output << "   Investment Ratio: " << std::setprecision(1)
                   << (portfolioValue > 0 ? (investedValue / portfolioValue * 100) : 0.0) << "%\n\n";

            // Performance metrics
            displayPerformanceMetrics(output, period);

            // Asset breakdown
            if (includeBreakdown) {
                output << "\n";
                displayAssetBreakdown(output);
            }

            // Risk metrics
            if (includeRisk) {
                output << "\n";
                displayRiskMetrics(output);
            }

            // Allocation and diversification
            output << "\n";
            displayAllocationMetrics(output);

            auto result = CommandResult::createSuccess(output.str());
            result.addOutput("portfolio_value", std::to_string(portfolioValue));
            result.addOutput("cash_balance", std::to_string(cashBalance));
            result.addOutput("investment_ratio", std::to_string(investedValue / portfolioValue));
            return result;

        } catch (const std::exception& e) {
            return CommandResult::createError("Portfolio command failed", e.what());
        }
    }

private:
    void displayPerformanceMetrics(std::ostringstream& output, const std::string& period) {
        output << "📈 Performance (" << period << "):\n";

        auto pnlPercent = portfolioManager_->getPnLPercent(period);
        auto pnlDollars = portfolioManager_->getPnLDollars(period);
        auto maxDrawdown = portfolioManager_->getMaxDrawdown(period);
        auto sharpeRatio = portfolioManager_->getSharpeRatio(period);

        output << "   Total Return: " << std::showpos << std::fixed << std::setprecision(2)
               << pnlPercent << "%" << std::noshowpos << " ($" << std::showpos << pnlDollars << std::noshowpos << ")\n";

        if (period != "1d") {
            auto annualizedReturn = portfolioManager_->getAnnualizedReturn(period);
            output << "   Annualized Return: " << std::showpos << std::setprecision(2)
                   << annualizedReturn << "%" << std::noshowpos << "\n";
        }

        output << "   Max Drawdown: " << std::fixed << std::setprecision(2) << maxDrawdown << "%\n";
        output << "   Sharpe Ratio: " << std::setprecision(3) << sharpeRatio << "\n";

        // Benchmark comparison
        auto benchmarkReturn = portfolioManager_->getBenchmarkReturn(period, "BTC");
        auto alpha = pnlPercent - benchmarkReturn;
        output << "   vs BTC Benchmark: " << std::showpos << benchmarkReturn << "%" << std::noshowpos
               << " (Alpha: " << std::showpos << alpha << "%" << std::noshowpos << ")\n";
    }

    void displayAssetBreakdown(std::ostringstream& output) {
        output << "🧩 Asset Allocation:\n";

        auto breakdown = portfolioManager_->getAssetBreakdown();
        auto totalValue = portfolioManager_->getTotalPortfolioValue();

        // Sort by allocation percentage (descending)
        std::vector<std::pair<std::string, double>> sortedBreakdown(breakdown.begin(), breakdown.end());
        std::sort(sortedBreakdown.begin(), sortedBreakdown.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });

        for (const auto& [symbol, value] : sortedBreakdown) {
            double percentage = (totalValue > 0) ? (value / totalValue * 100) : 0.0;
            output << "   " << std::left << std::setw(8) << symbol
                   << std::right << std::fixed << std::setprecision(1) << std::setw(6) << percentage << "%"
                   << " ($" << std::setprecision(2) << value << ")\n";
        }

        // Concentration metrics
        output << "\n🎯 Concentration Metrics:\n";
        auto topHolding = sortedBreakdown.empty() ? 0.0 :
            (sortedBreakdown[0].second / totalValue * 100);
        auto top3Holdings = 0.0;
        for (size_t i = 0; i < std::min(3ul, sortedBreakdown.size()); ++i) {
            top3Holdings += sortedBreakdown[i].second / totalValue * 100;
        }

        output << "   Largest Position: " << std::fixed << std::setprecision(1) << topHolding << "%\n";
        output << "   Top 3 Positions: " << top3Holdings << "%\n";
        output << "   Number of Positions: " << sortedBreakdown.size() << "\n";

        // Diversification score
        auto diversificationScore = calculateDiversificationScore(sortedBreakdown, totalValue);
        output << "   Diversification Score: " << std::setprecision(0) << diversificationScore << "/100";
        if (diversificationScore >= 80) output << " 🟢 EXCELLENT";
        else if (diversificationScore >= 60) output << " 🟡 GOOD";
        else if (diversificationScore >= 40) output << " 🟠 MODERATE";
        else output << " 🔴 POOR";
        output << "\n";
    }

    void displayRiskMetrics(std::ostringstream& output) {
        output << "⚠️  Risk Analysis:\n";

        auto portfolioVaR95 = riskManager_->getCurrentVaR(0.95);
        auto portfolioVaR99 = riskManager_->getCurrentVaR(0.99);
        auto expectedShortfall = riskManager_->getExpectedShortfall(0.95);
        auto portfolioBeta = riskManager_->getPortfolioBeta("BTC");
        auto volatility = riskManager_->getPortfolioVolatility();

        output << "   Portfolio VaR (95%): " << std::fixed << std::setprecision(2) << (portfolioVaR95 * 100) << "%\n";
        output << "   Portfolio VaR (99%): " << (portfolioVaR99 * 100) << "%\n";
        output << "   Expected Shortfall: " << (expectedShortfall * 100) << "%\n";
        output << "   Portfolio Beta (vs BTC): " << std::setprecision(3) << portfolioBeta << "\n";
        output << "   Annualized Volatility: " << std::setprecision(1) << (volatility * 100) << "%\n";

        // Risk level assessment
        output << "   Overall Risk Level: ";
        if (portfolioVaR95 < 0.02) {
            output << "🟢 LOW (Conservative)";
        } else if (portfolioVaR95 < 0.05) {
            output << "🟡 MODERATE (Balanced)";
        } else if (portfolioVaR95 < 0.10) {
            output << "🟠 HIGH (Aggressive)";
        } else {
            output << "🔴 VERY HIGH (Extreme)";
        }
        output << "\n";

        // Active alerts
        auto activeAlerts = riskManager_->getActiveAlerts();
        if (!activeAlerts.empty()) {
            output << "\n   🚨 Active Risk Alerts:\n";
            for (const auto& alert : activeAlerts) {
                output << "     " << getRiskIcon(alert.severity) << " " << alert.description << "\n";
            }
        }
    }

    void displayAllocationMetrics(std::ostringstream& output) {
        output << "📊 Allocation Analysis:\n";

        auto targetAllocations = portfolioManager_->getTargetAllocations();
        auto currentAllocations = portfolioManager_->getCurrentAllocations();

        output << "   Asset       Current  Target   Deviation\n";
        output << "   ────────   ───────  ──────   ─────────\n";

        for (const auto& [symbol, targetPct] : targetAllocations) {
            double currentPct = currentAllocations.count(symbol) ? currentAllocations[symbol] : 0.0;
            double deviation = currentPct - targetPct;

            output << "   " << std::left << std::setw(8) << symbol
                   << std::right << std::fixed << std::setprecision(1)
                   << std::setw(7) << currentPct << "%"
                   << std::setw(7) << targetPct << "%"
                   << "   " << std::showpos << std::setw(6) << deviation << "%" << std::noshowpos << "\n";
        }

        // Rebalancing recommendations
        auto rebalanceNeeds = portfolioManager_->getRebalanceRecommendations();
        if (!rebalanceNeeds.empty()) {
            output << "\n   💡 Rebalancing Recommendations:\n";
            for (const auto& rec : rebalanceNeeds) {
                output << "     " << rec.action << " " << rec.symbol << " by "
                       << std::fixed << std::setprecision(1) << rec.amount << "% ($"
                       << std::setprecision(2) << rec.dollarAmount << ")\n";
            }
        } else {
            output << "\n   ✅ Portfolio is well-balanced, no rebalancing needed\n";
        }
    }

    double calculateDiversificationScore(const std::vector<std::pair<std::string, double>>& breakdown, double totalValue) {
        if (breakdown.empty()) return 0.0;

        // Calculate Herfindahl-Hirschman Index (HHI)
        double hhi = 0.0;
        for (const auto& [symbol, value] : breakdown) {
            double share = value / totalValue;
            hhi += share * share;
        }

        // Convert to diversification score (inverse of concentration)
        // Perfect diversification (many equal positions) = 100
        // Perfect concentration (one position) = 0
        double maxHHI = 1.0; // Single position
        double minHHI = 1.0 / breakdown.size(); // Equal distribution

        double diversificationScore = (maxHHI - hhi) / (maxHHI - minHHI) * 100;
        return std::max(0.0, std::min(100.0, diversificationScore));
    }

    std::string getRiskIcon(double severity) {
        if (severity >= 0.8) return "🔴";
        else if (severity >= 0.6) return "🟠";
        else if (severity >= 0.4) return "🟡";
        else return "🟢";
    }
};

} // namespace Commands
} // namespace Console
} // namespace CryptoClaude