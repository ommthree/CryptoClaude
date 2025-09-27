#pragma once

#include "../Market/LiveMarketDataProvider.h"
#include "../Risk/ProductionRiskManager.h"
#include "../Http/HttpClient.h"
#include <memory>
#include <vector>
#include <map>
#include <string>
#include <chrono>
#include <atomic>
#include <mutex>
#include <thread>
#include <queue>
#include <functional>

namespace CryptoClaude {
namespace Trading {

/**
 * Order Management System - Day 23 Implementation
 * Production-grade order execution and management for live trading
 * Handles order routing, execution, and post-trade processing
 */
class OrderManagementSystem {
public:
    // Order types and properties
    enum class OrderType {
        MARKET,
        LIMIT,
        STOP_LOSS,
        TAKE_PROFIT,
        STOP_LIMIT,
        ICEBERG,
        TWAP,           // Time Weighted Average Price
        VWAP            // Volume Weighted Average Price
    };

    enum class OrderSide {
        BUY,
        SELL
    };

    enum class OrderStatus {
        PENDING,        // Order created but not yet submitted
        SUBMITTED,      // Order submitted to exchange
        PARTIALLY_FILLED, // Order partially executed
        FILLED,         // Order completely executed
        CANCELLED,      // Order cancelled
        REJECTED,       // Order rejected by exchange/risk system
        EXPIRED,        // Order expired
        FAILED          // Order failed due to system error
    };

    enum class TimeInForce {
        GTC,            // Good Till Cancelled
        IOC,            // Immediate Or Cancel
        FOK,            // Fill Or Kill
        DAY,            // Day order
        GTD             // Good Till Date
    };

    // Live trading order
    struct LiveOrder {
        std::string order_id;
        std::string parent_order_id;        // For child orders (stop-loss, etc.)
        std::string client_order_id;        // Internal tracking ID
        std::string symbol;
        std::string exchange;

        // Order specifications
        OrderType order_type;
        OrderSide order_side;
        double quantity;                    // Original order quantity
        double price;                       // Limit price (0 for market orders)
        double stop_price;                  // Stop trigger price
        TimeInForce time_in_force;
        std::chrono::system_clock::time_point expires_at;

        // Execution details
        OrderStatus status;
        double filled_quantity;             // Quantity executed so far
        double remaining_quantity;          // Quantity remaining
        double average_fill_price;          // Average execution price
        double total_commission;            // Total commission paid

        // Timestamps
        std::chrono::system_clock::time_point created_at;
        std::chrono::system_clock::time_point submitted_at;
        std::chrono::system_clock::time_point last_updated_at;
        std::chrono::system_clock::time_point completed_at;

        // Risk and compliance
        std::string risk_check_result;      // Risk check outcome
        bool is_trs_compliant;             // TRS compliance status
        std::vector<std::string> compliance_notes;

        // Execution quality metrics
        double expected_price;              // Expected execution price
        double slippage_bps;               // Actual slippage in basis points
        std::chrono::milliseconds execution_latency; // Time to execution
        double market_impact_bps;          // Estimated market impact

        // Error handling
        std::string last_error_message;
        int retry_count;
        std::chrono::system_clock::time_point last_retry_at;

        LiveOrder() : quantity(0), price(0), stop_price(0), filled_quantity(0),
                     remaining_quantity(0), average_fill_price(0), total_commission(0),
                     is_trs_compliant(true), expected_price(0), slippage_bps(0),
                     execution_latency(std::chrono::milliseconds{0}), market_impact_bps(0),
                     retry_count(0) {
            order_type = OrderType::MARKET;
            order_side = OrderSide::BUY;
            status = OrderStatus::PENDING;
            time_in_force = TimeInForce::GTC;
        }
    };

    // Order execution fill
    struct OrderFill {
        std::string fill_id;
        std::string order_id;
        std::string exchange_trade_id;
        std::chrono::system_clock::time_point fill_time;

        double fill_quantity;
        double fill_price;
        double commission;
        std::string commission_asset;

        // Market microstructure
        bool is_maker;                      // Was this a maker or taker fill
        double liquidity_indicator;         // Liquidity removed/added
        std::string counterparty_info;      // If available

        // Trade analysis
        double vwap_comparison;             // Fill price vs VWAP
        double bid_ask_spread_at_fill;      // Market spread when filled
        std::chrono::milliseconds order_to_fill_latency; // Order submission to fill

        OrderFill() : fill_quantity(0), fill_price(0), commission(0),
                     is_maker(false), liquidity_indicator(0), vwap_comparison(0),
                     bid_ask_spread_at_fill(0), order_to_fill_latency(std::chrono::milliseconds{0}) {}
    };

    // Order execution report
    struct ExecutionReport {
        std::string report_id;
        std::string order_id;
        std::chrono::system_clock::time_point report_time;

        OrderStatus previous_status;
        OrderStatus current_status;
        std::string status_change_reason;

        // Execution details
        std::vector<OrderFill> new_fills;
        double cumulative_quantity;
        double leaves_quantity;             // Remaining quantity

