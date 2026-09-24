// ExpressiveAnimationSystem.h
// Enhanced Expressive Animation System for Deep Tree Echo Avatar
// Implements expanded animation and vocalization mappings with 4E embodied cognition

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ExpressiveAnimationSystem.generated.h"

// Forward declarations
class UAdvancedEmotionBlending;
class UEchobeatsStreamEngine;
class USensoryInputIntegration;
class USkeletalMeshComponent;
class UAudioComponent;

/**
 * Expression channel type
 */
UENUM(BlueprintType)
enum class EExpressionChannel : uint8
{
    Facial          UMETA(DisplayName = "Facial"),
    Body            UMETA(DisplayName = "Body Posture"),
    Gesture         UMETA(DisplayName = "Gesture"),
    Gaze            UMETA(DisplayName = "Gaze"),
    Vocal           UMETA(DisplayName = "Vocal"),
    Breathing       UMETA(DisplayName = "Breathing"),
    Microexpression UMETA(DisplayName = "Microexpression")
};

/**
 * Animation layer type
 */
UENUM(BlueprintType)
enum class EAnimationLayer : uint8
{
    Base            UMETA(DisplayName = "Base"),
    Additive        UMETA(DisplayName = "Additive"),
    Override        UMETA(DisplayName = "Override"),
    Blend           UMETA(DisplayName = "Blend")
};

/**
 * Vocalization type
 */
UENUM(BlueprintType)
enum class EVocalizationType : uint8
{
    Speech          UMETA(DisplayName = "Speech"),
    Prosody         UMETA(DisplayName = "Prosody"),
    Paralinguistic  UMETA(DisplayName = "Paralinguistic"),
    Breathing       UMETA(DisplayName = "Breathing"),
    Silence         UMETA(DisplayName = "Silence")
};

/**
 * 4E cognition mode (Embodied, Embedded, Enacted, Extended)
 */
UENUM(BlueprintType)
enum class E4ECognitionMode : uint8
{
    Embodied        UMETA(DisplayName = "Embodied"),
    Embedded        UMETA(DisplayName = "Embedded"),
    Enacted         UMETA(DisplayName = "Enacted"),
    Extended        UMETA(DisplayName = "Extended")
};

/**
 * Expression target
 */
USTRUCT(BlueprintType)
struct FExpressionTarget
{
    GENERATED_BODY()

    /** Target name */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Name;

    /** Channel */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EExpressionChannel Channel = EExpressionChannel::Facial;

    /** Target value (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Value = 0.0f;

    /** Blend weight */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Weight = 1.0f;

    /** Transition duration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TransitionDuration = 0.3f;
};

/**
 * Facial action unit (FACS-based)
 */
USTRUCT(BlueprintType)
struct FFacialActionUnit
{
    GENERATED_BODY()

    /** AU number (FACS) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 AUNumber = 0;

    /** AU name */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Name;

    /** Intensity (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Intensity = 0.0f;

    /** Asymmetry (-1 to 1, 0 = symmetric) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Asymmetry = 0.0f;

    /** Associated morph target */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName MorphTarget;
};

/**
 * Body posture state
 */
USTRUCT(BlueprintType)
struct FBodyPostureState
{
    GENERATED_BODY()

    /** Posture name */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString PostureName;

    /** Openness (0-1, closed to open) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Openness = 0.5f;

    /** Tension (0-1, relaxed to tense) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Tension = 0.3f;

    /** Forward lean (-1 to 1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ForwardLean = 0.0f;

    /** Vertical expansion (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float VerticalExpansion = 0.5f;

    /** Symmetry (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Symmetry = 1.0f;
};

/**
 * Gesture definition
 */
USTRUCT(BlueprintType)
struct FGestureDefinition
{
    GENERATED_BODY()

    /** Gesture ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString GestureID;

    /** Gesture name */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Name;

    /** Animation montage */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName AnimationMontage;

    /** Duration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Duration = 1.0f;

    /** Intensity multiplier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float IntensityMultiplier = 1.0f;

    /** Associated emotions */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> AssociatedEmotions;

