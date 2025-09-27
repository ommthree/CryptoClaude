#pragma once

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <memory>
#include <atomic>
#include <mutex>
#include <future>
#include "../Data/Models/NewsArticle.h"
#include "../Analytics/TechnicalIndicators.h"

namespace CryptoClaude {
namespace AI {

/**
 * Claude Feature Provider - Generates ML features from Claude API analysis
 * Integrates Claude's qualitative analysis as quantitative features for Random Forest
 */
class ClaudeFeatureProvider {
public:
    // Input data structures for Claude analysis
    struct NewsArticle {
        std::string title;
        std::string content;
        std::string source;
        double sentiment_score;
        std::chrono::system_clock::time_point published_time;
        std::vector<std::string> mentioned_symbols;
        double relevance_score;

        NewsArticle() : sentiment_score(0.0), relevance_score(0.0) {}
    };

    struct TechnicalIndicators {
        double rsi_14;
        double macd_signal;
        double bollinger_position;  // Position within Bollinger bands
        double volume_sma_ratio;
        double price_sma_20_ratio;

        TechnicalIndicators() : rsi_14(50.0), macd_signal(0.0), bollinger_position(0.5),
                               volume_sma_ratio(1.0), price_sma_20_ratio(1.0) {}
    };

    struct MarketRegimeData {
        std::string current_phase;           // "bull_market", "bear_market", "consolidation", "transition"
        double volatility_percentile;       // VIX-equivalent for crypto (0-100)
        double correlation_with_traditional; // Correlation with S&P 500
        std::vector<std::string> major_events; // ["fed_meeting", "regulation_news", "institutional_adoption"]
        double market_cap_flow;             // Net inflow/outflow indicator

        MarketRegimeData() : volatility_percentile(50.0), correlation_with_traditional(0.0),
                            market_cap_flow(0.0) {}
    };

    struct ClaudeAnalysisInput {
        std::chrono::system_clock::time_point analysis_time;

        // Market data for context
        std::map<std::string, double> current_prices;        // Symbol -> current price
        std::map<std::string, double> price_changes_24h;     // Symbol -> 24h % change
        std::map<std::string, double> price_changes_7d;      // Symbol -> 7d % change

        // News and sentiment data (last 24-48h)
        std::vector<NewsArticle> recent_news;
        std::map<std::string, double> social_sentiment;      // Symbol -> social sentiment score

        // Technical analysis context
        std::map<std::string, TechnicalIndicators> technical_data;

        // Market regime and macro data
        MarketRegimeData market_regime;

        // Symbols to analyze
        std::vector<std::string> target_symbols;

        ClaudeAnalysisInput() {
            analysis_time = std::chrono::system_clock::now();
        }
    };

    // Claude's output as quantified features
    struct ClaudeFeatureSet {
        std::chrono::system_clock::time_point feature_time;
        std::string symbol;

        // Sentiment and narrative features
        double market_sentiment_score;       // -1.0 (very bearish) to +1.0 (very bullish)
        double news_impact_magnitude;        // 0.0 to 1.0 - how impactful recent news is
        double social_momentum_score;        // -1.0 to +1.0 - social media momentum
        double narrative_strength;           // 0.0 to 1.0 - strength of bullish/bearish narrative

        // Market regime assessment
        double regime_change_probability;    // 0.0 to 1.0 - likelihood of trend change
        double volatility_forecast;          // 0.0 to 1.0 - expected volatility increase
        double correlation_shift_signal;     // -1.0 to +1.0 - expected correlation change

        // Risk and uncertainty features
        double uncertainty_level;            // 0.0 to 1.0 - market uncertainty
        double tail_risk_indicator;          // 0.0 to 1.0 - probability of extreme moves
        double liquidity_concern_score;      // 0.0 to 1.0 - liquidity risk assessment

        // Timing and momentum features
        double momentum_sustainability;      // 0.0 to 1.0 - can current trend continue
        double mean_reversion_signal;        // 0.0 to 1.0 - likelihood of reversal
        double breakout_probability;         // 0.0 to 1.0 - chance of significant move

