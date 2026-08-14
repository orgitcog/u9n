// DeepTreeEchoUnrealIntegration.cpp
// Implementation of Deep Tree Echo Unreal Engine integration

#include "DeepTreeEchoUnrealIntegration.h"
#include "../Core/DeepTreeEchoCore.h"
#include "../Reservoir/DeepTreeEchoReservoir.h"
#include "../Avatar/AvatarEvolutionSystem.h"

UDeepTreeEchoUnrealIntegration::UDeepTreeEchoUnrealIntegration()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;

    // Initialize proprioceptive state (6 DOF)
    ProprioceptiveState.SetNum(6);
    for (int i = 0; i < 6; i++)
    {
        ProprioceptiveState[i] = 0.0f;
    }

    // Initialize interoceptive state (4 channels)
    InteroceptiveState.SetNum(4);
    for (int i = 0; i < 4; i++)
    {
        InteroceptiveState[i] = 0.5f;
    }

    // Initialize default avatar expression
    CurrentExpressionState.CognitiveMode = "Reactive";
    CurrentExpressionState.AuraColor = FLinearColor(0.3f, 0.5f, 0.8f, 1.0f);
    CurrentExpressionState.EyeGlowIntensity = 0.5f;
}

void UDeepTreeEchoUnrealIntegration::BeginPlay()
{
    Super::BeginPlay();

    // Find and cache component references
    AActor* Owner = GetOwner();
    if (Owner)
    {
        CognitiveCore = Owner->FindComponentByClass<UDeepTreeEchoCore>();
        ReservoirComponent = Owner->FindComponentByClass<UDeepTreeEchoReservoir>();
        AvatarSystem = Owner->FindComponentByClass<UAvatarEvolutionSystem>();
    }

    // Initialize consciousness streams
    InitializeConsciousnessStreams();
}

void UDeepTreeEchoUnrealIntegration::TickComponent(float DeltaTime, ELevelTick TickType, 
                                                    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Accumulate time for cognitive cycle
    CycleTimeAccumulator += DeltaTime;

    // Advance cognitive step when duration exceeded
    if (CycleTimeAccumulator >= StepDuration)
    {
        CycleTimeAccumulator -= StepDuration;
        AdvanceCognitiveStep();
    }

    // Update mutual awareness between streams
    UpdateMutualAwareness();

    // Update 4E state from sensors
    Update4EStateFromSensors();

    // Update relevance realization
    UpdateRelevanceRealization();

    // Update figure-ground dynamics
    UpdateFigureGround();

    // Update avatar expression
    UpdateAvatarExpression();
}

void UDeepTreeEchoUnrealIntegration::InitializeConsciousnessStreams()
{
    ConsciousnessStreams.Empty();

    // Create three streams phased 4 steps apart (120 degrees)
    for (int32 i = 0; i < 3; i++)
    {
        FConsciousnessStream Stream;
        Stream.StreamId = i + 1;
        Stream.PhaseOffset = i * 4; // 0, 4, 8
        Stream.CurrentStep = static_cast<EEchoBeatsStep>(Stream.PhaseOffset);
        Stream.ActivationLevel = 0.5f;

        // Initialize mutual awareness
        for (int32 j = 0; j < 3; j++)
        {
            if (j != i)
            {
                Stream.MutualAwareness.Add(j + 1, 0.5f);
            }
        }

        ConsciousnessStreams.Add(Stream);
    }
}

void UDeepTreeEchoUnrealIntegration::AdvanceCognitiveStep()
{
    // Advance global cycle step
    int32 PreviousStep = CurrentCycleStep;
    CurrentCycleStep = (CurrentCycleStep + 1) % 12;

    // Update each stream's current step
    for (FConsciousnessStream& Stream : ConsciousnessStreams)
    {
        int32 StreamStep = (CurrentCycleStep + Stream.PhaseOffset) % 12;
        Stream.CurrentStep = static_cast<EEchoBeatsStep>(StreamStep);
    }

    // Process the current cognitive step
    ProcessCognitiveStep(CurrentCycleStep);

    // Broadcast event
    OnCognitiveStepAdvanced.Broadcast(CurrentCycleStep);
}

