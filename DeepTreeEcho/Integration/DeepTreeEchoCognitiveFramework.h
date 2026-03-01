/**
 * @file DeepTreeEchoCognitiveFramework.h
 * @brief Complete integration of the Deep Tree Echo cognitive framework
 * 
 * This module implements the neuro-symbolic AI architecture where:
 * - Deep Tree Echo provides neural perception and intuitive vision
 * - Unreal Engine provides symbolic differential solver logic
 * 
 * The framework integrates:
 * - Echo State Networks for temporal pattern processing
 * - P-System membranes for hierarchical boundary management
 * - Butcher B-Series for differential temporal integration
 * - Hypergraph memory for multi-relational knowledge
 * - 4E Cognition (Embodied, Embedded, Enacted, Extended)
 * - Relevance Realization for wisdom cultivation
 * - Holistic Metamodel (Eric Schwarz's organizational theory)
 */
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeepTreeEchoCognitiveFramework.generated.h"

// Forward declarations
class UDeepTreeEchoReservoir;
class UReservoirCognitiveIntegration;
class UDeepCognitiveBridge;
class UTetradicReservoirIntegration;
class UEmbodied4ECognition;
class UEchobeatsReservoirDynamics;
class UWisdomCultivation;
class UHypergraphMemorySystem;
class UAutognosisSystem;

/**
 * @brief Membrane type enumeration following P-System architecture
 */
UENUM(BlueprintType)
enum class EMembraneType : uint8
{
    Root            UMETA(DisplayName = "Root Membrane"),
    Cognitive       UMETA(DisplayName = "Cognitive Membrane"),
    Memory          UMETA(DisplayName = "Memory Membrane"),
    Reasoning       UMETA(DisplayName = "Reasoning Membrane"),
    Grammar         UMETA(DisplayName = "Grammar Membrane"),
    Extension       UMETA(DisplayName = "Extension Membrane"),
    Security        UMETA(DisplayName = "Security Membrane")
};

/**
 * @brief Cognitive dimension for 4E cognition
 */
UENUM(BlueprintType)
enum class ECognitiveDimension : uint8
{
    Embodied        UMETA(DisplayName = "Embodied"),
    Embedded        UMETA(DisplayName = "Embedded"),
    Enacted         UMETA(DisplayName = "Enacted"),
    Extended        UMETA(DisplayName = "Extended")
};

/**
 * @brief Holistic metamodel level (Eric Schwarz)
 */
UENUM(BlueprintType)
enum class EMetamodelLevel : uint8
{
    Monad           UMETA(DisplayName = "1 - Hieroglyphic Monad"),
    Dual            UMETA(DisplayName = "2 - Dual Complementarity"),
    Triad           UMETA(DisplayName = "3 - Triadic Primitives"),
    Quaternary      UMETA(DisplayName = "4 - Self-Stabilizing Cycles"),
    Septenary       UMETA(DisplayName = "7 - Triad Production Steps"),
    Ennead          UMETA(DisplayName = "9 - Ennead Meta-Systems"),
    Hendecad        UMETA(DisplayName = "11 - Evolutionary Helix")
};

/**
 * @brief Dynamic stream type for organizational dynamics
 */
UENUM(BlueprintType)
enum class EDynamicStream : uint8
{
    Entropic        UMETA(DisplayName = "Entropic (en-tropis → auto-vortis → auto-morphosis)"),
    Negnentropic    UMETA(DisplayName = "Negnentropic (negen-tropis → auto-stasis → auto-poiesis)"),
    Identity        UMETA(DisplayName = "Identity (iden-tropis → auto-gnosis → auto-genesis)")
};

/**
 * @brief Membrane state structure
 */
USTRUCT(BlueprintType)
struct FMembraneState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EMembraneType Type = EMembraneType::Root;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Permeability = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Coherence = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<int32> ChildMembranes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FString, float> Contents;
};

/**
 * @brief 4E Cognition state structure
 */
USTRUCT(BlueprintType)
struct F4ECognitionState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EmbodiedActivation = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EmbeddedActivation = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EnactedActivation = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ExtendedActivation = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CrossDimensionalCoherence = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ECognitiveDimension DominantDimension = ECognitiveDimension::Embodied;
};

/**
 * @brief Metamodel state structure
 */
