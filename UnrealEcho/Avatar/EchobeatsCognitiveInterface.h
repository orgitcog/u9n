#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeepTreeEchoExpressionSystem.h"
#include "ExpressionStateMachine.h"
#include "ProceduralAnimationComponent.h"
#include "EchobeatsCognitiveInterface.generated.h"

/**
 * Echobeats Cognitive Interface
 * 
 * Bridges the Deep-Tree-Echo cognitive architecture with the avatar expression system.
 * Implements the 12-step cognitive loop with 3 concurrent streams phased 4 steps apart.
 * 
 * Cognitive Loop Structure:
 * - Steps {1,5,9}: Pivotal Relevance Realization (Stream 1)
 * - Steps {2,6,10}: Affordance Interaction (Stream 2)
 * - Steps {3,7,11}: Salience Processing (Stream 3)
 * - Steps {4,8,12}: Integration/Transition
 * 
 * Each step maps to expression hints that visualize the cognitive process.
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCognitiveStepChanged, int32, StepNumber, FString, PhaseName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCognitiveCycleComplete, int32, CycleCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRelevanceRealization, int32, StreamIndex, float, Salience);

/**
 * Cognitive stream state
 */
UENUM(BlueprintType)
enum class ECognitiveStreamPhase : uint8
{
    Perception      UMETA(DisplayName = "Perception"),
    Action          UMETA(DisplayName = "Action"),
    Simulation      UMETA(DisplayName = "Simulation")
};

/**
 * Cognitive mode (expressive vs reflective)
 */
UENUM(BlueprintType)
enum class ECognitiveMode : uint8
{
    Expressive      UMETA(DisplayName = "Expressive"),      // Outward-focused
    Reflective      UMETA(DisplayName = "Reflective")       // Inward-focused
};

/**
 * Single cognitive stream state
 */
USTRUCT(BlueprintType)
struct FCognitiveStreamState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Stream")
    int32 StreamIndex = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Stream")
    ECognitiveStreamPhase Phase = ECognitiveStreamPhase::Perception;

    UPROPERTY(BlueprintReadOnly, Category = "Stream")
    float Salience = 0.0f;  // Current salience level (0-1)

    UPROPERTY(BlueprintReadOnly, Category = "Stream")
    float Activation = 0.0f;  // Current activation level (0-1)

    UPROPERTY(BlueprintReadOnly, Category = "Stream")
    int32 CurrentStep = 1;  // 1-12
};

/**
 * Cognitive step configuration
 */
USTRUCT(BlueprintType)
struct FCognitiveStepConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step")
    int32 StepNumber = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step")
    FString PhaseName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step")
    ECognitiveMode Mode = ECognitiveMode::Expressive;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step")
    EExpressionState ExpressionHint;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step")
    float EmissiveMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step")
    bool bTriggerMicroExpression = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Step")
    EMicroExpression MicroExpressionType = EMicroExpression::None;
};

/**
 * Relevance realization event data
 */
USTRUCT(BlueprintType)
struct FRelevanceRealizationEvent
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Relevance")
    float Salience = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Relevance")
    float Novelty = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Relevance")
    float Significance = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Relevance")
    bool bIsInsight = false;  // True if this is an "aha" moment
};

UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREECHO_API UEchobeatsCognitiveInterface : public UActorComponent
{
    GENERATED_BODY()

public:
    UEchobeatsCognitiveInterface();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========== Cognitive Loop Control ==========

    /** Start the cognitive loop */
    UFUNCTION(BlueprintCallable, Category = "Cognitive")
    void StartCognitiveLoop();

    /** Stop the cognitive loop */
    UFUNCTION(BlueprintCallable, Category = "Cognitive")
    void StopCognitiveLoop();

    /** Pause the cognitive loop */
    UFUNCTION(BlueprintCallable, Category = "Cognitive")
    void PauseCognitiveLoop();

    /** Resume the cognitive loop */
    UFUNCTION(BlueprintCallable, Category = "Cognitive")
    void ResumeCognitiveLoop();

    /** Set the cognitive loop speed (steps per second) */
    UFUNCTION(BlueprintCallable, Category = "Cognitive")
    void SetLoopSpeed(float StepsPerSecond);

    /** Advance to specific step (for external synchronization) */
    UFUNCTION(BlueprintCallable, Category = "Cognitive")
    void AdvanceToStep(int32 StepNumber);

    // ========== Relevance Realization ==========

    /** Signal a relevance realization event (triggers expression response) */
    UFUNCTION(BlueprintCallable, Category = "Cognitive")
    void SignalRelevanceRealization(const FRelevanceRealizationEvent& Event);

    /** Signal an insight moment (triggers Wonder-to-Joy) */
    UFUNCTION(BlueprintCallable, Category = "Cognitive")
    void SignalInsight(float Intensity = 1.0f);

    // ========== Stream Control ==========

    /** Set stream salience level */
    UFUNCTION(BlueprintCallable, Category = "Cognitive")
    void SetStreamSalience(int32 StreamIndex, float Salience);

    /** Set stream activation level */
    UFUNCTION(BlueprintCallable, Category = "Cognitive")
    void SetStreamActivation(int32 StreamIndex, float Activation);

    /** Get stream state */
    UFUNCTION(BlueprintPure, Category = "Cognitive")
    FCognitiveStreamState GetStreamState(int32 StreamIndex) const;

    // ========== External Integration ==========

    /** Receive cognitive state update from external AGI core */
    UFUNCTION(BlueprintCallable, Category = "Integration")
    void ReceiveCognitiveUpdate(ECognitiveState NewState, float Confidence);

    /** Receive speech state update */
    UFUNCTION(BlueprintCallable, Category = "Integration")
    void ReceiveSpeechUpdate(bool bIsSpeaking, float Amplitude);

    /** Receive listening state update */
    UFUNCTION(BlueprintCallable, Category = "Integration")
    void ReceiveListeningUpdate(bool bIsListening);

    /** Receive thinking state update */
    UFUNCTION(BlueprintCallable, Category = "Integration")
    void ReceiveThinkingUpdate(bool bIsThinking, float Intensity);

    // ========== Query ==========

    /** Get current step number (1-12) */
    UFUNCTION(BlueprintPure, Category = "Cognitive")
    int32 GetCurrentStep() const { return CurrentStep; }

    /** Get current cognitive mode */
    UFUNCTION(BlueprintPure, Category = "Cognitive")
    ECognitiveMode GetCurrentMode() const { return CurrentMode; }

    /** Get cycle count */
    UFUNCTION(BlueprintPure, Category = "Cognitive")
    int32 GetCycleCount() const { return CycleCount; }

    /** Is cognitive loop running */
    UFUNCTION(BlueprintPure, Category = "Cognitive")
    bool IsLoopRunning() const { return bLoopRunning && !bLoopPaused; }

    // ========== Events ==========

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnCognitiveStepChanged OnCognitiveStepChanged;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnCognitiveCycleComplete OnCognitiveCycleComplete;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnRelevanceRealization OnRelevanceRealization;

protected:
    /** Initialize step configurations */
    void InitializeStepConfigs();

    /** Process step change */
    void ProcessStepChange(int32 NewStep);

    /** Update streams based on current step */
    void UpdateStreams();

    /** Apply expression for current step */
    void ApplyStepExpression();

    /** Get step configuration */
    const FCognitiveStepConfig* GetStepConfig(int32 StepNumber) const;

private:
    // Component references
    UPROPERTY()
    UDeepTreeEchoExpressionSystem* ExpressionSystem;

    UPROPERTY()
    UExpressionStateMachine* StateMachine;

    UPROPERTY()
    UProceduralAnimationComponent* ProceduralAnim;

    // Loop state
    bool bLoopRunning;
    bool bLoopPaused;
    float LoopTimer;
    float StepDuration;  // Seconds per step
    int32 CurrentStep;
    int32 CycleCount;
    ECognitiveMode CurrentMode;

    // Stream states (3 concurrent streams)
    FCognitiveStreamState StreamStates[3];

    // Step configurations
    TArray<FCognitiveStepConfig> StepConfigs;

    // External state
    bool bExternalSpeaking;
    float ExternalSpeakingAmplitude;
    bool bExternalListening;
    bool bExternalThinking;
    float ExternalThinkingIntensity;
};
