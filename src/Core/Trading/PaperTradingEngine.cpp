#include "PaperTradingEngine.h"
#include <cmath>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <fstream>
#include <random>

namespace CryptoClaude {
namespace Trading {

PaperTradingEngine::PaperTradingEngine() {
    std::cout << "PaperTradingEngine: Initialized" << std::endl;
}

PaperTradingEngine::~PaperTradingEngine() {
    shutdown();
}

bool PaperTradingEngine::initialize(std::shared_ptr<Database::DatabaseManager> db_manager,
                                   const PaperTradingConfig& config) {
    db_manager_ = db_manager;
    config_ = config;

    if (!db_manager_) {
        std::cerr << "PaperTradingEngine: Invalid database manager" << std::endl;
        return false;
    }

    // Initialize portfolio state
    cash_balance_.store(config_.initial_capital);
    realized_pnl_.store(0.0);

    // Create database tables
    if (!createPaperTradingTables()) {
        std::cerr << "PaperTradingEngine: Failed to create database tables" << std::endl;
        return false;
    }

    // Subscribe to configured symbols
    for (const auto& symbol : config_.symbols) {
        subscribeToSymbol(symbol);
    }

    std::cout << "PaperTradingEngine: Initialized with capital $"
              << std::fixed << std::setprecision(2) << config_.initial_capital
              << " and " << config_.symbols.size() << " symbols" << std::endl;

    return true;
}

void PaperTradingEngine::shutdown() {
    if (session_active_.load()) {
        stopTradingSession();
    }

    stop_background_threads_.store(true);

    if (market_processing_thread_.joinable()) {
        market_processing_thread_.join();
    }
    if (order_processing_thread_.joinable()) {
        order_processing_thread_.join();
    }
    if (performance_tracking_thread_.joinable()) {
        performance_tracking_thread_.join();
    }

    std::cout << "PaperTradingEngine: Shutdown complete" << std::endl;
}

bool PaperTradingEngine::startTradingSession(const std::string& session_name) {
    if (session_active_.load()) {
        std::cerr << "PaperTradingEngine: Session already active" << std::endl;
        return false;
    }

    current_session_name_ = session_name;
    session_start_time_ = std::chrono::system_clock::now();
    session_active_.store(true);
    stop_background_threads_.store(false);

    // Start background processing threads
    market_processing_thread_ = std::thread(&PaperTradingEngine::marketProcessingLoop, this);
    order_processing_thread_ = std::thread(&PaperTradingEngine::orderProcessingLoop, this);
    performance_tracking_thread_ = std::thread(&PaperTradingEngine::performanceTrackingLoop, this);

    // Save initial portfolio snapshot
    calculateAndSaveSnapshot();

    std::cout << "PaperTradingEngine: Trading session '" << session_name
              << "' started with $" << std::fixed << std::setprecision(2)
              << cash_balance_.load() << " capital" << std::endl;

    return true;
}

void PaperTradingEngine::stopTradingSession() {
    if (!session_active_.load()) {
        return;
    }

    session_active_.store(false);
    stop_background_threads_.store(true);

    // Close all open positions
    std::lock_guard<std::mutex> lock(positions_mutex_);
    for (auto& [symbol, position] : positions_) {
        if (position.status == "OPEN") {
            closePosition(symbol);
        }
    }

    // Save final session state
    saveSessionState();
    calculateAndSaveSnapshot();

    std::cout << "PaperTradingEngine: Trading session '" << current_session_name_
              << "' stopped. Final equity: $" << std::fixed << std::setprecision(2)
              << getTotalEquity() << std::endl;
}

void PaperTradingEngine::updateMarketData(const LiveMarketData& market_data) {
    std::lock_guard<std::mutex> lock(market_data_mutex_);
    market_data_[market_data.symbol] = market_data;

    // Update unrealized P&L for all positions
    updateUnrealizedPnL();
}

void PaperTradingEngine::subscribeToSymbol(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(market_data_mutex_);

    // Initialize with sample market data
    LiveMarketData sample_data;
    sample_data.symbol = symbol;
    sample_data.timestamp = std::chrono::system_clock::now();

    // Set realistic crypto prices
    if (symbol == "BTC-USD") {
        sample_data.last_price = 40000.0;
        sample_data.bid = 39990.0;
        sample_data.ask = 40010.0;
    } else if (symbol == "ETH-USD") {
        sample_data.last_price = 2500.0;
        sample_data.bid = 2498.0;
        sample_data.ask = 2502.0;
    } else {
        sample_data.last_price = 100.0;
        sample_data.bid = 99.5;
        sample_data.ask = 100.5;
    }

    sample_data.volume_24h = 1000000.0;
    sample_data.price_change_24h = 0.0;

    market_data_[symbol] = sample_data;

    std::cout << "PaperTradingEngine: Subscribed to " << symbol
              << " at $" << std::fixed << std::setprecision(2) << sample_data.last_price << std::endl;
}

std::string PaperTradingEngine::placeOrder(const std::string& symbol, const std::string& side,
                                          const std::string& order_type, double quantity, double price) {
    if (!session_active_.load()) {
        std::cerr << "PaperTradingEngine: No active trading session" << std::endl;
        return "";
    }

    std::string order_id = generateOrderId();
    VirtualOrder order(order_id, symbol, side, order_type, quantity, price);

    // Validate order
    if (!validateOrderRisk(order)) {
        order.status = "REJECTED";
        order.rejection_reason = "Risk limits exceeded";

        std::lock_guard<std::mutex> lock(orders_mutex_);
        orders_[order_id] = order;
        return order_id;
    }

    // Add to orders map
    {
        std::lock_guard<std::mutex> lock(orders_mutex_);
        orders_[order_id] = order;
    }

    // Queue for processing
    {
        std::lock_guard<std::mutex> queue_lock(order_queue_mutex_);
        order_processing_queue_.push(order_id);
    }

    std::cout << "PaperTradingEngine: Order placed - " << order_id
              << " " << side << " " << quantity << " " << symbol
              << " @ " << (order_type == "MARKET" ? "MARKET" : std::to_string(price)) << std::endl;

    return order_id;
}

VirtualPortfolioSnapshot PaperTradingEngine::getPortfolioSnapshot() {
    VirtualPortfolioSnapshot snapshot;
    snapshot.timestamp = std::chrono::system_clock::now();
    snapshot.cash_balance = cash_balance_.load();
    snapshot.realized_pnl = realized_pnl_.load();

    std::lock_guard<std::mutex> lock(positions_mutex_);
    double total_position_value = 0.0;
    double unrealized_pnl = 0.0;

    for (const auto& [symbol, position] : positions_) {
        if (position.status == "OPEN") {
            double current_price = getMarketPrice(symbol, position.direction == "LONG" ? "SELL" : "BUY");
            double position_value = position.quantity * current_price;
            double position_pnl = position.unrealized_pnl;

            snapshot.position_values[symbol] = position_value;
            snapshot.positions[symbol] = position;
            total_position_value += position_value;
            unrealized_pnl += position_pnl;
        }
    }

    snapshot.unrealized_pnl = unrealized_pnl;
    snapshot.total_pnl = snapshot.realized_pnl + snapshot.unrealized_pnl;
    snapshot.total_equity = snapshot.cash_balance + total_position_value;

    return snapshot;
}

double PaperTradingEngine::getTotalEquity() {
    auto snapshot = getPortfolioSnapshot();
    return snapshot.total_equity;
}

void PaperTradingEngine::processStrategySignal(const Analytics::TradingSignal& signal) {
    if (!auto_execute_signals_.load() || !session_active_.load()) {
        return;
    }

    double quantity_value = getTotalEquity() * signal.suggested_position_size * signal.strength;
    double current_price = getMarketPrice(signal.symbol, signal.action);

    if (current_price > 0) {
        double quantity = quantity_value / current_price;

        std::string order_id = placeOrder(signal.symbol, signal.action, "MARKET", quantity);

        std::cout << "PaperTradingEngine: Strategy signal executed - "
                  << signal.action << " " << quantity << " " << signal.symbol
                  << " (strength: " << signal.strength << ")" << std::endl;
    }
}

void PaperTradingEngine::marketProcessingLoop() {
    while (!stop_background_threads_.load()) {
        if (session_active_.load()) {
            // Simulate market data updates with realistic price movements
            std::lock_guard<std::mutex> lock(market_data_mutex_);

            for (auto& [symbol, data] : market_data_) {
                // Generate realistic price movement (0.1% to 0.5% per update)
                std::random_device rd;
                std::mt19937 gen(rd());
                std::normal_distribution<> price_change(0.0, 0.002); // 0.2% volatility per update

                double change = price_change(gen);
                data.last_price *= (1.0 + change);
                data.bid = data.last_price - (data.last_price * 0.0001); // 1bp spread
                data.ask = data.last_price + (data.last_price * 0.0001);
                data.timestamp = std::chrono::system_clock::now();
                data.price_change_24h += change;
            }

            updateUnrealizedPnL();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // Update every second
    }
}

void PaperTradingEngine::orderProcessingLoop() {
    while (!stop_background_threads_.load()) {
        std::string order_id;

        {
            std::lock_guard<std::mutex> queue_lock(order_queue_mutex_);
            if (!order_processing_queue_.empty()) {
                order_id = order_processing_queue_.front();
                order_processing_queue_.pop();
            }
        }

        if (!order_id.empty()) {
            processOrder(order_id);
        }

        // Check for stop-loss and take-profit triggers
        if (session_active_.load()) {
            checkAndExecuteStopLoss();
            checkAndExecuteTakeProfit();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void PaperTradingEngine::performanceTrackingLoop() {
    while (!stop_background_threads_.load()) {
        if (session_active_.load()) {
            calculateAndSaveSnapshot();
        }

        std::this_thread::sleep_for(std::chrono::minutes(1)); // Save snapshot every minute
    }
}

void PaperTradingEngine::processOrder(const std::string& order_id) {
    std::lock_guard<std::mutex> lock(orders_mutex_);
    auto it = orders_.find(order_id);
    if (it == orders_.end()) return;

    VirtualOrder& order = it->second;
    if (order.status != "PENDING") return;

    bool executed = false;

    if (order.order_type == "MARKET") {
        executed = executeMarketOrder(order);
    } else if (order.order_type == "LIMIT") {
        executed = executeLimitOrder(order);
    } else if (order.order_type == "STOP") {
        executed = executeStopOrder(order);
    }

    if (executed) {
        // Update position
        updatePosition(order.symbol, order.side, order.filled_quantity, order.average_fill_price);

        // Save order to database
        saveOrder(order);

        std::cout << "PaperTradingEngine: Order executed - " << order_id
                  << " filled " << order.filled_quantity << " @ $"
                  << std::fixed << std::setprecision(2) << order.average_fill_price << std::endl;
    }
}

bool PaperTradingEngine::executeMarketOrder(VirtualOrder& order) {
    double market_price = getMarketPrice(order.symbol, order.side);
    if (market_price <= 0) {
        order.status = "REJECTED";
        order.rejection_reason = "No market price available";
        return false;
    }

    // Apply slippage
    double fill_price = simulateSlippage(order.symbol, order.side, order.quantity);
    fillOrder(order, fill_price, order.quantity);

    return true;
}

bool PaperTradingEngine::executeLimitOrder(VirtualOrder& order) {
    double current_price = getMarketPrice(order.symbol, order.side);
    if (current_price <= 0) return false;

    // Check if limit price is reached
    bool can_fill = false;
    if (order.side == "BUY" && current_price <= order.price) {
        can_fill = true;
    } else if (order.side == "SELL" && current_price >= order.price) {
        can_fill = true;
    }

    if (can_fill) {
        fillOrder(order, order.price, order.quantity);
        return true;
    }

    return false;
}

void PaperTradingEngine::fillOrder(VirtualOrder& order, double fill_price, double fill_quantity) {
    order.status = "FILLED";
    order.filled_quantity = fill_quantity;
    order.average_fill_price = fill_price;
    order.filled_time = std::chrono::system_clock::now();

    // Calculate trade value and commission
    double trade_value = fill_quantity * fill_price;
    double commission = calculateCommission(trade_value);

    // Update cash balance
    if (order.side == "BUY") {
        double current = cash_balance_.load();
        while (!cash_balance_.compare_exchange_weak(current, current - (trade_value + commission)));
    } else { // SELL
        double current = cash_balance_.load();
        while (!cash_balance_.compare_exchange_weak(current, current + (trade_value - commission)));
    }
}

void PaperTradingEngine::updatePosition(const std::string& symbol, const std::string& side,
                                       double quantity, double price) {
    std::lock_guard<std::mutex> lock(positions_mutex_);

    auto it = positions_.find(symbol);
    if (it == positions_.end()) {
        // Create new position
        std::string position_id = generatePositionId(symbol);
        std::string direction = (side == "BUY") ? "LONG" : "SHORT";
        positions_[symbol] = VirtualPosition(position_id, symbol, direction, quantity, price);
    } else {
        // Update existing position
        VirtualPosition& position = it->second;

        if ((side == "BUY" && position.direction == "LONG") ||
            (side == "SELL" && position.direction == "SHORT")) {
            // Add to position
            double total_value = (position.quantity * position.entry_price) + (quantity * price);
            position.quantity += quantity;
            position.entry_price = total_value / position.quantity;
        } else {
            // Reduce or reverse position
            if (quantity >= position.quantity) {
                // Close position entirely
                double pnl = 0.0;
                if (position.direction == "LONG") {
                    pnl = (price - position.entry_price) * position.quantity;
                } else {
                    pnl = (position.entry_price - price) * position.quantity;
                }

                double current_pnl = realized_pnl_.load();
                while (!realized_pnl_.compare_exchange_weak(current_pnl, current_pnl + pnl));
                position.realized_pnl = pnl;
                position.status = "CLOSED";
                position.quantity = 0.0;

                // If there's remaining quantity, create new position in opposite direction
                double remaining = quantity - position.quantity;
                if (remaining > 0) {
                    std::string new_direction = (side == "BUY") ? "LONG" : "SHORT";
                    positions_[symbol] = VirtualPosition(generatePositionId(symbol), symbol,
                                                       new_direction, remaining, price);
                }
            } else {
                // Partial close
                double close_pnl = 0.0;
                if (position.direction == "LONG") {
                    close_pnl = (price - position.entry_price) * quantity;
                } else {
                    close_pnl = (position.entry_price - price) * quantity;
                }

                double current_pnl = realized_pnl_.load();
                while (!realized_pnl_.compare_exchange_weak(current_pnl, current_pnl + close_pnl));
                position.realized_pnl += close_pnl;
                position.quantity -= quantity;
            }
        }

        savePosition(position);
    }
}

void PaperTradingEngine::updateUnrealizedPnL() {
    std::lock_guard<std::mutex> lock(positions_mutex_);

    for (auto& [symbol, position] : positions_) {
        if (position.status == "OPEN") {
            double current_price = getMarketPrice(symbol, position.direction == "LONG" ? "SELL" : "BUY");
            if (current_price > 0) {
                if (position.direction == "LONG") {
                    position.unrealized_pnl = (current_price - position.entry_price) * position.quantity;
                } else {
                    position.unrealized_pnl = (position.entry_price - current_price) * position.quantity;
                }
            }
        }
    }
}

bool PaperTradingEngine::validateOrderRisk(const VirtualOrder& order) {
    double current_price = getMarketPrice(order.symbol, order.side);
    if (current_price <= 0) return false;

    double order_value = order.quantity * current_price;
    double total_equity = getTotalEquity();

    // Check position size limit
    if (order_value > total_equity * config_.max_position_size) {
        return false;
    }

    // Check available cash for buy orders
    if (order.side == "BUY" && order_value > cash_balance_.load()) {
        return false;
    }

    // Check maximum positions limit
    std::lock_guard<std::mutex> lock(positions_mutex_);
    if (positions_.size() >= static_cast<size_t>(config_.max_positions)) {
        // Allow if we're closing an existing position
        if (positions_.find(order.symbol) == positions_.end()) {
            return false;
        }
    }

    return true;
}

double PaperTradingEngine::getMarketPrice(const std::string& symbol, const std::string& side) {
    std::lock_guard<std::mutex> lock(market_data_mutex_);
    auto it = market_data_.find(symbol);
    if (it == market_data_.end()) return 0.0;

    const LiveMarketData& data = it->second;
    return (side == "BUY") ? data.ask : data.bid;
}

double PaperTradingEngine::simulateSlippage(const std::string& symbol, const std::string& side, double quantity) {
    double base_price = getMarketPrice(symbol, side);
    if (base_price <= 0) return 0.0;

    // Simulate market impact based on order size
    double market_impact = quantity * 0.0001; // 1bp per unit (simplified)
    double slippage_factor = (config_.slippage_rate + market_impact) / 10000.0;

    if (side == "BUY") {
        return base_price * (1.0 + slippage_factor);
    } else {
        return base_price * (1.0 - slippage_factor);
    }
}

double PaperTradingEngine::calculateCommission(double trade_value) {
    return trade_value * (config_.commission_rate / 10000.0);
}

void PaperTradingEngine::calculateAndSaveSnapshot() {
    auto snapshot = getPortfolioSnapshot();

    std::lock_guard<std::mutex> lock(performance_mutex_);
    performance_history_.push_back(snapshot);

    // Save to database
    savePerformanceSnapshot(snapshot);
}

bool PaperTradingEngine::createPaperTradingTables() {
    if (!db_manager_) return false;

    try {
        // Virtual orders table
        std::string create_orders_table = R"(
            CREATE TABLE IF NOT EXISTS paper_orders (
                order_id TEXT PRIMARY KEY,
                session_name TEXT NOT NULL,
                symbol TEXT NOT NULL,
                side TEXT NOT NULL,
                order_type TEXT NOT NULL,
                quantity REAL NOT NULL,
                price REAL,
                status TEXT NOT NULL,
                filled_quantity REAL DEFAULT 0,
                average_fill_price REAL DEFAULT 0,
                created_time TEXT NOT NULL,
                filled_time TEXT,
                rejection_reason TEXT
            )
        )";

        // Virtual positions table
        std::string create_positions_table = R"(
            CREATE TABLE IF NOT EXISTS paper_positions (
                position_id TEXT PRIMARY KEY,
                session_name TEXT NOT NULL,
                symbol TEXT NOT NULL,
                direction TEXT NOT NULL,
                quantity REAL NOT NULL,
                entry_price REAL NOT NULL,
                entry_time TEXT NOT NULL,
                unrealized_pnl REAL DEFAULT 0,
                realized_pnl REAL DEFAULT 0,
                status TEXT NOT NULL
            )
        )";

        // Performance snapshots table
        std::string create_snapshots_table = R"(
            CREATE TABLE IF NOT EXISTS paper_performance (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                session_name TEXT NOT NULL,
                timestamp TEXT NOT NULL,
                total_equity REAL NOT NULL,
                cash_balance REAL NOT NULL,
                unrealized_pnl REAL NOT NULL,
                realized_pnl REAL NOT NULL,
                total_pnl REAL NOT NULL
            )
        )";

        db_manager_->executeQuery(create_orders_table);
        db_manager_->executeQuery(create_positions_table);
        db_manager_->executeQuery(create_snapshots_table);

        return true;

    } catch (const std::exception& e) {
        std::cerr << "PaperTradingEngine: Error creating tables: " << e.what() << std::endl;
        return false;
    }
}

std::string PaperTradingEngine::generateOrderId() {
    return "ORDER_" + std::to_string(order_counter_.fetch_add(1)) + "_" +
           std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch()).count());
}

std::string PaperTradingEngine::generatePositionId(const std::string& symbol) {
    return "POS_" + symbol + "_" +
           std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch()).count());
}

