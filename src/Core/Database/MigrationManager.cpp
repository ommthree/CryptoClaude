#include "MigrationManager.h"
#include "DatabaseManager.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace CryptoClaude {
namespace Database {

MigrationManager::MigrationManager(DatabaseManager& dbManager)
    : m_dbManager(dbManager), m_currentVersion(0) {
    loadRegisteredMigrations();
}

bool MigrationManager::initialize() {
    if (!createMigrationTable()) {
        std::cerr << "Failed to create migration table" << std::endl;
        return false;
    }

    // Determine current database version
    std::string sql = "SELECT MAX(version) FROM schema_migrations WHERE applied = 1";
    auto stmt = m_dbManager.prepareStatement(sql);
    if (!stmt) {
        m_currentVersion = 0;
        return true;
    }

    StatementWrapper wrapper(stmt);
    if (wrapper.step()) {
        if (!wrapper.isColumnNull(0)) {
            m_currentVersion = wrapper.getIntColumn(0);
        }
    }

    return true;
}

bool MigrationManager::runMigrations() {
    auto pendingMigrations = getPendingMigrations();
    if (pendingMigrations.empty()) {
        std::cout << "Database is up to date (version " << m_currentVersion << ")" << std::endl;
        return true;
    }

    std::cout << "Running " << pendingMigrations.size() << " pending migrations..." << std::endl;

    for (const auto& migration : pendingMigrations) {
        std::cout << "Applying migration " << migration.version << ": " << migration.name << std::endl;

        if (!executeMigration(migration)) {
            std::cerr << "Failed to apply migration " << migration.version << std::endl;
            return false;
        }

        if (!recordMigration(migration)) {
            std::cerr << "Failed to record migration " << migration.version << std::endl;
            return false;
        }

        m_currentVersion = migration.version;
        std::cout << "Migration " << migration.version << " applied successfully" << std::endl;
    }

    std::cout << "All migrations completed. Database version: " << m_currentVersion << std::endl;
    return true;
}

bool MigrationManager::rollbackToVersion(int targetVersion) {
    if (targetVersion >= m_currentVersion) {
        std::cout << "Target version " << targetVersion << " is not older than current version "
                  << m_currentVersion << std::endl;
        return true;
    }

    auto appliedMigrations = getAppliedMigrations();
    std::sort(appliedMigrations.rbegin(), appliedMigrations.rend()); // Sort descending

    std::cout << "Rolling back from version " << m_currentVersion << " to " << targetVersion << std::endl;

    for (int version : appliedMigrations) {
        if (version <= targetVersion) {
            break;
        }

        // Find the migration to rollback
        auto it = std::find_if(m_migrations.begin(), m_migrations.end(),
            [version](const Migration& m) { return m.version == version; });

        if (it == m_migrations.end()) {
            std::cerr << "Migration " << version << " not found for rollback" << std::endl;
            return false;
        }

        std::cout << "Rolling back migration " << version << ": " << it->name << std::endl;

        if (!executeMigration(*it, true)) {
            std::cerr << "Failed to rollback migration " << version << std::endl;
            return false;
        }

        if (!removeMigrationRecord(version)) {
            std::cerr << "Failed to remove migration record " << version << std::endl;
            return false;
        }

        std::cout << "Migration " << version << " rolled back successfully" << std::endl;
    }

    m_currentVersion = targetVersion;
    std::cout << "Rollback completed. Database version: " << m_currentVersion << std::endl;
    return true;
}

bool MigrationManager::isUpToDate() const {
    return getCurrentVersion() == getTargetVersion();
}

int MigrationManager::getCurrentVersion() const {
    return m_currentVersion;
}

int MigrationManager::getTargetVersion() const {
    if (m_migrations.empty()) {
        return 0;
    }

    int maxVersion = 0;
    for (const auto& migration : m_migrations) {
        maxVersion = std::max(maxVersion, migration.version);
    }
    return maxVersion;
}

bool MigrationManager::validateMigrations() const {
    // Check for duplicate versions
    std::vector<int> versions;
    for (const auto& migration : m_migrations) {
        if (std::find(versions.begin(), versions.end(), migration.version) != versions.end()) {
            std::cerr << "Duplicate migration version: " << migration.version << std::endl;
            return false;
        }
        versions.push_back(migration.version);
    }

    // Validate checksums of applied migrations
    auto appliedMigrations = getAppliedMigrations();
    for (int version : appliedMigrations) {
        auto it = std::find_if(m_migrations.begin(), m_migrations.end(),
            [version](const Migration& m) { return m.version == version; });

        if (it == m_migrations.end()) {
            std::cerr << "Applied migration " << version << " not found in registered migrations" << std::endl;
            return false;
        }

        // TODO: Implement checksum validation against database
    }

    return true;
}

void MigrationManager::addMigration(const Migration& migration) {
    m_migrations.push_back(migration);
    std::sort(m_migrations.begin(), m_migrations.end(),
        [](const Migration& a, const Migration& b) { return a.version < b.version; });
}

std::vector<Migration> MigrationManager::getPendingMigrations() const {
    std::vector<Migration> pending;
    auto appliedMigrations = getAppliedMigrations();

    for (const auto& migration : m_migrations) {
        if (std::find(appliedMigrations.begin(), appliedMigrations.end(), migration.version)
            == appliedMigrations.end()) {
            pending.push_back(migration);
        }
    }

    return pending;
}

std::vector<int> MigrationManager::getAppliedMigrations() const {
    std::vector<int> applied;

    std::string sql = "SELECT version FROM schema_migrations WHERE applied = 1 ORDER BY version";
    auto stmt = m_dbManager.prepareStatement(sql);
    if (!stmt) {
        return applied;
    }

    StatementWrapper wrapper(stmt);
    while (wrapper.step()) {
        applied.push_back(wrapper.getIntColumn(0));
    }

    return applied;
}

bool MigrationManager::createMigrationTable() {
    std::string sql = R"(
        CREATE TABLE IF NOT EXISTS schema_migrations (
            version INTEGER PRIMARY KEY,
            name TEXT NOT NULL,
            description TEXT,
            checksum TEXT,
            applied INTEGER DEFAULT 0,
            applied_at INTEGER DEFAULT (strftime('%s', 'now')),
            execution_time_ms INTEGER DEFAULT 0
        )
    )";

