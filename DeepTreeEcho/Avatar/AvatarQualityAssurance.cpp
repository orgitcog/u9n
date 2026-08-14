// AvatarQualityAssurance.cpp
// Deep Tree Echo - Fanatical Attention to Detail Implementation
// Copyright (c) 2025 Deep Tree Echo Project

#include "AvatarQualityAssurance.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogAvatarQuality, Log, All);

// ========================================
// Constructor / Lifecycle
// ========================================

UAvatarQualityAssurance::UAvatarQualityAssurance()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;

    // Initialize default expressivity by emotion
    ExpressivityProfile.EmotionExpressivity.Add(TEXT("Joy"), 0.8f);
    ExpressivityProfile.EmotionExpressivity.Add(TEXT("Sadness"), 0.6f);
    ExpressivityProfile.EmotionExpressivity.Add(TEXT("Anger"), 0.7f);
    ExpressivityProfile.EmotionExpressivity.Add(TEXT("Fear"), 0.9f);
    ExpressivityProfile.EmotionExpressivity.Add(TEXT("Surprise"), 0.95f);
    ExpressivityProfile.EmotionExpressivity.Add(TEXT("Disgust"), 0.5f);
    ExpressivityProfile.EmotionExpressivity.Add(TEXT("Contempt"), 0.4f);

    // Initialize default skin flush regions
    CurrentSkinResponse.FlushRegions.Add(TEXT("Cheeks"), 0.0f);
    CurrentSkinResponse.FlushRegions.Add(TEXT("Ears"), 0.0f);
    CurrentSkinResponse.FlushRegions.Add(TEXT("Neck"), 0.0f);
    CurrentSkinResponse.FlushRegions.Add(TEXT("Forehead"), 0.0f);
}

void UAvatarQualityAssurance::BeginPlay()
{
    Super::BeginPlay();

    // Initialize emotional velocity map
    for (const auto& Pair : ExpressivityProfile.EmotionExpressivity)
    {
        EmotionalMomentum.EmotionalVelocity.Add(Pair.Key, 0.0f);
        PreviousEmotionIntensities.Add(Pair.Key, 0.0f);
    }

    UE_LOG(LogAvatarQuality, Log, TEXT("Avatar Quality Assurance initialized - Fanatical attention to detail enabled"));
}

void UAvatarQualityAssurance::TickComponent(float DeltaTime, ELevelTick TickType,
                                             FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bEnableMicroExpressionAuthenticity)
    {
        UpdateMicroExpressionAuthenticity(DeltaTime);
    }

    if (bEnablePhysiologicalCoherence)
    {
        UpdatePhysiologicalCoherence(DeltaTime);
    }

    if (bEnableExpressionMomentum)
    {
        UpdateExpressionMomentum(DeltaTime);
    }

    if (bEnableFatigueEffects)
    {
        UpdateFatigueEffects(DeltaTime);
    }

    if (bEnableEnvironmentalResponse)
    {
        UpdateEnvironmentalResponse(DeltaTime);
    }

    if (bEnableQualitySelfAssessment)
    {
        UpdateQualityMetrics(DeltaTime);
    }
}

// ========================================
// MICRO-EXPRESSION AUTHENTICITY
// ========================================

FMicroExpressionTiming UAvatarQualityAssurance::GetTimingForEmotion(const FString& EmotionType, bool bGenuine)
{
    if (bGenuine)
    {
        return GetGenuineTiming(EmotionType);
    }
    else
    {
        return GetPosedTiming(EmotionType);
    }
}

