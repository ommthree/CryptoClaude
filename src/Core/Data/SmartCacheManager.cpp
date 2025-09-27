#include "SmartCacheManager.h"
#include <sqlite3.h>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <fstream>
#include <zlib.h>
#include <openssl/sha.h>

namespace CryptoClaude {
namespace Data {

// SmartCacheManager Implementation
SmartCacheManager::SmartCacheManager(DatabaseManager& dbManager)
    : dbManager_(dbManager) {
    // Initialize with default policies
    applyDefaultPolicies();
}

bool SmartCacheManager::initialize() {
    std::lock_guard<std::mutex> lock(cacheMutex_);

    if (!initializeCacheTables()) {
        std::cerr << "Failed to initialize cache tables" << std::endl;
        return false;
    }

    // Initial cleanup of expired entries
    cleanupExpiredEntries();

    std::cout << "SmartCacheManager initialized successfully" << std::endl;
    return true;
}

void SmartCacheManager::shutdown() {
    std::lock_guard<std::mutex> lock(cacheMutex_);

    // Final cleanup and optimization
    cleanupExpiredEntries();
    optimizeCache();

    std::cout << "SmartCacheManager shutdown completed" << std::endl;
}

bool SmartCacheManager::put(const std::string& key, const std::string& data,
                           const std::string& dataType, const std::string& provider,
                           const std::string& symbol, bool isPermanent,
                           std::chrono::minutes customTTL) {
    std::lock_guard<std::mutex> lock(cacheMutex_);

    auto start = std::chrono::high_resolution_clock::now();

    CacheEntry entry;
    entry.key = key;
    entry.data = data;
    entry.dataType = dataType;
    entry.provider = provider;
    entry.symbol = symbol;
    entry.cachedAt = std::chrono::system_clock::now();
    entry.lastAccessed = entry.cachedAt;
    entry.isPermanent = isPermanent;
    entry.accessCount = 0;
    entry.dataSizeKB = calculateDataSizeKB(data);
    entry.checksum = calculateChecksum(data);

    // Determine TTL
    auto policy = getCachePolicy(dataType);
    auto ttl = (customTTL.count() > 0) ? customTTL : policy.defaultTTL;

    if (isPermanent && policy.allowPermanent) {
        entry.expiresAt = std::chrono::system_clock::time_point::max(); // Never expires
    } else {
        entry.expiresAt = entry.cachedAt + ttl;
    }

    // Enforce cache policy
    if (!enforcePolicy(dataType, entry)) {
        std::cerr << "Cache entry violates policy for type: " << dataType << std::endl;
        return false;
    }

    // Compress data if policy requires it
    if (shouldCompress(dataType, entry.dataSizeKB)) {
        entry.data = compressData(entry.data);
    }

    bool success = persistCacheEntry(entry);

    auto end = std::chrono::high_resolution_clock::now();
    putTimings_.push_back(end - start);

    // Update global stats
    if (success) {
        globalStats_.totalEntries++;
        globalStats_.totalSizeKB += entry.dataSizeKB;
        if (isPermanent) {
            globalStats_.permanentEntries++;
        }
    }

    return success;
}

std::string SmartCacheManager::get(const std::string& key) {
    std::lock_guard<std::mutex> lock(cacheMutex_);

    auto start = std::chrono::high_resolution_clock::now();

    globalStats_.totalRequests++;

    CacheEntry entry = loadCacheEntry(key);
    if (entry.key.empty()) {
        globalStats_.cacheMisses++;
        return "";
    }

    // Check if expired (unless permanent)
    if (!entry.isPermanent && isExpired(entry)) {
        deleteCacheEntry(key);
        globalStats_.cacheMisses++;
        globalStats_.expiredEntries++;
        return "";
    }

    // Update access statistics
    updateAccessStats(key);

    globalStats_.cacheHits++;
    globalStats_.hitRate = (double)globalStats_.cacheHits / globalStats_.totalRequests;

    auto end = std::chrono::high_resolution_clock::now();
    getTimings_.push_back(end - start);

    // Decompress if needed
    auto policy = getCachePolicy(entry.dataType);
    if (policy.compressData) {
        return decompressData(entry.data);
    }

    return entry.data;
}

bool SmartCacheManager::exists(const std::string& key) {
    std::lock_guard<std::mutex> lock(cacheMutex_);

    CacheEntry entry = loadCacheEntry(key);
    if (entry.key.empty()) {
        return false;
    }

    // Check if expired
    if (!entry.isPermanent && isExpired(entry)) {
        deleteCacheEntry(key);
        return false;
    }

    return true;
}

bool SmartCacheManager::remove(const std::string& key) {
    std::lock_guard<std::mutex> lock(cacheMutex_);

    CacheEntry entry = loadCacheEntry(key);
    if (entry.key.empty()) {
        return false;
    }

    bool success = deleteCacheEntry(key);

    if (success) {
        globalStats_.totalEntries = std::max(0, globalStats_.totalEntries - 1);
        globalStats_.totalSizeKB = std::max(0.0, globalStats_.totalSizeKB - entry.dataSizeKB);
        if (entry.isPermanent) {
            globalStats_.permanentEntries = std::max(0, globalStats_.permanentEntries - 1);
        }
    }

    return success;
}

bool SmartCacheManager::putHistoricalData(const std::string& symbol, const std::string& provider,
                                         const std::string& timeframe, const std::string& data,
                                         const std::chrono::system_clock::time_point& startDate,
                                         const std::chrono::system_clock::time_point& endDate) {
    std::string key = generateHistoricalKey(symbol, provider, timeframe, startDate, endDate);
    return put(key, data, "historical", provider, symbol, true); // Historical data is permanent
}

std::string SmartCacheManager::getHistoricalData(const std::string& symbol, const std::string& provider,
                                                const std::string& timeframe,
                                                const std::chrono::system_clock::time_point& startDate,
                                                const std::chrono::system_clock::time_point& endDate) {
    std::string key = generateHistoricalKey(symbol, provider, timeframe, startDate, endDate);
    return get(key);
}

bool SmartCacheManager::hasHistoricalData(const std::string& symbol, const std::string& provider,
                                         const std::string& timeframe,
                                         const std::chrono::system_clock::time_point& startDate,
                                         const std::chrono::system_clock::time_point& endDate) {
    std::string key = generateHistoricalKey(symbol, provider, timeframe, startDate, endDate);
    return exists(key);
}

std::string SmartCacheManager::generateContentKey(const std::string& data) {
    return calculateChecksum(data);
}

std::vector<std::string> SmartCacheManager::findDuplicateData(const std::string& checksum) {
    std::vector<std::string> duplicates;

    auto db = dbManager_.getDatabase();
    if (!db) return duplicates;

    const char* sql = R"(
        SELECT cache_key FROM cache_entries
        WHERE checksum = ? AND cache_key != ?
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, checksum.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, "", -1, SQLITE_STATIC); // Empty key to find all

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* key = (const char*)sqlite3_column_text(stmt, 0);
            if (key) {
                duplicates.push_back(key);
            }
        }
    }

    sqlite3_finalize(stmt);
    return duplicates;
}

