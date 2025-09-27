#pragma once

#include "../CommandFramework.h"
#include "../../Database/DatabaseManager.h"
#include "../../Trading/TradingEngine.h"
#include "../../Portfolio/PortfolioManager.h"
#include "../../Strategy/EnhancedSignalProcessor.h"
#include "../../AI/AIDecisionEngine.h"
#include <iostream>
#include <iomanip>
#include <sstream>

namespace CryptoClaude {
namespace Console {
namespace Commands {

using namespace CryptoClaude::Database;
using namespace CryptoClaude::Trading;
using namespace CryptoClaude::Portfolio;
using namespace CryptoClaude::Strategy;
using namespace CryptoClaude::AI;

// Trading control command - enable/disable automated trading
class TradingControlCommand : public TradingCommand {
private:
    DatabaseManager& dbManager_;
    std::shared_ptr<TradingEngine> tradingEngine_;

public:
    TradingControlCommand(DatabaseManager& dbManager, std::shared_ptr<TradingEngine> tradingEngine)
        : dbManager_(dbManager), tradingEngine_(tradingEngine) {}

    std::string getName() const override { return "trading"; }
    std::string getDescription() const override { return "Control automated trading execution"; }

    std::vector<ParameterDefinition> getParameters() const override {
        return {
            {"action", ParameterType::STRING, true, "Trading action: on or off", "", {"on", "off"}},
            {"mode", ParameterType::STRING, false, "Trading mode for 'on' action", "personal", {"personal", "professional"}},
            {"confirm", ParameterType::BOOLEAN, false, "Skip confirmation prompt", "false"}
        };
    }

    std::vector<std::string> getAliases() const override { return {"trade"}; }

    CommandResult execute(CommandContext& context) override {
        std::string action = context.getStringParameter("action");
        std::string mode = context.getStringParameter("mode", "personal");
        bool skipConfirm = context.getBoolParameter("confirm").value_or(false);

        std::ostringstream output;

        try {
            if (action == "on") {
                // Validate system readiness
                if (!validateSystemReadiness(output)) {
                    return CommandResult::createError("System not ready for trading", output.str());
                }

                // Get confirmation unless skipped
                if (!skipConfirm && !getConfirmation(action, mode)) {
                    return CommandResult::createSuccess("Trading activation cancelled by user");
                }

                // Configure trading mode
                TradingEngine::TradingMode tradingMode = (mode == "professional") ?
                    TradingEngine::TradingMode::PROFESSIONAL : TradingEngine::TradingMode::PERSONAL;

                // Activate trading
                if (tradingEngine_->enableTrading(tradingMode)) {
                    output << "✅ TRADING ACTIVATED\n\n";
                    output << "📊 Trading Configuration:\n";
                    output << "   Mode: " << (mode == "professional" ? "Professional" : "Personal") << "\n";

                    if (mode == "personal") {
                        output << "   Max Position Size: 2% of portfolio\n";
                        output << "   Max Portfolio Risk: 5%\n";
                        output << "   Max Concurrent Positions: 3\n";
                        output << "   API Usage: Conservative optimization\n";
                    } else {
                        output << "   Max Position Size: 10% of portfolio\n";
                        output << "   Max Portfolio Risk: 15%\n";
                        output << "   Max Concurrent Positions: 10\n";
                        output << "   API Usage: Full feature set\n";
                    }

                    // Display current portfolio status
                    appendPortfolioStatus(output);

                    // Display active risk limits
                    appendRiskLimits(output, mode);

                } else {
                    return CommandResult::createError("Failed to activate trading", tradingEngine_->getLastError());
                }

            } else if (action == "off") {
                // Get confirmation unless skipped
                if (!skipConfirm && !getConfirmation(action, "")) {
                    return CommandResult::createSuccess("Trading deactivation cancelled by user");
                }

                // Deactivate trading
                if (tradingEngine_->disableTrading()) {
                    output << "🛑 TRADING DEACTIVATED\n\n";
                    output << "📋 Deactivation Summary:\n";

                    auto pendingOrders = tradingEngine_->getPendingOrdersCount();
                    output << "   Pending Orders: " << pendingOrders << "\n";

                    if (pendingOrders > 0) {
                        output << "   ⚠️  Pending orders will remain active\n";
                        output << "   ⚠️  Use 'liquidate' command to cancel all orders\n";
                    }

                    output << "   Active Positions: " << tradingEngine_->getActivePositionCount() << "\n";
                    output << "   Portfolio Value: $" << std::fixed << std::setprecision(2)
                           << tradingEngine_->getCurrentPortfolioValue() << "\n";
                } else {
                    return CommandResult::createError("Failed to deactivate trading", tradingEngine_->getLastError());
                }
            }

            auto result = CommandResult::createSuccess(output.str());
            result.addOutput("trading_active", action == "on" ? "true" : "false");
            result.addOutput("trading_mode", mode);
            return result;

        } catch (const std::exception& e) {
            return CommandResult::createError("Trading control failed", e.what());
        }
    }

private:
    bool validateSystemReadiness(std::ostringstream& output) {
        bool ready = true;
        output << "🔍 System Readiness Check:\n";

        // Database connectivity
        if (dbManager_.isConnected()) {
            output << "   ✅ Database: Connected\n";
        } else {
            output << "   ❌ Database: Disconnected\n";
            ready = false;
        }

        // Data pipeline status
        if (tradingEngine_->isDataPipelineHealthy()) {
            output << "   ✅ Data Pipeline: Healthy\n";
        } else {
            output << "   ❌ Data Pipeline: Issues detected\n";
            ready = false;
        }

        // Risk management
        if (tradingEngine_->isRiskManagementActive()) {
            output << "   ✅ Risk Management: Active\n";
        } else {
            output << "   ❌ Risk Management: Inactive\n";
            ready = false;
        }

        // AI system
        if (tradingEngine_->isAISystemReady()) {
            output << "   ✅ AI System: Ready\n";
        } else {
            output << "   ⚠️  AI System: Limited functionality\n";
            // Note: AI issues don't prevent trading
        }

        output << "\n";
        return ready;
    }

