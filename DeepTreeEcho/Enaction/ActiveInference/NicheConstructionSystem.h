// NicheConstructionSystem.h
// Niche Construction System for Deep Tree Echo
// Implements ecological niche construction through active inference

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NicheConstructionSystem.generated.h"

// Forward declarations
class UActiveInferenceEngine;
class UEchobeatsStreamEngine;
class USys6LCMClockSynchronizer;
class UCognitiveMemoryManager;

/**
 * Niche construction type
 */
UENUM(BlueprintType)
enum class ENicheConstructionType : uint8
{
    Perturbation    UMETA(DisplayName = "Perturbation (Environment Modification)"),
    Relocation      UMETA(DisplayName = "Relocation (Environment Selection)"),
    Inheritance     UMETA(DisplayName = "Inheritance (Cultural Transmission)"),
    Scaffolding     UMETA(DisplayName = "Scaffolding (Cognitive Extension)")
};

/**
 * Affordance type
 */
UENUM(BlueprintType)
enum class EAffordanceType : uint8
{
    Physical        UMETA(DisplayName = "Physical"),
    Social          UMETA(DisplayName = "Social"),
    Cognitive       UMETA(DisplayName = "Cognitive"),
    Informational   UMETA(DisplayName = "Informational"),
    Temporal        UMETA(DisplayName = "Temporal")
};

/**
 * Niche dimension
 */
UENUM(BlueprintType)
enum class ENicheDimension : uint8
{
    Spatial         UMETA(DisplayName = "Spatial"),
    Temporal        UMETA(DisplayName = "Temporal"),
    Social          UMETA(DisplayName = "Social"),
    Informational   UMETA(DisplayName = "Informational"),
    Cognitive       UMETA(DisplayName = "Cognitive")
};

/**
 * Ecological affordance
 */
USTRUCT(BlueprintType)
struct FEcologicalAffordance
{
    GENERATED_BODY()

    /** Affordance ID */
    UPROPERTY(BlueprintReadOnly)
    FString AffordanceID;

    /** Affordance type */
    UPROPERTY(BlueprintReadOnly)
    EAffordanceType Type = EAffordanceType::Physical;

    /** Action possibilities */
    UPROPERTY(BlueprintReadOnly)
    TArray<FString> ActionPossibilities;

    /** Salience (0-1) */
    UPROPERTY(BlueprintReadOnly)
    float Salience = 0.5f;

    /** Relevance to current goals (0-1) */
    UPROPERTY(BlueprintReadOnly)
    float GoalRelevance = 0.5f;

    /** Modification potential (0-1) */
    UPROPERTY(BlueprintReadOnly)
    float ModificationPotential = 0.5f;

    /** Persistence (how long affordance lasts) */
    UPROPERTY(BlueprintReadOnly)
    float Persistence = 1.0f;

    /** Discovery timestamp */
    UPROPERTY(BlueprintReadOnly)
    float DiscoveryTime = 0.0f;
};

/**
 * Niche state
 */
USTRUCT(BlueprintType)
struct FNicheState
{
    GENERATED_BODY()

    /** Niche ID */
    UPROPERTY(BlueprintReadOnly)
    FString NicheID;

    /** Current state vector */
    UPROPERTY(BlueprintReadOnly)
    TArray<float> StateVector;

    /** Preferred state vector */
    UPROPERTY(BlueprintReadOnly)
    TArray<float> PreferredState;

    /** Fitness landscape gradient */
    UPROPERTY(BlueprintReadOnly)
    TArray<float> FitnessGradient;

    /** Niche stability (0-1) */
    UPROPERTY(BlueprintReadOnly)
    float Stability = 0.5f;

    /** Niche complexity */
    UPROPERTY(BlueprintReadOnly)
    float Complexity = 0.5f;

    /** Agent-environment coupling strength */
    UPROPERTY(BlueprintReadOnly)
    float CouplingStrength = 0.5f;

    /** Last modification time */
    UPROPERTY(BlueprintReadOnly)
    float LastModification = 0.0f;
};

/**
 * Niche modification action
 */
USTRUCT(BlueprintType)
struct FNicheModification
{
    GENERATED_BODY()

    /** Modification ID */
    UPROPERTY(BlueprintReadOnly)
    int32 ModificationID = 0;

    /** Construction type */
    UPROPERTY(BlueprintReadOnly)
    ENicheConstructionType Type = ENicheConstructionType::Perturbation;

