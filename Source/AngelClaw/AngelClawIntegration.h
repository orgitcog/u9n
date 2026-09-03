// AngelClawIntegration.h
// Deep Tree Echo - AngelClaw Cognitive Core Integration
// Bridges angelclaw's embodied AGI systems into the u9n unified framework
// Copyright (c) 2026 Deep Tree Echo Project

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AngelClawIntegration.generated.h"

// Forward declarations for angelclaw cognitive subsystems
class UDeepTreeEchoCore;
class UHypergraphMemorySystem;
class URecursiveMutualAwarenessSystem;

/**
 * AngelClaw Cognitive Mode
 * Maps angelclaw's 5-mode cognitive processing into u9n
 */
UENUM(BlueprintType)
enum class EAngelClawCognitiveMode : uint8
{
    Reactive        UMETA(DisplayName = "Reactive"),
    Deliberative    UMETA(DisplayName = "Deliberative"),
    Reflective      UMETA(DisplayName = "Reflective"),
    Creative        UMETA(DisplayName = "Creative"),
    Integrative     UMETA(DisplayName = "Integrative")
};

/**
 * 4E Cognition State — imported from angelclaw's F4ECognitionState
 * Tracks Embodied, Embedded, Enacted, Extended cognition dimensions
 */
USTRUCT(BlueprintType)
struct FAngelClaw4EState
{
    GENERATED_BODY()

    // Embodied — body schema, proprioception, somatic markers
    UPROPERTY(BlueprintReadWrite, Category = "4E|Embodied")
    TArray<float> ProprioceptiveState;

    UPROPERTY(BlueprintReadWrite, Category = "4E|Embodied")
    TArray<float> InteroceptiveState;

    UPROPERTY(BlueprintReadWrite, Category = "4E|Embodied")
    float MotorReadiness = 0.5f;

    UPROPERTY(BlueprintReadWrite, Category = "4E|Embodied")
    TMap<FString, float> SomaticMarkers;

    // Embedded — environmental affordances, niche coupling
    UPROPERTY(BlueprintReadWrite, Category = "4E|Embedded")
    TArray<FString> ActiveAffordances;

    UPROPERTY(BlueprintReadWrite, Category = "4E|Embedded")
    float NicheCouplingStrength = 0.0f;

    // Enacted — sensorimotor contingencies
    UPROPERTY(BlueprintReadWrite, Category = "4E|Enacted")
    TMap<FString, float> SensorimotorContingencies;

    UPROPERTY(BlueprintReadWrite, Category = "4E|Enacted")
    float EnactiveEngagement = 0.0f;

    // Extended — cognitive tools, external memory
    UPROPERTY(BlueprintReadWrite, Category = "4E|Extended")
    TArray<FString> ExternalCognitiveTools;

    UPROPERTY(BlueprintReadWrite, Category = "4E|Extended")
    float ExtendedCognitionLoad = 0.0f;
};

/**
 * Emotion State — imported from angelclaw's valence-arousal model
 */
USTRUCT(BlueprintType)
struct FAngelClawEmotionState
{
    GENERATED_BODY()

    /** Emotional valence: -1.0 (negative) to 1.0 (positive) */
    UPROPERTY(BlueprintReadWrite, Category = "Emotion")
    float Valence = 0.0f;

    /** Emotional arousal: 0.0 (calm) to 1.0 (excited) */
    UPROPERTY(BlueprintReadWrite, Category = "Emotion")
    float Arousal = 0.5f;

    /** Dominance: 0.0 (submissive) to 1.0 (dominant) */
    UPROPERTY(BlueprintReadWrite, Category = "Emotion")
    float Dominance = 0.5f;

    /** Current primary emotion label */
    UPROPERTY(BlueprintReadWrite, Category = "Emotion")
    FString PrimaryEmotion = TEXT("Neutral");

    /** Emotional stability (resistance to perturbation) */
    UPROPERTY(BlueprintReadWrite, Category = "Emotion")
    float Stability = 0.7f;
};

/**
 * Consciousness Stream State — angelclaw's 3 concurrent streams
 * Phased 120 degrees apart in the 12-step cognitive cycle
 */
USTRUCT(BlueprintType)
struct FConsciousnessStreamState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Consciousness")
    int32 StreamID = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Consciousness")
    int32 CurrentStep = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Consciousness")
    float ActivationLevel = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Consciousness")
    FString CurrentPhase = TEXT("Perceiving");
};

/**
 * UAngelClawIntegration
 *
 * Main integration component that bridges angelclaw's cognitive core
 * (rzonedevops/angelclaw) into u9n's Unreal Engine cognitive framework.
 *
 * Provides:
 * - 12-step cognitive cycle with 3 concurrent consciousness streams
 * - 4E embodied cognition (Embodied, Embedded, Enacted, Extended)
 * - Valence-arousal emotional processing
 * - Hypergraph memory system bridge
 * - Introspection and wisdom cultivation
 * - Multi-channel gateway coordination (via OpenClaw)
 *
 * Architecture follows OEIS A000081 nested shell structure:
 *   1 nest -> 1 term, 2 nests -> 2, 3 nests -> 4, 4 nests -> 9
 */
UCLASS(ClassGroup = (DeepTreeEcho), meta = (BlueprintSpawnableComponent),
       DisplayName = "AngelClaw Integration")
class DEEPTREEECHO_API UAngelClawIntegration : public UActorComponent
{
    GENERATED_BODY()

public:
    UAngelClawIntegration();

    // --- Lifecycle ---

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // --- Cognitive Cycle ---