    /** Semantic meaning */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString SemanticMeaning;
};

/**
 * Gaze target
 */
USTRUCT(BlueprintType)
struct FGazeTarget
{
    GENERATED_BODY()

    /** Target type */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString TargetType;

    /** World location */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector WorldLocation = FVector::ZeroVector;

    /** Target actor */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    AActor* TargetActor = nullptr;

    /** Attention weight (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AttentionWeight = 1.0f;

    /** Saccade probability */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SaccadeProbability = 0.1f;
};

/**
 * Vocal parameters
 */
USTRUCT(BlueprintType)
struct FVocalParameters
{
    GENERATED_BODY()

    /** Pitch (Hz) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Pitch = 150.0f;

    /** Pitch variation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PitchVariation = 20.0f;

    /** Volume (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Volume = 0.7f;

    /** Speech rate (words per minute) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SpeechRate = 120.0f;

    /** Breathiness (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Breathiness = 0.2f;

    /** Tension (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Tension = 0.3f;

    /** Tremor (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Tremor = 0.0f;
};

/**
 * Breathing state
 */
USTRUCT(BlueprintType)
struct FBreathingState
{
    GENERATED_BODY()

    /** Breathing rate (breaths per minute) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Rate = 12.0f;

    /** Depth (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Depth = 0.5f;

    /** Regularity (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Regularity = 0.9f;

    /** Current phase (0-1, 0=inhale start, 0.5=exhale start) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Phase = 0.0f;

    /** Chest vs abdominal (0=abdominal, 1=chest) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ChestAbdominalRatio = 0.5f;
};

/**
 * 4E expression state
 */
USTRUCT(BlueprintType)
struct F4EExpressionState
{
    GENERATED_BODY()

    /** Current 4E mode */
    UPROPERTY(BlueprintReadOnly)
    E4ECognitionMode Mode = E4ECognitionMode::Embodied;

    /** Embodied weight (body-based expression) */
    UPROPERTY(BlueprintReadOnly)
    float EmbodiedWeight = 0.25f;

    /** Embedded weight (context-sensitive expression) */
    UPROPERTY(BlueprintReadOnly)
    float EmbeddedWeight = 0.25f;

    /** Enacted weight (action-based expression) */
    UPROPERTY(BlueprintReadOnly)
    float EnactedWeight = 0.25f;

    /** Extended weight (tool/environment-mediated expression) */
    UPROPERTY(BlueprintReadOnly)
    float ExtendedWeight = 0.25f;

    /** Integration coherence */
    UPROPERTY(BlueprintReadOnly)
    float IntegrationCoherence = 1.0f;
};

/**
 * Expression event
 */
USTRUCT(BlueprintType)
struct FExpressionEvent
{
    GENERATED_BODY()

    /** Event ID */
    UPROPERTY(BlueprintReadOnly)
    int32 EventID = 0;

    /** Channel */
    UPROPERTY(BlueprintReadOnly)
    EExpressionChannel Channel = EExpressionChannel::Facial;

    /** Event type */
    UPROPERTY(BlueprintReadOnly)
    FString EventType;

    /** Intensity */
    UPROPERTY(BlueprintReadOnly)
    float Intensity = 0.0f;

    /** Timestamp */
    UPROPERTY(BlueprintReadOnly)
    float Timestamp = 0.0f;

