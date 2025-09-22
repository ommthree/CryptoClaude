# CryptoClaude SQL Schema Documentation

**Version**: 1.0
**Last Updated**: 2025-01-22
**Database Engine**: SQLite 3

## Overview

The CryptoClaude platform uses SQLite as its primary database for storing market data, sentiment analysis, portfolio information, and trading positions. The schema is designed to support high-frequency trading operations with leverage, comprehensive risk management, and advanced analytics.

## Database Structure

The database consists of four main table groups:

1. **Market Data Tables** - Price, volume, and technical indicators
2. **Sentiment Tables** - News sentiment analysis and aggregation
3. **Portfolio Tables** - Portfolio management and position tracking
4. **System Tables** - Configuration and utility tables

---

## Market Data Tables

### 1. market_data

**Purpose**: Stores primary market data with technical indicators and sentiment integration.

```sql
CREATE TABLE market_data (
    symbol TEXT,                    -- Cryptocurrency symbol (e.g., 'BTC', 'ETH')
    timestamp INTEGER,              -- Unix timestamp of data point
    date TEXT,                      -- Human-readable date (YYYY-MM-DD)
    close_price REAL,               -- Closing price in USD
    volume_from REAL,               -- Volume in base currency
    volume_to REAL,                 -- Volume in quote currency (USD)
    net_inflow REAL,                -- Net capital inflow calculation
    excess_inflow REAL,             -- Excess inflow indicator
    hourly_inflow REAL,             -- Hourly inflow measurement
    day_of_week INTEGER,            -- Day of week (1-7)
    month_of_year INTEGER,          -- Month of year (1-12)
    article_count INTEGER DEFAULT 0, -- Number of news articles
    average_sentiment REAL DEFAULT 0.0, -- Average sentiment score
    rsi REAL DEFAULT 0.0,           -- Relative Strength Index (0-100)
    macd REAL DEFAULT 0.0,          -- MACD indicator value
    bollinger_position REAL DEFAULT 0.0, -- Bollinger Band position (0-1)
    PRIMARY KEY (timestamp, symbol)
);
```

**Key Features**:
- Primary key ensures unique data points per symbol per timestamp
- Technical indicators (RSI, MACD, Bollinger) integrated for strategy development
- Sentiment integration with article count and average sentiment
- Inflow calculations for institutional money tracking

### 2. hourly_data

**Purpose**: High-frequency hourly market data for detailed analysis.

```sql
CREATE TABLE hourly_data (
    time INTEGER,                   -- Unix timestamp (hourly)
    date TEXT,                      -- Human-readable date
    symbol TEXT,                    -- Cryptocurrency symbol
    close REAL,                     -- Hourly close price
    volumefrom REAL,                -- Hourly volume from
    volumeto REAL,                  -- Hourly volume to
    PRIMARY KEY (time, symbol)
);
```

### 3. market_cap

**Purpose**: Market capitalization data for ranking and analysis.

```sql
CREATE TABLE market_cap (
    symbol TEXT PRIMARY KEY,        -- Cryptocurrency symbol
    market_cap REAL,                -- Market cap in USD
    rank INTEGER,                   -- Market cap ranking
    last_updated INTEGER            -- Last update timestamp
);
```

### 4. liquidity_lambda

**Purpose**: Liquidity metrics for market depth analysis.

```sql
CREATE TABLE liquidity_lambda (
    symbol TEXT PRIMARY KEY,        -- Cryptocurrency symbol
    lambda_value REAL,              -- Liquidity lambda calculation
    bid_ask_spread REAL,            -- Current bid-ask spread
    market_depth REAL,              -- Market depth indicator
    last_calculated INTEGER         -- Calculation timestamp
);
```

---

## Sentiment Analysis Tables

### 1. sentiment_data

**Purpose**: Core sentiment analysis data from news sources.

```sql
CREATE TABLE sentiment_data (
    ticker TEXT,                    -- Cryptocurrency ticker
    source_name TEXT,               -- News source name
    date TEXT,                      -- Date of sentiment analysis
    article_count INTEGER DEFAULT 1, -- Number of articles analyzed
    avg_sentiment REAL DEFAULT 0.0, -- Average sentiment (-1 to +1)
    timestamp INTEGER,              -- Unix timestamp
    sentiment_1d REAL DEFAULT 0.0,  -- 1-day sentiment trend
    sentiment_7d_avg REAL DEFAULT 0.0, -- 7-day rolling average
    sentiment_trend REAL DEFAULT 0.0, -- Sentiment trend direction
    confidence REAL DEFAULT 0.0,   -- Analysis confidence level
    PRIMARY KEY (ticker, source_name, date)
);
```

