#include "PortfolioRepository.h"
#include <iostream>

namespace CryptoClaude {
namespace Database {
namespace Repositories {

// Constructor
PortfolioRepository::PortfolioRepository(DatabaseManager& dbManager)
    : dbManager_(dbManager) {
}

// Destructor
PortfolioRepository::~PortfolioRepository() {
    // Clean up prepared statements
    for (auto& pair : preparedStatements_) {
        // The unique_ptr will automatically call sqlite3_finalize
    }
    preparedStatements_.clear();
}

// Helper methods (minimal implementations for compilation)
void PortfolioRepository::logError(const std::string& operation, const std::string& error) const {
    lastError_ = formatError(operation, error);
    std::cerr << "PortfolioRepository Error: " << lastError_ << std::endl;
}

std::string PortfolioRepository::formatError(const std::string& operation, const std::string& detail) const {
    return operation + ": " + detail;
}

sqlite3_stmt* PortfolioRepository::getOrCreatePreparedStatement(const std::string& key, const std::string& sql) const {
    // Stub implementation - would cache prepared statements in real implementation
    return nullptr;
}

Models::Portfolio PortfolioRepository::mapRowToPortfolio(StatementWrapper& stmt) const {
    // Create portfolio with basic data
    Models::Portfolio portfolio(
        stmt.getStringColumn(1),  // strategy_name
        stmt.getDoubleColumn(5),  // cash_balance (as initial cash)
        stmt.getDoubleColumn(7)   // max_allowed_leverage
    );

    // Set portfolio ID and timestamp
    portfolio.setPortfolioId(stmt.getIntColumn(0));
    portfolio.setTimestamp(unixToTime(stmt.getIntColumn(2)));

    // Set financial data
    portfolio.setTotalValue(stmt.getDoubleColumn(3));
    portfolio.setTotalPnL(stmt.getDoubleColumn(4));
    portfolio.setCashBalance(stmt.getDoubleColumn(5));

    // Set leverage data
    portfolio.setMarginUsed(stmt.getDoubleColumn(8));

    // Set risk management data
    portfolio.setPortfolioStopLevel(stmt.getDoubleColumn(11));
    portfolio.setMaxDrawdownLimit(stmt.getDoubleColumn(13));

    if (stmt.getIntColumn(12) == 1) {
        portfolio.triggerStopLoss();
    }

    return portfolio;
}

Models::Position PortfolioRepository::mapRowToPosition(StatementWrapper& stmt) const {
    // Create position with basic data
    Models::Position position(
        stmt.getStringColumn(2),  // symbol
        stmt.getDoubleColumn(3),  // quantity
        stmt.getDoubleColumn(4),  // entry_price
        stmt.getIntColumn(7) == 1, // is_long
        stmt.getDoubleColumn(10)  // leverage_ratio
    );

    // Set position and portfolio IDs
    position.setPositionId(stmt.getIntColumn(0));
    position.setPortfolioId(stmt.getIntColumn(1));

    // Set price and timing data
    position.setCurrentPrice(stmt.getDoubleColumn(5));
    // Note: entry_time is set in constructor via timestamp

    // Set calculated values (these are updated automatically in Position model)
    // pnl, margin_requirement are recalculated when current price is set

    // Set risk management data
    position.setStopLossPrice(stmt.getDoubleColumn(11));
    if (stmt.getIntColumn(12) == 1) {
        position.triggerStopLoss();
    }

    return position;
}

Models::BacktestResult PortfolioRepository::mapRowToBacktestResult(StatementWrapper& stmt) const {
    // Create backtest result with basic data
    Models::BacktestResult result(
        stmt.getStringColumn(1),  // strategy_name
        stmt.getDoubleColumn(4)   // initial_capital
    );

    // Set result ID
    result.setResultId(stmt.getIntColumn(0));

    // Set date range
    result.setStartDate(unixToTime(stmt.getIntColumn(2)));
    result.setEndDate(unixToTime(stmt.getIntColumn(3)));

    // Set performance metrics
    result.setFinalValue(stmt.getDoubleColumn(5)); // This also calculates total_return
    result.setSharpeRatio(stmt.getDoubleColumn(6));
    result.setMaxDrawdown(stmt.getDoubleColumn(7));
    result.setTotalTrades(stmt.getIntColumn(8));
    result.setWinRate(stmt.getDoubleColumn(9));

    // Set leverage metrics
    result.setMaxLeverageUsed(stmt.getDoubleColumn(10));
    result.setAvgLeverage(stmt.getDoubleColumn(11));
    result.setMarginCallsCount(stmt.getIntColumn(12));
    result.setForcedLiquidations(stmt.getIntColumn(13));

    return result;
}

int64_t PortfolioRepository::timeToUnix(const std::chrono::system_clock::time_point& tp) const {
    return std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch()).count();
}

std::chrono::system_clock::time_point PortfolioRepository::unixToTime(int64_t unixTime) const {
    return std::chrono::system_clock::from_time_t(static_cast<time_t>(unixTime));
}

// SQL constants (real implementations)
const std::string PortfolioRepository::SQL_INSERT_PORTFOLIO = R"(
    INSERT INTO portfolios (
        strategy_name, timestamp, total_value, total_pnl, cash_balance,
        current_leverage, max_allowed_leverage, margin_used, available_margin,
        margin_utilization, portfolio_stop_level, stop_loss_triggered,
        max_drawdown_limit
    ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
)";

const std::string PortfolioRepository::SQL_INSERT_POSITION = R"(
    INSERT INTO positions (
        portfolio_id, symbol, quantity, entry_price, current_price,
        entry_time, is_long, pnl, margin_requirement, leverage_ratio,
        stop_loss_price, position_stop_triggered, initial_margin
    ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
)";

const std::string PortfolioRepository::SQL_SELECT_PORTFOLIO_BY_ID = R"(
    SELECT portfolio_id, strategy_name, timestamp, total_value, total_pnl,
           cash_balance, current_leverage, max_allowed_leverage, margin_used,
           available_margin, margin_utilization, portfolio_stop_level,
           stop_loss_triggered, max_drawdown_limit
    FROM portfolios WHERE portfolio_id = ?
)";

const std::string PortfolioRepository::SQL_SELECT_POSITIONS_BY_PORTFOLIO = R"(
    SELECT position_id, portfolio_id, symbol, quantity, entry_price,
           current_price, entry_time, is_long, pnl, margin_requirement,
           leverage_ratio, stop_loss_price, position_stop_triggered,
           initial_margin
    FROM positions WHERE portfolio_id = ?
)";

