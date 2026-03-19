#pragma once

/**
 * DialogicalPracticeFramework - Socratic Dialogue and Mutual Awakening
 *
 * Implements Vervaeke's dialogical practices for wisdom cultivation:
 *
 * SOCRATIC METHOD
 *   - Ask clarifying questions to expose hidden assumptions
 *   - Seek precise definitions ("What do you mean by X?")
 *   - Examine implications and internal contradictions (elenchus)
 *   - Recognise productive aporia (productive not-knowing)
 *
 * FROM DIALECTIC TO DIALOGOS
 *   - Begin with competing theses (dialectic)
 *   - Move toward shared exploration (dialogos)
 *   - Listen deeply (not just waiting to respond)
 *   - Speak authentically (no performance)
 *   - Create shared meaning that neither party held alone
 *   - Foster mutual transformation through encounter
 *
 * COLLECTIVE INTELLIGENCE
 *   - Joint attention on a shared problem
 *   - Complementary perspectives amplify insight
 *   - Emergent understanding exceeds individual contributions
 *   - All participants are mutually transformed
 *
 * The framework tracks dialogue sessions, records insights, and exposes
 * metrics that integrate with the RelevanceRealizationEnnead component.
 */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DialogicalPracticeFramework.generated.h"

// ========================================
// ENUMERATIONS
// ========================================

/**
 * Phase of a Socratic dialogue exchange
 */
UENUM(BlueprintType)
enum class ESocraticPhase : uint8
{
    /** Opening: establishing shared vocabulary */
    OpeningExchange         UMETA(DisplayName = "Opening Exchange"),

    /** Clarifying definitions and scope */
    Clarification           UMETA(DisplayName = "Clarification"),

    /** Examining assumptions (elenchus) */
    AssumptionExamination   UMETA(DisplayName = "Assumption Examination"),

    /** Recognising contradiction or aporia */
    AporiaRecognition       UMETA(DisplayName = "Aporia Recognition"),

    /** Collaborative exploration (dialogos) */
    CollaborativeExploration UMETA(DisplayName = "Collaborative Exploration"),

    /** Emergent synthesis / shared meaning */
    EmergentSynthesis       UMETA(DisplayName = "Emergent Synthesis"),

    /** Mutual transformation acknowledged */
    MutualTransformation    UMETA(DisplayName = "Mutual Transformation")
};

/**
 * Role of a participant in the dialogue
 */
UENUM(BlueprintType)
enum class EDialogueRole : uint8
{
    /** Primary questioner / guide */
    Questioner  UMETA(DisplayName = "Questioner"),

    /** Respondent being questioned */
    Respondent  UMETA(DisplayName = "Respondent"),

    /** Equal collaborative partner */
    Partner     UMETA(DisplayName = "Partner"),

    /** Observer / witness */
    Observer    UMETA(DisplayName = "Observer")
};

/**
 * Quality of a dialogue exchange turn
 */
UENUM(BlueprintType)
enum class EExchangeQuality : uint8
{
    /** Shallow: information exchange only */
    Shallow         UMETA(DisplayName = "Shallow"),

    /** Clarifying: improving mutual understanding */
    Clarifying      UMETA(DisplayName = "Clarifying"),

    /** Deepening: exposing assumptions or contradictions */
    Deepening       UMETA(DisplayName = "Deepening"),

    /** Transformative: shifting perspective for one or both parties */
    Transformative  UMETA(DisplayName = "Transformative")
};

// ========================================
// STRUCTURES
// ========================================

/**
 * A single turn in a dialogue exchange
 */
USTRUCT(BlueprintType)
struct FDialogueTurn
{
    GENERATED_BODY()

    /** Identifier of the speaker */
    UPROPERTY(BlueprintReadWrite)
    FString SpeakerId;

    /** Role of the speaker in this exchange */
    UPROPERTY(BlueprintReadWrite)
    EDialogueRole Role = EDialogueRole::Partner;

    /** Content summary (not the full text, just a semantic tag) */
    UPROPERTY(BlueprintReadWrite)
    FString ContentTag;

    /** Quality of this turn */
    UPROPERTY(BlueprintReadWrite)
    EExchangeQuality Quality = EExchangeQuality::Shallow;

    /** Whether this turn advanced the dialogue phase */
    UPROPERTY(BlueprintReadWrite)
    bool bAdvancedPhase = false;

    /** Session-relative time (seconds) */
    UPROPERTY(BlueprintReadWrite)
    float Timestamp = 0.0f;
};

/**
 * An insight that emerged from the dialogue
 */
USTRUCT(BlueprintType)
struct FDialogicalInsight
{
    GENERATED_BODY()

    /** Semantic content of the insight */
    UPROPERTY(BlueprintReadWrite)
    FString Content;

    /** Which participant(s) it was new to ("both", speaker id, ...) */
    UPROPERTY(BlueprintReadWrite)
    FString NewToParticipants;

    /** Depth score (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Depth = 0.5f;

    /** Was this transformative for the self-agent? */
    UPROPERTY(BlueprintReadWrite)
    bool bTransformativeSelf = false;

    /** Session-relative time (seconds) */
    UPROPERTY(BlueprintReadWrite)
    float Timestamp = 0.0f;
};

/**
 * State of an active or completed dialogue session
 */
USTRUCT(BlueprintType)
struct FDialogueSession
{
    GENERATED_BODY()

    /** Session identifier */
    UPROPERTY(BlueprintReadWrite)
    FString SessionId;

