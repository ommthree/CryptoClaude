#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <stdexcept>
#include <optional>
#include <cmath>
#include "../../../Configuration/Config.h"

namespace CryptoClaude {
namespace Database {
namespace Models {

class Position {
private:
    int positionId_;
    int portfolioId_;
    std::string symbol_;
    double quantity_;
    double entryPrice_;
    double currentPrice_;
    std::chrono::system_clock::time_point entryTime_;
    bool isLong_;
    double pnl_;

    // Leverage fields
    double marginRequirement_;
    double leverageRatio_;

    // Risk management fields
    double stopLossPrice_;
    bool positionStopTriggered_;
    double initialMargin_;

    void validateQuantity(double qty) const {
        if (std::isnan(qty) || std::isinf(qty)) {
            throw std::invalid_argument("Quantity cannot be NaN or infinite");
        }
        if (qty == 0.0) {
            throw std::invalid_argument("Quantity cannot be zero");
        }
    }

    void validatePrice(double price, const std::string& fieldName) const {
        if (std::isnan(price) || std::isinf(price)) {
            throw std::invalid_argument(fieldName + " cannot be NaN or infinite");
        }
        if (price <= 0) {
            throw std::invalid_argument(fieldName + " must be positive");
        }
    }

    void validateLeverage(double leverage) const {
        if (std::isnan(leverage) || std::isinf(leverage)) {
            throw std::invalid_argument("Leverage cannot be NaN or infinite");
        }
        if (leverage < 1.0) {
            throw std::invalid_argument("Leverage cannot be less than 1.0");
        }
        if (leverage > Config::MAX_LEVERAGE_LIMIT) {
            throw std::invalid_argument("Leverage exceeds maximum limit of " +
                                      std::to_string(Config::MAX_LEVERAGE_LIMIT));
        }
    }

    void calculatePnL() {
        if (isLong_) {
            pnl_ = quantity_ * (currentPrice_ - entryPrice_);
        } else {
            pnl_ = quantity_ * (entryPrice_ - currentPrice_);
        }
    }

    void calculateMarginRequirement() {
        double positionValue = std::abs(quantity_) * entryPrice_;
        marginRequirement_ = positionValue / leverageRatio_;
    }

public:
    Position() = default;

    Position(const std::string& sym, double qty, double entry, bool long_position, double leverage = 1.0)
        : symbol_(sym), quantity_(qty), entryPrice_(entry), currentPrice_(entry),
          entryTime_(std::chrono::system_clock::now()), isLong_(long_position),
          leverageRatio_(leverage), positionStopTriggered_(false) {

        validateSymbol(sym);
        validateQuantity(qty);
        validatePrice(entry, "Entry price");
        validateLeverage(leverage);

        calculateMarginRequirement();
        calculatePnL();
        initialMargin_ = marginRequirement_;
        stopLossPrice_ = isLong_ ? entry * (1.0 + Config::POSITION_STOP_LOSS)
                                : entry * (1.0 - Config::POSITION_STOP_LOSS);
    }

    void validateSymbol(const std::string& sym) const {
        if (sym.empty()) {
            throw std::invalid_argument("Symbol cannot be empty");
        }
        if (sym.length() > 20) {
            throw std::invalid_argument("Symbol too long (max 20 characters)");
        }
    }

    // Getters
    int getPositionId() const { return positionId_; }
    int getPortfolioId() const { return portfolioId_; }
    const std::string& getSymbol() const { return symbol_; }
    double getQuantity() const { return quantity_; }
    double getEntryPrice() const { return entryPrice_; }
    double getCurrentPrice() const { return currentPrice_; }
    const std::chrono::system_clock::time_point& getEntryTime() const { return entryTime_; }
    bool isLong() const { return isLong_; }
    double getPnL() const { return pnl_; }
    double getMarginRequirement() const { return marginRequirement_; }
    double getLeverageRatio() const { return leverageRatio_; }
    double getStopLossPrice() const { return stopLossPrice_; }
    bool isStopLossTriggered() const { return positionStopTriggered_; }
    double getInitialMargin() const { return initialMargin_; }

    // Setters with validation
    void setPositionId(int id) { positionId_ = id; }
    void setPortfolioId(int id) { portfolioId_ = id; }

