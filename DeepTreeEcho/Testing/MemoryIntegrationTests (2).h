// MemoryIntegrationTests.h
// Unit and integration tests for Memory-Consciousness Integration System
// Tests hypergraph memory, consciousness stream bindings, and cross-stream associations

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BehavioralTestFramework.h"
#include "../Memory/HypergraphMemorySystem.h"
#include "../Memory/MemoryConsciousnessIntegration.h"
#include "MemoryIntegrationTests.generated.h"

/**
 * Memory test category
 */
UENUM(BlueprintType)
enum class EMemoryTestCategory : uint8
{
    HypergraphBasic     UMETA(DisplayName = "Hypergraph Basic Operations"),
    NodeOperations      UMETA(DisplayName = "Node CRUD Operations"),
    EdgeOperations      UMETA(DisplayName = "Edge Operations"),
    SpreadingActivation UMETA(DisplayName = "Spreading Activation"),
    IntentionalMemory   UMETA(DisplayName = "BDI Intentional Memory"),
    ConsciousnessBinding UMETA(DisplayName = "Consciousness Stream Binding"),
    WorkingMemory       UMETA(DisplayName = "Working Memory Management"),
    CrossStreamAssoc    UMETA(DisplayName = "Cross-Stream Associations"),
    CycleIntegration    UMETA(DisplayName = "Cognitive Cycle Integration"),
    PerformanceStress   UMETA(DisplayName = "Performance & Stress Tests")
};

/**
 * Memory test result
 */
USTRUCT(BlueprintType)
struct FMemoryTestResult
{
    GENERATED_BODY()

    /** Test name */
    UPROPERTY(BlueprintReadOnly)
    FString TestName;

    /** Category */
    UPROPERTY(BlueprintReadOnly)
    EMemoryTestCategory Category = EMemoryTestCategory::HypergraphBasic;

    /** Passed */
    UPROPERTY(BlueprintReadOnly)
    bool bPassed = false;

    /** Duration (ms) */
    UPROPERTY(BlueprintReadOnly)
    float DurationMs = 0.0f;

    /** Assertions passed */
    UPROPERTY(BlueprintReadOnly)
    int32 AssertionsPassed = 0;

    /** Assertions failed */
    UPROPERTY(BlueprintReadOnly)
    int32 AssertionsFailed = 0;

    /** Error message */
    UPROPERTY(BlueprintReadOnly)
    FString ErrorMessage;

    /** Details */
    UPROPERTY(BlueprintReadOnly)
    TArray<FString> Details;
};

/**
 * Memory test summary
 */
USTRUCT(BlueprintType)
struct FMemoryTestSummary
{
    GENERATED_BODY()

    /** Total tests */
    UPROPERTY(BlueprintReadOnly)
    int32 TotalTests = 0;

    /** Passed */
    UPROPERTY(BlueprintReadOnly)
    int32 Passed = 0;

    /** Failed */
    UPROPERTY(BlueprintReadOnly)
    int32 Failed = 0;

    /** Total duration (ms) */
    UPROPERTY(BlueprintReadOnly)
    float TotalDurationMs = 0.0f;

    /** Pass rate */
    UPROPERTY(BlueprintReadOnly)
    float PassRate = 0.0f;

    /** Results by category */
    UPROPERTY(BlueprintReadOnly)
    TMap<EMemoryTestCategory, int32> PassedByCategory;

    UPROPERTY(BlueprintReadOnly)
    TMap<EMemoryTestCategory, int32> FailedByCategory;
};

