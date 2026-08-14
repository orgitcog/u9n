#include "MeaningIntegrator.h"

UMeaningIntegrator::UMeaningIntegrator()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.0f;
}

void UMeaningIntegrator::BeginPlay()
{
    Super::BeginPlay();

    // Initialise order states
    IntegrationState.Nomological.Order = EMeaningOrder::Nomological;
    IntegrationState.Normative.Order   = EMeaningOrder::Normative;
    IntegrationState.Narrative.Order   = EMeaningOrder::Narrative;

    // Start at partial coherence until real data is received
    IntegrationState.MeaningCoherence = 0.5f;
    IntegrationState.CoherenceLevel   = EMeaningCoherenceLevel::Partial;
    IntegrationState.DominantOrder    = EMeaningOrder::Narrative;
}

void UMeaningIntegrator::TickComponent(float DeltaTime, ELevelTick TickType,
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
        EvaluateIntegration();
    }
}

// ========================================
// UPDATE ORDERS
// ========================================

void UMeaningIntegrator::UpdateNomologicalOrder(float InternalCoherence, float ContentRichness)
{
    IntegrationState.Nomological.InternalCoherence =
        FMath::Clamp(InternalCoherence, 0.0f, 1.0f);
    IntegrationState.Nomological.ContentRichness =
        FMath::Clamp(ContentRichness, 0.0f, 1.0f);
}

void UMeaningIntegrator::UpdateNormativeOrder(float InternalCoherence, float ContentRichness)
{
    IntegrationState.Normative.InternalCoherence =
        FMath::Clamp(InternalCoherence, 0.0f, 1.0f);
    IntegrationState.Normative.ContentRichness =
        FMath::Clamp(ContentRichness, 0.0f, 1.0f);
}

void UMeaningIntegrator::UpdateNarrativeOrder(float InternalCoherence, float ContentRichness)
{
    IntegrationState.Narrative.InternalCoherence =
        FMath::Clamp(InternalCoherence, 0.0f, 1.0f);
    IntegrationState.Narrative.ContentRichness =
        FMath::Clamp(ContentRichness, 0.0f, 1.0f);
}

// ========================================
// EVALUATION
// ========================================

void UMeaningIntegrator::EvaluateIntegration()
{
    ComputeAlignments();
    DetectFragmentations();

    IntegrationState.MeaningCoherence = ComputeOverallCoherence();
    IntegrationState.CoherenceLevel   = ClassifyCoherence(IntegrationState.MeaningCoherence);
    IntegrationState.DominantOrder    = FindDominantOrder();
}

float UMeaningIntegrator::GetMeaningCoherence() const
{
    return IntegrationState.MeaningCoherence;
}

EMeaningCoherenceLevel UMeaningIntegrator::GetCoherenceLevel() const
{
    return IntegrationState.CoherenceLevel;
}

EMeaningOrder UMeaningIntegrator::GetDominantOrder() const
{
    return IntegrationState.DominantOrder;
}

FMeaningOrderState UMeaningIntegrator::GetOrderState(EMeaningOrder Order) const
{
    switch (Order)
    {
    case EMeaningOrder::Nomological: return IntegrationState.Nomological;
    case EMeaningOrder::Normative:   return IntegrationState.Normative;
    case EMeaningOrder::Narrative:   return IntegrationState.Narrative;
    default:                         return FMeaningOrderState{};
    }
}

bool UMeaningIntegrator::IsMeaningCrisisActive() const
{
    return IntegrationState.CoherenceLevel == EMeaningCoherenceLevel::Crisis;
}

ERepairStrategy UMeaningIntegrator::GetRecommendedRepairStrategy() const
{
    if (IntegrationState.Nomological.bFragmented)
    {
        return ERepairStrategy::DeepCausalInquiry;
    }
    if (IntegrationState.Normative.bFragmented)
    {
        return ERepairStrategy::ValueClarification;
    }
    if (IntegrationState.Narrative.bFragmented)
    {
        return ERepairStrategy::NarrativeReintegration;
    }
    if (IsMeaningCrisisActive())
    {
        return ERepairStrategy::TransformativeExperience;
    }
    return ERepairStrategy::DialogicalGrounding;
}

// ========================================
// PROTECTED HELPERS
// ========================================

void UMeaningIntegrator::ComputeAlignments()
{
    // Nomological ↔ Normative
    float NomNorm = ComputeOrderPairAlignment(
        IntegrationState.Nomological, IntegrationState.Normative);
    IntegrationState.Nomological.AlignmentWithOther1 = NomNorm;
    IntegrationState.Normative.AlignmentWithOther1   = NomNorm;

    // Nomological ↔ Narrative
    float NomNarr = ComputeOrderPairAlignment(
        IntegrationState.Nomological, IntegrationState.Narrative);
    IntegrationState.Nomological.AlignmentWithOther2 = NomNarr;
    IntegrationState.Narrative.AlignmentWithOther1   = NomNarr;

    // Normative ↔ Narrative
    float NormNarr = ComputeOrderPairAlignment(
        IntegrationState.Normative, IntegrationState.Narrative);
    IntegrationState.Normative.AlignmentWithOther2 = NormNarr;
    IntegrationState.Narrative.AlignmentWithOther2 = NormNarr;
}