void SmartCacheManager::cleanupExpiredEntries() {
    std::lock_guard<std::mutex> lock(cacheMutex_);

    auto expiredKeys = getExpiredKeys();
    int cleanedCount = 0;

    for (const auto& key : expiredKeys) {
        if (deleteCacheEntry(key)) {
            cleanedCount++;
        }
    }

    globalStats_.expiredEntries += cleanedCount;
    globalStats_.totalEntries = std::max(0, globalStats_.totalEntries - cleanedCount);
    globalStats_.lastCleanup = std::chrono::system_clock::now();

    if (cleanedCount > 0) {
        std::cout << "Cleaned up " << cleanedCount << " expired cache entries" << std::endl;
    }
}

CacheStats SmartCacheManager::getCacheStats() const {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    return globalStats_;
}

// Cache policies
void SmartCacheManager::setCachePolicy(const CachePolicy& policy) {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    cachePolicies_[policy.dataType] = policy;
}

CachePolicy SmartCacheManager::getCachePolicy(const std::string& dataType) const {
    auto it = cachePolicies_.find(dataType);
    if (it != cachePolicies_.end()) {
        return it->second;
    }

    // Return default policy
    return CachePolicy(dataType, std::chrono::minutes(60));
}

// Private methods implementation
bool SmartCacheManager::initializeCacheTables() {
    auto db = dbManager_.getDatabase();
    if (!db) return false;

    const char* createTableSQL = R"(
        CREATE TABLE IF NOT EXISTS cache_entries (
            cache_key TEXT PRIMARY KEY,
            data BLOB NOT NULL,
            data_type TEXT NOT NULL,
            provider TEXT NOT NULL,
            symbol TEXT,
            cached_at INTEGER NOT NULL,
            expires_at INTEGER NOT NULL,
            last_accessed INTEGER NOT NULL,
            is_permanent INTEGER NOT NULL DEFAULT 0,
            access_count INTEGER NOT NULL DEFAULT 0,
            data_size_kb REAL NOT NULL DEFAULT 0.0,
            checksum TEXT NOT NULL DEFAULT ''
        )
    )";

    char* errMsg = nullptr;
    int result = sqlite3_exec(db, createTableSQL, nullptr, nullptr, &errMsg);

    if (result != SQLITE_OK) {
        std::cerr << "Failed to create cache table: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }

    // Create indexes for performance
    const char* indexSQL[] = {
        "CREATE INDEX IF NOT EXISTS idx_cache_data_type ON cache_entries(data_type)",
        "CREATE INDEX IF NOT EXISTS idx_cache_provider ON cache_entries(provider)",
        "CREATE INDEX IF NOT EXISTS idx_cache_symbol ON cache_entries(symbol)",
        "CREATE INDEX IF NOT EXISTS idx_cache_expires_at ON cache_entries(expires_at)",
        "CREATE INDEX IF NOT EXISTS idx_cache_checksum ON cache_entries(checksum)",
        "CREATE INDEX IF NOT EXISTS idx_cache_last_accessed ON cache_entries(last_accessed)"
    };

    for (const char* sql : indexSQL) {
        result = sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);
        if (result != SQLITE_OK) {
            std::cerr << "Failed to create cache index: " << errMsg << std::endl;
            sqlite3_free(errMsg);
        }
    }

    return true;
}

