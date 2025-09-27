#pragma once

#include <vector>
#include <string>
#include <map>
#include <set>
#include <chrono>
#include "../Config/ExpandedCoinUniverse.h"

namespace CryptoClaude {
namespace Risk {

/**
 * Sector Diversification Manager for TRS Phase 1 Implementation
 * Manages exposure across 12 market sectors with 25% maximum exposure per sector
 * Implements comprehensive diversification controls and monitoring
 */
class SectorDiversificationManager {
public:
    // Sector exposure tracking
    struct SectorExposure {
        Config::ExpandedCoinUniverse::MarketSector sector;
        std::vector<std::string> allocatedCoins;
        double currentExposure;          // Current % allocation to sector
        double targetExposure;           // Target % allocation
        double maxExposure;              // Maximum allowed % allocation (25%)
        int pairCount;                   // Number of pairs involving this sector
        double riskContribution;         // Sector contribution to portfolio risk

        bool isOverexposed() const { return currentExposure > maxExposure; }
        bool isUnderexposed() const { return currentExposure < (targetExposure * 0.5); }
        double getAvailableCapacity() const { return maxExposure - currentExposure; }
    };

    // Portfolio diversification metrics
    struct DiversificationMetrics {
        double sectorConcentrationRisk;  // Herfindahl index for sectors
        double crossSectorCorrelation;   // Average cross-sector correlation
        int activeSectorCount;           // Number of sectors with >1% exposure
        int minSectorsRequired;          // Minimum sectors for compliance (4)

        // TRS compliance metrics
        double maxSectorExposure;        // Highest sector exposure
        bool meetsMinimumSectors;        // At least 4 sectors represented
        bool meetsExposureLimits;        // No sector >25% exposure
        bool meetsDiversificationTarget; // Overall diversification adequate

        std::chrono::system_clock::time_point lastUpdated;
    };

    // Sector rebalancing recommendations
    struct RebalancingRecommendations {
        std::vector<std::string> overexposedSectors;    // Sectors to reduce
        std::vector<std::string> underexposedSectors;   // Sectors to increase
        std::vector<std::pair<std::string, std::string>> suggestedSwaps; // Pair swaps

        double estimatedImprovementScore;  // Expected diversification improvement
        int recommendedActions;            // Number of rebalancing actions
        bool urgentRebalancingRequired;    // Emergency rebalancing needed

        std::string rationale;             // Explanation of recommendations
    };

    // Sector allocation constraints for Phase 1
    struct SectorAllocationConstraints {
        double maxSectorExposure = 0.25;        // 25% maximum per sector (TRS)
        double minSectorExposure = 0.02;        // 2% minimum for active sectors
        int minActiveSectors = 4;               // Minimum 4 sectors required
        int targetActiveSectors = 8;            // Target 8+ sectors for diversification

        // Sector-specific constraints
        std::map<Config::ExpandedCoinUniverse::MarketSector, double> sectorMaxExposures;
        std::map<Config::ExpandedCoinUniverse::MarketSector, double> sectorTargets;

        // Dynamic constraints based on market conditions
        bool enableDynamicLimits = true;
        double volatilityAdjustmentFactor = 1.2; // Tighten limits in high vol
        double correlationAdjustmentFactor = 0.8; // Adjust for correlation spikes
    };

public:
    SectorDiversificationManager();
    ~SectorDiversificationManager() = default;

    // Main diversification management
    void initializeSectorAllocation();
    bool validateSectorDiversification(const std::vector<std::string>& selectedPairs);
    RebalancingRecommendations generateRebalancingPlan();

    // Exposure tracking and monitoring
    void updateSectorExposures(const std::map<std::string, double>& pairWeights);
    DiversificationMetrics calculateDiversificationMetrics();
    std::vector<SectorExposure> getCurrentSectorExposures() const;

    // Pair selection support
    bool canAddPairToSector(Config::ExpandedCoinUniverse::MarketSector sector, double weight);
    std::vector<Config::ExpandedCoinUniverse::MarketSector> getAvailableSectors(double requiredCapacity);
    double calculateSectorCapacity(Config::ExpandedCoinUniverse::MarketSector sector) const;

    // Risk assessment
    double assessSectorConcentrationRisk() const;
    double calculateCrossSectorCorrelation() const;
    std::map<Config::ExpandedCoinUniverse::MarketSector, double> getSectorRiskContributions() const;

    // TRS compliance validation
    bool validateTRSCompliance() const;
    std::vector<std::string> getTRSComplianceIssues() const;
    void generateComplianceReport() const;

    // Configuration management
    void setSectorConstraints(const SectorAllocationConstraints& constraints);
    SectorAllocationConstraints getSectorConstraints() const;

