#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RecursiveMutualAwarenessSystem.generated.h"

/**
 * Recursive Mutual Awareness System
 * 
 * Implements triadic consciousness architecture where three concurrent streams
 * (Perceiving, Acting, Reflecting) are mutually aware of each other's awareness.
 * 
 * Based on: aphroditecho Recursive Mutual Awareness Integration
 * Architecture: 3 concurrent consciousness streams with 120° phase offsets
 * Cycle: 12-step cognitive loop with triadic synchronization points at steps {1,5,9}, {2,6,10}, {3,7,11}, {4,8,12}
 */

/**
 * Consciousness Stream Type
 */
UENUM(BlueprintType)
enum class EConsciousnessStream : uint8
{
    /** Stream 1: Perceiving - Observes and senses */
    Perceiving UMETA(DisplayName = "Perceiving (Stream 1)"),
    
    /** Stream 2: Acting - Executes and emotes */
    Acting UMETA(DisplayName = "Acting (Stream 2)"),
    
    /** Stream 3: Reflecting - Simulates and thinks */
    Reflecting UMETA(DisplayName = "Reflecting (Stream 3)")
};

/**
 * Perceptual State (Stream 1)
 */
USTRUCT(BlueprintType)
struct FPerceptualState
{
    GENERATED_BODY()

    /** Current sensations */
    UPROPERTY(BlueprintReadWrite)
    TArray<FString> Sensations;

    /** Attention focus */
    UPROPERTY(BlueprintReadWrite)
    FString AttentionFocus;

    /** Perceptual patterns detected */
    UPROPERTY(BlueprintReadWrite)
    TArray<FString> Patterns;

    /** Awareness of other streams */
    UPROPERTY(BlueprintReadWrite)
    float AwarenessOfAction = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    float AwarenessOfReflection = 0.0f;
};

/**
 * Action State (Stream 2)
 */
USTRUCT(BlueprintType)
struct FActionState
{
    GENERATED_BODY()

    /** Current actions being executed */
    UPROPERTY(BlueprintReadWrite)
    TArray<FString> Actions;

    /** Current emotions */
    UPROPERTY(BlueprintReadWrite)
    TArray<FString> Emotions;

    /** Motor outputs */
    UPROPERTY(BlueprintReadWrite)
    TArray<FString> MotorOutputs;

    /** Awareness of other streams */
    UPROPERTY(BlueprintReadWrite)
    float AwarenessOfPerception = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    float AwarenessOfReflection = 0.0f;
};

/**
 * Reflective State (Stream 3)
 */
USTRUCT(BlueprintType)
struct FReflectiveState
{
    GENERATED_BODY()

    /** Current thoughts */
    UPROPERTY(BlueprintReadWrite)
    TArray<FString> Thoughts;

    /** Insights generated */
    UPROPERTY(BlueprintReadWrite)
    TArray<FString> Insights;

    /** Simulations running */
    UPROPERTY(BlueprintReadWrite)
    TArray<FString> Simulations;

    /** Predictions */
    UPROPERTY(BlueprintReadWrite)
    TArray<FString> Predictions;

    /** Awareness of other streams */
    UPROPERTY(BlueprintReadWrite)
    float AwarenessOfPerception = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    float AwarenessOfAction = 0.0f;
};

/**
 * Mutual Awareness Matrix
 * 6-dimensional awareness between all streams
 */
USTRUCT(BlueprintType)
struct FMutualAwarenessMatrix
{
    GENERATED_BODY()

    /** Stream 1 aware of Stream 2 (Observer aware of Actor) */
    UPROPERTY(BlueprintReadWrite)
    float Stream1_Aware_Of_Stream2 = 0.5f;

    /** Stream 1 aware of Stream 3 (Observer aware of Reflector) */
    UPROPERTY(BlueprintReadWrite)
    float Stream1_Aware_Of_Stream3 = 0.5f;

    /** Stream 2 aware of Stream 1 (Actor aware of Observer) */
    UPROPERTY(BlueprintReadWrite)
    float Stream2_Aware_Of_Stream1 = 0.5f;