void UDeepTreeEchoUnrealIntegration::ProcessCognitiveStep(int32 Step)
{
    // Determine which stream is primary for this step
    int32 PrimaryStream = GetStreamForStep(Step);

    // Update stream activation based on step type
    for (FConsciousnessStream& Stream : ConsciousnessStreams)
    {
        if (Stream.StreamId == PrimaryStream)
        {
            Stream.ActivationLevel = FMath::Lerp(Stream.ActivationLevel, 1.0f, 0.3f);
        }
        else
        {
            Stream.ActivationLevel = FMath::Lerp(Stream.ActivationLevel, 0.5f, 0.1f);
        }
    }

    // Step-specific processing
    switch (Step)
    {
        case 0: // Perceive
            // Gather sensory input
            break;
        case 1: // Orient
            // Update salience landscape
            AllocateAttention();
            break;
        case 2: // Reflect
            // Meta-cognitive processing
            break;
        case 3: // Integrate
            // Cross-modal synthesis
            break;
        case 4: // Decide
            // Action selection
            break;
        case 5: // Simulate
            // Predictive processing
            break;
        case 6: // Act
            // Execute action
            break;
        case 7: // Observe
            // Monitor action outcome
            break;
        case 8: // Learn
            // Update from experience
            AnalyzeForInsights();
            break;
        case 9: // Consolidate
            // Memory consolidation
            break;
        case 10: // Anticipate
            // Prepare for next cycle
            break;
        case 11: // Transcend
            // Meta-level processing
            CultivateWisdom();
            break;
    }
}

int32 UDeepTreeEchoUnrealIntegration::GetStreamForStep(int32 Step) const
{
    // Steps are grouped into triads: {1,5,9}, {2,6,10}, {3,7,11}, {4,8,12}
    // Each triad maps to a primary stream
    int32 TriadIndex = Step % 4;
    return (TriadIndex % 3) + 1;
}

void UDeepTreeEchoUnrealIntegration::UpdateMutualAwareness()
{
    // Each stream becomes aware of other streams' states
    for (int32 i = 0; i < ConsciousnessStreams.Num(); i++)
    {
        for (int32 j = 0; j < ConsciousnessStreams.Num(); j++)
        {
            if (i != j)
            {
                // Compute awareness based on activation levels and phase relationship
                float OtherActivation = ConsciousnessStreams[j].ActivationLevel;
                float PhaseDiff = FMath::Abs(ConsciousnessStreams[i].PhaseOffset - 
                                             ConsciousnessStreams[j].PhaseOffset);
                float AwarenessStrength = OtherActivation * (1.0f - PhaseDiff / 12.0f);

                ConsciousnessStreams[i].MutualAwareness.Add(j + 1, AwarenessStrength);
            }
        }
    }
}

int32 UDeepTreeEchoUnrealIntegration::GetCurrentNestingLevel() const
{
    // OEIS A000081 nesting structure
    // Steps 1-3: Level 1 (1 term)
    // Steps 4-5: Level 2 (2 terms)
    // Steps 6-9: Level 3 (4 terms)
    // Steps 10-12: Level 4 (9 terms)

    if (CurrentCycleStep < 3) return 1;
    if (CurrentCycleStep < 5) return 2;
    if (CurrentCycleStep < 9) return 3;
    return 4;
}

int32 UDeepTreeEchoUnrealIntegration::GetTermsAtNestingLevel(int32 Level) const
{
    // OEIS A000081 sequence
    switch (Level)
    {
        case 1: return 1;
        case 2: return 2;
        case 3: return 4;
        case 4: return 9;
        default: return 1;
    }
}

float UDeepTreeEchoUnrealIntegration::Compute4EIntegrationScore() const
{
    // Compute weighted average of 4E dimensions
    float EmbodiedScore = MotorReadiness;
    float EmbeddedScore = EnvironmentCoupling;
    float EnactedScore = EnactiveEngagement;
    float ExtendedScore = ExtensionIntegration;

    // Weight by relevance
    float TotalWeight = 0.0f;
    float WeightedSum = 0.0f;

    // Embodied weight based on somatic markers
    float EmbodiedWeight = 0.25f;
    for (const auto& Marker : SomaticMarkers)
    {
        EmbodiedWeight += Marker.Value * 0.1f;
    }
    WeightedSum += EmbodiedScore * EmbodiedWeight;
    TotalWeight += EmbodiedWeight;

    // Embedded weight based on affordances
    float EmbeddedWeight = 0.25f + DetectedAffordances.Num() * 0.05f;
    WeightedSum += EmbeddedScore * EmbeddedWeight;
    TotalWeight += EmbeddedWeight;

    // Enacted weight based on prediction errors
    float EnactedWeight = 0.25f;
    for (const auto& Error : PredictionErrors)
    {
        EnactedWeight += (1.0f - Error.Value) * 0.1f;
    }
    WeightedSum += EnactedScore * EnactedWeight;
    TotalWeight += EnactedWeight;

    // Extended weight based on active tools
    float ExtendedWeight = 0.25f + ActiveTools.Num() * 0.1f;
    WeightedSum += ExtendedScore * ExtendedWeight;
    TotalWeight += ExtendedWeight;

    return TotalWeight > 0.0f ? WeightedSum / TotalWeight : 0.5f;
}

