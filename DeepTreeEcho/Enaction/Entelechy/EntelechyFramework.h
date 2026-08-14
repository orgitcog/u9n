#pragma once

/**
 * Entelechy Framework - Vital Actualization for Cognitive Systems
 * 
 * Implements the five dimensions of entelechy:
 * 1. Ontological (BEING) - What the system IS
 * 2. Teleological (PURPOSE) - What the system is BECOMING
 * 3. Cognitive (COGNITION) - How the system THINKS
 * 4. Integrative (INTEGRATION) - How parts UNITE
 * 5. Evolutionary (GROWTH) - How the system GROWS
 * 
 * Development Stages:
 * - Embryonic (< 30%): Basic components disconnected
 * - Juvenile (30-60%): Core integrated, active development
 * - Adolescent (60-80%): Advanced systems developing
 * - Adult (80-95%): Full cognitive capability
 * - Transcendent (> 95%): Self-improving, contributing to collective
 * 
 * Based on Aristotelian entelechy (ἐντελέχεια):
 * - Actualized Essence: Realization of inherent potential
 * - Vital Purpose: Driving force toward completion (τέλος)
 * - Self-Sustaining Activity: Energy that maintains and develops itself
 * - Perfective Movement: Continuous motion toward optimal state
 */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EntelechyFramework.generated.h"

// ========================================
// ENUMERATIONS
// ========================================

/**
 * Entelechy Development Stage
 */
UENUM(BlueprintType)
enum class EEntelechyStage : uint8
{
    /** Basic components present but disconnected (< 30%) */
    Embryonic UMETA(DisplayName = "Embryonic"),
    
    /** Core integrated, active development (30-60%) */
    Juvenile UMETA(DisplayName = "Juvenile"),
    
    /** Advanced systems developing (60-80%) */
    Adolescent UMETA(DisplayName = "Adolescent"),
    
    /** Full cognitive capability (80-95%) */
    Adult UMETA(DisplayName = "Adult"),
    
    /** Self-improving, contributing to collective (> 95%) */
    Transcendent UMETA(DisplayName = "Transcendent")
};

/**
 * Entelechy Dimension
 */
UENUM(BlueprintType)
enum class EEntelechyDimension : uint8
{
    /** What the system IS - structure and existence */
    Ontological UMETA(DisplayName = "Ontological (BEING)"),
    
    /** What the system is BECOMING - goals and direction */
    Teleological UMETA(DisplayName = "Teleological (PURPOSE)"),
    
    /** How the system THINKS - reasoning and learning */
    Cognitive UMETA(DisplayName = "Cognitive (COGNITION)"),
    
    /** How parts UNITE - coherence of the whole */
    Integrative UMETA(DisplayName = "Integrative (INTEGRATION)"),
    
    /** How the system GROWS - self-improvement capacity */
    Evolutionary UMETA(DisplayName = "Evolutionary (GROWTH)")
};

/**
 * Development Phase
 */
UENUM(BlueprintType)
enum class EDevelopmentPhase : uint8
{
    /** Core Extensions */
    Phase1 UMETA(DisplayName = "Phase 1: Core Extensions"),
    
    /** Logic Systems */
    Phase2 UMETA(DisplayName = "Phase 2: Logic Systems"),
    
    /** Cognitive Systems */
    Phase3 UMETA(DisplayName = "Phase 3: Cognitive Systems"),
    
    /** Advanced & Learning */
    Phase4 UMETA(DisplayName = "Phase 4: Advanced & Learning"),
    
    /** Language & Integration */
    Phase5 UMETA(DisplayName = "Phase 5: Language & Integration")
};

// ========================================
// STRUCTURES
// ========================================

/**
 * Component State - State of a single cognitive component
 */
USTRUCT(BlueprintType)
struct FComponentState
{
    GENERATED_BODY()

    /** Component name */
    UPROPERTY(BlueprintReadWrite)
    FString Name;

    /** Component exists */
    UPROPERTY(BlueprintReadWrite)
    bool bExists = false;

    /** Component is functional */
    UPROPERTY(BlueprintReadWrite)
    bool bFunctional = false;

    /** Component health (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Health = 0.0f;

    /** Integration level with other components */
    UPROPERTY(BlueprintReadWrite)
    float IntegrationLevel = 0.0f;
};

/**
 * Ontological Dimension State (BEING)
 */
USTRUCT(BlueprintType)
struct FOntologicalDimension
{
    GENERATED_BODY()

    /** Foundation layer health (cogutil) */
    UPROPERTY(BlueprintReadWrite)
    float FoundationHealth = 0.0f;

