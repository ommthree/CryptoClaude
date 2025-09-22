#include <gtest/gtest.h>
#include <memory>
#include <chrono>
#include <vector>
#include "../../src/Core/Database/DatabaseManager.h"
#include "../../src/Core/Database/Repositories/PortfolioRepository.h"
#include "../../src/Core/Database/Repositories/MarketDataRepository.h"
#include "../../src/Core/Database/Repositories/SentimentRepository.h"
#include "TestDataFixtures.h"

using namespace CryptoClaude::Database;
using namespace CryptoClaude::Database::Repositories;
using namespace CryptoClaude::Database::Models;

class RepositoryIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Use in-memory database for tests
        dbManager_ = std::make_unique<DatabaseManager>(":memory:");
        ASSERT_TRUE(dbManager_->initialize()) << "Failed to initialize database";

        // Create repositories
        portfolioRepo_ = std::make_unique<PortfolioRepository>(*dbManager_);
        marketDataRepo_ = std::make_unique<MarketDataRepository>(*dbManager_);
        sentimentRepo_ = std::make_unique<SentimentRepository>(*dbManager_);

        // Create test fixtures
        fixtures_ = std::make_unique<TestDataFixtures>();
    }

    void TearDown() override {
        portfolioRepo_.reset();
        marketDataRepo_.reset();
        sentimentRepo_.reset();
        dbManager_.reset();
    }

    std::unique_ptr<DatabaseManager> dbManager_;
    std::unique_ptr<PortfolioRepository> portfolioRepo_;
    std::unique_ptr<MarketDataRepository> marketDataRepo_;
    std::unique_ptr<SentimentRepository> sentimentRepo_;
    std::unique_ptr<TestDataFixtures> fixtures_;
};

// === PORTFOLIO REPOSITORY INTEGRATION TESTS ===

TEST_F(RepositoryIntegrationTest, Portfolio_CreateRetrieveUpdate_Success) {
    // Arrange
    auto portfolio = fixtures_->createSamplePortfolio("MOMENTUM_STRATEGY", 100000.0, 3.0);

    // Act - Insert
    bool insertResult = portfolioRepo_->insertPortfolio(portfolio);
    ASSERT_TRUE(insertResult) << "Failed to insert portfolio";

    // Act - Retrieve
    auto retrievedPortfolio = portfolioRepo_->getPortfolioById(1);
    ASSERT_TRUE(retrievedPortfolio.has_value()) << "Failed to retrieve portfolio";

    // Assert - Verify data integrity
    EXPECT_EQ(retrievedPortfolio->getStrategyName(), "MOMENTUM_STRATEGY");
    EXPECT_DOUBLE_EQ(retrievedPortfolio->getTotalValue(), 100000.0);
    EXPECT_DOUBLE_EQ(retrievedPortfolio->getMaxAllowedLeverage(), 3.0);

    // Act - Update
    retrievedPortfolio->setTotalValue(120000.0);
    retrievedPortfolio->setTotalPnL(20000.0);
    bool updateResult = portfolioRepo_->updatePortfolio(retrievedPortfolio.value());
    ASSERT_TRUE(updateResult) << "Failed to update portfolio";

    // Assert - Verify update
    auto updatedPortfolio = portfolioRepo_->getPortfolioById(1);
    ASSERT_TRUE(updatedPortfolio.has_value());
    EXPECT_DOUBLE_EQ(updatedPortfolio->getTotalValue(), 120000.0);
    EXPECT_DOUBLE_EQ(updatedPortfolio->getTotalPnL(), 20000.0);
}