        // Cross-asset and macro features
        double institutional_sentiment;      // -1.0 to +1.0 - institutional interest
        double regulatory_risk_score;        // 0.0 to 1.0 - regulatory headwinds
        double adoption_momentum;            // 0.0 to 1.0 - adoption trend strength

        // Confidence and quality metrics
        double analysis_confidence;          // 0.0 to 1.0 - Claude's confidence in analysis
        double data_quality_score;           // 0.0 to 1.0 - quality of input data

        ClaudeFeatureSet() : market_sentiment_score(0.0), news_impact_magnitude(0.0),
                            social_momentum_score(0.0), narrative_strength(0.0),
                            regime_change_probability(0.0), volatility_forecast(0.0),
                            correlation_shift_signal(0.0), uncertainty_level(0.0),
                            tail_risk_indicator(0.0), liquidity_concern_score(0.0),
                            momentum_sustainability(0.0), mean_reversion_signal(0.0),
                            breakout_probability(0.0), institutional_sentiment(0.0),
                            regulatory_risk_score(0.0), adoption_momentum(0.0),
                            analysis_confidence(0.0), data_quality_score(0.0) {
            feature_time = std::chrono::system_clock::now();
        }
    };

    // Configuration for Claude feature generation
    struct ClaudeFeatureConfig {
        bool enabled = true;
        std::string claude_api_key;
        std::string claude_api_url = "https://api.anthropic.com/v1/messages";
        std::string claude_model = "claude-3-sonnet-20240229";

        // Feature generation settings
        std::chrono::minutes feature_cache_duration{15};     // Cache features for 15 minutes
        std::chrono::minutes max_analysis_age{60};           // Max age for analysis input
        int max_news_articles = 20;                          // Max news articles to analyze

        // Rate limiting
        int max_api_calls_per_hour = 30;                     // Conservative rate limiting
        std::chrono::minutes min_time_between_calls{2};      // 2 minute minimum between calls

        // Quality thresholds
        double min_confidence_threshold = 0.6;               // Minimum Claude confidence to use
        double max_uncertainty_threshold = 0.8;              // Max uncertainty to still use

        // === HARD-CODED SAFETY LIMITS FOR FEATURES (SDM Requirement) ===
        // These ensure Claude features stay within valid ranges
        static constexpr double FEATURE_VALUE_MIN = -1.0;    // Minimum feature value
        static constexpr double FEATURE_VALUE_MAX = 1.0;     // Maximum feature value
        static constexpr double PROBABILITY_MIN = 0.0;       // Minimum probability value
        static constexpr double PROBABILITY_MAX = 1.0;       // Maximum probability value

        // Feature validation thresholds
        static constexpr double EXTREME_VALUE_THRESHOLD = 0.95;  // Flag extreme values
        static constexpr int MAX_EXTREME_FEATURES_PER_SYMBOL = 3; // Max extreme features per symbol

        ClaudeFeatureConfig() = default;
    };

    // Result of feature generation
    struct FeatureGenerationResult {
        std::chrono::system_clock::time_point generation_time;
        bool success;
        std::string error_message;

        // Generated features by symbol
        std::map<std::string, ClaudeFeatureSet> features;

        // Generation metadata
        double api_response_time_ms;
        double processing_time_ms;
        int symbols_processed;
        int symbols_failed;

        FeatureGenerationResult() : success(false), api_response_time_ms(0.0),
                                   processing_time_ms(0.0), symbols_processed(0),
                                   symbols_failed(0) {
            generation_time = std::chrono::system_clock::now();
        }
    };

public:
    explicit ClaudeFeatureProvider(const ClaudeFeatureConfig& config = ClaudeFeatureConfig());
    ~ClaudeFeatureProvider();

    // === CORE FEATURE GENERATION ===

    // Generate features for multiple symbols
    FeatureGenerationResult generateFeatures(const ClaudeAnalysisInput& input);

    // Generate features for single symbol (convenience method)
    ClaudeFeatureSet generateSingleSymbolFeatures(const std::string& symbol,
                                                  const ClaudeAnalysisInput& input);

