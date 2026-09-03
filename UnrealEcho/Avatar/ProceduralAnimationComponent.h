#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ProceduralAnimationComponent.generated.h"

/**
 * Procedural Animation Component
 * 
 * Handles all procedural/secondary animation for the Deep-Tree-Echo avatar:
 * - Breathing (chest/shoulder movement)
 * - Blinking (procedural and reactive)
 * - Saccadic eye movement
 * - Head micro-movement/sway
 * - Idle fidgets
 * 
 * This component works alongside the Expression System to provide
 * the "life" layer that prevents the avatar from appearing robotic.
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBlinkTriggered);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBreathCycle, bool, bInhaling);

/**
 * Blink behavior settings
 */
USTRUCT(BlueprintType)
struct FBlinkSettings
{
    GENERATED_BODY()

    /** Minimum time between blinks (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blink")
    float MinInterval = 2.0f;

    /** Maximum time between blinks (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blink")
    float MaxInterval = 6.0f;

    /** Duration of a single blink (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blink")
    float BlinkDuration = 0.15f;

    /** Probability of double-blink (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blink", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DoubleBinkProbability = 0.1f;

    /** Blink rate multiplier when speaking */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blink")
    float SpeakingBlinkMultiplier = 1.5f;

    /** Blink rate multiplier when thinking */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blink")
    float ThinkingBlinkMultiplier = 0.7f;
};

/**
 * Breathing behavior settings
 */
USTRUCT(BlueprintType)
struct FBreathingSettings
{
    GENERATED_BODY()

    /** Breaths per minute at rest */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breathing")
    float RestingBreathsPerMinute = 12.0f;

    /** Breaths per minute when speaking */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breathing")
    float SpeakingBreathsPerMinute = 18.0f;

    /** Chest expansion amplitude */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breathing")
    float ChestAmplitude = 0.02f;

    /** Shoulder rise amplitude */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breathing")
    float ShoulderAmplitude = 0.01f;

    /** Inhale/exhale ratio (inhale time / total cycle time) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breathing", meta = (ClampMin = "0.2", ClampMax = "0.8"))
    float InhaleRatio = 0.4f;
};

/**
 * Saccade (eye movement) settings
 */
USTRUCT(BlueprintType)
struct FSaccadeSettings
{
    GENERATED_BODY()

    /** Average saccades per second */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saccade")
    float Frequency = 0.5f;

    /** Maximum saccade amplitude (degrees) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saccade")
    float MaxAmplitude = 5.0f;

    /** Saccade movement speed (degrees per second) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saccade")
    float Speed = 500.0f;

    /** Fixation duration range (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saccade")
    FVector2D FixationDuration = FVector2D(0.1f, 0.5f);

    /** Probability of returning to center after saccade */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saccade", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float CenterReturnProbability = 0.3f;
};

/**
 * Head micro-movement settings
 */
USTRUCT(BlueprintType)
struct FHeadMicroMovementSettings
{
    GENERATED_BODY()

    /** Amplitude of head sway (degrees) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HeadMovement")
    float SwayAmplitude = 1.0f;

    /** Frequency of head sway (Hz) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HeadMovement")
    float SwayFrequency = 0.1f;

    /** Amplitude of head nod during listening */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HeadMovement")
    float ListeningNodAmplitude = 3.0f;

    /** Frequency of listening nods */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HeadMovement")
    float ListeningNodFrequency = 0.3f;
};

/**
 * Output data from procedural animation
 */
USTRUCT(BlueprintType)
struct FProceduralAnimOutput
{
    GENERATED_BODY()

    // Eye closure (0 = open, 1 = closed)
    UPROPERTY(BlueprintReadOnly, Category = "Output")
    float EyeCloseL = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Output")
    float EyeCloseR = 0.0f;

    // Eye rotation offset
    UPROPERTY(BlueprintReadOnly, Category = "Output")
    FRotator EyeRotationOffset = FRotator::ZeroRotator;

    // Head rotation offset
    UPROPERTY(BlueprintReadOnly, Category = "Output")
    FRotator HeadRotationOffset = FRotator::ZeroRotator;

    // Breathing offsets
    UPROPERTY(BlueprintReadOnly, Category = "Output")
    float ChestExpansion = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Output")
    float ShoulderRise = 0.0f;

