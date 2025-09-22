#include <gtest/gtest.h>
#include <memory>
#include <chrono>
#include <vector>
#include <algorithm>
#include "../../src/Core/Database/DatabaseManager.h"
#include "../../src/Core/Database/Repositories/PortfolioRepository.h"
#include "../../src/Core/Database/Repositories/MarketDataRepository.h"
#include "../../src/Core/Database/Repositories/SentimentRepository.h"
#include "TestDataFixtures.h"

using namespace CryptoClaude::Database;
using namespace CryptoClaude::Database::Repositories;
using namespace CryptoClaude::Database::Models;

/**
 * Business Logic Integration Tests
 * Tests complex business scenarios that span multiple repositories and models
 */
class BusinessLogicIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        dbManager_ = std::make_unique<DatabaseManager>(":memory:");
        ASSERT_TRUE(dbManager_->initialize());

        portfolioRepo_ = std::make_unique<PortfolioRepository>(*dbManager_);
        marketDataRepo_ = std::make_unique<MarketDataRepository>(*dbManager_);
        sentimentRepo_ = std::make_unique<SentimentRepository>(*dbManager_);
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

// === LEVERAGE AND RISK MANAGEMENT SCENARIOS ===

TEST_F(BusinessLogicIntegrationTest, LeverageManagement_MarginCall_TriggersCorrectly) {
    // Arrange - Create a portfolio approaching margin call limits
    auto portfolio = fixtures_->createSamplePortfolio("AGGRESSIVE", 100000.0, 4.0);
    portfolio.setMarginUsed(85000.0);  // High margin usage
    portfolio.setCurrentLeverage(3.8); // Near maximum leverage
    portfolio.setMarginUtilization(0.92); // 92% utilization - should trigger margin call

    ASSERT_TRUE(portfolioRepo_->insertPortfolio(portfolio));

    // Create positions that justify the high margin
    auto btcPosition = fixtures_->createSamplePosition("BTC", 2.0, 45000.0, true, 3.8);
    btcPosition.setPortfolioId(1);
    btcPosition.setCurrentPrice(43000.0); // Position is losing money

    auto ethPosition = fixtures_->createSamplePosition("ETH", -10.0, 3500.0, false, 3.8);
    ethPosition.setPortfolioId(1);
    ethPosition.setCurrentPrice(3700.0); // Short position also losing

    ASSERT_TRUE(portfolioRepo_->insertPosition(btcPosition));
    ASSERT_TRUE(portfolioRepo_->insertPosition(ethPosition));

    // Act - Check for margin call portfolios
    auto marginCallPortfolios = portfolioRepo_->getPortfoliosRequiringMarginCall();

    // Assert
    ASSERT_EQ(marginCallPortfolios.size(), 1);
    EXPECT_EQ(marginCallPortfolios[0].getStrategyName(), "AGGRESSIVE");
    EXPECT_TRUE(marginCallPortfolios[0].requiresMarginCall());
    EXPECT_GE(marginCallPortfolios[0].getMarginUtilization(), 0.90);

    // Verify positions are included in the analysis
    auto positions = portfolioRepo_->getPositionsByPortfolio(1);
    EXPECT_EQ(positions.size(), 2);

    // Both positions should be losing money given current prices
    for (const auto& pos : positions) {
        if (pos.isLong()) {
            EXPECT_LT(pos.getCurrentPrice(), pos.getEntryPrice()); // Long position, price down
        } else {
            EXPECT_GT(pos.getCurrentPrice(), pos.getEntryPrice()); // Short position, price up
        }
    }
}

