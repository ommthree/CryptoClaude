# CryptoClaude Trading Algorithm Specification

## Overview
This document defines the comprehensive trading algorithm for the CryptoClaude platform, implementing a market-neutral long-short strategy based on inflow predictions derived from sentiment analysis.

## Algorithm Framework

### Core Strategy: Long-Short Pairing with Inflow Prediction
**Objective:** Generate market-neutral returns through symmetrical pairs trading based on predicted price momentum

**Key Principles:**
1. Universe ranking by expected return (inflow prediction)
2. Risk-based coin exclusion before pairing
3. Symmetrical pair formation (top performer paired with bottom performer)
4. Dynamic concentration based on prediction confidence
5. Pair-based risk management (no individual coin stop losses)

## Inflow Prediction Methodology

### Dependent Variable: "Inflow"
**Definition:** Change in price multiplied by trading volume over a one-hour window
**Formula:** `Inflow = ΔPrice × Volume (1-hour window)`
**Data Source:** CryptoCompare hourly data
**Purpose:** Captures momentum and liquidity-adjusted price movements

### Independent Variables: Sentiment and Additional Factors
**Primary Data Source:** CryptoNews API with quality-based source bucketing
**Feature Engineering:** Random Forest model with enhanced feature structure:

**Sentiment Features (Quality-Bucketed by News Source):**
- **High-Quality Sources Bucket:** Premium financial news outlets
- **Medium-Quality Sources Bucket:** Established crypto news sites
- **General Sources Bucket:** Broader news aggregation
- **Time Buckets per Quality Level:**
  - Last 24 hours: Positive, negative, neutral article counts
  - Last 3 days (excluding last 24 hours): Positive, negative, neutral article counts
  - Last 10 days (excluding last 3 days): Positive, negative, neutral article counts

**Additional Non-Sentiment Factors (TRS-Recommended):**
- Technical indicators (RSI, MACD, moving averages)
- Volume-based momentum indicators
- Cross-asset correlation factors
- Market microstructure indicators

**Model Structure:**
```
Enhanced Random Forest Model:
Input: Quality-bucketed sentiment features + additional technical factors
Output: Inflow prediction per coin
Calibration: Cross-coin training for generalization
Quality Scoring: Source-based weighting for sentiment reliability
```

### Liquidity Adjustment: Gamma Factor
**Purpose:** Convert inflow prediction to price shift expectation
**Formula:** `Expected Price Shift = Inflow Prediction × Gamma`
**Calibration:** Per-coin gamma calibration over shorter horizon than tree model
**Estimation Methods:** Ensemble approach using:
- Rolling window estimation
- Exponential decay weighting
- Regime-switching models
**Exclusion Criteria:** Coins with coefficient of variation > `gamma_cv_threshold`% over `gamma_window_days` day window
**Confidence Intervals:** Wide gamma confidence bands trigger exclusion

## Universe Definition and Filtering

### Initial Universe Criteria:
1. **Liquidity Requirement:** Reasonably liquid cryptocurrencies only
2. **Exclusion Rule:** No stablecoins in trading universe
3. **Data Availability:** Must have reliable CryptoCompare and news data

### Risk-Based Exclusions (Tunable Thresholds):
1. **Gamma Instability:** Coefficient of variation > `gamma_cv_threshold` over `gamma_window_days` window
2. **Liquidity Requirements:**
   - Minimum daily volume: `min_daily_volume` USD equivalent
   - Maximum bid-ask spread: `max_bid_ask_spread`%
   - Minimum market cap: `min_market_cap` USD
3. **Data Quality Thresholds:**
   - Minimum news coverage: `min_news_coverage` articles per week
   - Price data completeness: `min_data_completeness`% over trailing 30 days
   - Volume anomaly exclusion: >`volume_anomaly_std` standard deviations from 30-day mean

## Portfolio Construction Algorithm

