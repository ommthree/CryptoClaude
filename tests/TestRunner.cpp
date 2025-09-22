#include <gtest/gtest.h>
#include <iostream>
#include <chrono>

/**
 * Main test runner for CryptoClaude Integration Tests
 *
 * This runner provides:
 * - Comprehensive test execution
 * - Performance timing
 * - Test result summary
 * - Memory leak detection support
 */

// Custom test listener for detailed reporting
class DetailedTestListener : public ::testing::TestEventListener {
private:
    std::chrono::high_resolution_clock::time_point start_time_;
    std::chrono::high_resolution_clock::time_point test_start_time_;

public:
    void OnTestProgramStart(const ::testing::UnitTest&) override {
        start_time_ = std::chrono::high_resolution_clock::now();
        std::cout << "\n=== CryptoClaude Integration Test Suite ===" << std::endl;
        std::cout << "Starting comprehensive integration tests..." << std::endl;
        std::cout << "Testing: Repository layer, Business logic, Data integrity\n" << std::endl;
    }

    void OnTestStart(const ::testing::TestInfo& test_info) override {
        test_start_time_ = std::chrono::high_resolution_clock::now();
        std::cout << "[RUNNING ] " << test_info.test_case_name()
                  << "." << test_info.name() << std::endl;
    }

    void OnTestEnd(const ::testing::TestInfo& test_info) override {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - test_start_time_);

        const char* result = test_info.result()->Passed() ? "[  OK  ]" : "[ FAIL ]";
        std::cout << result << " " << test_info.test_case_name()
                  << "." << test_info.name()
                  << " (" << duration.count() << " ms)" << std::endl;

        if (!test_info.result()->Passed()) {
            // Print failure details
            for (int i = 0; i < test_info.result()->total_part_count(); ++i) {
                const auto& part = test_info.result()->GetTestPartResult(i);
                if (part.failed()) {
                    std::cout << "  FAILURE: " << part.message() << std::endl;
                }
            }
        }
    }

    void OnTestProgramEnd(const ::testing::UnitTest& unit_test) override {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time_);

        std::cout << "\n=== Test Results Summary ===" << std::endl;
        std::cout << "Total tests run: " << unit_test.test_to_run_count() << std::endl;
        std::cout << "Tests passed: " << unit_test.successful_test_count() << std::endl;
        std::cout << "Tests failed: " << unit_test.failed_test_count() << std::endl;
        std::cout << "Total execution time: " << total_duration.count() << " ms" << std::endl;

        if (unit_test.failed_test_count() > 0) {
            std::cout << "\n[WARNING] Some tests failed. Check the output above for details." << std::endl;
        } else {
            std::cout << "\n[SUCCESS] All tests passed! CryptoClaude integration layer is working correctly." << std::endl;
        }

        // Performance analysis
        double avg_test_time = static_cast<double>(total_duration.count()) / unit_test.test_to_run_count();
        std::cout << "Average test execution time: " << avg_test_time << " ms" << std::endl;

        if (avg_test_time > 100.0) {
            std::cout << "[NOTICE] Average test time exceeds 100ms. Consider optimizing database operations." << std::endl;
        }
    }

    // Required interface methods (no-op implementations)
    void OnTestIterationStart(const ::testing::UnitTest&, int) override {}
    void OnEnvironmentsSetUpStart(const ::testing::UnitTest&) override {}
    void OnEnvironmentsSetUpEnd(const ::testing::UnitTest&) override {}
    void OnTestCaseStart(const ::testing::TestCase&) override {}
    void OnTestCaseEnd(const ::testing::TestCase&) override {}
    void OnEnvironmentsTearDownStart(const ::testing::UnitTest&) override {}
    void OnEnvironmentsTearDownEnd(const ::testing::UnitTest&) override {}
    void OnTestIterationEnd(const ::testing::UnitTest&, int) override {}
};

int main(int argc, char** argv) {
    // Initialize Google Test
    ::testing::InitGoogleTest(&argc, argv);

    // Add custom listener for detailed reporting
    ::testing::TestEventListeners& listeners = ::testing::UnitTest::GetInstance()->listeners();

    // Remove default printer
    delete listeners.Release(listeners.default_result_printer());

    // Add our custom listener
    listeners.Append(new DetailedTestListener);

    // Configure test execution
    std::cout << "CryptoClaude Integration Test Configuration:" << std::endl;
    std::cout << "- Database: In-memory SQLite" << std::endl;
    std::cout << "- Test Data: Hybrid (fixtures + generators)" << std::endl;
    std::cout << "- Coverage: Repository layer + Business logic" << std::endl;
    std::cout << "- Performance: Benchmarking enabled" << std::endl;

    // Run all tests
    int result = RUN_ALL_TESTS();

    // Additional cleanup or reporting could go here
    if (result == 0) {
        std::cout << "\n🎉 Integration tests completed successfully!" << std::endl;
        std::cout << "CryptoClaude repository layer is ready for production use." << std::endl;
    } else {
        std::cout << "\n❌ Integration tests failed!" << std::endl;
        std::cout << "Please review and fix failing tests before proceeding." << std::endl;
    }

    return result;
}

// Example usage instructions:
/*
To compile and run tests:

1. With CMake:
   mkdir build && cd build
   cmake .. -DBUILD_TESTING=ON
   make
   ./tests/TestRunner

2. With Visual Studio:
   - Open solution in Visual Studio
   - Build the test project
   - Run tests via Test Explorer or command line

3. Manual compilation (Linux/macOS):
   g++ -std=c++17 -I./src -I./external/sqlite \
       tests/TestRunner.cpp \
       tests/Integration/RepositoryIntegrationTests.cpp \
       tests/Integration/BusinessLogicIntegrationTests.cpp \
       src/Core/Database/*.cpp \
       src/Core/Database/Repositories/*.cpp \
       external/sqlite/sqlite3.c \
       -lgtest -lgtest_main -lpthread \
       -o TestRunner

   ./TestRunner

Expected output for successful run:
- All repository CRUD operations working
- Business logic validation passing
- Performance within acceptable limits
- No memory leaks or data corruption
- Clean error handling for edge cases

Performance targets:
- Individual tests: < 100ms average
- Total test suite: < 30 seconds
- Memory usage: < 100MB peak
- Database operations: > 1000 ops/sec
*/