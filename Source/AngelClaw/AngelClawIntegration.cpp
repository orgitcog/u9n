// AngelClawIntegration.cpp
// Deep Tree Echo - AngelClaw Cognitive Core Integration
// Copyright (c) 2026 Deep Tree Echo Project

#include "AngelClawIntegration.h"

UAngelClawIntegration::UAngelClawIntegration()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.0f; // Tick every frame, internal accumulator controls cycle rate
}

void UAngelClawIntegration::BeginPlay()
{
    Super::BeginPlay();
    InitializeConsciousnessStreams();
    CurrentCycleStep = 1;
    CurrentMode = EAngelClawCognitiveMode::Reactive;
    WisdomLevel = 0.0f;
}

void UAngelClawIntegration::TickComponent(float DeltaTime, ELevelTick TickType,
                                           FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    CycleTimeAccumulator += DeltaTime;
    if (CycleTimeAccumulator >= CycleTickRate)
    {
        CycleTimeAccumulator -= CycleTickRate;
        AdvanceCognitiveCycle();
    }
}

// =============================================================================
// Cognitive Cycle
// =============================================================================

void UAngelClawIntegration::AdvanceCognitiveCycle()
{
    // Advance each consciousness stream according to its phase offset
    if (bEnableConcurrentStreams)
    {
        for (int32 i = 0; i < ConsciousnessStreams.Num(); ++i)
        {
            ProcessStreamStep(i);
        }
    }
    else
    {
        // Single-stream mode: only process stream 0
        if (ConsciousnessStreams.Num() > 0)
        {
            ProcessStreamStep(0);
        }
    }

    // Check for triadic synchronization
    if (IsTriadicSyncPoint())
    {
        SynchronizeStreams();
    }

    // Advance the global cycle step (1-12, wrapping)
    CurrentCycleStep = (CurrentCycleStep % 12) + 1;

    // Cultivate wisdom through reflective steps
    if (CurrentCycleStep % 3 == 0) // Steps 3, 6, 9, 12 are reflective
    {
        CultivateWisdom(0.001f);
    }
}

void UAngelClawIntegration::SetCognitiveMode(EAngelClawCognitiveMode NewMode)
{
    CurrentMode = NewMode;
}

// =============================================================================
// 4E Cognition
// =============================================================================

void UAngelClawIntegration::UpdateEmbodiedState(const TArray<float>& ProprioceptiveInput,
                                                  const TArray<float>& InteroceptiveInput)
{
    State4E.ProprioceptiveState = ProprioceptiveInput;
    State4E.InteroceptiveState = InteroceptiveInput;

    // Compute motor readiness from proprioceptive magnitude
    float PropSum = 0.0f;
    for (float Val : ProprioceptiveInput)
    {
        PropSum += FMath::Abs(Val);
    }
    State4E.MotorReadiness = FMath::Clamp(
        PropSum / FMath::Max(1.0f, static_cast<float>(ProprioceptiveInput.Num())), 0.0f, 1.0f);
}

void UAngelClawIntegration::RegisterAffordance(const FString& AffordanceID, float Strength)
{
    if (!State4E.ActiveAffordances.Contains(AffordanceID))
    {
        State4E.ActiveAffordances.Add(AffordanceID);
    }
    State4E.NicheCouplingStrength = FMath::Clamp(
        State4E.NicheCouplingStrength + Strength * 0.1f, 0.0f, 1.0f);
}

// =============================================================================
// Emotion
// =============================================================================

