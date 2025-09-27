#pragma once

#include "../CommandFramework.h"
#include "../../Database/DatabaseManager.h"
#include "../../DataPipeline/EnhancedMarketDataPipeline.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>

namespace CryptoClaude {
namespace Console {
namespace Commands {

using namespace CryptoClaude::Database;
using namespace CryptoClaude::DataPipeline;

// Data status command - comprehensive data pipeline monitoring
class DataStatusCommand : public DataCommand {
private:
    DatabaseManager& dbManager_;
    std::shared_ptr<EnhancedMarketDataPipeline> dataPipeline_;

public:
    DataStatusCommand(DatabaseManager& dbManager, std::shared_ptr<EnhancedMarketDataPipeline> dataPipeline)
        : dbManager_(dbManager), dataPipeline_(dataPipeline) {}

    std::string getName() const override { return "data"; }
    std::string getDescription() const override { return "Monitor data pipeline and quality status"; }

    std::vector<ParameterDefinition> getParameters() const override {
        return {
            {"action", ParameterType::STRING, true, "Action: status, gaps, refresh", "", {"status", "gaps", "refresh"}},
            {"days", ParameterType::INTEGER, false, "Number of days to analyze", "7"},
            {"symbol", ParameterType::SYMBOL, false, "Specific symbol to analyze", ""},
            {"detailed", ParameterType::BOOLEAN, false, "Show detailed information", "false"}
        };
    }