std::string PaperTradingEngine::timePointToString(const std::chrono::system_clock::time_point& tp) {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

bool PaperTradingEngine::saveOrder(const VirtualOrder& order) {
    if (!db_manager_) return false;

    try {
        std::string query = R"(
            INSERT OR REPLACE INTO paper_orders (
                order_id, session_name, symbol, side, order_type, quantity, price,
                status, filled_quantity, average_fill_price, created_time, filled_time, rejection_reason
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        )";

        std::vector<std::string> params = {
            order.order_id,
            current_session_name_,
            order.symbol,
            order.side,
            order.order_type,
            std::to_string(order.quantity),
            std::to_string(order.price),
            order.status,
            std::to_string(order.filled_quantity),
            std::to_string(order.average_fill_price),
            timePointToString(order.created_time),
            order.status == "FILLED" ? timePointToString(order.filled_time) : "",
            order.rejection_reason
        };
        return db_manager_->executeParameterizedQuery(query, params);

    } catch (const std::exception& e) {
        std::cerr << "PaperTradingEngine: Error saving order: " << e.what() << std::endl;
        return false;
    }
}

bool PaperTradingEngine::savePosition(const VirtualPosition& position) {
    if (!db_manager_) return false;

    try {
        std::string query = R"(
            INSERT OR REPLACE INTO paper_positions (
                position_id, session_name, symbol, direction, quantity, entry_price,
                entry_time, unrealized_pnl, realized_pnl, status
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        )";

        std::vector<std::string> params = {
            position.position_id,
            current_session_name_,
            position.symbol,
            position.direction,
            std::to_string(position.quantity),
            std::to_string(position.entry_price),
            timePointToString(position.entry_time),
            std::to_string(position.unrealized_pnl),
            std::to_string(position.realized_pnl),
            position.status
        };
        return db_manager_->executeParameterizedQuery(query, params);

    } catch (const std::exception& e) {
        std::cerr << "PaperTradingEngine: Error saving position: " << e.what() << std::endl;
        return false;
    }
}

bool PaperTradingEngine::savePerformanceSnapshot(const VirtualPortfolioSnapshot& snapshot) {
    if (!db_manager_) return false;

    try {
        std::string query = R"(
            INSERT INTO paper_performance (
                session_name, timestamp, total_equity, cash_balance,
                unrealized_pnl, realized_pnl, total_pnl
            ) VALUES (?, ?, ?, ?, ?, ?, ?)
        )";

        std::vector<std::string> params = {
            current_session_name_,
            timePointToString(snapshot.timestamp),
            std::to_string(snapshot.total_equity),
            std::to_string(snapshot.cash_balance),
            std::to_string(snapshot.unrealized_pnl),
            std::to_string(snapshot.realized_pnl),
            std::to_string(snapshot.total_pnl)
        };
        return db_manager_->executeParameterizedQuery(query, params);

    } catch (const std::exception& e) {
        std::cerr << "PaperTradingEngine: Error saving snapshot: " << e.what() << std::endl;
        return false;
    }
}

