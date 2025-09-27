#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <chrono>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include "../Analytics/BacktestingEngine.h"
#include "../Database/DatabaseManager.h"

namespace CryptoClaude {
namespace Trading {

/**
 * Real-time market data feed
 */
struct LiveMarketData {
    std::chrono::system_clock::time_point timestamp;
    std::string symbol;
    double bid;
    double ask;
    double last_price;
    double volume_24h;
    double price_change_24h;

    LiveMarketData() = default;
    LiveMarketData(const std::string& sym, double b, double a, double last)
        : timestamp(std::chrono::system_clock::now()), symbol(sym), bid(b), ask(a), last_price(last),
          volume_24h(0.0), price_change_24h(0.0) {}
};

/**
 * Virtual trading position
 */
struct VirtualPosition {
    std::string position_id;
    std::string symbol;
    std::string direction; // "LONG", "SHORT"
    double quantity;
    double entry_price;
    std::chrono::system_clock::time_point entry_time;
    double unrealized_pnl;
    double realized_pnl;
    std::string status; // "OPEN", "CLOSED", "PARTIAL"

    VirtualPosition() : quantity(0.0), entry_price(0.0), unrealized_pnl(0.0), realized_pnl(0.0) {}
    VirtualPosition(const std::string& id, const std::string& sym, const std::string& dir,
                   double qty, double price)
        : position_id(id), symbol(sym), direction(dir), quantity(qty), entry_price(price),
          entry_time(std::chrono::system_clock::now()), unrealized_pnl(0.0), realized_pnl(0.0),
          status("OPEN") {}
};

/**
 * Virtual order for paper trading
 */
struct VirtualOrder {
    std::string order_id;
    std::string symbol;
    std::string side; // "BUY", "SELL"
    std::string order_type; // "MARKET", "LIMIT", "STOP", "STOP_LIMIT"
    double quantity;
    double price; // For limit orders
    double stop_price; // For stop orders
    std::chrono::system_clock::time_point created_time;
    std::chrono::system_clock::time_point filled_time;
    std::string status; // "PENDING", "FILLED", "PARTIALLY_FILLED", "CANCELLED", "REJECTED"
    double filled_quantity;
    double average_fill_price;
    std::string rejection_reason;

    VirtualOrder() : quantity(0.0), price(0.0), stop_price(0.0), filled_quantity(0.0), average_fill_price(0.0) {}
    VirtualOrder(const std::string& id, const std::string& sym, const std::string& s,
                const std::string& type, double qty, double p = 0.0)
        : order_id(id), symbol(sym), side(s), order_type(type), quantity(qty), price(p),
          stop_price(0.0), created_time(std::chrono::system_clock::now()),
          status("PENDING"), filled_quantity(0.0), average_fill_price(0.0) {}
};

/**
 * Portfolio snapshot for performance tracking
 */
struct VirtualPortfolioSnapshot {
    std::chrono::system_clock::time_point timestamp;
    double total_equity;
    double cash_balance;
    double unrealized_pnl;
    double realized_pnl;
    double total_pnl;
    std::map<std::string, double> position_values; // symbol -> value
    std::map<std::string, VirtualPosition> positions;

    VirtualPortfolioSnapshot() : total_equity(0.0), cash_balance(0.0), unrealized_pnl(0.0),
                                realized_pnl(0.0), total_pnl(0.0) {}
};

/**
 * Paper trading session configuration
 */
struct PaperTradingConfig {
    double initial_capital;
    double commission_rate; // Basis points
    double slippage_rate; // Basis points
    std::vector<std::string> symbols;
    int max_positions;
    double max_position_size; // Percentage of portfolio
    bool enable_stop_loss;
    bool enable_take_profit;
    double stop_loss_percentage;
    double take_profit_percentage;

    PaperTradingConfig() : initial_capital(100000.0), commission_rate(25.0), slippage_rate(10.0),
                          max_positions(10), max_position_size(0.25), enable_stop_loss(true),
                          enable_take_profit(false), stop_loss_percentage(0.05), take_profit_percentage(0.10) {}
};

/**
 * Real-time paper trading engine with virtual portfolio management
 * Simulates live trading with realistic order execution and portfolio tracking
 */
class PaperTradingEngine {
public:
    PaperTradingEngine();
    ~PaperTradingEngine();

    // Initialization and Configuration
    bool initialize(std::shared_ptr<Database::DatabaseManager> db_manager,
                   const PaperTradingConfig& config);
    void shutdown();

    // Session Management
    bool startTradingSession(const std::string& session_name);
    void stopTradingSession();
    bool isSessionActive() const { return session_active_.load(); }
    std::string getCurrentSessionName() const { return current_session_name_; }

    // Market Data Feed
    void updateMarketData(const LiveMarketData& market_data);
    void subscribeToSymbol(const std::string& symbol);
    void unsubscribeFromSymbol(const std::string& symbol);
    LiveMarketData getLatestMarketData(const std::string& symbol);