const std::string PortfolioRepository::SQL_UPDATE_PORTFOLIO = R"(
    UPDATE portfolios SET
        strategy_name = ?, timestamp = ?, total_value = ?, total_pnl = ?,
        cash_balance = ?, current_leverage = ?, max_allowed_leverage = ?,
        margin_used = ?, available_margin = ?, margin_utilization = ?,
        portfolio_stop_level = ?, stop_loss_triggered = ?,
        max_drawdown_limit = ?
    WHERE portfolio_id = ?
)";

const std::string PortfolioRepository::SQL_UPDATE_POSITION = R"(
    UPDATE positions SET
        symbol = ?, quantity = ?, entry_price = ?, current_price = ?,
        entry_time = ?, is_long = ?, pnl = ?, margin_requirement = ?,
        leverage_ratio = ?, stop_loss_price = ?, position_stop_triggered = ?,
        initial_margin = ?
    WHERE position_id = ?
)";

const std::string PortfolioRepository::SQL_SELECT_PORTFOLIO_METRICS = R"(
    SELECT p.portfolio_id, p.strategy_name, p.total_value, p.total_pnl,
           p.current_leverage, p.margin_used, p.margin_utilization,
           COUNT(pos.position_id) as position_count,
           SUM(CASE WHEN pos.pnl > 0 THEN 1 ELSE 0 END) as profitable_positions
    FROM portfolios p
    LEFT JOIN positions pos ON p.portfolio_id = pos.portfolio_id
    WHERE p.portfolio_id = ?
    GROUP BY p.portfolio_id
)";

