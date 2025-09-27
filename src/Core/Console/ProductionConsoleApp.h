#pragma once

#include "CommandFramework.h"
#include "Commands/SystemCommands.h"
#include "Commands/TradingCommands.h"
#include "Commands/MonitoringCommands.h"
#include "Commands/ConfigurationCommands.h"
#include "Commands/BacktestingCommands.h"
#include "Commands/AnalysisCommands.h"
#include "Commands/AlgorithmCommands.h"
#include "Commands/DataQualityCommands.h"
#include "Commands/PerformanceCommands.h"
#include "../Database/DatabaseManager.h"
#include "../Trading/TradingEngine.h"
#include "../Portfolio/PortfolioManager.h"
#include "../Risk/RiskManager.h"
#include "../DataPipeline/EnhancedMarketDataPipeline.h"
#include "../AI/AIDecisionEngine.h"
#include "../Backtesting/BacktestEngine.h"
#include "../Config/ParameterManager.h"
#include <memory>
#include <iostream>
#include <string>
#include <signal.h>

namespace CryptoClaude {
namespace Console {

using namespace CryptoClaude::Database;
using namespace CryptoClaude::Trading;
using namespace CryptoClaude::Portfolio;
using namespace CryptoClaude::Risk;
using namespace CryptoClaude::DataPipeline;
using namespace CryptoClaude::AI;
using namespace CryptoClaude::Backtesting;
using namespace CryptoClaude::Config;

class ProductionConsoleApp {
private:
    // Core framework
    std::unique_ptr<CommandFramework> framework_;

    // Core system components
    DatabaseManager* dbManager_;
    std::shared_ptr<TradingEngine> tradingEngine_;
    std::shared_ptr<PortfolioManager> portfolioManager_;
    std::shared_ptr<RiskManager> riskManager_;
    std::shared_ptr<EnhancedMarketDataPipeline> dataPipeline_;
    std::shared_ptr<AIScorePolishEngine> aiEngine_;
    std::shared_ptr<BacktestEngine> backtestEngine_;
    std::shared_ptr<ParameterManager> parameterManager_;

    // Application state
    bool running_;
    bool shutdownRequested_;
    std::string configFile_;

public:
    explicit ProductionConsoleApp(const std::string& configFile = "config/production.json");
    ~ProductionConsoleApp();

    // Application lifecycle
    bool initialize();
    void run();
    void shutdown();

    // Signal handling
    void handleSignal(int signal);

    // Configuration
    bool loadConfiguration();
    void displayWelcomeBanner();
    void displayShutdownMessage();

private:
    // Initialization helpers
    bool initializeDatabase();
    bool initializeDataPipeline();
    bool initializeTradingEngine();
    bool initializeAISystem();
    bool initializeBacktestEngine();
    bool registerCommands();

    // Data management with caching
    bool ensureHistoricalDataAvailable();
    bool performDataGapAnalysis();
    bool fillDataGaps();

    // Console interface
    void runInteractiveSession();
    void processCommand(const std::string& commandLine);
    std::string getPrompt() const;

    // System monitoring
    void performSystemHealthCheck();
    void displaySystemStatus();

    // Production safety
    bool validateProductionReadiness();
    void enableProductionSafetyFeatures();

    // Paper trading initialization
    bool initializePaperTradingMode();
    void displayPaperTradingWarning();
};

} // namespace Console
} // namespace CryptoClaude