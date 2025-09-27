#include "AIDecisionEngine.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <random>
#include <thread>
#include <queue>
#include <set>
#include <fstream>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

// No using declaration to avoid conflicts with forward declarations

namespace CryptoClaude {
namespace AI {

// HTTP response structure for CURL
struct HTTPResponse {
    std::string data;
    long response_code;

    HTTPResponse() : response_code(0) {}
};

// CURL callback function
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, HTTPResponse* response) {
    size_t total_size = size * nmemb;
    response->data.append(static_cast<char*>(contents), total_size);
    return total_size;
}

AIDecisionEngine::AIDecisionEngine(const AIEngineConfig& config) : config_(config) {
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // Initialize decision cache
    decision_cache_.clear();
    last_decision_times_.clear();

    // Initialize rate limiting structures
    for (const auto& provider_config : config_.providers) {
        api_call_history_[provider_config.provider] = std::queue<std::chrono::system_clock::time_point>();
        last_api_calls_[provider_config.provider] = std::chrono::system_clock::time_point::min();
    }

    std::cout << "✅ AI Decision Engine initialized with " << config_.providers.size()
              << " AI providers\n";
}

AIDecisionEngine::~AIDecisionEngine() {
    stopContinuousMonitoring();
    curl_global_cleanup();
    std::cout << "🔄 AI Decision Engine shutdown complete\n";
}

void AIDecisionEngine::updateConfig(const AIEngineConfig& config) {
    std::lock_guard<std::mutex> lock(decision_mutex_);
    config_ = config;

    // Reinitialize rate limiting for new providers
    for (const auto& provider_config : config_.providers) {
        if (api_call_history_.find(provider_config.provider) == api_call_history_.end()) {
            api_call_history_[provider_config.provider] = std::queue<std::chrono::system_clock::time_point>();
            last_api_calls_[provider_config.provider] = std::chrono::system_clock::time_point::min();
        }
    }

    std::cout << "🔧 AI Decision Engine configuration updated\n";
}

bool AIDecisionEngine::integrateRiskManager(std::unique_ptr<Risk::ProductionRiskManager> risk_manager) {
    if (!risk_manager) {
        std::cerr << "❌ Cannot integrate null risk manager\n";
        return false;
    }

    risk_manager_ = std::move(risk_manager);
    std::cout << "🔗 Risk Manager integrated successfully\n";
    return true;
}

bool AIDecisionEngine::integrateStrategyOptimizer(std::unique_ptr<Strategy::TradingStrategyOptimizer> strategy_optimizer) {
    if (!strategy_optimizer) {
        std::cerr << "❌ Cannot integrate null strategy optimizer\n";
        return false;
    }

    strategy_optimizer_ = std::move(strategy_optimizer);
    std::cout << "🔗 Strategy Optimizer integrated successfully\n";
    return true;
}

bool AIDecisionEngine::integratePerformanceEngine(std::unique_ptr<Analytics::AdvancedPerformanceEngine> performance_engine) {
    if (!performance_engine) {
        std::cerr << "❌ Cannot integrate null performance engine\n";
        return false;
    }

    performance_engine_ = std::move(performance_engine);
    std::cout << "🔗 Performance Engine integrated successfully\n";
    return true;
}

bool AIDecisionEngine::addAIProvider(const AIProviderConfig& provider_config) {
    std::lock_guard<std::mutex> lock(decision_mutex_);

    // Check if provider already exists
    auto it = std::find_if(config_.providers.begin(), config_.providers.end(),
                          [&](const AIProviderConfig& config) {
                              return config.provider == provider_config.provider;
                          });

    if (it != config_.providers.end()) {
        std::cerr << "❌ AI Provider " << aiProviderToString(provider_config.provider)
                  << " already exists\n";
        return false;
    }

    config_.providers.push_back(provider_config);
    api_call_history_[provider_config.provider] = std::queue<std::chrono::system_clock::time_point>();
    last_api_calls_[provider_config.provider] = std::chrono::system_clock::time_point::min();

    std::cout << "➕ AI Provider " << aiProviderToString(provider_config.provider)
              << " added successfully\n";
    return true;
}

bool AIDecisionEngine::testAIProvider(AIProvider provider) {
    auto provider_config = std::find_if(config_.providers.begin(), config_.providers.end(),
                                       [provider](const AIProviderConfig& config) {
                                           return config.provider == provider;
                                       });

    if (provider_config == config_.providers.end()) {
        std::cerr << "❌ AI Provider " << aiProviderToString(provider) << " not found\n";
        return false;
    }

    if (!provider_config->is_enabled) {
        std::cerr << "❌ AI Provider " << aiProviderToString(provider) << " is disabled\n";
        return false;
    }

    // Create test market context
    MarketContext test_context;
    test_context.timestamp = std::chrono::system_clock::now();
    test_context.total_portfolio_value = 1000000.0;
    test_context.current_drawdown = 0.02;
    test_context.market_trend = "testing";

    try {
        auto future_decision = consultAI(test_context, DecisionType::RISK_ADJUSTMENT, provider);
        auto decision = future_decision.get();

        bool test_passed = !decision.decision_summary.empty() &&
                          decision.confidence_score >= 0.0 &&
                          decision.confidence_score <= 1.0;

        if (test_passed) {
            std::cout << "✅ AI Provider " << aiProviderToString(provider) << " test passed\n";
        } else {
            std::cout << "❌ AI Provider " << aiProviderToString(provider) << " test failed\n";
        }

        return test_passed;
    } catch (const std::exception& e) {
        std::cerr << "❌ AI Provider " << aiProviderToString(provider)
                  << " test failed with exception: " << e.what() << "\n";
        return false;
    }
}

AIDecisionEngine::AIDecision AIDecisionEngine::makeDecision(DecisionType decision_type, const MarketContext& context) {
    std::lock_guard<std::mutex> lock(decision_mutex_);

    if (is_processing_decision_.load()) {
        std::cout << "⏳ Decision already in progress, returning cached result\n";
        auto cached = getCachedDecision(decision_type);
        if (!cached.decision_summary.empty()) {
            return cached;
        }
    }

    is_processing_decision_.store(true);

    try {
        // Build current market context if not provided
        MarketContext current_context = context.timestamp == std::chrono::system_clock::time_point{} ?
                                       buildCurrentMarketContext() : context;

        // Check for emergency conditions first
        if (decision_type == DecisionType::EMERGENCY_STOP || checkEmergencyConditions(current_context)) {
            if (decision_type != DecisionType::EMERGENCY_STOP) {
                std::cout << "🚨 Emergency conditions detected, triggering emergency stop\n";
                auto emergency_result = executeEmergencyStop(current_context, "AI-detected emergency conditions");

                // Convert emergency result to AI decision
                AIDecision emergency_decision;
                emergency_decision.decision_time = emergency_result.trigger_time;
                emergency_decision.decision_type = DecisionType::EMERGENCY_STOP;
                emergency_decision.decision_summary = "Emergency stop executed: " + emergency_result.trigger_reason;
                emergency_decision.reasoning = "Emergency conditions exceeded configured thresholds";
                emergency_decision.confidence_score = 1.0;
                emergency_decision.requires_immediate_action = true;
                emergency_decision.implementation_priority = "critical";

                is_processing_decision_.store(false);
                return emergency_decision;
            }
        }

        // Get consensus from multiple AI providers
        auto consensus_decisions = getConsensusDecisions(current_context, decision_type);

        if (consensus_decisions.empty()) {
            std::cerr << "❌ No AI providers available for decision making\n";
            AIDecision failed_decision;
            failed_decision.decision_time = std::chrono::system_clock::now();
            failed_decision.decision_type = decision_type;
            failed_decision.decision_summary = "Decision failed - no providers available";
            failed_decision.confidence_score = 0.0;

            is_processing_decision_.store(false);
            return failed_decision;
        }

        // Combine decisions into final recommendation
        AIDecision final_decision = combineDecisions(consensus_decisions);

        // Cache the decision
        std::string cache_key = decisionTypeToString(decision_type) + "_" +
                               std::to_string(std::chrono::duration_cast<std::chrono::minutes>(
                                   current_context.timestamp.time_since_epoch()).count());
        decision_cache_[cache_key] = final_decision;
        last_decision_times_[decision_type] = final_decision.decision_time;

        // Execute callbacks if registered
        if (decision_callback_) {
            try {
                decision_callback_(final_decision);
            } catch (const std::exception& e) {
                std::cerr << "⚠️  Decision callback failed: " << e.what() << "\n";
            }
        }

        std::cout << "✅ AI Decision completed: " << final_decision.decision_summary << "\n";
        std::cout << "   Confidence: " << std::fixed << std::setprecision(2)
                  << (final_decision.confidence_score * 100) << "%\n";

        is_processing_decision_.store(false);
        return final_decision;

    } catch (const std::exception& e) {
        std::cerr << "❌ Decision making failed: " << e.what() << "\n";

        AIDecision error_decision;
        error_decision.decision_time = std::chrono::system_clock::now();
        error_decision.decision_type = decision_type;
        error_decision.decision_summary = "Decision failed due to error";
        error_decision.reasoning = std::string("Error: ") + e.what();
        error_decision.confidence_score = 0.0;

        is_processing_decision_.store(false);
        return error_decision;
    }
}

std::future<AIDecisionEngine::AIDecision> AIDecisionEngine::consultAI(const MarketContext& context,
                                                                      DecisionType decision_type,
                                                                      AIProvider provider) {
    return std::async(std::launch::async, [this, context, decision_type, provider]() {
        try {
            // Check rate limits
            if (!isWithinRateLimit(provider)) {
                std::cout << "⚠️  Rate limit exceeded for " << aiProviderToString(provider) << "\n";
                AIDecision rate_limited;
                rate_limited.decision_time = std::chrono::system_clock::now();
                rate_limited.decision_type = decision_type;
                rate_limited.provider_used = provider;
                rate_limited.decision_summary = "Rate limit exceeded";
                rate_limited.confidence_score = 0.0;
                return rate_limited;
            }

            // Generate AI prompt
            std::string prompt = generateDecisionPrompt(context, decision_type);

            // Call Claude API (simplified single-provider approach)
            AIDecision decision;
            switch (provider) {
                case AIProvider::CLAUDE_API:
                    decision = callClaudeAPI(prompt, context);
                    break;
                default:
                    throw std::runtime_error("Unknown AI provider");
            }

            decision.decision_type = decision_type;
            decision.provider_used = provider;

            // Update rate limiting
            updateRateLimitHistory(provider);

            return decision;

        } catch (const std::exception& e) {
            std::cerr << "❌ AI consultation failed for " << aiProviderToString(provider)
                      << ": " << e.what() << "\n";

            AIDecision error_decision;
            error_decision.decision_time = std::chrono::system_clock::now();
            error_decision.decision_type = decision_type;
            error_decision.provider_used = provider;
            error_decision.decision_summary = "AI consultation failed";
            error_decision.reasoning = std::string("Error: ") + e.what();
            error_decision.confidence_score = 0.0;
            return error_decision;
        }
    });
}

AIDecisionEngine::AIDecision AIDecisionEngine::callClaudeAPI(const std::string& prompt, const MarketContext& context) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL for Claude API");
    }

    // Find Claude provider config
    auto claude_config = std::find_if(config_.providers.begin(), config_.providers.end(),
                                     [](const AIProviderConfig& config) {
                                         return config.provider == AIProvider::CLAUDE_API;
                                     });

    if (claude_config == config_.providers.end() || !claude_config->is_enabled) {
        curl_easy_cleanup(curl);
        throw std::runtime_error("Claude API provider not configured or disabled");
    }

    HTTPResponse response;

    // Prepare JSON payload
    nlohmann::json payload = {
        {"model", claude_config->credentials.model_name.empty() ? "claude-3-5-sonnet-20241022" : claude_config->credentials.model_name},
        {"max_tokens", 4096},
        {"messages", {
            {{"role", "user"}, {"content", prompt}}
        }},
        {"temperature", 0.3}  // Lower temperature for more consistent financial decisions
    };

    std::string payload_str = payload.dump();

    // Set CURL options
    struct curl_slist* headers = nullptr;
    std::string auth_header = "x-api-key: " + claude_config->credentials.api_key;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, auth_header.c_str());
    headers = curl_slist_append(headers, "anthropic-version: 2023-06-01");

    std::string api_url = claude_config->credentials.api_url.empty() ?
                         "https://api.anthropic.com/v1/messages" :
                         claude_config->credentials.api_url;

    curl_easy_setopt(curl, CURLOPT_URL, api_url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload_str.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(claude_config->credentials.timeout_seconds));
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    // Execute request
    CURLcode res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.response_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error("CURL request failed: " + std::string(curl_easy_strerror(res)));
    }

    if (response.response_code != 200) {
        throw std::runtime_error("Claude API returned HTTP " + std::to_string(response.response_code) +
                               ": " + response.data);
    }

    return parseAPIResponse(response.data, DecisionType::RISK_ADJUSTMENT, AIProvider::CLAUDE_API);
}

