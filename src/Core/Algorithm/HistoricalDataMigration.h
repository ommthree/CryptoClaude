#pragma once

#include "../Database/DatabaseManager.h"
#include <string>
#include <vector>
#include <memory>

namespace CryptoClaude {
namespace Algorithm {

/**
 * Historical Data Migration Manager - Day 21 Implementation
 * Manages database schema updates for historical data storage
 * Essential for real correlation calculation and backtesting framework
 */
class HistoricalDataMigration {
public:
    // Migration result structure
    struct MigrationResult {
        bool success;
        std::string migration_name;
        std::string version_before;
        std::string version_after;
        std::vector<std::string> executed_statements;
        std::vector<std::string> errors;
        std::chrono::system_clock::time_point executed_at;

        MigrationResult() : success(false), executed_at(std::chrono::system_clock::now()) {}
    };

    // Database schema version tracking
    struct SchemaVersion {
        std::string version_number;
        std::string description;
        std::chrono::system_clock::time_point applied_at;
        bool is_current;

        SchemaVersion() : is_current(false), applied_at(std::chrono::system_clock::now()) {}
    };

private:
    std::unique_ptr<Database::DatabaseManager> db_manager_;

    // Migration SQL statements
    std::vector<std::string> getHistoricalPriceMigration();
    std::vector<std::string> getHistoricalSentimentMigration();
    std::vector<std::string> getCorrelationTrackingMigration();
    std::vector<std::string> getBacktestResultsMigration();
    std::vector<std::string> getAlgorithmPerformanceMigration();
    std::vector<std::string> getDataQualityMigration();

    // Migration execution helpers
    bool executeStatements(const std::vector<std::string>& statements,
                          std::vector<std::string>& errors);
    bool checkMigrationApplied(const std::string& migration_name);
    void recordMigration(const std::string& migration_name, const std::string& version);

public:
    explicit HistoricalDataMigration(std::unique_ptr<Database::DatabaseManager> db_manager);
    ~HistoricalDataMigration() = default;

    // Main migration interface
    std::vector<MigrationResult> applyAllMigrations();
    MigrationResult applyMigration(const std::string& migration_name);

    // Individual migration methods
    MigrationResult createHistoricalPricesTables();
    MigrationResult createHistoricalSentimentTables();
    MigrationResult createCorrelationTrackingTables();
    MigrationResult createBacktestResultsTables();
    MigrationResult createAlgorithmPerformanceTables();
    MigrationResult createDataQualityTables();

    // Schema management
    SchemaVersion getCurrentSchemaVersion();
    std::vector<SchemaVersion> getSchemaHistory();
    bool rollbackMigration(const std::string& migration_name);

    // Validation and integrity
    bool validateSchemaIntegrity();
    std::vector<std::string> checkMissingTables();
    std::vector<std::string> checkMissingIndices();

    // Migration status
    bool isFullyMigrated();
    std::vector<std::string> getPendingMigrations();
};

} // namespace Algorithm
} // namespace CryptoClaude