### Step 1: Universe Ranking
1. Generate inflow predictions for all eligible coins using trained Random Forest
2. Apply gamma factors to convert to expected price shifts
3. Rank entire universe by expected return (highest to lowest)

### Step 2: Risk-Based Filtering
1. Apply coin exclusion criteria based on risk metrics
2. Remove coins with unstable gamma factors
3. Ensure remaining universe meets liquidity requirements

### Step 3: Symmetrical Pair Formation
**Pairing Logic:**
- Pair #1: Top-ranked coin (long) + Bottom-ranked coin (short)
- Pair #2: Second-ranked coin (long) + Penultimate coin (short)
- Continue until optimal number of pairs formed

**Dynamic Pair Count:**
- **High Confidence:** Fewer pairs (concentrated positions)
- **Low Confidence:** More pairs (diversified positions)
- **Concentration Limit:** Maximum 5% per individual coin (tunable parameter)

### Step 4: Position Sizing
**Cash Buffer:** `cash_buffer_ratio`% cash maintained for operational smoothing
**Position Allocation:** Remaining (100% - `cash_buffer_ratio`%) allocated across pairs
**Risk Considerations:** Pair-level risk metrics and portfolio-level constraints

## Risk Management Framework

### Concentration Limits:
- **Individual Coin:** Maximum `max_coin_concentration`% of portfolio
- **Individual Pair:** Derived from coin limits and pair structure
- **Portfolio Diversification:** Between `min_pairs_count` and `max_pairs_count` pairs based on confidence levels

### Risk Metrics (Tunable Parameters):
**Pair-Level Metrics:**
- Individual pair stop-loss: `pair_stop_loss_threshold`% drawdown from pair inception
- Per-pair risk budget: `per_pair_risk_budget`% of total portfolio value maximum loss
- Pair correlation monitoring: Exclude if correlation exceeds `max_pair_correlation`

**Portfolio-Level Metrics:**
- Portfolio stop-loss: `portfolio_stop_loss_threshold`% from high-water mark
- Total active risk budget: `total_active_risk_budget`% of portfolio maximum
- Volatility spike trigger: Rebalance if volatility exceeds `max_volatility_multiple`x historical average

**Model Performance Monitoring:**
- Track prediction accuracy over time
- Implement model retraining triggers based on performance degradation
- Monitor sentiment factor stability and importance
- Sentiment-return correlation stability monitoring

## Rebalancing Strategy

### Rebalancing Frequency:
**Target Portfolio Review:** Hourly assessment
**Rebalancing Execution:** Only when material benefit exceeds trading costs
**Minimum Trading Frequency:** `min_rebalance_interval` hours (conservative default)
**Dynamic Frequency:** Conservative approach - if in doubt, trade less
**Frequency Adaptation:**
- High volatility periods: Reduce to `min_rebalance_interval` minimum intervals
- Low volatility periods: Allow more frequent rebalancing
- Market stress: Suspend when volatility exceeds `market_stress_suspend_threshold`

### Rebalancing Triggers:
1. **Cost-Benefit Analysis:** Expected return improvement (`min_expected_improvement` basis points minimum) > `transaction_cost_multiple`x trading costs
2. **Risk-Based Triggers:**
   - Pair correlation exceeds `max_pair_correlation` (loss of hedge effectiveness)
   - Pair volatility exceeds `max_volatility_multiple`x historical average
   - Prediction confidence falls below `prediction_confidence_threshold` percentile
3. **Threshold-Based:** Significant deviation from target allocation
4. **Data-Driven:** New information materially affecting predictions

### Trading Cost Considerations:
- Include all trading fees and spreads
- Include API licensing costs
- Factor transaction costs into rebalancing decisions
- Maintain cost tracking and reporting

## Model Enhancement Framework

### Continuous Improvement:
**Data Collection:** Continuously collect new market and sentiment data
**Model Recalibration:** Regular retraining of Random Forest model
**Gamma Adjustment:** Ongoing per-coin gamma factor optimization
**Performance Feedback:** Incorporate actual vs. predicted performance