    /** Echobeat step */
    UPROPERTY(BlueprintReadOnly)
    int32 EchobeatStep = 0;
};

/**
 * Delegate declarations
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnExpressionChanged, const FExpressionEvent&, Event);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGestureTriggered, const FGestureDefinition&, Gesture, float, Intensity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVocalParametersChanged, const FVocalParameters&, Parameters);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOn4EModeChanged, E4ECognitionMode, NewMode);

/**
 * Expressive Animation System
 * 
 * Provides comprehensive animation and vocalization control for the Deep Tree Echo avatar.
 * Implements 4E embodied cognition principles for natural, context-aware expression.
 * 
 * Key features:
 * - FACS-based facial animation with 44 action units
 * - Body posture and gesture system
 * - Gaze control with saccades and attention modeling
 * - Vocal parameter modulation (pitch, rate, breathiness)
 * - Breathing animation synchronized with emotional state
 * - 4E cognition integration (Embodied, Embedded, Enacted, Extended)
 * - Echobeats synchronization for expression timing
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UExpressiveAnimationSystem : public UActorComponent
{
    GENERATED_BODY()

public:
    UExpressiveAnimationSystem();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Enable facial animation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Channels")
    bool bEnableFacialAnimation = true;

    /** Enable body animation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Channels")
    bool bEnableBodyAnimation = true;

    /** Enable gesture system */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Channels")
    bool bEnableGestures = true;

    /** Enable gaze control */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Channels")
    bool bEnableGazeControl = true;

    /** Enable vocal modulation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Channels")
    bool bEnableVocalModulation = true;

    /** Enable breathing animation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Channels")
    bool bEnableBreathingAnimation = true;

    /** Expression smoothing factor */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Smoothing")
    float ExpressionSmoothingFactor = 0.1f;

    /** Gaze smoothing factor */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Smoothing")
    float GazeSmoothingFactor = 0.15f;

    /** Microexpression duration (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Timing")
    float MicroexpressionDuration = 0.25f;

    /** Saccade interval range (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Timing")
    FVector2D SaccadeIntervalRange = FVector2D(0.2f, 0.5f);

    /** Blink interval range (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Timing")
    FVector2D BlinkIntervalRange = FVector2D(2.0f, 6.0f);

    // ========================================
    // EVENTS
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnExpressionChanged OnExpressionChanged;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnGestureTriggered OnGestureTriggered;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnVocalParametersChanged OnVocalParametersChanged;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOn4EModeChanged On4EModeChanged;

    // ========================================
    // PUBLIC API - FACIAL ANIMATION
    // ========================================

    /** Set facial action unit intensity */
    UFUNCTION(BlueprintCallable, Category = "Facial")
    void SetActionUnitIntensity(int32 AUNumber, float Intensity, float TransitionTime = 0.3f);

    /** Get facial action unit intensity */
    UFUNCTION(BlueprintPure, Category = "Facial")
    float GetActionUnitIntensity(int32 AUNumber) const;

    /** Set multiple action units at once */
    UFUNCTION(BlueprintCallable, Category = "Facial")
    void SetActionUnits(const TArray<FFacialActionUnit>& ActionUnits, float TransitionTime = 0.3f);

    /** Get all active action units */
    UFUNCTION(BlueprintPure, Category = "Facial")
    TArray<FFacialActionUnit> GetActiveActionUnits() const;

    /** Trigger microexpression */
    UFUNCTION(BlueprintCallable, Category = "Facial")
    void TriggerMicroexpression(const FString& EmotionType, float Intensity = 0.5f);

    /** Trigger blink */
    UFUNCTION(BlueprintCallable, Category = "Facial")
    void TriggerBlink(float Duration = 0.15f);

    // ========================================
    // PUBLIC API - BODY ANIMATION
    // ========================================

    /** Set body posture */
    UFUNCTION(BlueprintCallable, Category = "Body")
    void SetBodyPosture(const FBodyPostureState& Posture, float TransitionTime = 0.5f);

    /** Get current body posture */
    UFUNCTION(BlueprintPure, Category = "Body")
    FBodyPostureState GetCurrentPosture() const;

    /** Set posture parameter */
    UFUNCTION(BlueprintCallable, Category = "Body")
    void SetPostureParameter(const FString& ParameterName, float Value, float TransitionTime = 0.3f);

    /** Apply posture from emotion */
    UFUNCTION(BlueprintCallable, Category = "Body")
    void ApplyPostureFromEmotion(const FString& EmotionType, float Intensity);

    // ========================================
    // PUBLIC API - GESTURES
    // ========================================