TEST_F(RepositoryIntegrationTest, Portfolio_WithPositions_CascadeOperations) {
    // Arrange
    auto portfolio = fixtures_->createSamplePortfolio("LONG_SHORT", 50000.0, 2.5);
    auto position1 = fixtures_->createSamplePosition("BTC", 0.5, 45000.0, true, 2.0);
    auto position2 = fixtures_->createSamplePosition("ETH", -10.0, 3500.0, false, 1.5);

    // Act - Insert portfolio and positions
    ASSERT_TRUE(portfolioRepo_->insertPortfolio(portfolio));

    position1.setPortfolioId(1);
    position2.setPortfolioId(1);
    ASSERT_TRUE(portfolioRepo_->insertPosition(position1));
    ASSERT_TRUE(portfolioRepo_->insertPosition(position2));

    // Act - Retrieve positions for portfolio
    auto positions = portfolioRepo_->getPositionsByPortfolio(1);

    // Assert
    EXPECT_EQ(positions.size(), 2);
    EXPECT_EQ(positions[0].getSymbol(), "BTC");
    EXPECT_EQ(positions[1].getSymbol(), "ETH");
    EXPECT_TRUE(positions[0].isLong());
    EXPECT_FALSE(positions[1].isLong());

    // Act - Delete portfolio (should cascade to positions)
    ASSERT_TRUE(portfolioRepo_->deletePortfolio(1));

    // Assert - Verify cascade deletion
    auto deletedPositions = portfolioRepo_->getPositionsByPortfolio(1);
    EXPECT_TRUE(deletedPositions.empty());
}

TEST_F(RepositoryIntegrationTest, Portfolio_LeverageManagement_BusinessLogic) {
    // Arrange
    auto portfolio = fixtures_->createHighLeveragePortfolio();

    // Act
    ASSERT_TRUE(portfolioRepo_->insertPortfolio(portfolio));

    // Test margin call detection
    auto marginCallPortfolios = portfolioRepo_->getPortfoliosRequiringMarginCall();

    // Assert
    EXPECT_EQ(marginCallPortfolios.size(), 1);
    EXPECT_EQ(marginCallPortfolios[0].getStrategyName(), portfolio.getStrategyName());
    EXPECT_TRUE(marginCallPortfolios[0].requiresMarginCall());
}

// === MARKET DATA REPOSITORY INTEGRATION TESTS ===

TEST_F(RepositoryIntegrationTest, MarketData_FullLifecycle_WithSentiment) {
    // Arrange
    auto marketData = fixtures_->createSampleMarketData("BTC", 45000.0, 1000000.0, 950000.0);
    marketData.setAverageSentiment(0.65); // Positive sentiment
    marketData.setArticleCount(25);

    // Act - Insert
    ASSERT_TRUE(marketDataRepo_->insertMarketData(marketData));

    // Act - Retrieve latest
    auto retrieved = marketDataRepo_->getLatestMarketData("BTC");
    ASSERT_TRUE(retrieved.has_value());

    // Assert - Data integrity
    EXPECT_EQ(retrieved->getSymbol(), "BTC");
    EXPECT_DOUBLE_EQ(retrieved->getClose(), 45000.0);
    EXPECT_TRUE(retrieved->hasSentimentData());
    EXPECT_DOUBLE_EQ(retrieved->getAverageSentiment().value(), 0.65);

    // Act - Update with technical indicators
    retrieved->setRSI(72.5);
    retrieved->setMACD(-150.0);
    retrieved->setBollingerPosition(0.8);
    ASSERT_TRUE(marketDataRepo_->updateMarketData(retrieved.value()));

    // Assert - Technical indicators saved
    auto updated = marketDataRepo_->getLatestMarketData("BTC");
    ASSERT_TRUE(updated.has_value());
    EXPECT_DOUBLE_EQ(updated->getRSI().value(), 72.5);
    EXPECT_DOUBLE_EQ(updated->getMACD().value(), -150.0);
    EXPECT_DOUBLE_EQ(updated->getBollingerPosition().value(), 0.8);
}

