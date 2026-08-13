// BehavioralTestFramework.h
// Behavioral Testing Framework for Deep Tree Echo
// Implements test scenarios, metrics collection, and validation for cognitive systems

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BehavioralTestFramework.generated.h"

// Forward declarations
class UEchobeatsStreamEngine;
class UPredictiveAdaptationEngine;
class UAdvancedEmotionBlending;
class UTensionalCouplingDynamics;
class USensoryInputIntegration;

/**
 * Test scenario type
 */
UENUM(BlueprintType)
enum class ETestScenarioType : uint8
{
    CognitiveLoop       UMETA(DisplayName = "Cognitive Loop"),
    EmotionalResponse   UMETA(DisplayName = "Emotional Response"),
    SensoryProcessing   UMETA(DisplayName = "Sensory Processing"),
    LearningAdaptation  UMETA(DisplayName = "Learning Adaptation"),
    StreamCoherence     UMETA(DisplayName = "Stream Coherence"),
    Sys6Synchronization UMETA(DisplayName = "Sys6 Synchronization"),
    StressTest          UMETA(DisplayName = "Stress Test"),
    Integration         UMETA(DisplayName = "Integration Test")
};

/**
 * Test result status
 */
UENUM(BlueprintType)
enum class ETestResultStatus : uint8
{
    NotRun      UMETA(DisplayName = "Not Run"),
    Running     UMETA(DisplayName = "Running"),
    Passed      UMETA(DisplayName = "Passed"),
    Failed      UMETA(DisplayName = "Failed"),
    Warning     UMETA(DisplayName = "Warning"),
    Skipped     UMETA(DisplayName = "Skipped"),
    Error       UMETA(DisplayName = "Error")
};

/**
 * Metric type
 */
UENUM(BlueprintType)
enum class EMetricType : uint8
{
    Latency         UMETA(DisplayName = "Latency"),
    Throughput      UMETA(DisplayName = "Throughput"),
    Accuracy        UMETA(DisplayName = "Accuracy"),
    Coherence       UMETA(DisplayName = "Coherence"),
    Stability       UMETA(DisplayName = "Stability"),
    Responsiveness  UMETA(DisplayName = "Responsiveness"),
    MemoryUsage     UMETA(DisplayName = "Memory Usage"),
    FreeEnergy      UMETA(DisplayName = "Free Energy")
};

/**
 * Test assertion
 */
USTRUCT(BlueprintType)
struct FTestAssertion
{
    GENERATED_BODY()

    /** Assertion name */
    UPROPERTY(BlueprintReadOnly)
    FString Name;

    /** Expected value description */
    UPROPERTY(BlueprintReadOnly)
    FString Expected;

    /** Actual value description */
    UPROPERTY(BlueprintReadOnly)
    FString Actual;

    /** Passed */
    UPROPERTY(BlueprintReadOnly)
    bool bPassed = false;

    /** Timestamp */
    UPROPERTY(BlueprintReadOnly)
    float Timestamp = 0.0f;
};

/**
 * Test metric sample
 */
USTRUCT(BlueprintType)
struct FTestMetricSample
{
    GENERATED_BODY()

    /** Metric type */
    UPROPERTY(BlueprintReadOnly)
    EMetricType Type = EMetricType::Latency;

    /** Metric name */
    UPROPERTY(BlueprintReadOnly)
    FString Name;

    /** Value */
    UPROPERTY(BlueprintReadOnly)
    float Value = 0.0f;

    /** Unit */
    UPROPERTY(BlueprintReadOnly)
    FString Unit;

    /** Timestamp */
    UPROPERTY(BlueprintReadOnly)
    float Timestamp = 0.0f;
};

/**
 * Test scenario definition
 */
USTRUCT(BlueprintType)
struct FTestScenario
{
    GENERATED_BODY()

    /** Scenario ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ScenarioID;

    /** Scenario name */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Name;

    /** Description */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Description;

    /** Scenario type */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ETestScenarioType Type = ETestScenarioType::CognitiveLoop;

    /** Duration (seconds, 0 = until complete) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Duration = 10.0f;

    /** Input parameters */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FString, FString> Parameters;

    /** Expected outcomes */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> ExpectedOutcomes;

