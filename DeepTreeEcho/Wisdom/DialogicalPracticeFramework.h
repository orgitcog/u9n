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
 *
 * Every Socratic question the agent asks is requested from the language
 * model through UDTELanguageModelBridge: the kind of question (clarifying,
 * assumption probe, implication test) is the instruction and the session
 * state (phase, topic, partner, recent turns, insights) is the context. The
 * generated question arrives through OnSocraticQuestionGenerated; when the
 * model does not answer, nothing is asked and a warning is logged.
 */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DialogicalPracticeFramework.generated.h"

class UDTELanguageModelBridge;

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

/**
 * Kind of Socratic question requested from the model
 */
UENUM(BlueprintType)
enum class ESocraticQuestionKind : uint8
{
    /** Seeks a precise definition of the topic under discussion */
    Clarifying          UMETA(DisplayName = "Clarifying Question"),

    /** Exposes the assumption a claim relies on */
    AssumptionProbe     UMETA(DisplayName = "Assumption Probe"),

    /** Tests what must follow if a claim is true */
    ImplicationTest     UMETA(DisplayName = "Implication Test")
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

/** Fired when the model has produced a Socratic question. Never fired with placeholder text. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSocraticQuestionGenerated, ESocraticQuestionKind, Kind, const FString&, Question);

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
    //
    // Questions are generated by the language model. Request* functions are
    // asynchronous: the question arrives through OnSocraticQuestionGenerated
    // and is also stored for the GetLast* getters. If the model does not
    // answer, nothing is broadcast and the stored question is left unchanged.

    /** Ask the model for a clarifying question appropriate for the current phase */
    UFUNCTION(BlueprintCallable, Category = "Dialogue|Socratic")
    void RequestClarifyingQuestion(const FString& CurrentTopic);

    /** Ask the model for a question that probes the assumption behind Claim */
    UFUNCTION(BlueprintCallable, Category = "Dialogue|Socratic")
    void RequestAssumptionProbe(const FString& Claim);

    /** Ask the model for a question that tests the implications of Claim */
    UFUNCTION(BlueprintCallable, Category = "Dialogue|Socratic")
    void RequestImplicationTest(const FString& Claim);

    /** Fired with each model-generated Socratic question */
    UPROPERTY(BlueprintAssignable, Category = "Dialogue|Socratic")
    FOnSocraticQuestionGenerated OnSocraticQuestionGenerated;

    /** Most recent model-generated clarifying question, or empty if none has been produced */
    UFUNCTION(BlueprintPure, Category = "Dialogue|Socratic")
    FString GetLastClarifyingQuestion() const { return LastClarifyingQuestion; }

    /** Most recent model-generated assumption probe, or empty if none has been produced */
    UFUNCTION(BlueprintPure, Category = "Dialogue|Socratic")
    FString GetLastAssumptionProbe() const { return LastAssumptionProbe; }

    /** Most recent model-generated implication test, or empty if none has been produced */
    UFUNCTION(BlueprintPure, Category = "Dialogue|Socratic")
    FString GetLastImplicationTest() const { return LastImplicationTest; }

    /** Number of Socratic question requests currently awaiting the model */
    UFUNCTION(BlueprintPure, Category = "Dialogue|Socratic")
    int32 GetPendingQuestionCount() const { return PendingQuestionCount; }

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
    /** The model bridge every question is requested from */
    UPROPERTY()
    UDTELanguageModelBridge* ModelBridge = nullptr;

    /** Stored model-generated questions (empty until the model has answered) */
    UPROPERTY(BlueprintReadOnly, Category = "Dialogue|Socratic")
    FString LastClarifyingQuestion;

    UPROPERTY(BlueprintReadOnly, Category = "Dialogue|Socratic")
    FString LastAssumptionProbe;

    UPROPERTY(BlueprintReadOnly, Category = "Dialogue|Socratic")
    FString LastImplicationTest;

    int32 PendingQuestionCount = 0;

    float SessionTime = 0.0f;
    int32 SessionCounter = 0;

    /** The cached bridge, resolving it on the owner when not yet cached */
    UDTELanguageModelBridge* GetModelBridge();

    /** Build the model request for one kind of question and dispatch it */
    void RequestSocraticQuestion(ESocraticQuestionKind Kind, const FString& Instruction, const FString& Focus);

    /** Session state as prompt context (phase, topic, partner, recent turns, insights) */
    FString BuildSessionContext() const;

    /** Model-facing name of a phase (prompt context, not character text) */
    FString DescribePhase(ESocraticPhase Phase) const;

    ESocraticPhase NextPhase(ESocraticPhase Current) const;
    EExchangeQuality EvaluateTurnQuality(const FString& ContentTag,
        EDialogueRole Role, ESocraticPhase Phase) const;
    void UpdateSessionQuality();
    void UpdateMutualTransformation();
    void UpdateCumulativeWisdom(const FDialogueSession& ClosedSession);
};