**Key Features**:
- Composite primary key prevents duplicate analyses
- Multi-timeframe sentiment tracking (1d, 7d)
- Confidence scoring for analysis reliability
- Source attribution for quality assessment

### 2. news_articles

**Purpose**: Individual news articles with sentiment analysis.

```sql
CREATE TABLE news_articles (
    article_id INTEGER PRIMARY KEY AUTOINCREMENT,
    source_id INTEGER,              -- Foreign key to news_sources
    ticker TEXT,                    -- Related cryptocurrency
    title TEXT,                     -- Article headline
    content TEXT,                   -- Full article content
    url TEXT UNIQUE,                -- Article URL (unique constraint)
    published_at INTEGER,           -- Publication timestamp
    sentiment_score REAL DEFAULT 0.0, -- Individual article sentiment
    confidence REAL DEFAULT 0.0,   -- Sentiment confidence
    processed INTEGER DEFAULT 0,   -- Processing status flag
    FOREIGN KEY (source_id) REFERENCES news_sources(source_id)
);
```

### 3. aggregated_sentiment

**Purpose**: Daily aggregated sentiment metrics for analysis.

```sql
CREATE TABLE aggregated_sentiment (
    ticker TEXT,                    -- Cryptocurrency ticker
    date TEXT,                      -- Aggregation date
    avg_sentiment REAL,             -- Daily average sentiment
    sentiment_volatility REAL DEFAULT 0.0, -- Sentiment volatility
    total_articles INTEGER DEFAULT 0, -- Total articles analyzed
    positive_count INTEGER DEFAULT 0, -- Count of positive articles
    negative_count INTEGER DEFAULT 0, -- Count of negative articles
    neutral_count INTEGER DEFAULT 0, -- Count of neutral articles
    weighted_sentiment REAL DEFAULT 0.0, -- Weighted by source reliability
    PRIMARY KEY (ticker, date)
);
```

### 4. news_sources

**Purpose**: Configuration and tracking of news sources.

```sql
CREATE TABLE news_sources (
    source_id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,             -- Source name
    urlMap TEXT,                    -- URL mapping configuration
    priority INTEGER DEFAULT 1     -- Source priority (1-10)
);
```

### 5. temp_news_sentiment

**Purpose**: Temporary storage for news sentiment processing.

```sql
CREATE TABLE temp_news_sentiment (
    source_name TEXT,               -- News source
    article_count INTEGER,          -- Article count
    avg_sentiment REAL,             -- Average sentiment
    date TEXT,                      -- Processing date
    ticker TEXT                     -- Cryptocurrency ticker
);
```

### 6. news_date_mapping

**Purpose**: Date format mapping for API integration.

```sql
CREATE TABLE news_date_mapping (
    api_date TEXT PRIMARY KEY,      -- API date format
    sql_date TEXT UNIQUE            -- SQL date format
);
```

---

## Portfolio Management Tables

### 1. portfolios

**Purpose**: Core portfolio information with comprehensive leverage support.

```sql
CREATE TABLE portfolios (
    portfolio_id INTEGER PRIMARY KEY AUTOINCREMENT,
    strategy_name TEXT NOT NULL,    -- Trading strategy identifier
    timestamp INTEGER,              -- Portfolio snapshot timestamp
    total_value REAL,               -- Current total portfolio value
    total_pnl REAL,                 -- Unrealized P&L
    cash_balance REAL,              -- Available cash

    -- Leverage Management
    current_leverage REAL DEFAULT 1.0, -- Current leverage ratio
    max_allowed_leverage REAL DEFAULT 3.0, -- Maximum leverage limit
    margin_used REAL DEFAULT 0.0,  -- Currently used margin
    available_margin REAL DEFAULT 0.0, -- Available margin
    margin_utilization REAL DEFAULT 0.0, -- Margin utilization %

    -- Risk Management
    portfolio_stop_level REAL DEFAULT -0.15, -- Portfolio stop-loss (-15%)
    stop_loss_triggered INTEGER DEFAULT 0, -- Stop-loss status
    liquidation_threshold REAL DEFAULT -0.20, -- Liquidation threshold
    risk_level TEXT DEFAULT 'MODERATE', -- Risk classification

    -- Performance Tracking
    daily_pnl REAL DEFAULT 0.0,     -- Daily P&L
    max_drawdown REAL DEFAULT 0.0,  -- Maximum drawdown experienced
    win_rate REAL DEFAULT 0.0,      -- Win rate percentage
    sharpe_ratio REAL DEFAULT 0.0,  -- Risk-adjusted returns

    -- Metadata
    created_at INTEGER DEFAULT (strftime('%s', 'now')),
    updated_at INTEGER DEFAULT (strftime('%s', 'now'))
);
```

