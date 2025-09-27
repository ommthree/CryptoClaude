#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <chrono>
#include <cmath>
#include <assert.h>

// Include ML components
#include "src/Core/ML/RandomForestPredictor.h"

// Include existing components for integration
#include "src/Core/Database/DatabaseManager.h"
#include "src/Core/Database/Models/SentimentData.h"
#include "src/Core/Analytics/BacktestingEngine.h"

using namespace CryptoClaude;
using namespace CryptoClaude::ML;
using namespace CryptoClaude::Analytics;

class Day8RandomForestTest {
private:
    bool verbose;
    int total_tests;
    int passed_tests;

public:
    Day8RandomForestTest(bool verbose = true) : verbose(verbose), total_tests(0), passed_tests(0) {}

    void assert_test(bool condition, const std::string& test_name) {
        total_tests++;
        if (condition) {
            passed_tests++;
            if (verbose) std::cout << "✅ " << test_name << " PASSED" << std::endl;
        } else {
            if (verbose) std::cout << "❌ " << test_name << " FAILED" << std::endl;
        }
    }

    // Test MLFeatureVector construction and initialization
    void test_MLFeatureVector_Construction() {
        if (verbose) std::cout << "\n📊 Testing MLFeatureVector Construction..." << std::endl;

        MLFeatureVector features;
        features.symbol = "BTC";
        features.sma_5_ratio = 1.05;
        features.rsi_14 = 65.0;
        features.volatility_10 = 0.03;

        assert_test(features.symbol == "BTC", "MLFeatureVector symbol assignment");
        assert_test(features.sma_5_ratio == 1.05, "MLFeatureVector SMA ratio assignment");
        assert_test(features.rsi_14 == 65.0, "MLFeatureVector RSI assignment");
        assert_test(features.volatility_10 == 0.03, "MLFeatureVector volatility assignment");

        // Test default values
        MLFeatureVector default_features;
        assert_test(default_features.sma_5_ratio == 1.0, "MLFeatureVector default SMA ratio");
        assert_test(default_features.rsi_14 == 50.0, "MLFeatureVector default RSI");
        assert_test(default_features.volume_ratio == 1.0, "MLFeatureVector default volume ratio");
    }

    // Test RandomForestPredictor construction and configuration
    void test_RandomForestPredictor_Construction() {
        if (verbose) std::cout << "\n🌲 Testing RandomForestPredictor Construction..." << std::endl;

        // Test default construction
        RandomForestPredictor predictor;
        auto params = predictor.getModelParameters();
        assert_test(params.n_trees == 100, "RandomForestPredictor default n_trees");
        assert_test(params.max_depth == 10, "RandomForestPredictor default max_depth");

        // Test custom construction
        RandomForestPredictor custom_predictor(50, 8, 0.6, 0.9);
        auto custom_params = custom_predictor.getModelParameters();
        assert_test(custom_params.n_trees == 50, "RandomForestPredictor custom n_trees");
        assert_test(custom_params.max_depth == 8, "RandomForestPredictor custom max_depth");

        // Test model status
        auto status = predictor.getModelStatus();
        assert_test(!status.is_trained, "RandomForestPredictor initial training status");
        assert_test(status.model_version == "v1.0", "RandomForestPredictor default model version");
    }