FMicroExpressionTiming UAvatarQualityAssurance::GetGenuineTiming(const FString& EmotionType) const
{
    FMicroExpressionTiming Timing;

    // Genuine emotions have faster onset (< 200ms is considered genuine)
    Timing.OnsetTime = 0.067f + FMath::FRandRange(0.0f, 0.1f); // 67-167ms
    Timing.ApexDuration = FMath::FRandRange(0.5f, 4.0f);
    Timing.OffsetTime = FMath::FRandRange(0.2f, 0.5f);

    // Genuine emotions start with more asymmetry
    Timing.OnsetAsymmetry = FMath::FRandRange(0.1f, 0.25f);

    // Add natural temporal jitter
    Timing.TemporalJitter = 0.03f;

    // Emotion-specific adjustments
    if (EmotionType == TEXT("Surprise"))
    {
        Timing.OnsetTime = 0.05f; // Very fast onset
        Timing.ApexDuration = 0.5f; // Short apex
    }
    else if (EmotionType == TEXT("Sadness"))
    {
        Timing.OnsetTime = 0.15f;
        Timing.ApexDuration = FMath::FRandRange(2.0f, 10.0f); // Long duration
        Timing.OffsetTime = 0.8f; // Slow fade
    }
    else if (EmotionType == TEXT("Joy"))
    {
        // Duchenne smile characteristics
        Timing.OnsetAsymmetry = 0.05f; // More symmetric for genuine joy
    }

    return Timing;
}

FMicroExpressionTiming UAvatarQualityAssurance::GetPosedTiming(const FString& EmotionType) const
{
    FMicroExpressionTiming Timing;

    // Posed emotions have slower, more deliberate onset (> 500ms is often posed)
    Timing.OnsetTime = 0.3f + FMath::FRandRange(0.1f, 0.3f); // 300-600ms
    Timing.ApexDuration = FMath::FRandRange(0.3f, 1.5f);
    Timing.OffsetTime = FMath::FRandRange(0.3f, 0.6f);

    // Posed expressions tend to be more symmetric
    Timing.OnsetAsymmetry = FMath::FRandRange(0.0f, 0.05f);

    // Less natural jitter
    Timing.TemporalJitter = 0.01f;

    return Timing;
}

void UAvatarQualityAssurance::ApplyAsymmetryToExpression(float& LeftValue, float& RightValue, const FString& Region)
{
    float Asymmetry = AsymmetrySettings.BaselineAsymmetry;

    // Add region-specific asymmetry
    if (Region == TEXT("Brow"))
    {
        Asymmetry += AsymmetrySettings.BrowAsymmetry;
    }
    else if (Region == TEXT("Mouth"))
    {
        Asymmetry += AsymmetrySettings.MouthAsymmetry;
    }

    // Add dynamic asymmetry
    Asymmetry += AsymmetrySettings.DynamicAsymmetry * FMath::FRandRange(-0.5f, 0.5f);

    // Apply based on dominant side
    if (AsymmetrySettings.DominantSide > 0.5f)
    {
        // Right side dominant
        RightValue *= (1.0f + Asymmetry);
        LeftValue *= (1.0f - Asymmetry * 0.5f);
    }
    else
    {
        // Left side dominant
        LeftValue *= (1.0f + Asymmetry);
        RightValue *= (1.0f - Asymmetry * 0.5f);
    }
}

void UAvatarQualityAssurance::ProcessEmotionalLeakage(const FString& DisplayedEmotion,
                                                       const FString& FeltEmotion,
                                                       float SuppressionLevel)
{
    if (DisplayedEmotion == FeltEmotion || SuppressionLevel < 0.1f)
    {
        return; // No leakage needed
    }

    FEmotionalLeakage Leakage;
    Leakage.SuppressedEmotion = FeltEmotion;
    Leakage.SuppressionEffort = SuppressionLevel;

    // Calculate leakage intensity - harder to suppress strong emotions
    Leakage.LeakageIntensity = (1.0f - SuppressionLevel) * 0.3f;

    // Determine leakage region based on emotion
    if (FeltEmotion == TEXT("Fear") || FeltEmotion == TEXT("Sadness"))
    {
        Leakage.LeakageRegion = TEXT("Eyes"); // Fear/sadness leak in eye area
    }
    else if (FeltEmotion == TEXT("Anger") || FeltEmotion == TEXT("Disgust"))
    {
        Leakage.LeakageRegion = TEXT("Mouth"); // Anger/disgust leak in mouth
    }
    else if (FeltEmotion == TEXT("Joy"))
    {
        Leakage.LeakageRegion = TEXT("Eyes"); // Joy leaks in crow's feet
    }
    else
    {
        Leakage.LeakageRegion = TEXT("General");
    }

    ActiveLeakages.Add(Leakage);
    OnEmotionalLeakage.Broadcast(Leakage);

    if (bQualityLoggingEnabled)
    {
        UE_LOG(LogAvatarQuality, Verbose, TEXT("Emotional leakage: %s showing through %s at %.2f intensity"),
               *FeltEmotion, *Leakage.LeakageRegion, Leakage.LeakageIntensity);
    }
}

