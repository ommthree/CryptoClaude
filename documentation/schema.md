# CryptoClaude Database Schema Specification

## Overview
This document defines the comprehensive database schema for the CryptoClaude trading platform, designed for SQLite with future PostgreSQL migration capability. The schema supports complete historical data retention, performance tracking, and operational monitoring.

## Schema Design Principles

### Data Retention Strategy
**Comprehensive Retention:** Store all available data for model improvement and historical analysis
**Scalability Consideration:** Monitor database growth and implement archiving strategies if required
**Performance Optimization:** Appropriate indexing for real-time queries and historical analysis
**Migration Ready:** Schema designed for easy migration to PostgreSQL if scaling requirements change

### Database Technology
**Primary:** SQLite for single-user desktop application
**Architecture:** Repository pattern with abstracted database layer
**Migration Path:** Schema compatible with PostgreSQL migration
**Backup Strategy:** Automated backup with point-in-time recovery capability

## Core Data Tables

### Market Data Tables

#### market_data
**Purpose:** Primary market data storage from CryptoCompare API
```sql
CREATE TABLE market_data (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    symbol VARCHAR(10) NOT NULL,
    timestamp DATETIME NOT NULL,
    open_price DECIMAL(20,8) NOT NULL,
    high_price DECIMAL(20,8) NOT NULL,
    low_price DECIMAL(20,8) NOT NULL,
    close_price DECIMAL(20,8) NOT NULL,
    volume_from DECIMAL(20,8) NOT NULL,
    volume_to DECIMAL(20,8) NOT NULL,
    trades_count INTEGER,
    market_cap DECIMAL(20,2),
    data_source VARCHAR(50) DEFAULT 'CryptoCompare',
    data_quality_score DECIMAL(3,2) DEFAULT 1.0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(symbol, timestamp)
);

CREATE INDEX idx_market_data_symbol_timestamp ON market_data(symbol, timestamp);
CREATE INDEX idx_market_data_timestamp ON market_data(timestamp);
CREATE INDEX idx_market_data_created_at ON market_data(created_at);
```

#### hourly_returns
**Purpose:** Calculated hourly returns for model calibration and risk analysis
```sql
CREATE TABLE hourly_returns (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    symbol VARCHAR(10) NOT NULL,
    timestamp DATETIME NOT NULL,
    return_rate DECIMAL(12,8) NOT NULL,
    price_change DECIMAL(20,8) NOT NULL,
    volume_weighted_return DECIMAL(12,8),
    inflow_metric DECIMAL(20,8),
    volatility DECIMAL(10,6),
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(symbol, timestamp)
);

CREATE INDEX idx_hourly_returns_symbol_timestamp ON hourly_returns(symbol, timestamp);
CREATE INDEX idx_hourly_returns_timestamp ON hourly_returns(timestamp);
```

### News and Sentiment Tables

#### news_articles
**Purpose:** Raw news data from CryptoNews API
```sql
CREATE TABLE news_articles (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    article_id VARCHAR(100) UNIQUE NOT NULL,
    title TEXT NOT NULL,
    content TEXT,
    url TEXT,
    author VARCHAR(200),
    source_name VARCHAR(100) NOT NULL,
    source_quality_tier VARCHAR(20) DEFAULT 'general',
    published_at DATETIME NOT NULL,
    symbols TEXT, -- JSON array of related symbols
    sentiment_score DECIMAL(4,3),
    confidence_score DECIMAL(4,3),
    processed_at DATETIME,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_news_articles_published_at ON news_articles(published_at);
CREATE INDEX idx_news_articles_source_name ON news_articles(source_name);
CREATE INDEX idx_news_articles_sentiment_score ON news_articles(sentiment_score);
```

