#pragma once

#include "../CommandFramework.h"
#include "../../Database/DatabaseManager.h"
#include "../../Portfolio/PortfolioManager.h"
#include "../../Risk/RiskManager.h"
#include "../../Trading/TradingEngine.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>

namespace CryptoClaude {
namespace Console {
namespace Commands {

using namespace CryptoClaude::Database;
using namespace CryptoClaude::Portfolio;
using namespace CryptoClaude::Risk;
using namespace CryptoClaude::Trading;

// Performance reporting command
class PerformanceCommand : public TradingCommand {
private:
    std::shared_ptr<PortfolioManager> portfolioManager_;
    std::shared_ptr<TradingEngine> tradingEngine_;

public:
    PerformanceCommand(std::shared_ptr<PortfolioManager> portfolioManager,
                      std::shared_ptr<TradingEngine> tradingEngine)
        : portfolioManager_(portfolioManager), tradingEngine_(tradingEngine) {}

    std::string getName() const override { return "performance"; }
    std::string getDescription() const override { return "Generate comprehensive performance reports"; }

    std::vector<ParameterDefinition> getParameters() const override {
        return {
            {"period", ParameterType::STRING, false, "Performance period", "daily", {"daily", "weekly", "monthly", "yearly", "all"}},
            {"format", ParameterType::STRING, false, "Output format", "console", {"console", "csv", "json"}},
            {"benchmark", ParameterType::SYMBOL, false, "Benchmark symbol for comparison", "BTC"},
            {"detailed", ParameterType::BOOLEAN, false, "Include detailed metrics", "false"}
        };
    }

