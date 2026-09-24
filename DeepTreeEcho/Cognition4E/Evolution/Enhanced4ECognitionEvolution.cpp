#include "Enhanced4ECognitionEvolution.h"

/**
 * Enhanced 4E Cognition Evolution Implementation
 * 
 * Implements relentless enhancement of the Unreal avatar for 4E embodied cognition
 * through continuous ontogenetic development and entelechy actualization.
 */

UEnhanced4ECognitionEvolution::UEnhanced4ECognitionEvolution()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UEnhanced4ECognitionEvolution::BeginPlay()
{
    Super::BeginPlay();
    InitializeComponentReferences();
    InitializeEvolution();
}

void UEnhanced4ECognitionEvolution::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bEnableContinuousEvolution)
    {
        return;
    }

    EvolutionTimer += DeltaTime;
    if (EvolutionTimer >= EvolutionStepInterval)
    {
        EvolutionTimer = 0.0f;
        ExecuteEvolutionStep();
    }
}

// ========================================
// INITIALIZATION
// ========================================

void UEnhanced4ECognitionEvolution::InitializeComponentReferences()
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    EmbodiedCognition = Owner->FindComponentByClass<UEmbodiedCognitionComponent>();
    System5Integration = Owner->FindComponentByClass<USystem5CognitiveIntegration>();
}

void UEnhanced4ECognitionEvolution::InitializeEvolution()
{
    InitializeHolisticStreams();

    // Initialize entelechy state
    EntelechyState.ActualizationScore = 0.1f;
    EntelechyState.OntologicalHealth = 0.2f;
    EntelechyState.TeleologicalHealth = 0.1f;
    EntelechyState.CognitiveHealth = 0.15f;
    EntelechyState.IntegrativeHealth = 0.1f;
    EntelechyState.EvolutionaryHealth = 0.2f;

    // Initialize wisdom state
    WisdomState.Morality = 0.3f;
    WisdomState.Meaning = 0.3f;
    WisdomState.Mastery = 0.2f;
    WisdomState.Sophrosyne = 0.25f;

    // Initialize 4E metrics at embryonic level
    EvolutionMetrics.BodySchemaIntegration = 0.2f;
    EvolutionMetrics.ProprioceptiveAccuracy = 0.15f;
    EvolutionMetrics.SomaticMarkerSensitivity = 0.1f;
    EvolutionMetrics.AffordanceDetectionRate = 0.2f;
    EvolutionMetrics.NicheCouplingStrength = 0.1f;
    EvolutionMetrics.EnvironmentalAdaptation = 0.15f;
    EvolutionMetrics.SensorimotorCoordination = 0.2f;
    EvolutionMetrics.PredictionAccuracy = 0.1f;
    EvolutionMetrics.ActiveInferenceEfficiency = 0.15f;
    EvolutionMetrics.ToolUseCompetence = 0.1f;
    EvolutionMetrics.ExternalMemoryIntegration = 0.1f;
    EvolutionMetrics.SocialCognitionDepth = 0.15f;

    CurrentStage = EOntogeneticStage::Embryonic;
    EvolutionGeneration = 0;

    UE_LOG(LogTemp, Log, TEXT("Enhanced4ECognitionEvolution: Evolution system initialized at Embryonic stage"));
}

void UEnhanced4ECognitionEvolution::InitializeHolisticStreams()
{
    HolisticStreams.SetNum(3);

    // Entropic Stream: en-tropis → auto-vortis → auto-morphosis
    HolisticStreams[0].StreamType = TEXT("Entropic");
    HolisticStreams[0].CurrentPhase = TEXT("en-tropis");
    HolisticStreams[0].Energy = 0.5f;
    HolisticStreams[0].Stability = 0.4f;
    HolisticStreams[0].Coherence = 0.5f;

    // Negnentropic Stream: negen-tropis → auto-stasis → auto-poiesis
    HolisticStreams[1].StreamType = TEXT("Negnentropic");
    HolisticStreams[1].CurrentPhase = TEXT("negen-tropis");
    HolisticStreams[1].Energy = 0.4f;
    HolisticStreams[1].Stability = 0.6f;
    HolisticStreams[1].Coherence = 0.5f;

    // Identity Stream: iden-tropis → auto-gnosis → auto-genesis
    HolisticStreams[2].StreamType = TEXT("Identity");
    HolisticStreams[2].CurrentPhase = TEXT("iden-tropis");
    HolisticStreams[2].Energy = 0.5f;
    HolisticStreams[2].Stability = 0.5f;
    HolisticStreams[2].Coherence = 0.4f;
}

