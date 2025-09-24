#pragma once

// Secure and portable JSON library inclusion
// This implementation removes all hard-coded paths and security vulnerabilities

#include <string>
#include <stdexcept>

// PROPER CONDITIONAL COMPILATION
// Respects external JSON_LIBRARY_AVAILABLE override for testing and deployment flexibility

#ifndef JSON_LIBRARY_AVAILABLE
  // Only auto-detect if not explicitly overridden
  #ifdef __has_include
    #if __has_include(<nlohmann/json.hpp>)
      #include <nlohmann/json.hpp>
      #define JSON_LIBRARY_AVAILABLE 1
    #else
      #define JSON_LIBRARY_AVAILABLE 0
      #warning "nlohmann-json library not found in standard include paths. Please run install_dependencies.sh to install properly."
    #endif
  #else
    // Fallback for older compilers without __has_include support
    // Attempt to include and assume availability (may cause compile error if not found)
    #include <nlohmann/json.hpp>
    #define JSON_LIBRARY_AVAILABLE 1
  #endif
#else
  // JSON_LIBRARY_AVAILABLE was explicitly set (0 or 1)
  #if JSON_LIBRARY_AVAILABLE
    // Include the library only if explicitly enabled
    #include <nlohmann/json.hpp>
  #endif
#endif

namespace CryptoClaude {
namespace Json {

#if JSON_LIBRARY_AVAILABLE
using json = nlohmann::json;
#else
// Stub class for when nlohmann-json is not available
// CONSISTENT BEHAVIOR: All operations either throw or return safe defaults
class json {
private:
    bool stub_accessed_ = false;

public:
    json() = default;
    explicit json(const std::string&) {}

    static json parse(const std::string&) {
        throw std::runtime_error("JSON library not available. Please install nlohmann-json and rebuild.");
    }

    std::string dump(int = -1) const {
        throw std::runtime_error("JSON library not available. Please install nlohmann-json and rebuild.");
    }

    // SECURITY FIX: Consistent behavior - all operations throw
    bool contains(const std::string&) const {
        throw std::runtime_error("JSON library not available. Please install nlohmann-json and rebuild.");
    }
    bool is_string() const {
        throw std::runtime_error("JSON library not available. Please install nlohmann-json and rebuild.");
    }
    bool is_number() const {
        throw std::runtime_error("JSON library not available. Please install nlohmann-json and rebuild.");
    }
    bool is_number_integer() const {
        throw std::runtime_error("JSON library not available. Please install nlohmann-json and rebuild.");
    }
    bool is_boolean() const {
        throw std::runtime_error("JSON library not available. Please install nlohmann-json and rebuild.");
    }
    bool is_array() const {
        throw std::runtime_error("JSON library not available. Please install nlohmann-json and rebuild.");
    }
    bool is_object() const {
        throw std::runtime_error("JSON library not available. Please install nlohmann-json and rebuild.");
    }

    template<typename T>
    T get() const {
        throw std::runtime_error("JSON library not available. Please install nlohmann-json and rebuild.");
    }

    // CRITICAL FIX: operator[] now throws instead of silent failure
    json operator[](const std::string&) const {
        throw std::runtime_error("JSON library not available. Please install nlohmann-json and rebuild.");
    }
    json& operator[](const std::string&) {
        throw std::runtime_error("JSON library not available. Please install nlohmann-json and rebuild.");
    }
};
#endif

// JSON utility functions with proper error handling
class JsonHelper {
public:
    // Safe JSON parsing
    static bool parseString(const std::string& jsonStr, json& result);
    static std::string toString(const json& jsonObj, int indent = -1);

    // Safe value extraction with defaults
    static std::string getString(const json& obj, const std::string& key, const std::string& defaultValue = "");
    static double getDouble(const json& obj, const std::string& key, double defaultValue = 0.0);
    static int getInt(const json& obj, const std::string& key, int defaultValue = 0);
    static bool getBool(const json& obj, const std::string& key, bool defaultValue = false);

    // Safe existence and type checks
    static bool hasKey(const json& obj, const std::string& key);
    static bool isString(const json& obj, const std::string& key);
    static bool isNumber(const json& obj, const std::string& key);
    static bool isBool(const json& obj, const std::string& key);
    static bool isArray(const json& obj, const std::string& key);
    static bool isObject(const json& obj, const std::string& key);

    // Runtime detection of JSON library availability
    static bool isJsonLibraryAvailable() {
        return JSON_LIBRARY_AVAILABLE == 1;
    }

    // Force stub mode (for testing and environments without JSON library)
    static bool isStubMode() {
        return JSON_LIBRARY_AVAILABLE == 0;
    }

    // Get installation instructions if library is not available
    static std::string getInstallationInstructions() {
        return "nlohmann-json library not found. Please run: ./install_dependencies.sh";
    }
};

} // namespace Json
} // namespace CryptoClaude