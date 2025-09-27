#include "TradingStrategyOptimizer.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <random>
#include <fstream>
#include <iomanip>
#include <thread>

namespace CryptoClaude {
namespace Strategy {

TradingStrategyOptimizer::TradingStrategyOptimizer(const OptimizerConfig& config)
    : config_(config) {
    // Initialize random seed
    std::srand(static_cast<unsigned>(std::time(nullptr)));
}

TradingStrategyOptimizer::~TradingStrategyOptimizer() {
    stopOptimization();
}

void TradingStrategyOptimizer::updateConfig(const OptimizerConfig& config) {
    std::lock_guard<std::mutex> lock(optimization_mutex_);
    config_ = config;
}

bool TradingStrategyOptimizer::integrateModelManager(std::unique_ptr<ML::ModelManager> model_manager) {
    if (!model_manager) return false;

    std::lock_guard<std::mutex> lock(optimization_mutex_);
    model_manager_ = std::move(model_manager);
    return true;
}

bool TradingStrategyOptimizer::integratePerformanceEngine(std::unique_ptr<Analytics::AdvancedPerformanceEngine> performance_engine) {
    if (!performance_engine) return false;

    std::lock_guard<std::mutex> lock(optimization_mutex_);
    performance_engine_ = std::move(performance_engine);
    return true;
}

bool TradingStrategyOptimizer::integrateRiskManager(std::unique_ptr<Risk::ProductionRiskManager> risk_manager) {
    if (!risk_manager) return false;

    std::lock_guard<std::mutex> lock(optimization_mutex_);
    risk_manager_ = std::move(risk_manager);
    return true;
}

bool TradingStrategyOptimizer::addStrategy(const StrategyConfig& config) {
    std::lock_guard<std::mutex> lock(optimization_mutex_);
    strategy_configs_[config.strategy_id] = config;
    return true;
}

bool TradingStrategyOptimizer::updateStrategy(const std::string& strategy_id, const StrategyConfig& config) {
    std::lock_guard<std::mutex> lock(optimization_mutex_);
    if (strategy_configs_.find(strategy_id) == strategy_configs_.end()) {
        return false;
    }

    strategy_configs_[strategy_id] = config;
    return true;
}

bool TradingStrategyOptimizer::removeStrategy(const std::string& strategy_id) {
    std::lock_guard<std::mutex> lock(optimization_mutex_);
    strategy_configs_.erase(strategy_id);
    latest_results_.erase(strategy_id);
    optimization_history_.erase(strategy_id);
    ml_suggestions_.erase(strategy_id);
    return true;
}

std::vector<std::string> TradingStrategyOptimizer::getStrategyIds() const {
    std::lock_guard<std::mutex> lock(optimization_mutex_);
    std::vector<std::string> ids;

    for (const auto& [id, config] : strategy_configs_) {
        ids.push_back(id);
    }

    return ids;
}

TradingStrategyOptimizer::StrategyConfig TradingStrategyOptimizer::getStrategyConfig(const std::string& strategy_id) const {
    std::lock_guard<std::mutex> lock(optimization_mutex_);
    auto it = strategy_configs_.find(strategy_id);
    if (it != strategy_configs_.end()) {
        return it->second;
    }
    return StrategyConfig{};
}

TradingStrategyOptimizer::OptimizationResult TradingStrategyOptimizer::optimizeStrategy(
    const std::string& strategy_id, OptimizationMethod method) {

    auto start_time = std::chrono::high_resolution_clock::now();
    is_optimizing_.store(true);

    OptimizationResult result;
    result.strategy_id = strategy_id;
    result.optimization_time = std::chrono::system_clock::now();
    result.optimization_method = optimizationMethodToString(method);

    try {
        // Get strategy configuration
        std::lock_guard<std::mutex> lock(optimization_mutex_);
        auto config_it = strategy_configs_.find(strategy_id);
        if (config_it == strategy_configs_.end()) {
            return result; // Return empty result if strategy not found
        }

        StrategyConfig config = config_it->second;
        lock.~lock_guard();

        // Store original parameters
        for (const auto& [param_name, param_def] : config.parameters) {
            result.original_parameters[param_name] = param_def.current_value;
        }

        // Run optimization based on method
        switch (method) {
            case OptimizationMethod::GRID_SEARCH:
                result = runGridSearch(config);
                break;
            case OptimizationMethod::RANDOM_SEARCH:
                result = runRandomSearch(config);
                break;
            case OptimizationMethod::GENETIC_ALGORITHM:
                result = runGeneticAlgorithm(config);
                break;
            case OptimizationMethod::BAYESIAN_OPTIMIZATION:
                result = runBayesianOptimization(config);
                break;
            case OptimizationMethod::PARTICLE_SWARM:
                result = runParticleSwarm(config);
                break;
            case OptimizationMethod::SIMULATED_ANNEALING:
                result = runSimulatedAnnealing(config);
                break;
            case OptimizationMethod::ML_GUIDED:
                result = runMLGuidedOptimization(config);
                break;
        }

        // Calculate improvements
        auto original_performance = evaluateParameterSet(config, result.original_parameters);
        result.sharpe_improvement = result.optimal_sharpe_ratio - original_performance;
        result.return_improvement = result.optimal_return - (original_performance * 0.8); // Rough estimation
        result.drawdown_improvement = original_performance * 0.1 - result.optimal_max_drawdown; // Rough estimation

        // Statistical significance test
        result.is_statistically_significant = isStatisticallySignificant(result);
        result.confidence_level = 0.95; // Default confidence level

        // Store result
        {
            std::lock_guard<std::mutex> results_lock(results_mutex_);
            latest_results_[strategy_id] = result;
            optimization_history_[strategy_id].push_back(result);
        }

        // Trigger callback
        if (optimization_complete_callback_) {
            optimization_complete_callback_(result);
        }

    } catch (const std::exception& e) {
        // Log error and return partial result
        result.risk_assessment = "Optimization failed: " + std::string(e.what());
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    result.optimization_duration = std::chrono::duration_cast<std::chrono::minutes>(end_time - start_time);

    is_optimizing_.store(false);
    return result;
}

TradingStrategyOptimizer::OptimizationResult TradingStrategyOptimizer::runBayesianOptimization(const StrategyConfig& config) {
    OptimizationResult result;
    result.strategy_id = config.strategy_id;
    result.optimization_method = "Bayesian Optimization";

    // Simplified Bayesian optimization implementation
    std::random_device rd;
    std::mt19937 gen(rd());

    std::vector<std::map<std::string, double>> evaluated_points;
    std::vector<double> evaluated_scores;

    double best_score = -std::numeric_limits<double>::infinity();
    std::map<std::string, double> best_parameters;

    // Initial random sampling phase
    int initial_samples = std::min(50, config_.max_evaluations / 4);
    for (int i = 0; i < initial_samples; ++i) {
        auto parameters = generateRandomParameters(config);
        if (!isValidParameterSet(config, parameters)) continue;

        double score = evaluateParameterSet(config, parameters);

        evaluated_points.push_back(parameters);
        evaluated_scores.push_back(score);

        if (score > best_score) {
            best_score = score;
            best_parameters = parameters;
        }

        result.evaluations_performed++;

        // Progress callback
        if (progress_callback_) {
            double progress = static_cast<double>(i) / initial_samples * 0.3; // 30% for initial phase
            progress_callback_(config.strategy_id, progress);
        }
    }

    // Bayesian optimization phase (simplified)
    int remaining_evaluations = config_.max_evaluations - initial_samples;
    for (int iter = 0; iter < remaining_evaluations && iter < config_.max_iterations; ++iter) {
        // Select next point using acquisition function (simplified as random with bias toward promising regions)
        auto candidate_parameters = generateRandomParameters(config);

        // Add exploration bias toward best parameters
        for (auto& [param_name, value] : candidate_parameters) {
            if (best_parameters.count(param_name)) {
                double best_value = best_parameters[param_name];
                double exploration_factor = 0.1; // 10% exploration
                std::normal_distribution<> noise(0, exploration_factor);
                value = best_value + noise(gen);

                // Clamp to valid range
                auto param_def = config.parameters.at(param_name);
                value = std::max(param_def.min_value, std::min(param_def.max_value, value));
            }
        }

        if (!isValidParameterSet(config, candidate_parameters)) continue;

        double score = evaluateParameterSet(config, candidate_parameters);

        evaluated_points.push_back(candidate_parameters);
        evaluated_scores.push_back(score);

        if (score > best_score) {
            best_score = score;
            best_parameters = candidate_parameters;
        }

        result.evaluations_performed++;
        result.iterations_performed++;

        // Progress callback
        if (progress_callback_) {
            double progress = 0.3 + (static_cast<double>(iter) / remaining_evaluations) * 0.7;
            progress_callback_(config.strategy_id, progress);
        }

        // Convergence check
        if (iter > 50 && iter % 10 == 0) {
            // Check if we've improved significantly in the last 20 iterations
            double recent_improvement = 0.0;
            if (evaluated_scores.size() > 20) {
                auto recent_scores = std::vector<double>(evaluated_scores.end() - 20, evaluated_scores.end());
                double recent_max = *std::max_element(recent_scores.begin(), recent_scores.end());
                double earlier_max = *std::max_element(evaluated_scores.begin(), evaluated_scores.end() - 20);
                recent_improvement = (recent_max - earlier_max) / std::abs(earlier_max);
            }

            if (recent_improvement < config_.convergence_threshold) {
                result.convergence_score = 1.0 - recent_improvement;
                break;
            }
        }
    }

    // Set optimal parameters
    result.optimal_parameters = best_parameters;
    result.optimal_sharpe_ratio = best_score;

    // Estimate other performance metrics (simplified)
    result.optimal_return = best_score * 0.15; // Rough estimation based on Sharpe ratio
    result.optimal_volatility = result.optimal_return / best_score;
    result.optimal_max_drawdown = std::min(0.25, result.optimal_volatility * 2);
    result.optimal_profit_factor = 1.0 + best_score * 0.5;
    result.optimal_win_rate = 50.0 + best_score * 10; // 50-70% win rate estimation

    // Risk constraint checks
    result.meets_risk_constraints = (result.optimal_max_drawdown <= config.target_max_drawdown &&
                                   result.optimal_volatility <= config.target_volatility);
    result.meets_return_targets = (result.optimal_return >= config.min_return_threshold);

    if (!result.meets_risk_constraints) {
        result.constraint_violations.push_back("Risk constraints violated");
    }
    if (!result.meets_return_targets) {
        result.constraint_violations.push_back("Return targets not met");
    }

    return result;
}

TradingStrategyOptimizer::OptimizationResult TradingStrategyOptimizer::runGeneticAlgorithm(const StrategyConfig& config) {
    OptimizationResult result;
    result.strategy_id = config.strategy_id;
    result.optimization_method = "Genetic Algorithm";

    std::random_device rd;
    std::mt19937 gen(rd());

    // Initialize population
    std::vector<std::map<std::string, double>> population;
    std::vector<double> fitness_scores;

    for (int i = 0; i < config_.population_size; ++i) {
        auto individual = generateRandomParameters(config);
        population.push_back(individual);

        double fitness = evaluateParameterSet(config, individual);
        fitness_scores.push_back(fitness);
        result.evaluations_performed++;
    }

    double best_fitness = *std::max_element(fitness_scores.begin(), fitness_scores.end());
    auto best_individual = population[std::distance(fitness_scores.begin(),
                                                   std::max_element(fitness_scores.begin(), fitness_scores.end()))];

    // Evolution loop
    for (int generation = 0; generation < config_.max_iterations; ++generation) {
        std::vector<std::map<std::string, double>> new_population;
        std::vector<double> new_fitness_scores;

        // Elite selection - keep best individuals
        std::vector<size_t> elite_indices(fitness_scores.size());
        std::iota(elite_indices.begin(), elite_indices.end(), 0);
        std::partial_sort(elite_indices.begin(), elite_indices.begin() + config_.elite_size,
                         elite_indices.end(),
                         [&](size_t a, size_t b) { return fitness_scores[a] > fitness_scores[b]; });

        // Add elite individuals
        for (int i = 0; i < config_.elite_size; ++i) {
            new_population.push_back(population[elite_indices[i]]);
            new_fitness_scores.push_back(fitness_scores[elite_indices[i]]);
        }

        // Generate offspring through crossover and mutation
        std::uniform_real_distribution<> rand_prob(0.0, 1.0);
        while (new_population.size() < config_.population_size) {
            // Tournament selection for parents
            auto parent1 = population[tournamentSelection(fitness_scores, 3, gen)];
            auto parent2 = population[tournamentSelection(fitness_scores, 3, gen)];

            // Crossover
            std::map<std::string, double> offspring;
            if (rand_prob(gen) < config_.crossover_rate) {
                offspring = crossover(parent1, parent2, gen);
            } else {
                offspring = (rand_prob(gen) < 0.5) ? parent1 : parent2;
            }

            // Mutation
            if (rand_prob(gen) < config_.mutation_rate) {
                offspring = mutateParameters(offspring, config, config_.mutation_rate);
            }

            if (isValidParameterSet(config, offspring)) {
                double fitness = evaluateParameterSet(config, offspring);
                new_population.push_back(offspring);
                new_fitness_scores.push_back(fitness);
                result.evaluations_performed++;

                if (fitness > best_fitness) {
                    best_fitness = fitness;
                    best_individual = offspring;
                }
            }
        }

        population = new_population;
        fitness_scores = new_fitness_scores;
        result.iterations_performed++;

        // Progress callback
        if (progress_callback_) {
            double progress = static_cast<double>(generation) / config_.max_iterations;
            progress_callback_(config.strategy_id, progress);
        }

        // Check for early termination
        if (result.evaluations_performed >= config_.max_evaluations) {
            break;
        }
    }

    // Set results
    result.optimal_parameters = best_individual;
    result.optimal_sharpe_ratio = best_fitness;

    // Estimate other metrics
    result.optimal_return = best_fitness * 0.15;
    result.optimal_volatility = result.optimal_return / best_fitness;
    result.optimal_max_drawdown = std::min(0.25, result.optimal_volatility * 2);
    result.optimal_profit_factor = 1.0 + best_fitness * 0.5;
    result.optimal_win_rate = 50.0 + best_fitness * 10;

    return result;
}

// Utility methods for genetic algorithm
size_t TradingStrategyOptimizer::tournamentSelection(const std::vector<double>& fitness_scores,
                                                    int tournament_size, std::mt19937& gen) {
    std::uniform_int_distribution<> dist(0, fitness_scores.size() - 1);

    size_t best_idx = dist(gen);
    double best_fitness = fitness_scores[best_idx];

    for (int i = 1; i < tournament_size; ++i) {
        size_t idx = dist(gen);
        if (fitness_scores[idx] > best_fitness) {
            best_fitness = fitness_scores[idx];
            best_idx = idx;
        }
    }

    return best_idx;
}

std::map<std::string, double> TradingStrategyOptimizer::crossover(
    const std::map<std::string, double>& parent1,
    const std::map<std::string, double>& parent2,
    std::mt19937& gen) {

    std::map<std::string, double> offspring;
    std::uniform_real_distribution<> rand_prob(0.0, 1.0);

    for (const auto& [param_name, value1] : parent1) {
        if (parent2.count(param_name)) {
            double value2 = parent2.at(param_name);
            // Uniform crossover
            offspring[param_name] = (rand_prob(gen) < 0.5) ? value1 : value2;
        } else {
            offspring[param_name] = value1;
        }
    }

    return offspring;
}

double TradingStrategyOptimizer::evaluateParameterSet(const StrategyConfig& config,
                                                     const std::map<std::string, double>& parameters) {
    // Simplified evaluation function - in practice this would run a full backtest

    // Simulate performance based on parameter values
    double base_performance = 1.0;

    // Add parameter-specific performance impacts (simplified)
    for (const auto& [param_name, value] : parameters) {
        if (config.parameters.count(param_name)) {
            const auto& param_def = config.parameters.at(param_name);

            // Normalize parameter value to 0-1 range
            double normalized = (value - param_def.min_value) / (param_def.max_value - param_def.min_value);

            // Apply parameter-specific performance curve (simplified)
            if (param_name.find("lookback") != std::string::npos ||
                param_name.find("period") != std::string::npos) {
                // Optimal lookback periods typically in middle range
                base_performance += 0.5 * (1.0 - 4.0 * (normalized - 0.5) * (normalized - 0.5));
            } else if (param_name.find("threshold") != std::string::npos) {
                // Thresholds often have optimal points
                base_performance += 0.3 * (normalized * (1.0 - normalized) * 4);
            } else {
                // Default smooth performance curve
                base_performance += 0.2 * std::sin(normalized * M_PI);
            }
        }
    }

    // Add some random variation to simulate market uncertainty
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<> noise(0.0, 0.1);
    base_performance += noise(gen);

    // Convert to Sharpe ratio scale
    return std::max(0.0, base_performance);
}

// Simplified implementations for other optimization methods
TradingStrategyOptimizer::OptimizationResult TradingStrategyOptimizer::runGridSearch(const StrategyConfig& config) {
    OptimizationResult result;
    result.strategy_id = config.strategy_id;
    result.optimization_method = "Grid Search";

    auto parameter_grid = generateParameterGrid(config);
    double best_score = -std::numeric_limits<double>::infinity();
    std::map<std::string, double> best_parameters;

    for (size_t i = 0; i < parameter_grid.size() && i < config_.max_evaluations; ++i) {
        const auto& parameters = parameter_grid[i];
        double score = evaluateParameterSet(config, parameters);

        if (score > best_score) {
            best_score = score;
            best_parameters = parameters;
        }

        result.evaluations_performed++;
        result.iterations_performed++;

        if (progress_callback_) {
            double progress = static_cast<double>(i) / std::min(parameter_grid.size(),
                                                              static_cast<size_t>(config_.max_evaluations));
            progress_callback_(config.strategy_id, progress);
        }
    }

    result.optimal_parameters = best_parameters;
    result.optimal_sharpe_ratio = best_score;
    result.optimal_return = best_score * 0.15;
    result.optimal_volatility = result.optimal_return / best_score;

    return result;
}

TradingStrategyOptimizer::OptimizationResult TradingStrategyOptimizer::runRandomSearch(const StrategyConfig& config) {
    OptimizationResult result;
    result.strategy_id = config.strategy_id;
    result.optimization_method = "Random Search";

    double best_score = -std::numeric_limits<double>::infinity();
    std::map<std::string, double> best_parameters;

    for (int i = 0; i < config_.max_evaluations; ++i) {
        auto parameters = generateRandomParameters(config);
        if (!isValidParameterSet(config, parameters)) continue;

        double score = evaluateParameterSet(config, parameters);

        if (score > best_score) {
            best_score = score;
            best_parameters = parameters;
        }

        result.evaluations_performed++;
        result.iterations_performed++;

        if (progress_callback_) {
            double progress = static_cast<double>(i) / config_.max_evaluations;
            progress_callback_(config.strategy_id, progress);
        }
    }

    result.optimal_parameters = best_parameters;
    result.optimal_sharpe_ratio = best_score;
    result.optimal_return = best_score * 0.15;
    result.optimal_volatility = result.optimal_return / best_score;

    return result;
}

// Utility methods
std::vector<std::map<std::string, double>> TradingStrategyOptimizer::generateParameterGrid(const StrategyConfig& config) {
    std::vector<std::map<std::string, double>> grid;
    std::vector<std::string> param_names;
    std::vector<std::vector<double>> param_values;

    // Build parameter value vectors
    for (const auto& [name, def] : config.parameters) {
        if (!def.is_optimizable) continue;

        param_names.push_back(name);
        std::vector<double> values;

        for (double val = def.min_value; val <= def.max_value; val += def.step_size) {
            values.push_back(val);
        }
        param_values.push_back(values);
    }

    // Generate grid combinations
    std::function<void(int, std::map<std::string, double>&)> generateCombinations =
        [&](int param_index, std::map<std::string, double>& current_params) {
            if (param_index >= param_names.size()) {
                grid.push_back(current_params);
                return;
            }

            for (double value : param_values[param_index]) {
                current_params[param_names[param_index]] = value;
                generateCombinations(param_index + 1, current_params);
            }
        };

    std::map<std::string, double> initial_params;
    generateCombinations(0, initial_params);

    return grid;
}

std::map<std::string, double> TradingStrategyOptimizer::generateRandomParameters(const StrategyConfig& config) {
    std::map<std::string, double> parameters;
    std::random_device rd;
    std::mt19937 gen(rd());

    for (const auto& [name, def] : config.parameters) {
        if (!def.is_optimizable) {
            parameters[name] = def.current_value;
            continue;
        }

        if (def.type == "double") {
            std::uniform_real_distribution<> dist(def.min_value, def.max_value);
            parameters[name] = dist(gen);
        } else if (def.type == "int") {
            std::uniform_int_distribution<> dist(static_cast<int>(def.min_value),
                                               static_cast<int>(def.max_value));
            parameters[name] = static_cast<double>(dist(gen));
        } else if (def.type == "bool") {
            std::uniform_int_distribution<> dist(0, 1);
            parameters[name] = static_cast<double>(dist(gen));
        }
    }

    return parameters;
}

std::map<std::string, double> TradingStrategyOptimizer::mutateParameters(
    const std::map<std::string, double>& parameters,
    const StrategyConfig& config,
    double mutation_rate) {

    std::map<std::string, double> mutated = parameters;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> prob(0.0, 1.0);

    for (auto& [name, value] : mutated) {
        if (prob(gen) < mutation_rate && config.parameters.count(name)) {
            const auto& def = config.parameters.at(name);

            if (def.type == "double") {
                std::normal_distribution<> noise(0.0, (def.max_value - def.min_value) * 0.1);
                value += noise(gen);
                value = std::max(def.min_value, std::min(def.max_value, value));
            } else if (def.type == "int") {
                std::uniform_int_distribution<> dist(static_cast<int>(def.min_value),
                                                   static_cast<int>(def.max_value));
                value = static_cast<double>(dist(gen));
            } else if (def.type == "bool") {
                value = (value > 0.5) ? 0.0 : 1.0; // Flip boolean
            }
        }
    }

    return mutated;
}

bool TradingStrategyOptimizer::isValidParameterSet(const StrategyConfig& config,
                                                  const std::map<std::string, double>& parameters) {
    for (const auto& [name, value] : parameters) {
        if (config.parameters.count(name)) {
            const auto& def = config.parameters.at(name);
            if (value < def.min_value || value > def.max_value) {
                return false;
            }
        }
    }
    return true;
}

bool TradingStrategyOptimizer::isStatisticallySignificant(const OptimizationResult& result) {
    // Simplified statistical significance test
    // In practice, this would involve proper hypothesis testing
    return result.sharpe_improvement > 0.5 && result.evaluations_performed > 100;
}

void TradingStrategyOptimizer::stopOptimization() {
    is_optimizing_.store(false);

    // Join all optimization threads
    for (auto& thread : optimization_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    optimization_threads_.clear();
}

// Static utility methods
std::string TradingStrategyOptimizer::optimizationMethodToString(OptimizationMethod method) {
    switch (method) {
        case OptimizationMethod::GRID_SEARCH: return "Grid Search";
        case OptimizationMethod::RANDOM_SEARCH: return "Random Search";
        case OptimizationMethod::GENETIC_ALGORITHM: return "Genetic Algorithm";
        case OptimizationMethod::BAYESIAN_OPTIMIZATION: return "Bayesian Optimization";
        case OptimizationMethod::PARTICLE_SWARM: return "Particle Swarm";
        case OptimizationMethod::SIMULATED_ANNEALING: return "Simulated Annealing";
        case OptimizationMethod::ML_GUIDED: return "ML Guided";
        default: return "Unknown";
    }
}

TradingStrategyOptimizer::OptimizationMethod TradingStrategyOptimizer::stringToOptimizationMethod(const std::string& method_str) {
    if (method_str == "Grid Search") return OptimizationMethod::GRID_SEARCH;
    if (method_str == "Random Search") return OptimizationMethod::RANDOM_SEARCH;
    if (method_str == "Genetic Algorithm") return OptimizationMethod::GENETIC_ALGORITHM;
    if (method_str == "Bayesian Optimization") return OptimizationMethod::BAYESIAN_OPTIMIZATION;
    if (method_str == "Particle Swarm") return OptimizationMethod::PARTICLE_SWARM;
    if (method_str == "Simulated Annealing") return OptimizationMethod::SIMULATED_ANNEALING;
    if (method_str == "ML Guided") return OptimizationMethod::ML_GUIDED;

    return OptimizationMethod::BAYESIAN_OPTIMIZATION; // Default
}

TradingStrategyOptimizer::StrategyConfig TradingStrategyOptimizer::createDefaultConfig(const std::string& strategy_type) {
    StrategyConfig config;
    config.strategy_type = strategy_type;

    // Add common parameters based on strategy type
    if (strategy_type == "momentum") {
        ParameterDefinition lookback;
        lookback.name = "lookback_period";
        lookback.type = "int";
        lookback.min_value = 5;
        lookback.max_value = 100;
        lookback.step_size = 5;
        lookback.current_value = 20;
        config.parameters["lookback_period"] = lookback;

        ParameterDefinition threshold;
        threshold.name = "momentum_threshold";
        threshold.type = "double";
        threshold.min_value = 0.01;
        threshold.max_value = 0.20;
        threshold.step_size = 0.01;
        threshold.current_value = 0.05;
        config.parameters["momentum_threshold"] = threshold;
    } else if (strategy_type == "mean_reversion") {
        ParameterDefinition lookback;
        lookback.name = "mean_lookback";
        lookback.type = "int";
        lookback.min_value = 10;
        lookback.max_value = 200;
        lookback.step_size = 10;
        lookback.current_value = 50;
        config.parameters["mean_lookback"] = lookback;

        ParameterDefinition deviation;
        deviation.name = "deviation_threshold";
        deviation.type = "double";
        deviation.min_value = 0.5;
        deviation.max_value = 3.0;
        deviation.step_size = 0.1;
        deviation.current_value = 2.0;
        config.parameters["deviation_threshold"] = deviation;
    }

    return config;
}

// Placeholder implementations for remaining optimization methods
TradingStrategyOptimizer::OptimizationResult TradingStrategyOptimizer::runParticleSwarm(const StrategyConfig& config) {
    // Simplified particle swarm implementation
    return runRandomSearch(config); // Fallback to random search
}

TradingStrategyOptimizer::OptimizationResult TradingStrategyOptimizer::runSimulatedAnnealing(const StrategyConfig& config) {
    // Simplified simulated annealing implementation
    return runRandomSearch(config); // Fallback to random search
}

TradingStrategyOptimizer::OptimizationResult TradingStrategyOptimizer::runMLGuidedOptimization(const StrategyConfig& config) {
    // Simplified ML-guided optimization implementation
    return runBayesianOptimization(config); // Fallback to Bayesian optimization
}

} // namespace Strategy
} // namespace CryptoClaude