const std::string PortfolioRepository::SQL_SELECT_MARGIN_CALL_PORTFOLIOS = R"(
    SELECT portfolio_id, strategy_name, margin_utilization
    FROM portfolios
    WHERE margin_utilization > 0.90 AND stop_loss_triggered = 0
)";

const std::string PortfolioRepository::SQL_SELECT_STOP_LOSS_POSITIONS = R"(
    SELECT position_id, portfolio_id, symbol, current_price, stop_loss_price,
           is_long, position_stop_triggered
    FROM positions
    WHERE portfolio_id = ? AND position_stop_triggered = 1
)";

const std::string PortfolioRepository::SQL_INSERT_BACKTEST_RESULT = R"(
    INSERT INTO backtest_results (
        strategy_name, start_date, end_date, initial_capital, final_value,
        total_return, sharpe_ratio, max_drawdown, total_trades, win_rate,
        max_leverage_used, avg_leverage, margin_calls_count, forced_liquidations
    ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
)";

// === PORTFOLIO CRUD IMPLEMENTATIONS ===

bool PortfolioRepository::insertPortfolio(const Models::Portfolio& portfolio) {
    return executeInTransaction([&]() {
        auto stmt = dbManager_.prepareStatement(SQL_INSERT_PORTFOLIO);
        if (!stmt) {
            logError("insertPortfolio", "Failed to prepare statement");
            return false;
        }

        StatementWrapper wrapper(stmt);

        // Bind portfolio data
        wrapper.bindString(1, portfolio.getStrategyName());
        wrapper.bindInt(2, static_cast<int>(timeToUnix(portfolio.getTimestamp())));
        wrapper.bindDouble(3, portfolio.getTotalValue());
        wrapper.bindDouble(4, portfolio.getTotalPnL());
        wrapper.bindDouble(5, portfolio.getCashBalance());
        wrapper.bindDouble(6, portfolio.getCurrentLeverage());
        wrapper.bindDouble(7, portfolio.getMaxAllowedLeverage());
        wrapper.bindDouble(8, portfolio.getMarginUsed());
        wrapper.bindDouble(9, portfolio.getAvailableMargin());
        wrapper.bindDouble(10, portfolio.getMarginUtilization());
        wrapper.bindDouble(11, portfolio.getPortfolioStopLevel());
        wrapper.bindInt(12, portfolio.isStopLossTriggered() ? 1 : 0);
        wrapper.bindDouble(13, portfolio.getMaxDrawdownLimit());

        return sqlite3_step(stmt) == SQLITE_DONE;
    }, "insertPortfolio");
}

std::optional<Models::Portfolio> PortfolioRepository::getPortfolioById(int portfolioId) {
    auto stmt = dbManager_.prepareStatement(SQL_SELECT_PORTFOLIO_BY_ID);
    if (!stmt) {
        logError("getPortfolioById", "Failed to prepare statement");
        return std::nullopt;
    }

    StatementWrapper wrapper(stmt);
    wrapper.bindInt(1, portfolioId);

    if (!wrapper.step()) {
        return std::nullopt;
    }

    return mapRowToPortfolio(wrapper);
}