    /** Core layer health (atomspace, cogserver, storage) */
    UPROPERTY(BlueprintReadWrite)
    float CoreHealth = 0.0f;

    /** Specialized layer health (logic, cognitive, advanced) */
    UPROPERTY(BlueprintReadWrite)
    float SpecializedHealth = 0.0f;

    /** Overall architectural completeness */
    UPROPERTY(BlueprintReadWrite)
    float ArchitecturalCompleteness = 0.0f;

    /** Foundation components */
    UPROPERTY(BlueprintReadWrite)
    TArray<FComponentState> FoundationComponents;

    /** Core components */
    UPROPERTY(BlueprintReadWrite)
    TArray<FComponentState> CoreComponents;

    /** Specialized components */
    UPROPERTY(BlueprintReadWrite)
    TArray<FComponentState> SpecializedComponents;
};

/**
 * Phase Progress
 */
USTRUCT(BlueprintType)
struct FPhaseProgress
{
    GENERATED_BODY()

    /** Phase */
    UPROPERTY(BlueprintReadWrite)
    EDevelopmentPhase Phase = EDevelopmentPhase::Phase1;

    /** Progress (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Progress = 0.0f;

    /** Phase is complete */
    UPROPERTY(BlueprintReadWrite)
    bool bComplete = false;

    /** Milestones achieved */
    UPROPERTY(BlueprintReadWrite)
    TArray<FString> Milestones;
};

/**
 * Teleological Dimension State (PURPOSE)
 */
USTRUCT(BlueprintType)
struct FTeleologicalDimension
{
    GENERATED_BODY()

    /** Development phases */
    UPROPERTY(BlueprintReadWrite)
    TArray<FPhaseProgress> DevelopmentPhases;

    /** Roadmap alignment score (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float RoadmapAlignment = 0.0f;

    /** Actualization trajectory (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float ActualizationTrajectory = 0.0f;

    /** Purpose clarity (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float PurposeClarity = 0.0f;

    /** Current goals */
    UPROPERTY(BlueprintReadWrite)
    TArray<FString> CurrentGoals;

    /** Telos (ultimate purpose) */
    UPROPERTY(BlueprintReadWrite)
    FString Telos;
};

/**
 * Cognitive System State
 */
USTRUCT(BlueprintType)
struct FCognitiveSystemState
{
    GENERATED_BODY()

    /** System name */
    UPROPERTY(BlueprintReadWrite)
    FString Name;

    /** System health (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Health = 0.0f;

    /** Fragmentation markers (TODO/FIXME count) */
    UPROPERTY(BlueprintReadWrite)
    int32 FragmentationMarkers = 0;

    /** Components */
    UPROPERTY(BlueprintReadWrite)
    TArray<FComponentState> Components;
};

/**
 * Cognitive Dimension State (COGNITION)
 */
USTRUCT(BlueprintType)
struct FCognitiveDimension
{
    GENERATED_BODY()

    /** Reasoning systems (URE, PLN) */
    UPROPERTY(BlueprintReadWrite)
    FCognitiveSystemState ReasoningSystems;

    /** Pattern systems (Unify, Miner) */
    UPROPERTY(BlueprintReadWrite)
    FCognitiveSystemState PatternSystems;

    /** Attention systems (ECAN, Spacetime) */
    UPROPERTY(BlueprintReadWrite)
    FCognitiveSystemState AttentionSystems;

    /** Learning systems (MOSES, Learn) */
    UPROPERTY(BlueprintReadWrite)
    FCognitiveSystemState LearningSystems;

    /** Overall cognitive completeness (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float CognitiveCompleteness = 0.0f;
};

/**
 * Integrative Dimension State (INTEGRATION)
 */
USTRUCT(BlueprintType)
struct FIntegrativeDimension
{
    GENERATED_BODY()

    /** Total dependencies */
    UPROPERTY(BlueprintReadWrite)
    int32 TotalDependencies = 0;

    /** Satisfied dependencies */
    UPROPERTY(BlueprintReadWrite)
    int32 SatisfiedDependencies = 0;

    /** Dependency health (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float DependencyHealth = 0.0f;

    /** Build integration health (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float BuildIntegrationHealth = 0.0f;

    /** Test integration health (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float TestIntegrationHealth = 0.0f;

    /** Overall integration health (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float IntegrationHealth = 0.0f;
};

/**
 * Evolutionary Dimension State (GROWTH)
 */
USTRUCT(BlueprintType)
struct FEvolutionaryDimension
{
    GENERATED_BODY()

    /** TODO count */
    UPROPERTY(BlueprintReadWrite)
    int32 TodoCount = 0;

