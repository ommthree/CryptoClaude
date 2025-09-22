#include "DatabaseManager.h"
#include "../../Configuration/Config.h"
#include <iostream>
#include <sstream>

namespace CryptoClaude {
namespace Database {

DatabaseManager& DatabaseManager::getInstance() {
    static DatabaseManager instance;
    return instance;
}

DatabaseManager::~DatabaseManager() {
    close();
}

bool DatabaseManager::initialize(const std::string& dbPath) {
    if (m_isConnected) {
        close();
    }

    int result = sqlite3_open(dbPath.c_str(), &m_db);
    if (result != SQLITE_OK) {
        m_lastError = "Failed to open database: " + std::string(sqlite3_errmsg(m_db));
        sqlite3_close(m_db);
        m_db = nullptr;
        return false;
    }

    m_isConnected = true;

    // Enable foreign key support
    executeQuery("PRAGMA foreign_keys = ON;");

    // Create tables if they don't exist
    return createTables();
}

void DatabaseManager::close() {
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
    m_isConnected = false;
}

bool DatabaseManager::executeQuery(const std::string& sql) {
    if (!m_isConnected) {
        m_lastError = "Database not connected";
        return false;
    }

    char* errorMsg = nullptr;
    int result = sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &errorMsg);

    if (result != SQLITE_OK) {
        m_lastError = "SQL execution failed: " + std::string(errorMsg);
        sqlite3_free(errorMsg);
        return false;
    }

    return true;
}

sqlite3_stmt* DatabaseManager::prepareStatement(const std::string& sql) {
    if (!m_isConnected) {
        return nullptr;
    }

    sqlite3_stmt* stmt;
    int result = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);

    if (result != SQLITE_OK) {
        m_lastError = "Failed to prepare statement: " + std::string(sqlite3_errmsg(m_db));
        return nullptr;
    }

    return stmt;
}

void DatabaseManager::finalizeStatement(sqlite3_stmt* stmt) {
    if (stmt) {
        sqlite3_finalize(stmt);
    }
}

bool DatabaseManager::beginTransaction() {
    return executeQuery("BEGIN TRANSACTION;");
}

bool DatabaseManager::commitTransaction() {
    return executeQuery("COMMIT;");
}

bool DatabaseManager::rollbackTransaction() {
    return executeQuery("ROLLBACK;");
}

std::string DatabaseManager::getLastError() const {
    return m_lastError;
}

bool DatabaseManager::isConnected() const {
    return m_isConnected;
}

bool DatabaseManager::createTables() {
    return createMarketDataTables() &&
           createSentimentTables() &&
           createPortfolioTables() &&
           createModelTables();
}

bool DatabaseManager::createMarketDataTables() {
    std::string marketDataTable = R"(
        CREATE TABLE IF NOT EXISTS market_data (
            symbol TEXT,
            timestamp INTEGER,
            date TEXT,
            close_price REAL,
            volume_from REAL,
            volume_to REAL,
            net_inflow REAL,
            excess_inflow REAL,
            hourly_inflow REAL,
            day_of_week INTEGER,
            month_of_year INTEGER,
            article_count INTEGER DEFAULT 0,
            average_sentiment REAL DEFAULT 0.0,
            rsi REAL DEFAULT 0.0,
            macd REAL DEFAULT 0.0,
            bollinger_position REAL DEFAULT 0.0,
            PRIMARY KEY (timestamp, symbol)
        )
    )";

    std::string hourlyDataTable = R"(
        CREATE TABLE IF NOT EXISTS hourly_data (
            time INTEGER,
            date TEXT,
            symbol TEXT,
            close REAL,
            volumefrom REAL,
            volumeto REAL,
            time_readable TEXT,
            net_inflow REAL,
            excess_inflow REAL,
            day_of_week INTEGER,
            month_of_year INTEGER,
            PRIMARY KEY (time, symbol)
        )
    )";

    std::string marketCapTable = R"(
        CREATE TABLE IF NOT EXISTS market_cap (
            symbol TEXT PRIMARY KEY,
            market_cap REAL
        )
    )";

    std::string liquidityLambdaTable = R"(
        CREATE TABLE IF NOT EXISTS liquidity_lambda (
            symbol TEXT PRIMARY KEY,
            lambda_250 REAL,
            lambda_250_500 REAL,
            r_squared_250 REAL
        )
    )";

    std::string filteredLiquidityTable = R"(
        CREATE TABLE IF NOT EXISTS filtered_liquidity_lambda (
            symbol TEXT PRIMARY KEY,
            lambda_250 REAL,
            lambda_250_500 REAL,
            r_squared_250 REAL
        )
    )";

    return executeQuery(marketDataTable) &&
           executeQuery(hourlyDataTable) &&
           executeQuery(marketCapTable) &&
           executeQuery(liquidityLambdaTable) &&
           executeQuery(filteredLiquidityTable);
}

