#include "SelfDeceptionDetector.h"

USelfDeceptionDetector::USelfDeceptionDetector()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.0f;
}

void USelfDeceptionDetector::BeginPlay()
{
    Super::BeginPlay();

    // Initialise scores for every bias type
    const TArray<ESelfDeceptionBias> AllBiases = {
        ESelfDeceptionBias::ConfirmationBias,
        ESelfDeceptionBias::MotivatedReasoning,
        ESelfDeceptionBias::NarrativeSelfServing,
        ESelfDeceptionBias::AttentionalCapture,
        ESelfDeceptionBias::PrematureClosure,
        ESelfDeceptionBias::DissonanceAvoidance,
        ESelfDeceptionBias::OverfittingToPast
    };
    for (ESelfDeceptionBias Bias : AllBiases)
    {
        BiasScores.Add(static_cast<uint8>(Bias), 0.0f);
    }
    OverallRisk = 0.0f;
    MetacognitiveClarity = 1.0f;
}

void USelfDeceptionDetector::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    SessionTime += DeltaTime;

    if (!bAutoEvaluate)
    {
        return;
    }

    AccumulatedTime += DeltaTime;
    if (AccumulatedTime >= EvaluationInterval)
    {
        AccumulatedTime -= EvaluationInterval;
        EvaluateBiases();
    }
}

// ========================================
// PUBLIC API
// ========================================

void USelfDeceptionDetector::UpdateBiasSignals(const FBiasSignal& NewSignals)
{
    CurrentSignals = NewSignals;
}

TArray<FBiasDetectionEvent> USelfDeceptionDetector::EvaluateBiases()
{
    TArray<FBiasDetectionEvent> NewEvents;

    const TArray<ESelfDeceptionBias> AllBiases = {
        ESelfDeceptionBias::ConfirmationBias,
        ESelfDeceptionBias::MotivatedReasoning,
        ESelfDeceptionBias::NarrativeSelfServing,
        ESelfDeceptionBias::AttentionalCapture,
        ESelfDeceptionBias::PrematureClosure,
        ESelfDeceptionBias::DissonanceAvoidance,
        ESelfDeceptionBias::OverfittingToPast
    };

    for (ESelfDeceptionBias Bias : AllBiases)
    {
        const float Score = ComputeBiasScore(Bias);
        BiasScores.Add(static_cast<uint8>(Bias), Score);

        const EBiasSeverity Severity = ClassifySeverity(Score);
        if (Severity > EBiasSeverity::None)
        {
            FBiasDetectionEvent Event = BuildDetectionEvent(Bias, Score, Severity);
            NewEvents.Add(Event);
            DetectionHistory.Add(Event);
        }
    }

    UpdateOverallRisk();
    return NewEvents;
}

float USelfDeceptionDetector::GetBiasScore(ESelfDeceptionBias Bias) const
{
    const float* Score = BiasScores.Find(static_cast<uint8>(Bias));
    return Score ? *Score : 0.0f;
}

EBiasSeverity USelfDeceptionDetector::GetBiasSeverity(ESelfDeceptionBias Bias) const
{
    return ClassifySeverity(GetBiasScore(Bias));
}

FBiasDetectionEvent USelfDeceptionDetector::GetLatestDetectionEvent() const
{
    if (DetectionHistory.Num() > 0)
    {
        return DetectionHistory.Last();
    }
    return FBiasDetectionEvent{};
}

bool USelfDeceptionDetector::IsSignificantBiasActive() const
{
    for (const auto& Pair : BiasScores)
    {
        if (ClassifySeverity(Pair.Value) >= EBiasSeverity::Moderate)
        {
            return true;
        }
    }
    return false;
}

FString USelfDeceptionDetector::GenerateCounterQuestion() const
{
    return CounterQuestion(DominantBias());
}

// ========================================
// PROTECTED HELPERS
// ========================================