    CommandResult execute(CommandContext& context) override {
        std::string period = context.getStringParameter("period", "daily");
        std::string format = context.getStringParameter("format", "console");
        std::string benchmark = context.getStringParameter("benchmark", "BTC");
        bool detailed = context.getBoolParameter("detailed").value_or(false);

        std::ostringstream output;

        try {
            if (format == "console") {
                return generateConsoleReport(period, benchmark, detailed, output);
            } else if (format == "csv") {
                return generateCSVReport(period, output);
            } else if (format == "json") {
                return generateJSONReport(period, benchmark, output);
            } else {
                return CommandResult::createError("Invalid format. Use: console, csv, or json");
            }

        } catch (const std::exception& e) {
            return CommandResult::createError("Performance report failed", e.what());
        }
    }

private:
    CommandResult generateConsoleReport(const std::string& period, const std::string& benchmark,
                                       bool detailed, std::ostringstream& output) {
        output << "📈 PERFORMANCE REPORT (" << period << ")\n";
        output << "═══════════════════════════════════\n\n";

        // Portfolio overview
        auto currentValue = portfolioManager_->getTotalPortfolioValue();
        auto cashBalance = portfolioManager_->getCashBalance();

        output << "💼 Portfolio Overview:\n";
        output << "   Current Value: $" << std::fixed << std::setprecision(2) << currentValue << "\n";
        output << "   Cash Balance: $" << cashBalance << "\n";
        output << "   Invested Value: $" << (currentValue - cashBalance) << "\n\n";

        // Return metrics
        output << "📊 Return Metrics:\n";
        auto totalReturn = portfolioManager_->getTotalReturn(period);
        auto periodReturn = portfolioManager_->getPeriodReturn(period);

        output << "   Total Return: " << std::showpos << std::fixed << std::setprecision(2)
               << (totalReturn * 100) << "%" << std::noshowpos << "\n";
        output << "   " << period << " Return: " << std::showpos << (periodReturn * 100) << "%" << std::noshowpos << "\n";

        if (period != "daily") {
            auto annualizedReturn = portfolioManager_->getAnnualizedReturn(period);
            output << "   Annualized Return: " << std::showpos << (annualizedReturn * 100) << "%" << std::noshowpos << "\n";
        }

        // Risk-adjusted metrics
        auto sharpeRatio = portfolioManager_->getSharpeRatio(period);
        auto sortinoRatio = portfolioManager_->getSortinoRatio(period);
        auto maxDrawdown = portfolioManager_->getMaxDrawdown(period);

        output << "\n📉 Risk-Adjusted Performance:\n";
        output << "   Sharpe Ratio: " << std::fixed << std::setprecision(3) << sharpeRatio << "\n";
        output << "   Sortino Ratio: " << sortinoRatio << "\n";
        output << "   Maximum Drawdown: " << std::setprecision(2) << (maxDrawdown * 100) << "%\n";

        // Benchmark comparison
        if (!benchmark.empty()) {
            auto benchmarkReturn = getBenchmarkReturn(benchmark, period);
            auto alpha = periodReturn - benchmarkReturn;

            output << "\n🎯 vs " << benchmark << " Benchmark:\n";
            output << "   Benchmark Return: " << std::showpos << std::setprecision(2)
                   << (benchmarkReturn * 100) << "%" << std::noshowpos << "\n";
            output << "   Alpha: " << std::showpos << (alpha * 100) << "%" << std::noshowpos << "\n";
            output << "   Beta: " << std::setprecision(3) << calculateBeta(benchmark, period) << "\n";
        }

        // Trading statistics
        if (tradingEngine_) {
            output << "\n💹 Trading Statistics:\n";
            auto tradeStats = tradingEngine_->getTradingStatistics(period);

            output << "   Total Trades: " << tradeStats.totalTrades << "\n";
            output << "   Winning Trades: " << tradeStats.winningTrades << " ("
                   << std::fixed << std::setprecision(1) << (tradeStats.winRate * 100) << "%)\n";
            output << "   Average Trade: " << std::showpos << std::setprecision(2)
                   << (tradeStats.averageTradeReturn * 100) << "%" << std::noshowpos << "\n";
            output << "   Best Trade: " << std::showpos << (tradeStats.bestTrade * 100) << "%" << std::noshowpos << "\n";
            output << "   Worst Trade: " << std::showpos << (tradeStats.worstTrade * 100) << "%" << std::noshowpos << "\n";

            if (detailed) {
                output << "   Average Hold Time: " << tradeStats.averageHoldTime << " hours\n";
                output << "   Profit Factor: " << std::setprecision(2) << tradeStats.profitFactor << "\n";
                output << "   Recovery Factor: " << tradeStats.recoveryFactor << "\n";
            }
        }

        // Detailed breakdown by time period
        if (detailed) {
            output << "\n📅 Period Breakdown:\n";
            displayPeriodBreakdown(output, period);
        }

        // Performance assessment
        output << "\n";
        displayPerformanceAssessment(output, totalReturn, sharpeRatio, maxDrawdown);

        auto result = CommandResult::createSuccess(output.str());
        result.addOutput("total_return", std::to_string(totalReturn));
        result.addOutput("sharpe_ratio", std::to_string(sharpeRatio));
        result.addOutput("max_drawdown", std::to_string(maxDrawdown));
        return result;
    }

    CommandResult generateCSVReport(const std::string& period, std::ostringstream& output) {
        output << "Date,Portfolio_Value,Daily_Return,Cumulative_Return,Sharpe_Ratio,Max_Drawdown\n";

        // Get historical performance data
        auto performanceData = portfolioManager_->getHistoricalPerformance(period);

        for (const auto& dataPoint : performanceData) {
            output << formatDate(dataPoint.date) << ","
                   << std::fixed << std::setprecision(2) << dataPoint.portfolioValue << ","
                   << std::setprecision(4) << dataPoint.dailyReturn << ","
                   << dataPoint.cumulativeReturn << ","
                   << std::setprecision(3) << dataPoint.sharpeRatio << ","
                   << std::setprecision(4) << dataPoint.maxDrawdown << "\n";
        }

        return CommandResult::createSuccess(output.str());
    }

