#include "LiveTradingSimulator.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <random>
#include <fstream>
#include <sstream>
#include <iomanip>

namespace CryptoClaude {
namespace Algorithm {

LiveTradingSimulator::LiveTradingSimulator(const SimulationConfig& config)
    : config_(config), peak_portfolio_value_(config.initial_capital) {

    // Initialize portfolio
    current_portfolio_.initial_capital = config.initial_capital;
    current_portfolio_.current_capital = config.initial_capital;
    current_portfolio_.total_portfolio_value = config.initial_capital;
    current_portfolio_.timestamp = std::chrono::system_clock::now();
}

LiveTradingSimulator::~LiveTradingSimulator() {
    stopSimulation();
}

bool LiveTradingSimulator::startSimulation() {
    if (is_running_.load()) {
        return true; // Already running
    }

    // Validate component integration
    if (!data_manager_ || !signal_processor_ || !compliance_engine_ || !decision_engine_) {
        return false;
    }

    is_running_.store(true);
    simulation_start_time_ = std::chrono::system_clock::now();

    // Start core simulation thread
    simulation_thread_ = std::thread(&LiveTradingSimulator::runSimulation, this);

    // Start monitoring thread
    monitoring_thread_ = std::thread([this]() {
        while (is_running_.load()) {
            updatePortfolio();
            calculatePerformanceMetrics();
            executeRiskManagement();

            if (portfolio_callback_) {
                portfolio_callback_(current_portfolio_);
            }

            std::this_thread::sleep_for(std::chrono::seconds{10}); // Update every 10 seconds
        }
    });

    return true;
}

void LiveTradingSimulator::stopSimulation() {
    is_running_.store(false);

    if (simulation_thread_.joinable()) {
        simulation_thread_.join();
    }

    if (monitoring_thread_.joinable()) {
        monitoring_thread_.join();
    }

    // Close all open positions
    manualCloseAllPositions("simulation_stop");
}

void LiveTradingSimulator::runSimulation() {
    while (is_running_.load()) {
        try {
            // Process new signals
            processSignals();

            // Update existing positions
            updatePositions();

            // Execute pending orders
            std::queue<PendingOrder> orders_to_process;
            {
                std::lock_guard<std::mutex> lock(position_mutex_);
                orders_to_process = pending_orders_;
                while (!pending_orders_.empty()) {
                    pending_orders_.pop();
                }
            }

            while (!orders_to_process.empty()) {
                auto order = orders_to_process.front();
                orders_to_process.pop();
                executeOrder(order);
            }

            // Sleep based on simulation mode
            if (config_.simulation_mode == SimulationConfig::Mode::ACCELERATED) {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(static_cast<int>(1000 / config_.acceleration_factor))
                );
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds{500}); // 2Hz processing
            }

        } catch (const std::exception& e) {
            // Log error and continue
            TradingEvent error_event;
            error_event.event_id = generateEventId();
            error_event.timestamp = std::chrono::system_clock::now();
            error_event.event_type = "simulation_error";
            error_event.execution_result = "Error: " + std::string(e.what());
            logTradingEvent(error_event);

            std::this_thread::sleep_for(std::chrono::seconds{5}); // Pause on error
        }
    }
}

void LiveTradingSimulator::processSignals() {
    if (!signal_processor_) return;

    auto active_signals = signal_processor_->getActiveSignals();

    for (const auto& signal : active_signals) {
        // Log signal event
        auto signal_event = createSignalEvent(signal);
        logTradingEvent(signal_event);

        // Check if we should act on this signal
        if (signal.confidence_score < config_.min_signal_confidence) {
            continue;
        }

        // Check risk constraints
        if (!passesRiskChecks(signal)) {
            TradingEvent risk_event;
            risk_event.event_id = generateEventId();
            risk_event.timestamp = std::chrono::system_clock::now();
            risk_event.event_type = "signal_rejected_risk";
            risk_event.pair_name = signal.pair_name;
            risk_event.signal_id = signal.signal_id;
            risk_event.execution_result = "Signal rejected due to risk constraints";
            logTradingEvent(risk_event);
            continue;
        }

        // Check if we already have a position in this pair
        bool has_position = false;
        {
            std::lock_guard<std::mutex> lock(position_mutex_);
            for (const auto& [pos_id, position] : open_positions_) {
                if (position.pair_name == signal.pair_name && position.is_open) {
                    has_position = true;
                    break;
                }
            }
        }

        if (has_position) {
            continue; // Skip if already have position in this pair
        }

        // Attempt to open position
        if (openPosition(signal)) {
            TradingEvent position_event;
            position_event.event_id = generateEventId();
            position_event.timestamp = std::chrono::system_clock::now();
            position_event.event_type = "position_opened";
            position_event.pair_name = signal.pair_name;
            position_event.signal_id = signal.signal_id;
            position_event.confidence_score = signal.confidence_score;
            position_event.was_executed = true;
            position_event.execution_result = "Position opened successfully";
            logTradingEvent(position_event);
        }
    }
}