TEST_F(RepositoryIntegrationTest, MarketData_VolumeAnalysis_TopPerformers) {
    // Arrange - Create multiple market data entries for same date
    std::vector<MarketData> marketDataList = {
        fixtures_->createSampleMarketData("BTC", 45000.0, 2000000.0, 1800000.0),
        fixtures_->createSampleMarketData("ETH", 3500.0, 1500000.0, 1400000.0),
        fixtures_->createSampleMarketData("ADA", 1.20, 500000.0, 480000.0),
        fixtures_->createSampleMarketData("SOL", 180.0, 800000.0, 750000.0)
    };

    // Insert all data
    for (const auto& data : marketDataList) {
        ASSERT_TRUE(marketDataRepo_->insertMarketData(data));
    }

    // Act - Get top volume performers
    auto topVolume = marketDataRepo_->getTopVolumeForDate("2025-01-01", 3);

    // Assert - Should return top 3 by total volume
    EXPECT_EQ(topVolume.size(), 3);
    EXPECT_EQ(topVolume[0].getSymbol(), "BTC"); // Highest total volume
    EXPECT_EQ(topVolume[1].getSymbol(), "ETH"); // Second highest
    EXPECT_EQ(topVolume[2].getSymbol(), "SOL"); // Third highest
}

// === SENTIMENT REPOSITORY INTEGRATION TESTS ===

TEST_F(RepositoryIntegrationTest, Sentiment_FullWorkflow_NewsToAggregated) {
    // Arrange
    auto newsSource = fixtures_->createSampleNewsSource("CoinDesk", "https://coindesk.com", 1);
    auto newsArticle = fixtures_->createSampleNewsArticle("Bitcoin Surges", "BTC hits new highs", "CoinDesk");
    auto sentimentData = fixtures_->createSampleSentimentData("BTC", "CoinDesk", "2025-01-01", 15, 0.7);

    // Act - Insert news source and article
    ASSERT_TRUE(sentimentRepo_->insertNewsSource(newsSource));
    ASSERT_TRUE(sentimentRepo_->insertNewsArticle(newsArticle));

    // Act - Insert sentiment data
    ASSERT_TRUE(sentimentRepo_->insertSentimentData(sentimentData));

    // Act - Retrieve sentiment for ticker
    auto sentiments = sentimentRepo_->getSentimentByTicker("BTC", 5);
    ASSERT_FALSE(sentiments.empty());

    // Assert
    EXPECT_EQ(sentiments[0].getTicker(), "BTC");
    EXPECT_EQ(sentiments[0].getSourceName(), "CoinDesk");
    EXPECT_DOUBLE_EQ(sentiments[0].getAvgSentiment(), 0.7);
    EXPECT_TRUE(sentiments[0].hasSignificantSentiment());
    EXPECT_TRUE(sentiments[0].isPositiveSentiment());

    // Act - Create aggregated sentiment
    auto aggregated = fixtures_->createSampleAggregatedSentiment("BTC", "2025-01-01", 45, 0.65);
    ASSERT_TRUE(sentimentRepo_->insertAggregatedSentiment(aggregated));

    // Act - Retrieve aggregated
    auto retrievedAgg = sentimentRepo_->getAggregatedSentiment("BTC", "2025-01-01");
    ASSERT_TRUE(retrievedAgg.has_value());

    // Assert
    EXPECT_TRUE(retrievedAgg->hasSignificantCoverage());
    EXPECT_DOUBLE_EQ(retrievedAgg->getWeightedSentiment(), 0.65);
}

// === CROSS-REPOSITORY INTEGRATION TESTS ===

