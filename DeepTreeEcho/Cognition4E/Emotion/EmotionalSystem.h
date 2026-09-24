// EmotionalSystem.h
// Affective Computing and Emotional Processing for Deep Tree Echo
// Implements dimensional and categorical emotion models with somatic markers

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EmotionalSystem.generated.h"

// Forward declarations
class UCognitiveCycleManager;
class UMemorySystems;
class UAttentionSystem;

/**
 * Basic emotion categories
 */
UENUM(BlueprintType)
enum class EBasicEmotion : uint8
{
    Joy         UMETA(DisplayName = "Joy"),
    Sadness     UMETA(DisplayName = "Sadness"),
    Fear        UMETA(DisplayName = "Fear"),
    Anger       UMETA(DisplayName = "Anger"),
    Surprise    UMETA(DisplayName = "Surprise"),
    Disgust     UMETA(DisplayName = "Disgust"),
    Trust       UMETA(DisplayName = "Trust"),
    Anticipation UMETA(DisplayName = "Anticipation"),
    Neutral     UMETA(DisplayName = "Neutral")
};

/**
 * Complex/social emotions
 */
UENUM(BlueprintType)
enum class EComplexEmotion : uint8
{
    Pride       UMETA(DisplayName = "Pride"),
    Shame       UMETA(DisplayName = "Shame"),
    Guilt       UMETA(DisplayName = "Guilt"),
    Envy        UMETA(DisplayName = "Envy"),
    Gratitude   UMETA(DisplayName = "Gratitude"),
    Compassion  UMETA(DisplayName = "Compassion"),
    Awe         UMETA(DisplayName = "Awe"),
    Contempt    UMETA(DisplayName = "Contempt"),
    Love        UMETA(DisplayName = "Love"),
    Hope        UMETA(DisplayName = "Hope")
};

/**
 * Mood state
 */
UENUM(BlueprintType)
enum class EMoodState : uint8
{
    Elated      UMETA(DisplayName = "Elated"),
    Happy       UMETA(DisplayName = "Happy"),
    Content     UMETA(DisplayName = "Content"),
    Calm        UMETA(DisplayName = "Calm"),
    Bored       UMETA(DisplayName = "Bored"),
    Sad         UMETA(DisplayName = "Sad"),
    Anxious     UMETA(DisplayName = "Anxious"),
    Stressed    UMETA(DisplayName = "Stressed"),
    Angry       UMETA(DisplayName = "Angry")
};

/**
 * Dimensional emotion state (PAD model)
 */
USTRUCT(BlueprintType)
struct FDimensionalEmotion
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Pleasure = 0.0f;  // Valence: -1 (negative) to +1 (positive)

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Arousal = 0.5f;   // Activation: 0 (calm) to 1 (excited)

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Dominance = 0.5f; // Control: 0 (submissive) to 1 (dominant)
};

/**
 * Categorical emotion state
 */
USTRUCT(BlueprintType)
struct FCategoricalEmotion
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EBasicEmotion PrimaryEmotion = EBasicEmotion::Neutral;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PrimaryIntensity = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EBasicEmotion SecondaryEmotion = EBasicEmotion::Neutral;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SecondaryIntensity = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<EBasicEmotion, float> EmotionProfile;
};

/**
 * Somatic marker
 */
USTRUCT(BlueprintType)
struct FSomaticMarker
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString MarkerID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString AssociatedContext;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Valence = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Intensity = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EBasicEmotion AssociatedEmotion = EBasicEmotion::Neutral;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CreationTime = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ActivationCount = 0;
};

/**
 * Emotional event
 */
USTRUCT(BlueprintType)
struct FEmotionalEvent
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString EventID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Trigger;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EBasicEmotion Emotion = EBasicEmotion::Neutral;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Intensity = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Timestamp = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Duration = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsResolved = false;
};

/**
 * Appraisal result
 */
USTRUCT(BlueprintType)
struct FAppraisalResult
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Relevance = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Congruence = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Controllability = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Novelty = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Certainty = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EBasicEmotion ResultingEmotion = EBasicEmotion::Neutral;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ResultingIntensity = 0.0f;
};

/**
 * Emotional state summary
 */
USTRUCT(BlueprintType)
struct FEmotionalState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FDimensionalEmotion Dimensional;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FCategoricalEmotion Categorical;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EMoodState CurrentMood = EMoodState::Calm;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MoodStability = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EmotionalReactivity = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RegulationCapacity = 0.5f;
};

