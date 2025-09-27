#pragma once

#include "../Risk/ProductionRiskManager.h"
#include "../Strategy/TradingStrategyOptimizer.h"
#include "../Analytics/AdvancedPerformanceEngine.h"
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <atomic>
#include <mutex>
#include <functional>
#include <future>

namespace CryptoClaude {
namespace AI {

/**
 * AI Score Polish Engine - Day 25 Implementation
 * Uses Claude API to refine random forest predictions by adjusting scores within limits
 * Provides intelligent score adjustments based on market context analysis
 */
class AIScorePolishEngine {
public:
    // AI Provider configuration - Simplified to Claude only
    enum class AIProvider {
        CLAUDE_API
    };

    struct APICredentials {
        std::string api_key;
        std::string api_url;
        std::string model_name;
        double timeout_seconds = 30.0;
        int max_retries = 3;
        double rate_limit_per_minute = 60.0;

        APICredentials() = default;
    };

    struct AIProviderConfig {
        AIProvider provider;
        APICredentials credentials;
        bool is_enabled = true;
        int priority = 1;  // Lower number = higher priority
        std::vector<std::string> supported_tasks;

        AIProviderConfig() : provider(AIProvider::CLAUDE_API) {}
    };

    // Market context for AI decision making
    struct MarketContext {
        std::chrono::system_clock::time_point timestamp;

        // Market data
        std::map<std::string, double> current_prices;
        std::map<std::string, double> price_changes_24h;
        std::map<std::string, double> volatility_measures;
        std::map<std::string, double> volume_indicators;

        // Portfolio state
        double total_portfolio_value;
        double unrealized_pnl;
        double realized_pnl_today;
        std::map<std::string, double> position_sizes;
        std::map<std::string, double> position_pnl;

        // Risk metrics
        double current_drawdown;
        double portfolio_volatility;
        double var_95_1day;
        double concentration_risk;

        // Market regime indicators
        std::string market_trend;        // "bullish", "bearish", "sideways"
        std::string volatility_regime;   // "low", "normal", "high", "extreme"
        double fear_greed_index;
        std::vector<std::string> active_alerts;

        MarketContext() : total_portfolio_value(0), unrealized_pnl(0), realized_pnl_today(0),
                         current_drawdown(0), portfolio_volatility(0), var_95_1day(0),
                         concentration_risk(0), fear_greed_index(50) {}
    };

    // Score polish types
    enum class PolishType {
        CONFIDENCE_ADJUSTMENT,   // Adjust confidence scores
        RANKING_REFINEMENT,     // Refine ranking orders
        RISK_WEIGHTED_POLISH,   // Risk-informed score adjustments
        MARKET_CONTEXT_POLISH   // Market condition-based refinements
    };

    // AI score polish result
    struct ScorePolishResult {
        std::chrono::system_clock::time_point polish_time;
        PolishType polish_type;
        AIProvider provider_used;

        // Polish details
        std::string polish_reasoning;
        double adjustment_confidence;   // 0.0 to 1.0 - Claude's confidence in adjustments
        std::string market_analysis;

        // Score adjustments (symbol -> adjustment factor)
        std::map<std::string, double> confidence_adjustments;  // -polish_limit to +polish_limit
        std::map<std::string, double> ranking_adjustments;     // -polish_limit to +polish_limit
        std::map<std::string, int> rank_position_changes;      // position changes in ranking

        // Limits applied
        double polish_limit_used;       // The limit that was applied (e.g., 0.20 for 20%)
        bool adjustments_capped;        // True if any adjustments hit the limit
        std::vector<std::string> capped_symbols;  // Symbols that hit adjustment limits

        ScorePolishResult() : adjustment_confidence(0), polish_limit_used(0.20), adjustments_capped(false) {}
    };

    // Score polish configuration
    struct ScorePolishConfig {
        bool enabled = true;
        double max_adjustment_percent = 0.20;      // Default 20% max adjustment
        bool adjust_confidence = true;             // Allow confidence score adjustments
        bool adjust_rankings = true;               // Allow ranking adjustments
        int max_rank_position_change = 3;          // Max positions a symbol can move up/down

