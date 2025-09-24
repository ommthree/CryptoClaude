#pragma once

#include "../CommandFramework.h"
#include "../../Database/DatabaseManager.h"
#include "../../Analytics/StatisticalTools.h"
#include "../../Analytics/TechnicalIndicators.h"
#include "../../Data/Providers/CryptoNewsProvider.h"
#include "../../Sentiment/SentimentQualityManager.h"
#include <iostream>
#include <iomanip>
#include <sstream>

namespace CryptoClaude {
namespace Console {
namespace Commands {

using namespace CryptoClaude::Database;
using namespace CryptoClaude::Analytics;
using namespace CryptoClaude::Data::Providers;
using namespace CryptoClaude::Sentiment;

// Market data analysis command
class AnalyzeMarketCommand : public DataCommand {
private:
    DatabaseManager& dbManager_;

public:
    explicit AnalyzeMarketCommand(DatabaseManager& dbManager) : dbManager_(dbManager) {}

    std::string getName() const override { return "analyze-market"; }
    std::string getDescription() const override { return "Perform comprehensive market data analysis"; }

    std::vector<ParameterDefinition> getParameters() const override {
        return {
            {"symbol", ParameterType::SYMBOL, true, "Cryptocurrency symbol to analyze", ""},
            {"timeframe", ParameterType::STRING, false, "Analysis timeframe", "30d", {"1d", "7d", "30d", "90d", "1y"}},
            {"indicators", ParameterType::STRING, false, "Technical indicators to include", "all", {"all", "momentum", "volatility", "trend"}},
            {"format", ParameterType::STRING, false, "Output format", "detailed", {"brief", "detailed", "json"}},
            {"correlations", ParameterType::BOOLEAN, false, "Include correlation analysis", "false"}
        };
    }

    std::vector<std::string> getAliases() const override { return {"analyze", "market"}; }