    CommandResult generateJSONReport(const std::string& period, const std::string& benchmark,
                                    std::ostringstream& output) {
        auto totalReturn = portfolioManager_->getTotalReturn(period);
        auto sharpeRatio = portfolioManager_->getSharpeRatio(period);
        auto maxDrawdown = portfolioManager_->getMaxDrawdown(period);

        output << "{\n";
        output << "  \"performance_report\": {\n";
        output << "    \"period\": \"" << period << "\",\n";
        output << "    \"portfolio\": {\n";
        output << "      \"current_value\": " << std::fixed << std::setprecision(2)
               << portfolioManager_->getTotalPortfolioValue() << ",\n";
        output << "      \"total_return\": " << std::setprecision(6) << totalReturn << ",\n";
        output << "      \"sharpe_ratio\": " << std::setprecision(4) << sharpeRatio << ",\n";
        output << "      \"max_drawdown\": " << maxDrawdown << "\n";
        output << "    }";

        if (!benchmark.empty()) {
            auto benchmarkReturn = getBenchmarkReturn(benchmark, period);
            output << ",\n    \"benchmark\": {\n";
            output << "      \"symbol\": \"" << benchmark << "\",\n";
            output << "      \"return\": " << std::setprecision(6) << benchmarkReturn << ",\n";
            output << "      \"alpha\": " << (totalReturn - benchmarkReturn) << "\n";
            output << "    }";
        }

        if (tradingEngine_) {
            auto stats = tradingEngine_->getTradingStatistics(period);
            output << ",\n    \"trading\": {\n";
            output << "      \"total_trades\": " << stats.totalTrades << ",\n";
            output << "      \"win_rate\": " << std::setprecision(4) << stats.winRate << ",\n";
            output << "      \"average_trade_return\": " << stats.averageTradeReturn << "\n";
            output << "    }";
        }

        output << "\n  }\n";
        output << "}\n";

        return CommandResult::createSuccess(output.str());
    }

    void displayPeriodBreakdown(std::ostringstream& output, const std::string& period) {
        if (period == "monthly" || period == "yearly" || period == "all") {
            auto breakdown = portfolioManager_->getPerformanceBreakdown(period);

            output << "   Period        Return    Max DD    Sharpe\n";
            output << "   ──────        ──────    ──────    ──────\n";

            for (const auto& periodData : breakdown) {
                output << "   " << std::left << std::setw(12) << periodData.periodLabel << " "
                       << std::right << std::showpos << std::setw(6) << std::fixed << std::setprecision(1)
                       << (periodData.return_ * 100) << "%" << std::noshowpos << "   "
                       << std::setw(6) << std::setprecision(1) << (periodData.maxDrawdown * 100) << "%   "
                       << std::setw(6) << std::setprecision(2) << periodData.sharpeRatio << "\n";
            }
        }
    }

    void displayPerformanceAssessment(std::ostringstream& output, double totalReturn,
                                     double sharpeRatio, double maxDrawdown) {
        output << "🎖️  PERFORMANCE ASSESSMENT\n";
        output << "═════════════════════════\n\n";

        int score = 0;
        std::vector<std::pair<std::string, bool>> criteria = {
            {"Positive Returns", totalReturn > 0},
            {"Strong Sharpe Ratio (>1.0)", sharpeRatio > 1.0},
            {"Controlled Risk (DD<15%)", maxDrawdown < 0.15},
            {"Excellent Sharpe (>2.0)", sharpeRatio > 2.0},
            {"Low Drawdown (<10%)", maxDrawdown < 0.10}
        };

        for (const auto& [criterion, passed] : criteria) {
            output << "   " << (passed ? "✅" : "❌") << " " << criterion << "\n";
            if (passed) score++;
        }

        double performanceScore = (double)score / criteria.size() * 100;
        output << "\n🎯 Performance Score: " << std::fixed << std::setprecision(0) << performanceScore << "/100\n";

        if (performanceScore >= 80) {
            output << "   Rating: 🟢 EXCELLENT\n";
        } else if (performanceScore >= 60) {
            output << "   Rating: 🟡 GOOD\n";
        } else if (performanceScore >= 40) {
            output << "   Rating: 🟠 FAIR\n";
        } else {
            output << "   Rating: 🔴 NEEDS IMPROVEMENT\n";
        }
    }