    CommandResult execute(CommandContext& context) override {
        std::string action = context.getStringParameter("action");
        int days = context.getIntParameter("days").value_or(7);
        std::string symbol = context.getStringParameter("symbol", "");
        bool detailed = context.getBoolParameter("detailed").value_or(false);

        std::ostringstream output;

        try {
            if (action == "status") {
                return executeDataStatus(symbol, days, detailed, output);
            } else if (action == "gaps") {
                return executeDataGaps(symbol, days, detailed, output);
            } else if (action == "refresh") {
                return executeDataRefresh(symbol, output);
            } else {
                return CommandResult::createError("Invalid action. Use: status, gaps, or refresh");
            }

        } catch (const std::exception& e) {
            return CommandResult::createError("Data command failed", e.what());
        }
    }

private:
    CommandResult executeDataStatus(const std::string& symbol, int days, bool detailed, std::ostringstream& output) {
        output << "📊 DATA PIPELINE STATUS\n";
        output << "══════════════════════\n\n";

        // Overall pipeline health
        bool pipelineHealthy = dataPipeline_ && dataPipeline_->isHealthy();
        output << "🎯 Pipeline Status: " << (pipelineHealthy ? "🟢 HEALTHY" : "🔴 ISSUES DETECTED") << "\n";

        if (pipelineHealthy && dataPipeline_) {
            auto lastUpdate = dataPipeline_->getLastUpdateTime();
            auto freshnessMinutes = dataPipeline_->getDataFreshnessMinutes();

            output << "   Last Update: " << formatTimestamp(lastUpdate) << "\n";
            output << "   Data Freshness: " << freshnessMinutes << " minutes ago\n";
            output << "   API Calls Today: " << dataPipeline_->getTodayAPICallCount() << "\n";
            output << "   Cache Hit Rate: " << std::fixed << std::setprecision(1)
                   << (dataPipeline_->getCacheHitRate() * 100) << "%\n\n";
        }

        // Database connectivity and size
        output << "💾 Database Status:\n";
        if (dbManager_.isConnected()) {
            output << "   Connection: 🟢 CONNECTED\n";

            // Get data counts per symbol
            auto symbols = symbol.empty() ?
                std::vector<std::string>{"BTC", "ETH", "ADA", "SOL", "DOT", "MATIC"} :
                std::vector<std::string>{symbol};

            output << "   Data Coverage (last " << days << " days):\n\n";
            output << "Symbol   Records    Latest Update       Completeness\n";
            output << "──────   ───────    ─────────────       ──────────────\n";

            int totalRecords = 0;
            int healthySymbols = 0;

            for (const auto& sym : symbols) {
                auto dataStats = getSymbolDataStats(sym, days);
                totalRecords += dataStats.recordCount;

                output << std::left << std::setw(6) << sym << "   "
                       << std::right << std::setw(7) << dataStats.recordCount << "   "
                       << std::setw(19) << formatTimestamp(dataStats.latestTimestamp) << "   ";

                if (dataStats.completenessRatio >= 0.95) {
                    output << "🟢 " << std::fixed << std::setprecision(1) << (dataStats.completenessRatio * 100) << "%";
                    healthySymbols++;
                } else if (dataStats.completenessRatio >= 0.90) {
                    output << "🟡 " << std::fixed << std::setprecision(1) << (dataStats.completenessRatio * 100) << "%";
                } else {
                    output << "🔴 " << std::fixed << std::setprecision(1) << (dataStats.completenessRatio * 100) << "%";
                }
                output << "\n";
            }

            output << "\n📈 Data Summary:\n";
            output << "   Total Records: " << totalRecords << "\n";
            output << "   Healthy Symbols: " << healthySymbols << "/" << symbols.size() << "\n";

            double overallHealth = symbols.empty() ? 0.0 : (double)healthySymbols / symbols.size();
            output << "   Overall Health: " << std::fixed << std::setprecision(0) << (overallHealth * 100) << "%\n\n";

        } else {
            output << "   Connection: 🔴 DISCONNECTED\n";
            output << "   Last Error: " << dbManager_.getLastError() << "\n\n";
        }

        // Data quality assessment
        if (dbManager_.isConnected()) {
            try {
                auto& qualityManager = dbManager_.getDataQualityManager();
                double qualityScore = qualityManager.getOverallQualityScore();

                output << "✅ Data Quality Assessment:\n";
                output << "   Overall Quality Score: " << std::fixed << std::setprecision(1)
                       << (qualityScore * 100) << "%\n";

                auto anomalies = qualityManager.detectAnomalies();
                output << "   Active Anomalies: " << anomalies.size() << "\n";

                if (!anomalies.empty() && detailed) {
                    output << "\n🚨 Data Quality Issues:\n";
                    for (const auto& anomaly : anomalies) {
                        output << "   " << getRiskIcon(anomaly.severity) << " " << anomaly.description << "\n";
                    }
                }
                output << "\n";

            } catch (const std::exception& e) {
                output << "⚠️  Data Quality: Unable to assess (" << e.what() << ")\n\n";
            }
        }

        // API usage and limits
        if (dataPipeline_) {
            output << "📡 API Usage Status:\n";
            auto todayUsage = dataPipeline_->getTodayAPICallCount();
            auto dailyLimit = dataPipeline_->getDailyAPILimit();
            double usagePercent = dailyLimit > 0 ? (double)todayUsage / dailyLimit * 100 : 0.0;

            output << "   Daily API Calls: " << todayUsage << "/" << dailyLimit;
            if (usagePercent >= 90) {
                output << " 🔴 CRITICAL (" << std::fixed << std::setprecision(1) << usagePercent << "%)";
            } else if (usagePercent >= 75) {
                output << " 🟡 WARNING (" << std::fixed << std::setprecision(1) << usagePercent << "%)";
            } else {
                output << " 🟢 OK (" << std::fixed << std::setprecision(1) << usagePercent << "%)";
            }
            output << "\n";

            auto avgResponseTime = dataPipeline_->getAverageResponseTime();
            output << "   Average Response Time: " << avgResponseTime.count() << "ms\n";

            if (detailed) {
                output << "   Cache Statistics:\n";
                output << "     Cache Hit Rate: " << std::fixed << std::setprecision(1)
                       << (dataPipeline_->getCacheHitRate() * 100) << "%\n";
                output << "     Cached Records: " << dataPipeline_->getCachedRecordCount() << "\n";
                output << "     Cache Size: " << formatBytes(dataPipeline_->getCacheSizeBytes()) << "\n";
            }
        }

        auto result = CommandResult::createSuccess(output.str());
        result.addOutput("pipeline_healthy", pipelineHealthy ? "true" : "false");
        result.addOutput("database_connected", dbManager_.isConnected() ? "true" : "false");
        return result;
    }