    bool getConfirmation(const std::string& action, const std::string& mode) {
        if (action == "on") {
            std::cout << "🚨 TRADING ACTIVATION CONFIRMATION\n";
            std::cout << "You are about to enable automated trading in " << mode << " mode.\n";
            std::cout << "This will allow the system to execute real trades with actual funds.\n";
            std::cout << "Are you sure you want to proceed? (y/N): ";
        } else {
            std::cout << "🛑 TRADING DEACTIVATION CONFIRMATION\n";
            std::cout << "This will disable automated trading execution.\n";
            std::cout << "Existing positions will remain open.\n";
            std::cout << "Are you sure you want to proceed? (y/N): ";
        }

        std::string response;
        std::getline(std::cin, response);
        return (response == "y" || response == "Y" || response == "yes" || response == "YES");
    }

    void appendPortfolioStatus(std::ostringstream& output) {
        output << "\n💼 Current Portfolio Status:\n";
        output << "   Total Value: $" << std::fixed << std::setprecision(2)
               << tradingEngine_->getCurrentPortfolioValue() << "\n";
        output << "   Cash Balance: $" << tradingEngine_->getCashBalance() << "\n";
        output << "   Active Positions: " << tradingEngine_->getActivePositionCount() << "\n";
        output << "   Daily P&L: " << std::showpos << tradingEngine_->getDailyPnL()
               << "%" << std::noshowpos << "\n";
    }

    void appendRiskLimits(std::ostringstream& output, const std::string& mode) {
        output << "\n⚠️  Active Risk Limits:\n";
        if (mode == "personal") {
            output << "   Max Daily Loss: -2%\n";
            output << "   Max Position Size: 2% portfolio value\n";
            output << "   VaR Limit (95%): 1%\n";
            output << "   Drawdown Stop: -10%\n";
        } else {
            output << "   Max Daily Loss: -5%\n";
            output << "   Max Position Size: 10% portfolio value\n";
            output << "   VaR Limit (95%): 3%\n";
            output << "   Drawdown Stop: -20%\n";
        }
        output << "   Correlation Limit: 0.8 max between positions\n";
        output << "   Leverage Limit: 3:1 maximum\n";
    }
};

// Liquidation command - force close positions
class LiquidateCommand : public TradingCommand {
private:
    std::shared_ptr<TradingEngine> tradingEngine_;
    std::shared_ptr<PortfolioManager> portfolioManager_;

public:
    LiquidateCommand(std::shared_ptr<TradingEngine> tradingEngine,
                    std::shared_ptr<PortfolioManager> portfolioManager)
        : tradingEngine_(tradingEngine), portfolioManager_(portfolioManager) {}