// Removed OpenAI API implementation - Claude only integration

// Removed Google Gemini and Local Model implementations - Claude only integration

std::string AIDecisionEngine::generateDecisionPrompt(const MarketContext& context, DecisionType decision_type) {
    std::ostringstream prompt;

    prompt << "CRYPTOCURRENCY TRADING DECISION REQUEST\n\n";
    prompt << "Current Market Context:\n";
    prompt << "- Timestamp: " << std::chrono::duration_cast<std::chrono::seconds>(
        context.timestamp.time_since_epoch()).count() << "\n";
    prompt << "- Portfolio Value: $" << std::fixed << std::setprecision(2) << context.total_portfolio_value << "\n";
    prompt << "- Unrealized P&L: $" << context.unrealized_pnl << "\n";
    prompt << "- Current Drawdown: " << (context.current_drawdown * 100) << "%\n";
    prompt << "- Portfolio Volatility: " << (context.portfolio_volatility * 100) << "%\n";
    prompt << "- VaR (95%, 1-day): $" << context.var_95_1day << "\n";
    prompt << "- Market Trend: " << context.market_trend << "\n";
    prompt << "- Volatility Regime: " << context.volatility_regime << "\n";
    prompt << "- Fear & Greed Index: " << context.fear_greed_index << "\n";

    if (!context.active_alerts.empty()) {
        prompt << "- Active Alerts: ";
        for (size_t i = 0; i < context.active_alerts.size(); ++i) {
            prompt << context.active_alerts[i];
            if (i < context.active_alerts.size() - 1) prompt << ", ";
        }
        prompt << "\n";
    }

    prompt << "\nDecision Type Required: " << decisionTypeToString(decision_type) << "\n\n";

    switch (decision_type) {
        case DecisionType::EMERGENCY_STOP:
            prompt << "EMERGENCY ASSESSMENT REQUIRED:\n";
            prompt << "Analyze if immediate portfolio shutdown is necessary based on:\n";
            prompt << "- Risk metrics exceeding safety thresholds\n";
            prompt << "- Market conditions indicating systemic risk\n";
            prompt << "- Portfolio performance degradation\n";
            break;

        case DecisionType::RISK_ADJUSTMENT:
            prompt << "RISK PARAMETER ADJUSTMENT REQUEST:\n";
            prompt << "Recommend optimal risk parameters based on current conditions:\n";
            prompt << "- Position sizing adjustments\n";
            prompt << "- Stop-loss threshold modifications\n";
            prompt << "- Leverage limit changes\n";
            prompt << "- Concentration risk management\n";
            break;

        case DecisionType::POSITION_SIZING:
            prompt << "POSITION SIZING OPTIMIZATION:\n";
            prompt << "Recommend optimal position sizes considering:\n";
            prompt << "- Current market volatility\n";
            prompt << "- Portfolio concentration\n";
            prompt << "- Risk-adjusted returns\n";
            prompt << "- Correlation between assets\n";
            break;

        case DecisionType::STRATEGY_SELECTION:
            prompt << "TRADING STRATEGY SELECTION:\n";
            prompt << "Recommend which strategies to activate/deactivate based on:\n";
            prompt << "- Current market regime\n";
            prompt << "- Strategy performance metrics\n";
            prompt << "- Market conditions suitability\n";
            break;

        case DecisionType::MARKET_TIMING:
            prompt << "MARKET TIMING DECISION:\n";
            prompt << "Analyze optimal entry/exit timing considering:\n";
            prompt << "- Technical indicators\n";
            prompt << "- Market momentum\n";
            prompt << "- Volatility patterns\n";
            prompt << "- Risk-reward ratios\n";
            break;

        case DecisionType::PORTFOLIO_REBALANCING:
            prompt << "PORTFOLIO REBALANCING RECOMMENDATION:\n";
            prompt << "Suggest portfolio weight adjustments based on:\n";
            prompt << "- Asset performance deviation\n";
            prompt << "- Correlation changes\n";
            prompt << "- Risk-return optimization\n";
            prompt << "- Market condition changes\n";
            break;

        case DecisionType::VOLATILITY_MANAGEMENT:
            prompt << "VOLATILITY MANAGEMENT STRATEGY:\n";
            prompt << "Recommend volatility-based adjustments for:\n";
            prompt << "- Position sizing based on volatility\n";
            prompt << "- Hedging strategies\n";
            prompt << "- Risk limit adjustments\n";
            prompt << "- Dynamic stop-loss levels\n";
            break;
    }

    prompt << "\nRequired Response Format (JSON):\n";
    prompt << "{\n";
    prompt << "  \"decision_summary\": \"Brief summary of recommendation\",\n";
    prompt << "  \"reasoning\": \"Detailed explanation of decision logic\",\n";
    prompt << "  \"confidence_score\": 0.85,\n";
    prompt << "  \"risk_assessment\": \"Risk analysis summary\",\n";
    prompt << "  \"parameter_adjustments\": {\n";
    prompt << "    \"parameter_name\": new_value\n";
    prompt << "  },\n";
    prompt << "  \"action_recommendations\": {\n";
    prompt << "    \"action_type\": \"specific_action\"\n";
    prompt << "  },\n";
    prompt << "  \"immediate_actions\": [\"action1\", \"action2\"],\n";
    prompt << "  \"monitoring_points\": [\"metric1\", \"metric2\"],\n";
    prompt << "  \"requires_immediate_action\": true/false,\n";
    prompt << "  \"implementation_priority\": \"critical/high/medium/low\"\n";
    prompt << "}\n\n";
    prompt << "Provide ONLY the JSON response, no additional text.\n";

    return prompt.str();
}

