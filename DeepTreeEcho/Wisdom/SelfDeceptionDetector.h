#pragma once

/**
 * SelfDeceptionDetector - Metacognitive Safeguards Against Self-Deception
 *
 * Implements explicit bias detection and active open-mindedness mechanisms
 * grounded in Vervaeke's analysis of self-deception as a fundamental
 * obstacle to wisdom and relevance realization.
 *
 * SELF-DECEPTION MODES DETECTED
 *   - Confirmation bias: seeking only confirming evidence
 *   - Motivated reasoning: reasoning backward from desired conclusion
 *   - Narrative self-serving: reshaping past events to flatter self
 *   - Attentional capture: fixating on emotionally charged stimuli
 *   - Premature closure: halting inquiry before genuine understanding
 *   - Dissonance avoidance: fleeing from belief-violating information
 *   - Overfitting to past: treating yesterday's solutions as today's answers
 *
 * DETECTION APPROACH
 *   Each bias is tracked via observable signals in the cognitive cycle:
 *   - Prediction-error patterns (unusual asymmetry)
 *   - Attention distribution (skewed salience)
 *   - Goal-evidence mismatch
 *   - Coherence collapse after new information
 *
 * COUNTERMEASURES
 *   When a bias is detected above threshold the detector:
 *   1. Flags the active reasoning context
 *   2. Injects a counter-question via the Socratic probe interface
 *   3. Suggests a practice from ContemplativeAffordanceDetector
 *   4. Reports to AutognosisSystem for self-model update
 */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SelfDeceptionDetector.generated.h"

// ========================================
// ENUMERATIONS
// ========================================

/**
 * Type of self-deception bias
 */
UENUM(BlueprintType)
enum class ESelfDeceptionBias : uint8
{
    ConfirmationBias        UMETA(DisplayName = "Confirmation Bias"),
    MotivatedReasoning      UMETA(DisplayName = "Motivated Reasoning"),
    NarrativeSelfServing    UMETA(DisplayName = "Narrative Self-Serving"),
    AttentionalCapture      UMETA(DisplayName = "Attentional Capture"),
    PrematureClosure        UMETA(DisplayName = "Premature Closure"),
    DissonanceAvoidance     UMETA(DisplayName = "Dissonance Avoidance"),
    OverfittingToPast       UMETA(DisplayName = "Overfitting to Past")
};

/**
 * Severity of a detected bias
 */
UENUM(BlueprintType)
enum class EBiasSeverity : uint8
{
    None        UMETA(DisplayName = "None"),
    Mild        UMETA(DisplayName = "Mild"),
    Moderate    UMETA(DisplayName = "Moderate"),
    Severe      UMETA(DisplayName = "Severe")
};

// ========================================
// STRUCTURES
// ========================================

/**
 * Signal feeding the bias estimator
 */
USTRUCT(BlueprintType)
struct FBiasSignal
{
    GENERATED_BODY()

    /** Asymmetry of prediction errors: positive = confirming, negative = disconfirming
     *  Range: -1 (only disconfirming) … +1 (only confirming) */
    UPROPERTY(BlueprintReadWrite)
    float ErrorAsymmetry = 0.0f;

    /** Proportion of attention allocated to emotionally charged stimuli (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float EmotionalAttentionRatio = 0.3f;

    /** How often reasoning reversed normal direction: conclusion → evidence (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float BackwardReasoningRate = 0.0f;

    /** Coherence drop after receiving disconfirming information (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float DissonanceCoherenceDrop = 0.0f;

    /** How similar new solutions are to historically successful ones (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float SolutionStaleness = 0.3f;

    /** Time elapsed without revising a belief under conflicting evidence (seconds) */
    UPROPERTY(BlueprintReadWrite)
    float BeliefRevisionLatency = 0.0f;
};

/**
 * Report of a single detected bias event
 */
USTRUCT(BlueprintType)
struct FBiasDetectionEvent
{
    GENERATED_BODY()

    /** Bias type detected */
    UPROPERTY(BlueprintReadWrite)
    ESelfDeceptionBias Bias = ESelfDeceptionBias::ConfirmationBias;

    /** Severity of the detection */
    UPROPERTY(BlueprintReadWrite)
    EBiasSeverity Severity = EBiasSeverity::Mild;

    /** Score that triggered the event (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Score = 0.0f;

    /** Human-readable description of the detection */
    UPROPERTY(BlueprintReadWrite)
    FString Description;

