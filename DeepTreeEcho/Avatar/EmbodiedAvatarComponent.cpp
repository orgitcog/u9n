// EmbodiedAvatarComponent.cpp
// Implementation of 4E Embodied Cognition Avatar Component

#include "EmbodiedAvatarComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

UEmbodiedAvatarComponent::UEmbodiedAvatarComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;

    // Initialize default aura state
    AuraState.PrimaryColor = FLinearColor(0.3f, 0.5f, 0.8f, 1.0f);
    AuraState.SecondaryColor = FLinearColor(0.5f, 0.3f, 0.7f, 1.0f);
    AuraState.Intensity = 0.5f;
    AuraState.PulseRate = 1.0f;
    AuraState.Radius = 100.0f;
}

void UEmbodiedAvatarComponent::BeginPlay()
{
    Super::BeginPlay();

    // Initialize 4E states with default values
    EmbodiedState.EnergyLevel = 0.5f;
    EmbodiedState.ArousalLevel = 0.5f;
    EmbodiedState.Valence = 0.0f;
    EmbodiedState.MotorReadiness = 0.5f;

    EmbeddedState.CouplingStrength = 0.5f;
    EmbeddedState.AmbientColor = FLinearColor::White;

    EnactedState.EngagementLevel = 0.5f;
    EnactedState.ActionPerceptionCoupling = 0.5f;

    ExtendedState.IntegrationLevel = 0.5f;
}

void UEmbodiedAvatarComponent::TickComponent(float DeltaTime, ELevelTick TickType, 
                                              FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Update all 4E states
    Update4EStates(DeltaTime);

    // Blend facial expression
    BlendExpression(DeltaTime);

    // Update breathing animation
    UpdateBreathing(DeltaTime);

    // Update aura from cognitive state
    UpdateAuraFromCognitiveState();

    // Update gaze direction
    if (bGazeTrackingEnabled)
    {
        UpdateGazeDirection();
    }

    // Update hair dynamics
    UpdateHairDynamics();

    // Check for 4E score change
    float Current4EScore = Compute4EIntegrationScore();
    if (FMath::Abs(Current4EScore - Previous4EScore) > 0.01f)
    {
        On4EScoreChanged.Broadcast(Current4EScore);
        Previous4EScore = Current4EScore;
    }
}

void UEmbodiedAvatarComponent::Update4EStates(float DeltaTime)
{
    // ========================================
    // EMBODIED STATE UPDATE
    // ========================================

    // Update proprioception from actor transform
    AActor* Owner = GetOwner();
    if (Owner)
    {
        FVector Location = Owner->GetActorLocation();
        FRotator Rotation = Owner->GetActorRotation();

        EmbodiedState.PositionX = Location.X;
        EmbodiedState.PositionY = Location.Y;
        EmbodiedState.PositionZ = Location.Z;
        EmbodiedState.RotationPitch = Rotation.Pitch;
        EmbodiedState.RotationYaw = Rotation.Yaw;
        EmbodiedState.RotationRoll = Rotation.Roll;
    }

    // Interoceptive state naturally tends toward homeostasis
    EmbodiedState.EnergyLevel = FMath::Lerp(EmbodiedState.EnergyLevel, 0.5f, DeltaTime * 0.1f);
    EmbodiedState.ArousalLevel = FMath::Lerp(EmbodiedState.ArousalLevel, 0.5f, DeltaTime * 0.05f);
    EmbodiedState.TensionLevel = FMath::Lerp(EmbodiedState.TensionLevel, 0.3f, DeltaTime * 0.1f);

    // ========================================
    // EMBEDDED STATE UPDATE
    // ========================================

    // Update environment coupling based on affordances
    float TargetCoupling = EmbeddedState.Affordances.Num() > 0 ? 
        FMath::Min(0.3f + EmbeddedState.Affordances.Num() * 0.1f, 1.0f) : 0.3f;
    EmbeddedState.CouplingStrength = FMath::Lerp(EmbeddedState.CouplingStrength, TargetCoupling, DeltaTime * 0.5f);

    // Decay salience map
    for (auto& Salience : EmbeddedState.SalienceMap)
    {
        Salience.Value *= (1.0f - DeltaTime * 0.1f);
    }

    // ========================================
    // ENACTED STATE UPDATE
    // ========================================

    // Update engagement based on action-perception coupling
    EnactedState.EngagementLevel = FMath::Lerp(
        EnactedState.EngagementLevel,
        EnactedState.ActionPerceptionCoupling,
        DeltaTime * 0.3f);

    // Decay prediction errors
    for (auto& Error : EnactedState.PredictionErrors)
    {
        Error.Value *= (1.0f - DeltaTime * 0.2f);
    }

    // ========================================
    // EXTENDED STATE UPDATE
    // ========================================

    // Update integration level based on active tools and scaffolding
    float TargetIntegration = 0.3f;
    TargetIntegration += ExtendedState.ActiveTools.Num() * 0.1f;
    TargetIntegration += ExtendedState.Scaffolding.Num() * 0.05f;
    TargetIntegration = FMath::Min(TargetIntegration, 1.0f);

    ExtendedState.IntegrationLevel = FMath::Lerp(
        ExtendedState.IntegrationLevel,
        TargetIntegration,
        DeltaTime * 0.2f);
}

