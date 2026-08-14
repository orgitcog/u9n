#include "EmotionalAuraComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "Kismet/GameplayStatics.h"

UEmotionalAuraComponent::UEmotionalAuraComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;

    CurrentEmotion = EAuraEmotionType::Neutral;
    TargetEmotion = EAuraEmotionType::Neutral;
    AuraTransitionProgress = 1.0f;
    AuraTransitionDuration = 1.0f;

    bEmotionalPulseActive = false;
    EmotionalPulseTimer = 0.0f;
    EmotionalPulseDuration = 0.5f;
    EmotionalPulseIntensity = 2.0f;
    BaseIntensityBeforePulse = 1.0f;

    AuraMorphingTime = 0.0f;
}

void UEmotionalAuraComponent::BeginPlay()
{
    Super::BeginPlay();

    InitializeEmotionColorMappings();

    // Set initial aura data
    CurrentAuraData = EmotionColorMappings[EAuraEmotionType::Neutral];
    TargetAuraData = CurrentAuraData;

    // Initialize procedural particle arrays
    ProceduralParticlePositions.Reserve(100);
    ProceduralParticleVelocities.Reserve(100);
    ProceduralParticleLifetimes.Reserve(100);

    UE_LOG(LogTemp, Log, TEXT("EmotionalAuraComponent initialized"));
}

void UEmotionalAuraComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bAuraEnabled)
        return;

    UpdateAuraTransition(DeltaTime);
    UpdateParticleSystem(DeltaTime);
    UpdateEmotionalPulse(DeltaTime);
    GenerateProceduralParticles(DeltaTime);
    UpdateAuraMorphing(DeltaTime);
}

