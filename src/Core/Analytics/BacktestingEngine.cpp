#include "BacktestingEngine.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <iomanip>
#include <sstream>
#include <iostream>

namespace CryptoClaude {
namespace Analytics {

BacktestingEngine::BacktestingEngine()
    : indicators_(std::make_unique<TechnicalIndicators>())
    , performance_engine_(std::make_unique<PerformanceAttributionEngine>())
    , starting_capital_(100000.0)
    , fixed_slippage_bps_(10.0)
    , commission_bps_(25.0) {
}

bool BacktestingEngine::initialize(std::shared_ptr<Database::DatabaseManager> db_manager) {
    db_manager_ = db_manager;
    if (!db_manager_) {
        std::cerr << "BacktestingEngine: Invalid database manager" << std::endl;
        return false;
    }

    // Create necessary database tables
    if (!createBacktestTables()) {
        std::cerr << "BacktestingEngine: Failed to create database tables" << std::endl;
        return false;
    }

    std::cout << "BacktestingEngine: Initialized successfully" << std::endl;
    return true;
}

void BacktestingEngine::setSlippageModel(double fixed_slippage_bps) {
    fixed_slippage_bps_ = fixed_slippage_bps;
}

void BacktestingEngine::setCommissionModel(double commission_bps) {
    commission_bps_ = commission_bps;
}

void BacktestingEngine::setStartingCapital(double capital) {
    starting_capital_ = capital;
}

bool BacktestingEngine::loadHistoricalData(const std::string& symbol,
                                         const std::chrono::system_clock::time_point& start_date,
                                         const std::chrono::system_clock::time_point& end_date) {
    // In a real implementation, this would fetch data from external APIs or database
    // For now, generate sample data for demonstration
    std::vector<MarketDataPoint> sample_data;

    auto current_time = start_date;
    double base_price = 40000.0; // Starting BTC price
    double price = base_price;

    // Generate hourly data points
    while (current_time < end_date) {
        // Simulate realistic price movement
        double volatility = 0.02; // 2% hourly volatility
        double random_change = (static_cast<double>(rand()) / RAND_MAX - 0.5) * volatility;

        double open = price;
        double high = open * (1.0 + std::abs(random_change) * 0.5);
        double low = open * (1.0 - std::abs(random_change) * 0.5);
        double close = open * (1.0 + random_change);
        double volume = 1000.0 + (static_cast<double>(rand()) / RAND_MAX) * 5000.0;

        sample_data.emplace_back(current_time, symbol, open, high, low, close, volume);

        price = close;
        current_time += std::chrono::hours(1);
    }

    historical_data_[symbol] = sample_data;
    std::cout << "BacktestingEngine: Loaded " << sample_data.size()
              << " data points for " << symbol << std::endl;

    return true;
}

bool BacktestingEngine::loadHistoricalDataFromDatabase(const std::vector<std::string>& symbols,
                                                     const std::chrono::system_clock::time_point& start_date,
                                                     const std::chrono::system_clock::time_point& end_date) {
    if (!db_manager_) {
        return false;
    }

    for (const auto& symbol : symbols) {
        try {
            std::string query = R"(
                SELECT timestamp, symbol, open, high, low, close, volume
                FROM market_data
                WHERE symbol = ? AND timestamp BETWEEN ? AND ?
                ORDER BY timestamp ASC
            )";

            // Use available database interface
            std::vector<std::string> params = {symbol, timePointToString(start_date), timePointToString(end_date)};
            auto results = db_manager_->executeSelectQuery(query, params);

            std::vector<MarketDataPoint> data_points;
            for (const auto& row : results) {
                MarketDataPoint point;
                if (row.find("timestamp") != row.end()) {
                    point.timestamp = stringToTimePoint(row.at("timestamp"));
                }
                if (row.find("symbol") != row.end()) {
                    point.symbol = row.at("symbol");
                }
                if (row.find("open") != row.end()) {
                    point.open = std::stod(row.at("open"));
                }
                if (row.find("high") != row.end()) {
                    point.high = std::stod(row.at("high"));
                }
                if (row.find("low") != row.end()) {
                    point.low = std::stod(row.at("low"));
                }
                if (row.find("close") != row.end()) {
                    point.close = std::stod(row.at("close"));
                }
                if (row.find("volume") != row.end()) {
                    point.volume = std::stod(row.at("volume"));
                }
                data_points.push_back(point);
            }

            if (!data_points.empty()) {
                historical_data_[symbol] = data_points;
                std::cout << "BacktestingEngine: Loaded " << data_points.size()
                          << " data points for " << symbol << " from database" << std::endl;
            } else {
                // Fallback to sample data generation
                loadHistoricalData(symbol, start_date, end_date);
            }

        } catch (const std::exception& e) {
            std::cerr << "BacktestingEngine: Error loading data for " << symbol
                      << ": " << e.what() << std::endl;
            // Fallback to sample data
            loadHistoricalData(symbol, start_date, end_date);
        }
    }