    /** Suggested corrective action */
    UPROPERTY(BlueprintReadWrite)
    FString SuggestedAction;

    /** Session-relative time (seconds) */
    UPROPERTY(BlueprintReadWrite)
    float Timestamp = 0.0f;
};

// ========================================
// COMPONENT
// ========================================

UCLASS(ClassGroup = (DeepTreeEcho), meta = (BlueprintSpawnableComponent),
    Blueprintable, BlueprintType)
class USelfDeceptionDetector : public UActorComponent
{
    GENERATED_BODY()

public:
    USelfDeceptionDetector();

    // ========================================
    // CONFIG
    // ========================================

    /** Score above which a bias is considered Mild */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SelfDeception|Config",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MildBiasThreshold = 0.3f;

    /** Score above which a bias is considered Moderate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SelfDeception|Config",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ModerateBiasThreshold = 0.55f;

    /** Score above which a bias is considered Severe */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SelfDeception|Config",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float SevereBiasThreshold = 0.75f;

    /** How often (seconds) bias scores are re-evaluated */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SelfDeception|Config",
        meta = (ClampMin = "0.1", ClampMax = "30.0"))
    float EvaluationInterval = 2.0f;

    /** Enable automatic tick-based evaluation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SelfDeception|Config")
    bool bAutoEvaluate = true;

    // ========================================
    // STATE
    // ========================================

    /** Current bias signals (updated externally by cognition systems) */
    UPROPERTY(BlueprintReadOnly, Category = "SelfDeception|State")
    FBiasSignal CurrentSignals;

    /** Per-bias score map (ESelfDeceptionBias index → score 0-1) */
    UPROPERTY(BlueprintReadOnly, Category = "SelfDeception|State")
    TMap<uint8, float> BiasScores;

    /** Detection events raised this session */
    UPROPERTY(BlueprintReadOnly, Category = "SelfDeception|State")
    TArray<FBiasDetectionEvent> DetectionHistory;

    /** Overall self-deception risk score (0-1) */
    UPROPERTY(BlueprintReadOnly, Category = "SelfDeception|State")
    float OverallRisk = 0.0f;

    /** Metacognitive clarity score (inverse of overall risk) */
    UPROPERTY(BlueprintReadOnly, Category = "SelfDeception|State")
    float MetacognitiveClarity = 1.0f;

    // ========================================
    // PUBLIC API
    // ========================================

    /** Update bias signals from external cognitive monitoring */
    UFUNCTION(BlueprintCallable, Category = "SelfDeception")
    void UpdateBiasSignals(const FBiasSignal& NewSignals);

    /** Trigger a manual evaluation pass */
    UFUNCTION(BlueprintCallable, Category = "SelfDeception")
    TArray<FBiasDetectionEvent> EvaluateBiases();

    /** Get current score for a specific bias (0-1) */
    UFUNCTION(BlueprintPure, Category = "SelfDeception")
    float GetBiasScore(ESelfDeceptionBias Bias) const;

    /** Get the severity level for a specific bias */
    UFUNCTION(BlueprintPure, Category = "SelfDeception")
    EBiasSeverity GetBiasSeverity(ESelfDeceptionBias Bias) const;

    /** Get the most recently detected event (if any) */
    UFUNCTION(BlueprintPure, Category = "SelfDeception")
    FBiasDetectionEvent GetLatestDetectionEvent() const;

    /** True if any bias is currently at or above Moderate severity */
    UFUNCTION(BlueprintPure, Category = "SelfDeception")
    bool IsSignificantBiasActive() const;

    /** Generate a Socratic counter-question for the dominant active bias */
    UFUNCTION(BlueprintPure, Category = "SelfDeception")
    FString GenerateCounterQuestion() const;

    // ========================================
    // UActorComponent overrides
    // ========================================

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

protected:
    float AccumulatedTime = 0.0f;
    float SessionTime = 0.0f;

    float ComputeBiasScore(ESelfDeceptionBias Bias) const;
    EBiasSeverity ClassifySeverity(float Score) const;
    FBiasDetectionEvent BuildDetectionEvent(ESelfDeceptionBias Bias,
        float Score, EBiasSeverity Severity) const;
    FString DescribeBias(ESelfDeceptionBias Bias) const;
    FString SuggestAction(ESelfDeceptionBias Bias) const;
    FString CounterQuestion(ESelfDeceptionBias Bias) const;
    void UpdateOverallRisk();
    ESelfDeceptionBias DominantBias() const;
};
