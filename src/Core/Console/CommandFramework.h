#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <optional>
#include <sstream>
#include <iostream>

namespace CryptoClaude {
namespace Console {

// Forward declarations
class Command;
class CommandContext;
class CommandFramework;

// Parameter types for command arguments
enum class ParameterType {
    STRING,
    INTEGER,
    DOUBLE,
    BOOLEAN,
    SYMBOL,      // Cryptocurrency symbol validation
    TIMEFRAME,   // Time period validation
    PERCENTAGE,  // 0-100 percentage validation
    FILEPATH     // File path validation
};

// Parameter definition for command arguments
struct ParameterDefinition {
    std::string name;
    ParameterType type;
    bool required = false;
    std::string description;
    std::string defaultValue;
    std::vector<std::string> validValues;  // For enum-like parameters

    // Validation constraints
    std::optional<double> minValue;
    std::optional<double> maxValue;
    std::optional<size_t> maxLength;

    bool hasValidValues() const { return !validValues.empty(); }
    bool isValid(const std::string& value) const;
    std::string getTypeDescription() const;
};

// Command execution result
struct CommandResult {
    bool success = false;
    std::string message;
    std::string errorDetails;
    std::map<std::string, std::string> outputData;

    static CommandResult createSuccess(const std::string& message = "") {
        CommandResult result;
        result.success = true;
        result.message = message;
        return result;
    }

    static CommandResult createError(const std::string& message, const std::string& details = "") {
        CommandResult result;
        result.success = false;
        result.message = message;
        result.errorDetails = details;
        return result;
    }

    void addOutput(const std::string& key, const std::string& value) {
        outputData[key] = value;
    }
};

// Command execution context
class CommandContext {
private:
    std::map<std::string, std::string> parameters_;
    CommandFramework* framework_;
    std::string originalCommandLine_;

public:
    CommandContext(CommandFramework* framework, const std::string& commandLine)
        : framework_(framework), originalCommandLine_(commandLine) {}

    // Parameter access
    void setParameter(const std::string& name, const std::string& value) {
        parameters_[name] = value;
    }

    std::optional<std::string> getParameter(const std::string& name) const {
        auto it = parameters_.find(name);
        return it != parameters_.end() ? std::optional<std::string>(it->second) : std::nullopt;
    }

    bool hasParameter(const std::string& name) const {
        return parameters_.find(name) != parameters_.end();
    }

    // Type-safe parameter getters
    std::optional<int> getIntParameter(const std::string& name) const;
    std::optional<double> getDoubleParameter(const std::string& name) const;
    std::optional<bool> getBoolParameter(const std::string& name) const;

    std::string getStringParameter(const std::string& name, const std::string& defaultValue = "") const {
        auto param = getParameter(name);
        return param ? *param : defaultValue;
    }

    // Framework access
    CommandFramework* getFramework() { return framework_; }
    const std::string& getOriginalCommandLine() const { return originalCommandLine_; }

    // Parameter validation
    std::vector<std::string> getParameterNames() const;
    std::map<std::string, std::string> getAllParameters() const { return parameters_; }
};

// Base command interface
class Command {
public:
    virtual ~Command() = default;

    // Command metadata
    virtual std::string getName() const = 0;
    virtual std::string getDescription() const = 0;
    virtual std::string getCategory() const = 0;
    virtual std::vector<ParameterDefinition> getParameters() const = 0;

    // Command execution
    virtual CommandResult execute(CommandContext& context) = 0;

    // Help and validation
    virtual std::string getUsage() const;
    virtual std::string getDetailedHelp() const;
    virtual bool validateParameters(const CommandContext& context, std::string& errorMessage) const;

    // Command aliases (optional)
    virtual std::vector<std::string> getAliases() const { return {}; }

protected:
    // Helper methods for parameter validation
    bool isRequired(const std::string& paramName) const;
    std::optional<ParameterDefinition> getParameterDefinition(const std::string& name) const;
    std::string formatParameterError(const std::string& paramName, const std::string& error) const;
};

// Command registry for organizing commands
class CommandRegistry {
private:
    std::map<std::string, std::unique_ptr<Command>> commands_;
    std::map<std::string, std::string> aliases_; // alias -> command name mapping
    std::map<std::string, std::vector<std::string>> categories_;

public:
    // Command registration
    bool registerCommand(std::unique_ptr<Command> command);
    bool unregisterCommand(const std::string& name);

    // Command lookup
    Command* findCommand(const std::string& name) const;
    std::vector<std::string> getCommandNames() const;
    std::vector<std::string> getCommandNames(const std::string& category) const;
    std::vector<std::string> getCategories() const;

