#include "../Core/Console/ProductionConsoleApp.h"
#include <iostream>
#include <string>
#include <cstdlib>

int main(int argc, char* argv[]) {
    using namespace CryptoClaude::Console;

    try {
        // Parse command line arguments
        std::string configFile = "config/production.json";
        bool verbose = false;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "--config" && i + 1 < argc) {
                configFile = argv[++i];
            } else if (arg == "--verbose" || arg == "-v") {
                verbose = true;
            } else if (arg == "--help" || arg == "-h") {
                std::cout << "CryptoClaude Production Console\n\n";
                std::cout << "Usage: " << argv[0] << " [options]\n\n";
                std::cout << "Options:\n";
                std::cout << "  --config <file>    Configuration file path (default: config/production.json)\n";
                std::cout << "  --verbose, -v      Enable verbose output\n";
                std::cout << "  --help, -h         Show this help message\n\n";
                std::cout << "Environment Variables:\n";
                std::cout << "  CRYPTOCOMPARE_API_KEY    Required for market data access\n";
                std::cout << "  CLAUDE_API_KEY          Optional for AI-enhanced features\n\n";
                std::cout << "Examples:\n";
                std::cout << "  " << argv[0] << "                           # Use default configuration\n";
                std::cout << "  " << argv[0] << " --config config/dev.json  # Use development configuration\n";
                std::cout << "  " << argv[0] << " --verbose                  # Enable verbose logging\n\n";
                return 0;
            } else {
                std::cerr << "Unknown argument: " << arg << "\n";
                std::cerr << "Use --help for usage information.\n";
                return 1;
            }
        }

        // Check for required environment variables
        const char* cryptoCompareKey = std::getenv("CRYPTOCOMPARE_API_KEY");
        if (!cryptoCompareKey) {
            std::cerr << "❌ Error: CRYPTOCOMPARE_API_KEY environment variable is required\n";
            std::cerr << "Please set your CryptoCompare API key:\n";
            std::cerr << "  export CRYPTOCOMPARE_API_KEY=your_api_key_here\n\n";
            return 1;
        }

        // Claude API key is optional
        const char* claudeApiKey = std::getenv("CLAUDE_API_KEY");
        if (!claudeApiKey) {
            std::cout << "⚠️  Note: CLAUDE_API_KEY not set - AI features will be disabled\n";
            std::cout << "To enable AI score polishing, set:\n";
            std::cout << "  export CLAUDE_API_KEY=your_claude_api_key_here\n\n";
        }

        if (verbose) {
            std::cout << "🔧 Configuration:\n";
            std::cout << "   Config file: " << configFile << "\n";
            std::cout << "   CryptoCompare API: " << (cryptoCompareKey ? "✅ Configured" : "❌ Missing") << "\n";
            std::cout << "   Claude API: " << (claudeApiKey ? "✅ Configured" : "❌ Missing") << "\n";
            std::cout << "   Verbose mode: Enabled\n\n";
        }

        // Create and initialize the production console application
        ProductionConsoleApp app(configFile);

        if (!app.initialize()) {
            std::cerr << "❌ Failed to initialize CryptoClaude console application\n";
            return 1;
        }

        // Run the interactive console
        app.run();

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "❌ Fatal error: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "❌ Unknown fatal error occurred\n";
        return 1;
    }
}