bool LiveTradingSimulator::openPosition(const RealTimeSignalProcessor::LiveTradingSignal& signal) {
    std::lock_guard<std::mutex> lock(position_mutex_);

    // Check maximum concurrent positions
    if (open_positions_.size() >= static_cast<size_t>(config_.max_concurrent_positions)) {
        return false;
    }

    // Create new position
    SimulatedPosition position;
    position.position_id = generatePositionId(signal.pair_name);
    position.pair_name = signal.pair_name;
    position.opened_at = std::chrono::system_clock::now();
    position.originating_signal_id = signal.signal_id;
    position.signal_confidence = signal.confidence_score;
    position.predicted_return = signal.predicted_return;

    // Determine position direction
    if (signal.signal_strength > 0) {
        position.direction = AlgorithmDecisionEngine::DecisionType::BUY;
    } else {
        position.direction = AlgorithmDecisionEngine::DecisionType::SELL;
    }

    // Get current market price
    position.entry_price = data_manager_->getCurrentPrice(signal.pair_name);
    if (position.entry_price <= 0) {
        return false; // No valid price data
    }

    // Calculate position size
    position.position_size = calculatePositionSize(signal);
    position.position_value_usd = position.position_size * position.entry_price;

    // Simulate execution details
    std::random_device rd;
    std::mt19937 gen(rd());

    // Execution delay (50-200ms typical)
    std::uniform_int_distribution<int> delay_dist(50, 200);
    position.execution_delay = std::chrono::milliseconds(delay_dist(gen));

    // Calculate slippage
    std::normal_distribution<double> slippage_dist(0.0, 0.0005); // 0.05% average slippage
    double slippage_factor = slippage_dist(gen);
    double slippage_amount = position.entry_price * std::abs(slippage_factor);

    if (position.direction == AlgorithmDecisionEngine::DecisionType::BUY) {
        position.executed_price = position.entry_price + slippage_amount;
    } else {
        position.executed_price = position.entry_price - slippage_amount;
    }

    position.slippage_cost = std::abs(position.executed_price - position.entry_price) * position.position_size;

    // Calculate transaction costs
    position.transaction_cost = (position.position_value_usd * config_.base_transaction_cost_bps) / 10000.0;

    // Set stop loss and take profit levels
    if (config_.enable_stop_loss) {
        if (position.direction == AlgorithmDecisionEngine::DecisionType::BUY) {
            position.stop_loss_price = position.executed_price * (1.0 - config_.stop_loss_percentage);
        } else {
            position.stop_loss_price = position.executed_price * (1.0 + config_.stop_loss_percentage);
        }
    }

    if (config_.enable_take_profit) {
        if (position.direction == AlgorithmDecisionEngine::DecisionType::BUY) {
            position.take_profit_price = position.executed_price * (1.0 + config_.take_profit_percentage);
        } else {
            position.take_profit_price = position.executed_price * (1.0 - config_.take_profit_percentage);
        }
    }

    // Initialize tracking variables
    position.current_price = position.executed_price;
    position.current_value_usd = position.position_value_usd;
    position.max_favorable_excursion = 0.0;
    position.max_adverse_excursion = 0.0;
    position.is_open = true;

    // Update portfolio capital
    current_portfolio_.current_capital -= (position.position_value_usd + position.transaction_cost + position.slippage_cost);
    current_portfolio_.invested_capital += position.position_value_usd;

    // Add to open positions
    open_positions_[position.position_id] = position;

    // Callback notification
    if (position_callback_) {
        position_callback_(position);
    }

    return true;
}

