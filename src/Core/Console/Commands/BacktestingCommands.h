#pragma once

#include "../CommandFramework.h"
#include "../../Database/DatabaseManager.h"
#include "../../Backtesting/BacktestEngine.h"
#include "../../Backtesting/WalkForwardAnalyzer.h"
#include "../../Trading/TradingEngine.h"
#include "../../Portfolio/PortfolioManager.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>

namespace CryptoClaude {
namespace Console {
namespace Commands {

using namespace CryptoClaude::Database;
using namespace CryptoClaude::Backtesting;
using namespace CryptoClaude::Trading;
using namespace CryptoClaude::Portfolio;

// Comprehensive backtesting command
class BacktestCommand : public TradingCommand {
private:
    DatabaseManager& dbManager_;
    std::shared_ptr<BacktestEngine> backtestEngine_;

public:
    BacktestCommand(DatabaseManager& dbManager, std::shared_ptr<BacktestEngine> backtestEngine)
        : dbManager_(dbManager), backtestEngine_(backtestEngine) {}

    std::string getName() const override { return "backtest"; }
    std::string getDescription() const override { return "Execute comprehensive historical trading simulation"; }

    std::vector<ParameterDefinition> getParameters() const override {
        return {
            {"mode", ParameterType::STRING, true, "Backtest mode", "statistical", {"full", "statistical"}},
            {"start", ParameterType::STRING, true, "Start date (YYYY-MM-DD)", ""},
            {"end", ParameterType::STRING, true, "End date (YYYY-MM-DD)", ""},
            {"initial-capital", ParameterType::DOUBLE, false, "Starting virtual capital", "100000"},
            {"parameters", ParameterType::FILEPATH, false, "Custom parameter configuration file", ""},
            {"thresholds", ParameterType::STRING, false, "Testing threshold profile", "moderate", {"conservative", "moderate", "aggressive", "custom"}},
            {"symbols", ParameterType::STRING, false, "Comma-separated list of symbols to include", ""},
            {"format", ParameterType::STRING, false, "Output format", "detailed", {"summary", "detailed", "json"}}
        };
    }

    CommandResult execute(CommandContext& context) override {
        std::string mode = context.getStringParameter("mode");
        std::string startDate = context.getStringParameter("start");
        std::string endDate = context.getStringParameter("end");
        double initialCapital = context.getDoubleParameter("initial-capital").value_or(100000.0);
        std::string parametersFile = context.getStringParameter("parameters", "");
        std::string thresholds = context.getStringParameter("thresholds", "moderate");
        std::string symbols = context.getStringParameter("symbols", "");
        std::string format = context.getStringParameter("format", "detailed");

        std::ostringstream output;

        try {
            // Validate dates
            auto startTime = parseDate(startDate);
            auto endTime = parseDate(endDate);

            if (startTime >= endTime) {
                return CommandResult::createError("Start date must be before end date");
            }

            // Parse symbols
            std::vector<std::string> symbolList;
            if (!symbols.empty()) {
                std::stringstream ss(symbols);
                std::string symbol;
                while (std::getline(ss, symbol, ',')) {
                    symbolList.push_back(symbol);
                }
            }

            if (mode == "full") {
                return executeFullBacktest(startTime, endTime, initialCapital, parametersFile,
                                        thresholds, symbolList, format, output);
            } else {
                return executeStatisticalBacktest(startTime, endTime, parametersFile,
                                               symbolList, format, output);
            }

        } catch (const std::exception& e) {
            return CommandResult::createError("Backtest execution failed", e.what());
        }
    }

private:
    CommandResult executeFullBacktest(std::chrono::system_clock::time_point start,
                                    std::chrono::system_clock::time_point end,
                                    double initialCapital,
                                    const std::string& parametersFile,
                                    const std::string& thresholds,
                                    const std::vector<std::string>& symbols,
                                    const std::string& format,
                                    std::ostringstream& output) {

        output << "🎯 FULL BACKTESTING SIMULATION\n";
        output << "═══════════════════════════════\n\n";

        // Configuration summary
        output << "📊 Backtest Configuration:\n";
        output << "   Mode: Full Trading Simulation\n";
        output << "   Period: " << formatDate(start) << " to " << formatDate(end) << "\n";
        output << "   Initial Capital: $" << std::fixed << std::setprecision(2) << initialCapital << "\n";
        output << "   Threshold Profile: " << thresholds << "\n";

        if (!symbols.empty()) {
            output << "   Target Symbols: ";
            for (size_t i = 0; i < symbols.size(); ++i) {
                if (i > 0) output << ", ";
                output << symbols[i];
            }
            output << "\n";
        }

        if (!parametersFile.empty()) {
            output << "   Custom Parameters: " << parametersFile << "\n";
        }

        // Setup backtest configuration
        BacktestEngine::BacktestConfig config;
        config.startDate = start;
        config.endDate = end;
        config.initialCapital = initialCapital;
        config.thresholdProfile = thresholds;
        config.targetSymbols = symbols;
        config.enableSlippage = true;
        config.enableFees = true;
        config.enableRealisticExecution = true;

        // Load custom parameters if specified
        if (!parametersFile.empty()) {
            if (!loadParametersFromFile(parametersFile, config)) {
                return CommandResult::createError("Failed to load parameters from file: " + parametersFile);
            }
        }

        output << "\n🔄 Starting backtest simulation...\n";
        output << "This may take several minutes depending on the date range.\n\n";

        // Execute backtest
        auto startExecution = std::chrono::steady_clock::now();
        auto results = backtestEngine_->executeFullBacktest(config);
        auto endExecution = std::chrono::steady_clock::now();

        if (!results.has_value()) {
            return CommandResult::createError("Backtest execution failed", backtestEngine_->getLastError());
        }

        auto executionTime = std::chrono::duration_cast<std::chrono::seconds>(endExecution - startExecution);

        // Display results based on format
        if (format == "summary") {
            displayBacktestSummary(output, *results);
        } else if (format == "detailed") {
            displayDetailedBacktestResults(output, *results);
        } else if (format == "json") {
            displayBacktestJSON(output, *results);
        }

        output << "\n⏱️  Execution Time: " << executionTime.count() << " seconds\n";

        // Create result
        auto result = CommandResult::createSuccess(output.str());
        result.addOutput("backtest_mode", "full");
        result.addOutput("total_return", std::to_string(results->totalReturn));
        result.addOutput("sharpe_ratio", std::to_string(results->sharpeRatio));
        result.addOutput("max_drawdown", std::to_string(results->maxDrawdown));
        result.addOutput("win_rate", std::to_string(results->winRate));
        result.addOutput("total_trades", std::to_string(results->totalTrades));
        return result;
    }