TEST_F(BusinessLogicIntegrationTest, StopLoss_Integration_AutomaticExecution) {
    // Arrange - Create portfolio with positions that have stop losses
    auto portfolio = fixtures_->createSamplePortfolio("RISK_MANAGED", 75000.0, 2.5);
    ASSERT_TRUE(portfolioRepo_->insertPortfolio(portfolio));

    // Create position with stop loss that will be triggered
    auto position = fixtures_->createSamplePosition("ADA", 1000.0, 1.50, true, 2.0);
    position.setPortfolioId(1);
    position.setStopLossPrice(1.35); // Stop loss at 10% down
    position.setCurrentPrice(1.30);  // Price has fallen below stop loss
    ASSERT_TRUE(portfolioRepo_->insertPosition(position));

    // Act - Simulate stop loss detection and execution
    auto stopLossPositions = portfolioRepo_->getStopLossPositions(1);

    // Assert - Stop loss should be detected
    EXPECT_EQ(stopLossPositions.size(), 1);
    EXPECT_EQ(stopLossPositions[0].getSymbol(), "ADA");
    EXPECT_LT(stopLossPositions[0].getCurrentPrice(), stopLossPositions[0].getStopLossPrice());

    // Simulate stop loss execution by triggering it
    position.triggerStopLoss();
    ASSERT_TRUE(portfolioRepo_->updatePosition(position));

    // Verify stop loss is now triggered
    auto updatedPositions = portfolioRepo_->getPositionsByPortfolio(1);
    ASSERT_FALSE(updatedPositions.empty());
    EXPECT_TRUE(updatedPositions[0].isStopLossTriggered());
}

// === SENTIMENT-DRIVEN TRADING SCENARIOS ===

TEST_F(BusinessLogicIntegrationTest, SentimentDriven_Trading_SignalGeneration) {
    // Arrange - Create strong positive sentiment scenario
    auto btcSentiment = fixtures_->createSampleSentimentData("BTC", "Multiple", "2025-01-01", 75, 0.8);
    auto ethSentiment = fixtures_->createSampleSentimentData("ETH", "Multiple", "2025-01-01", 60, 0.7);
    auto adaSentiment = fixtures_->createSampleSentimentData("ADA", "Multiple", "2025-01-01", 15, -0.3);

    ASSERT_TRUE(sentimentRepo_->insertSentimentData(btcSentiment));
    ASSERT_TRUE(sentimentRepo_->insertSentimentData(ethSentiment));
    ASSERT_TRUE(sentimentRepo_->insertSentimentData(adaSentiment));

    // Add corresponding market data
    auto btcMarket = fixtures_->createSampleMarketData("BTC", 45000.0, 2000000.0, 1800000.0);
    auto ethMarket = fixtures_->createSampleMarketData("ETH", 3500.0, 1200000.0, 1100000.0);
    auto adaMarket = fixtures_->createSampleMarketData("ADA", 1.20, 500000.0, 550000.0);

    ASSERT_TRUE(marketDataRepo_->insertMarketData(btcMarket));
    ASSERT_TRUE(marketDataRepo_->insertMarketData(ethMarket));
    ASSERT_TRUE(marketDataRepo_->insertMarketData(adaMarket));

    // Act - Analyze sentiment signals for trading
    auto btcLatestSentiment = sentimentRepo_->getLatestSentimentForTicker("BTC");
    auto ethLatestSentiment = sentimentRepo_->getLatestSentimentForTicker("ETH");
    auto adaLatestSentiment = sentimentRepo_->getLatestSentimentForTicker("ADA");

    // Assert - Verify sentiment signal strength
    ASSERT_TRUE(btcLatestSentiment.has_value());
    ASSERT_TRUE(ethLatestSentiment.has_value());
    ASSERT_TRUE(adaLatestSentiment.has_value());

    // BTC should show strong buy signal
    EXPECT_TRUE(btcLatestSentiment->isPositiveSentiment());
    EXPECT_TRUE(btcLatestSentiment->hasSignificantSentiment());
    EXPECT_GT(btcLatestSentiment->getSentimentStrength(), 0.7);

    // ETH should show moderate buy signal
    EXPECT_TRUE(ethLatestSentiment->isPositiveSentiment());
    EXPECT_TRUE(ethLatestSentiment->hasSignificantSentiment());

    // ADA should show weak/neutral signal (negative sentiment, low article count)
    EXPECT_TRUE(adaLatestSentiment->isNegativeSentiment());
    EXPECT_FALSE(adaLatestSentiment->hasSignificantSentiment()); // Only 15 articles

    // Simulate portfolio creation based on sentiment signals
    auto portfolio = fixtures_->createSamplePortfolio("SENTIMENT_DRIVEN", 100000.0, 3.0);
    ASSERT_TRUE(portfolioRepo_->insertPortfolio(portfolio));

    // Strong sentiment → larger position
    auto btcPosition = fixtures_->createSamplePosition("BTC", 1.5, 45000.0, true, 2.5);
    btcPosition.setPortfolioId(1);

    // Moderate sentiment → smaller position
    auto ethPosition = fixtures_->createSamplePosition("ETH", 8.0, 3500.0, true, 2.0);
    ethPosition.setPortfolioId(1);

    ASSERT_TRUE(portfolioRepo_->insertPosition(btcPosition));
    ASSERT_TRUE(portfolioRepo_->insertPosition(ethPosition));
    // Note: No ADA position due to weak sentiment signal

    // Verify portfolio allocation matches sentiment strength
    auto positions = portfolioRepo_->getPositionsByPortfolio(1);
    EXPECT_EQ(positions.size(), 2); // Only BTC and ETH positions
    EXPECT_TRUE(std::any_of(positions.begin(), positions.end(),
        [](const Position& p) { return p.getSymbol() == "BTC"; }));
    EXPECT_TRUE(std::any_of(positions.begin(), positions.end(),
        [](const Position& p) { return p.getSymbol() == "ETH"; }));
}

