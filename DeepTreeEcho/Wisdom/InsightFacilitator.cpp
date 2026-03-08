#include "InsightFacilitator.h"

UInsightFacilitator::UInsightFacilitator()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.0f;
}

void UInsightFacilitator::BeginPlay()
{
    Super::BeginPlay();
    RecentErrors.Reserve(ErrorBufferSize);
    CurrentPhase = EInsightPhase::Monitoring;
}

void UInsightFacilitator::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    SessionTime += DeltaTime;

    if (!bAutoProcess)
    {
        return;
    }

    AccumulatedTime += DeltaTime;
    if (AccumulatedTime < ProcessingInterval)
    {
        return;
    }
    AccumulatedTime -= ProcessingInterval;

    switch (CurrentPhase)
    {
    case EInsightPhase::Monitoring:    ProcessMonitoringPhase();    break;
    case EInsightPhase::Impasse:       ProcessImpassePhase();       break;
    case EInsightPhase::FrameSearch:   ProcessFrameSearchPhase();   break;
    case EInsightPhase::GestaltShift:  ProcessGestaltShiftPhase();  break;
    case EInsightPhase::Integration:   ProcessIntegrationPhase();   break;
    }
}

// ========================================
// PUBLIC API
// ========================================

void UInsightFacilitator::RecordPredictionError(const FString& PredictionId,
    float ErrorMagnitude, const FString& Domain)
{
    FPredictionErrorRecord Record;
    Record.PredictionId = PredictionId;
    Record.ErrorMagnitude = FMath::Clamp(ErrorMagnitude, 0.0f, 1.0f);
    Record.Domain = Domain;
    Record.Timestamp = SessionTime;

    if (RecentErrors.Num() >= ErrorBufferSize)
    {
        RecentErrors.RemoveAt(0);
    }
    RecentErrors.Add(Record);

    UpdateRunningError(Record.ErrorMagnitude);
}

void UInsightFacilitator::UpdateSalienceCoherence(float Coherence)
{
    CurrentSalienceCoherence = FMath::Clamp(Coherence, 0.0f, 1.0f);
}

void UInsightFacilitator::SetMetacognitiveOpenness(float Openness)
{
    MetacognitiveOpenness = FMath::Clamp(Openness, 0.0f, 1.0f);
}

bool UInsightFacilitator::CheckForImpasse()
{
    return IsHighSurprise() && IsLowCoherence();
}

TArray<FAlternativeFrame> UInsightFacilitator::GenerateAlternativeFrames()
{
    TArray<FAlternativeFrame> Candidates;
    for (int32 i = 0; i < FrameCandidateCount; ++i)
    {
        Candidates.Add(BuildFrameCandidate(i));
    }
    return Candidates;
}

bool UInsightFacilitator::TriggerGestaltShift()
{
    if (!IsOpenToShift())
    {
        return false;
    }

    TArray<FAlternativeFrame> Candidates = GenerateAlternativeFrames();
    if (Candidates.Num() == 0)
    {
        return false;
    }

    BestFrameCandidate = SelectBestFrame(Candidates);
    CurrentPhase = EInsightPhase::GestaltShift;
    return true;
}

void UInsightFacilitator::IntegrateNewGestalt(const FString& NewFrameLabel, float CoherenceGain)
{
    RecordInsight(EInsightType::FrameShift, CurrentFrameLabel, NewFrameLabel, CoherenceGain);
    CurrentFrameLabel = NewFrameLabel;
    ImpasseCycleCount = 0;
    CurrentPhase = EInsightPhase::Integration;
}

bool UInsightFacilitator::IsHighSurprise() const
{
    return RunningPredictionError >= SurpriseThreshold;
}

bool UInsightFacilitator::IsLowCoherence() const
{
    return CurrentSalienceCoherence <= CoherenceThreshold;
}

bool UInsightFacilitator::IsOpenToShift() const
{
    return MetacognitiveOpenness >= MetacognitiveReadinessThreshold;
}

FInsightRecord UInsightFacilitator::GetLastInsight() const
{
    if (InsightHistory.Num() > 0)
    {
        return InsightHistory.Last();
    }
    return FInsightRecord{};
}

// ========================================
// PHASE PROCESSORS
// ========================================

void UInsightFacilitator::ProcessMonitoringPhase()
{
    if (CheckForImpasse())
    {
        CurrentPhase = EInsightPhase::Impasse;
        ImpasseCycleCount = 0;
    }
}