bool PortfolioRepository::updatePortfolio(const Models::Portfolio& portfolio) {
    return executeInTransaction([&]() {
        auto stmt = dbManager_.prepareStatement(SQL_UPDATE_PORTFOLIO);
        if (!stmt) {
            logError("updatePortfolio", "Failed to prepare statement");
            return false;
        }

        StatementWrapper wrapper(stmt);

        // Bind updated portfolio data
        wrapper.bindString(1, portfolio.getStrategyName());
        wrapper.bindInt(2, static_cast<int>(timeToUnix(portfolio.getTimestamp())));
        wrapper.bindDouble(3, portfolio.getTotalValue());
        wrapper.bindDouble(4, portfolio.getTotalPnL());
        wrapper.bindDouble(5, portfolio.getCashBalance());
        wrapper.bindDouble(6, portfolio.getCurrentLeverage());
        wrapper.bindDouble(7, portfolio.getMaxAllowedLeverage());
        wrapper.bindDouble(8, portfolio.getMarginUsed());
        wrapper.bindDouble(9, portfolio.getAvailableMargin());
        wrapper.bindDouble(10, portfolio.getMarginUtilization());
        wrapper.bindDouble(11, portfolio.getPortfolioStopLevel());
        wrapper.bindInt(12, portfolio.isStopLossTriggered() ? 1 : 0);
        wrapper.bindDouble(13, portfolio.getMaxDrawdownLimit());
        wrapper.bindInt(14, portfolio.getPortfolioId());

        return sqlite3_step(stmt) == SQLITE_DONE;
    }, "updatePortfolio");
}

bool PortfolioRepository::deletePortfolio(int portfolioId) {
    return executeInTransaction([&]() {
        // First delete all positions for this portfolio
        std::string deletePositionsSQL = "DELETE FROM positions WHERE portfolio_id = ?";
        auto stmt1 = dbManager_.prepareStatement(deletePositionsSQL);
        if (!stmt1) {
            logError("deletePortfolio", "Failed to prepare positions deletion statement");
            return false;
        }

        StatementWrapper wrapper1(stmt1);
        wrapper1.bindInt(1, portfolioId);

        if (sqlite3_step(stmt1) != SQLITE_DONE) {
            logError("deletePortfolio", "Failed to delete positions");
            return false;
        }

        // Then delete the portfolio
        std::string deletePortfolioSQL = "DELETE FROM portfolios WHERE portfolio_id = ?";
        auto stmt2 = dbManager_.prepareStatement(deletePortfolioSQL);
        if (!stmt2) {
            logError("deletePortfolio", "Failed to prepare portfolio deletion statement");
            return false;
        }

        StatementWrapper wrapper2(stmt2);
        wrapper2.bindInt(1, portfolioId);

        return sqlite3_step(stmt2) == SQLITE_DONE;
    }, "deletePortfolio");
}

// === POSITION CRUD IMPLEMENTATIONS ===

bool PortfolioRepository::insertPosition(const Models::Position& position) {
    return executeInTransaction([&]() {
        auto stmt = dbManager_.prepareStatement(SQL_INSERT_POSITION);
        if (!stmt) {
            logError("insertPosition", "Failed to prepare statement");
            return false;
        }

        StatementWrapper wrapper(stmt);

        // Bind position data
        wrapper.bindInt(1, position.getPortfolioId());
        wrapper.bindString(2, position.getSymbol());
        wrapper.bindDouble(3, position.getQuantity());
        wrapper.bindDouble(4, position.getEntryPrice());
        wrapper.bindDouble(5, position.getCurrentPrice());
        wrapper.bindInt(6, static_cast<int>(timeToUnix(position.getEntryTime())));
        wrapper.bindInt(7, position.isLong() ? 1 : 0);
        wrapper.bindDouble(8, position.getPnL());
        wrapper.bindDouble(9, position.getMarginRequirement());
        wrapper.bindDouble(10, position.getLeverageRatio());
        wrapper.bindDouble(11, position.getStopLossPrice());
        wrapper.bindInt(12, position.isStopLossTriggered() ? 1 : 0);
        wrapper.bindDouble(13, position.getInitialMargin());

        return sqlite3_step(stmt) == SQLITE_DONE;
    }, "insertPosition");
}

std::vector<Models::Position> PortfolioRepository::getPositionsByPortfolio(int portfolioId) {
    std::vector<Models::Position> positions;

    auto stmt = dbManager_.prepareStatement(SQL_SELECT_POSITIONS_BY_PORTFOLIO);
    if (!stmt) {
        logError("getPositionsByPortfolio", "Failed to prepare statement");
        return positions;
    }

    StatementWrapper wrapper(stmt);
    wrapper.bindInt(1, portfolioId);

    while (wrapper.step()) {
        positions.push_back(mapRowToPosition(wrapper));
    }

    return positions;
}