USTRUCT(BlueprintType)
struct FMetamodelState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MonadCoherence = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DualTension = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TriadicBalance = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 CurrentPhase = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 TriadProductionStep = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<float> EnneadAspects;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 EvolutionaryHelixStage = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 SpiralLevel = 1;
};

/**
 * @brief Dynamic stream state structure
 */
USTRUCT(BlueprintType)
struct FDynamicStreamState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EntropicEnergy = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float NegnentropicStability = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float IdentityCoherence = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EDynamicStream DominantStream = EDynamicStream::Identity;
};

/**
 * @brief Relevance realization event
 */
USTRUCT(BlueprintType)
struct FRelevanceEvent
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Timestamp = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RelevanceScore = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Context;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> SalientFeatures;
};

/**
 * @brief Delegate for relevance realization events
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRelevanceRealized, int32, CognitiveStep, float, RelevanceScore);

/**
 * @brief Delegate for wisdom cultivation events
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnWisdomCultivated, FString, InsightType, float, WisdomScore, FString, Description);

/**
 * @brief Delegate for metamodel transitions
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMetamodelTransition, EMetamodelLevel, FromLevel, EMetamodelLevel, ToLevel);

/**
 * @brief Membrane lifecycle event type enumeration
 * Used to track membrane creation, division, and dissolution events
 */
UENUM(BlueprintType)
enum class EMembraneLifecycleEvent : uint8
{
    Created         UMETA(DisplayName = "Membrane Created"),
    Divided         UMETA(DisplayName = "Membrane Divided"),
    Dissolved       UMETA(DisplayName = "Membrane Dissolved"),
    Merged          UMETA(DisplayName = "Membrane Merged")
};

/**
 * @brief Result structure for membrane division operations
 */
USTRUCT(BlueprintType)
struct FMembraneDivisionResult
{
    GENERATED_BODY()

    /** Whether the division was successful */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bSuccess = false;

    /** Type of the parent membrane that was divided */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EMembraneType ParentType = EMembraneType::Root;

    /** Types of the child membranes created */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<EMembraneType> ChildTypes;

    /** Error message if division failed */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ErrorMessage;

    /** Division ratio used (how contents were split) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DivisionRatio = 0.5f;
};

/**
 * @brief Result structure for membrane dissolution operations
 */
USTRUCT(BlueprintType)
struct FMembraneDissolutionResult
{
    GENERATED_BODY()

    /** Whether the dissolution was successful */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bSuccess = false;

    /** Type of the membrane that was dissolved */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EMembraneType DissolvedType = EMembraneType::Root;

    /** Type of the parent membrane that received contents */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EMembraneType ReceiverType = EMembraneType::Root;

    /** Contents that were propagated to parent */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FString, float> PropagatedContents;

    /** Error message if dissolution failed */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ErrorMessage;
};

/**
 * @brief Delegate for membrane lifecycle events (Feature F1.3.3)
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnMembraneLifecycleEvent, EMembraneLifecycleEvent, EventType, EMembraneType, MembraneType, FString, Details);

/**
 * @class UDeepTreeEchoCognitiveFramework
 * @brief Main component integrating all Deep Tree Echo cognitive systems
 * 
 * This component serves as the central orchestrator for the Deep Tree Echo
 * cognitive framework, managing:
 * - P-System membrane hierarchy
 * - Echo state reservoir computing
 * - 4E embodied cognition
 * - Holistic metamodel dynamics
 * - Relevance realization optimization
 * - Wisdom cultivation
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UDeepTreeEchoCognitiveFramework : public UActorComponent
{
    GENERATED_BODY()

public:
    UDeepTreeEchoCognitiveFramework();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================================================
    // Configuration
    // ========================================================================

    /** Reservoir spectral radius (echo property) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration|Reservoir")
    float SpectralRadius = 0.95f;

    /** Reservoir input scaling */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration|Reservoir")
    float InputScaling = 0.5f;

    /** Reservoir leak rate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration|Reservoir")
    float LeakRate = 0.3f;

    /** Relevance realization threshold */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration|Relevance")
    float RelevanceThreshold = 0.7f;

    /** Enable autognosis (self-reflection) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration|Introspection")
    bool bEnableAutognosis = true;

    // ========================================================================
    // State Access
    // ========================================================================

    /** Get current 4E cognition state */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|State")
    F4ECognitionState Get4ECognitionState() const { return CognitionState; }