    CommandResult executeDataGaps(const std::string& symbol, int days, bool detailed, std::ostringstream& output) {
        output << "🔍 DATA GAP ANALYSIS\n";
        output << "══════════════════\n\n";

        output << "📊 Analysis Period: Last " << days << " days\n";
        if (!symbol.empty()) {
            output << "📍 Target Symbol: " << symbol << "\n";
        }
        output << "\n";

        auto symbols = symbol.empty() ?
            std::vector<std::string>{"BTC", "ETH", "ADA", "SOL", "DOT", "MATIC", "LINK", "AVAX"} :
            std::vector<std::string>{symbol};

        output << "Symbol   Gap Count   Largest Gap      Missing Hours   Status\n";
        output << "──────   ─────────   ───────────      ─────────────   ──────\n";

        int totalGaps = 0;
        int criticalGaps = 0;

        for (const auto& sym : symbols) {
            auto gapAnalysis = analyzeDataGaps(sym, days);
            totalGaps += gapAnalysis.totalGaps;

            output << std::left << std::setw(6) << sym << "   "
                   << std::right << std::setw(9) << gapAnalysis.totalGaps << "   "
                   << std::setw(11) << gapAnalysis.largestGapHours << "h" << "      "
                   << std::setw(13) << gapAnalysis.totalMissingHours << "      ";

            if (gapAnalysis.totalGaps == 0) {
                output << "🟢 COMPLETE";
            } else if (gapAnalysis.largestGapHours <= 2) {
                output << "🟡 MINOR";
            } else {
                output << "🔴 CRITICAL";
                criticalGaps++;
            }
            output << "\n";

            // Detailed gap information
            if (detailed && !gapAnalysis.gaps.empty()) {
                output << "   Gaps: ";
                for (size_t i = 0; i < std::min(3ul, gapAnalysis.gaps.size()); ++i) {
                    if (i > 0) output << ", ";
                    output << formatTimestamp(gapAnalysis.gaps[i].startTime) << " (" << gapAnalysis.gaps[i].durationHours << "h)";
                }
                if (gapAnalysis.gaps.size() > 3) {
                    output << " ... +" << (gapAnalysis.gaps.size() - 3) << " more";
                }
                output << "\n";
            }
        }

        output << "\n📊 Gap Analysis Summary:\n";
        output << "   Total Gaps Found: " << totalGaps << "\n";
        output << "   Critical Gaps: " << criticalGaps << " symbols\n";
        output << "   Symbols Analyzed: " << symbols.size() << "\n";

        if (totalGaps > 0) {
            output << "\n💡 Recommendations:\n";
            if (criticalGaps > 0) {
                output << "   🔴 " << criticalGaps << " symbols have critical data gaps\n";
                output << "   📥 Run 'data refresh' to fill gaps automatically\n";
            }
            output << "   🔄 Consider increasing data ingestion frequency\n";
            output << "   📊 Monitor data pipeline health more closely\n";
        } else {
            output << "\n✅ No data gaps detected - excellent data coverage!\n";
        }

        auto result = CommandResult::createSuccess(output.str());
        result.addOutput("total_gaps", std::to_string(totalGaps));
        result.addOutput("critical_gaps", std::to_string(criticalGaps));
        result.addOutput("symbols_analyzed", std::to_string(symbols.size()));
        return result;
    }