    CommandResult execute(CommandContext& context) override {
        std::string symbol = context.getStringParameter("symbol");
        std::string timeframe = context.getStringParameter("timeframe", "30d");
        std::string indicators = context.getStringParameter("indicators", "all");
        std::string format = context.getStringParameter("format", "detailed");
        bool includeCorrelations = context.getBoolParameter("correlations").value_or(false);

        std::ostringstream output;
        output << "=== Market Analysis: " << symbol << " (" << timeframe << ") ===\n\n";

        try {
            // Fetch market data
            auto marketData = getMarketData(symbol, timeframe);
            if (marketData.empty()) {
                return CommandResult::createError("No market data found for " + symbol);
            }

            // Extract price data
            std::vector<double> prices = extractPrices(marketData);
            std::vector<double> volumes = extractVolumes(marketData);
            std::vector<double> returns = StatisticalTools::calculateReturns(prices);

            // Basic statistics
            auto stats = StatisticalTools::calculateDistributionStats(returns);
            output << "📊 Statistical Analysis:\n";
            output << "  Current Price: $" << std::fixed << std::setprecision(2) << prices.back() << "\n";
            output << "  Average Daily Return: " << std::setprecision(4) << stats.mean * 100 << "%\n";
            output << "  Volatility: " << StatisticalTools::calculateVolatility(returns) * 100 << "%\n";
            output << "  Sharpe Ratio: " << StatisticalTools::calculateSharpeRatio(returns) << "\n";
            output << "  Max Drawdown: " << calculateMaxDrawdown(prices) * 100 << "%\n\n";

            // Technical indicators
            if (indicators == "all" || indicators == "momentum") {
                output << "📈 Momentum Indicators:\n";
                auto rsi = TechnicalIndicators::calculateRSI(prices);
                auto macd = TechnicalIndicators::calculateMACD(prices);

                output << "  RSI(14): " << std::setprecision(1) << rsi.getCurrentValue();
                if (rsi.isOverbought()) output << " (Overbought)";
                else if (rsi.isOversold()) output << " (Oversold)";
                output << "\n";

                output << "  MACD Signal: ";
                auto signal = macd.getCurrentSignal();
                if (signal == MACD::Signal::BUY) output << "BUY\n";
                else if (signal == MACD::Signal::SELL) output << "SELL\n";
                else output << "HOLD\n";
                output << "\n";
            }

            if (indicators == "all" || indicators == "volatility") {
                output << "📊 Volatility Indicators:\n";
                auto bollinger = TechnicalIndicators::calculateBollingerBands(prices);
                output << "  Bollinger Position: " << std::setprecision(1) << bollinger.getCurrentPercentB() * 100 << "%";
                if (bollinger.isCurrentlyOverbought()) output << " (Near Upper Band)";
                else if (bollinger.isCurrentlyOversold()) output << " (Near Lower Band)";
                output << "\n\n";
            }

            if (indicators == "all" || indicators == "trend") {
                output << "📈 Trend Analysis:\n";
                auto trendAnalysis = TechnicalIndicators::performTrendAnalysis(prices, prices, prices, volumes);
                output << "  Trend Direction: " << trendAnalysis.trendDirection << "\n";
                output << "  Trend Strength: " << std::setprecision(1) << trendAnalysis.trendStrength * 100 << "%\n";
                output << "  Confidence: " << trendAnalysis.confidenceLevel * 100 << "%\n\n";
            }

            // Correlation analysis
            if (includeCorrelations) {
                output << "🔗 Correlation Analysis:\n";
                auto correlations = calculateMarketCorrelations(symbol);
                output << "  Top Correlated Assets:\n";
                for (const auto& corr : correlations) {
                    output << "    " << corr.first << ": " << std::setprecision(3) << corr.second << "\n";
                }
                output << "\n";
            }

            // Risk metrics
            output << "⚠️  Risk Metrics:\n";
            output << "  VaR (95%): " << std::setprecision(2) << calculateVaR(returns, 0.95) * 100 << "%\n";
            output << "  Expected Shortfall: " << calculateExpectedShortfall(returns, 0.95) * 100 << "%\n";
            output << "  Downside Deviation: " << StatisticalTools::calculateDownsideDeviation(returns) * 100 << "%\n\n";

            // Summary and signals
            output << "🎯 Trading Signals:\n";
            auto signals = generateTradingSignals(symbol, prices, volumes);
            for (const auto& signal : signals) {
                output << "  " << signal << "\n";
            }

            auto result = CommandResult::createSuccess(output.str());
            if (format == "json") {
                result.addOutput("symbol", symbol);
                result.addOutput("volatility", std::to_string(StatisticalTools::calculateVolatility(returns)));
                result.addOutput("sharpe_ratio", std::to_string(StatisticalTools::calculateSharpeRatio(returns)));
                result.addOutput("rsi", std::to_string(TechnicalIndicators::calculateRSI(prices).getCurrentValue()));
            }
            return result;

        } catch (const std::exception& e) {
            return CommandResult::createError("Analysis failed", e.what());
        }
    }

private:
    // Helper methods
    std::vector<Database::Models::MarketData> getMarketData(const std::string& symbol, const std::string& timeframe) {
        // Implementation to fetch market data from database
        return {}; // Placeholder
    }

    std::vector<double> extractPrices(const std::vector<Database::Models::MarketData>& data) {
        std::vector<double> prices;
        for (const auto& item : data) {
            prices.push_back(item.getClose());
        }
        return prices;
    }

    std::vector<double> extractVolumes(const std::vector<Database::Models::MarketData>& data) {
        std::vector<double> volumes;
        for (const auto& item : data) {
            volumes.push_back(item.getVolume());
        }
        return volumes;
    }

    double calculateMaxDrawdown(const std::vector<double>& prices) {
        double maxDrawdown = 0.0;
        double peak = prices[0];

        for (double price : prices) {
            if (price > peak) peak = price;
            double drawdown = (peak - price) / peak;
            if (drawdown > maxDrawdown) maxDrawdown = drawdown;
        }
        return maxDrawdown;
    }