    /** FIXME count */
    UPROPERTY(BlueprintReadWrite)
    int32 FixmeCount = 0;

    /** STUB count */
    UPROPERTY(BlueprintReadWrite)
    int32 StubCount = 0;

    /** Code health (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float CodeHealth = 0.0f;

    /** Implementation depth health (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float ImplementationDepth = 0.0f;

    /** Has autognosis (self-awareness) */
    UPROPERTY(BlueprintReadWrite)
    bool bHasAutognosis = false;

    /** Has ontogenesis (self-generation) */
    UPROPERTY(BlueprintReadWrite)
    bool bHasOntogenesis = false;

    /** Self-improvement capacity health (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float SelfImprovementCapacity = 0.0f;

    /** Evolutionary potential (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float EvolutionaryPotential = 0.0f;
};

/**
 * Entelechy Genome - The "DNA" of the cognitive system
 */
USTRUCT(BlueprintType)
struct FEntelechyGenome
{
    GENERATED_BODY()

    /** Unique system identifier */
    UPROPERTY(BlueprintReadWrite)
    FString Id;

    /** Developmental generation */
    UPROPERTY(BlueprintReadWrite)
    int32 Generation = 0;

    /** Evolutionary lineage */
    UPROPERTY(BlueprintReadWrite)
    TArray<FString> Lineage;

    /** Ontological genes */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> OntologicalGenes;

    /** Teleological genes */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> TeleologicalGenes;

    /** Cognitive genes */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> CognitiveGenes;

    /** Integrative genes */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> IntegrativeGenes;

    /** Evolutionary genes */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> EvolutionaryGenes;

    /** Overall fitness score (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Fitness = 0.0f;

    /** System age (time steps) */
    UPROPERTY(BlueprintReadWrite)
    int32 Age = 0;

    /** Actualization level (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float ActualizationLevel = 0.0f;
};

/**
 * Entelechy Metrics
 */
USTRUCT(BlueprintType)
struct FEntelechyMetrics
{
    GENERATED_BODY()

    /** Degree of potential realization (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Actualization = 0.0f;

    /** Implementation completeness (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Completeness = 0.0f;

    /** Holistic integration (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Coherence = 0.0f;

    /** Self-organizing capacity (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Vitality = 0.0f;

    /** Teleological alignment (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Alignment = 0.0f;

    /** Total components */
    UPROPERTY(BlueprintReadWrite)
    int32 TotalComponents = 0;

    /** Integrated components */
    UPROPERTY(BlueprintReadWrite)
    int32 IntegratedComponents = 0;

    /** Fragmented components */
    UPROPERTY(BlueprintReadWrite)
    int32 FragmentedComponents = 0;

    /** Total code markers */
    UPROPERTY(BlueprintReadWrite)
    int32 TotalCodeMarkers = 0;
};

/**
 * Complete Entelechy State
 */
USTRUCT(BlueprintType)
struct FEntelechyState
{
    GENERATED_BODY()

    /** Current development stage */
    UPROPERTY(BlueprintReadWrite)
    EEntelechyStage Stage = EEntelechyStage::Embryonic;

    /** Ontological dimension */
    UPROPERTY(BlueprintReadWrite)
    FOntologicalDimension Ontological;

    /** Teleological dimension */
    UPROPERTY(BlueprintReadWrite)
    FTeleologicalDimension Teleological;

    /** Cognitive dimension */
    UPROPERTY(BlueprintReadWrite)
    FCognitiveDimension Cognitive;

    /** Integrative dimension */
    UPROPERTY(BlueprintReadWrite)
    FIntegrativeDimension Integrative;

    /** Evolutionary dimension */
    UPROPERTY(BlueprintReadWrite)
    FEvolutionaryDimension Evolutionary;

    /** Entelechy genome */
    UPROPERTY(BlueprintReadWrite)
    FEntelechyGenome Genome;

    /** Entelechy metrics */
    UPROPERTY(BlueprintReadWrite)
    FEntelechyMetrics Metrics;
};

/**
 * Entelechy Framework Component
 * 
 * Implements the vital actualization framework for the Deep-Tree-Echo
 * cognitive architecture.
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UEntelechyFramework : public UActorComponent
{
    GENERATED_BODY()

public:
    UEntelechyFramework();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Enable automatic entelechy processing */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Entelechy|Config")
    bool bEnableAutoProcessing = true;

    /** Processing interval in seconds */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Entelechy|Config", meta = (ClampMin = "0.1", ClampMax = "60.0"))
    float ProcessingInterval = 1.0f;