### Future Extensions:
**Additional Features:** Enhanced technical indicators and macro-economic data integration
**Alternative Data:** Additional sentiment sources beyond CryptoNews API (best-efforts basis)
**GenAI Layer:** Potential overlay for enhanced news analysis and strategy explanation
**Ensemble Methods:** Multiple model combination for improved predictions
**Operational Risk Controls:**
- Emergency liquidation procedures for extreme market conditions
- Data provider backup systems (best-efforts basis)
- Position size caps beyond standard concentration limits

## Performance Targets and Expectations

### Return Objectives:
**Primary Goal:** Generate consistent risk-adjusted returns through market-neutral strategy
**Cash Buffer Impact:** 10% cash allocation affects absolute return but smooths operations
**Risk-Return Profile:** Optimize Sharpe ratio through pair selection and risk management

### Risk Tolerance:
**Drawdown Management:** Portfolio-level and pair-level stop losses
**Concentration Risk:** Diversification requirements based on prediction confidence
**Operational Risk:** Robust data quality and system reliability requirements

## Tunable Parameters

### Core Strategy Parameters
| Parameter | Description | Default Value | Range | Unit |
|-----------|-------------|---------------|--------|------|
| `max_coin_concentration` | Maximum portfolio allocation per coin | 5.0 | 1.0 - 20.0 | % |
| `cash_buffer_ratio` | Cash buffer maintained for operations | 10.0 | 5.0 - 25.0 | % |
| `max_pairs_count` | Maximum number of active pairs | 10 | 2 - 20 | pairs |
| `min_pairs_count` | Minimum number of active pairs | 2 | 1 - 5 | pairs |

### Risk Management Parameters
| Parameter | Description | Default Value | Range | Unit |
|-----------|-------------|---------------|--------|------|
| `pair_stop_loss_threshold` | Individual pair stop-loss trigger | -15.0 | -5.0 to -25.0 | % |
| `portfolio_stop_loss_threshold` | Portfolio-wide stop-loss trigger | -10.0 | -5.0 to -20.0 | % |
| `per_pair_risk_budget` | Maximum loss per pair | 2.0 | 1.0 - 5.0 | % of portfolio |
| `total_active_risk_budget` | Total portfolio risk budget | 8.0 | 4.0 - 15.0 | % of portfolio |
| `max_pair_correlation` | Maximum allowed pair correlation | 0.7 | 0.5 - 0.9 | correlation |
| `max_volatility_multiple` | Volatility spike rebalancing trigger | 2.0 | 1.5 - 3.0 | multiple |

### Universe Filtering Parameters
| Parameter | Description | Default Value | Range | Unit |
|-----------|-------------|---------------|--------|------|
| `gamma_cv_threshold` | Gamma coefficient of variation limit | 50.0 | 25.0 - 100.0 | % |
| `min_daily_volume` | Minimum daily trading volume | 1.0 | 0.5 - 10.0 | $M USD |
| `max_bid_ask_spread` | Maximum bid-ask spread | 2.0 | 1.0 - 5.0 | % |
| `min_market_cap` | Minimum market capitalization | 100.0 | 50.0 - 500.0 | $M USD |
| `min_news_coverage` | Minimum articles per week | 3 | 1 - 10 | articles |
| `min_data_completeness` | Minimum price data availability | 95.0 | 90.0 - 99.0 | % |
| `volume_anomaly_std` | Volume anomaly exclusion threshold | 5.0 | 3.0 - 10.0 | std dev |