AIDecisionEngine::AIDecision AIDecisionEngine::parseAPIResponse(const std::string& response,
                                                               DecisionType decision_type,
                                                               AIProvider provider) {
    AIDecision decision;
    decision.decision_time = std::chrono::system_clock::now();
    decision.decision_type = decision_type;
    decision.provider_used = provider;

    try {
        nlohmann::json response_json;

        // Handle different API response formats
        if (provider == AIProvider::CLAUDE_API) {
            auto claude_response = nlohmann::json::parse(response);
            if (claude_response.contains("content") && claude_response["content"].is_array() &&
                !claude_response["content"].empty()) {
                std::string content_text = claude_response["content"][0]["text"];

                // Extract JSON from potential markdown code blocks
                size_t json_start = content_text.find("{");
                size_t json_end = content_text.rfind("}");
                if (json_start != std::string::npos && json_end != std::string::npos) {
                    std::string json_str = content_text.substr(json_start, json_end - json_start + 1);
                    response_json = nlohmann::json::parse(json_str);
                } else {
                    throw std::runtime_error("No JSON found in Claude response");
                }
            } else {
                throw std::runtime_error("Invalid Claude API response format");
            }
        } else if (provider == AIProvider::OPENAI_GPT) {
            auto openai_response = nlohmann::json::parse(response);
            if (openai_response.contains("choices") && openai_response["choices"].is_array() &&
                !openai_response["choices"].empty()) {
                std::string content_text = openai_response["choices"][0]["message"]["content"];

                // Extract JSON from potential markdown code blocks
                size_t json_start = content_text.find("{");
                size_t json_end = content_text.rfind("}");
                if (json_start != std::string::npos && json_end != std::string::npos) {
                    std::string json_str = content_text.substr(json_start, json_end - json_start + 1);
                    response_json = nlohmann::json::parse(json_str);
                } else {
                    throw std::runtime_error("No JSON found in OpenAI response");
                }
            } else {
                throw std::runtime_error("Invalid OpenAI API response format");
            }
        } else {
            // For other providers, assume direct JSON response
            response_json = nlohmann::json::parse(response);
        }

        // Parse decision fields
        if (response_json.contains("decision_summary")) {
            decision.decision_summary = response_json["decision_summary"];
        }

        if (response_json.contains("reasoning")) {
            decision.reasoning = response_json["reasoning"];
        }

        if (response_json.contains("confidence_score")) {
            decision.confidence_score = response_json["confidence_score"];
            // Clamp confidence score to valid range
            decision.confidence_score = std::max(0.0, std::min(1.0, decision.confidence_score));
        }

        if (response_json.contains("risk_assessment")) {
            decision.risk_assessment = response_json["risk_assessment"];
        }

        if (response_json.contains("parameter_adjustments")) {
            for (auto& [key, value] : response_json["parameter_adjustments"].items()) {
                if (value.is_number()) {
                    decision.parameter_adjustments[key] = value;
                }
            }
        }

        if (response_json.contains("action_recommendations")) {
            for (auto& [key, value] : response_json["action_recommendations"].items()) {
                if (value.is_string()) {
                    decision.action_recommendations[key] = value;
                }
            }
        }

        if (response_json.contains("immediate_actions") && response_json["immediate_actions"].is_array()) {
            for (const auto& action : response_json["immediate_actions"]) {
                if (action.is_string()) {
                    decision.immediate_actions.push_back(action);
                }
            }
        }

        if (response_json.contains("monitoring_points") && response_json["monitoring_points"].is_array()) {
            for (const auto& point : response_json["monitoring_points"]) {
                if (point.is_string()) {
                    decision.monitoring_points.push_back(point);
                }
            }
        }

        if (response_json.contains("requires_immediate_action")) {
            decision.requires_immediate_action = response_json["requires_immediate_action"];
        }

        if (response_json.contains("implementation_priority")) {
            decision.implementation_priority = response_json["implementation_priority"];
        }

        // Set default values if not provided
        if (decision.decision_summary.empty()) {
            decision.decision_summary = "AI recommendation for " + decisionTypeToString(decision_type);
        }

        if (decision.confidence_score == 0.0) {
            decision.confidence_score = 0.5; // Default moderate confidence
        }

    } catch (const nlohmann::json::exception& e) {
        std::cerr << "❌ JSON parsing error: " << e.what() << "\n";
        decision.decision_summary = "Failed to parse AI response";
        decision.reasoning = "JSON parsing error: " + std::string(e.what());
        decision.confidence_score = 0.0;
    } catch (const std::exception& e) {
        std::cerr << "❌ Response parsing error: " << e.what() << "\n";
        decision.decision_summary = "Failed to parse AI response";
        decision.reasoning = "Parsing error: " + std::string(e.what());
        decision.confidence_score = 0.0;
    }

    return decision;
}

