#pragma once

#include "StatisticalTools.h"
#include "../Common/TradingTypes.h"
#include <vector>
#include <map>
#include <string>
#include <chrono>
#include <memory>

namespace CryptoClaude {
namespace Analytics {

// Cross-asset correlation data structures
struct CrossAssetPair {
    std::string crypto_symbol;      // BTC, ETH, etc.
    std::string traditional_symbol; // SPX, GLD, DXY, etc.
    std::string description;

    CrossAssetPair() = default;
    CrossAssetPair(const std::string& crypto, const std::string& traditional, const std::string& desc)
        : crypto_symbol(crypto), traditional_symbol(traditional), description(desc) {}
};

struct CorrelationSnapshot {
    CrossAssetPair pair;
    double correlation;
    double correlation_3d;     // 3-day correlation
    double correlation_7d;     // 7-day correlation
    double correlation_30d;    // 30-day correlation
    double p_value;           // Statistical significance
    double correlation_change_24h;
    std::chrono::system_clock::time_point timestamp;
    int sample_size;
    bool is_significant;      // p < 0.05

    // Market stress indicators
    bool correlation_spike;   // >2 std dev change
    bool regime_change;       // Significant correlation shift
    double z_score;          // Correlation change z-score
};

struct MarketStressSignal {
    std::chrono::system_clock::time_point timestamp;
    std::string stress_type;  // "correlation_spike", "regime_change", "flight_to_quality"
    std::vector<std::string> affected_pairs;
    double stress_magnitude;  // 0.0 to 1.0
    std::string description;

    // Risk implications
    double portfolio_risk_multiplier; // Suggested risk adjustment
    std::vector<std::string> recommended_actions;
};

// Traditional asset data integration
struct TraditionalAssetDataPoint {
    std::string symbol;       // SPX, GLD, DXY, VIX
    double price;
    double volume;
    double change_24h;
    std::chrono::system_clock::time_point timestamp;
    std::string source;       // Data source identifier
};

class TraditionalAssetIntegration {
public:
    TraditionalAssetIntegration();
    ~TraditionalAssetIntegration();

    // Data retrieval methods
    bool updateTraditionalAssetData();
    std::vector<TraditionalAssetDataPoint> getAssetData(const std::string& symbol, int days = 30);
    std::map<std::string, std::vector<double>> getReturnsData(const std::vector<std::string>& symbols, int days = 30);

    // Asset availability
    std::vector<std::string> getSupportedAssets() const;
    bool isAssetSupported(const std::string& symbol) const;
    std::chrono::system_clock::time_point getLastUpdate(const std::string& symbol) const;

    // Data quality
    double getDataQuality(const std::string& symbol) const;
    bool isDataStale(const std::string& symbol, int max_age_minutes = 60) const;

private:
    std::map<std::string, std::vector<TraditionalAssetDataPoint>> asset_data_cache_;
    std::map<std::string, std::chrono::system_clock::time_point> last_update_times_;

    // Supported traditional assets
    std::vector<std::string> supported_assets_ = {
        "SPX",   // S&P 500 Index
        "GLD",   // Gold ETF
        "DXY",   // US Dollar Index
        "VIX",   // Volatility Index
        "TLT",   // 20+ Year Treasury Bond ETF
        "HYG"    // High Yield Corporate Bond ETF
    };

    bool fetchAssetDataFromAPI(const std::string& symbol);
    std::vector<double> calculateReturns(const std::vector<TraditionalAssetDataPoint>& data);
};

// Correlation stress detection algorithms
class CorrelationStressDetector {
public:
    CorrelationStressDetector(double spike_threshold = 2.0, double regime_threshold = 0.3);
    ~CorrelationStressDetector();

    // Stress detection methods
    std::vector<MarketStressSignal> detectStressSignals(
        const std::vector<CorrelationSnapshot>& correlations);
    bool isCorrelationSpike(const CorrelationSnapshot& current, const CorrelationSnapshot& previous);
    bool isRegimeChange(const std::vector<CorrelationSnapshot>& correlation_history);

    // Market stress analysis
    double calculateMarketStress(const std::vector<CorrelationSnapshot>& correlations);
    std::vector<std::string> generateRiskRecommendations(const std::vector<MarketStressSignal>& signals);

    // Configuration
    void setSpikeThreshold(double threshold) { spike_threshold_ = threshold; }
    void setRegimeThreshold(double threshold) { regime_threshold_ = threshold; }

    // Utility method for correlation analysis
    double calculateCorrelationZScore(double current, const std::vector<double>& history);

private:
    double spike_threshold_;   // Z-score threshold for correlation spikes
    double regime_threshold_;  // Threshold for regime change detection