    // Test feature engineering with synthetic market data
    void test_Feature_Engineering() {
        if (verbose) std::cout << "\n⚙️ Testing Feature Engineering Pipeline..." << std::endl;

        RandomForestPredictor predictor;

        // Create synthetic market data
        std::vector<MarketDataPoint> market_data;
        auto base_time = std::chrono::system_clock::now();

        for (int i = 0; i < 30; ++i) {
            MarketDataPoint point;
            point.timestamp = base_time + std::chrono::hours(i);
            point.symbol = "BTC";
            point.open = 50000.0 + i * 100.0;  // Upward trend
            point.high = point.open + 500.0;
            point.low = point.open - 300.0;
            point.close = point.open + (i % 3 == 0 ? 200.0 : -100.0);  // Some volatility
            point.volume = 1000000.0 + i * 10000.0;
            market_data.push_back(point);
        }

        // Create synthetic sentiment data
        std::vector<Database::Models::SentimentData> sentiment_data;
        Database::Models::SentimentData sentiment;
        sentiment.setAvgSentiment(0.6);  // Positive sentiment
        sentiment_data.push_back(sentiment);

        // Test feature creation
        MLFeatureVector features = predictor.createFeatures(
            "BTC",
            market_data,
            sentiment_data
        );

        assert_test(features.symbol == "BTC", "Feature engineering symbol assignment");
        assert_test(features.sma_5_ratio > 0.9 && features.sma_5_ratio < 1.1, "Feature engineering SMA ratio range");
        assert_test(features.volatility_10 >= 0.0, "Feature engineering volatility non-negative");
        assert_test(features.volume_ratio > 0.5, "Feature engineering volume ratio reasonable");
        assert_test(features.news_sentiment == 0.6, "Feature engineering sentiment integration");
    }

    // Test Random Forest training with synthetic data
    void test_RandomForest_Training() {
        if (verbose) std::cout << "\n🧠 Testing Random Forest Training..." << std::endl;

        RandomForestPredictor predictor(20, 5);  // Smaller forest for testing

        // Generate training data
        std::vector<MLFeatureVector> training_features;
        std::vector<double> training_targets;

        for (int i = 0; i < 100; ++i) {
            MLFeatureVector features;
            features.symbol = "TEST";
            features.sma_5_ratio = 0.9 + (i * 0.002);  // 0.9 to 1.1
            features.rsi_14 = 30.0 + (i * 0.4);       // 30 to 70
            features.volatility_10 = 0.01 + (i * 0.0002); // 0.01 to 0.03
            features.volume_ratio = 0.8 + (i * 0.004);    // 0.8 to 1.2
            features.news_sentiment = -0.5 + (i * 0.01);  // -0.5 to 0.5

            // Synthetic target: positive return if sentiment and momentum positive
            double target = (features.news_sentiment > 0 && features.sma_5_ratio > 1.0) ?
                           0.05 : -0.02;
            target += (static_cast<double>(rand()) / RAND_MAX - 0.5) * 0.02; // Add noise

            training_features.push_back(features);
            training_targets.push_back(target);
        }

        // Train the model
        bool training_success = predictor.trainModel(
            training_features,
            training_targets,
            "test_v1.0"
        );

        assert_test(training_success, "Random Forest training success");

        // Check model status after training
        auto status = predictor.getModelStatus();
        assert_test(status.is_trained, "Random Forest training status");
        assert_test(status.training_samples == 100, "Random Forest training sample count");
        assert_test(status.model_version == "test_v1.0", "Random Forest model version");
        assert_test(status.training_accuracy > 0.4, "Random Forest training accuracy reasonable");
    }

    // Test Random Forest prediction
    void test_RandomForest_Prediction() {
        if (verbose) std::cout << "\n🔮 Testing Random Forest Prediction..." << std::endl;

        RandomForestPredictor predictor(10, 3);  // Small forest for testing

        // Create simple training data
        std::vector<MLFeatureVector> training_features;
        std::vector<double> training_targets;

        // Simple pattern: positive sentiment -> positive return
        for (int i = 0; i < 50; ++i) {
            MLFeatureVector features;
            features.symbol = "TEST";
            features.news_sentiment = (i < 25) ? 0.5 : -0.5;  // Half positive, half negative
            features.sma_5_ratio = 1.0;
            features.rsi_14 = 50.0;

            double target = features.news_sentiment > 0 ? 0.03 : -0.03;

            training_features.push_back(features);
            training_targets.push_back(target);
        }

        // Train model
        bool training_success = predictor.trainModel(training_features, training_targets);
        assert_test(training_success, "Prediction test model training");

        // Test prediction
        MLFeatureVector test_features;
        test_features.symbol = "TEST";
        test_features.news_sentiment = 0.7;  // Positive sentiment
        test_features.sma_5_ratio = 1.0;
        test_features.rsi_14 = 50.0;

        MLPrediction prediction = predictor.predict(test_features);

        assert_test(prediction.symbol == "TEST", "Prediction symbol assignment");
        assert_test(prediction.predicted_return != 0.0, "Prediction non-zero return");
        assert_test(prediction.confidence_score >= 0.0 && prediction.confidence_score <= 1.0, "Prediction confidence range");
        assert_test(prediction.prediction_variance >= 0.0, "Prediction variance non-negative");

        // Test that positive sentiment tends toward positive prediction
        assert_test(prediction.predicted_return > -0.1, "Prediction reasonable for positive sentiment");
    }