#### sentiment_analysis
**Purpose:** Processed sentiment data aggregated by symbol and time buckets
```sql
CREATE TABLE sentiment_analysis (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    symbol VARCHAR(10) NOT NULL,
    analysis_timestamp DATETIME NOT NULL,
    time_bucket VARCHAR(20) NOT NULL, -- '24h', '3d_ex_24h', '10d_ex_3d'
    positive_count INTEGER DEFAULT 0,
    negative_count INTEGER DEFAULT 0,
    neutral_count INTEGER DEFAULT 0,
    high_quality_positive INTEGER DEFAULT 0,
    high_quality_negative INTEGER DEFAULT 0,
    high_quality_neutral INTEGER DEFAULT 0,
    medium_quality_positive INTEGER DEFAULT 0,
    medium_quality_negative INTEGER DEFAULT 0,
    medium_quality_neutral INTEGER DEFAULT 0,
    general_quality_positive INTEGER DEFAULT 0,
    general_quality_negative INTEGER DEFAULT 0,
    general_quality_neutral INTEGER DEFAULT 0,
    weighted_sentiment_score DECIMAL(4,3),
    article_count INTEGER DEFAULT 0,
    data_quality_score DECIMAL(3,2) DEFAULT 1.0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(symbol, analysis_timestamp, time_bucket)
);

CREATE INDEX idx_sentiment_symbol_timestamp ON sentiment_analysis(symbol, analysis_timestamp);
CREATE INDEX idx_sentiment_timestamp ON sentiment_analysis(analysis_timestamp);
```

### Model and Prediction Tables

#### model_training_runs
**Purpose:** Track model training sessions and performance
```sql
CREATE TABLE model_training_runs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    run_timestamp DATETIME NOT NULL,
    model_type VARCHAR(50) NOT NULL DEFAULT 'RandomForest',
    training_start_date DATE NOT NULL,
    training_end_date DATE NOT NULL,
    symbols_count INTEGER NOT NULL,
    total_samples INTEGER NOT NULL,
    validation_accuracy DECIMAL(5,4),
    feature_importance TEXT, -- JSON object
    hyperparameters TEXT, -- JSON object
    model_file_path TEXT,
    training_duration_seconds INTEGER,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_model_training_timestamp ON model_training_runs(run_timestamp);
```

#### gamma_factors
**Purpose:** Per-coin gamma factors for liquidity adjustment
```sql
CREATE TABLE gamma_factors (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    symbol VARCHAR(10) NOT NULL,
    calculation_timestamp DATETIME NOT NULL,
    gamma_value DECIMAL(12,8) NOT NULL,
    confidence_interval_lower DECIMAL(12,8),
    confidence_interval_upper DECIMAL(12,8),
    coefficient_of_variation DECIMAL(6,4),
    estimation_method VARCHAR(50) NOT NULL,
    calibration_window_days INTEGER NOT NULL,
    data_points_used INTEGER NOT NULL,
    quality_score DECIMAL(3,2) DEFAULT 1.0,
    is_active BOOLEAN DEFAULT TRUE,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(symbol, calculation_timestamp)
);

CREATE INDEX idx_gamma_factors_symbol_timestamp ON gamma_factors(symbol, calculation_timestamp);
CREATE INDEX idx_gamma_factors_symbol_active ON gamma_factors(symbol, is_active);
```

#### predictions
**Purpose:** Model predictions for trading decisions
```sql
CREATE TABLE predictions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    symbol VARCHAR(10) NOT NULL,
    prediction_timestamp DATETIME NOT NULL,
    prediction_horizon_hours INTEGER NOT NULL DEFAULT 1,
    predicted_inflow DECIMAL(20,8) NOT NULL,
    expected_price_shift DECIMAL(12,8) NOT NULL,
    confidence_score DECIMAL(4,3) NOT NULL,
    model_run_id INTEGER,
    gamma_factor_used DECIMAL(12,8),
    feature_values TEXT, -- JSON object with input features
    actual_inflow DECIMAL(20,8), -- Populated after observation
    actual_price_shift DECIMAL(12,8), -- Populated after observation
    prediction_error DECIMAL(12,8), -- Calculated after observation
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (model_run_id) REFERENCES model_training_runs(id),
    UNIQUE(symbol, prediction_timestamp, prediction_horizon_hours)
);

CREATE INDEX idx_predictions_symbol_timestamp ON predictions(symbol, prediction_timestamp);
CREATE INDEX idx_predictions_timestamp ON predictions(prediction_timestamp);
CREATE INDEX idx_predictions_confidence ON predictions(confidence_score);
```

