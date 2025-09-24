#pragma once
#include "../Models/PortfolioData.h"
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

class PortfolioRepository {
private:
    DatabaseManager& dbManager_;

    // Prepared statement cache for bulk operations
    mutable std::map<std::string, std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>> preparedStatements_;

    // Helper methods
    sqlite3_stmt* getOrCreatePreparedStatement(const std::string& key, const std::string& sql) const;
    Models::Portfolio mapRowToPortfolio(StatementWrapper& stmt) const;
    Models::Position mapRowToPosition(StatementWrapper& stmt) const;
    Models::BacktestResult mapRowToBacktestResult(StatementWrapper& stmt) const;

    // Transaction wrapper (automatic transaction handling)
    template<typename Func>
    bool executeInTransaction(Func operation, const std::string& operationName = "");

    // Error handling
    void logError(const std::string& operation, const std::string& error) const;
    std::string formatError(const std::string& operation, const std::string& detail) const;

public:
    explicit PortfolioRepository(DatabaseManager& dbManager);
    ~PortfolioRepository();

    // === PORTFOLIO OPERATIONS ===

    // Basic CRUD
    bool insertPortfolio(const Models::Portfolio& portfolio);
    std::optional<Models::Portfolio> getPortfolioById(int portfolioId);
    bool updatePortfolio(const Models::Portfolio& portfolio);
    bool deletePortfolio(int portfolioId);

    // Essential domain methods
    std::vector<Models::Portfolio> getPortfoliosByStrategy(const std::string& strategyName);
    std::optional<Models::Portfolio> getLatestPortfolio(const std::string& strategyName);

    // Leverage management methods (SQL-optimized for debugging)
    Models::Portfolio getPortfolioWithLeverageMetrics(int portfolioId);
    std::vector<Models::Portfolio> getPortfoliosRequiringMarginCall();
    std::vector<Models::Portfolio> getPortfoliosWithStopLossTriggered();
    double getTotalMarginUsedByPortfolio(int portfolioId);
    double getMaxLeverageUsedByPortfolio(int portfolioId);

    // === POSITION OPERATIONS ===

    // Basic CRUD
    bool insertPosition(const Models::Position& position);
    std::optional<Models::Position> getPositionById(int positionId);
    bool updatePosition(const Models::Position& position);
    bool deletePosition(int positionId);

    // Essential domain methods
    std::vector<Models::Position> getPositionsByPortfolio(int portfolioId);
    std::vector<Models::Position> getOpenPositionsByPortfolio(int portfolioId);
    std::optional<Models::Position> getPositionBySymbol(int portfolioId, const std::string& symbol);

    // Leverage and risk management (SQL-optimized)
    std::vector<Models::Position> getPositionsRequiringMarginCall(int portfolioId);
    std::vector<Models::Position> getPositionsWithStopLossTriggered(int portfolioId);
    std::vector<Models::Position> getStopLossPositions(int portfolioId);
    std::vector<Models::Position> getPositionsShouldLiquidation(int portfolioId);
    double getTotalPnLByPortfolio(int portfolioId);
    double getTotalMarginRequirementByPortfolio(int portfolioId);

    // Position analysis
    std::vector<Models::Position> getLongPositions(int portfolioId);
    std::vector<Models::Position> getShortPositions(int portfolioId);
    std::vector<Models::Position> getProfitablePositions(int portfolioId);
    std::vector<Models::Position> getLosingPositions(int portfolioId);

    // === BULK OPERATIONS ===

    bool insertPositionBatch(const std::vector<Models::Position>& positions);
    bool updatePositionBatch(const std::vector<Models::Position>& positions);
    bool updatePortfolioBatch(const std::vector<Models::Portfolio>& portfolios);

    // Bulk position operations for rebalancing
    bool closeAllPositions(int portfolioId, const std::string& reason = "");
    bool liquidatePositions(const std::vector<int>& positionIds, const std::string& reason = "");

    // === BACKTEST RESULTS OPERATIONS ===

    // Basic CRUD
    bool insertBacktestResult(const Models::BacktestResult& result);
    std::optional<Models::BacktestResult> getBacktestResultById(int resultId);
    bool updateBacktestResult(const Models::BacktestResult& result);
    bool deleteBacktestResult(int resultId);

    // Analysis methods
    std::vector<Models::BacktestResult> getBacktestResultsByStrategy(const std::string& strategyName);
    std::optional<Models::BacktestResult> getBestBacktestResult(const std::string& metric = "total_return");
    std::vector<Models::BacktestResult> getBacktestResultsWithLeverage(double minLeverage = 1.0);

    // === PORTFOLIO ANALYSIS & REPORTING ===