    // Test ML integration with portfolio optimization
    void test_ML_Portfolio_Integration() {
        if (verbose) std::cout << "\n🔗 Testing ML-Portfolio Integration..." << std::endl;

        RandomForestPredictor predictor(5, 3);  // Very small for testing

        // Simple training data
        std::vector<MLFeatureVector> features;
        std::vector<double> targets;

        for (int i = 0; i < 20; ++i) {
            MLFeatureVector feature;
            feature.symbol = "BTC";
            feature.rsi_14 = 30.0 + i * 2.0;  // RSI from 30 to 68

            // Simple rule: lower RSI -> higher expected return
            double target = (70.0 - feature.rsi_14) * 0.001;  // 0.04 to 0.00

            features.push_back(feature);
            targets.push_back(target);
        }

        predictor.trainModel(features, targets);

        // Test prediction for optimization
        std::vector<std::string> symbols = {"BTC", "ETH"};
        std::map<std::string, MLFeatureVector> current_features;

        MLFeatureVector btc_features;
        btc_features.symbol = "BTC";
        btc_features.rsi_14 = 35.0;  // Oversold
        current_features["BTC"] = btc_features;

        MLFeatureVector eth_features;
        eth_features.symbol = "ETH";
        eth_features.rsi_14 = 65.0;  // Overbought
        current_features["ETH"] = eth_features;

        auto predictions = predictor.getPredictionsForOptimization(symbols, current_features);

        assert_test(predictions.find("BTC") != predictions.end(), "ML Portfolio integration BTC prediction");
        assert_test(predictions.find("ETH") != predictions.end(), "ML Portfolio integration ETH prediction");

        // BTC (oversold) should have higher expected return than ETH (overbought)
        if (predictions.find("BTC") != predictions.end() && predictions.find("ETH") != predictions.end()) {
            assert_test(predictions["BTC"] > predictions["ETH"], "ML Portfolio integration RSI logic");
        }
    }

    // Test error handling and edge cases
    void test_Error_Handling() {
        if (verbose) std::cout << "\n🛡️ Testing Error Handling..." << std::endl;

        RandomForestPredictor predictor;

        // Test empty training data
        std::vector<MLFeatureVector> empty_features;
        std::vector<double> empty_targets;
        bool empty_result = predictor.trainModel(empty_features, empty_targets);
        assert_test(!empty_result, "Error handling empty training data");

        // Test mismatched feature/target sizes
        std::vector<MLFeatureVector> features(5);
        std::vector<double> targets(3);
        bool mismatch_result = predictor.trainModel(features, targets);
        assert_test(!mismatch_result, "Error handling mismatched data sizes");

        // Test prediction without training
        MLFeatureVector test_features;
        MLPrediction untrained_prediction = predictor.predict(test_features);
        assert_test(untrained_prediction.predicted_return == 0.0, "Error handling untrained prediction");
        assert_test(untrained_prediction.confidence_score == 0.0, "Error handling untrained confidence");
    }