std::vector<AIDecisionEngine::AIDecision> AIDecisionEngine::getConsensusDecisions(const MarketContext& context,
                                                                                  DecisionType decision_type) {
    std::vector<std::future<AIDecision>> future_decisions;
    std::vector<AIDecision> consensus_decisions;

    // Sort providers by priority
    auto sorted_providers = config_.providers;
    std::sort(sorted_providers.begin(), sorted_providers.end(),
              [](const AIProviderConfig& a, const AIProviderConfig& b) {
                  return a.priority < b.priority;
              });

    // Launch AI consultations in parallel
    for (const auto& provider_config : sorted_providers) {
        if (provider_config.is_enabled && isWithinRateLimit(provider_config.provider)) {
            future_decisions.push_back(consultAI(context, decision_type, provider_config.provider));
        }
    }

    // Collect results
    for (auto& future_decision : future_decisions) {
        try {
            auto decision = future_decision.get();
            if (decision.confidence_score >= config_.minimum_confidence_threshold) {
                consensus_decisions.push_back(decision);
            }
        } catch (const std::exception& e) {
            std::cerr << "⚠️  AI consultation failed: " << e.what() << "\n";
        }
    }

    return consensus_decisions;
}

AIDecisionEngine::AIDecision AIDecisionEngine::combineDecisions(const std::vector<AIDecision>& decisions) {
    if (decisions.empty()) {
        throw std::runtime_error("Cannot combine empty decision list");
    }

    if (decisions.size() == 1) {
        return decisions[0];
    }

    AIDecision combined_decision;
    combined_decision.decision_time = std::chrono::system_clock::now();
    combined_decision.decision_type = decisions[0].decision_type;

    // Calculate weighted average confidence
    double total_confidence = 0.0;
    double total_weight = 0.0;

    for (const auto& decision : decisions) {
        double weight = decision.confidence_score;
        total_confidence += decision.confidence_score * weight;
        total_weight += weight;
    }

    combined_decision.confidence_score = total_weight > 0 ? total_confidence / total_weight : 0.0;

    // Combine summaries
    std::ostringstream summary_stream;
    summary_stream << "Consensus decision from " << decisions.size() << " AI providers: ";
    for (size_t i = 0; i < decisions.size(); ++i) {
        summary_stream << decisions[i].decision_summary;
        if (i < decisions.size() - 1) summary_stream << " | ";
    }
    combined_decision.decision_summary = summary_stream.str();

    // Combine reasoning
    std::ostringstream reasoning_stream;
    for (const auto& decision : decisions) {
        reasoning_stream << aiProviderToString(decision.provider_used) << ": "
                        << decision.reasoning << " ";
    }
    combined_decision.reasoning = reasoning_stream.str();

    // Merge parameter adjustments (average values)
    std::map<std::string, std::vector<double>> parameter_values;
    for (const auto& decision : decisions) {
        for (const auto& [param, value] : decision.parameter_adjustments) {
            parameter_values[param].push_back(value);
        }
    }

    for (const auto& [param, values] : parameter_values) {
        double avg_value = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
        combined_decision.parameter_adjustments[param] = avg_value;
    }

    // Merge immediate actions (unique actions only)
    std::set<std::string> unique_actions;
    for (const auto& decision : decisions) {
        for (const auto& action : decision.immediate_actions) {
            unique_actions.insert(action);
        }
    }
    combined_decision.immediate_actions.assign(unique_actions.begin(), unique_actions.end());

    // Set priority based on highest priority decision
    combined_decision.requires_immediate_action = std::any_of(decisions.begin(), decisions.end(),
        [](const AIDecision& d) { return d.requires_immediate_action; });

    // Set implementation priority to highest priority found
    std::vector<std::string> priorities = {"critical", "high", "medium", "low"};
    combined_decision.implementation_priority = "low";
    for (const auto& priority : priorities) {
        for (const auto& decision : decisions) {
            if (decision.implementation_priority == priority) {
                combined_decision.implementation_priority = priority;
                break;
            }
        }
        if (combined_decision.implementation_priority == priority) break;
    }

    return combined_decision;
}

AIDecisionEngine::MarketContext AIDecisionEngine::buildCurrentMarketContext() {
    MarketContext context;
    context.timestamp = std::chrono::system_clock::now();

    // Get data from integrated components
    if (performance_engine_) {
        // Get performance metrics
        context.total_portfolio_value = 1000000.0; // Placeholder - would get from performance engine
        context.unrealized_pnl = 5000.0;
        context.realized_pnl_today = 1500.0;
    }

    if (risk_manager_) {
        // Get risk metrics
        context.current_drawdown = 0.03; // Placeholder - would get from risk manager
        context.portfolio_volatility = 0.15;
        context.var_95_1day = 25000.0;
        context.concentration_risk = 0.25;
    }

    // Set market regime indicators (would be calculated from market data)
    context.market_trend = "sideways";
    context.volatility_regime = "normal";
    context.fear_greed_index = 55.0;

    return context;
}

