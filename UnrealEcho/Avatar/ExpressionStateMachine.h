#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeepTreeEchoExpressionSystem.h"
#include "ExpressionStateMachine.generated.h"

/**
 * Expression State Machine
 * 
 * Manages expression state transitions with support for:
 * - Queued transitions
 * - Priority-based interruption
 * - Transition curves and timing
 * - Composite expressions (blending multiple states)
 * - Integration with echobeats cognitive cycle
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnExpressionChanged, EExpressionState, OldState, EExpressionState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTransitionComplete, EExpressionState, NewState);

/**
 * Expression transition priority levels
 */
UENUM(BlueprintType)
enum class EExpressionPriority : uint8
{
    Low         UMETA(DisplayName = "Low"),         // Ambient/idle expressions
    Normal      UMETA(DisplayName = "Normal"),      // Standard cognitive state changes
    High        UMETA(DisplayName = "High"),        // User interaction responses
    Critical    UMETA(DisplayName = "Critical")     // System alerts, errors
};

/**
 * Transition curve types
 */
UENUM(BlueprintType)
enum class ETransitionCurve : uint8
{
    Linear      UMETA(DisplayName = "Linear"),
    EaseIn      UMETA(DisplayName = "Ease In"),
    EaseOut     UMETA(DisplayName = "Ease Out"),
    EaseInOut   UMETA(DisplayName = "Ease In/Out"),
    Bounce      UMETA(DisplayName = "Bounce"),
    Overshoot   UMETA(DisplayName = "Overshoot")
};

/**
 * Expression transition request
 */
USTRUCT(BlueprintType)
struct FExpressionTransitionRequest
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition")
    EExpressionState TargetState;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition")
    float Duration = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition")
    EExpressionPriority Priority = EExpressionPriority::Normal;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition")
    ETransitionCurve Curve = ETransitionCurve::EaseInOut;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition")
    float HoldDuration = 0.0f;  // How long to hold before returning (0 = indefinite)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transition")
    bool bReturnToPrevious = false;  // Return to previous state after hold

    FExpressionTransitionRequest()
        : TargetState(EExpressionState::Neutral)
        , Duration(0.3f)
        , Priority(EExpressionPriority::Normal)
        , Curve(ETransitionCurve::EaseInOut)
        , HoldDuration(0.0f)
        , bReturnToPrevious(false)
    {}

    FExpressionTransitionRequest(EExpressionState InTarget, float InDuration = 0.3f)
        : TargetState(InTarget)
        , Duration(InDuration)
        , Priority(EExpressionPriority::Normal)
        , Curve(ETransitionCurve::EaseInOut)
        , HoldDuration(0.0f)
        , bReturnToPrevious(false)
    {}
};

/**
 * Composite expression layer
 */
USTRUCT(BlueprintType)
struct FExpressionLayer
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer")
    EExpressionState Expression;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer")
    float Weight = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer")
    bool bAdditive = false;  // If true, adds to base; if false, blends

    FExpressionLayer()
        : Expression(EExpressionState::Neutral)
        , Weight(1.0f)
        , bAdditive(false)
    {}
};

/**
 * Predefined expression sequence
 */
USTRUCT(BlueprintType)
struct FExpressionSequence
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sequence")
    FName SequenceName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sequence")
    TArray<FExpressionTransitionRequest> Steps;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sequence")
    bool bLoop = false;
};

UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREECHO_API UExpressionStateMachine : public UActorComponent
{
    GENERATED_BODY()

public:
    UExpressionStateMachine();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========== State Control ==========

    /** Request a transition to a new expression state */
    UFUNCTION(BlueprintCallable, Category = "State Machine")
    void RequestTransition(const FExpressionTransitionRequest& Request);

    /** Simple transition request */
    UFUNCTION(BlueprintCallable, Category = "State Machine")
    void TransitionTo(EExpressionState NewState, float Duration = 0.3f);

    /** Immediate state change (no transition) */
    UFUNCTION(BlueprintCallable, Category = "State Machine")
    void SetStateImmediate(EExpressionState NewState);

    /** Queue a transition to play after current completes */
    UFUNCTION(BlueprintCallable, Category = "State Machine")
    void QueueTransition(const FExpressionTransitionRequest& Request);

    /** Clear all queued transitions */
    UFUNCTION(BlueprintCallable, Category = "State Machine")
    void ClearQueue();

    /** Return to neutral state */
    UFUNCTION(BlueprintCallable, Category = "State Machine")
    void ReturnToNeutral(float Duration = 0.5f);

    // ========== Composite Expressions ==========

    /** Add an expression layer */
    UFUNCTION(BlueprintCallable, Category = "Composite")
    void AddExpressionLayer(const FExpressionLayer& Layer);

    /** Remove an expression layer by expression type */
    UFUNCTION(BlueprintCallable, Category = "Composite")
    void RemoveExpressionLayer(EExpressionState Expression);

    /** Clear all expression layers */
    UFUNCTION(BlueprintCallable, Category = "Composite")
    void ClearExpressionLayers();

    /** Set layer weight */
    UFUNCTION(BlueprintCallable, Category = "Composite")
    void SetLayerWeight(EExpressionState Expression, float Weight);

    // ========== Sequences ==========

    /** Play a predefined expression sequence */
    UFUNCTION(BlueprintCallable, Category = "Sequence")
    void PlaySequence(const FName& SequenceName);

    /** Stop current sequence */
    UFUNCTION(BlueprintCallable, Category = "Sequence")
    void StopSequence();

    /** Register a custom sequence */
    UFUNCTION(BlueprintCallable, Category = "Sequence")
    void RegisterSequence(const FExpressionSequence& Sequence);

    // ========== Special Transitions ==========

    /** Play the Wonder-to-Joy "aha" transition */
    UFUNCTION(BlueprintCallable, Category = "Special")
    void PlayWonderToJoy(float Duration = 1.5f);

    /** Play a thinking sequence (contemplative with occasional curious glances) */
    UFUNCTION(BlueprintCallable, Category = "Special")
    void PlayThinkingSequence();

    /** Play an engagement greeting */
    UFUNCTION(BlueprintCallable, Category = "Special")
    void PlayGreeting();

    // ========== Query ==========

    /** Get current expression state */
    UFUNCTION(BlueprintPure, Category = "State Machine")
    EExpressionState GetCurrentState() const { return CurrentState; }

    /** Get previous expression state */
    UFUNCTION(BlueprintPure, Category = "State Machine")
    EExpressionState GetPreviousState() const { return PreviousState; }

    /** Is currently transitioning */
    UFUNCTION(BlueprintPure, Category = "State Machine")
    bool IsTransitioning() const { return bIsTransitioning; }

    /** Get transition progress (0-1) */
    UFUNCTION(BlueprintPure, Category = "State Machine")
    float GetTransitionProgress() const { return TransitionProgress; }

    /** Get blended morph targets (base + layers) */
    UFUNCTION(BlueprintPure, Category = "State Machine")
    FDeepTreeEchoMorphTargets GetBlendedMorphTargets() const;

    // ========== Events ==========

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnExpressionChanged OnExpressionChanged;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnTransitionComplete OnTransitionComplete;

protected:
    /** Initialize predefined sequences */
    void InitializeSequences();

    /** Process transition queue */
    void ProcessQueue();

    /** Update current transition */
    void UpdateTransition(float DeltaTime);

    /** Update hold timer */
    void UpdateHold(float DeltaTime);

    /** Update sequence playback */
    void UpdateSequence(float DeltaTime);

    /** Apply transition curve */
    float ApplyCurve(float Alpha, ETransitionCurve Curve) const;

    /** Get morph targets for expression state */
    FDeepTreeEchoMorphTargets GetMorphTargetsForState(EExpressionState State) const;

private:
    // Reference to expression system
    UPROPERTY()
    class UDeepTreeEchoExpressionSystem* ExpressionSystem;

    // Current state
    EExpressionState CurrentState;
    EExpressionState PreviousState;
    EExpressionState TransitionFromState;
    EExpressionState TransitionToState;

    // Transition state
    bool bIsTransitioning;
    float TransitionProgress;
    float TransitionDuration;
    ETransitionCurve CurrentCurve;
    EExpressionPriority CurrentPriority;

    // Hold state
    bool bIsHolding;
    float HoldTimer;
    float HoldDuration;
    bool bReturnAfterHold;
    EExpressionState ReturnState;

    // Queue
    TArray<FExpressionTransitionRequest> TransitionQueue;

    // Layers
    TArray<FExpressionLayer> ExpressionLayers;

    // Sequences
    TMap<FName, FExpressionSequence> RegisteredSequences;
    bool bIsPlayingSequence;
    FName CurrentSequenceName;
    int32 CurrentSequenceStep;
    bool bSequenceStepComplete;

    // Cached morph targets
    FDeepTreeEchoMorphTargets CurrentMorphTargets;
    FDeepTreeEchoMorphTargets TargetMorphTargets;
};
