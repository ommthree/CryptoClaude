#include "PersonalMLPipeline.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <random>
#include <cmath>
#include <json/json.h>

namespace CryptoClaude {
namespace ML {

// PersonalMLPipeline Implementation
PersonalMLPipeline::PersonalMLPipeline(SmartCacheManager& cacheManager)
    : cacheManager_(cacheManager) {
}

bool PersonalMLPipeline::initialize(const PersonalMLConfig& config) {
    config_ = config;

    std::cout << "PersonalMLPipeline initialized with configuration:" << std::endl;
    std::cout << "  Max symbols: " << config_.maxSymbols << std::endl;
    std::cout << "  Max historical days: " << config_.maxHistoricalDays << std::endl;
    std::cout << "  Max features: " << config_.maxFeatures << std::endl;
    std::cout << "  Lightweight models: " << (config_.useLightweightModels ? "Yes" : "No") << std::endl;

    return true;
}

void PersonalMLPipeline::shutdown() {
    // Save any unsaved models
    std::cout << "PersonalMLPipeline shutdown - " << models_.size() << " models stored" << std::endl;
}

PersonalFeatureSet PersonalMLPipeline::extractFeaturesFromCache(const std::string& symbol,
                                                               const std::string& timeframe,
                                                               int days) {
    PersonalFeatureSet features;
    features.symbol = symbol;
    features.timeframe = timeframe;

    auto endDate = std::chrono::system_clock::now();
    auto startDate = endDate - std::chrono::hours(24 * days);

    features.startDate = startDate;
    features.endDate = endDate;

    // Try to get data from cache
    std::string cachedData = cacheManager_.getHistoricalData(symbol, "cryptocompare", timeframe, startDate, endDate);

    if (cachedData.empty()) {
        std::cout << "No cached data available for " << symbol << " (" << timeframe << ")" << std::endl;
        return features;
    }

    // Parse cached data (simplified JSON parsing)
    try {
        Json::Value jsonData;
        Json::Reader reader;

        if (reader.parse(cachedData, jsonData) && jsonData.isArray()) {
            std::vector<double> prices;
            std::vector<double> volumes;

            // Extract price and volume data from JSON
            for (const auto& dataPoint : jsonData) {
                if (dataPoint.isMember("close") && dataPoint["close"].isNumeric()) {
                    prices.push_back(dataPoint["close"].asDouble());
                }
                if (dataPoint.isMember("volumeto") && dataPoint["volumeto"].isNumeric()) {
                    volumes.push_back(dataPoint["volumeto"].asDouble());
                }
            }

            if (prices.size() < config_.minSamplesPerSymbol) {
                std::cout << "Insufficient data points for " << symbol
                          << " (" << prices.size() << " < " << config_.minSamplesPerSymbol << ")" << std::endl;
                return features;
            }

            // Calculate basic features
            features.prices = prices;
            features.returns = calculateSimpleReturns(prices);
            features.volatility = calculateVolatility(features.returns);
            features.momentum = calculateMomentum(prices);

            // Calculate technical indicators if we have enough data
            if (prices.size() >= 50) {
                features.sma20 = calculateSMA(prices, 20);
                features.sma50 = calculateSMA(prices, 50);
                features.rsi = calculateRSI(prices, 14);
            }

            if (!volumes.empty()) {
                features.volume = volumes;
                if (volumes.size() >= 20) {
                    features.volumeMA = calculateSMA(volumes, 20);
                }
            }

            features.totalSamples = prices.size();

            std::cout << "Extracted " << features.totalSamples << " samples for " << symbol << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error parsing cached data for " << symbol << ": " << e.what() << std::endl;
    }

    return features;
}

bool PersonalMLPipeline::validateFeatureSet(const PersonalFeatureSet& features) const {
    if (features.totalSamples < config_.minSamplesPerSymbol) {
        return false;
    }

    if (features.prices.empty() || features.returns.empty()) {
        return false;
    }

    // Check for data quality
    return hasMinimumDataQuality(features);
}

PersonalMLModel PersonalMLPipeline::trainPersonalModel(const std::string& symbol,
                                                      const PersonalFeatureSet& features) {
    if (!validateFeatureSet(features)) {
        std::cout << "Feature set validation failed for " << symbol << std::endl;
        return PersonalMLModel();
    }

    PersonalMLModel bestModel;
    double bestAccuracy = 0.0;

    auto startTime = std::chrono::high_resolution_clock::now();

    // Try different simple models
    if (config_.useLightweightModels) {
        // Linear model
        auto linearModel = trainSimpleLinearModel(features);
        auto linearValidation = validateModel(linearModel, features);

        if (linearValidation.accuracy > bestAccuracy) {
            bestModel = linearModel;
            bestAccuracy = linearValidation.accuracy;
        }

        // Simple tree model
        auto treeModel = trainSimpleTreeModel(features);
        auto treeValidation = validateModel(treeModel, features);

        if (treeValidation.accuracy > bestAccuracy) {
            bestModel = treeModel;
            bestAccuracy = treeValidation.accuracy;
        }

        // Simple ensemble if we have time
        auto endTime = std::chrono::high_resolution_clock::now();
        auto elapsedMinutes = std::chrono::duration_cast<std::chrono::minutes>(endTime - startTime).count();

        if (elapsedMinutes < config_.maxTrainingTimeMinutes / 2) {
            auto ensembleModel = trainSimpleEnsemble(features);
            auto ensembleValidation = validateModel(ensembleModel, features);

            if (ensembleValidation.accuracy > bestAccuracy) {
                bestModel = ensembleModel;
                bestAccuracy = ensembleValidation.accuracy;
            }
        }
    }

    // Finalize model
    bestModel.symbol = symbol;
    bestModel.accuracy = bestAccuracy;
    bestModel.trainedAt = std::chrono::system_clock::now();
    bestModel.trainingSamples = features.totalSamples;

    auto finalTime = std::chrono::high_resolution_clock::now();
    bestModel.trainingTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        finalTime - startTime).count() / 1000.0;

    std::cout << "Trained " << bestModel.modelType << " model for " << symbol
              << " with " << bestAccuracy << " accuracy" << std::endl;

    statistics_.totalModelsCreated++;
    return bestModel;
}

PersonalMLModel PersonalMLPipeline::trainSimpleLinearModel(const PersonalFeatureSet& features) {
    PersonalMLModel model;
    model.modelType = "linear";
    model.modelId = generateModelId(features.symbol, "linear");

    // Simple linear regression on returns
    if (features.returns.size() < 2) {
        return model;
    }

    // Use momentum as predictor for simplicity
    if (!features.momentum.empty() && features.momentum.size() == features.returns.size()) {
        // Calculate simple correlation-based weights
        double sumX = std::accumulate(features.momentum.begin(), features.momentum.end(), 0.0);
        double sumY = std::accumulate(features.returns.begin(), features.returns.end(), 0.0);
        double sumXY = 0.0, sumX2 = 0.0;

        for (size_t i = 0; i < std::min(features.momentum.size(), features.returns.size()); ++i) {
            sumXY += features.momentum[i] * features.returns[i];
            sumX2 += features.momentum[i] * features.momentum[i];
        }

        int n = std::min(features.momentum.size(), features.returns.size());
        if (n > 1 && sumX2 > 0) {
            double slope = (n * sumXY - sumX * sumY) / (n * sumX2 - sumX * sumX);
            double intercept = (sumY - slope * sumX) / n;

            model.weights["momentum"] = slope;
            model.weights["intercept"] = intercept;
            model.selectedFeatures.push_back("momentum");
        }
    }

    return model;
}

PersonalMLModel PersonalMLPipeline::trainSimpleTreeModel(const PersonalFeatureSet& features) {
    PersonalMLModel model;
    model.modelType = "decision_tree";
    model.modelId = generateModelId(features.symbol, "tree");

    // Simplified decision tree - use thresholds for RSI and momentum
    if (!features.rsi.empty() && !features.momentum.empty()) {
        // Simple threshold-based rules
        model.weights["rsi_threshold"] = 70.0;  // Overbought
        model.weights["rsi_lower"] = 30.0;      // Oversold
        model.weights["momentum_threshold"] = 0.05; // 5% momentum

        model.selectedFeatures.push_back("rsi");
        model.selectedFeatures.push_back("momentum");
    }

    return model;
}

PersonalMLModel PersonalMLPipeline::trainSimpleEnsemble(const PersonalFeatureSet& features) {
    PersonalMLModel model;
    model.modelType = "simple_ensemble";
    model.modelId = generateModelId(features.symbol, "ensemble");

    // Combine linear and tree models
    auto linear = trainSimpleLinearModel(features);
    auto tree = trainSimpleTreeModel(features);

    // Simple equal weighting
    model.weights["linear_weight"] = 0.5;
    model.weights["tree_weight"] = 0.5;

    // Combine selected features
    model.selectedFeatures = linear.selectedFeatures;
    model.selectedFeatures.insert(model.selectedFeatures.end(),
                                 tree.selectedFeatures.begin(),
                                 tree.selectedFeatures.end());

    // Remove duplicates
    std::sort(model.selectedFeatures.begin(), model.selectedFeatures.end());
    model.selectedFeatures.erase(
        std::unique(model.selectedFeatures.begin(), model.selectedFeatures.end()),
        model.selectedFeatures.end());

    return model;
}

PersonalPrediction PersonalMLPipeline::makePrediction(const std::string& symbol,
                                                     const std::string& modelId) {
    PersonalPrediction prediction;
    prediction.symbol = symbol;

    // Find the model
    PersonalMLModel model;
    if (!modelId.empty() && models_.find(modelId) != models_.end()) {
        model = models_[modelId];
    } else {
        // Use the best available model for this symbol
        model = selectBestModel(symbol);
    }

    if (model.modelId.empty()) {
        prediction.signal = "HOLD";
        prediction.confidence = 0.0;
        prediction.reasons.push_back("No suitable model found");
        return prediction;
    }

    // Get current features
    auto features = extractFeaturesFromCache(symbol, "1d", 30); // Last 30 days

    if (!validateFeatureSet(features)) {
        prediction.signal = "HOLD";
        prediction.confidence = 0.0;
        prediction.reasons.push_back("Insufficient current data");
        return prediction;
    }

    // Make prediction based on model type
    if (model.modelType == "linear") {
        prediction = makeLinearPrediction(model, features);
    } else if (model.modelType == "decision_tree") {
        prediction = makeTreePrediction(model, features);
    } else if (model.modelType == "simple_ensemble") {
        prediction = makeEnsemblePrediction(model, features);
    }

    // Apply personal trading constraints
    if (prediction.confidence < 0.6) { // 60% minimum confidence for personal use
        prediction.signal = "HOLD";
        prediction.reasons.push_back("Low confidence prediction");
    }

    statistics_.totalPredictions++;
    return prediction;
}

PersonalMLPipeline::ValidationResult PersonalMLPipeline::validateModel(const PersonalMLModel& model,
                                                                       const PersonalFeatureSet& testData) {
    ValidationResult result;

    if (testData.returns.empty()) {
        result.validationErrors.push_back("No test data available");
        return result;
    }

    // Simple validation using last 30% of data as test set
    int testSize = std::max(10, (int)(testData.returns.size() * 0.3));
    int trainSize = testData.returns.size() - testSize;

    std::vector<double> predictions;
    std::vector<double> actual;

    // Make predictions on test data (simplified)
    for (int i = trainSize; i < (int)testData.returns.size() - 1; ++i) {
        double prediction = 0.0;

        // Simple prediction based on model type
        if (model.modelType == "linear" && !model.weights.empty()) {
            if (i < (int)testData.momentum.size()) {
                prediction = model.weights.at("momentum") * testData.momentum[i] +
                           model.weights.at("intercept");
            }
        }

        predictions.push_back(prediction);
        actual.push_back(testData.returns[i + 1]); // Next day return
    }

    if (!predictions.empty() && predictions.size() == actual.size()) {
        // Calculate accuracy based on direction prediction
        int correct = 0;
        for (size_t i = 0; i < predictions.size(); ++i) {
            bool predictedUp = predictions[i] > 0;
            bool actualUp = actual[i] > 0;
            if (predictedUp == actualUp) correct++;
        }

        result.accuracy = (double)correct / predictions.size();
        result.totalPredictions = predictions.size();
        result.correctPredictions = correct;

        // Calculate other metrics
        if (result.accuracy > 0.5) {
            result.precision = result.accuracy; // Simplified
            result.recall = result.accuracy;    // Simplified
            result.f1Score = result.accuracy;   // Simplified
        }

        // Calculate Sharpe ratio for returns
        result.sharpeRatio = calculateSharpeRatio(actual);
    }

    return result;
}

PersonalMLModel PersonalMLPipeline::selectBestModel(const std::string& symbol) {
    PersonalMLModel bestModel;
    double bestScore = 0.0;

    for (const auto& modelPair : models_) {
        const auto& model = modelPair.second;
        if (model.symbol == symbol) {
            // Simple scoring: accuracy * freshness factor
            auto age = std::chrono::system_clock::now() - model.trainedAt;
            auto ageDays = std::chrono::duration_cast<std::chrono::hours>(age).count() / 24.0;
            double freshnessFactor = std::max(0.1, 1.0 - (ageDays / 30.0)); // Decay over 30 days

            double score = model.accuracy * freshnessFactor;
            if (score > bestScore) {
                bestScore = score;
                bestModel = model;
            }
        }
    }

    return bestModel;
}

// Helper method implementations
std::vector<double> PersonalMLPipeline::calculateSimpleReturns(const std::vector<double>& prices) const {
    std::vector<double> returns;
    for (size_t i = 1; i < prices.size(); ++i) {
        if (prices[i-1] > 0) {
            returns.push_back((prices[i] - prices[i-1]) / prices[i-1]);
        } else {
            returns.push_back(0.0);
        }
    }
    return returns;
}

std::vector<double> PersonalMLPipeline::calculateVolatility(const std::vector<double>& returns, int window) const {
    std::vector<double> volatility;

    for (int i = window; i <= (int)returns.size(); ++i) {
        double sum = 0.0, sumSq = 0.0;
        for (int j = i - window; j < i; ++j) {
            sum += returns[j];
            sumSq += returns[j] * returns[j];
        }

        double mean = sum / window;
        double variance = (sumSq / window) - (mean * mean);
        volatility.push_back(std::sqrt(std::max(0.0, variance)));
    }

    return volatility;
}

std::vector<double> PersonalMLPipeline::calculateSMA(const std::vector<double>& prices, int period) const {
    std::vector<double> sma;

    for (int i = period - 1; i < (int)prices.size(); ++i) {
        double sum = 0.0;
        for (int j = i - period + 1; j <= i; ++j) {
            sum += prices[j];
        }
        sma.push_back(sum / period);
    }

    return sma;
}

std::vector<double> PersonalMLPipeline::calculateRSI(const std::vector<double>& prices, int period) const {
    std::vector<double> rsi;

    if (prices.size() < (size_t)(period + 1)) return rsi;

    // Calculate price changes
    std::vector<double> gains, losses;
    for (size_t i = 1; i < prices.size(); ++i) {
        double change = prices[i] - prices[i-1];
        gains.push_back(std::max(0.0, change));
        losses.push_back(std::max(0.0, -change));
    }

    // Calculate RSI
    for (int i = period - 1; i < (int)gains.size(); ++i) {
        double avgGain = 0.0, avgLoss = 0.0;

        for (int j = i - period + 1; j <= i; ++j) {
            avgGain += gains[j];
            avgLoss += losses[j];
        }

        avgGain /= period;
        avgLoss /= period;

        if (avgLoss == 0.0) {
            rsi.push_back(100.0);
        } else {
            double rs = avgGain / avgLoss;
            rsi.push_back(100.0 - (100.0 / (1.0 + rs)));
        }
    }

    return rsi;
}

std::vector<double> PersonalMLPipeline::calculateMomentum(const std::vector<double>& prices, int period) const {
    std::vector<double> momentum;

    for (int i = period; i < (int)prices.size(); ++i) {
        if (prices[i - period] > 0) {
            momentum.push_back((prices[i] - prices[i - period]) / prices[i - period]);
        } else {
            momentum.push_back(0.0);
        }
    }

    return momentum;
}

bool PersonalMLPipeline::hasMinimumDataQuality(const PersonalFeatureSet& features) const {
    // Check for basic data requirements
    if (features.prices.empty() || features.returns.empty()) {
        return false;
    }

    // Check for excessive zeros or NaN values
    int zeroCount = std::count(features.prices.begin(), features.prices.end(), 0.0);
    double zeroRatio = (double)zeroCount / features.prices.size();

    return zeroRatio < 0.1; // Less than 10% zeros
}

double PersonalMLPipeline::calculateSharpeRatio(const std::vector<double>& returns) const {
    if (returns.empty()) return 0.0;

    double mean = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();

    double variance = 0.0;
    for (double r : returns) {
        variance += (r - mean) * (r - mean);
    }
    variance /= returns.size();

    double stdDev = std::sqrt(variance);
    return (stdDev > 0) ? (mean / stdDev) : 0.0;
}

std::string PersonalMLPipeline::generateModelId(const std::string& symbol, const std::string& type) const {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

    return symbol + "_" + type + "_" + std::to_string(timestamp);
}

PersonalMLPipeline::MLStatistics PersonalMLPipeline::getMLStatistics() const {
    MLStatistics stats = statistics_;
    stats.activeModels = models_.size();

    if (stats.totalPredictions > 0) {
        stats.averageAccuracy = (double)stats.accuratePredictions / stats.totalPredictions;
    }

    return stats;
}

// PersonalMLPipelineFactory Implementation
std::unique_ptr<PersonalMLPipeline> PersonalMLPipelineFactory::create(SmartCacheManager& cacheManager,
                                                                      PersonalStrategy strategy) {
    auto pipeline = std::make_unique<PersonalMLPipeline>(cacheManager);
    auto config = getConfigForStrategy(strategy);
    pipeline->initialize(config);
    return pipeline;
}

PersonalMLConfig PersonalMLPipelineFactory::getConfigForStrategy(PersonalStrategy strategy) {
    PersonalMLConfig config;

    switch (strategy) {
        case PersonalStrategy::CONSERVATIVE:
            config.maxSymbols = 3;
            config.maxHistoricalDays = 180;
            config.maxFeatures = 10;
            config.useLightweightModels = true;
            config.maxTrainingTimeMinutes = 2.0;
            config.minValidationAccuracy = 0.50;
            break;

        case PersonalStrategy::BALANCED:
            config.maxSymbols = 5;
            config.maxHistoricalDays = 365;
            config.maxFeatures = 20;
            config.useLightweightModels = true;
            config.maxTrainingTimeMinutes = 5.0;
            config.minValidationAccuracy = 0.55;
            break;

        case PersonalStrategy::PERFORMANCE:
            config.maxSymbols = 8;
            config.maxHistoricalDays = 730;
            config.maxFeatures = 30;
            config.useLightweightModels = false;
            config.maxTrainingTimeMinutes = 10.0;
            config.minValidationAccuracy = 0.60;
            break;

        case PersonalStrategy::MINIMAL:
            config.maxSymbols = 2;
            config.maxHistoricalDays = 90;
            config.maxFeatures = 5;
            config.useLightweightModels = true;
            config.maxTrainingTimeMinutes = 1.0;
            config.minValidationAccuracy = 0.45;
            break;
    }

    return config;
}

// PersonalMLUtils Implementation
std::vector<double> PersonalMLUtils::normalizeData(const std::vector<double>& data) {
    if (data.empty()) return data;

    double minVal = *std::min_element(data.begin(), data.end());
    double maxVal = *std::max_element(data.begin(), data.end());
    double range = maxVal - minVal;

    if (range == 0) return data;

    std::vector<double> normalized;
    for (double value : data) {
        normalized.push_back((value - minVal) / range);
    }

    return normalized;
}

PersonalMLModel PersonalMLUtils::selectBestModel(const std::vector<PersonalMLModel>& models) {
    if (models.empty()) return PersonalMLModel();

    return *std::max_element(models.begin(), models.end(),
        [](const PersonalMLModel& a, const PersonalMLModel& b) {
            return a.accuracy < b.accuracy;
        });
}

} // namespace ML
} // namespace CryptoClaude