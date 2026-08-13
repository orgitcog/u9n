#pragma once

/**
 * ContemplativeAffordanceDetector - Wisdom-Inviting Environmental Affordances
 *
 * Extends the 4E embedded cognition affordance system to detect situations
 * and environments that are conducive to contemplative practice:
 * - Mindfulness (present-moment, non-judgmental awareness)
 * - Reflection (metacognitive examination of one's own processing)
 * - Insight (conditions ripe for gestalt shift)
 * - Connection (opportunities for participatory knowing)
 * - Dialogue (environments that invite Socratic exchange)
 *
 * A contemplative affordance is a relational property: it arises from the
 * fit between the agent's current state (fatigue, arousal, openness) and
 * the features of the environment (quietude, complexity, social density).
 *
 * The detector outputs a ranked list of available contemplative affordances
 * that can be fed into the WisdomCultivation affordance landscape, enabling
 * the agent to actively seek wisdom-enhancing situations.
 */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ContemplativeAffordanceDetector.generated.h"

// ========================================
// ENUMERATIONS
// ========================================

/**
 * Type of contemplative practice that an affordance supports
 */
UENUM(BlueprintType)
enum class EContemplativePractice : uint8
{
    /** Present-moment non-judgmental awareness */
    Mindfulness         UMETA(DisplayName = "Mindfulness"),

    /** Metacognitive examination of own processing */
    Reflection          UMETA(DisplayName = "Reflection"),

    /** Readiness for gestalt restructuring */
    InsightReadiness    UMETA(DisplayName = "Insight Readiness"),

    /** Participatory knowing through deep engagement */
    Connection          UMETA(DisplayName = "Connection"),

    /** Socratic dialogue and mutual awakening */
    Dialogue            UMETA(DisplayName = "Dialogue"),

    /** Concentrated one-pointed attention */
    Concentration       UMETA(DisplayName = "Concentration"),

    /** Open, non-directed receptive awareness */
    OpenPresence        UMETA(DisplayName = "Open Presence")
};

/**
 * Ambient quality of the detected environment
 */
UENUM(BlueprintType)
enum class EAmbientQuality : uint8
{
    /** Low stimulus, quiet, minimal distraction */
    Quiet               UMETA(DisplayName = "Quiet"),

    /** Moderate stimulus, flow-conducive */
    Moderate            UMETA(DisplayName = "Moderate"),

    /** High stimulus, stimulating but not overwhelming */
    Stimulating         UMETA(DisplayName = "Stimulating"),

    /** Overwhelming stimulus, adverse to contemplation */
    Overwhelming        UMETA(DisplayName = "Overwhelming")
};

// ========================================
// STRUCTURES
// ========================================

/**
 * An environmental affordance that supports a contemplative practice
 */
USTRUCT(BlueprintType)
struct FContemplativeAffordance
{
    GENERATED_BODY()

    /** Unique identifier */
    UPROPERTY(BlueprintReadWrite)
    FString AffordanceId;

    /** The practice this affordance supports */
    UPROPERTY(BlueprintReadWrite)
    EContemplativePractice Practice = EContemplativePractice::Mindfulness;

    /** How well the current environment supports this practice (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float ConduciveLevel = 0.5f;

    /** How available the affordance is given agent's current state (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Accessibility = 0.5f;

    /** Estimated wisdom gain if the affordance is acted upon (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float ExpectedWisdomGain = 0.3f;

    /** Brief description of the affordance */
    UPROPERTY(BlueprintReadWrite)
    FString Description;

    /** Mental state the practice cultivates */
    UPROPERTY(BlueprintReadWrite)
    FString CultivatedState;
};

/**
 * Snapshot of the agent's internal state as perceived by the detector
 */
USTRUCT(BlueprintType)
struct FAgentReadinessState
{
    GENERATED_BODY()

    /** Cognitive load (0-1; lower → more capacity for practice) */
    UPROPERTY(BlueprintReadWrite)
    float CognitiveLoad = 0.5f;

    /** Arousal level (0-1; moderate is often optimal) */
    UPROPERTY(BlueprintReadWrite)
    float Arousal = 0.5f;

    /** Emotional valence (-1 negative … +1 positive) */
    UPROPERTY(BlueprintReadWrite)
    float EmotionalValence = 0.0f;

