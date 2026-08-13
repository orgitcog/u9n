#include "DialogicalPracticeFramework.h"

UDialogicalPracticeFramework::UDialogicalPracticeFramework()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.0f;
}

void UDialogicalPracticeFramework::BeginPlay()
{
    Super::BeginPlay();
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

FString UDialogicalPracticeFramework::GenerateClarifyingQuestion(
    const FString& CurrentTopic) const
{
    return FString::Printf(
        TEXT("What exactly do you mean when you say '%s'? Can we agree on a precise definition?"),
        *CurrentTopic);
}

FString UDialogicalPracticeFramework::GenerateAssumptionProbe(const FString& Claim) const
{
    return FString::Printf(
        TEXT("What assumption is '%s' relying on? Is that assumption always valid?"),
        *Claim);
}

FString UDialogicalPracticeFramework::GenerateImplicationTest(const FString& Claim) const
{
    return FString::Printf(
        TEXT("If '%s' is true, what must necessarily follow? Does that consequence seem acceptable?"),
        *Claim);
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