    /** Target dimension */
    UPROPERTY(BlueprintReadOnly)
    ENicheDimension Dimension = ENicheDimension::Spatial;

    /** Modification vector (change to niche state) */
    UPROPERTY(BlueprintReadOnly)
    TArray<float> ModificationVector;

    /** Expected fitness change */
    UPROPERTY(BlueprintReadOnly)
    float ExpectedFitnessChange = 0.0f;

    /** Actual fitness change */
    UPROPERTY(BlueprintReadOnly)
    float ActualFitnessChange = 0.0f;

    /** Energy cost */
    UPROPERTY(BlueprintReadOnly)
    float EnergyCost = 0.0f;

    /** Success (0-1) */
    UPROPERTY(BlueprintReadOnly)
    float Success = 0.0f;

    /** Timestamp */
    UPROPERTY(BlueprintReadOnly)
    float Timestamp = 0.0f;
};

/**
 * Cognitive scaffold
 */
USTRUCT(BlueprintType)
struct FCognitiveScaffold
{
    GENERATED_BODY()

    /** Scaffold ID */
    UPROPERTY(BlueprintReadOnly)
    FString ScaffoldID;

    /** Scaffold type */
    UPROPERTY(BlueprintReadOnly)
    FString ScaffoldType;

    /** Cognitive function supported */
    UPROPERTY(BlueprintReadOnly)
    FString SupportedFunction;

    /** Offloading degree (0-1, how much cognition is offloaded) */
    UPROPERTY(BlueprintReadOnly)
    float OffloadingDegree = 0.0f;

    /** Reliability (0-1) */
    UPROPERTY(BlueprintReadOnly)
    float Reliability = 1.0f;

    /** Integration level (0-1, how well integrated with cognition) */
    UPROPERTY(BlueprintReadOnly)
    float IntegrationLevel = 0.5f;

    /** Creation time */
    UPROPERTY(BlueprintReadOnly)
    float CreationTime = 0.0f;
};

/**
 * Selective pressure
 */
USTRUCT(BlueprintType)
struct FSelectivePressure
{
    GENERATED_BODY()

    /** Pressure ID */
    UPROPERTY(BlueprintReadOnly)
    FString PressureID;

    /** Pressure type */
    UPROPERTY(BlueprintReadOnly)
    FString PressureType;

    /** Direction vector */
    UPROPERTY(BlueprintReadOnly)
    TArray<float> Direction;

    /** Intensity (0-1) */
    UPROPERTY(BlueprintReadOnly)
    float Intensity = 0.5f;

    /** Agent-induced (vs environmental) */
    UPROPERTY(BlueprintReadOnly)
    bool bAgentInduced = false;

    /** Persistence */
    UPROPERTY(BlueprintReadOnly)
    float Persistence = 1.0f;
};

/**
 * Fitness landscape
 */
USTRUCT(BlueprintType)
struct FFitnessLandscape
{
    GENERATED_BODY()

    /** Landscape ID */
    UPROPERTY(BlueprintReadOnly)
    FString LandscapeID;

    /** Dimensionality */
    UPROPERTY(BlueprintReadOnly)
    int32 Dimensions = 0;

    /** Current fitness */
    UPROPERTY(BlueprintReadOnly)
    float CurrentFitness = 0.5f;

    /** Local gradient */
    UPROPERTY(BlueprintReadOnly)
    TArray<float> LocalGradient;

    /** Ruggedness (0-1, how many local optima) */
    UPROPERTY(BlueprintReadOnly)
    float Ruggedness = 0.5f;

    /** Epistasis (0-1, interaction between dimensions) */
    UPROPERTY(BlueprintReadOnly)
    float Epistasis = 0.3f;

    /** Neutrality (0-1, flat regions) */
    UPROPERTY(BlueprintReadOnly)
    float Neutrality = 0.2f;

    /** Agent modifiability (0-1) */
    UPROPERTY(BlueprintReadOnly)
    float AgentModifiability = 0.5f;
};

/**
 * Inheritance channel
 */
USTRUCT(BlueprintType)
struct FInheritanceChannel
{
    GENERATED_BODY()

    /** Channel ID */
    UPROPERTY(BlueprintReadOnly)
    FString ChannelID;

    /** Channel type (genetic, epigenetic, behavioral, symbolic) */
    UPROPERTY(BlueprintReadOnly)
    FString ChannelType;