    std::string getName() const override { return "liquidate"; }
    std::string getDescription() const override { return "Force liquidation of positions"; }

    std::vector<ParameterDefinition> getParameters() const override {
        return {
            {"symbol", ParameterType::SYMBOL, false, "Specific coin symbol to liquidate", ""},
            {"pair", ParameterType::STRING, false, "Specific pair ID to liquidate", ""},
            {"all", ParameterType::BOOLEAN, false, "Liquidate all positions", "false"},
            {"confirm", ParameterType::BOOLEAN, false, "Skip confirmation prompt", "false"}
        };
    }

    CommandResult execute(CommandContext& context) override {
        std::string symbol = context.getStringParameter("symbol", "");
        std::string pairId = context.getStringParameter("pair", "");
        bool liquidateAll = context.getBoolParameter("all").value_or(false);
        bool skipConfirm = context.getBoolParameter("confirm").value_or(false);

        // Validate parameters
        if (!liquidateAll && symbol.empty() && pairId.empty()) {
            return CommandResult::createError("Must specify --symbol, --pair, or --all");
        }

        std::ostringstream output;

        try {
            std::vector<Position> positionsToLiquidate;

            // Determine positions to liquidate
            if (liquidateAll) {
                positionsToLiquidate = portfolioManager_->getAllActivePositions();
                if (positionsToLiquidate.empty()) {
                    return CommandResult::createSuccess("No active positions to liquidate");
                }
            } else if (!symbol.empty()) {
                auto symbolPositions = portfolioManager_->getPositionsBySymbol(symbol);
                if (symbolPositions.empty()) {
                    return CommandResult::createError("No active positions found for symbol: " + symbol);
                }
                positionsToLiquidate = symbolPositions;
            } else if (!pairId.empty()) {
                auto pairPosition = portfolioManager_->getPositionByPairId(pairId);
                if (!pairPosition.has_value()) {
                    return CommandResult::createError("No active position found for pair ID: " + pairId);
                }
                positionsToLiquidate.push_back(*pairPosition);
            }

            // Display liquidation plan
            output << "🚨 LIQUIDATION PLAN\n\n";
            output << "Positions to liquidate: " << positionsToLiquidate.size() << "\n\n";

            double totalPnL = 0.0;
            for (const auto& position : positionsToLiquidate) {
                output << "📍 " << position.getSymbol() << ":\n";
                output << "   Quantity: " << std::fixed << std::setprecision(8) << position.getQuantity() << "\n";
                output << "   Entry Price: $" << std::setprecision(2) << position.getEntryPrice() << "\n";
                output << "   Current Price: $" << position.getCurrentPrice() << "\n";
                output << "   P&L: $" << std::showpos << position.getPnL() << std::noshowpos << "\n";
                output << "   P&L %: " << std::showpos << position.getPnLPercentage() << "%" << std::noshowpos << "\n\n";
                totalPnL += position.getPnL();
            }

            output << "💰 Total P&L Impact: $" << std::showpos << totalPnL << std::noshowpos << "\n\n";

            // Get confirmation unless skipped
            if (!skipConfirm && !getConfirmation(positionsToLiquidate.size(), totalPnL)) {
                return CommandResult::createSuccess("Liquidation cancelled by user");
            }

            // Execute liquidation
            output << "⚡ EXECUTING LIQUIDATION...\n\n";

            int successCount = 0;
            double realizedPnL = 0.0;

            for (const auto& position : positionsToLiquidate) {
                auto liquidationResult = tradingEngine_->liquidatePosition(position.getPositionId());

                if (liquidationResult.success) {
                    output << "✅ " << position.getSymbol() << ": Liquidated successfully\n";
                    output << "   Realized P&L: $" << std::showpos << liquidationResult.realizedPnL << std::noshowpos << "\n";
                    successCount++;
                    realizedPnL += liquidationResult.realizedPnL;
                } else {
                    output << "❌ " << position.getSymbol() << ": Liquidation failed - " << liquidationResult.error << "\n";
                }
            }

            output << "\n📊 LIQUIDATION SUMMARY\n";
            output << "   Positions Liquidated: " << successCount << "/" << positionsToLiquidate.size() << "\n";
            output << "   Total Realized P&L: $" << std::showpos << realizedPnL << std::noshowpos << "\n";

            if (successCount == positionsToLiquidate.size()) {
                output << "   Status: ✅ ALL LIQUIDATIONS SUCCESSFUL\n";
            } else {
                output << "   Status: ⚠️  PARTIAL LIQUIDATION - CHECK FAILED POSITIONS\n";
            }

            auto result = CommandResult::createSuccess(output.str());
            result.addOutput("positions_liquidated", std::to_string(successCount));
            result.addOutput("total_pnl", std::to_string(realizedPnL));
            result.addOutput("success_rate", std::to_string((double)successCount / positionsToLiquidate.size()));
            return result;

        } catch (const std::exception& e) {
            return CommandResult::createError("Liquidation failed", e.what());
        }
    }

private:
    bool getConfirmation(size_t positionCount, double totalPnL) {
        std::cout << "🚨 LIQUIDATION CONFIRMATION\n";
        std::cout << "You are about to liquidate " << positionCount << " position(s)\n";
        std::cout << "Estimated P&L impact: $" << std::showpos << totalPnL << std::noshowpos << "\n";
        std::cout << "This action cannot be undone.\n";
        std::cout << "Are you sure you want to proceed? (y/N): ";

        std::string response;
        std::getline(std::cin, response);
        return (response == "y" || response == "Y" || response == "yes" || response == "YES");
    }
};

// Personal limits management command
class PersonalLimitsCommand : public TradingCommand {
private:
    std::shared_ptr<TradingEngine> tradingEngine_;

public:
    explicit PersonalLimitsCommand(std::shared_ptr<TradingEngine> tradingEngine)
        : tradingEngine_(tradingEngine) {}