    return true;
}

BacktestResults BacktestingEngine::runBacktest(const std::string& strategy_name,
                                              const std::chrono::system_clock::time_point& start_date,
                                              const std::chrono::system_clock::time_point& end_date) {
    BacktestResults results;
    results.start_date = start_date;
    results.end_date = end_date;
    results.starting_capital = starting_capital_;

    std::cout << "BacktestingEngine: Running backtest for strategy '" << strategy_name
              << "' from " << timePointToString(start_date)
              << " to " << timePointToString(end_date) << std::endl;

    // Get market data for the period
    if (historical_data_.empty()) {
        std::vector<std::string> symbols = {"BTC-USD", "ETH-USD"};
        loadHistoricalDataFromDatabase(symbols, start_date, end_date);
    }

    // Generate trading signals
    std::vector<TradingSignal> signals;
    for (const auto& [symbol, data] : historical_data_) {
        if (data.empty()) continue;

        auto symbol_signals = generateSignals(data, end_date);
        signals.insert(signals.end(), symbol_signals.begin(), symbol_signals.end());
    }

    std::cout << "BacktestingEngine: Generated " << signals.size() << " trading signals" << std::endl;

    // Execute strategy and calculate trades
    std::vector<BacktestTrade> trades;
    if (!signals.empty()) {
        // Combine all market data for execution
        std::vector<MarketDataPoint> all_data;
        for (const auto& [symbol, data] : historical_data_) {
            all_data.insert(all_data.end(), data.begin(), data.end());
        }

        // Sort by timestamp
        std::sort(all_data.begin(), all_data.end(),
                 [](const MarketDataPoint& a, const MarketDataPoint& b) {
                     return a.timestamp < b.timestamp;
                 });

        trades = executeStrategy(signals, all_data);
    }

    std::cout << "BacktestingEngine: Executed " << trades.size() << " trades" << std::endl;

    // Calculate equity curve
    std::vector<double> equity_curve;
    std::vector<std::chrono::system_clock::time_point> timestamps;

    double current_capital = starting_capital_;
    equity_curve.push_back(current_capital);
    timestamps.push_back(start_date);

    // Build equity curve from trades
    for (const auto& trade : trades) {
        current_capital += trade.pnl - trade.fees;
        equity_curve.push_back(current_capital);
        timestamps.push_back(trade.exit_time);
    }

    results.ending_capital = current_capital;
    results.equity_curve = equity_curve;
    results.equity_timestamps = timestamps;

    // Calculate comprehensive performance metrics
    results = calculatePerformanceMetrics(trades, equity_curve, timestamps);
    results.starting_capital = starting_capital_;
    results.ending_capital = current_capital;

    // Save results to database
    saveBacktestResults(strategy_name, results);

    std::cout << "BacktestingEngine: Backtest completed. Total return: "
              << std::fixed << std::setprecision(2) << results.total_return * 100 << "%" << std::endl;

    return results;
}

std::vector<TradingSignal> BacktestingEngine::generateSignals(const std::vector<MarketDataPoint>& market_data,
                                                             const std::chrono::system_clock::time_point& current_time) {
    std::vector<TradingSignal> signals;

    if (market_data.size() < 50) {
        return signals; // Not enough data for signal generation
    }

    // Prepare price data for technical indicators
    std::vector<double> closes;
    for (const auto& point : market_data) {
        closes.push_back(point.close);
    }

    // Calculate moving averages using the correct interface
    auto sma_20 = indicators_->calculateSMA(closes, 20);
    auto sma_50 = indicators_->calculateSMA(closes, 50);
    auto rsi = indicators_->calculateRSI(closes, 14);

    // Generate signals based on crossover strategy with RSI filter
    for (size_t i = 50; i < market_data.size(); ++i) {
        if (i >= sma_20.size() || i >= sma_50.size() || i >= rsi.values.size()) continue;

        const auto& data_point = market_data[i];
        double current_rsi = (i < rsi.values.size()) ? rsi.values[i] : 50.0;

        // Golden Cross (SMA 20 crosses above SMA 50) with RSI confirmation
        if (i > 0 && sma_20.getValueAt(i) > sma_50.getValueAt(i) &&
            sma_20.getValueAt(i-1) <= sma_50.getValueAt(i-1) && current_rsi < 70) {
            TradingSignal signal(data_point.timestamp, data_point.symbol, "BUY", 0.8, 0.25);
            signal.indicators["SMA_20"] = sma_20.getValueAt(i);
            signal.indicators["SMA_50"] = sma_50.getValueAt(i);
            signal.indicators["RSI"] = current_rsi;
            signals.push_back(signal);
        }

        // Death Cross (SMA 20 crosses below SMA 50) with RSI confirmation
        else if (i > 0 && sma_20.getValueAt(i) < sma_50.getValueAt(i) &&
                 sma_20.getValueAt(i-1) >= sma_50.getValueAt(i-1) && current_rsi > 30) {
            TradingSignal signal(data_point.timestamp, data_point.symbol, "SELL", 0.8, 0.25);
            signal.indicators["SMA_20"] = sma_20.getValueAt(i);
            signal.indicators["SMA_50"] = sma_50.getValueAt(i);
            signal.indicators["RSI"] = current_rsi;
            signals.push_back(signal);
        }
    }

    return signals;
}

std::vector<BacktestTrade> BacktestingEngine::executeStrategy(const std::vector<TradingSignal>& signals,
                                                            const std::vector<MarketDataPoint>& market_data) {
    std::vector<BacktestTrade> trades;
    std::map<std::string, BacktestTrade> open_positions; // One position per symbol
    double current_capital = starting_capital_;

    for (const auto& signal : signals) {
        // Find corresponding market data point
        auto data_it = std::find_if(market_data.begin(), market_data.end(),
                                   [&signal](const MarketDataPoint& data) {
                                       return data.timestamp == signal.timestamp && data.symbol == signal.symbol;
                                   });

        if (data_it == market_data.end()) continue;

        const auto& market_point = *data_it;

        if (signal.action == "BUY" && open_positions.find(signal.symbol) == open_positions.end()) {
            // Open long position
            BacktestTrade trade;
            trade.trade_id = signal.symbol + "_" + std::to_string(trades.size());
            trade.entry_time = signal.timestamp;
            trade.symbol = signal.symbol;
            trade.direction = "LONG";
            trade.entry_price = applySlippageAndFees(market_point.close, market_point.volume, "BUY");

            double position_value = calculatePositionSize(signal, current_capital, trade.entry_price);
            trade.position_size = position_value / trade.entry_price;

            open_positions[signal.symbol] = trade;

        } else if (signal.action == "SELL" && open_positions.find(signal.symbol) != open_positions.end()) {
            // Close long position
            auto& open_trade = open_positions[signal.symbol];
            open_trade.exit_time = signal.timestamp;
            open_trade.exit_price = applySlippageAndFees(market_point.close, market_point.volume, "SELL");
            open_trade.exit_reason = "SIGNAL";

            // Calculate P&L
            open_trade.pnl = (open_trade.exit_price - open_trade.entry_price) * open_trade.position_size;
            open_trade.fees = (open_trade.entry_price + open_trade.exit_price) * open_trade.position_size * (commission_bps_ / 10000.0);

            current_capital += open_trade.pnl - open_trade.fees;
            trades.push_back(open_trade);
            open_positions.erase(signal.symbol);
        }
    }

    // Close any remaining open positions at the end of backtest period
    if (!market_data.empty()) {
        auto last_timestamp = market_data.back().timestamp;

        for (auto& [symbol, open_trade] : open_positions) {
            // Find last price for this symbol
            auto last_data = std::find_if(market_data.rbegin(), market_data.rend(),
                                        [&symbol](const MarketDataPoint& data) {
                                            return data.symbol == symbol;
                                        });

            if (last_data != market_data.rend()) {
                open_trade.exit_time = last_timestamp;
                open_trade.exit_price = applySlippageAndFees(last_data->close, last_data->volume, "SELL");
                open_trade.exit_reason = "END_OF_PERIOD";

                open_trade.pnl = (open_trade.exit_price - open_trade.entry_price) * open_trade.position_size;
                open_trade.fees = (open_trade.entry_price + open_trade.exit_price) * open_trade.position_size * (commission_bps_ / 10000.0);

                trades.push_back(open_trade);
            }
        }
    }

    return trades;
}

BacktestResults BacktestingEngine::calculatePerformanceMetrics(const std::vector<BacktestTrade>& trades,
                                                              const std::vector<double>& equity_curve,
                                                              const std::vector<std::chrono::system_clock::time_point>& timestamps) {
    BacktestResults results;

    if (trades.empty() || equity_curve.empty()) {
        return results;
    }

    results.total_trades = static_cast<int>(trades.size());
    results.starting_capital = equity_curve.front();
    results.ending_capital = equity_curve.back();
    results.peak_capital = *std::max_element(equity_curve.begin(), equity_curve.end());

    // Calculate total return
    results.total_return = (results.ending_capital - results.starting_capital) / results.starting_capital;

    // Calculate period in years for annualization
    auto period_duration = timestamps.back() - timestamps.front();
    double period_years = std::chrono::duration_cast<std::chrono::hours>(period_duration).count() / (24.0 * 365.25);

    if (period_years > 0) {
        results.annualized_return = std::pow(1.0 + results.total_return, 1.0 / period_years) - 1.0;
    }

    // Calculate returns for risk metrics
    auto returns = calculateReturns(equity_curve);

    // Risk metrics
    if (!returns.empty()) {
        double mean_return = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
        double variance = 0.0;
        for (double ret : returns) {
            variance += (ret - mean_return) * (ret - mean_return);
        }
        variance /= returns.size();
        results.volatility = std::sqrt(variance) * std::sqrt(252); // Annualized

        results.sharpe_ratio = calculateSharpeRatio(returns);
        results.sortino_ratio = calculateSortinoRatio(returns);
    }

    // Drawdown analysis
    auto drawdowns = calculateRollingDrawdown(equity_curve);
    results.max_drawdown = *std::min_element(drawdowns.begin(), drawdowns.end());

    // Trade analysis
    results.winning_trades = 0;
    results.losing_trades = 0;
    double total_wins = 0.0;
    double total_losses = 0.0;

    for (const auto& trade : trades) {
        double net_pnl = trade.pnl - trade.fees;
        if (net_pnl > 0) {
            results.winning_trades++;
            total_wins += net_pnl;
        } else if (net_pnl < 0) {
            results.losing_trades++;
            total_losses += std::abs(net_pnl);
        }
    }

    if (results.total_trades > 0) {
        results.win_rate = static_cast<double>(results.winning_trades) / results.total_trades;
    }

    if (results.winning_trades > 0) {
        results.average_win = total_wins / results.winning_trades;
    }

    if (results.losing_trades > 0) {
        results.average_loss = total_losses / results.losing_trades;
    }

    if (total_losses > 0) {
        results.profit_factor = total_wins / total_losses;
    }

    results.calmar_ratio = calculateCalmarRatio(results.annualized_return, results.max_drawdown);

    return results;
}

double BacktestingEngine::calculatePositionSize(const TradingSignal& signal, double current_capital, double current_price) {
    // Simple position sizing based on signal strength and suggested position size
    double max_position_value = current_capital * signal.suggested_position_size * signal.strength;
    return std::min(max_position_value, current_capital * 0.25); // Cap at 25% of capital
}

double BacktestingEngine::applySlippageAndFees(double price, double volume, const std::string& action) {
    // Apply slippage based on volume and action
    double slippage_factor = fixed_slippage_bps_ / 10000.0;

    if (action == "BUY") {
        return price * (1.0 + slippage_factor); // Pay higher price when buying
    } else {
        return price * (1.0 - slippage_factor); // Receive lower price when selling
    }
}

std::vector<double> BacktestingEngine::calculateReturns(const std::vector<double>& equity_curve) {
    std::vector<double> returns;
    for (size_t i = 1; i < equity_curve.size(); ++i) {
        if (equity_curve[i-1] > 0) {
            returns.push_back((equity_curve[i] - equity_curve[i-1]) / equity_curve[i-1]);
        }
    }
    return returns;
}

double BacktestingEngine::calculateSharpeRatio(const std::vector<double>& returns, double risk_free_rate) {
    if (returns.empty()) return 0.0;

    double mean_return = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
    double excess_return = mean_return * 252 - risk_free_rate; // Annualized excess return

    double variance = 0.0;
    for (double ret : returns) {
        variance += (ret - mean_return) * (ret - mean_return);
    }
    variance /= returns.size();
    double volatility = std::sqrt(variance) * std::sqrt(252); // Annualized

    return volatility > 0 ? excess_return / volatility : 0.0;
}

double BacktestingEngine::calculateSortinoRatio(const std::vector<double>& returns, double target_return) {
    if (returns.empty()) return 0.0;

    double mean_return = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
    double excess_return = mean_return * 252 - target_return; // Annualized

    double downside_variance = 0.0;
    int downside_count = 0;
    for (double ret : returns) {
        if (ret < target_return / 252) { // Daily target
            downside_variance += (ret - target_return / 252) * (ret - target_return / 252);
            downside_count++;
        }
    }

    if (downside_count == 0) return std::numeric_limits<double>::infinity();

    downside_variance /= downside_count;
    double downside_deviation = std::sqrt(downside_variance) * std::sqrt(252);

    return downside_deviation > 0 ? excess_return / downside_deviation : 0.0;
}

double BacktestingEngine::calculateCalmarRatio(double annualized_return, double max_drawdown) {
    return max_drawdown != 0 ? annualized_return / std::abs(max_drawdown) : 0.0;
}

std::vector<double> BacktestingEngine::calculateRollingDrawdown(const std::vector<double>& equity_curve) {
    std::vector<double> drawdowns;
    double peak = 0.0;

    for (double value : equity_curve) {
        peak = std::max(peak, value);
        double drawdown = peak > 0 ? (value - peak) / peak : 0.0;
        drawdowns.push_back(drawdown);
    }

    return drawdowns;
}

bool BacktestingEngine::createBacktestTables() {
    if (!db_manager_) return false;

    try {
        // Create backtest results table
        std::string create_results_table = R"(
            CREATE TABLE IF NOT EXISTS backtest_results (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                strategy_name TEXT NOT NULL,
                start_date TEXT NOT NULL,
                end_date TEXT NOT NULL,
                total_return REAL NOT NULL,
                annualized_return REAL NOT NULL,
                volatility REAL NOT NULL,
                sharpe_ratio REAL NOT NULL,
                sortino_ratio REAL NOT NULL,
                calmar_ratio REAL NOT NULL,
                max_drawdown REAL NOT NULL,
                total_trades INTEGER NOT NULL,
                win_rate REAL NOT NULL,
                profit_factor REAL NOT NULL,
                starting_capital REAL NOT NULL,
                ending_capital REAL NOT NULL,
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP
            )
        )";

        db_manager_->executeQuery(create_results_table);
        return true;

    } catch (const std::exception& e) {
        std::cerr << "BacktestingEngine: Error creating tables: " << e.what() << std::endl;
        return false;
    }
}

