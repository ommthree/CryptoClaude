# CryptoClaude Control Interface Guide

## 🚀 Overview

CryptoClaude now features two powerful new capabilities:
1. **Claude-as-Feature Architecture**: Claude AI integrated directly into Random Forest predictions
2. **Beautiful Web Control Dashboard**: Simple, attractive GUI for managing your trading system

---

## 🤖 Claude-as-Feature Architecture

### What Changed?

Previously, Claude acted as a "polish layer" that adjusted Random Forest predictions by ±20%. Now Claude provides **12 new features** that become part of the Random Forest training data.

### New Claude Features in Random Forest

```cpp
// Sentiment and narrative features
double claude_market_sentiment;      // -1.0 to +1.0 from Claude analysis
double claude_news_impact;           // 0.0 to 1.0 news impact magnitude
double claude_social_momentum;       // -1.0 to +1.0 social media momentum
double claude_narrative_strength;    // 0.0 to 1.0 narrative strength

// Market regime features from Claude
double claude_regime_change_prob;    // 0.0 to 1.0 regime change probability
double claude_volatility_forecast;   // 0.0 to 1.0 volatility forecast
double claude_uncertainty_level;     // 0.0 to 1.0 market uncertainty

// Risk and timing features from Claude
double claude_tail_risk;             // 0.0 to 1.0 tail risk indicator
double claude_momentum_sustainability; // 0.0 to 1.0 momentum sustainability
double claude_mean_reversion;        // 0.0 to 1.0 mean reversion signal

// Macro and institutional features from Claude
double claude_institutional_sentiment; // -1.0 to +1.0 institutional interest
double claude_regulatory_risk;       // 0.0 to 1.0 regulatory risk
double claude_analysis_confidence;   // 0.0 to 1.0 Claude's confidence
```

### Advantages

✅ **Auto-Calibration**: Random Forest learns Claude's prediction value automatically
✅ **Dynamic Weighting**: Forest adjusts Claude's influence based on historical performance
✅ **No Arbitrary Limits**: No more ±20% caps on adjustments
✅ **Backtestable**: Can validate Claude's contribution with historical simulations
✅ **Feature Importance**: Can see exactly how much Claude features contribute

### Usage

```cpp
// Create Claude feature provider
auto claude_provider = std::make_shared<AI::ClaudeFeatureProvider>(config);

// Set up Random Forest with Claude integration
RandomForestPredictor rf_predictor;
rf_predictor.setClaudeFeatureProvider(claude_provider);
rf_predictor.setClaudeFeaturesEnabled(true);

// Enhanced prediction with Claude features
MLFeatureVector features = rf_predictor.createFeaturesWithClaude(
    symbol, market_data, sentiment_data, claude_features, market_context
);

// Random Forest now uses all features including Claude's analysis
auto prediction = rf_predictor.predict(features);
```

---

## 🖥️ Control Dashboard

### Launch the Dashboard

**Simple Launch:**
```bash
./launch-dashboard.sh
```

**Custom Port:**
```bash
./launch-dashboard.sh --port 8081
```

**Manual Launch:**
```bash
cd src/Web
python3 dashboard_server.py --port 8080
```

### Dashboard Features

🎛️ **Trading Controls**
- Start/Stop/Pause trading with one click
- Emergency stop button for instant halt
- Trading mode switching (Paper/Live)

🤖 **AI Management**
- Toggle Claude AI features on/off
- Refresh predictions manually
- Recalibrate Random Forest model
- View Claude feature importance

📊 **Live Monitoring**
- Real-time portfolio value and P&L
- Active positions overview
- AI confidence levels
- System health status

📝 **Live Logs**
- Streaming system logs
- Color-coded log levels (INFO, SUCCESS, WARNING, ERROR)
- Auto-scrolling with history limit
- Clear logs functionality

⚙️ **System Management**
- System health checks
- Service restart capabilities
- Performance monitoring
- Data export functions

### Dashboard Architecture