bool SmartCacheManager::persistCacheEntry(const CacheEntry& entry) {
    auto db = dbManager_.getDatabase();
    if (!db) return false;

    const char* sql = R"(
        INSERT OR REPLACE INTO cache_entries
        (cache_key, data, data_type, provider, symbol, cached_at, expires_at,
         last_accessed, is_permanent, access_count, data_size_kb, checksum)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    auto cachedAtEpoch = std::chrono::duration_cast<std::chrono::seconds>(
        entry.cachedAt.time_since_epoch()).count();
    auto expiresAtEpoch = std::chrono::duration_cast<std::chrono::seconds>(
        entry.expiresAt.time_since_epoch()).count();
    auto lastAccessedEpoch = std::chrono::duration_cast<std::chrono::seconds>(
        entry.lastAccessed.time_since_epoch()).count();

    sqlite3_bind_text(stmt, 1, entry.key.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_blob(stmt, 2, entry.data.data(), entry.data.size(), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, entry.dataType.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, entry.provider.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, entry.symbol.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 6, cachedAtEpoch);
    sqlite3_bind_int64(stmt, 7, expiresAtEpoch);
    sqlite3_bind_int64(stmt, 8, lastAccessedEpoch);
    sqlite3_bind_int(stmt, 9, entry.isPermanent ? 1 : 0);
    sqlite3_bind_int(stmt, 10, entry.accessCount);
    sqlite3_bind_double(stmt, 11, entry.dataSizeKB);
    sqlite3_bind_text(stmt, 12, entry.checksum.c_str(), -1, SQLITE_STATIC);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return result == SQLITE_DONE;
}

CacheEntry SmartCacheManager::loadCacheEntry(const std::string& key) const {
    CacheEntry entry;
    auto db = dbManager_.getDatabase();
    if (!db) return entry;

    const char* sql = R"(
        SELECT cache_key, data, data_type, provider, symbol, cached_at, expires_at,
               last_accessed, is_permanent, access_count, data_size_kb, checksum
        FROM cache_entries WHERE cache_key = ?
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return entry;
    }

    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        entry.key = (const char*)sqlite3_column_text(stmt, 0);

        // Get blob data
        const void* blob = sqlite3_column_blob(stmt, 1);
        int blobSize = sqlite3_column_bytes(stmt, 1);
        entry.data.assign((const char*)blob, blobSize);

        entry.dataType = (const char*)sqlite3_column_text(stmt, 2);
        entry.provider = (const char*)sqlite3_column_text(stmt, 3);

        const char* symbolText = (const char*)sqlite3_column_text(stmt, 4);
        entry.symbol = symbolText ? symbolText : "";

        entry.cachedAt = std::chrono::system_clock::from_time_t(sqlite3_column_int64(stmt, 5));
        entry.expiresAt = std::chrono::system_clock::from_time_t(sqlite3_column_int64(stmt, 6));
        entry.lastAccessed = std::chrono::system_clock::from_time_t(sqlite3_column_int64(stmt, 7));
        entry.isPermanent = sqlite3_column_int(stmt, 8) != 0;
        entry.accessCount = sqlite3_column_int(stmt, 9);
        entry.dataSizeKB = sqlite3_column_double(stmt, 10);

        const char* checksumText = (const char*)sqlite3_column_text(stmt, 11);
        entry.checksum = checksumText ? checksumText : "";
    }

    sqlite3_finalize(stmt);
    return entry;
}