    // Historical correlation tracking for regime detection
    std::map<std::string, std::vector<double>> correlation_history_;
    MarketStressSignal createStressSignal(const std::string& type,
                                        const std::vector<std::string>& pairs,
                                        double magnitude);
};

// Main cross-asset correlation monitoring system
class CrossAssetCorrelationMonitor {
public:
    CrossAssetCorrelationMonitor();
    ~CrossAssetCorrelationMonitor();

    // Initialization and configuration
    bool initialize();
    void addCrossAssetPair(const std::string& crypto, const std::string& traditional,
                          const std::string& description);
    void removeCrossAssetPair(const std::string& crypto, const std::string& traditional);

    // Real-time monitoring
    bool updateCorrelations();
    std::vector<CorrelationSnapshot> getCurrentCorrelations() const;
    std::vector<CorrelationSnapshot> getCorrelationHistory(const CrossAssetPair& pair, int days = 30) const;

    // Market stress monitoring
    std::vector<MarketStressSignal> getCurrentStressSignals() const;
    double getCurrentMarketStress() const;

    // Correlation analysis
    CorrelationSnapshot calculateCorrelation(const CrossAssetPair& pair);
    std::map<std::string, double> getCryptoTraditionalCorrelations(const std::string& crypto_symbol);

    // Risk management integration
    double getPortfolioRiskAdjustment() const;
    std::vector<std::string> getRiskRecommendations() const;

    // Data access
    std::vector<CrossAssetPair> getMonitoredPairs() const { return monitored_pairs_; }
    bool isMonitoringActive() const { return monitoring_active_; }
    std::chrono::system_clock::time_point getLastUpdate() const { return last_update_; }

    // Correlation matrix access for stress testing integration
    std::map<std::pair<std::string, std::string>, double> getCorrelationMatrix() const;

    // Performance metrics
    struct PerformanceMetrics {
        double update_frequency_hz;
        double avg_calculation_time_ms;
        int successful_updates_24h;
        int failed_updates_24h;
        double data_quality_score;
    };
    PerformanceMetrics getPerformanceMetrics() const;

private:
    // Core components
    std::unique_ptr<TraditionalAssetIntegration> traditional_assets_;
    std::unique_ptr<CorrelationStressDetector> stress_detector_;

    // Configuration
    std::vector<CrossAssetPair> monitored_pairs_;
    bool monitoring_active_;
    std::chrono::system_clock::time_point last_update_;

    // Data storage
    std::map<std::string, std::vector<CorrelationSnapshot>> correlation_history_;
    std::vector<MarketStressSignal> current_stress_signals_;
    double current_market_stress_;

    // Performance tracking
    mutable PerformanceMetrics performance_metrics_;

    // Default cross-asset pairs for monitoring
    void initializeDefaultPairs();

    // Market data integration
    std::vector<double> getCryptoReturns(const std::string& symbol, int days);
    std::vector<double> getTraditionalReturns(const std::string& symbol, int days);

    // Correlation calculation helpers
    CorrelationSnapshot calculatePairCorrelation(const CrossAssetPair& pair);
    void updateCorrelationHistory(const CorrelationSnapshot& snapshot);

    // Stress monitoring
    void updateStressSignals();
    void calculateMarketStress();

    // Data validation
    bool validateCorrelationData(const std::vector<double>& crypto_returns,
                               const std::vector<double>& traditional_returns);
};

// Cross-asset correlation utilities
namespace CrossAssetUtils {
    // Predefined cross-asset pairs of interest
    const std::vector<CrossAssetPair> DEFAULT_PAIRS = {
        {"BTC", "SPX", "Bitcoin vs S&P 500"},
        {"BTC", "GLD", "Bitcoin vs Gold"},
        {"BTC", "DXY", "Bitcoin vs US Dollar Index"},
        {"BTC", "VIX", "Bitcoin vs Volatility Index"},
        {"ETH", "SPX", "Ethereum vs S&P 500"},
        {"ETH", "GLD", "Ethereum vs Gold"},
        {"ETH", "TLT", "Ethereum vs Treasury Bonds"},
        {"ADA", "HYG", "Cardano vs High Yield Bonds"}
    };

    // Correlation interpretation helpers
    std::string interpretCorrelation(double correlation);
    std::string getMarketRegime(double btc_spx_corr, double btc_vix_corr);
    std::vector<std::string> generateCorrelationInsights(const std::vector<CorrelationSnapshot>& correlations);

    // Risk management utilities
    double calculateCorrelationRiskAdjustment(const std::vector<CorrelationSnapshot>& correlations);
    std::map<std::string, double> getAssetCorrelationWeights(
        const std::vector<CorrelationSnapshot>& correlations,
        const std::vector<std::string>& portfolio_assets);
}

} // namespace Analytics
} // namespace CryptoClaude