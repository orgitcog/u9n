#include "ContemplativeAffordanceDetector.h"
#include "Algo/Sort.h"

UContemplativeAffordanceDetector::UContemplativeAffordanceDetector()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.0f;
}

void UContemplativeAffordanceDetector::BeginPlay()
{
    Super::BeginPlay();
    // Perform an initial scan so the state is never empty
    DetectedAffordances = ScanForAffordances();
}

void UContemplativeAffordanceDetector::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bAutoScan)
    {
        return;
    }

    AccumulatedTime += DeltaTime;
    if (AccumulatedTime >= ScanInterval)
    {
        AccumulatedTime -= ScanInterval;
        DetectedAffordances = ScanForAffordances();
    }
}

// ========================================
// PUBLIC API
// ========================================

void UContemplativeAffordanceDetector::UpdateAgentState(const FAgentReadinessState& NewState)
{
    AgentState = NewState;
}

void UContemplativeAffordanceDetector::UpdateEnvironmentState(
    const FEnvironmentalReadinessState& NewState)
{
    EnvironmentState = NewState;
}

TArray<FContemplativeAffordance> UContemplativeAffordanceDetector::ScanForAffordances()
{
    TArray<FContemplativeAffordance> Results;

    // Evaluate every practice type
    const TArray<EContemplativePractice> AllPractices = {
        EContemplativePractice::Mindfulness,
        EContemplativePractice::Reflection,
        EContemplativePractice::InsightReadiness,
        EContemplativePractice::Connection,
        EContemplativePractice::Dialogue,
        EContemplativePractice::Concentration,
        EContemplativePractice::OpenPresence
    };

    for (EContemplativePractice Practice : AllPractices)
    {
        FContemplativeAffordance Affordance = EvaluatePractice(Practice);
        if (Affordance.ConduciveLevel >= MinConduciveLevel)
        {
            Results.Add(Affordance);
        }
    }

    // Sort descending by ExpectedWisdomGain
    Algo::Sort(Results, [](const FContemplativeAffordance& A, const FContemplativeAffordance& B)
    {
        return A.ExpectedWisdomGain > B.ExpectedWisdomGain;
    });

    // Cap at max
    if (Results.Num() > MaxAffordancesReturned)
    {
        Results.SetNum(MaxAffordancesReturned);
    }

    DetectedAffordances = Results;
    return Results;
}

FContemplativeAffordance UContemplativeAffordanceDetector::GetBestAffordanceForPractice(
    EContemplativePractice Practice) const
{
    for (const FContemplativeAffordance& A : DetectedAffordances)
    {
        if (A.Practice == Practice)
        {
            return A;
        }
    }
    return FContemplativeAffordance{};
}

bool UContemplativeAffordanceDetector::IsPracticeAvailable(EContemplativePractice Practice) const
{
    for (const FContemplativeAffordance& A : DetectedAffordances)
    {
        if (A.Practice == Practice && A.ConduciveLevel >= MinConduciveLevel)
        {
            return true;
        }
    }
    return false;
}

float UContemplativeAffordanceDetector::GetOverallWisdomOpportunity() const
{
    if (DetectedAffordances.Num() == 0)
    {
        return 0.0f;
    }

    float Total = 0.0f;
    for (const FContemplativeAffordance& A : DetectedAffordances)
    {
        Total += A.ExpectedWisdomGain;
    }
    return FMath::Clamp(Total / static_cast<float>(DetectedAffordances.Num()), 0.0f, 1.0f);
}

// ========================================
// PROTECTED HELPERS
// ========================================

FContemplativeAffordance UContemplativeAffordanceDetector::EvaluatePractice(
    EContemplativePractice Practice) const
{
    FContemplativeAffordance Affordance;

    static int32 IdCounter = 0;
    Affordance.AffordanceId = FString::Printf(TEXT("CA_%d_%d"), static_cast<int32>(Practice), IdCounter++);
    Affordance.Practice = Practice;
    Affordance.ConduciveLevel = ComputeConduciveness(Practice);
    Affordance.Accessibility = ComputeAccessibility(Practice);
    Affordance.Description = DescribeAffordance(Practice);
    Affordance.CultivatedState = DescribeCultivatedState(Practice);

    // Expected wisdom gain: joint function of conduciveness and accessibility
    Affordance.ExpectedWisdomGain = Affordance.ConduciveLevel * 0.6f
        + Affordance.Accessibility * 0.4f;

    return Affordance;
}