    CommandResult executeDataRefresh(const std::string& symbol, std::ostringstream& output) {
        output << "🔄 DATA REFRESH OPERATION\n";
        output << "════════════════════════\n\n";

        if (!symbol.empty()) {
            output << "📍 Target Symbol: " << symbol << "\n";
        } else {
            output << "📍 Target: All configured symbols\n";
        }

        if (!dataPipeline_) {
            return CommandResult::createError("Data pipeline not available");
        }

        output << "🔄 Starting incremental data refresh...\n\n";

        try {
            // Perform incremental update
            auto result = dataPipeline_->runIncrementalUpdate();

            output << "📊 Refresh Results:\n";
            output << "   Status: " << (result.success ? "✅ SUCCESS" : "⚠️  PARTIAL SUCCESS") << "\n";
            output << "   Symbols Processed: " << result.symbolsProcessed << "\n";
            output << "   Data Points Ingested: " << result.dataPointsIngested << "\n";
            output << "   Data Points Rejected: " << result.dataPointsRejected << "\n";
            output << "   Execution Time: " << result.duration.count() << "ms\n";

            if (result.dataPointsIngested > 0) {
                output << "   Ingestion Rate: " << std::fixed << std::setprecision(1)
                       << result.averageIngestionRate << " points/sec\n";
            }

            // Quality assessment
            if (result.qualityIssuesFound > 0) {
                output << "\n⚠️  Quality Issues:\n";
                output << "   Issues Found: " << result.qualityIssuesFound << "\n";
                output << "   Issues Remediated: " << result.qualityIssuesRemediated << "\n";
                output << "   Overall Quality: " << std::fixed << std::setprecision(1)
                       << (result.overallQualityScore * 100) << "%\n";
            }

            if (!result.success && !result.errorMessage.empty()) {
                output << "\n❌ Errors Encountered:\n";
                output << "   " << result.errorMessage << "\n";
            }

            if (result.dataPointsIngested > 0) {
                output << "\n✅ Data refresh completed successfully!\n";
                output << "💡 Run 'data gaps' to verify gap closure\n";
            } else if (result.success) {
                output << "\n✅ No new data needed - system is up to date!\n";
            }

            auto cmdResult = CommandResult::createSuccess(output.str());
            cmdResult.addOutput("data_points_ingested", std::to_string(result.dataPointsIngested));
            cmdResult.addOutput("symbols_processed", std::to_string(result.symbolsProcessed));
            cmdResult.addOutput("quality_score", std::to_string(result.overallQualityScore));
            return cmdResult;

        } catch (const std::exception& e) {
            output << "❌ Data refresh failed: " << e.what() << "\n";
            return CommandResult::createError(output.str());
        }
    }

    // Helper structures and methods
    struct SymbolDataStats {
        int recordCount = 0;
        std::chrono::system_clock::time_point latestTimestamp;
        double completenessRatio = 0.0;
    };

    struct DataGap {
        std::chrono::system_clock::time_point startTime;
        std::chrono::system_clock::time_point endTime;
        int durationHours = 0;
    };

    struct GapAnalysis {
        int totalGaps = 0;
        int largestGapHours = 0;
        int totalMissingHours = 0;
        std::vector<DataGap> gaps;
    };

    SymbolDataStats getSymbolDataStats(const std::string& symbol, int days) {
        SymbolDataStats stats;

        try {
            // Calculate timestamp for N days ago
            auto now = std::chrono::system_clock::now();
            auto daysAgo = now - std::chrono::hours(days * 24);
            auto daysAgoTimestamp = std::chrono::system_clock::to_time_t(daysAgo);

            // Query for record count and latest timestamp
            std::string query = R"(
                SELECT
                    COUNT(*) as record_count,
                    MAX(timestamp) as latest_timestamp
                FROM market_data
                WHERE symbol = ? AND timestamp >= ?
            )";

            auto stmt = dbManager_.prepareStatement(query);
            if (stmt) {
                StatementWrapper wrapper(stmt);
                wrapper.bindString(1, symbol);
                wrapper.bindInt64(2, daysAgoTimestamp);

                if (wrapper.step()) {
                    stats.recordCount = wrapper.getIntColumn(0);
                    auto latestTimestamp = wrapper.getInt64Column(1);
                    stats.latestTimestamp = std::chrono::system_clock::from_time_t(latestTimestamp);

                    // Calculate completeness (assuming hourly data)
                    int expectedRecords = days * 24; // Hourly records for N days
                    stats.completenessRatio = expectedRecords > 0 ?
                        std::min(1.0, (double)stats.recordCount / expectedRecords) : 0.0;
                }
            }

        } catch (const std::exception& e) {
            // Return default stats on error
            stats = SymbolDataStats{};
        }

