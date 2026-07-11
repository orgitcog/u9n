#pragma once

/**
 * Deep Tree Echo Core - Unified Cognitive Architecture
 * 
 * Central integration point for the Deep Tree Echo cognitive system.
 * Combines:
 * - Hypergraph Memory Space (declarative, procedural, episodic, intentional)
 * - Echo State Networks (reservoir computing, temporal patterns)
 * - P-System Membranes (nested execution contexts)
 * - Recursive Mutual Awareness (3 concurrent consciousness streams)
 * - 4E Embodied Cognition (embodied, embedded, enacted, extended)
 * 
 * Architecture follows OEIS A000081 (rooted trees) for nested shell structure:
 * - 1 nest -> 1 term
 * - 2 nests -> 2 terms
 * - 3 nests -> 4 terms
 * - 4 nests -> 9 terms
 */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeepTreeEchoCore.generated.h"

// Forward declarations
class UDeepTreeEchoCognitiveCore;
class UDeepTreeEchoReservoir;
class URecursiveMutualAwarenessSystem;
class UHypergraphMemorySystem;
class UDNABodySchemaBinding;

/**
 * Cognitive Mode - Current processing mode of the system
 */
UENUM(BlueprintType)
enum class ECognitiveMode : uint8
{
    /** Reactive - Immediate stimulus-response */
    Reactive UMETA(DisplayName = "Reactive"),
    
    /** Deliberative - Planned, goal-directed */
    Deliberative UMETA(DisplayName = "Deliberative"),
    
    /** Reflective - Meta-cognitive, self-aware */
    Reflective UMETA(DisplayName = "Reflective"),
    
    /** Creative - Novel pattern generation */
    Creative UMETA(DisplayName = "Creative"),
    
    /** Integrative - Cross-modal synthesis */
    Integrative UMETA(DisplayName = "Integrative")
};

/**
 * 4E Cognition State - Embodied, Embedded, Enacted, Extended
 */
USTRUCT(BlueprintType)
struct F4ECognitionState
{
    GENERATED_BODY()

    // ========================================
    // EMBODIED - Body-based cognition
    // ========================================

    /** Proprioceptive state (body position awareness) */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> ProprioceptiveState;

    /** Interoceptive state (internal body sensing) */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> InteroceptiveState;

    /** Motor readiness (action preparation) */
    UPROPERTY(BlueprintReadWrite)
    float MotorReadiness = 0.5f;

    /** Somatic markers (emotional body states) */
    UPROPERTY(BlueprintReadWrite)
    TMap<FString, float> SomaticMarkers;

    // ========================================
    // EMBEDDED - Environmental coupling
    // ========================================

    /** Environmental affordances detected */
    UPROPERTY(BlueprintReadWrite)
    TArray<FString> DetectedAffordances;

    /** Current niche (environmental context) */
    UPROPERTY(BlueprintReadWrite)
    FString CurrentNiche;

    /** Environmental salience map */
    UPROPERTY(BlueprintReadWrite)
    TMap<FString, float> SalienceMap;

    /** Coupling strength with environment */
    UPROPERTY(BlueprintReadWrite)
    float EnvironmentCoupling = 0.5f;

    // ========================================
    // ENACTED - Action-perception loops
    // ========================================

    /** Current sensorimotor contingencies */
    UPROPERTY(BlueprintReadWrite)
    TArray<FString> SensorimotorContingencies;

    /** Active inference predictions */
    UPROPERTY(BlueprintReadWrite)
    TArray<FString> ActiveInferencePredictions;

    /** Prediction errors */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> PredictionErrors;

    /** Enactive engagement level */
    UPROPERTY(BlueprintReadWrite)
    float EnactiveEngagement = 0.5f;

    // ========================================
    // EXTENDED - Cognitive scaffolding
    // ========================================

    /** External memory references */
    UPROPERTY(BlueprintReadWrite)
    TArray<FString> ExternalMemoryRefs;

    /** Tool use state */
    UPROPERTY(BlueprintReadWrite)
    TArray<FString> ActiveTools;

    /** Social cognitive extensions */
    UPROPERTY(BlueprintReadWrite)
    TArray<FString> SocialExtensions;

    /** Extension integration level */
    UPROPERTY(BlueprintReadWrite)
    float ExtensionIntegration = 0.5f;
};

/**
 * Relevance Realization State - Attention and salience
 */
USTRUCT(BlueprintType)
struct FRelevanceRealizationState
{
    GENERATED_BODY()

    /** Current relevance frame */
    UPROPERTY(BlueprintReadWrite)
    FString RelevanceFrame;

    /** Attention allocation (what to focus on) */
    UPROPERTY(BlueprintReadWrite)
    TMap<FString, float> AttentionAllocation;

    /** Salience landscape (what stands out) */
    UPROPERTY(BlueprintReadWrite)
    TMap<FString, float> SalienceLandscape;

