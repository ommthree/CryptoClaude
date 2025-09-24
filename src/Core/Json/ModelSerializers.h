#pragma once

#include "JsonParser.h"
#include "../Database/Models/MarketData.h"
#include "../Database/Models/PortfolioData.h"
#include "../Database/Models/SentimentData.h"
#include <string>
#include <vector>
#include <optional>

namespace CryptoClaude {
namespace Json {

// Forward declarations
class JsonValue;
class JsonParser;

// Model-specific serializers (following separation of concerns principle)
// JSON parsing is separated from business logic validation
namespace ModelSerializers {

// ================================
// MarketData Serialization
// ================================

class MarketDataSerializer {
public:
    // JSON to MarketData conversion
    static Database::Models::MarketData fromJson(const JsonValue& json, const ParseOptions& options = ParseOptions{});
    static Database::Models::MarketData fromJsonString(const std::string& jsonString, const ParseOptions& options = ParseOptions{});

    // MarketData to JSON conversion
    static JsonValue toJson(const Database::Models::MarketData& marketData);
    static std::string toJsonString(const Database::Models::MarketData& marketData);

    // CryptoCompare API specific parsing
    static Database::Models::MarketData fromCryptoComparePrice(const JsonValue& json, const std::string& symbol);
    static std::vector<Database::Models::MarketData> fromCryptoCompareOHLCV(const JsonValue& json, const std::string& symbol);

    // Business logic validation (separate from JSON parsing)
    static ValidationResult validateMarketData(const Database::Models::MarketData& data);

private:
    static std::optional<std::chrono::system_clock::time_point> parseTimestamp(const JsonValue& json, const std::string& field);
    static double parsePrice(const JsonValue& json, const std::string& field, double defaultValue = 0.0);
};

// ================================
// SentimentData Serialization
// ================================

class SentimentDataSerializer {
public:
    // JSON to SentimentData conversion
    static Database::Models::SentimentData fromJson(const JsonValue& json, const ParseOptions& options = ParseOptions{});
    static Database::Models::SentimentData fromJsonString(const std::string& jsonString, const ParseOptions& options = ParseOptions{});

    // SentimentData to JSON conversion
    static JsonValue toJson(const Database::Models::SentimentData& sentimentData);
    static std::string toJsonString(const Database::Models::SentimentData& sentimentData);

    // NewsAPI specific parsing
    static Database::Models::SentimentData fromNewsApiArticle(const JsonValue& articleJson, const std::string& symbol);
    static std::vector<Database::Models::SentimentData> fromNewsApiResponse(const JsonValue& json, const std::string& symbol);

    // Business logic validation
    static ValidationResult validateSentimentData(const Database::Models::SentimentData& data);

private:
    static double calculateSentimentScore(const std::string& title, const std::string& description, const std::string& content);
    static std::string extractRelevantKeywords(const std::string& content);
};

// ================================
// PortfolioData Serialization
// ================================

class PortfolioDataSerializer {
public:
    // JSON to PortfolioData conversion
    static Database::Models::Portfolio fromJson(const JsonValue& json, const ParseOptions& options = ParseOptions{});
    static Database::Models::Portfolio fromJsonString(const std::string& jsonString, const ParseOptions& options = ParseOptions{});

    // PortfolioData to JSON conversion
    static JsonValue toJson(const Database::Models::Portfolio& portfolio);
    static std::string toJsonString(const Database::Models::Portfolio& portfolio);

    // Position serialization
    static Database::Models::Position positionFromJson(const JsonValue& json, const ParseOptions& options = ParseOptions{});
    static JsonValue positionToJson(const Database::Models::Position& position);

    // Business logic validation
    static ValidationResult validatePortfolio(const Database::Models::Portfolio& portfolio);
    static ValidationResult validatePosition(const Database::Models::Position& position);

private:
    static double parseCurrency(const JsonValue& json, const std::string& field);
    static void validateLeverageConstraints(const Database::Models::Position& position, ValidationResult& result);
};

// ================================
// Batch Operations & Utilities
// ================================

class BatchSerializer {
public:
    // Batch processing for multiple data points
    template<typename T>
    static std::vector<T> parseJsonArray(const JsonValue& jsonArray,
                                       std::function<T(const JsonValue&)> parser,
                                       const ParseOptions& options = ParseOptions{});

    template<typename T>
    static JsonValue serializeArray(const std::vector<T>& items,
                                   std::function<JsonValue(const T&)> serializer);

    // Error handling for batch operations
    struct BatchResult {
        int successCount = 0;
        int errorCount = 0;
        std::vector<std::string> errors;
        ValidationResult validation;

        bool hasErrors() const { return errorCount > 0; }
        double successRate() const {
            int total = successCount + errorCount;
            return total > 0 ? static_cast<double>(successCount) / total : 0.0;
        }
    };

    template<typename T>
    static BatchResult parseBatch(const std::string& jsonString,
                                 std::function<T(const JsonValue&)> parser,
                                 std::vector<T>& results,
                                 const ParseOptions& options = ParseOptions{});
};

// ================================
// API Response Integration
// ================================

class ApiResponseProcessor {
public:
    // Process complete API responses into our data models
    struct ProcessedResponse {
        std::vector<Database::Models::MarketData> marketData;
        std::vector<Database::Models::SentimentData> sentimentData;
        ValidationResult validation;
        std::optional<std::string> errorMessage;
    };

