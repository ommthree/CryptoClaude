/**
 * AI Score Polish Engine Implementation - Day 25
 * Uses Claude API to refine random forest predictions by adjusting scores within limits
 * Week 6 - Advanced AI-Powered Trading Systems
 */

#include "AIDecisionEngine.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <cmath>
#include <cstdlib>
#include <fstream>

using json = nlohmann::json;

namespace CryptoClaude {
namespace AI {

// CURL response callback
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t totalSize = size * nmemb;
    userp->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

AIScorePolishEngine::AIScorePolishEngine(const ScorePolishEngineConfig& config)
    : config_(config), is_processing_polish_(false) {

    // Initialize CURL
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // Set up default Claude configuration if not provided
    if (config_.claude_provider.credentials.api_key.empty()) {
        config_.claude_provider.provider = AIProvider::CLAUDE_API;
        config_.claude_provider.credentials.api_url = "https://api.anthropic.com/v1/messages";
        config_.claude_provider.credentials.model_name = "claude-3-haiku-20240307";
        config_.claude_provider.credentials.timeout_seconds = 30.0;
        config_.claude_provider.credentials.rate_limit_per_minute = 60.0;
        config_.claude_provider.supported_tasks = {"score_polish", "confidence_adjustment", "ranking_refinement"};

        // Try to load API key from environment variable
        const char* api_key = std::getenv("CLAUDE_API_KEY");
        if (api_key != nullptr) {
            config_.claude_provider.credentials.api_key = std::string(api_key);
            config_.claude_provider.is_enabled = true;
            std::cout << "✅ Claude API key loaded (length: " << std::string(api_key).length() << " characters)\n";
        } else {
            std::cout << "⚠️  CLAUDE_API_KEY environment variable not set - Claude integration disabled\n";
            config_.claude_provider.is_enabled = false;
        }
    }

    last_polish_time_ = std::chrono::system_clock::time_point::min();
    last_claude_api_call_ = std::chrono::system_clock::time_point::min();

    std::cout << "✅ AI Score Polish Engine initialized with Claude API integration\n";
}

AIScorePolishEngine::~AIScorePolishEngine() {
    curl_global_cleanup();
}

void AIScorePolishEngine::updateConfig(const ScorePolishEngineConfig& config) {
    std::lock_guard<std::mutex> lock(polish_mutex_);
    config_ = config;
}

AIScorePolishEngine::ScorePolishResult AIScorePolishEngine::polishScores(const std::vector<AIScorePolishEngine::RandomForestPrediction>& rf_predictions,
                                                   const AIScorePolishEngine::MarketContext& context) {

    std::cout << "🔍 Starting polishScores for " << rf_predictions.size() << " predictions\n";

    // Check if we should skip due to rate limiting
    if (!isWithinRateLimit()) {
        std::cout << "⚠️  Skipping polish due to rate limiting\n";
        ScorePolishResult result;
        result.polish_time = std::chrono::system_clock::now();
        result.polish_type = PolishType::CONFIDENCE_ADJUSTMENT;
        result.provider_used = AIProvider::CLAUDE_API;
        result.polish_reasoning = "Skipped due to rate limiting";
        result.adjustment_confidence = 0.0;
        return result;
    }

    // Check if polish is disabled
    if (!config_.polish_config.enabled) {
        std::cout << "⚠️  Polish is disabled in configuration\n";
        ScorePolishResult result;
        result.polish_time = std::chrono::system_clock::now();
        result.polish_type = PolishType::CONFIDENCE_ADJUSTMENT;
        result.provider_used = AIProvider::CLAUDE_API;
        result.polish_reasoning = "Score polish is disabled in configuration";
        result.adjustment_confidence = 0.0;
        return result;
    }

    // Check minimum time between polish calls
    auto now = std::chrono::system_clock::now();

    // Handle first call (when last_polish_time_ is min())
    if (last_polish_time_ == std::chrono::system_clock::time_point::min()) {
        std::cout << "🆕 First polish call - proceeding\n";
    } else {
        auto time_since_last = now - last_polish_time_;
        std::cout << "⏰ Time since last polish: " << std::chrono::duration_cast<std::chrono::seconds>(time_since_last).count() << " seconds\n";
        std::cout << "⏰ Min time required: " << std::chrono::duration_cast<std::chrono::seconds>(config_.polish_config.min_time_between_polish).count() << " seconds\n";
        if (time_since_last < config_.polish_config.min_time_between_polish) {
            std::cout << "⚠️  Too soon since last polish call\n";
            ScorePolishResult result;
            result.polish_time = now;
            result.polish_type = PolishType::CONFIDENCE_ADJUSTMENT;
            result.provider_used = AIProvider::CLAUDE_API;
            result.polish_reasoning = "Too soon since last polish call";
            result.adjustment_confidence = 0.0;
            return result;
        }
    }

    std::lock_guard<std::mutex> lock(polish_mutex_);
    is_processing_polish_ = true;

    std::cout << "🚀 Proceeding with Claude API call...\n";

    ScorePolishResult result;
    try {
        result = callClaudeAPI(rf_predictions, context);
        last_polish_time_ = std::chrono::system_clock::now();

        // Store in cache and history
        std::string cache_key = "latest_polish_" + std::to_string(rf_predictions.size()) + "_symbols";
        polish_cache_[cache_key] = result;
        polish_history_.push_back(result);

        // Limit history size to last 100 entries
        if (polish_history_.size() > 100) {
            polish_history_.erase(polish_history_.begin());
        }

        // Call callback if set
        if (polish_callback_) {
            polish_callback_(result);
        }

    } catch (const std::exception& e) {
        result.polish_time = std::chrono::system_clock::now();
        result.polish_type = PolishType::CONFIDENCE_ADJUSTMENT;
        result.provider_used = AIProvider::CLAUDE_API;
        result.polish_reasoning = "Error during Claude API call: " + std::string(e.what());
        result.adjustment_confidence = 0.0;
    }

    is_processing_polish_ = false;
    return result;
}

std::vector<AIScorePolishEngine::PolishedPrediction> AIScorePolishEngine::getPolishedPredictions(const std::vector<AIScorePolishEngine::RandomForestPrediction>& rf_predictions,
                                                                           const AIScorePolishEngine::MarketContext& context) {

    ScorePolishResult polish_result = polishScores(rf_predictions, context);

    std::vector<PolishedPrediction> polished_predictions;

    // Create polished predictions from the result
    for (const auto& rf_pred : rf_predictions) {
        PolishedPrediction polished;
        polished.symbol = rf_pred.symbol;
        polished.original_confidence = rf_pred.confidence_score;
        polished.original_ranking = rf_pred.ranking_score;
        polished.original_rank = rf_pred.original_rank;

        // Apply adjustments from Claude if available
        auto conf_it = polish_result.confidence_adjustments.find(rf_pred.symbol);
        auto rank_it = polish_result.ranking_adjustments.find(rf_pred.symbol);

        if (conf_it != polish_result.confidence_adjustments.end()) {
            polished.confidence_adjustment = conf_it->second;
            polished.polished_confidence = rf_pred.confidence_score + conf_it->second;
            // Clamp to valid range [0, 1]
            polished.polished_confidence = std::max(0.0, std::min(1.0, polished.polished_confidence));
        } else {
            polished.confidence_adjustment = 0.0;
            polished.polished_confidence = rf_pred.confidence_score;
        }

        if (rank_it != polish_result.ranking_adjustments.end()) {
            polished.ranking_adjustment = rank_it->second;
            polished.polished_ranking = rf_pred.ranking_score + rank_it->second;
            // Clamp to reasonable range
            polished.polished_ranking = std::max(0.0, polished.polished_ranking);
        } else {
            polished.ranking_adjustment = 0.0;
            polished.polished_ranking = rf_pred.ranking_score;
        }

        // Check if adjustment was capped
        double max_adj = config_.polish_config.max_adjustment_percent;
        polished.was_capped = (std::abs(polished.confidence_adjustment) >= max_adj) ||
                              (std::abs(polished.ranking_adjustment) >= max_adj);

        polished_predictions.push_back(polished);
    }

    // Recompute rankings based on polished scores
    polished_predictions = recomputeRankings(polished_predictions);

    return polished_predictions;
}

AIScorePolishEngine::ScorePolishResult AIScorePolishEngine::callClaudeAPI(const std::vector<AIScorePolishEngine::RandomForestPrediction>& predictions,
                                                     const AIScorePolishEngine::MarketContext& context) {

    CURL* curl = curl_easy_init();
    ScorePolishResult result;
    result.polish_time = std::chrono::system_clock::now();
    result.polish_type = PolishType::CONFIDENCE_ADJUSTMENT;
    result.provider_used = AIProvider::CLAUDE_API;

    if (!curl) {
        result.polish_reasoning = "Failed to initialize CURL";
        result.adjustment_confidence = 0.0;
        return result;
    }

    try {
        // Generate prompt for Claude
        std::string prompt = generatePolishPrompt(predictions, context);

        // Prepare Claude API request
        json claude_request = {
            {"model", config_.claude_provider.credentials.model_name},
            {"max_tokens", 1000},
            {"messages", json::array({
                {
                    {"role", "user"},
                    {"content", prompt}
                }
            })}
        };

        std::string request_body = claude_request.dump();
        std::string response_body;

        // Set CURL options
        struct curl_slist* headers = nullptr;
        std::string auth_header = "x-api-key: " + config_.claude_provider.credentials.api_key;
        std::string content_header = "Content-Type: application/json";
        std::string anthropic_version = "anthropic-version: 2023-06-01";

        headers = curl_slist_append(headers, auth_header.c_str());
        headers = curl_slist_append(headers, content_header.c_str());
        headers = curl_slist_append(headers, anthropic_version.c_str());

        curl_easy_setopt(curl, CURLOPT_URL, config_.claude_provider.credentials.api_url.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(config_.claude_provider.credentials.timeout_seconds));
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "CryptoClaude/1.0");

        // Make the API call
        CURLcode res = curl_easy_perform(curl);
        long response_code;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

        // Cleanup
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        // Update rate limiting
        updateRateLimitHistory();

        if (res != CURLE_OK) {
            result.polish_reasoning = "CURL error: " + std::string(curl_easy_strerror(res));
            result.adjustment_confidence = 0.0;
            return result;
        }

        if (response_code != 200) {
            result.polish_reasoning = "Claude API error: HTTP " + std::to_string(response_code) + " - " + response_body;
            result.adjustment_confidence = 0.0;
            std::cout << "❌ Claude API HTTP Error: " << response_code << std::endl;
            std::cout << "Response body: " << response_body << std::endl;
            return result;
        }

        // Parse Claude response and create result
        std::cout << "📝 Claude Raw Response: " << response_body << std::endl;
        result = parseClaudeResponse(response_body, predictions);

    } catch (const std::exception& e) {
        curl_easy_cleanup(curl);
        result.polish_reasoning = "Exception during Claude API call: " + std::string(e.what());
        result.adjustment_confidence = 0.0;
    }