bool PortfolioRepository::updatePosition(const Models::Position& position) {
    return executeInTransaction([&]() {
        auto stmt = dbManager_.prepareStatement(SQL_UPDATE_POSITION);
        if (!stmt) {
            logError("updatePosition", "Failed to prepare statement");
            return false;
        }

        StatementWrapper wrapper(stmt);

        // Bind updated position data
        wrapper.bindString(1, position.getSymbol());
        wrapper.bindDouble(2, position.getQuantity());
        wrapper.bindDouble(3, position.getEntryPrice());
        wrapper.bindDouble(4, position.getCurrentPrice());
        wrapper.bindInt(5, static_cast<int>(timeToUnix(position.getEntryTime())));
        wrapper.bindInt(6, position.isLong() ? 1 : 0);
        wrapper.bindDouble(7, position.getPnL());
        wrapper.bindDouble(8, position.getMarginRequirement());
        wrapper.bindDouble(9, position.getLeverageRatio());
        wrapper.bindDouble(10, position.getStopLossPrice());
        wrapper.bindInt(11, position.isStopLossTriggered() ? 1 : 0);
        wrapper.bindDouble(12, position.getInitialMargin());
        wrapper.bindInt(13, position.getPositionId());

        return sqlite3_step(stmt) == SQLITE_DONE;
    }, "updatePosition");
}

bool PortfolioRepository::deletePosition(int positionId) {
    return executeInTransaction([&]() {
        std::string deleteSQL = "DELETE FROM positions WHERE position_id = ?";
        auto stmt = dbManager_.prepareStatement(deleteSQL);
        if (!stmt) {
            logError("deletePosition", "Failed to prepare statement");
            return false;
        }

        StatementWrapper wrapper(stmt);
        wrapper.bindInt(1, positionId);

        return sqlite3_step(stmt) == SQLITE_DONE;
    }, "deletePosition");
}

// === MISSING METHODS IMPLEMENTATIONS ===

std::vector<Models::Portfolio> PortfolioRepository::getPortfoliosRequiringMarginCall() {
    std::vector<Models::Portfolio> results;

    std::string sql = "SELECT portfolio_id, strategy_name, timestamp, total_value, total_pnl, "
                     "cash_balance, current_leverage, max_allowed_leverage, margin_used, "
                     "available_margin, margin_utilization, portfolio_stop_level, "
                     "stop_loss_triggered, max_drawdown_limit "
                     "FROM portfolios WHERE margin_utilization > 0.90 AND stop_loss_triggered = 0";

    auto stmt = dbManager_.prepareStatement(sql);
    if (!stmt) {
        logError("getPortfoliosRequiringMarginCall", "Failed to prepare statement");
        return results;
    }

    StatementWrapper wrapper(stmt);
    while (wrapper.step()) {
        results.push_back(mapRowToPortfolio(wrapper));
    }

    return results;
}

std::vector<Models::Position> PortfolioRepository::getStopLossPositions(int portfolioId) {
    std::vector<Models::Position> results;

    std::string sql = "SELECT position_id, portfolio_id, symbol, quantity, entry_price, "
                     "current_price, entry_time, is_long, pnl, margin_requirement, "
                     "leverage_ratio, stop_loss_price, position_stop_triggered, "
                     "initial_margin "
                     "FROM positions WHERE portfolio_id = ? AND position_stop_triggered = 1";

    auto stmt = dbManager_.prepareStatement(sql);
    if (!stmt) {
        logError("getStopLossPositions", "Failed to prepare statement");
        return results;
    }

    StatementWrapper wrapper(stmt);
    wrapper.bindInt(1, portfolioId);

    while (wrapper.step()) {
        results.push_back(mapRowToPosition(wrapper));
    }

    return results;
}

} // namespace Repositories
} // namespace Database
} // namespace CryptoClaude