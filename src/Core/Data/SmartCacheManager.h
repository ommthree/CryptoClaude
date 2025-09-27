#pragma once

#include "../Database/DatabaseManager.h"
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <memory>
#include <functional>
#include <mutex>

namespace CryptoClaude {
namespace Data {

using namespace CryptoClaude::Database;

// Cache entry metadata
struct CacheEntry {
    std::string key;
    std::string data;
    std::string dataType; // "price", "historical", "news", "sentiment"
    std::string provider;
    std::string symbol;
    std::chrono::system_clock::time_point cachedAt;
    std::chrono::system_clock::time_point expiresAt;
    std::chrono::system_clock::time_point lastAccessed;
    bool isPermanent; // Never expires (for historical data)
    int accessCount;
    double dataSizeKB;
    std::string checksum; // For data integrity validation

    CacheEntry() : accessCount(0), dataSizeKB(0.0), isPermanent(false) {}
};

// Cache statistics
struct CacheStats {
    int totalEntries;
    int permanentEntries;
    int expiredEntries;
    double totalSizeKB;
    double hitRate; // 0.0 to 1.0
    int totalRequests;
    int cacheHits;
    int cacheMisses;
    std::chrono::system_clock::time_point lastCleanup;

    CacheStats() : totalEntries(0), permanentEntries(0), expiredEntries(0),
                   totalSizeKB(0.0), hitRate(0.0), totalRequests(0),
                   cacheHits(0), cacheMisses(0) {}
};

// Cache configuration for different data types
struct CachePolicy {
    std::string dataType;
    std::chrono::minutes defaultTTL; // Time to live
    bool allowPermanent; // Can this data type be permanent?
    double maxSizeKB; // Maximum size per entry
    int maxEntries; // Maximum entries of this type
    bool enableDeduplication; // Enable intelligent deduplication
    bool compressData; // Compress data for storage

    CachePolicy() : defaultTTL(std::chrono::minutes(60)), allowPermanent(false),
                   maxSizeKB(1024.0), maxEntries(10000), enableDeduplication(true),
                   compressData(false) {}

    CachePolicy(const std::string& type, std::chrono::minutes ttl, bool permanent = false)
        : dataType(type), defaultTTL(ttl), allowPermanent(permanent),
          maxSizeKB(1024.0), maxEntries(10000), enableDeduplication(true),
          compressData(false) {}
};

// Smart cache manager with intelligent caching strategies
class SmartCacheManager {
public:
    explicit SmartCacheManager(DatabaseManager& dbManager);
    ~SmartCacheManager() = default;

    // Initialization
    bool initialize();
    void shutdown();

    // Core cache operations
    bool put(const std::string& key, const std::string& data,
             const std::string& dataType, const std::string& provider,
             const std::string& symbol = "", bool isPermanent = false,
             std::chrono::minutes customTTL = std::chrono::minutes(0));

    std::string get(const std::string& key);
    bool exists(const std::string& key);
    bool remove(const std::string& key);

    // Intelligent cache queries
    std::vector<std::string> findSimilarData(const std::string& dataType,
                                            const std::string& symbol,
                                            const std::string& provider = "");

    std::vector<CacheEntry> getEntriesForSymbol(const std::string& symbol);
    std::vector<CacheEntry> getEntriesForProvider(const std::string& provider);
    std::vector<CacheEntry> getEntriesByType(const std::string& dataType);

    // Historical data specialization
    bool putHistoricalData(const std::string& symbol, const std::string& provider,
                          const std::string& timeframe, const std::string& data,
                          const std::chrono::system_clock::time_point& startDate,
                          const std::chrono::system_clock::time_point& endDate);

    std::string getHistoricalData(const std::string& symbol, const std::string& provider,
                                 const std::string& timeframe,
                                 const std::chrono::system_clock::time_point& startDate,
                                 const std::chrono::system_clock::time_point& endDate);

    bool hasHistoricalData(const std::string& symbol, const std::string& provider,
                          const std::string& timeframe,
                          const std::chrono::system_clock::time_point& startDate,
                          const std::chrono::system_clock::time_point& endDate);

    // Cache-first data retrieval
    template<typename T>
    T getCachedOrFetch(const std::string& key,
                       std::function<T()> fetchFunction,
                       const std::string& dataType,
                       const std::string& provider,
                       const std::string& symbol = "",
                       bool isPermanent = false,
                       std::chrono::minutes customTTL = std::chrono::minutes(0));

    // Deduplication system
    std::string generateContentKey(const std::string& data);
    std::vector<std::string> findDuplicateData(const std::string& checksum);
    bool deduplicateCache();
    int removeDuplicates(const std::string& dataType = "");

    // Cache policies and configuration
    void setCachePolicy(const CachePolicy& policy);
    CachePolicy getCachePolicy(const std::string& dataType) const;
    std::vector<CachePolicy> getAllCachePolicies() const;

    // Cache maintenance
    void cleanupExpiredEntries();
    void cleanupLeastRecentlyUsed(int targetCount = -1);
    void optimizeCache();
    bool validateCacheIntegrity();

    // Statistics and monitoring
    CacheStats getCacheStats() const;
    std::map<std::string, CacheStats> getCacheStatsByType() const;
    std::vector<CacheEntry> getMostAccessedEntries(int limit = 10) const;
    std::vector<CacheEntry> getLargestEntries(int limit = 10) const;