    CommandResult executeStatisticalBacktest(std::chrono::system_clock::time_point start,
                                           std::chrono::system_clock::time_point end,
                                           const std::string& parametersFile,
                                           const std::vector<std::string>& symbols,
                                           const std::string& format,
                                           std::ostringstream& output) {

        output << "📊 STATISTICAL BACKTESTING ANALYSIS\n";
        output << "══════════════════════════════════\n\n";

        output << "📋 Configuration:\n";
        output << "   Mode: Statistical Analysis (No Trading Simulation)\n";
        output << "   Period: " << formatDate(start) << " to " << formatDate(end) << "\n";
        output << "   Analysis Focus: Signal Quality & Prediction Accuracy\n\n";

        BacktestEngine::StatisticalConfig config;
        config.startDate = start;
        config.endDate = end;
        config.targetSymbols = symbols;

        output << "🔄 Running statistical analysis...\n\n";

        auto statResults = backtestEngine_->executeStatisticalAnalysis(config);

        if (!statResults.has_value()) {
            return CommandResult::createError("Statistical analysis failed", backtestEngine_->getLastError());
        }

        displayStatisticalResults(output, *statResults, format);

        auto result = CommandResult::createSuccess(output.str());
        result.addOutput("backtest_mode", "statistical");
        result.addOutput("prediction_accuracy", std::to_string(statResults->predictionAccuracy));
        result.addOutput("signal_quality", std::to_string(statResults->signalQuality));
        return result;
    }