        return stats;
    }

    GapAnalysis analyzeDataGaps(const std::string& symbol, int days) {
        GapAnalysis analysis;

        try {
            // Get all timestamps for this symbol in chronological order
            auto now = std::chrono::system_clock::now();
            auto daysAgo = now - std::chrono::hours(days * 24);
            auto daysAgoTimestamp = std::chrono::system_clock::to_time_t(daysAgo);

            std::string query = R"(
                SELECT timestamp
                FROM market_data
                WHERE symbol = ? AND timestamp >= ?
                ORDER BY timestamp ASC
            )";

            auto stmt = dbManager_.prepareStatement(query);
            if (stmt) {
                StatementWrapper wrapper(stmt);
                wrapper.bindString(1, symbol);
                wrapper.bindInt64(2, daysAgoTimestamp);

                std::vector<std::time_t> timestamps;
                while (wrapper.step()) {
                    timestamps.push_back(wrapper.getInt64Column(0));
                }

                // Analyze gaps (assuming hourly data, so gap > 2 hours is significant)
                for (size_t i = 1; i < timestamps.size(); ++i) {
                    int gapHours = (timestamps[i] - timestamps[i-1]) / 3600;

                    if (gapHours > 2) { // More than 2 hours is a gap
                        DataGap gap;
                        gap.startTime = std::chrono::system_clock::from_time_t(timestamps[i-1]);
                        gap.endTime = std::chrono::system_clock::from_time_t(timestamps[i]);
                        gap.durationHours = gapHours;

                        analysis.gaps.push_back(gap);
                        analysis.totalGaps++;
                        analysis.totalMissingHours += gapHours;
                        analysis.largestGapHours = std::max(analysis.largestGapHours, gapHours);
                    }
                }
            }

        } catch (const std::exception& e) {
            // Return default analysis on error
            analysis = GapAnalysis{};
        }

        return analysis;
    }

    std::string formatTimestamp(std::chrono::system_clock::time_point timestamp) {
        auto time_t = std::chrono::system_clock::to_time_t(timestamp);
        std::ostringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%m/%d %H:%M");
        return ss.str();
    }

    std::string formatBytes(size_t bytes) {
        const char* units[] = {"B", "KB", "MB", "GB"};
        int unit = 0;
        double size = bytes;

        while (size >= 1024 && unit < 3) {
            size /= 1024;
            unit++;
        }

        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << size << units[unit];
        return ss.str();
    }

    std::string getRiskIcon(double severity) {
        if (severity >= 0.8) return "🔴";
        else if (severity >= 0.6) return "🟠";
        else if (severity >= 0.4) return "🟡";
        else return "🟢";
    }
};

// Cache statistics command
class CacheStatsCommand : public DataCommand {
private:
    std::shared_ptr<EnhancedMarketDataPipeline> dataPipeline_;

public:
    explicit CacheStatsCommand(std::shared_ptr<EnhancedMarketDataPipeline> dataPipeline)
        : dataPipeline_(dataPipeline) {}

    std::string getName() const override { return "cache-stats"; }
    std::string getDescription() const override { return "Display data caching statistics and performance"; }

    std::vector<ParameterDefinition> getParameters() const override {
        return {
            {"detailed", ParameterType::BOOLEAN, false, "Show detailed cache statistics", "false"},
            {"breakdown", ParameterType::BOOLEAN, false, "Show cache breakdown by symbol", "false"}
        };
    }