    // Portfolio performance metrics (calculated in SQL for persistence/debugging)
    struct PortfolioMetrics {
        double totalValue;
        double totalPnL;
        double currentLeverage;
        double marginUtilization;
        int openPositions;
        int profitablePositions;
        double winRate;
        double maxDrawdown;
        bool requiresMarginCall;
        bool stopLossTriggered;
    };

    PortfolioMetrics getPortfolioMetrics(int portfolioId);
    std::vector<PortfolioMetrics> getAllPortfolioMetrics();

    // Risk analysis
    struct RiskMetrics {
        double var95; // Value at Risk 95%
        double var99; // Value at Risk 99%
        double expectedShortfall;
        double leverageRisk;
        double concentrationRisk;
        double correlationRisk;
        double portfolioVolatility;
        double diversificationRatio;
        double maxSinglePositionRisk;
        int marginCallCount;
        int liquidationCount;
        std::vector<std::pair<std::string, double>> topRiskyPositions;
    };

    RiskMetrics calculateRiskMetrics(int portfolioId);

    // Advanced risk query methods
    std::vector<Models::Position> getHighVolatilityPositions(int portfolioId, double volatilityThreshold = 0.5);
    std::vector<Models::Position> getPositionsAboveVaRLimit(int portfolioId, double varLimit = 0.05);
    std::vector<Models::Position> getConcentratedPositions(int portfolioId, double concentrationThreshold = 0.1);

    // Risk monitoring queries
    std::vector<Models::Portfolio> getPortfoliosWithHighRisk(double riskScoreThreshold = 75.0);
    std::vector<Models::Position> getPositionsRequiringRiskReview(int portfolioId);

    // Generate comprehensive risk report
    Models::RiskReport generateRiskReport(int portfolioId);

    // Risk historical data
    bool storeRiskSnapshot(int portfolioId, const Models::RiskReport& report);
    std::vector<Models::RiskReport> getRiskHistory(int portfolioId, int days = 30);

    // === SPECIALIZED QUERIES FOR TRADING SYSTEM ===

    // Portfolio rebalancing support
    std::vector<Models::Position> getPositionsForRebalancing(int portfolioId);
    double getAvailableCapacityForNewPositions(int portfolioId);
    bool canAccommodateNewPosition(int portfolioId, double marginRequired);

    // Margin monitoring
    std::vector<Models::Portfolio> getPortfoliosAboveMarginThreshold(double threshold = 0.80);
    std::vector<Models::Position> getPositionsNearStopLoss(double threshold = 0.02); // 2% from stop

    // Performance tracking
    std::vector<std::pair<std::string, double>> getTopPerformingStrategies(int limit = 10);
    std::vector<std::pair<std::string, double>> getWorstPerformingStrategies(int limit = 10);

    // === UTILITY & MAINTENANCE ===

    bool archiveOldPortfolios(const std::chrono::system_clock::time_point& cutoffDate);
    bool optimizePortfolioTables();
    size_t getPortfolioCount();
    size_t getPositionCount();
    std::vector<std::string> getAllStrategyNames();

    // === ERROR REPORTING ===

    std::string getLastError() const { return lastError_; }
    void clearLastError() { lastError_.clear(); }

private:
    mutable std::string lastError_; // Detailed error for debugging

    // Utility functions
    int64_t timeToUnix(const std::chrono::system_clock::time_point& tp) const;
    std::chrono::system_clock::time_point unixToTime(int64_t unixTime) const;

    // SQL constants for prepared statements
    static const std::string SQL_INSERT_PORTFOLIO;
    static const std::string SQL_INSERT_POSITION;
    static const std::string SQL_SELECT_PORTFOLIO_BY_ID;
    static const std::string SQL_SELECT_POSITIONS_BY_PORTFOLIO;
    static const std::string SQL_UPDATE_PORTFOLIO;
    static const std::string SQL_UPDATE_POSITION;
    static const std::string SQL_SELECT_PORTFOLIO_METRICS;
    static const std::string SQL_SELECT_MARGIN_CALL_PORTFOLIOS;
    static const std::string SQL_SELECT_STOP_LOSS_POSITIONS;
    static const std::string SQL_INSERT_BACKTEST_RESULT;

    // Advanced risk query SQL constants
    static const std::string SQL_SELECT_HIGH_VOLATILITY_POSITIONS;
    static const std::string SQL_SELECT_HIGH_VAR_POSITIONS;
    static const std::string SQL_SELECT_CONCENTRATED_POSITIONS;
    static const std::string SQL_SELECT_HIGH_RISK_PORTFOLIOS;
    static const std::string SQL_INSERT_RISK_SNAPSHOT;
    static const std::string SQL_SELECT_RISK_HISTORY;
};

// Template implementation for transaction wrapper
template<typename Func>
bool PortfolioRepository::executeInTransaction(Func operation, const std::string& operationName) {
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