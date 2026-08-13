// NeuroSymbolicBridge.h
// Deep Tree Echo - Neuro-Symbolic Integration Bridge
// Optimizes integration between neural (Deep Tree Echo) and symbolic (Unreal Engine) components
// Copyright (c) 2025 Deep Tree Echo Project

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NeuroSymbolicBridge.generated.h"

// Forward declarations
class UDeepTreeEchoCore;
class UCognitiveCycleManager;
class UReservoirCognitiveIntegration;
class UEntelechyFramework;

/**
 * Neural activation pattern from Deep Tree Echo reservoir
 */
USTRUCT(BlueprintType)
struct FNeuralActivationPattern
{
    GENERATED_BODY()

    /** Pattern identifier */
    UPROPERTY(BlueprintReadWrite, Category = "Neural")
    FString PatternID;

    /** Activation vector (normalized 0-1) */
    UPROPERTY(BlueprintReadWrite, Category = "Neural")
    TArray<float> Activations;

    /** Temporal context (echo state) */
    UPROPERTY(BlueprintReadWrite, Category = "Neural")
    TArray<float> TemporalContext;

    /** Confidence score */
    UPROPERTY(BlueprintReadWrite, Category = "Neural")
    float Confidence = 0.0f;

    /** Timestamp */
    UPROPERTY(BlueprintReadWrite, Category = "Neural")
    float Timestamp = 0.0f;
};

/**
 * Symbolic representation from Unreal Engine game logic
 */
USTRUCT(BlueprintType)
struct FSymbolicRepresentation
{
    GENERATED_BODY()

    /** Symbol identifier */
    UPROPERTY(BlueprintReadWrite, Category = "Symbolic")
    FString SymbolID;

    /** Symbol type (Entity, Action, State, Relation) */
    UPROPERTY(BlueprintReadWrite, Category = "Symbolic")
    FString SymbolType;

    /** Properties map */
    UPROPERTY(BlueprintReadWrite, Category = "Symbolic")
    TMap<FString, FString> Properties;

    /** Relations to other symbols */
    UPROPERTY(BlueprintReadWrite, Category = "Symbolic")
    TArray<FString> Relations;

    /** Truth value (for logical operations) */
    UPROPERTY(BlueprintReadWrite, Category = "Symbolic")
    float TruthValue = 1.0f;

    /** Attention value (salience) */
    UPROPERTY(BlueprintReadWrite, Category = "Symbolic")
    float AttentionValue = 0.5f;
};

/**
 * Neuro-symbolic binding connecting neural patterns to symbolic representations
 */
USTRUCT(BlueprintType)
struct FNeuroSymbolicBinding
{
    GENERATED_BODY()

    /** Binding identifier */
    UPROPERTY(BlueprintReadWrite, Category = "Binding")
    FString BindingID;

    /** Neural pattern */
    UPROPERTY(BlueprintReadWrite, Category = "Binding")
    FNeuralActivationPattern NeuralPattern;

    /** Symbolic representation */
    UPROPERTY(BlueprintReadWrite, Category = "Binding")
    FSymbolicRepresentation SymbolicRep;

    /** Binding strength (0-1) */
    UPROPERTY(BlueprintReadWrite, Category = "Binding")
    float BindingStrength = 0.0f;

    /** Binding type (Grounding, Abstraction, Composition) */
    UPROPERTY(BlueprintReadWrite, Category = "Binding")
    FString BindingType;

    /** Creation timestamp */
    UPROPERTY(BlueprintReadWrite, Category = "Binding")
    float CreationTime = 0.0f;

    /** Last activation time */
    UPROPERTY(BlueprintReadWrite, Category = "Binding")
    float LastActivationTime = 0.0f;

    /** Activation count */
    UPROPERTY(BlueprintReadWrite, Category = "Binding")
    int32 ActivationCount = 0;
};

/**
 * Integration metrics for neuro-symbolic processing
 */
USTRUCT(BlueprintType)
struct FNeuroSymbolicMetrics
{
    GENERATED_BODY()

    /** Total bindings */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    int32 TotalBindings = 0;

