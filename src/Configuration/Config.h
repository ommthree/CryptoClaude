#pragma once
#include <string>

namespace CryptoClaude {
namespace Config {

// API Configuration
constexpr const char* CRYPTOCOMPARE_API_KEY = "d6d220e8865d9b2ce4a32f24235dab4d2a62eeb1b10fabc348f3a3a5acf85e6d";
constexpr const char* CRYPTOCOMPARE_BASE_URL = "https://min-api.cryptocompare.com/data/";
constexpr const char* CRYPTO_NEWS_API_KEY = "i4uz1l78t5lfcg0hxm0ue9wvytquswzyqknxafbs";
constexpr const char* CRYPTO_NEWS_BASE_URL = "https://cryptonews-api.com/api/v1";

// Database Configuration
constexpr const char* DATABASE_PATH = "crypto_claude.db";

// Data Collection Parameters
constexpr int DEFAULT_COIN_COUNT = 300;
constexpr int DEFAULT_HISTORICAL_DAYS = 1000;
constexpr int COINS_PER_NEWS_BATCH = 25;
constexpr int DAYS_PER_NEWS_BATCH = 1;
constexpr int MAX_NEWS_ARTICLES = 100;
constexpr int MAX_RETRIES = 5;
constexpr int INITIAL_BACKOFF_MS = 500;

// Model Training Parameters
constexpr int NUM_FEATURES = 4;
constexpr int RANDOM_FOREST_TREES = 30;
constexpr double SAMPLE_RATIO = 0.9;
constexpr int RANDOM_SEED = 42;

// Strategy Parameters
constexpr int LONG_POSITIONS = 10;
constexpr int SHORT_POSITIONS = 10;
constexpr double MIN_R_SQUARED = 0.3;
constexpr double MAX_LAMBDA_DEVIATION = 0.5;

// Leverage and Risk Management Parameters
constexpr double DEFAULT_MAX_LEVERAGE = 3.0;         // Default 3:1 leverage (configurable)
constexpr double MAX_LEVERAGE_LIMIT = 5.0;           // Absolute maximum leverage allowed
constexpr double MARGIN_UTILIZATION_LIMIT = 0.80;   // Use max 80% of available margin
constexpr double POSITION_STOP_LOSS = -0.05;        // 5% stop-loss per position
constexpr double PORTFOLIO_STOP_LOSS = -0.15;       // 15% portfolio-level stop-loss
constexpr double MARGIN_CALL_THRESHOLD = 0.90;      // Reduce positions at 90% margin usage

// Position Precision
constexpr int CRYPTO_DECIMAL_PRECISION = 8;         // 8 decimal places for crypto quantities
constexpr int PRICE_DECIMAL_PRECISION = 8;          // 8 decimal places for crypto prices

// Utility Constants
constexpr double SCALING_FACTOR = 1000000.0;
constexpr double EPSILON = 1e-9;

} // namespace Config
} // namespace CryptoClaude