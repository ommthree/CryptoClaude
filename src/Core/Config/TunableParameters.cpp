#include "TunableParameters.h"
#include "../Json/JsonHelper.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>

// Use the CryptoClaude JSON wrapper
using CryptoClaude::Json::json;
using CryptoClaude::Json::JsonHelper;

namespace CryptoClaude {
namespace Config {

// Global instance
std::unique_ptr<TunableParametersManager> GlobalParameters::instance_ = nullptr;

TunableParametersManager::TunableParametersManager(const std::string& configPath)
    : currentProfile_(RiskProfile::MODERATE), configFilePath_(configPath),
      autoSave_(true), lastSave_(std::chrono::system_clock::now()) {

    initializeDefaultParameters();
    loadFromFile(configPath);
}

TunableParametersManager::~TunableParametersManager() {
    if (autoSave_ && hasUnsavedChanges()) {
        saveToFile();
    }
}

void TunableParametersManager::registerParameter(const std::string& name, ParameterType type,
                                                const std::string& defaultValue,
                                                const std::string& description,
                                                const std::string& category) {
    auto param = std::make_unique<Parameter>(name, type, defaultValue, description, category);
    parameters_[name] = std::move(param);
}

void TunableParametersManager::registerDoubleParameter(const std::string& name, double defaultValue,
                                                      double minVal, double maxVal,
                                                      const std::string& description,
                                                      const std::string& category) {
    auto param = std::make_unique<Parameter>(name, ParameterType::DOUBLE,
                                           std::to_string(defaultValue), description, category);
    param->minValue = minVal;
    param->maxValue = maxVal;
    param->validator = [minVal, maxVal](const std::string& value) {
        try {
            double val = std::stod(value);
            return val >= minVal && val <= maxVal;
        } catch (...) {
            return false;
        }
    };
    parameters_[name] = std::move(param);
}

void TunableParametersManager::registerIntParameter(const std::string& name, int defaultValue,
                                                   int minVal, int maxVal,
                                                   const std::string& description,
                                                   const std::string& category) {
    auto param = std::make_unique<Parameter>(name, ParameterType::INTEGER,
                                           std::to_string(defaultValue), description, category);
    param->minValue = static_cast<double>(minVal);
    param->maxValue = static_cast<double>(maxVal);
    param->validator = [minVal, maxVal](const std::string& value) {
        try {
            int val = std::stoi(value);
            return val >= minVal && val <= maxVal;
        } catch (...) {
            return false;
        }
    };
    parameters_[name] = std::move(param);
}

void TunableParametersManager::registerBoolParameter(const std::string& name, bool defaultValue,
                                                    const std::string& description,
                                                    const std::string& category) {
    auto param = std::make_unique<Parameter>(name, ParameterType::BOOLEAN,
                                           defaultValue ? "true" : "false", description, category);
    param->validator = [](const std::string& value) {
        return value == "true" || value == "false" || value == "1" || value == "0";
    };
    parameters_[name] = std::move(param);
}

double TunableParametersManager::getDouble(const std::string& name) const {
    auto it = parameters_.find(name);
    if (it != parameters_.end() && it->second->type == ParameterType::DOUBLE) {
        try {
            return std::stod(it->second->currentValue);
        } catch (...) {
            return std::stod(it->second->defaultValue);
        }
    }
    throw std::runtime_error("Parameter not found or wrong type: " + name);
}

int TunableParametersManager::getInt(const std::string& name) const {
    auto it = parameters_.find(name);
    if (it != parameters_.end() && it->second->type == ParameterType::INTEGER) {
        try {
            return std::stoi(it->second->currentValue);
        } catch (...) {
            return std::stoi(it->second->defaultValue);
        }
    }
    throw std::runtime_error("Parameter not found or wrong type: " + name);
}

bool TunableParametersManager::getBool(const std::string& name) const {
    auto it = parameters_.find(name);
    if (it != parameters_.end() && it->second->type == ParameterType::BOOLEAN) {
        const std::string& value = it->second->currentValue;
        return value == "true" || value == "1";
    }
    throw std::runtime_error("Parameter not found or wrong type: " + name);
}

std::string TunableParametersManager::getString(const std::string& name) const {
    auto it = parameters_.find(name);
    if (it != parameters_.end()) {
        return it->second->currentValue;
    }
    throw std::runtime_error("Parameter not found: " + name);
}

bool TunableParametersManager::setParameter(const std::string& name, const std::string& value) {
    auto it = parameters_.find(name);
    if (it == parameters_.end()) {
        return false;
    }

    if (!validateParameter(name, value)) {
        return false;
    }

    it->second->currentValue = value;
    it->second->lastModified = std::chrono::system_clock::now();
    pendingChanges_.push_back(name);

    notifyParameterChange(name, value);

    if (autoSave_) {
        saveToFile();
    }

    return true;
}

bool TunableParametersManager::setDouble(const std::string& name, double value) {
    return setParameter(name, std::to_string(value));
}

bool TunableParametersManager::setInt(const std::string& name, int value) {
    return setParameter(name, std::to_string(value));
}

bool TunableParametersManager::setBool(const std::string& name, bool value) {
    return setParameter(name, value ? "true" : "false");
}

void TunableParametersManager::initializeDefaultParameters() {
    // Risk Management Parameters
    registerDoubleParameter("risk.max_position_size", 0.25, 0.01, 1.0,
                           "Maximum position size as fraction of portfolio", "Risk Management");
    registerDoubleParameter("risk.max_sector_exposure", 0.25, 0.05, 0.50,
                           "Maximum exposure to any single sector", "Risk Management");
    registerDoubleParameter("risk.var_confidence_level", 0.95, 0.90, 0.99,
                           "VaR confidence level", "Risk Management");
    registerIntParameter("risk.var_lookback_days", 252, 30, 1000,
                        "VaR calculation lookback period in days", "Risk Management");

    // Algorithm Tuning Parameters
    registerDoubleParameter("algo.correlation_threshold", 0.87, 0.70, 0.95,
                           "Minimum correlation threshold for pair inclusion", "Algorithm Tuning");
    registerDoubleParameter("algo.confidence_threshold", 0.75, 0.50, 0.95,
                           "Algorithm confidence threshold for dynamic concentration", "Algorithm Tuning");
    registerIntParameter("algo.min_pairs", 3, 1, 10,
                        "Minimum number of trading pairs", "Algorithm Tuning");
    registerIntParameter("algo.max_pairs", 50, 10, 100,
                        "Maximum number of trading pairs", "Algorithm Tuning");
    registerDoubleParameter("algo.rebalance_threshold", 0.05, 0.01, 0.20,
                           "Portfolio rebalancing threshold", "Algorithm Tuning");

    // Data Processing Parameters
    registerIntParameter("data.lookback_days", 730, 90, 2000,
                        "Historical data lookback period", "Data Processing");
    registerDoubleParameter("data.quality_threshold", 0.95, 0.80, 1.0,
                           "Minimum data quality threshold", "Data Processing");
    registerIntParameter("data.min_observations", 100, 30, 500,
                        "Minimum observations required for analysis", "Data Processing");

    // Portfolio Optimization Parameters
    registerDoubleParameter("portfolio.target_volatility", 0.15, 0.05, 0.50,
                           "Target portfolio volatility", "Portfolio Optimization");
    registerDoubleParameter("portfolio.sharpe_ratio_target", 1.5, 0.5, 3.0,
                           "Target Sharpe ratio", "Portfolio Optimization");
    registerBoolParameter("portfolio.enable_sector_constraints", true,
                         "Enable sector-based constraints", "Portfolio Optimization");

    // Correlation Analysis Parameters
    registerDoubleParameter("correlation.rolling_window", 60, 20, 250,
                           "Rolling correlation window in days", "Correlation Analysis");
    registerDoubleParameter("correlation.statistical_significance", 0.05, 0.01, 0.10,
                           "Statistical significance level", "Correlation Analysis");
    registerBoolParameter("correlation.adjust_for_regime_change", true,
                         "Adjust correlations for market regime changes", "Correlation Analysis");

    // Performance Monitoring Parameters
    registerDoubleParameter("perf.benchmark_correlation", 0.30, -1.0, 1.0,
                           "Target correlation with benchmark", "Performance Monitoring");
    registerIntParameter("perf.reporting_frequency_days", 7, 1, 30,
                        "Performance reporting frequency", "Performance Monitoring");

    // API Configuration Parameters
    registerIntParameter("api.request_timeout_ms", 10000, 1000, 60000,
                        "API request timeout in milliseconds", "API Configuration");
    registerIntParameter("api.max_retries", 3, 1, 10,
                        "Maximum API request retries", "API Configuration");
    registerBoolParameter("api.enable_caching", true,
                         "Enable API response caching", "API Configuration");
}

void TunableParametersManager::setRiskProfile(RiskProfile profile) {
    currentProfile_ = profile;
    applyRiskProfileDefaults(profile);
}

void TunableParametersManager::applyRiskProfileDefaults(RiskProfile profile) {
    switch (profile) {
        case RiskProfile::CONSERVATIVE:
            setDouble("risk.max_position_size", 0.15);
            setDouble("risk.max_sector_exposure", 0.20);
            setDouble("algo.correlation_threshold", 0.88);
            setInt("algo.max_pairs", 25);
            setDouble("portfolio.target_volatility", 0.10);
            break;

        case RiskProfile::MODERATE:
            setDouble("risk.max_position_size", 0.25);
            setDouble("risk.max_sector_exposure", 0.25);
            setDouble("algo.correlation_threshold", 0.85);
            setInt("algo.max_pairs", 40);
            setDouble("portfolio.target_volatility", 0.15);
            break;

        case RiskProfile::AGGRESSIVE:
            setDouble("risk.max_position_size", 0.35);
            setDouble("risk.max_sector_exposure", 0.30);
            setDouble("algo.correlation_threshold", 0.80);
            setInt("algo.max_pairs", 50);
            setDouble("portfolio.target_volatility", 0.25);
            break;

        case RiskProfile::CUSTOM:
            // Don't override existing settings
            break;
    }
}

bool TunableParametersManager::validateParameter(const std::string& name, const std::string& value) const {
    auto it = parameters_.find(name);
    if (it == parameters_.end()) {
        return false;
    }

    if (it->second->validator) {
        return it->second->validator(value);
    }

    return true; // No specific validator, assume valid
}

std::vector<std::string> TunableParametersManager::getAllParameterNames() const {
    std::vector<std::string> names;
    for (const auto& [name, param] : parameters_) {
        names.push_back(name);
    }
    return names;
}

std::vector<std::string> TunableParametersManager::getParametersByCategory(const std::string& category) const {
    std::vector<std::string> names;
    for (const auto& [name, param] : parameters_) {
        if (param->category == category) {
            names.push_back(name);
        }
    }
    return names;
}

void TunableParametersManager::notifyParameterChange(const std::string& name, const std::string& newValue) {
    auto it = changeCallbacks_.find(name);
    if (it != changeCallbacks_.end() && it->second) {
        it->second(name, newValue);
    }
}

bool TunableParametersManager::saveToFile(const std::string& filePath) const {
    std::string path = filePath.empty() ? configFilePath_ : filePath;

    try {
        json config;
        config["profile"] = static_cast<int>(currentProfile_);
        config["last_save"] = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        json params;
        for (const auto& [name, param] : parameters_) {
            json paramData;
            paramData["value"] = param->currentValue;
            paramData["type"] = static_cast<int>(param->type);
            paramData["category"] = param->category;
            paramData["description"] = param->description;
            paramData["default"] = param->defaultValue;

            if (param->type == ParameterType::DOUBLE || param->type == ParameterType::INTEGER) {
                paramData["min"] = param->minValue;
                paramData["max"] = param->maxValue;
            }

            params[name] = paramData;
        }
        config["parameters"] = params;

        std::ofstream file(path);
        if (!file.is_open()) {
            return false;
        }

        file << std::setw(2) << config << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Error saving parameters: " << e.what() << std::endl;
        return false;
    }
}

bool TunableParametersManager::loadFromFile(const std::string& filePath) {
    std::string path = filePath.empty() ? configFilePath_ : filePath;

    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            // File doesn't exist, use defaults
            return true;
        }

