#pragma once

/**
 * Holistic Metamodel - Eric Schwarz's Organizational Systems Theory
 * 
 * Implements the complete metamodel structure:
 * - The 1: Hieroglyphic Monad (Unity Principle)
 * - The 2: Dual Complementarity (Actual-Virtual Dynamics)
 * - The 3: Triadic Primitives (Being, Becoming, Relation)
 * - The 4: Self-Stabilizing Cycle (Emergence, Development, Integration, Transcendence)
 * - The 7: Triad Production Steps (Developmental Stages)
 * - The 9: Ennead Meta-System (Creativity, Stability, Drift)
 * - The 11: Evolutionary Helix (Long-Term Transformation)
 * 
 * Three Dynamic Streams:
 * - Entropic: en-tropis → auto-vortis → auto-morphosis
 * - Negnentropic: negen-tropis → auto-stasis → auto-poiesis
 * - Identity: iden-tropis → auto-gnosis → auto-genesis
 */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HolisticMetamodel.generated.h"

// ========================================
// ENUMERATIONS
// ========================================

/**
 * Self-Stabilizing Cycle Phases (The 4)
 */
UENUM(BlueprintType)
enum class ECyclePhase : uint8
{
    /** Initial manifestation and potential */
    Emergence UMETA(DisplayName = "Emergence"),
    
    /** Growth, elaboration, and complexity building */
    Development UMETA(DisplayName = "Development"),
    
    /** Synthesis, coherence, and stabilization */
    Integration UMETA(DisplayName = "Integration"),
    
    /** Transformation, elevation, and renewal */
    Transcendence UMETA(DisplayName = "Transcendence")
};

/**
 * Triad Production Steps (The 7)
 */
UENUM(BlueprintType)
enum class ETriadProductionStep : uint8
{
    /** First separation from unity */
    InitialDifferentiation UMETA(DisplayName = "Initial Differentiation"),
    
    /** Establishment of polar opposites */
    PolarTension UMETA(DisplayName = "Polar Tension"),
    
    /** Interaction between poles */
    DynamicInteraction UMETA(DisplayName = "Dynamic Interaction"),
    
    /** Emergence of synthetic third */
    SyntheticEmergence UMETA(DisplayName = "Synthetic Emergence"),
    
    /** Stabilization of triadic form */
    TriadicStabilization UMETA(DisplayName = "Triadic Stabilization"),
    
    /** Recursive self-elaboration */
    RecursiveElaboration UMETA(DisplayName = "Recursive Elaboration"),
    
    /** Integration at higher level */
    TranscendentIntegration UMETA(DisplayName = "Transcendent Integration")
};

/**
 * Ennead Aspects (The 9)
 */
UENUM(BlueprintType)
enum class EEnneadAspect : uint8
{
    /** Pure creative force */
    CreativePotential UMETA(DisplayName = "Creative Potential"),
    
    /** Form-giving capacity */
    FormativePower UMETA(DisplayName = "Formative Power"),
    
    /** Structural maintenance */
    StructuralStability UMETA(DisplayName = "Structural Stability"),
    
    /** Process dynamics */
    DynamicProcess UMETA(DisplayName = "Dynamic Process"),
    
    /** Harmonic equilibrium */
    HarmonicBalance UMETA(DisplayName = "Harmonic Balance"),
    
    /** Adaptive capabilities */
    AdaptiveResponse UMETA(DisplayName = "Adaptive Response"),
    
    /** Reflective consciousness */
    CognitiveReflection UMETA(DisplayName = "Cognitive Reflection"),
    
    /** Evolutionary tendencies */
    EvolutionaryDrift UMETA(DisplayName = "Evolutionary Drift"),
    
    /** Unity transcendence */
    TranscendentUnity UMETA(DisplayName = "Transcendent Unity")
};

/**
 * Ennead Tendencies (Groupings of The 9)
 */
