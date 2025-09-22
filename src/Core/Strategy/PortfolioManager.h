#pragma once
#include "IStrategy.h"
#include "../Database/Models/PortfolioData.h"
#include "../Database/Repositories/MarketDataRepository.h"
#include <memory>
#include <vector>
#include <map>

namespace CryptoClaude {
namespace Strategy {

// Portfolio management events
enum class PortfolioEvent {
    POSITION_OPENED,
    POSITION_CLOSED,
    POSITION_MODIFIED,
    REBALANCE_EXECUTED,
    RISK_LIMIT_BREACHED,
    MARGIN_CALL
};

// Portfolio event details
struct PortfolioEventDetails {
    PortfolioEvent eventType;
    std::string symbol;
    std::string description;
    std::chrono::system_clock::time_point timestamp;
    std::map<std::string, double> eventData;

    PortfolioEventDetails(PortfolioEvent type, const std::string& sym, const std::string& desc)
        : eventType(type), symbol(sym), description(desc),
          timestamp(std::chrono::system_clock::now()) {}
};

// Real-time portfolio metrics
struct RealTimeMetrics {
    double totalValue;
    double totalPnL;
    double unrealizedPnL;
    double realizedPnL;
    double exposure;
    double netExposure;
    double grossExposure;
    double leverage;
    double availableCash;
    double marginUsed;

    int longPositions;
    int shortPositions;
    double avgPositionSize;

    std::chrono::system_clock::time_point lastUpdate;

    RealTimeMetrics()
        : totalValue(0.0), totalPnL(0.0), unrealizedPnL(0.0), realizedPnL(0.0),
          exposure(0.0), netExposure(0.0), grossExposure(0.0), leverage(0.0),
          availableCash(0.0), marginUsed(0.0), longPositions(0), shortPositions(0),
          avgPositionSize(0.0), lastUpdate(std::chrono::system_clock::now()) {}
};

class PortfolioManager {
public:
    PortfolioManager();
    ~PortfolioManager() = default;

    // Core dependencies
    void setMarketDataRepository(std::shared_ptr<Database::Repositories::MarketDataRepository> repository);
    void setStrategy(std::shared_ptr<IStrategy> strategy);

    // Portfolio lifecycle
    bool createPortfolio(const std::string& strategyName, double initialCapital);
    bool loadPortfolio(const std::string& portfolioId);
    bool savePortfolio();

    // Position management
    bool executeSignal(const TradingSignal& signal);
    bool executeSignals(const std::vector<TradingSignal>& signals);
    bool closePosition(const std::string& symbol);
    bool closeAllPositions();

    // Portfolio operations
    bool updatePositionPrices();
    bool rebalancePortfolio(const std::vector<TradingSignal>& signals);
    RealTimeMetrics calculateMetrics();

    // Risk management
    bool checkRiskLimits();
    bool enforceStopLoss();
    bool handleMarginCall();

    // Portfolio access
    Database::Models::Portfolio getCurrentPortfolio() const { return m_currentPortfolio; }
    std::vector<Database::Models::Position> getPositions() const;
    std::vector<Database::Models::Position> getLongPositions() const;
    std::vector<Database::Models::Position> getShortPositions() const;

    // Performance tracking
    std::vector<PortfolioEventDetails> getEventHistory() const { return m_eventHistory; }
    std::vector<RealTimeMetrics> getMetricsHistory() const { return m_metricsHistory; }

    // Configuration
    void setTransactionCosts(double basisPoints) { m_transactionCostBps = basisPoints; }
    void setMarginRate(double rate) { m_marginRate = rate; }
    void setMaxLeverage(double leverage) { m_maxLeverage = leverage; }
    void setRiskLimits(const std::map<std::string, double>& limits) { m_riskLimits = limits; }

    // Event handling
    using EventCallback = std::function<void(const PortfolioEventDetails&)>;
    void setEventCallback(EventCallback callback) { m_eventCallback = callback; }

    // Reporting
    bool generateDailyReport(const std::string& filename);
    bool generatePositionReport(const std::string& filename);
    bool generatePnLReport(const std::string& startDate, const std::string& endDate, const std::string& filename);

    // Error handling
    std::string getLastError() const { return m_lastError; }
    bool hasError() const { return !m_lastError.empty(); }
    void clearError() { m_lastError.clear(); }

private:
    // Core components
    std::shared_ptr<Database::Repositories::MarketDataRepository> m_marketDataRepository;
    std::shared_ptr<IStrategy> m_strategy;

    // Portfolio state
    Database::Models::Portfolio m_currentPortfolio;
    std::string m_portfolioId;
    bool m_portfolioLoaded;

    // Configuration
    double m_transactionCostBps;
    double m_marginRate;
    double m_maxLeverage;
    std::map<std::string, double> m_riskLimits;