/**
 * Memory Integration Tests Component
 *
 * Provides comprehensive testing for the Memory-Consciousness integration:
 *
 * - Hypergraph basic operations (create, read, update, delete)
 * - Node and edge management
 * - Spreading activation
 * - BDI intentional memory (beliefs, desires, intentions)
 * - Consciousness stream binding
 * - Working memory management
 * - Cross-stream associations
 * - Cognitive cycle integration
 * - Performance and stress tests
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UMemoryIntegrationTests : public UActorComponent
{
    GENERATED_BODY()

public:
    UMemoryIntegrationTests();

protected:
    virtual void BeginPlay() override;

public:
    // ========================================
    // CONFIGURATION
    // ========================================

    /** Auto-run tests on begin play */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    bool bAutoRunTests = false;

    /** Verbose logging */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    bool bVerboseLogging = true;

    /** Stress test node count */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    int32 StressTestNodeCount = 1000;

    // ========================================
    // PUBLIC API - TEST EXECUTION
    // ========================================

    /** Run all memory tests */
    UFUNCTION(BlueprintCallable, Category = "Tests")
    FMemoryTestSummary RunAllTests();

    /** Run tests by category */
    UFUNCTION(BlueprintCallable, Category = "Tests")
    TArray<FMemoryTestResult> RunTestsByCategory(EMemoryTestCategory Category);

    /** Run specific test */
    UFUNCTION(BlueprintCallable, Category = "Tests")
    FMemoryTestResult RunTest(const FString& TestName);

    /** Get test summary */
    UFUNCTION(BlueprintPure, Category = "Tests")
    FMemoryTestSummary GetTestSummary() const;

    /** Get all test results */
    UFUNCTION(BlueprintPure, Category = "Tests")
    TArray<FMemoryTestResult> GetAllResults() const;

    /** Clear results */
    UFUNCTION(BlueprintCallable, Category = "Tests")
    void ClearResults();

    // ========================================
    // HYPERGRAPH BASIC TESTS
    // ========================================

    /** Test node creation */
    UFUNCTION(BlueprintCallable, Category = "Tests|Hypergraph")
    FMemoryTestResult Test_NodeCreation();

    /** Test node retrieval */
    UFUNCTION(BlueprintCallable, Category = "Tests|Hypergraph")
    FMemoryTestResult Test_NodeRetrieval();

    /** Test node update */
    UFUNCTION(BlueprintCallable, Category = "Tests|Hypergraph")
    FMemoryTestResult Test_NodeUpdate();

    /** Test node deletion */
    UFUNCTION(BlueprintCallable, Category = "Tests|Hypergraph")
    FMemoryTestResult Test_NodeDeletion();

    /** Test edge creation */
    UFUNCTION(BlueprintCallable, Category = "Tests|Hypergraph")
    FMemoryTestResult Test_EdgeCreation();

    /** Test bidirectional edges */
    UFUNCTION(BlueprintCallable, Category = "Tests|Hypergraph")
    FMemoryTestResult Test_BidirectionalEdges();

    /** Test hyperedge creation */
    UFUNCTION(BlueprintCallable, Category = "Tests|Hypergraph")
    FMemoryTestResult Test_HyperedgeCreation();

    // ========================================
    // SPREADING ACTIVATION TESTS
    // ========================================

    /** Test basic activation spreading */
    UFUNCTION(BlueprintCallable, Category = "Tests|Activation")
    FMemoryTestResult Test_BasicActivationSpreading();

    /** Test activation decay */
    UFUNCTION(BlueprintCallable, Category = "Tests|Activation")
    FMemoryTestResult Test_ActivationDecay();

    /** Test multi-source activation */
    UFUNCTION(BlueprintCallable, Category = "Tests|Activation")
    FMemoryTestResult Test_MultiSourceActivation();

    // ========================================
    // INTENTIONAL MEMORY (BDI) TESTS
    // ========================================

    /** Test belief creation */
    UFUNCTION(BlueprintCallable, Category = "Tests|BDI")
    FMemoryTestResult Test_BeliefCreation();

    /** Test desire creation */
    UFUNCTION(BlueprintCallable, Category = "Tests|BDI")
    FMemoryTestResult Test_DesireCreation();

    /** Test intention creation and status */
    UFUNCTION(BlueprintCallable, Category = "Tests|BDI")
    FMemoryTestResult Test_IntentionLifecycle();

    /** Test belief contradiction detection */
    UFUNCTION(BlueprintCallable, Category = "Tests|BDI")
    FMemoryTestResult Test_BeliefContradictions();

    // ========================================
    // CONSCIOUSNESS BINDING TESTS
    // ========================================

    /** Test stream binding */
    UFUNCTION(BlueprintCallable, Category = "Tests|Consciousness")
    FMemoryTestResult Test_StreamBinding();

    /** Test working memory loading */
    UFUNCTION(BlueprintCallable, Category = "Tests|Consciousness")
    FMemoryTestResult Test_WorkingMemoryLoading();

    /** Test working memory capacity */
    UFUNCTION(BlueprintCallable, Category = "Tests|Consciousness")
    FMemoryTestResult Test_WorkingMemoryCapacity();

    /** Test working memory decay */
    UFUNCTION(BlueprintCallable, Category = "Tests|Consciousness")
    FMemoryTestResult Test_WorkingMemoryDecay();

    /** Test memory-guided attention */
    UFUNCTION(BlueprintCallable, Category = "Tests|Consciousness")
    FMemoryTestResult Test_MemoryGuidedAttention();

    // ========================================
    // CROSS-STREAM ASSOCIATION TESTS
    // ========================================

    /** Test cross-stream association creation */
    UFUNCTION(BlueprintCallable, Category = "Tests|CrossStream")
    FMemoryTestResult Test_CrossStreamAssociationCreation();

    /** Test cross-stream activation propagation */
    UFUNCTION(BlueprintCallable, Category = "Tests|CrossStream")
    FMemoryTestResult Test_CrossStreamActivationPropagation();

    /** Test triadic synchronization */
    UFUNCTION(BlueprintCallable, Category = "Tests|CrossStream")
    FMemoryTestResult Test_TriadicSynchronization();

    // ========================================
    // CYCLE INTEGRATION TESTS
    // ========================================

    /** Test cycle step processing */
    UFUNCTION(BlueprintCallable, Category = "Tests|Cycle")
    FMemoryTestResult Test_CycleStepProcessing();

    /** Test consolidation at cycle end */
    UFUNCTION(BlueprintCallable, Category = "Tests|Cycle")
    FMemoryTestResult Test_CycleConsolidation();

    // ========================================
    // PERFORMANCE TESTS
    // ========================================

    /** Test bulk node creation performance */
    UFUNCTION(BlueprintCallable, Category = "Tests|Performance")
    FMemoryTestResult Test_BulkNodeCreationPerformance();

    /** Test similarity search performance */
    UFUNCTION(BlueprintCallable, Category = "Tests|Performance")
    FMemoryTestResult Test_SimilaritySearchPerformance();

    /** Test spreading activation performance */
    UFUNCTION(BlueprintCallable, Category = "Tests|Performance")
    FMemoryTestResult Test_SpreadingActivationPerformance();

    /** Test consolidation performance */
    UFUNCTION(BlueprintCallable, Category = "Tests|Performance")
    FMemoryTestResult Test_ConsolidationPerformance();