    // Emergency controls
    void enableEmergencyDiversification();  // Force maximum diversification
    void applyEmergencyLimits();           // Apply strictest sector limits
    bool requiresEmergencyIntervention() const;

    // Reporting and analytics
    void generateSectorAllocationReport() const;
    void exportSectorData(const std::string& filename) const;
    void printDiversificationSummary() const;

private:
    // Internal state
    std::map<Config::ExpandedCoinUniverse::MarketSector, SectorExposure> sectorExposures_;
    SectorAllocationConstraints constraints_;
    DiversificationMetrics lastMetrics_;
    bool emergencyMode_ = false;

    // Sector mapping and utilities
    std::map<std::string, Config::ExpandedCoinUniverse::MarketSector> coinSectorMap_;

    // Helper methods
    void initializeSectorMapping();
    void updateSectorMetrics();
    Config::ExpandedCoinUniverse::MarketSector getCoinSector(const std::string& symbol) const;

    // Risk calculation helpers
    double calculateHerfindahlIndex() const;
    double calculateSectorCorrelationMatrix() const;
    std::vector<std::pair<std::string, std::string>> identifyRebalancingOpportunities();

    // Constants
    static constexpr double TRS_MAX_SECTOR_EXPOSURE = 0.25;    // 25% TRS limit
    static constexpr int TRS_MIN_ACTIVE_SECTORS = 4;           // Minimum sector count
    static constexpr double EMERGENCY_SECTOR_LIMIT = 0.20;     // 20% emergency limit
};

/**
 * Advanced sector correlation monitor for dynamic risk management
 * Tracks inter-sector correlations and adjusts limits dynamically
 */
class SectorCorrelationMonitor {
public:
    // Correlation tracking structure
    struct SectorCorrelationData {
        Config::ExpandedCoinUniverse::MarketSector sector1;
        Config::ExpandedCoinUniverse::MarketSector sector2;
        double currentCorrelation;
        double averageCorrelation;
        double correlationVolatility;

        std::vector<double> historicalCorrelations;
        std::chrono::system_clock::time_point lastUpdate;

        bool isHighCorrelation() const { return currentCorrelation > 0.7; }
        bool isCorrelationSpiking() const { return currentCorrelation > (averageCorrelation + 2 * correlationVolatility); }
    };

    // Correlation matrix for all sector pairs
    struct SectorCorrelationMatrix {
        std::map<std::pair<Config::ExpandedCoinUniverse::MarketSector, Config::ExpandedCoinUniverse::MarketSector>, double> correlations;
        double averageCorrelation;
        double maxCorrelation;
        int highCorrelationPairs;      // Pairs with >0.7 correlation

        std::chrono::system_clock::time_point lastUpdate;
        bool requiresRebalancing;      // High correlations detected
    };

    // Dynamic adjustment recommendations
    struct CorrelationAdjustments {
        std::map<Config::ExpandedCoinUniverse::MarketSector, double> adjustedLimits;
        std::vector<std::pair<Config::ExpandedCoinUniverse::MarketSector, Config::ExpandedCoinUniverse::MarketSector>> problematicPairs;

        double recommendedMaxExposure; // Adjusted max exposure recommendation
        bool emergencyAdjustmentRequired;
        std::string adjustmentReason;
    };

public:
    SectorCorrelationMonitor();
    ~SectorCorrelationMonitor() = default;

    // Correlation tracking
    void updateSectorCorrelations();
    SectorCorrelationMatrix calculateCorrelationMatrix();
    std::vector<SectorCorrelationData> getHighCorrelationPairs() const;

    // Dynamic limit adjustment
    CorrelationAdjustments calculateDynamicLimits();
    void applyCorrelationAdjustments(SectorDiversificationManager& diversificationManager);

    // Risk monitoring
    bool detectCorrelationSpikes() const;
    std::vector<std::string> getCorrelationAlerts() const;
    double assessCorrelationRisk() const;

    // Configuration
    void setCorrelationThresholds(double warning, double critical);
    void setUpdateFrequency(std::chrono::minutes frequency);

private:
    // Internal tracking
    std::map<std::pair<Config::ExpandedCoinUniverse::MarketSector, Config::ExpandedCoinUniverse::MarketSector>, SectorCorrelationData> correlationData_;
    SectorCorrelationMatrix lastMatrix_;

    // Thresholds
    double warningCorrelation_ = 0.6;
    double criticalCorrelation_ = 0.8;
    std::chrono::minutes updateFrequency_{60}; // 1 hour default

    // Helper methods
    double calculateSectorCorrelation(Config::ExpandedCoinUniverse::MarketSector sector1,
                                    Config::ExpandedCoinUniverse::MarketSector sector2);
    void updateCorrelationHistory();
    std::vector<std::string> generateCorrelationAlerts();
};

} // namespace Risk
} // namespace CryptoClaude