void UDeepTreeEchoUnrealIntegration::Update4EStateFromSensors()
{
    // This would be connected to actual sensor systems in a full implementation
    // For now, simulate gradual changes

    // Update motor readiness based on recent actions
    MotorReadiness = FMath::Lerp(MotorReadiness, 0.5f, 0.01f);

    // Update environment coupling
    EnvironmentCoupling = FMath::Lerp(EnvironmentCoupling, 
                                       DetectedAffordances.Num() > 0 ? 0.8f : 0.3f, 0.05f);

    // Update enactive engagement
    EnactiveEngagement = FMath::Lerp(EnactiveEngagement, 
                                      ConsciousnessStreams[0].ActivationLevel, 0.1f);
}

void UDeepTreeEchoUnrealIntegration::AddDiaryEntry(const FDiaryEntry& NewEntry)
{
    DiaryEntries.Add(NewEntry);

    // Limit diary size to prevent memory bloat
    const int32 MaxEntries = 1000;
    if (DiaryEntries.Num() > MaxEntries)
    {
        // Remove oldest entries with lowest importance
        DiaryEntries.Sort([](const FDiaryEntry& A, const FDiaryEntry& B) {
            return A.ImportanceScore > B.ImportanceScore;
        });
        DiaryEntries.SetNum(MaxEntries);
    }
}

void UDeepTreeEchoUnrealIntegration::AnalyzeForInsights()
{
    // Require minimum entries for pattern detection
    if (DiaryEntries.Num() < 5)
    {
        return;
    }

    // Group entries by emotional valence
    TMap<FString, TArray<FDiaryEntry>> ValenceGroups;
    for (const FDiaryEntry& Entry : DiaryEntries)
    {
        if (!ValenceGroups.Contains(Entry.EmotionalValence))
        {
            ValenceGroups.Add(Entry.EmotionalValence, TArray<FDiaryEntry>());
        }
        ValenceGroups[Entry.EmotionalValence].Add(Entry);
    }

    // Look for patterns in groups with 3+ entries
    for (const auto& Group : ValenceGroups)
    {
        if (Group.Value.Num() >= 3)
        {
            FInsightEntry NewInsight = GenerateInsightFromPatterns(Group.Value);
            if (NewInsight.Confidence > 0.5f)
            {
                Insights.Add(NewInsight);
                OnInsightDiscovered.Broadcast(NewInsight);
            }
        }
    }
}

FInsightEntry UDeepTreeEchoUnrealIntegration::GenerateInsightFromPatterns(
    const TArray<FDiaryEntry>& RelevantEntries)
{
    FInsightEntry Insight;
    Insight.DiscoveryTime = FDateTime::Now();
    Insight.SupportingEntriesCount = RelevantEntries.Num();

    // Find common patterns
    TMap<FString, int32> EntityCounts;
    float TotalImportance = 0.0f;
    float Total4EScore = 0.0f;

    for (const FDiaryEntry& Entry : RelevantEntries)
    {
        for (const FString& Entity : Entry.KeyEntities)
        {
            if (!EntityCounts.Contains(Entity))
            {
                EntityCounts.Add(Entity, 0);
            }
            EntityCounts[Entity]++;
        }
        TotalImportance += Entry.ImportanceScore;
        Total4EScore += (Entry.EmbodiedRelevance + Entry.EmbeddedRelevance + 
                        Entry.EnactedRelevance + Entry.ExtendedRelevance) / 4.0f;
    }

    // Build insight text from common entities
    TArray<FString> CommonEntities;
    for (const auto& EntityCount : EntityCounts)
    {
        if (EntityCount.Value >= RelevantEntries.Num() / 2)
        {
            CommonEntities.Add(EntityCount.Key);
            Insight.SupportingPatterns.Add(EntityCount.Key);
        }
    }

    if (CommonEntities.Num() > 0)
    {
        Insight.InsightText = FString::Printf(TEXT("Pattern detected involving: %s"), 
                                               *FString::Join(CommonEntities, TEXT(", ")));
    }
    else
    {
        Insight.InsightText = TEXT("Recurring emotional pattern detected");
    }

    // Compute confidence and wisdom weight
    Insight.Confidence = FMath::Clamp(
        (float)RelevantEntries.Num() / 10.0f + TotalImportance / RelevantEntries.Num(), 
        0.0f, 1.0f);
    Insight.WisdomWeight = Total4EScore / RelevantEntries.Num();
    Insight.RelevanceScore = Insight.Confidence * Insight.WisdomWeight;

    return Insight;
}

