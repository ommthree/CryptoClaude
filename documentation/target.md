# CryptoClaude Target State Specification

## Overview
This document defines the target functionality for the CryptoClaude trading platform, specifying user interface capabilities, system outputs, and operational requirements for the complete trading system.

## System Architecture Overview

### Deployment Environment
**Primary Platform:** AWS Lightsail instance for 24/7 operation
**Local Interface:** Desktop application interface for monitoring and control
**Data Storage:** Local SQLite database with comprehensive historical retention
**Connectivity:** Real-time API connections to CryptoCompare and CryptoNews API

### Core System Components
1. **Trading Engine:** Automated long-short pair trading execution
2. **Prediction Model:** Random Forest with sentiment analysis and technical factors
3. **Risk Management:** Real-time portfolio and pair-level risk monitoring with VaR and stress testing
4. **Data Pipeline:** Continuous market and news data ingestion with smart caching
5. **Reporting System:** Comprehensive performance and risk analytics
6. **Console Interface:** Command-line interface for system control and monitoring
7. **Personal API Integration:** Smart caching and API conservation for personal-use optimization ✅ **NEW**

## Console Interface Functionality

### System Control Commands

#### Model Calibration and Training
```
calibrate tree [--symbols SYMBOL_LIST] [--timeframe DAYS]
```
**Purpose:** Calibrate the Random Forest prediction model
**Parameters:**
- `--symbols`: Specific coin list for calibration (default: all eligible)
- `--timeframe`: Historical data window for training (default: 90 days)
**Output:** Model performance metrics, feature importance analysis, calibration status

```
calibrate gamma [--symbol SYMBOL] [--window DAYS]
```
**Purpose:** Calibrate per-coin gamma factors for liquidity adjustment
**Parameters:**
- `--symbol`: Specific coin for gamma calibration (default: all coins)
- `--window`: Calibration window in days (default: 30 days)
**Output:** Gamma factor values, stability metrics, confidence intervals

#### Parameter Management
```
set parameter PARAMETER_NAME VALUE
```
**Purpose:** Modify tunable algorithm parameters
**Examples:**
- `set parameter max_coin_concentration 7.5`
- `set parameter min_rebalance_interval 6.0`
**Output:** Parameter confirmation, validation status, impact assessment

```
get parameter [PARAMETER_NAME]
```
**Purpose:** Display current parameter values
**Output:** Current parameter values, ranges, and last modification timestamps

```
reset parameters [--category CATEGORY]
```
**Purpose:** Reset parameters to default values
**Parameters:**
- `--category`: Specific parameter category (risk, rebalancing, etc.)
**Output:** Reset confirmation and new parameter values

#### Trading Control
```
trading on [--mode personal|professional]
```
**Purpose:** Enable automated trading execution
**Parameters:**
- `--mode`: Trading mode (personal: conservative limits, professional: full features) ✅ **ENHANCED**
**Output:** Trading activation confirmation, current portfolio status, active risk limits

```
trading off
```
**Purpose:** Disable automated trading execution
**Output:** Trading suspension confirmation, pending orders status

```
liquidate [--symbol SYMBOL] [--pair PAIR_ID]
```
**Purpose:** Force liquidation of positions
**Parameters:**
- `--symbol`: Liquidate specific coin positions
- `--pair`: Liquidate specific trading pair
**Output:** Liquidation execution status, realized P&L impact

```
personal-limits [--show|--update]
```
**Purpose:** Manage personal trading limits and constraints ✅ **NEW**
**Parameters:**
- `--show`: Display current personal limits
- `--update`: Interactive update of personal risk parameters
**Output:**
- Current personal risk limits (2% per trade, 5% portfolio, 3 max positions)
- API usage constraints and optimization settings
- Personal ML pipeline configuration and performance thresholds

```
volatility-limits [--show|--update]
```
**Purpose:** Manage volatility-based position limits and controls ✅ **NEW**
**Parameters:**
- `--show`: Display current volatility limits and regime classifications
- `--update`: Interactive update of volatility sensitivity and downweighting parameters
**Output:**
- Current volatility regime classifications (LOW/NORMAL/HIGH/EXTREME)
- Downweighting factors and position size limits
- Volatility trend analysis and regime adaptation settings