float UMeaningIntegrator::ComputeOrderPairAlignment(
    const FMeaningOrderState& A, const FMeaningOrderState& B) const
{
    // Alignment is higher when both orders have similar coherence levels
    // and rich content — this is a proxy for genuine integration.
    const float CoherenceSimilarity = 1.0f - FMath::Abs(
        A.InternalCoherence - B.InternalCoherence);
    const float ContentProduct = A.ContentRichness * B.ContentRichness;
    return FMath::Clamp(CoherenceSimilarity * 0.5f + ContentProduct * 0.5f, 0.0f, 1.0f);
}

float UMeaningIntegrator::ComputeOverallCoherence() const
{
    // Average of all three pair alignments weighted by internal coherences
    const float NomNorm  = IntegrationState.Nomological.AlignmentWithOther1;
    const float NomNarr  = IntegrationState.Nomological.AlignmentWithOther2;
    const float NormNarr = IntegrationState.Normative.AlignmentWithOther2;

    const float PairAvg = (NomNorm + NomNarr + NormNarr) / 3.0f;

    const float InternalAvg = (
        IntegrationState.Nomological.InternalCoherence +
        IntegrationState.Normative.InternalCoherence   +
        IntegrationState.Narrative.InternalCoherence) / 3.0f;

    return FMath::Clamp(PairAvg * 0.6f + InternalAvg * 0.4f, 0.0f, 1.0f);
}

EMeaningCoherenceLevel UMeaningIntegrator::ClassifyCoherence(float Coherence) const
{
    if (Coherence >= IntegrationThreshold)  return EMeaningCoherenceLevel::Integrated;
    if (Coherence >= FragmentationThreshold) return EMeaningCoherenceLevel::Partial;
    if (Coherence >= CrisisThreshold)        return EMeaningCoherenceLevel::Fragmented;
    return EMeaningCoherenceLevel::Crisis;
}

EMeaningOrder UMeaningIntegrator::FindDominantOrder() const
{
    float BestScore = -1.0f;
    EMeaningOrder Best = EMeaningOrder::Narrative;

    auto Score = [](const FMeaningOrderState& S) {
        return S.InternalCoherence * 0.5f + S.ContentRichness * 0.5f;
    };

    if (Score(IntegrationState.Nomological) > BestScore)
    {
        BestScore = Score(IntegrationState.Nomological);
        Best = EMeaningOrder::Nomological;
    }
    if (Score(IntegrationState.Normative) > BestScore)
    {
        BestScore = Score(IntegrationState.Normative);
        Best = EMeaningOrder::Normative;
    }
    if (Score(IntegrationState.Narrative) > BestScore)
    {
        Best = EMeaningOrder::Narrative;
    }

    return Best;
}

void UMeaningIntegrator::DetectFragmentations()
{
    // Check each order for internal fragmentation
    IntegrationState.Nomological.bFragmented =
        IntegrationState.Nomological.InternalCoherence < FragmentationThreshold;
    IntegrationState.Normative.bFragmented =
        IntegrationState.Normative.InternalCoherence < FragmentationThreshold;
    IntegrationState.Narrative.bFragmented =
        IntegrationState.Narrative.InternalCoherence < FragmentationThreshold;

    // Check each pair for alignment fragmentation
    auto CheckPair = [&](EMeaningOrder A, EMeaningOrder B, float Alignment)
    {
        if (Alignment < FragmentationThreshold)
        {
            FFragmentationEvent Event;
            Event.OrderA = A;
            Event.OrderB = B;
            Event.MisalignmentDegree = 1.0f - Alignment;
            Event.RecommendedRepair  = RecommendRepair(A, B, Event.MisalignmentDegree);
            Event.Timestamp = SessionTime;
            FragmentationHistory.Add(Event);
        }
    };

    CheckPair(EMeaningOrder::Nomological, EMeaningOrder::Normative,
        IntegrationState.Nomological.AlignmentWithOther1);
    CheckPair(EMeaningOrder::Nomological, EMeaningOrder::Narrative,
        IntegrationState.Nomological.AlignmentWithOther2);
    CheckPair(EMeaningOrder::Normative,   EMeaningOrder::Narrative,
        IntegrationState.Normative.AlignmentWithOther2);
}

ERepairStrategy UMeaningIntegrator::RecommendRepair(
    EMeaningOrder A, EMeaningOrder B, float Misalignment) const
{
    // When nomological and normative are misaligned → value clarification
    if ((A == EMeaningOrder::Nomological && B == EMeaningOrder::Normative)
        || (A == EMeaningOrder::Normative && B == EMeaningOrder::Nomological))
    {
        return Misalignment > 0.6f
            ? ERepairStrategy::TransformativeExperience
            : ERepairStrategy::ValueClarification;
    }

    // When nomological and narrative are misaligned → causal inquiry
    if ((A == EMeaningOrder::Nomological && B == EMeaningOrder::Narrative)
        || (A == EMeaningOrder::Narrative && B == EMeaningOrder::Nomological))
    {
        return ERepairStrategy::DeepCausalInquiry;
    }

    // When normative and narrative are misaligned → narrative re-integration
    return Misalignment > 0.6f
        ? ERepairStrategy::DialogicalGrounding
        : ERepairStrategy::NarrativeReintegration;
}