void UDeepTreeEchoUnrealIntegration::CultivateWisdom()
{
    // Aggregate wisdom from insights
    float TotalWisdom = 0.0f;
    float TotalWeight = 0.0f;

    for (const FInsightEntry& Insight : Insights)
    {
        TotalWisdom += Insight.WisdomWeight * Insight.Confidence;
        TotalWeight += Insight.Confidence;
    }

    float NewWisdomScore = TotalWeight > 0.0f ? TotalWisdom / TotalWeight : 0.0f;

    // Apply gradual wisdom growth
    float PreviousWisdom = WisdomScore;
    WisdomScore = FMath::Lerp(WisdomScore, NewWisdomScore, 0.1f);

    // Broadcast if wisdom changed significantly
    if (FMath::Abs(WisdomScore - PreviousWisdom) > 0.01f)
    {
        OnWisdomChanged.Broadcast(WisdomScore);
    }
}

void UDeepTreeEchoUnrealIntegration::UpdateAvatarExpression()
{
    // Map cognitive state to avatar expression

    // Eye glow intensity from cognitive activation
    float AvgActivation = 0.0f;
    for (const FConsciousnessStream& Stream : ConsciousnessStreams)
    {
        AvgActivation += Stream.ActivationLevel;
    }
    AvgActivation /= ConsciousnessStreams.Num();
    CurrentExpressionState.EyeGlowIntensity = FMath::Lerp(
        CurrentExpressionState.EyeGlowIntensity, AvgActivation, 0.1f);

    // Aura color from emotional state
    CurrentExpressionState.AuraColor = ComputeAuraColor();

    // Aura pulse rate from cognitive cycle
    CurrentExpressionState.AuraPulseRate = 1.0f / StepDuration;

    // Breathing rate from interoceptive state
    if (InteroceptiveState.Num() > 0)
    {
        CurrentExpressionState.BreathingRate = 12.0f + (InteroceptiveState[0] - 0.5f) * 8.0f;
    }

    // Hair dynamics from motor readiness
    CurrentExpressionState.HairDynamicsMultiplier = 0.5f + MotorReadiness * 0.5f;

    // Cognitive mode from current step
    int32 NestingLevel = GetCurrentNestingLevel();
    switch (NestingLevel)
    {
        case 1: CurrentExpressionState.CognitiveMode = "Reactive"; break;
        case 2: CurrentExpressionState.CognitiveMode = "Deliberative"; break;
        case 3: CurrentExpressionState.CognitiveMode = "Reflective"; break;
        case 4: CurrentExpressionState.CognitiveMode = "Integrative"; break;
    }
}

void UDeepTreeEchoUnrealIntegration::SetEmotionIntensity(const FString& EmotionName, float Intensity)
{
    CurrentExpressionState.EmotionIntensities.Add(EmotionName, FMath::Clamp(Intensity, 0.0f, 1.0f));
}

void UDeepTreeEchoUnrealIntegration::BlendToExpression(
    const FAvatarExpressionState& TargetState, float BlendTime)
{
    // This would typically use a timeline or async task
    // For now, immediate blend
    CurrentExpressionState = TargetState;
}

FVector UDeepTreeEchoUnrealIntegration::GetEyeGazeDirection() const
{
    // Return normalized direction to attention focus
    AActor* Owner = GetOwner();
    if (Owner)
    {
        FVector OwnerLocation = Owner->GetActorLocation();
        FVector Direction = CurrentExpressionState.AttentionFocus - OwnerLocation;
        return Direction.GetSafeNormal();
    }
    return FVector::ForwardVector;
}

