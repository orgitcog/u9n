#include "AvatarCognitionBridge.h"

UAvatarCognitionBridge::UAvatarCognitionBridge()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;

    ExpressionSystem = nullptr;
    EchobeatsInterface = nullptr;
    Reservoir = nullptr;
    ReservoirCognition = nullptr;
    EmbodiedCognition = nullptr;

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

    UE_LOG(LogTemp, Log, TEXT("AvatarCognitionBridge found components: Expression=%s, Echobeats=%s, Reservoir=%s"),
        ExpressionSystem ? TEXT("Yes") : TEXT("No"),
        EchobeatsInterface ? TEXT("Yes") : TEXT("No"),
        Reservoir ? TEXT("Yes") : TEXT("No"));
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
    CognitiveExpressionMappings.Empty();

    // Idle -> Neutral
    {
        FCognitiveExpressionMapping Mapping;
        Mapping.CognitiveState = ECognitiveState::Idle;
        Mapping.PrimaryExpression = EExpressionState::Neutral;
        Mapping.EmissiveModifier = 0.8f;
        Mapping.BreathingModifier = 0.8f;
        CognitiveExpressionMappings.Add(Mapping);
    }

    // Listening -> Alert Gaze
    {
        FCognitiveExpressionMapping Mapping;
        Mapping.CognitiveState = ECognitiveState::Listening;
        Mapping.PrimaryExpression = EExpressionState::AlertGaze;
        Mapping.SecondaryExpression = EExpressionState::CuriousGaze;
        Mapping.SecondaryBlendWeight = 0.3f;
        Mapping.EmissiveModifier = 1.1f;
        Mapping.BlinkModifier = 0.7f;
        CognitiveExpressionMappings.Add(Mapping);
    }

    // Processing -> Contemplative
    {
        FCognitiveExpressionMapping Mapping;
        Mapping.CognitiveState = ECognitiveState::Processing;
        Mapping.PrimaryExpression = EExpressionState::Contemplative;
        Mapping.EmissiveModifier = 1.2f;
        Mapping.BreathingModifier = 0.9f;
        Mapping.BlinkModifier = 0.8f;
        CognitiveExpressionMappings.Add(Mapping);
    }

    // Speaking -> Speaking Vowel
    {
        FCognitiveExpressionMapping Mapping;
        Mapping.CognitiveState = ECognitiveState::Speaking;
        Mapping.PrimaryExpression = EExpressionState::SpeakingVowel;
        Mapping.EmissiveModifier = 1.0f;
        Mapping.BreathingModifier = 1.2f;
        CognitiveExpressionMappings.Add(Mapping);
    }

    // Thinking -> Contemplative with upward gaze
    {
        FCognitiveExpressionMapping Mapping;
        Mapping.CognitiveState = ECognitiveState::Thinking;
        Mapping.PrimaryExpression = EExpressionState::Contemplative;
        Mapping.SecondaryExpression = EExpressionState::UpwardGaze;
        Mapping.SecondaryBlendWeight = 0.4f;
        Mapping.EmissiveModifier = 1.3f;
        Mapping.BreathingModifier = 0.7f;
        Mapping.BlinkModifier = 0.6f;
        CognitiveExpressionMappings.Add(Mapping);
    }

    // Excited -> Broad Smile
    {
        FCognitiveExpressionMapping Mapping;
        Mapping.CognitiveState = ECognitiveState::Excited;
        Mapping.PrimaryExpression = EExpressionState::BroadSmile;
        Mapping.SecondaryExpression = EExpressionState::Laughing;
        Mapping.SecondaryBlendWeight = 0.2f;
        Mapping.EmissiveModifier = 1.5f;
        Mapping.BreathingModifier = 1.3f;
        Mapping.BlinkModifier = 1.2f;
        CognitiveExpressionMappings.Add(Mapping);
    }

    // Curious -> Curious Gaze
    {
        FCognitiveExpressionMapping Mapping;
        Mapping.CognitiveState = ECognitiveState::Curious;
        Mapping.PrimaryExpression = EExpressionState::CuriousGaze;
        Mapping.SecondaryExpression = EExpressionState::AlertGaze;
        Mapping.SecondaryBlendWeight = 0.2f;
        Mapping.EmissiveModifier = 1.2f;
        Mapping.BlinkModifier = 0.9f;
        CognitiveExpressionMappings.Add(Mapping);
    }

    // Confused -> Contemplative with concern blend
    {
        FCognitiveExpressionMapping Mapping;
        Mapping.CognitiveState = ECognitiveState::Confused;
        Mapping.PrimaryExpression = EExpressionState::Contemplative;
        Mapping.EmissiveModifier = 0.9f;
        Mapping.BreathingModifier = 1.1f;
        CognitiveExpressionMappings.Add(Mapping);
    }

    UE_LOG(LogTemp, Log, TEXT("Setup %d default cognitive-expression mappings"), CognitiveExpressionMappings.Num());
}