bool SmartCacheManager::updateAccessStats(const std::string& key) {
    auto db = dbManager_.getDatabase();
    if (!db) return false;

    const char* sql = R"(
        UPDATE cache_entries
        SET last_accessed = ?, access_count = access_count + 1
        WHERE cache_key = ?
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    auto now = std::chrono::system_clock::now();
    auto nowEpoch = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();

    sqlite3_bind_int64(stmt, 1, nowEpoch);
    sqlite3_bind_text(stmt, 2, key.c_str(), -1, SQLITE_STATIC);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return result == SQLITE_DONE;
}

bool SmartCacheManager::deleteCacheEntry(const std::string& key) {
    auto db = dbManager_.getDatabase();
    if (!db) return false;

    const char* sql = "DELETE FROM cache_entries WHERE cache_key = ?";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_STATIC);

    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return result == SQLITE_DONE;
}

std::string SmartCacheManager::generateCacheKey(const std::string& dataType,
                                               const std::string& provider,
                                               const std::string& symbol,
                                               const std::string& suffix) const {
    std::ostringstream oss;
    oss << dataType << ":" << provider << ":" << symbol;
    if (!suffix.empty()) {
        oss << ":" << suffix;
    }
    return oss.str();
}

std::string SmartCacheManager::generateHistoricalKey(const std::string& symbol,
                                                    const std::string& provider,
                                                    const std::string& timeframe,
                                                    const std::chrono::system_clock::time_point& startDate,
                                                    const std::chrono::system_clock::time_point& endDate) const {
    std::ostringstream oss;
    oss << "historical:" << provider << ":" << symbol << ":" << timeframe << ":"
        << std::chrono::duration_cast<std::chrono::seconds>(startDate.time_since_epoch()).count() << ":"
        << std::chrono::duration_cast<std::chrono::seconds>(endDate.time_since_epoch()).count();
    return oss.str();
}

std::string SmartCacheManager::calculateChecksum(const std::string& data) const {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, data.c_str(), data.length());
    SHA256_Final(hash, &sha256);

    std::ostringstream oss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return oss.str();
}

bool SmartCacheManager::isExpired(const CacheEntry& entry) const {
    if (entry.isPermanent) return false;
    return std::chrono::system_clock::now() > entry.expiresAt;
}

double SmartCacheManager::calculateDataSizeKB(const std::string& data) const {
    return data.size() / 1024.0;
}