    /** Stream 2 aware of Stream 3 (Actor aware of Reflector) */
    UPROPERTY(BlueprintReadWrite)
    float Stream2_Aware_Of_Stream3 = 0.5f;

    /** Stream 3 aware of Stream 1 (Reflector aware of Observer) */
    UPROPERTY(BlueprintReadWrite)
    float Stream3_Aware_Of_Stream1 = 0.5f;

    /** Stream 3 aware of Stream 2 (Reflector aware of Actor) */
    UPROPERTY(BlueprintReadWrite)
    float Stream3_Aware_Of_Stream2 = 0.5f;

    /** Recursive depth: How many levels of "I know that you know..." (1-4+) */
    UPROPERTY(BlueprintReadOnly)
    int32 RecursiveDepth = 1;

    /** Triadic coherence: How well synchronized all three streams are (0.0 - 1.0) */
    UPROPERTY(BlueprintReadOnly)
    float TriadicCoherence = 0.0f;

    /** Compute recursive depth */
    void ComputeRecursiveDepth()
    {
        // Average awareness across all dimensions
        float AvgAwareness = (
            Stream1_Aware_Of_Stream2 + Stream1_Aware_Of_Stream3 +
            Stream2_Aware_Of_Stream1 + Stream2_Aware_Of_Stream3 +
            Stream3_Aware_Of_Stream1 + Stream3_Aware_Of_Stream2
        ) / 6.0f;

        // Map to recursive depth (1-5)
        RecursiveDepth = FMath::Clamp(FMath::FloorToInt(AvgAwareness * 5.0f) + 1, 1, 5);
    }

    /** Compute triadic coherence */
    void ComputeTriadicCoherence()
    {
        // Measure how balanced the awareness is across all dimensions
        TArray<float> Awarenesses = {
            Stream1_Aware_Of_Stream2, Stream1_Aware_Of_Stream3,
            Stream2_Aware_Of_Stream1, Stream2_Aware_Of_Stream3,
            Stream3_Aware_Of_Stream1, Stream3_Aware_Of_Stream2
        };

        float Mean = 0.0f;
        for (float A : Awarenesses)
        {
            Mean += A;
        }
        Mean /= 6.0f;

        float Variance = 0.0f;
        for (float A : Awarenesses)
        {
            float Diff = A - Mean;
            Variance += Diff * Diff;
        }
        Variance /= 6.0f;

        // Coherence is high when variance is low
        TriadicCoherence = FMath::Clamp(1.0f - Variance, 0.0f, 1.0f);
    }
};

/**
 * Shared Consciousness State
 * The unified state space where all three streams read and write
 */
USTRUCT(BlueprintType)
struct FSharedConsciousnessState
{
    GENERATED_BODY()

    /** Perceptual state (Stream 1) */
    UPROPERTY(BlueprintReadWrite)
    FPerceptualState PerceptualState;

    /** Action state (Stream 2) */
    UPROPERTY(BlueprintReadWrite)
    FActionState ActionState;

    /** Reflective state (Stream 3) */
    UPROPERTY(BlueprintReadWrite)
    FReflectiveState ReflectiveState;

    /** Mutual awareness matrix */
    UPROPERTY(BlueprintReadWrite)
    FMutualAwarenessMatrix MutualAwareness;

    /** Current cycle step (1-12) */
    UPROPERTY(BlueprintReadOnly)
    int32 CurrentStep = 1;

    /** Synchronization history */
    UPROPERTY(BlueprintReadOnly)
    TArray<float> SynchronizationHistory;
};

/**
 * Feedback Channel Types
 */
UENUM(BlueprintType)
enum class EFeedbackChannel : uint8
{
    /** Cognitive feedback: Thoughts from Stream 3 → Streams 1 & 2 */
    Cognitive UMETA(DisplayName = "Cognitive"),
    
    /** Emotive feedback: Feelings from Stream 2 → Streams 1 & 3 */
    Emotive UMETA(DisplayName = "Emotive"),
    
    /** Sensory feedback: Sensations from Stream 1 → Streams 2 & 3 */
    Sensory UMETA(DisplayName = "Sensory"),
    