    /** Get current metamodel state */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|State")
    FMetamodelState GetMetamodelState() const { return MetamodelState; }

    /** Get current dynamic stream state */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|State")
    FDynamicStreamState GetDynamicStreamState() const { return StreamState; }

    /** Get membrane state by type */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|State")
    FMembraneState GetMembraneState(EMembraneType Type) const;

    // ========================================================================
    // Cognitive Processing
    // ========================================================================

    /** Process sensory input through the cognitive framework */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Processing")
    void ProcessSensoryInput(const TArray<float>& SensoryData);

    /** Execute one cognitive cycle step */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Processing")
    void ExecuteCognitiveStep();

    /** Compute relevance realization for current state */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Processing")
    float ComputeRelevanceRealization();

    /** Cultivate wisdom from accumulated experiences */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Processing")
    void CultivateWisdom();

    // ========================================================================
    // 4E Cognition Interface
    // ========================================================================

    /** Update embodied dimension (body schema, proprioception) */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|4E")
    void UpdateEmbodiedDimension(float BodyTension, float Groundedness);

    /** Update embedded dimension (environmental affordances) */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|4E")
    void UpdateEmbeddedDimension(float EnvironmentalEngagement, const TArray<FString>& Affordances);

    /** Update enacted dimension (sensorimotor contingencies) */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|4E")
    void UpdateEnactedDimension(float ActionReadiness, float SensorimotorCoupling);

    /** Update extended dimension (cognitive tool integration) */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|4E")
    void UpdateExtendedDimension(float ToolIntegration, const TArray<FString>& ExtendedTools);

    // ========================================================================
    // Membrane Interface
    // ========================================================================

    /** Set membrane permeability */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Membrane")
    void SetMembranePermeability(EMembraneType Type, float Permeability);

    /** Propagate information through membrane hierarchy */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Membrane")
    void PropagateMembraneContents(EMembraneType FromMembrane, EMembraneType ToMembrane, const FString& ContentKey);

    // ========================================================================
    // Membrane Lifecycle Operations (Feature F1.3.3)
    // ========================================================================

    /**
     * @brief Create a new membrane as a child of an existing parent membrane
     * 
     * Creates a new membrane with the specified type and adds it to the hierarchy.
     * The new membrane inherits properties from the parent based on permeability.
     * 
     * @param NewType The type of membrane to create
     * @param ParentType The parent membrane to attach to
     * @param InitialPermeability Initial permeability value (0.0-1.0)
     * @param InitialCoherence Initial coherence value (0.0-1.0)
     * @return True if creation was successful, false otherwise
     */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Membrane|Lifecycle")
    bool CreateMembrane(EMembraneType NewType, EMembraneType ParentType, float InitialPermeability = 0.5f, float InitialCoherence = 1.0f);

    /**
     * @brief Divide a membrane into two child membranes
     * 
     * Performs P-System membrane division, creating two child membranes from a parent.
     * Contents are distributed between children based on the division ratio.
     * The parent membrane becomes a container for the new children.
     * 
     * @param MembraneType The membrane to divide
     * @param DivisionRatio How to split contents between children (0.0-1.0, default 0.5 = equal)
     * @param ChildType1 Type for the first child membrane
     * @param ChildType2 Type for the second child membrane
     * @return Division result containing success status and child information
     */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Membrane|Lifecycle")
    FMembraneDivisionResult DivideMembrane(EMembraneType MembraneType, float DivisionRatio, EMembraneType ChildType1, EMembraneType ChildType2);

    /**
     * @brief Dissolve a membrane and merge its contents into parent
     * 
     * Performs P-System membrane dissolution, removing the membrane and 
     * propagating its contents to the parent membrane based on permeability.
     * Child membranes are re-parented to the dissolved membrane's parent.
     * 
     * @param MembraneType The membrane to dissolve
     * @return Dissolution result containing success status and propagated contents
     */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Membrane|Lifecycle")
    FMembraneDissolutionResult DissolveMembrane(EMembraneType MembraneType);

    /**
     * @brief Check if a membrane can be safely dissolved
     * 
     * Validates dissolution preconditions:
     * - Membrane must exist
     * - Cannot dissolve root membrane
     * - Must have a valid parent to receive contents
     * 
     * @param MembraneType The membrane to check
     * @return True if the membrane can be dissolved, false otherwise
     */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Membrane|Lifecycle")
    bool CanDissolveMembrane(EMembraneType MembraneType) const;