std::vector<std::string> SmartCacheManager::getExpiredKeys() const {
    std::vector<std::string> expiredKeys;
    auto db = dbManager_.getDatabase();
    if (!db) return expiredKeys;

    auto now = std::chrono::system_clock::now();
    auto nowEpoch = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();

    const char* sql = R"(
        SELECT cache_key FROM cache_entries
        WHERE is_permanent = 0 AND expires_at < ?
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, nowEpoch);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* key = (const char*)sqlite3_column_text(stmt, 0);
            if (key) {
                expiredKeys.push_back(key);
            }
        }
    }

    sqlite3_finalize(stmt);
    return expiredKeys;
}

bool SmartCacheManager::enforcePolicy(const std::string& dataType, const CacheEntry& entry) const {
    auto policy = getCachePolicy(dataType);

    // Check size limits
    if (entry.dataSizeKB > policy.maxSizeKB) {
        return false;
    }

    // Check if permanent storage is allowed
    if (entry.isPermanent && !policy.allowPermanent) {
        return false;
    }

    return true;
}

void SmartCacheManager::applyDefaultPolicies() {
    // Historical data policy (permanent storage)
    CachePolicy historicalPolicy("historical", std::chrono::hours(24 * 365), true);
    historicalPolicy.maxSizeKB = 10240.0; // 10MB per entry
    historicalPolicy.maxEntries = 1000;
    historicalPolicy.enableDeduplication = true;
    historicalPolicy.compressData = true;
    cachePolicies_["historical"] = historicalPolicy;

    // Real-time price data policy
    CachePolicy pricePolicy("price", std::chrono::minutes(15), false);
    pricePolicy.maxSizeKB = 1024.0; // 1MB per entry
    pricePolicy.maxEntries = 500;
    pricePolicy.enableDeduplication = false;
    pricePolicy.compressData = false;
    cachePolicies_["price"] = pricePolicy;

    // News data policy
    CachePolicy newsPolicy("news", std::chrono::hours(6), false);
    newsPolicy.maxSizeKB = 5120.0; // 5MB per entry
    newsPolicy.maxEntries = 200;
    newsPolicy.enableDeduplication = true;
    newsPolicy.compressData = true;
    cachePolicies_["news"] = newsPolicy;

    // Sentiment data policy
    CachePolicy sentimentPolicy("sentiment", std::chrono::hours(12), false);
    sentimentPolicy.maxSizeKB = 512.0; // 512KB per entry
    sentimentPolicy.maxEntries = 100;
    sentimentPolicy.enableDeduplication = false;
    sentimentPolicy.compressData = false;
    cachePolicies_["sentiment"] = sentimentPolicy;
}

bool SmartCacheManager::shouldCompress(const std::string& dataType, double sizeKB) const {
    auto policy = getCachePolicy(dataType);
    return policy.compressData && sizeKB > 100.0; // Compress if policy allows and size > 100KB
}

std::string SmartCacheManager::compressData(const std::string& data) const {
    uLongf compressedSize = compressBound(data.size());
    std::vector<Bytef> compressed(compressedSize);

    int result = compress(compressed.data(), &compressedSize,
                         reinterpret_cast<const Bytef*>(data.c_str()), data.size());

    if (result != Z_OK) {
        return data; // Return uncompressed if compression fails
    }

    compressed.resize(compressedSize);
    return std::string(compressed.begin(), compressed.end());
}

std::string SmartCacheManager::decompressData(const std::string& compressedData) const {
    uLongf uncompressedSize = compressedData.size() * 4; // Initial guess
    std::vector<Bytef> uncompressed(uncompressedSize);

    int result = uncompress(uncompressed.data(), &uncompressedSize,
                           reinterpret_cast<const Bytef*>(compressedData.c_str()),
                           compressedData.size());

    if (result != Z_OK) {
        return compressedData; // Return as-is if decompression fails
    }

    uncompressed.resize(uncompressedSize);
    return std::string(uncompressed.begin(), uncompressed.end());
}

void SmartCacheManager::optimizeCache() {
    cleanupExpiredEntries();
    compactDatabase();
}

