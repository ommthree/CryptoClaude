#include "AlgorithmDecisionEngine.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <stdexcept>

namespace CryptoClaude {
namespace Algorithm {

AlgorithmDecisionEngine::AlgorithmDecisionEngine(const DecisionConfig& config)
    : config_(config) {

    // Initialize performance tracking
    factor_performance_["technical"] = 0.0;
    factor_performance_["sentiment"] = 0.0;
    factor_performance_["momentum"] = 0.0;
    factor_performance_["mean_reversion"] = 0.0;
    factor_performance_["volatility"] = 0.0;
}

bool AlgorithmDecisionEngine::initialize() {
    try {
        // Initialize signal generator
        RealMarketSignalGenerator::RealSignalConfig signal_config;
        signal_config.min_confidence = config_.min_signal_confidence;
        signal_config.min_historical_days = config_.min_historical_periods;

        signal_generator_ = std::make_unique<RealMarketSignalGenerator>(signal_config);

        // Initialize data processor
        RealTimeMarketDataProcessor::ProcessorConfig processor_config;
        processor_config.data_quality_threshold = config_.min_data_quality;
        processor_config.min_historical_periods = config_.min_historical_periods;

        data_processor_ = std::make_unique<RealTimeMarketDataProcessor>(processor_config);

        // Initialize data processor
        if (!data_processor_->initialize()) {
            return false;
        }

        // Start data processing
        if (!data_processor_->start()) {
            return false;
        }

        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

bool AlgorithmDecisionEngine::isInitialized() const {
    return signal_generator_ != nullptr &&
           data_processor_ != nullptr &&
           data_processor_->isRunning();
}

AlgorithmDecisionEngine::TradingDecision
AlgorithmDecisionEngine::makeDecision(const std::string& pair) {

    TradingDecision decision;
    decision.pair_name = pair;
    decision.decision_time = std::chrono::system_clock::now();

    try {
        // Check if we should make a decision for this pair (avoid over-trading)
        auto last_decision_it = last_decision_times_.find(pair);
        if (last_decision_it != last_decision_times_.end()) {
            auto time_since_last = std::chrono::duration_cast<std::chrono::hours>(
                decision.decision_time - last_decision_it->second);

            if (time_since_last.count() < 1) { // Minimum 1 hour between decisions
                decision.action = TradingDecision::Action::NO_ACTION;
                decision.decision_reasons.push_back("Too soon since last decision");
                return decision;
            }
        }

        // Get real market data for actual market signals processing
        auto market_data = data_processor_->getAlgorithmMarketData(pair);

        // Validate data quality
        if (!data_processor_->validateDataForSignalGeneration(pair)) {
            decision.action = TradingDecision::Action::NO_ACTION;
            decision.decision_reasons.push_back("Insufficient data quality");
            decision.confidence_score = 0.0;
            return decision;
        }

        // Generate trading signal
        auto signal = signal_generator_->generateRealPairSignal(pair);

        // Validate signal quality
        if (signal.confidence_score < config_.min_signal_confidence) {
            decision.action = TradingDecision::Action::NO_ACTION;
            decision.decision_reasons.push_back("Signal confidence below threshold");
            decision.confidence_score = signal.confidence_score;
            return decision;
        }

        // Apply risk adjustment to signal
        decision.signal_strength = signal.signal_strength;
        decision.risk_adjusted_strength = applyRiskAdjustment(signal.signal_strength, market_data);

        // Calculate position risk
        double position_size = calculateRecommendedWeight(decision.risk_adjusted_strength,
                                                         signal.risk_score, current_portfolio_);
        decision.position_risk_score = calculatePositionRisk(pair, position_size, market_data);

        // Calculate correlation risk with existing portfolio
        decision.correlation_risk = calculateCorrelationRisk(pair, current_portfolio_);

        // Apply correlation filtering if enabled
        if (config_.enable_correlation_filtering && decision.correlation_risk > config_.correlation_threshold) {
            decision.recommended_weight *= (1.0 - decision.correlation_risk);
            decision.decision_reasons.push_back("Position size reduced due to correlation risk");
        }

        // Determine action based on risk-adjusted signal
        decision.action = determineAction(decision.risk_adjusted_strength, signal.confidence_score);

        // Calculate recommended weight
        decision.recommended_weight = calculateRecommendedWeight(decision.risk_adjusted_strength,
                                                               decision.position_risk_score,
                                                               current_portfolio_);

        // Set other decision attributes
        decision.confidence_score = signal.confidence_score * (1.0 - decision.position_risk_score * 0.5);
        decision.expected_return = signal.predicted_return;
        decision.expected_volatility = signal.predicted_volatility;

        // Market regime adaptation
        if (config_.enable_regime_adaptation) {
            auto regime = data_processor_->detectMarketRegime(pair);
            adaptToMarketRegime(decision, regime);
        }

        // Generate decision rationale
        generateDecisionRationale(decision, signal, market_data);

        // Calculate portfolio impact
        decision.portfolio_impact_score = calculatePortfolioImpact(decision, current_portfolio_);

        // Generate unique decision ID
        decision.decision_id = pair + "_" + std::to_string(
            std::chrono::duration_cast<std::chrono::seconds>(
                decision.decision_time.time_since_epoch()).count());

        // Validate decision
        if (!validateDecision(decision, current_portfolio_)) {
            decision.action = TradingDecision::Action::NO_ACTION;
            decision.decision_reasons.push_back("Decision failed validation");
        }

        // Record decision time
        last_decision_times_[pair] = decision.decision_time;

        // Store decision in history
        decision_history_.push_back(decision);

        // Keep history manageable
        if (decision_history_.size() > 1000) {
            decision_history_.erase(decision_history_.begin());
        }

    } catch (const std::exception& e) {
        decision.action = TradingDecision::Action::NO_ACTION;
        decision.decision_reasons.push_back("Error making decision: " + std::string(e.what()));
        decision.confidence_score = 0.0;
    }

    return decision;
}

std::vector<AlgorithmDecisionEngine::TradingDecision>
AlgorithmDecisionEngine::makeDecisions(const std::vector<std::string>& pairs) {

    std::vector<TradingDecision> decisions;
    decisions.reserve(pairs.size());

    for (const auto& pair : pairs) {
        auto decision = makeDecision(pair);
        decisions.push_back(decision);
    }

    return decisions;
}

AlgorithmDecisionEngine::DecisionBatch
AlgorithmDecisionEngine::makeBatchDecision(const std::vector<std::string>& pairs) {

    DecisionBatch batch;
    batch.batch_time = std::chrono::system_clock::now();
    batch.portfolio_context = current_portfolio_;

    // Generate individual decisions
    batch.decisions = makeDecisions(pairs);

    // Calculate batch-level metrics
    batch.total_recommended_exposure = 0.0;
    batch.strong_signals_count = 0;
    batch.expected_portfolio_return = 0.0;

    for (const auto& decision : batch.decisions) {
        batch.total_recommended_exposure += decision.recommended_weight;

        if (decision.action == TradingDecision::Action::STRONG_BUY ||
            decision.action == TradingDecision::Action::STRONG_SELL) {
            batch.strong_signals_count++;
        }

        batch.expected_portfolio_return += decision.expected_return * decision.recommended_weight;
    }

    // Apply risk management to the batch
    batch = applyRiskManagement(batch);

    return batch;
}

AlgorithmDecisionEngine::TradingDecision::Action
AlgorithmDecisionEngine::determineAction(double signal_strength, double confidence) {

    // Apply confidence scaling to thresholds
    double confidence_factor = std::max(0.5, confidence); // Minimum 50% confidence

    double adjusted_strong_buy = config_.strong_buy_threshold * confidence_factor;
    double adjusted_buy = config_.buy_threshold * confidence_factor;
    double adjusted_sell = config_.sell_threshold * confidence_factor;
    double adjusted_strong_sell = config_.strong_sell_threshold * confidence_factor;

    if (signal_strength >= adjusted_strong_buy) {
        return TradingDecision::Action::STRONG_BUY;
    } else if (signal_strength >= adjusted_buy) {
        return TradingDecision::Action::BUY;
    } else if (signal_strength <= adjusted_strong_sell) {
        return TradingDecision::Action::STRONG_SELL;
    } else if (signal_strength <= adjusted_sell) {
        return TradingDecision::Action::SELL;
    } else {
        return TradingDecision::Action::HOLD;
    }
}

double AlgorithmDecisionEngine::calculateRecommendedWeight(double signal_strength,
                                                          double risk_score,
                                                          const PortfolioContext& portfolio) {

    // Base position size based on signal strength
    double base_weight = std::abs(signal_strength) * config_.max_position_size;

    // Risk adjustment - reduce position size for higher risk
    double risk_adjusted_weight = base_weight * (1.0 - risk_score * 0.5);

    // Portfolio context adjustment
    double cash_constraint = portfolio.cash_available / portfolio.total_portfolio_value;
    if (risk_adjusted_weight > cash_constraint) {
        risk_adjusted_weight = cash_constraint * 0.8; // Leave some cash buffer
    }

    // Volatility scaling if enabled
    if (config_.enable_volatility_scaling) {
        double volatility_factor = 0.15 / std::max(0.05, portfolio.current_volatility);
        risk_adjusted_weight *= std::min(2.0, volatility_factor); // Cap at 2x scaling
    }

    // Apply maximum position size limit
    return std::min(risk_adjusted_weight, config_.max_position_size);
}

double AlgorithmDecisionEngine::calculatePositionRisk(const std::string& pair,
                                                     double position_size,
                                                     const RealTimeMarketDataProcessor::AlgorithmMarketData& data) {

    double risk_score = 0.0;

    // Volatility risk component (higher volatility = higher risk)
    if (data.expected_volatility > 0) {
        risk_score += std::min(0.4, data.expected_volatility / 0.3); // Cap at 40% for volatility
    }

    // Data quality risk component
    risk_score += (1.0 - data.quality.overall_quality) * 0.3;

    // Position size risk component
    risk_score += (position_size / config_.max_position_size) * 0.2;

    // Market structure risk (bid-ask spread)
    if (data.market_structure.bid_ask_spread > 0.005) { // More than 0.5% spread
        risk_score += 0.1;
    }

    return std::min(1.0, risk_score);
}

double AlgorithmDecisionEngine::calculateCorrelationRisk(const std::string& pair,
                                                        const PortfolioContext& portfolio) {

    if (portfolio.current_positions.empty()) {
        return 0.0; // No correlation risk with empty portfolio
    }

    // Simplified correlation risk calculation
    // In practice, this would calculate actual correlation with existing positions

    double max_correlation = 0.0;

    for (const auto& position : portfolio.current_positions) {
        // Parse symbols from pair names for correlation calculation
        auto delimiter_pos = pair.find('/');
        auto pos_delimiter_pos = position.pair_name.find('/');

        if (delimiter_pos != std::string::npos && pos_delimiter_pos != std::string::npos) {
            std::string pair_base = pair.substr(0, delimiter_pos);
            std::string pair_quote = pair.substr(delimiter_pos + 1);
            std::string pos_base = position.pair_name.substr(0, pos_delimiter_pos);
            std::string pos_quote = position.pair_name.substr(pos_delimiter_pos + 1);

            // Simple correlation estimation based on shared symbols
            double correlation = 0.0;
            if (pair_base == pos_base || pair_quote == pos_quote) {
                correlation = 0.7; // High correlation for shared symbols
            } else if (pair_base == pos_quote || pair_quote == pos_base) {
                correlation = 0.5; // Medium correlation for inverse pairs
            } else {
                correlation = 0.2; // Low baseline correlation
            }

            max_correlation = std::max(max_correlation, correlation * position.weight);
        }
    }

    return max_correlation;
}

double AlgorithmDecisionEngine::applyRiskAdjustment(double raw_signal,
                                                   const RealTimeMarketDataProcessor::AlgorithmMarketData& data) {

    double adjusted_signal = raw_signal;

    // Quality adjustment
    adjusted_signal *= data.quality.overall_quality;

    // Volatility adjustment
    if (data.expected_volatility > 0.2) { // High volatility
        adjusted_signal *= 0.8; // Reduce signal strength
    }

    // Data freshness adjustment
    adjusted_signal *= data.quality.freshness_score;

    // Technical confirmation adjustment
    double technical_confirmation = 1.0;
    if (data.technical.rsi_14 > 80 || data.technical.rsi_14 < 20) {
        technical_confirmation += 0.1; // Boost for extreme RSI
    }

    if (std::abs(data.technical.macd.histogram) > 0.01) {
        technical_confirmation += 0.05; // Boost for MACD divergence
    }

    adjusted_signal *= technical_confirmation;

    return std::max(-1.0, std::min(1.0, adjusted_signal)); // Clamp to [-1, 1]
}

void AlgorithmDecisionEngine::adaptToMarketRegime(TradingDecision& decision,
                                                 RealTimeMarketDataProcessor::MarketRegime regime) {

    switch (regime) {
        case RealTimeMarketDataProcessor::MarketRegime::BULL_MARKET:
            // In bull markets, bias toward buy signals
            if (decision.signal_strength > 0) {
                decision.signal_strength *= 1.1;
                decision.decision_reasons.push_back("Bull market bias applied");
            }
            break;

        case RealTimeMarketDataProcessor::MarketRegime::BEAR_MARKET:
            // In bear markets, be more cautious with buy signals
            if (decision.signal_strength > 0) {
                decision.signal_strength *= 0.8;
                decision.decision_reasons.push_back("Bear market caution applied");
            }
            break;

        case RealTimeMarketDataProcessor::MarketRegime::HIGH_VOLATILITY:
            // Reduce position sizes in high volatility
            decision.recommended_weight *= 0.7;
            decision.decision_reasons.push_back("Position size reduced for high volatility");
            break;

        case RealTimeMarketDataProcessor::MarketRegime::CRISIS_MODE:
            // In crisis mode, drastically reduce activity
            decision.recommended_weight *= 0.3;
            decision.decision_reasons.push_back("Crisis mode - minimal position size");
            break;

        default:
            break;
    }
}

void AlgorithmDecisionEngine::generateDecisionRationale(TradingDecision& decision,
                                                       const RealMarketSignalGenerator::RealTradingSignal& signal,
                                                       const RealTimeMarketDataProcessor::AlgorithmMarketData& data) {

    // Add signal-based reasons
    decision.decision_reasons.insert(decision.decision_reasons.end(),
                                   signal.signal_reasons.begin(),
                                   signal.signal_reasons.end());

    // Add technical analysis reasons
    if (data.technical.rsi_14 < 30) {
        decision.decision_reasons.push_back("RSI indicates oversold condition");
    } else if (data.technical.rsi_14 > 70) {
        decision.decision_reasons.push_back("RSI indicates overbought condition");
    }

    if (data.technical.macd.histogram > 0 && data.technical.macd.macd_line > data.technical.macd.signal_line) {
        decision.decision_reasons.push_back("MACD shows bullish momentum");
    } else if (data.technical.macd.histogram < 0 && data.technical.macd.macd_line < data.technical.macd.signal_line) {
        decision.decision_reasons.push_back("MACD shows bearish momentum");
    }

    // Add volume confirmation
    if (data.volume.volume_ratio > 1.5) {
        decision.decision_reasons.push_back("High volume confirms signal");
    } else if (data.volume.volume_ratio < 0.5) {
        decision.decision_reasons.push_back("Low volume weakens signal");
    }

    // Add risk considerations
    if (decision.position_risk_score > 0.7) {
        decision.decision_reasons.push_back("High position risk detected");
    }

    if (decision.correlation_risk > config_.correlation_threshold) {
        decision.decision_reasons.push_back("High correlation risk with existing positions");
    }

    // Populate factor contributions
    decision.factor_contributions["technical_analysis"] = 0.4;
    decision.factor_contributions["momentum"] = 0.3;
    decision.factor_contributions["volatility"] = 0.2;
    decision.factor_contributions["risk_adjustment"] = 0.1;
}

bool AlgorithmDecisionEngine::validateDecision(const TradingDecision& decision,
                                              const PortfolioContext& portfolio) const {

    // Check minimum confidence requirement
    if (decision.confidence_score < config_.min_signal_confidence) {
        return false;
    }

    // Check position size limits
    if (decision.recommended_weight > config_.max_position_size) {
        return false;
    }

    // Check available cash for buy decisions
    if (decision.action == TradingDecision::Action::BUY ||
        decision.action == TradingDecision::Action::STRONG_BUY) {

        double required_cash = decision.recommended_weight * portfolio.total_portfolio_value;
        if (required_cash > portfolio.cash_available) {
            return false;
        }
    }

    // Check maximum risk tolerance
    if (decision.position_risk_score > 0.9) {
        return false;
    }

    return true;
}

double AlgorithmDecisionEngine::calculatePortfolioImpact(const TradingDecision& decision,
                                                        const PortfolioContext& portfolio) {

    double impact_score = 0.0;

    // Size impact - larger positions have higher impact
    impact_score += decision.recommended_weight * 0.5;

    // Volatility impact
    impact_score += decision.expected_volatility * 0.3;

    // Correlation impact
    impact_score += decision.correlation_risk * 0.2;

    return std::min(1.0, impact_score);
}

AlgorithmDecisionEngine::DecisionBatch
AlgorithmDecisionEngine::applyRiskManagement(const DecisionBatch& raw_batch) {

    DecisionBatch managed_batch = raw_batch;

    // Check total exposure
    if (managed_batch.total_recommended_exposure > 0.8) { // 80% max exposure
        double scale_factor = 0.8 / managed_batch.total_recommended_exposure;

        for (auto& decision : managed_batch.decisions) {
            decision.recommended_weight *= scale_factor;
        }

        managed_batch.risk_warnings.push_back("Position sizes scaled down due to excessive exposure");
        managed_batch.total_recommended_exposure = 0.8;
    }

    // Check sector concentration
    std::map<std::string, double> sector_exposure;
    for (const auto& decision : managed_batch.decisions) {
        if (decision.recommended_weight > 0) {
            // Simplified sector mapping - would be more sophisticated in practice
            std::string sector = "crypto"; // Default sector
            sector_exposure[sector] += decision.recommended_weight;
        }
    }

    // Apply sector limits
    for (const auto& sector_pair : sector_exposure) {
        if (sector_pair.second > config_.max_sector_exposure) {
            managed_batch.risk_warnings.push_back("Sector exposure exceeds limits: " + sector_pair.first);
        }
    }

    // Final approval based on risk checks
    managed_batch.batch_approved = managed_batch.risk_warnings.size() < 3; // Max 3 warnings

    return managed_batch;
}

double AlgorithmDecisionEngine::getCurrentAlgorithmCorrelation() const {
    if (!signal_generator_) {
        return 0.0;
    }

    return signal_generator_->calculateCurrentAlgorithmCorrelation();
}

bool AlgorithmDecisionEngine::meetsTargetCorrelation(double target) const {
    return getCurrentAlgorithmCorrelation() >= target;
}

AlgorithmDecisionEngine::EngineStatus AlgorithmDecisionEngine::getEngineStatus() const {
    EngineStatus status;

    status.is_operational = isInitialized();
    status.decisions_made_today = static_cast<int>(decision_history_.size()); // Simplified
    status.current_algorithm_correlation = getCurrentAlgorithmCorrelation();

    if (!decision_history_.empty()) {
        status.last_decision_time = decision_history_.back().decision_time;
    }

    // Component health checks
    status.signal_generator_healthy = (signal_generator_ != nullptr);
    status.data_processor_healthy = (data_processor_ != nullptr && data_processor_->isRunning());

    if (data_processor_) {
        auto processing_status = data_processor_->getProcessingStatus();
        status.data_quality_average = processing_status.overall_data_quality;
    }

    return status;
}

void AlgorithmDecisionEngine::updatePortfolioContext(const PortfolioContext& portfolio) {
    current_portfolio_ = portfolio;
}

// Factory implementations
std::unique_ptr<AlgorithmDecisionEngine> DecisionEngineFactory::createStandard() {
    AlgorithmDecisionEngine::DecisionConfig config;
    return std::make_unique<AlgorithmDecisionEngine>(config);
}

std::unique_ptr<AlgorithmDecisionEngine> DecisionEngineFactory::createConservative() {
    AlgorithmDecisionEngine::DecisionConfig config;
    config.min_signal_confidence = 0.8;        // Higher confidence required
    config.max_position_size = 0.1;            // Smaller positions
    config.strong_buy_threshold = 0.9;         // Higher threshold for strong signals
    config.buy_threshold = 0.75;               // Higher threshold for buy signals
    config.enable_volatility_scaling = true;   // Scale by volatility

    return std::make_unique<AlgorithmDecisionEngine>(config);
}

std::unique_ptr<AlgorithmDecisionEngine> DecisionEngineFactory::createAggressive() {
    AlgorithmDecisionEngine::DecisionConfig config;
    config.min_signal_confidence = 0.6;        // Lower confidence required
    config.max_position_size = 0.25;           // Larger positions
    config.strong_buy_threshold = 0.7;         // Lower threshold for strong signals
    config.buy_threshold = 0.5;                // Lower threshold for buy signals

    return std::make_unique<AlgorithmDecisionEngine>(config);
}

} // namespace Algorithm
} // namespace CryptoClaude