void UAngelClawIntegration::ProcessEmotionalStimulus(float ValenceShift, float ArousalShift,
                                                      const FString& Context)
{
    // Apply stimulus with stability-modulated dampening
    float Dampening = EmotionState.Stability;
    EmotionState.Valence = FMath::Clamp(
        EmotionState.Valence + ValenceShift * (1.0f - Dampening * 0.5f), -1.0f, 1.0f);
    EmotionState.Arousal = FMath::Clamp(
        EmotionState.Arousal + ArousalShift * (1.0f - Dampening * 0.3f), 0.0f, 1.0f);

    // Derive primary emotion from valence-arousal quadrant
    if (EmotionState.Valence > 0.3f && EmotionState.Arousal > 0.5f)
    {
        EmotionState.PrimaryEmotion = TEXT("Joy");
    }
    else if (EmotionState.Valence > 0.3f && EmotionState.Arousal <= 0.5f)
    {
        EmotionState.PrimaryEmotion = TEXT("Contentment");
    }
    else if (EmotionState.Valence < -0.3f && EmotionState.Arousal > 0.5f)
    {
        EmotionState.PrimaryEmotion = TEXT("Anger");
    }
    else if (EmotionState.Valence < -0.3f && EmotionState.Arousal <= 0.5f)
    {
        EmotionState.PrimaryEmotion = TEXT("Sadness");
    }
    else
    {
        EmotionState.PrimaryEmotion = TEXT("Neutral");
    }
}

// =============================================================================
// Consciousness Streams
// =============================================================================

void UAngelClawIntegration::InitializeConsciousnessStreams()
{
    ConsciousnessStreams.Empty();

    // 3 streams phased 120 degrees apart (4 steps offset in 12-step cycle)
    for (int32 i = 0; i < 3; ++i)
    {
        FConsciousnessStreamState Stream;
        Stream.StreamID = i;
        Stream.CurrentStep = 1 + (i * 4); // Phase offsets: 1, 5, 9
        Stream.ActivationLevel = 0.5f;
        Stream.CurrentPhase = StepToPhase(Stream.CurrentStep);
        ConsciousnessStreams.Add(Stream);
    }
}

FConsciousnessStreamState UAngelClawIntegration::GetStreamState(int32 StreamID) const
{
    if (StreamID >= 0 && StreamID < ConsciousnessStreams.Num())
    {
        return ConsciousnessStreams[StreamID];
    }
    return FConsciousnessStreamState();
}

void UAngelClawIntegration::ProcessStreamStep(int32 StreamIndex)
{
    if (StreamIndex < 0 || StreamIndex >= ConsciousnessStreams.Num())
    {
        return;
    }

    FConsciousnessStreamState& Stream = ConsciousnessStreams[StreamIndex];

    // Update phase label
    Stream.CurrentPhase = StepToPhase(Stream.CurrentStep);

    // Modulate activation based on cognitive mode
    float ModeModulation = 1.0f;
    switch (CurrentMode)
    {
    case EAngelClawCognitiveMode::Creative:
        ModeModulation = 1.2f; // Heightened activation in creative mode
        break;
    case EAngelClawCognitiveMode::Reflective:
        ModeModulation = 0.8f; // Dampened for deeper introspection
        break;
    default:
        break;
    }

    Stream.ActivationLevel = FMath::Clamp(
        Stream.ActivationLevel * ModeModulation, 0.0f, 1.0f);

    // Advance stream step (1-12, wrapping)
    Stream.CurrentStep = (Stream.CurrentStep % 12) + 1;
}

void UAngelClawIntegration::SynchronizeStreams()
{
    if (ConsciousnessStreams.Num() < 3)
    {
        return;
    }

    // At triadic sync points, cross-pollinate activation levels
    float MeanActivation = 0.0f;
    for (const FConsciousnessStreamState& Stream : ConsciousnessStreams)
    {
        MeanActivation += Stream.ActivationLevel;
    }
    MeanActivation /= static_cast<float>(ConsciousnessStreams.Num());

    // Blend each stream toward the mean (partial synchronization)
    const float SyncFactor = 0.3f;
    for (FConsciousnessStreamState& Stream : ConsciousnessStreams)
    {
        Stream.ActivationLevel = FMath::Lerp(Stream.ActivationLevel, MeanActivation, SyncFactor);
    }
}

bool UAngelClawIntegration::IsTriadicSyncPoint() const
{
    // Triadic sync at steps {4, 8, 12}
    return (CurrentCycleStep % 4 == 0);
}