        // === HARD-CODED SAFETY LIMITS (SDM Requirement) ===
        // These limits CANNOT be exceeded regardless of configuration
        static constexpr double ABSOLUTE_MAX_ADJUSTMENT = 0.35;     // 35% absolute ceiling
        static constexpr double ABSOLUTE_MIN_CONFIDENCE = 0.05;     // Confidence floor 5%
        static constexpr double ABSOLUTE_MAX_CONFIDENCE = 0.95;     // Confidence ceiling 95%
        static constexpr int ABSOLUTE_MAX_RANK_CHANGE = 5;          // Max rank position change

        // Sanity check thresholds
        static constexpr double SANITY_CHECK_THRESHOLD = 0.50;      // Flag adjustments >50%
        static constexpr int MAX_SYMBOLS_ADJUSTED_PER_CALL = 20;    // Limit batch adjustments

        // Emergency circuit breakers
        bool enable_emergency_limits = true;       // Emergency safety override
        double circuit_breaker_threshold = 0.30;   // Disable if too many large adjustments
        int max_consecutive_large_adjustments = 3; // Circuit breaker trigger

        // Rate limiting for API calls
        int max_polish_calls_per_hour = 50;
        std::chrono::minutes min_time_between_polish{10};

        ScorePolishConfig() = default;
    };

    // Random Forest prediction input for polishing
    struct RandomForestPrediction {
        std::string symbol;
        double confidence_score;    // Original confidence from random forest
        double ranking_score;      // Original ranking score from random forest
        int original_rank;         // Position in ranking (1 = best)

        // Additional context for AI analysis
        std::map<std::string, double> feature_importances;
        std::string prediction_rationale;

        RandomForestPrediction() : confidence_score(0), ranking_score(0), original_rank(0) {}
    };

    // Polished prediction result
    struct PolishedPrediction {
        std::string symbol;
        double original_confidence;
        double polished_confidence;    // Adjusted confidence score
        double original_ranking;
        double polished_ranking;      // Adjusted ranking score
        int original_rank;
        int polished_rank;           // New position in ranking

        // Adjustment details
        double confidence_adjustment; // The actual adjustment applied (-limit to +limit)
        double ranking_adjustment;   // The actual adjustment applied (-limit to +limit)
        bool was_capped;             // True if adjustment was limited by max_adjustment_percent

        PolishedPrediction() : original_confidence(0), polished_confidence(0),
                              original_ranking(0), polished_ranking(0),
                              original_rank(0), polished_rank(0),
                              confidence_adjustment(0), ranking_adjustment(0), was_capped(false) {}
    };

    // Configuration struct (moved to public)
    struct ScorePolishEngineConfig {
        AIProviderConfig claude_provider;
        ScorePolishConfig polish_config;

        // AI interaction parameters
        double minimum_claude_confidence = 0.6;  // Minimum confidence in Claude's adjustments
        std::chrono::minutes polish_cache_duration{30};  // Cache polish results

        // Rate limiting
        int max_api_calls_per_hour = 50;
        std::chrono::minutes cooldown_period{10};

        ScorePolishEngineConfig() {}
    };

private:
    // Configuration instance
    ScorePolishEngineConfig config_;

    // State management
    std::atomic<bool> is_processing_polish_{false};
    mutable std::mutex polish_mutex_;

    // Polish result caching
    std::map<std::string, ScorePolishResult> polish_cache_;  // Symbol -> last polish result
    std::chrono::system_clock::time_point last_polish_time_;

    // API rate limiting
    std::queue<std::chrono::system_clock::time_point> claude_api_call_history_;
    std::chrono::system_clock::time_point last_claude_api_call_;

    // Polish tracking
    std::vector<ScorePolishResult> polish_history_;

    // Callback function
    std::function<void(const ScorePolishResult&)> polish_callback_;

    // Claude API interaction
    ScorePolishResult callClaudeAPI(const std::vector<RandomForestPrediction>& predictions, const MarketContext& context);