    /** Metric thresholds */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FString, float> MetricThresholds;

    /** Is enabled */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnabled = true;
};

/**
 * Test result
 */
USTRUCT(BlueprintType)
struct FTestResult
{
    GENERATED_BODY()

    /** Scenario ID */
    UPROPERTY(BlueprintReadOnly)
    FString ScenarioID;

    /** Status */
    UPROPERTY(BlueprintReadOnly)
    ETestResultStatus Status = ETestResultStatus::NotRun;

    /** Start time */
    UPROPERTY(BlueprintReadOnly)
    float StartTime = 0.0f;

    /** End time */
    UPROPERTY(BlueprintReadOnly)
    float EndTime = 0.0f;

    /** Duration */
    UPROPERTY(BlueprintReadOnly)
    float Duration = 0.0f;

    /** Assertions */
    UPROPERTY(BlueprintReadOnly)
    TArray<FTestAssertion> Assertions;

    /** Metrics collected */
    UPROPERTY(BlueprintReadOnly)
    TArray<FTestMetricSample> Metrics;

    /** Error message (if failed) */
    UPROPERTY(BlueprintReadOnly)
    FString ErrorMessage;

    /** Warnings */
    UPROPERTY(BlueprintReadOnly)
    TArray<FString> Warnings;

    /** Pass rate (0-1) */
    UPROPERTY(BlueprintReadOnly)
    float PassRate = 0.0f;
};

/**
 * Test suite summary
 */
USTRUCT(BlueprintType)
struct FTestSuiteSummary
{
    GENERATED_BODY()

    /** Suite name */
    UPROPERTY(BlueprintReadOnly)
    FString SuiteName;

    /** Total scenarios */
    UPROPERTY(BlueprintReadOnly)
    int32 TotalScenarios = 0;

    /** Passed */
    UPROPERTY(BlueprintReadOnly)
    int32 Passed = 0;

    /** Failed */
    UPROPERTY(BlueprintReadOnly)
    int32 Failed = 0;

    /** Warnings */
    UPROPERTY(BlueprintReadOnly)
    int32 Warnings = 0;

    /** Skipped */
    UPROPERTY(BlueprintReadOnly)
    int32 Skipped = 0;

    /** Errors */
    UPROPERTY(BlueprintReadOnly)
    int32 Errors = 0;

    /** Total duration */
    UPROPERTY(BlueprintReadOnly)
    float TotalDuration = 0.0f;

    /** Overall pass rate */
    UPROPERTY(BlueprintReadOnly)
    float OverallPassRate = 0.0f;

    /** Timestamp */
    UPROPERTY(BlueprintReadOnly)
    FDateTime Timestamp;
};

