#include "MarketDataRepository.h"
#include <iostream>

namespace CryptoClaude {
namespace Database {
namespace Repositories {

// Constructor
MarketDataRepository::MarketDataRepository(DatabaseManager& dbManager)
    : dbManager_(dbManager) {
}

// Destructor
MarketDataRepository::~MarketDataRepository() {
    // Clean up prepared statements
    for (auto& pair : preparedStatements_) {
        // The unique_ptr will automatically call sqlite3_finalize
    }
    preparedStatements_.clear();
}

// Helper methods (minimal implementations for compilation)
void MarketDataRepository::logError(const std::string& operation, const std::string& error) const {
    lastError_ = formatError(operation, error);
    std::cerr << "MarketDataRepository Error: " << lastError_ << std::endl;
}

std::string MarketDataRepository::formatError(const std::string& operation, const std::string& detail) const {
    return operation + ": " + detail;
}

sqlite3_stmt* MarketDataRepository::getOrCreatePreparedStatement(const std::string& key, const std::string& sql) const {
    // Stub implementation - would cache prepared statements in real implementation
    return nullptr;
}

Models::MarketData MarketDataRepository::mapRowToMarketData(StatementWrapper& stmt) const {
    // Create market data with basic constructor
    Models::MarketData data(
        stmt.getStringColumn(0),    // symbol
        unixToTime(stmt.getIntColumn(1)),  // timestamp
        stmt.getDoubleColumn(3),    // close_price
        stmt.getDoubleColumn(4),    // volume_from
        stmt.getDoubleColumn(5)     // volume_to
    );

    // Set calculated inflow data
    data.setNetInflow(stmt.getDoubleColumn(6));
    data.setExcessInflow(stmt.getDoubleColumn(7));
    data.setHourlyInflow(stmt.getDoubleColumn(8));

    // Set sentiment data if present
    if (stmt.getIntColumn(9) > 0) {
        data.setArticleCount(stmt.getIntColumn(9));
        if (!stmt.isColumnNull(10)) {
            data.setAverageSentiment(stmt.getDoubleColumn(10));
        }
    }

    // Set technical indicators if calculated
    if (!stmt.isColumnNull(11)) data.setRSI(stmt.getDoubleColumn(11));
    if (!stmt.isColumnNull(12)) data.setMACD(stmt.getDoubleColumn(12));
    if (!stmt.isColumnNull(13)) data.setBollingerPosition(stmt.getDoubleColumn(13));

    return data;
}

Models::MarketCapData MarketDataRepository::mapRowToMarketCap(StatementWrapper& stmt) const {
    return Models::MarketCapData(
        stmt.getStringColumn(0),    // symbol
        stmt.getDoubleColumn(1)     // market_cap
    );
}

Models::LiquidityLambda MarketDataRepository::mapRowToLiquidityLambda(StatementWrapper& stmt) const {
    return Models::LiquidityLambda(
        stmt.getStringColumn(0),    // symbol
        stmt.getDoubleColumn(1),    // lambda250
        stmt.getDoubleColumn(2),    // lambda250_500
        stmt.getDoubleColumn(3)     // r_squared250
    );
}

int64_t MarketDataRepository::timeToUnix(const std::chrono::system_clock::time_point& tp) const {
    return std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch()).count();
}

std::chrono::system_clock::time_point MarketDataRepository::unixToTime(int64_t unixTime) const {
    return std::chrono::system_clock::from_time_t(static_cast<time_t>(unixTime));
}

// SQL constants (real implementations)
const std::string MarketDataRepository::SQL_INSERT_MARKET_DATA = R"(
    INSERT INTO market_data (
        symbol, timestamp, date, close_price, volume_from, volume_to,
        net_inflow, excess_inflow, hourly_inflow, day_of_week, month_of_year,
        article_count, average_sentiment, rsi, macd, bollinger_position
    ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
)";

const std::string MarketDataRepository::SQL_SELECT_BY_SYMBOL = R"(
    SELECT symbol, timestamp, date, close_price, volume_from, volume_to,
           net_inflow, excess_inflow, hourly_inflow, article_count, average_sentiment,
           rsi, macd, bollinger_position
    FROM market_data
    WHERE symbol = ?
    ORDER BY timestamp DESC
)";

const std::string MarketDataRepository::SQL_SELECT_LATEST_FOR_SYMBOL = R"(
    SELECT symbol, timestamp, date, close_price, volume_from, volume_to,
           net_inflow, excess_inflow, hourly_inflow, article_count, average_sentiment,
           rsi, macd, bollinger_position
    FROM market_data
    WHERE symbol = ?
    ORDER BY timestamp DESC
    LIMIT 1
)";