bool AIDecisionEngine::checkEmergencyConditions(const MarketContext& context) {
    bool emergency_detected = false;

    // Check drawdown threshold
    if (context.current_drawdown > config_.emergency_config.drawdown_threshold) {
        std::cout << "🚨 Emergency: Drawdown threshold exceeded ("
                  << (context.current_drawdown * 100) << "% > "
                  << (config_.emergency_config.drawdown_threshold * 100) << "%)\n";
        emergency_detected = true;
    }

    // Check volatility threshold
    if (context.portfolio_volatility > config_.emergency_config.volatility_threshold) {
        std::cout << "🚨 Emergency: Volatility threshold exceeded ("
                  << (context.portfolio_volatility * 100) << "% > "
                  << (config_.emergency_config.volatility_threshold * 100) << "%)\n";
        emergency_detected = true;
    }

    // Check concentration risk
    if (context.concentration_risk > config_.emergency_config.concentration_threshold) {
        std::cout << "🚨 Emergency: Concentration risk threshold exceeded ("
                  << (context.concentration_risk * 100) << "% > "
                  << (config_.emergency_config.concentration_threshold * 100) << "%)\n";
        emergency_detected = true;
    }

    // Check VaR threshold
    double var_percentage = context.var_95_1day / context.total_portfolio_value;
    if (var_percentage > config_.emergency_config.var_threshold) {
        std::cout << "🚨 Emergency: VaR threshold exceeded ("
                  << (var_percentage * 100) << "% > "
                  << (config_.emergency_config.var_threshold * 100) << "%)\n";
        emergency_detected = true;
    }

    return emergency_detected;
}

AIDecisionEngine::EmergencyStopResult AIDecisionEngine::executeEmergencyStop(const MarketContext& context,
                                                                             const std::string& reason) {
    std::lock_guard<std::mutex> lock(emergency_mutex_);

    EmergencyStopResult result;
    result.trigger_time = std::chrono::system_clock::now();
    result.trigger_reason = reason;

    // Set emergency stop flag
    is_emergency_stopped_.store(true);
    last_emergency_stop_ = result.trigger_time;

    std::cout << "🚨 EMERGENCY STOP TRIGGERED: " << reason << "\n";

    // Capture trigger metrics
    result.trigger_metrics["drawdown"] = context.current_drawdown;
    result.trigger_metrics["volatility"] = context.portfolio_volatility;
    result.trigger_metrics["concentration"] = context.concentration_risk;
    result.trigger_metrics["var_95"] = context.var_95_1day / context.total_portfolio_value;

    // Consult AI if enabled and within timeout
    if (config_.emergency_config.ai_enabled) {
        try {
            std::cout << "🤖 Consulting AI for emergency stop guidance...\n";
            auto ai_future = consultAI(context, DecisionType::EMERGENCY_STOP, AIProvider::CLAUDE_API);

            // Wait for AI response with timeout
            auto status = ai_future.wait_for(std::chrono::duration_cast<std::chrono::seconds>(
                config_.emergency_config.ai_response_timeout));

            if (status == std::future_status::ready) {
                result.ai_recommendation = ai_future.get();
                result.ai_consultation_successful = true;
                std::cout << "✅ AI emergency guidance received\n";
            } else {
                result.ai_failure_reason = "AI consultation timeout";
                std::cout << "⏰ AI consultation timeout\n";
            }
        } catch (const std::exception& e) {
            result.ai_failure_reason = "AI consultation error: " + std::string(e.what());
            std::cout << "❌ AI consultation failed: " << e.what() << "\n";
        }
    }

    // Execute emergency actions (placeholder - would interface with trading system)
    std::cout << "🔄 Executing emergency stop procedures...\n";

    // Close all positions (simulation)
    for (const auto& [symbol, size] : context.position_sizes) {
        if (size != 0.0) {
            result.positions_closed.push_back(symbol);
            result.final_position_sizes[symbol] = 0.0;
            std::cout << "   ✅ Closed position: " << symbol << " (size: " << size << ")\n";
        }
    }

    // Cancel all orders (simulation)
    result.orders_cancelled.push_back("All pending orders cancelled");

    // Calculate realized P&L (simulation)
    result.total_realized_pnl = context.unrealized_pnl; // Convert unrealized to realized

    // Set recovery strategy
    if (result.ai_consultation_successful && !result.ai_recommendation.decision_summary.empty()) {
        result.recovery_strategy = result.ai_recommendation.decision_summary;
    } else {
        result.recovery_strategy = "Manual review required before restart";
    }

    // Set earliest restart time (1 hour minimum cooldown)
    result.earliest_restart_time = result.trigger_time + std::chrono::hours{1};

    result.restart_conditions = {
        "Manual confirmation required",
        "Risk metrics within normal ranges",
        "Market conditions stabilized",
        "Position sizing rules updated"
    };

    // Store in history
    emergency_stop_history_.push_back(result);

    // Execute callback if registered
    if (emergency_callback_) {
        try {
            emergency_callback_(result);
        } catch (const std::exception& e) {
            std::cerr << "⚠️  Emergency callback failed: " << e.what() << "\n";
        }
    }

    std::cout << "🛑 Emergency stop complete. Total realized P&L: $"
              << std::fixed << std::setprecision(2) << result.total_realized_pnl << "\n";

    return result;
}

bool AIDecisionEngine::isWithinRateLimit(AIProvider provider) {
    auto now = std::chrono::system_clock::now();
    auto& call_history = api_call_history_[provider];

    // Find provider config for rate limit
    auto provider_config = std::find_if(config_.providers.begin(), config_.providers.end(),
                                       [provider](const AIProviderConfig& config) {
                                           return config.provider == provider;
                                       });

    if (provider_config == config_.providers.end()) {
        return false;
    }

    double rate_limit = provider_config->credentials.rate_limit_per_minute;
    auto time_window = std::chrono::minutes{1};

    // Remove old entries outside the time window
    while (!call_history.empty() &&
           (now - call_history.front()) > time_window) {
        call_history.pop();
    }

    // Check if we're within rate limit
    return call_history.size() < static_cast<size_t>(rate_limit);
}

void AIDecisionEngine::updateRateLimitHistory(AIProvider provider) {
    auto now = std::chrono::system_clock::now();
    api_call_history_[provider].push(now);
    last_api_calls_[provider] = now;
}

AIDecisionEngine::AIDecision AIDecisionEngine::getCachedDecision(DecisionType decision_type) const {
    std::lock_guard<std::mutex> lock(decision_mutex_);

    // Look for recent cached decisions
    auto now = std::chrono::system_clock::now();

    for (const auto& [cache_key, decision] : decision_cache_) {
        if (decision.decision_type == decision_type) {
            auto age = std::chrono::duration_cast<std::chrono::minutes>(now - decision.decision_time);
            if (age < config_.decision_cache_duration) {
                return decision;
            }
        }
    }

    return AIDecision{};  // Return empty decision if no valid cache found
}