float UEmbodiedAvatarComponent::Compute4EIntegrationScore() const
{
    float Embodied = GetEmbodiedScore();
    float Embedded = GetEmbeddedScore();
    float Enacted = GetEnactedScore();
    float Extended = GetExtendedScore();

    // Weighted geometric mean for integration
    return FMath::Pow(Embodied * Embedded * Enacted * Extended, 0.25f);
}

float UEmbodiedAvatarComponent::GetEmbodiedScore() const
{
    // Combine interoceptive and motor readiness
    float InteroceptiveBalance = 1.0f - FMath::Abs(EmbodiedState.EnergyLevel - 0.5f) * 2.0f;
    float ArousalBalance = 1.0f - FMath::Abs(EmbodiedState.ArousalLevel - 0.5f) * 2.0f;
    
    return (InteroceptiveBalance + ArousalBalance + EmbodiedState.MotorReadiness) / 3.0f;
}

float UEmbodiedAvatarComponent::GetEmbeddedScore() const
{
    return EmbeddedState.CouplingStrength;
}

float UEmbodiedAvatarComponent::GetEnactedScore() const
{
    return EnactedState.EngagementLevel;
}

float UEmbodiedAvatarComponent::GetExtendedScore() const
{
    return ExtendedState.IntegrationLevel;
}

void UEmbodiedAvatarComponent::BlendExpression(float DeltaTime)
{
    CurrentExpression = LerpExpression(CurrentExpression, TargetExpression, 
                                        DeltaTime * ExpressionBlendSpeed);
}