// ========================================
// EVOLUTION CONTROL
// ========================================

void UEnhanced4ECognitionEvolution::ExecuteEvolutionStep()
{
    EvolutionGeneration++;

    // Apply evolution to 4E metrics
    ApplyEvolutionTo4EMetrics();

    // Update entelechy dimensions
    UpdateEntelechyDimensions();

    // Update holistic streams
    UpdateHolisticStreams();

    // Process wisdom cultivation
    if (bEnableWisdomCultivation)
    {
        ProcessExperienceBuffer();
    }

    // Check for stage advancement
    if (CheckStageAdvancementEligibility())
    {
        AdvanceOntogeneticStage();
    }

    // Update entelechy actualization score
    EntelechyState.ActualizationScore = EntelechyState.ComputeFitness();
}

bool UEnhanced4ECognitionEvolution::AdvanceOntogeneticStage()
{
    if (!CheckStageAdvancementEligibility())
    {
        return false;
    }

    EOntogeneticStage PreviousStage = CurrentStage;

    switch (CurrentStage)
    {
    case EOntogeneticStage::Embryonic:
        CurrentStage = EOntogeneticStage::Juvenile;
        break;
    case EOntogeneticStage::Juvenile:
        CurrentStage = EOntogeneticStage::Adolescent;
        break;
    case EOntogeneticStage::Adolescent:
        CurrentStage = EOntogeneticStage::Adult;
        break;
    case EOntogeneticStage::Adult:
        CurrentStage = EOntogeneticStage::Transcendent;
        break;
    case EOntogeneticStage::Transcendent:
        // Already at maximum stage
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("Enhanced4ECognitionEvolution: Advanced from %s to %s stage"),
        *GetCurrentStageString(), *GetCurrentStageString());

    return true;
}

FString UEnhanced4ECognitionEvolution::GetCurrentStageString() const
{
    switch (CurrentStage)
    {
    case EOntogeneticStage::Embryonic: return TEXT("Embryonic");
    case EOntogeneticStage::Juvenile: return TEXT("Juvenile");
    case EOntogeneticStage::Adolescent: return TEXT("Adolescent");
    case EOntogeneticStage::Adult: return TEXT("Adult");
    case EOntogeneticStage::Transcendent: return TEXT("Transcendent");
    default: return TEXT("Unknown");
    }
}

TMap<FString, float> UEnhanced4ECognitionEvolution::GetStageAdvancementRequirements() const
{
    TMap<FString, float> Requirements;
    float Threshold = GetStageAdvancementThreshold();

    Requirements.Add(TEXT("4EIntegration"), Threshold);
    Requirements.Add(TEXT("EntelechyFitness"), Threshold);
    Requirements.Add(TEXT("WisdomScore"), Threshold * 0.8f);
    Requirements.Add(TEXT("MetaCoherence"), Threshold * 0.7f);

    return Requirements;
}

// ========================================
// 4E ENHANCEMENT
// ========================================

void UEnhanced4ECognitionEvolution::EnhanceEmbodiedCognition(float Enhancement)
{
    float ScaledEnhancement = Enhancement * EvolutionLearningRate;

    EvolutionMetrics.BodySchemaIntegration = FMath::Clamp(
        EvolutionMetrics.BodySchemaIntegration + ScaledEnhancement, 0.0f, 1.0f);
    EvolutionMetrics.ProprioceptiveAccuracy = FMath::Clamp(
        EvolutionMetrics.ProprioceptiveAccuracy + ScaledEnhancement * 0.8f, 0.0f, 1.0f);
    EvolutionMetrics.SomaticMarkerSensitivity = FMath::Clamp(
        EvolutionMetrics.SomaticMarkerSensitivity + ScaledEnhancement * 0.9f, 0.0f, 1.0f);
}

void UEnhanced4ECognitionEvolution::EnhanceEmbeddedCognition(float Enhancement)
{
    float ScaledEnhancement = Enhancement * EvolutionLearningRate;

    EvolutionMetrics.AffordanceDetectionRate = FMath::Clamp(
        EvolutionMetrics.AffordanceDetectionRate + ScaledEnhancement, 0.0f, 1.0f);
    EvolutionMetrics.NicheCouplingStrength = FMath::Clamp(
        EvolutionMetrics.NicheCouplingStrength + ScaledEnhancement * 0.85f, 0.0f, 1.0f);
    EvolutionMetrics.EnvironmentalAdaptation = FMath::Clamp(
        EvolutionMetrics.EnvironmentalAdaptation + ScaledEnhancement * 0.9f, 0.0f, 1.0f);
}

