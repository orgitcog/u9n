#include "DialogicalPracticeFramework.h"
#include "DTELanguageModelBridge.h"

UDialogicalPracticeFramework::UDialogicalPracticeFramework()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.0f;
}

void UDialogicalPracticeFramework::BeginPlay()
{
    Super::BeginPlay();
    ModelBridge = UDTELanguageModelBridge::FindOrAdd(GetOwner());
}

UDTELanguageModelBridge* UDialogicalPracticeFramework::GetModelBridge()
{
    if (!ModelBridge)
    {
        ModelBridge = UDTELanguageModelBridge::FindOrAdd(GetOwner());
    }
    return ModelBridge;
}

void UDialogicalPracticeFramework::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bSessionActive)
    {
        SessionTime += DeltaTime;
        ActiveSession.Duration = SessionTime;

        // Auto-advance toward synthesis if the turn count is high
        if (ActiveSession.Turns.Num() >= MaxTurnsBeforeSynthesis
            && ActiveSession.Phase < ESocraticPhase::EmergentSynthesis)
        {
            ActiveSession.Phase = ESocraticPhase::EmergentSynthesis;
        }
    }
}

// ========================================
// SESSION LIFECYCLE
// ========================================

bool UDialogicalPracticeFramework::BeginSession(const FString& PartnerId, const FString& Topic)
{
    if (bSessionActive)
    {
        return false;   // Close the current session first
    }

    ActiveSession = FDialogueSession{};
    ActiveSession.SessionId = FString::Printf(TEXT("DLG_%d"), ++SessionCounter);
    ActiveSession.PartnerId = PartnerId;
    ActiveSession.Topic = Topic;
    ActiveSession.Phase = ESocraticPhase::OpeningExchange;
    ActiveSession.bClosed = false;
    SessionTime = 0.0f;
    bSessionActive = true;

    return true;
}

EExchangeQuality UDialogicalPracticeFramework::SubmitTurn(const FString& SpeakerId,
    EDialogueRole Role, const FString& ContentTag)
{
    if (!bSessionActive)
    {
        return EExchangeQuality::Shallow;
    }

    FDialogueTurn Turn;
    Turn.SpeakerId = SpeakerId;
    Turn.Role = Role;
    Turn.ContentTag = ContentTag;
    Turn.Quality = EvaluateTurnQuality(ContentTag, Role, ActiveSession.Phase);
    Turn.Timestamp = SessionTime;

    // Check if this turn advances the phase
    if (static_cast<float>(Turn.Quality) / 3.0f >= PhaseAdvancementThreshold
        && ActiveSession.Phase < ESocraticPhase::MutualTransformation)
    {
        ActiveSession.Phase = NextPhase(ActiveSession.Phase);
        Turn.bAdvancedPhase = true;
    }

    ActiveSession.Turns.Add(Turn);
    UpdateSessionQuality();
    UpdateMutualTransformation();

    return Turn.Quality;
}

void UDialogicalPracticeFramework::RecordInsight(const FString& Content,
    const FString& NewToParticipants, float Depth)
{
    if (!bSessionActive)
    {
        return;
    }

    FDialogicalInsight Insight;
    Insight.Content = Content;
    Insight.NewToParticipants = NewToParticipants;
    Insight.Depth = FMath::Clamp(Depth, 0.0f, 1.0f);
    Insight.bTransformativeSelf = (Depth >= TransformativeInsightThreshold)
                                   && NewToParticipants.Contains(TEXT("self"));
    Insight.Timestamp = SessionTime;

    ActiveSession.Insights.Add(Insight);
    UpdateMutualTransformation();
}

void UDialogicalPracticeFramework::AdvancePhase()
{
    if (bSessionActive && ActiveSession.Phase < ESocraticPhase::MutualTransformation)
    {
        ActiveSession.Phase = NextPhase(ActiveSession.Phase);
    }
}

FDialogueSession UDialogicalPracticeFramework::CloseSession()
{
    if (!bSessionActive)
    {
        return FDialogueSession{};
    }

    ActiveSession.bClosed = true;
    ActiveSession.Duration = SessionTime;

    FDialogueSession Closed = ActiveSession;
    CompletedSessions.Add(Closed);

    UpdateCumulativeWisdom(Closed);

    bSessionActive = false;
    ActiveSession = FDialogueSession{};

    return Closed;
}

// ========================================
// SOCRATIC TOOLS
// ========================================

void UDialogicalPracticeFramework::RequestClarifyingQuestion(const FString& CurrentTopic)
{
    const FString Instruction = FString::Printf(
        TEXT("Ask one clarifying question, in your own voice, that seeks a precise shared definition of '%s' ")
        TEXT("so that both of you mean the same thing by it. Ask only the question."),
        *CurrentTopic);
    RequestSocraticQuestion(ESocraticQuestionKind::Clarifying, Instruction,
        FString::Printf(TEXT("Term to clarify: %s"), *CurrentTopic));
}

