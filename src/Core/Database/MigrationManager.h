#pragma once
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <sqlite3.h>

namespace CryptoClaude {
namespace Database {

class DatabaseManager;

struct Migration {
    int version;
    std::string name;
    std::string description;
    std::function<bool(sqlite3*)> up;
    std::function<bool(sqlite3*)> down;
    std::string checksum;
};

class MigrationManager {
public:
    explicit MigrationManager(DatabaseManager& dbManager);
    ~MigrationManager() = default;

    // Initialize migration system
    bool initialize();

    // Run all pending migrations
    bool runMigrations();

    // Rollback to specific version
    bool rollbackToVersion(int targetVersion);

    // Check if database is up to date
    bool isUpToDate() const;

    // Get current database version
    int getCurrentVersion() const;

    // Get target version (latest migration)
    int getTargetVersion() const;

    // Validate migration integrity
    bool validateMigrations() const;

    // Add a new migration
    void addMigration(const Migration& migration);

    // Get pending migrations
    std::vector<Migration> getPendingMigrations() const;

    // Get applied migrations
    std::vector<int> getAppliedMigrations() const;

private:
    DatabaseManager& m_dbManager;
    std::vector<Migration> m_migrations;
    int m_currentVersion;

    bool createMigrationTable();
    bool recordMigration(const Migration& migration);
    bool removeMigrationRecord(int version);
    std::string calculateChecksum(const Migration& migration) const;
    bool executeMigration(const Migration& migration, bool isRollback = false);
    void loadRegisteredMigrations();
};

} // namespace Database
} // namespace CryptoClaude