/**
 * Delegate declarations
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTestStarted, const FTestScenario&, Scenario);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTestCompleted, const FTestResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAssertionResult, const FTestAssertion&, Assertion);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMetricCollected, const FTestMetricSample&, Metric);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSuiteCompleted, const FTestSuiteSummary&, Summary);

/**
 * Behavioral Test Framework
 * 
 * Provides comprehensive testing capabilities for Deep Tree Echo cognitive systems.
 * 
 * Key features:
 * - Predefined test scenarios for cognitive loops, emotions, learning
 * - Custom scenario creation and configuration
 * - Real-time metric collection and analysis
 * - Assertion-based validation
 * - Stress testing and integration testing
 * - Sys6 synchronization validation
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UBehavioralTestFramework : public UActorComponent
{
    GENERATED_BODY()

public:
    UBehavioralTestFramework();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Enable verbose logging */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    bool bVerboseLogging = false;

    /** Auto-run tests on begin play */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    bool bAutoRunTests = false;

    /** Stop on first failure */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    bool bStopOnFirstFailure = false;

    /** Metric sampling interval (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    float MetricSamplingInterval = 0.1f;

    /** Default test timeout (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    float DefaultTimeout = 30.0f;

    // ========================================
    // EVENTS
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnTestStarted OnTestStarted;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnTestCompleted OnTestCompleted;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnAssertionResult OnAssertionResult;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnMetricCollected OnMetricCollected;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnSuiteCompleted OnSuiteCompleted;

    // ========================================
    // PUBLIC API - SCENARIO MANAGEMENT
    // ========================================

    /** Register a test scenario */
    UFUNCTION(BlueprintCallable, Category = "Scenarios")
    void RegisterScenario(const FTestScenario& Scenario);

    /** Unregister a scenario */
    UFUNCTION(BlueprintCallable, Category = "Scenarios")
    void UnregisterScenario(const FString& ScenarioID);

    /** Get scenario by ID */
    UFUNCTION(BlueprintPure, Category = "Scenarios")
    FTestScenario GetScenario(const FString& ScenarioID) const;

    /** Get all scenarios */
    UFUNCTION(BlueprintPure, Category = "Scenarios")
    TArray<FTestScenario> GetAllScenarios() const;

    /** Get scenarios by type */
    UFUNCTION(BlueprintPure, Category = "Scenarios")
    TArray<FTestScenario> GetScenariosByType(ETestScenarioType Type) const;

    /** Enable/disable scenario */
    UFUNCTION(BlueprintCallable, Category = "Scenarios")
    void SetScenarioEnabled(const FString& ScenarioID, bool bEnabled);

    /** Create default scenarios */
    UFUNCTION(BlueprintCallable, Category = "Scenarios")
    void CreateDefaultScenarios();

    // ========================================
    // PUBLIC API - TEST EXECUTION
    // ========================================

    /** Run a single scenario */
    UFUNCTION(BlueprintCallable, Category = "Execution")
    void RunScenario(const FString& ScenarioID);

    /** Run all enabled scenarios */
    UFUNCTION(BlueprintCallable, Category = "Execution")
    void RunAllScenarios();

    /** Run scenarios by type */
    UFUNCTION(BlueprintCallable, Category = "Execution")
    void RunScenariosByType(ETestScenarioType Type);

    /** Stop current test */
    UFUNCTION(BlueprintCallable, Category = "Execution")
    void StopCurrentTest();

    /** Stop all tests */
    UFUNCTION(BlueprintCallable, Category = "Execution")
    void StopAllTests();

    /** Is test running */
    UFUNCTION(BlueprintPure, Category = "Execution")
    bool IsTestRunning() const;

    /** Get current scenario ID */
    UFUNCTION(BlueprintPure, Category = "Execution")
    FString GetCurrentScenarioID() const;

    // ========================================
    // PUBLIC API - ASSERTIONS
    // ========================================

    /** Assert equality */
    UFUNCTION(BlueprintCallable, Category = "Assertions")
    bool AssertEqual(const FString& Name, float Expected, float Actual, float Tolerance = 0.001f);

    /** Assert greater than */
    UFUNCTION(BlueprintCallable, Category = "Assertions")
    bool AssertGreaterThan(const FString& Name, float Value, float Threshold);

    /** Assert less than */
    UFUNCTION(BlueprintCallable, Category = "Assertions")
    bool AssertLessThan(const FString& Name, float Value, float Threshold);

    /** Assert in range */
    UFUNCTION(BlueprintCallable, Category = "Assertions")
    bool AssertInRange(const FString& Name, float Value, float Min, float Max);

    /** Assert true */
    UFUNCTION(BlueprintCallable, Category = "Assertions")
    bool AssertTrue(const FString& Name, bool Condition);

    /** Assert false */
    UFUNCTION(BlueprintCallable, Category = "Assertions")
    bool AssertFalse(const FString& Name, bool Condition);

    /** Assert not null */
    UFUNCTION(BlueprintCallable, Category = "Assertions")
    bool AssertNotNull(const FString& Name, UObject* Object);

    /** Custom assertion */
    UFUNCTION(BlueprintCallable, Category = "Assertions")
    void RecordAssertion(const FString& Name, const FString& Expected, const FString& Actual, bool bPassed);

    // ========================================
    // PUBLIC API - METRICS
    // ========================================

    /** Record metric */
    UFUNCTION(BlueprintCallable, Category = "Metrics")
    void RecordMetric(EMetricType Type, const FString& Name, float Value, const FString& Unit);

    /** Get metrics for current test */
    UFUNCTION(BlueprintPure, Category = "Metrics")
    TArray<FTestMetricSample> GetCurrentMetrics() const;

    /** Get metrics by type */
    UFUNCTION(BlueprintPure, Category = "Metrics")
    TArray<FTestMetricSample> GetMetricsByType(EMetricType Type) const;

    /** Get metric average */
    UFUNCTION(BlueprintPure, Category = "Metrics")
    float GetMetricAverage(const FString& Name) const;

    /** Get metric min/max */
    UFUNCTION(BlueprintPure, Category = "Metrics")
    void GetMetricMinMax(const FString& Name, float& OutMin, float& OutMax) const;

    // ========================================
    // PUBLIC API - RESULTS
    // ========================================

    /** Get result for scenario */
    UFUNCTION(BlueprintPure, Category = "Results")
    FTestResult GetResult(const FString& ScenarioID) const;

    /** Get all results */
    UFUNCTION(BlueprintPure, Category = "Results")
    TArray<FTestResult> GetAllResults() const;

    /** Get suite summary */
    UFUNCTION(BlueprintPure, Category = "Results")
    FTestSuiteSummary GetSuiteSummary() const;

    /** Clear results */
    UFUNCTION(BlueprintCallable, Category = "Results")
    void ClearResults();

    /** Export results to string */
    UFUNCTION(BlueprintPure, Category = "Results")
    FString ExportResultsToString() const;

    // ========================================
    // PUBLIC API - SPECIFIC TESTS
    // ========================================

    /** Test cognitive loop timing */
    UFUNCTION(BlueprintCallable, Category = "SpecificTests")
    void TestCognitiveLoopTiming();

    /** Test emotional response */
    UFUNCTION(BlueprintCallable, Category = "SpecificTests")
    void TestEmotionalResponse();

    /** Test stream coherence */
    UFUNCTION(BlueprintCallable, Category = "SpecificTests")
    void TestStreamCoherence();

    /** Test sys6 synchronization */
    UFUNCTION(BlueprintCallable, Category = "SpecificTests")
    void TestSys6Synchronization();

    /** Test learning adaptation */
    UFUNCTION(BlueprintCallable, Category = "SpecificTests")
    void TestLearningAdaptation();

    /** Run stress test */
    UFUNCTION(BlueprintCallable, Category = "SpecificTests")
    void RunStressTest(float Duration, float Intensity);