UENUM(BlueprintType)
enum class EEnneadTendency : uint8
{
    /** Creative and formative forces (aspects 1, 2, 6) */
    Creativity UMETA(DisplayName = "Creativity"),
    
    /** Stabilizing and harmonizing forces (aspects 3, 5, 9) */
    Stability UMETA(DisplayName = "Stability"),
    
    /** Dynamic and evolutionary forces (aspects 4, 7, 8) */
    Drift UMETA(DisplayName = "Drift")
};

/**
 * Evolutionary Helix Stages (The 11)
 */
UENUM(BlueprintType)
enum class EHelixStage : uint8
{
    /** Original undifferentiated state */
    PrimordialUnity UMETA(DisplayName = "Primordial Unity"),
    
    /** First stirring of differentiation */
    InitialAwakening UMETA(DisplayName = "Initial Awakening"),
    
    /** Manifestation of polarities */
    PolarManifestation UMETA(DisplayName = "Polar Manifestation"),
    
    /** Formation of triadic structures */
    TriadicFormation UMETA(DisplayName = "Triadic Formation"),
    
    /** Four-fold stabilization */
    QuaternaryStabilization UMETA(DisplayName = "Quaternary Stabilization"),
    
    /** Five-fold elaboration */
    QuinternaryElaboration UMETA(DisplayName = "Quinternary Elaboration"),
    
    /** Seven-fold development */
    SeptenaryDevelopment UMETA(DisplayName = "Septenary Development"),
    
    /** Nine-fold integration */
    EnneadIntegration UMETA(DisplayName = "Ennead Integration"),
    
    /** Ten-fold completion */
    DecimalCompletion UMETA(DisplayName = "Decimal Completion"),
    
    /** Eleven-fold transcendence */
    HendecadTranscendence UMETA(DisplayName = "Hendecad Transcendence"),
    
    /** Return to unity at higher level */
    CosmicReturn UMETA(DisplayName = "Cosmic Return")
};

/**
 * Dynamic Stream Type
 */
UENUM(BlueprintType)
enum class EDynamicStream : uint8
{
    /** Entropic: en-tropis → auto-vortis → auto-morphosis */
    Entropic UMETA(DisplayName = "Entropic"),
    
    /** Negnentropic: negen-tropis → auto-stasis → auto-poiesis */
    Negnentropic UMETA(DisplayName = "Negnentropic"),
    
    /** Identity: iden-tropis → auto-gnosis → auto-genesis */
    Identity UMETA(DisplayName = "Identity")
};

// ========================================
// STRUCTURES
// ========================================

/**
 * Hieroglyphic Monad (The 1)
 */
USTRUCT(BlueprintType)
struct FHieroglyphicMonad
{
    GENERATED_BODY()

    /** Integration degree across system levels */
    UPROPERTY(BlueprintReadWrite)
    float IntegrationDegree = 1.0f;

    /** Current manifestation level */
    UPROPERTY(BlueprintReadWrite)
    int32 ManifestationLevel = 0;

    /** Coherence at current level */
    UPROPERTY(BlueprintReadWrite)
    float Coherence = 1.0f;

    /** Unity principle strength */
    UPROPERTY(BlueprintReadWrite)
    float UnityStrength = 1.0f;
};

/**
 * Dual Complementarity State (The 2)
 */
USTRUCT(BlueprintType)
struct FDualComplementarity
{
    GENERATED_BODY()

    /** Actual (manifest/realized) state value */
    UPROPERTY(BlueprintReadWrite)
    float ActualState = 0.5f;

    /** Virtual (potential/latent) state value */
    UPROPERTY(BlueprintReadWrite)
    float VirtualState = 0.5f;

    /** Tension level between actual and virtual */
    UPROPERTY(BlueprintReadWrite)
    float TensionLevel = 0.0f;

    /** Complementarity degree */
    UPROPERTY(BlueprintReadWrite)
    float ComplementarityDegree = 1.0f;