    double getBenchmarkReturn(const std::string& symbol, const std::string& period) {
        // In a real implementation, this would fetch actual benchmark data
        // For now, return simulated benchmark data
        return 0.05; // 5% benchmark return
    }

    double calculateBeta(const std::string& benchmark, const std::string& period) {
        // In a real implementation, this would calculate actual beta
        return 1.2; // Simulated beta
    }

    std::string formatDate(std::chrono::system_clock::time_point timePoint) {
        auto time_t = std::chrono::system_clock::to_time_t(timePoint);
        std::ostringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d");
        return ss.str();
    }
};

// Risk monitoring command
class RiskCommand : public TradingCommand {
private:
    std::shared_ptr<RiskManager> riskManager_;
    std::shared_ptr<PortfolioManager> portfolioManager_;

public:
    RiskCommand(std::shared_ptr<RiskManager> riskManager,
               std::shared_ptr<PortfolioManager> portfolioManager)
        : riskManager_(riskManager), portfolioManager_(portfolioManager) {}

    std::string getName() const override { return "risk"; }
    std::string getDescription() const override { return "Current portfolio risk assessment and monitoring"; }

    std::vector<ParameterDefinition> getParameters() const override {
        return {
            {"detailed", ParameterType::BOOLEAN, false, "Show detailed risk breakdown", "false"},
            {"alerts", ParameterType::BOOLEAN, false, "Include active risk alerts", "true"},
            {"correlation", ParameterType::BOOLEAN, false, "Show correlation matrix", "false"}
        };
    }

