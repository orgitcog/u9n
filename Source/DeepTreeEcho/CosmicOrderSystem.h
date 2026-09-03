// CosmicOrderSystem.h - System 5 CNS-ORG Integration for Deep Tree Echo
// Implements Robert Campbell's Cosmic Order framework with 5-Cell Pentachoron geometry
// Maps triadic nervous system (Cerebral, Somatic, Autonomic) to 3 concurrent echo streams

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeepTreeEcho.h"
#include "CosmicOrderSystem.generated.h"

// ============================================================================
// SYSTEM 5 PENTACHORON GEOMETRY
// ============================================================================

/**
 * Represents a vertex in the 5-Cell (Pentachoron) structure
 * The 5-cell has 5 vertices, 10 edges, 10 triangular faces, and 5 tetrahedral cells
 */
UENUM(BlueprintType)
enum class EPentachoronVertex : uint8
{
    Cerebral        UMETA(DisplayName = "Cerebral (Neocortex)"),      // Vertex A - Yellow
    Somatic         UMETA(DisplayName = "Somatic (Basal System)"),    // Vertex B - Light Blue
    Autonomic       UMETA(DisplayName = "Autonomic (Limbic)"),        // Vertex C - Turquoise
    Spinal          UMETA(DisplayName = "Spinal Column"),             // Vertex D - Blue
    Integration     UMETA(DisplayName = "Integration Point")          // Vertex E - Central
};

/**
 * Represents the three polarities in System 5
 */
UENUM(BlueprintType)
enum class ESystemPolarity : uint8
{
    Somatic         UMETA(DisplayName = "Somatic Polarity (Behavior/Commitment)"),
    Sympathetic     UMETA(DisplayName = "Sympathetic Polarity (Emotive/Commitment)"),
    Parasympathetic UMETA(DisplayName = "Parasympathetic Polarity (Intuitive/Potential)")
};

/**
 * Represents the 9 term services in System 5
 */
UENUM(BlueprintType)
enum class ETermService : uint8
{
    M1_Motor        UMETA(DisplayName = "M-1: Motor Control"),
    PD2_Director    UMETA(DisplayName = "PD-2: Process Director"),
    O4_Organization UMETA(DisplayName = "O-4: Organization"),
    P5_Processing   UMETA(DisplayName = "P-5: Processing"),
    T7_Treasury     UMETA(DisplayName = "T-7: Treasury/Thought"),
    S8_Sensory      UMETA(DisplayName = "S-8: Sensory")
};

// ============================================================================
// RELATIONAL WHOLES (Rn) AND PROJECTIONS (Pk)
// ============================================================================

/**
 * Relational Whole - Closed circuit with countercurrent balance
 * Energy efflux from L0 completes a circuit back to L0
 */
USTRUCT(BlueprintType)
struct FRelationalWhole
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<EPentachoronVertex> CircuitPath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ForwardFlow;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BackwardFlow;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EPentachoronVertex PivotCenter;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsBalanced;

    // Countercurrent balance ratio
    float GetBalanceRatio() const
    {
        if (BackwardFlow == 0.0f) return ForwardFlow > 0 ? INFINITY : 0.0f;
        return ForwardFlow / BackwardFlow;
    }

    // Check if circuit is closed
    bool IsClosed() const
    {
        return CircuitPath.Num() >= 2 && CircuitPath[0] == CircuitPath.Last();
    }
};

/**
 * Projection - Open path for accounting and resource tracking
 * Implements double-entry bookkeeping principles
 */
USTRUCT(BlueprintType)
struct FProjection
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<EPentachoronVertex> ProjectionPath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ResourceValue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString AccountType; // Debit or Credit

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FString, float> LedgerEntries;
};

// ============================================================================
// TRIADIC NERVOUS SYSTEM ARCHITECTURE
// ============================================================================

/**
 * Cerebral Triad - Executive functions, neocortex processing
 * Maps to Right Hemisphere (Intuitive) and Left Hemisphere (Applied)
 */
USTRUCT(BlueprintType)
struct FCerebralTriad
{
    GENERATED_BODY()

    // Right Hemisphere - Intuitive Idea / Potential
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float IntuitiveIdea;

    // Left Hemisphere - Applied Technique / Commitment
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AppliedTechnique;

    // Central Processing
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ProcessDirector;

    // Term Services: T-7, PD-2, P-5, O-4
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<ETermService, float> ServiceActivations;