        json config;
        file >> config;

        if (config.contains("profile")) {
            currentProfile_ = static_cast<RiskProfile>(config["profile"].get<int>());
        }

        if (config.contains("parameters")) {
            for (const auto& [name, paramData] : config["parameters"].items()) {
                auto it = parameters_.find(name);
                if (it != parameters_.end() && paramData.contains("value")) {
                    std::string value = paramData["value"].get<std::string>();
                    if (validateParameter(name, value)) {
                        it->second->currentValue = value;
                    }
                }
            }
        }

        pendingChanges_.clear();
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Error loading parameters: " << e.what() << std::endl;
        return false;
    }
}

// Global Parameters Implementation
TunableParametersManager& GlobalParameters::getInstance() {
    if (!instance_) {
        instance_ = std::make_unique<TunableParametersManager>();
    }
    return *instance_;
}

void GlobalParameters::initialize(const std::string& configPath) {
    instance_ = std::make_unique<TunableParametersManager>(configPath);
}

void GlobalParameters::cleanup() {
    instance_.reset();
}

double GlobalParameters::getDouble(const std::string& name) {
    return getInstance().getDouble(name);
}

int GlobalParameters::getInt(const std::string& name) {
    return getInstance().getInt(name);
}

bool GlobalParameters::getBool(const std::string& name) {
    return getInstance().getBool(name);
}

std::string GlobalParameters::getString(const std::string& name) {
    return getInstance().getString(name);
}

} // namespace Config
} // namespace CryptoClaude