### Portfolio and Trading Tables

#### portfolios
**Purpose:** Portfolio configurations and metadata
```sql
CREATE TABLE portfolios (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name VARCHAR(100) NOT NULL UNIQUE,
    strategy_name VARCHAR(100) NOT NULL DEFAULT 'LongShortStrategy',
    initial_value DECIMAL(20,2) NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    is_active BOOLEAN DEFAULT TRUE,
    parameters TEXT -- JSON object with strategy parameters
);
```

#### positions
**Purpose:** Individual position tracking
```sql
CREATE TABLE positions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    portfolio_id INTEGER NOT NULL,
    symbol VARCHAR(10) NOT NULL,
    position_type VARCHAR(10) NOT NULL CHECK (position_type IN ('LONG', 'SHORT')),
    quantity DECIMAL(20,8) NOT NULL,
    entry_price DECIMAL(20,8) NOT NULL,
    entry_timestamp DATETIME NOT NULL,
    exit_price DECIMAL(20,8),
    exit_timestamp DATETIME,
    current_price DECIMAL(20,8) NOT NULL,
    unrealized_pnl DECIMAL(20,2),
    realized_pnl DECIMAL(20,2),
    fees_paid DECIMAL(20,2) DEFAULT 0,
    position_value DECIMAL(20,2) NOT NULL,
    risk_metrics TEXT, -- JSON object with position risk data
    is_active BOOLEAN DEFAULT TRUE,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (portfolio_id) REFERENCES portfolios(id)
);

CREATE INDEX idx_positions_portfolio_symbol ON positions(portfolio_id, symbol);
CREATE INDEX idx_positions_active ON positions(is_active);
CREATE INDEX idx_positions_entry_timestamp ON positions(entry_timestamp);
```

#### trading_pairs
**Purpose:** Long-short pair tracking and management
```sql
CREATE TABLE trading_pairs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    portfolio_id INTEGER NOT NULL,
    pair_name VARCHAR(50) NOT NULL,
    long_symbol VARCHAR(10) NOT NULL,
    short_symbol VARCHAR(10) NOT NULL,
    long_position_id INTEGER,
    short_position_id INTEGER,
    pair_allocation DECIMAL(5,4) NOT NULL,
    entry_timestamp DATETIME NOT NULL,
    exit_timestamp DATETIME,
    current_pair_value DECIMAL(20,2) NOT NULL,
    pair_pnl DECIMAL(20,2),
    correlation_coefficient DECIMAL(6,4),
    pair_volatility DECIMAL(10,6),
    confidence_score DECIMAL(4,3),
    is_active BOOLEAN DEFAULT TRUE,
    risk_metrics TEXT, -- JSON object with pair risk data
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (portfolio_id) REFERENCES portfolios(id),
    FOREIGN KEY (long_position_id) REFERENCES positions(id),
    FOREIGN KEY (short_position_id) REFERENCES positions(id)
);

CREATE INDEX idx_trading_pairs_portfolio ON trading_pairs(portfolio_id);
CREATE INDEX idx_trading_pairs_active ON trading_pairs(is_active);
CREATE INDEX idx_trading_pairs_entry_timestamp ON trading_pairs(entry_timestamp);
```

#### trades
**Purpose:** Individual trade execution tracking
```sql
CREATE TABLE trades (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    portfolio_id INTEGER NOT NULL,
    position_id INTEGER,
    trading_pair_id INTEGER,
    symbol VARCHAR(10) NOT NULL,
    trade_type VARCHAR(10) NOT NULL CHECK (trade_type IN ('BUY', 'SELL')),
    quantity DECIMAL(20,8) NOT NULL,
    price DECIMAL(20,8) NOT NULL,
    total_value DECIMAL(20,2) NOT NULL,
    fees DECIMAL(20,2) NOT NULL DEFAULT 0,
    exchange VARCHAR(50),
    order_id VARCHAR(100),
    execution_timestamp DATETIME NOT NULL,
    trade_rationale TEXT,
    slippage DECIMAL(8,4),
    execution_quality_score DECIMAL(3,2),
    api_costs DECIMAL(10,2) DEFAULT 0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (portfolio_id) REFERENCES portfolios(id),
    FOREIGN KEY (position_id) REFERENCES positions(id),
    FOREIGN KEY (trading_pair_id) REFERENCES trading_pairs(id)
);

CREATE INDEX idx_trades_portfolio_timestamp ON trades(portfolio_id, execution_timestamp);
CREATE INDEX idx_trades_symbol_timestamp ON trades(symbol, execution_timestamp);
CREATE INDEX idx_trades_timestamp ON trades(execution_timestamp);
```