    // Event tracking
    std::vector<PortfolioEventDetails> m_eventHistory;
    std::vector<RealTimeMetrics> m_metricsHistory;
    EventCallback m_eventCallback;

    // State
    std::string m_lastError;

    // Core operations
    bool executeOrder(const std::string& symbol, double quantity, double price, bool isLong);
    bool modifyPosition(const std::string& symbol, double newQuantity);

    // Position management helpers
    Database::Models::Position* findPosition(const std::string& symbol);
    double calculatePositionValue(const Database::Models::Position& position, double currentPrice);
    double calculateTransactionCost(double notionalAmount);

    // Risk calculations
    double calculatePortfolioLeverage();
    double calculatePortfolioVaR(double confidenceLevel = 0.05);
    bool isWithinRiskLimits();

    // Market data integration
    std::map<std::string, double> getCurrentPrices(const std::vector<std::string>& symbols);
    double getCurrentPrice(const std::string& symbol);

    // Event management
    void logEvent(PortfolioEvent eventType, const std::string& symbol, const std::string& description);
    void logMetrics(const RealTimeMetrics& metrics);
    void notifyEvent(const PortfolioEventDetails& event);

    // Validation
    bool validateSignal(const TradingSignal& signal);
    bool validatePortfolioState();

    // Database operations
    bool savePositionToDatabase(const Database::Models::Position& position);
    bool savePortfolioToDatabase(const Database::Models::Portfolio& portfolio);
    bool loadPositionsFromDatabase();

    // Error handling
    void setError(const std::string& error);
};

// Portfolio analytics utilities
class PortfolioAnalyzer {
public:
    // Performance metrics
    static double calculateSharpeRatio(const std::vector<RealTimeMetrics>& metrics, double riskFreeRate = 0.0);
    static double calculateMaxDrawdown(const std::vector<RealTimeMetrics>& metrics);
    static double calculateVolatility(const std::vector<RealTimeMetrics>& metrics);

    // Risk metrics
    static double calculateBeta(
        const std::vector<RealTimeMetrics>& portfolioMetrics,
        const std::vector<Database::Models::MarketData>& marketData);

    static double calculateTrackingError(
        const std::vector<RealTimeMetrics>& portfolioMetrics,
        const std::vector<Database::Models::MarketData>& benchmarkData);

    // Position analysis
    static std::map<std::string, double> analyzePositionContribution(
        const Database::Models::Portfolio& portfolio);

    static std::map<std::string, double> calculatePositionRisk(
        const Database::Models::Portfolio& portfolio);

    // Correlation analysis
    static std::map<std::pair<std::string, std::string>, double> calculatePositionCorrelations(
        const std::vector<std::string>& symbols,
        const std::string& startDate,
        const std::string& endDate);

    // Scenario analysis
    struct StressTestResult {
        std::string scenario;
        double portfolioChange;
        std::map<std::string, double> positionChanges;
        double newPortfolioValue;
        double newLeverage;
        bool riskLimitsBreach;
    };

    static std::vector<StressTestResult> performStressTest(
        const Database::Models::Portfolio& portfolio,
        const std::vector<std::map<std::string, double>>& scenarios);

private:
    static std::vector<double> extractPortfolioValues(const std::vector<RealTimeMetrics>& metrics);
    static std::vector<double> calculateReturns(const std::vector<double>& values);
};

// Order execution simulator (for backtesting)
class OrderExecutor {
public:
    enum class OrderType {
        MARKET,
        LIMIT,
        STOP,
        STOP_LIMIT
    };

    struct Order {
        std::string symbol;
        OrderType orderType;
        double quantity;
        double price;          // For limit orders
        double stopPrice;      // For stop orders
        bool isLong;
        std::chrono::system_clock::time_point timestamp;

        Order() : orderType(OrderType::MARKET), quantity(0.0), price(0.0),
                 stopPrice(0.0), isLong(true),
                 timestamp(std::chrono::system_clock::now()) {}
    };

    struct ExecutionResult {
        bool executed;
        double executionPrice;
        double executionQuantity;
        double slippage;
        double commission;
        std::string failureReason;

        ExecutionResult() : executed(false), executionPrice(0.0),
                           executionQuantity(0.0), slippage(0.0), commission(0.0) {}
    };

    OrderExecutor() = default;

    ExecutionResult executeOrder(
        const Order& order,
        double currentPrice,
        double bidAskSpread = 0.001);

    void setSlippageModel(const std::function<double(double, double)>& model) { m_slippageModel = model; }
    void setCommissionModel(const std::function<double(double)>& model) { m_commissionModel = model; }

private:
    std::function<double(double, double)> m_slippageModel;
    std::function<double(double)> m_commissionModel;

    double calculateSlippage(double quantity, double price);
    double calculateCommission(double notionalAmount);
};

} // namespace Strategy
} // namespace CryptoClaude