void UEmotionalAuraComponent::InitializeEmotionColorMappings()
{
    // Neutral aura
    FAuraVisualizationData Neutral;
    Neutral.PrimaryColor = FLinearColor(0.8f, 0.8f, 0.9f, 0.5f);
    Neutral.SecondaryColor = FLinearColor(0.6f, 0.6f, 0.7f, 0.3f);
    Neutral.Intensity = 0.5f;
    Neutral.Radius = 80.0f;
    Neutral.ParticleSize = 8.0f;
    Neutral.EmissionRate = 30.0f;
    Neutral.Turbulence = 0.3f;
    EmotionColorMappings.Add(EAuraEmotionType::Neutral, Neutral);

    // Joy aura (bright, warm colors - super-hot-girl aesthetic)
    FAuraVisualizationData Joy;
    Joy.PrimaryColor = FLinearColor(1.0f, 0.9f, 0.2f, 0.8f);
    Joy.SecondaryColor = FLinearColor(1.0f, 0.6f, 0.3f, 0.6f);
    Joy.Intensity = 1.2f;
    Joy.Radius = 120.0f;
    Joy.ParticleSize = 12.0f;
    Joy.EmissionRate = 80.0f;
    Joy.Turbulence = 0.6f;
    EmotionColorMappings.Add(EAuraEmotionType::Joy, Joy);

    // Sadness aura
    FAuraVisualizationData Sadness;
    Sadness.PrimaryColor = FLinearColor(0.2f, 0.3f, 0.6f, 0.6f);
    Sadness.SecondaryColor = FLinearColor(0.3f, 0.4f, 0.5f, 0.4f);
    Sadness.Intensity = 0.4f;
    Sadness.Radius = 60.0f;
    Sadness.ParticleSize = 6.0f;
    Sadness.EmissionRate = 20.0f;
    Sadness.Turbulence = 0.2f;
    EmotionColorMappings.Add(EAuraEmotionType::Sadness, Sadness);

    // Anger aura
    FAuraVisualizationData Anger;
    Anger.PrimaryColor = FLinearColor(1.0f, 0.2f, 0.1f, 0.9f);
    Anger.SecondaryColor = FLinearColor(0.8f, 0.3f, 0.0f, 0.7f);
    Anger.Intensity = 1.5f;
    Anger.Radius = 100.0f;
    Anger.ParticleSize = 14.0f;
    Anger.EmissionRate = 100.0f;
    Anger.Turbulence = 0.9f;
    EmotionColorMappings.Add(EAuraEmotionType::Anger, Anger);

    // Fear aura
    FAuraVisualizationData Fear;
    Fear.PrimaryColor = FLinearColor(0.5f, 0.4f, 0.6f, 0.7f);
    Fear.SecondaryColor = FLinearColor(0.3f, 0.2f, 0.4f, 0.5f);
    Fear.Intensity = 0.8f;
    Fear.Radius = 70.0f;
    Fear.ParticleSize = 7.0f;
    Fear.EmissionRate = 60.0f;
    Fear.Turbulence = 0.8f;
    EmotionColorMappings.Add(EAuraEmotionType::Fear, Fear);

    // Love aura (pink, romantic - super-hot-girl aesthetic)
    FAuraVisualizationData Love;
    Love.PrimaryColor = FLinearColor(1.0f, 0.4f, 0.7f, 0.9f);
    Love.SecondaryColor = FLinearColor(1.0f, 0.6f, 0.8f, 0.7f);
    Love.Intensity = 1.3f;
    Love.Radius = 110.0f;
    Love.ParticleSize = 13.0f;
    Love.EmissionRate = 70.0f;
    Love.Turbulence = 0.5f;
    EmotionColorMappings.Add(EAuraEmotionType::Love, Love);

    // Excitement aura (vibrant, energetic - super-hot-girl aesthetic)
    FAuraVisualizationData Excitement;
    Excitement.PrimaryColor = FLinearColor(1.0f, 0.3f, 0.9f, 1.0f);
    Excitement.SecondaryColor = FLinearColor(0.5f, 0.8f, 1.0f, 0.8f);
    Excitement.Intensity = 1.6f;
    Excitement.Radius = 130.0f;
    Excitement.ParticleSize = 15.0f;
    Excitement.EmissionRate = 120.0f;
    Excitement.Turbulence = 1.0f;
    EmotionColorMappings.Add(EAuraEmotionType::Excitement, Excitement);

    // Calm aura
    FAuraVisualizationData Calm;
    Calm.PrimaryColor = FLinearColor(0.4f, 0.8f, 0.9f, 0.6f);
    Calm.SecondaryColor = FLinearColor(0.6f, 0.9f, 0.8f, 0.4f);
    Calm.Intensity = 0.6f;
    Calm.Radius = 90.0f;
    Calm.ParticleSize = 9.0f;
    Calm.EmissionRate = 40.0f;
    Calm.Turbulence = 0.2f;
    EmotionColorMappings.Add(EAuraEmotionType::Calm, Calm);

    // Confidence aura (golden, radiant - super-hot-girl aesthetic)
    FAuraVisualizationData Confidence;
    Confidence.PrimaryColor = FLinearColor(1.0f, 0.8f, 0.3f, 1.0f);
    Confidence.SecondaryColor = FLinearColor(1.0f, 0.9f, 0.6f, 0.8f);
    Confidence.Intensity = 1.4f;
    Confidence.Radius = 115.0f;
    Confidence.ParticleSize = 14.0f;
    Confidence.EmissionRate = 90.0f;
    Confidence.Turbulence = 0.4f;
    EmotionColorMappings.Add(EAuraEmotionType::Confidence, Confidence);

    // Playfulness aura (colorful, dynamic - super-hot-girl aesthetic)
    FAuraVisualizationData Playfulness;
    Playfulness.PrimaryColor = FLinearColor(0.9f, 0.5f, 1.0f, 0.9f);
    Playfulness.SecondaryColor = FLinearColor(0.5f, 1.0f, 0.7f, 0.7f);
    Playfulness.Intensity = 1.1f;
    Playfulness.Radius = 105.0f;
    Playfulness.ParticleSize = 11.0f;
    Playfulness.EmissionRate = 75.0f;
    Playfulness.Turbulence = 0.7f;
    EmotionColorMappings.Add(EAuraEmotionType::Playfulness, Playfulness);

    UE_LOG(LogTemp, Log, TEXT("Emotion color mappings initialized: %d emotions"), EmotionColorMappings.Num());
}

void UEmotionalAuraComponent::SetEmotionalState(EAuraEmotionType NewEmotion, float TransitionTime)
{
    if (NewEmotion == CurrentEmotion && AuraTransitionProgress >= 1.0f)
        return;

    TargetEmotion = NewEmotion;
    AuraTransitionDuration = TransitionTime;
    AuraTransitionProgress = 0.0f;

    if (EmotionColorMappings.Contains(TargetEmotion))
    {
        TargetAuraData = EmotionColorMappings[TargetEmotion];
    }

    UE_LOG(LogTemp, Log, TEXT("Emotional aura transition: %d -> %d (%.2fs)"), 
        (int32)CurrentEmotion, (int32)TargetEmotion, TransitionTime);
}