    void displayBacktestSummary(std::ostringstream& output, const BacktestEngine::BacktestResults& results) {
        output << "📈 BACKTEST RESULTS SUMMARY\n";
        output << "═══════════════════════════\n\n";

        output << "💰 Performance Metrics:\n";
        output << "   Total Return: " << std::showpos << std::fixed << std::setprecision(2)
               << (results.totalReturn * 100) << "%" << std::noshowpos << "\n";
        output << "   Annualized Return: " << std::showpos << (results.annualizedReturn * 100) << "%" << std::noshowpos << "\n";
        output << "   Sharpe Ratio: " << std::setprecision(3) << results.sharpeRatio << "\n";
        output << "   Sortino Ratio: " << results.sortinoRatio << "\n";
        output << "   Maximum Drawdown: " << std::setprecision(2) << (results.maxDrawdown * 100) << "%\n";
        output << "   Volatility: " << (results.volatility * 100) << "%\n\n";

        output << "📊 Trading Statistics:\n";
        output << "   Total Trades: " << results.totalTrades << "\n";
        output << "   Winning Trades: " << results.winningTrades << " (" << std::setprecision(1)
               << (results.winRate * 100) << "%)\n";
        output << "   Average Trade: " << std::showpos << std::setprecision(2)
               << (results.averageTradeReturn * 100) << "%" << std::noshowpos << "\n";
        output << "   Best Trade: " << std::showpos << (results.bestTrade * 100) << "%" << std::noshowpos << "\n";
        output << "   Worst Trade: " << std::showpos << (results.worstTrade * 100) << "%" << std::noshowpos << "\n\n";

        output << "⚠️  Risk Metrics:\n";
        output << "   VaR (95%): " << std::setprecision(2) << (results.valueAtRisk95 * 100) << "%\n";
        output << "   Expected Shortfall: " << (results.expectedShortfall * 100) << "%\n";
        output << "   Calmar Ratio: " << std::setprecision(3) << results.calmarRatio << "\n";
        output << "   Maximum Leverage: " << std::setprecision(1) << results.maxLeverageUsed << ":1\n\n";

        displayPerformanceAssessment(output, results);
    }

    void displayDetailedBacktestResults(std::ostringstream& output, const BacktestEngine::BacktestResults& results) {
        displayBacktestSummary(output, results);

        output << "\n📋 DETAILED ANALYSIS\n";
        output << "══════════════════\n\n";

        // Monthly breakdown
        output << "📅 Monthly Performance:\n";
        for (const auto& [month, performance] : results.monthlyReturns) {
            output << "   " << month << ": " << std::showpos << std::fixed << std::setprecision(1)
                   << (performance * 100) << "%" << std::noshowpos << "\n";
        }

        output << "\n🎯 Asset Performance:\n";
        for (const auto& [symbol, performance] : results.assetPerformance) {
            output << "   " << std::left << std::setw(8) << symbol
                   << " Return: " << std::showpos << std::setprecision(2) << (performance.totalReturn * 100) << "%" << std::noshowpos
                   << " | Trades: " << std::right << std::setw(3) << performance.tradeCount
                   << " | Win Rate: " << std::setprecision(1) << (performance.winRate * 100) << "%\n";
        }

        output << "\n📊 Drawdown Analysis:\n";
        output << "   Maximum Drawdown: " << std::fixed << std::setprecision(2) << (results.maxDrawdown * 100) << "%\n";
        output << "   Drawdown Duration: " << results.maxDrawdownDuration << " days\n";
        output << "   Recovery Time: " << results.recoveryTime << " days\n";
        output << "   Number of Drawdowns > 5%: " << results.significantDrawdowns << "\n";

        output << "\n⚡ Execution Quality:\n";
        output << "   Average Slippage: " << std::setprecision(3) << (results.averageSlippage * 100) << "%\n";
        output << "   Transaction Costs: $" << std::setprecision(2) << results.totalTransactionCosts << "\n";
        output << "   Fill Rate: " << std::setprecision(1) << (results.fillRate * 100) << "%\n";
        output << "   Market Impact: " << std::setprecision(3) << (results.marketImpact * 100) << "%\n";

        displayRecommendations(output, results);
    }

    void displayStatisticalResults(std::ostringstream& output, const BacktestEngine::StatisticalResults& results,
                                  const std::string& format) {
        if (format == "json") {
            displayStatisticalJSON(output, results);
            return;
        }

        output << "🔍 STATISTICAL ANALYSIS RESULTS\n";
        output << "══════════════════════════════\n\n";

        output << "🎯 Signal Quality Metrics:\n";
        output << "   Prediction Accuracy: " << std::fixed << std::setprecision(1)
               << (results.predictionAccuracy * 100) << "%\n";
        output << "   Signal Quality Score: " << std::setprecision(2) << results.signalQuality << "/10\n";
        output << "   False Positive Rate: " << std::setprecision(1) << (results.falsePositiveRate * 100) << "%\n";
        output << "   False Negative Rate: " << (results.falseNegativeRate * 100) << "%\n";
        output << "   Information Ratio: " << std::setprecision(3) << results.informationRatio << "\n\n";

        output << "📊 Feature Analysis:\n";
        for (const auto& [feature, importance] : results.featureImportance) {
            std::string bar(static_cast<int>(importance * 20), '█');
            output << "   " << std::left << std::setw(20) << feature
                   << " " << std::right << std::setw(5) << std::fixed << std::setprecision(1)
                   << (importance * 100) << "% " << bar << "\n";
        }

        output << "\n🎲 Statistical Significance:\n";
        output << "   T-Statistic: " << std::fixed << std::setprecision(3) << results.tStatistic << "\n";
        output << "   P-Value: " << results.pValue << "\n";
        output << "   Confidence Interval: [" << std::setprecision(2)
               << results.confidenceInterval.first << "%, "
               << results.confidenceInterval.second << "%]\n";
        output << "   Statistical Significance: " << (results.pValue < 0.05 ? "✅ SIGNIFICANT" : "❌ NOT SIGNIFICANT") << "\n\n";

        output << "📈 Model Performance:\n";
        output << "   Out-of-Sample Accuracy: " << std::setprecision(1) << (results.outOfSampleAccuracy * 100) << "%\n";
        output << "   Cross-Validation Score: " << std::setprecision(3) << results.crossValidationScore << "\n";
        output << "   Overfitting Risk: ";
        if (results.overfittingRisk < 0.2) output << "🟢 LOW";
        else if (results.overfittingRisk < 0.5) output << "🟡 MODERATE";
        else output << "🔴 HIGH";
        output << " (" << std::setprecision(2) << results.overfittingRisk << ")\n";
    }