protected:
    // Component references
    UPROPERTY()
    UEchobeatsStreamEngine* EchobeatsEngine;

    UPROPERTY()
    UPredictiveAdaptationEngine* AdaptationEngine;

    UPROPERTY()
    UAdvancedEmotionBlending* EmotionBlending;

    UPROPERTY()
    UTensionalCouplingDynamics* CouplingDynamics;

    UPROPERTY()
    USensoryInputIntegration* SensoryIntegration;

    // Internal state
    TMap<FString, FTestScenario> RegisteredScenarios;
    TMap<FString, FTestResult> TestResults;
    FTestResult CurrentResult;
    FString CurrentScenarioID;
    bool bIsRunning = false;
    float TestStartTime = 0.0f;
    float LastMetricSampleTime = 0.0f;
    TArray<FString> ScenarioQueue;

    // Internal methods
    void FindComponentReferences();
    void ExecuteScenario(const FTestScenario& Scenario);
    void UpdateCurrentTest(float DeltaTime);
    void FinalizeCurrentTest(ETestResultStatus Status, const FString& ErrorMessage = TEXT(""));
    void CollectMetrics();
    void ProcessNextScenario();
    float ComputePassRate() const;

    // Scenario execution methods
    void ExecuteCognitiveLoopTest(const FTestScenario& Scenario);
    void ExecuteEmotionalResponseTest(const FTestScenario& Scenario);
    void ExecuteSensoryProcessingTest(const FTestScenario& Scenario);
    void ExecuteLearningAdaptationTest(const FTestScenario& Scenario);
    void ExecuteStreamCoherenceTest(const FTestScenario& Scenario);
    void ExecuteSys6SynchronizationTest(const FTestScenario& Scenario);
    void ExecuteStressTest(const FTestScenario& Scenario);
    void ExecuteIntegrationTest(const FTestScenario& Scenario);
};