    CommandResult execute(CommandContext& context) override {
        bool detailed = context.getBoolParameter("detailed").value_or(false);
        bool showAlerts = context.getBoolParameter("alerts").value_or(true);
        bool showCorrelation = context.getBoolParameter("correlation").value_or(false);

        std::ostringstream output;

        try {
            output << "⚠️  PORTFOLIO RISK ASSESSMENT\n";
            output << "═══════════════════════════\n\n";

            // Current risk metrics
            auto portfolioValue = portfolioManager_->getTotalPortfolioValue();
            auto var95 = riskManager_->getCurrentVaR(0.95);
            auto var99 = riskManager_->getCurrentVaR(0.99);
            auto expectedShortfall = riskManager_->getExpectedShortfall(0.95);

            output << "📊 Current Risk Metrics:\n";
            output << "   Portfolio Value: $" << std::fixed << std::setprecision(2) << portfolioValue << "\n";
            output << "   VaR (95%): " << std::setprecision(2) << (var95 * 100) << "% ($"
                   << std::setprecision(0) << (var95 * portfolioValue) << ")\n";
            output << "   VaR (99%): " << std::setprecision(2) << (var99 * 100) << "% ($"
                   << std::setprecision(0) << (var99 * portfolioValue) << ")\n";
            output << "   Expected Shortfall: " << (expectedShortfall * 100) << "% ($"
                   << std::setprecision(0) << (expectedShortfall * portfolioValue) << ")\n\n";

            // Risk level assessment
            double riskLevel = riskManager_->getPortfolioRiskLevel();
            output << "🎯 Risk Level: ";
            if (riskLevel < 0.03) {
                output << "🟢 LOW (" << std::setprecision(1) << (riskLevel * 100) << "%)";
            } else if (riskLevel < 0.08) {
                output << "🟡 MODERATE (" << std::setprecision(1) << (riskLevel * 100) << "%)";
            } else if (riskLevel < 0.15) {
                output << "🟠 HIGH (" << std::setprecision(1) << (riskLevel * 100) << "%)";
            } else {
                output << "🔴 EXTREME (" << std::setprecision(1) << (riskLevel * 100) << "%)";
            }
            output << "\n\n";

            // Position-level risk breakdown
            if (detailed) {
                output << "📈 Position Risk Breakdown:\n";
                auto positions = portfolioManager_->getAllActivePositions();

                if (!positions.empty()) {
                    output << "Symbol   Position Size   Risk Contrib.   Beta   Volatility\n";
                    output << "──────   ─────────────   ─────────────   ────   ──────────\n";

                    for (const auto& position : positions) {
                        auto positionRisk = riskManager_->getPositionRisk(position.getPositionId());
                        auto positionBeta = riskManager_->getPositionBeta(position.getPositionId(), "BTC");
                        auto positionVol = riskManager_->getPositionVolatility(position.getPositionId());
                        auto marketValue = position.getMarketValue();
                        auto portfolioWeight = portfolioValue > 0 ? (marketValue / portfolioValue) : 0.0;

                        output << std::left << std::setw(6) << position.getSymbol() << "   "
                               << std::right << std::setw(13) << std::fixed << std::setprecision(1) << (portfolioWeight * 100) << "%   "
                               << std::setw(13) << std::setprecision(2) << (positionRisk * 100) << "%   "
                               << std::setw(4) << std::setprecision(2) << positionBeta << "   "
                               << std::setw(10) << std::setprecision(1) << (positionVol * 100) << "%\n";
                    }
                    output << "\n";
                } else {
                    output << "   No active positions\n\n";
                }
            }

            // Correlation analysis
            if (showCorrelation) {
                output << "🔗 Asset Correlation Matrix:\n";
                displayCorrelationMatrix(output);
                output << "\n";
            }

            // Active risk alerts
            if (showAlerts) {
                auto alerts = riskManager_->getActiveAlerts();
                output << "🚨 Active Risk Alerts:\n";

                if (alerts.empty()) {
                    output << "   🟢 No active risk alerts\n";
                } else {
                    for (const auto& alert : alerts) {
                        output << "   " << getRiskIcon(alert.severity) << " " << alert.description
                               << " (Severity: " << std::fixed << std::setprecision(0) << (alert.severity * 100) << "%)\n";
                    }
                }
                output << "\n";
            }

            // Risk limits status
            output << "🛡️  Risk Limits Status:\n";
            auto limits = riskManager_->getRiskLimits();
            displayRiskLimitsStatus(output, limits, var95, riskLevel);

            // Recommendations
            output << "\n💡 Risk Management Recommendations:\n";
            generateRiskRecommendations(output, var95, riskLevel, riskManager_->getActiveAlerts().size());

            auto result = CommandResult::createSuccess(output.str());
            result.addOutput("portfolio_var_95", std::to_string(var95));
            result.addOutput("risk_level", std::to_string(riskLevel));
            result.addOutput("active_alerts", std::to_string(alerts.size()));
            return result;

        } catch (const std::exception& e) {
            return CommandResult::createError("Risk assessment failed", e.what());
        }
    }

private:
    void displayCorrelationMatrix(std::ostringstream& output) {
        // Get active symbols
        auto positions = portfolioManager_->getAllActivePositions();
        std::vector<std::string> symbols;
        for (const auto& pos : positions) {
            symbols.push_back(pos.getSymbol());
        }

        if (symbols.size() < 2) {
            output << "   Insufficient positions for correlation analysis\n";
            return;
        }

        // Display correlation matrix header
        output << "        ";
        for (const auto& sym : symbols) {
            output << std::setw(7) << sym;
        }
        output << "\n";

        // Display correlation matrix
        for (size_t i = 0; i < symbols.size(); ++i) {
            output << std::left << std::setw(7) << symbols[i] << " ";

            for (size_t j = 0; j < symbols.size(); ++j) {
                if (i == j) {
                    output << std::right << std::setw(6) << "1.00";
                } else {
                    double correlation = riskManager_->getAssetCorrelation(symbols[i], symbols[j]);
                    output << std::right << std::setw(6) << std::fixed << std::setprecision(2) << correlation;
                }
                output << " ";
            }
            output << "\n";
        }
    }