bool DatabaseManager::createSentimentTables() {
    std::string newsSourcesTable = R"(
        CREATE TABLE IF NOT EXISTS news_sources (
            source_id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            urlMap TEXT,
            priority INTEGER DEFAULT 1
        )
    )";

    std::string sentimentTable = R"(
        CREATE TABLE IF NOT EXISTS temp_news_sentiment (
            source_name TEXT,
            article_count INTEGER,
            avg_sentiment REAL,
            date TEXT,
            ticker TEXT
        )
    )";

    std::string dateMappingTable = R"(
        CREATE TABLE IF NOT EXISTS news_date_mapping (
            api_date TEXT PRIMARY KEY,
            sql_date TEXT UNIQUE
        )
    )";

    std::string sentimentDataTable = R"(
        CREATE TABLE IF NOT EXISTS sentiment_data (
            ticker TEXT,
            source_name TEXT,
            date TEXT,
            article_count INTEGER DEFAULT 1,
            avg_sentiment REAL DEFAULT 0.0,
            timestamp INTEGER,
            sentiment_1d REAL DEFAULT 0.0,
            sentiment_7d_avg REAL DEFAULT 0.0,
            sentiment_trend REAL DEFAULT 0.0,
            confidence REAL DEFAULT 0.0,
            PRIMARY KEY (ticker, source_name, date)
        )
    )";

    std::string newsArticlesTable = R"(
        CREATE TABLE IF NOT EXISTS news_articles (
            article_id INTEGER PRIMARY KEY AUTOINCREMENT,
            source_id INTEGER,
            ticker TEXT,
            title TEXT,
            content TEXT,
            url TEXT UNIQUE,
            published_at INTEGER,
            sentiment_score REAL DEFAULT 0.0,
            confidence REAL DEFAULT 0.0,
            processed INTEGER DEFAULT 0,
            FOREIGN KEY (source_id) REFERENCES news_sources(source_id)
        )
    )";

    std::string aggregatedSentimentTable = R"(
        CREATE TABLE IF NOT EXISTS aggregated_sentiment (
            ticker TEXT,
            date TEXT,
            avg_sentiment REAL,
            sentiment_volatility REAL DEFAULT 0.0,
            total_articles INTEGER DEFAULT 0,
            positive_count INTEGER DEFAULT 0,
            negative_count INTEGER DEFAULT 0,
            neutral_count INTEGER DEFAULT 0,
            weighted_sentiment REAL DEFAULT 0.0,
            PRIMARY KEY (ticker, date)
        )
    )";

    return executeQuery(newsSourcesTable) &&
           executeQuery(sentimentTable) &&
           executeQuery(dateMappingTable) &&
           executeQuery(sentimentDataTable) &&
           executeQuery(newsArticlesTable) &&
           executeQuery(aggregatedSentimentTable);
}

bool DatabaseManager::createPortfolioTables() {
    std::string portfolioTable = R"(
        CREATE TABLE IF NOT EXISTS portfolios (
            portfolio_id INTEGER PRIMARY KEY AUTOINCREMENT,
            strategy_name TEXT NOT NULL,
            timestamp INTEGER,
            total_value REAL,
            total_pnl REAL,
            cash_balance REAL,
            -- Leverage fields
            current_leverage REAL DEFAULT 1.0,
            max_allowed_leverage REAL DEFAULT 3.0,
            margin_used REAL DEFAULT 0.0,
            available_margin REAL DEFAULT 0.0,
            margin_utilization REAL DEFAULT 0.0,
            -- Risk management fields
            portfolio_stop_level REAL DEFAULT -0.15,
            stop_loss_triggered INTEGER DEFAULT 0,
            max_drawdown_limit REAL DEFAULT -0.15
        )
    )";

    std::string positionsTable = R"(
        CREATE TABLE IF NOT EXISTS positions (
            position_id INTEGER PRIMARY KEY AUTOINCREMENT,
            portfolio_id INTEGER,
            symbol TEXT,
            quantity REAL,
            entry_price REAL,
            current_price REAL,
            entry_time INTEGER,
            is_long INTEGER,
            pnl REAL,
            -- Leverage fields
            margin_requirement REAL DEFAULT 0.0,
            leverage_ratio REAL DEFAULT 1.0,
            -- Risk management fields
            stop_loss_price REAL DEFAULT 0.0,
            position_stop_triggered INTEGER DEFAULT 0,
            initial_margin REAL DEFAULT 0.0,
            FOREIGN KEY (portfolio_id) REFERENCES portfolios(portfolio_id)
        )
    )";

    std::string brokerAccountsTable = R"(
        CREATE TABLE IF NOT EXISTS broker_accounts (
            account_id TEXT PRIMARY KEY,
            broker_name TEXT NOT NULL,
            api_endpoint TEXT,
            account_status TEXT DEFAULT 'active',
            available_balance REAL DEFAULT 0.0,
            margin_balance REAL DEFAULT 0.0,
            buying_power REAL DEFAULT 0.0,
            maintenance_margin REAL DEFAULT 0.0,
            created_time INTEGER DEFAULT (strftime('%s', 'now'))
        )
    )";

    std::string ordersTable = R"(
        CREATE TABLE IF NOT EXISTS orders (
            order_id TEXT PRIMARY KEY,
            portfolio_id INTEGER,
            symbol TEXT NOT NULL,
            order_type TEXT NOT NULL,
            side TEXT NOT NULL,
            quantity REAL NOT NULL,
            price REAL DEFAULT 0.0,
            stop_price REAL DEFAULT 0.0,
            status TEXT DEFAULT 'pending',
            broker_order_id TEXT,
            created_time INTEGER DEFAULT (strftime('%s', 'now')),
            filled_time INTEGER DEFAULT 0,
            FOREIGN KEY (portfolio_id) REFERENCES portfolios(portfolio_id)
        )
    )";

    std::string backtestResultsTable = R"(
        CREATE TABLE IF NOT EXISTS backtest_results (
            result_id INTEGER PRIMARY KEY AUTOINCREMENT,
            strategy_name TEXT,
            start_date INTEGER,
            end_date INTEGER,
            initial_capital REAL,
            final_value REAL,
            total_return REAL,
            sharpe_ratio REAL,
            max_drawdown REAL,
            total_trades INTEGER,
            win_rate REAL,
            -- NEW: Leverage metrics
            max_leverage_used REAL DEFAULT 1.0,
            avg_leverage REAL DEFAULT 1.0,
            margin_calls_count INTEGER DEFAULT 0,
            forced_liquidations INTEGER DEFAULT 0
        )
    )";

    return executeQuery(portfolioTable) &&
           executeQuery(positionsTable) &&
           executeQuery(brokerAccountsTable) &&
           executeQuery(ordersTable) &&
           executeQuery(backtestResultsTable);
}