    // === CACHING AND PERFORMANCE ===

    // Get cached features if still valid
    std::map<std::string, ClaudeFeatureSet> getCachedFeatures(
        const std::vector<std::string>& symbols) const;

    // Check if we have recent valid features
    bool hasValidCachedFeatures(const std::string& symbol,
                               std::chrono::minutes max_age = std::chrono::minutes{15}) const;

    // Clear feature cache
    void clearFeatureCache();

    // === CONFIGURATION ===

    void updateConfig(const ClaudeFeatureConfig& config);
    const ClaudeFeatureConfig& getConfig() const { return config_; }

    // === INTEGRATION HELPERS ===

    // Convert ClaudeFeatureSet to Random Forest feature vector addition
    std::map<std::string, double> convertToMLFeatures(const ClaudeFeatureSet& claude_features);

    // Batch convert multiple symbols
    std::map<std::string, std::map<std::string, double>> convertBatchToMLFeatures(
        const std::map<std::string, ClaudeFeatureSet>& claude_features);

    // === MONITORING AND DIAGNOSTICS ===

    struct ProviderMetrics {
        std::chrono::system_clock::time_point metrics_time;

        // API usage stats
        int api_calls_today;
        double average_response_time_ms;
        double api_success_rate;

        // Feature quality stats
        double average_confidence_score;
        double average_data_quality_score;
        int features_generated_today;
        int cache_hits_today;

        // Performance metrics
        double average_processing_time_ms;
        int symbols_processed_today;

        ProviderMetrics() : api_calls_today(0), average_response_time_ms(0.0),
                           api_success_rate(0.0), average_confidence_score(0.0),
                           average_data_quality_score(0.0), features_generated_today(0),
                           cache_hits_today(0), average_processing_time_ms(0.0),
                           symbols_processed_today(0) {
            metrics_time = std::chrono::system_clock::now();
        }
    };

    ProviderMetrics getMetrics() const;

    // Test Claude API connectivity
    bool testClaudeConnectivity();

    // === DATA EXPORT ===

    bool exportFeatureHistory(const std::string& filepath, const std::string& format = "csv") const;
    bool exportMetrics(const std::string& filepath) const;

private:
    // Configuration
    ClaudeFeatureConfig config_;

    // Feature cache
    mutable std::mutex cache_mutex_;
    std::map<std::string, ClaudeFeatureSet> feature_cache_;
    std::map<std::string, std::chrono::system_clock::time_point> cache_timestamps_;

    // API rate limiting
    mutable std::mutex rate_limit_mutex_;
    std::queue<std::chrono::system_clock::time_point> api_call_history_;
    std::chrono::system_clock::time_point last_api_call_;

    // Performance tracking
    mutable std::atomic<int> api_calls_today_;
    mutable std::atomic<int> features_generated_today_;
    mutable std::atomic<int> cache_hits_today_;

    // Claude API interaction
    FeatureGenerationResult callClaudeForFeatures(const ClaudeAnalysisInput& input);
    std::string generateClaudePrompt(const ClaudeAnalysisInput& input);
    std::map<std::string, ClaudeFeatureSet> parseClaudeFeatureResponse(const std::string& response,
                                                                       const std::vector<std::string>& symbols);

    // Rate limiting helpers
    bool canMakeAPICall() const;
    void recordAPICall();

    // Cache management
    void updateFeatureCache(const std::string& symbol, const ClaudeFeatureSet& features);
    bool isFeatureCacheValid(const std::string& symbol, std::chrono::minutes max_age) const;

    // Utility methods
    std::string formatNewsForClaude(const std::vector<NewsArticle>& news) const;
    std::string formatTechnicalDataForClaude(const std::map<std::string, TechnicalIndicators>& tech_data) const;
    std::string formatMarketRegimeForClaude(const MarketRegimeData& regime) const;

    // HTTP client helper
    std::string makeHTTPRequest(const std::string& url, const std::string& headers,
                               const std::string& body, double timeout_seconds = 30.0);
};

}} // namespace CryptoClaude::AI