FFacialExpressionBlend UEmbodiedAvatarComponent::LerpExpression(
    const FFacialExpressionBlend& A, 
    const FFacialExpressionBlend& B, 
    float Alpha) const
{
    FFacialExpressionBlend Result;

    // Lerp all emotion values
    Result.Joy = FMath::Lerp(A.Joy, B.Joy, Alpha);
    Result.Sadness = FMath::Lerp(A.Sadness, B.Sadness, Alpha);
    Result.Anger = FMath::Lerp(A.Anger, B.Anger, Alpha);
    Result.Fear = FMath::Lerp(A.Fear, B.Fear, Alpha);
    Result.Surprise = FMath::Lerp(A.Surprise, B.Surprise, Alpha);
    Result.Disgust = FMath::Lerp(A.Disgust, B.Disgust, Alpha);

    // Lerp cognitive expressions
    Result.Concentration = FMath::Lerp(A.Concentration, B.Concentration, Alpha);
    Result.Confusion = FMath::Lerp(A.Confusion, B.Confusion, Alpha);
    Result.Curiosity = FMath::Lerp(A.Curiosity, B.Curiosity, Alpha);
    Result.Contemplation = FMath::Lerp(A.Contemplation, B.Contemplation, Alpha);
    Result.Realization = FMath::Lerp(A.Realization, B.Realization, Alpha);

    // Lerp micro-expressions
    Result.BrowRaise = FMath::Lerp(A.BrowRaise, B.BrowRaise, Alpha);
    Result.BrowFurrow = FMath::Lerp(A.BrowFurrow, B.BrowFurrow, Alpha);
    Result.EyeWiden = FMath::Lerp(A.EyeWiden, B.EyeWiden, Alpha);
    Result.EyeSquint = FMath::Lerp(A.EyeSquint, B.EyeSquint, Alpha);
    Result.NoseWrinkle = FMath::Lerp(A.NoseWrinkle, B.NoseWrinkle, Alpha);
    Result.LipPurse = FMath::Lerp(A.LipPurse, B.LipPurse, Alpha);
    Result.SmileLeft = FMath::Lerp(A.SmileLeft, B.SmileLeft, Alpha);
    Result.SmileRight = FMath::Lerp(A.SmileRight, B.SmileRight, Alpha);

    // Lerp visemes
    Result.Viseme_AA = FMath::Lerp(A.Viseme_AA, B.Viseme_AA, Alpha);
    Result.Viseme_EE = FMath::Lerp(A.Viseme_EE, B.Viseme_EE, Alpha);
    Result.Viseme_OO = FMath::Lerp(A.Viseme_OO, B.Viseme_OO, Alpha);
    Result.Viseme_CH = FMath::Lerp(A.Viseme_CH, B.Viseme_CH, Alpha);

    return Result;
}

void UEmbodiedAvatarComponent::SetEmotionIntensity(const FString& EmotionName, float Intensity)
{
    ApplyEmotionToExpression(EmotionName, FMath::Clamp(Intensity, 0.0f, 1.0f));
}

void UEmbodiedAvatarComponent::ApplyEmotionToExpression(const FString& EmotionName, float Intensity)
{
    if (EmotionName == "Joy")
    {
        TargetExpression.Joy = Intensity;
        TargetExpression.SmileLeft = Intensity * 0.8f;
        TargetExpression.SmileRight = Intensity * 0.8f;
        TargetExpression.EyeSquint = Intensity * 0.3f;
    }
    else if (EmotionName == "Sadness")
    {
        TargetExpression.Sadness = Intensity;
        TargetExpression.BrowFurrow = Intensity * 0.5f;
        TargetExpression.LipPurse = Intensity * 0.3f;
    }
    else if (EmotionName == "Anger")
    {
        TargetExpression.Anger = Intensity;
        TargetExpression.BrowFurrow = Intensity * 0.8f;
        TargetExpression.EyeSquint = Intensity * 0.4f;
        TargetExpression.NoseWrinkle = Intensity * 0.3f;
    }
    else if (EmotionName == "Fear")
    {
        TargetExpression.Fear = Intensity;
        TargetExpression.BrowRaise = Intensity * 0.7f;
        TargetExpression.EyeWiden = Intensity * 0.8f;
    }
    else if (EmotionName == "Surprise")
    {
        TargetExpression.Surprise = Intensity;
        TargetExpression.BrowRaise = Intensity * 0.9f;
        TargetExpression.EyeWiden = Intensity * 0.9f;
    }
    else if (EmotionName == "Disgust")
    {
        TargetExpression.Disgust = Intensity;
        TargetExpression.NoseWrinkle = Intensity * 0.7f;
        TargetExpression.LipPurse = Intensity * 0.5f;
    }
    else if (EmotionName == "Concentration")
    {
        TargetExpression.Concentration = Intensity;
        TargetExpression.BrowFurrow = Intensity * 0.4f;
        TargetExpression.EyeSquint = Intensity * 0.2f;
    }
    else if (EmotionName == "Curiosity")
    {
        TargetExpression.Curiosity = Intensity;
        TargetExpression.BrowRaise = Intensity * 0.5f;
        TargetExpression.EyeWiden = Intensity * 0.3f;
    }
    else if (EmotionName == "Contemplation")
    {
        TargetExpression.Contemplation = Intensity;
        TargetExpression.EyeSquint = Intensity * 0.2f;
    }
    else if (EmotionName == "Realization")
    {
        TargetExpression.Realization = Intensity;
        TargetExpression.BrowRaise = Intensity * 0.6f;
        TargetExpression.EyeWiden = Intensity * 0.5f;
    }

    OnExpressionChanged.Broadcast(TargetExpression);
}

