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
};