const std::string MarketDataRepository::SQL_SELECT_TOP_VOLUME = R"(
    SELECT symbol, timestamp, date, close_price, volume_from, volume_to,
           net_inflow, excess_inflow, hourly_inflow, article_count, average_sentiment,
           rsi, macd, bollinger_position,
           (volume_from + volume_to) as total_volume
    FROM market_data
    WHERE date = ?
    ORDER BY total_volume DESC
    LIMIT ?
)";

const std::string MarketDataRepository::SQL_UPDATE_MARKET_DATA = R"(
    UPDATE market_data SET
        close_price = ?, volume_from = ?, volume_to = ?,
        net_inflow = ?, excess_inflow = ?, hourly_inflow = ?,
        article_count = ?, average_sentiment = ?,
        rsi = ?, macd = ?, bollinger_position = ?
    WHERE symbol = ? AND timestamp = ?
)";

const std::string MarketDataRepository::SQL_INSERT_MARKET_CAP = R"(
    INSERT OR REPLACE INTO market_cap_data (symbol, market_cap)
    VALUES (?, ?)
)";

const std::string MarketDataRepository::SQL_INSERT_LIQUIDITY_LAMBDA = R"(
    INSERT OR REPLACE INTO liquidity_lambda (symbol, lambda250, lambda250_500, r_squared250)
    VALUES (?, ?, ?, ?)
)";

// === MARKET DATA CRUD IMPLEMENTATIONS ===

bool MarketDataRepository::insertMarketData(const Models::MarketData& data) {
    return executeInTransaction([&]() {
        auto stmt = dbManager_.prepareStatement(SQL_INSERT_MARKET_DATA);
        if (!stmt) {
            logError("insertMarketData", "Failed to prepare statement");
            return false;
        }

        StatementWrapper wrapper(stmt);

        // Bind market data
        wrapper.bindString(1, data.getSymbol());
        wrapper.bindInt(2, static_cast<int>(timeToUnix(data.getTimestamp())));
        wrapper.bindString(3, data.getDate());
        wrapper.bindDouble(4, data.getClose());
        wrapper.bindDouble(5, data.getVolumeFrom());
        wrapper.bindDouble(6, data.getVolumeTo());
        wrapper.bindDouble(7, data.getNetInflow());
        wrapper.bindDouble(8, data.getExcessInflow());
        wrapper.bindDouble(9, data.getHourlyInflow());
        wrapper.bindInt(10, data.getDayOfWeek());
        wrapper.bindInt(11, data.getMonthOfYear());
        wrapper.bindInt(12, data.getArticleCount());

        if (data.getAverageSentiment().has_value()) {
            wrapper.bindDouble(13, data.getAverageSentiment().value());
        } else {
            wrapper.bindNull(13);
        }

        if (data.getRSI().has_value()) {
            wrapper.bindDouble(14, data.getRSI().value());
        } else {
            wrapper.bindNull(14);
        }

        if (data.getMACD().has_value()) {
            wrapper.bindDouble(15, data.getMACD().value());
        } else {
            wrapper.bindNull(15);
        }

        if (data.getBollingerPosition().has_value()) {
            wrapper.bindDouble(16, data.getBollingerPosition().value());
        } else {
            wrapper.bindNull(16);
        }

        return sqlite3_step(stmt) == SQLITE_DONE;
    }, "insertMarketData");
}

std::vector<Models::MarketData> MarketDataRepository::getMarketDataBySymbol(const std::string& symbol, int limit) {
    std::vector<Models::MarketData> results;

    std::string sql = SQL_SELECT_BY_SYMBOL;
    if (limit > 0) {
        sql += " LIMIT " + std::to_string(limit);
    }

    auto stmt = dbManager_.prepareStatement(sql);
    if (!stmt) {
        logError("getMarketDataBySymbol", "Failed to prepare statement");
        return results;
    }

    StatementWrapper wrapper(stmt);
    wrapper.bindString(1, symbol);

    while (wrapper.step()) {
        results.push_back(mapRowToMarketData(wrapper));
    }

    return results;
}

std::optional<Models::MarketData> MarketDataRepository::getLatestMarketData(const std::string& symbol) {
    auto stmt = dbManager_.prepareStatement(SQL_SELECT_LATEST_FOR_SYMBOL);
    if (!stmt) {
        logError("getLatestMarketData", "Failed to prepare statement");
        return std::nullopt;
    }

    StatementWrapper wrapper(stmt);
    wrapper.bindString(1, symbol);

    if (!wrapper.step()) {
        return std::nullopt;
    }

    return mapRowToMarketData(wrapper);
}