void UEmbodiedAvatarComponent::ApplyExpressionToMesh(USkeletalMeshComponent* Mesh)
{
    if (!Mesh)
    {
        return;
    }

    // Apply morph targets
    Mesh->SetMorphTarget(FName("Joy"), CurrentExpression.Joy);
    Mesh->SetMorphTarget(FName("Sadness"), CurrentExpression.Sadness);
    Mesh->SetMorphTarget(FName("Anger"), CurrentExpression.Anger);
    Mesh->SetMorphTarget(FName("Fear"), CurrentExpression.Fear);
    Mesh->SetMorphTarget(FName("Surprise"), CurrentExpression.Surprise);
    Mesh->SetMorphTarget(FName("Disgust"), CurrentExpression.Disgust);

    Mesh->SetMorphTarget(FName("BrowRaise"), CurrentExpression.BrowRaise);
    Mesh->SetMorphTarget(FName("BrowFurrow"), CurrentExpression.BrowFurrow);
    Mesh->SetMorphTarget(FName("EyeWiden"), CurrentExpression.EyeWiden);
    Mesh->SetMorphTarget(FName("EyeSquint"), CurrentExpression.EyeSquint);
    Mesh->SetMorphTarget(FName("NoseWrinkle"), CurrentExpression.NoseWrinkle);
    Mesh->SetMorphTarget(FName("LipPurse"), CurrentExpression.LipPurse);
    Mesh->SetMorphTarget(FName("SmileLeft"), CurrentExpression.SmileLeft);
    Mesh->SetMorphTarget(FName("SmileRight"), CurrentExpression.SmileRight);

    Mesh->SetMorphTarget(FName("Viseme_AA"), CurrentExpression.Viseme_AA);
    Mesh->SetMorphTarget(FName("Viseme_EE"), CurrentExpression.Viseme_EE);
    Mesh->SetMorphTarget(FName("Viseme_OO"), CurrentExpression.Viseme_OO);
    Mesh->SetMorphTarget(FName("Viseme_CH"), CurrentExpression.Viseme_CH);
}

void UEmbodiedAvatarComponent::UpdateAuraFromCognitiveState()
{
    // Compute aura color from 4E integration
    AuraState.PrimaryColor = ComputeAuraColor();

    // Secondary color is complementary
    AuraState.SecondaryColor = FLinearColor(
        1.0f - AuraState.PrimaryColor.R * 0.5f,
        1.0f - AuraState.PrimaryColor.G * 0.5f,
        1.0f - AuraState.PrimaryColor.B * 0.5f,
        AuraState.PrimaryColor.A);

    // Intensity from overall 4E integration
    AuraState.Intensity = Compute4EIntegrationScore();

    // Pulse rate from arousal level
    AuraState.PulseRate = 0.5f + EmbodiedState.ArousalLevel * 2.0f;

    // Particle rate from energy level
    AuraState.ParticleRate = 5.0f + EmbodiedState.EnergyLevel * 20.0f;

    // Radius from extension level
    AuraState.Radius = 50.0f + ExtendedState.IntegrationLevel * 100.0f;

    // Resonance pattern from cognitive cycle step
    AuraState.ResonancePattern = CurrentCycleStep;
}