    // Order Management
    std::string placeOrder(const std::string& symbol, const std::string& side,
                          const std::string& order_type, double quantity, double price = 0.0);
    bool cancelOrder(const std::string& order_id);
    VirtualOrder getOrderStatus(const std::string& order_id);
    std::vector<VirtualOrder> getActiveOrders();
    std::vector<VirtualOrder> getOrderHistory();

    // Position Management
    std::vector<VirtualPosition> getActivePositions();
    VirtualPosition getPosition(const std::string& symbol);
    bool closePosition(const std::string& symbol, double quantity = -1.0); // -1 = close all
    double getPositionValue(const std::string& symbol);

    // Portfolio Management
    VirtualPortfolioSnapshot getPortfolioSnapshot();
    double getCashBalance() const { return cash_balance_.load(); }
    double getTotalEquity();
    double getUnrealizedPnL();
    double getRealizedPnL() const { return realized_pnl_.load(); }

    // Performance Metrics
    std::vector<VirtualPortfolioSnapshot> getPerformanceHistory();
    double getSessionReturn();
    double getDailyVolatility();
    double getMaxDrawdown();
    int getTotalTrades();
    double getWinRate();

    // Risk Management
    bool checkRiskLimits(const std::string& symbol, double quantity, double price);
    void setStopLoss(const std::string& symbol, double stop_price);
    void setTakeProfit(const std::string& symbol, double target_price);
    void updateRiskParameters(double max_position_size, bool enable_stops);

    // Strategy Integration
    void processStrategySignal(const Analytics::TradingSignal& signal);
    void setStrategyMode(bool auto_execute) { auto_execute_signals_.store(auto_execute); }

    // Reporting and Analytics
    std::string generateTradingReport();
    std::string generatePerformanceReport();
    bool exportTradingData(const std::string& filename);

    // Session Persistence
    bool saveSessionState();
    bool loadSessionState(const std::string& session_name);

private:
    // Configuration
    PaperTradingConfig config_;
    std::shared_ptr<Database::DatabaseManager> db_manager_;

    // Session State
    std::atomic<bool> session_active_{false};
    std::atomic<bool> auto_execute_signals_{false};
    std::string current_session_name_;
    std::chrono::system_clock::time_point session_start_time_;

    // Portfolio State
    std::atomic<double> cash_balance_{100000.0};
    std::atomic<double> realized_pnl_{0.0};
    std::map<std::string, VirtualPosition> positions_;
    std::mutex positions_mutex_;

    // Order Management
    std::map<std::string, VirtualOrder> orders_;
    std::mutex orders_mutex_;
    std::atomic<int> order_counter_{0};

    // Market Data
    std::map<std::string, LiveMarketData> market_data_;
    std::mutex market_data_mutex_;

    // Performance Tracking
    std::vector<VirtualPortfolioSnapshot> performance_history_;
    std::mutex performance_mutex_;

    // Background Processing
    std::thread market_processing_thread_;
    std::thread order_processing_thread_;
    std::thread performance_tracking_thread_;
    std::atomic<bool> stop_background_threads_{false};

    // Order Processing Queue
    std::queue<std::string> order_processing_queue_;
    std::mutex order_queue_mutex_;

    // Internal Processing Methods
    void marketProcessingLoop();
    void orderProcessingLoop();
    void performanceTrackingLoop();

    // Order Execution
    void processOrder(const std::string& order_id);
    bool executeMarketOrder(VirtualOrder& order);
    bool executeLimitOrder(VirtualOrder& order);
    bool executeStopOrder(VirtualOrder& order);
    void fillOrder(VirtualOrder& order, double fill_price, double fill_quantity);

    // Position Updates
    void updatePosition(const std::string& symbol, const std::string& side,
                       double quantity, double price);
    void closePositionPartial(const std::string& symbol, double quantity);
    void updateUnrealizedPnL();

    // Risk Management Implementation
    bool validateOrderRisk(const VirtualOrder& order);
    void checkAndExecuteStopLoss();
    void checkAndExecuteTakeProfit();

    // Market Simulation
    double simulateSlippage(const std::string& symbol, const std::string& side, double quantity);
    double calculateCommission(double trade_value);
    double getMarketPrice(const std::string& symbol, const std::string& side);

    // Database Operations
    bool createPaperTradingTables();
    bool saveTradingData();
    bool saveOrder(const VirtualOrder& order);
    bool savePosition(const VirtualPosition& position);
    bool savePerformanceSnapshot(const VirtualPortfolioSnapshot& snapshot);

    // Utility Functions
    std::string generateOrderId();
    std::string generatePositionId(const std::string& symbol);
    std::string timePointToString(const std::chrono::system_clock::time_point& tp);
    double roundToTickSize(double price, const std::string& symbol);

    // Performance Calculations
    void calculateAndSaveSnapshot();
    double calculateEquityValue();
    std::vector<double> getEquityCurve();
};

}} // namespace CryptoClaude::Trading