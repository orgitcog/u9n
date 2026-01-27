#pragma once

/**
 * Deep Tree Echo Reservoir Computing Integration
 * 
 * Integrates Echo State Networks with Deep Tree Echo cognitive architecture.
 * Implements hierarchical reservoir computing for temporal pattern recognition,
 * memory consolidation, and echo propagation.
 * 
 * Based on:
 * - Feature F1.2.1: Echo State Network Core Implementation
 * - Deep Tree Echo: Hierarchical membrane-based cognitive architecture
 * - 4E Cognition: Embodied, Embedded, Enacted, Extended
 * 
 * Architecture:
 * - 3 concurrent consciousness streams (Perceiving, Acting, Reflecting)
 * - 12-step cognitive loop with triadic synchronization
 * - Nested membrane execution contexts (OEIS A000081)
 */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EchoStateNetwork.h"
#include "DeepTreeEchoReservoir.generated.h"

// Forward declarations for ReservoirCpp integration
namespace ReservoirCpp {
    class Reservoir;
    class Ridge;
    class ESN;
    class Model;
}

/**
 * Reservoir State - Internal state of an Echo State Network reservoir
 */
USTRUCT(BlueprintType)
struct FReservoirState
{
    GENERATED_BODY()

    /** Reservoir ID */
    UPROPERTY(BlueprintReadWrite)
    FString ReservoirID;

    /** Number of units in reservoir */
    UPROPERTY(BlueprintReadWrite)
    int32 Units = 100;

    /** Spectral radius (controls echo memory) */
    UPROPERTY(BlueprintReadWrite)
    float SpectralRadius = 0.9f;

    /** Leak rate (controls temporal dynamics) */
    UPROPERTY(BlueprintReadWrite)
    float LeakRate = 0.3f;

    /** Input scaling */
    UPROPERTY(BlueprintReadWrite)
    float InputScaling = 1.0f;

    /** Current activation state (flattened) */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> ActivationState;

    /** Reservoir is initialized */
    UPROPERTY(BlueprintReadWrite)
    bool bIsInitialized = false;

    /** Last update timestamp */
    UPROPERTY(BlueprintReadWrite)
    float LastUpdateTime = 0.0f;
};

/**
 * Echo Propagation Config - Configuration for echo spreading
 */
USTRUCT(BlueprintType)
struct FEchoPropagationConfig
{
    GENERATED_BODY()

    /** Enable hierarchical echo propagation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableHierarchicalEcho = true;

    /** Number of hierarchical levels */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 HierarchyLevels = 3;

    /** Echo decay factor per level */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EchoDecayFactor = 0.8f;

    /** Cross-reservoir coupling strength */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CrossCouplingStrength = 0.1f;

    /** Intrinsic plasticity enabled */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableIntrinsicPlasticity = true;

    /** Intrinsic plasticity learning rate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float IntrinsicPlasticityRate = 0.001f;
};

/**
 * Temporal Pattern - Detected temporal pattern in reservoir dynamics
 */
USTRUCT(BlueprintType)
struct FTemporalPattern
{
    GENERATED_BODY()

    /** Pattern ID */
    UPROPERTY(BlueprintReadWrite)
    FString PatternID;

    /** Pattern type (periodic, chaotic, transient) */
    UPROPERTY(BlueprintReadWrite)
    FString PatternType;

    /** Pattern frequency (Hz) */
    UPROPERTY(BlueprintReadWrite)
    float Frequency = 0.0f;

    /** Pattern strength (0.0 - 1.0) */
    UPROPERTY(BlueprintReadWrite)
    float Strength = 0.0f;

    /** Associated memory nodes */
    UPROPERTY(BlueprintReadWrite)
    TArray<FString> AssociatedMemoryNodes;

    /** Pattern signature (characteristic activation) */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> Signature;
};

/**
 * Cognitive Stream State - State of one consciousness stream
 */
USTRUCT(BlueprintType)
struct FCognitiveStreamState
{
    GENERATED_BODY()

    /** Stream ID (1=Perceiving, 2=Acting, 3=Reflecting) */
    UPROPERTY(BlueprintReadWrite)
    int32 StreamID = 1;

    /** Current phase in 12-step cycle (1-12) */
    UPROPERTY(BlueprintReadWrite)
    int32 CurrentPhase = 1;

    /** Reservoir state for this stream (legacy) */
    UPROPERTY(BlueprintReadWrite)
    FReservoirState ReservoirState;

    /** ESN for this stream (new implementation) */
    UPROPERTY()
    UEchoStateNetwork* ESN = nullptr;

    /** Current attention focus */
    UPROPERTY(BlueprintReadWrite)
    FString AttentionFocus;

    /** Activation level (0.0 - 1.0) */
    UPROPERTY(BlueprintReadWrite)
    float ActivationLevel = 0.5f;

    /** Coherence with other streams */
    UPROPERTY(BlueprintReadWrite)
    float StreamCoherence = 0.5f;
};

/**
 * Deep Tree Echo Reservoir Component
 * Integrates Echo State Networks with cognitive architecture
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UDeepTreeEchoReservoir : public UActorComponent
{
    GENERATED_BODY()

public:
    UDeepTreeEchoReservoir();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Enable reservoir computing */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reservoir|Config")
    bool bEnableReservoirComputing = true;