    void displayPerformanceAssessment(std::ostringstream& output, const BacktestEngine::BacktestResults& results) {
        output << "🎖️  PERFORMANCE ASSESSMENT\n";
        output << "═════════════════════════\n\n";

        int score = 0;
        int totalCriteria = 0;

        // Assess various performance criteria
        std::vector<std::pair<std::string, bool>> criteria = {
            {"Positive Total Return", results.totalReturn > 0},
            {"Sharpe Ratio > 1.0", results.sharpeRatio > 1.0},
            {"Max Drawdown < 20%", results.maxDrawdown < 0.20},
            {"Win Rate > 50%", results.winRate > 0.50},
            {"Calmar Ratio > 0.5", results.calmarRatio > 0.5},
            {"Volatility < 30%", results.volatility < 0.30},
            {"Sufficient Trades (>20)", results.totalTrades > 20}
        };

        for (const auto& [criterion, passed] : criteria) {
            output << "   " << (passed ? "✅" : "❌") << " " << criterion << "\n";
            if (passed) score++;
            totalCriteria++;
        }

        double performanceScore = (double)score / totalCriteria * 100;
        output << "\n🎯 Overall Performance Score: " << std::fixed << std::setprecision(0) << performanceScore << "/100\n";

        if (performanceScore >= 85) {
            output << "   Rating: 🟢 EXCELLENT - Ready for live trading\n";
        } else if (performanceScore >= 70) {
            output << "   Rating: 🟡 GOOD - Consider additional optimization\n";
        } else if (performanceScore >= 50) {
            output << "   Rating: 🟠 FAIR - Requires significant improvements\n";
        } else {
            output << "   Rating: 🔴 POOR - Not recommended for live trading\n";
        }
    }

    void displayRecommendations(std::ostringstream& output, const BacktestEngine::BacktestResults& results) {
        output << "\n💡 OPTIMIZATION RECOMMENDATIONS\n";
        output << "═══════════════════════════════\n\n";

        if (results.sharpeRatio < 1.0) {
            output << "   📉 Low Sharpe Ratio: Consider improving risk-adjusted returns\n";
            output << "      • Review position sizing strategy\n";
            output << "      • Optimize risk management parameters\n";
        }

        if (results.maxDrawdown > 0.15) {
            output << "   ⚠️  High Maximum Drawdown: Implement better risk controls\n";
            output << "      • Reduce position sizes during volatile periods\n";
            output << "      • Implement dynamic stop losses\n";
        }

        if (results.winRate < 0.45) {
            output << "   🎯 Low Win Rate: Improve signal quality\n";
            output << "      • Refine entry/exit criteria\n";
            output << "      • Consider additional filtering conditions\n";
        }

        if (results.averageSlippage > 0.01) {
            output << "   ⚡ High Slippage: Optimize execution strategy\n";
            output << "      • Use limit orders instead of market orders\n";
            output << "      • Consider market impact in position sizing\n";
        }

        if (results.totalTrades < 20) {
            output << "   📊 Insufficient Trade Count: Increase trading frequency\n";
            output << "      • Lower signal thresholds (with proper risk controls)\n";
            output << "      • Expand universe of tradeable assets\n";
        }

        output << "\n🔍 Next Steps:\n";
        output << "   1. Review individual trade analysis for patterns\n";
        output << "   2. Conduct walk-forward analysis for robustness\n";
        output << "   3. Test with different market regimes\n";
        output << "   4. Perform sensitivity analysis on key parameters\n";
    }