        // Performance metrics
        double execution_shortfall;         // Actual vs expected cost
        double implementation_shortfall;    // Total trading cost

        ExecutionReport() : previous_status(OrderStatus::PENDING), current_status(OrderStatus::PENDING),
                          cumulative_quantity(0), leaves_quantity(0), execution_shortfall(0),
                          implementation_shortfall(0) {}
    };

    // Exchange connection configuration
    struct ExchangeConnection {
        std::string exchange_name;
        std::unique_ptr<Http::HttpClient> http_client;

        // API configuration
        std::string api_endpoint;
        std::string websocket_endpoint;
        std::map<std::string, std::string> auth_headers;

        // Rate limiting
        int max_orders_per_second = 10;
        int max_requests_per_minute = 1000;
        std::chrono::milliseconds min_request_interval{100};

        // Status
        bool is_connected = false;
        std::chrono::system_clock::time_point last_heartbeat;
        std::chrono::milliseconds average_latency{0};

        ExchangeConnection() = default;
    };

private:
    // Core components
    std::unique_ptr<Market::LiveMarketDataProvider> market_data_provider_;
    std::unique_ptr<Risk::ProductionRiskManager> risk_manager_;
    std::map<std::string, std::unique_ptr<ExchangeConnection>> exchange_connections_;

    // Order management state
    std::atomic<bool> is_running_{false};
    mutable std::mutex orders_mutex_;
    mutable std::mutex fills_mutex_;
    mutable std::mutex execution_mutex_;

    // Threading
    std::thread order_processing_thread_;
    std::thread execution_monitoring_thread_;
    std::thread fill_processing_thread_;

    // Order storage
    std::map<std::string, LiveOrder> active_orders_;
    std::vector<LiveOrder> completed_orders_;
    std::queue<LiveOrder> pending_submissions_;
    std::vector<OrderFill> recent_fills_;
    std::vector<ExecutionReport> execution_reports_;

    // Performance tracking
    struct PerformanceMetrics {
        int total_orders_today = 0;
        int successful_orders_today = 0;
        double average_slippage_bps = 0;
        std::chrono::milliseconds average_execution_time{0};
        double total_commission_paid = 0;

        // Execution quality
        double fill_rate = 0;               // Percentage of orders filled
        double maker_rate = 0;              // Percentage of fills as maker
        double average_market_impact = 0;

        // Error rates
        int rejected_orders_today = 0;
        int failed_orders_today = 0;
        double error_rate = 0;
    } daily_performance_;

    // Callback functions
    std::function<void(const ExecutionReport&)> execution_callback_;
    std::function<void(const OrderFill&)> fill_callback_;
    std::function<void(const std::string&, const std::string&)> error_callback_;

    // Core processing methods
    void runOrderProcessing();
    void runExecutionMonitoring();
    void runFillProcessing();

    // Order lifecycle management
    bool validateOrder(const LiveOrder& order);
    bool submitOrderToExchange(LiveOrder& order);
    void processOrderUpdate(const std::string& order_id, const std::string& update_data);
    void handleOrderFill(const OrderFill& fill);
    void processOrderCancellation(const std::string& order_id, const std::string& reason);

    // Exchange communication
    bool sendOrderToExchange(const std::string& exchange, const LiveOrder& order);
    bool cancelOrderOnExchange(const std::string& exchange, const std::string& order_id);
    std::string getOrderStatusFromExchange(const std::string& exchange, const std::string& order_id);

    // Risk integration
    bool performPreTradeRiskCheck(const LiveOrder& order);
    void updateRiskManagerWithFill(const OrderFill& fill);

    // Execution algorithms
    void executeMarketOrder(LiveOrder& order);
    void executeLimitOrder(LiveOrder& order);
    void executeTWAPOrder(LiveOrder& order);
    void executeVWAPOrder(LiveOrder& order);

    // Order routing
    std::string selectOptimalExchange(const std::string& symbol, OrderSide side, double quantity);
    double estimateExecutionCost(const std::string& exchange, const LiveOrder& order);

    // Performance analysis
    void calculateExecutionMetrics(const LiveOrder& order, const std::vector<OrderFill>& fills);
    void updateDailyPerformanceMetrics();

public:
    OrderManagementSystem();
    ~OrderManagementSystem();

    // System lifecycle
    bool startOrderProcessing();
    void stopOrderProcessing();
    bool isRunning() const { return is_running_.load(); }

    // Integration
    bool integrateMarketDataProvider(std::unique_ptr<Market::LiveMarketDataProvider> provider);
    bool integrateRiskManager(std::unique_ptr<Risk::ProductionRiskManager> risk_manager);
    bool addExchangeConnection(const std::string& exchange_name, std::unique_ptr<ExchangeConnection> connection);

    // Order submission
    std::string submitOrder(const LiveOrder& order);
    bool modifyOrder(const std::string& order_id, double new_quantity, double new_price);
    bool cancelOrder(const std::string& order_id, const std::string& reason = "user_request");
    bool cancelAllOrders(const std::string& symbol = "");