```
claude-features [--show|--test|--configure]
```
**Purpose:** Monitor and configure Claude AI feature generation ✅ **NEW**
**Parameters:**
- `--show`: Display current Claude AI features and confidence scores
- `--test`: Test Claude API connectivity and feature generation
- `--configure`: Update Claude API settings and safety parameters
**Output:**
- 15 Claude AI features with confidence scores
- Feature validation status and bounds checking
- API usage statistics and rate limiting status

```
safety-limits [--show|--test|--emergency-override]
```
**Purpose:** Monitor and test multi-layer safety control systems ✅ **NEW**
**Parameters:**
- `--show`: Display all active safety limits and circuit breaker status
- `--test`: Execute safety limit testing with simulated extreme scenarios
- `--emergency-override`: Emergency override procedures (requires confirmation)
**Output:**
- Hard-coded safety limits status (35% max adjustment ceiling)
- Circuit breaker status and trigger history
- Sanity check alerts and audit trail

#### Historical Testing & Simulation

##### Full Backtesting Engine
```
backtest --mode full --start DATE --end DATE --initial-capital AMOUNT [--parameters PARAM_FILE] [--thresholds THRESHOLD_PROFILE]
```
**Purpose:** Comprehensive historical trading simulation with virtual portfolio management
**Parameters:**
- `--mode full`: Execute complete trading simulation (default: statistical only)
- `--start`: Backtest start date (YYYY-MM-DD format)
- `--end`: Backtest end date (YYYY-MM-DD format)
- `--initial-capital`: Starting virtual capital amount (required for full mode)
- `--parameters`: Custom algorithm parameter configuration file
- `--thresholds`: Testing threshold profile (conservative|moderate|aggressive|custom)
**Features:**
- Virtual portfolio with exact historical data replay
- Realistic order execution with slippage and fee simulation
- Position tracking identical to live trading engine
- Risk management validation under historical conditions
**Output:** Comprehensive performance report including:
- Sharpe ratio, maximum drawdown, volatility metrics
- Trade-by-trade execution log with P&L attribution
- Risk metrics evolution over testing period
- Portfolio allocation and rebalancing history

```
backtest --mode statistical --start DATE --end DATE [--parameters PARAM_FILE]
```
**Purpose:** Statistical performance analysis without full trading simulation
**Parameters:**
- `--mode statistical`: Run statistical analysis only (faster execution)
- `--start`: Backtest start date (YYYY-MM-DD format)
- `--end`: Backtest end date (YYYY-MM-DD format)
- `--parameters`: Custom parameter configuration file
**Output:** Statistical performance metrics, signal analysis, prediction accuracy

```
walk-forward --periods NUM --window DAYS [--initial-capital AMOUNT]
```
**Purpose:** Walk-forward analysis for robust model validation
**Parameters:**
- `--periods`: Number of forward periods to test
- `--window`: Rolling window size for each period (days)
- `--initial-capital`: Virtual capital for trading simulation (optional)
**Output:** Out-of-sample performance metrics, model stability analysis, parameter drift detection

##### Real-Time Test Mode
```
set-mode test
```
**Purpose:** Switch entire system to test/paper trading mode
**Features:**
- Live data feeds with virtual trading execution
- Identical monitoring, alerts, and diagnostics as live mode
- Virtual balance and position management
- Clear UI indicators showing "TEST MODE ACTIVE"
- All risk management systems active but non-binding
**Output:** Test mode activation confirmation, virtual portfolio initialization

```
set-mode live
```
**Purpose:** Switch to live trading mode (requires confirmation)
**Safety:** Requires explicit confirmation and production readiness validation
**Output:** Live mode activation confirmation, real portfolio status

```
get-mode
```
**Purpose:** Display current system mode (test/live) and virtual portfolio status
**Output:** Current mode, virtual/real balance, active positions, mode duration

##### Paper Trading Operations
```
paper-trade --duration DAYS [--parallel true]
```
**Purpose:** Execute paper trading for specified duration
**Parameters:**
- `--duration`: Paper trading duration in days
- `--parallel true`: Run paper trading alongside live trading (comparison mode)
**Features:**
- Real-time strategy execution with virtual capital
- Performance comparison with live trading (if parallel)
- Risk validation in safe environment
- Strategy parameter optimization
**Output:** Paper trading session results, performance vs live comparison

