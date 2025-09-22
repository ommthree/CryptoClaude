#pragma once
#include "../Models/MarketData.h"
#include "../DatabaseManager.h"
#include <vector>
#include <optional>
#include <chrono>
#include <string>
#include <memory>
#include <map>

namespace CryptoClaude {
namespace Database {
namespace Repositories {

using CryptoClaude::Database::StatementWrapper;

class MarketDataRepository {
private:
    DatabaseManager& dbManager_;

    // Prepared statement cache for bulk operations
    mutable std::map<std::string, std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>> preparedStatements_;

    // Helper methods
    sqlite3_stmt* getOrCreatePreparedStatement(const std::string& key, const std::string& sql) const;
    Models::MarketData mapRowToMarketData(StatementWrapper& stmt) const;
    Models::MarketCapData mapRowToMarketCap(StatementWrapper& stmt) const;
    Models::LiquidityLambda mapRowToLiquidityLambda(StatementWrapper& stmt) const;

    // Transaction wrapper (automatic transaction handling)
    template<typename Func>
    bool executeInTransaction(Func operation, const std::string& operationName = "");

    // Error handling
    void logError(const std::string& operation, const std::string& error) const;
    std::string formatError(const std::string& operation, const std::string& detail) const;

public:
    explicit MarketDataRepository(DatabaseManager& dbManager);
    ~MarketDataRepository();

    // === CORE CRUD OPERATIONS ===

    // Basic insertion
    bool insertMarketData(const Models::MarketData& data);
    std::optional<Models::MarketData> getMarketDataById(int64_t time, const std::string& symbol);
    bool updateMarketData(const Models::MarketData& data);
    bool deleteMarketData(int64_t time, const std::string& symbol);
    bool deleteMarketData(const std::string& symbol, const std::chrono::system_clock::time_point& timestamp);

    // Additional methods used in implementation
    std::vector<Models::MarketData> getMarketDataBySymbol(const std::string& symbol, int limit = 0);
    std::optional<Models::MarketData> getLatestMarketData(const std::string& symbol);
    std::vector<Models::MarketData> getTopVolumeForDate(const std::string& date, int limit);
    bool insertMarketCapData(const Models::MarketCapData& data);

    // === ESSENTIAL DOMAIN METHODS (moderate specialization) ===

    // Frequently used queries optimized in SQL
    std::vector<Models::MarketData> getBySymbol(const std::string& symbol, int limit = 100);
    std::vector<Models::MarketData> getByDateRange(
        const std::chrono::system_clock::time_point& start,
        const std::chrono::system_clock::time_point& end);
    std::vector<Models::MarketData> getLatestForAllSymbols();
    std::optional<Models::MarketData> getLatestForSymbol(const std::string& symbol);

    // Aggregation methods (expensive in C++, efficient in SQL)
    std::vector<std::pair<std::string, double>> getTopVolumeSymbols(int limit);
    double getAverageVolumeForSymbol(const std::string& symbol, int days);

    // Complex joins (leverage data integration)
    std::vector<Models::MarketData> getMarketDataWithSentiment(const std::string& symbol);

    // === BULK OPERATIONS (prepared statement caching) ===

    // Bulk operations with prepared statement reuse
    bool insertBatch(const std::vector<Models::MarketData>& dataList);
    bool updateBatch(const std::vector<Models::MarketData>& dataList);

    // === MARKET CAP OPERATIONS ===

    bool insertMarketCap(const Models::MarketCapData& data);
    std::optional<Models::MarketCapData> getMarketCap(const std::string& symbol);
    bool updateMarketCap(const Models::MarketCapData& data);
    bool insertMarketCapBatch(const std::vector<Models::MarketCapData>& dataList);
    std::vector<std::string> getTopCoinsByMarketCap(int limit);

    // === LIQUIDITY LAMBDA OPERATIONS ===

    bool insertLiquidityLambda(const Models::LiquidityLambda& data);
    std::optional<Models::LiquidityLambda> getLiquidityLambda(const std::string& symbol);
    bool updateLiquidityLambda(const Models::LiquidityLambda& data);
    std::vector<Models::LiquidityLambda> getLiquidityLambdas();
    std::vector<Models::LiquidityLambda> getFilteredLiquidityLambdas();

    // === HOURLY DATA OPERATIONS ===

    bool insertHourlyData(const Models::MarketData& data);
    bool insertHourlyDataBatch(const std::vector<Models::MarketData>& dataList);
    std::vector<Models::MarketData> getHourlyData(
        const std::string& symbol,
        const std::chrono::system_clock::time_point& start = {},
        const std::chrono::system_clock::time_point& end = {});

    // === ANALYTICS & DERIVED DATA ===

    // Strategic SQL operations for performance
    bool updateDerivedColumns();
    bool computeDailyInflowFromHourly();
    bool mergeHourlyWithDaily();

    // Market analysis methods
    std::vector<std::string> getSymbolsWithUnusualVolume(double volumeThreshold = 2.0);
    std::vector<Models::MarketData> getSymbolsWithSentimentAbove(double threshold);

    // === UTILITY & MAINTENANCE ===

    bool removeStaleData(const std::chrono::system_clock::time_point& cutoffDate);
    bool optimizeIndexes();
    size_t getRecordCount();
    std::vector<std::string> getAllSymbols();

    // === ERROR REPORTING ===

    std::string getLastError() const { return lastError_; }
    void clearLastError() { lastError_.clear(); }

private:
    mutable std::string lastError_; // Detailed error for debugging

    // Utility functions
    int64_t timeToUnix(const std::chrono::system_clock::time_point& tp) const;
    std::chrono::system_clock::time_point unixToTime(int64_t unixTime) const;

    // SQL constants for prepared statements
    static const std::string SQL_INSERT_MARKET_DATA;
    static const std::string SQL_SELECT_BY_SYMBOL;
    static const std::string SQL_SELECT_LATEST_FOR_SYMBOL;
    static const std::string SQL_SELECT_TOP_VOLUME;
    static const std::string SQL_UPDATE_MARKET_DATA;
    static const std::string SQL_INSERT_MARKET_CAP;
    static const std::string SQL_INSERT_LIQUIDITY_LAMBDA;
};

// Template implementation for transaction wrapper
template<typename Func>
bool MarketDataRepository::executeInTransaction(Func operation, const std::string& operationName) {
    if (!dbManager_.beginTransaction()) {
        logError(operationName.empty() ? "Transaction" : operationName,
                "Failed to begin transaction: " + dbManager_.getLastError());
        return false;
    }

    try {
        bool result = operation();

        if (result) {
            if (!dbManager_.commitTransaction()) {
                logError(operationName.empty() ? "Transaction" : operationName,
                        "Failed to commit transaction: " + dbManager_.getLastError());
                dbManager_.rollbackTransaction();
                return false;
            }
        } else {
            dbManager_.rollbackTransaction();
        }

        return result;

    } catch (const std::exception& e) {
        dbManager_.rollbackTransaction();
        logError(operationName.empty() ? "Transaction" : operationName,
                "Exception during transaction: " + std::string(e.what()));
        return false;
    }
}

} // namespace Repositories
} // namespace Database
} // namespace CryptoClaude