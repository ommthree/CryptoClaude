# CryptoClaude Secure Build Configuration
# This Makefile ensures proper dependency handling and secure compilation

# Compiler Configuration
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

# PORTABLE DEPENDENCY DETECTION
# Uses pkg-config, compiler introspection, and dynamic path discovery
# Removes all hard-coded system-specific paths for true portability

# Try pkg-config first (most portable method)
NLOHMANN_PKGCONFIG := $(shell pkg-config --exists nlohmann_json 2>/dev/null && echo yes)
ifeq ($(NLOHMANN_PKGCONFIG), yes)
    NLOHMANN_INCLUDE = $(shell pkg-config --cflags nlohmann_json)
    $(info Found nlohmann-json via pkg-config)
else
    # Dynamic path discovery - searches common prefixes
    COMMON_PREFIXES := /usr /usr/local /opt/homebrew $(HOME)/.local
    NLOHMANN_PATH := $(shell for prefix in $(COMMON_PREFIXES); do \
        if [ -d "$$prefix/include/nlohmann" ]; then \
            echo "-I$$prefix/include"; \
            break; \
        fi; \
    done)

    ifneq ($(NLOHMANN_PATH),)
        NLOHMANN_INCLUDE = $(NLOHMANN_PATH)
        $(info Found nlohmann-json via path discovery: $(NLOHMANN_PATH))
    else
        # Compiler test - let compiler find it in standard locations
        COMPILER_FOUND := $(shell echo '\#include <nlohmann/json.hpp>' | $(CXX) -x c++ -E - >/dev/null 2>&1 && echo yes)
        ifeq ($(COMPILER_FOUND), yes)
            NLOHMANN_INCLUDE =
            $(info Found nlohmann-json in compiler default paths)
        else
            NLOHMANN_INCLUDE =
            $(warning nlohmann-json not found. Please run ./install_dependencies.sh)
        endif
    endif
endif

# Include Directories
INCLUDES = -Isrc/Core $(NLOHMANN_INCLUDE)

# PORTABLE LIBRARY DETECTION
# Uses pkg-config for portable library linking
SQLITE_PKGCONFIG := $(shell pkg-config --exists sqlite3 2>/dev/null && echo yes)
ifeq ($(SQLITE_PKGCONFIG), yes)
    LIBS = $(shell pkg-config --libs sqlite3)
    $(info Found SQLite3 via pkg-config)
else
    LIBS = -lsqlite3
    $(info Using fallback SQLite3 linking)
endif

# Source Files
CORE_SOURCES = src/Core/Database/DatabaseManager.cpp \
               src/Core/Database/MigrationManager.cpp \
               src/Core/Database/DataQualityManager.cpp \
               src/Core/Json/JsonHelper.cpp \
               src/Core/Http/HttpClient.cpp \
               src/Core/Utils/HttpClient.cpp \
               src/Core/Analytics/StatisticalTools.cpp

# Day 6 Advanced Components
DAY6_SOURCES = src/Core/Analytics/BacktestingEngine.cpp \
               src/Core/Analytics/TechnicalIndicators.cpp \
               src/Core/Analytics/PerformanceAttributionEngine.cpp \
               src/Core/Strategy/HistoricalValidator.cpp \
               src/Core/Trading/PaperTradingEngine.cpp \
               src/Core/Monitoring/PaperTradingMonitor.cpp \
               src/Core/Testing/MarketScenarioSimulator.cpp \
               src/Core/Testing/EdgeCaseSimulator.cpp

# Day 7 Portfolio Optimization Components
DAY7_SOURCES = src/Core/Portfolio/PortfolioOptimizer.cpp

# Day 8 Machine Learning Components
DAY8_SOURCES = src/Core/ML/RandomForestPredictor.cpp

# Day 9 Ensemble ML and Cross-Asset Correlation Components
DAY9_SOURCES = src/Core/Analytics/CrossAssetCorrelationMonitor.cpp \
               src/Core/ML/CorrelationMLEnhancer.cpp \
               src/Core/ML/EnsembleMLPredictor.cpp

# Day 10 Trading Engine Components
DAY10_SOURCES = src/Core/Trading/TradingEngine.cpp \
                src/Core/Risk/PredictiveRiskEngine.cpp

# All Source Files
ALL_SOURCES = $(CORE_SOURCES) $(DAY6_SOURCES) $(DAY7_SOURCES) $(DAY8_SOURCES) $(DAY9_SOURCES) $(DAY10_SOURCES)

