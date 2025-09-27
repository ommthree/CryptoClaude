// Component stub implementations to resolve linking issues
#include "src/Core/Risk/ProductionRiskManager.h"
#include "src/Core/Strategy/TradingStrategyOptimizer.h"
#include "src/Core/Analytics/AdvancedPerformanceEngine.h"

namespace CryptoClaude {

// DatabaseManager singleton stub
namespace Database {
    class DatabaseManager {
    public:
        static DatabaseManager& getInstance() {
            static DatabaseManager instance;
            return instance;
        }
        ~DatabaseManager() = default;
    };
}

// Risk Manager stub destructor
namespace Risk {
    ProductionRiskManager::~ProductionRiskManager() {
        // Let unique_ptr members clean themselves up
        // For singleton components, release without deleting
        if (database_manager_) {
            database_manager_.release();
        }
    }
}

// Strategy Optimizer stub destructor
namespace Strategy {
    TradingStrategyOptimizer::~TradingStrategyOptimizer() {
        // Let unique_ptr members clean themselves up naturally
    }
}

// Performance Engine stub destructor
namespace Analytics {
    AdvancedPerformanceEngine::~AdvancedPerformanceEngine() {
        // Let unique_ptr members clean themselves up naturally
    }
}

}