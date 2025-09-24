#pragma once

#include <string>
#include <vector>
#include <optional>
#include <map>
#include <chrono>
#include <stdexcept>
#include <type_traits>

// Using nlohmann/json - header-only library
// In production, would include: #include <nlohmann/json.hpp>
// For now, we'll create a simplified interface that matches nlohmann/json API

namespace CryptoClaude {
namespace Json {

// Forward declaration for our JSON implementation
class JsonValue;

// Exception types for JSON parsing errors
class JsonParseException : public std::runtime_error {
public:
    explicit JsonParseException(const std::string& message)
        : std::runtime_error("JSON Parse Error: " + message) {}
};

class JsonAccessException : public std::runtime_error {
public:
    explicit JsonAccessException(const std::string& message)
        : std::runtime_error("JSON Access Error: " + message) {}
};

class JsonTypeException : public std::runtime_error {
public:
    explicit JsonTypeException(const std::string& message)
        : std::runtime_error("JSON Type Error: " + message) {}
};

// Validation result for business logic separation
struct ValidationResult {
    bool isValid = true;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;

    void addError(const std::string& error) {
        errors.push_back(error);
        isValid = false;
    }

    void addWarning(const std::string& warning) {
        warnings.push_back(warning);
    }

    bool hasErrors() const { return !errors.empty(); }
    bool hasWarnings() const { return !warnings.empty(); }
};

// Parse options for configuring JSON parsing behavior
struct ParseOptions {
    bool strictMode = false;           // Fail on any missing optional fields
    bool allowUnknownFields = true;    // Ignore unknown JSON fields
    bool logWarnings = true;           // Log warnings for optional field issues
    bool convertStringsToNumbers = true; // Try to convert string numbers to numeric types

    // Mixed approach error handling
    bool failOnMissingRequired = true;   // Throw on missing required fields
    bool warnOnMissingOptional = true;   // Warn but continue on missing optional fields
    bool warnOnTypeMismatch = true;      // Warn on type mismatches, try conversion
};

// Simplified JSON value class (would use nlohmann::json in production)
class JsonValue {
private:
    enum class Type {
        Null, Boolean, Number, String, Array, Object
    };

    Type type_;
    std::string stringValue_;
    double numberValue_;
    bool boolValue_;
    std::map<std::string, JsonValue> objectValue_;
    std::vector<JsonValue> arrayValue_;

public:
    JsonValue() : type_(Type::Null) {}
    JsonValue(bool value) : type_(Type::Boolean), boolValue_(value) {}
    JsonValue(double value) : type_(Type::Number), numberValue_(value) {}
    JsonValue(int value) : type_(Type::Number), numberValue_(static_cast<double>(value)) {}
    JsonValue(const char* value) : type_(Type::String), stringValue_(value) {}
    JsonValue(const std::string& value) : type_(Type::String), stringValue_(value) {}
    JsonValue(const std::map<std::string, JsonValue>& value) : type_(Type::Object), objectValue_(value) {}
    JsonValue(const std::vector<JsonValue>& value) : type_(Type::Array), arrayValue_(value) {}

    // Copy constructor
    JsonValue(const JsonValue& other) : type_(other.type_) {
        switch (type_) {
            case Type::Boolean: boolValue_ = other.boolValue_; break;
            case Type::Number: numberValue_ = other.numberValue_; break;
            case Type::String: stringValue_ = other.stringValue_; break;
            case Type::Object: objectValue_ = other.objectValue_; break;
            case Type::Array: arrayValue_ = other.arrayValue_; break;
            case Type::Null: break;
        }
    }

    // Assignment operator
    JsonValue& operator=(const JsonValue& other) {
        if (this != &other) {
            type_ = other.type_;
            switch (type_) {
                case Type::Boolean: boolValue_ = other.boolValue_; break;
                case Type::Number: numberValue_ = other.numberValue_; break;
                case Type::String: stringValue_ = other.stringValue_; break;
                case Type::Object: objectValue_ = other.objectValue_; break;
                case Type::Array: arrayValue_ = other.arrayValue_; break;
                case Type::Null: break;
            }
        }
        return *this;
    }

    // Type checking
    bool is_null() const { return type_ == Type::Null; }
    bool is_boolean() const { return type_ == Type::Boolean; }
    bool is_number() const { return type_ == Type::Number; }
    bool is_string() const { return type_ == Type::String; }
    bool is_array() const { return type_ == Type::Array; }
    bool is_object() const { return type_ == Type::Object; }

    // Value accessors (with type checking)
    bool get_bool() const;
    double get_number() const;
    std::string get_string() const;
    const std::map<std::string, JsonValue>& get_object() const;
    const std::vector<JsonValue>& get_array() const;

    // Object/array access
    bool contains(const std::string& key) const;
    const JsonValue& at(const std::string& key) const;
    const JsonValue& at(size_t index) const;
    JsonValue& operator[](const std::string& key);
    const JsonValue& operator[](const std::string& key) const;

    // Size and iteration
    size_t size() const;
    bool empty() const;

    // Conversion utilities
    template<typename T>
    T get() const;
};

// Main JSON parsing utilities class
class JsonParser {
private:
    ParseOptions options_;
    mutable ValidationResult lastValidation_;

public:
    explicit JsonParser(const ParseOptions& options = ParseOptions{});