**Key Features**:
- Comprehensive leverage tracking with utilization metrics
- Multi-level risk management (stop-loss, liquidation)
- Performance analytics with drawdown and Sharpe ratio
- Timestamp tracking for portfolio evolution

### 2. positions

**Purpose**: Individual trading positions with detailed leverage metrics.

```sql
CREATE TABLE positions (
    position_id INTEGER PRIMARY KEY AUTOINCREMENT,
    portfolio_id INTEGER NOT NULL,  -- Foreign key to portfolios
    symbol TEXT NOT NULL,           -- Asset symbol
    side TEXT NOT NULL,             -- 'LONG' or 'SHORT'
    quantity REAL NOT NULL,         -- Position size
    entry_price REAL NOT NULL,      -- Entry price
    current_price REAL,             -- Current market price

    -- Leverage Details
    leverage REAL DEFAULT 1.0,      -- Position leverage
    margin_required REAL,           -- Margin requirement
    maintenance_margin REAL,        -- Maintenance margin

    -- P&L Tracking
    unrealized_pnl REAL DEFAULT 0.0, -- Current unrealized P&L
    realized_pnl REAL DEFAULT 0.0,  -- Realized P&L on close
    pnl_percentage REAL DEFAULT 0.0, -- P&L as percentage

    -- Risk Management
    stop_loss_price REAL,           -- Stop-loss price
    take_profit_price REAL,         -- Take-profit price
    stop_loss_triggered INTEGER DEFAULT 0, -- Stop-loss status
    liquidation_price REAL,         -- Liquidation price

    -- Position Lifecycle
    status TEXT DEFAULT 'OPEN',     -- 'OPEN', 'CLOSED', 'LIQUIDATED'
    opened_at INTEGER DEFAULT (strftime('%s', 'now')),
    closed_at INTEGER,              -- Close timestamp
    close_reason TEXT,              -- Reason for closing

    FOREIGN KEY (portfolio_id) REFERENCES portfolios(portfolio_id)
);
```

**Key Features**:
- Comprehensive leverage position tracking
- Automatic P&L calculations with percentage tracking
- Multi-level risk management per position
- Full position lifecycle management
- Integration with portfolio-level metrics

### 3. backtest_results

**Purpose**: Storage of backtesting results for strategy evaluation.

```sql
CREATE TABLE backtest_results (
    result_id INTEGER PRIMARY KEY AUTOINCREMENT,
    strategy_name TEXT NOT NULL,    -- Strategy identifier
    start_date TEXT,                -- Backtest period start
    end_date TEXT,                  -- Backtest period end

    -- Performance Metrics
    total_return REAL,              -- Total return percentage
    annualized_return REAL,         -- Annualized return
    max_drawdown REAL,              -- Maximum drawdown
    sharpe_ratio REAL,              -- Risk-adjusted returns
    sortino_ratio REAL,             -- Downside-adjusted returns
    win_rate REAL,                  -- Percentage of winning trades

    -- Trading Statistics
    total_trades INTEGER,           -- Total number of trades
    winning_trades INTEGER,         -- Number of winning trades
    losing_trades INTEGER,          -- Number of losing trades
    avg_trade_return REAL,          -- Average trade return

    -- Leverage Metrics
    avg_leverage REAL,              -- Average leverage used
    max_leverage REAL,              -- Maximum leverage used
    margin_calls INTEGER DEFAULT 0, -- Number of margin calls

    -- Risk Metrics
    var_95 REAL,                    -- Value at Risk (95%)
    expected_shortfall REAL,        -- Expected shortfall

    created_at INTEGER DEFAULT (strftime('%s', 'now'))
);
```

---

## System Tables

### 1. model_metadata

**Purpose**: Tracks AI/ML model versions and performance.

```sql
CREATE TABLE model_metadata (
    model_id INTEGER PRIMARY KEY AUTOINCREMENT,
    model_name TEXT NOT NULL,       -- Model identifier
    version TEXT,                   -- Model version
    training_date INTEGER,          -- Training timestamp
    accuracy_score REAL,            -- Model accuracy
    features_used TEXT,             -- JSON array of features
    hyperparameters TEXT,           -- JSON object of parameters
    performance_metrics TEXT,       -- JSON object of metrics
    is_active INTEGER DEFAULT 1,   -- Active status
    created_at INTEGER DEFAULT (strftime('%s', 'now'))
);
```

---

## Indexes and Performance

### Key Indexes

```sql
-- Market data performance
CREATE INDEX idx_market_data_symbol_timestamp ON market_data(symbol, timestamp);
CREATE INDEX idx_market_data_date ON market_data(date);

-- Sentiment analysis performance
CREATE INDEX idx_sentiment_ticker_date ON sentiment_data(ticker, date);
CREATE INDEX idx_news_articles_ticker ON news_articles(ticker);
CREATE INDEX idx_news_articles_published ON news_articles(published_at);

-- Portfolio performance
CREATE INDEX idx_positions_portfolio ON positions(portfolio_id);
CREATE INDEX idx_positions_symbol ON positions(symbol);
CREATE INDEX idx_positions_status ON positions(status);

-- Backtest performance
CREATE INDEX idx_backtest_strategy ON backtest_results(strategy_name);
```

