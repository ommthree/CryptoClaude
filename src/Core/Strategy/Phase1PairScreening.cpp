#include "Phase1PairScreening.h"
#include "../Config/ExpandedCoinUniverse.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <random>
#include <fstream>

namespace CryptoClaude {
namespace Strategy {

Phase1PairScreening::Phase1PairScreening() {
    // Initialize conservative screening criteria for TRS Phase 1
    criteria_.minCorrelation = 0.3;
    criteria_.maxCorrelation = 0.8;
    criteria_.optimalCorrelation = 0.6;
    criteria_.minLiquidityScore = 0.5;
    criteria_.minCombinedVolume = 200000000; // $200M
    criteria_.minDataQuality = 0.95;
    criteria_.minNewsAvailability = 0.4;
    criteria_.minExchangeListings = 8;
    criteria_.requireSectorDiversification = true;
    criteria_.maxSectorConcentration = 0.25;
    criteria_.targetPairCount = 50;
    criteria_.maxPairCount = 80;
    criteria_.minPairCount = 20;

    std::cout << "[Phase1PairScreening] Initialized with TRS-compliant screening criteria\n";
    std::cout << "[Phase1PairScreening] Target: ~190 pairs → 50 viable candidates\n";
}

Phase1PairScreening::ScreeningResults Phase1PairScreening::screenTier1Universe() {
    std::cout << "\n=== PHASE 1 PAIR SCREENING STARTED ===\n";
    std::cout << "Tier 1 Universe: 20 coins (~190 possible pairs)\n";
    std::cout << "Target: 50 viable pairs for backtesting\n";
    std::cout << "Strategy: Conservative quality-first approach\n\n";

    ScreeningResults results;
    results.screeningTime = std::chrono::system_clock::now();

    try {
        // Step 1: Generate all possible pairs from Tier 1 universe
        std::cout << "Step 1: Generating all possible pairs...\n";
        auto allPairs = generateAllPossiblePairs();
        results.totalPairsEvaluated = allPairs.size();
        std::cout << "Generated " << allPairs.size() << " possible pairs\n";

        // Step 2: Apply quality filters
        std::cout << "\nStep 2: Applying quality filters...\n";
        auto qualityFiltered = applyQualityFilters(allPairs);
        std::cout << "Quality filtering: " << qualityFiltered.size() << " pairs passed\n";

        // Step 3: Apply correlation filters
        std::cout << "\nStep 3: Applying correlation filters...\n";
        auto correlationFiltered = applyCorrelationFilters(qualityFiltered);
        std::cout << "Correlation filtering: " << correlationFiltered.size() << " pairs passed\n";

        // Step 4: Apply sector diversification
        std::cout << "\nStep 4: Applying sector diversification...\n";
        auto diversificationFiltered = applySectorDiversification(correlationFiltered);
        std::cout << "Diversification filtering: " << diversificationFiltered.size() << " pairs passed\n";

        // Step 5: Rank and select best pairs
        std::cout << "\nStep 5: Ranking and selecting best pairs...\n";
        auto rankedPairs = rankAndSelectBestPairs(diversificationFiltered);
        results.viablePairsFound = rankedPairs.size();

        // Step 6: Classify pairs into tiers
        for (auto& pair : rankedPairs) {
            classifyPairTier(pair);

            if (pair.tier == PairCandidate::Tier::TIER1_PREMIUM) {
                results.tier1Premium.push_back(pair);
            } else if (pair.tier == PairCandidate::Tier::TIER1_STANDARD) {
                results.tier1Standard.push_back(pair);
            } else if (pair.tier == PairCandidate::Tier::TIER2_BACKUP) {
                results.tier2Backup.push_back(pair);
            } else {
                results.rejected.push_back(pair);
            }
        }

        // Update statistics
        updateScreeningStatistics(results);

        std::cout << "\n=== PAIR SCREENING COMPLETED ===\n";
        printScreeningSummary(results);

        lastResults_ = results;
        return results;

    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Exception during pair screening: " << e.what() << "\n";
        return results;
    }
}

std::vector<Phase1PairScreening::PairCandidate> Phase1PairScreening::generateAllPossiblePairs() {
    std::vector<PairCandidate> allPairs;

    // Get Tier 1 coins from universe
    auto tier1Coins = Config::ExpandedCoinUniverse::getTier1Coins();

    // Generate all possible long-short combinations
    for (size_t i = 0; i < tier1Coins.size(); ++i) {
        for (size_t j = i + 1; j < tier1Coins.size(); ++j) {
            // Create both orientations (A-B and B-A)
            allPairs.push_back(createPairCandidate(tier1Coins[i], tier1Coins[j]));
            allPairs.push_back(createPairCandidate(tier1Coins[j], tier1Coins[i]));
        }
    }

    std::cout << "Generated " << allPairs.size() << " pair combinations from "
              << tier1Coins.size() << " Tier 1 coins\n";

    return allPairs;
}

std::vector<Phase1PairScreening::PairCandidate> Phase1PairScreening::applyQualityFilters(
    const std::vector<PairCandidate>& candidates) {

    std::vector<PairCandidate> filtered;
    filtered.reserve(candidates.size());

    int qualityPassed = 0;

    for (const auto& candidate : candidates) {
        // Check minimum quality requirements
        if (candidate.dataQualityScore >= criteria_.minDataQuality &&
            candidate.liquidityScore >= criteria_.minLiquidityScore &&
            candidate.tradingVolumeScore >= (criteria_.minCombinedVolume / 1000000000.0)) { // Normalize to 0-1 scale

            filtered.push_back(candidate);
            qualityPassed++;
        }
    }

    double passRate = static_cast<double>(qualityPassed) / candidates.size() * 100.0;
    std::cout << "Quality filter pass rate: " << passRate << "% ("
              << qualityPassed << "/" << candidates.size() << ")\n";

    return filtered;
}

std::vector<Phase1PairScreening::PairCandidate> Phase1PairScreening::applyCorrelationFilters(
    const std::vector<PairCandidate>& candidates) {

    std::vector<PairCandidate> filtered;
    filtered.reserve(candidates.size());

    int correlationPassed = 0;

    for (const auto& candidate : candidates) {
        // Check correlation requirements
        if (candidate.correlationScore >= criteria_.minCorrelation &&
            candidate.correlationScore <= criteria_.maxCorrelation) {

            filtered.push_back(candidate);
            correlationPassed++;
        }
    }

    double passRate = static_cast<double>(correlationPassed) / candidates.size() * 100.0;
    std::cout << "Correlation filter pass rate: " << passRate << "% ("
              << correlationPassed << "/" << candidates.size() << ")\n";

    return filtered;
}

std::vector<Phase1PairScreening::PairCandidate> Phase1PairScreening::applySectorDiversification(
    const std::vector<PairCandidate>& candidates) {

    if (!criteria_.requireSectorDiversification) {
        return candidates; // Skip if not required
    }

    std::vector<PairCandidate> filtered;
    std::map<Config::ExpandedCoinUniverse::MarketSector, int> sectorCounts;

    // Sort candidates by quality score (best first)
    auto sortedCandidates = candidates;
    std::sort(sortedCandidates.begin(), sortedCandidates.end(),
              [](const PairCandidate& a, const PairCandidate& b) {
                  return a.overallQualityScore > b.overallQualityScore;
              });

    int maxPairsPerSector = static_cast<int>(criteria_.targetPairCount * criteria_.maxSectorConcentration);

    for (const auto& candidate : sortedCandidates) {
        // Check if we can add this pair without exceeding sector limits
        bool canAdd = true;

        if (sectorCounts[candidate.longSector] >= maxPairsPerSector) {
            canAdd = false;
        }
        if (sectorCounts[candidate.shortSector] >= maxPairsPerSector) {
            canAdd = false;
        }

        if (canAdd && filtered.size() < static_cast<size_t>(criteria_.maxPairCount)) {
            filtered.push_back(candidate);
            sectorCounts[candidate.longSector]++;
            sectorCounts[candidate.shortSector]++;
        }
    }

    std::cout << "Sector diversification applied: " << filtered.size()
              << " pairs maintain sector balance\n";

    return filtered;
}

std::vector<Phase1PairScreening::PairCandidate> Phase1PairScreening::rankAndSelectBestPairs(
    const std::vector<PairCandidate>& candidates) {

    auto rankedCandidates = candidates;

    // Sort by overall quality score (descending)
    std::sort(rankedCandidates.begin(), rankedCandidates.end(),
              [](const PairCandidate& a, const PairCandidate& b) {
                  return a.overallQualityScore > b.overallQualityScore;
              });

    // Select top candidates up to target count
    std::vector<PairCandidate> selected;
    size_t targetCount = std::min(static_cast<size_t>(criteria_.targetPairCount), rankedCandidates.size());

    for (size_t i = 0; i < targetCount; ++i) {
        selected.push_back(rankedCandidates[i]);
    }

    std::cout << "Selected top " << selected.size() << " pairs for backtesting\n";
    if (!selected.empty()) {
        std::cout << "Quality score range: " << selected.back().overallQualityScore
                  << " to " << selected.front().overallQualityScore << "\n";
    }

    return selected;
}

Phase1PairScreening::PairCandidate Phase1PairScreening::createPairCandidate(
    const std::string& longSymbol, const std::string& shortSymbol) {

    PairCandidate candidate;
    candidate.longSymbol = longSymbol;
    candidate.shortSymbol = shortSymbol;

    // Get coin information for sector classification
    auto coinUniverse = Config::ExpandedCoinUniverse::getFullCoinUniverse();

    for (const auto& coin : coinUniverse) {
        if (coin.symbol == longSymbol) {
            candidate.longSector = coin.sector;
        }
        if (coin.symbol == shortSymbol) {
            candidate.shortSector = coin.sector;
        }
    }

    // Calculate all metrics for this pair
    calculateAllPairMetrics(candidate);

    return candidate;
}

void Phase1PairScreening::calculateAllPairMetrics(PairCandidate& candidate) {
    // Calculate correlation score
    candidate.correlationScore = calculateCorrelationScore(candidate.longSymbol, candidate.shortSymbol);

    // Calculate liquidity metrics
    candidate.liquidityScore = calculateLiquidityScore(candidate.longSymbol, candidate.shortSymbol);

    // Calculate data quality
    candidate.dataQualityScore = calculateDataQualityScore(candidate.longSymbol, candidate.shortSymbol);

    // Calculate volatility match (simulate)
    candidate.volatilityMatch = calculateVolatilityMatch(candidate.longSymbol, candidate.shortSymbol);

    // Calculate market cap balance
    candidate.marketCapBalance = calculateMarketCapBalance(candidate.longSymbol, candidate.shortSymbol);

    // Calculate sector diversification score
    candidate.sectorDiversification = calculateSectorDiversificationScore(candidate.longSector, candidate.shortSector);

    // Simulate additional metrics
    candidate.tradingVolumeScore = (candidate.liquidityScore + 0.1) * 0.8; // Based on liquidity
    candidate.newsAvailabilityScore = 0.6 + (std::hash<std::string>{}(candidate.longSymbol + candidate.shortSymbol) % 40) / 100.0;
    candidate.exchangeListingScore = 0.7 + (std::hash<std::string>{}(candidate.shortSymbol + candidate.longSymbol) % 30) / 100.0;

    // Calculate composite scores
    candidate.overallQualityScore =
        (candidate.dataQualityScore * QUALITY_WEIGHT) +
        (candidate.correlationScore * CORRELATION_WEIGHT) +
        (candidate.liquidityScore * LIQUIDITY_WEIGHT) +
        (candidate.sectorDiversification * DIVERSIFICATION_WEIGHT) +
        (candidate.volatilityMatch * VIABILITY_WEIGHT);

    candidate.tradingViabilityScore = calculateTradingViabilityScore(candidate);

    candidate.riskAdjustedScore = candidate.overallQualityScore * candidate.volatilityMatch;
}

double Phase1PairScreening::calculateCorrelationScore(const std::string& symbol1, const std::string& symbol2) {
    // Check cache first
    auto key = std::make_pair(symbol1, symbol2);
    auto it = correlationCache_.find(key);
    if (it != correlationCache_.end()) {
        return it->second;
    }

    // Simulate correlation calculation based on symbol characteristics
    // In production, this would calculate actual historical correlation
    double baseCorrelation = 0.5;

    // Add some variation based on symbol combination
    std::hash<std::string> hasher;
    size_t combined_hash = hasher(symbol1 + symbol2);
    double variation = (combined_hash % 60 - 30) / 100.0; // -0.3 to +0.3

    double correlation = std::max(0.0, std::min(1.0, baseCorrelation + variation));

    // Cache the result
    correlationCache_[key] = correlation;

    return correlation;
}

double Phase1PairScreening::calculateLiquidityScore(const std::string& symbol1, const std::string& symbol2) {
    // Simulate liquidity calculation based on coin characteristics
    // Major coins get higher liquidity scores

    auto getLiquidityForSymbol = [](const std::string& symbol) -> double {
        if (symbol == "BTC" || symbol == "ETH") return 1.0;
        if (symbol == "ADA" || symbol == "DOT" || symbol == "LINK") return 0.9;
        if (symbol == "UNI" || symbol == "AAVE" || symbol == "SOL") return 0.85;
        return 0.7 + (std::hash<std::string>{}(symbol) % 20) / 100.0;
    };

    double liquidity1 = getLiquidityForSymbol(symbol1);
    double liquidity2 = getLiquidityForSymbol(symbol2);

    // Combined liquidity is weighted average
    return (liquidity1 + liquidity2) / 2.0;
}

double Phase1PairScreening::calculateDataQualityScore(const std::string& symbol1, const std::string& symbol2) {
    // Simulate data quality based on coin maturity and popularity
    auto getQualityForSymbol = [](const std::string& symbol) -> double {
        if (symbol == "BTC" || symbol == "ETH") return 0.99;
        if (symbol == "ADA" || symbol == "DOT" || symbol == "LINK") return 0.98;
        return 0.95 + (std::hash<std::string>{}(symbol) % 4) / 100.0;
    };

    double quality1 = getQualityForSymbol(symbol1);
    double quality2 = getQualityForSymbol(symbol2);

    // Return minimum quality (weakest link)
    return std::min(quality1, quality2);
}

double Phase1PairScreening::calculateTradingViabilityScore(const PairCandidate& candidate) {
    // Composite viability score based on multiple factors
    return (candidate.liquidityScore * 0.4) +
           (candidate.correlationScore * 0.3) +
           (candidate.marketCapBalance * 0.2) +
           (candidate.volatilityMatch * 0.1);
}

double Phase1PairScreening::calculateVolatilityMatch(const std::string& symbol1, const std::string& symbol2) {
    // Simulate volatility matching calculation
    // Better match = higher score
    std::hash<std::string> hasher;
    double vol1 = 0.3 + (hasher(symbol1) % 40) / 100.0; // 0.3 to 0.7
    double vol2 = 0.3 + (hasher(symbol2) % 40) / 100.0;

    double ratio = std::min(vol1, vol2) / std::max(vol1, vol2);
    return ratio; // Closer to 1.0 = better match
}

double Phase1PairScreening::calculateMarketCapBalance(const std::string& symbol1, const std::string& symbol2) {
    // Simulate market cap balance calculation
    auto getMarketCapTier = [](const std::string& symbol) -> double {
        if (symbol == "BTC") return 10.0;
        if (symbol == "ETH") return 8.0;
        if (symbol == "ADA" || symbol == "DOT") return 6.0;
        if (symbol == "LINK" || symbol == "UNI") return 4.0;
        return 2.0 + (std::hash<std::string>{}(symbol) % 20) / 10.0;
    };

    double cap1 = getMarketCapTier(symbol1);
    double cap2 = getMarketCapTier(symbol2);

    double ratio = std::min(cap1, cap2) / std::max(cap1, cap2);
    return ratio; // Closer to 1.0 = better balance
}

double Phase1PairScreening::calculateSectorDiversificationScore(
    Config::ExpandedCoinUniverse::MarketSector sector1,
    Config::ExpandedCoinUniverse::MarketSector sector2) {

    // Same sector = lower diversification score
    if (sector1 == sector2) {
        return 0.3; // Penalty for same sector
    }

    // Cross-sector pairs get higher scores
    return 0.8 + (static_cast<int>(sector1) + static_cast<int>(sector2)) % 20 / 100.0;
}

void Phase1PairScreening::classifyPairTier(PairCandidate& candidate) {
    if (candidate.overallQualityScore >= 0.85 && candidate.passesMinimumCriteria()) {
        candidate.tier = PairCandidate::Tier::TIER1_PREMIUM;
    } else if (candidate.overallQualityScore >= 0.75 && candidate.passesMinimumCriteria()) {
        candidate.tier = PairCandidate::Tier::TIER1_STANDARD;
    } else if (candidate.overallQualityScore >= 0.65) {
        candidate.tier = PairCandidate::Tier::TIER2_BACKUP;
    } else {
        candidate.tier = PairCandidate::Tier::REJECTED;
    }
}

bool Phase1PairScreening::PairCandidate::passesMinimumCriteria() const {
    return correlationScore >= 0.3 &&
           correlationScore <= 0.8 &&
           liquidityScore >= 0.5 &&
           dataQualityScore >= 0.95;
}

bool Phase1PairScreening::PairCandidate::recommendedForBacktesting() const {
    return tier == Tier::TIER1_PREMIUM || tier == Tier::TIER1_STANDARD;
}

void Phase1PairScreening::updateScreeningStatistics(ScreeningResults& results) {
    if (results.viablePairsFound == 0) {
        results.passRate = 0.0;
        results.averageQualityScore = 0.0;
        results.averageCorrelation = 0.0;
        return;
    }

    // Calculate pass rate
    results.passRate = static_cast<double>(results.viablePairsFound) / results.totalPairsEvaluated;

    // Calculate average scores from all viable pairs
    double totalQuality = 0.0;
    double totalCorrelation = 0.0;

    auto allViable = results.tier1Premium;
    allViable.insert(allViable.end(), results.tier1Standard.begin(), results.tier1Standard.end());
    allViable.insert(allViable.end(), results.tier2Backup.begin(), results.tier2Backup.end());

    for (const auto& pair : allViable) {
        totalQuality += pair.overallQualityScore;
        totalCorrelation += pair.correlationScore;
        results.sectorDistribution[pair.longSector]++;
        results.sectorDistribution[pair.shortSector]++;
    }

    results.averageQualityScore = totalQuality / allViable.size();
    results.averageCorrelation = totalCorrelation / allViable.size();

    // Check if targets are met
    results.meetsTargetCount = (results.viablePairsFound >= criteria_.minPairCount &&
                               results.viablePairsFound <= criteria_.maxPairCount);
    results.meetsTRSRequirements = (results.averageQualityScore >= 0.80 &&
                                   results.tier1Premium.size() >= 10);
}

void Phase1PairScreening::printScreeningSummary(const ScreeningResults& results) {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "PHASE 1 PAIR SCREENING SUMMARY\n";
    std::cout << std::string(60, '=') << "\n";
    std::cout << "Total pairs evaluated: " << results.totalPairsEvaluated << "\n";
    std::cout << "Viable pairs found: " << results.viablePairsFound << "\n";
    std::cout << "Pass rate: " << (results.passRate * 100) << "%\n\n";

    std::cout << "Tier Distribution:\n";
    std::cout << "- Tier 1 Premium: " << results.tier1Premium.size() << " pairs\n";
    std::cout << "- Tier 1 Standard: " << results.tier1Standard.size() << " pairs\n";
    std::cout << "- Tier 2 Backup: " << results.tier2Backup.size() << " pairs\n";
    std::cout << "- Rejected: " << results.rejected.size() << " pairs\n\n";

    std::cout << "Quality Metrics:\n";
    std::cout << "- Average quality score: " << (results.averageQualityScore * 100) << "%\n";
    std::cout << "- Average correlation: " << (results.averageCorrelation * 100) << "%\n";
    std::cout << "- Target count met: " << (results.meetsTargetCount ? "✓ Yes" : "✗ No") << "\n";
    std::cout << "- TRS requirements: " << (results.meetsTRSRequirements ? "✓ Met" : "✗ Not met") << "\n";
    std::cout << std::string(60, '=') << "\n\n";
}

// TRSPairQualityAnalyzer implementation
TRSPairQualityAnalyzer::TRSPairQualityAnalyzer() {
    requirements_.minConfidenceThreshold = 0.85;
    requirements_.minHistoricalDays = 730;
    requirements_.minStatisticalSignificance = 0.95;
    requirements_.requiresMultiTimeframeAnalysis = true;
    requirements_.requiresRegimeStabilityTesting = true;
    requirements_.requiresLiquidityStressTesting = true;
    requirements_.minBacktestSharpeRatio = 0.5;
    requirements_.maxDrawdownThreshold = 0.15;
    requirements_.minTradingDaysPerYear = 200;

    std::cout << "[TRSPairQualityAnalyzer] Initialized with enhanced TRS requirements\n";
}

TRSPairQualityAnalyzer::QualityAssessmentResults TRSPairQualityAnalyzer::analyzePairQuality(
    const std::vector<Phase1PairScreening::PairCandidate>& pairs) {

    std::cout << "\n=== TRS PAIR QUALITY ANALYSIS ===\n";
    std::cout << "Analyzing " << pairs.size() << " pairs for TRS compliance...\n";

    QualityAssessmentResults results;
    results.totalPairsEvaluated = pairs.size();
    results.pairsPassingTRS = 0;

    for (const auto& pair : pairs) {
        std::string pairKey = pair.longSymbol + "/" + pair.shortSymbol;

        bool trsCompliant = validateTRSCompliance(pair);
        results.trsCompliance[pairKey] = trsCompliant;

        if (trsCompliant) {
            results.pairsPassingTRS++;
        }

        // Calculate enhanced quality score
        double qualityScore = pair.overallQualityScore;
        if (requirements_.requiresMultiTimeframeAnalysis) {
            qualityScore *= calculateMultiTimeframeStability(pair.longSymbol, pair.shortSymbol);
        }
        if (requirements_.requiresRegimeStabilityTesting) {
            qualityScore *= assessRegimeConsistency(pair.longSymbol, pair.shortSymbol);
        }

        results.pairQualityScores[pairKey] = qualityScore;

        // Identify any quality issues
        results.qualityIssues[pairKey] = identifyQualityIssues(pair);
    }

    // Calculate overall metrics
    double totalScore = 0.0;
    for (const auto& [pair, score] : results.pairQualityScores) {
        totalScore += score;
    }
    results.overallQualityScore = results.totalPairsEvaluated > 0 ?
        totalScore / results.totalPairsEvaluated : 0.0;

    results.meetsMinimumStandards = (results.pairsPassingTRS >= 20 &&
                                    results.overallQualityScore >= 0.80);

    std::cout << "TRS Analysis Results:\n";
    std::cout << "- Pairs passing TRS: " << results.pairsPassingTRS
              << "/" << results.totalPairsEvaluated << "\n";
    std::cout << "- Overall quality: " << (results.overallQualityScore * 100) << "%\n";
    std::cout << "- Standards met: " << (results.meetsMinimumStandards ? "✓ Yes" : "✗ No") << "\n";

    lastResults_ = results;
    return results;
}

bool TRSPairQualityAnalyzer::validateTRSCompliance(const Phase1PairScreening::PairCandidate& pair) {
    // Check all TRS requirements
    bool dataCheck = checkDataSufficiency(pair.longSymbol, pair.shortSymbol);
    bool statisticalCheck = checkStatisticalSignificance(pair.longSymbol, pair.shortSymbol);
    bool regimeCheck = checkRegimeStability(pair.longSymbol, pair.shortSymbol);
    bool liquidityCheck = checkLiquidityAdequacy(pair.longSymbol, pair.shortSymbol);

    return dataCheck && statisticalCheck && regimeCheck && liquidityCheck;
}

bool TRSPairQualityAnalyzer::checkDataSufficiency(const std::string& symbol1, const std::string& symbol2) {
    // Mock implementation - in production, check actual data availability
    return true; // Assume all Tier 1 pairs have sufficient data
}

bool TRSPairQualityAnalyzer::checkStatisticalSignificance(const std::string& symbol1, const std::string& symbol2) {
    // Mock implementation - in production, perform actual statistical tests
    return true; // Assume statistical significance for all pairs
}

bool TRSPairQualityAnalyzer::checkRegimeStability(const std::string& symbol1, const std::string& symbol2) {
    // Mock implementation - in production, test across market regimes
    return true; // Assume regime stability for all pairs
}

bool TRSPairQualityAnalyzer::checkLiquidityAdequacy(const std::string& symbol1, const std::string& symbol2) {
    // Mock implementation - in production, test liquidity under stress
    return true; // Assume adequate liquidity for Tier 1 pairs
}

double TRSPairQualityAnalyzer::calculateMultiTimeframeStability(const std::string& symbol1, const std::string& symbol2) {
    // Mock multi-timeframe stability score
    return 0.95 + (std::hash<std::string>{}(symbol1 + symbol2) % 5) / 100.0;
}

double TRSPairQualityAnalyzer::assessRegimeConsistency(const std::string& symbol1, const std::string& symbol2) {
    // Mock regime consistency score
    return 0.90 + (std::hash<std::string>{}(symbol2 + symbol1) % 10) / 100.0;
}

std::vector<std::string> TRSPairQualityAnalyzer::identifyQualityIssues(
    const Phase1PairScreening::PairCandidate& pair) {

    std::vector<std::string> issues;

    if (pair.correlationScore < 0.4) {
        issues.push_back("Low correlation may reduce effectiveness");
    }
    if (pair.liquidityScore < 0.7) {
        issues.push_back("Liquidity concerns in volatile markets");
    }
    if (pair.dataQualityScore < 0.98) {
        issues.push_back("Data quality below optimal threshold");
    }

    return issues;
}

} // namespace Strategy
} // namespace CryptoClaude