    // Core parsing methods
    JsonValue parse(const std::string& jsonString) const;
    JsonValue parseFromFile(const std::string& filePath) const;

    // Safe extraction methods with mixed error handling
    template<typename T>
    std::optional<T> extractOptional(const JsonValue& json, const std::string& path) const;

    template<typename T>
    T extractRequired(const JsonValue& json, const std::string& path) const;

    template<typename T>
    T extractWithDefault(const JsonValue& json, const std::string& path, const T& defaultValue) const;

    // Specialized extractors for common types
    std::optional<std::string> getString(const JsonValue& json, const std::string& path) const;
    std::optional<double> getDouble(const JsonValue& json, const std::string& path) const;
    std::optional<int> getInt(const JsonValue& json, const std::string& path) const;
    std::optional<bool> getBool(const JsonValue& json, const std::string& path) const;
    std::optional<std::chrono::system_clock::time_point> getTimestamp(const JsonValue& json, const std::string& path) const;

    // Array and object utilities
    std::vector<JsonValue> getArray(const JsonValue& json, const std::string& path) const;
    JsonValue getObject(const JsonValue& json, const std::string& path) const;

    // Path-based access (supports "field.subfield.array[0]" syntax)
    JsonValue getByPath(const JsonValue& json, const std::string& path) const;
    bool pathExists(const JsonValue& json, const std::string& path) const;

    // Validation and error handling
    ValidationResult getLastValidation() const { return lastValidation_; }
    void clearValidation() { lastValidation_ = ValidationResult{}; }

    // Configuration
    void setOptions(const ParseOptions& options) { options_ = options; }
    ParseOptions getOptions() const { return options_; }

private:
    void logWarning(const std::string& message) const;
    void logError(const std::string& message) const;
    std::vector<std::string> splitPath(const std::string& path) const;
    JsonValue navigateToPath(const JsonValue& json, const std::vector<std::string>& pathParts) const;
    JsonValue parseJsonValue(const std::string& jsonString) const;
};

// Serialization utilities (JSON -> C++ models)
class JsonSerializer {
public:
    // Generic serialization interface
    template<typename T>
    static std::string toJson(const T& object);

    template<typename T>
    static T fromJson(const std::string& jsonString, const ParseOptions& options = ParseOptions{});

    template<typename T>
    static T fromJson(const JsonValue& json, const ParseOptions& options = ParseOptions{});

    // Validation utilities (separated from parsing)
    template<typename T>
    static ValidationResult validate(const T& object);
};

// API-specific response parsers
namespace ApiParsers {

// CryptoCompare API response structures
struct CryptoComparePrice {
    std::map<std::string, double> prices;  // Currency -> Price mapping
    std::optional<std::string> errorMessage;
};

struct CryptoCompareOHLCVData {
    std::chrono::system_clock::time_point timestamp;
    double open, high, low, close, volume;
};

struct CryptoCompareOHLCV {
    std::vector<CryptoCompareOHLCVData> data;
    std::optional<std::string> errorMessage;
};

// NewsAPI response structures
struct NewsApiArticle {
    std::string title;
    std::string description;
    std::string content;
    std::string url;
    std::string publishedAt;
    std::optional<std::string> author;
    std::optional<std::string> source;
};

struct NewsApiResponse {
    std::string status;
    int totalResults;
    std::vector<NewsApiArticle> articles;
    std::optional<std::string> errorMessage;
};

// Specialized parsers for each API
class CryptoCompareParser {
public:
    static CryptoComparePrice parsePrice(const std::string& jsonResponse);
    static CryptoCompareOHLCV parseHistoricalData(const std::string& jsonResponse);
    static bool isErrorResponse(const JsonValue& json);
    static std::string extractErrorMessage(const JsonValue& json);
};

class NewsApiParser {
public:
    static NewsApiResponse parseEverything(const std::string& jsonResponse);
    static NewsApiResponse parseTopHeadlines(const std::string& jsonResponse);
    static NewsApiArticle parseArticle(const JsonValue& articleJson);
    static bool isErrorResponse(const JsonValue& json);
    static std::string extractErrorMessage(const JsonValue& json);
};

} // namespace ApiParsers

// Utility functions for common JSON operations
namespace JsonUtils {

// JSON string manipulation
std::string prettify(const std::string& jsonString);
std::string minify(const std::string& jsonString);
bool isValidJson(const std::string& jsonString);

// Type conversion utilities
std::optional<double> stringToDouble(const std::string& str);
std::optional<int> stringToInt(const std::string& str);
std::optional<bool> stringToBool(const std::string& str);
std::chrono::system_clock::time_point parseTimestamp(const std::string& timestamp);

// JSON comparison and merging
bool jsonEquals(const JsonValue& a, const JsonValue& b);
JsonValue mergeJson(const JsonValue& base, const JsonValue& override);
JsonValue filterJsonFields(const JsonValue& json, const std::vector<std::string>& allowedFields);

// Debug and logging utilities
std::string jsonToDebugString(const JsonValue& json, int indent = 2);
void logJsonStructure(const JsonValue& json, const std::string& name = "JSON");

} // namespace JsonUtils

} // namespace Json
} // namespace CryptoClaude