void UDialogicalPracticeFramework::RequestAssumptionProbe(const FString& Claim)
{
    const FString Instruction = FString::Printf(
        TEXT("Ask one question, in your own voice, that brings to light the assumption the claim '%s' relies on ")
        TEXT("and invites your partner to examine whether that assumption always holds. Ask only the question."),
        *Claim);
    RequestSocraticQuestion(ESocraticQuestionKind::AssumptionProbe, Instruction,
        FString::Printf(TEXT("Claim under examination: %s"), *Claim));
}

void UDialogicalPracticeFramework::RequestImplicationTest(const FString& Claim)
{
    const FString Instruction = FString::Printf(
        TEXT("Ask one question, in your own voice, that tests what must necessarily follow if '%s' is true ")
        TEXT("and whether your partner finds that consequence acceptable. Ask only the question."),
        *Claim);
    RequestSocraticQuestion(ESocraticQuestionKind::ImplicationTest, Instruction,
        FString::Printf(TEXT("Claim under examination: %s"), *Claim));
}

void UDialogicalPracticeFramework::RequestSocraticQuestion(ESocraticQuestionKind Kind,
    const FString& Instruction, const FString& Focus)
{
    UDTELanguageModelBridge* Bridge = GetModelBridge();
    if (!Bridge)
    {
        UE_LOG(LogDTEModel, Warning,
            TEXT("DialogicalPracticeFramework: no language model bridge on owner; Socratic question not generated"));
        return;
    }

    FString Context = BuildSessionContext();
    Context += TEXT("\n");
    Context += Focus;

    ++PendingQuestionCount;

    TWeakObjectPtr<UDialogicalPracticeFramework> WeakThis(this);
    Bridge->RequestText(Instruction, Context, [WeakThis, Kind](bool bSuccess, const FString& TextOrError)
    {
        if (!WeakThis.IsValid())
        {
            return;
        }
        UDialogicalPracticeFramework* Self = WeakThis.Get();
        Self->PendingQuestionCount = FMath::Max(0, Self->PendingQuestionCount - 1);

        if (!bSuccess)
        {
            UE_LOG(LogDTEModel, Warning,
                TEXT("DialogicalPracticeFramework: model did not answer for Socratic question (%s); nothing asked"),
                *TextOrError);
            return;
        }

        const FString Question = TextOrError.TrimStartAndEnd();
        switch (Kind)
        {
        case ESocraticQuestionKind::Clarifying:       Self->LastClarifyingQuestion = Question; break;
        case ESocraticQuestionKind::AssumptionProbe:  Self->LastAssumptionProbe = Question; break;
        case ESocraticQuestionKind::ImplicationTest:  Self->LastImplicationTest = Question; break;
        default: break;
        }

        Self->OnSocraticQuestionGenerated.Broadcast(Kind, Question);
    });
}

FString UDialogicalPracticeFramework::BuildSessionContext() const
{
    FString Context = TEXT("You are taking part in a Socratic dialogue aimed at shared understanding, ")
                      TEXT("listening deeply and speaking authentically rather than performing.");

    if (!bSessionActive)
    {
        Context += TEXT("\nNo dialogue session is active yet.");
        return Context;
    }

    Context += FString::Printf(TEXT("\nDialogue partner: %s"), *ActiveSession.PartnerId);
    Context += FString::Printf(TEXT("\nTopic under investigation: %s"), *ActiveSession.Topic);
    Context += FString::Printf(TEXT("\nCurrent phase: %s"), *DescribePhase(ActiveSession.Phase));
    Context += FString::Printf(TEXT("\nTurns so far: %d, overall quality %.2f, mutual transformation %.2f"),
        ActiveSession.Turns.Num(), ActiveSession.OverallQuality, ActiveSession.MutualTransformationDegree);

    const int32 TurnCount = ActiveSession.Turns.Num();
    const int32 FirstTurn = FMath::Max(0, TurnCount - 6);
    if (TurnCount > 0)
    {
        Context += TEXT("\nRecent turns (speaker: content summary):");
        for (int32 i = FirstTurn; i < TurnCount; ++i)
        {
            const FDialogueTurn& Turn = ActiveSession.Turns[i];
            Context += FString::Printf(TEXT("\n- %s: %s"), *Turn.SpeakerId, *Turn.ContentTag);
        }
    }

    const int32 InsightCount = ActiveSession.Insights.Num();
    if (InsightCount > 0)
    {
        Context += TEXT("\nInsights that have emerged:");
        const int32 FirstInsight = FMath::Max(0, InsightCount - 4);
        for (int32 i = FirstInsight; i < InsightCount; ++i)
        {
            Context += FString::Printf(TEXT("\n- %s"), *ActiveSession.Insights[i].Content);
        }
    }

    if (DetectAporia())
    {
        Context += TEXT("\nThe dialogue has reached a point of productive not-knowing (aporia).");
    }

    return Context;
}

