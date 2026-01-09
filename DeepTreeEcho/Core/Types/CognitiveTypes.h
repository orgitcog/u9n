// CognitiveTypes.h
// Unified type definitions for the Deep Tree Echo neuro-symbolic fusion reactor
// This is the SINGLE SOURCE OF TRUTH for all cognitive type definitions

#pragma once

#include "CoreMinimal.h"
#include "CognitiveTypes.generated.h"

// ============================================================================
// CONSCIOUSNESS STREAM TYPES
// ============================================================================

/**
 * Consciousness Stream - The three concurrent processing channels
 * Phased 120° apart (4 steps) in the 12-step cognitive cycle
 *
 * Stream mapping:
 *   Perception:  Steps 1, 4, 7, 10 (sensing and interpreting)
 *   Action:      Steps 2, 5, 8, 11 (executing and expressing)
 *   Simulation:  Steps 3, 6, 9, 12 (predicting and imagining)
 */
UENUM(BlueprintType)
enum class EConsciousnessStream : uint8
{
    /** Stream 1: Perception - sensing, interpreting, relevance detection */
    Perception   UMETA(DisplayName = "Perception"),

    /** Stream 2: Action - executing, expressing, motor output */
    Action       UMETA(DisplayName = "Action"),

    /** Stream 3: Simulation - predicting, imagining, world modeling */
    Simulation   UMETA(DisplayName = "Simulation")
};

/**
 * Stream Role - Functional role in current processing phase
 * Maps to EConsciousnessStream but emphasizes function over identity
 */
UENUM(BlueprintType)
enum class EStreamRole : uint8
{
    /** Perceiving: Active sensing and input processing */
    Perceiving   UMETA(DisplayName = "Perceiving"),

    /** Acting: Motor execution and output generation */
    Acting       UMETA(DisplayName = "Acting"),

    /** Reflecting: Internal simulation and prediction */
    Reflecting   UMETA(DisplayName = "Reflecting")
};

// ============================================================================
// COGNITIVE MODE TYPES
// ============================================================================

/**
 * Cognitive Mode - Processing orientation
 *
 * Expressive mode (Steps 1-7): Outward-directed, world-engaging
 * Reflective mode (Steps 8-12): Inward-directed, self-modeling
 */
UENUM(BlueprintType)
enum class ECognitiveMode : uint8
{
    /** Expressive: Outward-directed processing, engaging with world */
    Expressive   UMETA(DisplayName = "Expressive"),

    /** Reflective: Inward-directed processing, self-modeling */
    Reflective   UMETA(DisplayName = "Reflective")
};

// ============================================================================
// COGNITIVE STEP TYPES
// ============================================================================

/**
 * Step Category - High-level cognitive operation categories
 * Used for broad classification of processing type
 */
UENUM(BlueprintType)
enum class EStepCategory : uint8
{
    /** Pivotal relevance realization - orienting present commitment */
    RelevanceRealization     UMETA(DisplayName = "Relevance Realization"),

    /** Actual affordance interaction - conditioning past performance */
    AffordanceInteraction    UMETA(DisplayName = "Affordance Interaction"),

    /** Virtual salience simulation - anticipating future potential */
    SalienceSimulation       UMETA(DisplayName = "Salience Simulation")
};

/**
 * Cognitive Step Type - Granular 12-step classification
 * Each step maps to a specific cognitive operation
 *
 * Step mapping to categories:
 *   RelevanceRealization: Perceive (1), Orient (7)
 *   AffordanceInteraction: Orient→Decide (2-6), Act, Observe
 *   SalienceSimulation: Simulate, Learn, Consolidate, Anticipate, Transcend
 */
UENUM(BlueprintType)
enum class ECognitiveStepType : uint8
{
    /** Step 1: Perceive - Initial sensory intake */
    Perceive      UMETA(DisplayName = "Perceive"),

    /** Step 2: Orient - Situational assessment */
    Orient        UMETA(DisplayName = "Orient"),

    /** Step 3: Reflect - Internal consideration */
    Reflect       UMETA(DisplayName = "Reflect"),

    /** Step 4: Integrate - Multimodal fusion */
    Integrate     UMETA(DisplayName = "Integrate"),

    /** Step 5: Decide - Action selection */
    Decide        UMETA(DisplayName = "Decide"),

    /** Step 6: Simulate - Action prediction */
    Simulate      UMETA(DisplayName = "Simulate"),