bool BacktestingEngine::saveBacktestResults(const std::string& strategy_name, const BacktestResults& results) {
    if (!db_manager_) return false;

    try {
        std::string insert_query = R"(
            INSERT INTO backtest_results (
                strategy_name, start_date, end_date, total_return, annualized_return,
                volatility, sharpe_ratio, sortino_ratio, calmar_ratio, max_drawdown,
                total_trades, win_rate, profit_factor, starting_capital, ending_capital
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        )";

        std::vector<std::string> params = {
            strategy_name,
            timePointToString(results.start_date),
            timePointToString(results.end_date),
            std::to_string(results.total_return),
            std::to_string(results.annualized_return),
            std::to_string(results.volatility),
            std::to_string(results.sharpe_ratio),
            std::to_string(results.sortino_ratio),
            std::to_string(results.calmar_ratio),
            std::to_string(results.max_drawdown),
            std::to_string(results.total_trades),
            std::to_string(results.win_rate),
            std::to_string(results.profit_factor),
            std::to_string(results.starting_capital),
            std::to_string(results.ending_capital)
        };

        return db_manager_->executeParameterizedQuery(insert_query, params);

    } catch (const std::exception& e) {
        std::cerr << "BacktestingEngine: Error saving results: " << e.what() << std::endl;
        return false;
    }
}