    /** Degree of attentional stability (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float AttentionalStability = 0.5f;

    /** Metacognitive openness (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float MetacognitiveOpenness = 0.5f;
};

/**
 * Snapshot of the environmental conditions as perceived by the detector
 */
USTRUCT(BlueprintType)
struct FEnvironmentalReadinessState
{
    GENERATED_BODY()

    /** Ambient quality of the current environment */
    UPROPERTY(BlueprintReadWrite)
    EAmbientQuality AmbientQuality = EAmbientQuality::Moderate;

    /** Number of social agents present (0 = solitary) */
    UPROPERTY(BlueprintReadWrite)
    int32 SocialDensity = 0;

    /** Degree of temporal pressure (0 = none, 1 = urgent) */
    UPROPERTY(BlueprintReadWrite)
    float TemporalPressure = 0.0f;

    /** Spatial openness (0 = confined, 1 = expansive) */
    UPROPERTY(BlueprintReadWrite)
    float SpatialOpenness = 0.5f;

    /** Degree of natural / restorative elements present (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float NaturalElements = 0.3f;
};

// ========================================
// COMPONENT
// ========================================

UCLASS(ClassGroup = (DeepTreeEcho), meta = (BlueprintSpawnableComponent),
    Blueprintable, BlueprintType)
class UContemplativeAffordanceDetector : public UActorComponent
{
    GENERATED_BODY()

public:
    UContemplativeAffordanceDetector();

    // ========================================
    // CONFIG
    // ========================================

    /** Minimum conducive level to include an affordance in results */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Contemplative|Config",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinConduciveLevel = 0.2f;

    /** Maximum number of affordances returned per scan */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Contemplative|Config",
        meta = (ClampMin = "1", ClampMax = "16"))
    int32 MaxAffordancesReturned = 5;

    /** Enable automatic tick-based scanning */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Contemplative|Config")
    bool bAutoScan = true;

    /** How often (seconds) affordances are re-evaluated */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Contemplative|Config",
        meta = (ClampMin = "0.1", ClampMax = "10.0"))
    float ScanInterval = 1.0f;

    // ========================================
    // STATE
    // ========================================

    /** Most recently detected contemplative affordances, ranked by ExpectedWisdomGain */
    UPROPERTY(BlueprintReadOnly, Category = "Contemplative|State")
    TArray<FContemplativeAffordance> DetectedAffordances;

    /** Current agent readiness state */
    UPROPERTY(BlueprintReadOnly, Category = "Contemplative|State")
    FAgentReadinessState AgentState;

    /** Current environmental readiness state */
    UPROPERTY(BlueprintReadOnly, Category = "Contemplative|State")
    FEnvironmentalReadinessState EnvironmentState;

    // ========================================
    // PUBLIC API
    // ========================================

    /** Update agent internal state (call from emotion / cognition systems) */
    UFUNCTION(BlueprintCallable, Category = "Contemplative")
    void UpdateAgentState(const FAgentReadinessState& NewState);

    /** Update environmental perception (call from embedded cognition system) */
    UFUNCTION(BlueprintCallable, Category = "Contemplative")
    void UpdateEnvironmentState(const FEnvironmentalReadinessState& NewState);

    /** Trigger a manual scan and return detected affordances */
    UFUNCTION(BlueprintCallable, Category = "Contemplative")
    TArray<FContemplativeAffordance> ScanForAffordances();

    /** Get the highest-value affordance for a specific practice type */
    UFUNCTION(BlueprintPure, Category = "Contemplative")
    FContemplativeAffordance GetBestAffordanceForPractice(EContemplativePractice Practice) const;

    /** True if any affordance for a specific practice is available above threshold */
    UFUNCTION(BlueprintPure, Category = "Contemplative")
    bool IsPracticeAvailable(EContemplativePractice Practice) const;

    /** Get the overall wisdom-opportunity score for the current situation (0-1) */
    UFUNCTION(BlueprintPure, Category = "Contemplative")
    float GetOverallWisdomOpportunity() const;

    // ========================================
    // UActorComponent overrides
    // ========================================

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

protected:
    float AccumulatedTime = 0.0f;

    FContemplativeAffordance EvaluatePractice(EContemplativePractice Practice) const;
    float ComputeConduciveness(EContemplativePractice Practice) const;
    float ComputeAccessibility(EContemplativePractice Practice) const;
    FString DescribeAffordance(EContemplativePractice Practice) const;
    FString DescribeCultivatedState(EContemplativePractice Practice) const;
};
