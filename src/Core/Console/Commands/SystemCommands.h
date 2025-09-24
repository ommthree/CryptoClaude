#pragma once

#include "../CommandFramework.h"
#include "../../Database/DatabaseManager.h"
#include "../../DataPipeline/EnhancedMarketDataPipeline.h"
#include <iostream>
#include <iomanip>

namespace CryptoClaude {
namespace Console {
namespace Commands {

using namespace CryptoClaude::Database;
using namespace CryptoClaude::DataPipeline;

// Help command - shows available commands and usage
class HelpCommand : public SystemCommand {
public:
    std::string getName() const override { return "help"; }
    std::string getDescription() const override { return "Show help information for commands"; }

    std::vector<ParameterDefinition> getParameters() const override {
        return {
            {"command", ParameterType::STRING, false, "Specific command to get help for", ""}
        };
    }

    std::vector<std::string> getAliases() const override { return {"h", "?"}; }

    CommandResult execute(CommandContext& context) override {
        auto commandName = context.getParameter("command");

        if (commandName) {
            // Help for specific command
            std::string helpText = context.getFramework()->getHelp(*commandName);
            return CommandResult::createSuccess(helpText);
        } else {
            // General help
            std::string helpText = context.getFramework()->getHelp();
            return CommandResult::createSuccess(helpText);
        }
    }
};

// Status command - shows system status
class StatusCommand : public SystemCommand {
private:
    DatabaseManager& dbManager_;

public:
    explicit StatusCommand(DatabaseManager& dbManager) : dbManager_(dbManager) {}

    std::string getName() const override { return "status"; }
    std::string getDescription() const override { return "Display system status and health"; }

    std::vector<ParameterDefinition> getParameters() const override {
        return {
            {"detailed", ParameterType::BOOLEAN, false, "Show detailed status information", "false"}
        };
    }

    CommandResult execute(CommandContext& context) override {
        auto result = CommandResult::createSuccess();
        bool detailed = context.getBoolParameter("detailed").value_or(false);

        std::ostringstream status;
        status << "=== CryptoClaude System Status ===\n";

        // Database status
        status << "Database Status: " << (dbManager_.isConnected() ? "Connected" : "Disconnected") << "\n";

        // Migration status
        try {
            MigrationManager& migrationMgr = dbManager_.getMigrationManager();
            status << "Database Version: " << migrationMgr.getCurrentVersion()
                   << "/" << migrationMgr.getTargetVersion() << "\n";
            status << "Migrations Up to Date: " << (migrationMgr.isUpToDate() ? "Yes" : "No") << "\n";
        } catch (const std::exception& e) {
            status << "Migration Status: Error (" << e.what() << ")\n";
        }

        // Data quality status
        try {
            DataQualityManager& qualityMgr = dbManager_.getDataQualityManager();
            double qualityScore = qualityMgr.getOverallQualityScore();
            status << "Overall Data Quality: " << std::fixed << std::setprecision(1)
                   << (qualityScore * 100) << "%\n";
        } catch (const std::exception& e) {
            status << "Data Quality Status: Error (" << e.what() << ")\n";
        }

        if (detailed) {
            // Additional detailed information
            status << "\n=== Detailed System Information ===\n";
            status << "Command Framework: Initialized\n";
            status << "Available Tables: ";

            // List some key tables
            std::vector<std::string> keyTables = {
                "market_data", "sentiment_data", "portfolios", "schema_migrations"
            };

            bool first = true;
            for (const std::string& tableName : keyTables) {
                if (!first) status << ", ";
                status << tableName << " (" << (dbManager_.tableExists(tableName) ? "✓" : "✗") << ")";
                first = false;
            }
            status << "\n";
        }

        result.message = status.str();
        result.addOutput("database_connected", dbManager_.isConnected() ? "true" : "false");

        return result;
    }
};

// History command - shows command history
class HistoryCommand : public SystemCommand {
public:
    std::string getName() const override { return "history"; }
    std::string getDescription() const override { return "Show command execution history"; }

    std::vector<ParameterDefinition> getParameters() const override {
        return {
            {"count", ParameterType::INTEGER, false, "Number of recent commands to show", "10"}
        };
    }