    CommandResult execute(CommandContext& context) override {
        bool detailed = context.getBoolParameter("detailed").value_or(false);
        bool breakdown = context.getBoolParameter("breakdown").value_or(false);

        std::ostringstream output;

        try {
            output << "💾 DATA CACHE STATISTICS\n";
            output << "═══════════════════════\n\n";

            if (!dataPipeline_) {
                output << "❌ Data pipeline not available\n";
                return CommandResult::createSuccess(output.str());
            }

            // Overall cache statistics
            output << "📊 Cache Performance:\n";
            output << "   Hit Rate: " << std::fixed << std::setprecision(1)
                   << (dataPipeline_->getCacheHitRate() * 100) << "%\n";
            output << "   Miss Rate: " << std::setprecision(1)
                   << ((1.0 - dataPipeline_->getCacheHitRate()) * 100) << "%\n";
            output << "   Cached Records: " << dataPipeline_->getCachedRecordCount() << "\n";
            output << "   Cache Size: " << formatBytes(dataPipeline_->getCacheSizeBytes()) << "\n";

            auto avgResponseTime = dataPipeline_->getAverageResponseTime();
            auto cacheResponseTime = dataPipeline_->getCacheResponseTime();

            output << "   Avg Response Time: " << avgResponseTime.count() << "ms\n";
            output << "   Cache Response Time: " << cacheResponseTime.count() << "ms\n";
            output << "   Performance Gain: " << std::setprecision(1)
                   << (avgResponseTime.count() > 0 ?
                       ((double)(avgResponseTime.count() - cacheResponseTime.count()) / avgResponseTime.count() * 100) : 0.0)
                   << "%\n\n";

            // Cache efficiency metrics
            if (detailed) {
                output << "⚡ Cache Efficiency:\n";
                output << "   Memory Usage: " << formatBytes(dataPipeline_->getCacheMemoryUsage()) << "\n";
                output << "   Eviction Rate: " << std::setprecision(2) << dataPipeline_->getCacheEvictionRate() << "/min\n";
                output << "   Cache Age (avg): " << dataPipeline_->getAverageCacheAge().count() << " minutes\n";
                output << "   Refresh Rate: " << dataPipeline_->getCacheRefreshRate() << "/hour\n\n";
            }

            // Symbol-specific breakdown
            if (breakdown) {
                output << "📈 Cache Breakdown by Symbol:\n";
                output << "Symbol   Cached Rec.   Hit Rate   Avg Age   Size\n";
                output << "──────   ───────────   ────────   ───────   ────\n";

                auto symbols = std::vector<std::string>{"BTC", "ETH", "ADA", "SOL", "DOT", "MATIC", "LINK", "AVAX"};
                for (const auto& symbol : symbols) {
                    auto stats = dataPipeline_->getSymbolCacheStats(symbol);

                    output << std::left << std::setw(6) << symbol << "   "
                           << std::right << std::setw(11) << stats.cachedRecords << "   "
                           << std::setw(8) << std::fixed << std::setprecision(1) << (stats.hitRate * 100) << "%   "
                           << std::setw(7) << stats.averageAge.count() << "m   "
                           << std::setw(4) << formatBytes(stats.sizeBytes) << "\n";
                }
                output << "\n";
            }

            // Recommendations
            auto hitRate = dataPipeline_->getCacheHitRate();
            if (hitRate < 0.7) {
                output << "💡 Recommendations:\n";
                output << "   🔴 Low cache hit rate (" << std::setprecision(1) << (hitRate * 100) << "%)\n";
                output << "   • Increase cache size\n";
                output << "   • Review cache eviction policy\n";
                output << "   • Consider longer cache TTL\n";
            } else if (hitRate > 0.95) {
                output << "✅ Excellent cache performance!\n";
                output << "   Cache is highly effective for API conservation\n";
            }

            auto result = CommandResult::createSuccess(output.str());
            result.addOutput("cache_hit_rate", std::to_string(hitRate));
            result.addOutput("cached_records", std::to_string(dataPipeline_->getCachedRecordCount()));
            return result;

        } catch (const std::exception& e) {
            return CommandResult::createError("Cache stats command failed", e.what());
        }
    }

private:
    std::string formatBytes(size_t bytes) {
        const char* units[] = {"B", "KB", "MB", "GB"};
        int unit = 0;
        double size = bytes;

        while (size >= 1024 && unit < 3) {
            size /= 1024;
            unit++;
        }

        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << size << units[unit];
        return ss.str();
    }
};

} // namespace Commands
} // namespace Console
} // namespace CryptoClaude