    void setSymbol(const std::string& sym) {
        validateSymbol(sym);
        symbol_ = sym;
    }

    void setQuantity(double qty) {
        validateQuantity(qty);
        quantity_ = qty;
        calculateMarginRequirement();
        calculatePnL();
    }

    void setCurrentPrice(double price) {
        validatePrice(price, "Current price");
        currentPrice_ = price;
        calculatePnL();

        // Check stop-loss trigger
        if (isLong_ && currentPrice_ <= stopLossPrice_) {
            positionStopTriggered_ = true;
        } else if (!isLong_ && currentPrice_ >= stopLossPrice_) {
            positionStopTriggered_ = true;
        }
    }

    void setLeverageRatio(double leverage) {
        validateLeverage(leverage);
        leverageRatio_ = leverage;
        calculateMarginRequirement();
    }

    void setStopLossPrice(double stopPrice) {
        validatePrice(stopPrice, "Stop-loss price");
        stopLossPrice_ = stopPrice;
    }

    void triggerStopLoss() {
        positionStopTriggered_ = true;
    }

    void resetStopLoss() {
        positionStopTriggered_ = false;
    }

    // Business logic methods
    double getPositionValue() const {
        return std::abs(quantity_) * currentPrice_;
    }

    double getPnLPercentage() const {
        double initialValue = std::abs(quantity_) * entryPrice_;
        return initialValue > 0 ? (pnl_ / initialValue) * 100.0 : 0.0;
    }

    double getLeveragedPnL() const {
        return pnl_ * leverageRatio_;
    }

    bool requiresMarginCall() const {
        // Position requires margin call if losses approach initial margin
        double lossThreshold = initialMargin_ * 0.8; // 80% of initial margin
        return pnl_ < -lossThreshold;
    }

    bool shouldLiquidatePosition() const {
        // Liquidate if losses exceed initial margin
        return pnl_ < -initialMargin_;
    }
};

class Portfolio {
private:
    int portfolioId_;
    std::string strategyName_;
    std::chrono::system_clock::time_point timestamp_;
    double totalValue_;
    double totalPnL_;
    double cashBalance_;

    // Leverage fields
    double currentLeverage_;
    double maxAllowedLeverage_;
    double marginUsed_;
    double availableMargin_;
    double marginUtilization_;

    // Risk management fields
    double portfolioStopLevel_;
    bool stopLossTriggered_;
    double maxDrawdownLimit_;

    void validateValue(double value, const std::string& fieldName) const {
        if (std::isnan(value) || std::isinf(value)) {
            throw std::invalid_argument(fieldName + " cannot be NaN or infinite");
        }
    }

    void validateLeverage(double leverage) const {
        if (std::isnan(leverage) || std::isinf(leverage)) {
            throw std::invalid_argument("Leverage cannot be NaN or infinite");
        }
        if (leverage < 1.0) {
            throw std::invalid_argument("Leverage cannot be less than 1.0");
        }
        if (leverage > Config::MAX_LEVERAGE_LIMIT) {
            throw std::invalid_argument("Leverage exceeds maximum limit");
        }
    }

    void validateStopLevel(double level) const {
        if (std::isnan(level) || std::isinf(level)) {
            throw std::invalid_argument("Stop level cannot be NaN or infinite");
        }
        if (level >= 0.0) {
            throw std::invalid_argument("Stop level must be negative (percentage loss)");
        }
        if (level < -1.0) {
            throw std::invalid_argument("Stop level cannot exceed -100%");
        }
    }

public:
    Portfolio() = default;

    Portfolio(const std::string& name, double initialCash, double maxLeverage = Config::DEFAULT_MAX_LEVERAGE)
        : strategyName_(name), timestamp_(std::chrono::system_clock::now()),
          totalValue_(initialCash), totalPnL_(0.0), cashBalance_(initialCash),
          currentLeverage_(1.0), maxAllowedLeverage_(maxLeverage),
          marginUsed_(0.0), marginUtilization_(0.0),
          portfolioStopLevel_(Config::PORTFOLIO_STOP_LOSS),
          stopLossTriggered_(false), maxDrawdownLimit_(Config::PORTFOLIO_STOP_LOSS) {

        validateStrategyName(name);
        validateValue(initialCash, "Initial cash");
        validateLeverage(maxLeverage);

        availableMargin_ = initialCash * maxLeverage;
    }

