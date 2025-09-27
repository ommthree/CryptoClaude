#pragma once

#include "../Data/ProductionApiManager.h"
#include "../Data/RealDataPipeline.h"
#include "../ML/RealDataMLPipeline.h"
#include "../Config/ApiConfigurationManager.h"
#include "../Database/DatabaseManager.h"
#include <memory>
#include <vector>
#include <map>
#include <string>
#include <chrono>
#include <functional>

namespace CryptoClaude {
namespace Testing {

/**
 * Comprehensive API Integration Testing Framework for validating
 * end-to-end data flow from API → Database → ML → Signals
 */
class ApiIntegrationTestFramework {
public:
    ApiIntegrationTestFramework(Data::ProductionApiManager& apiManager,
                               Data::RealDataPipeline& dataPipeline,
                               ML::RealDataMLPipeline& mlPipeline,
                               Config::ApiConfigurationManager& configManager,
                               Database::DatabaseManager& dbManager);
    ~ApiIntegrationTestFramework();

    // === TEST SUITE MANAGEMENT ===

    enum class TestSeverity {
        SMOKE,      // Basic connectivity and availability tests
        FUNCTIONAL, // Core functionality tests
        PERFORMANCE,// Performance and throughput tests
        STRESS,     // Stress and load tests
        INTEGRATION // End-to-end integration tests
    };

    struct TestResult {
        std::string testName;
        std::string testCategory;
        TestSeverity severity;
        bool passed;
        double executionTimeMs;
        std::string description;
        std::string expectedResult;
        std::string actualResult;
        std::string errorMessage;
        std::chrono::system_clock::time_point timestamp;
        std::map<std::string, std::string> metadata;
    };

    struct TestSuiteResult {
        std::string suiteName;
        int totalTests;
        int passedTests;
        int failedTests;
        int skippedTests;
        double totalExecutionTimeMs;
        double passRate;
        std::vector<TestResult> testResults;
        std::chrono::system_clock::time_point startTime;
        std::chrono::system_clock::time_point endTime;
        std::string summary;
    };

    // === TEST EXECUTION ===

    // Run all test suites
    bool runAllTests();

    // Run specific test categories
    bool runSmokeTests();
    bool runFunctionalTests();
    bool runPerformanceTests();
    bool runStressTests();
    bool runIntegrationTests();

    // Run individual test suites
    TestSuiteResult runApiConnectivityTests();
    TestSuiteResult runDataIngestionTests();
    TestSuiteResult runMLPipelineTests();
    TestSuiteResult runEndToEndWorkflowTests();
    TestSuiteResult runPerformanceBenchmarkTests();

    // === SPECIFIC TEST IMPLEMENTATIONS ===

    // API Connectivity Tests
    TestResult testApiProviderAvailability();
    TestResult testApiKeyValidation();
    TestResult testApiRateLimiting();
    TestResult testApiFailoverMechanism();
    TestResult testApiHealthMonitoring();

    // Data Ingestion Tests
    TestResult testMarketDataRetrieval();
    TestResult testSentimentDataRetrieval();
    TestResult testDataQualityValidation();
    TestResult testDataStorageIntegrity();
    TestResult testRealTimePipelineFlow();

    // ML Pipeline Tests
    TestResult testFeatureExtraction();
    TestResult testModelTraining();
    TestResult testPredictionGeneration();
    TestResult testSignalGeneration();
    TestResult testMLModelValidation();

    // Integration Tests
    TestResult testCompleteDataFlow();
    TestResult testSystemRecoveryMechanisms();
    TestResult testDataConsistencyAcrossComponents();
    TestResult testConcurrentOperations();

    // Performance Benchmark Tests
    TestResult testThroughputUnderLoad();
    TestResult testLatencyBenchmarks();
    TestResult testResourceUtilization();
    TestResult testScalabilityLimits();

    // === TEST CONFIGURATION ===