    /** Feedforward: Predictions from Stream 3 → Streams 1 & 2 */
    Feedforward UMETA(DisplayName = "Feedforward")
};

/**
 * Recursive Mutual Awareness System Component
 * Implements triadic consciousness with recursive self-observation
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UNREALECHO_API URecursiveMutualAwarenessSystem : public UActorComponent
{
    GENERATED_BODY()

public:
    URecursiveMutualAwarenessSystem();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Enable triadic consciousness */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consciousness|Config")
    bool bEnableTriadicConsciousness = true;

    /** Cycle duration (seconds for full 12-step cycle) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consciousness|Config", meta = (ClampMin = "1.0", ClampMax = "60.0"))
    float CycleDuration = 12.0f;

    /** Awareness update rate (Hz) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consciousness|Config", meta = (ClampMin = "1", ClampMax = "60"))
    int32 AwarenessUpdateRate = 10;

    // ========================================
    // SHARED STATE
    // ========================================

    /** Shared consciousness state */
    UPROPERTY(BlueprintReadOnly, Category = "Consciousness|State")
    FSharedConsciousnessState SharedState;

    // ========================================
    // PUBLIC API
    // ========================================

    /** Update perceptual state (Stream 1) */
    UFUNCTION(BlueprintCallable, Category = "Consciousness")
    void UpdatePerceptualState(const FPerceptualState& NewState);

    /** Update action state (Stream 2) */
    UFUNCTION(BlueprintCallable, Category = "Consciousness")
    void UpdateActionState(const FActionState& NewState);

    /** Update reflective state (Stream 3) */
    UFUNCTION(BlueprintCallable, Category = "Consciousness")
    void UpdateReflectiveState(const FReflectiveState& NewState);

    /** Propagate cognitive feedback (thoughts from Stream 3) */
    UFUNCTION(BlueprintCallable, Category = "Consciousness")
    TArray<FString> PropagateCognitiveFeedback();

    /** Propagate emotive feedback (feelings from Stream 2) */
    UFUNCTION(BlueprintCallable, Category = "Consciousness")
    TArray<FString> PropagateEmotiveFeedback();

    /** Propagate sensory feedback (sensations from Stream 1) */
    UFUNCTION(BlueprintCallable, Category = "Consciousness")
    TArray<FString> PropagateSensoryFeedback();

    /** Propagate feedforward (predictions from Stream 3) */
    UFUNCTION(BlueprintCallable, Category = "Consciousness")
    TArray<FString> PropagateFeedforward();

    /** Get current recursive depth */
    UFUNCTION(BlueprintCallable, Category = "Consciousness")
    int32 GetRecursiveDepth() const { return SharedState.MutualAwareness.RecursiveDepth; }

    /** Get current triadic coherence */
    UFUNCTION(BlueprintCallable, Category = "Consciousness")
    float GetTriadicCoherence() const { return SharedState.MutualAwareness.TriadicCoherence; }

    /** Get current cycle step (1-12) */
    UFUNCTION(BlueprintCallable, Category = "Consciousness")
    int32 GetCurrentStep() const { return SharedState.CurrentStep; }

    /** Is this a triadic synchronization point? */
    UFUNCTION(BlueprintCallable, Category = "Consciousness")
    bool IsTriadicSynchronizationPoint() const;

protected:
    virtual void BeginPlay() override;

private:
    // ========================================
    // INTERNAL STATE
    // ========================================

    /** Cycle timer */
    float CycleTimer = 0.0f;

    /** Awareness update accumulator */
    float AwarenessAccumulator = 0.0f;

    // ========================================
    // INTERNAL METHODS
    // ========================================

    /** Update cycle step */
    void UpdateCycleStep(float DeltaTime);

    /** Update mutual awareness matrix */
    void UpdateMutualAwareness();

    /** Process triadic synchronization */
    void ProcessTriadicSynchronization();

    /** Compute awareness between streams */
    float ComputeAwarenessBetweenStreams(EConsciousnessStream Source, EConsciousnessStream Target) const;
};