    void validateStrategyName(const std::string& name) const {
        if (name.empty()) {
            throw std::invalid_argument("Strategy name cannot be empty");
        }
        if (name.length() > 100) {
            throw std::invalid_argument("Strategy name too long (max 100 characters)");
        }
    }

    // Getters
    int getPortfolioId() const { return portfolioId_; }
    const std::string& getStrategyName() const { return strategyName_; }
    const std::chrono::system_clock::time_point& getTimestamp() const { return timestamp_; }
    double getTotalValue() const { return totalValue_; }
    double getTotalPnL() const { return totalPnL_; }
    double getCashBalance() const { return cashBalance_; }
    double getCurrentLeverage() const { return currentLeverage_; }
    double getMaxAllowedLeverage() const { return maxAllowedLeverage_; }
    double getMarginUsed() const { return marginUsed_; }
    double getAvailableMargin() const { return availableMargin_; }
    double getMarginUtilization() const { return marginUtilization_; }
    double getPortfolioStopLevel() const { return portfolioStopLevel_; }
    bool isStopLossTriggered() const { return stopLossTriggered_; }
    double getMaxDrawdownLimit() const { return maxDrawdownLimit_; }

    // Setters with validation
    void setPortfolioId(int id) { portfolioId_ = id; }

    void setStrategyName(const std::string& name) {
        validateStrategyName(name);
        strategyName_ = name;
    }

    void setTimestamp(const std::chrono::system_clock::time_point& timestamp) {
        timestamp_ = timestamp;
    }

    void setTotalValue(double value) {
        validateValue(value, "Total value");
        totalValue_ = value;
        updateLeverageMetrics();
    }

    void setTotalPnL(double pnl) {
        validateValue(pnl, "Total PnL");
        totalPnL_ = pnl;
        checkStopLossConditions();
    }

    void setCashBalance(double balance) {
        validateValue(balance, "Cash balance");
        cashBalance_ = balance;
    }

    void setMaxAllowedLeverage(double leverage) {
        validateLeverage(leverage);
        maxAllowedLeverage_ = leverage;
        availableMargin_ = (cashBalance_ + totalValue_) * leverage;
        updateLeverageMetrics();
    }

    void setMarginUsed(double margin) {
        validateValue(margin, "Margin used");
        marginUsed_ = margin;
        updateLeverageMetrics();
    }

    void setPortfolioStopLevel(double level) {
        validateStopLevel(level);
        portfolioStopLevel_ = level;
    }

    void setMaxDrawdownLimit(double limit) {
        validateStopLevel(limit);
        maxDrawdownLimit_ = limit;
    }

    // Leverage management
    void updateLeverageMetrics() {
        if (totalValue_ > 0) {
            currentLeverage_ = (totalValue_ + marginUsed_) / totalValue_;
        } else {
            currentLeverage_ = 1.0;
        }

        if (availableMargin_ > 0) {
            marginUtilization_ = marginUsed_ / availableMargin_;
        } else {
            marginUtilization_ = 0.0;
        }
    }

    bool canTakePosition(double marginRequired) const {
        double newMarginUsed = marginUsed_ + marginRequired;
        double newUtilization = newMarginUsed / availableMargin_;
        return newUtilization <= Config::MARGIN_UTILIZATION_LIMIT;
    }

    double getMaxPositionSize(double leverage) const {
        double availableMarginForNewPosition = availableMargin_ * Config::MARGIN_UTILIZATION_LIMIT - marginUsed_;
        return availableMarginForNewPosition * leverage;
    }

    void addMarginUsage(double marginRequired) {
        if (!canTakePosition(marginRequired)) {
            throw std::runtime_error("Insufficient margin for position");
        }
        marginUsed_ += marginRequired;
        updateLeverageMetrics();
    }

    void removeMarginUsage(double marginToFree) {
        marginUsed_ = std::max(0.0, marginUsed_ - marginToFree);
        updateLeverageMetrics();
    }

