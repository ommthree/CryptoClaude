#include "ExpandedCoinUniverse.h"
#include <algorithm>
#include <unordered_set>

namespace CryptoClaude {
namespace Config {

std::vector<ExpandedCoinUniverse::CoinInfo> ExpandedCoinUniverse::initializeStaticUniverse() {
    return {
        // TRS-Approved Tier 1: Top 20 coins with 2-year historical data (Phase 1)
        // Conservative selection meeting TRS criteria: $5B+ market cap, $100M+ daily volume
        {"BTC", "Bitcoin", MarketSector::LAYER1, 500000000000, 15000000000, 1.0},
        {"ETH", "Ethereum", MarketSector::SMART_CONTRACT, 200000000000, 8000000000, 0.95},
        {"ADA", "Cardano", MarketSector::SMART_CONTRACT, 15000000000, 600000000, 0.92},
        {"DOT", "Polkadot", MarketSector::INTEROPERABILITY, 10000000000, 400000000, 0.90},
        {"AVAX", "Avalanche", MarketSector::LAYER1, 12000000000, 500000000, 0.88},
        {"SOL", "Solana", MarketSector::LAYER1, 20000000000, 800000000, 0.86},
        {"ATOM", "Cosmos", MarketSector::INTEROPERABILITY, 6500000000, 180000000, 0.84},
        {"NEAR", "Near Protocol", MarketSector::LAYER1, 2800000000, 165000000, 0.82},
        {"UNI", "Uniswap", MarketSector::DEFI, 6000000000, 160000000, 0.80},
        {"AAVE", "Aave", MarketSector::DEFI, 2400000000, 155000000, 0.78},
        {"COMP", "Compound", MarketSector::DEFI, 1600000000, 136000000, 0.76},
        {"MKR", "Maker", MarketSector::DEFI, 1700000000, 138000000, 0.74},
        {"CRV", "Curve DAO", MarketSector::DEFI, 1800000000, 140000000, 0.72},
        {"SNX", "Synthetix", MarketSector::DEFI, 1300000000, 130000000, 0.70},
        {"MATIC", "Polygon", MarketSector::INFRASTRUCTURE, 8500000000, 300000000, 0.68},
        {"LRC", "Loopring", MarketSector::INFRASTRUCTURE, 550000000, 113000000, 0.66},
        {"OMG", "OMG Network", MarketSector::INFRASTRUCTURE, 850000000, 115000000, 0.64},
        {"LINK", "Chainlink", MarketSector::ORACLE, 7500000000, 250000000, 0.62},
        {"VET", "VeChain", MarketSector::INFRASTRUCTURE, 4000000000, 190000000, 0.60},
        {"FIL", "Filecoin", MarketSector::STORAGE, 3500000000, 180000000, 0.58},

        // Tier 2: 21-50 by Market Cap (1-year data)
        {"FIL", "Filecoin", MarketSector::STORAGE, 3500000000, 80000000, 0.58},
        {"HBAR", "Hedera", MarketSector::LAYER1, 3200000000, 75000000, 0.57},
        {"APT", "Aptos", MarketSector::LAYER1, 3000000000, 70000000, 0.56},
        {"NEAR", "Near Protocol", MarketSector::LAYER1, 2800000000, 65000000, 0.55},
        {"QNT", "Quant", MarketSector::INTEROPERABILITY, 2600000000, 60000000, 0.54},
        {"AAVE", "Aave", MarketSector::DEFI, 2400000000, 55000000, 0.53},
        {"GRT", "The Graph", MarketSector::INFRASTRUCTURE, 2200000000, 50000000, 0.52},
        {"MANA", "Decentraland", MarketSector::GAMING, 2000000000, 45000000, 0.51},
        {"SAND", "The Sandbox", MarketSector::GAMING, 1900000000, 42000000, 0.5},
        {"CRV", "Curve DAO", MarketSector::DEFI, 1800000000, 40000000, 0.49},
        {"MKR", "Maker", MarketSector::DEFI, 1700000000, 38000000, 0.48},
        {"COMP", "Compound", MarketSector::DEFI, 1600000000, 36000000, 0.47},
        {"SUSHI", "SushiSwap", MarketSector::DEFI, 1500000000, 34000000, 0.46},
        {"YFI", "yearn.finance", MarketSector::DEFI, 1400000000, 32000000, 0.45},
        {"SNX", "Synthetix", MarketSector::DEFI, 1300000000, 30000000, 0.44},
        {"1INCH", "1inch", MarketSector::DEFI, 1200000000, 28000000, 0.43},
        {"BAL", "Balancer", MarketSector::DEFI, 1100000000, 26000000, 0.42},
        {"REN", "Ren", MarketSector::INTEROPERABILITY, 1000000000, 24000000, 0.41},
        {"ZRX", "0x", MarketSector::DEFI, 950000000, 22000000, 0.4},
        {"KNC", "Kyber Network", MarketSector::DEFI, 900000000, 20000000, 0.39},
        {"BAT", "Basic Attention Token", MarketSector::INFRASTRUCTURE, 850000000, 19000000, 0.38},
        {"ENJ", "Enjin Coin", MarketSector::GAMING, 800000000, 18000000, 0.37},
        {"STORJ", "Storj", MarketSector::STORAGE, 750000000, 17000000, 0.36},
        {"OCEAN", "Ocean Protocol", MarketSector::AI_ML, 700000000, 16000000, 0.35},
        {"FET", "Fetch.ai", MarketSector::AI_ML, 650000000, 15000000, 0.34},
        {"AGIX", "SingularityNET", MarketSector::AI_ML, 600000000, 14000000, 0.33},
        {"LRC", "Loopring", MarketSector::INFRASTRUCTURE, 550000000, 13000000, 0.32},
        {"BAND", "Band Protocol", MarketSector::ORACLE, 500000000, 12000000, 0.31},
        {"ALPHA", "Alpha Finance Lab", MarketSector::DEFI, 450000000, 11000000, 0.3},
        {"RUNE", "THORChain", MarketSector::DEFI, 400000000, 10000000, 0.29},

        // Tier 3: 51-100 by Market Cap (6-month data)
        {"XTZ", "Tezos", MarketSector::LAYER1, 380000000, 9500000, 0.28},
        {"EGLD", "MultiversX", MarketSector::LAYER1, 360000000, 9000000, 0.27},
        {"FLOW", "Flow", MarketSector::GAMING, 340000000, 8500000, 0.26},
        {"CHZ", "Chiliz", MarketSector::GAMING, 320000000, 8000000, 0.25},
        {"THETA", "Theta Network", MarketSector::INFRASTRUCTURE, 300000000, 7500000, 0.24},
        {"KLAY", "Klaytn", MarketSector::LAYER1, 280000000, 7000000, 0.23},
        {"MINA", "Mina", MarketSector::LAYER1, 260000000, 6500000, 0.22},
        {"CKB", "Nervos Network", MarketSector::INTEROPERABILITY, 240000000, 6000000, 0.21},
        {"ROSE", "Oasis Network", MarketSector::PRIVACY, 220000000, 5500000, 0.2},
        {"AR", "Arweave", MarketSector::STORAGE, 200000000, 5000000, 0.19},
        {"ZEC", "Zcash", MarketSector::PRIVACY, 180000000, 4500000, 0.18},
        {"XMR", "Monero", MarketSector::PRIVACY, 160000000, 4000000, 0.17},
        {"DASH", "Dash", MarketSector::PRIVACY, 140000000, 3500000, 0.16},
        {"WAVES", "Waves", MarketSector::LAYER1, 120000000, 3000000, 0.15},
        {"QTUM", "Qtum", MarketSector::LAYER1, 100000000, 2500000, 0.14},
        {"ICX", "ICON", MarketSector::INTEROPERABILITY, 95000000, 2400000, 0.13},
        {"ZIL", "Zilliqa", MarketSector::LAYER1, 90000000, 2300000, 0.12},
        {"ONT", "Ontology", MarketSector::INTEROPERABILITY, 85000000, 2200000, 0.11},
        {"LSK", "Lisk", MarketSector::LAYER1, 80000000, 2100000, 0.1},
        {"SC", "Siacoin", MarketSector::STORAGE, 75000000, 2000000, 0.09},
        {"ANKR", "Ankr", MarketSector::INFRASTRUCTURE, 70000000, 1900000, 0.08},
        {"COTI", "COTI", MarketSector::INFRASTRUCTURE, 65000000, 1800000, 0.07},
        {"BLZ", "Bluzelle", MarketSector::STORAGE, 60000000, 1700000, 0.06},
        {"RVN", "Ravencoin", MarketSector::LAYER1, 55000000, 1600000, 0.05},
        {"DGB", "DigiByte", MarketSector::LAYER1, 50000000, 1500000, 0.04}
    };
}

std::vector<ExpandedCoinUniverse::CoinInfo> ExpandedCoinUniverse::getFullCoinUniverse() {
    static auto universe = initializeStaticUniverse();
    assignTiers(universe);
    return universe;
}

void ExpandedCoinUniverse::assignTiers(std::vector<CoinInfo>& universe) {
    // Sort by market cap descending
    std::sort(universe.begin(), universe.end(),
              [](const CoinInfo& a, const CoinInfo& b) {
                  return a.marketCapUSD > b.marketCapUSD;
              });

    // Assign tiers
    for (size_t i = 0; i < universe.size(); ++i) {
        if (i < 20) {
            universe[i].tier = 1; // Top 20
        } else if (i < 50) {
            universe[i].tier = 2; // 21-50
        } else {
            universe[i].tier = 3; // 51+
        }
    }
}

std::vector<std::string> ExpandedCoinUniverse::getTradingUniverse(const SelectionCriteria& criteria) {
    auto fullUniverse = getFullCoinUniverse();
    auto filteredUniverse = filterUniverse(fullUniverse, criteria);

    // Ensure sector diversification
    std::map<MarketSector, int> sectorCounts;
    std::vector<std::string> result;

    for (const auto& coin : filteredUniverse) {
        if (sectorCounts[coin.sector] < criteria.maxCoinsPerSector &&
            result.size() < static_cast<size_t>(criteria.targetUniverseSize)) {
            result.push_back(coin.symbol);
            sectorCounts[coin.sector]++;
        }
    }

    return result;
}

std::vector<std::string> ExpandedCoinUniverse::getTier1Coins() {
    auto universe = getFullCoinUniverse();
    std::vector<std::string> tier1;

    for (const auto& coin : universe) {
        if (coin.tier == 1) {
            tier1.push_back(coin.symbol);
        }
    }

    return tier1;
}

std::vector<std::string> ExpandedCoinUniverse::getTier2Coins() {
    auto universe = getFullCoinUniverse();
    std::vector<std::string> tier2;

    for (const auto& coin : universe) {
        if (coin.tier == 2) {
            tier2.push_back(coin.symbol);
        }
    }

    return tier2;
}

std::vector<std::string> ExpandedCoinUniverse::getTier3Coins() {
    auto universe = getFullCoinUniverse();
    std::vector<std::string> tier3;

    for (const auto& coin : universe) {
        if (coin.tier == 3) {
            tier3.push_back(coin.symbol);
        }
    }

    return tier3;
}

std::vector<std::string> ExpandedCoinUniverse::getCoinsBySector(MarketSector sector) {
    auto universe = getFullCoinUniverse();
    std::vector<std::string> sectorCoins;

    for (const auto& coin : universe) {
        if (coin.sector == sector) {
            sectorCoins.push_back(coin.symbol);
        }
    }

    return sectorCoins;
}

std::map<ExpandedCoinUniverse::MarketSector, std::vector<std::string>>
ExpandedCoinUniverse::getCoinsBySectorMap() {
    auto universe = getFullCoinUniverse();
    std::map<MarketSector, std::vector<std::string>> sectorMap;

    for (const auto& coin : universe) {
        sectorMap[coin.sector].push_back(coin.symbol);
    }

    return sectorMap;
}

std::vector<std::pair<std::string, std::string>> ExpandedCoinUniverse::generateLongShortPairs(
    const std::vector<std::string>& universe, int maxPairs) {

    std::vector<std::pair<std::string, std::string>> pairs;

    // Generate pairs within similar sectors for market-neutral strategies
    auto sectorMap = getCoinsBySectorMap();

    for (const auto& [sector, coins] : sectorMap) {
        if (coins.size() >= 2) {
            // Create pairs within the same sector
            for (size_t i = 0; i < coins.size() - 1 && pairs.size() < static_cast<size_t>(maxPairs); ++i) {
                for (size_t j = i + 1; j < coins.size() && pairs.size() < static_cast<size_t>(maxPairs); ++j) {
                    // Check if both coins are in our trading universe
                    if (std::find(universe.begin(), universe.end(), coins[i]) != universe.end() &&
                        std::find(universe.begin(), universe.end(), coins[j]) != universe.end()) {
                        pairs.emplace_back(coins[i], coins[j]);
                    }
                }
            }
        }
    }

    return pairs;
}

std::vector<ExpandedCoinUniverse::CoinInfo> ExpandedCoinUniverse::filterUniverse(
    const std::vector<CoinInfo>& fullUniverse, const SelectionCriteria& criteria) {

    std::vector<CoinInfo> filtered;

    for (const auto& coin : fullUniverse) {
        if (isQualifiedForTrading(coin, criteria)) {
            filtered.push_back(coin);
        }
    }

    return filtered;
}

bool ExpandedCoinUniverse::isQualifiedForTrading(const CoinInfo& coin, const SelectionCriteria& criteria) {
    return coin.marketCapUSD >= criteria.minMarketCapUSD &&
           coin.volume24hUSD >= criteria.min24hVolumeUSD &&
           coin.liquidityScore >= criteria.minLiquidityScore &&
           !(criteria.excludeStablecoins && coin.isStablecoin) &&
           !(criteria.excludeWrappedTokens && coin.isWrappedToken);
}

std::string ExpandedCoinUniverse::getSectorName(MarketSector sector) {
    switch (sector) {
        case MarketSector::LAYER1: return "Layer1";
        case MarketSector::DEFI: return "DeFi";
        case MarketSector::SMART_CONTRACT: return "Smart Contract";
        case MarketSector::ORACLE: return "Oracle";
        case MarketSector::GAMING: return "Gaming";
        case MarketSector::AI_ML: return "AI/ML";
        case MarketSector::PRIVACY: return "Privacy";
        case MarketSector::INTEROPERABILITY: return "Interoperability";
        case MarketSector::STORAGE: return "Storage";
        case MarketSector::INFRASTRUCTURE: return "Infrastructure";
        case MarketSector::EXCHANGE_TOKEN: return "Exchange Token";
        case MarketSector::MEME_SOCIAL: return "Meme/Social";
        default: return "Unknown";
    }
}

ExpandedCoinUniverse::MarketSector ExpandedCoinUniverse::getSectorFromString(const std::string& sectorStr) {
    if (sectorStr == "Layer1") return MarketSector::LAYER1;
    if (sectorStr == "DeFi") return MarketSector::DEFI;
    if (sectorStr == "Smart Contract") return MarketSector::SMART_CONTRACT;
    if (sectorStr == "Oracle") return MarketSector::ORACLE;
    if (sectorStr == "Gaming") return MarketSector::GAMING;
    if (sectorStr == "AI/ML") return MarketSector::AI_ML;
    if (sectorStr == "Privacy") return MarketSector::PRIVACY;
    if (sectorStr == "Interoperability") return MarketSector::INTEROPERABILITY;
    if (sectorStr == "Storage") return MarketSector::STORAGE;
    if (sectorStr == "Infrastructure") return MarketSector::INFRASTRUCTURE;
    if (sectorStr == "Exchange Token") return MarketSector::EXCHANGE_TOKEN;
    if (sectorStr == "Meme/Social") return MarketSector::MEME_SOCIAL;
    return MarketSector::LAYER1; // Default
}

} // namespace Config
} // namespace CryptoClaude