std::string BacktestingEngine::timePointToString(const std::chrono::system_clock::time_point& tp) {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::chrono::system_clock::time_point BacktestingEngine::stringToTimePoint(const std::string& str) {
    std::tm tm = {};
    std::istringstream ss(str);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

std::string BacktestingEngine::generatePerformanceReport(const BacktestResults& results) {
    std::stringstream report;

    report << "=== BACKTEST PERFORMANCE REPORT ===" << std::endl;
    report << "Period: " << timePointToString(results.start_date)
           << " to " << timePointToString(results.end_date) << std::endl;
    report << std::endl;

    report << "RETURNS:" << std::endl;
    report << "  Total Return: " << std::fixed << std::setprecision(2)
           << results.total_return * 100 << "%" << std::endl;
    report << "  Annualized Return: " << results.annualized_return * 100 << "%" << std::endl;
    report << "  Starting Capital: $" << std::setprecision(0) << results.starting_capital << std::endl;
    report << "  Ending Capital: $" << results.ending_capital << std::endl;
    report << std::endl;

    report << "RISK METRICS:" << std::endl;
    report << "  Volatility: " << std::setprecision(2) << results.volatility * 100 << "%" << std::endl;
    report << "  Sharpe Ratio: " << results.sharpe_ratio << std::endl;
    report << "  Sortino Ratio: " << results.sortino_ratio << std::endl;
    report << "  Calmar Ratio: " << results.calmar_ratio << std::endl;
    report << "  Max Drawdown: " << results.max_drawdown * 100 << "%" << std::endl;
    report << std::endl;

    report << "TRADE ANALYSIS:" << std::endl;
    report << "  Total Trades: " << results.total_trades << std::endl;
    report << "  Winning Trades: " << results.winning_trades << std::endl;
    report << "  Losing Trades: " << results.losing_trades << std::endl;
    report << "  Win Rate: " << results.win_rate * 100 << "%" << std::endl;
    report << "  Average Win: $" << std::setprecision(2) << results.average_win << std::endl;
    report << "  Average Loss: $" << results.average_loss << std::endl;
    report << "  Profit Factor: " << results.profit_factor << std::endl;

    return report.str();
}

}} // namespace CryptoClaude::Analytics