    /** Resolution progress (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float ResolutionProgress = 0.0f;
};

/**
 * Triadic System (The 3)
 */
USTRUCT(BlueprintType)
struct FTriadicSystem
{
    GENERATED_BODY()

    /** Being - Pure existence/presence/structural foundation */
    UPROPERTY(BlueprintReadWrite)
    float Being = 0.333f;

    /** Becoming - Process/transformation/dynamic evolution */
    UPROPERTY(BlueprintReadWrite)
    float Becoming = 0.333f;

    /** Relation - Connection/interaction/relational coherence */
    UPROPERTY(BlueprintReadWrite)
    float Relation = 0.334f;

    /** Dynamic equilibrium state */
    UPROPERTY(BlueprintReadWrite)
    float Equilibrium = 1.0f;

    /** Transformation potential */
    UPROPERTY(BlueprintReadWrite)
    float TransformationPotential = 0.5f;

    /** Triad coherence */
    UPROPERTY(BlueprintReadWrite)
    float Coherence = 1.0f;
};

/**
 * Self-Stabilizing Cycle State (The 4)
 */
USTRUCT(BlueprintType)
struct FSelfStabilizingCycle
{
    GENERATED_BODY()

    /** Current phase */
    UPROPERTY(BlueprintReadWrite)
    ECyclePhase CurrentPhase = ECyclePhase::Emergence;

    /** Progress within current phase (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float PhaseProgress = 0.0f;

    /** Total cycles completed */
    UPROPERTY(BlueprintReadWrite)
    int32 CyclesCompleted = 0;

    /** Phase energies */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> PhaseEnergies;

    /** Emergent qualities from phase transitions */
    UPROPERTY(BlueprintReadWrite)
    TArray<FString> EmergentQualities;
};

/**
 * Triad Production Process State (The 7)
 */
USTRUCT(BlueprintType)
struct FTriadProductionProcess
{
    GENERATED_BODY()

    /** Current step */
    UPROPERTY(BlueprintReadWrite)
    ETriadProductionStep CurrentStep = ETriadProductionStep::InitialDifferentiation;

    /** Step progress (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float StepProgress = 0.0f;

    /** Energy level */
    UPROPERTY(BlueprintReadWrite)
    float Energy = 1.0f;

    /** Integration level */
    UPROPERTY(BlueprintReadWrite)
    float IntegrationLevel = 0.0f;

    /** Production cycles completed */
    UPROPERTY(BlueprintReadWrite)
    int32 ProductionCycles = 0;
};

/**
 * Ennead Aspect State
 */
USTRUCT(BlueprintType)
struct FEnneadAspectState
{
    GENERATED_BODY()

    /** Aspect type */
    UPROPERTY(BlueprintReadWrite)
    EEnneadAspect Aspect = EEnneadAspect::CreativePotential;

    /** Aspect strength (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Strength = 0.5f;

    /** Active state */
    UPROPERTY(BlueprintReadWrite)
    bool bIsActive = true;
};

/**
 * Ennead Meta-System State (The 9)
 */
USTRUCT(BlueprintType)
struct FEnneadMetaSystem
{
    GENERATED_BODY()

    /** All nine aspect states */
    UPROPERTY(BlueprintReadWrite)
    TArray<FEnneadAspectState> Aspects;

    /** Creativity tendency strength */
    UPROPERTY(BlueprintReadWrite)
    float CreativityStrength = 0.333f;

    /** Stability tendency strength */
    UPROPERTY(BlueprintReadWrite)
    float StabilityStrength = 0.333f;

    /** Drift tendency strength */
    UPROPERTY(BlueprintReadWrite)
    float DriftStrength = 0.334f;

    /** Dominant tendency */
    UPROPERTY(BlueprintReadWrite)
    EEnneadTendency DominantTendency = EEnneadTendency::Stability;

    /** Meta-coherence */
    UPROPERTY(BlueprintReadWrite)
    float MetaCoherence = 1.0f;
};

/**
 * Evolutionary Helix State (The 11)
 */
USTRUCT(BlueprintType)
struct FEvolutionaryHelix
{
    GENERATED_BODY()