void PaperTradingEngine::checkAndExecuteStopLoss() {
    // Implementation placeholder - would check positions against stop-loss levels
}

void PaperTradingEngine::checkAndExecuteTakeProfit() {
    // Implementation placeholder - would check positions against take-profit levels
}

bool PaperTradingEngine::saveSessionState() {
    return saveTradingData();
}

bool PaperTradingEngine::saveTradingData() {
    // Save current orders and positions to database
    std::lock_guard<std::mutex> orders_lock(orders_mutex_);
    for (const auto& [id, order] : orders_) {
        saveOrder(order);
    }

    std::lock_guard<std::mutex> positions_lock(positions_mutex_);
    for (const auto& [symbol, position] : positions_) {
        savePosition(position);
    }

    return true;
}

std::string PaperTradingEngine::generateTradingReport() {
    std::stringstream report;

    report << "=== PAPER TRADING SESSION REPORT ===" << std::endl;
    report << "Session: " << current_session_name_ << std::endl;
    report << "Status: " << (session_active_.load() ? "ACTIVE" : "COMPLETED") << std::endl;

    if (session_active_.load()) {
        report << "Started: " << timePointToString(session_start_time_) << std::endl;
    }

    report << std::endl;

    auto snapshot = getPortfolioSnapshot();
    report << "PORTFOLIO SUMMARY:" << std::endl;
    report << "  Total Equity: $" << std::fixed << std::setprecision(2) << snapshot.total_equity << std::endl;
    report << "  Cash Balance: $" << snapshot.cash_balance << std::endl;
    report << "  Unrealized P&L: $" << snapshot.unrealized_pnl << std::endl;
    report << "  Realized P&L: $" << snapshot.realized_pnl << std::endl;
    report << "  Total P&L: $" << snapshot.total_pnl << std::endl;
    report << "  Return: " << (snapshot.total_pnl / config_.initial_capital) * 100 << "%" << std::endl;

    return report.str();
}