```
Local Machine (Dashboard)     →     AWS Lightsail (CryptoClaude)
┌─────────────────────────┐         ┌────────────────────────────┐
│  Web Dashboard (8080)   │   SSH   │  cryptoclaude-console      │
│  Python Server         │  ────→  │  (Remote Application)      │
│  API Endpoints         │         │                            │
└─────────────────────────┘         └────────────────────────────┘
```

### Control Methods

**Method 1: Web Dashboard (Recommended)**
- Beautiful graphical interface
- Real-time monitoring
- One-click controls
- Live log streaming

**Method 2: SSH Console (Advanced)**
```bash
ssh -i ~/.ssh/lightsail-key.pem ubuntu@your-instance-ip
sudo systemctl status cryptoclaude
sudo journalctl -u cryptoclaude -f
```

**Method 3: Direct Console Commands**
```bash
# Via SSH to the remote instance
/opt/cryptoclaude/bin/cryptoclaude-console
```

---

## 🔗 Integration Guide

### Both Approaches Working Together

The system now supports **both** approaches simultaneously:

1. **Claude-as-Feature** (Primary): Claude analysis becomes Random Forest features
2. **Claude Polish** (Backup): Traditional ±20% adjustment system remains available

### Configuration

```json
{
  "claude_features": {
    "enabled": true,
    "provider": "claude-3-sonnet",
    "cache_duration_minutes": 15,
    "confidence_threshold": 0.6
  },
  "claude_polish": {
    "enabled": true,
    "max_adjustment_percent": 0.20,
    "fallback_mode": true
  }
}
```

### Deployment Integration

The dashboard works with both:
- **Local Development**: Run dashboard locally, connect to local CryptoClaude
- **AWS Production**: Run dashboard locally, connect via SSH/API to AWS Lightsail instance
- **Remote Dashboard**: Deploy dashboard on AWS alongside CryptoClaude

---

## 🚀 Quick Start

### 1. Launch Dashboard
```bash
./launch-dashboard.sh
```

### 2. Access Control Interface
- Open: http://localhost:8080
- Monitor: Portfolio performance, AI confidence, system logs
- Control: Start/stop trading, toggle Claude features

### 3. Test Claude Features
- Click "Toggle Claude AI" to enable/disable Claude-as-feature
- Click "Refresh Predictions" to get new Claude analysis
- Monitor logs to see Claude feature generation

### 4. Production Deployment
- Deploy to AWS: `./deploy/deploy-application.sh`
- Configure API keys: `cp deploy/api-keys.conf.template deploy/api-keys.conf`
- Launch dashboard: `./launch-dashboard.sh`

---

## 📊 Monitoring & Performance

### Key Metrics to Watch

**Dashboard Status Panel:**
- Portfolio Value & Change
- Active Positions Count
- Trading Mode (Paper/Live)
- AI Confidence Level

**Claude Feature Quality:**
- Analysis Confidence: >60% recommended
- Feature Coverage: All 12 features populated
- Update Frequency: Every 15 minutes optimal
- API Response Time: <5 seconds target

**System Performance:**
- Random Forest Prediction Time: <1 second
- Claude Feature Generation: <30 seconds
- Overall System Response: <2 seconds

### Troubleshooting

**Dashboard Won't Start:**
```bash
# Check if port is in use
lsof -i :8080

# Try different port
./launch-dashboard.sh --port 8081
```

**Claude Features Not Working:**
- Check API key in `deploy/api-keys.conf`
- Verify internet connectivity
- Monitor logs for Claude API errors

**AWS Connection Issues:**
- Verify SSH key path in deployment scripts
- Check AWS Lightsail instance status
- Test instance accessibility: `ping your-instance-ip`

---

## 🎯 Next Steps

With both Claude-as-feature and the control dashboard implemented, you now have:

✅ **Advanced AI Integration**: Claude analysis as Random Forest features
✅ **Professional Control Interface**: Beautiful web dashboard
✅ **Production-Ready Deployment**: Secure AWS infrastructure
✅ **Multiple Control Methods**: Web GUI, SSH, and direct console
✅ **Real-time Monitoring**: Live logs, metrics, and system health

**Ready for sophisticated algorithmic trading with AI-enhanced decision making and professional-grade control interfaces!**