FString UAngelClawIntegration::StepToPhase(int32 Step)
{
    // Steps map to phases in groups of 3:
    // 1,4,7,10 = Perceiving; 2,5,8,11 = Acting; 3,6,9,12 = Reflecting
    int32 PhaseIndex = ((Step - 1) % 3);
    switch (PhaseIndex)
    {
    case 0: return TEXT("Perceiving");
    case 1: return TEXT("Acting");
    case 2: return TEXT("Reflecting");
    default: return TEXT("Unknown");
    }
}

// =============================================================================
// Introspection & Wisdom
// =============================================================================

FString UAngelClawIntegration::PerformIntrospection()
{
    FString Report = FString::Printf(
        TEXT("[Introspection] Mode=%s Step=%d/%d Emotion=%s(V=%.2f A=%.2f) "
             "MotorReadiness=%.2f NicheCoupling=%.2f Wisdom=%.3f Streams=["),
        *UEnum::GetValueAsString(CurrentMode),
        CurrentCycleStep, 12,
        *EmotionState.PrimaryEmotion,
        EmotionState.Valence,
        EmotionState.Arousal,
        State4E.MotorReadiness,
        State4E.NicheCouplingStrength,
        WisdomLevel);

    for (int32 i = 0; i < ConsciousnessStreams.Num(); ++i)
    {
        const FConsciousnessStreamState& S = ConsciousnessStreams[i];
        Report += FString::Printf(TEXT("%s(%.2f)"), *S.CurrentPhase, S.ActivationLevel);
        if (i < ConsciousnessStreams.Num() - 1)
        {
            Report += TEXT(", ");
        }
    }
    Report += TEXT("]");

    // Introspection deepens wisdom
    CultivateWisdom(0.005f);

    return Report;
}

void UAngelClawIntegration::CultivateWisdom(float IntrospectiveDepth)
{
    // Wisdom grows logarithmically — fast early gains, slower over time
    WisdomLevel = FMath::Clamp(
        WisdomLevel + IntrospectiveDepth * (1.0f - WisdomLevel), 0.0f, 1.0f);
}

// =============================================================================
// Bridge to u9n Systems
// =============================================================================

void UAngelClawIntegration::ConnectToStrategicCognitionBridge()
{
    // Find StrategicCognitionBridge on same actor or owner and register
    // angelclaw's cognitive state as input to left-right hemisphere coordination
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    // The StrategicCognitionBridge handles DTE (right hemisphere, creative/intuitive)
    // and Marduk (left hemisphere, analytical/structured) coordination.
    // AngelClaw feeds its 4E state and emotion into the right hemisphere input.
    UE_LOG(LogTemp, Log, TEXT("AngelClaw: Connected to StrategicCognitionBridge on %s"),
           *Owner->GetName());
}

void UAngelClawIntegration::SyncWithNeurochemicalSystems()
{
    // Map angelclaw's valence-arousal emotion to u9n's neurochemical systems:
    // - Valence > 0 -> CognitiveEndorphinJelly boost
    // - Arousal > threshold -> EchoPulseResonanceChamber activation
    // - Stability deviation -> HomeostasisRegulator correction signal
    UE_LOG(LogTemp, Verbose,
           TEXT("AngelClaw: Syncing emotion (V=%.2f A=%.2f) to neurochemical systems"),
           EmotionState.Valence, EmotionState.Arousal);
}

void UAngelClawIntegration::BridgeToBehaviorTree()
{
    // Expose angelclaw 4E cognition as blackboard keys for u9n behavior trees:
    // - MotorReadiness -> BB_MotorReadiness
    // - NicheCouplingStrength -> BB_NicheCoupling
    // - CognitiveMode -> BB_CognitiveMode
    // - WisdomLevel -> BB_WisdomLevel
    UE_LOG(LogTemp, Verbose,
           TEXT("AngelClaw: Bridging 4E state to behavior tree blackboard"));
}
