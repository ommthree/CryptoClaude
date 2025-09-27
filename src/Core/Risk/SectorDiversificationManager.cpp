#include "SectorDiversificationManager.h"
#include "../Config/ExpandedCoinUniverse.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <fstream>

namespace CryptoClaude {
namespace Risk {

SectorDiversificationManager::SectorDiversificationManager() {
    // Initialize TRS-compliant constraints
    constraints_.maxSectorExposure = 0.25;      // 25% TRS limit
    constraints_.minSectorExposure = 0.02;      // 2% minimum for active sectors
    constraints_.minActiveSectors = 4;          // TRS minimum 4 sectors
    constraints_.targetActiveSectors = 8;       // Target diversification
    constraints_.enableDynamicLimits = true;
    constraints_.volatilityAdjustmentFactor = 1.2;
    constraints_.correlationAdjustmentFactor = 0.8;

    // Initialize sector-specific targets based on market representation
    constraints_.sectorTargets[Config::ExpandedCoinUniverse::MarketSector::LAYER1] = 0.20;          // 20% target
    constraints_.sectorTargets[Config::ExpandedCoinUniverse::MarketSector::DEFI] = 0.25;            // 25% target
    constraints_.sectorTargets[Config::ExpandedCoinUniverse::MarketSector::SMART_CONTRACT] = 0.15;  // 15% target
    constraints_.sectorTargets[Config::ExpandedCoinUniverse::MarketSector::INTEROPERABILITY] = 0.12; // 12% target
    constraints_.sectorTargets[Config::ExpandedCoinUniverse::MarketSector::INFRASTRUCTURE] = 0.15;  // 15% target
    constraints_.sectorTargets[Config::ExpandedCoinUniverse::MarketSector::ORACLE] = 0.05;          // 5% target
    constraints_.sectorTargets[Config::ExpandedCoinUniverse::MarketSector::STORAGE] = 0.08;         // 8% target

    std::cout << "[SectorDiversificationManager] Initialized with TRS Phase 1 constraints\n";
    std::cout << "[SectorDiversificationManager] Maximum sector exposure: 25%\n";
    std::cout << "[SectorDiversificationManager] Minimum active sectors: 4\n";

    initializeSectorMapping();
    initializeSectorAllocation();
}

void SectorDiversificationManager::initializeSectorMapping() {
    // Create mapping from coin symbols to sectors using universe configuration
    auto coinUniverse = Config::ExpandedCoinUniverse::getFullCoinUniverse();

    for (const auto& coin : coinUniverse) {
        coinSectorMap_[coin.symbol] = coin.sector;
    }

    std::cout << "[SectorDiversificationManager] Mapped " << coinSectorMap_.size()
              << " coins to sectors\n";
}

void SectorDiversificationManager::initializeSectorAllocation() {
    // Initialize sector exposure tracking for all relevant sectors
    auto sectorTargets = constraints_.sectorTargets;

    for (const auto& [sector, target] : sectorTargets) {
        SectorExposure exposure;
        exposure.sector = sector;
        exposure.currentExposure = 0.0;
        exposure.targetExposure = target;
        exposure.maxExposure = constraints_.maxSectorExposure;
        exposure.pairCount = 0;
        exposure.riskContribution = 0.0;

        sectorExposures_[sector] = exposure;
    }

    std::cout << "[SectorDiversificationManager] Initialized " << sectorExposures_.size()
              << " sector allocation trackers\n";
}

bool SectorDiversificationManager::validateSectorDiversification(const std::vector<std::string>& selectedPairs) {
    std::cout << "\n--- Validating Sector Diversification ---\n";
    std::cout << "Analyzing " << selectedPairs.size() << " selected pairs for TRS compliance...\n";

    // Reset sector tracking
    for (auto& [sector, exposure] : sectorExposures_) {
        exposure.allocatedCoins.clear();
        exposure.pairCount = 0;
        exposure.currentExposure = 0.0;
    }

    // Analyze sector representation in selected pairs
    std::map<Config::ExpandedCoinUniverse::MarketSector, int> sectorPairCounts;
    std::set<std::string> uniqueCoins;

    for (const auto& pairStr : selectedPairs) {
        // Parse pair string (assuming format "SYMBOL1/SYMBOL2" or similar)
        // For now, simulate pair analysis
        size_t delimiter = pairStr.find('/');
        if (delimiter != std::string::npos) {
            std::string coin1 = pairStr.substr(0, delimiter);
            std::string coin2 = pairStr.substr(delimiter + 1);

            uniqueCoins.insert(coin1);
            uniqueCoins.insert(coin2);

            auto sector1 = getCoinSector(coin1);
            auto sector2 = getCoinSector(coin2);

            sectorPairCounts[sector1]++;
            sectorPairCounts[sector2]++;
        }
    }

    // Calculate sector exposures (equal weight assumption for validation)
    double equalWeight = 1.0 / selectedPairs.size();

    for (const auto& [sector, count] : sectorPairCounts) {
        if (sectorExposures_.find(sector) != sectorExposures_.end()) {
            sectorExposures_[sector].pairCount = count;
            sectorExposures_[sector].currentExposure = count * equalWeight;
        }
    }

    // Validate TRS compliance
    bool compliant = validateTRSCompliance();
    auto issues = getTRSComplianceIssues();

    std::cout << "Sector diversification validation:\n";
    for (const auto& [sector, exposure] : sectorExposures_) {
        if (exposure.pairCount > 0) {
            std::cout << "- " << Config::ExpandedCoinUniverse::getSectorName(sector)
                      << ": " << (exposure.currentExposure * 100) << "% ("
                      << exposure.pairCount << " pairs)\n";
        }
    }

    std::cout << "\nTRS Compliance: " << (compliant ? "✓ PASSED" : "✗ FAILED") << "\n";
    if (!issues.empty()) {
        std::cout << "Issues identified:\n";
        for (const auto& issue : issues) {
            std::cout << "  - " << issue << "\n";
        }
    }

    return compliant;
}

bool SectorDiversificationManager::validateTRSCompliance() const {
    // Check maximum sector exposure constraint (25%)
    for (const auto& [sector, exposure] : sectorExposures_) {
        if (exposure.currentExposure > constraints_.maxSectorExposure) {
            return false;
        }
    }

    // Check minimum active sectors constraint (4)
    int activeSectors = 0;
    for (const auto& [sector, exposure] : sectorExposures_) {
        if (exposure.currentExposure >= constraints_.minSectorExposure) {
            activeSectors++;
        }
    }

    if (activeSectors < constraints_.minActiveSectors) {
        return false;
    }

    // Check sector concentration risk
    double concentrationRisk = assessSectorConcentrationRisk();
    if (concentrationRisk > 0.5) { // Herfindahl index threshold
        return false;
    }

    return true;
}

std::vector<std::string> SectorDiversificationManager::getTRSComplianceIssues() const {
    std::vector<std::string> issues;

    // Check for overexposed sectors
    for (const auto& [sector, exposure] : sectorExposures_) {
        if (exposure.isOverexposed()) {
            issues.push_back("Sector " + Config::ExpandedCoinUniverse::getSectorName(sector) +
                            " exceeds 25% limit (" + std::to_string(exposure.currentExposure * 100) + "%)");
        }
    }

    // Check minimum sector count
    int activeSectors = 0;
    for (const auto& [sector, exposure] : sectorExposures_) {
        if (exposure.currentExposure >= constraints_.minSectorExposure) {
            activeSectors++;
        }
    }

    if (activeSectors < constraints_.minActiveSectors) {
        issues.push_back("Insufficient sector diversification: " + std::to_string(activeSectors) +
                        " active sectors (minimum: " + std::to_string(constraints_.minActiveSectors) + ")");
    }

    // Check concentration risk
    double concentrationRisk = assessSectorConcentrationRisk();
    if (concentrationRisk > 0.5) {
        issues.push_back("High sector concentration risk (Herfindahl Index: " +
                        std::to_string(concentrationRisk) + ")");
    }

    return issues;
}

SectorDiversificationManager::DiversificationMetrics SectorDiversificationManager::calculateDiversificationMetrics() {
    DiversificationMetrics metrics;
    metrics.lastUpdated = std::chrono::system_clock::now();

    // Calculate sector concentration risk (Herfindahl Index)
    metrics.sectorConcentrationRisk = calculateHerfindahlIndex();

    // Calculate cross-sector correlation
    metrics.crossSectorCorrelation = calculateSectorCorrelationMatrix();

    // Count active sectors
    metrics.activeSectorCount = 0;
    metrics.maxSectorExposure = 0.0;

    for (const auto& [sector, exposure] : sectorExposures_) {
        if (exposure.currentExposure >= constraints_.minSectorExposure) {
            metrics.activeSectorCount++;
        }
        metrics.maxSectorExposure = std::max(metrics.maxSectorExposure, exposure.currentExposure);
    }

    metrics.minSectorsRequired = constraints_.minActiveSectors;

    // TRS compliance checks
    metrics.meetsMinimumSectors = (metrics.activeSectorCount >= constraints_.minActiveSectors);
    metrics.meetsExposureLimits = (metrics.maxSectorExposure <= constraints_.maxSectorExposure);
    metrics.meetsDiversificationTarget = (metrics.sectorConcentrationRisk <= 0.4 &&
                                         metrics.activeSectorCount >= constraints_.targetActiveSectors);

    lastMetrics_ = metrics;
    return metrics;
}

double SectorDiversificationManager::calculateHerfindahlIndex() const {
    double herfindahlIndex = 0.0;

    for (const auto& [sector, exposure] : sectorExposures_) {
        herfindahlIndex += exposure.currentExposure * exposure.currentExposure;
    }

    return herfindahlIndex;
}

double SectorDiversificationManager::calculateSectorCorrelationMatrix() const {
    // Simulate cross-sector correlation calculation
    // In production, this would analyze actual historical correlations between sectors

    double totalCorrelation = 0.0;
    int correlationCount = 0;

    auto activeSectors = std::vector<Config::ExpandedCoinUniverse::MarketSector>();
    for (const auto& [sector, exposure] : sectorExposures_) {
        if (exposure.currentExposure > 0.0) {
            activeSectors.push_back(sector);
        }
    }

    // Calculate pairwise correlations
    for (size_t i = 0; i < activeSectors.size(); ++i) {
        for (size_t j = i + 1; j < activeSectors.size(); ++j) {
            // Simulate sector correlation (in production, calculate from historical data)
            double correlation = 0.3 + (static_cast<int>(activeSectors[i]) + static_cast<int>(activeSectors[j])) % 40 / 100.0;
            totalCorrelation += correlation;
            correlationCount++;
        }
    }

    return correlationCount > 0 ? totalCorrelation / correlationCount : 0.0;
}

double SectorDiversificationManager::assessSectorConcentrationRisk() const {
    // Use Herfindahl index as primary concentration risk measure
    double herfindahlIndex = calculateHerfindahlIndex();

    // Adjust for number of active sectors
    int activeSectors = 0;
    for (const auto& [sector, exposure] : sectorExposures_) {
        if (exposure.currentExposure >= constraints_.minSectorExposure) {
            activeSectors++;
        }
    }

    // Penalty for insufficient diversification
    if (activeSectors < constraints_.minActiveSectors) {
        herfindahlIndex *= 1.5; // Increase risk score
    }

    return std::min(1.0, herfindahlIndex);
}

SectorDiversificationManager::RebalancingRecommendations SectorDiversificationManager::generateRebalancingPlan() {
    std::cout << "\n--- Generating Sector Rebalancing Plan ---\n";

    RebalancingRecommendations recommendations;
    recommendations.estimatedImprovementScore = 0.0;
    recommendations.recommendedActions = 0;
    recommendations.urgentRebalancingRequired = false;

    // Identify overexposed and underexposed sectors
    for (const auto& [sector, exposure] : sectorExposures_) {
        std::string sectorName = Config::ExpandedCoinUniverse::getSectorName(sector);

        if (exposure.isOverexposed()) {
            recommendations.overexposedSectors.push_back(sectorName);
            recommendations.recommendedActions++;

            if (exposure.currentExposure > 0.30) { // Critical overexposure
                recommendations.urgentRebalancingRequired = true;
            }
        }

        if (exposure.isUnderexposed() && exposure.targetExposure > 0.0) {
            recommendations.underexposedSectors.push_back(sectorName);
            recommendations.recommendedActions++;
        }
    }

    // Generate specific rebalancing suggestions
    auto rebalancingOps = identifyRebalancingOpportunities();
    recommendations.suggestedSwaps = rebalancingOps;

    // Calculate improvement score
    if (recommendations.recommendedActions > 0) {
        double currentRisk = assessSectorConcentrationRisk();
        recommendations.estimatedImprovementScore = std::max(0.0, currentRisk - 0.3); // Target improvement
    }

    // Generate rationale
    if (recommendations.urgentRebalancingRequired) {
        recommendations.rationale = "Critical sector overexposure detected. Immediate rebalancing required for TRS compliance.";
    } else if (recommendations.recommendedActions > 0) {
        recommendations.rationale = "Sector imbalances detected. Rebalancing recommended to improve diversification.";
    } else {
        recommendations.rationale = "Sector allocation is within acceptable parameters. No immediate action required.";
    }

    std::cout << "Rebalancing recommendations:\n";
    std::cout << "- Actions needed: " << recommendations.recommendedActions << "\n";
    std::cout << "- Urgent rebalancing: " << (recommendations.urgentRebalancingRequired ? "Yes" : "No") << "\n";
    std::cout << "- Estimated improvement: " << (recommendations.estimatedImprovementScore * 100) << "%\n";

    return recommendations;
}

std::vector<std::pair<std::string, std::string>> SectorDiversificationManager::identifyRebalancingOpportunities() {
    std::vector<std::pair<std::string, std::string>> opportunities;

    // Find overexposed sectors that can be reduced and underexposed sectors that can be increased
    std::vector<Config::ExpandedCoinUniverse::MarketSector> overexposed, underexposed;

    for (const auto& [sector, exposure] : sectorExposures_) {
        if (exposure.isOverexposed()) {
            overexposed.push_back(sector);
        }
        if (exposure.isUnderexposed() && exposure.targetExposure > 0.0) {
            underexposed.push_back(sector);
        }
    }

    // Generate swap recommendations
    for (size_t i = 0; i < overexposed.size() && i < underexposed.size(); ++i) {
        std::string fromSector = Config::ExpandedCoinUniverse::getSectorName(overexposed[i]);
        std::string toSector = Config::ExpandedCoinUniverse::getSectorName(underexposed[i]);
        opportunities.emplace_back(fromSector, toSector);
    }

    return opportunities;
}

bool SectorDiversificationManager::canAddPairToSector(Config::ExpandedCoinUniverse::MarketSector sector, double weight) {
    auto it = sectorExposures_.find(sector);
    if (it == sectorExposures_.end()) {
        return false;
    }

    return (it->second.currentExposure + weight) <= constraints_.maxSectorExposure;
}

std::vector<Config::ExpandedCoinUniverse::MarketSector> SectorDiversificationManager::getAvailableSectors(double requiredCapacity) {
    std::vector<Config::ExpandedCoinUniverse::MarketSector> available;

    for (const auto& [sector, exposure] : sectorExposures_) {
        if (exposure.getAvailableCapacity() >= requiredCapacity) {
            available.push_back(sector);
        }
    }

    return available;
}

Config::ExpandedCoinUniverse::MarketSector SectorDiversificationManager::getCoinSector(const std::string& symbol) const {
    auto it = coinSectorMap_.find(symbol);
    if (it != coinSectorMap_.end()) {
        return it->second;
    }

    // Default to Layer 1 if not found
    return Config::ExpandedCoinUniverse::MarketSector::LAYER1;
}

void SectorDiversificationManager::enableEmergencyDiversification() {
    std::cout << "\n[EMERGENCY] Enabling emergency diversification mode\n";

    emergencyMode_ = true;

    // Apply stricter limits
    constraints_.maxSectorExposure = EMERGENCY_SECTOR_LIMIT; // 20% emergency limit
    constraints_.minActiveSectors = 6; // Require more sectors

    // Update all sector exposures
    for (auto& [sector, exposure] : sectorExposures_) {
        exposure.maxExposure = EMERGENCY_SECTOR_LIMIT;
    }

    std::cout << "[EMERGENCY] Applied emergency sector limits: 20% maximum per sector\n";
}

bool SectorDiversificationManager::requiresEmergencyIntervention() const {
    // Check for critical violations
    for (const auto& [sector, exposure] : sectorExposures_) {
        if (exposure.currentExposure > 0.35) { // 35% critical threshold
            return true;
        }
    }

    // Check for insufficient diversification
    int activeSectors = 0;
    for (const auto& [sector, exposure] : sectorExposures_) {
        if (exposure.currentExposure >= constraints_.minSectorExposure) {
            activeSectors++;
        }
    }

    if (activeSectors < 3) { // Critical minimum
        return true;
    }

    // Check concentration risk
    if (assessSectorConcentrationRisk() > 0.7) {
        return true;
    }

    return false;
}

void SectorDiversificationManager::generateSectorAllocationReport() const {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "SECTOR DIVERSIFICATION REPORT\n";
    std::cout << std::string(70, '=') << "\n";

    auto metrics = const_cast<SectorDiversificationManager*>(this)->calculateDiversificationMetrics();

    std::cout << "Overall Metrics:\n";
    std::cout << "- Active sectors: " << metrics.activeSectorCount << " (min: " << metrics.minSectorsRequired << ")\n";
    std::cout << "- Max sector exposure: " << (metrics.maxSectorExposure * 100) << "%\n";
    std::cout << "- Concentration risk: " << (metrics.sectorConcentrationRisk * 100) << "%\n";
    std::cout << "- Cross-sector correlation: " << (metrics.crossSectorCorrelation * 100) << "%\n\n";

    std::cout << "TRS Compliance:\n";
    std::cout << "- Minimum sectors: " << (metrics.meetsMinimumSectors ? "✓ Met" : "✗ Not met") << "\n";
    std::cout << "- Exposure limits: " << (metrics.meetsExposureLimits ? "✓ Met" : "✗ Not met") << "\n";
    std::cout << "- Diversification target: " << (metrics.meetsDiversificationTarget ? "✓ Met" : "✗ Not met") << "\n\n";

    std::cout << "Sector Breakdown:\n";
    for (const auto& [sector, exposure] : sectorExposures_) {
        if (exposure.currentExposure > 0.001) { // Only show sectors with meaningful exposure
            std::string sectorName = Config::ExpandedCoinUniverse::getSectorName(sector);
            std::cout << "- " << sectorName << ": " << (exposure.currentExposure * 100) << "% "
                      << "(target: " << (exposure.targetExposure * 100) << "%, "
                      << "pairs: " << exposure.pairCount << ")\n";
        }
    }

    if (emergencyMode_) {
        std::cout << "\n⚠️  EMERGENCY MODE ACTIVE - Enhanced diversification limits applied\n";
    }

    std::cout << std::string(70, '=') << "\n\n";
}

// SectorCorrelationMonitor implementation
SectorCorrelationMonitor::SectorCorrelationMonitor() {
    warningCorrelation_ = 0.6;
    criticalCorrelation_ = 0.8;
    updateFrequency_ = std::chrono::minutes(60);

    std::cout << "[SectorCorrelationMonitor] Initialized with correlation thresholds\n";
    std::cout << "[SectorCorrelationMonitor] Warning: 60%, Critical: 80%\n";
}

SectorCorrelationMonitor::SectorCorrelationMatrix SectorCorrelationMonitor::calculateCorrelationMatrix() {
    SectorCorrelationMatrix matrix;
    matrix.lastUpdate = std::chrono::system_clock::now();
    matrix.averageCorrelation = 0.0;
    matrix.maxCorrelation = 0.0;
    matrix.highCorrelationPairs = 0;
    matrix.requiresRebalancing = false;

    // Get all active sectors
    std::vector<Config::ExpandedCoinUniverse::MarketSector> sectors = {
        Config::ExpandedCoinUniverse::MarketSector::LAYER1,
        Config::ExpandedCoinUniverse::MarketSector::DEFI,
        Config::ExpandedCoinUniverse::MarketSector::SMART_CONTRACT,
        Config::ExpandedCoinUniverse::MarketSector::INTEROPERABILITY,
        Config::ExpandedCoinUniverse::MarketSector::INFRASTRUCTURE,
        Config::ExpandedCoinUniverse::MarketSector::ORACLE,
        Config::ExpandedCoinUniverse::MarketSector::STORAGE
    };

    double totalCorrelation = 0.0;
    int pairCount = 0;

    // Calculate correlations between all sector pairs
    for (size_t i = 0; i < sectors.size(); ++i) {
        for (size_t j = i + 1; j < sectors.size(); ++j) {
            double correlation = calculateSectorCorrelation(sectors[i], sectors[j]);

            auto sectorPair = std::make_pair(sectors[i], sectors[j]);
            matrix.correlations[sectorPair] = correlation;

            totalCorrelation += correlation;
            pairCount++;

            matrix.maxCorrelation = std::max(matrix.maxCorrelation, correlation);

            if (correlation > 0.7) {
                matrix.highCorrelationPairs++;
            }

            if (correlation > criticalCorrelation_) {
                matrix.requiresRebalancing = true;
            }
        }
    }

    matrix.averageCorrelation = pairCount > 0 ? totalCorrelation / pairCount : 0.0;

    lastMatrix_ = matrix;
    return matrix;
}

double SectorCorrelationMonitor::calculateSectorCorrelation(
    Config::ExpandedCoinUniverse::MarketSector sector1,
    Config::ExpandedCoinUniverse::MarketSector sector2) {

    // Simulate sector correlation calculation
    // In production, this would analyze actual historical price correlations between sectors

    // Base correlation depends on sector relationship
    double baseCorrelation = 0.3;

    // Similar sectors have higher correlation
    if ((sector1 == Config::ExpandedCoinUniverse::MarketSector::LAYER1 &&
         sector2 == Config::ExpandedCoinUniverse::MarketSector::SMART_CONTRACT) ||
        (sector1 == Config::ExpandedCoinUniverse::MarketSector::DEFI &&
         sector2 == Config::ExpandedCoinUniverse::MarketSector::INFRASTRUCTURE)) {
        baseCorrelation = 0.6;
    }

    // Add some time-based variation
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::hours>(now.time_since_epoch()).count();
    double variation = (timestamp + static_cast<int>(sector1) + static_cast<int>(sector2)) % 30 / 100.0;

    return std::max(0.1, std::min(0.9, baseCorrelation + variation - 0.15));
}

bool SectorCorrelationMonitor::detectCorrelationSpikes() const {
    for (const auto& [sectorPair, data] : correlationData_) {
        if (data.isCorrelationSpiking()) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> SectorCorrelationMonitor::getCorrelationAlerts() const {
    std::vector<std::string> alerts;

    for (const auto& [sectorPair, correlation] : lastMatrix_.correlations) {
        if (correlation > criticalCorrelation_) {
            std::string sector1Name = Config::ExpandedCoinUniverse::getSectorName(sectorPair.first);
            std::string sector2Name = Config::ExpandedCoinUniverse::getSectorName(sectorPair.second);

            alerts.push_back("CRITICAL: High correlation between " + sector1Name +
                           " and " + sector2Name + " (" + std::to_string(correlation * 100) + "%)");
        }
    }

    return alerts;
}

} // namespace Risk
} // namespace CryptoClaude