protected:
    // Component references
    UPROPERTY()
    UHypergraphMemorySystem* MemorySystem;

    UPROPERTY()
    UMemoryConsciousnessIntegration* ConsciousnessIntegration;

    UPROPERTY()
    UBehavioralTestFramework* TestFramework;

    // Test results
    TArray<FMemoryTestResult> TestResults;

    // Internal test helpers
    void FindComponentReferences();
    void CreateTestComponents();
    void CleanupTestComponents();

    // Assertion helpers
    bool Assert(FMemoryTestResult& Result, const FString& Assertion, bool Condition);
    bool AssertEqual(FMemoryTestResult& Result, const FString& Assertion, float Expected, float Actual, float Tolerance = 0.001f);
    bool AssertEqual(FMemoryTestResult& Result, const FString& Assertion, int32 Expected, int32 Actual);
    bool AssertNotEqual(FMemoryTestResult& Result, const FString& Assertion, int64 NotExpected, int64 Actual);
    bool AssertGreaterThan(FMemoryTestResult& Result, const FString& Assertion, float Value, float Threshold);
    bool AssertLessThan(FMemoryTestResult& Result, const FString& Assertion, float Value, float Threshold);

    // Test result recording
    void StartTest(FMemoryTestResult& Result, const FString& Name, EMemoryTestCategory Category);
    void EndTest(FMemoryTestResult& Result, bool bSuccess, const FString& ErrorMessage = TEXT(""));
    void LogTestResult(const FMemoryTestResult& Result);

    // Utility
    TArray<float> GenerateRandomEmbedding(int32 Dimension) const;
    void ClearMemorySystem();
};
