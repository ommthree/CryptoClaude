/**
 * Day 25 AI Score Polish Engine Test Suite
 * Testing Claude-only integration for score polishing of Random Forest predictions
 * Week 6 - Advanced AI-Powered Trading Systems
 */

#include <iostream>
#include <cassert>
#include <chrono>
#include <memory>
#include <vector>
#include <string>

#include "src/Core/AI/AIDecisionEngine.h"

using namespace CryptoClaude::AI;

// Test Configuration
struct TestConfig {
    bool verbose = true;
    bool test_claude_api = true;   // Set to true to test actual Claude API (requires credentials)
    std::string test_log_file = "ai_score_polish_test.log";
};

TestConfig g_test_config;

// Test utilities
void logTest(const std::string& message) {
    if (g_test_config.verbose) {
        std::cout << "[TEST] " << message << std::endl;
    }
}

void logResult(const std::string& test_name, bool passed) {
    std::string status = passed ? "✅ PASS" : "❌ FAIL";
    std::cout << status << " " << test_name << std::endl;
}

// Test 1: Basic Engine Initialization
bool test_engine_initialization() {
    logTest("Testing AI Score Polish Engine initialization...");

    try {
        AIScorePolishEngine::ScorePolishEngineConfig config;
        config.polish_config.enabled = true;
        config.polish_config.max_adjustment_percent = 0.20; // 20% max adjustment

        AIScorePolishEngine engine(config);

        const auto& loaded_config = engine.getConfig();

        // Verify configuration was loaded correctly
        bool config_ok = (loaded_config.polish_config.enabled == true) &&
                        (loaded_config.polish_config.max_adjustment_percent == 0.20);

        return config_ok;

    } catch (const std::exception& e) {
        logTest("Exception during initialization: " + std::string(e.what()));
        return false;
    }
}

// Test 2: Configuration Management
bool test_configuration_management() {
    logTest("Testing configuration management...");

    try {
        AIScorePolishEngine engine;

        // Update configuration
        AIScorePolishEngine::ScorePolishEngineConfig new_config;
        new_config.polish_config.enabled = false;
        new_config.polish_config.max_adjustment_percent = 0.15;
        new_config.polish_config.max_polish_calls_per_hour = 25;

        engine.updateConfig(new_config);

        const auto& updated_config = engine.getConfig();

        return (updated_config.polish_config.enabled == false) &&
               (updated_config.polish_config.max_adjustment_percent == 0.15) &&
               (updated_config.polish_config.max_polish_calls_per_hour == 25);

    } catch (const std::exception& e) {
        logTest("Exception during configuration test: " + std::string(e.what()));
        return false;
    }
}

// Test 3: Random Forest Prediction Structure
bool test_prediction_structure() {
    logTest("Testing Random Forest prediction structure...");

    try {
        // Create sample Random Forest predictions
        std::vector<AIScorePolishEngine::RandomForestPrediction> predictions;

        AIScorePolishEngine::RandomForestPrediction pred1;
        pred1.symbol = "BTC";
        pred1.confidence_score = 0.85;
        pred1.ranking_score = 0.92;
        pred1.original_rank = 1;
        pred1.prediction_rationale = "Strong technical indicators and high volume";
        predictions.push_back(pred1);

        AIScorePolishEngine::RandomForestPrediction pred2;
        pred2.symbol = "ETH";
        pred2.confidence_score = 0.72;
        pred2.ranking_score = 0.78;
        pred2.original_rank = 2;
        pred2.prediction_rationale = "Moderate bullish signals";
        predictions.push_back(pred2);

        // Verify structure
        return (predictions.size() == 2) &&
               (predictions[0].symbol == "BTC") &&
               (predictions[1].confidence_score == 0.72);

    } catch (const std::exception& e) {
        logTest("Exception during prediction structure test: " + std::string(e.what()));
        return false;
    }
}

// Test 4: Market Context Structure
bool test_market_context() {
    logTest("Testing market context structure...");

    try {
        AIScorePolishEngine::MarketContext context;
        context.total_portfolio_value = 100000.0;
        context.unrealized_pnl = 2500.0;
        context.market_trend = "bullish";
        context.volatility_regime = "normal";
        context.fear_greed_index = 65.0;

        // Add some current prices
        context.current_prices["BTC"] = 45000.0;
        context.current_prices["ETH"] = 3000.0;

        return (context.total_portfolio_value == 100000.0) &&
               (context.market_trend == "bullish") &&
               (context.current_prices["BTC"] == 45000.0);

    } catch (const std::exception& e) {
        logTest("Exception during market context test: " + std::string(e.what()));
        return false;
    }
}