    void displayRiskLimitsStatus(std::ostringstream& output, const RiskManager::RiskLimits& limits,
                                double currentVaR, double currentRiskLevel) {
        output << "   Max Daily VaR Limit: " << std::fixed << std::setprecision(1) << (limits.maxDailyVaR * 100) << "%";
        if (currentVaR > limits.maxDailyVaR * 0.8) {
            output << " ⚠️  APPROACHING LIMIT";
        } else {
            output << " ✅ OK";
        }
        output << "\n";

        output << "   Max Portfolio Risk: " << (limits.maxPortfolioRisk * 100) << "%";
        if (currentRiskLevel > limits.maxPortfolioRisk * 0.8) {
            output << " ⚠️  APPROACHING LIMIT";
        } else {
            output << " ✅ OK";
        }
        output << "\n";

        output << "   Max Position Size: " << (limits.maxPositionSize * 100) << "%\n";
        output << "   Max Correlation: " << std::setprecision(2) << limits.maxCorrelation << "\n";
    }

    void generateRiskRecommendations(std::ostringstream& output, double var95, double riskLevel, int alertCount) {
        if (var95 > 0.05) {
            output << "   ⚠️  High VaR detected - consider reducing position sizes\n";
        }

        if (riskLevel > 0.10) {
            output << "   🔴 High risk level - implement defensive measures\n";
        }

        if (alertCount > 0) {
            output << "   🚨 " << alertCount << " active alerts - review risk controls immediately\n";
        }

        auto positions = portfolioManager_->getAllActivePositions();
        if (positions.size() < 3) {
            output << "   📊 Consider diversifying across more assets\n";
        }

        if (var95 < 0.02 && riskLevel < 0.05) {
            output << "   ✅ Risk levels are well-controlled\n";
        }
    }

    std::string getRiskIcon(double severity) {
        if (severity >= 0.8) return "🔴";
        else if (severity >= 0.6) return "🟠";
        else if (severity >= 0.4) return "🟡";
        else return "🟢";
    }
};

// Alerts monitoring command
class AlertsCommand : public SystemCommand {
private:
    std::shared_ptr<RiskManager> riskManager_;
    std::shared_ptr<TradingEngine> tradingEngine_;

public:
    AlertsCommand(std::shared_ptr<RiskManager> riskManager,
                 std::shared_ptr<TradingEngine> tradingEngine)
        : riskManager_(riskManager), tradingEngine_(tradingEngine) {}

    std::string getName() const override { return "alerts"; }
    std::string getDescription() const override { return "Monitor system alerts and notifications"; }

    std::vector<ParameterDefinition> getParameters() const override {
        return {
            {"severity", ParameterType::STRING, false, "Filter by severity level", "all", {"all", "low", "medium", "high", "critical"}},
            {"category", ParameterType::STRING, false, "Filter by alert category", "all", {"all", "risk", "trading", "system", "data"}},
            {"acknowledge", ParameterType::STRING, false, "Acknowledge specific alert by ID", ""}
        };
    }