    double calculateVaR(const std::vector<double>& returns, double confidence) {
        std::vector<double> sortedReturns = returns;
        std::sort(sortedReturns.begin(), sortedReturns.end());
        int index = static_cast<int>((1.0 - confidence) * sortedReturns.size());
        return -sortedReturns[index]; // Positive VaR
    }

    double calculateExpectedShortfall(const std::vector<double>& returns, double confidence) {
        std::vector<double> sortedReturns = returns;
        std::sort(sortedReturns.begin(), sortedReturns.end());
        int cutoff = static_cast<int>((1.0 - confidence) * sortedReturns.size());

        double sum = 0.0;
        for (int i = 0; i < cutoff; ++i) {
            sum += sortedReturns[i];
        }
        return cutoff > 0 ? -sum / cutoff : 0.0; // Positive ES
    }

    std::vector<std::pair<std::string, double>> calculateMarketCorrelations(const std::string& symbol) {
        // Implementation to calculate correlations with other assets
        return {{"BTC", 0.85}, {"ETH", 0.72}, {"SOL", 0.65}}; // Placeholder
    }

    std::vector<std::string> generateTradingSignals(const std::string& symbol,
                                                   const std::vector<double>& prices,
                                                   const std::vector<double>& volumes) {
        std::vector<std::string> signals;

        auto rsi = TechnicalIndicators::calculateRSI(prices);
        if (rsi.isOversold()) {
            signals.push_back("🟢 RSI Oversold - Potential Buy Signal");
        } else if (rsi.isOverbought()) {
            signals.push_back("🔴 RSI Overbought - Potential Sell Signal");
        }

        auto macd = TechnicalIndicators::calculateMACD(prices);
        auto macdSignal = macd.getCurrentSignal();
        if (macdSignal == MACD::Signal::BUY) {
            signals.push_back("🟢 MACD Bullish Crossover");
        } else if (macdSignal == MACD::Signal::SELL) {
            signals.push_back("🔴 MACD Bearish Crossover");
        }

        if (signals.empty()) {
            signals.push_back("🟡 No clear signals - Hold position");
        }

        return signals;
    }
};

// Sentiment analysis reporting command
class SentimentReportCommand : public DataCommand {
private:
    DatabaseManager& dbManager_;
    std::shared_ptr<SentimentQualityManager> qualityManager_;

public:
    SentimentReportCommand(DatabaseManager& dbManager, std::shared_ptr<SentimentQualityManager> qualityManager)
        : dbManager_(dbManager), qualityManager_(qualityManager) {}

    std::string getName() const override { return "sentiment-report"; }
    std::string getDescription() const override { return "Generate comprehensive sentiment analysis report"; }

    std::vector<ParameterDefinition> getParameters() const override {
        return {
            {"symbol", ParameterType::SYMBOL, false, "Cryptocurrency symbol (all if not specified)", ""},
            {"timeframe", ParameterType::STRING, false, "Report timeframe", "24h", {"1h", "6h", "24h", "7d", "30d"}},
            {"sources", ParameterType::STRING, false, "Include source breakdown", "true", {"true", "false"}},
            {"quality", ParameterType::STRING, false, "Quality assessment level", "standard", {"basic", "standard", "detailed"}},
            {"trends", ParameterType::BOOLEAN, false, "Include trend analysis", "true"},
            {"alerts", ParameterType::BOOLEAN, false, "Show sentiment alerts", "true"}
        };
    }

    std::vector<std::string> getAliases() const override { return {"sentiment", "news-sentiment"}; }