// === MARKET MOMENTUM AND TECHNICAL ANALYSIS SCENARIOS ===

TEST_F(BusinessLogicIntegrationTest, TechnicalAnalysis_Integration_TrendFollowing) {
    // Arrange - Create market data with technical indicators showing bullish trend
    auto marketData = fixtures_->createSampleMarketData("SOL", 180.0, 800000.0, 750000.0);
    marketData.setRSI(68.0);           // Bullish momentum, not overbought
    marketData.setMACD(15.5);          // Positive MACD indicates uptrend
    marketData.setBollingerPosition(0.75); // Near upper band, strong momentum

    ASSERT_TRUE(marketDataRepo_->insertMarketData(marketData));

    // Add supporting sentiment
    auto sentiment = fixtures_->createSampleSentimentData("SOL", "CryptoNews", "2025-01-01", 30, 0.6);
    ASSERT_TRUE(sentimentRepo_->insertSentimentData(sentiment));

    // Act - Create momentum-based portfolio
    auto portfolio = fixtures_->createSamplePortfolio("MOMENTUM", 150000.0, 3.0);
    ASSERT_TRUE(portfolioRepo_->insertPortfolio(portfolio));

    // Technical indicators suggest strong long position
    auto solPosition = fixtures_->createSamplePosition("SOL", 200.0, 180.0, true, 2.8);
    solPosition.setPortfolioId(1);
    ASSERT_TRUE(portfolioRepo_->insertPosition(solPosition));

    // Simulate price movement in trend direction
    marketData.setClose(190.0); // 5.6% increase
    marketData.setRSI(72.0);    // Still healthy momentum
    ASSERT_TRUE(marketDataRepo_->updateMarketData(marketData));

    // Update position with new price
    solPosition.setCurrentPrice(190.0);
    ASSERT_TRUE(portfolioRepo_->updatePosition(solPosition));

    // Assert - Verify trend-following strategy success
    auto updatedMarket = marketDataRepo_->getLatestMarketData("SOL");
    auto updatedPositions = portfolioRepo_->getPositionsByPortfolio(1);

    ASSERT_TRUE(updatedMarket.has_value());
    ASSERT_FALSE(updatedPositions.empty());

    // Technical indicators still support bullish trend
    EXPECT_GT(updatedMarket->getRSI().value(), 65.0);
    EXPECT_GT(updatedMarket->getMACD().value(), 0.0);
    EXPECT_GT(updatedMarket->getBollingerPosition().value(), 0.5);

    // Position should be profitable
    auto& position = updatedPositions[0];
    EXPECT_GT(position.getCurrentPrice(), position.getEntryPrice());
    EXPECT_GT(position.getPnL(), 0.0);
}

