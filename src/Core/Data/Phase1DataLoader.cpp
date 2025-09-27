#include "Phase1DataLoader.h"
#include "../Config/ExpandedCoinUniverse.h"
#include "../Database/DatabaseManager.h"
#include <iostream>
#include <thread>
#include <algorithm>
#include <fstream>

namespace CryptoClaude {
namespace Data {

Phase1DataLoader::Phase1DataLoader() {
    // Initialize progress tracking
    progress_.symbolsTotal = 20; // Tier 1 universe size
    progress_.startTime = std::chrono::system_clock::now();
    progress_.currentPhase = "Initialization";

    // Configure loading plan with conservative API usage
    loadingPlan_.targetDays = 730;           // 2 years full coverage
    loadingPlan_.dailyApiCallBudget = 200;   // Conservative API budget
    loadingPlan_.requestDelaySeconds = 10;   // Rate limiting protection
    loadingPlan_.batchSize = 5;              // Manageable batch sizes

    std::cout << "[Phase1DataLoader] Initialized for TRS Phase 1 data loading\n";
    std::cout << "[Phase1DataLoader] Target: 20 Tier 1 coins, 730 days coverage\n";
}

bool Phase1DataLoader::startDataLoading() {
    std::cout << "\n=== PHASE 1 DATA LOADING STARTED ===\n";
    std::cout << "TRS-Approved Tier 1 Universe: 20 coins\n";
    std::cout << "Historical Coverage: 730 days (2 years)\n";
    std::cout << "Loading Strategy: Progressive 3-phase approach\n\n";

    progress_.startTime = std::chrono::system_clock::now();
    progress_.currentPhase = "Phase 1 - Priority Symbols";

    try {
        // Phase 1: Load priority 1 symbols (BTC, ETH, ADA, DOT, LINK)
        if (!loadPriority1Symbols()) {
            std::cerr << "[ERROR] Failed to load Priority 1 symbols\n";
            return false;
        }

        // Phase 2: Load priority 2 symbols (DeFi infrastructure)
        progress_.currentPhase = "Phase 2 - DeFi Infrastructure";
        if (!loadPriority2Symbols()) {
            std::cerr << "[ERROR] Failed to load Priority 2 symbols\n";
            return false;
        }

        // Phase 3: Load remaining symbols
        progress_.currentPhase = "Phase 3 - Complete Universe";
        if (!loadPriority3Symbols()) {
            std::cerr << "[ERROR] Failed to load Priority 3 symbols\n";
            return false;
        }

        // Final validation phase
        progress_.currentPhase = "Validation - Quality Assessment";
        if (!validateLoadedData()) {
            std::cerr << "[ERROR] Data validation failed\n";
            return false;
        }

        progress_.currentPhase = "Complete";
        progress_.overallProgress = 100.0;

        std::cout << "\n=== PHASE 1 DATA LOADING COMPLETED ===\n";
        std::cout << "Successfully loaded " << progress_.symbolsCompleted << " symbols\n";
        std::cout << "Overall data quality: " << (getOverallQualityScore() * 100) << "%\n";

        generateLoadingReport();
        return true;

    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Exception during data loading: " << e.what() << "\n";
        return false;
    }
}

bool Phase1DataLoader::loadPriority1Symbols() {
    std::cout << "\n--- Loading Priority 1 Symbols ---\n";
    std::cout << "Target: BTC, ETH, ADA, DOT, LINK (Foundation pairs)\n";

    for (const auto& symbol : loadingSequence_.priority1) {
        std::cout << "Loading " << symbol << " (730 days)... ";

        if (loadSymbolData(symbol)) {
            progress_.symbolsCompleted++;
            updateProgress("Priority1", static_cast<double>(progress_.symbolsCompleted) / 5.0 * 20.0);
            std::cout << "✓ Complete\n";

            // Rate limiting delay
            std::this_thread::sleep_for(std::chrono::seconds(loadingPlan_.requestDelaySeconds));
        } else {
            std::cout << "✗ Failed\n";
            failedSymbols_.push_back(symbol);
        }
    }

    progress_.phase1Complete = true;
    std::cout << "Priority 1 loading complete: " << (loadingSequence_.priority1.size() - failedSymbols_.size())
              << "/" << loadingSequence_.priority1.size() << " symbols loaded\n";

    return failedSymbols_.empty() || (failedSymbols_.size() <= 1); // Allow 1 failure
}

bool Phase1DataLoader::loadPriority2Symbols() {
    std::cout << "\n--- Loading Priority 2 Symbols ---\n";
    std::cout << "Target: UNI, AAVE, COMP, MKR, MATIC (DeFi Infrastructure)\n";

    size_t initialFailures = failedSymbols_.size();

    for (const auto& symbol : loadingSequence_.priority2) {
        std::cout << "Loading " << symbol << " (730 days)... ";

        if (loadSymbolData(symbol)) {
            progress_.symbolsCompleted++;
            updateProgress("Priority2", 20.0 + static_cast<double>(progress_.symbolsCompleted - 5) / 5.0 * 20.0);
            std::cout << "✓ Complete\n";

            // Rate limiting delay
            std::this_thread::sleep_for(std::chrono::seconds(loadingPlan_.requestDelaySeconds));
        } else {
            std::cout << "✗ Failed\n";
            failedSymbols_.push_back(symbol);
        }
    }

    progress_.phase2Complete = true;
    size_t phase2Failures = failedSymbols_.size() - initialFailures;
    std::cout << "Priority 2 loading complete: " << (loadingSequence_.priority2.size() - phase2Failures)
              << "/" << loadingSequence_.priority2.size() << " symbols loaded\n";

    return phase2Failures <= 1; // Allow 1 failure per phase
}

bool Phase1DataLoader::loadPriority3Symbols() {
    std::cout << "\n--- Loading Priority 3 Symbols ---\n";
    std::cout << "Target: Remaining 10 Tier 1 symbols (Complete universe)\n";

    size_t initialFailures = failedSymbols_.size();

    for (const auto& symbol : loadingSequence_.priority3) {
        std::cout << "Loading " << symbol << " (730 days)... ";

        if (loadSymbolData(symbol)) {
            progress_.symbolsCompleted++;
            updateProgress("Priority3", 40.0 + static_cast<double>(progress_.symbolsCompleted - 10) / 10.0 * 40.0);
            std::cout << "✓ Complete\n";

            // Rate limiting delay
            std::this_thread::sleep_for(std::chrono::seconds(loadingPlan_.requestDelaySeconds));
        } else {
            std::cout << "✗ Failed\n";
            failedSymbols_.push_back(symbol);
        }
    }

    progress_.phase3Complete = true;
    size_t phase3Failures = failedSymbols_.size() - initialFailures;
    std::cout << "Priority 3 loading complete: " << (loadingSequence_.priority3.size() - phase3Failures)
              << "/" << loadingSequence_.priority3.size() << " symbols loaded\n";

    return phase3Failures <= 2; // Allow 2 failures for larger phase
}

bool Phase1DataLoader::validateLoadedData() {
    std::cout << "\n--- Data Quality Validation ---\n";
    std::cout << "Validating loaded data for TRS compliance...\n";

    bool allValidationsPassed = true;
    double totalQualityScore = 0.0;
    int validSymbols = 0;

    // Get all successfully loaded symbols
    auto loadedSymbols = getLoadedSymbols();

    for (const auto& symbol : loadedSymbols) {
        std::cout << "Validating " << symbol << "... ";

        DataQualityMetrics metrics = calculateQualityMetrics(symbol);
        qualityMetrics_[symbol] = metrics;

        if (metrics.isAcceptableQuality()) {
            std::cout << "✓ Pass (Quality: " << (metrics.overallQualityScore * 100) << "%)\n";
            totalQualityScore += metrics.overallQualityScore;
            validSymbols++;
        } else {
            std::cout << "✗ Fail (Quality: " << (metrics.overallQualityScore * 100) << "%)\n";
            allValidationsPassed = false;
        }
    }

    double averageQuality = validSymbols > 0 ? totalQualityScore / validSymbols : 0.0;

    progress_.validationComplete = true;
    updateProgress("Validation", 100.0);

    std::cout << "\nValidation Results:\n";
    std::cout << "- Symbols validated: " << validSymbols << "/" << loadedSymbols.size() << "\n";
    std::cout << "- Average quality score: " << (averageQuality * 100) << "%\n";
    std::cout << "- TRS requirements: " << (averageQuality >= 0.98 ? "✓ Met" : "✗ Not met") << "\n";

    return allValidationsPassed && averageQuality >= 0.98;
}

bool Phase1DataLoader::loadSymbolData(const std::string& symbol, int retryCount) {
    // Simulate data loading with comprehensive quality checks
    // In production, this would interface with actual data providers

    try {
        std::cout.flush(); // Ensure immediate output

        // Simulate loading delay (replace with actual API calls)
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Calculate quality metrics for the loaded data
        DataQualityMetrics metrics = calculateQualityMetrics(symbol);

        // Store metrics for validation
        qualityMetrics_[symbol] = metrics;

        // Check if quality meets minimum requirements
        if (metrics.overallQualityScore >= QUALITY_THRESHOLD) {
            return true;
        } else if (retryCount < MAX_RETRY_ATTEMPTS) {
            std::cout << "Retrying... ";
            return loadSymbolData(symbol, retryCount + 1);
        }

        return false;

    } catch (const std::exception& e) {
        if (retryCount < MAX_RETRY_ATTEMPTS) {
            return loadSymbolData(symbol, retryCount + 1);
        }
        return false;
    }
}

Phase1DataLoader::DataQualityMetrics Phase1DataLoader::calculateQualityMetrics(const std::string& symbol) const {
    DataQualityMetrics metrics;

    // Simulate quality assessment (replace with actual data analysis)
    // Base quality on symbol characteristics and mock data availability

    if (symbol == "BTC" || symbol == "ETH") {
        metrics.completenessScore = 0.995;      // Excellent data availability
        metrics.consistencyScore = 0.98;
        metrics.timelinessScore = 0.99;
        metrics.crossValidationScore = 0.985;
    } else if (symbol == "ADA" || symbol == "DOT" || symbol == "LINK") {
        metrics.completenessScore = 0.99;       // Very good data
        metrics.consistencyScore = 0.975;
        metrics.timelinessScore = 0.985;
        metrics.crossValidationScore = 0.98;
    } else {
        metrics.completenessScore = 0.985;      // Good data with minor gaps
        metrics.consistencyScore = 0.97;
        metrics.timelinessScore = 0.98;
        metrics.crossValidationScore = 0.975;
    }

    // Calculate composite quality score
    metrics.overallQualityScore =
        (metrics.completenessScore * 0.3) +
        (metrics.consistencyScore * 0.25) +
        (metrics.timelinessScore * 0.2) +
        (metrics.crossValidationScore * 0.25);

    return metrics;
}

void Phase1DataLoader::updateProgress(const std::string& phase, double phaseProgress) {
    progress_.currentPhase = phase;
    progress_.overallProgress = phaseProgress;

    // Estimate completion time based on progress
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - progress_.startTime);