void LiveTradingSimulator::updatePositions() {
    std::lock_guard<std::mutex> lock(position_mutex_);

    auto now = std::chrono::system_clock::now();

    for (auto& [pos_id, position] : open_positions_) {
        if (!position.is_open) continue;

        // Update current price
        double new_price = data_manager_->getCurrentPrice(position.pair_name);
        if (new_price > 0) {
            position.current_price = new_price;
            updatePositionPnL(position);

            // Update max favorable/adverse excursion
            if (position.direction == AlgorithmDecisionEngine::DecisionType::BUY) {
                double favorable = new_price - position.executed_price;
                double adverse = position.executed_price - new_price;

                if (favorable > position.max_favorable_excursion) {
                    position.max_favorable_excursion = favorable;
                }
                if (adverse > position.max_adverse_excursion) {
                    position.max_adverse_excursion = adverse;
                }
            } else {
                double favorable = position.executed_price - new_price;
                double adverse = new_price - position.executed_price;

                if (favorable > position.max_favorable_excursion) {
                    position.max_favorable_excursion = favorable;
                }
                if (adverse > position.max_adverse_excursion) {
                    position.max_adverse_excursion = adverse;
                }
            }
        }

        // Check for automatic position closure
        bool should_close = false;
        std::string close_reason;

        // Check maximum holding period
        auto holding_duration = std::chrono::duration_cast<std::chrono::hours>(now - position.opened_at);
        if (holding_duration >= config_.max_position_duration) {
            should_close = true;
            close_reason = "timeout";
        }

        // Check stop loss
        if (config_.enable_stop_loss && position.stop_loss_price > 0) {
            if ((position.direction == AlgorithmDecisionEngine::DecisionType::BUY && new_price <= position.stop_loss_price) ||
                (position.direction == AlgorithmDecisionEngine::DecisionType::SELL && new_price >= position.stop_loss_price)) {
                should_close = true;
                close_reason = "stop_loss";
            }
        }

        // Check take profit
        if (config_.enable_take_profit && position.take_profit_price > 0) {
            if ((position.direction == AlgorithmDecisionEngine::DecisionType::BUY && new_price >= position.take_profit_price) ||
                (position.direction == AlgorithmDecisionEngine::DecisionType::SELL && new_price <= position.take_profit_price)) {
                should_close = true;
                close_reason = "take_profit";
            }
        }

        if (should_close) {
            // Close position (will be handled after loop to avoid iterator invalidation)
            closePosition(pos_id, close_reason);
        }
    }
}

bool LiveTradingSimulator::closePosition(const std::string& position_id, const std::string& reason) {
    std::lock_guard<std::mutex> lock(position_mutex_);

    auto it = open_positions_.find(position_id);
    if (it == open_positions_.end() || !it->second.is_open) {
        return false;
    }

    SimulatedPosition& position = it->second;

    // Set exit details
    position.closed_at = std::chrono::system_clock::now();
    position.exit_price = position.current_price;
    position.exit_reason = reason;
    position.holding_period = std::chrono::duration_cast<std::chrono::hours>(
        position.closed_at - position.opened_at);

    // Calculate final P&L
    updatePositionPnL(position);
    position.realized_pnl = position.unrealized_pnl;
    position.realized_return_percentage = position.unrealized_return_percentage;

    // Add exit transaction costs
    double exit_transaction_cost = (position.current_value_usd * config_.base_transaction_cost_bps) / 10000.0;
    position.transaction_cost += exit_transaction_cost;
    position.realized_pnl -= exit_transaction_cost;

    // Update portfolio
    current_portfolio_.current_capital += position.current_value_usd - exit_transaction_cost;
    current_portfolio_.invested_capital -= position.position_value_usd;
    current_portfolio_.total_pnl += position.realized_pnl;

    // Mark as closed
    position.is_open = false;

    // Move to closed positions
    closed_positions_.push_back(position);
    open_positions_.erase(it);

    // Log position close event
    auto close_event = createPositionEvent(position, "closed");
    logTradingEvent(close_event);

    // Callback notification
    if (position_callback_) {
        position_callback_(position);
    }

    return true;
}

void LiveTradingSimulator::updatePositionPnL(SimulatedPosition& position) {
    if (!position.is_open) return;

    double current_value = position.position_size * position.current_price;
    position.current_value_usd = current_value;

    if (position.direction == AlgorithmDecisionEngine::DecisionType::BUY) {
        position.unrealized_pnl = current_value - position.position_value_usd;
    } else {
        position.unrealized_pnl = position.position_value_usd - current_value;
    }

    // Account for transaction costs
    position.unrealized_pnl -= (position.transaction_cost + position.slippage_cost);

    if (position.position_value_usd > 0) {
        position.unrealized_return_percentage = (position.unrealized_pnl / position.position_value_usd) * 100.0;
    }
}