    struct TestConfiguration {
        // Test execution settings
        std::chrono::seconds defaultTimeout = std::chrono::seconds(30);
        int maxRetries = 3;
        bool stopOnFirstFailure = false;
        bool enableDetailedLogging = true;
        bool enablePerformanceMetrics = true;

        // Test data settings
        std::vector<std::string> testSymbols = {"BTC", "ETH", "ADA"};
        std::chrono::hours testDataRange = std::chrono::hours(24);
        int maxTestDataPoints = 100;

        // Performance test settings
        int concurrentConnectionsTest = 10;
        int requestsPerSecondTest = 50;
        std::chrono::seconds performanceTestDuration = std::chrono::seconds(60);
        double acceptableLatencyMs = 1000.0;
        double acceptableSuccessRate = 0.95;

        // Integration test settings
        bool enableRealApiCalls = true;
        bool enableDatabaseWrites = true;
        bool enableMLModelUpdates = false; // Usually false for tests
        std::string testDatabasePrefix = "test_";
    };

    void setTestConfiguration(const TestConfiguration& config);
    TestConfiguration getTestConfiguration() const { return config_; }

    // === MOCK AND STUB MANAGEMENT ===

    struct MockApiResponse {
        std::string providerId;
        std::string endpoint;
        int httpStatusCode = 200;
        std::string responseBody;
        std::chrono::milliseconds responseDelay = std::chrono::milliseconds(100);
        bool shouldFail = false;
        std::string failureReason;
    };

    void setupMockResponses(const std::vector<MockApiResponse>& responses);
    void clearMockResponses();
    void enableMockMode(bool enable = true) { mockModeEnabled_ = enable; }

    // === VALIDATION HELPERS ===

    struct ValidationCriteria {
        double minDataQualityScore = 0.90;
        int minDataPoints = 10;
        std::chrono::minutes maxDataAge = std::chrono::minutes(30);
        double minMLAccuracy = 0.55;
        double minMLConfidence = 0.5;
        double maxAcceptableLatency = 2000.0; // milliseconds
        double minSystemSuccessRate = 0.95;
    };

    void setValidationCriteria(const ValidationCriteria& criteria);
    ValidationCriteria getValidationCriteria() const { return validationCriteria_; }

    // Validation methods
    bool validateDataQuality(const std::vector<Database::Models::MarketData>& data);
    bool validateMLPrediction(const ML::MLPrediction& prediction);
    bool validateSystemPerformance(const std::map<std::string, double>& metrics);

    // === TEST REPORTING ===

    // Generate comprehensive test reports
    std::string generateTestReport(const std::vector<TestSuiteResult>& results) const;
    std::string generatePerformanceReport(const TestSuiteResult& performanceResults) const;
    std::string generateSummaryReport() const;

    // Export test results
    bool exportResultsToJson(const std::string& filePath) const;
    bool exportResultsToHtml(const std::string& filePath) const;
    bool exportPerformanceMetrics(const std::string& filePath) const;

    // === CONTINUOUS TESTING SUPPORT ===

    struct ContinuousTestConfig {
        bool enableContinuousTesting = false;
        std::chrono::minutes testInterval = std::chrono::minutes(60);
        std::vector<TestSeverity> enabledTestLevels = {TestSeverity::SMOKE, TestSeverity::FUNCTIONAL};
        int maxFailuresBeforeAlert = 3;
        std::string alertEndpoint;
    };

    void setContinuousTestConfig(const ContinuousTestConfig& config);
    void startContinuousTesting();
    void stopContinuousTesting();

    // === TEST UTILITIES ===

    // Test data generation
    std::vector<Database::Models::MarketData> generateTestMarketData(
        const std::string& symbol, int dataPoints) const;
    std::vector<Database::Models::SentimentData> generateTestSentimentData(
        const std::string& symbol, int dataPoints) const;

    // System state management
    bool setupTestEnvironment();
    bool teardownTestEnvironment();
    bool resetSystemState();

    // Metrics collection
    struct TestMetrics {
        double averageLatency;
        double maxLatency;
        double minLatency;
        double throughputPerSecond;
        double errorRate;
        double resourceUtilization;
        int totalRequests;
        int successfulRequests;
        int failedRequests;
    };

