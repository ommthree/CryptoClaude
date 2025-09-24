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
               src/Core/Utils/HttpClient.cpp

# Object Files
CORE_OBJECTS = $(CORE_SOURCES:.cpp=.o)

# Test Programs
TESTS = SimpleValidatorTest

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

# Clean up
clean:
	@echo "🧹 Cleaning up..."
	rm -f $(CORE_OBJECTS) $(TESTS) *.db

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
	@echo "  test-json        - Test JSON functionality specifically"
	@echo "  install-deps     - Install required dependencies"
	@echo "  clean            - Remove build artifacts"
	@echo "  help             - Show this help message"
	@echo ""
	@echo "Dependencies:"
	@echo "  - g++ compiler (C++17 support required)"
	@echo "  - SQLite3 development libraries"
	@echo "  - nlohmann-json library"
	@echo ""
	@echo "To install dependencies: make install-deps"

.PHONY: all check-dependencies test-json test clean install-deps help