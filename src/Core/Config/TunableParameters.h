#pragma once

#include <string>
#include <map>
#include <memory>
#include <functional>
#include <vector>
#include <chrono>

namespace CryptoClaude {
namespace Config {

// Parameter types
enum class ParameterType {
    DOUBLE,
    INTEGER,
    BOOLEAN,
    STRING
};

// Parameter validation function type
using ValidationFunc = std::function<bool(const std::string&)>;

// Individual parameter configuration
struct Parameter {
    std::string name;
    ParameterType type;
    std::string currentValue;
    std::string defaultValue;
    std::string description;

    // Validation constraints
    ValidationFunc validator;
    double minValue = 0.0;
    double maxValue = 1.0;
    std::vector<std::string> allowedValues; // For enumerated parameters

    // Metadata
    std::string category;
    bool requiresRestart = false;
    std::chrono::system_clock::time_point lastModified;

    Parameter(const std::string& n, ParameterType t, const std::string& def,
             const std::string& desc, const std::string& cat = "General")
        : name(n), type(t), currentValue(def), defaultValue(def),
          description(desc), category(cat),
          lastModified(std::chrono::system_clock::now()) {}
};

// Parameter categories for organization
enum class ParameterCategory {
    RISK_MANAGEMENT,
    ALGORITHM_TUNING,
    DATA_PROCESSING,
    PORTFOLIO_OPTIMIZATION,
    CORRELATION_ANALYSIS,
    PERFORMANCE_MONITORING,
    API_CONFIGURATION
};

// Risk profile presets
enum class RiskProfile {
    CONSERVATIVE,
    MODERATE,
    AGGRESSIVE,
    CUSTOM
};

// Main tunable parameters manager
class TunableParametersManager {
private:
    std::map<std::string, std::unique_ptr<Parameter>> parameters_;
    RiskProfile currentProfile_;
    std::string configFilePath_;
    bool autoSave_;

    // Change tracking
    std::vector<std::string> pendingChanges_;
    std::chrono::system_clock::time_point lastSave_;

public:
    explicit TunableParametersManager(const std::string& configPath = "config/parameters.json");
    ~TunableParametersManager();

    // Parameter registration and management
    void registerParameter(const std::string& name, ParameterType type,
                          const std::string& defaultValue, const std::string& description,
                          const std::string& category = "General");
    void registerDoubleParameter(const std::string& name, double defaultValue,
                                double minVal, double maxVal, const std::string& description,
                                const std::string& category = "General");
    void registerIntParameter(const std::string& name, int defaultValue,
                             int minVal, int maxVal, const std::string& description,
                             const std::string& category = "General");
    void registerBoolParameter(const std::string& name, bool defaultValue,
                              const std::string& description, const std::string& category = "General");

    // Parameter access
    double getDouble(const std::string& name) const;
    int getInt(const std::string& name) const;
    bool getBool(const std::string& name) const;
    std::string getString(const std::string& name) const;

    // Parameter modification
    bool setParameter(const std::string& name, const std::string& value);
    bool setDouble(const std::string& name, double value);
    bool setInt(const std::string& name, int value);
    bool setBool(const std::string& name, bool value);

    // Risk profile management
    void setRiskProfile(RiskProfile profile);
    RiskProfile getCurrentProfile() const { return currentProfile_; }
    void loadRiskProfilePresets(RiskProfile profile);

    // Configuration persistence
    bool loadFromFile(const std::string& filePath = "");
    bool saveToFile(const std::string& filePath = "") const;
    void enableAutoSave(bool enable) { autoSave_ = enable; }

    // Parameter discovery and validation
    std::vector<std::string> getAllParameterNames() const;
    std::vector<std::string> getParametersByCategory(const std::string& category) const;
    bool validateParameter(const std::string& name, const std::string& value) const;

    // Change tracking
    bool hasUnsavedChanges() const { return !pendingChanges_.empty(); }
    std::vector<std::string> getPendingChanges() const { return pendingChanges_; }
    void clearPendingChanges() { pendingChanges_.clear(); }

    // Parameter information
    Parameter* getParameter(const std::string& name) const;
    std::string getParameterInfo(const std::string& name) const;
    std::map<std::string, std::string> getAllParameters() const;

    // Real-time parameter updates
    using ParameterChangeCallback = std::function<void(const std::string&, const std::string&)>;
    void setChangeCallback(const std::string& parameterName, ParameterChangeCallback callback);
    void removeChangeCallback(const std::string& parameterName);

private:
    std::map<std::string, ParameterChangeCallback> changeCallbacks_;
    void notifyParameterChange(const std::string& name, const std::string& newValue);
    void initializeDefaultParameters();
    void applyRiskProfileDefaults(RiskProfile profile);
    bool parseJsonConfig(const std::string& jsonContent);
    std::string generateJsonConfig() const;
};

// Global parameter access singleton for convenience
class GlobalParameters {
private:
    static std::unique_ptr<TunableParametersManager> instance_;

public:
    static TunableParametersManager& getInstance();
    static void initialize(const std::string& configPath = "config/parameters.json");
    static void cleanup();

    // Convenience static access methods
    static double getDouble(const std::string& name);
    static int getInt(const std::string& name);
    static bool getBool(const std::string& name);
    static std::string getString(const std::string& name);
};

// Macros for easy parameter access
#define GET_PARAM_DOUBLE(name) GlobalParameters::getDouble(name)
#define GET_PARAM_INT(name) GlobalParameters::getInt(name)
#define GET_PARAM_BOOL(name) GlobalParameters::getBool(name)
#define GET_PARAM_STRING(name) GlobalParameters::getString(name)

} // namespace Config
} // namespace CryptoClaude