    return result;
}

std::string AIScorePolishEngine::generatePolishPrompt(const std::vector<AIScorePolishEngine::RandomForestPrediction>& predictions,
                                                     const AIScorePolishEngine::MarketContext& context) {

    std::ostringstream prompt;

    prompt << "You are an expert cryptocurrency trading analyst. I have predictions from a Random Forest model "
           << "for " << predictions.size() << " cryptocurrencies. Your task is to refine these predictions "
           << "by adjusting confidence and ranking scores within strict limits.\n\n";

    prompt << "IMPORTANT CONSTRAINTS:\n";
    prompt << "- You may adjust confidence scores by at most ±"
           << (config_.polish_config.max_adjustment_percent * 100) << "%\n";
    prompt << "- You may adjust ranking scores by at most ±"
           << (config_.polish_config.max_adjustment_percent * 100) << "%\n";
    prompt << "- Confidence scores must stay between 0.0 and 1.0\n";
    prompt << "- Provide your confidence in your adjustments (0.0 to 1.0)\n\n";

    // Add market context
    prompt << formatMarketContextForClaude(context);

    // Add predictions
    prompt << formatPredictionsForClaude(predictions);

    prompt << "\nPlease analyze these predictions and provide:\n";
    prompt << "1. Your overall assessment of market conditions\n";
    prompt << "2. For each symbol, suggest confidence and ranking adjustments (within limits)\n";
    prompt << "3. Your confidence level in these adjustments\n\n";

    prompt << "Respond in JSON format:\n";
    prompt << "{\n";
    prompt << "  \"market_analysis\": \"your market analysis\",\n";
    prompt << "  \"adjustment_confidence\": 0.85,\n";
    prompt << "  \"adjustments\": {\n";
    prompt << "    \"SYMBOL\": {\n";
    prompt << "      \"confidence_adj\": 0.05,\n";
    prompt << "      \"ranking_adj\": -0.02,\n";
    prompt << "      \"reasoning\": \"brief reasoning\"\n";
    prompt << "    }\n";
    prompt << "  }\n";
    prompt << "}";

    return prompt.str();
}

AIScorePolishEngine::ScorePolishResult AIScorePolishEngine::parseClaudeResponse(const std::string& response,
                                                          const std::vector<AIScorePolishEngine::RandomForestPrediction>& predictions) {

    ScorePolishResult result;
    result.polish_time = std::chrono::system_clock::now();
    result.polish_type = PolishType::CONFIDENCE_ADJUSTMENT;
    result.provider_used = AIProvider::CLAUDE_API;
    result.polish_limit_used = config_.polish_config.max_adjustment_percent;

    try {
        // Parse Claude's response
        json claude_response = json::parse(response);

        // Extract Claude's response content
        if (claude_response.contains("content") && claude_response["content"].is_array() &&
            !claude_response["content"].empty()) {

            std::string content = claude_response["content"][0]["text"];

            // Find JSON in the content
            size_t json_start = content.find('{');
            size_t json_end = content.rfind('}');

            if (json_start != std::string::npos && json_end != std::string::npos && json_end > json_start) {
                std::string json_str = content.substr(json_start, json_end - json_start + 1);
                json analysis = json::parse(json_str);

                // Extract market analysis
                if (analysis.contains("market_analysis")) {
                    result.market_analysis = analysis["market_analysis"];
                }

                // Extract adjustment confidence
                if (analysis.contains("adjustment_confidence")) {
                    result.adjustment_confidence = analysis["adjustment_confidence"];
                }

                // Extract adjustments
                if (analysis.contains("adjustments") && analysis["adjustments"].is_object()) {
                    for (const auto& [symbol, adj_data] : analysis["adjustments"].items()) {

                        // Apply confidence adjustment with HARD-CODED SAFETY LIMITS
                        if (adj_data.contains("confidence_adj")) {
                            double conf_adj = adj_data["confidence_adj"];

                            // FIRST: Apply configuration limits
                            conf_adj = std::max(-config_.polish_config.max_adjustment_percent,
                                              std::min(config_.polish_config.max_adjustment_percent, conf_adj));

                            // SECOND: Apply ABSOLUTE HARD-CODED SAFETY LIMITS (SDM Requirement)
                            conf_adj = std::max(-ScorePolishConfig::ABSOLUTE_MAX_ADJUSTMENT,
                                              std::min(ScorePolishConfig::ABSOLUTE_MAX_ADJUSTMENT, conf_adj));

                            // THIRD: Sanity check - flag suspicious adjustments
                            if (std::abs(conf_adj) > ScorePolishConfig::SANITY_CHECK_THRESHOLD) {
                                std::cout << "⚠️  SANITY CHECK: Large confidence adjustment for " << symbol
                                         << ": " << (conf_adj * 100) << "%" << std::endl;
                                // Still allow it but log the warning
                            }

                            result.confidence_adjustments[symbol] = conf_adj;

                            // Track if we capped the adjustment
                            if (std::abs(conf_adj) >= ScorePolishConfig::ABSOLUTE_MAX_ADJUSTMENT * 0.95) {
                                result.adjustments_capped = true;
                                result.capped_symbols.push_back(symbol);
                            }
                        }

                        // Apply ranking adjustment with HARD-CODED SAFETY LIMITS
                        if (adj_data.contains("ranking_adj")) {
                            double rank_adj = adj_data["ranking_adj"];

                            // FIRST: Apply configuration limits
                            rank_adj = std::max(-config_.polish_config.max_adjustment_percent,
                                              std::min(config_.polish_config.max_adjustment_percent, rank_adj));

                            // SECOND: Apply ABSOLUTE HARD-CODED SAFETY LIMITS (SDM Requirement)
                            rank_adj = std::max(-ScorePolishConfig::ABSOLUTE_MAX_ADJUSTMENT,
                                              std::min(ScorePolishConfig::ABSOLUTE_MAX_ADJUSTMENT, rank_adj));

                            // THIRD: Sanity check - flag suspicious adjustments
                            if (std::abs(rank_adj) > ScorePolishConfig::SANITY_CHECK_THRESHOLD) {
                                std::cout << "⚠️  SANITY CHECK: Large ranking adjustment for " << symbol
                                         << ": " << (rank_adj * 100) << "%" << std::endl;
                            }

                            result.ranking_adjustments[symbol] = rank_adj;

                            // Track if we capped the adjustment
                            if (std::abs(rank_adj) >= ScorePolishConfig::ABSOLUTE_MAX_ADJUSTMENT * 0.95) {
                                result.adjustments_capped = true;
                                result.capped_symbols.push_back(symbol);
                            }
                        }
                    }
                }

                result.polish_reasoning = "Successfully parsed Claude adjustments";

            } else {
                result.polish_reasoning = "Could not find valid JSON in Claude response";
                result.adjustment_confidence = 0.0;
            }

        } else {
            result.polish_reasoning = "Invalid Claude response format";
            result.adjustment_confidence = 0.0;
        }

    } catch (const std::exception& e) {
        result.polish_reasoning = "Error parsing Claude response: " + std::string(e.what());
        result.adjustment_confidence = 0.0;
    }

    return result;
}

std::vector<AIScorePolishEngine::PolishedPrediction> AIScorePolishEngine::recomputeRankings(const std::vector<AIScorePolishEngine::PolishedPrediction>& polished) {

    std::vector<PolishedPrediction> result = polished;

    // Sort by polished ranking score (descending - higher is better)
    std::sort(result.begin(), result.end(), [](const PolishedPrediction& a, const PolishedPrediction& b) {
        return a.polished_ranking > b.polished_ranking;
    });

    // Assign new ranks
    for (size_t i = 0; i < result.size(); ++i) {
        result[i].polished_rank = static_cast<int>(i + 1);
    }

    return result;
}

bool AIScorePolishEngine::isWithinRateLimit() {
    auto now = std::chrono::system_clock::now();

    // Clean old entries (older than 1 hour)
    while (!claude_api_call_history_.empty()) {
        auto oldest = claude_api_call_history_.front();
        if (now - oldest > std::chrono::hours(1)) {
            claude_api_call_history_.pop();
        } else {
            break;
        }
    }

    // Check if we're within rate limit
    return claude_api_call_history_.size() < static_cast<size_t>(config_.max_api_calls_per_hour);
}

void AIScorePolishEngine::updateRateLimitHistory() {
    auto now = std::chrono::system_clock::now();
    claude_api_call_history_.push(now);
    last_claude_api_call_ = now;
}

std::string AIScorePolishEngine::formatPredictionsForClaude(const std::vector<AIScorePolishEngine::RandomForestPrediction>& predictions) {
    std::ostringstream ss;

    ss << "RANDOM FOREST PREDICTIONS:\n";
    ss << "Rank | Symbol | Confidence | Ranking Score | Rationale\n";
    ss << "-----|--------|------------|---------------|----------\n";

    for (const auto& pred : predictions) {
        ss << std::setw(4) << pred.original_rank << " | "
           << std::setw(6) << pred.symbol << " | "
           << std::fixed << std::setprecision(3) << std::setw(10) << pred.confidence_score << " | "
           << std::fixed << std::setprecision(3) << std::setw(13) << pred.ranking_score << " | "
           << pred.prediction_rationale << "\n";
    }

    return ss.str();
}

std::string AIScorePolishEngine::formatMarketContextForClaude(const AIScorePolishEngine::MarketContext& context) {
    std::ostringstream ss;

    ss << "MARKET CONTEXT:\n";
    ss << "Portfolio Value: $" << std::fixed << std::setprecision(2) << context.total_portfolio_value << "\n";
    ss << "Unrealized P&L: $" << std::fixed << std::setprecision(2) << context.unrealized_pnl << "\n";
    ss << "Market Trend: " << context.market_trend << "\n";
    ss << "Volatility Regime: " << context.volatility_regime << "\n";
    ss << "Fear/Greed Index: " << context.fear_greed_index << "\n";

    if (!context.current_prices.empty()) {
        ss << "\nCurrent Prices:\n";
        for (const auto& [symbol, price] : context.current_prices) {
            ss << symbol << ": $" << std::fixed << std::setprecision(4) << price << "\n";
        }
    }

    ss << "\n";
    return ss.str();
}

bool AIScorePolishEngine::testClaudeAPI() {
    // Create a minimal test prediction
    std::vector<RandomForestPrediction> test_predictions;
    RandomForestPrediction test_pred;
    test_pred.symbol = "BTC";
    test_pred.confidence_score = 0.75;
    test_pred.ranking_score = 0.85;
    test_pred.original_rank = 1;
    test_pred.prediction_rationale = "Test prediction for API connectivity";
    test_predictions.push_back(test_pred);

    MarketContext test_context;
    test_context.market_trend = "testing";
    test_context.volatility_regime = "normal";
    test_context.fear_greed_index = 50.0;

    try {
        ScorePolishResult result = callClaudeAPI(test_predictions, test_context);
        std::cout << "Claude API test result - Confidence: " << result.adjustment_confidence << std::endl;
        std::cout << "Claude API test reasoning: " << result.polish_reasoning << std::endl;
        return result.adjustment_confidence > 0.0;
    } catch (const std::exception& e) {
        std::cout << "Claude API test exception: " << e.what() << std::endl;
        return false;
    } catch (...) {
        std::cout << "Claude API test unknown exception" << std::endl;
        return false;
    }
}

AIScorePolishEngine::PolishEngineMetrics AIScorePolishEngine::getEngineMetrics() const {
    PolishEngineMetrics metrics;
    metrics.metrics_time = std::chrono::system_clock::now();

    // Calculate metrics from history
    if (!polish_history_.empty()) {
        int successful_count = 0;
        double total_confidence = 0.0;
        double total_conf_adj = 0.0;
        double total_rank_adj = 0.0;
        int total_predictions = 0;

        for (const auto& result : polish_history_) {
            if (result.adjustment_confidence > 0.0) {
                successful_count++;
                total_confidence += result.adjustment_confidence;

                for (const auto& [symbol, adj] : result.confidence_adjustments) {
                    total_conf_adj += std::abs(adj);
                    total_predictions++;
                }

                for (const auto& [symbol, adj] : result.ranking_adjustments) {
                    total_rank_adj += std::abs(adj);
                }
            }
        }

        metrics.successful_polish_count = successful_count;
        metrics.failed_polish_count = polish_history_.size() - successful_count;
        metrics.polish_success_rate = polish_history_.size() > 0 ?
            static_cast<double>(successful_count) / polish_history_.size() : 0.0;

        metrics.average_claude_confidence = successful_count > 0 ?
            total_confidence / successful_count : 0.0;

        metrics.predictions_polished_count = total_predictions;
        metrics.average_confidence_adjustment = total_predictions > 0 ?
            total_conf_adj / total_predictions : 0.0;
        metrics.average_ranking_adjustment = total_predictions > 0 ?
            total_rank_adj / total_predictions : 0.0;
    }

    // API usage stats
    metrics.claude_api_calls_today = claude_api_call_history_.size();
    metrics.claude_success_rate = metrics.polish_success_rate; // Approximation

    return metrics;
}

// Static utility methods
std::string AIScorePolishEngine::polishTypeToString(PolishType polish_type) {
    switch (polish_type) {
        case PolishType::CONFIDENCE_ADJUSTMENT: return "CONFIDENCE_ADJUSTMENT";
        case PolishType::RANKING_REFINEMENT: return "RANKING_REFINEMENT";
        case PolishType::RISK_WEIGHTED_POLISH: return "RISK_WEIGHTED_POLISH";
        case PolishType::MARKET_CONTEXT_POLISH: return "MARKET_CONTEXT_POLISH";
        default: return "UNKNOWN";
    }
}

AIScorePolishEngine::PolishType AIScorePolishEngine::stringToPolishType(const std::string& polish_type_str) {
    if (polish_type_str == "CONFIDENCE_ADJUSTMENT") return PolishType::CONFIDENCE_ADJUSTMENT;
    if (polish_type_str == "RANKING_REFINEMENT") return PolishType::RANKING_REFINEMENT;
    if (polish_type_str == "RISK_WEIGHTED_POLISH") return PolishType::RISK_WEIGHTED_POLISH;
    if (polish_type_str == "MARKET_CONTEXT_POLISH") return PolishType::MARKET_CONTEXT_POLISH;
    return PolishType::CONFIDENCE_ADJUSTMENT; // Default
}

AIScorePolishEngine::AIProviderConfig AIScorePolishEngine::createDefaultClaudeConfig() {
    AIProviderConfig config;
    config.provider = AIProvider::CLAUDE_API;
    config.credentials.api_url = "https://api.anthropic.com/v1/messages";
    config.credentials.model_name = "claude-3-haiku-20240307";
    config.credentials.timeout_seconds = 30.0;
    config.credentials.max_retries = 3;
    config.credentials.rate_limit_per_minute = 60.0;
    config.is_enabled = true;
    config.priority = 1;
    config.supported_tasks = {"score_polish", "confidence_adjustment", "ranking_refinement"};
    return config;
}

// Additional method implementations
AIScorePolishEngine::ScorePolishResult AIScorePolishEngine::getCachedPolishResult(const std::string& cache_key) const {
    auto it = polish_cache_.find(cache_key);
    if (it != polish_cache_.end()) {
        return it->second;
    }
    return ScorePolishResult{}; // Return empty result if not found
}

void AIScorePolishEngine::clearPolishCache() {
    std::lock_guard<std::mutex> lock(polish_mutex_);
    polish_cache_.clear();
}

bool AIScorePolishEngine::hasRecentPolishResult(std::chrono::minutes max_age) const {
    auto now = std::chrono::system_clock::now();
    auto age = now - last_polish_time_;
    return age < max_age;
}

bool AIScorePolishEngine::isClaudeAPIAvailable() const {
    return !config_.claude_provider.credentials.api_key.empty() &&
           config_.claude_provider.is_enabled;
}

void AIScorePolishEngine::setPolishCallback(std::function<void(const ScorePolishResult&)> callback) {
    std::lock_guard<std::mutex> lock(polish_mutex_);
    polish_callback_ = callback;
}

bool AIScorePolishEngine::exportPolishHistory(const std::string& filename, const std::string& format) const {
    // Basic JSON export implementation
    try {
        json export_data = json::array();

        for (const auto& result : polish_history_) {
            json result_json = {
                {"polish_time", std::chrono::duration_cast<std::chrono::seconds>(
                    result.polish_time.time_since_epoch()).count()},
                {"polish_type", polishTypeToString(result.polish_type)},
                {"polish_reasoning", result.polish_reasoning},
                {"adjustment_confidence", result.adjustment_confidence},
                {"confidence_adjustments", result.confidence_adjustments},
                {"ranking_adjustments", result.ranking_adjustments}
            };
            export_data.push_back(result_json);
        }

        std::ofstream file(filename);
        file << export_data.dump(2);
        return true;

    } catch (...) {
        return false;
    }
}

bool AIScorePolishEngine::exportPolishPerformanceAnalysis(const std::string& filename) const {
    try {
        PolishEngineMetrics metrics = getEngineMetrics();

        json analysis = {
            {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                metrics.metrics_time.time_since_epoch()).count()},
            {"claude_api_calls_today", metrics.claude_api_calls_today},
            {"claude_success_rate", metrics.claude_success_rate},
            {"average_claude_confidence", metrics.average_claude_confidence},
            {"successful_polish_count", metrics.successful_polish_count},
            {"failed_polish_count", metrics.failed_polish_count},
            {"polish_success_rate", metrics.polish_success_rate},
            {"predictions_polished_count", metrics.predictions_polished_count},
            {"average_confidence_adjustment", metrics.average_confidence_adjustment},
            {"average_ranking_adjustment", metrics.average_ranking_adjustment}
        };

        std::ofstream file(filename);
        file << analysis.dump(2);
        return true;

    } catch (...) {
        return false;
    }
}