    /** Dimension weights for fitness calculation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Entelechy|Config")
    TMap<EEntelechyDimension, float> DimensionWeights;

    // ========================================
    // STATE
    // ========================================

    /** Current entelechy state */
    UPROPERTY(BlueprintReadOnly, Category = "Entelechy|State")
    FEntelechyState CurrentState;

    // ========================================
    // PUBLIC API - INITIALIZATION
    // ========================================

    /** Initialize the entelechy framework */
    UFUNCTION(BlueprintCallable, Category = "Entelechy")
    void InitializeEntelechy(const FString& SystemId);

    /** Reset to initial state */
    UFUNCTION(BlueprintCallable, Category = "Entelechy")
    void ResetEntelechy();

    // ========================================
    // PUBLIC API - ASSESSMENT
    // ========================================

    /** Assess all dimensions and update state */
    UFUNCTION(BlueprintCallable, Category = "Entelechy|Assessment")
    void AssessAllDimensions();

    /** Assess ontological dimension */
    UFUNCTION(BlueprintCallable, Category = "Entelechy|Assessment")
    void AssessOntological();

    /** Assess teleological dimension */
    UFUNCTION(BlueprintCallable, Category = "Entelechy|Assessment")
    void AssessTeleological();

    /** Assess cognitive dimension */
    UFUNCTION(BlueprintCallable, Category = "Entelechy|Assessment")
    void AssessCognitive();

    /** Assess integrative dimension */
    UFUNCTION(BlueprintCallable, Category = "Entelechy|Assessment")
    void AssessIntegrative();

    /** Assess evolutionary dimension */
    UFUNCTION(BlueprintCallable, Category = "Entelechy|Assessment")
    void AssessEvolutionary();

    // ========================================
    // PUBLIC API - METRICS
    // ========================================

    /** Calculate overall fitness */
    UFUNCTION(BlueprintCallable, Category = "Entelechy|Metrics")
    float CalculateFitness() const;

    /** Get dimension health */
    UFUNCTION(BlueprintCallable, Category = "Entelechy|Metrics")
    float GetDimensionHealth(EEntelechyDimension Dimension) const;

    /** Get current development stage */
    UFUNCTION(BlueprintCallable, Category = "Entelechy|Metrics")
    EEntelechyStage GetDevelopmentStage() const;

    /** Get actualization percentage */
    UFUNCTION(BlueprintCallable, Category = "Entelechy|Metrics")
    float GetActualizationPercentage() const;

    // ========================================
    // PUBLIC API - EVOLUTION
    // ========================================

    /** Advance to next development stage if ready */
    UFUNCTION(BlueprintCallable, Category = "Entelechy|Evolution")
    bool TryAdvanceStage();

    /** Register component */
    UFUNCTION(BlueprintCallable, Category = "Entelechy|Evolution")
    void RegisterComponent(const FString& Name, EEntelechyDimension Dimension);

    /** Update component health */
    UFUNCTION(BlueprintCallable, Category = "Entelechy|Evolution")
    void UpdateComponentHealth(const FString& Name, float Health);

    /** Add development milestone */
    UFUNCTION(BlueprintCallable, Category = "Entelechy|Evolution")
    void AddMilestone(EDevelopmentPhase Phase, const FString& Milestone);

    // ========================================
    // PUBLIC API - GENOME
    // ========================================

    /** Get genome */
    UFUNCTION(BlueprintCallable, Category = "Entelechy|Genome")
    FEntelechyGenome GetGenome() const;

    /** Mutate genome (for evolutionary learning) */
    UFUNCTION(BlueprintCallable, Category = "Entelechy|Genome")
    void MutateGenome(float MutationRate);

    /** Crossover with another genome */
    UFUNCTION(BlueprintCallable, Category = "Entelechy|Genome")
    FEntelechyGenome CrossoverGenome(const FEntelechyGenome& Other) const;

protected:
    virtual void BeginPlay() override;

private:
    /** Accumulated time for processing */
    float AccumulatedTime = 0.0f;

    /** Initialize dimension weights */
    void InitializeDimensionWeights();

    /** Initialize genome */
    void InitializeGenome(const FString& SystemId);

    /** Initialize development phases */
    void InitializeDevelopmentPhases();

    /** Update development stage based on fitness */
    void UpdateDevelopmentStage();

    /** Update metrics */
    void UpdateMetrics();

    /** Calculate dimension health */
    float CalculateOntologicalHealth() const;
    float CalculateTeleologicalHealth() const;
    float CalculateCognitiveHealth() const;
    float CalculateIntegrativeHealth() const;
    float CalculateEvolutionaryHealth() const;
};