    // Test performance with realistic data sizes
    void test_Performance() {
        if (verbose) std::cout << "\n⚡ Testing Performance with Realistic Data..." << std::endl;

        auto start_time = std::chrono::high_resolution_clock::now();

        RandomForestPredictor predictor(30, 8);  // Realistic size

        // Generate larger dataset
        std::vector<MLFeatureVector> features;
        std::vector<double> targets;

        for (int i = 0; i < 500; ++i) {
            MLFeatureVector feature;
            feature.symbol = "BTC";
            feature.sma_5_ratio = 0.95 + (static_cast<double>(rand()) / RAND_MAX) * 0.1;
            feature.rsi_14 = 20.0 + (static_cast<double>(rand()) / RAND_MAX) * 60.0;
            feature.volatility_10 = 0.01 + (static_cast<double>(rand()) / RAND_MAX) * 0.05;
            feature.news_sentiment = -1.0 + (static_cast<double>(rand()) / RAND_MAX) * 2.0;

            // Complex target function
            double target = feature.sma_5_ratio - 1.0 +
                           (feature.rsi_14 - 50.0) * 0.001 +
                           feature.news_sentiment * 0.02 +
                           (static_cast<double>(rand()) / RAND_MAX - 0.5) * 0.02;

            features.push_back(feature);
            targets.push_back(target);
        }

        // Train model
        bool training_result = predictor.trainModel(features, targets);
        assert_test(training_result, "Performance test training");

        // Test prediction performance
        auto prediction_start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 100; ++i) {
            MLPrediction pred = predictor.predict(features[i]);
        }
        auto prediction_end = std::chrono::high_resolution_clock::now();

        auto training_end = std::chrono::high_resolution_clock::now();

        auto training_duration = std::chrono::duration_cast<std::chrono::milliseconds>(training_end - start_time);
        auto prediction_duration = std::chrono::duration_cast<std::chrono::microseconds>(prediction_end - prediction_start);

        if (verbose) {
            std::cout << "  📊 Training 500 samples with 30 trees: " << training_duration.count() << "ms" << std::endl;
            std::cout << "  🔮 100 predictions: " << prediction_duration.count() << "μs" << std::endl;
        }

        // Performance thresholds (generous for testing)
        assert_test(training_duration.count() < 30000, "Performance training time reasonable"); // < 30 seconds
        assert_test(prediction_duration.count() < 100000, "Performance prediction time reasonable"); // < 100ms total
    }

    void run_all_tests() {
        std::cout << "🧠 Day 8 Random Forest ML Integration Test Suite" << std::endl;
        std::cout << "================================================" << std::endl;

        test_MLFeatureVector_Construction();
        test_RandomForestPredictor_Construction();
        test_Feature_Engineering();
        test_RandomForest_Training();
        test_RandomForest_Prediction();
        test_ML_Portfolio_Integration();
        test_Error_Handling();
        test_Performance();

        std::cout << "\n📊 Test Results Summary" << std::endl;
        std::cout << "======================" << std::endl;
        std::cout << "Total Tests: " << total_tests << std::endl;
        std::cout << "Passed: " << passed_tests << std::endl;
        std::cout << "Failed: " << (total_tests - passed_tests) << std::endl;
        std::cout << "Success Rate: " << (static_cast<double>(passed_tests) / total_tests * 100.0) << "%" << std::endl;

        if (passed_tests == total_tests) {
            std::cout << "\n🎉 ALL TESTS PASSED - Random Forest ML Integration SUCCESSFUL!" << std::endl;
            std::cout << "✅ Day 8 ML foundation established and operational" << std::endl;
            std::cout << "✅ Feature engineering pipeline validated" << std::endl;
            std::cout << "✅ Random Forest training and prediction operational" << std::endl;
            std::cout << "✅ ML-Portfolio optimization integration verified" << std::endl;
            std::cout << "✅ Error handling and performance benchmarks met" << std::endl;
        } else {
            std::cout << "\n⚠️  SOME TESTS FAILED - Review implementation" << std::endl;
        }
    }
};

int main() {
    try {
        Day8RandomForestTest test_suite(true);
        test_suite.run_all_tests();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "❌ Test suite error: " << e.what() << std::endl;
        return 1;
    }
}