### Performance and Risk Tables

#### portfolio_snapshots
**Purpose:** Regular portfolio state captures for performance analysis
```sql
CREATE TABLE portfolio_snapshots (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    portfolio_id INTEGER NOT NULL,
    snapshot_timestamp DATETIME NOT NULL,
    total_value DECIMAL(20,2) NOT NULL,
    cash_balance DECIMAL(20,2) NOT NULL,
    unrealized_pnl DECIMAL(20,2) NOT NULL,
    realized_pnl DECIMAL(20,2) NOT NULL,
    daily_return DECIMAL(8,6),
    cumulative_return DECIMAL(10,6),
    portfolio_volatility DECIMAL(8,6),
    sharpe_ratio DECIMAL(6,4),
    max_drawdown DECIMAL(8,6),
    active_pairs_count INTEGER DEFAULT 0,
    concentration_risk DECIMAL(6,4),
    var_95 DECIMAL(20,2),
    var_99 DECIMAL(20,2),
    expected_shortfall DECIMAL(20,2),
    risk_metrics TEXT, -- JSON object with detailed risk data
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (portfolio_id) REFERENCES portfolios(id),
    UNIQUE(portfolio_id, snapshot_timestamp)
);

CREATE INDEX idx_portfolio_snapshots_portfolio_timestamp ON portfolio_snapshots(portfolio_id, snapshot_timestamp);
CREATE INDEX idx_portfolio_snapshots_timestamp ON portfolio_snapshots(snapshot_timestamp);
```

#### risk_alerts
**Purpose:** Risk monitoring and alert tracking
```sql
CREATE TABLE risk_alerts (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    portfolio_id INTEGER,
    trading_pair_id INTEGER,
    position_id INTEGER,
    alert_type VARCHAR(50) NOT NULL,
    severity VARCHAR(20) NOT NULL CHECK (severity IN ('INFO', 'WARNING', 'ERROR', 'CRITICAL')),
    title VARCHAR(200) NOT NULL,
    description TEXT NOT NULL,
    alert_timestamp DATETIME NOT NULL,
    threshold_value DECIMAL(20,8),
    current_value DECIMAL(20,8),
    recommended_action TEXT,
    acknowledged BOOLEAN DEFAULT FALSE,
    acknowledged_by VARCHAR(100),
    acknowledged_at DATETIME,
    resolved BOOLEAN DEFAULT FALSE,
    resolved_at DATETIME,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (portfolio_id) REFERENCES portfolios(id),
    FOREIGN KEY (trading_pair_id) REFERENCES trading_pairs(id),
    FOREIGN KEY (position_id) REFERENCES positions(id)
);

CREATE INDEX idx_risk_alerts_portfolio_timestamp ON risk_alerts(portfolio_id, alert_timestamp);
CREATE INDEX idx_risk_alerts_severity ON risk_alerts(severity);
CREATE INDEX idx_risk_alerts_unresolved ON risk_alerts(resolved, acknowledged);
```

### System Configuration and Monitoring Tables