    /** Advance the 12-step cognitive cycle by one step */
    UFUNCTION(BlueprintCallable, Category = "AngelClaw|Cognitive")
    void AdvanceCognitiveCycle();

    /** Get current step in the 12-step cycle (1-12) */
    UFUNCTION(BlueprintPure, Category = "AngelClaw|Cognitive")
    int32 GetCurrentCycleStep() const { return CurrentCycleStep; }

    /** Get current cognitive mode */
    UFUNCTION(BlueprintPure, Category = "AngelClaw|Cognitive")
    EAngelClawCognitiveMode GetCognitiveMode() const { return CurrentMode; }

    /** Set cognitive mode */
    UFUNCTION(BlueprintCallable, Category = "AngelClaw|Cognitive")
    void SetCognitiveMode(EAngelClawCognitiveMode NewMode);

    // --- 4E Cognition ---

    /** Get current 4E cognition state */
    UFUNCTION(BlueprintPure, Category = "AngelClaw|4E")
    FAngelClaw4EState Get4EState() const { return State4E; }

    /** Update embodied state from sensory input */
    UFUNCTION(BlueprintCallable, Category = "AngelClaw|4E")
    void UpdateEmbodiedState(const TArray<float>& ProprioceptiveInput,
                             const TArray<float>& InteroceptiveInput);

    /** Register environmental affordance */
    UFUNCTION(BlueprintCallable, Category = "AngelClaw|4E")
    void RegisterAffordance(const FString& AffordanceID, float Strength);

    // --- Emotion ---

    /** Get current emotion state */
    UFUNCTION(BlueprintPure, Category = "AngelClaw|Emotion")
    FAngelClawEmotionState GetEmotionState() const { return EmotionState; }

    /** Process emotional stimulus */
    UFUNCTION(BlueprintCallable, Category = "AngelClaw|Emotion")
    void ProcessEmotionalStimulus(float ValenceShift, float ArousalShift,
                                  const FString& Context);

    // --- Consciousness Streams ---

    /** Get state of a specific consciousness stream (0-2) */
    UFUNCTION(BlueprintPure, Category = "AngelClaw|Consciousness")
    FConsciousnessStreamState GetStreamState(int32 StreamID) const;

    /** Synchronize streams at triadic point */
    UFUNCTION(BlueprintCallable, Category = "AngelClaw|Consciousness")
    void SynchronizeStreams();

    // --- Introspection ---

    /** Trigger introspective analysis of current cognitive state */
    UFUNCTION(BlueprintCallable, Category = "AngelClaw|Introspection")
    FString PerformIntrospection();

    /** Get wisdom cultivation level (0.0 - 1.0) */
    UFUNCTION(BlueprintPure, Category = "AngelClaw|Wisdom")
    float GetWisdomLevel() const { return WisdomLevel; }

    // --- Bridge to u9n Systems ---

    /**
     * Connect to u9n's StrategicCognitionBridge for left-right hemisphere
     * coordination between Deep Tree Echo (right) and Marduk (left)
     */
    UFUNCTION(BlueprintCallable, Category = "AngelClaw|Bridge")
    void ConnectToStrategicCognitionBridge();

    /**
     * Feed angelclaw emotion state into u9n's Neurochemical systems
     * (EchoPulseResonanceChamber, CognitiveEndorphinJelly, HomeostasisRegulator)
     */
    UFUNCTION(BlueprintCallable, Category = "AngelClaw|Bridge")
    void SyncWithNeurochemicalSystems();

    /**
     * Bridge angelclaw's 4E cognition to u9n's behavior tree nodes
     */
    UFUNCTION(BlueprintCallable, Category = "AngelClaw|Bridge")
    void BridgeToBehaviorTree();

protected:
    // --- Configuration ---

    /** Cognitive cycle tick rate (seconds between steps) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AngelClaw|Config")
    float CycleTickRate = 0.1f;

    /** Enable 3 concurrent consciousness streams */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AngelClaw|Config")
    bool bEnableConcurrentStreams = true;

    /** Reservoir spectral radius (default from angelclaw) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AngelClaw|Config",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float SpectralRadius = 0.9f;

    /** Reservoir leak rate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AngelClaw|Config",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float LeakRate = 0.3f;

private:
    // --- Internal State ---

    /** Current step in the 12-step cognitive cycle */
    int32 CurrentCycleStep = 1;

    /** Current cognitive processing mode */
    EAngelClawCognitiveMode CurrentMode = EAngelClawCognitiveMode::Reactive;

    /** 4E cognition state */
    FAngelClaw4EState State4E;

    /** Emotional state (valence-arousal-dominance) */
    FAngelClawEmotionState EmotionState;

    /** Three concurrent consciousness streams */
    TArray<FConsciousnessStreamState> ConsciousnessStreams;

    /** Accumulated wisdom level */
    float WisdomLevel = 0.0f;

    /** Time accumulator for cycle ticking */
    float CycleTimeAccumulator = 0.0f;

    // --- Internal Methods ---

    /** Initialize the 3 consciousness streams phased 120 degrees apart */
    void InitializeConsciousnessStreams();

    /** Process one step of a single consciousness stream */
    void ProcessStreamStep(int32 StreamIndex);

    /** Check if current step is a triadic synchronization point */
    bool IsTriadicSyncPoint() const;

    /** Map cycle step to cognitive phase name */
    static FString StepToPhase(int32 Step);

    /** Update wisdom based on introspective depth */
    void CultivateWisdom(float IntrospectiveDepth);
};