### Rebalancing Parameters
| Parameter | Description | Default Value | Range | Unit |
|-----------|-------------|---------------|--------|------|
| `min_rebalance_interval` | Minimum time between rebalances | 4.0 | 1.0 - 24.0 | hours |
| `min_expected_improvement` | Minimum return improvement to rebalance | 25.0 | 10.0 - 50.0 | basis points |
| `transaction_cost_multiple` | Cost-benefit multiplier threshold | 1.5 | 1.2 - 3.0 | multiple |
| `prediction_confidence_threshold` | Minimum confidence for rebalancing | 60.0 | 50.0 - 80.0 | percentile |
| `portfolio_review_frequency` | Target portfolio assessment frequency | 1.0 | 0.5 - 4.0 | hours |

### Model Parameters
| Parameter | Description | Default Value | Range | Unit |
|-----------|-------------|---------------|--------|------|
| `gamma_window_days` | Gamma estimation window | 30 | 14 - 90 | days |
| `model_retraining_threshold` | Performance degradation retraining trigger | 20.0 | 10.0 - 40.0 | % accuracy drop |
| `sentiment_stability_window` | Sentiment correlation monitoring window | 30 | 14 - 90 | days |
| `news_quality_weight_high` | High-quality source weighting | 1.5 | 1.0 - 3.0 | weight |
| `news_quality_weight_medium` | Medium-quality source weighting | 1.0 | 0.5 - 2.0 | weight |
| `news_quality_weight_general` | General source weighting | 0.5 | 0.1 - 1.0 | weight |

### Time Bucket Parameters
| Parameter | Description | Default Value | Range | Unit |
|-----------|-------------|---------------|--------|------|
| `recent_news_hours` | Recent news time bucket | 24 | 12 - 48 | hours |
| `medium_news_days` | Medium-term news time bucket | 3 | 2 - 7 | days |
| `long_news_days` | Long-term news time bucket | 10 | 7 - 21 | days |

### Performance Monitoring Parameters
| Parameter | Description | Default Value | Range | Unit |
|-----------|-------------|---------------|--------|------|
| `hourly_reporting_enabled` | Enable hourly performance reports | true | true/false | boolean |
| `daily_reporting_enabled` | Enable daily aggregated reports | true | true/false | boolean |
| `var_confidence_level` | Value at Risk confidence level | 95.0 | 90.0 - 99.0 | % |
| `var_time_horizon` | VaR calculation time horizon | 1 | 1 - 7 | days |
| `sharpe_rolling_window` | Sharpe ratio calculation window | 30 | 14 - 90 | days |

### Emergency Controls
| Parameter | Description | Default Value | Range | Unit |
|-----------|-------------|---------------|--------|------|
| `emergency_liquidation_threshold` | Emergency liquidation trigger | -20.0 | -15.0 to -30.0 | % portfolio loss |
| `market_stress_suspend_threshold` | Market stress suspension trigger | 80.0 | 60.0 - 100.0 | volatility index |
| `max_position_override` | Emergency position size cap | 10.0 | 5.0 - 20.0 | % of portfolio |

## Implementation Requirements

### Data Infrastructure:
- Hourly market data from CryptoCompare
- Real-time news sentiment from CryptoNews API
- Robust data quality monitoring and validation
- Historical data retention for model improvement

### Model Operations:
- Automated Random Forest training and retraining
- Real-time gamma factor calculation and monitoring
- Continuous risk metric evaluation
- Portfolio optimization and rebalancing execution

### Risk Controls:
- Real-time risk monitoring and alerting
- Automated coin exclusion based on risk criteria
- Stop-loss execution at pair and portfolio levels
- Trading cost tracking and optimization

---

**Algorithm Status:** Specification Complete - Awaiting TRS Review
**TRS Review Required For:**
1. Default risk metrics and thresholds for coin exclusion
2. Pair-level and portfolio-level stop-loss recommendations
3. Risk-return optimization approach for rebalancing decisions
4. Specific performance and risk metrics for hourly/daily reporting

*Document Owner: SDM*
*Approval Authority: Project Owner*
*Created: Setup Phase*
*Modification Rights: Project Owner Only*