##### Testing Threshold Management
```
show-thresholds [--category CATEGORY] [--profile PROFILE]
```
**Purpose:** Display current testing threshold configuration
**Parameters:**
- `--category`: Filter by category (all|performance|reliability|execution|crypto)
- `--profile`: Show thresholds for specific profile (conservative|moderate|aggressive|custom)
**Output:** Current threshold values with configurable ranges and active profile

```
set-threshold --metric METRIC_NAME --value VALUE [--profile PROFILE_NAME]
```
**Purpose:** Configure individual testing threshold values
**Parameters:**
- `--metric`: Threshold metric name (e.g., sharpe_ratio, max_drawdown, uptime_requirement)
- `--value`: New threshold value (within configurable range)
- `--profile`: Target profile to modify (default: active profile)
**Output:** Threshold update confirmation, validation status

```
load-profile --name PROFILE_NAME [--environment ENVIRONMENT]
```
**Purpose:** Load pre-defined or custom threshold profile
**Parameters:**
- `--name`: Profile name (conservative|moderate|aggressive|custom_name)
- `--environment`: Target environment (staging|production|testing)
**Output:** Profile activation confirmation, threshold summary

```
validate-thresholds [--profile PROFILE_NAME]
```
**Purpose:** Validate threshold configuration consistency and feasibility
**Parameters:**
- `--profile`: Profile to validate (default: active profile)
**Output:** Validation results, inconsistency warnings, feasibility assessment

### System Monitoring Commands

#### Real-Time Status
```
status
```
**Purpose:** Display comprehensive system status
**Output:**
- Trading engine status (active/inactive)
- Current portfolio composition and allocation
- Active trading pairs and performance
- Data feed connectivity status
- Risk metrics and alerts
- System health indicators

```
positions
```
**Purpose:** Display detailed position information
**Output:**
- Current positions by coin and pair
- Position sizes, entry prices, current P&L
- Risk metrics per position and pair
- Time since position inception

```
portfolio
```
**Purpose:** Display portfolio-level information
**Output:**
- Total portfolio value and cash allocation
- Overall P&L (daily, weekly, monthly)
- Portfolio risk metrics (VaR, drawdown, volatility)
- Concentration metrics and diversification status

#### Performance Reporting
```
performance [--period PERIOD] [--format FORMAT]
```
**Purpose:** Generate performance reports
**Parameters:**
- `--period`: daily/weekly/monthly/yearly/all
- `--format`: console/csv/json
**Output:**
- Return metrics by specified period
- Risk-adjusted performance (Sharpe ratio, Sortino ratio)
- Maximum drawdown analysis
- Trade statistics and win rate

```
attribution [--timeframe DAYS]
```
**Purpose:** Performance attribution analysis
**Parameters:**
- `--timeframe`: Analysis window in days (default: 30)
**Output:**
- Return decomposition by pair
- Alpha vs beta attribution
- Transaction cost impact
- Model prediction accuracy assessment

#### Risk Monitoring
```
risk
```
**Purpose:** Current risk assessment
**Output:**
- Portfolio VaR at multiple confidence levels
- Individual pair risk contributions
- Correlation matrix for active pairs
- Risk budget utilization
- Current vs target risk allocation

```
alerts [--severity LEVEL]
```
**Purpose:** Display active risk alerts
**Parameters:**
- `--severity`: info/warning/error/critical
**Output:**
- Active risk alerts by severity
- Alert timestamps and descriptions
- Recommended actions for each alert

#### Data Quality Monitoring
```
data status
```
**Purpose:** Data feed and quality status
**Output:**
- CryptoCompare API connectivity and latency
- CryptoNews API connectivity and article counts
- Data completeness metrics by symbol
- Recent data quality alerts
- Data provider backup status
- Smart cache performance metrics (hit rate, storage usage) ✅ **ENHANCED**

```
data gaps [--days DAYS]
```
**Purpose:** Identify data gaps and quality issues
**Parameters:**
- `--days`: Lookback period for gap analysis (default: 7)
**Output:**
- Missing data periods by symbol and data type
- Data quality score by symbol
- Recommended actions for data issues
- Cache efficiency analysis and optimization recommendations ✅ **ENHANCED**

```
cache-stats [--detailed] [--breakdown]
```
**Purpose:** Smart cache performance monitoring ✅ **NEW**
**Parameters:**
- `--detailed`: Show comprehensive cache metrics
- `--breakdown`: Break down stats by cache type (historical, realtime, news)
**Output:**
- Cache hit rate by data type
- Storage usage and compression efficiency
- API calls saved and cost reduction metrics
- Performance optimization recommendations

