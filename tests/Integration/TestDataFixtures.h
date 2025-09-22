#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <random>
#include "../../src/Core/Database/Models/PortfolioData.h"
#include "../../src/Core/Database/Models/MarketData.h"
#include "../../src/Core/Database/Models/SentimentData.h"

using namespace CryptoClaude::Database::Models;

/**
 * Hybrid test data provider combining fixtures and generators
 * - Fixtures: Predefined test data for consistent scenarios
 * - Generators: Dynamic data creation for stress/property testing
 */
class TestDataFixtures {
private:
    std::mt19937 rng_;
    std::uniform_real_distribution<double> priceDistribution_;
    std::uniform_real_distribution<double> volumeDistribution_;
    std::uniform_real_distribution<double> sentimentDistribution_;
    std::uniform_int_distribution<int> countDistribution_;

    std::vector<std::string> cryptoSymbols_ = {
        "BTC", "ETH", "ADA", "SOL", "MATIC", "DOT", "LINK", "AVAX", "UNI", "ATOM"
    };

    std::vector<std::string> strategyNames_ = {
        "MOMENTUM", "MEAN_REVERSION", "BREAKOUT", "PAIRS_TRADING", "ARBITRAGE",
        "LONG_SHORT", "VOLATILITY", "SENTIMENT_DRIVEN", "TECHNICAL_ANALYSIS"
    };

    std::vector<std::string> newsSources_ = {
        "CoinDesk", "CoinTelegraph", "CryptoNews", "Decrypt", "The Block",
        "CoinMarketCap", "CryptoSlate", "Bitcoin.com", "NewsBTC"
    };

public:
    TestDataFixtures() :
        rng_(std::chrono::steady_clock::now().time_since_epoch().count()),
        priceDistribution_(100.0, 100000.0),
        volumeDistribution_(10000.0, 10000000.0),
        sentimentDistribution_(-1.0, 1.0),
        countDistribution_(1, 100) {}

    // === PORTFOLIO FIXTURES ===

    /**
     * Creates a standard portfolio for basic testing
     */
    Portfolio createSamplePortfolio(const std::string& strategy = "TEST_STRATEGY",
                                   double initialCash = 100000.0,
                                   double maxLeverage = 3.0) {
        Portfolio portfolio(strategy, initialCash, maxLeverage);
        portfolio.setTimestamp(std::chrono::system_clock::now());
        portfolio.setTotalValue(initialCash);
        portfolio.setCashBalance(initialCash * 0.8); // 80% cash, 20% in positions
        portfolio.setPortfolioStopLevel(initialCash * 0.7); // Stop at 30% loss
        portfolio.setMaxDrawdownLimit(0.25); // 25% max drawdown
        return portfolio;
    }

    /**
     * Creates a portfolio with high leverage for margin call testing
     */
    Portfolio createHighLeveragePortfolio() {
        Portfolio portfolio("HIGH_LEVERAGE_STRATEGY", 50000.0, 5.0);
        portfolio.setTimestamp(std::chrono::system_clock::now());
        portfolio.setTotalValue(48000.0); // Down 4%
        portfolio.setCashBalance(5000.0); // Low cash
        portfolio.setMarginUsed(40000.0); // High margin usage
        portfolio.setCurrentLeverage(4.8); // Near maximum
        portfolio.setMarginUtilization(0.95); // 95% utilization - triggers margin call
        return portfolio;
    }

    /**
     * Creates a diversified portfolio with multiple positions
     */
    Portfolio createDiversifiedPortfolio() {
        Portfolio portfolio("DIVERSIFIED", 200000.0, 2.5);
        portfolio.setTimestamp(std::chrono::system_clock::now());
        portfolio.setTotalValue(215000.0); // Up 7.5%
        portfolio.setTotalPnL(15000.0);
        portfolio.setCashBalance(50000.0); // 25% cash
        portfolio.setCurrentLeverage(1.8);
        portfolio.setMarginUsed(120000.0);
        portfolio.setMarginUtilization(0.6); // Safe leverage
        return portfolio;
    }

    /**
     * Generates a random portfolio for property testing
     */
    Portfolio generateRandomPortfolio() {
        std::string strategy = strategyNames_[countDistribution_(rng_) % strategyNames_.size()];
        double initialCash = priceDistribution_(rng_);
        double maxLeverage = 1.5 + (rng_() % 300) / 100.0; // 1.5x to 4.5x

        Portfolio portfolio(strategy, initialCash, maxLeverage);
        portfolio.setTimestamp(std::chrono::system_clock::now());
        portfolio.setTotalValue(initialCash * (0.8 + (rng_() % 50) / 100.0)); // ±20% from initial
        portfolio.setCashBalance(initialCash * (0.2 + (rng_() % 60) / 100.0)); // 20-80% cash

        return portfolio;
    }