    /**
     * @brief Check if a membrane can be divided
     * 
     * Validates division preconditions:
     * - Membrane must exist
     * - Child types must not already exist
     * - Membrane must have sufficient coherence
     * 
     * @param MembraneType The membrane to check
     * @param ChildType1 First proposed child type
     * @param ChildType2 Second proposed child type
     * @return True if the membrane can be divided, false otherwise
     */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Membrane|Lifecycle")
    bool CanDivideMembrane(EMembraneType MembraneType, EMembraneType ChildType1, EMembraneType ChildType2) const;

    /**
     * @brief Get the parent membrane type for a given membrane
     * 
     * @param MembraneType The membrane to find parent for
     * @param OutParentType Output parameter for parent type
     * @return True if parent was found, false if membrane is root or doesn't exist
     */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Membrane|Lifecycle")
    bool GetParentMembrane(EMembraneType MembraneType, EMembraneType& OutParentType) const;

    // ========================================================================
    // Metamodel Interface
    // ========================================================================

    /** Advance metamodel phase */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Metamodel")
    void AdvanceMetamodelPhase();

    /** Process triad production step */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Metamodel")
    void ProcessTriadProductionStep();

    /** Evolve on the evolutionary helix */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Metamodel")
    void EvolveOnHelix();

    // ========================================================================
    // Events
    // ========================================================================

    /** Event fired when relevance is realized */
    UPROPERTY(BlueprintAssignable, Category = "DeepTreeEcho|Events")
    FOnRelevanceRealized OnRelevanceRealized;

    /** Event fired when wisdom is cultivated */
    UPROPERTY(BlueprintAssignable, Category = "DeepTreeEcho|Events")
    FOnWisdomCultivated OnWisdomCultivated;

    /** Event fired on metamodel level transition */
    UPROPERTY(BlueprintAssignable, Category = "DeepTreeEcho|Events")
    FOnMetamodelTransition OnMetamodelTransition;

    /** Event fired on membrane lifecycle changes (Feature F1.3.3) */
    UPROPERTY(BlueprintAssignable, Category = "DeepTreeEcho|Events")
    FOnMembraneLifecycleEvent OnMembraneLifecycleEvent;

protected:
    // ========================================================================
    // Internal State
    // ========================================================================

    /** Current 4E cognition state */
    UPROPERTY()
    F4ECognitionState CognitionState;

    /** Current metamodel state */
    UPROPERTY()
    FMetamodelState MetamodelState;

    /** Current dynamic stream state */
    UPROPERTY()
    FDynamicStreamState StreamState;

    /** Membrane states */
    UPROPERTY()
    TMap<EMembraneType, FMembraneState> Membranes;

    /** Relevance event history */
    UPROPERTY()
    TArray<FRelevanceEvent> RelevanceHistory;

    /** Current cognitive cycle step (1-12) */
    UPROPERTY()
    int32 CurrentCognitiveStep = 1;

    // ========================================================================
    // Internal Methods
    // ========================================================================

    /** Initialize membrane hierarchy */
    void InitializeMembranes();

    /** Initialize metamodel state */
    void InitializeMetamodel();

    /** Update dynamic streams */
    void UpdateDynamicStreams(float DeltaTime);

    /** Compute cross-dimensional coherence */
    float ComputeCrossDimensionalCoherence() const;

    /** Identify dominant 4E dimension */
    ECognitiveDimension IdentifyDominantDimension() const;

    /** Process OEIS A000081 nested shell structure */
    void ProcessNestedShells();

    /** Integrate with reservoir computing */
    void IntegrateWithReservoir(const TArray<float>& Input);

    /** Map 4E state to avatar expression hints */
    TMap<FString, float> Map4EToExpressionHints() const;

    /** Find parent membrane type for a given membrane (internal helper) */
    bool FindParentMembrane(EMembraneType ChildType, EMembraneType& OutParentType) const;

    /** Remove membrane from parent's child list (internal helper) */
    void RemoveChildFromParent(EMembraneType ChildType, EMembraneType ParentType);

    /** Add membrane to parent's child list (internal helper) */
    void AddChildToParent(EMembraneType ChildType, EMembraneType ParentType);
};