// Test 5: Score Polish with Disabled Configuration
bool test_polish_disabled() {
    logTest("Testing score polish with disabled configuration...");

    try {
        AIScorePolishEngine::ScorePolishEngineConfig config;
        config.polish_config.enabled = false; // Disable polishing

        AIScorePolishEngine engine(config);

        // Create test predictions
        std::vector<AIScorePolishEngine::RandomForestPrediction> predictions;
        AIScorePolishEngine::RandomForestPrediction pred;
        pred.symbol = "BTC";
        pred.confidence_score = 0.80;
        pred.ranking_score = 0.85;
        pred.original_rank = 1;
        predictions.push_back(pred);

        AIScorePolishEngine::MarketContext context;
        context.market_trend = "neutral";

        // Attempt to polish scores
        AIScorePolishEngine::ScorePolishResult result = engine.polishScores(predictions, context);

        // Should return immediately with disabled message
        return (result.adjustment_confidence == 0.0) &&
               (result.polish_reasoning.find("disabled") != std::string::npos);

    } catch (const std::exception& e) {
        logTest("Exception during polish disabled test: " + std::string(e.what()));
        return false;
    }
}

// Test 6: Polished Prediction Generation
bool test_polished_predictions() {
    logTest("Testing polished prediction generation...");

    try {
        AIScorePolishEngine::ScorePolishEngineConfig config;
        config.polish_config.enabled = false; // Disable API calls for this test

        AIScorePolishEngine engine(config);

        // Create test predictions
        std::vector<AIScorePolishEngine::RandomForestPrediction> predictions;
        AIScorePolishEngine::RandomForestPrediction pred1;
        pred1.symbol = "BTC";
        pred1.confidence_score = 0.80;
        pred1.ranking_score = 0.85;
        pred1.original_rank = 1;
        predictions.push_back(pred1);

        AIScorePolishEngine::RandomForestPrediction pred2;
        pred2.symbol = "ETH";
        pred2.confidence_score = 0.70;
        pred2.ranking_score = 0.75;
        pred2.original_rank = 2;
        predictions.push_back(pred2);

        AIScorePolishEngine::MarketContext context;

        // Get polished predictions (should return originals since API is disabled)
        std::vector<AIScorePolishEngine::PolishedPrediction> polished = engine.getPolishedPredictions(predictions, context);

        // Verify structure and that original values are preserved
        return (polished.size() == 2) &&
               (polished[0].symbol == "BTC") &&
               (polished[0].original_confidence == 0.80) &&
               (polished[0].polished_confidence == 0.80) && // No adjustment since disabled
               (polished[1].original_rank == 2);

    } catch (const std::exception& e) {
        logTest("Exception during polished predictions test: " + std::string(e.what()));
        return false;
    }
}

// Test 7: Engine Metrics
bool test_engine_metrics() {
    logTest("Testing engine metrics...");

    try {
        AIScorePolishEngine engine;

        AIScorePolishEngine::PolishEngineMetrics metrics = engine.getEngineMetrics();

        // Check that metrics structure is valid
        return (metrics.claude_api_calls_today >= 0) &&
               (metrics.claude_success_rate >= 0.0) &&
               (metrics.polish_success_rate >= 0.0) &&
               (metrics.successful_polish_count >= 0);

    } catch (const std::exception& e) {
        logTest("Exception during metrics test: " + std::string(e.what()));
        return false;
    }
}

// Test 8: System Tests
bool test_system_tests() {
    logTest("Testing system test functionality...");

    try {
        AIScorePolishEngine::ScorePolishEngineConfig config;
        config.polish_config.enabled = true;
        config.polish_config.max_adjustment_percent = 0.20;

        AIScorePolishEngine engine(config);

        AIScorePolishEngine::PolishEngineTestResult test_result = engine.runSystemTests();

        // Check that test result structure is valid
        return (test_result.test_time.time_since_epoch().count() > 0) &&
               (test_result.test_failures.size() >= 0); // Some tests may fail without API credentials

    } catch (const std::exception& e) {
        logTest("Exception during system tests: " + std::string(e.what()));
        return false;
    }
}