    /** Fidelity (0-1) */
    UPROPERTY(BlueprintReadOnly)
    float Fidelity = 0.9f;

    /** Bandwidth (information capacity) */
    UPROPERTY(BlueprintReadOnly)
    float Bandwidth = 1.0f;

    /** Latency (transmission delay) */
    UPROPERTY(BlueprintReadOnly)
    float Latency = 0.0f;

    /** Active */
    UPROPERTY(BlueprintReadOnly)
    bool bActive = true;
};

/**
 * Niche construction event
 */
USTRUCT(BlueprintType)
struct FNicheConstructionEvent
{
    GENERATED_BODY()

    /** Event ID */
    UPROPERTY(BlueprintReadOnly)
    int32 EventID = 0;

    /** Construction type */
    UPROPERTY(BlueprintReadOnly)
    ENicheConstructionType Type = ENicheConstructionType::Perturbation;

    /** Modification applied */
    UPROPERTY(BlueprintReadOnly)
    FNicheModification Modification;

    /** Niche state before */
    UPROPERTY(BlueprintReadOnly)
    FNicheState StateBefore;

    /** Niche state after */
    UPROPERTY(BlueprintReadOnly)
    FNicheState StateAfter;

    /** Fitness change */
    UPROPERTY(BlueprintReadOnly)
    float FitnessChange = 0.0f;

    /** Echobeat step */
    UPROPERTY(BlueprintReadOnly)
    int32 EchobeatStep = 0;

    /** LCM step */
    UPROPERTY(BlueprintReadOnly)
    int32 LCMStep = 0;

    /** Timestamp */
    UPROPERTY(BlueprintReadOnly)
    float Timestamp = 0.0f;
};