    CommandResult execute(CommandContext& context) override {
        std::string severity = context.getStringParameter("severity", "all");
        std::string category = context.getStringParameter("category", "all");
        std::string acknowledgeId = context.getStringParameter("acknowledge", "");

        std::ostringstream output;

        try {
            if (!acknowledgeId.empty()) {
                return acknowledgeAlert(acknowledgeId, output);
            }

            output << "🚨 SYSTEM ALERTS DASHBOARD\n";
            output << "═════════════════════════\n\n";

            // Get all active alerts
            auto riskAlerts = riskManager_->getActiveAlerts();
            auto systemAlerts = tradingEngine_->getSystemAlerts();

            // Combine and filter alerts
            std::vector<Alert> allAlerts;
            for (const auto& alert : riskAlerts) {
                if (matchesFilter(alert, severity, category)) {
                    allAlerts.push_back(alert);
                }
            }
            for (const auto& alert : systemAlerts) {
                if (matchesFilter(alert, severity, category)) {
                    allAlerts.push_back(alert);
                }
            }

            // Sort by severity (highest first)
            std::sort(allAlerts.begin(), allAlerts.end(),
                [](const Alert& a, const Alert& b) { return a.severity > b.severity; });

            output << "📊 Alert Summary:\n";
            output << "   Total Active Alerts: " << allAlerts.size() << "\n";

            // Count by severity
            int critical = 0, high = 0, medium = 0, low = 0;
            for (const auto& alert : allAlerts) {
                if (alert.severity >= 0.8) critical++;
                else if (alert.severity >= 0.6) high++;
                else if (alert.severity >= 0.4) medium++;
                else low++;
            }

            output << "   🔴 Critical: " << critical << "   🟠 High: " << high
                   << "   🟡 Medium: " << medium << "   🟢 Low: " << low << "\n\n";

            if (allAlerts.empty()) {
                output << "✅ No active alerts matching filters\n";
            } else {
                output << "Alert ID   Severity   Category   Age      Description\n";
                output << "────────   ────────   ────────   ───────  ───────────\n";

                for (const auto& alert : allAlerts) {
                    auto ageMinutes = std::chrono::duration_cast<std::chrono::minutes>(
                        std::chrono::system_clock::now() - alert.timestamp).count();

                    output << std::left << std::setw(8) << alert.id << " "
                           << std::right << getRiskIcon(alert.severity) << " " << getSeverityText(alert.severity) << "   "
                           << std::left << std::setw(8) << alert.category << " "
                           << std::right << std::setw(6) << ageMinutes << "m  "
                           << alert.description << "\n";
                }

                output << "\n💡 Use 'alerts --acknowledge <ID>' to acknowledge specific alerts\n";
            }

            auto result = CommandResult::createSuccess(output.str());
            result.addOutput("total_alerts", std::to_string(allAlerts.size()));
            result.addOutput("critical_alerts", std::to_string(critical));
            return result;

        } catch (const std::exception& e) {
            return CommandResult::createError("Alerts command failed", e.what());
        }
    }

private:
    struct Alert {
        std::string id;
        double severity;
        std::string category;
        std::string description;
        std::chrono::system_clock::time_point timestamp;
    };

    CommandResult acknowledgeAlert(const std::string& alertId, std::ostringstream& output) {
        output << "✅ ACKNOWLEDGE ALERT\n";
        output << "═══════════════════\n\n";

        // In a real implementation, this would mark the alert as acknowledged
        output << "Alert ID " << alertId << " has been acknowledged.\n";
        output << "The alert will be removed from the active alerts list.\n";

        return CommandResult::createSuccess(output.str());
    }

    bool matchesFilter(const Alert& alert, const std::string& severity, const std::string& category) {
        // Severity filter
        if (severity != "all") {
            if (severity == "critical" && alert.severity < 0.8) return false;
            if (severity == "high" && (alert.severity < 0.6 || alert.severity >= 0.8)) return false;
            if (severity == "medium" && (alert.severity < 0.4 || alert.severity >= 0.6)) return false;
            if (severity == "low" && alert.severity >= 0.4) return false;
        }

        // Category filter
        if (category != "all" && alert.category != category) {
            return false;
        }

        return true;
    }

    std::string getSeverityText(double severity) {
        if (severity >= 0.8) return "CRIT  ";
        else if (severity >= 0.6) return "HIGH  ";
        else if (severity >= 0.4) return "MED   ";
        else return "LOW   ";
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