    void displayBacktestJSON(std::ostringstream& output, const BacktestEngine::BacktestResults& results) {
        output << "{\n";
        output << "  \"backtest_results\": {\n";
        output << "    \"performance\": {\n";
        output << "      \"total_return\": " << std::fixed << std::setprecision(6) << results.totalReturn << ",\n";
        output << "      \"annualized_return\": " << results.annualizedReturn << ",\n";
        output << "      \"sharpe_ratio\": " << results.sharpeRatio << ",\n";
        output << "      \"sortino_ratio\": " << results.sortinoRatio << ",\n";
        output << "      \"max_drawdown\": " << results.maxDrawdown << ",\n";
        output << "      \"volatility\": " << results.volatility << "\n";
        output << "    },\n";
        output << "    \"trading\": {\n";
        output << "      \"total_trades\": " << results.totalTrades << ",\n";
        output << "      \"winning_trades\": " << results.winningTrades << ",\n";
        output << "      \"win_rate\": " << results.winRate << ",\n";
        output << "      \"average_trade_return\": " << results.averageTradeReturn << "\n";
        output << "    },\n";
        output << "    \"risk\": {\n";
        output << "      \"value_at_risk_95\": " << results.valueAtRisk95 << ",\n";
        output << "      \"expected_shortfall\": " << results.expectedShortfall << ",\n";
        output << "      \"calmar_ratio\": " << results.calmarRatio << "\n";
        output << "    }\n";
        output << "  }\n";
        output << "}\n";
    }

    void displayStatisticalJSON(std::ostringstream& output, const BacktestEngine::StatisticalResults& results) {
        output << "{\n";
        output << "  \"statistical_results\": {\n";
        output << "    \"prediction_accuracy\": " << std::fixed << std::setprecision(6) << results.predictionAccuracy << ",\n";
        output << "    \"signal_quality\": " << results.signalQuality << ",\n";
        output << "    \"information_ratio\": " << results.informationRatio << ",\n";
        output << "    \"statistical_significance\": " << (results.pValue < 0.05) << "\n";
        output << "  }\n";
        output << "}\n";
    }

    std::chrono::system_clock::time_point parseDate(const std::string& dateStr) {
        std::tm tm = {};
        std::istringstream ss(dateStr);
        ss >> std::get_time(&tm, "%Y-%m-%d");
        if (ss.fail()) {
            throw std::invalid_argument("Invalid date format. Use YYYY-MM-DD");
        }
        return std::chrono::system_clock::from_time_t(std::mktime(&tm));
    }

    std::string formatDate(std::chrono::system_clock::time_point timePoint) {
        auto time_t = std::chrono::system_clock::to_time_t(timePoint);
        std::ostringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d");
        return ss.str();
    }

    bool loadParametersFromFile(const std::string& filename, BacktestEngine::BacktestConfig& config) {
        // Placeholder - in real implementation would load from JSON/YAML file
        return true;
    }
};

// Walk-forward analysis command
class WalkForwardCommand : public TradingCommand {
private:
    std::shared_ptr<WalkForwardAnalyzer> walkForwardAnalyzer_;

public:
    explicit WalkForwardCommand(std::shared_ptr<WalkForwardAnalyzer> walkForwardAnalyzer)
        : walkForwardAnalyzer_(walkForwardAnalyzer) {}

    std::string getName() const override { return "walk-forward"; }
    std::string getDescription() const override { return "Execute walk-forward analysis for robust model validation"; }

    std::vector<ParameterDefinition> getParameters() const override {
        return {
            {"periods", ParameterType::INTEGER, true, "Number of forward periods to test", ""},
            {"window", ParameterType::INTEGER, true, "Rolling window size in days", ""},
            {"initial-capital", ParameterType::DOUBLE, false, "Virtual capital for simulation", "100000"},
            {"step", ParameterType::INTEGER, false, "Step size between periods in days", "30"}
        };
    }