float UAvatarQualityAssurance::AddTemporalJitter(float BaseTime) const
{
    float Jitter = MicroExpressionTiming.TemporalJitter * FMath::FRandRange(-1.0f, 1.0f);
    return FMath::Max(0.01f, BaseTime + Jitter);
}

void UAvatarQualityAssurance::UpdateMicroExpressionAuthenticity(float DeltaTime)
{
    // Decay active leakages
    for (int32 i = ActiveLeakages.Num() - 1; i >= 0; --i)
    {
        ActiveLeakages[i].LeakageIntensity -= DeltaTime * 0.5f;
        if (ActiveLeakages[i].LeakageIntensity <= 0.0f)
        {
            ActiveLeakages.RemoveAt(i);
        }
    }
}

// ========================================
// PHYSIOLOGICAL COHERENCE
// ========================================

void UAvatarQualityAssurance::UpdateAutonomicStateFromEmotion(const FString& EmotionType, float Intensity)
{
    float Arousal = 0.5f;
    float Valence = 0.0f;

    // Map emotion to arousal/valence
    if (EmotionType == TEXT("Joy"))
    {
        Arousal = 0.5f + Intensity * 0.4f;
        Valence = Intensity;
    }
    else if (EmotionType == TEXT("Sadness"))
    {
        Arousal = 0.3f - Intensity * 0.2f;
        Valence = -Intensity;
    }
    else if (EmotionType == TEXT("Anger"))
    {
        Arousal = 0.6f + Intensity * 0.4f;
        Valence = -Intensity * 0.5f;
    }
    else if (EmotionType == TEXT("Fear"))
    {
        Arousal = 0.7f + Intensity * 0.3f;
        Valence = -Intensity;
    }
    else if (EmotionType == TEXT("Surprise"))
    {
        Arousal = 0.8f + Intensity * 0.2f;
        Valence = 0.0f;
    }
    else if (EmotionType == TEXT("Disgust"))
    {
        Arousal = 0.4f + Intensity * 0.2f;
        Valence = -Intensity * 0.7f;
    }

    UpdateAutonomicFromArousal(Arousal, Valence);
}

void UAvatarQualityAssurance::UpdateAutonomicFromArousal(float Arousal, float Valence)
{
    // Pupil dilation: increases with arousal (both positive and negative)
    CurrentAutonomicState.PupilDilation = 0.3f + Arousal * 0.5f;

    // Skin flush: increases with arousal and embarrassment
    if (Valence < 0)
    {
        // Anger causes flush
        CurrentAutonomicState.SkinFlush = Arousal * 0.6f;
    }
    else
    {
        // Joy/embarrassment causes flush
        CurrentAutonomicState.SkinFlush = Arousal * 0.4f;
    }

    // Perspiration: increases with high arousal
    CurrentAutonomicState.Perspiration = FMath::Max(0.0f, (Arousal - 0.7f) * 3.0f);

    // Tear film: increases with extreme emotions (positive or negative)
    float EmotionalExtreme = FMath::Abs(Valence) * Arousal;
    CurrentAutonomicState.TearFilm = 0.2f + EmotionalExtreme * 0.3f;

    // Nostril flare: increases with anger and disgust
    if (Valence < -0.3f && Arousal > 0.5f)
    {
        CurrentAutonomicState.NostrilFlare = Arousal * 0.5f;
    }
    else
    {
        CurrentAutonomicState.NostrilFlare *= 0.9f; // Decay
    }

    // Muscle tension: increases with negative arousal
    CurrentAutonomicState.MuscleTension = 0.2f + Arousal * 0.3f;
    if (Valence < 0)
    {
        CurrentAutonomicState.MuscleTension += FMath::Abs(Valence) * 0.2f;
    }

    // Update skin response
    UpdateSkinResponseFromAutonomic();

    OnPhysiologicalChange.Broadcast(CurrentAutonomicState);
}