/**
 * Delegate declarations
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNicheModified, const FNicheModification&, Modification);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAffordanceDiscovered, const FEcologicalAffordance&, Affordance);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnScaffoldCreated, const FCognitiveScaffold&, Scaffold);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFitnessChanged, float, OldFitness, float, NewFitness);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSelectivePressureChanged, const FSelectivePressure&, Pressure);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNicheConstructionEvent, const FNicheConstructionEvent&, Event);

/**
 * Niche Construction System
 * 
 * Implements ecological niche construction for the Deep Tree Echo avatar.
 * Enables the agent to actively shape its environment to reduce uncertainty
 * and create favorable conditions for its existence.
 * 
 * Key features:
 * - Perturbation: Direct environment modification
 * - Relocation: Environment selection and navigation
 * - Inheritance: Cultural and informational transmission
 * - Scaffolding: Cognitive extension through tools and artifacts
 * - Affordance detection and exploitation
 * - Fitness landscape navigation and modification
 * - Integration with active inference for niche-aware planning
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UNicheConstructionSystem : public UActorComponent
{
    GENERATED_BODY()

public:
    UNicheConstructionSystem();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Niche state dimensionality */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Niche")
    int32 NicheDimensionality = 16;

    /** Maximum affordances to track */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Affordances")
    int32 MaxAffordances = 32;

    /** Affordance decay rate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Affordances")
    float AffordanceDecayRate = 0.01f;

    /** Maximum scaffolds */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Scaffolds")
    int32 MaxScaffolds = 16;

    /** Modification energy budget */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Energy")
    float ModificationEnergyBudget = 100.0f;

    /** Energy regeneration rate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Energy")
    float EnergyRegenerationRate = 1.0f;

    /** Fitness improvement threshold */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Fitness")
    float FitnessImprovementThreshold = 0.01f;

    /** Enable inheritance */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Inheritance")
    bool bEnableInheritance = true;

    /** Enable scaffolding */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Scaffolds")
    bool bEnableScaffolding = true;

    // ========================================
    // EVENTS
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnNicheModified OnNicheModified;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnAffordanceDiscovered OnAffordanceDiscovered;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnScaffoldCreated OnScaffoldCreated;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnFitnessChanged OnFitnessChanged;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnSelectivePressureChanged OnSelectivePressureChanged;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnNicheConstructionEvent OnNicheConstructionEvent;

    // ========================================
    // PUBLIC API - NICHE STATE
    // ========================================

    /** Get current niche state */
    UFUNCTION(BlueprintPure, Category = "Niche")
    FNicheState GetNicheState() const;

    /** Set preferred niche state */
    UFUNCTION(BlueprintCallable, Category = "Niche")
    void SetPreferredNicheState(const TArray<float>& PreferredState);

    /** Get niche-preference distance */
    UFUNCTION(BlueprintPure, Category = "Niche")
    float GetNichePreferenceDistance() const;

    /** Update niche state from observations */
    UFUNCTION(BlueprintCallable, Category = "Niche")
    void UpdateNicheState(const TArray<float>& Observations);

    /** Get niche stability */
    UFUNCTION(BlueprintPure, Category = "Niche")
    float GetNicheStability() const;

    // ========================================
    // PUBLIC API - AFFORDANCES
    // ========================================

    /** Detect affordances in current state */
    UFUNCTION(BlueprintCallable, Category = "Affordances")
    TArray<FEcologicalAffordance> DetectAffordances();

    /** Get affordance by ID */
    UFUNCTION(BlueprintPure, Category = "Affordances")
    FEcologicalAffordance GetAffordance(const FString& AffordanceID) const;

    /** Get most salient affordances */
    UFUNCTION(BlueprintPure, Category = "Affordances")
    TArray<FEcologicalAffordance> GetMostSalientAffordances(int32 Count) const;

    /** Get affordances by type */
    UFUNCTION(BlueprintPure, Category = "Affordances")
    TArray<FEcologicalAffordance> GetAffordancesByType(EAffordanceType Type) const;

    /** Exploit affordance */
    UFUNCTION(BlueprintCallable, Category = "Affordances")
    FNicheModification ExploitAffordance(const FString& AffordanceID);

    // ========================================
    // PUBLIC API - NICHE CONSTRUCTION
    // ========================================

    /** Plan niche modification */
    UFUNCTION(BlueprintCallable, Category = "Construction")
    FNicheModification PlanNicheModification(ENicheConstructionType Type, ENicheDimension Dimension);

    /** Execute niche modification */
    UFUNCTION(BlueprintCallable, Category = "Construction")
    FNicheConstructionEvent ExecuteNicheModification(const FNicheModification& Modification);

    /** Evaluate modification potential */
    UFUNCTION(BlueprintPure, Category = "Construction")
    float EvaluateModificationPotential(const FNicheModification& Modification) const;

    /** Get modification history */
    UFUNCTION(BlueprintPure, Category = "Construction")
    TArray<FNicheModification> GetModificationHistory() const;

    /** Undo last modification */
    UFUNCTION(BlueprintCallable, Category = "Construction")
    bool UndoLastModification();

    // ========================================
    // PUBLIC API - COGNITIVE SCAFFOLDING
    // ========================================

    /** Create cognitive scaffold */
    UFUNCTION(BlueprintCallable, Category = "Scaffolding")
    FCognitiveScaffold CreateScaffold(const FString& ScaffoldType, const FString& SupportedFunction);

    /** Get scaffold by ID */
    UFUNCTION(BlueprintPure, Category = "Scaffolding")
    FCognitiveScaffold GetScaffold(const FString& ScaffoldID) const;

    /** Get all scaffolds */
    UFUNCTION(BlueprintPure, Category = "Scaffolding")
    TArray<FCognitiveScaffold> GetAllScaffolds() const;

    /** Use scaffold */
    UFUNCTION(BlueprintCallable, Category = "Scaffolding")
    float UseScaffold(const FString& ScaffoldID);

    /** Remove scaffold */
    UFUNCTION(BlueprintCallable, Category = "Scaffolding")
    bool RemoveScaffold(const FString& ScaffoldID);

    /** Get scaffold integration level */
    UFUNCTION(BlueprintPure, Category = "Scaffolding")
    float GetScaffoldIntegration(const FString& ScaffoldID) const;

    // ========================================
    // PUBLIC API - FITNESS LANDSCAPE
    // ========================================

    /** Get fitness landscape */
    UFUNCTION(BlueprintPure, Category = "Fitness")
    FFitnessLandscape GetFitnessLandscape() const;

    /** Compute current fitness */
    UFUNCTION(BlueprintCallable, Category = "Fitness")
    float ComputeCurrentFitness();

    /** Compute fitness gradient */
    UFUNCTION(BlueprintCallable, Category = "Fitness")
    TArray<float> ComputeFitnessGradient();

    /** Modify fitness landscape */
    UFUNCTION(BlueprintCallable, Category = "Fitness")
    void ModifyFitnessLandscape(const TArray<float>& Modification);

    /** Find local optimum */
    UFUNCTION(BlueprintCallable, Category = "Fitness")
    TArray<float> FindLocalOptimum(int32 MaxIterations);

    // ========================================
    // PUBLIC API - SELECTIVE PRESSURES
    // ========================================

    /** Get active selective pressures */
    UFUNCTION(BlueprintPure, Category = "Pressures")
    TArray<FSelectivePressure> GetSelectivePressures() const;

    /** Add selective pressure */
    UFUNCTION(BlueprintCallable, Category = "Pressures")
    void AddSelectivePressure(const FSelectivePressure& Pressure);

    /** Remove selective pressure */
    UFUNCTION(BlueprintCallable, Category = "Pressures")
    void RemoveSelectivePressure(const FString& PressureID);

    /** Compute net selective force */
    UFUNCTION(BlueprintPure, Category = "Pressures")
    TArray<float> ComputeNetSelectiveForce() const;

    // ========================================
    // PUBLIC API - INHERITANCE
    // ========================================

    /** Get inheritance channels */
    UFUNCTION(BlueprintPure, Category = "Inheritance")
    TArray<FInheritanceChannel> GetInheritanceChannels() const;

    /** Transmit information through channel */
    UFUNCTION(BlueprintCallable, Category = "Inheritance")
    bool TransmitInformation(const FString& ChannelID, const TArray<float>& Information);

    /** Receive information from channel */
    UFUNCTION(BlueprintCallable, Category = "Inheritance")
    TArray<float> ReceiveInformation(const FString& ChannelID);

    /** Create inheritance channel */
    UFUNCTION(BlueprintCallable, Category = "Inheritance")
    FInheritanceChannel CreateInheritanceChannel(const FString& ChannelType);

    // ========================================
    // PUBLIC API - ENERGY MANAGEMENT
    // ========================================

    /** Get current energy */
    UFUNCTION(BlueprintPure, Category = "Energy")
    float GetCurrentEnergy() const;

    /** Get energy budget */
    UFUNCTION(BlueprintPure, Category = "Energy")
    float GetEnergyBudget() const;

    /** Consume energy */
    UFUNCTION(BlueprintCallable, Category = "Energy")
    bool ConsumeEnergy(float Amount);

    /** Regenerate energy */
    UFUNCTION(BlueprintCallable, Category = "Energy")
    void RegenerateEnergy(float DeltaTime);

    // ========================================
    // PUBLIC API - SYS6 INTEGRATION
    // ========================================

    /** Process sys6 step for niche construction */
    UFUNCTION(BlueprintCallable, Category = "Sys6")
    void ProcessSys6Step(int32 LCMStep);

    /** Get niche construction mode for echobeat step */
    UFUNCTION(BlueprintPure, Category = "Sys6")
    ENicheConstructionType GetConstructionModeForStep(int32 EchobeatStep) const;