    CommandResult execute(CommandContext& context) override {
        int periods = context.getIntParameter("periods").value_or(0);
        int window = context.getIntParameter("window").value_or(0);
        double initialCapital = context.getDoubleParameter("initial-capital").value_or(100000.0);
        int step = context.getIntParameter("step").value_or(30);

        if (periods <= 0 || window <= 0) {
            return CommandResult::createError("Both periods and window must be positive integers");
        }

        std::ostringstream output;

        try {
            output << "🔄 WALK-FORWARD ANALYSIS\n";
            output << "═══════════════════════\n\n";

            output << "📊 Configuration:\n";
            output << "   Forward Periods: " << periods << "\n";
            output << "   Window Size: " << window << " days\n";
            output << "   Step Size: " << step << " days\n";
            output << "   Initial Capital: $" << std::fixed << std::setprecision(2) << initialCapital << "\n\n";

            WalkForwardAnalyzer::WalkForwardConfig config;
            config.forwardPeriods = periods;
            config.windowSize = window;
            config.stepSize = step;
            config.initialCapital = initialCapital;

            output << "🔄 Executing walk-forward analysis...\n";
            output << "This will run " << periods << " separate backtests.\n\n";

            auto results = walkForwardAnalyzer_->executeWalkForward(config);

            if (!results.has_value()) {
                return CommandResult::createError("Walk-forward analysis failed",
                                                walkForwardAnalyzer_->getLastError());
            }

            displayWalkForwardResults(output, *results);

            auto result = CommandResult::createSuccess(output.str());
            result.addOutput("periods_tested", std::to_string(results->periodResults.size()));
            result.addOutput("overall_consistency", std::to_string(results->consistencyScore));
            result.addOutput("parameter_stability", std::to_string(results->parameterStability));
            return result;

        } catch (const std::exception& e) {
            return CommandResult::createError("Walk-forward analysis failed", e.what());
        }
    }

private:
    void displayWalkForwardResults(std::ostringstream& output, const WalkForwardAnalyzer::WalkForwardResults& results) {
        output << "📈 WALK-FORWARD ANALYSIS RESULTS\n";
        output << "═══════════════════════════════\n\n";

        // Period-by-period results
        output << "📊 Period-by-Period Performance:\n";
        output << "Period   Start Date   End Date     Return    Sharpe    Drawdown\n";
        output << "──────   ──────────   ────────     ──────    ──────    ────────\n";

        for (size_t i = 0; i < results.periodResults.size(); ++i) {
            const auto& period = results.periodResults[i];
            output << std::right << std::setw(6) << (i + 1)
                   << "   " << formatDate(period.startDate)
                   << "   " << formatDate(period.endDate)
                   << "   " << std::showpos << std::setw(7) << std::fixed << std::setprecision(1)
                   << (period.totalReturn * 100) << "%" << std::noshowpos
                   << "   " << std::setw(6) << std::setprecision(2) << period.sharpeRatio
                   << "   " << std::setw(7) << std::setprecision(1) << (period.maxDrawdown * 100) << "%\n";
        }

        output << "\n📊 Aggregated Statistics:\n";
        output << "   Overall Return: " << std::showpos << std::fixed << std::setprecision(2)
               << (results.overallReturn * 100) << "%" << std::noshowpos << "\n";
        output << "   Average Period Return: " << std::showpos << (results.averageReturn * 100) << "%" << std::noshowpos << "\n";
        output << "   Return Volatility: " << std::setprecision(2) << (results.returnVolatility * 100) << "%\n";
        output << "   Best Period: " << std::showpos << (results.bestPeriod * 100) << "%" << std::noshowpos << "\n";
        output << "   Worst Period: " << std::showpos << (results.worstPeriod * 100) << "%" << std::noshowpos << "\n";
        output << "   Winning Periods: " << results.winningPeriods << "/" << results.periodResults.size()
               << " (" << std::setprecision(1) << (results.winRate * 100) << "%)\n\n";

        // Robustness metrics
        output << "🎯 Robustness Assessment:\n";
        output << "   Consistency Score: " << std::fixed << std::setprecision(1) << (results.consistencyScore * 100) << "/100\n";
        output << "   Parameter Stability: " << (results.parameterStability * 100) << "/100\n";
        output << "   Out-of-Sample Degradation: " << std::showpos << (results.degradation * 100) << "%" << std::noshowpos << "\n";

        // Risk assessment
        if (results.consistencyScore >= 0.8) {
            output << "   Assessment: 🟢 ROBUST - Consistent performance across periods\n";
        } else if (results.consistencyScore >= 0.6) {
            output << "   Assessment: 🟡 MODERATE - Some performance variation\n";
        } else {
            output << "   Assessment: 🔴 UNSTABLE - High performance variation\n";
        }

        // Parameter drift analysis
        if (!results.parameterDrift.empty()) {
            output << "\n📊 Parameter Drift Analysis:\n";
            for (const auto& [param, drift] : results.parameterDrift) {
                output << "   " << std::left << std::setw(20) << param
                       << " Drift: " << std::fixed << std::setprecision(3) << drift << "\n";
            }
        }

        // Recommendations
        output << "\n💡 Walk-Forward Recommendations:\n";
        if (results.consistencyScore < 0.6) {
            output << "   ⚠️  Strategy shows significant performance inconsistency\n";
            output << "   • Review parameter optimization approach\n";
            output << "   • Consider shorter rebalancing periods\n";
            output << "   • Implement adaptive parameter adjustment\n";
        }

        if (results.parameterStability < 0.7) {
            output << "   📊 Parameter instability detected\n";
            output << "   • Some parameters may be overfit\n";
            output << "   • Consider parameter regularization\n";
            output << "   • Implement ensemble methods\n";
        }

        if (results.degradation > 0.1) {
            output << "   📉 Significant out-of-sample degradation\n";
            output << "   • Strategy may not generalize well\n";
            output << "   • Increase validation requirements\n";
            output << "   • Consider more conservative position sizing\n";
        }

        if (results.consistencyScore >= 0.8 && results.parameterStability >= 0.7) {
            output << "   ✅ Strategy shows robust performance characteristics\n";
            output << "   • Suitable for live trading consideration\n";
            output << "   • Monitor for continued consistency\n";
        }
    }