void UEmotionalAuraComponent::SetAuraIntensity(float NewIntensity)
{
    CurrentAuraData.Intensity = FMath::Clamp(NewIntensity, 0.0f, 2.0f);
    TargetAuraData.Intensity = CurrentAuraData.Intensity;
}

void UEmotionalAuraComponent::SetAuraEnabled(bool bEnabled)
{
    bAuraEnabled = bEnabled;

    if (!bEnabled && ParticleSystemComponent)
    {
        ParticleSystemComponent->DeactivateSystem();
    }
    else if (bEnabled && ParticleSystemComponent)
    {
        ParticleSystemComponent->ActivateSystem();
    }

    UE_LOG(LogTemp, Log, TEXT("Aura visualization %s"), bEnabled ? TEXT("enabled") : TEXT("disabled"));
}

void UEmotionalAuraComponent::TriggerEmotionalPulse(float PulseIntensity, float Duration)
{
    if (bEmotionalPulseActive)
        return;

    BaseIntensityBeforePulse = CurrentAuraData.Intensity;
    EmotionalPulseIntensity = PulseIntensity;
    EmotionalPulseDuration = Duration;
    EmotionalPulseTimer = 0.0f;
    bEmotionalPulseActive = true;

    UE_LOG(LogTemp, Log, TEXT("Emotional pulse triggered: intensity %.2f for %.2fs"), PulseIntensity, Duration);
}

void UEmotionalAuraComponent::SetCustomAuraColors(const FLinearColor& Primary, const FLinearColor& Secondary)
{
    CurrentAuraData.PrimaryColor = Primary;
    CurrentAuraData.SecondaryColor = Secondary;
    TargetAuraData.PrimaryColor = Primary;
    TargetAuraData.SecondaryColor = Secondary;
}

void UEmotionalAuraComponent::UpdateAuraTransition(float DeltaTime)
{
    if (AuraTransitionProgress >= 1.0f)
        return;

    AuraTransitionProgress += DeltaTime / AuraTransitionDuration;
    AuraTransitionProgress = FMath::Clamp(AuraTransitionProgress, 0.0f, 1.0f);

    // Smooth interpolation
    float Alpha = FMath::SmoothStep(0.0f, 1.0f, AuraTransitionProgress);

    // Interpolate aura data
    FAuraVisualizationData BaseData = EmotionColorMappings.Contains(CurrentEmotion) ?
        EmotionColorMappings[CurrentEmotion] : FAuraVisualizationData();
    
    CurrentAuraData = InterpolateAuraData(BaseData, TargetAuraData, Alpha);

    if (AuraTransitionProgress >= 1.0f)
    {
        CurrentEmotion = TargetEmotion;
        UE_LOG(LogTemp, Log, TEXT("Emotional aura transition completed: %d"), (int32)CurrentEmotion);
    }
}

void UEmotionalAuraComponent::UpdateParticleSystem(float DeltaTime)
{
    // Update particle system parameters based on current aura data
    if (ParticleSystemComponent)
    {
        ParticleSystemComponent->SetColorParameter(TEXT("PrimaryColor"), CurrentAuraData.PrimaryColor);
        ParticleSystemComponent->SetColorParameter(TEXT("SecondaryColor"), CurrentAuraData.SecondaryColor);
        ParticleSystemComponent->SetFloatParameter(TEXT("Intensity"), CurrentAuraData.Intensity);
        ParticleSystemComponent->SetFloatParameter(TEXT("Radius"), CurrentAuraData.Radius);
        ParticleSystemComponent->SetFloatParameter(TEXT("EmissionRate"), CurrentAuraData.EmissionRate);
    }
}

void UEmotionalAuraComponent::UpdateEmotionalPulse(float DeltaTime)
{
    if (!bEmotionalPulseActive)
        return;

    EmotionalPulseTimer += DeltaTime;

    float Progress = EmotionalPulseTimer / EmotionalPulseDuration;

    if (Progress >= 1.0f)
    {
        // End pulse
        CurrentAuraData.Intensity = BaseIntensityBeforePulse;
        bEmotionalPulseActive = false;
        EmotionalPulseTimer = 0.0f;
    }
    else
    {
        // Pulse curve (smooth in and out)
        float PulseCurve = FMath::Sin(Progress * PI);
        CurrentAuraData.Intensity = BaseIntensityBeforePulse + (EmotionalPulseIntensity - BaseIntensityBeforePulse) * PulseCurve;
    }
}