---

## Data Relationships

### Primary Relationships

1. **portfolios** ← **positions** (1:N)
   - One portfolio contains multiple positions
   - Cascade delete protection implemented

2. **news_sources** ← **news_articles** (1:N)
   - One source produces multiple articles
   - Foreign key constraint maintained

3. **market_data** ↔ **sentiment_data** (logical)
   - Linked by symbol/ticker and date
   - Cross-repository integration for trading signals

### Integration Points

1. **Trading Signal Generation**:
   - market_data.rsi + sentiment_data.avg_sentiment → trading signals
   - Volume analysis + sentiment trend → signal strength

2. **Risk Management**:
   - portfolios.margin_utilization + positions.liquidation_price → margin calls
   - Portfolio-level stop-loss + position-level stops → risk cascade

3. **Performance Analytics**:
   - backtest_results ↔ portfolios → strategy validation
   - positions.pnl_percentage → portfolios.total_pnl aggregation

---

## Schema Validation Rules

### Data Integrity Constraints

1. **Market Data**:
   - close_price > 0
   - rsi BETWEEN 0 AND 100
   - sentiment BETWEEN -1 AND 1

2. **Portfolio Management**:
   - current_leverage >= 1.0
   - margin_utilization BETWEEN 0 AND 1
   - total_value >= 0

3. **Sentiment Analysis**:
   - avg_sentiment BETWEEN -1 AND 1
   - confidence BETWEEN 0 AND 1
   - article_count >= 0

### Business Logic Constraints

1. **Leverage Limits**:
   - current_leverage <= max_allowed_leverage
   - margin_used <= available_margin

2. **Risk Management**:
   - stop_loss_price validation based on position side
   - liquidation_threshold < portfolio_stop_level

---

## Migration and Versioning

### Version History

- **v1.0** (2025-01-22): Initial schema with full leverage support
  - Complete market data integration
  - Advanced sentiment analysis
  - Comprehensive portfolio management
  - Backtesting framework

### Future Enhancements

1. **Planned Tables**:
   - `order_history` - Trade execution tracking
   - `strategy_parameters` - Dynamic strategy configuration
   - `risk_events` - Risk event logging

2. **Schema Extensions**:
   - Options/derivatives support
   - Multi-asset portfolio allocation
   - Advanced risk metrics (VaR, CVaR)

---

## Performance Characteristics

### Query Performance

- **Market Data Queries**: Optimized for time-series analysis
- **Sentiment Aggregation**: Efficient daily/weekly rollups
- **Portfolio Analytics**: Real-time P&L calculations
- **Risk Monitoring**: Sub-second margin call detection

### Storage Efficiency

- **Estimated Size**: ~100MB per million market data points
- **Compression**: SQLite built-in compression active
- **Archival**: Automated cleanup of stale data >1 year

---

## Usage Examples

### Trading Signal Generation

```sql
-- Generate trading signals combining technical and sentiment analysis
SELECT
    m.symbol,
    m.close_price,
    m.rsi,
    s.avg_sentiment,
    CASE
        WHEN m.rsi > 70 AND s.avg_sentiment < -0.2 THEN 'STRONG_SELL'
        WHEN m.rsi < 30 AND s.avg_sentiment > 0.2 THEN 'STRONG_BUY'
        WHEN m.rsi > 50 AND s.avg_sentiment > 0.1 THEN 'BUY'
        WHEN m.rsi < 50 AND s.avg_sentiment < -0.1 THEN 'SELL'
        ELSE 'HOLD'
    END as signal
FROM market_data m
JOIN sentiment_data s ON m.symbol = s.ticker AND m.date = s.date
WHERE m.date = '2025-01-22';
```

### Risk Monitoring

```sql
-- Monitor portfolios requiring margin calls
SELECT
    p.strategy_name,
    p.margin_utilization,
    p.current_leverage,
    COUNT(pos.position_id) as open_positions,
    SUM(pos.unrealized_pnl) as total_unrealized_pnl
FROM portfolios p
JOIN positions pos ON p.portfolio_id = pos.portfolio_id
WHERE p.margin_utilization > 0.80  -- Above 80% margin utilization
  AND pos.status = 'OPEN'
GROUP BY p.portfolio_id;
```

---

**Note**: This schema documentation is automatically updated with each significant database change. For implementation details, see the repository source code in `src/Core/Database/DatabaseManager.cpp`.