#### system_parameters
**Purpose:** Store all tunable algorithm parameters
```sql
CREATE TABLE system_parameters (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    parameter_name VARCHAR(100) NOT NULL UNIQUE,
    parameter_value TEXT NOT NULL,
    parameter_type VARCHAR(20) NOT NULL CHECK (parameter_type IN ('DECIMAL', 'INTEGER', 'BOOLEAN', 'STRING')),
    category VARCHAR(50) NOT NULL,
    description TEXT,
    min_value DECIMAL(20,8),
    max_value DECIMAL(20,8),
    default_value TEXT NOT NULL,
    last_modified_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    modified_by VARCHAR(100) DEFAULT 'system'
);

CREATE UNIQUE INDEX idx_system_parameters_name ON system_parameters(parameter_name);
CREATE INDEX idx_system_parameters_category ON system_parameters(category);
```

#### data_quality_metrics
**Purpose:** Monitor data provider quality and availability
```sql
CREATE TABLE data_quality_metrics (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    data_source VARCHAR(50) NOT NULL,
    symbol VARCHAR(10),
    metric_timestamp DATETIME NOT NULL,
    completeness_score DECIMAL(5,4) NOT NULL,
    accuracy_score DECIMAL(5,4) NOT NULL,
    timeliness_score DECIMAL(5,4) NOT NULL,
    consistency_score DECIMAL(5,4) NOT NULL,
    overall_quality_score DECIMAL(5,4) NOT NULL,
    missing_data_points INTEGER DEFAULT 0,
    anomalous_data_points INTEGER DEFAULT 0,
    total_data_points INTEGER NOT NULL,
    last_update_timestamp DATETIME,
    issues TEXT, -- JSON array of quality issues
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(data_source, symbol, metric_timestamp)
);

CREATE INDEX idx_data_quality_source_timestamp ON data_quality_metrics(data_source, metric_timestamp);
CREATE INDEX idx_data_quality_symbol_timestamp ON data_quality_metrics(symbol, metric_timestamp);
```

#### system_events
**Purpose:** Comprehensive system activity logging
```sql
CREATE TABLE system_events (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    event_timestamp DATETIME NOT NULL,
    event_type VARCHAR(50) NOT NULL,
    event_category VARCHAR(30) NOT NULL,
    severity VARCHAR(20) NOT NULL CHECK (severity IN ('INFO', 'WARNING', 'ERROR', 'CRITICAL')),
    title VARCHAR(200) NOT NULL,
    description TEXT,
    user_id VARCHAR(100),
    component VARCHAR(50),
    metadata TEXT, -- JSON object with additional event data
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_system_events_timestamp ON system_events(event_timestamp);
CREATE INDEX idx_system_events_type ON system_events(event_type);
CREATE INDEX idx_system_events_severity ON system_events(severity);
```

## Database Maintenance and Optimization

### Indexing Strategy
**Primary Indexes:** All tables have appropriate primary and foreign key indexes
**Performance Indexes:** Time-series queries optimized with timestamp indexes
**Composite Indexes:** Symbol-timestamp combinations for efficient data retrieval
**Unique Constraints:** Prevent duplicate data while allowing efficient updates

### Data Archiving Strategy
**Growth Monitoring:** Implement database size monitoring with configurable thresholds
**Archiving Triggers:** Automatic archiving when database exceeds size limits
**Archive Strategy:** Move older data to separate archive database while maintaining accessibility
**Performance Maintenance:** Regular VACUUM and ANALYZE operations for SQLite optimization

### Backup and Recovery
**Automated Backup:** Daily full database backup with point-in-time recovery capability
**Incremental Backup:** Hourly incremental backups during active trading periods
**Recovery Testing:** Monthly recovery testing to ensure backup reliability
**Cloud Backup:** Secure cloud backup storage with encryption for disaster recovery

### Migration Considerations
**PostgreSQL Compatibility:** Schema designed for easy migration to PostgreSQL
**Data Type Mapping:** Compatible data types chosen for seamless migration
**Performance Scaling:** Partitioning strategy planned for large-scale deployment
**Connection Pooling:** Repository pattern ready for connection pooling implementation

---

**Schema Status:** Complete specification - Ready for Project Owner Approval
**Implementation Notes:** Start with core tables, add advanced features incrementally
**Evolution Rights:** Developer may modify with SDM + Project Owner approval

*Document Owner: SDM*
*Approval Authority: Project Owner*
*Created: Setup Phase*
*Modification Rights: Developer (with approvals) + Project Owner*