    /** Step 7: Act - Motor execution */
    Act           UMETA(DisplayName = "Act"),

    /** Step 8: Observe - Outcome monitoring */
    Observe       UMETA(DisplayName = "Observe"),

    /** Step 9: Learn - Error correction */
    Learn         UMETA(DisplayName = "Learn"),

    /** Step 10: Consolidate - Memory integration */
    Consolidate   UMETA(DisplayName = "Consolidate"),

    /** Step 11: Anticipate - Future prediction */
    Anticipate    UMETA(DisplayName = "Anticipate"),

    /** Step 12: Transcend - Meta-cognitive integration */
    Transcend     UMETA(DisplayName = "Transcend")
};

// ============================================================================
// TRIAD TYPES
// ============================================================================

/**
 * Triad Group - Synchronization points in the 12-step cycle
 * Triads occur every 4 steps, creating cross-stream coherence
 *
 * Triads:
 *   {1, 5, 9}:  First synchronization (early expressive)
 *   {2, 6, 10}: Second synchronization (mid expressive/reflective)
 *   {3, 7, 11}: Third synchronization (late expressive/reflective)
 *   {4, 8, 12}: Fourth synchronization (cycle boundary)
 */
UENUM(BlueprintType)
enum class ETriadGroup : uint8
{
    /** Triad 1: Steps {1, 5, 9} */
    Triad1   UMETA(DisplayName = "Triad 1 {1,5,9}"),

    /** Triad 2: Steps {2, 6, 10} */
    Triad2   UMETA(DisplayName = "Triad 2 {2,6,10}"),

    /** Triad 3: Steps {3, 7, 11} */
    Triad3   UMETA(DisplayName = "Triad 3 {3,7,11}"),

    /** Triad 4: Steps {4, 8, 12} */
    Triad4   UMETA(DisplayName = "Triad 4 {4,8,12}")
};

// ============================================================================
// NEURO-SYMBOLIC REACTOR TYPES
// ============================================================================

/**
 * Reactor Phase - Neuro-symbolic processing phases
 * Defines the current operation in the fusion cycle
 */
UENUM(BlueprintType)
enum class EReactorPhase : uint8
{
    /** Encoding: Symbolic → Neural transformation */
    Encoding    UMETA(DisplayName = "Encoding"),

    /** Processing: Neural reservoir dynamics */
    Processing  UMETA(DisplayName = "Processing"),

    /** Decoding: Neural → Symbolic transformation */
    Decoding    UMETA(DisplayName = "Decoding"),

    /** Integration: Cross-stream synchronization */
    Integration UMETA(DisplayName = "Integration"),

    /** Output: Apply symbolic actions */
    Output      UMETA(DisplayName = "Output")
};

/**
 * Hemisphere Type - Neural vs Symbolic processing domain
 */
UENUM(BlueprintType)
enum class EHemisphere : uint8
{
    /** Neural: Reservoir computing, pattern recognition, intuition */
    Neural      UMETA(DisplayName = "Neural"),

    /** Symbolic: Physics, animation, explicit reasoning */
    Symbolic    UMETA(DisplayName = "Symbolic"),

    /** Fusion: Integration boundary between hemispheres */
    Fusion      UMETA(DisplayName = "Fusion")
};

// ============================================================================
// STRUCTURES
// ============================================================================

/**
 * Unified Stream State - Complete state representation for a consciousness stream
 */
USTRUCT(BlueprintType)
struct FUnifiedStreamState
{
    GENERATED_BODY()

    /** Stream identifier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stream")
    EConsciousnessStream Stream = EConsciousnessStream::Perception;

    /** Functional role in current phase */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stream")
    EStreamRole Role = EStreamRole::Perceiving;

    /** Current step in 12-step cycle (1-12) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cycle")
    int32 CurrentStep = 1;

    /** Phase offset for 120° separation (0, 4, or 8) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cycle")
    int32 PhaseOffset = 0;

    /** Current cognitive mode */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mode")
    ECognitiveMode Mode = ECognitiveMode::Expressive;

    /** Current granular step type */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step")
    ECognitiveStepType StepType = ECognitiveStepType::Perceive;

    /** High-level step category */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step")
    EStepCategory StepCategory = EStepCategory::RelevanceRealization;

    /** Stream activation level (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ActivationLevel = 1.0f;

    /** Coherence with other streams (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coherence", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Coherence = 1.0f;

    /** Current focus/attention target */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Focus")
    FString CurrentFocus;