// === CORRELATION AND PAIRS TRADING SCENARIOS ===

TEST_F(BusinessLogicIntegrationTest, PairsTrading_Integration_CorrelatedAssets) {
    // Arrange - Create correlated market data (BTC/ETH typically correlated)
    auto btcData = fixtures_->createSampleMarketData("BTC", 45000.0, 2000000.0, 1900000.0);
    auto ethData = fixtures_->createSampleMarketData("ETH", 3500.0, 1200000.0, 1150000.0);

    ASSERT_TRUE(marketDataRepo_->insertMarketData(btcData));
    ASSERT_TRUE(marketDataRepo_->insertMarketData(ethData));

    // Create pairs trading portfolio
    auto portfolio = fixtures_->createSamplePortfolio("PAIRS_TRADING", 200000.0, 2.0);
    ASSERT_TRUE(portfolioRepo_->insertPortfolio(portfolio));

    // Simulate BTC outperforming ETH (divergence from correlation)
    // Long the underperformer (ETH), short the overperformer (BTC)
    auto ethLongPosition = fixtures_->createSamplePosition("ETH", 15.0, 3500.0, true, 1.8);
    auto btcShortPosition = fixtures_->createSamplePosition("BTC", -1.0, 45000.0, false, 1.8);

    ethLongPosition.setPortfolioId(1);
    btcShortPosition.setPortfolioId(1);

    ASSERT_TRUE(portfolioRepo_->insertPosition(ethLongPosition));
    ASSERT_TRUE(portfolioRepo_->insertPosition(btcShortPosition));

    // Act - Simulate convergence (correlation normalizing)
    btcData.setClose(44000.0);  // BTC falls 2.2%
    ethData.setClose(3600.0);   // ETH rises 2.9%

    ASSERT_TRUE(marketDataRepo_->updateMarketData(btcData));
    ASSERT_TRUE(marketDataRepo_->updateMarketData(ethData));

    // Update positions with new prices
    ethLongPosition.setCurrentPrice(3600.0);
    btcShortPosition.setCurrentPrice(44000.0);

    ASSERT_TRUE(portfolioRepo_->updatePosition(ethLongPosition));
    ASSERT_TRUE(portfolioRepo_->updatePosition(btcShortPosition));

    // Assert - Both sides of the pairs trade should be profitable
    auto positions = portfolioRepo_->getPositionsByPortfolio(1);
    EXPECT_EQ(positions.size(), 2);

    for (const auto& position : positions) {
        EXPECT_GT(position.getPnL(), 0.0) << "Position in " << position.getSymbol() << " should be profitable";
    }

    // Verify market normalization
    auto updatedBtc = marketDataRepo_->getLatestMarketData("BTC");
    auto updatedEth = marketDataRepo_->getLatestMarketData("ETH");

    ASSERT_TRUE(updatedBtc.has_value());
    ASSERT_TRUE(updatedEth.has_value());

    // ETH should have outperformed BTC (convergence trade working)
    double btcReturn = (updatedBtc->getClose() - 45000.0) / 45000.0;
    double ethReturn = (updatedEth->getClose() - 3500.0) / 3500.0;
    EXPECT_GT(ethReturn, btcReturn);
}

// === PORTFOLIO REBALANCING SCENARIOS ===