    /** Active bindings */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    int32 ActiveBindings = 0;

    /** Average binding strength */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    float AverageBindingStrength = 0.0f;

    /** Neural-to-symbolic conversion rate */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    float NeuralToSymbolicRate = 0.0f;

    /** Symbolic-to-neural conversion rate */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    float SymbolicToNeuralRate = 0.0f;

    /** Integration coherence (0-1) */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    float IntegrationCoherence = 0.0f;

    /** Processing latency (ms) */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    float ProcessingLatency = 0.0f;
};

/**
 * Neuro-Symbolic Bridge Component
 * 
 * Bridges the neural perception/intuition of Deep Tree Echo with the
 * symbolic logic/physics of Unreal Engine, enabling:
 * 
 * - Neural grounding: Connecting abstract neural patterns to concrete game entities
 * - Symbolic abstraction: Lifting game state to neural-processable representations
 * - Bidirectional inference: Neural intuition informing symbolic reasoning and vice versa
 * - Temporal integration: Maintaining coherent bindings across cognitive cycles
 * 
 * This component is central to achieving 4E embodied cognition where:
 * - Embodied: Neural patterns ground in avatar body state
 * - Embedded: Symbolic representations capture environmental context
 * - Enacted: Bindings evolve through action-perception loops
 * - Extended: External game objects extend cognitive processing
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UNeuroSymbolicBridge : public UActorComponent
{
    GENERATED_BODY()

public:
    UNeuroSymbolicBridge();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Enable automatic binding maintenance */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|Config")
    bool bEnableAutoMaintenance = true;

    /** Binding decay rate per second */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|Config", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BindingDecayRate = 0.01f;

    /** Minimum binding strength threshold */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|Config", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinBindingStrength = 0.1f;

    /** Maximum bindings to maintain */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|Config", meta = (ClampMin = "10", ClampMax = "10000"))
    int32 MaxBindings = 1000;

    /** Neural pattern dimension */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bridge|Config", meta = (ClampMin = "8", ClampMax = "1024"))
    int32 NeuralPatternDimension = 128;

    // ========================================
    // SUBSYSTEM REFERENCES
    // ========================================

    /** Deep Tree Echo Core reference */
    UPROPERTY(BlueprintReadOnly, Category = "Bridge|Subsystems")
    UDeepTreeEchoCore* DeepTreeEchoCore;

    /** Cognitive Cycle Manager reference */
    UPROPERTY(BlueprintReadOnly, Category = "Bridge|Subsystems")
    UCognitiveCycleManager* CognitiveCycleManager;

    /** Reservoir Integration reference */
    UPROPERTY(BlueprintReadOnly, Category = "Bridge|Subsystems")
    UReservoirCognitiveIntegration* ReservoirIntegration;

    /** Entelechy Framework reference */
    UPROPERTY(BlueprintReadOnly, Category = "Bridge|Subsystems")
    UEntelechyFramework* EntelechyFramework;

    // ========================================
    // STATE
    // ========================================

    /** Current bindings */
    UPROPERTY(BlueprintReadOnly, Category = "Bridge|State")
    TArray<FNeuroSymbolicBinding> Bindings;

    /** Current metrics */
    UPROPERTY(BlueprintReadOnly, Category = "Bridge|State")
    FNeuroSymbolicMetrics Metrics;

    // ========================================
    // PUBLIC API - INITIALIZATION
    // ========================================

    /** Initialize the bridge */
    UFUNCTION(BlueprintCallable, Category = "Bridge")
    void InitializeBridge();

    /** Discover and link subsystems */
    UFUNCTION(BlueprintCallable, Category = "Bridge")
    void DiscoverSubsystems();

    /** Reset all bindings */
    UFUNCTION(BlueprintCallable, Category = "Bridge")
    void ResetBindings();

    // ========================================
    // PUBLIC API - NEURAL TO SYMBOLIC
    // ========================================

    /** Ground a neural pattern to a symbolic representation */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Neural")
    FSymbolicRepresentation GroundNeuralPattern(const FNeuralActivationPattern& Pattern);

    /** Extract symbolic entities from neural activation */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Neural")
    TArray<FSymbolicRepresentation> ExtractSymbolicEntities(const TArray<float>& NeuralActivation);

    /** Infer symbolic relations from neural patterns */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Neural")
    TArray<FSymbolicRepresentation> InferSymbolicRelations(const TArray<FNeuralActivationPattern>& Patterns);

    // ========================================
    // PUBLIC API - SYMBOLIC TO NEURAL
    // ========================================

    /** Encode a symbolic representation as neural pattern */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Symbolic")
    FNeuralActivationPattern EncodeSymbolicRepresentation(const FSymbolicRepresentation& Symbol);

    /** Generate neural context from game state */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Symbolic")
    TArray<float> GenerateNeuralContext(const TArray<FSymbolicRepresentation>& GameState);

    /** Create neural embedding for symbolic query */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Symbolic")
    TArray<float> CreateQueryEmbedding(const FString& SymbolicQuery);

    // ========================================
    // PUBLIC API - BINDING MANAGEMENT
    // ========================================

    /** Create a new neuro-symbolic binding */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Binding")
    FNeuroSymbolicBinding CreateBinding(const FNeuralActivationPattern& Neural, const FSymbolicRepresentation& Symbolic, const FString& BindingType);

    /** Strengthen an existing binding */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Binding")
    void StrengthenBinding(const FString& BindingID, float Amount);

    /** Find bindings by neural pattern similarity */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Binding")
    TArray<FNeuroSymbolicBinding> FindBindingsByNeuralSimilarity(const FNeuralActivationPattern& Pattern, float Threshold);

    /** Find bindings by symbolic match */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Binding")
    TArray<FNeuroSymbolicBinding> FindBindingsBySymbolicMatch(const FSymbolicRepresentation& Symbol);

    /** Get active bindings */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Binding")
    TArray<FNeuroSymbolicBinding> GetActiveBindings() const;

    // ========================================
    // PUBLIC API - INFERENCE
    // ========================================

    /** Perform neural-guided symbolic inference */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Inference")
    TArray<FSymbolicRepresentation> NeuralGuidedInference(const FNeuralActivationPattern& Pattern, const TArray<FSymbolicRepresentation>& KnowledgeBase);

    /** Perform symbolic-constrained neural prediction */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Inference")
    FNeuralActivationPattern SymbolicConstrainedPrediction(const FSymbolicRepresentation& Constraint, const TArray<float>& CurrentState);

    /** Bidirectional inference cycle */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Inference")
    void BidirectionalInferenceCycle(float DeltaTime);

    // ========================================
    // PUBLIC API - METRICS
    // ========================================

    /** Get current metrics */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Metrics")
    FNeuroSymbolicMetrics GetMetrics() const;

    /** Calculate integration coherence */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Metrics")
    float CalculateIntegrationCoherence() const;

    /** Generate diagnostic report */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Metrics")
    TArray<FString> GenerateDiagnosticReport() const;

protected:
    virtual void BeginPlay() override;

private:
    /** Accumulated time for maintenance */
    float AccumulatedTime = 0.0f;

    /** Binding ID counter */
    int32 BindingIDCounter = 0;

    /** Maintain bindings (decay, pruning) */
    void MaintainBindings(float DeltaTime);

    /** Prune weak bindings */
    void PruneWeakBindings();

    /** Update metrics */
    void UpdateMetrics();

    /** Calculate neural similarity */
    float CalculateNeuralSimilarity(const TArray<float>& A, const TArray<float>& B) const;

    /** Calculate symbolic similarity */
    float CalculateSymbolicSimilarity(const FSymbolicRepresentation& A, const FSymbolicRepresentation& B) const;

    /** Generate unique binding ID */
    FString GenerateBindingID();

    /** Hash neural pattern for fast lookup */
    uint32 HashNeuralPattern(const TArray<float>& Pattern) const;

    /** Hash symbolic representation for fast lookup */
    uint32 HashSymbolicRep(const FSymbolicRepresentation& Symbol) const;
};