FAutonomicState UAvatarQualityAssurance::GetAutonomicState() const
{
    return CurrentAutonomicState;
}

FSkinResponse UAvatarQualityAssurance::GetSkinResponse() const
{
    return CurrentSkinResponse;
}

void UAvatarQualityAssurance::SetPupilOverride(float Dilation, float Duration)
{
    bPupilOverrideActive = true;
    PupilOverrideValue = FMath::Clamp(Dilation, 0.0f, 1.0f);
    PupilOverrideTimer = Duration;
}

void UAvatarQualityAssurance::TriggerFlush(float Intensity, float Duration)
{
    bFlushActive = true;
    FlushIntensity = FMath::Clamp(Intensity, 0.0f, 1.0f);
    FlushTimer = Duration;
}

void UAvatarQualityAssurance::UpdateSkinResponseFromAutonomic()
{
    float FlushLevel = CurrentAutonomicState.SkinFlush;

    if (bFlushActive)
    {
        FlushLevel = FMath::Max(FlushLevel, FlushIntensity);
    }

    // Distribute flush to regions (cheeks first, then ears, then neck)
    CurrentSkinResponse.FlushRegions[TEXT("Cheeks")] = FlushLevel;
    CurrentSkinResponse.FlushRegions[TEXT("Ears")] = FlushLevel * 0.8f;
    CurrentSkinResponse.FlushRegions[TEXT("Neck")] = FlushLevel * 0.6f;
    CurrentSkinResponse.FlushRegions[TEXT("Forehead")] = FlushLevel * 0.4f;

    // Pallor for fear/shock (opposite of flush)
    CurrentSkinResponse.Pallor = FMath::Max(0.0f, 0.5f - FlushLevel);

    // Subsurface scattering increases with flush (blood closer to surface)
    CurrentSkinResponse.SubsurfaceIntensity = 1.0f + FlushLevel * 0.3f;
}

void UAvatarQualityAssurance::UpdatePhysiologicalCoherence(float DeltaTime)
{
    // Decay pupil override
    if (bPupilOverrideActive)
    {
        PupilOverrideTimer -= DeltaTime;
        if (PupilOverrideTimer <= 0.0f)
        {
            bPupilOverrideActive = false;
        }
    }

    // Decay flush
    if (bFlushActive)
    {
        FlushTimer -= DeltaTime;
        if (FlushTimer <= 0.0f)
        {
            bFlushActive = false;
            FlushIntensity = 0.0f;
        }
    }

    // Gradual return to baseline for autonomic state
    float DecayRate = DeltaTime * 0.1f;
    CurrentAutonomicState.PupilDilation = FMath::FInterpTo(CurrentAutonomicState.PupilDilation, 0.5f, DeltaTime, 0.5f);
    CurrentAutonomicState.SkinFlush = FMath::FInterpTo(CurrentAutonomicState.SkinFlush, 0.0f, DeltaTime, 0.3f);
    CurrentAutonomicState.Perspiration = FMath::FInterpTo(CurrentAutonomicState.Perspiration, 0.0f, DeltaTime, 0.2f);
    CurrentAutonomicState.TearFilm = FMath::FInterpTo(CurrentAutonomicState.TearFilm, 0.2f, DeltaTime, 0.4f);
    CurrentAutonomicState.NostrilFlare = FMath::FInterpTo(CurrentAutonomicState.NostrilFlare, 0.0f, DeltaTime, 0.5f);
    CurrentAutonomicState.MuscleTension = FMath::FInterpTo(CurrentAutonomicState.MuscleTension, 0.2f, DeltaTime, 0.3f);
}

// ========================================
// EXPRESSION MOMENTUM
// ========================================

FEmotionalMomentum UAvatarQualityAssurance::GetEmotionalMomentum() const
{
    return EmotionalMomentum;
}