### Advanced Analytics Commands

#### Model Diagnostics
```
model performance
```
**Purpose:** Model prediction accuracy assessment
**Output:**
- Prediction vs actual performance correlation
- Model feature importance rankings
- Recent model performance trends
- Recommended model retraining schedule

```
model features [--symbol SYMBOL]
```
**Purpose:** Feature analysis and importance
**Parameters:**
- `--symbol`: Symbol-specific feature analysis
**Output:**
- Current feature values by symbol
- Feature importance weights
- Feature stability over time
- News source quality scores

#### Market Analysis
```
market sentiment [--timeframe HOURS]
```
**Purpose:** Current market sentiment analysis
**Parameters:**
- `--timeframe`: Sentiment analysis window (default: 24 hours)
**Output:**
- Sentiment scores by symbol and news source quality
- Sentiment trend analysis
- News volume and coverage metrics
- Sentiment-based trading signals

```
pairs analysis
```
**Purpose:** Trading pair analysis and optimization
**Output:**
- Current pair performance and correlation
- Pair selection rationale and confidence scores
- Alternative pair recommendations
- Pair rebalancing recommendations

## System Outputs and Reporting

### Automated Reports

#### Daily Performance Summary (End of Day)
**Content:**
- Daily portfolio return and attribution
- New positions opened and closed
- Risk metrics update
- Data quality assessment
- Trading cost summary including API licensing costs

#### Weekly Comprehensive Report
**Content:**
- Weekly performance vs benchmark and targets
- Risk analysis with VaR and drawdown metrics
- Model performance and prediction accuracy
- Data quality metrics and provider statistics
- Parameter optimization recommendations

#### Monthly Strategic Review
**Content:**
- Monthly performance attribution analysis
- Model effectiveness and feature importance evolution
- Risk management effectiveness assessment
- Trading cost analysis and optimization opportunities
- Strategic parameter adjustment recommendations

### Real-Time Monitoring Outputs

#### Hourly Risk Reports (if enabled)
**Content:**
- Current portfolio risk metrics
- Position-level risk attribution
- Real-time P&L and drawdown monitoring
- Active risk alerts and recommended actions
- Data feed health status

#### Trade Execution Reports (Real-Time)
**Content:**
- Trade execution details with timestamps
- Trade rationale and model confidence scores
- Execution quality metrics (slippage, timing)
- Post-trade risk impact assessment
- Running trading cost accumulation

### Alert and Notification System

#### Risk Alerts
- Portfolio stop-loss threshold breaches
- Individual pair risk limit violations
- Unusual correlation or volatility spikes
- Model prediction confidence degradation

#### Operational Alerts
- Data feed interruptions or quality degradation
- API rate limit approaches or violations
- System performance or connectivity issues
- Model retraining requirements

#### Performance Alerts
- Significant deviation from expected returns
- Unusual trading cost accumulation
- Benchmark underperformance beyond thresholds
- Cash buffer depletion or excess accumulation

## System Operational Requirements

### Data Management
**Historical Data Retention:** Complete retention of all market data, news data, predictions, and trading history
**Data Quality Monitoring:** Continuous monitoring for missing data, implausible values, stale data, and provider issues
**Backup and Recovery:** Automated database backup with point-in-time recovery capability

### Performance Requirements
**Response Time:** Console commands respond within 2 seconds for status queries, 30 seconds for complex analytics
**System Availability:** 99.5% uptime target with graceful degradation during data provider outages
**Data Latency:** Market data processing within 5 minutes of availability, news sentiment within 15 minutes

### Integration Requirements
**API Management:** Robust handling of rate limits, authentication, and failover for data providers
**Database Performance:** Efficient query processing for large historical datasets
**Monitoring Integration:** Health monitoring compatible with AWS CloudWatch and standard monitoring tools

### Security and Compliance
**API Key Management:** Secure storage and rotation of third-party API credentials
**Data Protection:** Encrypted storage of sensitive configuration and trading data
**Access Control:** Role-based access control for console commands and system modification
**Audit Trail:** Complete logging of all system actions, parameter changes, and trade decisions

## User Experience Requirements