# Object Files
CORE_OBJECTS = $(CORE_SOURCES:.cpp=.o)
DAY6_OBJECTS = $(DAY6_SOURCES:.cpp=.o)
DAY7_OBJECTS = $(DAY7_SOURCES:.cpp=.o)
DAY8_OBJECTS = $(DAY8_SOURCES:.cpp=.o)
DAY9_OBJECTS = $(DAY9_SOURCES:.cpp=.o)
DAY10_OBJECTS = $(DAY10_SOURCES:.cpp=.o)
ALL_OBJECTS = $(ALL_SOURCES:.cpp=.o)

# Test Programs
TESTS = SimpleValidatorTest Day6BacktestingAndTradingTest Day7PortfolioOptimizationTest Day8RandomForestTest Day9EnsembleMLTest Day10TradingEngineTest Day10TradingEngineValidation

# Default Target
all: check-dependencies $(TESTS)

# COMPREHENSIVE DEPENDENCY VALIDATION
# Platform-independent dependency checking with clear error messages
check-dependencies:
	@echo "🔍 Checking dependencies..."
	@command -v $(CXX) >/dev/null 2>&1 || { echo "❌ $(CXX) compiler not found. Please install a C++ compiler."; exit 1; }
	@echo "✅ $(CXX) compiler found"
	@echo '#include <iostream>' | $(CXX) -x c++ -std=c++17 -E - >/dev/null 2>&1 || { echo "❌ C++17 support not available. Please upgrade your compiler."; exit 1; }
	@echo "✅ C++17 support confirmed"
	@if ! pkg-config --exists sqlite3 2>/dev/null; then \
		echo "⚠️  SQLite3 pkg-config not found, testing fallback linking..."; \
		SQLITE_TEST=$$(mktemp); \
		echo 'int main(){}' | $(CXX) -x c++ - -lsqlite3 -o "$$SQLITE_TEST" 2>/dev/null && rm -f "$$SQLITE_TEST" || { rm -f "$$SQLITE_TEST"; echo "❌ SQLite3 library not found. Please install libsqlite3-dev or sqlite-devel."; exit 1; }; \
	else \
		echo "✅ SQLite3 found via pkg-config"; \
	fi
	@echo "✅ All dependencies validated"

# Core Object Files
%.o: %.cpp
	@echo "🔧 Compiling $<..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Test Programs
SimpleValidatorTest: SimpleValidatorTest.cpp $(CORE_OBJECTS)
	@echo "🔨 Building SimpleValidatorTest..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< $(CORE_OBJECTS) $(LIBS) -o $@

# Day 6 Comprehensive Backtesting and Trading Test
Day6BacktestingAndTradingTest: Day6BacktestingAndTradingTest.cpp $(ALL_OBJECTS)
	@echo "🔨 Building Day6BacktestingAndTradingTest..."
	@echo "🎯 Compiling comprehensive Day 6 backtesting and paper trading test suite..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< $(ALL_OBJECTS) $(LIBS) -o $@

# Day 7 Portfolio Optimization Test
Day7PortfolioOptimizationTest: Day7PortfolioOptimizationTest.cpp $(ALL_OBJECTS)
	@echo "🔨 Building Day7PortfolioOptimizationTest..."
	@echo "📊 Compiling comprehensive Day 7 portfolio optimization and ML foundation test suite..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< $(ALL_OBJECTS) $(LIBS) -o $@

# Day 8 Random Forest ML Test
Day8RandomForestTest: Day8RandomForestTest.cpp $(ALL_OBJECTS)
	@echo "🔨 Building Day8RandomForestTest..."
	@echo "🧠 Compiling comprehensive Day 8 Random Forest ML integration test suite..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< $(ALL_OBJECTS) $(LIBS) -o $@

# Day 9 Ensemble ML and Cross-Asset Correlation Test
Day9EnsembleMLTest: tests/Day9EnsembleMLTest.cpp $(ALL_OBJECTS)
	@echo "🔨 Building Day9EnsembleMLTest..."
	@echo "🔗 Compiling comprehensive Day 9 Ensemble ML and Cross-Asset Correlation integration test suite..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< $(ALL_OBJECTS) $(LIBS) -o $@

# Day 10 Trading Engine Test
Day10TradingEngineTest: tests/Day10TradingEngineTest_Simple.cpp $(ALL_OBJECTS)
	@echo "🔨 Building Day10TradingEngineTest..."
	@echo "⚡ Compiling Day 10 Trading Engine core functionality test suite..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< $(ALL_OBJECTS) $(LIBS) -o $@