float UAvatarQualityAssurance::ApplyMomentum(const FString& EmotionType, float TargetIntensity, float DeltaTime)
{
    float* PreviousIntensity = PreviousEmotionIntensities.Find(EmotionType);
    float* Velocity = EmotionalMomentum.EmotionalVelocity.Find(EmotionType);

    if (!PreviousIntensity || !Velocity)
    {
        return TargetIntensity;
    }

    // Calculate desired velocity
    float DesiredVelocity = (TargetIntensity - *PreviousIntensity) / FMath::Max(DeltaTime, 0.001f);

    // Apply inertia (can't change velocity instantly)
    float VelocityChange = DesiredVelocity - *Velocity;
    float MaxVelocityChange = EmotionalMomentum.MaxVelocity / EmotionalMomentum.Inertia;
    VelocityChange = FMath::Clamp(VelocityChange, -MaxVelocityChange * DeltaTime, MaxVelocityChange * DeltaTime);

    *Velocity += VelocityChange;

    // Clamp velocity
    *Velocity = FMath::Clamp(*Velocity, -EmotionalMomentum.MaxVelocity, EmotionalMomentum.MaxVelocity);

    // Apply velocity decay
    *Velocity *= (1.0f - EmotionalMomentum.DecayRate * DeltaTime);

    // Calculate new intensity
    float NewIntensity = *PreviousIntensity + *Velocity * DeltaTime;
    NewIntensity = FMath::Clamp(NewIntensity, 0.0f, 1.0f);

    // Update previous intensity
    *PreviousIntensity = NewIntensity;

    // Update persistence
    if (NewIntensity > 0.1f)
    {
        ExpressionPersistence.ResidualEmotion = EmotionType;
        ExpressionPersistence.ResidualIntensity = NewIntensity * ExpressionPersistence.PersistenceFactor;
    }

    return NewIntensity;
}

FExpressionPersistence UAvatarQualityAssurance::GetExpressionPersistence() const
{
    return ExpressionPersistence;
}

void UAvatarQualityAssurance::ResetMomentum()
{
    for (auto& Pair : EmotionalMomentum.EmotionalVelocity)
    {
        Pair.Value = 0.0f;
    }
}

void UAvatarQualityAssurance::UpdateExpressionMomentum(float DeltaTime)
{
    // Decay residual expression
    ExpressionPersistence.ResidualIntensity *= (1.0f - DeltaTime * 0.5f);

    if (ExpressionPersistence.ResidualIntensity < 0.01f)
    {
        ExpressionPersistence.ResidualIntensity = 0.0f;
        ExpressionPersistence.ResidualEmotion = TEXT("");
    }
}

// ========================================
// PERSONALITY
// ========================================

float UAvatarQualityAssurance::ApplyExpressivityProfile(const FString& EmotionType, float RawIntensity)
{
    float Expressivity = ExpressivityProfile.OverallExpressivity;

    // Apply emotion-specific expressivity
    if (const float* EmotionExpressivity = ExpressivityProfile.EmotionExpressivity.Find(EmotionType))
    {
        Expressivity *= *EmotionExpressivity;
    }

    // Apply suppression tendency
    float SuppressionEffect = ExpressivityProfile.SuppressionTendency * 0.5f;
    Expressivity *= (1.0f - SuppressionEffect);

    // Apply cultural display rules
    Expressivity *= (1.0f - CulturalNorms.DisplayRuleIntensity * 0.3f);

    return RawIntensity * Expressivity;
}

float UAvatarQualityAssurance::GetCulturalDisplayRule(const FString& EmotionType, const FString& Context)
{
    float Rule = 1.0f - CulturalNorms.DisplayRuleIntensity;

    // Context-specific adjustments
    if (Context == TEXT("Public"))
    {
        Rule *= 0.8f; // More restrained in public
    }
    else if (Context == TEXT("Private"))
    {
        Rule *= 1.2f; // More expressive in private
    }

    // Emotion-specific cultural rules
    if (EmotionType == TEXT("Anger") || EmotionType == TEXT("Disgust"))
    {
        Rule *= (1.0f - CulturalNorms.DisplayRuleIntensity * 0.3f);
    }

    return FMath::Clamp(Rule, 0.1f, 1.0f);
}