    // Help system
    std::string generateCommandList() const;
    std::string generateCategoryHelp(const std::string& category) const;
    std::string generateCommandHelp(const std::string& commandName) const;

    // Command discovery
    std::vector<std::string> findCommandsByPrefix(const std::string& prefix) const;
    std::vector<std::string> getSimilarCommands(const std::string& input) const;

private:
    void updateCategories();
    int calculateLevenshteinDistance(const std::string& a, const std::string& b) const;
};

// Command line parser
class CommandParser {
public:
    struct ParsedCommand {
        std::string commandName;
        std::map<std::string, std::string> parameters;
        std::vector<std::string> errors;
        bool valid = true;

        void addError(const std::string& error) {
            errors.push_back(error);
            valid = false;
        }
    };

    static ParsedCommand parseCommandLine(const std::string& commandLine);

private:
    static std::vector<std::string> tokenize(const std::string& input);
    static std::string unquote(const std::string& input);
    static bool isQuoted(const std::string& input);
    static std::string normalizeName(const std::string& name);
};

// Main command framework
class CommandFramework {
private:
    CommandRegistry registry_;
    std::vector<std::string> commandHistory_;
    size_t maxHistorySize_;
    bool enableHistory_;
    std::string prompt_;

    // Framework state
    bool initialized_;
    std::string lastError_;

    // Framework services (can be injected)
    std::function<void(const std::string&)> outputCallback_;
    std::function<void(const std::string&)> errorCallback_;

public:
    explicit CommandFramework(size_t maxHistorySize = 1000, bool enableHistory = true);
    virtual ~CommandFramework() = default;

    // Framework initialization
    bool initialize();
    bool isInitialized() const { return initialized_; }

    // Command registration
    bool registerCommand(std::unique_ptr<Command> command) {
        return registry_.registerCommand(std::move(command));
    }

    // Command execution
    CommandResult executeCommand(const std::string& commandLine);
    CommandResult executeCommand(const std::string& commandName,
                                const std::map<std::string, std::string>& parameters);

    // History management
    void addToHistory(const std::string& commandLine);
    std::vector<std::string> getHistory() const { return commandHistory_; }
    std::vector<std::string> getHistory(size_t count) const;
    void clearHistory() { commandHistory_.clear(); }

    // Help system
    std::string getHelp() const;
    std::string getHelp(const std::string& commandName) const;
    std::string getCommandList() const { return registry_.generateCommandList(); }

    // Command discovery and completion
    std::vector<std::string> getCommandCompletions(const std::string& partial) const;
    std::vector<std::string> getSuggestions(const std::string& input) const;

    // Configuration
    void setPrompt(const std::string& prompt) { prompt_ = prompt; }
    std::string getPrompt() const { return prompt_; }
    void setMaxHistorySize(size_t size) { maxHistorySize_ = size; }
    void enableCommandHistory(bool enable) { enableHistory_ = enable; }

    // Output handling
    void setOutputCallback(std::function<void(const std::string&)> callback) {
        outputCallback_ = callback;
    }

    void setErrorCallback(std::function<void(const std::string&)> callback) {
        errorCallback_ = callback;
    }

    void output(const std::string& message) {
        if (outputCallback_) outputCallback_(message);
        else std::cout << message << std::endl;
    }

    void error(const std::string& message) {
        if (errorCallback_) errorCallback_(message);
        else std::cerr << message << std::endl;
    }

    // Framework status
    std::string getLastError() const { return lastError_; }

    // Framework services access (for commands)
    CommandRegistry& getRegistry() { return registry_; }

private:
    void trimHistory();
    CommandResult createValidationError(const std::vector<std::string>& errors);
    void logCommandExecution(const std::string& commandName, bool success);
};

// Base classes for common command types
class SystemCommand : public Command {
public:
    std::string getCategory() const override { return "System"; }
};

class DataCommand : public Command {
public:
    std::string getCategory() const override { return "Data"; }
};

class TradingCommand : public Command {
public:
    std::string getCategory() const override { return "Trading"; }
};

class ConfigurationCommand : public Command {
public:
    std::string getCategory() const override { return "Configuration"; }
};

// Command framework factory
class CommandFrameworkFactory {
public:
    static std::unique_ptr<CommandFramework> createDefault();
    static std::unique_ptr<CommandFramework> createWithBuiltinCommands();
    static std::unique_ptr<CommandFramework> createForTesting();
};

} // namespace Console
} // namespace CryptoClaude