TEST_F(RepositoryIntegrationTest, CrossRepository_TradingScenario_FullIntegration) {
    // Arrange - Create a complete trading scenario

    // 1. Market data shows BTC is trending up
    auto marketData = fixtures_->createSampleMarketData("BTC", 45000.0, 2000000.0, 1900000.0);
    marketData.setRSI(65.0); // Bullish but not overbought
    ASSERT_TRUE(marketDataRepo_->insertMarketData(marketData));

    // 2. Sentiment is positive
    auto sentimentData = fixtures_->createSampleSentimentData("BTC", "Multiple", "2025-01-01", 50, 0.6);
    ASSERT_TRUE(sentimentRepo_->insertSentimentData(sentimentData));

    // 3. Portfolio takes a leveraged long position
    auto portfolio = fixtures_->createSamplePortfolio("MOMENTUM", 100000.0, 3.0);
    ASSERT_TRUE(portfolioRepo_->insertPortfolio(portfolio));

    auto position = fixtures_->createSamplePosition("BTC", 2.0, 45000.0, true, 2.5);
    position.setPortfolioId(1);
    ASSERT_TRUE(portfolioRepo_->insertPosition(position));

    // Act - Simulate price movement and portfolio update
    marketData.setClose(47000.0); // 4.4% price increase
    ASSERT_TRUE(marketDataRepo_->updateMarketData(marketData));

    // Update position with new price
    position.setCurrentPrice(47000.0);
    // PnL should be calculated: 2.0 * (47000 - 45000) = 4000
    ASSERT_TRUE(portfolioRepo_->updatePosition(position));

    // Update portfolio total value
    portfolio.setTotalValue(104000.0); // Profit from position
    portfolio.setTotalPnL(4000.0);
    ASSERT_TRUE(portfolioRepo_->updatePortfolio(portfolio));

    // Assert - Verify integrated scenario
    auto updatedMarketData = marketDataRepo_->getLatestMarketData("BTC");
    auto updatedPortfolio = portfolioRepo_->getPortfolioById(1);
    auto updatedPositions = portfolioRepo_->getPositionsByPortfolio(1);
    auto latestSentiment = sentimentRepo_->getLatestSentimentForTicker("BTC");

    ASSERT_TRUE(updatedMarketData.has_value());
    ASSERT_TRUE(updatedPortfolio.has_value());
    ASSERT_FALSE(updatedPositions.empty());
    ASSERT_TRUE(latestSentiment.has_value());

    // Verify the trading scenario results
    EXPECT_DOUBLE_EQ(updatedMarketData->getClose(), 47000.0);
    EXPECT_DOUBLE_EQ(updatedPortfolio->getTotalPnL(), 4000.0);
    EXPECT_DOUBLE_EQ(updatedPositions[0].getCurrentPrice(), 47000.0);
    EXPECT_TRUE(latestSentiment->isPositiveSentiment());

    // Verify business logic
    EXPECT_TRUE(updatedPortfolio->getTotalPnL() > 0); // Profitable
    EXPECT_FALSE(updatedPortfolio->requiresMarginCall()); // Safe leverage
    EXPECT_TRUE(updatedMarketData->hasValidPrice()); // Data integrity
}

// === PERFORMANCE AND STRESS TESTS ===

TEST_F(RepositoryIntegrationTest, Performance_BulkOperations_AcceptableSpeed) {
    const int BATCH_SIZE = 1000;
    auto startTime = std::chrono::high_resolution_clock::now();

    // Bulk insert market data
    for (int i = 0; i < BATCH_SIZE; ++i) {
        auto data = fixtures_->createSampleMarketData(
            "SYM" + std::to_string(i),
            1000.0 + i,
            100000.0 + i * 1000,
            95000.0 + i * 1000
        );
        ASSERT_TRUE(marketDataRepo_->insertMarketData(data));
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    // Assert performance target (should complete in under 5 seconds)
    EXPECT_LT(duration.count(), 5000) << "Bulk insert took " << duration.count() << "ms";
}

TEST_F(RepositoryIntegrationTest, DataConsistency_ConcurrentOperations_NoCorruption) {
    // This test would verify data consistency under concurrent access
    // For now, we'll test sequential operations that simulate race conditions

    auto portfolio = fixtures_->createSamplePortfolio("TEST", 50000.0, 2.0);
    ASSERT_TRUE(portfolioRepo_->insertPortfolio(portfolio));

    // Simulate multiple rapid updates
    for (int i = 0; i < 10; ++i) {
        auto retrieved = portfolioRepo_->getPortfolioById(1);
        ASSERT_TRUE(retrieved.has_value());

        retrieved->setTotalValue(50000.0 + i * 1000);
        retrieved->setTotalPnL(i * 100);
        ASSERT_TRUE(portfolioRepo_->updatePortfolio(retrieved.value()));
    }

    // Verify final state is consistent
    auto final = portfolioRepo_->getPortfolioById(1);
    ASSERT_TRUE(final.has_value());
    EXPECT_DOUBLE_EQ(final->getTotalValue(), 59000.0);
    EXPECT_DOUBLE_EQ(final->getTotalPnL(), 900.0);
}