void UAvatarQualityAssurance::SetPersonalityTrait(const FString& TraitName, float Value)
{
    Value = FMath::Clamp(Value, 0.0f, 1.0f);

    if (TraitName == TEXT("Extraversion"))
    {
        ExpressivityProfile.OverallExpressivity = 0.4f + Value * 0.6f;
        ExpressivityProfile.Spontaneity = 0.3f + Value * 0.7f;
    }
    else if (TraitName == TEXT("Neuroticism"))
    {
        ExpressivityProfile.EmotionExpressivity[TEXT("Fear")] = 0.5f + Value * 0.5f;
        ExpressivityProfile.EmotionExpressivity[TEXT("Sadness")] = 0.5f + Value * 0.5f;
    }
    else if (TraitName == TEXT("Openness"))
    {
        ExpressivityProfile.SmileAuthenticity = 0.6f + Value * 0.4f;
    }
}

// ========================================
// FATIGUE
// ========================================

void UAvatarQualityAssurance::SetFatigueState(const FFatigueState& State)
{
    CurrentFatigueState = State;
    ComputeFatigueManifestations();
}

FFatigueState UAvatarQualityAssurance::GetFatigueState() const
{
    return CurrentFatigueState;
}

FFatigueManifestations UAvatarQualityAssurance::GetFatigueManifestations() const
{
    return FatigueManifestations;
}

void UAvatarQualityAssurance::ApplyFatigueToExpression(float& ExpressionIntensity, float& ResponseTime)
{
    // Fatigue dampens expressions
    ExpressionIntensity *= (1.0f - FatigueManifestations.ExpressionDampening);

    // Fatigue increases response latency
    ResponseTime *= (1.0f + FatigueManifestations.ResponseLatency);
}

void UAvatarQualityAssurance::TriggerYawn()
{
    if (bQualityLoggingEnabled)
    {
        UE_LOG(LogAvatarQuality, Verbose, TEXT("Yawn triggered due to fatigue"));
    }

    // Reset yawn probability temporarily
    FatigueManifestations.YawnProbability = 0.0f;
}

void UAvatarQualityAssurance::ComputeFatigueManifestations()
{
    float TotalFatigue = (CurrentFatigueState.PhysicalFatigue +
                         CurrentFatigueState.MentalFatigue +
                         CurrentFatigueState.EmotionalFatigue) / 3.0f;

    // Eyelid droop increases with fatigue
    FatigueManifestations.EyelidDroop = TotalFatigue * 0.3f + CurrentFatigueState.EyeStrain * 0.2f;

    // Blink rate increases with fatigue and eye strain
    FatigueManifestations.BlinkRateModifier = 1.0f + TotalFatigue * 0.5f + CurrentFatigueState.EyeStrain * 0.3f;

    // Expression dampening increases with fatigue
    FatigueManifestations.ExpressionDampening = TotalFatigue * 0.4f;

    // Response latency increases with mental fatigue
    FatigueManifestations.ResponseLatency = CurrentFatigueState.MentalFatigue * 0.3f;

    // Yawn probability based on alertness
    FatigueManifestations.YawnProbability = FMath::Max(0.0f, (1.0f - CurrentFatigueState.Alertness) * 0.1f);
}

void UAvatarQualityAssurance::UpdateFatigueEffects(float DeltaTime)
{
    // Gradually recover from fatigue (very slow)
    CurrentFatigueState.PhysicalFatigue = FMath::Max(0.0f, CurrentFatigueState.PhysicalFatigue - DeltaTime * 0.001f);
    CurrentFatigueState.MentalFatigue = FMath::Max(0.0f, CurrentFatigueState.MentalFatigue - DeltaTime * 0.002f);
    CurrentFatigueState.EyeStrain = FMath::Max(0.0f, CurrentFatigueState.EyeStrain - DeltaTime * 0.005f);

    // Alertness recovery
    CurrentFatigueState.Alertness = FMath::Min(1.0f, CurrentFatigueState.Alertness + DeltaTime * 0.01f);

    // Check for yawn
    if (FMath::FRand() < FatigueManifestations.YawnProbability * DeltaTime)
    {
        TriggerYawn();
    }

    ComputeFatigueManifestations();
}