    std::string getName() const override { return "personal-limits"; }
    std::string getDescription() const override { return "Manage personal trading limits and constraints"; }

    std::vector<ParameterDefinition> getParameters() const override {
        return {
            {"action", ParameterType::STRING, false, "Action to perform", "show", {"show", "update"}},
            {"interactive", ParameterType::BOOLEAN, false, "Interactive update mode", "true"}
        };
    }

    CommandResult execute(CommandContext& context) override {
        std::string action = context.getStringParameter("action", "show");
        bool interactive = context.getBoolParameter("interactive").value_or(true);

        std::ostringstream output;

        try {
            if (action == "show") {
                displayPersonalLimits(output);
            } else if (action == "update") {
                if (interactive) {
                    return updatePersonalLimitsInteractive();
                } else {
                    output << "Non-interactive update not implemented. Use --interactive true\n";
                }
            }

            return CommandResult::createSuccess(output.str());

        } catch (const std::exception& e) {
            return CommandResult::createError("Personal limits command failed", e.what());
        }
    }

private:
    void displayPersonalLimits(std::ostringstream& output) {
        output << "👤 PERSONAL TRADING LIMITS\n\n";

        auto limits = tradingEngine_->getPersonalLimits();

        output << "💰 Risk Limits:\n";
        output << "   Max Daily Loss: " << limits.maxDailyLoss << "%\n";
        output << "   Max Position Size: " << limits.maxPositionSize << "% of portfolio\n";
        output << "   Max Portfolio Risk: " << limits.maxPortfolioRisk << "%\n";
        output << "   Max Concurrent Positions: " << limits.maxConcurrentPositions << "\n";
        output << "   Stop Loss Threshold: " << limits.stopLossThreshold << "%\n\n";

        output << "📡 API Constraints:\n";
        output << "   Daily API Call Limit: " << limits.dailyAPICallLimit << "\n";
        output << "   API Calls Used Today: " << tradingEngine_->getTodayAPIUsage() << "\n";
        output << "   Conservative Mode: " << (limits.conservativeAPIMode ? "Enabled" : "Disabled") << "\n";
        output << "   Cache Priority: " << (limits.prioritizeCaching ? "High" : "Standard") << "\n\n";

        output << "🤖 ML Configuration:\n";
        output << "   Confidence Threshold: " << limits.mlConfidenceThreshold << "\n";
        output << "   Feature Count Limit: " << limits.maxMLFeatures << "\n";
        output << "   Prediction Horizon: " << limits.predictionHorizonHours << " hours\n";
        output << "   Model Retraining Frequency: " << limits.retrainingFrequencyDays << " days\n\n";

        output << "⚡ Performance Thresholds:\n";
        output << "   Min Sharpe Ratio: " << limits.minSharpeRatio << "\n";
        output << "   Max Drawdown Alert: " << limits.maxDrawdownAlert << "%\n";
        output << "   Min Win Rate: " << limits.minWinRate << "%\n";
        output << "   Performance Review Period: " << limits.performanceReviewDays << " days\n";
    }