    // Breath phase (0-1, 0-0.4 = inhale, 0.4-1 = exhale based on InhaleRatio)
    UPROPERTY(BlueprintReadOnly, Category = "Output")
    float BreathPhase = 0.0f;
};

UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREECHO_API UProceduralAnimationComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UProceduralAnimationComponent();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========== Settings ==========

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
    FBlinkSettings BlinkSettings;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
    FBreathingSettings BreathingSettings;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
    FSaccadeSettings SaccadeSettings;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
    FHeadMicroMovementSettings HeadMovementSettings;

    // ========== Enable/Disable ==========

    UFUNCTION(BlueprintCallable, Category = "Procedural")
    void SetBlinkingEnabled(bool bEnabled);

    UFUNCTION(BlueprintCallable, Category = "Procedural")
    void SetBreathingEnabled(bool bEnabled);

    UFUNCTION(BlueprintCallable, Category = "Procedural")
    void SetSaccadesEnabled(bool bEnabled);

    UFUNCTION(BlueprintCallable, Category = "Procedural")
    void SetHeadMovementEnabled(bool bEnabled);

    UFUNCTION(BlueprintCallable, Category = "Procedural")
    void SetAllEnabled(bool bEnabled);

    // ========== Manual Triggers ==========

    /** Trigger a manual blink */
    UFUNCTION(BlueprintCallable, Category = "Procedural")
    void TriggerBlink();

    /** Trigger a double blink */
    UFUNCTION(BlueprintCallable, Category = "Procedural")
    void TriggerDoubleBlink();

    /** Trigger a slow, deliberate blink (for "aha" moments) */
    UFUNCTION(BlueprintCallable, Category = "Procedural")
    void TriggerDeliberateBlink(float Duration = 0.4f);

    /** Trigger a sigh (deep breath) */
    UFUNCTION(BlueprintCallable, Category = "Procedural")
    void TriggerSigh();

    /** Force eye position to specific offset */
    UFUNCTION(BlueprintCallable, Category = "Procedural")
    void SetEyeOffset(FVector2D Offset);

    /** Clear forced eye offset */
    UFUNCTION(BlueprintCallable, Category = "Procedural")
    void ClearEyeOffset();

    // ========== State Modifiers ==========

    /** Set speaking state (affects blink rate and breathing) */
    UFUNCTION(BlueprintCallable, Category = "Procedural")
    void SetSpeakingState(bool bSpeaking);

    /** Set thinking state (affects blink rate) */
    UFUNCTION(BlueprintCallable, Category = "Procedural")
    void SetThinkingState(bool bThinking);

    /** Set listening state (enables head nods) */
    UFUNCTION(BlueprintCallable, Category = "Procedural")
    void SetListeningState(bool bListening);

    // ========== Output ==========

    /** Get current procedural animation output */
    UFUNCTION(BlueprintPure, Category = "Procedural")
    FProceduralAnimOutput GetOutput() const { return Output; }

    // ========== Events ==========

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnBlinkTriggered OnBlinkTriggered;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnBreathCycle OnBreathCycle;

protected:
    void UpdateBlinking(float DeltaTime);
    void UpdateBreathing(float DeltaTime);
    void UpdateSaccades(float DeltaTime);
    void UpdateHeadMovement(float DeltaTime);

    float GetCurrentBlinkInterval() const;
    float GetCurrentBreathRate() const;

private:
    // Enable flags
    bool bBlinkingEnabled;
    bool bBreathingEnabled;
    bool bSaccadesEnabled;
    bool bHeadMovementEnabled;

    // State flags
    bool bIsSpeaking;
    bool bIsThinking;
    bool bIsListening;

    // Blink state
    float BlinkTimer;
    float NextBlinkTime;
    bool bIsBlinking;
    float BlinkProgress;
    float CurrentBlinkDuration;
    bool bPendingDoubleBlink;

    // Breathing state
    float BreathingPhase;
    bool bWasInhaling;
    bool bSighPending;

    // Saccade state
    FVector2D CurrentEyeOffset;
    FVector2D TargetEyeOffset;
    float SaccadeTimer;
    float FixationTimer;
    bool bHasForcedEyeOffset;
    FVector2D ForcedEyeOffset;

    // Head movement state
    float HeadSwayPhaseX;
    float HeadSwayPhaseY;
    float ListeningNodPhase;

    // Output
    FProceduralAnimOutput Output;
};