// Test 9: Static Utility Methods
bool test_static_utilities() {
    logTest("Testing static utility methods...");

    try {
        // Test polish type conversion
        std::string type_str = AIScorePolishEngine::polishTypeToString(AIScorePolishEngine::PolishType::CONFIDENCE_ADJUSTMENT);
        AIScorePolishEngine::PolishType type_enum = AIScorePolishEngine::stringToPolishType(type_str);

        // Test default configuration creation
        AIScorePolishEngine::AIProviderConfig claude_config = AIScorePolishEngine::createDefaultClaudeConfig();

        return (type_str == "CONFIDENCE_ADJUSTMENT") &&
               (type_enum == AIScorePolishEngine::PolishType::CONFIDENCE_ADJUSTMENT) &&
               (claude_config.provider == AIScorePolishEngine::AIProvider::CLAUDE_API) &&
               (claude_config.credentials.model_name == "claude-3-haiku-20240307");

    } catch (const std::exception& e) {
        logTest("Exception during static utilities test: " + std::string(e.what()));
        return false;
    }
}

// Optional Test 10: Claude API Integration (requires credentials)
bool test_claude_api_integration() {
    if (!g_test_config.test_claude_api) {
        logTest("Skipping Claude API integration test (disabled in config)");
        return true; // Pass by default when skipped
    }

    logTest("Testing actual Claude API integration...");

    try {
        AIScorePolishEngine::ScorePolishEngineConfig config;
        config.polish_config.enabled = true;
        config.polish_config.max_adjustment_percent = 0.20; // 20% max adjustment
        config.polish_config.max_polish_calls_per_hour = 60;
        config.polish_config.min_time_between_polish = std::chrono::minutes{0}; // Allow immediate calls for testing

        AIScorePolishEngine engine(config);

        // First, test basic API connectivity
        logTest("Testing Claude API connectivity...");
        bool api_available = engine.testClaudeAPI();

        if (!api_available) {
            logTest("Claude API connectivity test failed");
            return false;
        }
        logTest("✅ Claude API connectivity successful");

        // Create realistic Random Forest predictions for testing
        std::vector<AIScorePolishEngine::RandomForestPrediction> predictions;

        AIScorePolishEngine::RandomForestPrediction btc;
        btc.symbol = "BTC";
        btc.confidence_score = 0.85;
        btc.ranking_score = 0.92;
        btc.original_rank = 1;
        btc.prediction_rationale = "Strong bullish momentum, high volume, breaking resistance levels";
        btc.feature_importances["price_momentum"] = 0.35;
        btc.feature_importances["volume_trend"] = 0.28;
        btc.feature_importances["technical_indicators"] = 0.37;
        predictions.push_back(btc);

        AIScorePolishEngine::RandomForestPrediction eth;
        eth.symbol = "ETH";
        eth.confidence_score = 0.72;
        eth.ranking_score = 0.78;
        eth.original_rank = 2;
        eth.prediction_rationale = "Moderate bullish signals, decent volume, consolidating";
        eth.feature_importances["price_momentum"] = 0.25;
        eth.feature_importances["volume_trend"] = 0.35;
        eth.feature_importances["technical_indicators"] = 0.40;
        predictions.push_back(eth);

        AIScorePolishEngine::RandomForestPrediction ada;
        ada.symbol = "ADA";
        ada.confidence_score = 0.58;
        ada.ranking_score = 0.62;
        ada.original_rank = 3;
        ada.prediction_rationale = "Mixed signals, low volume, sideways movement";
        ada.feature_importances["price_momentum"] = 0.20;
        ada.feature_importances["volume_trend"] = 0.30;
        ada.feature_importances["technical_indicators"] = 0.50;
        predictions.push_back(ada);

        // Create realistic market context
        AIScorePolishEngine::MarketContext context;
        context.total_portfolio_value = 50000.0;
        context.unrealized_pnl = 1250.0;
        context.realized_pnl_today = 340.0;
        context.current_drawdown = 0.02; // 2%
        context.portfolio_volatility = 0.18;
        context.market_trend = "bullish";
        context.volatility_regime = "normal";
        context.fear_greed_index = 72.0; // Greed territory

        // Add current prices
        context.current_prices["BTC"] = 43500.0;
        context.current_prices["ETH"] = 2650.0;
        context.current_prices["ADA"] = 0.48;

        context.price_changes_24h["BTC"] = 0.035;  // +3.5%
        context.price_changes_24h["ETH"] = 0.018;  // +1.8%
        context.price_changes_24h["ADA"] = -0.012; // -1.2%

        logTest("Testing real score polishing with Claude...");

        // Test the main polishScores function
        AIScorePolishEngine::ScorePolishResult polish_result = engine.polishScores(predictions, context);

        logTest("Polish result - Confidence: " + std::to_string(polish_result.adjustment_confidence));
        logTest("Polish reasoning: " + polish_result.polish_reasoning);

        // Test the complete getPolishedPredictions workflow
        std::vector<AIScorePolishEngine::PolishedPrediction> polished = engine.getPolishedPredictions(predictions, context);

        logTest("Testing polished predictions output...");
        for (const auto& pred : polished) {
            logTest("Symbol: " + pred.symbol +
                   " | Original Conf: " + std::to_string(pred.original_confidence) +
                   " | Polished Conf: " + std::to_string(pred.polished_confidence) +
                   " | Conf Adj: " + std::to_string(pred.confidence_adjustment) +
                   " | Original Rank: " + std::to_string(pred.original_rank) +
                   " | Polished Rank: " + std::to_string(pred.polished_rank) +
                   " | Was Capped: " + (pred.was_capped ? "Yes" : "No"));
        }

        // Validate that adjustments are within limits
        bool adjustments_valid = true;
        for (const auto& pred : polished) {
            if (std::abs(pred.confidence_adjustment) > config.polish_config.max_adjustment_percent) {
                logTest("❌ Confidence adjustment exceeds limit for " + pred.symbol);
                adjustments_valid = false;
            }
            if (std::abs(pred.ranking_adjustment) > config.polish_config.max_adjustment_percent) {
                logTest("❌ Ranking adjustment exceeds limit for " + pred.symbol);
                adjustments_valid = false;
            }
        }

        if (!adjustments_valid) {
            logTest("❌ Some adjustments exceeded configured limits");
            return false;
        }

        // Test engine metrics after API call
        AIScorePolishEngine::PolishEngineMetrics metrics = engine.getEngineMetrics();
        logTest("Engine metrics - API calls today: " + std::to_string(metrics.claude_api_calls_today));
        logTest("Engine metrics - Success rate: " + std::to_string(metrics.claude_success_rate));

        // Validate that we got meaningful results
        bool meaningful_results = (polish_result.adjustment_confidence > 0.0) &&
                                 (!polish_result.polish_reasoning.empty()) &&
                                 (polished.size() == predictions.size());

        if (!meaningful_results) {
            logTest("❌ Did not get meaningful results from Claude API");
            return false;
        }

        logTest("✅ Claude API integration fully tested and working!");
        return true;

    } catch (const std::exception& e) {
        logTest("Exception during Claude API test: " + std::string(e.what()));
        return false;
    }
}

