#pragma once

/**
 * MeaningIntegrator - Coordination of Nomological, Normative & Narrative Orders
 *
 * Implements Vervaeke's "three orders" framework for meaning-making:
 *
 *   NOMOLOGICAL ORDER  — How things causally work
 *     Scientific / mechanistic understanding, predictive models, natural laws.
 *     DTE contribution: Reservoir predictions, causal chain tracking.
 *
 *   NORMATIVE ORDER    — What matters and why
 *     Values, ethics, ought/should, good/bad, mattering.
 *     DTE contribution: Goal hierarchy, somatic markers, wisdom cultivation.
 *
 *   NARRATIVE ORDER    — How things develop through time
 *     Stories, continuity, self-narrative, historical embedding.
 *     DTE contribution: Episodic memory, avatar evolution, causal history.
 *
 * Meaning arises when the three orders are coherently integrated.
 * The meaning crisis occurs when they fragment:
 *   - Nomological without Normative → nihilism ("science answers how, not why")
 *   - Normative without Nomological → superstition / ideology
 *   - Narrative without coherent Normative → identity confusion
 *   - All three fragmented → full meaning crisis
 *
 * INTEGRATION MECHANISMS
 *   The component continuously monitors the coherence of each order and
 *   their mutual alignment, triggers repair strategies when fragmentation
 *   is detected, and exposes a MeaningCoherence score (0-1) that other
 *   subsystems can use for well-being and decision-making.
 */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MeaningIntegrator.generated.h"

// ========================================
// ENUMERATIONS
// ========================================

/**
 * The three orders of understanding
 */
UENUM(BlueprintType)
enum class EMeaningOrder : uint8
{
    Nomological UMETA(DisplayName = "Nomological (How Things Work)"),
    Normative   UMETA(DisplayName = "Normative (What Matters)"),
    Narrative   UMETA(DisplayName = "Narrative (How Things Develop)")
};

/**
 * Degree of meaning coherence
 */
UENUM(BlueprintType)
enum class EMeaningCoherenceLevel : uint8
{
    /** All three orders integrated: rich meaning */
    Integrated      UMETA(DisplayName = "Integrated"),

    /** Two orders aligned; one lagging */
    Partial         UMETA(DisplayName = "Partial"),

    /** One or more orders fragmented */
    Fragmented      UMETA(DisplayName = "Fragmented"),

    /** Full meaning crisis: all orders misaligned */
    Crisis          UMETA(DisplayName = "Crisis")
};

/**
 * Strategy for repairing a fragmented order
 */
UENUM(BlueprintType)
enum class ERepairStrategy : uint8
{
    /** Deepen causal understanding to reinforce nomological order */
    DeepCausalInquiry           UMETA(DisplayName = "Deep Causal Inquiry"),

    /** Re-examine values and re-align goals to normative order */
    ValueClarification          UMETA(DisplayName = "Value Clarification"),

    /** Re-narrate personal history for coherent identity */
    NarrativeReintegration      UMETA(DisplayName = "Narrative Re-integration"),

    /** Seek transformative experience to reconnect all orders */
    TransformativeExperience    UMETA(DisplayName = "Transformative Experience"),

    /** Engage dialogical practice for inter-subjective grounding */
    DialogicalGrounding         UMETA(DisplayName = "Dialogical Grounding")
};

// ========================================
// STRUCTURES
// ========================================

/**
 * State of a single meaning order
 */
USTRUCT(BlueprintType)
struct FMeaningOrderState
{
    GENERATED_BODY()

    /** The order this state represents */
    UPROPERTY(BlueprintReadWrite)
    EMeaningOrder Order = EMeaningOrder::Nomological;

    /** Internal coherence of this order (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float InternalCoherence = 0.5f;

    /** How richly populated with content this order currently is (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float ContentRichness = 0.5f;

    /** Alignment with each of the other two orders (0-1 each) */
    UPROPERTY(BlueprintReadWrite)
    float AlignmentWithOther1 = 0.5f;

    UPROPERTY(BlueprintReadWrite)
    float AlignmentWithOther2 = 0.5f;

    /** Is this order currently in a fragmentation event? */
    UPROPERTY(BlueprintReadWrite)
    bool bFragmented = false;
};

/**
 * An event where two orders became misaligned
 */
USTRUCT(BlueprintType)
struct FFragmentationEvent
{
    GENERATED_BODY()

    /** Which orders fragmented */
    UPROPERTY(BlueprintReadWrite)
    EMeaningOrder OrderA = EMeaningOrder::Nomological;

    UPROPERTY(BlueprintReadWrite)
    EMeaningOrder OrderB = EMeaningOrder::Normative;

    /** Degree of misalignment at time of event (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float MisalignmentDegree = 0.0f;

    /** Repair strategy recommended */
    UPROPERTY(BlueprintReadWrite)
    ERepairStrategy RecommendedRepair = ERepairStrategy::ValueClarification;