    /** Echo propagation configuration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reservoir|Config")
    FEchoPropagationConfig EchoPropagationConfig;

    /** 12-step cognitive cycle duration (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reservoir|Config", meta = (ClampMin = "1.0", ClampMax = "60.0"))
    float CycleDuration = 12.0f;

    // ========================================
    // RESERVOIR STATE
    // ========================================

    /** Hierarchical reservoirs (one per level) */
    UPROPERTY(BlueprintReadOnly, Category = "Reservoir|State")
    TArray<FReservoirState> HierarchicalReservoirs;

    /** Cognitive stream states (3 streams) */
    UPROPERTY(BlueprintReadOnly, Category = "Reservoir|State")
    TArray<FCognitiveStreamState> CognitiveStreams;

    /** Detected temporal patterns */
    UPROPERTY(BlueprintReadOnly, Category = "Reservoir|State")
    TArray<FTemporalPattern> DetectedPatterns;

    /** Current cycle step (1-12) */
    UPROPERTY(BlueprintReadOnly, Category = "Reservoir|State")
    int32 CurrentCycleStep = 1;

    // ========================================
    // PUBLIC API - RESERVOIR OPERATIONS
    // ========================================

    /** Initialize hierarchical reservoirs */
    UFUNCTION(BlueprintCallable, Category = "Reservoir")
    void InitializeReservoirs(int32 NumLevels = 3, int32 UnitsPerLevel = 100);

    /** Process input through reservoir hierarchy */
    UFUNCTION(BlueprintCallable, Category = "Reservoir")
    TArray<float> ProcessInput(const TArray<float>& Input, int32 StreamID = 1);

    /** Get current reservoir state */
    UFUNCTION(BlueprintCallable, Category = "Reservoir")
    FReservoirState GetReservoirState(int32 Level = 0) const;

    /** Detect temporal patterns in reservoir dynamics */
    UFUNCTION(BlueprintCallable, Category = "Reservoir")
    TArray<FTemporalPattern> DetectTemporalPatterns();

    /** Propagate echo through hierarchy */
    UFUNCTION(BlueprintCallable, Category = "Reservoir")
    void PropagateEcho(const TArray<float>& Activation, int32 SourceLevel = 0);

    // ========================================
    // PUBLIC API - COGNITIVE STREAM OPERATIONS
    // ========================================

    /** Update cognitive stream state */
    UFUNCTION(BlueprintCallable, Category = "Reservoir|Cognitive")
    void UpdateCognitiveStream(int32 StreamID, const TArray<float>& Input);

    /** Get stream state */
    UFUNCTION(BlueprintCallable, Category = "Reservoir|Cognitive")
    FCognitiveStreamState GetStreamState(int32 StreamID) const;

    /** Synchronize streams at triadic point */
    UFUNCTION(BlueprintCallable, Category = "Reservoir|Cognitive")
    void SynchronizeStreams();

    /** Check if current step is triadic synchronization point */
    UFUNCTION(BlueprintCallable, Category = "Reservoir|Cognitive")
    bool IsTriadicSyncPoint() const;

    // ========================================
    // PUBLIC API - MEMORY INTEGRATION
    // ========================================

    /** Associate pattern with memory node */
    UFUNCTION(BlueprintCallable, Category = "Reservoir|Memory")
    void AssociatePatternWithMemory(const FString& PatternID, const FString& MemoryNodeID);

    /** Retrieve patterns associated with memory */
    UFUNCTION(BlueprintCallable, Category = "Reservoir|Memory")
    TArray<FTemporalPattern> GetPatternsForMemory(const FString& MemoryNodeID) const;

    /** Consolidate short-term patterns to long-term */
    UFUNCTION(BlueprintCallable, Category = "Reservoir|Memory")
    void ConsolidatePatterns();

protected:
    virtual void BeginPlay() override;

private:
    // ========================================
    // INTERNAL STATE
    // ========================================

    /** Cycle timer */
    float CycleTimer = 0.0f;

    /** Pattern ID counter */
    int32 PatternIDCounter = 0;

    /** Reservoir ID counter */
    int32 ReservoirIDCounter = 0;

    // ========================================
    // INTERNAL METHODS
    // ========================================

    /** Initialize single reservoir */
    FReservoirState CreateReservoir(int32 Units, float SpectralRadius, float LeakRate);

    /** Update cycle step */
    void UpdateCycleStep(float DeltaTime);

    /** Apply intrinsic plasticity */
    void ApplyIntrinsicPlasticity(FReservoirState& Reservoir, const TArray<float>& Input);

    /** Compute cross-reservoir coupling */
    TArray<float> ComputeCrossCoupling(int32 SourceLevel, int32 TargetLevel);

    /** Generate pattern ID */
    FString GeneratePatternID();

    /** Generate reservoir ID */
    FString GenerateReservoirID();

    /** Compute stream coherence */
    float ComputeStreamCoherence(int32 StreamID) const;
};