    // CryptoCompare API responses
    static ProcessedResponse processCryptoComparePrice(const std::string& jsonResponse,
                                                      const std::vector<std::string>& symbols);
    static ProcessedResponse processCryptoCompareHistorical(const std::string& jsonResponse,
                                                           const std::string& symbol);

    // NewsAPI responses
    static ProcessedResponse processNewsApiResponse(const std::string& jsonResponse,
                                                   const std::vector<std::string>& relevantSymbols);

    // Generic API error handling
    static bool isApiError(const JsonValue& json);
    static std::string extractApiError(const JsonValue& json);
    static void logApiResponse(const std::string& apiName, const ProcessedResponse& response);

private:
    static std::vector<std::string> extractRelevantSymbols(const std::string& content,
                                                          const std::vector<std::string>& knownSymbols);
    static double calculateRelevanceScore(const std::string& content, const std::string& symbol);
};

// ================================
// Schema Validation & Migration
// ================================

class SchemaValidator {
public:
    // JSON schema validation
    struct SchemaRequirements {
        std::vector<std::string> requiredFields;
        std::map<std::string, std::string> expectedTypes;  // field -> type
        std::vector<std::string> optionalFields;
        bool allowAdditionalFields = true;
    };

    static ValidationResult validateSchema(const JsonValue& json, const SchemaRequirements& requirements);
    static SchemaRequirements getMarketDataSchema();
    static SchemaRequirements getSentimentDataSchema();
    static SchemaRequirements getPortfolioSchema();

    // Schema migration utilities
    static JsonValue migrateSchema(const JsonValue& oldJson, const std::string& fromVersion, const std::string& toVersion);
    static bool needsMigration(const JsonValue& json, const std::string& currentVersion);

private:
    static bool isTypeMatch(const JsonValue& value, const std::string& expectedType);
    static JsonValue applyMigrationRules(const JsonValue& json, const std::string& fromVersion, const std::string& toVersion);
};

// ================================
// Configuration & Settings Serialization
// ================================

class ConfigSerializer {
public:
    // System configuration serialization
    static JsonValue serializeConfig(const Configuration::Config& config);
    static Configuration::Config deserializeConfig(const JsonValue& json);
    static Configuration::Config loadConfigFromFile(const std::string& filePath);
    static void saveConfigToFile(const Configuration::Config& config, const std::string& filePath);

    // Trading parameters serialization
    static JsonValue serializeTradingParams(const Trading::TradingParameters& params);
    static Trading::TradingParameters deserializeTradingParams(const JsonValue& json);

    // Risk limits serialization
    static JsonValue serializeRiskLimits(const Trading::RiskLimits& limits);
    static Trading::RiskLimits deserializeRiskLimits(const JsonValue& json);

private:
    static ValidationResult validateConfig(const Configuration::Config& config);
};

} // namespace ModelSerializers

// ================================
// Template Implementations
// ================================

// Template implementations for BatchSerializer
template<typename T>
std::vector<T> ModelSerializers::BatchSerializer::parseJsonArray(
    const JsonValue& jsonArray,
    std::function<T(const JsonValue&)> parser,
    const ParseOptions& options) {

    std::vector<T> results;

    if (!jsonArray.is_array()) {
        if (options.logWarnings) {
            std::cout << "[BATCH] Warning: Expected array but got different type" << std::endl;
        }
        return results;
    }

    for (const auto& item : jsonArray.get_array()) {
        try {
            results.push_back(parser(item));
        } catch (const std::exception& e) {
            if (options.logWarnings) {
                std::cout << "[BATCH] Warning: Failed to parse array item: " << e.what() << std::endl;
            }
            // Continue with other items
        }
    }

    return results;
}

template<typename T>
JsonValue ModelSerializers::BatchSerializer::serializeArray(
    const std::vector<T>& items,
    std::function<JsonValue(const T&)> serializer) {

    std::vector<JsonValue> jsonItems;

    for (const auto& item : items) {
        try {
            jsonItems.push_back(serializer(item));
        } catch (const std::exception& e) {
            std::cout << "[BATCH] Warning: Failed to serialize item: " << e.what() << std::endl;
            // Continue with other items
        }
    }

    return JsonValue{jsonItems};
}

template<typename T>
typename ModelSerializers::BatchSerializer::BatchResult ModelSerializers::BatchSerializer::parseBatch(
    const std::string& jsonString,
    std::function<T(const JsonValue&)> parser,
    std::vector<T>& results,
    const ParseOptions& options) {

    BatchResult batchResult;

    try {
        JsonParser jsonParser(options);
        JsonValue json = jsonParser.parse(jsonString);

        if (json.is_array()) {
            for (const auto& item : json.get_array()) {
                try {
                    results.push_back(parser(item));
                    batchResult.successCount++;
                } catch (const std::exception& e) {
                    batchResult.errorCount++;
                    batchResult.errors.push_back(e.what());
                }
            }
        } else {
            // Single item
            try {
                results.push_back(parser(json));
                batchResult.successCount++;
            } catch (const std::exception& e) {
                batchResult.errorCount++;
                batchResult.errors.push_back(e.what());
            }
        }

        batchResult.validation = jsonParser.getLastValidation();

    } catch (const JsonParseException& e) {
        batchResult.errorCount++;
        batchResult.errors.push_back("JSON parsing failed: " + std::string(e.what()));
    }

    return batchResult;
}

} // namespace Json
} // namespace CryptoClaude