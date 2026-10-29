#include "AvatarCognitionBridge.h"
#include "../Neurochemical/NeurochemicalSimulationComponent.h"

UAvatarCognitionBridge::UAvatarCognitionBridge()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;

    ExpressionSystem = nullptr;
    EchobeatsInterface = nullptr;
    Reservoir = nullptr;
    ReservoirCognition = nullptr;
    EmbodiedCognition = nullptr;
    Neurochemistry = nullptr;

    LastEchobeatStep = 0;
    EmbodimentCoherence = 1.0f;
}

void UAvatarCognitionBridge::BeginPlay()
{
    Super::BeginPlay();

    FindComponentReferences();
    SetupDefaultMappings();
    InitializeDefaultEchobeatsProfiles();
    BindCognitiveEvents();

    UE_LOG(LogTemp, Log, TEXT("AvatarCognitionBridge initialized"));
}

void UAvatarCognitionBridge::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bEnableCognitiveCoupling)
        return;

    // Update from reservoir state
    if (bEnableReservoirMapping && ReservoirCognition)
    {
        UpdateFromReservoirState();
    }

    // The face is decided by the expression system (affect / model); the bridge mirrors it
    if (ExpressionSystem)
    {
        TargetEmbodiment.Expression = ExpressionSystem->GetTargetExpressionState();
    }

    // Smooth transition to target embodiment
    SmoothEmbodimentTransition(DeltaTime);

    // Apply embodiment to expression system
    ApplyEmbodimentToExpression();

    // Update coherence metric
    UpdateEmbodimentCoherence();
}

void UAvatarCognitionBridge::FindComponentReferences()
{
    AActor* Owner = GetOwner();
    if (!Owner)
        return;

    ExpressionSystem = Owner->FindComponentByClass<UDeepTreeEchoExpressionSystem>();
    EchobeatsInterface = Owner->FindComponentByClass<UEchobeatsCognitiveInterface>();
    Reservoir = Owner->FindComponentByClass<UDeepTreeEchoReservoir>();
    ReservoirCognition = Owner->FindComponentByClass<UReservoirCognitiveIntegration>();
    EmbodiedCognition = Owner->FindComponentByClass<UEmbodiedCognitionComponent>();
    Neurochemistry = Owner->FindComponentByClass<UNeurochemicalSimulationComponent>();

    UE_LOG(LogTemp, Log, TEXT("AvatarCognitionBridge found components: Expression=%s, Echobeats=%s, Reservoir=%s, Affect=%s"),
        ExpressionSystem ? TEXT("Yes") : TEXT("No"),
        EchobeatsInterface ? TEXT("Yes") : TEXT("No"),
        Reservoir ? TEXT("Yes") : TEXT("No"),
        Neurochemistry ? TEXT("Yes") : TEXT("No"));
}

void UAvatarCognitionBridge::BindCognitiveEvents()
{
    if (EchobeatsInterface)
    {
        EchobeatsInterface->OnCognitiveStepChanged.AddDynamic(this, &UAvatarCognitionBridge::OnEchobeatStepChanged);
        EchobeatsInterface->OnCognitiveCycleComplete.AddDynamic(this, &UAvatarCognitionBridge::OnCognitiveCycleComplete);
    }
}

void UAvatarCognitionBridge::InitializeBridge()
{
    FindComponentReferences();
    SetupDefaultMappings();
    InitializeDefaultEchobeatsProfiles();
    BindCognitiveEvents();
}

void UAvatarCognitionBridge::SetupDefaultMappings()
{
    // Cognitive states modulate autonomic rates and the tech glow only. None of them names an
    // expression: the face is derived from affect, or chosen by the model for an utterance.
    CognitiveExpressionMappings.Empty();

    auto AddMapping = [this](ECognitiveState State, float Emissive, float Breathing, float Blink)
    {
        FCognitiveExpressionMapping Mapping;
        Mapping.CognitiveState = State;
        Mapping.EmissiveModifier = Emissive;
        Mapping.BreathingModifier = Breathing;
        Mapping.BlinkModifier = Blink;
        CognitiveExpressionMappings.Add(Mapping);
    };

    AddMapping(ECognitiveState::Idle,          0.8f, 0.8f, 1.0f);
    AddMapping(ECognitiveState::Listening,     1.1f, 1.0f, 0.7f);
    AddMapping(ECognitiveState::Processing,    1.2f, 0.9f, 0.8f);
    AddMapping(ECognitiveState::Reasoning,     1.3f, 0.7f, 0.6f);
    AddMapping(ECognitiveState::Insight,       1.5f, 1.3f, 1.2f);
    AddMapping(ECognitiveState::Communicating, 1.0f, 1.2f, 1.0f);
    AddMapping(ECognitiveState::Reflecting,    0.9f, 0.8f, 0.8f);
    AddMapping(ECognitiveState::Engaging,      1.2f, 1.0f, 0.9f);

    UE_LOG(LogTemp, Log, TEXT("Setup %d default cognitive modulation mappings"), CognitiveExpressionMappings.Num());
}