    // Order query
    LiveOrder getOrder(const std::string& order_id) const;
    std::vector<LiveOrder> getActiveOrders() const;
    std::vector<LiveOrder> getCompletedOrders(std::chrono::hours lookback = std::chrono::hours{24}) const;
    std::vector<LiveOrder> getOrdersBySymbol(const std::string& symbol) const;

    // Fill and execution data
    std::vector<OrderFill> getOrderFills(const std::string& order_id) const;
    std::vector<OrderFill> getRecentFills(std::chrono::hours lookback = std::chrono::hours{1}) const;
    std::vector<ExecutionReport> getExecutionReports(std::chrono::hours lookback = std::chrono::hours{24}) const;

    // Execution quality analysis
    struct ExecutionQualityReport {
        std::chrono::system_clock::time_point report_period_start;
        std::chrono::system_clock::time_point report_period_end;

        // Volume and count statistics
        int total_orders;
        int successful_orders;
        double total_volume_traded;
        double average_order_size;

        // Execution performance
        double average_slippage_bps;
        double median_slippage_bps;
        std::chrono::milliseconds average_execution_time;
        std::chrono::milliseconds median_execution_time;

        // Cost analysis
        double total_commission;
        double average_commission_bps;
        double total_market_impact;
        double implementation_shortfall;

        // Fill analysis
        double fill_rate;
        double maker_percentage;
        double price_improvement_frequency;

        // Risk and compliance
        int risk_rejections;
        int compliance_violations;
        double trs_compliance_rate;
    };

    ExecutionQualityReport generateExecutionQualityReport(std::chrono::hours lookback = std::chrono::hours{24}) const;

    // Algorithm trading support
    struct AlgorithmicOrderConfig {
        OrderType algo_type;                // TWAP, VWAP, etc.
        std::chrono::minutes execution_duration{60}; // Time to complete order
        double participation_rate = 0.20;   // Max % of volume to consume
        double price_tolerance_bps = 10.0;  // Price movement tolerance

        // Risk parameters
        bool enable_adaptive_sizing = true;
        double max_slice_size_pct = 0.05;   // Max % of total order per slice
        std::chrono::seconds min_slice_interval{30}; // Minimum time between slices
    };

    std::string submitAlgorithmicOrder(const LiveOrder& parent_order, const AlgorithmicOrderConfig& algo_config);
    bool pauseAlgorithmicOrder(const std::string& algo_order_id);
    bool resumeAlgorithmicOrder(const std::string& algo_order_id);

    // Performance monitoring
    PerformanceMetrics getDailyPerformanceMetrics() const;

    struct TradingVenueAnalysis {
        std::string venue_name;
        int order_count;
        double fill_rate;
        std::chrono::milliseconds average_latency;
        double average_slippage;
        double cost_per_trade;
        double quality_score;               // Composite quality metric
    };

    std::vector<TradingVenueAnalysis> analyzeTradingVenues() const;

    // Risk and compliance integration
    bool enablePreTradeRiskChecks(bool enable = true);
    bool setRiskLimits(const std::string& symbol, double max_position_size, double max_daily_volume);
    std::vector<std::string> getComplianceViolations(std::chrono::hours lookback = std::chrono::hours{24}) const;

    // Market impact analysis
    struct MarketImpactAnalysis {
        std::string symbol;
        OrderSide side;
        double order_size;
        double pre_trade_mid;
        double post_trade_mid;
        double temporary_impact;            // Immediate price impact
        double permanent_impact;            // Lasting price impact
        std::chrono::minutes impact_duration; // How long impact lasted
    };

    std::vector<MarketImpactAnalysis> analyzeMarketImpact(std::chrono::hours lookback = std::chrono::hours{24}) const;

    // Emergency controls
    void enableEmergencyMode();
    void disableEmergencyMode();
    bool isEmergencyModeActive() const;
    void cancelAllOrdersEmergency();

    // Callback registration
    void setExecutionCallback(std::function<void(const ExecutionReport&)> callback);
    void setFillCallback(std::function<void(const OrderFill&)> callback);
    void setErrorCallback(std::function<void(const std::string&, const std::string&)> callback);

    // Data export
    bool exportOrderHistory(const std::string& filename, const std::string& format = "csv",
                           std::chrono::hours lookback = std::chrono::hours{168}) const; // 1 week
    bool exportFillData(const std::string& filename, const std::string& format = "csv",
                       std::chrono::hours lookback = std::chrono::hours{24}) const;

    // Static utility methods
    static std::string orderTypeToString(OrderType type);
    static OrderType stringToOrderType(const std::string& type_str);
    static std::string orderStatusToString(OrderStatus status);
    static OrderStatus stringToOrderStatus(const std::string& status_str);
    static std::string generateOrderId();
    static double calculateSlippage(double expected_price, double actual_price, OrderSide side);
    static std::chrono::milliseconds calculateExecutionTime(
        const std::chrono::system_clock::time_point& submit_time,
        const std::chrono::system_clock::time_point& fill_time);
};

} // namespace Trading
} // namespace CryptoClaude