void UAvatarCognitionBridge::InitializeDefaultEchobeatsProfiles()
{
    EchobeatsProfiles.Empty();
    EchobeatsProfiles.Reserve(12);

    // Step 1: Pivotal-Perception - Alert
    {
        FEchobeatsExpressionProfile Profile;
        Profile.StepNumber = 1;
        Profile.TargetExpression = EExpressionState::AlertGaze;
        Profile.TransitionDuration = 0.3f;
        Profile.bPulseOnEntry = true;
        Profile.MicroExpression = EMicroExpression::UnilateralBrowRaise;
        EchobeatsProfiles.Add(Profile);
    }

    // Step 2: Affordance-Perception - Curious
    {
        FEchobeatsExpressionProfile Profile;
        Profile.StepNumber = 2;
        Profile.TargetExpression = EExpressionState::CuriousGaze;
        Profile.TransitionDuration = 0.4f;
        EchobeatsProfiles.Add(Profile);
    }

    // Step 3: Salience-Perception - Awe
    {
        FEchobeatsExpressionProfile Profile;
        Profile.StepNumber = 3;
        Profile.TargetExpression = EExpressionState::Awe;
        Profile.TransitionDuration = 0.4f;
        Profile.bPulseOnEntry = true;
        Profile.MicroExpression = EMicroExpression::EyelidFlutter;
        EchobeatsProfiles.Add(Profile);
    }

    // Step 4: Integration-1 - Gentle Smile
    {
        FEchobeatsExpressionProfile Profile;
        Profile.StepNumber = 4;
        Profile.TargetExpression = EExpressionState::GentleSmile;
        Profile.TransitionDuration = 0.5f;
        EchobeatsProfiles.Add(Profile);
    }

    // Step 5: Pivotal-Action - Gentle Smile
    {
        FEchobeatsExpressionProfile Profile;
        Profile.StepNumber = 5;
        Profile.TargetExpression = EExpressionState::GentleSmile;
        Profile.TransitionDuration = 0.4f;
        EchobeatsProfiles.Add(Profile);
    }

    // Step 6: Affordance-Action - Speaking
    {
        FEchobeatsExpressionProfile Profile;
        Profile.StepNumber = 6;
        Profile.TargetExpression = EExpressionState::SpeakingVowel;
        Profile.TransitionDuration = 0.3f;
        EchobeatsProfiles.Add(Profile);
    }

    // Step 7: Salience-Action - Broad Smile
    {
        FEchobeatsExpressionProfile Profile;
        Profile.StepNumber = 7;
        Profile.TargetExpression = EExpressionState::BroadSmile;
        Profile.TransitionDuration = 0.4f;
        Profile.bPulseOnEntry = true;
        EchobeatsProfiles.Add(Profile);
    }

    // Step 8: Integration-2 - Neutral (rest)
    {
        FEchobeatsExpressionProfile Profile;
        Profile.StepNumber = 8;
        Profile.TargetExpression = EExpressionState::Neutral;
        Profile.TransitionDuration = 0.5f;
        Profile.MicroExpression = EMicroExpression::Blink;
        EchobeatsProfiles.Add(Profile);
    }

    // Step 9: Pivotal-Simulation - Contemplative
    {
        FEchobeatsExpressionProfile Profile;
        Profile.StepNumber = 9;
        Profile.TargetExpression = EExpressionState::Contemplative;
        Profile.TransitionDuration = 0.4f;
        EchobeatsProfiles.Add(Profile);
    }

    // Step 10: Affordance-Simulation - Contemplative
    {
        FEchobeatsExpressionProfile Profile;
        Profile.StepNumber = 10;
        Profile.TargetExpression = EExpressionState::Contemplative;
        Profile.TransitionDuration = 0.4f;
        EchobeatsProfiles.Add(Profile);
    }

    // Step 11: Salience-Simulation - Blissful
    {
        FEchobeatsExpressionProfile Profile;
        Profile.StepNumber = 11;
        Profile.TargetExpression = EExpressionState::Blissful;
        Profile.TransitionDuration = 0.5f;
        EchobeatsProfiles.Add(Profile);
    }

    // Step 12: Integration-3 - Contemplative (cycle end)
    {
        FEchobeatsExpressionProfile Profile;
        Profile.StepNumber = 12;
        Profile.TargetExpression = EExpressionState::Contemplative;
        Profile.TransitionDuration = 0.5f;
        EchobeatsProfiles.Add(Profile);
    }

    UE_LOG(LogTemp, Log, TEXT("Initialized %d echobeats expression profiles"), EchobeatsProfiles.Num());
}