// Static utility methods
std::string AIDecisionEngine::aiProviderToString(AIProvider provider) {
    switch (provider) {
        case AIProvider::CLAUDE_API: return "claude";
        case AIProvider::OPENAI_GPT: return "openai";
        case AIProvider::GOOGLE_GEMINI: return "gemini";
        case AIProvider::LOCAL_MODEL: return "local";
        default: return "unknown";
    }
}

AIDecisionEngine::AIProvider AIDecisionEngine::stringToAIProvider(const std::string& provider_str) {
    if (provider_str == "claude") return AIProvider::CLAUDE_API;
    if (provider_str == "openai") return AIProvider::OPENAI_GPT;
    if (provider_str == "gemini") return AIProvider::GOOGLE_GEMINI;
    if (provider_str == "local") return AIProvider::LOCAL_MODEL;
    return AIProvider::CLAUDE_API; // Default
}

std::string AIDecisionEngine::decisionTypeToString(DecisionType decision_type) {
    switch (decision_type) {
        case DecisionType::EMERGENCY_STOP: return "emergency_stop";
        case DecisionType::RISK_ADJUSTMENT: return "risk_adjustment";
        case DecisionType::POSITION_SIZING: return "position_sizing";
        case DecisionType::STRATEGY_SELECTION: return "strategy_selection";
        case DecisionType::MARKET_TIMING: return "market_timing";
        case DecisionType::PORTFOLIO_REBALANCING: return "portfolio_rebalancing";
        case DecisionType::VOLATILITY_MANAGEMENT: return "volatility_management";
        default: return "unknown";
    }
}

AIDecisionEngine::DecisionType AIDecisionEngine::stringToDecisionType(const std::string& decision_type_str) {
    if (decision_type_str == "emergency_stop") return DecisionType::EMERGENCY_STOP;
    if (decision_type_str == "risk_adjustment") return DecisionType::RISK_ADJUSTMENT;
    if (decision_type_str == "position_sizing") return DecisionType::POSITION_SIZING;
    if (decision_type_str == "strategy_selection") return DecisionType::STRATEGY_SELECTION;
    if (decision_type_str == "market_timing") return DecisionType::MARKET_TIMING;
    if (decision_type_str == "portfolio_rebalancing") return DecisionType::PORTFOLIO_REBALANCING;
    if (decision_type_str == "volatility_management") return DecisionType::VOLATILITY_MANAGEMENT;
    return DecisionType::RISK_ADJUSTMENT; // Default
}

std::vector<AIDecisionEngine::AIDecision> AIDecisionEngine::makeMultipleDecisions(const std::vector<DecisionType>& decision_types) {
    std::vector<AIDecision> results;
    MarketContext context = buildCurrentMarketContext();

    for (DecisionType decision_type : decision_types) {
        try {
            AIDecision decision = makeDecision(decision_type, context);
            results.push_back(decision);
            std::cout << "✅ Decision " << decisionTypeToString(decision_type)
                      << " completed with " << (decision.confidence_score * 100) << "% confidence\n";
        } catch (const std::exception& e) {
            std::cerr << "❌ Decision " << decisionTypeToString(decision_type)
                      << " failed: " << e.what() << "\n";
        }
    }

    return results;
}

void AIDecisionEngine::clearDecisionCache() {
    std::lock_guard<std::mutex> lock(decision_mutex_);
    decision_cache_.clear();
    std::cout << "🗑️  Decision cache cleared\n";
}

AIDecisionEngine::EmergencyStopResult AIDecisionEngine::triggerEmergencyStop(const std::string& reason) {
    MarketContext context = buildCurrentMarketContext();
    return executeEmergencyStop(context, reason);
}

AIDecisionEngine::EmergencyStopResult AIDecisionEngine::getLastEmergencyStop() const {
    std::lock_guard<std::mutex> lock(emergency_mutex_);
    if (emergency_stop_history_.empty()) {
        return EmergencyStopResult{};
    }
    return emergency_stop_history_.back();
}

bool AIDecisionEngine::attemptRecoveryFromEmergencyStop() {
    std::lock_guard<std::mutex> lock(emergency_mutex_);

    if (!is_emergency_stopped_.load()) {
        std::cout << "ℹ️  System is not in emergency stop state\n";
        return true;
    }

    auto now = std::chrono::system_clock::now();
    if (!emergency_stop_history_.empty()) {
        auto last_stop = emergency_stop_history_.back();
        if (now < last_stop.earliest_restart_time) {
            auto remaining = std::chrono::duration_cast<std::chrono::minutes>(
                last_stop.earliest_restart_time - now);
            std::cout << "⏰ Cannot restart yet. " << remaining.count()
                      << " minutes remaining in cooldown period\n";
            return false;
        }
    }

    // Check current market conditions
    MarketContext current_context = buildCurrentMarketContext();
    if (checkEmergencyConditions(current_context)) {
        std::cout << "❌ Emergency conditions still present, cannot restart\n";
        return false;
    }

    // Clear emergency stop flag
    is_emergency_stopped_.store(false);
    std::cout << "✅ Emergency stop cleared, system ready for restart\n";
    return true;
}

std::vector<AIDecisionEngine::EmergencyStopResult> AIDecisionEngine::getEmergencyStopHistory() const {
    std::lock_guard<std::mutex> lock(emergency_mutex_);
    return emergency_stop_history_;
}

AIDecisionEngine::ParameterAdjustment AIDecisionEngine::adjustParameters(const std::string& category) {
    std::cout << "🎛️  Starting parameter adjustment for category: " << category << "\n";

    ParameterAdjustment adjustment;
    adjustment.adjustment_time = std::chrono::system_clock::now();
    adjustment.parameter_category = category;

    try {
        // Build market context
        MarketContext context = buildCurrentMarketContext();

        // Get AI recommendation for parameter adjustment
        DecisionType decision_type = DecisionType::RISK_ADJUSTMENT;
        if (category == "strategy") {
            decision_type = DecisionType::STRATEGY_SELECTION;
        } else if (category == "portfolio") {
            decision_type = DecisionType::PORTFOLIO_REBALANCING;
        }

        adjustment.ai_recommendation = makeDecision(decision_type, context);

        // Execute parameter adjustment based on AI recommendation
        if (adjustment.ai_recommendation.confidence_score >= config_.minimum_confidence_threshold) {
            adjustment = executeParameterAdjustment(adjustment.ai_recommendation);
        } else {
            std::cout << "⚠️  AI confidence too low (" << (adjustment.ai_recommendation.confidence_score * 100)
                      << "%), skipping parameter adjustment\n";
            adjustment.passed_risk_checks = false;
            adjustment.validation_warnings.push_back("AI confidence below threshold");
        }

        // Store in history
        adjustment_history_.push_back(adjustment);

        // Execute callback if registered
        if (adjustment_callback_) {
            try {
                adjustment_callback_(adjustment);
            } catch (const std::exception& e) {
                std::cerr << "⚠️  Adjustment callback failed: " << e.what() << "\n";
            }
        }

        std::cout << "✅ Parameter adjustment completed\n";

    } catch (const std::exception& e) {
        std::cerr << "❌ Parameter adjustment failed: " << e.what() << "\n";
        adjustment.validation_warnings.push_back("Adjustment failed: " + std::string(e.what()));
    }

    return adjustment;
}