    CommandResult execute(CommandContext& context) override {
        std::string symbol = context.getStringParameter("symbol", "");
        std::string timeframe = context.getStringParameter("timeframe", "24h");
        bool includeSources = context.getStringParameter("sources", "true") == "true";
        std::string qualityLevel = context.getStringParameter("quality", "standard");
        bool includeTrends = context.getBoolParameter("trends").value_or(true);
        bool showAlerts = context.getBoolParameter("alerts").value_or(true);

        std::ostringstream output;

        if (symbol.empty()) {
            output << "=== Global Sentiment Report (" << timeframe << ") ===\n\n";
        } else {
            output << "=== Sentiment Report: " << symbol << " (" << timeframe << ") ===\n\n";
        }

        try {
            // Overall sentiment summary
            auto overallScore = qualityManager_->getOverallSentimentQualityScore();
            output << "📊 Overall Sentiment Quality: " << std::fixed << std::setprecision(1)
                   << (overallScore * 100) << "%\n\n";

            // Symbol-specific or global analysis
            if (!symbol.empty()) {
                generateSymbolSentimentReport(output, symbol, timeframe, qualityLevel, includeTrends);
            } else {
                generateGlobalSentimentReport(output, timeframe, qualityLevel);
            }

            // Source breakdown
            if (includeSources) {
                output << "📰 Source Quality Breakdown:\n";
                auto sourceProfiles = qualityManager_->getSourceQualityProfiles();
                for (const auto& profile : sourceProfiles) {
                    output << "  " << profile.sourceName << ": "
                           << std::setprecision(1) << (profile.overallQuality * 100) << "% "
                           << "(" << profile.articlesAnalyzed << " articles)\n";
                }
                output << "\n";
            }

            // Quality alerts and anomalies
            if (showAlerts) {
                output << "⚠️  Sentiment Alerts:\n";
                auto qualityReport = qualityManager_->generateQualityReport();
                if (qualityReport.activeAnomalies > 0) {
                    output << "  " << qualityReport.activeAnomalies << " active quality anomalies detected\n";
                    for (const auto& anomaly : qualityReport.criticalAnomalies) {
                        output << "  🔴 " << anomaly.description << " (Severity: "
                               << std::setprecision(1) << (anomaly.severity * 100) << "%)\n";
                    }
                } else {
                    output << "  🟢 No active sentiment anomalies\n";
                }
                output << "\n";
            }

            // Trend analysis
            if (includeTrends) {
                output << "📈 Sentiment Trends:\n";
                std::vector<std::string> symbols = symbol.empty() ?
                    std::vector<std::string>{"BTC", "ETH", "ADA", "SOL"} :
                    std::vector<std::string>{symbol};

                auto trends = qualityManager_->analyzeSentimentQualityTrends(symbols, 7);
                for (const auto& trend : trends) {
                    output << "  " << trend.symbol << ": ";
                    if (trend.currentTrendDirection > 0.1) {
                        output << "📈 Improving";
                    } else if (trend.currentTrendDirection < -0.1) {
                        output << "📉 Declining";
                    } else {
                        output << "➡️ Stable";
                    }
                    output << " (" << trend.primaryQualityDriver << ")\n";
                }
                output << "\n";
            }

            // Recommendations
            output << "💡 Recommendations:\n";
            generateRecommendations(output, symbol, qualityLevel);

            auto result = CommandResult::createSuccess(output.str());
            result.addOutput("overall_quality", std::to_string(overallScore));
            return result;

        } catch (const std::exception& e) {
            return CommandResult::createError("Sentiment report generation failed", e.what());
        }
    }

private:
    void generateSymbolSentimentReport(std::ostringstream& output, const std::string& symbol,
                                     const std::string& timeframe, const std::string& qualityLevel,
                                     bool includeTrends) {
        // Get recent sentiment data for symbol
        auto sentimentData = getRecentSentimentData(symbol, timeframe);

        if (sentimentData.empty()) {
            output << "No sentiment data available for " << symbol << "\n\n";
            return;
        }

        // Calculate average sentiment
        double avgSentiment = 0.0;
        for (const auto& data : sentimentData) {
            avgSentiment += data.getAvgSentiment();
        }
        avgSentiment /= sentimentData.size();

        output << "🎯 Current Sentiment: ";
        if (avgSentiment > 0.6) {
            output << "🟢 Bullish (" << std::setprecision(1) << (avgSentiment * 100) << "%)";
        } else if (avgSentiment < 0.4) {
            output << "🔴 Bearish (" << std::setprecision(1) << (avgSentiment * 100) << "%)";
        } else {
            output << "🟡 Neutral (" << std::setprecision(1) << (avgSentiment * 100) << "%)";
        }
        output << "\n\n";

        // Quality metrics
        if (qualityLevel != "basic") {
            auto qualityMetric = qualityManager_->assessSentimentQuality(symbol, sentimentData);
            output << "📋 Quality Metrics:\n";
            output << "  Data Completeness: " << std::setprecision(1) << (qualityMetric.dataCompletenessScore * 100) << "%\n";
            output << "  Source Credibility: " << (qualityMetric.sourceCredibilityScore * 100) << "%\n";
            output << "  Freshness: " << (qualityMetric.freshnessScore * 100) << "%\n";
            if (qualityLevel == "detailed") {
                output << "  Market Relevance: " << (qualityMetric.marketRelevanceScore * 100) << "%\n";
                output << "  Consistency: " << (qualityMetric.consistencyScore * 100) << "%\n";
            }
            output << "\n";
        }
    }

