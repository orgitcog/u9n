// DeepTreeEchoUnrealIntegration.h
// Enhanced Unreal Engine integration for Deep Tree Echo cognitive framework
// Implements full 4E embodied cognition with avatar system integration

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "DeepTreeEchoUnrealIntegration.generated.h"

// Forward declarations
class UDeepTreeEchoCore;
class UDeepTreeEchoReservoir;
class UAvatarEvolutionSystem;

/**
 * EchoBeats Step - The 12-step cognitive cycle
 * Based on OEIS A000081 nesting structure
 */
UENUM(BlueprintType)
enum class EEchoBeatsStep : uint8
{
    Step1_Perceive UMETA(DisplayName = "Perceive (Stream 1)"),
    Step2_Orient UMETA(DisplayName = "Orient (Stream 2)"),
    Step3_Reflect UMETA(DisplayName = "Reflect (Stream 3)"),
    Step4_Integrate UMETA(DisplayName = "Integrate (Stream 1)"),
    Step5_Decide UMETA(DisplayName = "Decide (Stream 2)"),
    Step6_Simulate UMETA(DisplayName = "Simulate (Stream 3)"),
    Step7_Act UMETA(DisplayName = "Act (Stream 1)"),
    Step8_Observe UMETA(DisplayName = "Observe (Stream 2)"),
    Step9_Learn UMETA(DisplayName = "Learn (Stream 3)"),
    Step10_Consolidate UMETA(DisplayName = "Consolidate (Stream 1)"),
    Step11_Anticipate UMETA(DisplayName = "Anticipate (Stream 2)"),
    Step12_Transcend UMETA(DisplayName = "Transcend (Stream 3)")
};

/**
 * Consciousness Stream - One of three concurrent cognitive streams
 */
USTRUCT(BlueprintType)
struct FConsciousnessStream
{
    GENERATED_BODY()

    /** Stream identifier (1, 2, or 3) */
    UPROPERTY(BlueprintReadOnly)
    int32 StreamId = 0;

    /** Current step in the 12-step cycle */
    UPROPERTY(BlueprintReadOnly)
    EEchoBeatsStep CurrentStep = EEchoBeatsStep::Step1_Perceive;

    /** Phase offset (0, 4, or 8 steps) */
    UPROPERTY(BlueprintReadOnly)
    int32 PhaseOffset = 0;

    /** Current activation level (0.0 to 1.0) */
    UPROPERTY(BlueprintReadWrite)
    float ActivationLevel = 0.5f;

    /** Awareness of other streams */
    UPROPERTY(BlueprintReadWrite)
    TMap<int32, float> MutualAwareness;

    /** Current processing focus */
    UPROPERTY(BlueprintReadWrite)
    FString ProcessingFocus;
};

/**
 * Diary Entry - Experience logging for cognitive learning
 */
USTRUCT(BlueprintType)
struct FDiaryEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FDateTime Timestamp;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Summary;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Location;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> KeyEntities;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGameplayTagContainer ContextTags;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString EmotionalValence;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ImportanceScore = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Reflection;

    /** 4E cognition markers */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EmbodiedRelevance = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EmbeddedRelevance = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EnactedRelevance = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ExtendedRelevance = 0.5f;
};

/**
 * Insight Entry - Pattern recognition from diary analysis
 */
USTRUCT(BlueprintType)
struct FInsightEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FDateTime DiscoveryTime;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString InsightText;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> SupportingPatterns;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 SupportingEntriesCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float WisdomWeight = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Confidence = 0.0f;

    /** Relevance realization score */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RelevanceScore = 0.0f;
};

/**
 * Avatar Expression State - Current avatar emotional/cognitive expression
 */
USTRUCT(BlueprintType)
struct FAvatarExpressionState
{
    GENERATED_BODY()

    /** Primary emotion (0-1 intensity) */
    UPROPERTY(BlueprintReadWrite)
    TMap<FString, float> EmotionIntensities;

    /** Cognitive mode indicator */
    UPROPERTY(BlueprintReadWrite)
    FString CognitiveMode;

    /** Attention focus point in world space */
    UPROPERTY(BlueprintReadWrite)
    FVector AttentionFocus;

    /** Eye glow intensity (tied to cognitive activation) */
    UPROPERTY(BlueprintReadWrite)
    float EyeGlowIntensity = 0.5f;

    /** Aura color (HSV) */
    UPROPERTY(BlueprintReadWrite)
    FLinearColor AuraColor;

    /** Aura pulse rate (Hz) */
    UPROPERTY(BlueprintReadWrite)
    float AuraPulseRate = 1.0f;

    /** Hair physics modulation */
    UPROPERTY(BlueprintReadWrite)
    float HairDynamicsMultiplier = 1.0f;

    /** Breathing rate (breaths per minute) */
    UPROPERTY(BlueprintReadWrite)
    float BreathingRate = 12.0f;

    /** Micro-expression blend weights */
    UPROPERTY(BlueprintReadWrite)
    TMap<FString, float> MicroExpressions;
};