void UEmotionalAuraComponent::GenerateProceduralParticles(float DeltaTime)
{
    AActor* Owner = GetOwner();
    if (!Owner)
        return;

    FVector OwnerLocation = Owner->GetActorLocation();

    // Spawn new particles based on emission rate
    int32 ParticlesToSpawn = FMath::RoundToInt(CurrentAuraData.EmissionRate * DeltaTime);

    for (int32 i = 0; i < ParticlesToSpawn; i++)
    {
        // Random position within aura radius
        FVector RandomOffset = FVector(
            FMath::FRandRange(-1.0f, 1.0f),
            FMath::FRandRange(-1.0f, 1.0f),
            FMath::FRandRange(-0.5f, 1.0f)
        ).GetSafeNormal() * FMath::FRandRange(0.0f, CurrentAuraData.Radius);

        FVector ParticlePosition = OwnerLocation + RandomOffset;
        
        // Random velocity with turbulence
        FVector RandomVelocity = FVector(
            FMath::FRandRange(-1.0f, 1.0f),
            FMath::FRandRange(-1.0f, 1.0f),
            FMath::FRandRange(0.0f, 1.0f)
        ) * CurrentAuraData.Turbulence * 50.0f;

        ProceduralParticlePositions.Add(ParticlePosition);
        ProceduralParticleVelocities.Add(RandomVelocity);
        ProceduralParticleLifetimes.Add(FMath::FRandRange(1.0f, 3.0f));
    }

    // Update existing particles
    for (int32 i = ProceduralParticlePositions.Num() - 1; i >= 0; i--)
    {
        ProceduralParticleLifetimes[i] -= DeltaTime;

        if (ProceduralParticleLifetimes[i] <= 0.0f)
        {
            // Remove dead particle
            ProceduralParticlePositions.RemoveAt(i);
            ProceduralParticleVelocities.RemoveAt(i);
            ProceduralParticleLifetimes.RemoveAt(i);
        }
        else
        {
            // Update particle position
            ProceduralParticlePositions[i] += ProceduralParticleVelocities[i] * DeltaTime;

            // Apply gravity and turbulence
            ProceduralParticleVelocities[i].Z += 20.0f * DeltaTime; // Upward drift
            ProceduralParticleVelocities[i] += FVector(
                FMath::FRandRange(-1.0f, 1.0f),
                FMath::FRandRange(-1.0f, 1.0f),
                FMath::FRandRange(-0.5f, 0.5f)
            ) * CurrentAuraData.Turbulence * 10.0f * DeltaTime;
        }
    }

    // Limit particle count
    if (ProceduralParticlePositions.Num() > 500)
    {
        int32 ExcessParticles = ProceduralParticlePositions.Num() - 500;
        ProceduralParticlePositions.RemoveAt(0, ExcessParticles);
        ProceduralParticleVelocities.RemoveAt(0, ExcessParticles);
        ProceduralParticleLifetimes.RemoveAt(0, ExcessParticles);
    }
}

void UEmotionalAuraComponent::UpdateAuraMorphing(float DeltaTime)
{
    AuraMorphingTime += DeltaTime;

    // Apply subtle morphing to aura radius and turbulence
    float MorphFactor = FMath::Sin(AuraMorphingTime * 0.5f) * 0.1f;
    
    CurrentAuraData.Radius *= (1.0f + MorphFactor);
    CurrentAuraData.Turbulence *= (1.0f + MorphFactor * 0.5f);
}

FAuraVisualizationData UEmotionalAuraComponent::InterpolateAuraData(const FAuraVisualizationData& From, const FAuraVisualizationData& To, float Alpha)
{
    FAuraVisualizationData Result;

    Result.PrimaryColor = FLinearColor::LerpUsingHSV(From.PrimaryColor, To.PrimaryColor, Alpha);
    Result.SecondaryColor = FLinearColor::LerpUsingHSV(From.SecondaryColor, To.SecondaryColor, Alpha);
    Result.Intensity = FMath::Lerp(From.Intensity, To.Intensity, Alpha);
    Result.Radius = FMath::Lerp(From.Radius, To.Radius, Alpha);
    Result.ParticleSize = FMath::Lerp(From.ParticleSize, To.ParticleSize, Alpha);
    Result.EmissionRate = FMath::Lerp(From.EmissionRate, To.EmissionRate, Alpha);
    Result.Turbulence = FMath::Lerp(From.Turbulence, To.Turbulence, Alpha);

    return Result;
}