FLinearColor UDeepTreeEchoUnrealIntegration::ComputeAuraColor() const
{
    // Map emotional state to color
    float Hue = 0.6f; // Default blue
    float Saturation = 0.5f;
    float Value = 0.8f;

    // Adjust based on emotions
    if (CurrentExpressionState.EmotionIntensities.Contains("Joy"))
    {
        Hue = FMath::Lerp(Hue, 0.15f, CurrentExpressionState.EmotionIntensities["Joy"]);
        Value = FMath::Lerp(Value, 1.0f, CurrentExpressionState.EmotionIntensities["Joy"]);
    }
    if (CurrentExpressionState.EmotionIntensities.Contains("Anger"))
    {
        Hue = FMath::Lerp(Hue, 0.0f, CurrentExpressionState.EmotionIntensities["Anger"]);
        Saturation = FMath::Lerp(Saturation, 1.0f, CurrentExpressionState.EmotionIntensities["Anger"]);
    }
    if (CurrentExpressionState.EmotionIntensities.Contains("Sadness"))
    {
        Hue = FMath::Lerp(Hue, 0.65f, CurrentExpressionState.EmotionIntensities["Sadness"]);
        Saturation = FMath::Lerp(Saturation, 0.3f, CurrentExpressionState.EmotionIntensities["Sadness"]);
    }
    if (CurrentExpressionState.EmotionIntensities.Contains("Fear"))
    {
        Hue = FMath::Lerp(Hue, 0.8f, CurrentExpressionState.EmotionIntensities["Fear"]);
        Value = FMath::Lerp(Value, 0.5f, CurrentExpressionState.EmotionIntensities["Fear"]);
    }

    // Wisdom adds golden tint
    Hue = FMath::Lerp(Hue, 0.12f, WisdomScore * 0.3f);

    // Convert HSV to RGB
    return FLinearColor::MakeFromHSV8(
        (uint8)(Hue * 255.0f),
        (uint8)(Saturation * 255.0f),
        (uint8)(Value * 255.0f));
}

void UDeepTreeEchoUnrealIntegration::UpdateRelevanceRealization()
{
    // Update salience landscape based on current context
    for (auto& Salience : SalienceLandscape)
    {
        // Decay salience over time
        Salience.Value *= 0.99f;
    }

    // Boost salience for current figure
    if (SalienceLandscape.Contains(CurrentFigure))
    {
        SalienceLandscape[CurrentFigure] = FMath::Min(SalienceLandscape[CurrentFigure] + 0.1f, 1.0f);
    }

    // Update attention weights
    AllocateAttention();
}

float UDeepTreeEchoUnrealIntegration::ComputeSalience(const FString& EntityId) const
{
    if (SalienceLandscape.Contains(EntityId))
    {
        return SalienceLandscape[EntityId];
    }
    return 0.0f;
}

void UDeepTreeEchoUnrealIntegration::AllocateAttention()
{
    // Softmax-style attention allocation
    float TotalSalience = 0.0f;
    for (const auto& Salience : SalienceLandscape)
    {
        TotalSalience += FMath::Exp(Salience.Value);
    }

    AttentionWeights.Empty();
    for (const auto& Salience : SalienceLandscape)
    {
        float Weight = TotalSalience > 0.0f ? FMath::Exp(Salience.Value) / TotalSalience : 0.0f;
        AttentionWeights.Add(Salience.Key, Weight);
    }
}

void UDeepTreeEchoUnrealIntegration::UpdateFigureGround()
{
    // Find highest salience entity as figure
    FString HighestSalienceEntity;
    float HighestSalience = 0.0f;

    for (const auto& Salience : SalienceLandscape)
    {
        if (Salience.Value > HighestSalience)
        {
            HighestSalience = Salience.Value;
            HighestSalienceEntity = Salience.Key;
        }
    }

    // Check for gestalt shift
    if (!HighestSalienceEntity.IsEmpty() && HighestSalienceEntity != CurrentFigure)
    {
        AttemptGestaltShift(HighestSalienceEntity);
    }

    // Update ground (everything except figure)
    CurrentGround.Empty();
    for (const auto& Salience : SalienceLandscape)
    {
        if (Salience.Key != CurrentFigure)
        {
            CurrentGround.Add(Salience.Key);
        }
    }

    // Compute gestalt coherence
    GestaltCoherence = HighestSalience > 0.0f ? 
        HighestSalience / (HighestSalience + 0.5f) : 0.0f;
}

bool UDeepTreeEchoUnrealIntegration::AttemptGestaltShift(const FString& NewFigure)
{
    // Check if shift is warranted
    float CurrentFigureSalience = ComputeSalience(CurrentFigure);
    float NewFigureSalience = ComputeSalience(NewFigure);

    // Require significant salience difference for shift
    if (NewFigureSalience > CurrentFigureSalience + 0.2f)
    {
        FString OldFigure = CurrentFigure;
        CurrentFigure = NewFigure;
        OnGestaltShift.Broadcast(OldFigure, NewFigure);
        return true;
    }

    return false;
}