bool DatabaseManager::createModelTables() {
    std::string forestInputTable = R"(
        CREATE TABLE IF NOT EXISTS forest_input (
            symbol TEXT,
            date TEXT,
            sentiment_1d REAL,
            sentiment_7d_avg REAL,
            inflow_gradient REAL,
            inflow_100d_avg REAL,
            target_f REAL
        )
    )";

    std::string diagnosticsTable = R"(
        CREATE TABLE IF NOT EXISTS rf_diagnostics (
            symbol TEXT,
            date TEXT,
            actual REAL,
            predicted REAL,
            abs_error REAL,
            feature_1 REAL,
            feature_2 REAL,
            feature_3 REAL,
            feature_4 REAL
        )
    )";

    return executeQuery(forestInputTable) &&
           executeQuery(diagnosticsTable);
}

bool DatabaseManager::tableExists(const std::string& tableName) {
    std::string sql = "SELECT name FROM sqlite_master WHERE type='table' AND name=?";
    auto stmt = prepareStatement(sql);
    if (!stmt) {
        return false;
    }

    StatementWrapper wrapper(stmt);
    wrapper.bindString(1, tableName);

    return wrapper.step();
}

bool DatabaseManager::dropAllTables() {
    std::vector<std::string> tables = {
        "market_data", "hourly_data", "market_cap", "liquidity_lambda",
        "filtered_liquidity_lambda", "news_sources", "temp_news_sentiment",
        "news_date_mapping", "portfolios", "positions", "backtest_results",
        "forest_input", "rf_diagnostics"
    };

    for (const auto& table : tables) {
        std::string dropSQL = "DROP TABLE IF EXISTS " + table;
        if (!executeQuery(dropSQL)) {
            return false;
        }
    }
    return true;
}

// StatementWrapper implementation
StatementWrapper::StatementWrapper(sqlite3_stmt* stmt) : m_stmt(stmt) {}

StatementWrapper::~StatementWrapper() {
    if (m_stmt) {
        sqlite3_finalize(m_stmt);
    }
}

bool StatementWrapper::bindString(int index, const std::string& value) {
    return sqlite3_bind_text(m_stmt, index, value.c_str(), -1, SQLITE_STATIC) == SQLITE_OK;
}

bool StatementWrapper::bindDouble(int index, double value) {
    return sqlite3_bind_double(m_stmt, index, value) == SQLITE_OK;
}

bool StatementWrapper::bindInt(int index, int value) {
    return sqlite3_bind_int(m_stmt, index, value) == SQLITE_OK;
}

bool StatementWrapper::bindNull(int index) {
    return sqlite3_bind_null(m_stmt, index) == SQLITE_OK;
}

bool StatementWrapper::step() {
    int result = sqlite3_step(m_stmt);
    return result == SQLITE_ROW;
}

int StatementWrapper::getColumnCount() const {
    return sqlite3_column_count(m_stmt);
}

std::string StatementWrapper::getStringColumn(int index) const {
    const char* text = reinterpret_cast<const char*>(sqlite3_column_text(m_stmt, index));
    return text ? text : "";
}

double StatementWrapper::getDoubleColumn(int index) const {
    return sqlite3_column_double(m_stmt, index);
}

int StatementWrapper::getIntColumn(int index) const {
    return sqlite3_column_int(m_stmt, index);
}

bool StatementWrapper::isColumnNull(int index) const {
    return sqlite3_column_type(m_stmt, index) == SQLITE_NULL;
}

} // namespace Database
} // namespace CryptoClaude