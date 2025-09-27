# API Usage Calculations for 50-100 Coin Universe

## Expanded Universe Overview

**Target Coins**: 75 coins across 3 tiers
- **Tier 1**: 20 coins (2-year data, high priority)
- **Tier 2**: 30 coins (1-year data, medium priority)
- **Tier 3**: 25 coins (6-month data, lower priority)

## API Request Calculations

### CryptoNews API (Trial Basic Plan)
**Your Plan Details**: Trial Basic - likely 2,000-5,000 requests/month

#### Historical Data Loading (One-Time)
**Tier 1 (Top 20 coins - 2 years)**:
- 20 coins × 730 days = 14,600 requests
- With smart batching: 20 coins × 104 weeks × 1 request = 2,080 requests

**Tier 2 (30 coins - 1 year)**:
- 30 coins × 365 days = 10,950 requests
- With smart batching: 30 coins × 52 weeks × 1 request = 1,560 requests

**Tier 3 (25 coins - 6 months)**:
- 25 coins × 180 days = 4,500 requests
- With smart batching: 25 coins × 26 weeks × 1 request = 650 requests

**Total Historical**: 4,290 requests (over 2-3 months gradual loading)

#### Ongoing News Updates
**Daily operations** (using tiered prioritization):
- Tier 1 coins: 20 coins × 1 request/day = 20 requests/day
- Tier 2 coins: 30 coins × 1 request/2 days = 15 requests/day
- Tier 3 coins: 25 coins × 1 request/3 days = 8 requests/day
- **Total**: 43 requests/day = 1,290 requests/month

### NewsAPI (1,000 requests/month)
**Conservative Backup Usage**:
- General crypto market news: 15 requests/day = 450/month
- Tier 1 coin validation: 10 requests/day = 300/month
- Emergency backup: 8 requests/day = 250/month
- **Total**: 33 requests/day = 1,000/month (at limit)

### Binance API (Free - High Limits)
**Price Data for 75 coins**:
- All 75 coins × 1 batch request every 30 minutes = 48 requests/day
- Monthly: 1,440 requests (well within free limits)

### Alpha Vantage (500 requests/day)
**Backup Price Data**:
- 75 coins × 1 request every 2 hours = 900 requests/day (exceeds limit)
- **Adjusted**: 75 coins × 1 request every 4 hours = 450 requests/day ✅

## Smart API Conservation Strategies

### 1. Intelligent Batching
```cpp
// CryptoNews: Request multiple time periods per call
struct SmartNewsRequest {
    string symbol = "BTC";
    string timeRange = "7d";  // Get week of data per request
    int articlesPerRequest = 50; // Maximize articles per call
};

// Binance: Batch multiple symbols per request
struct BatchPriceRequest {
    vector<string> symbols = {"BTCUSDT", "ETHUSDT", "ADAUSDT", /*...75 symbols*/};
    string interval = "1h"; // Hourly data
    int limit = 100; // Historical points per request
};
```

### 2. Tiered Update Frequencies
```cpp
struct TieredUpdateStrategy {
    // Tier 1: High frequency updates
    chrono::minutes tier1UpdateInterval = chrono::minutes(30);

    // Tier 2: Medium frequency updates
    chrono::hours tier2UpdateInterval = chrono::hours(2);

    // Tier 3: Low frequency updates
    chrono::hours tier3UpdateInterval = chrono::hours(8);
};
```

### 3. Smart Caching with TTL
```cpp
struct CacheStrategy {
    // Historical data: Never expire
    int historicalDataTTL = -1;

    // Tier 1 news: 1 hour cache
    int tier1NewsTTL = 3600;

    // Tier 2 news: 4 hour cache
    int tier2NewsTTL = 14400;

    // Tier 3 news: 12 hour cache
    int tier3NewsTTL = 43200;

    // Price data: 15 minute cache
    int priceDataTTL = 900;
};
```

## Monthly API Budget Analysis

### CryptoNews Usage
- **Historical Loading**: 1,430 requests/month (3-month period)
- **Ongoing Operations**: 1,290 requests/month
- **Total**: 2,720 requests/month (likely within Trial Basic limits)

### NewsAPI Usage
- **Backup/Validation**: 1,000 requests/month (at limit)
- **Cost**: $0/month (free tier)

### Binance API Usage
- **Price Data**: 1,440 requests/month
- **Cost**: $0/month (free tier, well within limits)

### Alpha Vantage Usage
- **Backup Price Data**: 13,500 requests/month
- **Cost**: $0/month (free tier, within daily limits)

## Risk Mitigation Strategies

### 1. Request Prioritization
```cpp
enum class RequestPriority {
    CRITICAL,    // Tier 1 coins, real-time price data
    HIGH,        // Tier 2 coins, daily news updates
    MEDIUM,      // Tier 3 coins, weekly news updates
    LOW,         // Historical backfill, non-essential data
    DEFERRED     // Can wait for next cycle
};
```

### 2. Graceful Degradation
- **API Limit Hit**: Switch to cached data with extended TTL
- **Primary Source Down**: Automatic failover to backup APIs
- **Quality Degradation**: Use older cached data with quality warnings

### 3. Emergency Mode
```cpp
struct EmergencyMode {
    bool enabled = false;
    int maxDailyRequests = 50;  // Severely limited requests
    vector<string> essentialCoins = {"BTC", "ETH"}; // Only most critical
    chrono::hours updateInterval = chrono::hours(24); // Daily updates only
};
```

## Expected Performance with 75-Coin Universe

### Data Coverage
- **Historical Depth**: 2 years for top 20, 1 year for next 30, 6 months for remaining 25
- **News Coverage**: Comprehensive for major coins, selective for smaller caps
- **Price Data**: Real-time for all 75 coins
- **Update Frequency**: Tiered based on coin importance

### Pair Formation Capacity
- **Total Possible Pairs**: 2,775 combinations (75 × 74 / 2)
- **Sector-Balanced Pairs**: ~200 high-quality pairs across sectors
- **Long-Short Opportunities**: 50+ viable market-neutral pairs

### ML Training Benefits
- **Feature Diversity**: 12 market sectors represented
- **Sample Size**: 75 × 365 days = 27,375 data points annually
- **Signal Quality**: Sector-specific patterns and cross-correlations
- **Robustness**: Reduced single-coin dependency risk

This expanded universe provides proper diversification while staying within free/trial API limits through intelligent batching, tiered priorities, and smart caching strategies.