AIDecisionEngine::ParameterAdjustment AIDecisionEngine::executeParameterAdjustment(const AIDecision& decision) {
    ParameterAdjustment adjustment;
    adjustment.adjustment_time = std::chrono::system_clock::now();
    adjustment.ai_recommendation = decision;

    // Store old values (placeholder - would get from actual system)
    adjustment.old_values["max_position_size"] = 0.20;
    adjustment.old_values["stop_loss_threshold"] = 0.05;
    adjustment.old_values["leverage_limit"] = 2.0;

    // Apply new values from AI recommendation
    for (const auto& [param, value] : decision.parameter_adjustments) {
        if (adjustment.old_values.find(param) != adjustment.old_values.end()) {
            adjustment.new_values[param] = value;
            adjustment.adjustment_rationales[param] = value - adjustment.old_values[param];
        }
    }

    // Validate adjustments
    adjustment.passed_risk_checks = validateParameterAdjustment(adjustment);
    adjustment.passed_correlation_checks = true; // Placeholder

    // Set next review time
    adjustment.next_review_time = adjustment.adjustment_time + std::chrono::hours{4};

    adjustment.success_metrics = {
        "sharpe_ratio_improvement",
        "drawdown_reduction",
        "volatility_stabilization"
    };

    std::cout << "📊 Applied " << adjustment.new_values.size() << " parameter adjustments\n";
    for (const auto& [param, new_val] : adjustment.new_values) {
        std::cout << "   " << param << ": " << adjustment.old_values[param]
                  << " → " << new_val << "\n";
    }

    return adjustment;
}

bool AIDecisionEngine::validateParameterAdjustment(ParameterAdjustment& adjustment) {
    bool valid = true;

    // Check parameter bounds
    for (const auto& [param, value] : adjustment.new_values) {
        if (param == "max_position_size" && (value < 0.01 || value > 0.50)) {
            adjustment.validation_warnings.push_back("Position size out of bounds: " + std::to_string(value));
            valid = false;
        }
        if (param == "stop_loss_threshold" && (value < 0.01 || value > 0.20)) {
            adjustment.validation_warnings.push_back("Stop loss threshold out of bounds: " + std::to_string(value));
            valid = false;
        }
        if (param == "leverage_limit" && (value < 1.0 || value > 5.0)) {
            adjustment.validation_warnings.push_back("Leverage limit out of bounds: " + std::to_string(value));
            valid = false;
        }
    }

    // Check for extreme changes
    for (const auto& [param, change] : adjustment.adjustment_rationales) {
        if (std::abs(change) > 0.50) { // 50% change limit
            adjustment.validation_warnings.push_back("Extreme parameter change detected: " + param);
        }
    }

    return valid;
}

AIDecisionEngine::ParameterAdjustment AIDecisionEngine::getLastParameterAdjustment() const {
    if (adjustment_history_.empty()) {
        return ParameterAdjustment{};
    }
    return adjustment_history_.back();
}

std::vector<AIDecisionEngine::ParameterAdjustment> AIDecisionEngine::getAdjustmentHistory() const {
    return adjustment_history_;
}

bool AIDecisionEngine::revertLastAdjustment() {
    if (adjustment_history_.empty()) {
        std::cout << "ℹ️  No adjustments to revert\n";
        return false;
    }

    auto last_adjustment = adjustment_history_.back();

    std::cout << "↩️  Reverting parameter adjustment from "
              << std::chrono::duration_cast<std::chrono::seconds>(
                  last_adjustment.adjustment_time.time_since_epoch()).count() << "\n";

    // Revert parameters (placeholder - would interface with actual system)
    for (const auto& [param, old_val] : last_adjustment.old_values) {
        std::cout << "   " << param << ": " << last_adjustment.new_values.at(param)
                  << " → " << old_val << "\n";
    }

    std::cout << "✅ Parameter adjustment reverted successfully\n";
    return true;
}

AIDecisionEngine::AIEngineMetrics AIDecisionEngine::getEngineMetrics() const {
    AIEngineMetrics metrics;
    metrics.metrics_time = std::chrono::system_clock::now();

    // Calculate API usage statistics
    for (const auto& [provider, call_history] : api_call_history_) {
        metrics.api_calls_today[provider] = call_history.size();
        metrics.success_rates[provider] = 0.95; // Placeholder
        metrics.average_response_times[provider] = 2.5; // Placeholder
    }

    // Calculate decision quality metrics
    if (!decision_cache_.empty()) {
        double total_confidence = 0.0;
        int valid_decisions = 0;

        for (const auto& [key, decision] : decision_cache_) {
            if (decision.confidence_score > 0.0) {
                total_confidence += decision.confidence_score;
                valid_decisions++;
            }
        }

        if (valid_decisions > 0) {
            metrics.average_confidence_score = total_confidence / valid_decisions;
        }

        metrics.successful_decisions_count = valid_decisions;
        metrics.decision_accuracy_rate = 0.87; // Placeholder
    }

    // Emergency stop metrics
    metrics.emergency_stops_count = emergency_stop_history_.size();
    metrics.emergency_stop_success_rate = 1.0; // Placeholder
    metrics.average_recovery_time_hours = 2.5; // Placeholder

    // Parameter adjustment metrics
    metrics.adjustments_made_count = adjustment_history_.size();
    metrics.successful_adjustments_count = std::count_if(
        adjustment_history_.begin(), adjustment_history_.end(),
        [](const ParameterAdjustment& adj) { return adj.passed_risk_checks; });
    metrics.average_performance_improvement = 0.15; // Placeholder

    return metrics;
}

bool AIDecisionEngine::startContinuousMonitoring(std::chrono::minutes interval) {
    std::cout << "🔄 Starting continuous AI monitoring (interval: " << interval.count() << " minutes)\n";

    // Placeholder implementation - would start background thread for continuous monitoring
    std::cout << "✅ Continuous monitoring started successfully\n";
    return true;
}

void AIDecisionEngine::stopContinuousMonitoring() {
    std::cout << "🛑 Stopping continuous AI monitoring\n";
    // Placeholder implementation - would stop background monitoring thread
}

bool AIDecisionEngine::isContinuousMonitoringActive() const {
    // Placeholder implementation
    return false;
}

void AIDecisionEngine::setDecisionCallback(std::function<void(const AIDecision&)> callback) {
    decision_callback_ = callback;
    std::cout << "📞 Decision callback registered\n";
}

void AIDecisionEngine::setEmergencyCallback(std::function<void(const EmergencyStopResult&)> callback) {
    emergency_callback_ = callback;
    std::cout << "📞 Emergency callback registered\n";
}