    void generateGlobalSentimentReport(std::ostringstream& output, const std::string& timeframe,
                                     const std::string& qualityLevel) {
        output << "🌍 Global Cryptocurrency Sentiment Overview:\n\n";

        // Top sentiment performers
        std::vector<std::string> topSymbols = {"BTC", "ETH", "ADA", "SOL", "MATIC", "DOT"};
        for (const auto& sym : topSymbols) {
            auto sentimentData = getRecentSentimentData(sym, timeframe);
            if (!sentimentData.empty()) {
                double avgSentiment = 0.0;
                for (const auto& data : sentimentData) {
                    avgSentiment += data.getAvgSentiment();
                }
                avgSentiment /= sentimentData.size();

                output << "  " << sym << ": ";
                if (avgSentiment > 0.6) output << "🟢";
                else if (avgSentiment < 0.4) output << "🔴";
                else output << "🟡";
                output << " " << std::setprecision(1) << (avgSentiment * 100) << "%\n";
            }
        }
        output << "\n";
    }

    void generateRecommendations(std::ostringstream& output, const std::string& symbol,
                               const std::string& qualityLevel) {
        // Generate recommendations based on analysis
        if (!symbol.empty()) {
            auto recommendedSources = qualityManager_->getRecommendedSources(symbol);
            if (!recommendedSources.empty()) {
                output << "  Consider monitoring these high-quality sources:\n";
                for (const auto& source : recommendedSources) {
                    output << "    📰 " << source << "\n";
                }
            }
        }

        output << "  📊 Monitor sentiment quality metrics regularly\n";
        output << "  🔄 Set up alerts for sentiment anomalies\n";
        if (qualityLevel == "detailed") {
            output << "  📈 Correlate sentiment with price movements for validation\n";
            output << "  🎯 Focus on high-credibility sources for trading signals\n";
        }
    }

    std::vector<Database::Models::SentimentData> getRecentSentimentData(const std::string& symbol,
                                                                       const std::string& timeframe) {
        // Implementation to fetch recent sentiment data
        return {}; // Placeholder
    }
};

// Data quality monitoring command
class DataQualityCommand : public SystemCommand {
private:
    DatabaseManager& dbManager_;

public:
    explicit DataQualityCommand(DatabaseManager& dbManager) : dbManager_(dbManager) {}

    std::string getName() const override { return "data-quality"; }
    std::string getDescription() const override { return "Monitor and report data pipeline quality"; }

    std::vector<ParameterDefinition> getParameters() const override {
        return {
            {"component", ParameterType::STRING, false, "Data component to check", "all",
             {"all", "market", "sentiment", "pipeline", "database"}},
            {"detail", ParameterType::STRING, false, "Detail level", "summary", {"summary", "detailed", "diagnostic"}},
            {"alerts", ParameterType::BOOLEAN, false, "Show active alerts", "true"},
            {"history", ParameterType::INTEGER, false, "Hours of history to analyze", "24"}
        };
    }