    return m_dbManager.executeQuery(sql);
}

bool MigrationManager::recordMigration(const Migration& migration) {
    std::string sql = R"(
        INSERT OR REPLACE INTO schema_migrations
        (version, name, description, checksum, applied, applied_at)
        VALUES (?, ?, ?, ?, 1, strftime('%s', 'now'))
    )";

    auto stmt = m_dbManager.prepareStatement(sql);
    if (!stmt) {
        return false;
    }

    StatementWrapper wrapper(stmt);
    wrapper.bindInt(1, migration.version);
    wrapper.bindString(2, migration.name);
    wrapper.bindString(3, migration.description);
    wrapper.bindString(4, migration.checksum);

    // Execute the prepared statement, not the raw SQL
    return wrapper.execute();
}

bool MigrationManager::removeMigrationRecord(int version) {
    std::string sql = "DELETE FROM schema_migrations WHERE version = ?";

    auto stmt = m_dbManager.prepareStatement(sql);
    if (!stmt) {
        return false;
    }

    StatementWrapper wrapper(stmt);
    wrapper.bindInt(1, version);

    // Execute the prepared statement, not the raw SQL
    return wrapper.execute();
}

std::string MigrationManager::calculateChecksum(const Migration& migration) const {
    // Simple checksum based on version and name
    std::stringstream ss;
    ss << migration.version << "|" << migration.name << "|" << migration.description;

    // TODO: Implement proper SHA-256 checksum
    return ss.str();
}