    // === POSITION FIXTURES ===

    /**
     * Creates a standard position for testing
     */
    Position createSamplePosition(const std::string& symbol = "BTC",
                                 double quantity = 1.0,
                                 double entryPrice = 45000.0,
                                 bool isLong = true,
                                 double leverage = 2.0) {
        Position position(symbol, quantity, entryPrice, isLong, leverage);
        position.setCurrentPrice(entryPrice * (0.98 + (rng_() % 5) / 100.0)); // ±2% from entry
        position.setStopLossPrice(isLong ? entryPrice * 0.9 : entryPrice * 1.1); // 10% stop loss
        return position;
    }

    /**
     * Creates a position that would trigger stop loss
     */
    Position createStopLossPosition() {
        Position position("ETH", -5.0, 3500.0, false, 2.5); // Short position
        position.setCurrentPrice(3800.0); // Price moved against short
        position.setStopLossPrice(3700.0);
        position.triggerStopLoss(); // Already triggered
        return position;
    }

    /**
     * Generates a random position for property testing
     */
    Position generateRandomPosition() {
        std::string symbol = cryptoSymbols_[countDistribution_(rng_) % cryptoSymbols_.size()];
        double quantity = (rng_() % 1000) / 100.0; // 0.01 to 10.0
        double entryPrice = priceDistribution_(rng_);
        bool isLong = (rng_() % 2) == 0;
        double leverage = 1.0 + (rng_() % 300) / 100.0; // 1.0x to 4.0x

        Position position(symbol, quantity, entryPrice, isLong, leverage);
        position.setCurrentPrice(entryPrice * (0.9 + (rng_() % 21) / 100.0)); // ±10% from entry
        return position;
    }

    // === MARKET DATA FIXTURES ===

    /**
     * Creates standard market data for testing
     */
    MarketData createSampleMarketData(const std::string& symbol = "BTC",
                                     double closePrice = 45000.0,
                                     double volumeFrom = 1000000.0,
                                     double volumeTo = 950000.0) {
        auto now = std::chrono::system_clock::now();
        MarketData data(symbol, now, closePrice, volumeFrom, volumeTo);

        // Add calculated inflow data
        data.setNetInflow(volumeFrom - volumeTo);
        data.setExcessInflow((volumeFrom - volumeTo) * 0.1);
        data.setHourlyInflow((volumeFrom - volumeTo) / 24.0);

        return data;
    }

    /**
     * Creates market data with full technical indicators
     */
    MarketData createMarketDataWithIndicators(const std::string& symbol = "ETH") {
        auto data = createSampleMarketData(symbol, 3500.0, 800000.0, 750000.0);

        // Add technical indicators
        data.setRSI(65.5);
        data.setMACD(-50.0);
        data.setBollingerPosition(0.7);

        // Add sentiment data
        data.setArticleCount(18);
        data.setAverageSentiment(0.4);

        return data;
    }

    /**
     * Generates random market data for property testing
     */
    MarketData generateRandomMarketData() {
        std::string symbol = cryptoSymbols_[countDistribution_(rng_) % cryptoSymbols_.size()];
        double closePrice = priceDistribution_(rng_);
        double volumeFrom = volumeDistribution_(rng_);
        double volumeTo = volumeFrom * (0.8 + (rng_() % 40) / 100.0); // 80-120% of volumeFrom

        auto data = createSampleMarketData(symbol, closePrice, volumeFrom, volumeTo);

        // Randomly add technical indicators
        if ((rng_() % 3) == 0) {
            data.setRSI(rng_() % 100);
            data.setMACD(-500.0 + (rng_() % 1000));
            data.setBollingerPosition(-1.0 + (rng_() % 200) / 100.0);
        }

        return data;
    }

    // === SENTIMENT DATA FIXTURES ===

    /**
     * Creates standard sentiment data for testing
     */
    SentimentData createSampleSentimentData(const std::string& ticker = "BTC",
                                           const std::string& source = "CoinDesk",
                                           const std::string& date = "2025-01-01",
                                           int articleCount = 20,
                                           double avgSentiment = 0.5) {
        SentimentData data(ticker, source, date, articleCount, avgSentiment);
        data.setTimestamp(std::chrono::system_clock::now());

        // Add calculated aggregated metrics
        data.setSentiment1d(avgSentiment * 0.9);
        data.setSentiment7dAvg(avgSentiment * 0.8);
        data.setSentimentTrend(0.1); // Improving trend

        return data;
    }