    /** Identifier of the dialogue partner (or "group") */
    UPROPERTY(BlueprintReadWrite)
    FString PartnerId;

    /** Current phase */
    UPROPERTY(BlueprintReadWrite)
    ESocraticPhase Phase = ESocraticPhase::OpeningExchange;

    /** Topic or problem under investigation */
    UPROPERTY(BlueprintReadWrite)
    FString Topic;

    /** Turn log */
    UPROPERTY(BlueprintReadWrite)
    TArray<FDialogueTurn> Turns;

    /** Insights that have emerged so far */
    UPROPERTY(BlueprintReadWrite)
    TArray<FDialogicalInsight> Insights;

    /** Overall quality score (0-1, updated after each turn) */
    UPROPERTY(BlueprintReadWrite)
    float OverallQuality = 0.0f;

    /** Degree of mutual transformation achieved (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float MutualTransformationDegree = 0.0f;

    /** True when the session has been formally closed */
    UPROPERTY(BlueprintReadWrite)
    bool bClosed = false;

    /** Session elapsed time (seconds) */
    UPROPERTY(BlueprintReadWrite)
    float Duration = 0.0f;
};

// ========================================
// COMPONENT
// ========================================

UCLASS(ClassGroup = (DeepTreeEcho), meta = (BlueprintSpawnableComponent),
    Blueprintable, BlueprintType)
class UDialogicalPracticeFramework : public UActorComponent
{
    GENERATED_BODY()

public:
    UDialogicalPracticeFramework();

    // ========================================
    // CONFIG
    // ========================================

    /** Minimum quality for an exchange turn to advance the dialogue phase */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue|Config",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float PhaseAdvancementThreshold = 0.6f;

    /** Maximum number of turns before the session auto-progresses to synthesis */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue|Config",
        meta = (ClampMin = "2", ClampMax = "128"))
    int32 MaxTurnsBeforeSynthesis = 20;

    /** Minimum insight depth required to count as transformative */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue|Config",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float TransformativeInsightThreshold = 0.7f;

    // ========================================
    // STATE
    // ========================================

    /** Currently active dialogue session (invalid if no session) */
    UPROPERTY(BlueprintReadOnly, Category = "Dialogue|State")
    FDialogueSession ActiveSession;

    /** Whether a dialogue session is currently in progress */
    UPROPERTY(BlueprintReadOnly, Category = "Dialogue|State")
    bool bSessionActive = false;

    /** All completed sessions this run */
    UPROPERTY(BlueprintReadOnly, Category = "Dialogue|State")
    TArray<FDialogueSession> CompletedSessions;

    /** Cumulative wisdom gain from all dialogue sessions (0-1, capped) */
    UPROPERTY(BlueprintReadOnly, Category = "Dialogue|State")
    float CumulativeWisdomGain = 0.0f;

    // ========================================
    // PUBLIC API - SESSION LIFECYCLE
    // ========================================

    /** Begin a new Socratic dialogue session */
    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    bool BeginSession(const FString& PartnerId, const FString& Topic);

    /** Submit a dialogue turn and receive quality feedback */
    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    EExchangeQuality SubmitTurn(const FString& SpeakerId, EDialogueRole Role,
        const FString& ContentTag);

    /** Record an insight that emerged from the current session */
    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    void RecordInsight(const FString& Content, const FString& NewToParticipants, float Depth);

    /** Manually advance the dialogue to the next phase */
    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    void AdvancePhase();

    /** Close the current session and archive it */
    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    FDialogueSession CloseSession();

    // ========================================
    // PUBLIC API - SOCRATIC TOOLS
    // ========================================

    /** Generate a clarifying question appropriate for the current phase */
    UFUNCTION(BlueprintPure, Category = "Dialogue|Socratic")
    FString GenerateClarifyingQuestion(const FString& CurrentTopic) const;

    /** Generate an assumption-probing question */
    UFUNCTION(BlueprintPure, Category = "Dialogue|Socratic")
    FString GenerateAssumptionProbe(const FString& Claim) const;

    /** Generate an implication-testing question */
    UFUNCTION(BlueprintPure, Category = "Dialogue|Socratic")
    FString GenerateImplicationTest(const FString& Claim) const;

    /** Detect whether an aporia (productive not-knowing) has been reached */
    UFUNCTION(BlueprintPure, Category = "Dialogue|Socratic")
    bool DetectAporia() const;

    // ========================================
    // PUBLIC API - METRICS
    // ========================================

    /** Get the overall quality of the active session (0-1) */
    UFUNCTION(BlueprintPure, Category = "Dialogue|Metrics")
    float GetSessionQuality() const;

    /** Get the degree of mutual transformation in the active session (0-1) */
    UFUNCTION(BlueprintPure, Category = "Dialogue|Metrics")
    float GetMutualTransformationDegree() const;

    /** Count of transformative turns in the active session */
    UFUNCTION(BlueprintPure, Category = "Dialogue|Metrics")
    int32 GetTransformativeTurnCount() const;

    // ========================================
    // UActorComponent overrides
    // ========================================

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

protected:
    float SessionTime = 0.0f;
    int32 SessionCounter = 0;

    ESocraticPhase NextPhase(ESocraticPhase Current) const;
    EExchangeQuality EvaluateTurnQuality(const FString& ContentTag,
        EDialogueRole Role, ESocraticPhase Phase) const;
    void UpdateSessionQuality();
    void UpdateMutualTransformation();
    void UpdateCumulativeWisdom(const FDialogueSession& ClosedSession);
};