/**
 * Delegate declarations
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEmotionTriggered, EBasicEmotion, Emotion, float, Intensity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMoodChanged, EMoodState, OldMood, EMoodState, NewMood);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSomaticMarkerActivated, const FSomaticMarker&, Marker);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEmotionalEvent, const FEmotionalEvent&, Event);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAppraisalComplete, const FAppraisalResult&, Result);

/**
 * Emotional System Component
 * Implements affective computing with dimensional and categorical models
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UEmotionalSystem : public UActorComponent
{
    GENERATED_BODY()

public:
    UEmotionalSystem();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, 
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float EmotionDecayRate = 0.05f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float MoodInertia = 0.9f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float BaselineArousal = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float BaselineValence = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    int32 MaxSomaticMarkers = 100;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    int32 MaxEmotionalEvents = 50;

    // ========================================
    // EVENTS
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnEmotionTriggered OnEmotionTriggered;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnMoodChanged OnMoodChanged;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnSomaticMarkerActivated OnSomaticMarkerActivated;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnEmotionalEvent OnEmotionalEvent;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnAppraisalComplete OnAppraisalComplete;

    // ========================================
    // EMOTION TRIGGERING
    // ========================================

    /** Trigger a basic emotion */
    UFUNCTION(BlueprintCallable, Category = "Emotions")
    void TriggerEmotion(EBasicEmotion Emotion, float Intensity, const FString& Trigger);

    /** Trigger a complex emotion */
    UFUNCTION(BlueprintCallable, Category = "Emotions")
    void TriggerComplexEmotion(EComplexEmotion Emotion, float Intensity, const FString& Trigger);

    /** Set dimensional emotion directly */
    UFUNCTION(BlueprintCallable, Category = "Emotions")
    void SetDimensionalEmotion(float Pleasure, float Arousal, float Dominance);

    /** Blend emotion into current state */
    UFUNCTION(BlueprintCallable, Category = "Emotions")
    void BlendEmotion(EBasicEmotion Emotion, float Intensity, float BlendFactor);

    // ========================================
    // APPRAISAL
    // ========================================

    /** Perform cognitive appraisal of a situation */
    UFUNCTION(BlueprintCallable, Category = "Appraisal")
    FAppraisalResult AppraiseSituation(const FString& Situation, float Relevance, 
                                        float Congruence, float Controllability);

    /** Appraise an event for emotional significance */
    UFUNCTION(BlueprintCallable, Category = "Appraisal")
    FAppraisalResult AppraiseEvent(const FString& EventDescription, 
                                    const TArray<FString>& ContextTags);

    // ========================================
    // SOMATIC MARKERS
    // ========================================

    /** Create a somatic marker */
    UFUNCTION(BlueprintCallable, Category = "Somatic Markers")
    FSomaticMarker CreateSomaticMarker(const FString& Context, float Valence, 
                                        EBasicEmotion Emotion);

    /** Activate somatic marker by context */
    UFUNCTION(BlueprintCallable, Category = "Somatic Markers")
    FSomaticMarker ActivateSomaticMarker(const FString& Context);

    /** Get somatic marker for context */
    UFUNCTION(BlueprintPure, Category = "Somatic Markers")
    FSomaticMarker GetSomaticMarker(const FString& Context) const;

    /** Get all somatic markers */
    UFUNCTION(BlueprintPure, Category = "Somatic Markers")
    TArray<FSomaticMarker> GetAllSomaticMarkers() const;

    /** Get somatic valence for decision */
    UFUNCTION(BlueprintPure, Category = "Somatic Markers")
    float GetSomaticValence(const FString& Context) const;

    // ========================================
    // STATE QUERIES
    // ========================================

    /** Get current emotional state */
    UFUNCTION(BlueprintPure, Category = "State")
    FEmotionalState GetEmotionalState() const;

    /** Get dimensional emotion */
    UFUNCTION(BlueprintPure, Category = "State")
    FDimensionalEmotion GetDimensionalEmotion() const;

    /** Get categorical emotion */
    UFUNCTION(BlueprintPure, Category = "State")
    FCategoricalEmotion GetCategoricalEmotion() const;

    /** Get current mood */
    UFUNCTION(BlueprintPure, Category = "State")
    EMoodState GetCurrentMood() const;

    /** Get primary emotion */
    UFUNCTION(BlueprintPure, Category = "State")
    EBasicEmotion GetPrimaryEmotion() const;

    /** Get primary emotion intensity */
    UFUNCTION(BlueprintPure, Category = "State")
    float GetPrimaryEmotionIntensity() const;

    /** Get valence */
    UFUNCTION(BlueprintPure, Category = "State")
    float GetValence() const;

    /** Get arousal */
    UFUNCTION(BlueprintPure, Category = "State")
    float GetArousal() const;

    /** Get recent emotional events */
    UFUNCTION(BlueprintPure, Category = "State")
    TArray<FEmotionalEvent> GetRecentEvents(int32 Count) const;

    // ========================================
    // EMOTION REGULATION
    // ========================================

    /** Regulate current emotion (suppress/enhance) */
    UFUNCTION(BlueprintCallable, Category = "Regulation")
    void RegulateEmotion(float SuppressionFactor);

    /** Reappraise situation to change emotion */
    UFUNCTION(BlueprintCallable, Category = "Regulation")
    void Reappraise(const FString& NewInterpretation);

    /** Shift attention to regulate emotion */
    UFUNCTION(BlueprintCallable, Category = "Regulation")
    void AttentionalDeployment(bool bTowardsPositive);

    /** Reset to baseline emotional state */
    UFUNCTION(BlueprintCallable, Category = "Regulation")
    void ResetToBaseline();

protected:
    // Component references
    UPROPERTY()
    UCognitiveCycleManager* CycleManager;

    UPROPERTY()
    UMemorySystems* MemoryComponent;

    UPROPERTY()
    UAttentionSystem* AttentionComponent;

    // Internal state
    FEmotionalState EmotionalState;
    TArray<FSomaticMarker> SomaticMarkers;
    TArray<FEmotionalEvent> EmotionalEvents;

    int32 MarkerIDCounter = 0;
    int32 EventIDCounter = 0;

    // Internal methods
    void FindComponentReferences();
    void InitializeEmotionalSystem();

    void UpdateEmotionDecay(float DeltaTime);
    void UpdateMood(float DeltaTime);
    void UpdateCategoricalFromDimensional();
    void UpdateDimensionalFromCategorical();

    EMoodState ComputeMoodFromDimensional() const;
    EBasicEmotion MapDimensionalToBasic(float Pleasure, float Arousal) const;
    FDimensionalEmotion MapBasicToDimensional(EBasicEmotion Emotion, float Intensity) const;

    void RecordEmotionalEvent(EBasicEmotion Emotion, float Intensity, const FString& Trigger);

    FString GenerateMarkerID();
    FString GenerateEventID();
    int32 FindMarkerIndex(const FString& Context) const;
};