    /** Reservoir state vector (neural activations) */
    UPROPERTY(BlueprintReadWrite, Category = "Neural")
    TArray<float> ReservoirState;

    /** Salience value for current processing */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Salience", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float SalienceValue = 0.5f;

    /** Affordance value for current processing */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Affordance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float AffordanceValue = 0.5f;
};

/**
 * Cognitive Step Configuration - Configuration for a single cognitive step
 */
USTRUCT(BlueprintType)
struct FCognitiveStepConfig
{
    GENERATED_BODY()

    /** Step number (1-12) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step")
    int32 StepNumber = 0;

    /** Granular step type */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step")
    ECognitiveStepType StepType = ECognitiveStepType::Perceive;

    /** High-level category */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step")
    EStepCategory Category = EStepCategory::RelevanceRealization;

    /** Cognitive mode for this step */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mode")
    ECognitiveMode Mode = ECognitiveMode::Expressive;

    /** Primary stream responsible for this step */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stream")
    EConsciousnessStream PrimaryStream = EConsciousnessStream::Perception;

    /** Whether this is a pivotal (synchronization) step */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sync")
    bool bIsPivotalStep = false;

    /** Nesting level for OEIS A000081 structure */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Structure")
    int32 NestingLevel = 1;

    /** Triad group membership */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sync")
    ETriadGroup TriadicGroup = ETriadGroup::Triad1;

    /** Human-readable description */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meta")
    FString Description;
};

/**
 * Triadic Synchronization State - State of cross-stream synchronization
 */
USTRUCT(BlueprintType)
struct FTriadicSyncState
{
    GENERATED_BODY()

    /** Triad group index (0-3) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Triad")
    int32 TriadIndex = 0;

    /** Steps in this triad (e.g., {1, 5, 9}) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Triad")
    TArray<int32> TriadSteps;

    /** Synchronization quality (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quality", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float SyncQuality = 1.0f;

    /** Phase alignment across streams (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quality", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float PhaseAlignment = 1.0f;

    /** 3x3 coherence matrix (flattened) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Matrix")
    TArray<float> CoherenceMatrix;
};

/**
 * Nested Shell State - OEIS A000081 structure for execution contexts
 * 1 nest → 1 term, 2 nests → 2 terms, 3 nests → 4 terms, 4 nests → 9 terms
 */
USTRUCT(BlueprintType)
struct FNestedShellState
{
    GENERATED_BODY()

    /** Current nesting level (1-4) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shell")
    int32 NestingLevel = 1;

    /** Number of active terms at this level */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shell")
    int32 ActiveTerms = 1;

    /** Shell activation energy */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ShellEnergy = 1.0f;

    /** Term weights within shell */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shell")
    TArray<float> TermWeights;
};

/**
 * Fusion Reactor Metrics - Health and performance metrics for the reactor
 */
USTRUCT(BlueprintType)
struct FFusionReactorMetrics
{
    GENERATED_BODY()

    /** Neural-symbolic coherence (0-1) */
    UPROPERTY(BlueprintReadWrite, Category = "Coherence")
    float Coherence = 0.0f;

    /** Energy balance ratio (neural/symbolic, ideal = 1.0) */
    UPROPERTY(BlueprintReadWrite, Category = "Energy")
    float EnergyBalance = 1.0f;

    /** Total neural energy (reservoir activation) */
    UPROPERTY(BlueprintReadWrite, Category = "Energy")
    float NeuralEnergy = 0.0f;

    /** Total symbolic energy (physics/animation) */
    UPROPERTY(BlueprintReadWrite, Category = "Energy")
    float SymbolicEnergy = 0.0f;

    /** Encoding efficiency (bits/second) */
    UPROPERTY(BlueprintReadWrite, Category = "Performance")
    float EncodingEfficiency = 0.0f;

    /** Decoding latency (milliseconds) */
    UPROPERTY(BlueprintReadWrite, Category = "Performance")
    float DecodingLatency = 0.0f;

    /** Triadic synchronization quality (0-1) */
    UPROPERTY(BlueprintReadWrite, Category = "Sync")
    float TriadicSyncQuality = 0.0f;

    /** Cognitive cycles per second */
    UPROPERTY(BlueprintReadWrite, Category = "Performance")
    float CyclesPerSecond = 0.0f;

    /** Current reactor phase */
    UPROPERTY(BlueprintReadWrite, Category = "Phase")
    EReactorPhase CurrentPhase = EReactorPhase::Encoding;