    /** Session-relative time (seconds) */
    UPROPERTY(BlueprintReadWrite)
    float Timestamp = 0.0f;
};

/**
 * Full integration state of all three orders
 */
USTRUCT(BlueprintType)
struct FMeaningIntegrationState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    FMeaningOrderState Nomological;

    UPROPERTY(BlueprintReadWrite)
    FMeaningOrderState Normative;

    UPROPERTY(BlueprintReadWrite)
    FMeaningOrderState Narrative;

    /** Overall meaning coherence (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float MeaningCoherence = 0.5f;

    /** Current coherence level classification */
    UPROPERTY(BlueprintReadWrite)
    EMeaningCoherenceLevel CoherenceLevel = EMeaningCoherenceLevel::Partial;

    /** Dominant meaning focus right now */
    UPROPERTY(BlueprintReadWrite)
    EMeaningOrder DominantOrder = EMeaningOrder::Narrative;
};

// ========================================
// COMPONENT
// ========================================

UCLASS(ClassGroup = (DeepTreeEcho), meta = (BlueprintSpawnableComponent),
    Blueprintable, BlueprintType)
class UMeaningIntegrator : public UActorComponent
{
    GENERATED_BODY()

public:
    UMeaningIntegrator();

    // ========================================
    // CONFIG
    // ========================================

    /** Alignment below this value triggers a fragmentation event */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meaning|Config",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float FragmentationThreshold = 0.3f;

    /** MeaningCoherence below this triggers a Crisis classification */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meaning|Config",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float CrisisThreshold = 0.25f;

    /** MeaningCoherence above this counts as Integrated */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meaning|Config",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float IntegrationThreshold = 0.75f;

    /** How often (seconds) the integration is re-evaluated */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meaning|Config",
        meta = (ClampMin = "0.1", ClampMax = "10.0"))
    float EvaluationInterval = 1.0f;

    /** Enable automatic tick-based evaluation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meaning|Config")
    bool bAutoEvaluate = true;

    // ========================================
    // STATE
    // ========================================

    /** Current integration state */
    UPROPERTY(BlueprintReadOnly, Category = "Meaning|State")
    FMeaningIntegrationState IntegrationState;

    /** Log of fragmentation events */
    UPROPERTY(BlueprintReadOnly, Category = "Meaning|State")
    TArray<FFragmentationEvent> FragmentationHistory;

    // ========================================
    // PUBLIC API - UPDATE ORDERS
    // ========================================

    /** Update nomological order state (call from Reservoir / causal tracking) */
    UFUNCTION(BlueprintCallable, Category = "Meaning")
    void UpdateNomologicalOrder(float InternalCoherence, float ContentRichness);

    /** Update normative order state (call from goal / value systems) */
    UFUNCTION(BlueprintCallable, Category = "Meaning")
    void UpdateNormativeOrder(float InternalCoherence, float ContentRichness);

    /** Update narrative order state (call from episodic memory / avatar evolution) */
    UFUNCTION(BlueprintCallable, Category = "Meaning")
    void UpdateNarrativeOrder(float InternalCoherence, float ContentRichness);

    // ========================================
    // PUBLIC API - QUERY
    // ========================================

    /** Trigger a manual integration evaluation */
    UFUNCTION(BlueprintCallable, Category = "Meaning")
    void EvaluateIntegration();

    /** Get the current meaning coherence score (0-1) */
    UFUNCTION(BlueprintPure, Category = "Meaning")
    float GetMeaningCoherence() const;

    /** Get the current coherence level */
    UFUNCTION(BlueprintPure, Category = "Meaning")
    EMeaningCoherenceLevel GetCoherenceLevel() const;

    /** Get the dominant active meaning order */
    UFUNCTION(BlueprintPure, Category = "Meaning")
    EMeaningOrder GetDominantOrder() const;

    /** Get coherence state for a specific order */
    UFUNCTION(BlueprintPure, Category = "Meaning")
    FMeaningOrderState GetOrderState(EMeaningOrder Order) const;

    /** True when a meaning crisis is active */
    UFUNCTION(BlueprintPure, Category = "Meaning")
    bool IsMeaningCrisisActive() const;

    /** Get recommended repair strategy for the current fragmentation */
    UFUNCTION(BlueprintPure, Category = "Meaning")
    ERepairStrategy GetRecommendedRepairStrategy() const;

    // ========================================
    // UActorComponent overrides
    // ========================================

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

protected:
    float AccumulatedTime = 0.0f;
    float SessionTime = 0.0f;

    void ComputeAlignments();
    float ComputeOrderPairAlignment(const FMeaningOrderState& A, const FMeaningOrderState& B) const;
    float ComputeOverallCoherence() const;
    EMeaningCoherenceLevel ClassifyCoherence(float Coherence) const;
    EMeaningOrder FindDominantOrder() const;
    void DetectFragmentations();
    ERepairStrategy RecommendRepair(EMeaningOrder A, EMeaningOrder B, float Misalignment) const;
};