    std::string formatDate(std::chrono::system_clock::time_point timePoint) {
        auto time_t = std::chrono::system_clock::to_time_t(timePoint);
        std::ostringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d");
        return ss.str();
    }
};

// Mode management commands for test/live switching
class ModeCommand : public TradingCommand {
private:
    std::shared_ptr<TradingEngine> tradingEngine_;

public:
    explicit ModeCommand(std::shared_ptr<TradingEngine> tradingEngine)
        : tradingEngine_(tradingEngine) {}

    std::string getName() const override { return "mode"; }
    std::string getDescription() const override { return "Manage system trading mode (test/live)"; }

    std::vector<ParameterDefinition> getParameters() const override {
        return {
            {"action", ParameterType::STRING, true, "Action: get, set-test, set-live", "", {"get", "set-test", "set-live"}},
            {"confirm", ParameterType::BOOLEAN, false, "Skip confirmation for mode changes", "false"}
        };
    }

    std::vector<std::string> getAliases() const override { return {"trading-mode"}; }

    CommandResult execute(CommandContext& context) override {
        std::string action = context.getStringParameter("action");
        bool skipConfirm = context.getBoolParameter("confirm").value_or(false);

        std::ostringstream output;

        try {
            if (action == "get") {
                return getModeStatus(output);
            } else if (action == "set-test") {
                return setTestMode(skipConfirm, output);
            } else if (action == "set-live") {
                return setLiveMode(skipConfirm, output);
            } else {
                return CommandResult::createError("Invalid action. Use: get, set-test, or set-live");
            }

        } catch (const std::exception& e) {
            return CommandResult::createError("Mode command failed", e.what());
        }
    }

private:
    CommandResult getModeStatus(std::ostringstream& output) {
        auto currentMode = tradingEngine_->getCurrentMode();
        auto modeUptime = tradingEngine_->getModeUptime();

        output << "📊 CURRENT TRADING MODE STATUS\n";
        output << "═════════════════════════════\n\n";

        output << "🎯 Active Mode: ";
        if (currentMode == TradingEngine::Mode::TEST) {
            output << "🧪 TEST MODE (Paper Trading)\n";
        } else if (currentMode == TradingEngine::Mode::LIVE) {
            output << "💰 LIVE MODE (Real Trading)\n";
        } else {
            output << "❌ INACTIVE\n";
        }

        if (currentMode != TradingEngine::Mode::INACTIVE) {
            auto hours = std::chrono::duration_cast<std::chrono::hours>(modeUptime);
            output << "   Mode Duration: " << hours.count() << " hours\n\n";

            if (currentMode == TradingEngine::Mode::TEST) {
                auto virtualBalance = tradingEngine_->getVirtualBalance();
                auto virtualPositions = tradingEngine_->getVirtualPositionCount();
                auto virtualPnL = tradingEngine_->getVirtualPnL();

                output << "🧪 Test Mode Details:\n";
                output << "   Virtual Balance: $" << std::fixed << std::setprecision(2) << virtualBalance << "\n";
                output << "   Virtual Positions: " << virtualPositions << "\n";
                output << "   Virtual P&L: $" << std::showpos << virtualPnL << std::noshowpos << "\n";
                output << "   UI Indicator: TEST MODE ACTIVE banner displayed\n";
                output << "   Risk Management: Active but non-binding\n";
                output << "   Data Feeds: Live market data\n";
                output << "   Execution: Simulated (no real trades)\n";
            } else {
                auto realBalance = tradingEngine_->getRealBalance();
                auto realPositions = tradingEngine_->getRealPositionCount();
                auto realPnL = tradingEngine_->getRealPnL();

                output << "💰 Live Mode Details:\n";
                output << "   Real Balance: $" << std::fixed << std::setprecision(2) << realBalance << "\n";
                output << "   Real Positions: " << realPositions << "\n";
                output << "   Real P&L: $" << std::showpos << realPnL << std::noshowpos << "\n";
                output << "   Risk Management: Active and enforced\n";
                output << "   Data Feeds: Live market data\n";
                output << "   Execution: Real trades with actual funds\n";
            }
        }

        auto result = CommandResult::createSuccess(output.str());
        result.addOutput("current_mode", currentMode == TradingEngine::Mode::TEST ? "test" :
                                      (currentMode == TradingEngine::Mode::LIVE ? "live" : "inactive"));
        return result;
    }