double PaperTradingEngine::getWinRate() {
    std::lock_guard<std::mutex> orders_lock(orders_mutex_);

    int total_filled_trades = 0;
    int winning_trades = 0;

    for (const auto& [id, order] : orders_) {
        if (order.status == "FILLED" && (order.side == "SELL" || order.side == "BUY")) {
            total_filled_trades++;
            // Simple win detection: if this was a sell order and we had profit
            if (order.side == "SELL" && order.average_fill_price > order.price * 1.001) {
                winning_trades++;
            }
        }
    }

    return total_filled_trades > 0 ? (double)winning_trades / total_filled_trades : 0.0;
}

bool PaperTradingEngine::closePosition(const std::string& symbol, double quantity) {
    std::lock_guard<std::mutex> positions_lock(positions_mutex_);

    auto it = positions_.find(symbol);
    if (it == positions_.end()) {
        return false; // No position to close
    }

    VirtualPosition& position = it->second;
    double close_quantity = (quantity < 0) ? position.quantity : std::min(quantity, position.quantity);

    // Get current market price for closing
    auto market_data = getLatestMarketData(symbol);
    double close_price = market_data.last_price;

    // Calculate P&L
    double pnl = 0.0;
    if (position.direction == "LONG") {
        pnl = (close_price - position.entry_price) * close_quantity;
    } else {
        pnl = (position.entry_price - close_price) * close_quantity;
    }

    // Update realized P&L
    double current_pnl = realized_pnl_.load();
    while (!realized_pnl_.compare_exchange_weak(current_pnl, current_pnl + pnl));

    // Update cash balance
    double trade_value = close_price * close_quantity;
    double current_cash = cash_balance_.load();
    while (!cash_balance_.compare_exchange_weak(current_cash, current_cash + trade_value));

    // Update position
    position.quantity -= close_quantity;
    position.realized_pnl += pnl;

    // Remove position if fully closed
    if (position.quantity <= 0.0001) {
        positions_.erase(it);
    }

    return true;
}