    TestMetrics collectPerformanceMetrics();
    void resetMetrics();

    // === EVENT CALLBACKS ===

    using TestCompletionCallback = std::function<void(const TestResult&)>;
    using SuiteCompletionCallback = std::function<void(const TestSuiteResult&)>;
    using ErrorCallback = std::function<void(const std::string&, const std::string&)>;

    void setTestCompletionCallback(TestCompletionCallback callback) {
        testCompletionCallback_ = callback;
    }
    void setSuiteCompletionCallback(SuiteCompletionCallback callback) {
        suiteCompletionCallback_ = callback;
    }
    void setErrorCallback(ErrorCallback callback) {
        errorCallback_ = callback;
    }

private:
    // Core components
    Data::ProductionApiManager& apiManager_;
    Data::RealDataPipeline& dataPipeline_;
    ML::RealDataMLPipeline& mlPipeline_;
    Config::ApiConfigurationManager& configManager_;
    Database::DatabaseManager& dbManager_;

    // Configuration and state
    TestConfiguration config_;
    ValidationCriteria validationCriteria_;
    ContinuousTestConfig continuousConfig_;

    // Mock and testing state
    bool mockModeEnabled_;
    std::vector<MockApiResponse> mockResponses_;

    // Results storage
    std::vector<TestSuiteResult> allTestResults_;
    TestMetrics currentMetrics_;

    // Continuous testing
    std::unique_ptr<std::thread> continuousTestThread_;
    std::atomic<bool> continuousTestingActive_;

    // Callbacks
    TestCompletionCallback testCompletionCallback_;
    SuiteCompletionCallback suiteCompletionCallback_;
    ErrorCallback errorCallback_;

    // === PRIVATE HELPER METHODS ===

    // Test execution helpers
    TestResult executeTest(const std::string& testName, const std::string& category,
                          TestSeverity severity, std::function<bool()> testFunction,
                          const std::string& description = "");

    TestSuiteResult executeSuite(const std::string& suiteName,
                                const std::vector<std::function<TestResult()>>& tests);

    // Mock system helpers
    bool shouldUseMockResponse(const std::string& providerId, const std::string& endpoint);
    MockApiResponse getMockResponse(const std::string& providerId, const std::string& endpoint);

    // Validation helpers
    bool validateTestPreconditions();
    bool validateComponentHealth();
    bool validateTestData(const std::string& dataType, const void* data, size_t dataSize);

    // Performance monitoring
    void startPerformanceMonitoring();
    void stopPerformanceMonitoring();
    void recordLatency(double latencyMs);
    void recordThroughput(int requestCount, std::chrono::milliseconds duration);

    // Test environment management
    bool setupTestDatabase();
    bool cleanupTestDatabase();
    bool setupTestConfiguration();
    bool restoreOriginalConfiguration();

    // Data generation helpers
    Database::Models::MarketData createTestMarketDataPoint(const std::string& symbol,
                                                          std::chrono::system_clock::time_point timestamp,
                                                          double basePrice) const;

    Database::Models::SentimentData createTestSentimentDataPoint(const std::string& symbol,
                                                               std::chrono::system_clock::time_point timestamp,
                                                               double sentiment) const;

    // Report generation helpers
    std::string formatTestResult(const TestResult& result) const;
    std::string formatTestSummary(const TestSuiteResult& suite) const;
    std::string generateMetricsTable(const TestMetrics& metrics) const;

    // Continuous testing implementation
    void continuousTestingMain();
    void runScheduledTests();
    bool checkTestFailureThreshold();

    // Error handling
    void recordTestError(const std::string& testName, const std::string& error);
    void notifyTestError(const std::string& testName, const std::string& error);

    // Utility functions
    std::string getCurrentTimestamp() const;
    std::string formatDuration(std::chrono::milliseconds duration) const;
    double calculatePassRate(int passed, int total) const;
};

} // namespace Testing
} // namespace CryptoClaude