### Console Interface Standards
**Command Syntax:** Consistent, intuitive command structure with comprehensive help system
**Output Formatting:** Clear, readable output with appropriate precision for financial data
**Error Handling:** Informative error messages with suggested corrective actions
**Command History:** Complete command history with search and recall capabilities

### Remote Access Capability
**Local Interface:** Desktop application for secure access to Lightsail instance
**Session Management:** Secure authentication and session management for remote access
**Data Synchronization:** Real-time synchronization of system status and reports
**Network Resilience:** Graceful handling of network interruptions and reconnection

### Documentation and Help
**Inline Help:** Comprehensive help system accessible via console commands
**Parameter Documentation:** Dynamic help showing current parameter values and acceptable ranges
**Example Library:** Built-in examples for complex commands and analysis scenarios
**Error Resolution:** Context-sensitive help for error conditions and system issues

### Future GUI Integration (Planned Enhancement) ✅ **STRATEGIC ROADMAP**

#### Web-Based Dashboard Interface
**Primary Purpose:** Provide intuitive visual interface for monitoring and parameter management
**Implementation Timeline:** Post-console interface completion (Week 5+)

**Core Dashboard Components:**
1. **Real-Time Portfolio Monitoring**
   - Live position tracking with P&L visualization
   - Risk metrics dashboard with VaR and correlation heatmaps
   - Trading activity feed with execution quality metrics

2. **Volatility Management Interface** ✅ **NEW FEATURE**
   - **Volatility Regime Visualization:** Real-time regime classification with historical transitions
   - **Downweighting Control Panel:** Tunable sensitivity parameters with live impact preview
   - **Position Size Optimizer:** Interactive volatility-based position sizing with scenario analysis
   - **Regime Transition Alerts:** Visual alerts for volatility regime changes

3. **Claude AI Feature Monitoring** ✅ **NEW FEATURE**
   - **AI Feature Dashboard:** Real-time Claude AI feature values with confidence indicators
   - **Feature Performance Analytics:** Historical accuracy and feature importance trends
   - **API Usage Monitoring:** Claude API call statistics and cost tracking
   - **Feature Validation Status:** Bounds checking and extreme value detection alerts

4. **Multi-Layer Safety Control Interface** ✅ **NEW FEATURE**
   - **Safety Limits Dashboard:** Hard-coded limit status with proximity alerts
   - **Circuit Breaker Control Panel:** Emergency shutdown controls with manual override
   - **Sanity Check Log:** Historical adjustment review with audit trail
   - **Emergency Procedures:** One-click emergency liquidation and system shutdown

5. **Parameter Management Interface**
   - **Interactive Parameter Tuning:** Real-time parameter adjustment with backtesting preview
   - **Scenario Testing:** Monte Carlo simulation with parameter sensitivity analysis
   - **Configuration Profiles:** Save/load parameter sets for different market regimes

6. **Advanced Analytics Visualization**
   - **Performance Attribution:** Interactive charts showing volatility impact and Claude AI contribution
   - **Risk Decomposition:** Multi-dimensional risk analysis with volatility correlation
   - **Market Regime Analysis:** Historical regime performance with strategy adaptation

**Technical Implementation:**
- **Frontend Framework:** React.js with real-time WebSocket connections
- **Backend Integration:** REST API with existing console interface
- **Data Visualization:** D3.js and Chart.js for financial charting
- **Security:** OAuth authentication with role-based access control

**GUI Benefits:**
- **Enhanced Monitoring:** Visual pattern recognition for volatility regimes and Claude AI feature trends
- **Faster Response:** Immediate visual alerts for safety limit approaches and regime transitions
- **Better Decision Making:** Interactive scenario analysis for parameter optimization
- **Improved Accessibility:** Non-technical stakeholder access to system monitoring

**Implementation Priority:** Medium - Provides significant operational value after core console functionality is stable

---

**Target State Status:** Complete specification with enhanced safety and volatility features - Ready for Project Owner Approval
**Dependencies:** Algorithm specification (approved), database schema (completed), Claude AI integration (implemented), volatility management (implemented)
**Implementation Priority:** Core console interface and trading control functionality first, followed by GUI dashboard

*Document Owner: SDM*
*Approval Authority: Project Owner*
*Created: Setup Phase*
*Last Updated: September 27, 2025 - Enhanced with volatility controls, Claude AI features, and multi-layer safety systems*
*Modification Rights: Project Owner Only*