void UAvatarCognitionBridge::InitializeDefaultEchobeatsProfiles()
{
    // Per-step timing for the affect re-evaluation, plus a glow pulse on the pivotal steps
    // {1,5,9} that open each triad. No step names an expression.
    EchobeatsProfiles.Empty();
    EchobeatsProfiles.Reserve(12);

    for (int32 Step = 1; Step <= 12; ++Step)
    {
        FEchobeatsExpressionProfile Profile;
        Profile.StepNumber = Step;

        const bool bPivotal = (Step % 4) == 1;         // 1, 5, 9
        const bool bIntegration = (Step % 4) == 0;     // 4, 8, 12
        Profile.TransitionDuration = bPivotal ? 0.3f : (bIntegration ? 0.5f : 0.4f);
        Profile.bPulseOnEntry = bPivotal;

        EchobeatsProfiles.Add(Profile);
    }

    UE_LOG(LogTemp, Log, TEXT("Initialized %d echobeats profiles"), EchobeatsProfiles.Num());
}

// ========================================
// COGNITIVE COUPLING
// ========================================

void UAvatarCognitionBridge::UpdateFromCognitiveState(ECognitiveState NewState)
{
    FCognitiveExpressionMapping Mapping = GetMappingForCognitiveState(NewState);

    TargetEmbodiment.EmissiveIntensity = Mapping.EmissiveModifier;
    TargetEmbodiment.BreathingRate = Mapping.BreathingModifier;
    TargetEmbodiment.BlinkRate = Mapping.BlinkModifier;

    if (ExpressionSystem)
    {
        // Records the state and re-derives the face from affect
        ExpressionSystem->SetCognitiveState(NewState);
    }
}

void UAvatarCognitionBridge::ProcessRelevanceRealization(float Salience, bool bIsInsight)
{
    if (bIsInsight)
    {
        OnInsightTriggered(Salience);
        return;
    }

    // Salience is real attention; the expression system maps it onto gaze deterministically
    if (ExpressionSystem)
    {
        ExpressionSystem->SetAttentionLevel(Salience);
    }

    if (Salience > 0.7f)
    {
        TargetEmbodiment.EmissiveIntensity = 1.0f + Salience * 0.5f;

        if (ExpressionSystem)
        {
            ExpressionSystem->RefreshExpressionFromAffect(0.3f);
            ExpressionSystem->PulseEmissives(0.3f, 1.5f + Salience);
        }
    }
}

void UAvatarCognitionBridge::UpdateAttentionFocus(const FVector& WorldPosition, float Salience)
{
    TargetEmbodiment.AttentionFocus = WorldPosition;

    // Higher salience = more focused gaze
    if (Salience > 0.5f)
    {
        TargetEmbodiment.SaccadeFrequency = 0.5f;  // Reduce saccades when focused
    }
    else
    {
        TargetEmbodiment.SaccadeFrequency = 1.0f + (1.0f - Salience);  // More saccades when scanning
    }
}

// ========================================
// RESERVOIR COUPLING
// ========================================

void UAvatarCognitionBridge::UpdateFromReservoirState()
{
    if (!ReservoirCognition)
        return;

    FReservoirCognitiveState CogState = ReservoirCognition->GetCognitiveState();

    // Salience becomes attention
    MapSalienceToExpression(CogState.SalienceMap);

    // Map affordances to posture
    MapAffordanceToPosture(CogState.AffordanceMap);

    // Prediction confidence affects breathing
    TargetEmbodiment.BreathingRate = 0.8f + CogState.PredictionConfidence * 0.4f;

    // Integration coherence affects overall expression intensity
    TargetEmbodiment.ExpressionIntensity = 0.5f + CogState.IntegrationCoherence * 0.5f;
}