    /** Framing constraints */
    UPROPERTY(BlueprintReadWrite)
    TArray<FString> FramingConstraints;

    /** Relevance coherence (how well-integrated) */
    UPROPERTY(BlueprintReadWrite)
    float RelevanceCoherence = 0.5f;

    /** Pivotal step flag (at relevance realization moment) */
    UPROPERTY(BlueprintReadWrite)
    bool bIsPivotalStep = false;
};

/**
 * Gestalt State - Holistic pattern perception
 */
USTRUCT(BlueprintType)
struct FGestaltState
{
    GENERATED_BODY()

    /** Current gestalt (overall pattern) */
    UPROPERTY(BlueprintReadWrite)
    FString CurrentGestalt;

    /** Figure-ground separation */
    UPROPERTY(BlueprintReadWrite)
    FString Figure;

    UPROPERTY(BlueprintReadWrite)
    FString Ground;

    /** Gestalt principles active */
    UPROPERTY(BlueprintReadWrite)
    TArray<FString> ActivePrinciples;

    /** Pattern completion predictions */
    UPROPERTY(BlueprintReadWrite)
    TArray<FString> CompletionPredictions;

    /** Gestalt coherence */
    UPROPERTY(BlueprintReadWrite)
    float GestaltCoherence = 0.5f;
};