    /**
     * Creates negative sentiment data for bear market testing
     */
    SentimentData createNegativeSentimentData() {
        return createSampleSentimentData("BTC", "CryptoNews", "2025-01-01", 35, -0.6);
    }

    /**
     * Generates random sentiment data for property testing
     */
    SentimentData generateRandomSentimentData() {
        std::string ticker = cryptoSymbols_[countDistribution_(rng_) % cryptoSymbols_.size()];
        std::string source = newsSources_[countDistribution_(rng_) % newsSources_.size()];
        int articleCount = 1 + (rng_() % 100);
        double sentiment = sentimentDistribution_(rng_);

        return createSampleSentimentData(ticker, source, "2025-01-01", articleCount, sentiment);
    }

    // === NEWS SOURCE AND ARTICLE FIXTURES ===

    /**
     * Creates a standard news source for testing
     */
    NewsSource createSampleNewsSource(const std::string& name = "CoinDesk",
                                     const std::string& url = "https://coindesk.com",
                                     int priority = 1) {
        NewsSource source(name, url, priority);
        source.setActive(true);
        return source;
    }

    /**
     * Creates a news article for testing
     */
    NewsArticle createSampleNewsArticle(const std::string& title = "Crypto Markets Rise",
                                       const std::string& content = "Cryptocurrency markets showing positive momentum...",
                                       const std::string& source = "CoinDesk") {
        NewsArticle article(title, content, source);
        article.setDate("2025-01-01");
        article.setUrl("https://example.com/article");
        article.setSentimentScore(0.6);
        article.addTicker("BTC");
        article.addTicker("ETH");
        article.setProcessed(true);
        return article;
    }

    // === AGGREGATED SENTIMENT FIXTURES ===

    /**
     * Creates aggregated sentiment data for testing
     */
    AggregatedSentiment createSampleAggregatedSentiment(const std::string& ticker = "BTC",
                                                       const std::string& date = "2025-01-01",
                                                       int totalArticles = 50,
                                                       double weightedSentiment = 0.6) {
        AggregatedSentiment aggregated(ticker, date);
        aggregated.setTotalArticles(totalArticles);
        aggregated.setWeightedSentiment(weightedSentiment);
        aggregated.setSentimentVolatility(0.25);
        aggregated.setTopSources({"CoinDesk", "CoinTelegraph", "Decrypt"});
        return aggregated;
    }

    // === BATCH DATA GENERATORS FOR STRESS TESTING ===

    /**
     * Generates a batch of portfolios for bulk testing
     */
    std::vector<Portfolio> generatePortfolioBatch(int count) {
        std::vector<Portfolio> portfolios;
        portfolios.reserve(count);

        for (int i = 0; i < count; ++i) {
            portfolios.push_back(generateRandomPortfolio());
        }

        return portfolios;
    }

    /**
     * Generates a batch of market data for bulk testing
     */
    std::vector<MarketData> generateMarketDataBatch(int count) {
        std::vector<MarketData> marketDataList;
        marketDataList.reserve(count);

        for (int i = 0; i < count; ++i) {
            marketDataList.push_back(generateRandomMarketData());
        }

        return marketDataList;
    }

    /**
     * Generates a complete trading scenario with related data
     */
    struct TradingScenario {
        Portfolio portfolio;
        std::vector<Position> positions;
        std::vector<MarketData> marketData;
        std::vector<SentimentData> sentimentData;
    };

    TradingScenario generateTradingScenario() {
        TradingScenario scenario;

        // Create portfolio
        scenario.portfolio = generateRandomPortfolio();

        // Create 2-5 positions
        int positionCount = 2 + (rng_() % 4);
        for (int i = 0; i < positionCount; ++i) {
            scenario.positions.push_back(generateRandomPosition());
        }

        // Create market data for each position symbol
        for (const auto& position : scenario.positions) {
            auto data = generateRandomMarketData();
            // Override symbol to match position
            MarketData marketData(position.getSymbol(),
                                std::chrono::system_clock::now(),
                                position.getCurrentPrice(),
                                volumeDistribution_(rng_),
                                volumeDistribution_(rng_));
            scenario.marketData.push_back(marketData);
        }

        // Create sentiment data for each symbol
        for (const auto& position : scenario.positions) {
            auto sentiment = generateRandomSentimentData();
            // Create new sentiment with correct ticker
            SentimentData sentimentData(position.getSymbol(),
                                      newsSources_[rng_() % newsSources_.size()],
                                      "2025-01-01",
                                      1 + (rng_() % 50),
                                      sentimentDistribution_(rng_));
            scenario.sentimentData.push_back(sentimentData);
        }

        return scenario;
    }
};