void UAvatarCognitionBridge::MapSalienceToExpression(const TMap<FString, float>& SalienceMap)
{
    // Find highest salience
    float MaxSalience = 0.0f;
    FString MostSalient;

    for (const auto& Pair : SalienceMap)
    {
        if (Pair.Value > MaxSalience)
        {
            MaxSalience = Pair.Value;
            MostSalient = Pair.Key;
        }
    }

    // Peak salience is the character's real attention level; the expression system folds it
    // into the affect mapping on the next re-evaluation (gaze sharpens with attention).
    if (ExpressionSystem)
    {
        ExpressionSystem->SetAttentionLevel(MaxSalience);
    }

    // The tech glow brightens with salience
    if (MaxSalience > 0.8f)
    {
        TargetEmbodiment.EmissiveIntensity = 1.2f + MaxSalience * 0.3f;
    }
    else if (MaxSalience > 0.5f)
    {
        TargetEmbodiment.EmissiveIntensity = 1.0f + MaxSalience * 0.2f;
    }
}

void UAvatarCognitionBridge::MapAffordanceToPosture(const TMap<FString, float>& AffordanceMap)
{
    // Calculate average affordance confidence
    float TotalAffordance = 0.0f;
    int32 Count = 0;

    for (const auto& Pair : AffordanceMap)
    {
        TotalAffordance += Pair.Value;
        Count++;
    }

    float AvgAffordance = Count > 0 ? TotalAffordance / Count : 0.5f;

    // More affordances = more open posture
    TargetEmbodiment.PostureOpenness = 0.3f + AvgAffordance * 0.7f;
}

// ========================================
// ECHOBEATS COUPLING
// ========================================

void UAvatarCognitionBridge::OnEchobeatStepChanged(int32 NewStep, const FString& PhaseName)
{
    if (!bEnableEchobeatSync)
        return;

    FEchobeatsExpressionProfile Profile = GetProfileForStep(NewStep);

    if (ExpressionSystem)
    {
        // The step is when affect is re-read; it does not say what the face shows
        ExpressionSystem->RefreshExpressionFromAffect(Profile.TransitionDuration);

        if (Profile.bPulseOnEntry)
        {
            ExpressionSystem->PulseEmissives(0.2f, 1.5f);
        }

        TargetEmbodiment.Expression = ExpressionSystem->GetTargetExpressionState();
    }

    LastEchobeatStep = NewStep;

    UE_LOG(LogTemp, Verbose, TEXT("Echobeat step %d (%s): affect re-evaluated -> expression %d"),
        NewStep, *PhaseName, (int32)TargetEmbodiment.Expression);
}

void UAvatarCognitionBridge::OnCognitiveCycleComplete(int32 CycleCount)
{
    // Subtle emissive pulse marks the cycle boundary; the face is not touched
    if (ExpressionSystem)
    {
        ExpressionSystem->PulseEmissives(0.5f, 1.2f);
    }

    UE_LOG(LogTemp, Log, TEXT("Cognitive cycle %d complete"), CycleCount);
}

void UAvatarCognitionBridge::OnInsightTriggered(float Intensity)
{
    Intensity = FMath::Clamp(Intensity, 0.0f, 1.0f);

    // An insight is a real event: it rewards the affect source, and the face follows affect
    if (Neurochemistry)
    {
        Neurochemistry->TriggerRewardResponse(Intensity);
    }

    if (ExpressionSystem)
    {
        ExpressionSystem->RefreshExpressionFromAffect(0.3f);

        // The model decides what, if anything, the face does for this moment
        const FString Event = FString::Printf(
            TEXT("An insight just landed (intensity %.2f of 1): something the character had been reasoning about resolved into a clear pattern."),
            Intensity);
        if (!ExpressionSystem->RequestExpressionForEvent(Event))
        {
            UE_LOG(LogTemp, Warning, TEXT("AvatarCognitionBridge: insight expression not requested, no model bridge; face follows affect only"));
        }

        ExpressionSystem->PulseEmissives(0.5f, 2.0f + Intensity);
    }

    TargetEmbodiment.EmissiveIntensity = 1.5f + Intensity * 0.5f;

    UE_LOG(LogTemp, Log, TEXT("Insight triggered with intensity %.2f"), Intensity);
}

// ========================================
// 4E COGNITION
// ========================================

void UAvatarCognitionBridge::UpdateEmbodiedGrounding()
{
    if (!EmbodiedCognition)
        return;

    // Get body schema influence
    // This would integrate with the EmbodiedCognitionComponent
    // to ground cognitive states in physical avatar state
}

void UAvatarCognitionBridge::ProcessEnvironmentalEmbedding(const FString& ContextDescription)
{
    // Environmental context modulates expression
    // E.g., dark environment -> more subdued expressions
    // Social context -> more expressive
}