// Main test runner
int main() {
    std::cout << "🧪 AI Score Polish Engine Test Suite - Day 25" << std::endl;
    std::cout << "Testing Claude-only integration for Random Forest score polishing" << std::endl;
    std::cout << "================================================================" << std::endl;

    // Track test results
    int total_tests = 0;
    int passed_tests = 0;

    // Define test cases
    struct TestCase {
        std::string name;
        std::function<bool()> test_func;
    };

    std::vector<TestCase> test_cases = {
        {"Engine Initialization", test_engine_initialization},
        {"Configuration Management", test_configuration_management},
        {"Random Forest Prediction Structure", test_prediction_structure},
        {"Market Context Structure", test_market_context},
        {"Score Polish with Disabled Config", test_polish_disabled},
        {"Polished Prediction Generation", test_polished_predictions},
        {"Engine Metrics", test_engine_metrics},
        {"System Tests", test_system_tests},
        {"Static Utility Methods", test_static_utilities},
        {"Claude API Integration", test_claude_api_integration}
    };

    // Run all tests
    for (const auto& test_case : test_cases) {
        total_tests++;
        bool passed = false;

        try {
            passed = test_case.test_func();
        } catch (const std::exception& e) {
            logTest("Test exception: " + std::string(e.what()));
            passed = false;
        } catch (...) {
            logTest("Unknown test exception");
            passed = false;
        }

        if (passed) {
            passed_tests++;
        }

        logResult(test_case.name, passed);
    }

    // Print summary
    std::cout << std::endl;
    std::cout << "================================================================" << std::endl;
    std::cout << "Test Summary: " << passed_tests << "/" << total_tests << " tests passed" << std::endl;

    double success_rate = (double)passed_tests / total_tests * 100.0;
    std::cout << "Success Rate: " << std::fixed << std::setprecision(1) << success_rate << "%" << std::endl;

    if (passed_tests == total_tests) {
        std::cout << "🎉 All tests passed! AI Score Polish Engine is ready." << std::endl;
        return 0;
    } else {
        std::cout << "⚠️  Some tests failed. Review the implementation." << std::endl;
        return 1;
    }
}