// ========================================
// COGNITIVE COUPLING
// ========================================

void UAvatarCognitionBridge::UpdateFromCognitiveState(ECognitiveState NewState)
{
    FCognitiveExpressionMapping Mapping = GetMappingForCognitiveState(NewState);

    TargetEmbodiment.Expression = Mapping.PrimaryExpression;
    TargetEmbodiment.ExpressionIntensity = 1.0f - Mapping.SecondaryBlendWeight;
    TargetEmbodiment.EmissiveIntensity = Mapping.EmissiveModifier;
    TargetEmbodiment.BreathingRate = Mapping.BreathingModifier;
    TargetEmbodiment.BlinkRate = Mapping.BlinkModifier;

    if (ExpressionSystem)
    {
        ExpressionSystem->SetCognitiveState(NewState);
    }
}

void UAvatarCognitionBridge::ProcessRelevanceRealization(float Salience, bool bIsInsight)
{
    if (bIsInsight)
    {
        OnInsightTriggered(Salience);
    }
    else if (Salience > 0.7f)
    {
        // High salience triggers alert expression
        TargetEmbodiment.Expression = EExpressionState::AlertGaze;
        TargetEmbodiment.EmissiveIntensity = 1.0f + Salience * 0.5f;

        if (ExpressionSystem)
        {
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

    // Map salience to expression
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

    // High salience triggers more alert expression
    if (MaxSalience > 0.8f)
    {
        TargetEmbodiment.Expression = EExpressionState::AlertGaze;
        TargetEmbodiment.EmissiveIntensity = 1.2f + MaxSalience * 0.3f;
    }
    else if (MaxSalience > 0.5f)
    {
        TargetEmbodiment.Expression = EExpressionState::CuriousGaze;
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

    TargetEmbodiment.Expression = Profile.TargetExpression;

    if (Profile.bPulseOnEntry && ExpressionSystem)
    {
        ExpressionSystem->PulseEmissives(0.2f, 1.5f);
    }

    if (Profile.MicroExpression != EMicroExpression::None && ExpressionSystem)
    {
        ExpressionSystem->TriggerMicroExpression(Profile.MicroExpression, 0.2f);
    }

    LastEchobeatStep = NewStep;

    UE_LOG(LogTemp, Verbose, TEXT("Echobeat step %d (%s): Expression -> %d"),
        NewStep, *PhaseName, (int32)Profile.TargetExpression);
}

void UAvatarCognitionBridge::OnCognitiveCycleComplete(int32 CycleCount)
{
    // Trigger a satisfied expression at cycle completion
    if (ExpressionSystem)
    {
        ExpressionSystem->TriggerMicroExpression(EMicroExpression::Blink, 0.3f);
    }

    // Subtle emissive pulse to mark cycle boundary
    if (ExpressionSystem)
    {
        ExpressionSystem->PulseEmissives(0.5f, 1.2f);
    }

    UE_LOG(LogTemp, Log, TEXT("Cognitive cycle %d complete"), CycleCount);
}

void UAvatarCognitionBridge::OnInsightTriggered(float Intensity)
{
    // Insight triggers Wonder-to-Joy transition
    if (ExpressionSystem)
    {
        ExpressionSystem->TriggerWonderToJoyTransition(1.5f * (2.0f - Intensity));
        ExpressionSystem->PulseEmissives(0.5f, 2.0f + Intensity);
    }

    TargetEmbodiment.Expression = EExpressionState::BroadSmile;
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

    // Apply current embodiment state to expression system
    ExpressionSystem->SetExpressionState(CurrentEmbodiment.Expression, GlobalSmoothing);
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

    // Default mapping
    FCognitiveExpressionMapping Default;
    Default.CognitiveState = State;
    Default.PrimaryExpression = EExpressionState::Neutral;
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
    Default.TargetExpression = EExpressionState::Neutral;
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