    // Cache warming strategies
    struct WarmupStrategy {
        std::string name;
        std::vector<std::string> symbols;
        std::vector<std::string> dataTypes;
        std::vector<std::string> providers;
        std::chrono::hours warmupPeriod; // How much historical data to warm
        int priority; // 1-5, 1 being highest

        WarmupStrategy() : warmupPeriod(std::chrono::hours(24)), priority(3) {}
    };

    void addWarmupStrategy(const WarmupStrategy& strategy);
    void executeWarmupStrategies();
    bool warmupSymbolData(const std::string& symbol, const std::string& dataType,
                         std::chrono::hours period = std::chrono::hours(24));

    // Advanced cache operations
    struct CacheQueryOptions {
        std::string dataType;
        std::string provider;
        std::string symbol;
        std::chrono::system_clock::time_point fromDate;
        std::chrono::system_clock::time_point toDate;
        int maxResults;
        bool onlyPermanent;
        bool sortByAccessTime;

        CacheQueryOptions() : maxResults(1000), onlyPermanent(false), sortByAccessTime(false) {}
    };

    std::vector<CacheEntry> queryCache(const CacheQueryOptions& options) const;
    bool bulkDelete(const CacheQueryOptions& options);
    bool bulkUpdate(const CacheQueryOptions& options, const std::map<std::string, std::string>& updates);

    // Import/Export functionality
    struct CacheExportOptions {
        bool includePermanentData;
        bool includeExpiredData;
        std::vector<std::string> dataTypes;
        std::vector<std::string> symbols;
        bool compressExport;

        CacheExportOptions() : includePermanentData(true), includeExpiredData(false),
                              compressExport(true) {}
    };

    bool exportCache(const std::string& filePath, const CacheExportOptions& options = CacheExportOptions()) const;
    bool importCache(const std::string& filePath, bool mergeWithExisting = true);

    // Performance monitoring
    struct PerformanceMetrics {
        std::chrono::milliseconds averageGetTime;
        std::chrono::milliseconds averagePutTime;
        std::chrono::milliseconds averageQueryTime;
        double cacheEfficiency; // Hit rate weighted by access frequency
        double storageEfficiency; // Used space / total space

        PerformanceMetrics() : averageGetTime(0), averagePutTime(0), averageQueryTime(0),
                              cacheEfficiency(0.0), storageEfficiency(0.0) {}
    };

    PerformanceMetrics getPerformanceMetrics() const;
    void resetPerformanceMetrics();

private:
    DatabaseManager& dbManager_;
    std::map<std::string, CachePolicy> cachePolicies_;
    std::vector<WarmupStrategy> warmupStrategies_;

    // Thread safety
    mutable std::mutex cacheMutex_;

    // Performance tracking
    mutable std::vector<std::chrono::high_resolution_clock::time_point> getTimings_;
    mutable std::vector<std::chrono::high_resolution_clock::time_point> putTimings_;
    mutable CacheStats globalStats_;

    // Database operations
    bool initializeCacheTables();
    bool persistCacheEntry(const CacheEntry& entry);
    CacheEntry loadCacheEntry(const std::string& key) const;
    bool updateAccessStats(const std::string& key);
    bool deleteCacheEntry(const std::string& key);

    // Cache key generation
    std::string generateCacheKey(const std::string& dataType, const std::string& provider,
                               const std::string& symbol, const std::string& suffix = "") const;
    std::string generateHistoricalKey(const std::string& symbol, const std::string& provider,
                                    const std::string& timeframe,
                                    const std::chrono::system_clock::time_point& startDate,
                                    const std::chrono::system_clock::time_point& endDate) const;

    // Cache validation and integrity
    std::string calculateChecksum(const std::string& data) const;
    bool validateEntry(const CacheEntry& entry) const;
    bool isExpired(const CacheEntry& entry) const;

    // Cache policies enforcement
    bool enforcePolicy(const std::string& dataType, const CacheEntry& entry) const;
    void applyDefaultPolicies();
    bool shouldCompress(const std::string& dataType, double sizeKB) const;

    // Cleanup and optimization helpers
    std::vector<std::string> getExpiredKeys() const;
    std::vector<std::string> getLRUKeys(int count) const;
    bool compactDatabase();

    // Utility functions
    std::string compressData(const std::string& data) const;
    std::string decompressData(const std::string& compressedData) const;
    double calculateDataSizeKB(const std::string& data) const;
    std::string timePointToString(const std::chrono::system_clock::time_point& tp) const;
    std::chrono::system_clock::time_point stringToTimePoint(const std::string& str) const;
};

// Cache manager factory for different cache strategies
class CacheManagerFactory {
public:
    enum class Strategy {
        CONSERVATIVE,    // Minimal caching, frequent cleanup
        BALANCED,       // Balanced approach for personal use
        AGGRESSIVE,     // Maximum caching, minimal cleanup
        HISTORICAL_ONLY // Only cache permanent historical data
    };

    static std::unique_ptr<SmartCacheManager> create(DatabaseManager& dbManager, Strategy strategy);
    static std::vector<CachePolicy> getPoliciesForStrategy(Strategy strategy);

private:
    static std::vector<CachePolicy> getConservativePolicies();
    static std::vector<CachePolicy> getBalancedPolicies();
    static std::vector<CachePolicy> getAggressivePolicies();
    static std::vector<CachePolicy> getHistoricalOnlyPolicies();
};

} // namespace Data
} // namespace CryptoClaude