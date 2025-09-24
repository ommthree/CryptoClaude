#include "CommandFramework.h"
#include <iostream>
#include <algorithm>
#include <sstream>
#include <regex>
#include <cctype>

namespace CryptoClaude {
namespace Console {

// ParameterDefinition implementation
bool ParameterDefinition::isValid(const std::string& value) const {
    if (value.empty() && required) {
        return false;
    }

    if (value.empty() && !required) {
        return true;
    }

    // Check valid values (enum-like)
    if (hasValidValues()) {
        return std::find(validValues.begin(), validValues.end(), value) != validValues.end();
    }

    // Type-specific validation
    switch (type) {
        case ParameterType::STRING:
            return maxLength ? value.length() <= *maxLength : true;

        case ParameterType::INTEGER: {
            try {
                int intValue = std::stoi(value);
                if (minValue && intValue < *minValue) return false;
                if (maxValue && intValue > *maxValue) return false;
                return true;
            } catch (...) {
                return false;
            }
        }

        case ParameterType::DOUBLE: {
            try {
                double doubleValue = std::stod(value);
                if (minValue && doubleValue < *minValue) return false;
                if (maxValue && doubleValue > *maxValue) return false;
                return true;
            } catch (...) {
                return false;
            }
        }

        case ParameterType::BOOLEAN:
            return value == "true" || value == "false" || value == "1" || value == "0" ||
                   value == "yes" || value == "no" || value == "on" || value == "off";

        case ParameterType::SYMBOL: {
            // Basic cryptocurrency symbol validation
            if (value.length() < 2 || value.length() > 10) return false;
            return std::all_of(value.begin(), value.end(), [](char c) {
                return std::isalnum(c) || c == '-' || c == '_';
            });
        }

        case ParameterType::TIMEFRAME: {
            // Validate timeframes like "1d", "1h", "30m", "365d"
            std::regex timeframeRegex(R"(^\d+[smhdwy]$)");
            return std::regex_match(value, timeframeRegex);
        }

        case ParameterType::PERCENTAGE: {
            try {
                double percent = std::stod(value);
                return percent >= 0.0 && percent <= 100.0;
            } catch (...) {
                return false;
            }
        }

        case ParameterType::FILEPATH:
            // Basic filepath validation - allow most characters
            return !value.empty() && value.find('\0') == std::string::npos;

        default:
            return true;
    }
}

std::string ParameterDefinition::getTypeDescription() const {
    switch (type) {
        case ParameterType::STRING:
            return maxLength ? "string (max " + std::to_string(*maxLength) + " chars)" : "string";
        case ParameterType::INTEGER:
            return "integer" + (minValue || maxValue ?
                " (" + (minValue ? std::to_string(*minValue) : "") +
                (minValue && maxValue ? " to " : "") +
                (maxValue ? std::to_string(*maxValue) : "") + ")" : "");
        case ParameterType::DOUBLE:
            return "number" + (minValue || maxValue ?
                " (" + (minValue ? std::to_string(*minValue) : "") +
                (minValue && maxValue ? " to " : "") +
                (maxValue ? std::to_string(*maxValue) : "") + ")" : "");
        case ParameterType::BOOLEAN:
            return "boolean (true/false, yes/no, on/off, 1/0)";
        case ParameterType::SYMBOL:
            return "cryptocurrency symbol (e.g., BTC, ETH)";
        case ParameterType::TIMEFRAME:
            return "timeframe (e.g., 1d, 24h, 30m)";
        case ParameterType::PERCENTAGE:
            return "percentage (0-100)";
        case ParameterType::FILEPATH:
            return "file path";
        default:
            return "unknown";
    }
}

// CommandContext implementation
std::optional<int> CommandContext::getIntParameter(const std::string& name) const {
    auto param = getParameter(name);
    if (!param) return std::nullopt;

    try {
        return std::stoi(*param);
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<double> CommandContext::getDoubleParameter(const std::string& name) const {
    auto param = getParameter(name);
    if (!param) return std::nullopt;

    try {
        return std::stod(*param);
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<bool> CommandContext::getBoolParameter(const std::string& name) const {
    auto param = getParameter(name);
    if (!param) return std::nullopt;

    std::string value = *param;
    std::transform(value.begin(), value.end(), value.begin(), ::tolower);

    if (value == "true" || value == "1" || value == "yes" || value == "on") {
        return true;
    } else if (value == "false" || value == "0" || value == "no" || value == "off") {
        return false;
    }

    return std::nullopt;
}

std::vector<std::string> CommandContext::getParameterNames() const {
    std::vector<std::string> names;
    for (const auto& pair : parameters_) {
        names.push_back(pair.first);
    }
    return names;
}

// Command base class implementation
std::string Command::getUsage() const {
    std::ostringstream usage;
    usage << getName();

    auto params = getParameters();
    for (const auto& param : params) {
        if (param.required) {
            usage << " --" << param.name << " <" << param.getTypeDescription() << ">";
        } else {
            usage << " [--" << param.name << " <" << param.getTypeDescription() << ">]";
        }
    }

    return usage.str();
}

std::string Command::getDetailedHelp() const {
    std::ostringstream help;
    help << "Command: " << getName() << "\n";
    help << "Description: " << getDescription() << "\n";
    help << "Category: " << getCategory() << "\n";
    help << "Usage: " << getUsage() << "\n";

    auto params = getParameters();
    if (!params.empty()) {
        help << "\nParameters:\n";
        for (const auto& param : params) {
            help << "  --" << param.name;
            if (param.required) {
                help << " (required)";
            } else {
                help << " (optional";
                if (!param.defaultValue.empty()) {
                    help << ", default: " << param.defaultValue;
                }
                help << ")";
            }
            help << "\n";
            help << "    " << param.description << "\n";
            help << "    Type: " << param.getTypeDescription() << "\n";

            if (param.hasValidValues()) {
                help << "    Valid values: ";
                for (size_t i = 0; i < param.validValues.size(); ++i) {
                    if (i > 0) help << ", ";
                    help << param.validValues[i];
                }
                help << "\n";
            }
        }
    }

    auto aliases = getAliases();
    if (!aliases.empty()) {
        help << "\nAliases: ";
        for (size_t i = 0; i < aliases.size(); ++i) {
            if (i > 0) help << ", ";
            help << aliases[i];
        }
        help << "\n";
    }

    return help.str();
}

bool Command::validateParameters(const CommandContext& context, std::string& errorMessage) const {
    auto params = getParameters();
    std::vector<std::string> errors;

    for (const auto& paramDef : params) {
        auto value = context.getParameter(paramDef.name);

        // Check required parameters
        if (paramDef.required && !value) {
            errors.push_back("Required parameter --" + paramDef.name + " is missing");
            continue;
        }

        // Validate parameter value if present
        if (value && !paramDef.isValid(*value)) {
            errors.push_back("Invalid value for --" + paramDef.name + ": " + *value +
                           " (expected: " + paramDef.getTypeDescription() + ")");
        }
    }

    if (!errors.empty()) {
        errorMessage = "Parameter validation failed:\n";
        for (const auto& error : errors) {
            errorMessage += "  " + error + "\n";
        }
        return false;
    }

    return true;
}

std::optional<ParameterDefinition> Command::getParameterDefinition(const std::string& name) const {
    auto params = getParameters();
    auto it = std::find_if(params.begin(), params.end(),
        [&name](const ParameterDefinition& param) { return param.name == name; });

    return it != params.end() ? std::optional<ParameterDefinition>(*it) : std::nullopt;
}

// CommandRegistry implementation
bool CommandRegistry::registerCommand(std::unique_ptr<Command> command) {
    if (!command) {
        return false;
    }

    std::string name = command->getName();
    if (commands_.find(name) != commands_.end()) {
        return false; // Command already exists
    }

    // Register aliases
    for (const std::string& alias : command->getAliases()) {
        if (aliases_.find(alias) != aliases_.end() || commands_.find(alias) != commands_.end()) {
            return false; // Alias conflicts with existing command or alias
        }
        aliases_[alias] = name;
    }

    commands_[name] = std::move(command);
    updateCategories();
    return true;
}

bool CommandRegistry::unregisterCommand(const std::string& name) {
    auto it = commands_.find(name);
    if (it == commands_.end()) {
        return false;
    }

    // Remove aliases
    auto aliases = it->second->getAliases();
    for (const std::string& alias : aliases) {
        aliases_.erase(alias);
    }

    commands_.erase(it);
    updateCategories();
    return true;
}

Command* CommandRegistry::findCommand(const std::string& name) const {
    // Try direct command lookup
    auto it = commands_.find(name);
    if (it != commands_.end()) {
        return it->second.get();
    }

    // Try alias lookup
    auto aliasIt = aliases_.find(name);
    if (aliasIt != aliases_.end()) {
        auto commandIt = commands_.find(aliasIt->second);
        if (commandIt != commands_.end()) {
            return commandIt->second.get();
        }
    }

    return nullptr;
}

std::vector<std::string> CommandRegistry::getCommandNames() const {
    std::vector<std::string> names;
    for (const auto& pair : commands_) {
        names.push_back(pair.first);
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::vector<std::string> CommandRegistry::getCategories() const {
    std::vector<std::string> categoryNames;
    for (const auto& pair : categories_) {
        categoryNames.push_back(pair.first);
    }
    std::sort(categoryNames.begin(), categoryNames.end());
    return categoryNames;
}

std::string CommandRegistry::generateCommandList() const {
    std::ostringstream output;
    output << "Available Commands:\n";

    auto categoryNames = getCategories();
    for (const std::string& category : categoryNames) {
        output << "\n" << category << ":\n";
        auto commandNames = getCommandNames(category);
        for (const std::string& commandName : commandNames) {
            auto command = findCommand(commandName);
            if (command) {
                output << "  " << commandName << " - " << command->getDescription() << "\n";
            }
        }
    }

    return output.str();
}

std::vector<std::string> CommandRegistry::getCommandNames(const std::string& category) const {
    auto it = categories_.find(category);
    return it != categories_.end() ? it->second : std::vector<std::string>();
}

void CommandRegistry::updateCategories() {
    categories_.clear();
    for (const auto& pair : commands_) {
        std::string category = pair.second->getCategory();
        categories_[category].push_back(pair.first);
    }

    // Sort commands within each category
    for (auto& pair : categories_) {
        std::sort(pair.second.begin(), pair.second.end());
    }
}

std::vector<std::string> CommandRegistry::findCommandsByPrefix(const std::string& prefix) const {
    std::vector<std::string> matches;

    for (const auto& pair : commands_) {
        if (pair.first.find(prefix) == 0) {
            matches.push_back(pair.first);
        }
    }

    for (const auto& pair : aliases_) {
        if (pair.first.find(prefix) == 0) {
            matches.push_back(pair.first);
        }
    }

    std::sort(matches.begin(), matches.end());
    return matches;
}

// CommandParser implementation
CommandParser::ParsedCommand CommandParser::parseCommandLine(const std::string& commandLine) {
    ParsedCommand result;

    if (commandLine.empty()) {
        result.addError("Empty command line");
        return result;
    }

    auto tokens = tokenize(commandLine);
    if (tokens.empty()) {
        result.addError("No command found");
        return result;
    }

    result.commandName = tokens[0];

    // Parse parameters
    for (size_t i = 1; i < tokens.size(); ++i) {
        const std::string& token = tokens[i];

        if (token.length() > 2 && token.substr(0, 2) == "--") {
            // Long parameter format: --name value
            std::string paramName = token.substr(2);
            if (paramName.empty()) {
                result.addError("Invalid parameter format: " + token);
                continue;
            }

            // Check for value
            if (i + 1 < tokens.size() && tokens[i + 1].substr(0, 2) != "--") {
                result.parameters[paramName] = unquote(tokens[i + 1]);
                i++; // Skip the value token
            } else {
                result.parameters[paramName] = "true"; // Boolean flag
            }
        } else if (token.length() > 1 && token[0] == '-') {
            // Short parameter format: -n value
            std::string paramName = token.substr(1);
            if (paramName.empty()) {
                result.addError("Invalid parameter format: " + token);
                continue;
            }

            if (i + 1 < tokens.size() && tokens[i + 1][0] != '-') {
                result.parameters[paramName] = unquote(tokens[i + 1]);
                i++; // Skip the value token
            } else {
                result.parameters[paramName] = "true"; // Boolean flag
            }
        } else {
            result.addError("Unexpected token: " + token);
        }
    }

    return result;
}

std::vector<std::string> CommandParser::tokenize(const std::string& input) {
    std::vector<std::string> tokens;
    std::string current;
    bool inQuotes = false;
    char quoteChar = 0;

    for (size_t i = 0; i < input.length(); ++i) {
        char c = input[i];

        if (!inQuotes && (c == '"' || c == '\'')) {
            inQuotes = true;
            quoteChar = c;
            current += c;
        } else if (inQuotes && c == quoteChar) {
            inQuotes = false;
            current += c;
        } else if (!inQuotes && std::isspace(c)) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }

    if (!current.empty()) {
        tokens.push_back(current);
    }

    return tokens;
}

std::string CommandParser::unquote(const std::string& input) {
    if (input.length() < 2) {
        return input;
    }

    if ((input.front() == '"' && input.back() == '"') ||
        (input.front() == '\'' && input.back() == '\'')) {
        return input.substr(1, input.length() - 2);
    }

    return input;
}

// CommandFramework implementation
CommandFramework::CommandFramework(size_t maxHistorySize, bool enableHistory)
    : maxHistorySize_(maxHistorySize)
    , enableHistory_(enableHistory)
    , prompt_("crypto> ")
    , initialized_(false)
{
}

bool CommandFramework::initialize() {
    if (initialized_) {
        return true;
    }

    try {
        // Basic initialization - could register built-in commands here
        initialized_ = true;
        std::cout << "Console Command Framework initialized successfully" << std::endl;
        return true;

    } catch (const std::exception& e) {
        lastError_ = "Framework initialization failed: " + std::string(e.what());
        return false;
    }
}

CommandResult CommandFramework::executeCommand(const std::string& commandLine) {
    if (!initialized_) {
        return CommandResult::createError("Framework not initialized");
    }

    // Add to history
    if (enableHistory_ && !commandLine.empty()) {
        addToHistory(commandLine);
    }

    // Parse command line
    auto parsed = CommandParser::parseCommandLine(commandLine);
    if (!parsed.valid) {
        std::string errorMessage = "Command parsing failed:\n";
        for (const auto& error : parsed.errors) {
            errorMessage += "  " + error + "\n";
        }
        return CommandResult::createError(errorMessage);
    }

    // Find command
    Command* command = registry_.findCommand(parsed.commandName);
    if (!command) {
        auto suggestions = getSuggestions(parsed.commandName);
        std::string errorMessage = "Command '" + parsed.commandName + "' not found";
        if (!suggestions.empty()) {
            errorMessage += "\nDid you mean: ";
            for (size_t i = 0; i < suggestions.size() && i < 3; ++i) {
                if (i > 0) errorMessage += ", ";
                errorMessage += suggestions[i];
            }
        }
        return CommandResult::createError(errorMessage);
    }

    // Create context and set parameters
    CommandContext context(this, commandLine);
    for (const auto& param : parsed.parameters) {
        context.setParameter(param.first, param.second);
    }

    // Validate parameters
    std::string validationError;
    if (!command->validateParameters(context, validationError)) {
        return CommandResult::createError("Parameter validation failed", validationError);
    }

    // Execute command
    try {
        logCommandExecution(parsed.commandName, true);
        auto result = command->execute(context);
        logCommandExecution(parsed.commandName, result.success);
        return result;

    } catch (const std::exception& e) {
        logCommandExecution(parsed.commandName, false);
        return CommandResult::createError("Command execution failed", e.what());
    }
}

void CommandFramework::addToHistory(const std::string& commandLine) {
    if (!enableHistory_) {
        return;
    }

    // Don't add duplicate consecutive commands
    if (!commandHistory_.empty() && commandHistory_.back() == commandLine) {
        return;
    }

    commandHistory_.push_back(commandLine);
    trimHistory();
}

std::vector<std::string> CommandFramework::getHistory(size_t count) const {
    if (count >= commandHistory_.size()) {
        return commandHistory_;
    }

    return std::vector<std::string>(commandHistory_.end() - count, commandHistory_.end());
}

std::string CommandFramework::getHelp() const {
    return registry_.generateCommandList();
}

std::string CommandFramework::getHelp(const std::string& commandName) const {
    Command* command = registry_.findCommand(commandName);
    if (!command) {
        return "Command '" + commandName + "' not found";
    }

    return command->getDetailedHelp();
}

std::vector<std::string> CommandFramework::getCommandCompletions(const std::string& partial) const {
    return registry_.findCommandsByPrefix(partial);
}

std::vector<std::string> CommandFramework::getSuggestions(const std::string& input) const {
    return registry_.getSimilarCommands(input);
}

void CommandFramework::trimHistory() {
    if (commandHistory_.size() > maxHistorySize_) {
        commandHistory_.erase(commandHistory_.begin(),
                             commandHistory_.begin() + (commandHistory_.size() - maxHistorySize_));
    }
}

void CommandFramework::logCommandExecution(const std::string& commandName, bool success) {
    // Basic logging - could be enhanced
    if (!success) {
        std::cerr << "[CommandFramework] Command failed: " << commandName << std::endl;
    }
}

// Factory implementation
std::unique_ptr<CommandFramework> CommandFrameworkFactory::createDefault() {
    auto framework = std::make_unique<CommandFramework>();
    if (framework->initialize()) {
        return framework;
    }
    return nullptr;
}

} // namespace Console
} // namespace CryptoClaude