    /** Current stage */
    UPROPERTY(BlueprintReadWrite)
    EHelixStage CurrentStage = EHelixStage::PrimordialUnity;

    /** Stage progress (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float StageProgress = 0.0f;

    /** Current spiral level */
    UPROPERTY(BlueprintReadWrite)
    int32 SpiralLevel = 0;

    /** Evolutionary momentum */
    UPROPERTY(BlueprintReadWrite)
    float Momentum = 0.5f;

    /** Stage completion history */
    UPROPERTY(BlueprintReadWrite)
    TArray<bool> StageCompletions;
};

/**
 * Dynamic Stream State
 */
USTRUCT(BlueprintType)
struct FDynamicStreamState
{
    GENERATED_BODY()

    /** Stream type */
    UPROPERTY(BlueprintReadWrite)
    EDynamicStream StreamType = EDynamicStream::Entropic;

    /** Stream energy (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Energy = 0.5f;

    /** Stream stability (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Stability = 0.5f;

    /** Stream coherence (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Coherence = 0.5f;

    /** Flow rate */
    UPROPERTY(BlueprintReadWrite)
    float FlowRate = 1.0f;

    /** Phase within stream cycle */
    UPROPERTY(BlueprintReadWrite)
    int32 Phase = 0;
};

/**
 * Complete Metamodel State
 */
USTRUCT(BlueprintType)
struct FMetamodelState
{
    GENERATED_BODY()

    /** The 1 - Hieroglyphic Monad */
    UPROPERTY(BlueprintReadWrite)
    FHieroglyphicMonad Monad;

    /** The 2 - Dual Complementarity */
    UPROPERTY(BlueprintReadWrite)
    FDualComplementarity DualComplementarity;

    /** The 3 - Triadic System */
    UPROPERTY(BlueprintReadWrite)
    FTriadicSystem TriadicSystem;

    /** The 4 - Self-Stabilizing Cycle */
    UPROPERTY(BlueprintReadWrite)
    FSelfStabilizingCycle StabilizingCycle;

    /** The 7 - Triad Production Process */
    UPROPERTY(BlueprintReadWrite)
    FTriadProductionProcess TriadProduction;

    /** The 9 - Ennead Meta-System */
    UPROPERTY(BlueprintReadWrite)
    FEnneadMetaSystem EnneadSystem;

    /** The 11 - Evolutionary Helix */
    UPROPERTY(BlueprintReadWrite)
    FEvolutionaryHelix EvolutionaryHelix;

    /** Dynamic Streams */
    UPROPERTY(BlueprintReadWrite)
    TArray<FDynamicStreamState> DynamicStreams;

    /** Overall metamodel coherence */
    UPROPERTY(BlueprintReadWrite)
    float MetamodelCoherence = 1.0f;
};

/**
 * Holistic Metamodel Component
 * 
 * Implements Eric Schwarz's organizational systems theory for
 * the Deep-Tree-Echo cognitive framework.
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UHolisticMetamodel : public UActorComponent
{
    GENERATED_BODY()

public:
    UHolisticMetamodel();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Enable automatic metamodel processing */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metamodel|Config")
    bool bEnableAutoProcessing = true;

    /** Processing interval in seconds */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metamodel|Config", meta = (ClampMin = "0.01", ClampMax = "10.0"))
    float ProcessingInterval = 0.1f;

    /** Maximum hierarchical levels for monad manifestation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metamodel|Config", meta = (ClampMin = "1", ClampMax = "11"))
    int32 MaxHierarchicalLevels = 5;

    // ========================================
    // STATE
    // ========================================

    /** Current metamodel state */
    UPROPERTY(BlueprintReadOnly, Category = "Metamodel|State")
    FMetamodelState CurrentState;

    // ========================================
    // PUBLIC API - INITIALIZATION
    // ========================================