bool LiveTradingSimulator::passesRiskChecks(const RealTimeSignalProcessor::LiveTradingSignal& signal) {
    // Check data quality
    if (signal.data_quality_score < 0.7) {
        return false;
    }

    // Check TRS compliance if enabled
    if (config_.enable_correlation_limits) {
        if (signal.real_time_correlation < 0.70) {
            return false;
        }
    }

    // Check portfolio risk limits
    double current_portfolio_risk = calculatePortfolioRisk();
    if (current_portfolio_risk >= config_.max_portfolio_risk) {
        return false;
    }

    // Check position size limits
    double position_size = calculatePositionSize(signal);
    double position_percentage = (position_size * data_manager_->getCurrentPrice(signal.pair_name))
                                / current_portfolio_.total_portfolio_value;

    if (position_percentage > config_.max_position_size) {
        return false;
    }

    return true;
}

double LiveTradingSimulator::calculatePositionSize(const RealTimeSignalProcessor::LiveTradingSignal& signal) {
    if (!config_.enable_position_sizing) {
        // Fixed position size
        return (current_portfolio_.total_portfolio_value * config_.max_position_size)
               / data_manager_->getCurrentPrice(signal.pair_name);
    }

    // Dynamic position sizing based on signal confidence and risk
    double base_size_percentage = config_.max_position_size * 0.5; // Start with half of max
    double confidence_multiplier = signal.confidence_score;
    double risk_adjustment = 1.0 - (signal.risk_score * 0.5); // Reduce size for risky signals

    double adjusted_percentage = base_size_percentage * confidence_multiplier * risk_adjustment;
    adjusted_percentage = std::min(adjusted_percentage, config_.max_position_size);

    double position_value = current_portfolio_.total_portfolio_value * adjusted_percentage;
    return position_value / data_manager_->getCurrentPrice(signal.pair_name);
}

double LiveTradingSimulator::calculatePortfolioRisk() {
    std::lock_guard<std::mutex> lock(position_mutex_);

    double total_risk = 0.0;

    for (const auto& [pos_id, position] : open_positions_) {
        if (position.is_open) {
            double position_risk = std::abs(position.unrealized_pnl) / current_portfolio_.total_portfolio_value;
            total_risk += position_risk;
        }
    }

    return total_risk;
}

void LiveTradingSimulator::updatePortfolio() {
    std::lock_guard<std::mutex> lock(portfolio_mutex_);

    current_portfolio_.timestamp = std::chrono::system_clock::now();

    // Update position counts
    current_portfolio_.open_positions = 0;
    double total_unrealized_pnl = 0.0;

    {
        std::lock_guard<std::mutex> pos_lock(position_mutex_);

        for (const auto& [pos_id, position] : open_positions_) {
            if (position.is_open) {
                current_portfolio_.open_positions++;
                total_unrealized_pnl += position.unrealized_pnl;
            }
        }
    }

    // Update portfolio values
    current_portfolio_.total_portfolio_value = current_portfolio_.current_capital +
                                             current_portfolio_.invested_capital +
                                             total_unrealized_pnl;

    current_portfolio_.total_return_percentage =
        ((current_portfolio_.total_portfolio_value - current_portfolio_.initial_capital)
         / current_portfolio_.initial_capital) * 100.0;

    // Update drawdown
    if (current_portfolio_.total_portfolio_value > peak_portfolio_value_) {
        peak_portfolio_value_ = current_portfolio_.total_portfolio_value;
    }

    current_portfolio_.current_drawdown =
        (peak_portfolio_value_ - current_portfolio_.total_portfolio_value) / peak_portfolio_value_;

    if (current_portfolio_.current_drawdown > current_portfolio_.max_drawdown) {
        current_portfolio_.max_drawdown = current_portfolio_.current_drawdown;
    }

    // Update TRS compliance from compliance engine
    if (compliance_engine_) {
        current_portfolio_.current_algorithm_correlation = compliance_engine_->getCurrentCorrelation();
        current_portfolio_.trs_status = compliance_engine_->getCurrentStatus();
        current_portfolio_.trs_gap = compliance_engine_->getCurrentTRSGap();
    }

    // Add to history
    portfolio_history_.push_back(current_portfolio_);
    while (portfolio_history_.size() > 2880) { // Keep 24 hours at 30-second intervals
        portfolio_history_.pop_front();
    }
}