    // Risk management
    void checkStopLossConditions() {
        double portfolioReturn = (totalValue_ - (cashBalance_ + marginUsed_)) / (cashBalance_ + marginUsed_);

        if (portfolioReturn <= portfolioStopLevel_) {
            stopLossTriggered_ = true;
        }

        if (portfolioReturn <= maxDrawdownLimit_) {
            stopLossTriggered_ = true;
        }
    }

    bool requiresMarginCall() const {
        return marginUtilization_ > Config::MARGIN_CALL_THRESHOLD;
    }

    bool shouldForceClosePositions() const {
        return marginUtilization_ > 0.95; // Emergency threshold
    }

    void triggerStopLoss() {
        stopLossTriggered_ = true;
    }

    void resetStopLoss() {
        stopLossTriggered_ = false;
    }

    // Business logic
    double getEquityValue() const {
        return totalValue_ - marginUsed_;
    }

    double getFreeMargin() const {
        return availableMargin_ - marginUsed_;
    }

    double getLeverageCapacity() const {
        return getFreeMargin() / totalValue_;
    }
};

class BacktestResult {
private:
    int resultId_;
    std::string strategyName_;
    std::chrono::system_clock::time_point startDate_;
    std::chrono::system_clock::time_point endDate_;
    double initialCapital_;
    double finalValue_;
    double totalReturn_;
    double sharpeRatio_;
    double maxDrawdown_;
    int totalTrades_;
    double winRate_;

    // Leverage metrics
    double maxLeverageUsed_;
    double avgLeverage_;
    int marginCallsCount_;
    int forcedLiquidations_;

public:
    BacktestResult() = default;

    BacktestResult(const std::string& strategy, double initialCap)
        : strategyName_(strategy), initialCapital_(initialCap),
          finalValue_(initialCap), totalReturn_(0.0), sharpeRatio_(0.0),
          maxDrawdown_(0.0), totalTrades_(0), winRate_(0.0),
          maxLeverageUsed_(1.0), avgLeverage_(1.0),
          marginCallsCount_(0), forcedLiquidations_(0) {}

    // Getters
    int getResultId() const { return resultId_; }
    const std::string& getStrategyName() const { return strategyName_; }
    const std::chrono::system_clock::time_point& getStartDate() const { return startDate_; }
    const std::chrono::system_clock::time_point& getEndDate() const { return endDate_; }
    double getInitialCapital() const { return initialCapital_; }
    double getFinalValue() const { return finalValue_; }
    double getTotalReturn() const { return totalReturn_; }
    double getSharpeRatio() const { return sharpeRatio_; }
    double getMaxDrawdown() const { return maxDrawdown_; }
    int getTotalTrades() const { return totalTrades_; }
    double getWinRate() const { return winRate_; }
    double getMaxLeverageUsed() const { return maxLeverageUsed_; }
    double getAvgLeverage() const { return avgLeverage_; }
    int getMarginCallsCount() const { return marginCallsCount_; }
    int getForcedLiquidations() const { return forcedLiquidations_; }

    // Setters
    void setResultId(int id) { resultId_ = id; }
    void setStrategyName(const std::string& name) { strategyName_ = name; }
    void setStartDate(const std::chrono::system_clock::time_point& date) { startDate_ = date; }
    void setEndDate(const std::chrono::system_clock::time_point& date) { endDate_ = date; }
    void setInitialCapital(double capital) { initialCapital_ = capital; }
    void setFinalValue(double value) { finalValue_ = value; calculateTotalReturn(); }
    void setSharpeRatio(double ratio) { sharpeRatio_ = ratio; }
    void setMaxDrawdown(double drawdown) { maxDrawdown_ = drawdown; }
    void setTotalTrades(int trades) { totalTrades_ = trades; }
    void setWinRate(double rate) { winRate_ = rate; }
    void setMaxLeverageUsed(double leverage) { maxLeverageUsed_ = leverage; }
    void setAvgLeverage(double leverage) { avgLeverage_ = leverage; }
    void setMarginCallsCount(int count) { marginCallsCount_ = count; }
    void setForcedLiquidations(int count) { forcedLiquidations_ = count; }

private:
    void calculateTotalReturn() {
        if (initialCapital_ > 0) {
            totalReturn_ = ((finalValue_ - initialCapital_) / initialCapital_) * 100.0;
        }
    }
};

} // namespace Models
} // namespace Database
} // namespace CryptoClaude