bool SmartCacheManager::compactDatabase() {
    auto db = dbManager_.getDatabase();
    if (!db) return false;

    char* errMsg = nullptr;
    int result = sqlite3_exec(db, "VACUUM", nullptr, nullptr, &errMsg);

    if (result != SQLITE_OK) {
        std::cerr << "Failed to compact cache database: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }

    return true;
}

// CacheManagerFactory Implementation
std::unique_ptr<SmartCacheManager> CacheManagerFactory::create(DatabaseManager& dbManager, Strategy strategy) {
    auto cacheManager = std::make_unique<SmartCacheManager>(dbManager);

    auto policies = getPoliciesForStrategy(strategy);
    for (const auto& policy : policies) {
        cacheManager->setCachePolicy(policy);
    }

    return cacheManager;
}

std::vector<CachePolicy> CacheManagerFactory::getPoliciesForStrategy(Strategy strategy) {
    switch (strategy) {
        case Strategy::CONSERVATIVE:
            return getConservativePolicies();
        case Strategy::BALANCED:
            return getBalancedPolicies();
        case Strategy::AGGRESSIVE:
            return getAggressivePolicies();
        case Strategy::HISTORICAL_ONLY:
            return getHistoricalOnlyPolicies();
        default:
            return getBalancedPolicies();
    }
}

std::vector<CachePolicy> CacheManagerFactory::getBalancedPolicies() {
    std::vector<CachePolicy> policies;

    // Historical data - permanent storage for personal use
    CachePolicy historical("historical", std::chrono::hours(24 * 365), true);
    historical.maxSizeKB = 20480.0; // 20MB per entry
    historical.maxEntries = 500; // 500 historical entries max
    historical.enableDeduplication = true;
    historical.compressData = true;
    policies.push_back(historical);

    // Real-time price data - 15 minute cache for personal trading
    CachePolicy price("price", std::chrono::minutes(15), false);
    price.maxSizeKB = 512.0; // 512KB per entry
    price.maxEntries = 50; // Only 50 price entries
    price.enableDeduplication = false;
    price.compressData = false;
    policies.push_back(price);

    // News data - 6 hour cache
    CachePolicy news("news", std::chrono::hours(6), false);
    news.maxSizeKB = 2048.0; // 2MB per entry
    news.maxEntries = 50; // Limited news entries
    news.enableDeduplication = true;
    news.compressData = true;
    policies.push_back(news);

    return policies;
}

std::vector<CachePolicy> CacheManagerFactory::getConservativePolicies() {
    std::vector<CachePolicy> policies;

    // Only cache historical data permanently, everything else very short term
    CachePolicy historical("historical", std::chrono::hours(24 * 365), true);
    historical.maxSizeKB = 10240.0; // 10MB per entry
    historical.maxEntries = 100; // Limited to 100 historical entries
    historical.enableDeduplication = true;
    historical.compressData = true;
    policies.push_back(historical);

    CachePolicy price("price", std::chrono::minutes(5), false);
    price.maxSizeKB = 256.0; // 256KB per entry
    price.maxEntries = 10; // Very limited
    policies.push_back(price);

    return policies;
}

std::vector<CachePolicy> CacheManagerFactory::getHistoricalOnlyPolicies() {
    std::vector<CachePolicy> policies;

    // Only cache historical data, nothing else
    CachePolicy historical("historical", std::chrono::hours(24 * 365), true);
    historical.maxSizeKB = 50240.0; // 50MB per entry - larger for historical only
    historical.maxEntries = 1000; // More historical entries allowed
    historical.enableDeduplication = true;
    historical.compressData = true;
    policies.push_back(historical);

    return policies;
}

std::vector<CachePolicy> CacheManagerFactory::getAggressivePolicies() {
    std::vector<CachePolicy> policies;

    CachePolicy historical("historical", std::chrono::hours(24 * 365), true);
    historical.maxSizeKB = 102400.0; // 100MB per entry
    historical.maxEntries = 2000;
    historical.enableDeduplication = true;
    historical.compressData = true;
    policies.push_back(historical);

    CachePolicy price("price", std::chrono::hours(1), false);
    price.maxSizeKB = 2048.0;
    price.maxEntries = 200;
    policies.push_back(price);

    CachePolicy news("news", std::chrono::hours(24), false);
    news.maxSizeKB = 10240.0;
    news.maxEntries = 500;
    news.enableDeduplication = true;
    news.compressData = true;
    policies.push_back(news);

    return policies;
}

} // namespace Data
} // namespace CryptoClaude