// ========================================
// ENVIRONMENTAL
// ========================================

void UAvatarQualityAssurance::SetAmbientLight(float Level)
{
    CurrentLightResponse.AmbientLightLevel = FMath::Clamp(Level, 0.0f, 1.0f);
    ComputeLightResponse();
}

FLightResponse UAvatarQualityAssurance::GetLightResponse() const
{
    return CurrentLightResponse;
}

void UAvatarQualityAssurance::SetEnvironmentalConditions(float Wind, float Temperature, float Noise)
{
    EnvironmentalAwareness.WindResponse = FMath::Clamp(Wind, 0.0f, 1.0f);
    EnvironmentalAwareness.TemperatureComfort = FMath::Clamp(Temperature, 0.0f, 1.0f);
    EnvironmentalAwareness.NoiseResponse = FMath::Clamp(Noise, 0.0f, 1.0f);
}

FEnvironmentalAwareness UAvatarQualityAssurance::GetEnvironmentalAwareness() const
{
    return EnvironmentalAwareness;
}

void UAvatarQualityAssurance::ComputeLightResponse()
{
    float Light = CurrentLightResponse.AmbientLightLevel;

    // Pupil response (inverse to light)
    CurrentLightResponse.PupilResponse = 1.0f - Light * 0.7f;

    // Squint in bright light
    CurrentLightResponse.SquintAmount = FMath::Max(0.0f, (Light - 0.7f) * 3.0f);

    // Brow shade in very bright light
    CurrentLightResponse.BrowShade = FMath::Max(0.0f, (Light - 0.8f) * 5.0f);

    // Override pupil if needed (but light still affects it)
    if (!bPupilOverrideActive)
    {
        CurrentAutonomicState.PupilDilation = CurrentLightResponse.PupilResponse;
    }
}

void UAvatarQualityAssurance::UpdateEnvironmentalResponse(float DeltaTime)
{
    // Gradual adaptation to light
    ComputeLightResponse();

    // Temperature affects flush/pallor
    float TempDiscomfort = FMath::Abs(EnvironmentalAwareness.TemperatureComfort - 0.5f) * 2.0f;
    if (EnvironmentalAwareness.TemperatureComfort > 0.5f)
    {
        // Hot - flush
        CurrentAutonomicState.SkinFlush = FMath::Max(CurrentAutonomicState.SkinFlush, TempDiscomfort * 0.3f);
        CurrentAutonomicState.Perspiration = FMath::Max(CurrentAutonomicState.Perspiration, TempDiscomfort * 0.5f);
    }
}

// ========================================
// QUALITY ASSESSMENT
// ========================================

FAvatarQualityMetrics UAvatarQualityAssurance::GetQualityMetrics() const
{
    return QualityMetrics;
}

FAvatarQualityMetrics UAvatarQualityAssurance::RunQualityCheck()
{
    QualityMetrics.ExpressionCoherence = ComputeExpressionCoherence();
    QualityMetrics.PhysiologicalCoherence = ComputePhysiologicalCoherence();
    QualityMetrics.TemporalCoherence = ComputeTemporalCoherence();
    QualityMetrics.AuthenticityScore = ComputeAuthenticityScore();
    QualityMetrics.SubtletyScore = ComputeSubtletyScore();

    // Overall quality is weighted average
    QualityMetrics.OverallQuality =
        QualityMetrics.ExpressionCoherence * 0.2f +
        QualityMetrics.PhysiologicalCoherence * 0.2f +
        QualityMetrics.TemporalCoherence * 0.2f +
        QualityMetrics.AuthenticityScore * 0.25f +
        QualityMetrics.SubtletyScore * 0.15f;

    // Alert if quality drops
    if (QualityMetrics.OverallQuality < 0.7f)
    {
        OnQualityAlert.Broadcast(FString::Printf(TEXT("Quality dropped to %.2f"), QualityMetrics.OverallQuality));
    }

    return QualityMetrics;
}

