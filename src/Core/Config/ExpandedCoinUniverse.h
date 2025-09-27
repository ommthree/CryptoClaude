#pragma once

#include <vector>
#include <string>
#include <map>
#include <set>

namespace CryptoClaude {
namespace Config {

/**
 * Expanded coin universe selection for proper diversification and pair formation
 * Based on market cap, liquidity, and sector representation
 */
class ExpandedCoinUniverse {
public:
    // Market sectors for diversification
    enum class MarketSector {
        LAYER1,           // Bitcoin, Ethereum, Solana, etc.
        DEFI,             // Uniswap, Aave, Compound, etc.
        SMART_CONTRACT,   // Ethereum, Cardano, Polkadot, etc.
        ORACLE,           // Chainlink, Band Protocol, etc.
        GAMING,           // Axie Infinity, Sandbox, etc.
        AI_ML,            // SingularityNET, Fetch.ai, etc.
        PRIVACY,          // Monero, Zcash, etc.
        INTEROPERABILITY, // Polkadot, Cosmos, etc.
        STORAGE,          // Filecoin, Arweave, etc.
        INFRASTRUCTURE,   // Polygon, Arbitrum, etc.
        EXCHANGE_TOKEN,   // BNB, CRO, FTT, etc.
        MEME_SOCIAL       // Dogecoin, Shiba Inu, etc.
    };

    // Coin metadata for universe selection
    struct CoinInfo {
        std::string symbol;
        std::string name;
        MarketSector sector;
        double marketCapUSD;
        double volume24hUSD;
        double liquidityScore;
        bool isStablecoin;
        bool isWrappedToken;
        int tier; // 1, 2, or 3

        CoinInfo(const std::string& sym, const std::string& nm, MarketSector sec,
                double mcap, double vol, double liq, bool stable = false, bool wrapped = false)
            : symbol(sym), name(nm), sector(sec), marketCapUSD(mcap), volume24hUSD(vol),
              liquidityScore(liq), isStablecoin(stable), isWrappedToken(wrapped), tier(3) {}
    };

    // Selection criteria for coin universe
    struct SelectionCriteria {
        double minMarketCapUSD = 100000000;      // $100M minimum
        double min24hVolumeUSD = 10000000;       // $10M daily volume
        double minLiquidityScore = 0.6;          // Liquidity threshold
        bool excludeStablecoins = true;          // No USDT, USDC, etc.
        bool excludeWrappedTokens = true;        // No WBTC, WETH variants
        int maxCoinsPerSector = 15;              // Sector diversification
        int targetUniverseSize = 75;             // Target 50-100 coins

        // Tier thresholds
        double tier1MarketCapThreshold = 10000000000; // $10B for Tier 1
        double tier2MarketCapThreshold = 1000000000;  // $1B for Tier 2
    };

    static std::vector<CoinInfo> getFullCoinUniverse();
    static std::vector<std::string> getTradingUniverse(const SelectionCriteria& criteria = SelectionCriteria());
    static std::vector<std::string> getTier1Coins();  // Top 20 by market cap
    static std::vector<std::string> getTier2Coins();  // 21-50 by market cap
    static std::vector<std::string> getTier3Coins();  // 51-100 by market cap

    // Sector-based selection
    static std::vector<std::string> getCoinsBySector(MarketSector sector);
    static std::map<MarketSector, std::vector<std::string>> getCoinsBySectorMap();

    // Dynamic universe updates (for production)
    static bool updateUniverseFromMarketData(); // Fetch latest market caps/volumes
    static std::vector<std::string> getRecommendedRemovals(); // Coins that no longer qualify
    static std::vector<std::string> getRecommendedAdditions(); // New coins that qualify

    // Pair formation helpers
    static std::vector<std::pair<std::string, std::string>> generateLongShortPairs(
        const std::vector<std::string>& universe, int maxPairs = 20);

    // Utility functions
    static std::string getSectorName(MarketSector sector);
    static MarketSector getSectorFromString(const std::string& sectorStr);
    static bool isQualifiedForTrading(const CoinInfo& coin, const SelectionCriteria& criteria);

private:
    // Static universe definition (can be updated from market data)
    static std::vector<CoinInfo> initializeStaticUniverse();
    static void assignTiers(std::vector<CoinInfo>& universe);
    static std::vector<CoinInfo> filterUniverse(const std::vector<CoinInfo>& fullUniverse,
                                               const SelectionCriteria& criteria);
};

} // namespace Config
} // namespace CryptoClaude