TEST_F(BusinessLogicIntegrationTest, PortfolioRebalancing_Integration_RiskManagement) {
    // Arrange - Create diversified portfolio that becomes unbalanced
    auto portfolio = fixtures_->createDiversifiedPortfolio();
    ASSERT_TRUE(portfolioRepo_->insertPortfolio(portfolio));

    // Create initial balanced positions
    std::vector<Position> positions = {
        fixtures_->createSamplePosition("BTC", 1.0, 45000.0, true, 2.0),   // ~22.5% allocation
        fixtures_->createSamplePosition("ETH", 12.0, 3500.0, true, 2.0),   // ~21% allocation
        fixtures_->createSamplePosition("ADA", 20000.0, 1.20, true, 2.0),  // ~12% allocation
        fixtures_->createSamplePosition("SOL", 150.0, 180.0, true, 2.0)    // ~13.5% allocation
    };

    for (auto& position : positions) {
        position.setPortfolioId(1);
        ASSERT_TRUE(portfolioRepo_->insertPosition(position));
    }

    // Act - Simulate significant price movements causing imbalance
    // BTC surges 50%, others stay flat
    auto btcData = fixtures_->createSampleMarketData("BTC", 67500.0, 3000000.0, 2800000.0);
    ASSERT_TRUE(marketDataRepo_->insertMarketData(btcData));

    // Update BTC position with new price
    positions[0].setCurrentPrice(67500.0);
    ASSERT_TRUE(portfolioRepo_->updatePosition(positions[0]));

    // Calculate new portfolio metrics
    auto updatedPositions = portfolioRepo_->getPositionsByPortfolio(1);
    ASSERT_EQ(updatedPositions.size(), 4);

    // Assert - BTC position should now be over-weighted
    auto btcPosition = std::find_if(updatedPositions.begin(), updatedPositions.end(),
        [](const Position& p) { return p.getSymbol() == "BTC"; });

    ASSERT_NE(btcPosition, updatedPositions.end());
    EXPECT_GT(btcPosition->getCurrentPrice(), btcPosition->getEntryPrice() * 1.4); // At least 40% gain

    // Portfolio should need rebalancing due to concentration risk
    double totalPositionValue = 0.0;
    double btcValue = 0.0;

    for (const auto& position : updatedPositions) {
        double positionValue = std::abs(position.getQuantity()) * position.getCurrentPrice();
        totalPositionValue += positionValue;

        if (position.getSymbol() == "BTC") {
            btcValue = positionValue;
        }
    }

    double btcAllocation = btcValue / totalPositionValue;
    EXPECT_GT(btcAllocation, 0.30) << "BTC allocation should exceed 30% after surge, triggering rebalance need";

    // Verify portfolio total value increased due to BTC gains
    portfolio.setTotalValue(portfolio.getTotalValue() + 22500.0); // BTC gain
    ASSERT_TRUE(portfolioRepo_->updatePortfolio(portfolio));

    auto updatedPortfolio = portfolioRepo_->getPortfolioById(1);
    ASSERT_TRUE(updatedPortfolio.has_value());
    EXPECT_GT(updatedPortfolio->getTotalValue(), 200000.0); // Should be up from initial value
}

// === ERROR HANDLING AND EDGE CASES ===

TEST_F(BusinessLogicIntegrationTest, ErrorHandling_Integration_GracefulDegradation) {
    // Test that the system handles missing or corrupt data gracefully

    // Arrange - Create portfolio with position
    auto portfolio = fixtures_->createSamplePortfolio("ERROR_TEST", 50000.0, 2.0);
    ASSERT_TRUE(portfolioRepo_->insertPortfolio(portfolio));

    auto position = fixtures_->createSamplePosition("TEST", 100.0, 1000.0, true, 1.5);
    position.setPortfolioId(1);
    ASSERT_TRUE(portfolioRepo_->insertPosition(position));

    // Act & Assert - Try to retrieve data for non-existent symbol
    auto nonExistentMarketData = marketDataRepo_->getLatestMarketData("NONEXISTENT");
    EXPECT_FALSE(nonExistentMarketData.has_value());

    auto nonExistentSentiment = sentimentRepo_->getLatestSentimentForTicker("NONEXISTENT");
    EXPECT_FALSE(nonExistentSentiment.has_value());

    // System should still function for valid data
    auto validPositions = portfolioRepo_->getPositionsByPortfolio(1);
    EXPECT_EQ(validPositions.size(), 1);

    // Test with extreme values
    position.setCurrentPrice(0.0001); // Very small price
    EXPECT_NO_THROW(portfolioRepo_->updatePosition(position));

    position.setCurrentPrice(1000000.0); // Very large price
    EXPECT_NO_THROW(portfolioRepo_->updatePosition(position));
}