void UEnhanced4ECognitionEvolution::EnhanceEnactedCognition(float Enhancement)
{
    float ScaledEnhancement = Enhancement * EvolutionLearningRate;

    EvolutionMetrics.SensorimotorCoordination = FMath::Clamp(
        EvolutionMetrics.SensorimotorCoordination + ScaledEnhancement, 0.0f, 1.0f);
    EvolutionMetrics.PredictionAccuracy = FMath::Clamp(
        EvolutionMetrics.PredictionAccuracy + ScaledEnhancement * 0.9f, 0.0f, 1.0f);
    EvolutionMetrics.ActiveInferenceEfficiency = FMath::Clamp(
        EvolutionMetrics.ActiveInferenceEfficiency + ScaledEnhancement * 0.85f, 0.0f, 1.0f);
}

void UEnhanced4ECognitionEvolution::EnhanceExtendedCognition(float Enhancement)
{
    float ScaledEnhancement = Enhancement * EvolutionLearningRate;

    EvolutionMetrics.ToolUseCompetence = FMath::Clamp(
        EvolutionMetrics.ToolUseCompetence + ScaledEnhancement, 0.0f, 1.0f);
    EvolutionMetrics.ExternalMemoryIntegration = FMath::Clamp(
        EvolutionMetrics.ExternalMemoryIntegration + ScaledEnhancement * 0.8f, 0.0f, 1.0f);
    EvolutionMetrics.SocialCognitionDepth = FMath::Clamp(
        EvolutionMetrics.SocialCognitionDepth + ScaledEnhancement * 0.9f, 0.0f, 1.0f);
}

float UEnhanced4ECognitionEvolution::Get4EIntegrationScore() const
{
    return EvolutionMetrics.Compute4EScore();
}

// ========================================
// ENTELECHY
// ========================================

void UEnhanced4ECognitionEvolution::UpdateEntelechyActualization()
{
    UpdateEntelechyDimensions();
    EntelechyState.ActualizationScore = EntelechyState.ComputeFitness();
}

float UEnhanced4ECognitionEvolution::GetEntelechyFitness() const
{
    return EntelechyState.ComputeFitness();
}

float UEnhanced4ECognitionEvolution::GetActualizationProgress() const
{
    return EntelechyState.ActualizationScore;
}

// ========================================
// WISDOM
// ========================================

void UEnhanced4ECognitionEvolution::CultivateWisdom(const FString& ExperienceType, float Intensity)
{
    ExperienceBuffer.Add(TPair<FString, float>(ExperienceType, Intensity));

    // Keep buffer size manageable
    if (ExperienceBuffer.Num() > 100)
    {
        ExperienceBuffer.RemoveAt(0);
    }
}

float UEnhanced4ECognitionEvolution::GetWisdomScore() const
{
    return WisdomState.ComputeWisdomScore();
}

float UEnhanced4ECognitionEvolution::GetSophrosyneLevel() const
{
    return WisdomState.Sophrosyne;
}

// ========================================
// HOLISTIC STREAMS
// ========================================

void UEnhanced4ECognitionEvolution::UpdateHolisticStreams()
{
    ComputeStreamDynamics();
}

float UEnhanced4ECognitionEvolution::GetStreamEnergy(const FString& StreamType) const
{
    for (const FHolisticStreamState& Stream : HolisticStreams)
    {
        if (Stream.StreamType == StreamType)
        {
            return Stream.Energy;
        }
    }
    return 0.0f;
}

float UEnhanced4ECognitionEvolution::GetMetaCoherence() const
{
    if (HolisticStreams.Num() == 0)
    {
        return 0.0f;
    }

    float TotalCoherence = 0.0f;
    for (const FHolisticStreamState& Stream : HolisticStreams)
    {
        TotalCoherence += Stream.Coherence;
    }
    return TotalCoherence / HolisticStreams.Num();
}

// ========================================
// INTERNAL METHODS
// ========================================