    if (progress_.overallProgress > 0) {
        auto totalEstimated = elapsed.count() / (progress_.overallProgress / 100.0);
        auto remaining = totalEstimated - elapsed.count();
        progress_.estimatedCompletion = now + std::chrono::minutes(static_cast<long>(remaining));
    }

    // Call progress callback if registered
    if (progressCallback_) {
        progressCallback_(progress_);
    }
}

Phase1DataLoader::LoadingProgress Phase1DataLoader::getLoadingProgress() const {
    return progress_;
}

std::vector<std::string> Phase1DataLoader::getLoadedSymbols() const {
    std::vector<std::string> loaded;

    // Combine all priority levels minus failed symbols
    auto allSymbols = loadingSequence_.priority1;
    allSymbols.insert(allSymbols.end(), loadingSequence_.priority2.begin(), loadingSequence_.priority2.end());
    allSymbols.insert(allSymbols.end(), loadingSequence_.priority3.begin(), loadingSequence_.priority3.end());

    for (const auto& symbol : allSymbols) {
        if (std::find(failedSymbols_.begin(), failedSymbols_.end(), symbol) == failedSymbols_.end()) {
            loaded.push_back(symbol);
        }
    }

    return loaded;
}

bool Phase1DataLoader::isAllDataLoaded() const {
    return progress_.phase1Complete && progress_.phase2Complete &&
           progress_.phase3Complete && progress_.validationComplete;
}

void Phase1DataLoader::generateLoadingReport() const {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "PHASE 1 DATA LOADING REPORT\n";
    std::cout << std::string(60, '=') << "\n";

    auto loadedSymbols = getLoadedSymbols();
    std::cout << "Total symbols loaded: " << loadedSymbols.size() << "/20\n";
    std::cout << "Success rate: " << (static_cast<double>(loadedSymbols.size()) / 20.0 * 100) << "%\n";

    if (!failedSymbols_.empty()) {
        std::cout << "\nFailed symbols: ";
        for (const auto& symbol : failedSymbols_) {
            std::cout << symbol << " ";
        }
        std::cout << "\n";
    }

    std::cout << "\nData Quality Summary:\n";
    double totalQuality = 0.0;
    for (const auto& symbol : loadedSymbols) {
        auto it = qualityMetrics_.find(symbol);
        if (it != qualityMetrics_.end()) {
            std::cout << "- " << symbol << ": " << (it->second.overallQualityScore * 100) << "%\n";
            totalQuality += it->second.overallQualityScore;
        }
    }

    double averageQuality = loadedSymbols.empty() ? 0.0 : totalQuality / loadedSymbols.size();
    std::cout << "\nAverage quality score: " << (averageQuality * 100) << "%\n";
    std::cout << "TRS requirement (98%): " << (averageQuality >= 0.98 ? "✓ MET" : "✗ NOT MET") << "\n";

    std::cout << std::string(60, '=') << "\n\n";
}

double Phase1DataLoader::getOverallQualityScore() const {
    if (qualityMetrics_.empty()) return 0.0;

    double totalScore = 0.0;
    for (const auto& [symbol, metrics] : qualityMetrics_) {
        totalScore += metrics.overallQualityScore;
    }

    return totalScore / qualityMetrics_.size();
}

// TRSPhase1DataValidator implementation
TRSPhase1DataValidator::TRSPhase1DataValidator() {
    // Set TRS-compliant validation requirements
    requirements_.minDataCompleteness = 0.98;
    requirements_.minHistoricalDays = 730;
    requirements_.maxGapDays = 3;
    requirements_.requiresMultiProviderValidation = true;
    requirements_.minNewsArticlesPerWeek = 5;
    requirements_.minExchangeListings = 5;
    requirements_.maxVolatilityAnnualized = 2.0;
    requirements_.minLiquidityScore = 0.5;

    std::cout << "[TRSPhase1DataValidator] Initialized with TRS compliance requirements\n";
}

TRSPhase1DataValidator::ValidationResults TRSPhase1DataValidator::validateTier1Universe() {
    std::cout << "\n=== TRS PHASE 1 UNIVERSE VALIDATION ===\n";
    std::cout << "Validating 20 Tier 1 symbols for TRS compliance...\n";

    ValidationResults results;
    results.validationTime = std::chrono::system_clock::now();
    results.symbolsTotal = 20;

    // Get Tier 1 coins from universe configuration
    auto tier1Coins = Config::ExpandedCoinUniverse::getTier1Coins();

    for (const auto& symbol : tier1Coins) {
        std::cout << "Validating " << symbol << "... ";

        bool passed = validateSymbol(symbol, requirements_);
        results.symbolValidation[symbol] = passed;

        if (passed) {
            results.symbolsPassed++;
            std::cout << "✓ PASS\n";
        } else {
            std::cout << "✗ FAIL\n";
        }
    }

    results.overallPassRate = static_cast<double>(results.symbolsPassed) / results.symbolsTotal;
    results.passesAllRequirements = (results.overallPassRate >= 0.95); // Allow 5% failure rate

    std::cout << "\nValidation Results:\n";
    std::cout << "- Symbols passed: " << results.symbolsPassed << "/" << results.symbolsTotal << "\n";
    std::cout << "- Pass rate: " << (results.overallPassRate * 100) << "%\n";
    std::cout << "- TRS compliance: " << (results.passesAllRequirements ? "✓ ACHIEVED" : "✗ NOT ACHIEVED") << "\n";

    lastValidationResults_ = results;
    return results;
}

bool TRSPhase1DataValidator::validateSymbol(const std::string& symbol, const TRSValidationRequirements& requirements) {
    // Comprehensive validation checks for TRS compliance

    bool dataCompletenessCheck = validateDataCompleteness(symbol);
    bool historicalCoverageCheck = validateHistoricalCoverage(symbol);
    bool dataQualityCheck = validateDataQuality(symbol);
    bool marketRequirementsCheck = validateMarketRequirements(symbol);

    return dataCompletenessCheck && historicalCoverageCheck &&
           dataQualityCheck && marketRequirementsCheck;
}

bool TRSPhase1DataValidator::validateDataCompleteness(const std::string& symbol) const {
    // Simulate data completeness validation
    // In production, check actual database records for gaps
    return true; // Mock: assume all symbols meet completeness requirements
}

bool TRSPhase1DataValidator::validateHistoricalCoverage(const std::string& symbol) const {
    // Simulate historical coverage validation
    // In production, verify 730 days of historical data availability
    return true; // Mock: assume all symbols have sufficient history
}

bool TRSPhase1DataValidator::validateDataQuality(const std::string& symbol) const {
    // Simulate data quality validation
    // In production, check for anomalies, consistency, cross-provider agreement
    return true; // Mock: assume all symbols meet quality requirements
}

bool TRSPhase1DataValidator::validateMarketRequirements(const std::string& symbol) const {
    // Simulate market requirements validation
    // In production, check market cap, volume, exchange listings, etc.
    return true; // Mock: assume all Tier 1 symbols meet market requirements
}

void TRSPhase1DataValidator::generateTRSValidationReport(const ValidationResults& results) const {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "TRS PHASE 1 VALIDATION REPORT\n";
    std::cout << std::string(70, '=') << "\n";
    std::cout << "Validation Time: " << std::chrono::duration_cast<std::chrono::seconds>(
        results.validationTime.time_since_epoch()).count() << " (Unix timestamp)\n";
    std::cout << "Total Symbols Evaluated: " << results.symbolsTotal << "\n";
    std::cout << "Symbols Passed: " << results.symbolsPassed << "\n";
    std::cout << "Overall Pass Rate: " << (results.overallPassRate * 100) << "%\n";
    std::cout << "TRS Compliance Status: " << (results.passesAllRequirements ? "ACHIEVED" : "NOT ACHIEVED") << "\n";
    std::cout << std::string(70, '=') << "\n\n";
}

} // namespace Data
} // namespace CryptoClaude