float UContemplativeAffordanceDetector::ComputeConduciveness(
    EContemplativePractice Practice) const
{
    float Score = 0.0f;
    const float Load = AgentState.CognitiveLoad;
    const float Arousal = AgentState.Arousal;
    const float Pressure = EnvironmentState.TemporalPressure;
    const float NaturalEl = EnvironmentState.NaturalElements;
    const float Openness = EnvironmentState.SpatialOpenness;

    switch (Practice)
    {
    case EContemplativePractice::Mindfulness:
        // Favoured by low load, low arousal, low pressure, natural elements
        Score = (1.0f - Load) * 0.3f
              + (1.0f - Arousal) * 0.2f
              + (1.0f - Pressure) * 0.25f
              + NaturalEl * 0.25f;
        break;

    case EContemplativePractice::Reflection:
        // Favoured by moderate load, low pressure, high metacognitive openness
        Score = (1.0f - FMath::Abs(Load - 0.5f) * 2.0f) * 0.3f
              + (1.0f - Pressure) * 0.3f
              + AgentState.MetacognitiveOpenness * 0.4f;
        break;

    case EContemplativePractice::InsightReadiness:
        // Favoured by moderate arousal, high openness, moderate load
        Score = (1.0f - FMath::Abs(Arousal - 0.5f) * 2.0f) * 0.3f
              + AgentState.MetacognitiveOpenness * 0.4f
              + (1.0f - FMath::Abs(Load - 0.4f) * 2.5f) * 0.3f;
        break;

    case EContemplativePractice::Connection:
        // Favoured by positive valence, social density, spatial openness
        Score = FMath::Clamp((AgentState.EmotionalValence + 1.0f) * 0.5f, 0.0f, 1.0f) * 0.3f
              + FMath::Clamp(static_cast<float>(EnvironmentState.SocialDensity) / 4.0f, 0.0f, 1.0f) * 0.4f
              + Openness * 0.3f;
        break;

    case EContemplativePractice::Dialogue:
        // Requires social presence and positive valence
        Score = FMath::Clamp(static_cast<float>(EnvironmentState.SocialDensity), 0.0f, 1.0f) * 0.5f
              + FMath::Clamp((AgentState.EmotionalValence + 1.0f) * 0.5f, 0.0f, 1.0f) * 0.3f
              + (1.0f - Pressure) * 0.2f;
        break;

    case EContemplativePractice::Concentration:
        // Favoured by quiet, low social density, low load
        Score = (EnvironmentState.AmbientQuality == EAmbientQuality::Quiet ? 0.5f : 0.15f)
              + (1.0f - FMath::Clamp(static_cast<float>(EnvironmentState.SocialDensity) / 4.0f, 0.0f, 1.0f)) * 0.3f
              + (1.0f - Load) * 0.2f;
        break;

    case EContemplativePractice::OpenPresence:
        // Favoured by low pressure, natural elements, attentional stability
        Score = (1.0f - Pressure) * 0.35f
              + NaturalEl * 0.35f
              + AgentState.AttentionalStability * 0.3f;
        break;

    default:
        Score = 0.3f;
        break;
    }

    return FMath::Clamp(Score, 0.0f, 1.0f);
}

float UContemplativeAffordanceDetector::ComputeAccessibility(
    EContemplativePractice Practice) const
{
    // Accessibility falls when cognitive load is high or temporal pressure is high
    float BaseAccessibility = 1.0f - AgentState.CognitiveLoad * 0.4f
                                    - EnvironmentState.TemporalPressure * 0.4f;

    // Dialogue also requires at least one social agent
    if (Practice == EContemplativePractice::Dialogue && EnvironmentState.SocialDensity == 0)
    {
        BaseAccessibility *= 0.0f;
    }

    return FMath::Clamp(BaseAccessibility, 0.0f, 1.0f);
}

FString UContemplativeAffordanceDetector::DescribeAffordance(
    EContemplativePractice Practice) const
{
    switch (Practice)
    {
    case EContemplativePractice::Mindfulness:
        return TEXT("Conditions support present-moment non-judgmental awareness");
    case EContemplativePractice::Reflection:
        return TEXT("Conditions support metacognitive examination of own processing");
    case EContemplativePractice::InsightReadiness:
        return TEXT("Conditions support readiness for gestalt restructuring");
    case EContemplativePractice::Connection:
        return TEXT("Conditions support participatory knowing through deep engagement");
    case EContemplativePractice::Dialogue:
        return TEXT("Conditions support Socratic exchange and mutual awakening");
    case EContemplativePractice::Concentration:
        return TEXT("Conditions support one-pointed focused attention");
    case EContemplativePractice::OpenPresence:
        return TEXT("Conditions support open, non-directed receptive awareness");
    default:
        return TEXT("Contemplative affordance available");
    }
}

FString UContemplativeAffordanceDetector::DescribeCultivatedState(
    EContemplativePractice Practice) const
{
    switch (Practice)
    {
    case EContemplativePractice::Mindfulness:
        return TEXT("Enhanced decentering, reduced rumination, heightened salience clarity");
    case EContemplativePractice::Reflection:
        return TEXT("Increased metacognitive accuracy, reduced self-deception");
    case EContemplativePractice::InsightReadiness:
        return TEXT("Openness to perspectival shift, anagogic ascent potential");
    case EContemplativePractice::Connection:
        return TEXT("Participatory knowing, identity-through-relationship, agape");
    case EContemplativePractice::Dialogue:
        return TEXT("Shared meaning, mutual transformation, collective intelligence");
    case EContemplativePractice::Concentration:
        return TEXT("Samadhi-like stability, reduced mind-wandering, task mastery");
    case EContemplativePractice::OpenPresence:
        return TEXT("Broad attentional field, reduced attentional narrowing, sophrosyne");
    default:
        return TEXT("Cultivated wisdom state");
    }
}