    /** Initialize the metamodel with system context */
    UFUNCTION(BlueprintCallable, Category = "Metamodel")
    void InitializeMetamodel(int32 TotalComponents, int32 ActiveComponents, float SystemEnergy);

    /** Reset metamodel to initial state */
    UFUNCTION(BlueprintCallable, Category = "Metamodel")
    void ResetMetamodel();

    // ========================================
    // PUBLIC API - PROCESSING
    // ========================================

    /** Process one metamodel cycle */
    UFUNCTION(BlueprintCallable, Category = "Metamodel|Processing")
    void ProcessCycle(float DeltaTime);

    /** Update monad manifestation */
    UFUNCTION(BlueprintCallable, Category = "Metamodel|Processing")
    void UpdateMonad(int32 Level);

    /** Resolve dual complementarity tension */
    UFUNCTION(BlueprintCallable, Category = "Metamodel|Processing")
    void ResolveDualComplementarity();

    /** Update triadic equilibrium */
    UFUNCTION(BlueprintCallable, Category = "Metamodel|Processing")
    void UpdateTriadicEquilibrium();

    /** Advance self-stabilizing cycle */
    UFUNCTION(BlueprintCallable, Category = "Metamodel|Processing")
    void AdvanceCyclePhase();

    /** Advance triad production step */
    UFUNCTION(BlueprintCallable, Category = "Metamodel|Processing")
    void AdvanceTriadProduction();

    /** Update ennead dynamics */
    UFUNCTION(BlueprintCallable, Category = "Metamodel|Processing")
    void UpdateEnneadDynamics();

    /** Advance evolutionary helix */
    UFUNCTION(BlueprintCallable, Category = "Metamodel|Processing")
    void AdvanceEvolutionaryHelix();

    /** Update dynamic streams */
    UFUNCTION(BlueprintCallable, Category = "Metamodel|Processing")
    void UpdateDynamicStreams(float DeltaTime);

    // ========================================
    // PUBLIC API - STATE ACCESS
    // ========================================

    /** Get current metamodel coherence */
    UFUNCTION(BlueprintCallable, Category = "Metamodel|State")
    float GetMetamodelCoherence() const;

    /** Get dominant ennead tendency */
    UFUNCTION(BlueprintCallable, Category = "Metamodel|State")
    EEnneadTendency GetDominantTendency() const;

    /** Get current evolutionary stage */
    UFUNCTION(BlueprintCallable, Category = "Metamodel|State")
    EHelixStage GetEvolutionaryStage() const;

    /** Get current cycle phase */
    UFUNCTION(BlueprintCallable, Category = "Metamodel|State")
    ECyclePhase GetCyclePhase() const;

    /** Get stream state by type */
    UFUNCTION(BlueprintCallable, Category = "Metamodel|State")
    FDynamicStreamState GetStreamState(EDynamicStream StreamType) const;

    // ========================================
    // PUBLIC API - HOLISTIC INSIGHTS
    // ========================================

    /** Generate holistic insights from current state */
    UFUNCTION(BlueprintCallable, Category = "Metamodel|Insights")
    TArray<FString> GenerateHolisticInsights() const;

    /** Assess self-awareness level */
    UFUNCTION(BlueprintCallable, Category = "Metamodel|Insights")
    float AssessSelfAwareness() const;

    /** Get organizational health */
    UFUNCTION(BlueprintCallable, Category = "Metamodel|Insights")
    float GetOrganizationalHealth() const;

protected:
    virtual void BeginPlay() override;

private:
    /** Accumulated time for processing */
    float AccumulatedTime = 0.0f;

    /** Initialize ennead aspects */
    void InitializeEnneadAspects();

    /** Initialize dynamic streams */
    void InitializeDynamicStreams();

    /** Calculate tendency strength from aspects */
    float CalculateTendencyStrength(EEnneadTendency Tendency) const;

    /** Update dominant tendency */
    void UpdateDominantTendency();

    /** Calculate overall metamodel coherence */
    void CalculateMetamodelCoherence();
};