FLinearColor UEmbodiedAvatarComponent::ComputeAuraColor() const
{
    // Base color from 4E dimensions
    float Hue = 0.6f; // Default blue

    // Embodied: shifts toward red/orange (warm)
    Hue = FMath::Lerp(Hue, 0.05f, GetEmbodiedScore() * 0.3f);

    // Embedded: shifts toward green
    Hue = FMath::Lerp(Hue, 0.33f, GetEmbeddedScore() * 0.2f);

    // Enacted: shifts toward yellow
    Hue = FMath::Lerp(Hue, 0.15f, GetEnactedScore() * 0.2f);

    // Extended: shifts toward purple
    Hue = FMath::Lerp(Hue, 0.75f, GetExtendedScore() * 0.2f);

    // Valence affects saturation
    float Saturation = 0.5f + EmbodiedState.Valence * 0.3f;

    // Energy affects value
    float Value = 0.5f + EmbodiedState.EnergyLevel * 0.5f;

    // Convert HSV to RGB
    return FLinearColor::MakeFromHSV8(
        (uint8)(Hue * 255.0f),
        (uint8)(FMath::Clamp(Saturation, 0.0f, 1.0f) * 255.0f),
        (uint8)(FMath::Clamp(Value, 0.0f, 1.0f) * 255.0f));
}

void UEmbodiedAvatarComponent::ApplyAuraToNiagara(UNiagaraComponent* NiagaraComp)
{
    if (!NiagaraComp)
    {
        return;
    }

    // Set Niagara parameters
    NiagaraComp->SetColorParameter(FName("PrimaryColor"), AuraState.PrimaryColor);
    NiagaraComp->SetColorParameter(FName("SecondaryColor"), AuraState.SecondaryColor);
    NiagaraComp->SetFloatParameter(FName("Intensity"), AuraState.Intensity);
    NiagaraComp->SetFloatParameter(FName("PulseRate"), AuraState.PulseRate);
    NiagaraComp->SetFloatParameter(FName("ParticleRate"), AuraState.ParticleRate);
    NiagaraComp->SetFloatParameter(FName("Radius"), AuraState.Radius);
    NiagaraComp->SetIntParameter(FName("ResonancePattern"), AuraState.ResonancePattern);
}

void UEmbodiedAvatarComponent::UpdateGazeDirection()
{
    // Gaze direction is updated based on salience map
    // Find highest salience target
    FString HighestSalienceTarget;
    float HighestSalience = 0.0f;

    for (const auto& Salience : EmbeddedState.SalienceMap)
    {
        if (Salience.Value > HighestSalience)
        {
            HighestSalience = Salience.Value;
            HighestSalienceTarget = Salience.Key;
        }
    }

    // Eye glow intensity from cognitive activation
    EyeGlowIntensity = FMath::Lerp(EyeGlowIntensity, 
                                    0.3f + HighestSalience * 0.7f, 
                                    0.1f);
}

FRotator UEmbodiedAvatarComponent::GetEyeRotation() const
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return FRotator::ZeroRotator;
    }

    FVector EyeLocation = Owner->GetActorLocation() + FVector(0, 0, 160); // Approximate eye height
    FVector Direction = (GazeTarget - EyeLocation).GetSafeNormal();

    return Direction.Rotation();
}

void UEmbodiedAvatarComponent::UpdateBreathing(float DeltaTime)
{
    // Advance breath phase
    float BreathCycleDuration = 60.0f / BreathingRate;
    BreathTimeAccumulator += DeltaTime;

    if (BreathTimeAccumulator >= BreathCycleDuration)
    {
        BreathTimeAccumulator -= BreathCycleDuration;
    }

    // Compute phase (0-1)
    BreathPhase = BreathTimeAccumulator / BreathCycleDuration;

    // Breathing rate affected by arousal
    BreathingRate = 12.0f + (EmbodiedState.ArousalLevel - 0.5f) * 12.0f;
    BreathingRate = FMath::Clamp(BreathingRate, 6.0f, 30.0f);

    // Breathing depth affected by tension
    BreathingDepth = 0.5f + (1.0f - EmbodiedState.TensionLevel) * 0.5f;
}

float UEmbodiedAvatarComponent::GetChestExpansion() const
{
    // Sinusoidal breathing pattern
    float Phase = BreathPhase * 2.0f * PI;
    float Expansion = (FMath::Sin(Phase - PI * 0.5f) + 1.0f) * 0.5f;

    return Expansion * BreathingDepth;
}

