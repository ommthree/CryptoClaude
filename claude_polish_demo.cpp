/**
 * Claude Score Polish Demo - Real API Integration Test
 * Demonstrates actual score polishing with Claude API
 */

#include <iostream>
#include "src/Core/AI/AIDecisionEngine.h"

using namespace CryptoClaude::AI;

int main() {
    std::cout << "🚀 Claude AI Score Polish Engine - Live Demo\n";
    std::cout << "============================================\n\n";

    try {
        // Configure for real API testing
        AIScorePolishEngine::ScorePolishEngineConfig config;
        config.polish_config.enabled = true;
        config.polish_config.max_adjustment_percent = 0.20; // 20% max adjustment
        config.polish_config.min_time_between_polish = std::chrono::minutes{0}; // No delay for demo

        AIScorePolishEngine engine(config);

        // Create realistic Random Forest predictions
        std::vector<AIScorePolishEngine::RandomForestPrediction> predictions;

        AIScorePolishEngine::RandomForestPrediction btc;
        btc.symbol = "BTC";
        btc.confidence_score = 0.82;
        btc.ranking_score = 0.89;
        btc.original_rank = 1;
        btc.prediction_rationale = "Strong bullish momentum with high volume breakout";
        predictions.push_back(btc);

        AIScorePolishEngine::RandomForestPrediction eth;
        eth.symbol = "ETH";
        eth.confidence_score = 0.68;
        eth.ranking_score = 0.71;
        eth.original_rank = 2;
        eth.prediction_rationale = "Moderate signals, awaiting breakout confirmation";
        predictions.push_back(eth);

        AIScorePolishEngine::RandomForestPrediction sol;
        sol.symbol = "SOL";
        sol.confidence_score = 0.55;
        sol.ranking_score = 0.60;
        sol.original_rank = 3;
        sol.prediction_rationale = "Mixed signals, high volatility concern";
        predictions.push_back(sol);

        // Create market context
        AIScorePolishEngine::MarketContext context;
        context.total_portfolio_value = 75000.0;
        context.unrealized_pnl = 2100.0;
        context.market_trend = "bullish";
        context.volatility_regime = "elevated";
        context.fear_greed_index = 78.0; // Extreme greed

        context.current_prices["BTC"] = 44200.0;
        context.current_prices["ETH"] = 2720.0;
        context.current_prices["SOL"] = 98.50;

        std::cout << "📊 Original Random Forest Predictions:\n";
        for (const auto& pred : predictions) {
            std::cout << "   " << pred.symbol
                     << " - Confidence: " << std::fixed << std::setprecision(3) << pred.confidence_score
                     << " | Ranking: " << pred.ranking_score
                     << " | Rank: #" << pred.original_rank << "\n";
        }
        std::cout << "\n📈 Market Context: " << context.market_trend
                  << " trend, " << context.volatility_regime << " volatility, "
                  << "Fear/Greed: " << context.fear_greed_index << "\n\n";

        std::cout << "🤖 Consulting Claude for score polishing...\n";

        // Get Claude's analysis and polished predictions
        std::vector<AIScorePolishEngine::PolishedPrediction> polished =
            engine.getPolishedPredictions(predictions, context);

        std::cout << "\n✨ Claude-Polished Results:\n";
        std::cout << "Symbol | Original Conf → Polished | Original Rank → Polished | Adjustment | Capped\n";
        std::cout << "-------|-------------------------|--------------------------|------------|---------\n";

        for (const auto& pred : polished) {
            std::cout << std::setw(6) << pred.symbol << " | "
                     << std::fixed << std::setprecision(3)
                     << std::setw(5) << pred.original_confidence << " → " << std::setw(5) << pred.polished_confidence << "    | "
                     << std::setw(5) << pred.original_rank << "    → " << std::setw(5) << pred.polished_rank << "       | "
                     << std::showpos << std::setw(6) << pred.confidence_adjustment << std::noshowpos << "     | "
                     << (pred.was_capped ? "Yes" : "No") << "\n";
        }

        // Get engine metrics
        AIScorePolishEngine::PolishEngineMetrics metrics = engine.getEngineMetrics();

        std::cout << "\n📈 Performance Metrics:\n";
        std::cout << "   API Calls Today: " << metrics.claude_api_calls_today << "\n";
        std::cout << "   Success Rate: " << std::fixed << std::setprecision(1)
                  << (metrics.claude_success_rate * 100) << "%\n";
        std::cout << "   Avg Claude Confidence: " << std::setprecision(3)
                  << metrics.average_claude_confidence << "\n";
        std::cout << "   Predictions Polished: " << metrics.predictions_polished_count << "\n";

        std::cout << "\n🎉 Claude AI Score Polish Demo Complete!\n";
        std::cout << "The AI Score Polish Engine successfully refined Random Forest predictions\n";
        std::cout << "within the configured 20% adjustment limits.\n";

        return 0;

    } catch (const std::exception& e) {
        std::cout << "❌ Demo failed: " << e.what() << std::endl;
        return 1;
    }
}