void UAvatarCognitionBridge::ExecuteEnactedLoop(const FString& ActionType)
{
    // Expression-action coupling
    // Actions trigger corresponding expressions
    // Expressions prepare for actions
}

void UAvatarCognitionBridge::ExtendCognition(const FString& ExtensionType)
{
    // Avatar as cognitive extension
    // User's cognitive processes extend through avatar
}

// ========================================
// INTERNAL METHODS
// ========================================

void UAvatarCognitionBridge::ApplyEmbodimentToExpression()
{
    if (!ExpressionSystem)
        return;

    // Only the tech glow is pushed; the expression itself belongs to the expression system
    ExpressionSystem->SetEmissiveIntensity(CurrentEmbodiment.EmissiveIntensity);
}

void UAvatarCognitionBridge::SmoothEmbodimentTransition(float DeltaTime)
{
    float Alpha = 1.0f - FMath::Pow(GlobalSmoothing, DeltaTime * 10.0f);

    // Smooth numeric values
    CurrentEmbodiment.ExpressionIntensity = FMath::Lerp(CurrentEmbodiment.ExpressionIntensity, TargetEmbodiment.ExpressionIntensity, Alpha);
    CurrentEmbodiment.EmissiveIntensity = FMath::Lerp(CurrentEmbodiment.EmissiveIntensity, TargetEmbodiment.EmissiveIntensity, Alpha);
    CurrentEmbodiment.BreathingRate = FMath::Lerp(CurrentEmbodiment.BreathingRate, TargetEmbodiment.BreathingRate, Alpha);
    CurrentEmbodiment.BlinkRate = FMath::Lerp(CurrentEmbodiment.BlinkRate, TargetEmbodiment.BlinkRate, Alpha);
    CurrentEmbodiment.SaccadeFrequency = FMath::Lerp(CurrentEmbodiment.SaccadeFrequency, TargetEmbodiment.SaccadeFrequency, Alpha);
    CurrentEmbodiment.HeadMovementAmplitude = FMath::Lerp(CurrentEmbodiment.HeadMovementAmplitude, TargetEmbodiment.HeadMovementAmplitude, Alpha);
    CurrentEmbodiment.PostureOpenness = FMath::Lerp(CurrentEmbodiment.PostureOpenness, TargetEmbodiment.PostureOpenness, Alpha);
    CurrentEmbodiment.AttentionFocus = FMath::Lerp(CurrentEmbodiment.AttentionFocus, TargetEmbodiment.AttentionFocus, Alpha);

    // Expression state snaps when intensity is high enough
    if (Alpha > 0.5f)
    {
        CurrentEmbodiment.Expression = TargetEmbodiment.Expression;
    }
}

FCognitiveExpressionMapping UAvatarCognitionBridge::GetMappingForCognitiveState(ECognitiveState State) const
{
    for (const FCognitiveExpressionMapping& Mapping : CognitiveExpressionMappings)
    {
        if (Mapping.CognitiveState == State)
        {
            return Mapping;
        }
    }

    // Default mapping: no modulation
    FCognitiveExpressionMapping Default;
    Default.CognitiveState = State;
    return Default;
}

FEchobeatsExpressionProfile UAvatarCognitionBridge::GetProfileForStep(int32 Step) const
{
    for (const FEchobeatsExpressionProfile& Profile : EchobeatsProfiles)
    {
        if (Profile.StepNumber == Step)
        {
            return Profile;
        }
    }

    // Default profile
    FEchobeatsExpressionProfile Default;
    Default.StepNumber = Step;
    return Default;
}

void UAvatarCognitionBridge::UpdateEmbodimentCoherence()
{
    // Coherence is high when all systems are aligned
    float ExpressionMatch = (CurrentEmbodiment.Expression == TargetEmbodiment.Expression) ? 1.0f : 0.5f;
    float IntensityMatch = 1.0f - FMath::Abs(CurrentEmbodiment.ExpressionIntensity - TargetEmbodiment.ExpressionIntensity);
    float EmissiveMatch = 1.0f - FMath::Abs(CurrentEmbodiment.EmissiveIntensity - TargetEmbodiment.EmissiveIntensity) / 2.0f;

    EmbodimentCoherence = (ExpressionMatch + IntensityMatch + EmissiveMatch) / 3.0f;
}

float UAvatarCognitionBridge::GetEmbodimentCoherence() const
{
    return EmbodimentCoherence;
}
