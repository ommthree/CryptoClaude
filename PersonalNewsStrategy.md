# Personal News & Sentiment Strategy - 50-100 Coin Universe

## Expanded Coin Selection Strategy

### Target Universe: 50-100 Most Liquid Coins (Excluding Stablecoins)
**Rationale for Expansion:**
- ✅ **Proper diversification** across market sectors
- ✅ **Sufficient pair formation options** for long-short strategies
- ✅ **Market sector representation** (L1, DeFi, Gaming, AI, etc.)
- ✅ **Liquidity-based selection** ensures tradeable pairs
- ✅ **Dynamic universe** allows for market evolution

### Coin Selection Methodology
```cpp
// Market cap + liquidity based selection
struct CoinSelectionCriteria {
    double minMarketCapUSD = 100000000;      // $100M minimum
    double min24hVolumeUSD = 10000000;       // $10M daily volume
    bool excludeStablecoins = true;          // No USDT, USDC, etc.
    bool excludeWrappedTokens = true;        // No WBTC, WETH on other chains
    int maxCoinsPerSector = 15;              // Sector diversification

    // Market sectors for balanced representation
    std::vector<std::string> targetSectors = {
        "Layer1", "DeFi", "Smart Contract", "Oracle", "Gaming",
        "AI/ML", "Privacy", "Interoperability", "Storage", "Infrastructure"
    };
};
```

## News Data Collection Strategy

### CryptoNews API (Primary Source)
**Advantages of Your Trial Basic Plan:**
- ✅ Historical news data access
- ✅ Built-in positive/negative sentiment flags
- ✅ Per-coin sentiment scores
- ✅ Higher request limits than NewsAPI

**Collection Strategy for 50-100 Coins:**
1. **Tiered Historical Loading**:
   - **Tier 1 (Top 20)**: Full 2-year historical data
   - **Tier 2 (21-50)**: 1-year historical data
   - **Tier 3 (51-100)**: 6-month historical data + market-wide sentiment

2. **Smart Prioritization System**:
   - **Market leaders (BTC, ETH)**: 25% of news requests
   - **Large caps (Top 20)**: 50% of news requests
   - **Mid caps (21-50)**: 20% of news requests
   - **Smaller caps (51-100)**: 5% of news requests

3. **News Categories to Focus On**:
   - Price movements and technical analysis
   - Adoption and partnerships
   - Regulatory developments
   - Technology updates and upgrades
   - Market sentiment and analyst opinions

### NewsAPI (Backup/Validation)
**Conservative Usage (1000/month limit)**:
- 25 requests/month per target coin (125 total)
- General "cryptocurrency" market news (100 requests/month)
- Emergency backup when CryptoNews hits limits

## Sentiment Processing Strategy

### Leveraging CryptoNews Built-in Sentiment
```cpp
// CryptoNews provides ready-to-use sentiment data:
struct CryptoNewsArticle {
    std::string sentimentCategory; // "BULLISH", "BEARISH", "NEUTRAL", "MIXED"
    double confidenceScore;        // 0-1 confidence in sentiment
    std::map<std::string, double> coinSentimentScores; // Per-coin scores

    // Convert to our ML features directly:
    double getBullishScore() {
        return (sentimentCategory == "BULLISH") ? confidenceScore : 0.0;
    }

    double getBearishScore() {
        return (sentimentCategory == "BEARISH") ? confidenceScore : 0.0;
    }
}
```

### Multi-Timeframe Sentiment Features
1. **Immediate (1-day)**: Real-time sentiment for short-term signals
2. **Short-term (7-day)**: Weekly sentiment trend
3. **Medium-term (30-day)**: Monthly sentiment momentum
4. **Long-term (90-day)**: Quarterly sentiment baseline

### Smart Caching for News Data
```cpp
// Never refetch the same historical news
class NewsDataCache {
    // Permanent storage for historical articles
    bool hasHistoricalNews(string symbol, string date);

    // Smart deduplication by article URL and content hash
    bool isDuplicateArticle(string url, string contentHash);

    // Sentiment preprocessing and storage
    void storeSentimentFeatures(string symbol, string date,
                               double bullish, double bearish, double neutral);
}
```

## Implementation Priorities

### Phase 1: Historical Data + Pair Screening (Day 16)
1. **Historical Data Loading**:
   - Tier 1 (20 coins): 730 days full historical data
   - Tier 2 (30 coins): 365 days historical data
   - Tier 3 (25 coins): 180 days historical data
2. **Initial Pair Screening**: Reduce 2,775 pairs to ~500 viable candidates
3. **Correlation Analysis**: Calculate rolling correlations for pair identification
4. **Sentiment Feature Extraction**: Convert CryptoNews positive/negative flags to ML features

### Phase 2: Extensive Backtesting + Pair Calibration (Day 17)
1. **Comprehensive Pair Backtesting**: Test all ~500 candidate pairs across market conditions
2. **Dynamic Concentration Calibration**: Optimize pair counts for different market regimes
3. **Pair Quality Scoring**: Rank pairs by performance, stability, and diversification
4. **Market Regime Analysis**: Identify optimal strategies for bull/bear/sideways markets

### Phase 3: Integration + Production Optimization (Day 18)
1. **ML Pipeline Integration**: Feed calibrated pair selections to Random Forest
2. **Dynamic Concentration Implementation**: Real-time concentration adjustments
3. **Performance Validation**: Out-of-sample testing of calibrated parameters
4. **Cost Optimization**: Fine-tune API usage patterns for ongoing operations

### Phase 4: Advanced Features + Deployment (Day 19)
1. **Walk-Forward Validation**: Rolling optimization of pair selections
2. **Stress Testing**: Validate performance under extreme market conditions
3. **Real-time Monitoring**: Deploy concentration and pair selection systems
4. **Documentation**: Complete deployment guides and operational procedures

## Expected Performance Benefits

### Data Quality Advantages
- **Ready-to-use sentiment**: No complex NLP processing needed
- **Professional quality**: CryptoNews provides curated, crypto-focused content
- **Historical consistency**: Same sentiment methodology across time periods
- **Per-coin specificity**: Targeted sentiment scores for each trading symbol

### Cost Efficiency
- **Reduced processing**: No need for sentiment analysis infrastructure
- **API conservation**: Use built-in features vs multiple processing calls
- **Smart caching**: Permanent historical storage, minimal ongoing requests
- **Focused targeting**: 5 coins vs broad market scanning

### ML Performance Expected
- **Higher signal quality**: Crypto-specific news vs general financial news
- **Faster training**: Pre-processed sentiment features vs raw text
- **Better accuracy**: Professional sentiment analysis vs basic NLP
- **Consistent features**: Stable sentiment methodology for model training

This strategy leverages your CryptoNews Trial Basic plan's advantages while staying within personal-use API limits and providing high-quality sentiment data for ML validation.