    /** Total processing cycles completed */
    UPROPERTY(BlueprintReadWrite, Category = "Stats")
    int64 TotalCycles = 0;
};

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

namespace CognitiveTypeUtils
{
    /**
     * Get the step category for a granular step type
     */
    inline EStepCategory GetStepCategory(ECognitiveStepType Step)
    {
        switch (Step)
        {
            case ECognitiveStepType::Perceive:
            case ECognitiveStepType::Orient:
                return EStepCategory::RelevanceRealization;

            case ECognitiveStepType::Reflect:
            case ECognitiveStepType::Integrate:
            case ECognitiveStepType::Decide:
            case ECognitiveStepType::Act:
            case ECognitiveStepType::Observe:
                return EStepCategory::AffordanceInteraction;

            case ECognitiveStepType::Simulate:
            case ECognitiveStepType::Learn:
            case ECognitiveStepType::Consolidate:
            case ECognitiveStepType::Anticipate:
            case ECognitiveStepType::Transcend:
                return EStepCategory::SalienceSimulation;

            default:
                return EStepCategory::RelevanceRealization;
        }
    }

    /**
     * Get the cognitive mode for a step number
     */
    inline ECognitiveMode GetModeForStep(int32 StepNumber)
    {
        return (StepNumber <= 7) ? ECognitiveMode::Expressive : ECognitiveMode::Reflective;
    }

    /**
     * Get the primary stream for a step number
     */
    inline EConsciousnessStream GetStreamForStep(int32 StepNumber)
    {
        int32 StreamIndex = (StepNumber - 1) % 3;
        switch (StreamIndex)
        {
            case 0: return EConsciousnessStream::Perception;
            case 1: return EConsciousnessStream::Action;
            case 2: return EConsciousnessStream::Simulation;
            default: return EConsciousnessStream::Perception;
        }
    }

    /**
     * Get the triad group for a step number
     */
    inline ETriadGroup GetTriadForStep(int32 StepNumber)
    {
        int32 TriadIndex = (StepNumber - 1) % 4;
        return static_cast<ETriadGroup>(TriadIndex);
    }

    /**
     * Get step type from step number
     */
    inline ECognitiveStepType GetStepTypeFromNumber(int32 StepNumber)
    {
        static const ECognitiveStepType StepMap[] = {
            ECognitiveStepType::Perceive,     // 1
            ECognitiveStepType::Orient,       // 2
            ECognitiveStepType::Reflect,      // 3
            ECognitiveStepType::Integrate,    // 4
            ECognitiveStepType::Decide,       // 5
            ECognitiveStepType::Simulate,     // 6
            ECognitiveStepType::Act,          // 7
            ECognitiveStepType::Observe,      // 8
            ECognitiveStepType::Learn,        // 9
            ECognitiveStepType::Consolidate,  // 10
            ECognitiveStepType::Anticipate,   // 11
            ECognitiveStepType::Transcend     // 12
        };

        if (StepNumber >= 1 && StepNumber <= 12)
        {
            return StepMap[StepNumber - 1];
        }
        return ECognitiveStepType::Perceive;
    }

    /**
     * Get OEIS A000081 term count for nesting level
     * 1→1, 2→2, 3→4, 4→9
     */
    inline int32 GetA000081TermCount(int32 NestingLevel)
    {
        static const int32 TermCounts[] = { 1, 2, 4, 9 };
        if (NestingLevel >= 1 && NestingLevel <= 4)
        {
            return TermCounts[NestingLevel - 1];
        }
        return 1;
    }

    /**
     * Convert stream role to consciousness stream
     */
    inline EConsciousnessStream RoleToStream(EStreamRole Role)
    {
        switch (Role)
        {
            case EStreamRole::Perceiving: return EConsciousnessStream::Perception;
            case EStreamRole::Acting: return EConsciousnessStream::Action;
            case EStreamRole::Reflecting: return EConsciousnessStream::Simulation;
            default: return EConsciousnessStream::Perception;
        }
    }

    /**
     * Convert consciousness stream to stream role
     */
    inline EStreamRole StreamToRole(EConsciousnessStream Stream)
    {
        switch (Stream)
        {
            case EConsciousnessStream::Perception: return EStreamRole::Perceiving;
            case EConsciousnessStream::Action: return EStreamRole::Acting;
            case EConsciousnessStream::Simulation: return EStreamRole::Reflecting;
            default: return EStreamRole::Perceiving;
        }
    }
}