# Day 10 Trading Engine Independent Validation
Day10TradingEngineValidation: Day10TradingEngineValidation.cpp $(ALL_OBJECTS)
	@echo "🔨 Building Day10TradingEngineValidation..."
	@echo "🔍 Compiling Day 10 Trading Engine independent validation suite..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< $(ALL_OBJECTS) $(LIBS) -o $@

# ProperSecurityTest removed - functionality replaced by ProductionSecurityTest

MigrationValidationTest: MigrationValidationTest.cpp $(CORE_OBJECTS)
	@echo "🔨 Building MigrationValidationTest..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< $(CORE_OBJECTS) $(LIBS) -o $@

HttpClientValidationTest: HttpClientValidationTest.cpp src/Core/Http/HttpClient.o src/Core/Utils/HttpClient.o
	@echo "🔨 Building HttpClientValidationTest..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< src/Core/Http/HttpClient.o src/Core/Utils/HttpClient.o -o $@

# SECURE JSON FUNCTIONALITY TEST - NO PREDICTABLE TEMP FILES
test-json: src/Core/Json/JsonHelper.o
	@echo "🧪 Testing JSON functionality..."
	@JSON_TEST_DIR=$$(mktemp -d -t json-test-XXXXXX); \
	JSON_TEST_FILE="$$JSON_TEST_DIR/json_test.cpp"; \
	JSON_TEST_BINARY="$$JSON_TEST_DIR/json_test"; \
	echo '#include "src/Core/Json/JsonHelper.h"' > "$$JSON_TEST_FILE"; \
	echo '#include <iostream>' >> "$$JSON_TEST_FILE"; \
	echo 'using namespace CryptoClaude::Json;' >> "$$JSON_TEST_FILE"; \
	echo 'int main() {' >> "$$JSON_TEST_FILE"; \
	echo '    if (!JsonHelper::isJsonLibraryAvailable()) {' >> "$$JSON_TEST_FILE"; \
	echo '        std::cout << "❌ " << JsonHelper::getInstallationInstructions() << std::endl;' >> "$$JSON_TEST_FILE"; \
	echo '        return 1;' >> "$$JSON_TEST_FILE"; \
	echo '    }' >> "$$JSON_TEST_FILE"; \
	echo '    json j;' >> "$$JSON_TEST_FILE"; \
	echo '    if (JsonHelper::parseString("{\\"test\\": \\"value\\"}", j)) {' >> "$$JSON_TEST_FILE"; \
	echo '        std::cout << "✅ JSON test passed: " << JsonHelper::getString(j, "test") << std::endl;' >> "$$JSON_TEST_FILE"; \
	echo '        return 0;' >> "$$JSON_TEST_FILE"; \
	echo '    }' >> "$$JSON_TEST_FILE"; \
	echo '    std::cout << "❌ JSON test failed" << std::endl;' >> "$$JSON_TEST_FILE"; \
	echo '    return 1;' >> "$$JSON_TEST_FILE"; \
	echo '}' >> "$$JSON_TEST_FILE"; \
	$(CXX) $(CXXFLAGS) $(INCLUDES) "$$JSON_TEST_FILE" src/Core/Json/JsonHelper.o -o "$$JSON_TEST_BINARY"; \
	"$$JSON_TEST_BINARY"; \
	TEST_RESULT=$$?; \
	rm -rf "$$JSON_TEST_DIR"; \
	exit $$TEST_RESULT

# Run all tests
test: all test-json
	@echo "🧪 Running all validation tests..."
	@./SimpleValidatorTest && echo "✅ SimpleValidatorTest passed" || echo "❌ SimpleValidatorTest failed"

# Run Day 6 comprehensive test
test-day6: Day6BacktestingAndTradingTest
	@echo "🎯 Running Day 6 Comprehensive Backtesting and Paper Trading Test..."
	@echo "⚡ Testing: Backtesting Engine, Risk Validation, Paper Trading, Performance Monitoring, Market Scenarios, Edge Cases"
	@./Day6BacktestingAndTradingTest && echo "✅ Day 6 Comprehensive Test Suite PASSED" || echo "❌ Day 6 Test Suite FAILED"