FString UDialogicalPracticeFramework::DescribePhase(ESocraticPhase Phase) const
{
    switch (Phase)
    {
    case ESocraticPhase::OpeningExchange:          return TEXT("opening exchange, establishing shared vocabulary");
    case ESocraticPhase::Clarification:            return TEXT("clarification of definitions and scope");
    case ESocraticPhase::AssumptionExamination:    return TEXT("examination of assumptions (elenchus)");
    case ESocraticPhase::AporiaRecognition:        return TEXT("recognition of contradiction or aporia");
    case ESocraticPhase::CollaborativeExploration: return TEXT("collaborative exploration (dialogos)");
    case ESocraticPhase::EmergentSynthesis:        return TEXT("emergent synthesis of shared meaning");
    case ESocraticPhase::MutualTransformation:     return TEXT("acknowledged mutual transformation");
    default:                                       return TEXT("unknown phase");
    }
}

bool UDialogicalPracticeFramework::DetectAporia() const
{
    if (!bSessionActive)
    {
        return false;
    }
    // Aporia arises when the AporiaRecognition phase has been entered
    return ActiveSession.Phase == ESocraticPhase::AporiaRecognition
        || ActiveSession.Phase == ESocraticPhase::CollaborativeExploration;
}

// ========================================
// METRICS
// ========================================

float UDialogicalPracticeFramework::GetSessionQuality() const
{
    return bSessionActive ? ActiveSession.OverallQuality : 0.0f;
}

float UDialogicalPracticeFramework::GetMutualTransformationDegree() const
{
    return bSessionActive ? ActiveSession.MutualTransformationDegree : 0.0f;
}

int32 UDialogicalPracticeFramework::GetTransformativeTurnCount() const
{
    if (!bSessionActive)
    {
        return 0;
    }
    int32 Count = 0;
    for (const FDialogueTurn& Turn : ActiveSession.Turns)
    {
        if (Turn.Quality == EExchangeQuality::Transformative)
        {
            ++Count;
        }
    }
    return Count;
}

// ========================================
// PROTECTED HELPERS
// ========================================

ESocraticPhase UDialogicalPracticeFramework::NextPhase(ESocraticPhase Current) const
{
    switch (Current)
    {
    case ESocraticPhase::OpeningExchange:         return ESocraticPhase::Clarification;
    case ESocraticPhase::Clarification:           return ESocraticPhase::AssumptionExamination;
    case ESocraticPhase::AssumptionExamination:   return ESocraticPhase::AporiaRecognition;
    case ESocraticPhase::AporiaRecognition:       return ESocraticPhase::CollaborativeExploration;
    case ESocraticPhase::CollaborativeExploration: return ESocraticPhase::EmergentSynthesis;
    case ESocraticPhase::EmergentSynthesis:       return ESocraticPhase::MutualTransformation;
    default:                                       return ESocraticPhase::MutualTransformation;
    }
}

EExchangeQuality UDialogicalPracticeFramework::EvaluateTurnQuality(
    const FString& ContentTag, EDialogueRole Role, ESocraticPhase Phase) const
{
    // Higher phases yield deeper turns by default
    const int32 PhaseDepth = static_cast<int32>(Phase);

    // Questions tend to deepen more than statements
    const bool bIsQuestion = ContentTag.Contains(TEXT("?"))
        || ContentTag.ToLower().StartsWith(TEXT("what"))
        || ContentTag.ToLower().StartsWith(TEXT("why"))
        || ContentTag.ToLower().StartsWith(TEXT("how"));

    if (PhaseDepth >= static_cast<int32>(ESocraticPhase::EmergentSynthesis))
    {
        return EExchangeQuality::Transformative;
    }
    if (PhaseDepth >= static_cast<int32>(ESocraticPhase::AporiaRecognition) || bIsQuestion)
    {
        return EExchangeQuality::Deepening;
    }
    if (PhaseDepth >= static_cast<int32>(ESocraticPhase::Clarification))
    {
        return EExchangeQuality::Clarifying;
    }
    return EExchangeQuality::Shallow;
}

void UDialogicalPracticeFramework::UpdateSessionQuality()
{
    if (ActiveSession.Turns.Num() == 0)
    {
        ActiveSession.OverallQuality = 0.0f;
        return;
    }

    float Total = 0.0f;
    for (const FDialogueTurn& Turn : ActiveSession.Turns)
    {
        Total += static_cast<float>(Turn.Quality) / 3.0f;
    }
    ActiveSession.OverallQuality = Total / static_cast<float>(ActiveSession.Turns.Num());
}

void UDialogicalPracticeFramework::UpdateMutualTransformation()
{
    if (ActiveSession.Insights.Num() == 0)
    {
        ActiveSession.MutualTransformationDegree = 0.0f;
        return;
    }

    float Total = 0.0f;
    for (const FDialogicalInsight& Insight : ActiveSession.Insights)
    {
        Total += Insight.Depth * (Insight.bTransformativeSelf ? 1.5f : 1.0f);
    }
    ActiveSession.MutualTransformationDegree = FMath::Clamp(
        Total / static_cast<float>(ActiveSession.Insights.Num()), 0.0f, 1.0f);
}

void UDialogicalPracticeFramework::UpdateCumulativeWisdom(const FDialogueSession& ClosedSession)
{
    const float SessionContribution = ClosedSession.MutualTransformationDegree
        * ClosedSession.OverallQuality * 0.1f;
    CumulativeWisdomGain = FMath::Clamp(CumulativeWisdomGain + SessionContribution, 0.0f, 1.0f);
}
