#include "RandomForestPredictor.h"
#include "../Analytics/BacktestingEngine.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <iostream>

namespace CryptoClaude {
namespace ML {

// === DECISION TREE IMPLEMENTATION ===

DecisionTree::DecisionTree(int max_depth, int min_samples_split, double min_impurity_decrease)
    : max_depth_(max_depth), min_samples_split_(min_samples_split),
      min_impurity_decrease_(min_impurity_decrease), rng_(std::random_device{}()) {
}

void DecisionTree::train(const std::vector<MLFeatureVector>& features,
                        const std::vector<double>& targets) {
    if (features.size() != targets.size() || features.empty()) {
        return;
    }

    std::vector<int> sample_indices(features.size());
    std::iota(sample_indices.begin(), sample_indices.end(), 0);
    root_ = buildTree(features, targets, sample_indices, 0);
}

double DecisionTree::predict(const MLFeatureVector& features) const {
    if (!root_) return 0.0;
    return predictNode(features, root_.get());
}

std::unique_ptr<DecisionTreeNode> DecisionTree::buildTree(
    const std::vector<MLFeatureVector>& features,
    const std::vector<double>& targets,
    const std::vector<int>& sample_indices,
    int depth) {

    auto node = std::make_unique<DecisionTreeNode>();
    node->sample_count = sample_indices.size();
    node->mse = calculateMSE(targets, sample_indices);

    if (depth >= max_depth_ || sample_indices.size() < min_samples_split_ || node->mse < min_impurity_decrease_) {
        node->is_leaf = true;
        node->prediction_value = calculateMean(targets, sample_indices);
        return node;
    }

    auto split_result = findBestSplit(features, targets, sample_indices);
    if (split_result.impurity_decrease < min_impurity_decrease_) {
        node->is_leaf = true;
        node->prediction_value = calculateMean(targets, sample_indices);
        return node;
    }

    node->split_feature = split_result.feature;
    node->split_value = split_result.threshold;

    if (!split_result.left_indices.empty()) {
        node->left_child = buildTree(features, targets, split_result.left_indices, depth + 1);
    }
    if (!split_result.right_indices.empty()) {
        node->right_child = buildTree(features, targets, split_result.right_indices, depth + 1);
    }

    return node;
}

DecisionTree::SplitResult DecisionTree::findBestSplit(
    const std::vector<MLFeatureVector>& features,
    const std::vector<double>& targets,
    const std::vector<int>& sample_indices) {

    SplitResult best_split;
    best_split.impurity_decrease = 0.0;

    if (sample_indices.size() < 2) return best_split;

    double current_mse = calculateMSE(targets, sample_indices);
    std::vector<std::string> feature_names = {
        "sma_5_ratio", "sma_20_ratio", "rsi_14", "volatility_10", "volume_ratio",
        "price_momentum_3", "price_momentum_7", "high_low_ratio", "open_close_gap",
        "btc_correlation_30", "market_beta", "news_sentiment", "sentiment_momentum",
        "sentiment_quality", "day_of_week", "hour_of_day"
    };

    for (const std::string& feature_name : feature_names) {
        std::vector<double> feature_values;
        for (int idx : sample_indices) {
            double value = 0.0;
            const auto& feat = features[idx];

            if (feature_name == "sma_5_ratio") value = feat.sma_5_ratio;
            else if (feature_name == "sma_20_ratio") value = feat.sma_20_ratio;
            else if (feature_name == "rsi_14") value = feat.rsi_14;
            else if (feature_name == "volatility_10") value = feat.volatility_10;
            else if (feature_name == "volume_ratio") value = feat.volume_ratio;
            else if (feature_name == "price_momentum_3") value = feat.price_momentum_3;
            else if (feature_name == "price_momentum_7") value = feat.price_momentum_7;
            else if (feature_name == "high_low_ratio") value = feat.high_low_ratio;
            else if (feature_name == "open_close_gap") value = feat.open_close_gap;
            else if (feature_name == "btc_correlation_30") value = feat.btc_correlation_30;
            else if (feature_name == "market_beta") value = feat.market_beta;
            else if (feature_name == "news_sentiment") value = feat.news_sentiment;
            else if (feature_name == "sentiment_momentum") value = feat.sentiment_momentum;
            else if (feature_name == "sentiment_quality") value = feat.sentiment_quality;
            else if (feature_name == "day_of_week") value = feat.day_of_week;
            else if (feature_name == "hour_of_day") value = feat.hour_of_day;

            feature_values.push_back(value);
        }

        std::vector<double> sorted_values = feature_values;
        std::sort(sorted_values.begin(), sorted_values.end());

        for (double percentile : {0.25, 0.5, 0.75}) {
            int index = std::max(0, std::min((int)(sorted_values.size() * percentile), (int)(sorted_values.size() - 1)));
            double threshold = sorted_values[index];

            std::vector<int> left_indices, right_indices;
            for (int i = 0; i < feature_values.size(); ++i) {
                int original_idx = sample_indices[i];
                if (feature_values[i] <= threshold) {
                    left_indices.push_back(original_idx);
                } else {
                    right_indices.push_back(original_idx);
                }
            }

            if (left_indices.empty() || right_indices.empty()) continue;

            double left_mse = calculateMSE(targets, left_indices);
            double right_mse = calculateMSE(targets, right_indices);
            double left_weight = (double)left_indices.size() / sample_indices.size();
            double right_weight = (double)right_indices.size() / sample_indices.size();
            double weighted_mse = left_weight * left_mse + right_weight * right_mse;
            double impurity_decrease = current_mse - weighted_mse;

            if (impurity_decrease > best_split.impurity_decrease) {
                best_split.feature = feature_name;
                best_split.threshold = threshold;
                best_split.impurity_decrease = impurity_decrease;
                best_split.left_indices = left_indices;
                best_split.right_indices = right_indices;
            }
        }
    }
    return best_split;
}

double DecisionTree::calculateMSE(const std::vector<double>& targets, const std::vector<int>& indices) {
    if (indices.empty()) return 0.0;
    double mean = calculateMean(targets, indices);
    double sum_sq_diff = 0.0;
    for (int idx : indices) {
        double diff = targets[idx] - mean;
        sum_sq_diff += diff * diff;
    }
    return sum_sq_diff / indices.size();
}

double DecisionTree::calculateMean(const std::vector<double>& targets, const std::vector<int>& indices) {
    if (indices.empty()) return 0.0;
    double sum = 0.0;
    for (int idx : indices) sum += targets[idx];
    return sum / indices.size();
}

double DecisionTree::predictNode(const MLFeatureVector& features, const DecisionTreeNode* node) const {
    if (!node || node->is_leaf) return node ? node->prediction_value : 0.0;

    double feature_value = 0.0;
    if (node->split_feature == "sma_5_ratio") feature_value = features.sma_5_ratio;
    else if (node->split_feature == "sma_20_ratio") feature_value = features.sma_20_ratio;
    else if (node->split_feature == "rsi_14") feature_value = features.rsi_14;
    else if (node->split_feature == "volatility_10") feature_value = features.volatility_10;
    else if (node->split_feature == "volume_ratio") feature_value = features.volume_ratio;
    else if (node->split_feature == "price_momentum_3") feature_value = features.price_momentum_3;
    else if (node->split_feature == "price_momentum_7") feature_value = features.price_momentum_7;
    else if (node->split_feature == "news_sentiment") feature_value = features.news_sentiment;

    if (feature_value <= node->split_value && node->left_child) {
        return predictNode(features, node->left_child.get());
    } else if (node->right_child) {
        return predictNode(features, node->right_child.get());
    }
    return 0.0;
}

// === RANDOM FOREST IMPLEMENTATION ===

RandomForestPredictor::RandomForestPredictor(int n_trees, int max_depth, double feature_subsample_ratio, double bootstrap_sample_ratio)
    : rng_(std::random_device{}()) {
    params_.n_trees = n_trees;
    params_.max_depth = max_depth;
    params_.feature_subsample_ratio = feature_subsample_ratio;
    params_.bootstrap_sample_ratio = bootstrap_sample_ratio;
    params_.min_samples_split = 20;
    params_.min_impurity_decrease = 1e-7;

    status_.is_trained = false;
    status_.model_version = "v1.0";
    status_.training_samples = 0;
    status_.feature_count = 0;
    status_.training_accuracy = 0.0;
    status_.validation_accuracy = 0.0;
}

bool RandomForestPredictor::trainModel(const std::vector<MLFeatureVector>& features,
                                      const std::vector<double>& targets,
                                      const std::string& model_version) {
    if (features.size() != targets.size() || features.empty()) return false;

    status_.model_version = model_version;
    status_.training_date = std::chrono::system_clock::now();
    status_.training_samples = features.size();
    status_.feature_count = 8; // Core features

    trees_.clear();
    trees_.reserve(params_.n_trees);

    for (int i = 0; i < params_.n_trees; ++i) {
        auto tree = std::make_unique<DecisionTree>(params_.max_depth, params_.min_samples_split, params_.min_impurity_decrease);

        auto bootstrap_indices = createBootstrapSample(features.size());
        std::vector<MLFeatureVector> bootstrap_features;
        std::vector<double> bootstrap_targets;

        for (int idx : bootstrap_indices) {
            bootstrap_features.push_back(features[idx]);
            bootstrap_targets.push_back(targets[idx]);
        }

        tree->train(bootstrap_features, bootstrap_targets);
        trees_.push_back(std::move(tree));
    }

    // Calculate training accuracy
    int correct_direction = 0;
    for (size_t i = 0; i < features.size(); ++i) {
        double prediction = predict(features[i]).predicted_return;
        double actual = targets[i];
        if ((prediction > 0 && actual > 0) || (prediction < 0 && actual < 0) ||
            (std::abs(prediction) < 1e-6 && std::abs(actual) < 1e-6)) {
            correct_direction++;
        }
    }

    status_.training_accuracy = (double)correct_direction / features.size();
    status_.is_trained = true;
    return true;
}

MLPrediction RandomForestPredictor::predict(const MLFeatureVector& features) {
    MLPrediction result;
    result.symbol = features.symbol;
    result.prediction_time = std::chrono::system_clock::now();

    if (!status_.is_trained || trees_.empty()) return result;

    std::vector<double> tree_predictions;
    tree_predictions.reserve(trees_.size());

    for (const auto& tree : trees_) {
        tree_predictions.push_back(tree->predict(features));
    }

    double sum = std::accumulate(tree_predictions.begin(), tree_predictions.end(), 0.0);
    result.predicted_return = sum / tree_predictions.size();

    double variance_sum = 0.0;
    for (double pred : tree_predictions) {
        double diff = pred - result.predicted_return;
        variance_sum += diff * diff;
    }
    result.prediction_variance = variance_sum / tree_predictions.size();
    result.confidence_score = std::max(0.0, 1.0 - std::sqrt(result.prediction_variance));

    return result;
}

MLFeatureVector RandomForestPredictor::createFeatures(
    const std::string& symbol,
    const std::vector<Analytics::MarketDataPoint>& market_data,
    const std::vector<Database::Models::SentimentData>& sentiment_data,
    const std::map<std::string, std::vector<Analytics::MarketDataPoint>>& market_context) {

    MLFeatureVector features;
    features.symbol = symbol;
    features.timestamp = std::chrono::system_clock::now();

    if (market_data.empty()) return features;

    // Technical indicators
    features.sma_5_ratio = calculateTechnicalIndicator(market_data, "sma_ratio", 5);
    features.sma_20_ratio = calculateTechnicalIndicator(market_data, "sma_ratio", 20);
    features.rsi_14 = calculateTechnicalIndicator(market_data, "rsi", 14);
    features.volatility_10 = calculateTechnicalIndicator(market_data, "volatility", 10);
    features.volume_ratio = calculateTechnicalIndicator(market_data, "volume_ratio", 20);
    features.price_momentum_3 = calculateTechnicalIndicator(market_data, "momentum", 3);
    features.price_momentum_7 = calculateTechnicalIndicator(market_data, "momentum", 7);

    // Sentiment features
    features.news_sentiment = calculateSentimentFeature(sentiment_data, "aggregate");

    return features;
}

std::vector<int> RandomForestPredictor::createBootstrapSample(int sample_size) const {
    std::vector<int> indices;
    indices.reserve(sample_size);
    std::uniform_int_distribution<int> dist(0, sample_size - 1);
    for (int i = 0; i < sample_size; ++i) {
        indices.push_back(dist(rng_));
    }
    return indices;
}

double RandomForestPredictor::calculateTechnicalIndicator(
    const std::vector<Analytics::MarketDataPoint>& data,
    const std::string& indicator_type, int period) const {

    if (data.empty() || period <= 0 || period > data.size()) return 0.0;

    if (indicator_type == "sma_ratio") {
        double sum = 0.0;
        for (int i = data.size() - period; i < data.size(); ++i) {
            sum += data[i].close;
        }
        double sma = sum / period;
        return sma > 0 ? data.back().close / sma : 1.0;

    } else if (indicator_type == "volatility") {
        if (period < 2) return 0.0;
        std::vector<double> returns;
        for (int i = data.size() - period; i < data.size() - 1; ++i) {
            if (data[i].close > 0) {
                returns.push_back((data[i+1].close - data[i].close) / data[i].close);
            }
        }
        if (returns.empty()) return 0.0;
        double mean_return = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
        double variance = 0.0;
        for (double ret : returns) {
            variance += (ret - mean_return) * (ret - mean_return);
        }
        return std::sqrt(variance / (returns.size() - 1));

    } else if (indicator_type == "momentum") {
        if (data.size() < period + 1) return 0.0;
        double old_price = data[data.size() - period - 1].close;
        double new_price = data.back().close;
        return old_price > 0 ? (new_price - old_price) / old_price : 0.0;

    } else if (indicator_type == "volume_ratio") {
        double sum = 0.0;
        for (int i = data.size() - period; i < data.size(); ++i) {
            sum += data[i].volume;
        }
        double avg_volume = sum / period;
        return avg_volume > 0 ? data.back().volume / avg_volume : 1.0;
    }
    return 0.0;
}

double RandomForestPredictor::calculateSentimentFeature(
    const std::vector<Database::Models::SentimentData>& sentiment_data,
    const std::string& feature_type) const {

    if (sentiment_data.empty()) return 0.0;

    if (feature_type == "aggregate") {
        double sum = 0.0;
        for (const auto& data : sentiment_data) {
            sum += data.getSentimentScore();
        }
        return sum / sentiment_data.size();
    }
    return 0.0;
}

RandomForestPredictor::ModelStatus RandomForestPredictor::getModelStatus() const {
    return status_;
}

RandomForestPredictor::ModelParameters RandomForestPredictor::getModelParameters() const {
    return params_;
}

std::map<std::string, double> RandomForestPredictor::getPredictionsForOptimization(
    const std::vector<std::string>& symbols,
    const std::map<std::string, MLFeatureVector>& current_features) {

    std::map<std::string, double> predictions;

    for (const std::string& symbol : symbols) {
        auto it = current_features.find(symbol);
        if (it != current_features.end()) {
            MLPrediction pred = predict(it->second);
            predictions[symbol] = pred.predicted_return;
        } else {
            // Default prediction for missing features
            predictions[symbol] = 0.0;
        }
    }

    return predictions;
}

}} // namespace CryptoClaude::ML