    CommandResult execute(CommandContext& context) override {
        std::string component = context.getStringParameter("component", "all");
        std::string detail = context.getStringParameter("detail", "summary");
        bool showAlerts = context.getBoolParameter("alerts").value_or(true);
        int historyHours = context.getIntParameter("history").value_or(24);

        std::ostringstream output;
        output << "=== Data Quality Report ===\n\n";

        try {
            auto& qualityManager = dbManager_.getDataQualityManager();

            // Overall quality score
            double overallQuality = qualityManager.getOverallQualityScore();
            output << "🎯 Overall Data Quality: " << std::fixed << std::setprecision(1)
                   << (overallQuality * 100) << "%\n\n";

            // Component-specific analysis
            if (component == "all" || component == "database") {
                output << "💾 Database Quality:\n";
                output << "  Connection Status: " << (dbManager_.isConnected() ? "🟢 Connected" : "🔴 Disconnected") << "\n";

                // Check key tables
                std::vector<std::string> keyTables = {"market_data", "sentiment_data", "portfolios"};
                for (const std::string& table : keyTables) {
                    bool exists = dbManager_.tableExists(table);
                    output << "  " << table << ": " << (exists ? "🟢 OK" : "🔴 Missing") << "\n";
                }
                output << "\n";
            }

            if (component == "all" || component == "market") {
                output << "📈 Market Data Quality:\n";
                // Assess market data quality
                auto marketQuality = qualityManager.assessTable("market_data");
                output << "  Completeness: " << std::setprecision(1) << (marketQuality.completenessRatio * 100) << "%\n";
                output << "  Accuracy Score: " << (marketQuality.accuracyScore * 100) << "%\n";
                output << "  Outliers Detected: " << marketQuality.outlierCount << "\n\n";
            }

            if (component == "all" || component == "sentiment") {
                output << "📰 Sentiment Data Quality:\n";
                auto sentimentQuality = qualityManager.assessTable("sentiment_data");
                output << "  Completeness: " << std::setprecision(1) << (sentimentQuality.completenessRatio * 100) << "%\n";
                output << "  Accuracy Score: " << (sentimentQuality.accuracyScore * 100) << "%\n";
                output << "  Recent Records: " << sentimentQuality.totalRecords << "\n\n";
            }

            // Active alerts and anomalies
            if (showAlerts) {
                output << "⚠️  Active Quality Alerts:\n";
                auto anomalies = qualityManager.detectAnomalies();
                if (anomalies.empty()) {
                    output << "  🟢 No active data quality issues\n";
                } else {
                    for (const auto& anomaly : anomalies) {
                        output << "  🔴 " << anomaly.description
                               << " (Severity: " << std::setprecision(1) << (anomaly.severity * 100) << "%)\n";
                    }
                }
                output << "\n";
            }

            // Detailed diagnostics
            if (detail == "diagnostic") {
                output << "🔧 Diagnostic Information:\n";
                output << "  Database Version: ";
                try {
                    auto& migrationManager = dbManager_.getMigrationManager();
                    output << migrationManager.getCurrentVersion() << "/" << migrationManager.getTargetVersion() << "\n";
                } catch (...) {
                    output << "Unable to determine\n";
                }

                output << "  Quality Framework: Active\n";
                output << "  Auto-remediation: " << (qualityManager.isAutoRemediationEnabled() ? "Enabled" : "Disabled") << "\n";
                output << "  Last Assessment: " << getCurrentTimestamp() << "\n";
            }

            auto result = CommandResult::createSuccess(output.str());
            result.addOutput("overall_quality", std::to_string(overallQuality));
            result.addOutput("alerts_count", std::to_string(qualityManager.detectAnomalies().size()));
            return result;

        } catch (const std::exception& e) {
            return CommandResult::createError("Data quality check failed", e.what());
        }
    }

private:
    std::string getCurrentTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::ostringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }
};

} // namespace Commands
} // namespace Console
} // namespace CryptoClaude