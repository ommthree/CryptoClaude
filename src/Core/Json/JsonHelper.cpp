#include "JsonHelper.h"
#include <iostream>

namespace CryptoClaude {
namespace Json {

json JsonHelper::parse(const std::string& jsonStr) {
#if JSON_LIBRARY_AVAILABLE
    try {
        return json::parse(jsonStr);
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
        return json{};
    } catch (const std::exception& e) {
        std::cerr << "JSON parsing exception: " << e.what() << std::endl;
        return json{};
    }
#else
    std::cerr << "Error: " << getInstallationInstructions() << std::endl;
    return json{};
#endif
}

bool JsonHelper::parseString(const std::string& jsonStr, json& result) {
#if JSON_LIBRARY_AVAILABLE
    try {
        result = json::parse(jsonStr);
        return true;
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "JSON parsing exception: " << e.what() << std::endl;
        return false;
    }
#else
    std::cerr << "Error: " << getInstallationInstructions() << std::endl;
    return false;
#endif
}

std::string JsonHelper::toString(const json& jsonObj, int indent) {
#if JSON_LIBRARY_AVAILABLE
    try {
        return jsonObj.dump(indent);
    } catch (const std::exception& e) {
        std::cerr << "JSON stringify error: " << e.what() << std::endl;
        return "{}";
    }
#else
    std::cerr << "Error: " << getInstallationInstructions() << std::endl;
    return "{}";
#endif
}

std::string JsonHelper::getString(const json& obj, const std::string& key, const std::string& defaultValue) {
#if JSON_LIBRARY_AVAILABLE
    try {
        if (obj.contains(key) && obj[key].is_string()) {
            return obj[key].get<std::string>();
        }
    } catch (const std::exception& e) {
        std::cerr << "Error getting string value for key '" << key << "': " << e.what() << std::endl;
    }
#else
    std::cerr << "Warning: JSON library not available, returning default for key '" << key << "'" << std::endl;
#endif
    return defaultValue;
}

double JsonHelper::getDouble(const json& obj, const std::string& key, double defaultValue) {
#if JSON_LIBRARY_AVAILABLE
    try {
        if (obj.contains(key) && obj[key].is_number()) {
            return obj[key].get<double>();
        }
    } catch (const std::exception& e) {
        std::cerr << "Error getting double value for key '" << key << "': " << e.what() << std::endl;
    }
#else
    std::cerr << "Warning: JSON library not available, returning default for key '" << key << "'" << std::endl;
#endif
    return defaultValue;
}

int JsonHelper::getInt(const json& obj, const std::string& key, int defaultValue) {
#if JSON_LIBRARY_AVAILABLE
    try {
        if (obj.contains(key) && obj[key].is_number_integer()) {
            return obj[key].get<int>();
        }
    } catch (const std::exception& e) {
        std::cerr << "Error getting int value for key '" << key << "': " << e.what() << std::endl;
    }
#else
    std::cerr << "Warning: JSON library not available, returning default for key '" << key << "'" << std::endl;
#endif
    return defaultValue;
}

bool JsonHelper::getBool(const json& obj, const std::string& key, bool defaultValue) {
#if JSON_LIBRARY_AVAILABLE
    try {
        if (obj.contains(key) && obj[key].is_boolean()) {
            return obj[key].get<bool>();
        }
    } catch (const std::exception& e) {
        std::cerr << "Error getting bool value for key '" << key << "': " << e.what() << std::endl;
    }
#else
    std::cerr << "Warning: JSON library not available, returning default for key '" << key << "'" << std::endl;
#endif
    return defaultValue;
}

bool JsonHelper::hasKey(const json& obj, const std::string& key) {
#if JSON_LIBRARY_AVAILABLE
    try {
        return obj.contains(key);
    } catch (const std::exception& e) {
        std::cerr << "Error checking key existence '" << key << "': " << e.what() << std::endl;
        return false;
    }
#else
    // CONSISTENCY FIX: Stub implementation will throw, so catch it
    try {
        return obj.contains(key);
    } catch (const std::runtime_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return false;
    }
#endif
}

bool JsonHelper::isString(const json& obj, const std::string& key) {
#if JSON_LIBRARY_AVAILABLE
    try {
        return obj.contains(key) && obj[key].is_string();
    } catch (const std::exception& e) {
        return false;
    }
#else
    try {
        return obj.contains(key) && obj[key].is_string();
    } catch (const std::runtime_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return false;
    }
#endif
}

bool JsonHelper::isNumber(const json& obj, const std::string& key) {
#if JSON_LIBRARY_AVAILABLE
    try {
        return obj.contains(key) && obj[key].is_number();
    } catch (const std::exception& e) {
        return false;
    }
#else
    try {
        return obj.contains(key) && obj[key].is_number();
    } catch (const std::runtime_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return false;
    }
#endif
}

bool JsonHelper::isBool(const json& obj, const std::string& key) {
#if JSON_LIBRARY_AVAILABLE
    try {
        return obj.contains(key) && obj[key].is_boolean();
    } catch (const std::exception& e) {
        return false;
    }
#else
    try {
        return obj.contains(key) && obj[key].is_boolean();
    } catch (const std::runtime_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return false;
    }
#endif
}

bool JsonHelper::isArray(const json& obj, const std::string& key) {
#if JSON_LIBRARY_AVAILABLE
    try {
        return obj.contains(key) && obj[key].is_array();
    } catch (const std::exception& e) {
        return false;
    }
#else
    try {
        return obj.contains(key) && obj[key].is_array();
    } catch (const std::runtime_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return false;
    }
#endif
}

bool JsonHelper::isObject(const json& obj, const std::string& key) {
#if JSON_LIBRARY_AVAILABLE
    try {
        return obj.contains(key) && obj[key].is_object();
    } catch (const std::exception& e) {
        return false;
    }
#else
    try {
        return obj.contains(key) && obj[key].is_object();
    } catch (const std::runtime_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return false;
    }
#endif
}

} // namespace Json
} // namespace CryptoClaude