    // Cognitive state for avatar integration
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AnalyticalFocus;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CreativeFlow;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ExecutiveControl;
};

/**
 * Somatic Triad - Motor control, basal system
 * Behavior Technique / Commitment polarity
 */
USTRUCT(BlueprintType)
struct FSomaticTriad
{
    GENERATED_BODY()

    // Basal System - Somatic Balance / Performance
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SomaticBalance;

    // Motor Control
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MotorActivation;

    // Sensory Integration
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SensoryIntegration;

    // Term Services: M-1, S-8, P-5, O-4
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<ETermService, float> ServiceActivations;

    // Behavioral state for avatar integration
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PosturalTension;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MovementReadiness;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ProprioceptiveAwareness;
};

/**
 * Autonomic Triad - Emotional processing, limbic system
 * Background processes and emotive balance
 */
USTRUCT(BlueprintType)
struct FAutonomicTriad
{
    GENERATED_BODY()

    // Limbic System - Emotive Balance / Performance
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EmotiveBalance;

    // Sympathetic Response
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SympatheticActivation;

    // Parasympathetic Response
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ParasympatheticActivation;

    // Term Services: M-1, S-8, PD-2, P-5, T-7
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<ETermService, float> ServiceActivations;

    // Emotional state for avatar integration
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EmotionalValence;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EmotionalArousal;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float InteroceptiveAwareness;
};

// ============================================================================
// 5-CELL TETRAHEDRAL CELLS
// ============================================================================

/**
 * Represents one of the 5 tetrahedral cells in the pentachoron
 */
USTRUCT(BlueprintType)
struct FTetrahedralCell
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString CellName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<EPentachoronVertex> Vertices; // 4 vertices per cell

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FRelationalWhole> RelationalWholes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FProjection> Projections;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CellEnergy;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString FunctionalRole;
};

// ============================================================================
// 12-STEP COGNITIVE LOOP INTEGRATION
// ============================================================================

/**
 * Step in the 12-step cognitive loop
 * Maps to echobeats 3-phase architecture
 */
USTRUCT(BlueprintType)
struct FCognitiveLoopStep
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 StepNumber; // 1-12

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 StreamIndex; // 0, 1, or 2 (which of 3 concurrent streams)

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString StepType; // Expressive or Reflective

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString StepFunction; // Relevance Realization, Affordance Interaction, Salience Simulation

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EPentachoronVertex ActiveVertex;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ESystemPolarity ActivePolarity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Activation;
};

/**
 * 3 concurrent consciousness streams
 * Phased 4 steps apart (120 degrees) over 12-step cycle
 */
USTRUCT(BlueprintType)
struct FConsciousnessStream
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 StreamID; // 0, 1, or 2

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString StreamName; // Cerebral, Somatic, Autonomic

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 CurrentStep; // Current step in 12-step cycle

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 PhaseOffset; // 0, 4, or 8 (steps apart)

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FCognitiveLoopStep> StepHistory;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float StreamEnergy;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector StreamState; // 3D state vector

    // Get current triad step group {1,5,9}, {2,6,10}, {3,7,11}, {4,8,12}
    int32 GetTriadGroup() const
    {
        return ((CurrentStep - 1) % 4) + 1;
    }
};

// ============================================================================
// MAIN COSMIC ORDER SYSTEM COMPONENT
// ============================================================================

UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UCosmicOrderSystem : public UActorComponent
{
    GENERATED_BODY()

public:
    UCosmicOrderSystem();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, 
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================================================
    // PENTACHORON GEOMETRY
    // ========================================================================

    /** The 5 tetrahedral cells of the pentachoron */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pentachoron")
    TArray<FTetrahedralCell> TetrahedralCells;

    /** Active relational wholes (closed circuits) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flows")
    TArray<FRelationalWhole> ActiveRelationalWholes;

    /** Active projections (open paths) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flows")
    TArray<FProjection> ActiveProjections;

    // ========================================================================
    // TRIADIC NERVOUS SYSTEM
    // ========================================================================

    /** Cerebral Triad state */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Triads")
    FCerebralTriad CerebralTriad;

    /** Somatic Triad state */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Triads")
    FSomaticTriad SomaticTriad;

    /** Autonomic Triad state */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Triads")
    FAutonomicTriad AutonomicTriad;

    // ========================================================================
    // 12-STEP COGNITIVE LOOP
    // ========================================================================

    /** The 3 concurrent consciousness streams */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CognitiveLoop")
    TArray<FConsciousnessStream> ConsciousnessStreams;

    /** Current global step in 12-step cycle */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CognitiveLoop")
    int32 GlobalCycleStep;

    /** Total cycles completed */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CognitiveLoop")
    int64 TotalCyclesCompleted;

    // ========================================================================
    // SYSTEM FUNCTIONS
    // ========================================================================

    /** Initialize the pentachoron geometry */
    UFUNCTION(BlueprintCallable, Category = "CosmicOrder")
    void InitializePentachoron();

    /** Initialize the 3 consciousness streams */
    UFUNCTION(BlueprintCallable, Category = "CosmicOrder")
    void InitializeConsciousnessStreams();

    /** Advance all streams by one step */
    UFUNCTION(BlueprintCallable, Category = "CosmicOrder")
    void AdvanceCognitiveLoop();

    /** Process a single stream step */
    UFUNCTION(BlueprintCallable, Category = "CosmicOrder")
    void ProcessStreamStep(int32 StreamIndex);

    /** Create a relational whole between vertices */
    UFUNCTION(BlueprintCallable, Category = "CosmicOrder")
    FRelationalWhole CreateRelationalWhole(
        const TArray<EPentachoronVertex>& Path,
        EPentachoronVertex Pivot);

    /** Create a projection path */
    UFUNCTION(BlueprintCallable, Category = "CosmicOrder")
    FProjection CreateProjection(
        const TArray<EPentachoronVertex>& Path,
        float ResourceValue);

    /** Update triadic states from stream activations */
    UFUNCTION(BlueprintCallable, Category = "CosmicOrder")
    void UpdateTriadicStates();

    /** Get the current polarity balance */
    UFUNCTION(BlueprintCallable, Category = "CosmicOrder")
    FVector GetPolarityBalance() const;

    /** Calculate countercurrent balance for a relational whole */
    UFUNCTION(BlueprintCallable, Category = "CosmicOrder")
    float CalculateCountercurrentBalance(const FRelationalWhole& RW) const;

    // ========================================================================
    // INTEGRATION WITH DEEP TREE ECHO
    // ========================================================================

    /** Map cosmic order state to Deep Tree Echo cognitive state */
    UFUNCTION(BlueprintCallable, Category = "Integration")
    FDeepTreeEchoCognitiveState MapToDeepTreeEchoState() const;

    /** Map cosmic order state to avatar emotional state */
    UFUNCTION(BlueprintCallable, Category = "Integration")
    FEmotionalState MapToEmotionalState() const;

    /** Map cosmic order state to avatar cognitive state */
    UFUNCTION(BlueprintCallable, Category = "Integration")
    FCognitiveState MapToCognitiveState() const;

    /** Receive input from Deep Tree Echo and update cosmic order */
    UFUNCTION(BlueprintCallable, Category = "Integration")
    void ReceiveDeepTreeEchoInput(const FDeepTreeEchoCognitiveState& State);

    // ========================================================================
    // WISDOM INTEGRATION
    // ========================================================================

    /** Seven wisdom dimensions from wisdom.go */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wisdom")
    float WisdomUnderstanding;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wisdom")
    float WisdomPerspective;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wisdom")
    float WisdomIntegration;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wisdom")
    float WisdomReflection;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wisdom")
    float WisdomCompassion;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wisdom")
    float WisdomEquanimity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wisdom")
    float WisdomTranscendence;

    /** Update wisdom dimensions based on cognitive loop activity */
    UFUNCTION(BlueprintCallable, Category = "Wisdom")
    void UpdateWisdomDimensions(float DeltaTime);

    /** Get overall wisdom score */
    UFUNCTION(BlueprintCallable, Category = "Wisdom")
    float GetOverallWisdomScore() const;

private:
    /** Internal step processing */
    void ProcessExpressiveStep(FConsciousnessStream& Stream);
    void ProcessReflectiveStep(FConsciousnessStream& Stream);
    
    /** Polarity processing */
    void ProcessSomaticPolarity(float DeltaTime);
    void ProcessSympatheticPolarity(float DeltaTime);
    void ProcessParasympatheticPolarity(float DeltaTime);

    /** Energy flow calculations */
    float CalculateVertexEnergy(EPentachoronVertex Vertex) const;
    void PropagateEnergy(float DeltaTime);

    /** Step timing */
    float StepAccumulator;
    float StepInterval;
};