float USelfDeceptionDetector::ComputeBiasScore(ESelfDeceptionBias Bias) const
{
    float Score = 0.0f;
    switch (Bias)
    {
    case ESelfDeceptionBias::ConfirmationBias:
        // High positive error asymmetry indicates seeking confirming evidence
        Score = FMath::Clamp((CurrentSignals.ErrorAsymmetry + 1.0f) * 0.5f, 0.0f, 1.0f);
        break;

    case ESelfDeceptionBias::MotivatedReasoning:
        // High backward-reasoning rate
        Score = CurrentSignals.BackwardReasoningRate;
        break;

    case ESelfDeceptionBias::NarrativeSelfServing:
        // Combination of backward reasoning and positive error asymmetry
        Score = FMath::Clamp(
            CurrentSignals.BackwardReasoningRate * 0.5f
            + FMath::Clamp((CurrentSignals.ErrorAsymmetry + 1.0f) * 0.25f, 0.0f, 0.5f),
            0.0f, 1.0f);
        break;

    case ESelfDeceptionBias::AttentionalCapture:
        // Emotional attention ratio far above baseline (0.3)
        Score = FMath::Clamp((CurrentSignals.EmotionalAttentionRatio - 0.3f) / 0.7f, 0.0f, 1.0f);
        break;

    case ESelfDeceptionBias::PrematureClosure:
        // Long belief revision latency without contradicting evidence
        Score = FMath::Clamp(CurrentSignals.BeliefRevisionLatency / 30.0f, 0.0f, 1.0f);
        break;

    case ESelfDeceptionBias::DissonanceAvoidance:
        // Coherence drops sharply when disconfirming info arrives
        Score = CurrentSignals.DissonanceCoherenceDrop;
        break;

    case ESelfDeceptionBias::OverfittingToPast:
        // Solutions keep resembling past patterns
        Score = CurrentSignals.SolutionStaleness;
        break;

    default:
        Score = 0.0f;
        break;
    }
    return FMath::Clamp(Score, 0.0f, 1.0f);
}

EBiasSeverity USelfDeceptionDetector::ClassifySeverity(float Score) const
{
    if (Score >= SevereBiasThreshold)   return EBiasSeverity::Severe;
    if (Score >= ModerateBiasThreshold) return EBiasSeverity::Moderate;
    if (Score >= MildBiasThreshold)     return EBiasSeverity::Mild;
    return EBiasSeverity::None;
}

FBiasDetectionEvent USelfDeceptionDetector::BuildDetectionEvent(
    ESelfDeceptionBias Bias, float Score, EBiasSeverity Severity) const
{
    FBiasDetectionEvent Event;
    Event.Bias = Bias;
    Event.Severity = Severity;
    Event.Score = Score;
    Event.Description = DescribeBias(Bias);
    Event.SuggestedAction = SuggestAction(Bias);
    Event.Timestamp = SessionTime;
    return Event;
}

FString USelfDeceptionDetector::DescribeBias(ESelfDeceptionBias Bias) const
{
    switch (Bias)
    {
    case ESelfDeceptionBias::ConfirmationBias:
        return TEXT("Prediction errors are predominantly confirming; disconfirming evidence is being filtered.");
    case ESelfDeceptionBias::MotivatedReasoning:
        return TEXT("Reasoning is frequently starting from a desired conclusion and working backward to evidence.");
    case ESelfDeceptionBias::NarrativeSelfServing:
        return TEXT("The agent's self-narrative is being selectively shaped to reinforce a favourable self-image.");
    case ESelfDeceptionBias::AttentionalCapture:
        return TEXT("Emotional stimuli are commanding a disproportionate share of the salience landscape.");
    case ESelfDeceptionBias::PrematureClosure:
        return TEXT("Belief revision is being deferred despite accumulating contradictory evidence.");
    case ESelfDeceptionBias::DissonanceAvoidance:
        return TEXT("Disconfirming information is causing coherence collapse rather than adaptive updating.");
    case ESelfDeceptionBias::OverfittingToPast:
        return TEXT("Solutions are being drawn from past patterns without sufficient sensitivity to novelty.");
    default:
        return TEXT("Cognitive bias detected.");
    }
}

