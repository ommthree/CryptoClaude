#include "OrderManagementSystem.h"
#include <iostream>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <random>

namespace CryptoClaude {
namespace Trading {

OrderManagementSystem::OrderManagementSystem() {
    // Initialize performance metrics
    daily_performance_ = PerformanceMetrics{};
}

OrderManagementSystem::~OrderManagementSystem() {
    if (is_running_.load()) {
        stopOrderProcessing();
    }
}

bool OrderManagementSystem::integrateMarketDataProvider(
    std::unique_ptr<Market::LiveMarketDataProvider> provider) {
    if (!provider) {
        return false;
    }

    market_data_provider_ = std::move(provider);
    return true;
}

bool OrderManagementSystem::integrateRiskManager(
    std::unique_ptr<Risk::ProductionRiskManager> risk_manager) {
    if (!risk_manager) {
        return false;
    }

    risk_manager_ = std::move(risk_manager);
    return true;
}

bool OrderManagementSystem::addExchangeConnection(const std::string& exchange_name,
                                                 std::unique_ptr<ExchangeConnection> connection) {
    if (!connection) {
        return false;
    }

    exchange_connections_[exchange_name] = std::move(connection);
    return true;
}

bool OrderManagementSystem::startOrderProcessing() {
    if (is_running_.load()) {
        return false; // Already running
    }

    if (!market_data_provider_ || !risk_manager_) {
        std::cerr << "Required components not integrated for order processing" << std::endl;
        return false;
    }

    if (exchange_connections_.empty()) {
        std::cerr << "No exchange connections configured" << std::endl;
        return false;
    }

    is_running_.store(true);

    // Start processing threads
    order_processing_thread_ = std::thread(&OrderManagementSystem::runOrderProcessing, this);
    execution_monitoring_thread_ = std::thread(&OrderManagementSystem::runExecutionMonitoring, this);
    fill_processing_thread_ = std::thread(&OrderManagementSystem::runFillProcessing, this);

    std::cout << "Order Management System started with " << exchange_connections_.size()
              << " exchange connections" << std::endl;

    return true;
}

void OrderManagementSystem::stopOrderProcessing() {
    if (!is_running_.load()) {
        return;
    }

    is_running_.store(false);

    // Wait for threads to complete
    if (order_processing_thread_.joinable()) {
        order_processing_thread_.join();
    }

    if (execution_monitoring_thread_.joinable()) {
        execution_monitoring_thread_.join();
    }

    if (fill_processing_thread_.joinable()) {
        fill_processing_thread_.join();
    }

    std::cout << "Order Management System stopped" << std::endl;
}

std::string OrderManagementSystem::submitOrder(const LiveOrder& order) {
    // Validate order
    if (!validateOrder(order)) {
        std::cerr << "Order validation failed for symbol: " << order.symbol << std::endl;
        return "";
    }

    // Perform pre-trade risk check
    if (!performPreTradeRiskCheck(order)) {
        std::cerr << "Pre-trade risk check failed for order" << std::endl;
        return "";
    }

    // Create order with generated ID
    LiveOrder new_order = order;
    new_order.order_id = generateOrderId();
    new_order.client_order_id = new_order.order_id; // Use same ID for simplicity
    new_order.created_at = std::chrono::system_clock::now();
    new_order.status = OrderStatus::PENDING;
    new_order.remaining_quantity = order.quantity;

    // Add to active orders
    {
        std::lock_guard<std::mutex> lock(orders_mutex_);
        active_orders_[new_order.order_id] = new_order;
    }

    // Queue for submission
    {
        std::lock_guard<std::mutex> lock(execution_mutex_);
        pending_submissions_.push(new_order);
    }

    daily_performance_.total_orders_today++;

    std::cout << "Order submitted: " << new_order.order_id << " for " << new_order.quantity
              << " " << new_order.symbol << std::endl;

    return new_order.order_id;
}

bool OrderManagementSystem::validateOrder(const LiveOrder& order) {
    // Basic validation checks
    if (order.symbol.empty()) {
        return false;
    }

    if (order.quantity <= 0) {
        return false;
    }

    if (order.order_type == OrderType::LIMIT && order.price <= 0) {
        return false;
    }

    if ((order.order_type == OrderType::STOP_LOSS || order.order_type == OrderType::STOP_LIMIT) &&
        order.stop_price <= 0) {
        return false;
    }

    // Check if we have a connection for the target exchange
    if (!order.exchange.empty()) {
        if (exchange_connections_.find(order.exchange) == exchange_connections_.end()) {
            return false;
        }
    }

    return true;
}

bool OrderManagementSystem::performPreTradeRiskCheck(const LiveOrder& order) {
    if (!risk_manager_) {
        return true; // No risk manager, allow order
    }

    // Convert order to risk check parameters
    double quantity = (order.order_side == OrderSide::BUY) ? order.quantity : -order.quantity;
    double estimated_price = (order.order_type == OrderType::MARKET) ?
        getEstimatedMarketPrice(order.symbol, order.order_side) : order.price;

    if (estimated_price <= 0) {
        return false; // Could not get price estimate
    }

    auto risk_check = risk_manager_->evaluateProposedTrade(
        order.symbol, quantity, estimated_price, order.order_side == OrderSide::BUY);

    return risk_check.is_approved;
}

double OrderManagementSystem::getEstimatedMarketPrice(const std::string& symbol, OrderSide side) {
    if (!market_data_provider_) {
        return 0;
    }

    auto market_view = market_data_provider_->getAggregatedView(symbol);
    if (side == OrderSide::BUY) {
        return market_view.best_ask > 0 ? market_view.best_ask : market_view.weighted_mid;
    } else {
        return market_view.best_bid > 0 ? market_view.best_bid : market_view.weighted_mid;
    }
}

void OrderManagementSystem::runOrderProcessing() {
    while (is_running_.load()) {
        try {
            // Process pending order submissions
            std::queue<LiveOrder> orders_to_process;
            {
                std::lock_guard<std::mutex> lock(execution_mutex_);
                orders_to_process.swap(pending_submissions_);
            }

            while (!orders_to_process.empty()) {
                auto order = orders_to_process.front();
                orders_to_process.pop();

                // Submit order to exchange
                bool submitted = submitOrderToExchange(order);
                if (submitted) {
                    // Update order status
                    {
                        std::lock_guard<std::mutex> lock(orders_mutex_);
                        if (active_orders_.find(order.order_id) != active_orders_.end()) {
                            active_orders_[order.order_id].status = OrderStatus::SUBMITTED;
                            active_orders_[order.order_id].submitted_at = std::chrono::system_clock::now();
                        }
                    }
                } else {
                    // Mark order as failed
                    {
                        std::lock_guard<std::mutex> lock(orders_mutex_);
                        if (active_orders_.find(order.order_id) != active_orders_.end()) {
                            active_orders_[order.order_id].status = OrderStatus::FAILED;
                            active_orders_[order.order_id].last_error_message = "Failed to submit to exchange";
                        }
                    }
                    daily_performance_.failed_orders_today++;
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds{100}); // Process every 100ms

        } catch (const std::exception& e) {
            std::cerr << "Order processing error: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds{1});
        }
    }
}

bool OrderManagementSystem::submitOrderToExchange(LiveOrder& order) {
    // Select exchange if not specified
    if (order.exchange.empty()) {
        order.exchange = selectOptimalExchange(order.symbol, order.order_side, order.quantity);
    }

    if (order.exchange.empty()) {
        return false; // No suitable exchange found
    }

    // Simulate order submission (in production, this would make actual API calls)
    bool success = sendOrderToExchange(order.exchange, order);

    if (success) {
        // Simulate immediate or quick execution for demonstration
        simulateOrderExecution(order);
    }

    return success;
}

std::string OrderManagementSystem::selectOptimalExchange(const std::string& symbol,
                                                        OrderSide side,
                                                        double quantity) {
    // Simple exchange selection logic - in production this would be more sophisticated
    std::string best_exchange;
    double best_score = -1;

    for (const auto& [exchange_name, connection] : exchange_connections_) {
        if (!connection->is_connected) {
            continue;
        }

        // Simple scoring based on latency (lower is better)
        double score = 1000.0 / (connection->average_latency.count() + 1);

        if (score > best_score) {
            best_score = score;
            best_exchange = exchange_name;
        }
    }

    return best_exchange;
}

bool OrderManagementSystem::sendOrderToExchange(const std::string& exchange, const LiveOrder& order) {
    auto it = exchange_connections_.find(exchange);
    if (it == exchange_connections_.end()) {
        return false;
    }

    // In production, this would construct and send the actual API request
    // For simulation, we'll just return success with some probability of failure
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 1.0);

    return dis(gen) > 0.05; // 5% failure rate for simulation
}

void OrderManagementSystem::simulateOrderExecution(const LiveOrder& order) {
    // Simulate realistic order execution for demonstration
    std::thread([this, order]() {
        // Wait a bit to simulate network delay
        std::this_thread::sleep_for(std::chrono::milliseconds{50 + rand() % 200});

        // Create a fill for the order
        OrderFill fill;
        fill.fill_id = "FILL_" + order.order_id;
        fill.order_id = order.order_id;
        fill.fill_time = std::chrono::system_clock::now();

        if (order.order_type == OrderType::MARKET) {
            // Market orders fill completely
            fill.fill_quantity = order.quantity;
            fill.fill_price = getEstimatedMarketPrice(order.symbol, order.order_side);

            // Add some slippage
            double slippage_factor = (rand() % 20 - 10) / 10000.0; // ±0.1% slippage
            fill.fill_price *= (1.0 + slippage_factor);

        } else {
            // Limit orders might fill partially or completely
            double fill_probability = 0.8; // 80% chance to fill
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_real_distribution<> dis(0.0, 1.0);

            if (dis(gen) < fill_probability) {
                fill.fill_quantity = order.quantity; // Full fill for simplicity
                fill.fill_price = order.price;
            } else {
                return; // No fill
            }
        }

        // Calculate commission (simple 0.1% fee)
        fill.commission = fill.fill_quantity * fill.fill_price * 0.001;
        fill.commission_asset = "USD";

        // Set other fill properties
        fill.is_maker = (order.order_type != OrderType::MARKET);
        fill.order_to_fill_latency = std::chrono::duration_cast<std::chrono::milliseconds>(
            fill.fill_time - order.submitted_at);

        // Process the fill
        handleOrderFill(fill);

    }).detach();
}

void OrderManagementSystem::handleOrderFill(const OrderFill& fill) {
    {
        std::lock_guard<std::mutex> lock(fills_mutex_);
        recent_fills_.push_back(fill);

        // Limit fill history size
        if (recent_fills_.size() > 10000) {
            recent_fills_.erase(recent_fills_.begin());
        }
    }

    // Update order status
    {
        std::lock_guard<std::mutex> lock(orders_mutex_);
        auto it = active_orders_.find(fill.order_id);
        if (it != active_orders_.end()) {
            auto& order = it->second;

            order.filled_quantity += fill.fill_quantity;
            order.remaining_quantity = order.quantity - order.filled_quantity;
            order.total_commission += fill.commission;

            // Update average fill price
            double total_value = order.average_fill_price * (order.filled_quantity - fill.fill_quantity) +
                               fill.fill_price * fill.fill_quantity;
            order.average_fill_price = total_value / order.filled_quantity;

            // Update order status
            if (order.remaining_quantity <= 0.000001) { // Allow for floating point precision
                order.status = OrderStatus::FILLED;
                order.completed_at = fill.fill_time;

                // Move to completed orders
                completed_orders_.push_back(order);
                active_orders_.erase(it);

                daily_performance_.successful_orders_today++;
            } else {
                order.status = OrderStatus::PARTIALLY_FILLED;
            }

            order.last_updated_at = fill.fill_time;

            // Calculate execution metrics
            if (order.expected_price > 0) {
                order.slippage_bps = calculateSlippage(order.expected_price, fill.fill_price, order.order_side);
            }

            order.execution_latency = fill.order_to_fill_latency;
        }
    }

    // Update performance metrics
    updateDailyPerformanceMetrics();

    // Trigger callback
    if (fill_callback_) {
        fill_callback_(fill);
    }

    std::cout << "Order fill processed: " << fill.order_id << " - "
              << fill.fill_quantity << " @ " << fill.fill_price << std::endl;

    // Update risk manager
    updateRiskManagerWithFill(fill);
}

void OrderManagementSystem::updateRiskManagerWithFill(const OrderFill& fill) {
    if (!risk_manager_) {
        return;
    }

    // Get the original order
    LiveOrder order;
    {
        std::lock_guard<std::mutex> lock(orders_mutex_);
        auto it = active_orders_.find(fill.order_id);
        if (it != active_orders_.end()) {
            order = it->second;
        } else {
            // Check completed orders
            auto completed_it = std::find_if(completed_orders_.begin(), completed_orders_.end(),
                [&fill](const LiveOrder& o) { return o.order_id == fill.order_id; });
            if (completed_it != completed_orders_.end()) {
                order = *completed_it;
            }
        }
    }

    if (order.order_id.empty()) {
        return; // Order not found
    }

    // Create or update position in risk manager
    Risk::ProductionRiskManager::LivePosition position;
    position.position_id = order.order_id; // Use order ID as position ID for simplicity
    position.symbol = order.symbol;
    position.quantity = (order.order_side == OrderSide::BUY) ? fill.fill_quantity : -fill.fill_quantity;
    position.entry_price = fill.fill_price;
    position.current_price = fill.fill_price;
    position.position_value_usd = std::abs(position.quantity) * fill.fill_price;
    position.is_long = (order.order_side == OrderSide::BUY);
    position.opened_at = fill.fill_time;

    // Add position to risk manager
    risk_manager_->addPosition(position);
}

void OrderManagementSystem::runExecutionMonitoring() {
    while (is_running_.load()) {
        try {
            // Monitor active orders for timeouts, cancellations, etc.
            std::vector<std::string> orders_to_check;
            {
                std::lock_guard<std::mutex> lock(orders_mutex_);
                for (const auto& [order_id, order] : active_orders_) {
                    if (order.status == OrderStatus::SUBMITTED ||
                        order.status == OrderStatus::PARTIALLY_FILLED) {
                        orders_to_check.push_back(order_id);
                    }
                }
            }

            // Check each active order
            for (const auto& order_id : orders_to_check) {
                // In production, this would query the exchange for order status
                // For simulation, we'll just track time-based expirations
                checkOrderExpiration(order_id);
            }

            std::this_thread::sleep_for(std::chrono::seconds{5}); // Check every 5 seconds

        } catch (const std::exception& e) {
            std::cerr << "Execution monitoring error: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds{5});
        }
    }
}

void OrderManagementSystem::checkOrderExpiration(const std::string& order_id) {
    std::lock_guard<std::mutex> lock(orders_mutex_);
    auto it = active_orders_.find(order_id);
    if (it == active_orders_.end()) {
        return;
    }

    auto& order = it->second;
    auto now = std::chrono::system_clock::now();

    // Check for expiration based on TimeInForce
    bool should_expire = false;

    if (order.time_in_force == TimeInForce::DAY) {
        // Check if it's past end of trading day (simplified check)
        auto time_since_creation = now - order.created_at;
        if (time_since_creation > std::chrono::hours{8}) { // 8 hour trading day
            should_expire = true;
        }
    } else if (order.time_in_force == TimeInForce::GTD) {
        if (now > order.expires_at) {
            should_expire = true;
        }
    }

    if (should_expire) {
        order.status = OrderStatus::EXPIRED;
        order.completed_at = now;
        order.last_updated_at = now;

        // Move to completed orders
        completed_orders_.push_back(order);
        active_orders_.erase(it);

        std::cout << "Order expired: " << order_id << std::endl;
    }
}

void OrderManagementSystem::runFillProcessing() {
    while (is_running_.load()) {
        try {
            // Process any additional fill-related tasks
            // In a real system, this might handle fill confirmations, trade reporting, etc.

            std::this_thread::sleep_for(std::chrono::seconds{1});

        } catch (const std::exception& e) {
            std::cerr << "Fill processing error: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds{1});
        }
    }
}

void OrderManagementSystem::updateDailyPerformanceMetrics() {
    std::lock_guard<std::mutex> lock(fills_mutex_);

    if (recent_fills_.empty()) {
        return;
    }

    // Calculate average slippage
    double total_slippage = 0;
    int slippage_count = 0;
    std::chrono::milliseconds total_exec_time{0};
    double total_commission = 0;
    int maker_count = 0;

    for (const auto& fill : recent_fills_) {
        if (fill.order_to_fill_latency > std::chrono::milliseconds{0}) {
            total_exec_time += fill.order_to_fill_latency;
        }
        total_commission += fill.commission;

        if (fill.is_maker) {
            maker_count++;
        }
    }

    if (!recent_fills_.empty()) {
        daily_performance_.average_execution_time = total_exec_time / recent_fills_.size();
        daily_performance_.total_commission_paid = total_commission;
        daily_performance_.maker_rate = static_cast<double>(maker_count) / recent_fills_.size();
    }

    if (daily_performance_.total_orders_today > 0) {
        daily_performance_.fill_rate = static_cast<double>(daily_performance_.successful_orders_today) /
                                      daily_performance_.total_orders_today;
        daily_performance_.error_rate = static_cast<double>(daily_performance_.failed_orders_today) /
                                       daily_performance_.total_orders_today;
    }
}

// Query methods implementation
OrderManagementSystem::LiveOrder OrderManagementSystem::getOrder(const std::string& order_id) const {
    std::lock_guard<std::mutex> lock(orders_mutex_);

    auto it = active_orders_.find(order_id);
    if (it != active_orders_.end()) {
        return it->second;
    }

    // Check completed orders
    auto completed_it = std::find_if(completed_orders_.begin(), completed_orders_.end(),
        [&order_id](const LiveOrder& order) { return order.order_id == order_id; });

    return (completed_it != completed_orders_.end()) ? *completed_it : LiveOrder{};
}

std::vector<OrderManagementSystem::LiveOrder> OrderManagementSystem::getActiveOrders() const {
    std::lock_guard<std::mutex> lock(orders_mutex_);

    std::vector<LiveOrder> orders;
    for (const auto& [id, order] : active_orders_) {
        orders.push_back(order);
    }
    return orders;
}

std::vector<OrderManagementSystem::OrderFill>
OrderManagementSystem::getOrderFills(const std::string& order_id) const {
    std::lock_guard<std::mutex> lock(fills_mutex_);

    std::vector<OrderFill> fills;
    for (const auto& fill : recent_fills_) {
        if (fill.order_id == order_id) {
            fills.push_back(fill);
        }
    }
    return fills;
}

OrderManagementSystem::PerformanceMetrics
OrderManagementSystem::getDailyPerformanceMetrics() const {
    return daily_performance_;
}

// Static utility methods
std::string OrderManagementSystem::orderTypeToString(OrderType type) {
    switch (type) {
        case OrderType::MARKET: return "MARKET";
        case OrderType::LIMIT: return "LIMIT";
        case OrderType::STOP_LOSS: return "STOP_LOSS";
        case OrderType::TAKE_PROFIT: return "TAKE_PROFIT";
        case OrderType::STOP_LIMIT: return "STOP_LIMIT";
        case OrderType::ICEBERG: return "ICEBERG";
        case OrderType::TWAP: return "TWAP";
        case OrderType::VWAP: return "VWAP";
        default: return "UNKNOWN";
    }
}

std::string OrderManagementSystem::orderStatusToString(OrderStatus status) {
    switch (status) {
        case OrderStatus::PENDING: return "PENDING";
        case OrderStatus::SUBMITTED: return "SUBMITTED";
        case OrderStatus::PARTIALLY_FILLED: return "PARTIALLY_FILLED";
        case OrderStatus::FILLED: return "FILLED";
        case OrderStatus::CANCELLED: return "CANCELLED";
        case OrderStatus::REJECTED: return "REJECTED";
        case OrderStatus::EXPIRED: return "EXPIRED";
        case OrderStatus::FAILED: return "FAILED";
        default: return "UNKNOWN";
    }
}

std::string OrderManagementSystem::generateOrderId() {
    static int order_counter = 0;
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::ostringstream oss;
    oss << "ORD_" << time_t << "_" << (++order_counter);
    return oss.str();
}

double OrderManagementSystem::calculateSlippage(double expected_price, double actual_price, OrderSide side) {
    if (expected_price <= 0) return 0.0;

    double price_diff = actual_price - expected_price;
    if (side == OrderSide::SELL) {
        price_diff = -price_diff; // For sells, lower prices are worse
    }

    return (price_diff / expected_price) * 10000.0; // Return in basis points
}

} // namespace Trading
} // namespace CryptoClaude