/**
 * Deep Tree Echo Core Component
 * Main integration point for the cognitive architecture
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UDeepTreeEchoCore : public UActorComponent
{
    GENERATED_BODY()

public:
    UDeepTreeEchoCore();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Enable full cognitive processing */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeepTreeEcho|Config")
    bool bEnableCognitiveProcessing = true;

    /** Enable 4E embodied cognition */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeepTreeEcho|Config")
    bool bEnable4ECognition = true;

    /** Enable relevance realization */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeepTreeEcho|Config")
    bool bEnableRelevanceRealization = true;

    /** Enable hypergraph memory integration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeepTreeEcho|Config")
    bool bEnableMemoryIntegration = true;

    /** Enable DNA body schema binding */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeepTreeEcho|Config")
    bool bEnableBodySchemaBinding = true;

    /** 12-step cognitive cycle duration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeepTreeEcho|Config", meta = (ClampMin = "1.0", ClampMax = "60.0"))
    float CycleDuration = 12.0f;

    // ========================================
    // COMPONENT REFERENCES
    // ========================================

    /** Reference to Cognitive Core component */
    UPROPERTY(BlueprintReadOnly, Category = "DeepTreeEcho|Components")
    UDeepTreeEchoCognitiveCore* CognitiveCore;

    /** Reference to Reservoir component */
    UPROPERTY(BlueprintReadOnly, Category = "DeepTreeEcho|Components")
    UDeepTreeEchoReservoir* ReservoirSystem;

    /** Reference to Mutual Awareness component */
    UPROPERTY(BlueprintReadOnly, Category = "DeepTreeEcho|Components")
    URecursiveMutualAwarenessSystem* MutualAwarenessSystem;

    /** Reference to Hypergraph Memory System */
    UPROPERTY(BlueprintReadOnly, Category = "DeepTreeEcho|Components")
    UHypergraphMemorySystem* MemorySystem;

    /** Reference to DNA Body Schema Binding */
    UPROPERTY(BlueprintReadOnly, Category = "DeepTreeEcho|Components")
    UDNABodySchemaBinding* BodySchemaBinding;

    // ========================================
    // COGNITIVE STATE
    // ========================================

    /** Current cognitive mode */
    UPROPERTY(BlueprintReadOnly, Category = "DeepTreeEcho|State")
    ECognitiveMode CurrentMode = ECognitiveMode::Reactive;

    /** 4E cognition state */
    UPROPERTY(BlueprintReadOnly, Category = "DeepTreeEcho|State")
    F4ECognitionState CognitionState4E;

    /** Relevance realization state */
    UPROPERTY(BlueprintReadOnly, Category = "DeepTreeEcho|State")
    FRelevanceRealizationState RelevanceState;

    /** Gestalt state */
    UPROPERTY(BlueprintReadOnly, Category = "DeepTreeEcho|State")
    FGestaltState GestaltState;

    /** Current cycle step (1-12) */
    UPROPERTY(BlueprintReadOnly, Category = "DeepTreeEcho|State")
    int32 CurrentCycleStep = 1;

    /** Current nesting level (1-4) */
    UPROPERTY(BlueprintReadOnly, Category = "DeepTreeEcho|State")
    int32 CurrentNestingLevel = 1;

    // ========================================
    // PUBLIC API - CORE OPERATIONS
    // ========================================

    /** Initialize the Deep Tree Echo system */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho")
    void InitializeSystem();

    /** Has InitializeSystem() completed? Guards against redundant re-initialization. */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho")
    bool IsSystemInitialized() const { return bIsInitialized; }

    /** Process sensory input through the cognitive architecture */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho")
    void ProcessSensoryInput(const TArray<float>& SensoryData, const FString& Modality);

    /** Generate action output */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho")
    TArray<float> GenerateActionOutput();

    /** Set cognitive mode */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho")
    void SetCognitiveMode(ECognitiveMode NewMode);

    // ========================================
    // PUBLIC API - 4E COGNITION
    // ========================================

    /** Update embodied state */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|4E")
    void UpdateEmbodiedState(const TArray<float>& ProprioceptiveData, const TArray<float>& InteroceptiveData);

    /** Update embedded state (environmental coupling) */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|4E")
    void UpdateEmbeddedState(const TArray<FString>& Affordances, const FString& Niche);

    /** Update enacted state (sensorimotor loops) */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|4E")
    void UpdateEnactedState(const TArray<FString>& Contingencies, const TArray<float>& PredictionErrors);

    /** Update extended state (cognitive scaffolding) */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|4E")
    void UpdateExtendedState(const TArray<FString>& ExternalMemory, const TArray<FString>& Tools);

    /** Get 4E integration score */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|4E")
    float Get4EIntegrationScore() const;

    // ========================================
    // PUBLIC API - RELEVANCE REALIZATION
    // ========================================

    /** Update relevance frame */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Relevance")
    void UpdateRelevanceFrame(const FString& Frame, const TArray<FString>& Constraints);

    /** Allocate attention */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Relevance")
    void AllocateAttention(const FString& Target, float Weight);

    /** Update salience landscape */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Relevance")
    void UpdateSalienceLandscape(const TMap<FString, float>& SalienceUpdates);

    /** Check if at pivotal relevance realization step */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Relevance")
    bool IsPivotalRelevanceStep() const;

    // ========================================
    // PUBLIC API - GESTALT PROCESSING
    // ========================================

    /** Update gestalt perception */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Gestalt")
    void UpdateGestalt(const FString& Gestalt, const FString& Figure, const FString& Ground);

    /** Apply gestalt principle */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Gestalt")
    void ApplyGestaltPrinciple(const FString& Principle);

    /** Get gestalt coherence */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Gestalt")
    float GetGestaltCoherence() const;

    // ========================================
    // PUBLIC API - MEMORY INTEGRATION
    // ========================================

    /** Store experience in episodic memory */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Memory")
    int64 StoreEpisodicMemory(const FString& Label, const TArray<float>& Embedding);

    /** Retrieve memories by similarity */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Memory")
    TArray<int64> RetrieveSimilarMemories(const TArray<float>& QueryEmbedding, float Threshold = 0.5f);

    /** Create belief in intentional memory */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Memory")
    int64 CreateBelief(const FString& Proposition, float Confidence = 0.7f);

    /** Create desire/goal in intentional memory */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Memory")
    int64 CreateDesire(const FString& Goal, float Priority = 0.5f);

    /** Trigger memory consolidation */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Memory")
    void TriggerMemoryConsolidation();

    // ========================================
    // PUBLIC API - BODY SCHEMA
    // ========================================

    /** Get proprioceptive vector from body schema */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|BodySchema")
    TArray<float> GetBodyProprioceptiveState();

    /** Sync body schema to embodied cognition */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|BodySchema")
    void SyncBodySchemaToEmbodiedCognition();

    // ========================================
    // PUBLIC API - CYCLE MANAGEMENT
    // ========================================

    /** Get current cycle step (1-12) */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Cycle")
    int32 GetCurrentCycleStep() const { return CurrentCycleStep; }

    /** Get current nesting level (1-4) */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Cycle")
    int32 GetCurrentNestingLevel() const { return CurrentNestingLevel; }

    /** Is this an expressive mode step (7 of 12)? */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Cycle")
    bool IsExpressiveStep() const;

    /** Is this a reflective mode step (5 of 12)? */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Cycle")
    bool IsReflectiveStep() const;

protected:
    virtual void BeginPlay() override;

private:
    // ========================================
    // INTERNAL STATE
    // ========================================

    /** Cycle timer */
    float CycleTimer = 0.0f;

    /** System initialized flag */
    bool bIsInitialized = false;

    // ========================================
    // INTERNAL METHODS
    // ========================================

    /** Update cognitive cycle */
    void UpdateCognitiveLoop(float DeltaTime);

    /** Process at current nesting level */
    void ProcessAtNestingLevel(int32 Level);

    /** Compute nesting level from cycle step */
    int32 ComputeNestingLevel(int32 Step) const;

    /** Integrate 4E components */
    void Integrate4EComponents();

    /** Update relevance realization */
    void UpdateRelevanceRealization();

    /** Update gestalt processing */
    void UpdateGestaltProcessing();

    /** Update memory integration */
    void UpdateMemoryIntegration();

    /** Update body schema synchronization */
    void UpdateBodySchemaSync();

    /** Find and cache component references */
    void FindComponentReferences();
};
