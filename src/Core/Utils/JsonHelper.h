#pragma once
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <nlohmann/json.hpp>

namespace CryptoClaude {
namespace Utils {

using Json = nlohmann::json;

class JsonHelper {
public:
    // Safe JSON extraction with default values
    static int safeGetInt(const Json& json, const std::string& key, int defaultValue = 0);
    static double safeGetDouble(const Json& json, const std::string& key, double defaultValue = 0.0);
    static std::string safeGetString(const Json& json, const std::string& key, const std::string& defaultValue = "");
    static bool safeGetBool(const Json& json, const std::string& key, bool defaultValue = false);

    // Array operations
    static std::vector<std::string> jsonArrayToStringVector(const Json& jsonArray);
    static Json stringVectorToJsonArray(const std::vector<std::string>& stringVector);

    // Nested access with path notation (e.g., "data.items[0].name")
    static std::optional<Json> getNestedValue(const Json& json, const std::string& path);

    // JSON validation and parsing
    static bool isValidJson(const std::string& jsonString);
    static std::optional<Json> parseJson(const std::string& jsonString);

    // Pretty printing and formatting
    static std::string prettyPrint(const Json& json, int indent = 2);
    static std::string minify(const Json& json);

    // Type checking utilities
    static bool hasKey(const Json& json, const std::string& key);
    static bool isArray(const Json& json, const std::string& key = "");
    static bool isObject(const Json& json, const std::string& key = "");
    static bool isString(const Json& json, const std::string& key = "");
    static bool isNumber(const Json& json, const std::string& key = "");

    // Merging and combining JSON objects
    static Json merge(const Json& base, const Json& overlay);
    static Json combineArrays(const std::vector<Json>& arrays);

    // Filtering and transformation
    static Json filterKeys(const Json& json, const std::vector<std::string>& keysToKeep);
    static Json removeKeys(const Json& json, const std::vector<std::string>& keysToRemove);

private:
    static std::vector<std::string> splitPath(const std::string& path);
    static std::optional<Json> navigatePath(const Json& json, const std::vector<std::string>& pathParts);
};

// Helper class for building JSON objects fluently
class JsonBuilder {
public:
    JsonBuilder();

    JsonBuilder& add(const std::string& key, const std::string& value);
    JsonBuilder& add(const std::string& key, int value);
    JsonBuilder& add(const std::string& key, double value);
    JsonBuilder& add(const std::string& key, bool value);
    JsonBuilder& add(const std::string& key, const Json& value);

    JsonBuilder& addArray(const std::string& key);
    JsonBuilder& addToArray(const std::string& value);
    JsonBuilder& addToArray(int value);
    JsonBuilder& addToArray(double value);
    JsonBuilder& addToArray(const Json& value);

    JsonBuilder& addObject(const std::string& key);
    JsonBuilder& endObject();

    Json build();
    std::string buildString(int indent = -1);

private:
    Json m_json;
    std::vector<Json*> m_stack;
    Json* m_current;
};

} // namespace Utils
} // namespace CryptoClaude