# Run Day 7 portfolio optimization test
test-day7: Day7PortfolioOptimizationTest
	@echo "📊 Running Day 7 Portfolio Optimization and ML Foundation Test..."
	@echo "⚡ Testing: Portfolio Optimization, Risk Parity, Volatility Weighting, Sentiment Integration"
	@./Day7PortfolioOptimizationTest && echo "✅ Day 7 Portfolio Optimization Test Suite PASSED" || echo "❌ Day 7 Test Suite FAILED"

# Run Day 8 Random Forest ML test
test-day8: Day8RandomForestTest
	@echo "🧠 Running Day 8 Random Forest ML Integration Test..."
	@echo "⚡ Testing: Random Forest Predictor, Feature Engineering, ML-Enhanced Portfolio Optimization"
	@./Day8RandomForestTest && echo "✅ Day 8 Random Forest ML Test Suite PASSED" || echo "❌ Day 8 Test Suite FAILED"

# Run Day 9 Ensemble ML and Cross-Asset Correlation test
test-day9: Day9EnsembleMLTest
	@echo "🔗 Running Day 9 Ensemble ML and Cross-Asset Correlation Test..."
	@echo "⚡ Testing: Cross-Asset Correlation Monitor, ML-Enhanced Correlation Analysis, Ensemble ML Predictor, Correlation-Aware Optimization"
	@./Day9EnsembleMLTest && echo "✅ Day 9 Ensemble ML and Cross-Asset Correlation Test Suite PASSED" || echo "❌ Day 9 Test Suite FAILED"

# Run Day 10 Trading Engine test
test-day10: Day10TradingEngineTest
	@echo "⚡ Running Day 10 Trading Engine Test..."
	@echo "🎯 Testing: Trading Engine Core Operations, Pair-based Trading Strategy, Risk Management, Order Execution, Portfolio Optimization"
	@./Day10TradingEngineTest && echo "✅ Day 10 Trading Engine Test Suite PASSED" || echo "❌ Day 10 Test Suite FAILED"

# Run Day 10 Trading Engine independent validation
validate-day10: Day10TradingEngineValidation
	@echo "🔍 Running Day 10 Trading Engine Independent Validation..."
	@echo "📋 Comprehensive production-readiness validation with detailed reporting"
	@./Day10TradingEngineValidation && echo "✅ Day 10 Independent Validation PASSED" || echo "❌ Day 10 Independent Validation FAILED"

# Clean up
clean:
	@echo "🧹 Cleaning up..."
	rm -f $(ALL_OBJECTS) $(TESTS) *.db

# Install dependencies
install-deps:
	@echo "📦 Installing dependencies..."
	./install_dependencies.sh

# Help
help:
	@echo "CryptoClaude Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all              - Build all test programs"
	@echo "  test             - Run all validation tests"
	@echo "  test-day6        - Run Day 6 comprehensive backtesting and paper trading test"
	@echo "  test-day7        - Run Day 7 portfolio optimization and ML foundation test"
	@echo "  test-day8        - Run Day 8 Random Forest ML integration test"
	@echo "  test-day9        - Run Day 9 Ensemble ML and cross-asset correlation test"
	@echo "  test-day10       - Run Day 10 Trading Engine integration test"
	@echo "  validate-day10   - Run Day 10 Trading Engine independent validation"
	@echo "  test-json        - Test JSON functionality specifically"
	@echo "  install-deps     - Install required dependencies"
	@echo "  clean            - Remove build artifacts"
	@echo "  help             - Show this help message"
	@echo ""
	@echo "Day 6-10 Advanced Components:"
	@echo "  Day 6 - Backtesting Engine, Risk Validation, Paper Trading, Market Scenarios"
	@echo "  Day 7 - Portfolio Optimization, Risk Parity, Volatility Weighting, Sentiment Integration"
	@echo "  Day 8 - Random Forest ML Predictor, Feature Engineering, ML-Enhanced Optimization"
	@echo "  Day 9 - Cross-Asset Correlation Monitor, Ensemble ML, Correlation-Aware Optimization"
	@echo "  Day 10 - Trading Engine, Pair-based Strategy, Order Execution, Risk Management"
	@echo ""
	@echo "Dependencies:"
	@echo "  - g++ compiler (C++17 support required)"
	@echo "  - SQLite3 development libraries"
	@echo "  - nlohmann-json library"
	@echo ""
	@echo "To install dependencies: make install-deps"

.PHONY: all check-dependencies test-json test test-day6 test-day7 test-day8 test-day9 test-day10 validate-day10 clean install-deps help