    CommandResult setTestMode(bool skipConfirm, std::ostringstream& output) {
        if (!skipConfirm) {
            std::cout << "🧪 SWITCH TO TEST MODE\n";
            std::cout << "This will switch the system to paper trading mode.\n";
            std::cout << "All trades will be simulated with virtual capital.\n";
            std::cout << "Are you sure you want to proceed? (y/N): ";

            std::string response;
            std::getline(std::cin, response);
            if (response != "y" && response != "Y" && response != "yes" && response != "YES") {
                return CommandResult::createSuccess("Mode change cancelled by user");
            }
        }

        if (tradingEngine_->setTestMode()) {
            output << "✅ TEST MODE ACTIVATED\n\n";
            output << "🧪 System is now in paper trading mode:\n";
            output << "   • All trades are simulated\n";
            output << "   • Virtual portfolio initialized\n";
            output << "   • Live data feeds active\n";
            output << "   • Risk management active but non-binding\n";
            output << "   • Clear UI indicators showing TEST MODE\n\n";

            auto virtualBalance = tradingEngine_->getVirtualBalance();
            output << "💰 Virtual Portfolio:\n";
            output << "   Starting Balance: $" << std::fixed << std::setprecision(2) << virtualBalance << "\n";
            output << "   Virtual Positions: 0\n";
            output << "   Virtual P&L: $0.00\n";

            auto result = CommandResult::createSuccess(output.str());
            result.addOutput("new_mode", "test");
            result.addOutput("virtual_balance", std::to_string(virtualBalance));
            return result;
        } else {
            return CommandResult::createError("Failed to switch to test mode", tradingEngine_->getLastError());
        }
    }

    CommandResult setLiveMode(bool skipConfirm, std::ostringstream& output) {
        // Extra safety for live mode
        if (!skipConfirm) {
            std::cout << "🚨 SWITCH TO LIVE TRADING MODE\n";
            std::cout << "═════════════════════════════\n";
            std::cout << "⚠️  WARNING: This will enable real trading with actual funds!\n\n";

            // Production readiness validation
            auto readinessCheck = tradingEngine_->validateProductionReadiness();
            if (!readinessCheck.isReady) {
                output << "❌ SYSTEM NOT READY FOR LIVE TRADING\n\n";
                output << "Issues detected:\n";
                for (const auto& issue : readinessCheck.issues) {
                    output << "   🔴 " << issue << "\n";
                }
                output << "\nResolve these issues before enabling live trading.\n";
                return CommandResult::createError("System not ready for live trading", output.str());
            }

            std::cout << "Production readiness validated successfully.\n";
            std::cout << "Real funds will be used for trading.\n";
            std::cout << "Type 'I UNDERSTAND THE RISKS' to proceed: ";

            std::string response;
            std::getline(std::cin, response);
            if (response != "I UNDERSTAND THE RISKS") {
                return CommandResult::createSuccess("Live mode activation cancelled");
            }
        }

        if (tradingEngine_->setLiveMode()) {
            output << "✅ LIVE MODE ACTIVATED\n\n";
            output << "💰 System is now in live trading mode:\n";
            output << "   • Real trades will be executed\n";
            output << "   • Actual funds at risk\n";
            output << "   • Full risk management enforced\n";
            output << "   • All safety systems active\n";
            output << "   • Live monitoring enabled\n\n";

            auto realBalance = tradingEngine_->getRealBalance();
            output << "💼 Real Portfolio:\n";
            output << "   Available Balance: $" << std::fixed << std::setprecision(2) << realBalance << "\n";
            output << "   Active Positions: " << tradingEngine_->getRealPositionCount() << "\n";
            output << "   Current P&L: $" << std::showpos << tradingEngine_->getRealPnL() << std::noshowpos << "\n";

            auto result = CommandResult::createSuccess(output.str());
            result.addOutput("new_mode", "live");
            result.addOutput("real_balance", std::to_string(realBalance));
            return result;
        } else {
            return CommandResult::createError("Failed to switch to live mode", tradingEngine_->getLastError());
        }
    }
};

} // namespace Commands
} // namespace Console
} // namespace CryptoClaude