int PaperTradingEngine::getTotalTrades() {
    std::lock_guard<std::mutex> orders_lock(orders_mutex_);

    int total_trades = 0;
    for (const auto& [id, order] : orders_) {
        if (order.status == "FILLED") {
            total_trades++;
        }
    }

    return total_trades;
}

std::vector<VirtualOrder> PaperTradingEngine::getOrderHistory() {
    std::lock_guard<std::mutex> orders_lock(orders_mutex_);

    std::vector<VirtualOrder> history;
    history.reserve(orders_.size());

    for (const auto& [id, order] : orders_) {
        history.push_back(order);
    }

    // Sort by creation time (most recent first)
    std::sort(history.begin(), history.end(),
        [](const VirtualOrder& a, const VirtualOrder& b) {
            return a.created_time > b.created_time;
        });

    return history;
}

bool PaperTradingEngine::executeStopOrder(VirtualOrder& order) {
    // Get current market price
    auto market_data = getLatestMarketData(order.symbol);
    double current_price = market_data.last_price;

    // Check if stop condition is met
    bool should_execute = false;
    if (order.side == "BUY" && current_price >= order.stop_price) {
        should_execute = true;
    } else if (order.side == "SELL" && current_price <= order.stop_price) {
        should_execute = true;
    }

    if (!should_execute) {
        return false;
    }

    // Execute as market order
    order.status = "FILLED";
    order.filled_quantity = order.quantity;
    order.average_fill_price = current_price;
    order.filled_time = std::chrono::system_clock::now();

    // Update position
    updatePosition(order.symbol, order.side, order.quantity, current_price);

    // Update cash balance
    double trade_value = current_price * order.quantity;
    double commission = trade_value * (config_.commission_rate / 10000.0);

    if (order.side == "BUY") {
        double current = cash_balance_.load();
        while (!cash_balance_.compare_exchange_weak(current, current - (trade_value + commission)));
    } else { // SELL
        double current = cash_balance_.load();
        while (!cash_balance_.compare_exchange_weak(current, current + (trade_value - commission)));
    }

    return true;
}

std::vector<VirtualPosition> PaperTradingEngine::getActivePositions() {
    std::lock_guard<std::mutex> positions_lock(positions_mutex_);

    std::vector<VirtualPosition> active_positions;
    active_positions.reserve(positions_.size());

    for (const auto& [symbol, position] : positions_) {
        if (position.quantity > 0.0001) { // Only include non-zero positions
            active_positions.push_back(position);
        }
    }

    return active_positions;
}

LiveMarketData PaperTradingEngine::getLatestMarketData(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(market_data_mutex_);

    auto it = market_data_.find(symbol);
    if (it != market_data_.end()) {
        return it->second;
    }

    // Return default market data if not found
    LiveMarketData default_data;
    default_data.symbol = symbol;
    default_data.last_price = 50000.0; // Default BTC price for testing
    default_data.bid = default_data.last_price - 0.5;
    default_data.ask = default_data.last_price + 0.5;
    default_data.volume_24h = 1000.0;
    default_data.timestamp = std::chrono::system_clock::now();

    return default_data;
}

}} // namespace CryptoClaude::Trading