void UInsightFacilitator::ProcessImpassePhase()
{
    ++ImpasseCycleCount;

    if (IsOpenToShift())
    {
        CurrentPhase = EInsightPhase::FrameSearch;
        return;
    }

    if (ImpasseCycleCount >= MaxImpasseCycles)
    {
        // Force a random exploratory shift to escape prolonged impasse
        CurrentPhase = EInsightPhase::FrameSearch;
        MetacognitiveOpenness = FMath::Max(MetacognitiveOpenness,
            MetacognitiveReadinessThreshold + 0.05f);
    }
}

void UInsightFacilitator::ProcessFrameSearchPhase()
{
    TArray<FAlternativeFrame> Candidates = GenerateAlternativeFrames();
    if (Candidates.Num() > 0)
    {
        BestFrameCandidate = SelectBestFrame(Candidates);
        CurrentPhase = EInsightPhase::GestaltShift;
    }
}

void UInsightFacilitator::ProcessGestaltShiftPhase()
{
    // Adopt the best candidate frame
    const FString NewLabel = BestFrameCandidate.Label;
    const float Gain = BestFrameCandidate.ProjectedCoherence - CurrentSalienceCoherence;
    IntegrateNewGestalt(NewLabel, FMath::Max(0.0f, Gain));
}

void UInsightFacilitator::ProcessIntegrationPhase()
{
    // Allow one processing cycle for consolidation, then return to monitoring
    CurrentPhase = EInsightPhase::Monitoring;
}

// ========================================
// HELPERS
// ========================================

void UInsightFacilitator::UpdateRunningError(float NewError)
{
    // Exponential moving average for running prediction error
    const float Alpha = 0.15f;
    RunningPredictionError = Alpha * NewError + (1.0f - Alpha) * RunningPredictionError;
}

FAlternativeFrame UInsightFacilitator::BuildFrameCandidate(int32 Index) const
{
    // Generate plausible candidate frames by varying the key parameters
    // In a full implementation these would come from a knowledge base or
    // generative model; here we produce diverse synthetic alternatives.
    static const TArray<FString> FrameLabels = {
        TEXT("Relational"),
        TEXT("Causal"),
        TEXT("Functional"),
        TEXT("Narrative"),
        TEXT("Systemic"),
        TEXT("Embodied"),
        TEXT("Temporal"),
        TEXT("Evaluative")
    };

    FAlternativeFrame Frame;
    const int32 LabelIdx = Index % FrameLabels.Num();
    Frame.Label = FrameLabels[LabelIdx];
    Frame.Description = FString::Printf(
        TEXT("Reframe current situation through a %s lens"), *Frame.Label);

    // Projected coherence: higher for more novel frames during impasse
    Frame.Novelty = FMath::Clamp(0.3f + 0.1f * static_cast<float>(Index), 0.0f, 1.0f);
    Frame.Plausibility = FMath::Clamp(0.8f - 0.05f * static_cast<float>(Index), 0.2f, 1.0f);
    Frame.ProjectedCoherence = (Frame.Novelty * 0.4f + Frame.Plausibility * 0.6f)
        * (1.0f - CurrentSalienceCoherence * 0.5f);

    return Frame;
}

FAlternativeFrame UInsightFacilitator::SelectBestFrame(
    const TArray<FAlternativeFrame>& Candidates) const
{
    FAlternativeFrame Best = Candidates[0];
    for (const FAlternativeFrame& Candidate : Candidates)
    {
        const float Score = Candidate.ProjectedCoherence * 0.5f
            + Candidate.Plausibility * 0.3f
            + Candidate.Novelty * 0.2f;
        const float BestScore = Best.ProjectedCoherence * 0.5f
            + Best.Plausibility * 0.3f
            + Best.Novelty * 0.2f;
        if (Score > BestScore)
        {
            Best = Candidate;
        }
    }
    return Best;
}

void UInsightFacilitator::RecordInsight(EInsightType Type, const FString& Prev,
    const FString& Next, float Gain)
{
    FInsightRecord Record;
    Record.InsightType = Type;
    Record.PreviousFrame = Prev;
    Record.NewFrame = Next;
    Record.CoherenceGain = Gain;
    Record.OccurrenceTime = SessionTime;
    Record.ImpasseCycles = ImpasseCycleCount;

    InsightHistory.Add(Record);
    ++TotalInsightCount;
}
