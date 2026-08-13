#pragma once

/**
 * InsightFacilitator - Surprise Detection and Insight Facilitation
 *
 * Implements John Vervaeke's insight framework:
 * - Monitors prediction errors to detect anomalies (high surprise)
 * - Triggers metacognitive reflection when coherence is low
 * - Facilitates gestalt shifts by exploring alternative frames
 * - Integrates new understanding after a restructuring event
 *
 * Insight arises when three conditions converge:
 *   1. High prediction error (surprise)
 *   2. Low salience landscape coherence
 *   3. Meta-cognitive readiness (openness to shift)
 *
 * The system drives anagogic ascent: each insight cycle elevates the
 * agent toward a higher-order relevance-realization equilibrium.
 */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InsightFacilitator.generated.h"

// ========================================
// ENUMERATIONS
// ========================================

/**
 * Phase of the insight cycle
 */
UENUM(BlueprintType)
enum class EInsightPhase : uint8
{
    /** Normal operation - monitoring prediction errors */
    Monitoring          UMETA(DisplayName = "Monitoring"),

    /** Impasse detected - high surprise, low coherence */
    Impasse             UMETA(DisplayName = "Impasse"),

    /** Searching for alternative frames */
    FrameSearch         UMETA(DisplayName = "Frame Search"),

    /** Gestalt shift in progress */
    GestaltShift        UMETA(DisplayName = "Gestalt Shift"),

    /** New frame integrated - post-insight consolidation */
    Integration         UMETA(DisplayName = "Integration")
};

/**
 * Type of insight event
 */
UENUM(BlueprintType)
enum class EInsightType : uint8
{
    /** Sudden restructuring of a problem representation */
    ProblemRestructuring    UMETA(DisplayName = "Problem Restructuring"),

    /** Shift in perspectival frame */
    FrameShift              UMETA(DisplayName = "Frame Shift"),

    /** Discovery of hidden affordance */
    AffordanceDiscovery     UMETA(DisplayName = "Affordance Discovery"),

    /** Metacognitive self-correction */
    SelfCorrection          UMETA(DisplayName = "Self Correction"),

    /** Anagogic ascent - upward wisdom leap */
    AnagogicAscent          UMETA(DisplayName = "Anagogic Ascent")
};

// ========================================
// STRUCTURES
// ========================================

/**
 * Prediction error record from a single sensorimotor cycle
 */
USTRUCT(BlueprintType)
struct FPredictionErrorRecord
{
    GENERATED_BODY()

    /** Identifier of the prediction being checked */
    UPROPERTY(BlueprintReadWrite)
    FString PredictionId;

    /** Magnitude of the error (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float ErrorMagnitude = 0.0f;

    /** Domain: sensory, motor, cognitive, evaluative */
    UPROPERTY(BlueprintReadWrite)
    FString Domain;

    /** Timestamp relative to session start (seconds) */
    UPROPERTY(BlueprintReadWrite)
    float Timestamp = 0.0f;
};

/**
 * Candidate alternative frame for exploration
 */
USTRUCT(BlueprintType)
struct FAlternativeFrame
{
    GENERATED_BODY()

    /** Human-readable label for this frame */
    UPROPERTY(BlueprintReadWrite)
    FString Label;

    /** Brief description of the reframing */
    UPROPERTY(BlueprintReadWrite)
    FString Description;

    /** Estimated coherence if this frame is adopted (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float ProjectedCoherence = 0.5f;

    /** Novelty of this frame relative to current one (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Novelty = 0.5f;

    /** Plausibility score (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Plausibility = 0.5f;
};

/**
 * Record of a completed insight event
 */
USTRUCT(BlueprintType)
struct FInsightRecord
{
    GENERATED_BODY()

    /** Type of insight */
    UPROPERTY(BlueprintReadWrite)
    EInsightType InsightType = EInsightType::FrameShift;

    /** Frame that was abandoned */
    UPROPERTY(BlueprintReadWrite)
    FString PreviousFrame;

    /** Frame that was adopted */
    UPROPERTY(BlueprintReadWrite)
    FString NewFrame;

    /** Coherence gain from the insight (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float CoherenceGain = 0.0f;

    /** Session time when insight occurred (seconds) */
    UPROPERTY(BlueprintReadWrite)
    float OccurrenceTime = 0.0f;

    /** Number of impasse cycles before insight */
    UPROPERTY(BlueprintReadWrite)
    int32 ImpasseCycles = 0;
};

// ========================================
// COMPONENT
// ========================================

UCLASS(ClassGroup = (DeepTreeEcho), meta = (BlueprintSpawnableComponent),
    Blueprintable, BlueprintType)
class UInsightFacilitator : public UActorComponent
{
    GENERATED_BODY()

public:
    UInsightFacilitator();

    // ========================================
    // CONFIG
    // ========================================

    /** Prediction-error threshold above which impasse is detected */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insight|Config",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float SurpriseThreshold = 0.65f;

    /** Salience-landscape coherence below which impasse is confirmed */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insight|Config",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float CoherenceThreshold = 0.35f;

    /** Minimum metacognitive openness required for frame search */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insight|Config",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MetacognitiveReadinessThreshold = 0.4f;