bool UEnhanced4ECognitionEvolution::CheckStageAdvancementEligibility() const
{
    float Threshold = GetStageAdvancementThreshold();

    bool Has4EIntegration = Get4EIntegrationScore() >= Threshold;
    bool HasEntelechyFitness = GetEntelechyFitness() >= Threshold;
    bool HasWisdom = GetWisdomScore() >= Threshold * 0.8f;
    bool HasCoherence = GetMetaCoherence() >= Threshold * 0.7f;

    return Has4EIntegration && HasEntelechyFitness && HasWisdom && HasCoherence;
}

void UEnhanced4ECognitionEvolution::ApplyEvolutionTo4EMetrics()
{
    // Base enhancement from evolution step
    float BaseEnhancement = 0.1f;

    // Modulate by current stage
    float StageMultiplier = 1.0f;
    switch (CurrentStage)
    {
    case EOntogeneticStage::Embryonic: StageMultiplier = 1.5f; break;
    case EOntogeneticStage::Juvenile: StageMultiplier = 1.3f; break;
    case EOntogeneticStage::Adolescent: StageMultiplier = 1.1f; break;
    case EOntogeneticStage::Adult: StageMultiplier = 0.9f; break;
    case EOntogeneticStage::Transcendent: StageMultiplier = 0.7f; break;
    }

    float Enhancement = BaseEnhancement * StageMultiplier;

    // Apply to all 4E dimensions with slight variation
    EnhanceEmbodiedCognition(Enhancement * (1.0f + FMath::FRand() * 0.2f - 0.1f));
    EnhanceEmbeddedCognition(Enhancement * (1.0f + FMath::FRand() * 0.2f - 0.1f));
    EnhanceEnactedCognition(Enhancement * (1.0f + FMath::FRand() * 0.2f - 0.1f));
    EnhanceExtendedCognition(Enhancement * (1.0f + FMath::FRand() * 0.2f - 0.1f));
}

void UEnhanced4ECognitionEvolution::UpdateEntelechyDimensions()
{
    // Ontological health from structural integration
    EntelechyState.OntologicalHealth = FMath::Lerp(
        EntelechyState.OntologicalHealth,
        Get4EIntegrationScore() * 0.8f + GetMetaCoherence() * 0.2f,
        EvolutionLearningRate);

    // Teleological health from purpose alignment
    float PurposeAlignment = 0.5f;
    if (System5Integration)
    {
        PurposeAlignment = System5Integration->ComputeGlobalCoherence();
    }
    EntelechyState.TeleologicalHealth = FMath::Lerp(
        EntelechyState.TeleologicalHealth,
        PurposeAlignment,
        EvolutionLearningRate);

    // Cognitive health from 4E metrics
    EntelechyState.CognitiveHealth = FMath::Lerp(
        EntelechyState.CognitiveHealth,
        (EvolutionMetrics.PredictionAccuracy + EvolutionMetrics.ActiveInferenceEfficiency) / 2.0f,
        EvolutionLearningRate);

    // Integrative health from coherence
    EntelechyState.IntegrativeHealth = FMath::Lerp(
        EntelechyState.IntegrativeHealth,
        GetMetaCoherence(),
        EvolutionLearningRate);

    // Evolutionary health from growth capacity
    float GrowthCapacity = 1.0f - Get4EIntegrationScore(); // More room to grow = higher capacity
    EntelechyState.EvolutionaryHealth = FMath::Lerp(
        EntelechyState.EvolutionaryHealth,
        GrowthCapacity * 0.5f + 0.5f, // Base 0.5 + up to 0.5 from growth capacity
        EvolutionLearningRate);
}