AIScorePolishEngine::PolishEngineTestResult AIScorePolishEngine::runSystemTests() {
    PolishEngineTestResult test_result;
    test_result.test_time = std::chrono::system_clock::now();

    // Test Claude connectivity
    test_result.claude_connectivity_test_passed = testClaudeAPI();
    if (!test_result.claude_connectivity_test_passed) {
        test_result.test_failures.push_back("Claude API connectivity test failed");
    }

    // Test rate limiting
    test_result.rate_limiting_test_passed = isWithinRateLimit();
    if (!test_result.rate_limiting_test_passed) {
        test_result.test_failures.push_back("Rate limiting test failed");
    }

    // Test adjustment limits (simple validation)
    test_result.adjustment_limits_test_passed =
        (config_.polish_config.max_adjustment_percent > 0.0 &&
         config_.polish_config.max_adjustment_percent <= 1.0);
    if (!test_result.adjustment_limits_test_passed) {
        test_result.test_failures.push_back("Invalid adjustment limits configuration");
    }

    // Test basic functionality
    test_result.polish_functionality_test_passed =
        config_.polish_config.enabled && isClaudeAPIAvailable();
    if (!test_result.polish_functionality_test_passed) {
        test_result.test_failures.push_back("Polish functionality test failed");
    }

    // Overall success
    test_result.overall_success = test_result.claude_connectivity_test_passed &&
                                  test_result.rate_limiting_test_passed &&
                                  test_result.adjustment_limits_test_passed &&
                                  test_result.polish_functionality_test_passed;

    // Generate summary
    if (test_result.overall_success) {
        test_result.test_summary = "All tests passed successfully";
    } else {
        test_result.test_summary = "Some tests failed: " +
            std::to_string(test_result.test_failures.size()) + " failures";
    }

    return test_result;
}

bool AIScorePolishEngine::validatePolishResult(const ScorePolishResult& result) const {
    // Validate that adjustments are within limits
    for (const auto& [symbol, adj] : result.confidence_adjustments) {
        if (std::abs(adj) > config_.polish_config.max_adjustment_percent) {
            return false;
        }
    }

    for (const auto& [symbol, adj] : result.ranking_adjustments) {
        if (std::abs(adj) > config_.polish_config.max_adjustment_percent) {
            return false;
        }
    }

    // Validate confidence score
    if (result.adjustment_confidence < 0.0 || result.adjustment_confidence > 1.0) {
        return false;
    }

    return true;
}

} // namespace AI
} // namespace CryptoClaude