bool MigrationManager::executeMigration(const Migration& migration, bool isRollback) {
    if (!m_dbManager.beginTransaction()) {
        return false;
    }

    bool success;
    sqlite3* dbHandle = m_dbManager.getSqliteHandle();

    if (isRollback && migration.down) {
        success = migration.down(dbHandle);
    } else if (!isRollback && migration.up) {
        success = migration.up(dbHandle);
    } else {
        success = false;
    }

    if (success) {
        return m_dbManager.commitTransaction();
    } else {
        m_dbManager.rollbackTransaction();
        return false;
    }
}

void MigrationManager::loadRegisteredMigrations() {
    // Migration 001: Enhanced data quality monitoring
    Migration migration001;
    migration001.version = 1;
    migration001.name = "add_data_quality_monitoring";
    migration001.description = "Add tables for data quality monitoring and remediation";
    migration001.checksum = calculateChecksum(migration001);
    migration001.up = []([[maybe_unused]] sqlite3* db) -> bool {
        const std::string sql = R"(
            CREATE TABLE IF NOT EXISTS data_quality_metrics (
                metric_id INTEGER PRIMARY KEY AUTOINCREMENT,
                table_name TEXT NOT NULL,
                column_name TEXT,
                quality_score REAL DEFAULT 0.0,
                completeness_ratio REAL DEFAULT 0.0,
                accuracy_score REAL DEFAULT 0.0,
                outlier_count INTEGER DEFAULT 0,
                total_records INTEGER DEFAULT 0,
                measurement_timestamp INTEGER DEFAULT (strftime('%s', 'now')),
                remediation_applied INTEGER DEFAULT 0,
                remediation_details TEXT
            );

            CREATE INDEX IF NOT EXISTS idx_data_quality_timestamp
            ON data_quality_metrics(measurement_timestamp);

            CREATE INDEX IF NOT EXISTS idx_data_quality_table
            ON data_quality_metrics(table_name, column_name);
        )";

        DatabaseManager& dbManager = DatabaseManager::getInstance();
        return dbManager.executeQuery(sql);
    };
    migration001.down = []([[maybe_unused]] sqlite3* db) -> bool {
        DatabaseManager& dbManager = DatabaseManager::getInstance();
        return dbManager.executeQuery("DROP TABLE IF EXISTS data_quality_metrics");
    };

    addMigration(migration001);

    // Migration 002: Cross-asset correlation tracking
    Migration migration002;
    migration002.version = 2;
    migration002.name = "add_cross_asset_correlation";
    migration002.description = "Add tables for cross-asset correlation monitoring";
    migration002.checksum = calculateChecksum(migration002);
    migration002.up = []([[maybe_unused]] sqlite3* db) -> bool {
        const std::string sql = R"(
            CREATE TABLE IF NOT EXISTS cross_asset_data (
                asset_id INTEGER PRIMARY KEY AUTOINCREMENT,
                asset_symbol TEXT NOT NULL,
                asset_type TEXT NOT NULL, -- 'crypto', 'equity', 'commodity', 'currency'
                price REAL NOT NULL,
                volume REAL DEFAULT 0.0,
                timestamp INTEGER NOT NULL,
                data_source TEXT DEFAULT 'unknown',
                UNIQUE(asset_symbol, timestamp)
            );

            CREATE TABLE IF NOT EXISTS correlation_matrix (
                correlation_id INTEGER PRIMARY KEY AUTOINCREMENT,
                asset1_symbol TEXT NOT NULL,
                asset2_symbol TEXT NOT NULL,
                correlation_coefficient REAL NOT NULL,
                calculation_period INTEGER NOT NULL, -- days
                calculation_timestamp INTEGER DEFAULT (strftime('%s', 'now')),
                sample_size INTEGER DEFAULT 0,
                p_value REAL DEFAULT 1.0,
                UNIQUE(asset1_symbol, asset2_symbol, calculation_period, calculation_timestamp)
            );

            CREATE TABLE IF NOT EXISTS market_stress_indicators (
                indicator_id INTEGER PRIMARY KEY AUTOINCREMENT,
                indicator_name TEXT NOT NULL,
                indicator_value REAL NOT NULL,
                threshold_warning REAL DEFAULT 0.0,
                threshold_critical REAL DEFAULT 0.0,
                status TEXT DEFAULT 'normal', -- 'normal', 'warning', 'critical'
                timestamp INTEGER DEFAULT (strftime('%s', 'now')),
                details TEXT
            );

            CREATE INDEX IF NOT EXISTS idx_cross_asset_timestamp
            ON cross_asset_data(timestamp);

            CREATE INDEX IF NOT EXISTS idx_correlation_timestamp
            ON correlation_matrix(calculation_timestamp);
        )";

        DatabaseManager& dbManager = DatabaseManager::getInstance();
        return dbManager.executeQuery(sql);
    };
    migration002.down = []([[maybe_unused]] sqlite3* db) -> bool {
        DatabaseManager& dbManager = DatabaseManager::getInstance();
        return dbManager.executeQuery(R"(
            DROP TABLE IF EXISTS market_stress_indicators;
            DROP TABLE IF EXISTS correlation_matrix;
            DROP TABLE IF EXISTS cross_asset_data;
        )");
    };

    addMigration(migration002);

    // Migration 003: Sentiment source diversification framework
    Migration migration003;
    migration003.version = 3;
    migration003.name = "enhance_sentiment_diversification";
    migration003.description = "Add manual override and multi-source sentiment capabilities";
    migration003.checksum = calculateChecksum(migration003);
    migration003.up = []([[maybe_unused]] sqlite3* db) -> bool {
        const std::string sql = R"(
            ALTER TABLE news_sources ADD COLUMN quality_tier TEXT DEFAULT 'general';
            ALTER TABLE news_sources ADD COLUMN reliability_score REAL DEFAULT 0.5;
            ALTER TABLE news_sources ADD COLUMN is_active INTEGER DEFAULT 1;
            ALTER TABLE news_sources ADD COLUMN backup_priority INTEGER DEFAULT 100;

            CREATE TABLE IF NOT EXISTS sentiment_overrides (
                override_id INTEGER PRIMARY KEY AUTOINCREMENT,
                symbol TEXT NOT NULL,
                manual_sentiment REAL NOT NULL,
                confidence REAL DEFAULT 0.8,
                override_reason TEXT,
                valid_from INTEGER NOT NULL,
                valid_until INTEGER NOT NULL,
                created_by TEXT DEFAULT 'system',
                created_at INTEGER DEFAULT (strftime('%s', 'now')),
                is_active INTEGER DEFAULT 1
            );

            CREATE TABLE IF NOT EXISTS multi_source_sentiment (
                record_id INTEGER PRIMARY KEY AUTOINCREMENT,
                symbol TEXT NOT NULL,
                timestamp INTEGER NOT NULL,
                primary_sentiment REAL,
                backup_sentiment REAL,
                manual_override REAL,
                final_sentiment REAL NOT NULL,
                confidence_score REAL DEFAULT 0.0,
                sources_used TEXT, -- JSON array of sources
                quality_weighted_score REAL DEFAULT 0.0,
                UNIQUE(symbol, timestamp)
            );

            CREATE INDEX IF NOT EXISTS idx_sentiment_overrides_symbol
            ON sentiment_overrides(symbol, valid_from, valid_until);
        )";

        DatabaseManager& dbManager = DatabaseManager::getInstance();
        return dbManager.executeQuery(sql);
    };
    migration003.down = []([[maybe_unused]] sqlite3* db) -> bool {
        DatabaseManager& dbManager = DatabaseManager::getInstance();
        return dbManager.executeQuery(R"(
            DROP TABLE IF EXISTS multi_source_sentiment;
            DROP TABLE IF EXISTS sentiment_overrides;
        )");
    };

    addMigration(migration003);
}

} // namespace Database
} // namespace CryptoClaude