    CommandResult updatePersonalLimitsInteractive() {
        std::cout << "\n🔧 INTERACTIVE PERSONAL LIMITS UPDATE\n";
        std::cout << "Current limits will be shown. Press Enter to keep current value.\n\n";

        auto currentLimits = tradingEngine_->getPersonalLimits();
        TradingEngine::PersonalLimits newLimits = currentLimits;

        // Risk limits
        std::cout << "💰 RISK LIMITS\n";
        newLimits.maxDailyLoss = promptForDouble("Max Daily Loss (%)", currentLimits.maxDailyLoss, 0.5, 10.0);
        newLimits.maxPositionSize = promptForDouble("Max Position Size (% of portfolio)", currentLimits.maxPositionSize, 1.0, 25.0);
        newLimits.maxPortfolioRisk = promptForDouble("Max Portfolio Risk (%)", currentLimits.maxPortfolioRisk, 1.0, 20.0);
        newLimits.maxConcurrentPositions = promptForInt("Max Concurrent Positions", currentLimits.maxConcurrentPositions, 1, 20);

        // API constraints
        std::cout << "\n📡 API CONSTRAINTS\n";
        newLimits.dailyAPICallLimit = promptForInt("Daily API Call Limit", currentLimits.dailyAPICallLimit, 100, 10000);
        newLimits.conservativeAPIMode = promptForBool("Conservative API Mode", currentLimits.conservativeAPIMode);

        // ML configuration
        std::cout << "\n🤖 ML CONFIGURATION\n";
        newLimits.mlConfidenceThreshold = promptForDouble("ML Confidence Threshold", currentLimits.mlConfidenceThreshold, 0.5, 0.95);
        newLimits.predictionHorizonHours = promptForInt("Prediction Horizon (hours)", currentLimits.predictionHorizonHours, 1, 168);

        // Apply new limits
        if (tradingEngine_->updatePersonalLimits(newLimits)) {
            std::ostringstream output;
            output << "✅ Personal limits updated successfully!\n\n";
            displayPersonalLimits(output);
            return CommandResult::createSuccess(output.str());
        } else {
            return CommandResult::createError("Failed to update personal limits", tradingEngine_->getLastError());
        }
    }

    double promptForDouble(const std::string& prompt, double currentValue, double minVal, double maxVal) {
        std::cout << prompt << " [" << currentValue << "] (range: " << minVal << "-" << maxVal << "): ";
        std::string input;
        std::getline(std::cin, input);

        if (input.empty()) return currentValue;

        try {
            double value = std::stod(input);
            return std::max(minVal, std::min(maxVal, value));
        } catch (...) {
            std::cout << "Invalid input, keeping current value.\n";
            return currentValue;
        }
    }

    int promptForInt(const std::string& prompt, int currentValue, int minVal, int maxVal) {
        std::cout << prompt << " [" << currentValue << "] (range: " << minVal << "-" << maxVal << "): ";
        std::string input;
        std::getline(std::cin, input);

        if (input.empty()) return currentValue;

        try {
            int value = std::stoi(input);
            return std::max(minVal, std::min(maxVal, value));
        } catch (...) {
            std::cout << "Invalid input, keeping current value.\n";
            return currentValue;
        }
    }

    bool promptForBool(const std::string& prompt, bool currentValue) {
        std::cout << prompt << " [" << (currentValue ? "yes" : "no") << "] (y/n): ";
        std::string input;
        std::getline(std::cin, input);

        if (input.empty()) return currentValue;

        return (input == "y" || input == "Y" || input == "yes" || input == "YES");
    }
};

} // namespace Commands
} // namespace Console
} // namespace CryptoClaude