    // Polish processing
    std::string generatePolishPrompt(const std::vector<RandomForestPrediction>& predictions, const MarketContext& context);
    ScorePolishResult parseClaudeResponse(const std::string& response, const std::vector<RandomForestPrediction>& predictions);
    std::vector<PolishedPrediction> applyPolishLimits(const std::vector<PolishedPrediction>& raw_polished);
    std::vector<PolishedPrediction> recomputeRankings(const std::vector<PolishedPrediction>& polished);

    // Utility methods
    bool isWithinRateLimit();
    void updateRateLimitHistory();
    std::string formatPredictionsForClaude(const std::vector<RandomForestPrediction>& predictions);
    std::string formatMarketContextForClaude(const MarketContext& context);

public:
    explicit AIScorePolishEngine(const ScorePolishEngineConfig& config = ScorePolishEngineConfig());
    ~AIScorePolishEngine();

    // Configuration management
    void updateConfig(const ScorePolishEngineConfig& config);
    const ScorePolishEngineConfig& getConfig() const { return config_; }

    // Core score polishing functionality
    ScorePolishResult polishScores(const std::vector<RandomForestPrediction>& rf_predictions,
                                  const MarketContext& context = MarketContext{});

    // Get polished predictions ready for trading system
    std::vector<PolishedPrediction> getPolishedPredictions(const std::vector<RandomForestPrediction>& rf_predictions,
                                                          const MarketContext& context = MarketContext{});

    // Cache management
    ScorePolishResult getCachedPolishResult(const std::string& cache_key) const;
    void clearPolishCache();
    bool hasRecentPolishResult(std::chrono::minutes max_age = std::chrono::minutes{30}) const;

    // Claude API management
    bool testClaudeAPI();
    bool isClaudeAPIAvailable() const;

    // Monitoring and analysis
    struct PolishEngineMetrics {
        std::chrono::system_clock::time_point metrics_time;

        // Claude API usage statistics
        int claude_api_calls_today;
        double average_claude_response_time;
        double claude_success_rate;

        // Polish quality metrics
        double average_claude_confidence;
        int successful_polish_count;
        int failed_polish_count;
        double polish_success_rate;

        // Score adjustment statistics
        int predictions_polished_count;
        double average_confidence_adjustment;
        double average_ranking_adjustment;
        int predictions_improved_count;  // Based on subsequent performance

        PolishEngineMetrics() : claude_api_calls_today(0), average_claude_response_time(0),
                               claude_success_rate(0), average_claude_confidence(0),
                               successful_polish_count(0), failed_polish_count(0),
                               polish_success_rate(0), predictions_polished_count(0),
                               average_confidence_adjustment(0), average_ranking_adjustment(0),
                               predictions_improved_count(0) {}
    };

    PolishEngineMetrics getEngineMetrics() const;

    // Callback registration
    void setPolishCallback(std::function<void(const ScorePolishResult&)> callback);

    // Data export and reporting
    bool exportPolishHistory(const std::string& filename, const std::string& format = "json") const;
    bool exportPolishPerformanceAnalysis(const std::string& filename) const;

    // Testing and validation
    struct PolishEngineTestResult {
        std::chrono::system_clock::time_point test_time;
        bool overall_success;

        bool claude_connectivity_test_passed;
        bool polish_functionality_test_passed;
        bool rate_limiting_test_passed;
        bool adjustment_limits_test_passed;

        std::vector<std::string> test_failures;
        std::string test_summary;

        PolishEngineTestResult() : overall_success(false), claude_connectivity_test_passed(false),
                                  polish_functionality_test_passed(false),
                                  rate_limiting_test_passed(false),
                                  adjustment_limits_test_passed(false) {}
    };

    PolishEngineTestResult runSystemTests();
    bool validatePolishResult(const ScorePolishResult& result) const;

    // Static utility methods
    static std::string polishTypeToString(PolishType polish_type);
    static PolishType stringToPolishType(const std::string& polish_type_str);
    static AIProviderConfig createDefaultClaudeConfig();
};

} // namespace AI
} // namespace CryptoClaude