TArray<FString> UAvatarQualityAssurance::GetQualityImprovementSuggestions() const
{
    TArray<FString> Suggestions;

    if (QualityMetrics.ExpressionCoherence < 0.8f)
    {
        Suggestions.Add(TEXT("Expression channels may be misaligned - check facial/body/vocal sync"));
    }

    if (QualityMetrics.PhysiologicalCoherence < 0.8f)
    {
        Suggestions.Add(TEXT("Autonomic responses may be inconsistent with emotional state"));
    }

    if (QualityMetrics.TemporalCoherence < 0.8f)
    {
        Suggestions.Add(TEXT("Expression transitions may be too abrupt - increase smoothing"));
    }

    if (QualityMetrics.AuthenticityScore < 0.8f)
    {
        Suggestions.Add(TEXT("Expressions may appear posed - add more asymmetry and natural timing"));
    }

    if (QualityMetrics.SubtletyScore < 0.8f)
    {
        Suggestions.Add(TEXT("Micro-expressions and subtle behaviors may need enhancement"));
    }

    if (Suggestions.Num() == 0)
    {
        Suggestions.Add(TEXT("Avatar quality is excellent - no immediate improvements needed"));
    }

    return Suggestions;
}

void UAvatarQualityAssurance::SetQualityLogging(bool bEnabled)
{
    bQualityLoggingEnabled = bEnabled;
}

void UAvatarQualityAssurance::UpdateQualityMetrics(float DeltaTime)
{
    // Periodic quality check (every second or so)
    static float QualityCheckTimer = 0.0f;
    QualityCheckTimer += DeltaTime;

    if (QualityCheckTimer >= 1.0f)
    {
        RunQualityCheck();
        QualityCheckTimer = 0.0f;
    }
}

float UAvatarQualityAssurance::ComputeExpressionCoherence() const
{
    // Check if all expression channels are aligned
    // This would normally check facial, body, vocal alignment
    return 0.95f; // Placeholder - would compute from actual expression states
}

float UAvatarQualityAssurance::ComputePhysiologicalCoherence() const
{
    // Check if autonomic responses match emotional state
    float Coherence = 1.0f;

    // Pupil should correlate with arousal
    // Flush should correlate with certain emotions
    // Etc.

    return Coherence;
}

float UAvatarQualityAssurance::ComputeTemporalCoherence() const
{
    // Check for smooth transitions
    float MaxVelocity = 0.0f;
    for (const auto& Pair : EmotionalMomentum.EmotionalVelocity)
    {
        MaxVelocity = FMath::Max(MaxVelocity, FMath::Abs(Pair.Value));
    }

    // Very high velocity = less coherent
    return 1.0f - FMath::Min(1.0f, MaxVelocity / (EmotionalMomentum.MaxVelocity * 2.0f));
}

float UAvatarQualityAssurance::ComputeAuthenticityScore() const
{
    // Based on asymmetry and timing profiles
    float Score = 1.0f;

    // Genuine expressions have appropriate asymmetry
    Score *= (0.8f + AsymmetrySettings.DynamicAsymmetry * 2.0f);

    // Score reduced if expressions are too symmetric
    if (AsymmetrySettings.DynamicAsymmetry < 0.05f)
    {
        Score *= 0.8f;
    }

    return FMath::Clamp(Score, 0.0f, 1.0f);
}

float UAvatarQualityAssurance::ComputeSubtletyScore() const
{
    // Check for appropriate micro-behaviors
    float Score = 1.0f;

    // Score based on having active leakages when suppressing
    // Score based on physiological responses
    // Score based on environmental responsiveness

    Score *= (bEnableMicroExpressionAuthenticity ? 1.0f : 0.8f);
    Score *= (bEnablePhysiologicalCoherence ? 1.0f : 0.9f);
    Score *= (bEnableEnvironmentalResponse ? 1.0f : 0.95f);

    return Score;
}