    /** Register gesture */
    UFUNCTION(BlueprintCallable, Category = "Gestures")
    void RegisterGesture(const FGestureDefinition& Gesture);

    /** Trigger gesture by ID */
    UFUNCTION(BlueprintCallable, Category = "Gestures")
    void TriggerGesture(const FString& GestureID, float Intensity = 1.0f);

    /** Get gesture definition */
    UFUNCTION(BlueprintPure, Category = "Gestures")
    FGestureDefinition GetGestureDefinition(const FString& GestureID) const;

    /** Get gestures for emotion */
    UFUNCTION(BlueprintPure, Category = "Gestures")
    TArray<FGestureDefinition> GetGesturesForEmotion(const FString& EmotionType) const;

    /** Is gesture playing */
    UFUNCTION(BlueprintPure, Category = "Gestures")
    bool IsGesturePlaying() const;

    // ========================================
    // PUBLIC API - GAZE CONTROL
    // ========================================

    /** Set gaze target */
    UFUNCTION(BlueprintCallable, Category = "Gaze")
    void SetGazeTarget(const FGazeTarget& Target);

    /** Look at location */
    UFUNCTION(BlueprintCallable, Category = "Gaze")
    void LookAtLocation(const FVector& Location, float AttentionWeight = 1.0f);

    /** Look at actor */
    UFUNCTION(BlueprintCallable, Category = "Gaze")
    void LookAtActor(AActor* Actor, float AttentionWeight = 1.0f);

    /** Get current gaze direction */
    UFUNCTION(BlueprintPure, Category = "Gaze")
    FVector GetCurrentGazeDirection() const;

    /** Trigger saccade */
    UFUNCTION(BlueprintCallable, Category = "Gaze")
    void TriggerSaccade();

    /** Set gaze aversion */
    UFUNCTION(BlueprintCallable, Category = "Gaze")
    void SetGazeAversion(float Amount, float Duration = 1.0f);

    // ========================================
    // PUBLIC API - VOCAL MODULATION
    // ========================================

    /** Set vocal parameters */
    UFUNCTION(BlueprintCallable, Category = "Vocal")
    void SetVocalParameters(const FVocalParameters& Parameters, float TransitionTime = 0.2f);

    /** Get current vocal parameters */
    UFUNCTION(BlueprintPure, Category = "Vocal")
    FVocalParameters GetCurrentVocalParameters() const;

    /** Set vocal parameter */
    UFUNCTION(BlueprintCallable, Category = "Vocal")
    void SetVocalParameter(const FString& ParameterName, float Value, float TransitionTime = 0.2f);

    /** Apply vocal parameters from emotion */
    UFUNCTION(BlueprintCallable, Category = "Vocal")
    void ApplyVocalFromEmotion(const FString& EmotionType, float Intensity);

    // ========================================
    // PUBLIC API - BREATHING
    // ========================================

    /** Set breathing state */
    UFUNCTION(BlueprintCallable, Category = "Breathing")
    void SetBreathingState(const FBreathingState& State, float TransitionTime = 1.0f);

    /** Get current breathing state */
    UFUNCTION(BlueprintPure, Category = "Breathing")
    FBreathingState GetCurrentBreathingState() const;

    /** Set breathing rate */
    UFUNCTION(BlueprintCallable, Category = "Breathing")
    void SetBreathingRate(float Rate, float TransitionTime = 1.0f);

    /** Trigger sigh */
    UFUNCTION(BlueprintCallable, Category = "Breathing")
    void TriggerSigh();

    /** Trigger gasp */
    UFUNCTION(BlueprintCallable, Category = "Breathing")
    void TriggerGasp();

    // ========================================
    // PUBLIC API - 4E COGNITION
    // ========================================

    /** Set 4E cognition mode */
    UFUNCTION(BlueprintCallable, Category = "4E")
    void Set4EMode(E4ECognitionMode Mode);

    /** Get current 4E state */
    UFUNCTION(BlueprintPure, Category = "4E")
    F4EExpressionState Get4EState() const;

