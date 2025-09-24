#pragma once
#include <string>
#include <memory>
#include <vector>
#include <sqlite3.h>

namespace CryptoClaude {
namespace Database {

class MigrationManager;
class DataQualityManager;

class DatabaseManager {
public:
    static DatabaseManager& getInstance();

    bool initialize(const std::string& dbPath);
    void close();

    bool executeQuery(const std::string& sql);
    bool executeQuery(const std::string& sql,
                      const std::vector<std::string>& params);

    sqlite3_stmt* prepareStatement(const std::string& sql);
    void finalizeStatement(sqlite3_stmt* stmt);

    bool beginTransaction();
    bool commitTransaction();
    bool rollbackTransaction();

    std::string getLastError() const;
    bool isConnected() const;

    // Schema management
    bool createTables();
    bool dropAllTables();
    bool tableExists(const std::string& tableName);

    // Enhanced database management (VE005, VE004)
    bool initializeWithMigrations(const std::string& dbPath);
    MigrationManager& getMigrationManager();
    DataQualityManager& getDataQualityManager();

    // Direct SQLite access for migrations
    sqlite3* getSqliteHandle() { return m_db; }

private:
    DatabaseManager() = default;
    ~DatabaseManager();

    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    sqlite3* m_db = nullptr;
    std::string m_lastError;
    bool m_isConnected = false;

    // Enhancement managers
    std::unique_ptr<MigrationManager> m_migrationManager;
    std::unique_ptr<DataQualityManager> m_dataQualityManager;

    bool createMarketDataTables();
    bool createSentimentTables();
    bool createPortfolioTables();
    bool createModelTables();
};

// RAII wrapper for SQLite statements
class StatementWrapper {
public:
    explicit StatementWrapper(sqlite3_stmt* stmt);
    ~StatementWrapper();

    sqlite3_stmt* get() { return m_stmt; }
    bool isValid() const { return m_stmt != nullptr; }

    bool bindString(int index, const std::string& value);
    bool bindDouble(int index, double value);
    bool bindInt(int index, int value);
    bool bindNull(int index);

    bool step();
    int getColumnCount() const;
    std::string getStringColumn(int index) const;
    double getDoubleColumn(int index) const;
    int getIntColumn(int index) const;
    bool isColumnNull(int index) const;

private:
    sqlite3_stmt* m_stmt;
};

} // namespace Database
} // namespace CryptoClaude