    CommandResult execute(CommandContext& context) override {
        int count = context.getIntParameter("count").value_or(10);

        auto history = context.getFramework()->getHistory(count);

        std::ostringstream output;
        output << "Recent Commands:\n";

        for (size_t i = 0; i < history.size(); ++i) {
            output << "  " << (i + 1) << ". " << history[i] << "\n";
        }

        if (history.empty()) {
            output << "  (no commands in history)\n";
        }

        return CommandResult::createSuccess(output.str());
    }
};

// Clear command - clears the console or specific data
class ClearCommand : public SystemCommand {
public:
    std::string getName() const override { return "clear"; }
    std::string getDescription() const override { return "Clear console or command history"; }

    std::vector<ParameterDefinition> getParameters() const override {
        return {
            {"target", ParameterType::STRING, false, "What to clear: console, history", "console"}
        };
    }

    std::vector<std::string> getAliases() const override { return {"cls"}; }

    CommandResult execute(CommandContext& context) override {
        std::string target = context.getStringParameter("target", "console");

        if (target == "console" || target == "screen") {
            // Clear console screen
            #ifdef _WIN32
                system("cls");
            #else
                system("clear");
            #endif
            return CommandResult::createSuccess("Console cleared");
        } else if (target == "history") {
            context.getFramework()->clearHistory();
            return CommandResult::createSuccess("Command history cleared");
        } else {
            return CommandResult::createError("Invalid clear target. Use 'console' or 'history'");
        }
    }
};

// Exit command - graceful system shutdown
class ExitCommand : public SystemCommand {
public:
    std::string getName() const override { return "exit"; }
    std::string getDescription() const override { return "Exit the CryptoClaude system"; }

    std::vector<ParameterDefinition> getParameters() const override {
        return {
            {"force", ParameterType::BOOLEAN, false, "Force exit without confirmation", "false"}
        };
    }

    std::vector<std::string> getAliases() const override { return {"quit", "q"}; }

    CommandResult execute(CommandContext& context) override {
        bool force = context.getBoolParameter("force").value_or(false);

        if (!force) {
            // In a real implementation, you might check for running operations
            std::cout << "Are you sure you want to exit? (y/N): ";
            std::string response;
            std::getline(std::cin, response);

            if (response != "y" && response != "Y" && response != "yes" && response != "YES") {
                return CommandResult::createSuccess("Exit cancelled");
            }
        }

        // Perform graceful shutdown
        std::cout << "Shutting down CryptoClaude system..." << std::endl;

        // In a real implementation, you would:
        // - Stop any running data pipelines
        // - Close database connections
        // - Save any pending data

        auto result = CommandResult::createSuccess("System shutdown initiated");
        result.addOutput("shutdown", "true");

        return result;
    }
};

// Version command - shows system version information
class VersionCommand : public SystemCommand {
public:
    std::string getName() const override { return "version"; }
    std::string getDescription() const override { return "Display system version information"; }

    std::vector<ParameterDefinition> getParameters() const override { return {}; }

    std::vector<std::string> getAliases() const override { return {"ver", "-v", "--version"}; }

    CommandResult execute(CommandContext& context) override {
        std::ostringstream version;
        version << "CryptoClaude Trading Platform\n";
        version << "Version: 1.0.0-dev\n";
        version << "Build: Week 1, Day 2 Development Build\n";
        version << "Database Schema Version: " << "1.0" << "\n";
        version << "Console Framework: 1.0\n";
        version << "\nFeatures:\n";
        version << "  - Enhanced Database Foundation with Migration System\n";
        version << "  - Automated Data Quality Management\n";
        version << "  - CryptoCompare API Integration\n";
        version << "  - Command-Line Interface\n";
        version << "\nDevelopment Status: Active Development - Week 1, Day 2\n";

        auto result = CommandResult::createSuccess(version.str());
        result.addOutput("version", "1.0.0-dev");
        result.addOutput("build", "Week1-Day2");

        return result;
    }
};

} // namespace Commands
} // namespace Console
} // namespace CryptoClaude