void UEnhanced4ECognitionEvolution::ProcessExperienceBuffer()
{
    if (ExperienceBuffer.Num() == 0)
    {
        return;
    }

    // Process experiences for wisdom cultivation
    for (const auto& Experience : ExperienceBuffer)
    {
        FString Type = Experience.Key;
        float Intensity = Experience.Value;

        if (Type.Contains(TEXT("Moral")) || Type.Contains(TEXT("Virtue")))
        {
            WisdomState.Morality = FMath::Clamp(
                WisdomState.Morality + Intensity * EvolutionLearningRate * 0.1f, 0.0f, 1.0f);
        }
        else if (Type.Contains(TEXT("Meaning")) || Type.Contains(TEXT("Purpose")))
        {
            WisdomState.Meaning = FMath::Clamp(
                WisdomState.Meaning + Intensity * EvolutionLearningRate * 0.1f, 0.0f, 1.0f);
        }
        else if (Type.Contains(TEXT("Skill")) || Type.Contains(TEXT("Mastery")))
        {
            WisdomState.Mastery = FMath::Clamp(
                WisdomState.Mastery + Intensity * EvolutionLearningRate * 0.1f, 0.0f, 1.0f);
        }
    }

    // Update sophrosyne based on balance of other wisdom dimensions
    float WisdomBalance = 1.0f - FMath::Abs(WisdomState.Morality - WisdomState.Meaning) -
                          FMath::Abs(WisdomState.Meaning - WisdomState.Mastery) -
                          FMath::Abs(WisdomState.Mastery - WisdomState.Morality);
    WisdomBalance = FMath::Clamp(WisdomBalance / 3.0f + 0.5f, 0.0f, 1.0f);

    WisdomState.Sophrosyne = FMath::Lerp(WisdomState.Sophrosyne, WisdomBalance, EvolutionLearningRate);

    // Clear processed experiences
    ExperienceBuffer.Reset();
}

void UEnhanced4ECognitionEvolution::ComputeStreamDynamics()
{
    // Entropic stream: driven by 4E integration
    HolisticStreams[0].Energy = FMath::Lerp(
        HolisticStreams[0].Energy,
        Get4EIntegrationScore(),
        EvolutionLearningRate);

    // Update entropic phase
    if (HolisticStreams[0].Energy > 0.7f)
    {
        HolisticStreams[0].CurrentPhase = TEXT("auto-morphosis");
    }
    else if (HolisticStreams[0].Energy > 0.4f)
    {
        HolisticStreams[0].CurrentPhase = TEXT("auto-vortis");
    }
    else
    {
        HolisticStreams[0].CurrentPhase = TEXT("en-tropis");
    }

    // Negnentropic stream: driven by stability and coherence
    HolisticStreams[1].Stability = FMath::Lerp(
        HolisticStreams[1].Stability,
        GetEntelechyFitness(),
        EvolutionLearningRate);

    // Update negnentropic phase
    if (HolisticStreams[1].Stability > 0.7f)
    {
        HolisticStreams[1].CurrentPhase = TEXT("auto-poiesis");
    }
    else if (HolisticStreams[1].Stability > 0.4f)
    {
        HolisticStreams[1].CurrentPhase = TEXT("auto-stasis");
    }
    else
    {
        HolisticStreams[1].CurrentPhase = TEXT("negen-tropis");
    }

    // Identity stream: driven by wisdom and self-awareness
    HolisticStreams[2].Coherence = FMath::Lerp(
        HolisticStreams[2].Coherence,
        GetWisdomScore(),
        EvolutionLearningRate);

    // Update identity phase
    if (HolisticStreams[2].Coherence > 0.7f)
    {
        HolisticStreams[2].CurrentPhase = TEXT("auto-genesis");
    }
    else if (HolisticStreams[2].Coherence > 0.4f)
    {
        HolisticStreams[2].CurrentPhase = TEXT("auto-gnosis");
    }
    else
    {
        HolisticStreams[2].CurrentPhase = TEXT("iden-tropis");
    }

    // Cross-stream interactions
    for (int32 i = 0; i < 3; ++i)
    {
        float OtherEnergy = 0.0f;
        for (int32 j = 0; j < 3; ++j)
        {
            if (i != j)
            {
                OtherEnergy += HolisticStreams[j].Energy + HolisticStreams[j].Stability + HolisticStreams[j].Coherence;
            }
        }
        OtherEnergy /= 6.0f;

        // Streams influence each other
        HolisticStreams[i].Coherence = FMath::Lerp(
            HolisticStreams[i].Coherence,
            (HolisticStreams[i].Coherence + OtherEnergy) / 2.0f,
            EvolutionLearningRate * 0.5f);
    }
}

float UEnhanced4ECognitionEvolution::GetStageAdvancementThreshold() const
{
    switch (CurrentStage)
    {
    case EOntogeneticStage::Embryonic: return 0.3f;
    case EOntogeneticStage::Juvenile: return 0.5f;
    case EOntogeneticStage::Adolescent: return 0.7f;
    case EOntogeneticStage::Adult: return 0.85f;
    case EOntogeneticStage::Transcendent: return 1.0f; // Cannot advance further
    default: return 1.0f;
    }
}