/**
 * Deep Tree Echo Unreal Integration Component
 * 
 * Central integration point between Deep Tree Echo cognitive architecture
 * and Unreal Engine systems. Implements:
 * - 12-step EchoBeats cognitive cycle
 * - 3 concurrent consciousness streams
 * - 4E embodied cognition
 * - Avatar expression system
 * - Diary/Insight/Wisdom cultivation
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UDeepTreeEchoUnrealIntegration : public UActorComponent
{
    GENERATED_BODY()

public:
    UDeepTreeEchoUnrealIntegration();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, 
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // ECHOBEATS COGNITIVE CYCLE
    // ========================================

    /** Current step in the global cognitive cycle */
    UPROPERTY(BlueprintReadOnly, Category = "EchoBeats")
    int32 CurrentCycleStep = 0;

    /** Time accumulator for cycle progression */
    UPROPERTY(BlueprintReadOnly, Category = "EchoBeats")
    float CycleTimeAccumulator = 0.0f;

    /** Duration of each cognitive step in seconds */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EchoBeats")
    float StepDuration = 0.0833f; // ~12 steps per second at 60Hz

    /** The three concurrent consciousness streams */
    UPROPERTY(BlueprintReadOnly, Category = "EchoBeats")
    TArray<FConsciousnessStream> ConsciousnessStreams;

    /** Advance the cognitive cycle by one step */
    UFUNCTION(BlueprintCallable, Category = "EchoBeats")
    void AdvanceCognitiveStep();

    /** Get the current nesting level (OEIS A000081) */
    UFUNCTION(BlueprintCallable, Category = "EchoBeats")
    int32 GetCurrentNestingLevel() const;

    /** Get the number of terms at current nesting level */
    UFUNCTION(BlueprintCallable, Category = "EchoBeats")
    int32 GetTermsAtNestingLevel(int32 Level) const;

    // ========================================
    // 4E EMBODIED COGNITION
    // ========================================

    /** Embodied state - body-based cognition */
    UPROPERTY(BlueprintReadWrite, Category = "4E Cognition|Embodied")
    TArray<float> ProprioceptiveState;

    UPROPERTY(BlueprintReadWrite, Category = "4E Cognition|Embodied")
    TArray<float> InteroceptiveState;

    UPROPERTY(BlueprintReadWrite, Category = "4E Cognition|Embodied")
    float MotorReadiness = 0.5f;

    UPROPERTY(BlueprintReadWrite, Category = "4E Cognition|Embodied")
    TMap<FString, float> SomaticMarkers;

    /** Embedded state - environment coupling */
    UPROPERTY(BlueprintReadWrite, Category = "4E Cognition|Embedded")
    TArray<FString> DetectedAffordances;

    UPROPERTY(BlueprintReadWrite, Category = "4E Cognition|Embedded")
    FString CurrentNiche;

    UPROPERTY(BlueprintReadWrite, Category = "4E Cognition|Embedded")
    TMap<FString, float> SalienceMap;

    UPROPERTY(BlueprintReadWrite, Category = "4E Cognition|Embedded")
    float EnvironmentCoupling = 0.5f;

    /** Enacted state - action-oriented cognition */
    UPROPERTY(BlueprintReadWrite, Category = "4E Cognition|Enacted")
    TMap<FString, float> SensorimotorContingencies;

    UPROPERTY(BlueprintReadWrite, Category = "4E Cognition|Enacted")
    TMap<FString, float> PredictionErrors;

    UPROPERTY(BlueprintReadWrite, Category = "4E Cognition|Enacted")
    float EnactiveEngagement = 0.5f;

    /** Extended state - tool-using cognition */
    UPROPERTY(BlueprintReadWrite, Category = "4E Cognition|Extended")
    TArray<FString> ExternalMemoryRefs;

    UPROPERTY(BlueprintReadWrite, Category = "4E Cognition|Extended")
    TArray<FString> ActiveTools;

    UPROPERTY(BlueprintReadWrite, Category = "4E Cognition|Extended")
    float ExtensionIntegration = 0.5f;

    /** Compute overall 4E integration score */
    UFUNCTION(BlueprintCallable, Category = "4E Cognition")
    float Compute4EIntegrationScore() const;

    /** Update 4E state from sensory input */
    UFUNCTION(BlueprintCallable, Category = "4E Cognition")
    void Update4EStateFromSensors();

    // ========================================
    // DIARY & INSIGHT SYSTEM
    // ========================================

    /** All diary entries */
    UPROPERTY(BlueprintReadOnly, Category = "Diary")
    TArray<FDiaryEntry> DiaryEntries;

    /** Discovered insights */
    UPROPERTY(BlueprintReadOnly, Category = "Insights")
    TArray<FInsightEntry> Insights;

    /** Current wisdom score */
    UPROPERTY(BlueprintReadOnly, Category = "Wisdom")
    float WisdomScore = 0.0f;

    /** Add a new diary entry */
    UFUNCTION(BlueprintCallable, Category = "Diary")
    void AddDiaryEntry(const FDiaryEntry& NewEntry);

    /** Analyze diary for insights using reservoir computing */
    UFUNCTION(BlueprintCallable, Category = "Insights")
    void AnalyzeForInsights();

    /** Generate insight from pattern detection */
    UFUNCTION(BlueprintCallable, Category = "Insights")
    FInsightEntry GenerateInsightFromPatterns(const TArray<FDiaryEntry>& RelevantEntries);

    /** Cultivate wisdom from insights */
    UFUNCTION(BlueprintCallable, Category = "Wisdom")
    void CultivateWisdom();

    // ========================================
    // AVATAR EXPRESSION SYSTEM
    // ========================================

    /** Current avatar expression state */
    UPROPERTY(BlueprintReadOnly, Category = "Avatar")
    FAvatarExpressionState CurrentExpressionState;

    /** Update avatar expression from cognitive state */
    UFUNCTION(BlueprintCallable, Category = "Avatar")
    void UpdateAvatarExpression();

    /** Set emotion intensity */
    UFUNCTION(BlueprintCallable, Category = "Avatar")
    void SetEmotionIntensity(const FString& EmotionName, float Intensity);

    /** Blend to target expression over time */
    UFUNCTION(BlueprintCallable, Category = "Avatar")
    void BlendToExpression(const FAvatarExpressionState& TargetState, float BlendTime);

    /** Get eye gaze direction based on attention */
    UFUNCTION(BlueprintCallable, Category = "Avatar")
    FVector GetEyeGazeDirection() const;

    /** Compute aura color from emotional state */
    UFUNCTION(BlueprintCallable, Category = "Avatar")
    FLinearColor ComputeAuraColor() const;

    // ========================================
    // RELEVANCE REALIZATION
    // ========================================

    /** Current salience landscape */
    UPROPERTY(BlueprintReadOnly, Category = "Relevance")
    TMap<FString, float> SalienceLandscape;

    /** Attention allocation weights */
    UPROPERTY(BlueprintReadOnly, Category = "Relevance")
    TMap<FString, float> AttentionWeights;

    /** Update relevance realization */
    UFUNCTION(BlueprintCallable, Category = "Relevance")
    void UpdateRelevanceRealization();

    /** Compute salience for an entity */
    UFUNCTION(BlueprintCallable, Category = "Relevance")
    float ComputeSalience(const FString& EntityId) const;

    /** Allocate attention based on salience */
    UFUNCTION(BlueprintCallable, Category = "Relevance")
    void AllocateAttention();

    // ========================================
    // GESTALT PROCESSING
    // ========================================

    /** Current figure (foreground focus) */
    UPROPERTY(BlueprintReadOnly, Category = "Gestalt")
    FString CurrentFigure;

    /** Current ground (background context) */
    UPROPERTY(BlueprintReadOnly, Category = "Gestalt")
    TArray<FString> CurrentGround;

    /** Gestalt coherence score */
    UPROPERTY(BlueprintReadOnly, Category = "Gestalt")
    float GestaltCoherence = 0.5f;

    /** Update figure-ground dynamics */
    UFUNCTION(BlueprintCallable, Category = "Gestalt")
    void UpdateFigureGround();

    /** Attempt gestalt shift */
    UFUNCTION(BlueprintCallable, Category = "Gestalt")
    bool AttemptGestaltShift(const FString& NewFigure);

    // ========================================
    // COMPONENT REFERENCES
    // ========================================

    /** Reference to core cognitive component */
    UPROPERTY(BlueprintReadOnly, Category = "Components")
    UDeepTreeEchoCore* CognitiveCore;

    /** Reference to reservoir computing component */
    UPROPERTY(BlueprintReadOnly, Category = "Components")
    UDeepTreeEchoReservoir* ReservoirComponent;

    /** Reference to avatar evolution system */
    UPROPERTY(BlueprintReadOnly, Category = "Components")
    UAvatarEvolutionSystem* AvatarSystem;

    // ========================================
    // EVENTS
    // ========================================

    /** Called when cognitive step advances */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCognitiveStepAdvanced, int32, NewStep);
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnCognitiveStepAdvanced OnCognitiveStepAdvanced;

    /** Called when insight is discovered */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInsightDiscovered, const FInsightEntry&, NewInsight);
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnInsightDiscovered OnInsightDiscovered;

    /** Called when gestalt shift occurs */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGestaltShift, const FString&, OldFigure, const FString&, NewFigure);
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnGestaltShift OnGestaltShift;

    /** Called when wisdom level changes */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWisdomChanged, float, NewWisdomScore);
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnWisdomChanged OnWisdomChanged;

private:
    /** Initialize consciousness streams */
    void InitializeConsciousnessStreams();

    /** Update mutual awareness between streams */
    void UpdateMutualAwareness();

    /** Process current cognitive step */
    void ProcessCognitiveStep(int32 Step);

    /** Map step to stream */
    int32 GetStreamForStep(int32 Step) const;
};