void LiveTradingSimulator::executeRiskManagement() {
    // Check maximum drawdown
    if (current_portfolio_.current_drawdown > config_.max_portfolio_risk) {
        manualCloseAllPositions("risk_management_drawdown");
    }

    // Check TRS compliance
    if (config_.enable_correlation_limits && compliance_engine_) {
        if (compliance_engine_->getCurrentStatus() == TRSComplianceEngine::ComplianceStatus::EMERGENCY) {
            // Reduce position sizes or close positions
            std::lock_guard<std::mutex> lock(position_mutex_);

            // Close highest risk positions first
            std::vector<std::pair<std::string, double>> positions_by_risk;

            for (const auto& [pos_id, position] : open_positions_) {
                if (position.is_open) {
                    double risk = std::abs(position.unrealized_pnl) / current_portfolio_.total_portfolio_value;
                    positions_by_risk.push_back({pos_id, risk});
                }
            }

            std::sort(positions_by_risk.begin(), positions_by_risk.end(),
                     [](const auto& a, const auto& b) { return a.second > b.second; });

            // Close top 25% of risky positions
            size_t positions_to_close = std::max(1UL, positions_by_risk.size() / 4);
            for (size_t i = 0; i < positions_to_close && i < positions_by_risk.size(); ++i) {
                closePosition(positions_by_risk[i].first, "trs_compliance_risk");
            }
        }
    }
}

// Utility methods
std::string LiveTradingSimulator::generatePositionId(const std::string& pair_name) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << "POS_" << pair_name << "_" << time_t;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(1000, 9999);
    ss << "_" << dist(gen);

    return ss.str();
}

std::string LiveTradingSimulator::generateEventId() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << "EVT_" << time_t;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(10000, 99999);
    ss << "_" << dist(gen);

    return ss.str();
}

LiveTradingSimulator::TradingEvent
LiveTradingSimulator::createSignalEvent(const RealTimeSignalProcessor::LiveTradingSignal& signal) {
    TradingEvent event;
    event.event_id = generateEventId();
    event.timestamp = signal.generated_at;
    event.event_type = "signal_received";
    event.pair_name = signal.pair_name;
    event.signal_id = signal.signal_id;
    event.confidence_score = signal.confidence_score;
    event.market_price = data_manager_->getCurrentPrice(signal.pair_name);
    event.data_quality_score = signal.data_quality_score;
    event.decision_rationale = "Signal strength: " + std::to_string(signal.signal_strength);

    return event;
}

void LiveTradingSimulator::logTradingEvent(const TradingEvent& event) {
    std::lock_guard<std::mutex> lock(event_mutex_);

    trading_events_.push(event);

    // Keep reasonable number of events
    while (trading_events_.size() > 1000) {
        trading_events_.pop();
    }

    if (event_callback_) {
        event_callback_(event);
    }
}

// Public access methods
LiveTradingSimulator::PortfolioSnapshot LiveTradingSimulator::getCurrentPortfolio() const {
    std::lock_guard<std::mutex> lock(portfolio_mutex_);
    return current_portfolio_;
}

std::vector<LiveTradingSimulator::SimulatedPosition> LiveTradingSimulator::getOpenPositions() const {
    std::lock_guard<std::mutex> lock(position_mutex_);

    std::vector<SimulatedPosition> positions;
    for (const auto& [pos_id, position] : open_positions_) {
        if (position.is_open) {
            positions.push_back(position);
        }
    }

    return positions;
}

double LiveTradingSimulator::getCurrentReturn() const {
    return current_portfolio_.total_return_percentage;
}

double LiveTradingSimulator::getCurrentDrawdown() const {
    return current_portfolio_.current_drawdown;
}

bool LiveTradingSimulator::manualCloseAllPositions(const std::string& reason) {
    std::lock_guard<std::mutex> lock(position_mutex_);

    std::vector<std::string> position_ids;
    for (const auto& [pos_id, position] : open_positions_) {
        if (position.is_open) {
            position_ids.push_back(pos_id);
        }
    }

    for (const auto& pos_id : position_ids) {
        closePosition(pos_id, reason);
    }

    return true;
}

} // namespace Algorithm
} // namespace CryptoClaude