void UEmbodiedAvatarComponent::UpdateHairDynamics()
{
    // Hair dynamics affected by motor readiness and arousal
    float BaseMultiplier = 1.0f;

    // More dynamic when aroused
    BaseMultiplier += EmbodiedState.ArousalLevel * 0.5f;

    // More dynamic when motor ready
    BaseMultiplier += EmbodiedState.MotorReadiness * 0.3f;

    // Less dynamic when tense
    BaseMultiplier -= EmbodiedState.TensionLevel * 0.3f;

    HairDynamicsMultiplier = FMath::Clamp(BaseMultiplier, 0.5f, 2.0f);
}

void UEmbodiedAvatarComponent::SetCognitiveStep(int32 Step)
{
    CurrentCycleStep = Step % 12;

    // Update expression based on cognitive step
    switch (CurrentCycleStep)
    {
        case 0: // Perceive
            TargetExpression.Curiosity = 0.3f;
            TargetExpression.EyeWiden = 0.2f;
            break;
        case 1: // Orient
            TargetExpression.Concentration = 0.4f;
            break;
        case 2: // Reflect
            TargetExpression.Contemplation = 0.5f;
            break;
        case 3: // Integrate
            TargetExpression.Concentration = 0.3f;
            break;
        case 4: // Decide
            TargetExpression.Concentration = 0.5f;
            TargetExpression.BrowFurrow = 0.2f;
            break;
        case 5: // Simulate
            TargetExpression.Contemplation = 0.4f;
            TargetExpression.EyeSquint = 0.2f;
            break;
        case 6: // Act
            TargetExpression.Concentration = 0.3f;
            EmbodiedState.MotorReadiness = 0.8f;
            break;
        case 7: // Observe
            TargetExpression.Curiosity = 0.4f;
            break;
        case 8: // Learn
            TargetExpression.Realization = 0.3f;
            break;
        case 9: // Consolidate
            TargetExpression.Contemplation = 0.3f;
            break;
        case 10: // Anticipate
            TargetExpression.Curiosity = 0.2f;
            TargetExpression.BrowRaise = 0.1f;
            break;
        case 11: // Transcend
            TargetExpression.Realization = 0.4f;
            TargetExpression.Joy = 0.2f;
            break;
    }
}

void UEmbodiedAvatarComponent::GetCognitiveStepVisuals(
    FLinearColor& OutColor, 
    float& OutIntensity, 
    float& OutPulseRate) const
{
    // Color mapping for each step
    static const FLinearColor StepColors[12] = {
        FLinearColor(0.2f, 0.4f, 0.8f, 1.0f),  // Perceive - Blue
        FLinearColor(0.3f, 0.5f, 0.7f, 1.0f),  // Orient - Light Blue
        FLinearColor(0.5f, 0.3f, 0.7f, 1.0f),  // Reflect - Purple
        FLinearColor(0.4f, 0.6f, 0.4f, 1.0f),  // Integrate - Green
        FLinearColor(0.6f, 0.4f, 0.2f, 1.0f),  // Decide - Orange
        FLinearColor(0.5f, 0.5f, 0.5f, 1.0f),  // Simulate - Gray
        FLinearColor(0.8f, 0.3f, 0.2f, 1.0f),  // Act - Red
        FLinearColor(0.3f, 0.6f, 0.6f, 1.0f),  // Observe - Cyan
        FLinearColor(0.7f, 0.7f, 0.2f, 1.0f),  // Learn - Yellow
        FLinearColor(0.4f, 0.4f, 0.6f, 1.0f),  // Consolidate - Slate
        FLinearColor(0.6f, 0.5f, 0.3f, 1.0f),  // Anticipate - Tan
        FLinearColor(0.8f, 0.6f, 0.2f, 1.0f)   // Transcend - Gold
    };

    OutColor = StepColors[CurrentCycleStep];
    OutIntensity = 0.5f + (CurrentCycleStep % 4) * 0.1f;
    OutPulseRate = 1.0f + (CurrentCycleStep / 4) * 0.5f;
}