    /** Set 4E weights */
    UFUNCTION(BlueprintCallable, Category = "4E")
    void Set4EWeights(float Embodied, float Embedded, float Enacted, float Extended);

    /** Update 4E from context */
    UFUNCTION(BlueprintCallable, Category = "4E")
    void Update4EFromContext(const FString& ContextType, float Intensity);

    // ========================================
    // PUBLIC API - ECHOBEATS INTEGRATION
    // ========================================

    /** Process echobeat step for expression */
    UFUNCTION(BlueprintCallable, Category = "Echobeats")
    void ProcessEchobeatStep(int32 Step);

    /** Get expression for echobeat phase */
    UFUNCTION(BlueprintPure, Category = "Echobeats")
    TArray<FExpressionTarget> GetEchobeatPhaseExpression(int32 Phase) const;

    /** Synchronize expression with stream */
    UFUNCTION(BlueprintCallable, Category = "Echobeats")
    void SynchronizeWithStream(int32 StreamIndex);

    // ========================================
    // PUBLIC API - COMPOSITE EXPRESSIONS
    // ========================================

    /** Apply expression preset */
    UFUNCTION(BlueprintCallable, Category = "Composite")
    void ApplyExpressionPreset(const FString& PresetName, float Intensity = 1.0f, float TransitionTime = 0.3f);

    /** Create expression from emotion blend */
    UFUNCTION(BlueprintCallable, Category = "Composite")
    void CreateExpressionFromEmotionBlend(const TMap<FString, float>& EmotionWeights);

    /** Get expression intensity */
    UFUNCTION(BlueprintPure, Category = "Composite")
    float GetOverallExpressionIntensity() const;

    /** Reset all expressions */
    UFUNCTION(BlueprintCallable, Category = "Composite")
    void ResetAllExpressions(float TransitionTime = 0.5f);

protected:
    // Component references
    UPROPERTY()
    UAdvancedEmotionBlending* EmotionBlending;

    UPROPERTY()
    UEchobeatsStreamEngine* EchobeatsEngine;

    UPROPERTY()
    USensoryInputIntegration* SensoryIntegration;

    UPROPERTY()
    USkeletalMeshComponent* AvatarMesh;

    UPROPERTY()
    UAudioComponent* VocalAudio;

    // Internal state
    TMap<int32, FFacialActionUnit> ActionUnits;
    TMap<FString, FGestureDefinition> RegisteredGestures;
    FBodyPostureState CurrentPosture;
    FBodyPostureState TargetPosture;
    FGazeTarget CurrentGazeTarget;
    FVocalParameters CurrentVocalParams;
    FVocalParameters TargetVocalParams;
    FBreathingState CurrentBreathingState;
    FBreathingState TargetBreathingState;
    F4EExpressionState Current4EState;

    FVector CurrentGazeDirection;
    float NextBlinkTime = 0.0f;
    float NextSaccadeTime = 0.0f;
    bool bIsGesturePlaying = false;
    int32 CurrentEchobeatStep = 1;
    int32 NextEventID = 1;

    // Internal methods
    void FindComponentReferences();
    void InitializeActionUnits();
    void InitializeDefaultGestures();
    void UpdateFacialAnimation(float DeltaTime);
    void UpdateBodyAnimation(float DeltaTime);
    void UpdateGazeControl(float DeltaTime);
    void UpdateVocalParameters(float DeltaTime);
    void UpdateBreathing(float DeltaTime);
    void Update4EIntegration(float DeltaTime);
    void ProcessAutonomicBehaviors(float DeltaTime);
    void BroadcastExpressionEvent(EExpressionChannel Channel, const FString& EventType, float Intensity);
    FBodyPostureState GetPostureForEmotion(const FString& EmotionType, float Intensity) const;
    FVocalParameters GetVocalForEmotion(const FString& EmotionType, float Intensity) const;
    TArray<FFacialActionUnit> GetActionUnitsForEmotion(const FString& EmotionType, float Intensity) const;
};