protected:
    // Component references
    UPROPERTY()
    UActiveInferenceEngine* ActiveInferenceEngine;

    UPROPERTY()
    UEchobeatsStreamEngine* EchobeatsEngine;

    UPROPERTY()
    USys6LCMClockSynchronizer* LCMClock;

    UPROPERTY()
    UCognitiveMemoryManager* MemoryManager;

    // Internal state
    FNicheState CurrentNicheState;
    FFitnessLandscape FitnessLandscape;
    TArray<FEcologicalAffordance> DetectedAffordances;
    TArray<FCognitiveScaffold> CognitiveScaffolds;
    TArray<FSelectivePressure> SelectivePressures;
    TArray<FInheritanceChannel> InheritanceChannels;
    TArray<FNicheModification> ModificationHistory;
    TArray<FNicheConstructionEvent> ConstructionEvents;

    float CurrentEnergy = 0.0f;
    int32 NextModificationID = 1;
    int32 NextEventID = 1;

    // Internal methods
    void FindComponentReferences();
    void InitializeNicheState();
    void InitializeFitnessLandscape();
    void InitializeInheritanceChannels();
    void UpdateAffordanceDecay(float DeltaTime);
    void UpdateScaffoldIntegration(float DeltaTime);
    float ComputeFitnessAtState(const TArray<float>& State) const;
    FEcologicalAffordance GenerateAffordance(EAffordanceType Type);
    float ComputeModificationCost(const FNicheModification& Modification) const;
    void ApplyModificationToNiche(const FNicheModification& Modification);
    void BroadcastConstructionEvent(const FNicheConstructionEvent& Event);
};