    /** Number of alternative frames evaluated per search cycle */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insight|Config",
        meta = (ClampMin = "1", ClampMax = "16"))
    int32 FrameCandidateCount = 4;

    /** Maximum impasse cycles before forcing a random frame shift */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insight|Config",
        meta = (ClampMin = "1", ClampMax = "64"))
    int32 MaxImpasseCycles = 12;

    /** Enable automatic tick-based processing */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Insight|Config")
    bool bAutoProcess = true;

    // ========================================
    // STATE (read-only from Blueprint)
    // ========================================

    /** Current phase of the insight cycle */
    UPROPERTY(BlueprintReadOnly, Category = "Insight|State")
    EInsightPhase CurrentPhase = EInsightPhase::Monitoring;

    /** Running average of recent prediction errors (0-1) */
    UPROPERTY(BlueprintReadOnly, Category = "Insight|State")
    float RunningPredictionError = 0.0f;

    /** Current salience coherence (0-1, mirrored from WisdomCultivation) */
    UPROPERTY(BlueprintReadOnly, Category = "Insight|State")
    float CurrentSalienceCoherence = 1.0f;

    /** Current metacognitive openness (0-1) */
    UPROPERTY(BlueprintReadOnly, Category = "Insight|State")
    float MetacognitiveOpenness = 0.5f;

    /** How many consecutive impasse cycles have occurred */
    UPROPERTY(BlueprintReadOnly, Category = "Insight|State")
    int32 ImpasseCycleCount = 0;

    /** Total insight events recorded this session */
    UPROPERTY(BlueprintReadOnly, Category = "Insight|State")
    int32 TotalInsightCount = 0;

    /** Log of insight events (most recent last) */
    UPROPERTY(BlueprintReadOnly, Category = "Insight|State")
    TArray<FInsightRecord> InsightHistory;

    // ========================================
    // PUBLIC API
    // ========================================

    /** Record a prediction error from an external source */
    UFUNCTION(BlueprintCallable, Category = "Insight")
    void RecordPredictionError(const FString& PredictionId, float ErrorMagnitude, const FString& Domain);

    /** Update the salience coherence observed externally (e.g. from WisdomCultivation) */
    UFUNCTION(BlueprintCallable, Category = "Insight")
    void UpdateSalienceCoherence(float Coherence);

    /** Set current metacognitive openness (e.g. from AutognosisSystem) */
    UFUNCTION(BlueprintCallable, Category = "Insight")
    void SetMetacognitiveOpenness(float Openness);

    /** Manually trigger an impasse check */
    UFUNCTION(BlueprintCallable, Category = "Insight")
    bool CheckForImpasse();

    /** Generate candidate alternative frames for the current problem */
    UFUNCTION(BlueprintCallable, Category = "Insight")
    TArray<FAlternativeFrame> GenerateAlternativeFrames();

    /** Adopt the best candidate frame, triggering a gestalt shift */
    UFUNCTION(BlueprintCallable, Category = "Insight")
    bool TriggerGestaltShift();

    /** Integrate a successfully adopted frame (call after shift) */
    UFUNCTION(BlueprintCallable, Category = "Insight")
    void IntegrateNewGestalt(const FString& NewFrameLabel, float CoherenceGain);

    /** True when all three insight conditions hold */
    UFUNCTION(BlueprintPure, Category = "Insight")
    bool IsHighSurprise() const;

    /** True when salience landscape is fragmented */
    UFUNCTION(BlueprintPure, Category = "Insight")
    bool IsLowCoherence() const;

    /** True when agent is open to perspectival change */
    UFUNCTION(BlueprintPure, Category = "Insight")
    bool IsOpenToShift() const;

    /** Get the most recent insight record */
    UFUNCTION(BlueprintPure, Category = "Insight")
    FInsightRecord GetLastInsight() const;

    // ========================================
    // UActorComponent overrides
    // ========================================

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

protected:
    void ProcessMonitoringPhase();
    void ProcessImpassePhase();
    void ProcessFrameSearchPhase();
    void ProcessGestaltShiftPhase();
    void ProcessIntegrationPhase();

    void UpdateRunningError(float NewError);
    FAlternativeFrame BuildFrameCandidate(int32 Index) const;
    FAlternativeFrame SelectBestFrame(const TArray<FAlternativeFrame>& Candidates) const;
    void RecordInsight(EInsightType Type, const FString& Prev, const FString& Next, float Gain);

    /** Accumulated delta time for processing cadence */
    float AccumulatedTime = 0.0f;

    /** Processing interval (seconds) */
    float ProcessingInterval = 0.1f;

    /** Ring buffer of recent prediction errors */
    TArray<FPredictionErrorRecord> RecentErrors;

    /** Maximum entries in the error ring buffer */
    static constexpr int32 ErrorBufferSize = 32;

    /** Current active frame label */
    FString CurrentFrameLabel = TEXT("Default");

    /** Best frame candidate from last search */
    FAlternativeFrame BestFrameCandidate;

    /** Session elapsed time (seconds) */
    float SessionTime = 0.0f;
};
