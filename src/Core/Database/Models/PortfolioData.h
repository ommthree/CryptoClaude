#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <stdexcept>
#include <optional>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <map>
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

    // Advanced risk metrics
    double volatility_;
    double beta_;
    double deltaEquivalent_;
    std::vector<double> priceHistory_;
    std::chrono::system_clock::time_point lastPriceUpdate_;
    double maxPriceDrawdown_;
    double timeDecayRisk_;

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
          leverageRatio_(leverage), positionStopTriggered_(false),
          volatility_(0.0), beta_(1.0), deltaEquivalent_(0.0),
          lastPriceUpdate_(std::chrono::system_clock::now()),
          maxPriceDrawdown_(0.0), timeDecayRisk_(0.0) {

        validateSymbol(sym);
        validateQuantity(qty);
        validatePrice(entry, "Entry price");
        validateLeverage(leverage);

        calculateMarginRequirement();
        calculatePnL();
        initialMargin_ = marginRequirement_;
        stopLossPrice_ = isLong_ ? entry * (1.0 + Config::POSITION_STOP_LOSS)
                                : entry * (1.0 - Config::POSITION_STOP_LOSS);

        // Initialize price history
        priceHistory_.push_back(entry);
        deltaEquivalent_ = std::abs(qty) * entry;
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

    // Advanced risk metric getters
    double getVolatility() const { return volatility_; }
    double getBeta() const { return beta_; }
    double getDeltaEquivalent() const { return deltaEquivalent_; }
    const std::vector<double>& getPriceHistory() const { return priceHistory_; }
    double getMaxPriceDrawdown() const { return maxPriceDrawdown_; }
    double getTimeDecayRisk() const { return timeDecayRisk_; }

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

        // Update price history and risk metrics
        updatePriceHistory(price);
        calculateVolatility();
        updateMaxDrawdown();
        updateDeltaEquivalent();

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

    // Advanced risk calculation methods
private:
    void updatePriceHistory(double price) {
        priceHistory_.push_back(price);
        lastPriceUpdate_ = std::chrono::system_clock::now();

        // Keep only last 100 price points for performance
        if (priceHistory_.size() > 100) {
            priceHistory_.erase(priceHistory_.begin());
        }
    }

    void calculateVolatility() {
        if (priceHistory_.size() < 2) {
            volatility_ = 0.0;
            return;
        }

        std::vector<double> returns;
        for (size_t i = 1; i < priceHistory_.size(); ++i) {
            double ret = (priceHistory_[i] - priceHistory_[i-1]) / priceHistory_[i-1];
            returns.push_back(ret);
        }

        if (returns.empty()) {
            volatility_ = 0.0;
            return;
        }

        double mean = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
        double variance = 0.0;
        for (double ret : returns) {
            variance += std::pow(ret - mean, 2);
        }
        variance /= returns.size();
        volatility_ = std::sqrt(variance) * std::sqrt(365.0); // Annualized volatility
    }

    void updateMaxDrawdown() {
        if (priceHistory_.size() < 2) return;

        double peak = *std::max_element(priceHistory_.begin(), priceHistory_.end());
        double currentDrawdown = (peak - currentPrice_) / peak;
        maxPriceDrawdown_ = std::max(maxPriceDrawdown_, currentDrawdown);
    }

    void updateDeltaEquivalent() {
        deltaEquivalent_ = std::abs(quantity_) * currentPrice_;
    }

public:
    // Advanced risk metrics methods
    double calculatePositionVaR(double confidenceLevel = 0.05, int timeHorizon = 1) const {
        if (volatility_ <= 0.0) return 0.0;

        // Using parametric VaR calculation
        double zScore = 1.645; // 95% confidence level default
        if (confidenceLevel <= 0.01) zScore = 2.326; // 99%
        else if (confidenceLevel <= 0.05) zScore = 1.645; // 95%
        else if (confidenceLevel <= 0.10) zScore = 1.282; // 90%

        double dailyVol = volatility_ / std::sqrt(365.0);
        double timeAdjustedVol = dailyVol * std::sqrt(timeHorizon);

        return getPositionValue() * zScore * timeAdjustedVol;
    }

    double calculateExpectedShortfall(double confidenceLevel = 0.05, int timeHorizon = 1) const {
        double var = calculatePositionVaR(confidenceLevel, timeHorizon);
        // ES is typically 20-30% higher than VaR for normal distribution
        return var * 1.25;
    }

    void setBeta(double beta) { beta_ = beta; }

    double calculateTimeDecayRisk() {
        auto now = std::chrono::system_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::hours>(now - entryTime_);
        double hoursHeld = static_cast<double>(duration.count());

        // Simple time decay model - risk increases over time
        timeDecayRisk_ = volatility_ * std::sqrt(hoursHeld / (24.0 * 7.0)); // Weekly normalization
        return timeDecayRisk_;
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

    // Advanced portfolio risk metrics
    double portfolioVaR_;
    double portfolioExpectedShortfall_;
    double portfolioVolatility_;
    double portfolioBeta_;
    double concentrationRisk_;
    double correlationRisk_;
    std::vector<double> valueHistory_;
    std::map<std::string, double> sectorExposure_;
    double maxCorrelation_;
    double diversificationRatio_;

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
          stopLossTriggered_(false), maxDrawdownLimit_(Config::PORTFOLIO_STOP_LOSS),
          portfolioVaR_(0.0), portfolioExpectedShortfall_(0.0),
          portfolioVolatility_(0.0), portfolioBeta_(1.0),
          concentrationRisk_(0.0), correlationRisk_(0.0),
          maxCorrelation_(0.0), diversificationRatio_(1.0) {

        validateStrategyName(name);
        validateValue(initialCash, "Initial cash");
        validateLeverage(maxLeverage);

        availableMargin_ = initialCash * maxLeverage;
        valueHistory_.push_back(initialCash);
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

    // Advanced risk metric getters
    double getPortfolioVaR() const { return portfolioVaR_; }
    double getPortfolioExpectedShortfall() const { return portfolioExpectedShortfall_; }
    double getPortfolioVolatility() const { return portfolioVolatility_; }
    double getPortfolioBeta() const { return portfolioBeta_; }
    double getConcentrationRisk() const { return concentrationRisk_; }
    double getCorrelationRisk() const { return correlationRisk_; }
    const std::vector<double>& getValueHistory() const { return valueHistory_; }
    const std::map<std::string, double>& getSectorExposure() const { return sectorExposure_; }
    double getMaxCorrelation() const { return maxCorrelation_; }
    double getDiversificationRatio() const { return diversificationRatio_; }

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
        updateValueHistory(value);
        calculatePortfolioVolatility();
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

    // Advanced portfolio risk calculation methods
private:
    void updateValueHistory(double value) {
        valueHistory_.push_back(value);
        // Keep only last 100 value points for performance
        if (valueHistory_.size() > 100) {
            valueHistory_.erase(valueHistory_.begin());
        }
    }

    void calculatePortfolioVolatility() {
        if (valueHistory_.size() < 2) {
            portfolioVolatility_ = 0.0;
            return;
        }

        std::vector<double> returns;
        for (size_t i = 1; i < valueHistory_.size(); ++i) {
            double ret = (valueHistory_[i] - valueHistory_[i-1]) / valueHistory_[i-1];
            returns.push_back(ret);
        }

        if (returns.empty()) {
            portfolioVolatility_ = 0.0;
            return;
        }

        double mean = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
        double variance = 0.0;
        for (double ret : returns) {
            variance += std::pow(ret - mean, 2);
        }
        variance /= returns.size();
        portfolioVolatility_ = std::sqrt(variance) * std::sqrt(365.0); // Annualized
    }

public:
    // Advanced portfolio risk methods
    double calculatePortfolioVaR(const std::vector<Position>& positions, double confidenceLevel = 0.05, int timeHorizon = 1) {
        if (positions.empty() || portfolioVolatility_ <= 0.0) {
            return 0.0;
        }

        // Using parametric VaR calculation
        double zScore = 1.645; // 95% confidence level default
        if (confidenceLevel <= 0.01) zScore = 2.326; // 99%
        else if (confidenceLevel <= 0.05) zScore = 1.645; // 95%
        else if (confidenceLevel <= 0.10) zScore = 1.282; // 90%

        double dailyVol = portfolioVolatility_ / std::sqrt(365.0);
        double timeAdjustedVol = dailyVol * std::sqrt(timeHorizon);

        portfolioVaR_ = totalValue_ * zScore * timeAdjustedVol;
        return portfolioVaR_;
    }

    double calculatePortfolioExpectedShortfall(const std::vector<Position>& positions, double confidenceLevel = 0.05, int timeHorizon = 1) {
        double var = calculatePortfolioVaR(positions, confidenceLevel, timeHorizon);
        portfolioExpectedShortfall_ = var * 1.25; // ES typically 25% higher than VaR
        return portfolioExpectedShortfall_;
    }

    double calculateConcentrationRisk(const std::vector<Position>& positions) {
        if (positions.empty()) return 0.0;

        double totalValue = 0.0;
        std::map<std::string, double> symbolExposure;

        // Calculate exposure by symbol
        for (const auto& pos : positions) {
            double posValue = pos.getPositionValue();
            totalValue += posValue;
            symbolExposure[pos.getSymbol()] += posValue;
        }

        if (totalValue <= 0.0) return 0.0;

        // Calculate Herfindahl-Hirschman Index (HHI) for concentration
        double hhi = 0.0;
        for (const auto& exposure : symbolExposure) {
            double weight = exposure.second / totalValue;
            hhi += weight * weight;
        }

        concentrationRisk_ = hhi; // Higher HHI = more concentrated = higher risk
        return concentrationRisk_;
    }

    double calculateCorrelationRisk(const std::vector<Position>& positions) {
        if (positions.size() < 2) return 0.0;

        // Simple correlation risk estimate based on position overlap
        std::map<std::string, double> sectorWeights;
        double totalValue = 0.0;

        for (const auto& pos : positions) {
            double posValue = pos.getPositionValue();
            totalValue += posValue;

            // Simplified: treat all crypto as same sector for now
            // In practice, you'd categorize by DeFi, Layer1, Layer2, etc.
            sectorWeights["crypto"] += posValue;
        }

        if (totalValue <= 0.0) return 0.0;

        // Calculate maximum single sector exposure as correlation proxy
        double maxSectorWeight = 0.0;
        for (const auto& sector : sectorWeights) {
            double weight = sector.second / totalValue;
            maxSectorWeight = std::max(maxSectorWeight, weight);
        }

        correlationRisk_ = maxSectorWeight; // Higher concentration = higher correlation risk
        maxCorrelation_ = maxSectorWeight;
        return correlationRisk_;
    }

    void calculateDiversificationRatio(const std::vector<Position>& positions) {
        if (positions.empty()) {
            diversificationRatio_ = 1.0;
            return;
        }

        // Simple diversification ratio = 1 / concentration risk
        double concentration = calculateConcentrationRisk(positions);
        if (concentration > 0.0) {
            diversificationRatio_ = 1.0 / concentration;
        } else {
            diversificationRatio_ = static_cast<double>(positions.size()); // Perfect diversification
        }
    }

    void updateSectorExposure(const std::vector<Position>& positions) {
        sectorExposure_.clear();
        double totalValue = 0.0;

        for (const auto& pos : positions) {
            double posValue = pos.getPositionValue();
            totalValue += posValue;
            sectorExposure_["crypto"] += posValue; // Simplified sectoring
        }

        // Convert to percentages
        if (totalValue > 0.0) {
            for (auto& sector : sectorExposure_) {
                sector.second = (sector.second / totalValue) * 100.0;
            }
        }
    }

    // Comprehensive portfolio risk update method
    void updatePortfolioRiskMetrics(const std::vector<Position>& positions) {
        calculatePortfolioVaR(positions);
        calculatePortfolioExpectedShortfall(positions);
        calculateConcentrationRisk(positions);
        calculateCorrelationRisk(positions);
        calculateDiversificationRatio(positions);
        updateSectorExposure(positions);
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

// Risk reporting framework
struct RiskReport {
    std::chrono::system_clock::time_point timestamp;

    // Portfolio-level risks
    double portfolioVaR95;
    double portfolioVaR99;
    double portfolioExpectedShortfall;
    double portfolioVolatility;
    double concentrationRisk;
    double correlationRisk;
    double diversificationRatio;

    // Position-level risks
    struct PositionRisk {
        std::string symbol;
        double positionVaR;
        double expectedShortfall;
        double volatility;
        double timeDecayRisk;
        double deltaEquivalent;
        double maxDrawdown;
    };
    std::vector<PositionRisk> positionRisks;

    // Risk thresholds and alerts
    struct RiskAlert {
        std::string alertType;
        std::string message;
        double severity; // 0-1, where 1 is critical
        std::chrono::system_clock::time_point alertTime;
    };
    std::vector<RiskAlert> alerts;

    // Risk metrics summary
    double totalRiskScore; // Aggregate risk score 0-100
    std::string riskLevel; // "LOW", "MEDIUM", "HIGH", "CRITICAL"

    RiskReport() : timestamp(std::chrono::system_clock::now()),
                   portfolioVaR95(0.0), portfolioVaR99(0.0),
                   portfolioExpectedShortfall(0.0), portfolioVolatility(0.0),
                   concentrationRisk(0.0), correlationRisk(0.0),
                   diversificationRatio(1.0), totalRiskScore(0.0),
                   riskLevel("LOW") {}

    void generateRiskAlerts(const Portfolio& portfolio, const std::vector<Position>& positions) {
        alerts.clear();

        // Check concentration risk
        if (concentrationRisk > 0.5) { // More than 50% in single position
            alerts.push_back({
                "CONCENTRATION_RISK",
                "Portfolio heavily concentrated in single position",
                0.8,
                std::chrono::system_clock::now()
            });
        }

        // Check VaR limits
        if (portfolioVaR95 > portfolio.getTotalValue() * 0.05) { // VaR > 5% of portfolio
            alerts.push_back({
                "HIGH_VAR",
                "Portfolio VaR exceeds 5% threshold",
                0.7,
                std::chrono::system_clock::now()
            });
        }

        // Check margin utilization
        if (portfolio.getMarginUtilization() > 0.8) { // >80% margin used
            alerts.push_back({
                "HIGH_MARGIN_USAGE",
                "Margin utilization exceeds 80%",
                0.9,
                std::chrono::system_clock::now()
            });
        }

        // Check position-specific risks
        for (const auto& pos : positions) {
            if (pos.getVolatility() > 1.0) { // Annualized volatility > 100%
                alerts.push_back({
                    "HIGH_VOLATILITY",
                    "Position " + pos.getSymbol() + " has high volatility",
                    0.6,
                    std::chrono::system_clock::now()
                });
            }

            if (pos.isStopLossTriggered()) {
                alerts.push_back({
                    "STOP_LOSS_TRIGGERED",
                    "Stop loss triggered for " + pos.getSymbol(),
                    1.0,
                    std::chrono::system_clock::now()
                });
            }
        }

        calculateTotalRiskScore();
    }

    void calculateTotalRiskScore() {
        // Simple risk scoring algorithm (0-100)
        double score = 0.0;

        // Concentration risk (0-30 points)
        score += std::min(30.0, concentrationRisk * 60.0);

        // VaR risk (0-25 points)
        if (portfolioVaR95 > 0) {
            score += std::min(25.0, (portfolioVaR95 / 0.1) * 25.0); // Normalized to 10% VaR
        }

        // Volatility risk (0-20 points)
        score += std::min(20.0, portfolioVolatility * 10.0);

        // Alert severity (0-25 points)
        double maxAlertSeverity = 0.0;
        for (const auto& alert : alerts) {
            maxAlertSeverity = std::max(maxAlertSeverity, alert.severity);
        }
        score += maxAlertSeverity * 25.0;

        totalRiskScore = std::min(100.0, score);

        // Determine risk level
        if (totalRiskScore < 25.0) riskLevel = "LOW";
        else if (totalRiskScore < 50.0) riskLevel = "MEDIUM";
        else if (totalRiskScore < 75.0) riskLevel = "HIGH";
        else riskLevel = "CRITICAL";
    }
};

} // namespace Models
} // namespace Database
} // namespace CryptoClaude