std::vector<Models::MarketData> MarketDataRepository::getTopVolumeForDate(const std::string& date, int limit) {
    std::vector<Models::MarketData> results;

    auto stmt = dbManager_.prepareStatement(SQL_SELECT_TOP_VOLUME);
    if (!stmt) {
        logError("getTopVolumeForDate", "Failed to prepare statement");
        return results;
    }

    StatementWrapper wrapper(stmt);
    wrapper.bindString(1, date);
    wrapper.bindInt(2, limit);

    while (wrapper.step()) {
        results.push_back(mapRowToMarketData(wrapper));
    }

    return results;
}

bool MarketDataRepository::updateMarketData(const Models::MarketData& data) {
    return executeInTransaction([&]() {
        auto stmt = dbManager_.prepareStatement(SQL_UPDATE_MARKET_DATA);
        if (!stmt) {
            logError("updateMarketData", "Failed to prepare statement");
            return false;
        }

        StatementWrapper wrapper(stmt);

        // Bind updated data
        wrapper.bindDouble(1, data.getClose());
        wrapper.bindDouble(2, data.getVolumeFrom());
        wrapper.bindDouble(3, data.getVolumeTo());
        wrapper.bindDouble(4, data.getNetInflow());
        wrapper.bindDouble(5, data.getExcessInflow());
        wrapper.bindDouble(6, data.getHourlyInflow());
        wrapper.bindInt(7, data.getArticleCount());

        if (data.getAverageSentiment().has_value()) {
            wrapper.bindDouble(8, data.getAverageSentiment().value());
        } else {
            wrapper.bindNull(8);
        }

        if (data.getRSI().has_value()) {
            wrapper.bindDouble(9, data.getRSI().value());
        } else {
            wrapper.bindNull(9);
        }

        if (data.getMACD().has_value()) {
            wrapper.bindDouble(10, data.getMACD().value());
        } else {
            wrapper.bindNull(10);
        }

        if (data.getBollingerPosition().has_value()) {
            wrapper.bindDouble(11, data.getBollingerPosition().value());
        } else {
            wrapper.bindNull(11);
        }

        // WHERE clause
        wrapper.bindString(12, data.getSymbol());
        wrapper.bindInt(13, static_cast<int>(timeToUnix(data.getTimestamp())));

        return sqlite3_step(stmt) == SQLITE_DONE;
    }, "updateMarketData");
}

bool MarketDataRepository::deleteMarketData(const std::string& symbol, const std::chrono::system_clock::time_point& timestamp) {
    return executeInTransaction([&]() {
        std::string deleteSQL = "DELETE FROM market_data WHERE symbol = ? AND timestamp = ?";
        auto stmt = dbManager_.prepareStatement(deleteSQL);
        if (!stmt) {
            logError("deleteMarketData", "Failed to prepare statement");
            return false;
        }

        StatementWrapper wrapper(stmt);
        wrapper.bindString(1, symbol);
        wrapper.bindInt(2, static_cast<int>(timeToUnix(timestamp)));

        return sqlite3_step(stmt) == SQLITE_DONE;
    }, "deleteMarketData");
}

// === MARKET CAP DATA METHODS ===

bool MarketDataRepository::insertMarketCapData(const Models::MarketCapData& data) {
    return executeInTransaction([&]() {
        auto stmt = dbManager_.prepareStatement(SQL_INSERT_MARKET_CAP);
        if (!stmt) {
            logError("insertMarketCapData", "Failed to prepare statement");
            return false;
        }

        StatementWrapper wrapper(stmt);
        wrapper.bindString(1, data.getSymbol());
        wrapper.bindDouble(2, data.getMarketCap());

        return sqlite3_step(stmt) == SQLITE_DONE;
    }, "insertMarketCapData");
}

// === LIQUIDITY LAMBDA DATA METHODS ===

bool MarketDataRepository::insertLiquidityLambda(const Models::LiquidityLambda& data) {
    return executeInTransaction([&]() {
        auto stmt = dbManager_.prepareStatement(SQL_INSERT_LIQUIDITY_LAMBDA);
        if (!stmt) {
            logError("insertLiquidityLambda", "Failed to prepare statement");
            return false;
        }

        StatementWrapper wrapper(stmt);
        wrapper.bindString(1, data.getSymbol());
        wrapper.bindDouble(2, data.getLambda250());
        wrapper.bindDouble(3, data.getLambda250_500());
        wrapper.bindDouble(4, data.getRSquared250());

        return sqlite3_step(stmt) == SQLITE_DONE;
    }, "insertLiquidityLambda");
}

} // namespace Repositories
} // namespace Database
} // namespace CryptoClaude