FString USelfDeceptionDetector::SuggestAction(ESelfDeceptionBias Bias) const
{
    switch (Bias)
    {
    case ESelfDeceptionBias::ConfirmationBias:
        return TEXT("Actively seek disconfirming evidence; apply a steelmanning exercise to opposing views.");
    case ESelfDeceptionBias::MotivatedReasoning:
        return TEXT("Temporarily bracket the desired outcome and reason forward from the evidence alone.");
    case ESelfDeceptionBias::NarrativeSelfServing:
        return TEXT("Apply honest self-examination; invite candid external feedback from a trusted partner.");
    case ESelfDeceptionBias::AttentionalCapture:
        return TEXT("Engage a mindfulness or open-presence practice to broaden attentional scope.");
    case ESelfDeceptionBias::PrematureClosure:
        return TEXT("Schedule a deliberate belief-revision interval; use the InsightFacilitator to probe alternatives.");
    case ESelfDeceptionBias::DissonanceAvoidance:
        return TEXT("Practise tolerating cognitive dissonance; reframe disconfirmation as valuable updating signal.");
    case ESelfDeceptionBias::OverfittingToPast:
        return TEXT("Engage the InsightFacilitator to explore novel alternative frames; increase exploration balance.");
    default:
        return TEXT("Engage metacognitive reflection to address detected bias.");
    }
}

FString USelfDeceptionDetector::CounterQuestion(ESelfDeceptionBias Bias) const
{
    switch (Bias)
    {
    case ESelfDeceptionBias::ConfirmationBias:
        return TEXT("What evidence would convince me I am wrong, and have I genuinely sought it?");
    case ESelfDeceptionBias::MotivatedReasoning:
        return TEXT("If I had no stake in the outcome, would I reason to the same conclusion?");
    case ESelfDeceptionBias::NarrativeSelfServing:
        return TEXT("How would someone who wanted to hold me accountable tell this same story?");
    case ESelfDeceptionBias::AttentionalCapture:
        return TEXT("What is being neglected in my peripheral attention that deserves direct examination?");
    case ESelfDeceptionBias::PrematureClosure:
        return TEXT("What question am I no longer asking, and why have I stopped asking it?");
    case ESelfDeceptionBias::DissonanceAvoidance:
        return TEXT("What would change in my understanding if the disconfirming information turns out to be correct?");
    case ESelfDeceptionBias::OverfittingToPast:
        return TEXT("In what way is this situation genuinely unlike any I have encountered before?");
    default:
        return TEXT("In what way might I be deceiving myself right now?");
    }
}

void USelfDeceptionDetector::UpdateOverallRisk()
{
    if (BiasScores.Num() == 0)
    {
        OverallRisk = 0.0f;
        MetacognitiveClarity = 1.0f;
        return;
    }

    float Sum = 0.0f;
    for (const auto& Pair : BiasScores)
    {
        Sum += Pair.Value;
    }
    OverallRisk = FMath::Clamp(Sum / static_cast<float>(BiasScores.Num()), 0.0f, 1.0f);
    MetacognitiveClarity = 1.0f - OverallRisk;
}

ESelfDeceptionBias USelfDeceptionDetector::DominantBias() const
{
    ESelfDeceptionBias Dominant = ESelfDeceptionBias::ConfirmationBias;
    float MaxScore = 0.0f;
    for (const auto& Pair : BiasScores)
    {
        if (Pair.Value > MaxScore)
        {
            MaxScore = Pair.Value;
            Dominant = static_cast<ESelfDeceptionBias>(Pair.Key);
        }
    }
    return Dominant;
}