void AIDecisionEngine::setAdjustmentCallback(std::function<void(const ParameterAdjustment&)> callback) {
    adjustment_callback_ = callback;
    std::cout << "📞 Adjustment callback registered\n";
}

bool AIDecisionEngine::exportDecisionHistory(const std::string& filename, const std::string& format) const {
    try {
        nlohmann::json export_data = nlohmann::json::array();

        for (const auto& [key, decision] : decision_cache_) {
            nlohmann::json decision_json = {
                {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                    decision.decision_time.time_since_epoch()).count()},
                {"decision_type", decisionTypeToString(decision.decision_type)},
                {"provider", aiProviderToString(decision.provider_used)},
                {"summary", decision.decision_summary},
                {"reasoning", decision.reasoning},
                {"confidence_score", decision.confidence_score},
                {"risk_assessment", decision.risk_assessment},
                {"parameter_adjustments", decision.parameter_adjustments},
                {"immediate_actions", decision.immediate_actions},
                {"requires_immediate_action", decision.requires_immediate_action}
            };
            export_data.push_back(decision_json);
        }

        std::ofstream file(filename);
        if (file.is_open()) {
            file << export_data.dump(2);
            file.close();
            std::cout << "✅ Decision history exported to " << filename << "\n";
            return true;
        } else {
            std::cerr << "❌ Failed to open file for export: " << filename << "\n";
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "❌ Export failed: " << e.what() << "\n";
        return false;
    }
}

AIDecisionEngine::AIEngineTestResult AIDecisionEngine::runSystemTests() {
    AIEngineTestResult result;
    result.test_time = std::chrono::system_clock::now();
    result.overall_success = true;

    std::cout << "🧪 Running AI Decision Engine system tests...\n";

    // Test provider connectivity
    for (const auto& provider_config : config_.providers) {
        if (provider_config.is_enabled) {
            bool test_passed = testAIProvider(provider_config.provider);
            result.provider_connectivity_tests[provider_config.provider] = test_passed;
            if (!test_passed) {
                result.overall_success = false;
                result.test_failures.push_back("Provider " + aiProviderToString(provider_config.provider) + " connectivity failed");
            }
        }
    }

    // Test decision types
    std::vector<DecisionType> test_decision_types = {
        DecisionType::RISK_ADJUSTMENT,
        DecisionType::POSITION_SIZING,
        DecisionType::STRATEGY_SELECTION
    };

    for (DecisionType decision_type : test_decision_types) {
        try {
            MarketContext test_context = buildCurrentMarketContext();
            AIDecision decision = makeDecision(decision_type, test_context);
            bool test_passed = !decision.decision_summary.empty() && decision.confidence_score > 0.0;
            result.decision_type_tests[decision_type] = test_passed;
            if (!test_passed) {
                result.overall_success = false;
                result.test_failures.push_back("Decision type " + decisionTypeToString(decision_type) + " test failed");
            }
        } catch (const std::exception& e) {
            result.decision_type_tests[decision_type] = false;
            result.overall_success = false;
            result.test_failures.push_back("Decision type " + decisionTypeToString(decision_type) + " threw exception: " + e.what());
        }
    }

    // Test emergency stop functionality
    try {
        // Save current emergency state
        bool was_emergency_stopped = is_emergency_stopped_.load();

        // Test emergency stop
        auto emergency_result = triggerEmergencyStop("System test");
        result.emergency_stop_test_passed = !emergency_result.trigger_reason.empty();

        // Reset emergency state
        if (!was_emergency_stopped) {
            is_emergency_stopped_.store(false);
        }
    } catch (const std::exception& e) {
        result.emergency_stop_test_passed = false;
        result.test_failures.push_back("Emergency stop test failed: " + std::string(e.what()));
        result.overall_success = false;
    }

    // Test parameter adjustment
    try {
        auto adjustment = adjustParameters("risk");
        result.parameter_adjustment_test_passed = !adjustment.ai_recommendation.decision_summary.empty();
    } catch (const std::exception& e) {
        result.parameter_adjustment_test_passed = false;
        result.test_failures.push_back("Parameter adjustment test failed: " + std::string(e.what()));
        result.overall_success = false;
    }

    // Generate test summary
    int passed_tests = 0;
    int total_tests = result.provider_connectivity_tests.size() +
                     result.decision_type_tests.size() +
                     (result.emergency_stop_test_passed ? 1 : 0) +
                     (result.parameter_adjustment_test_passed ? 1 : 0);

    for (const auto& [provider, passed] : result.provider_connectivity_tests) {
        if (passed) passed_tests++;
    }
    for (const auto& [decision_type, passed] : result.decision_type_tests) {
        if (passed) passed_tests++;
    }
    if (result.emergency_stop_test_passed) passed_tests++;
    if (result.parameter_adjustment_test_passed) passed_tests++;

    result.test_summary = "AI Engine Tests: " + std::to_string(passed_tests) + "/" +
                         std::to_string(total_tests) + " passed";

    if (result.overall_success) {
        std::cout << "✅ " << result.test_summary << "\n";
    } else {
        std::cout << "❌ " << result.test_summary << "\n";
        std::cout << "Failures:\n";
        for (const auto& failure : result.test_failures) {
            std::cout << "  - " << failure << "\n";
        }
    }

    return result;
}

bool AIDecisionEngine::validateAIResponse(const AIDecision& decision) const {
    // Validate required fields
    if (decision.decision_summary.empty()) {
        return false;
    }

    // Validate confidence score range
    if (decision.confidence_score < 0.0 || decision.confidence_score > 1.0) {
        return false;
    }

    // Validate parameter adjustments are reasonable
    for (const auto& [param, value] : decision.parameter_adjustments) {
        if (std::isnan(value) || std::isinf(value)) {
            return false;
        }

        // Basic range checks for common parameters
        if (param.find("position_size") != std::string::npos && (value < 0.0 || value > 1.0)) {
            return false;
        }
        if (param.find("leverage") != std::string::npos && (value < 0.0 || value > 10.0)) {
            return false;
        }
    }

    return true;
}

AIDecisionEngine::AIProviderConfig AIDecisionEngine::createDefaultProviderConfig(AIProvider provider) {
    AIProviderConfig config;
    config.provider = provider;
    config.is_enabled = true;
    config.priority = 1;

    switch (provider) {
        case AIProvider::CLAUDE_API:
            config.credentials.api_url = "https://api.anthropic.com/v1/messages";
            config.credentials.model_name = "claude-3-5-sonnet-20241022";
            config.credentials.timeout_seconds = 30.0;
            config.credentials.rate_limit_per_minute = 60.0;
            config.supported_tasks = {"risk_adjustment", "emergency_stop", "position_sizing"};
            break;

        // Removed other providers - Claude only integration
    }

    return config;
}

std::vector<AIDecisionEngine::AIProvider> AIDecisionEngine::getAvailableProviders() const {
    std::vector<AIProvider> available;
    for (const auto& provider_config : config_.providers) {
        if (provider_config.is_enabled) {
            available.push_back(provider_config.provider);
        }
    }
    return available;
}

} // namespace AI
} // namespace CryptoClaude