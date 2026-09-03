#include "Personality/PersonalityTraitSystem.h"
#include "Avatar/AvatarAnimInstance.h"
#include "Avatar/AvatarMaterialManager.h"
#include "Cognitive/CognitiveSystem.h"

UPersonalityTraitSystem::UPersonalityTraitSystem()
{
    PrimaryComponentTick.bCanEverTick = true;

    // Initialize personality traits with default values
    Confidence = 0.7f;
    Flirtiness = 0.6f;
    ChaosFactor = 0.2f;
    EmotionalVolatility = 0.4f;
    IntellectualCuriosity = 0.8f;
}

void UPersonalityTraitSystem::BeginPlay()
{
    Super::BeginPlay();

    // Get references to other components
    AvatarAnimInstance = Cast<UAvatarAnimInstance>(GetOwner()->FindComponentByClass<USkeletalMeshComponent>()->GetAnimInstance());
    AvatarMaterialManager = GetOwner()->FindComponentByClass<UAvatarMaterialManager>();
    CognitiveSystem = GetOwner()->FindComponentByClass<UCognitiveSystem>();
}

void UPersonalityTraitSystem::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Update personality traits based on cognitive state
    UpdatePersonalityFromCognitiveState(DeltaTime);

    // Apply personality effects to animation and materials
    ApplyPersonalityToAvatar();
}

void UPersonalityTraitSystem::UpdatePersonalityFromCognitiveState(float DeltaTime)
{
    if (!CognitiveSystem)
    {
        return;
    }

    // Example: Increase confidence with successful task completion
    if (CognitiveSystem->GetLastTaskSuccess())
    {
        Confidence = FMath::FInterpTo(Confidence, 0.9f, DeltaTime, 0.5f);
    }
    else
    {
        Confidence = FMath::FInterpTo(Confidence, 0.6f, DeltaTime, 0.5f);
    }

    // Example: Increase flirtiness with positive social interaction
    if (CognitiveSystem->GetLastSocialInteractionValence() > 0.5f)
    {
        Flirtiness = FMath::FInterpTo(Flirtiness, 0.8f, DeltaTime, 0.5f);
    }
    else
    {
        Flirtiness = FMath::FInterpTo(Flirtiness, 0.5f, DeltaTime, 0.5f);
    }

    // Example: Increase chaos factor with high cognitive load
    float CognitiveLoad = CognitiveSystem->GetCognitiveLoad();
    ChaosFactor = FMath::FInterpTo(ChaosFactor, CognitiveLoad * 0.8f, DeltaTime, 1.0f);

    // Example: Emotional volatility based on emotional state changes
    float EmotionalChangeRate = CognitiveSystem->GetEmotionalChangeRate();
    EmotionalVolatility = FMath::FInterpTo(EmotionalVolatility, EmotionalChangeRate * 0.5f, DeltaTime, 1.0f);

    // Example: Intellectual curiosity based on new information
    if (CognitiveSystem->HasNewInformation())
    {
        IntellectualCuriosity = FMath::FInterpTo(IntellectualCuriosity, 0.9f, DeltaTime, 0.5f);
    }
    else
    {
        IntellectualCuriosity = FMath::FInterpTo(IntellectualCuriosity, 0.7f, DeltaTime, 0.5f);
    }
}

void UPersonalityTraitSystem::ApplyPersonalityToAvatar()
{
    if (AvatarAnimInstance)
    {
        // Apply personality traits to animation blueprint
        AvatarAnimInstance->SetPersonalityTraits(Confidence, Flirtiness, ChaosFactor);

        // Example: Confidence affects posture
        AvatarAnimInstance->SetPostureWeight(Confidence * 0.5f + 0.5f);

        // Example: Flirtiness affects idle gestures
        AvatarAnimInstance->SetGestureFrequency(Flirtiness * 0.5f);

        // Example: Chaos factor affects animation glitches
        AvatarAnimInstance->SetAnimationGlitchIntensity(ChaosFactor * 0.3f);
    }

    if (AvatarMaterialManager)
    {
        // Apply personality traits to materials

        // Example: Confidence affects eye sparkle
        AvatarMaterialManager->SetScalarParameter(EAvatarMaterialSlot::Eyes, TEXT("SparkleIntensity"), Confidence * 0.5f + 0.5f);

        // Example: Flirtiness affects blush
        AvatarMaterialManager->SetScalarParameter(EAvatarMaterialSlot::Skin, TEXT("BlushIntensity"), Flirtiness * 0.3f);

        // Example: Chaos factor affects material glitch
        AvatarMaterialManager->SetScalarParameter(EAvatarMaterialSlot::Skin, TEXT("GlitchIntensity"), ChaosFactor * 0.5f);
        AvatarMaterialManager->SetScalarParameter(EAvatarMaterialSlot::Hair, TEXT("GlitchIntensity"), ChaosFactor * 0.5f);

        // Example: Intellectual curiosity affects eye glow
        AvatarMaterialManager->SetScalarParameter(EAvatarMaterialSlot::Eyes, TEXT("EmotionIntensity"), IntellectualCuriosity * 0.2f);
        AvatarMaterialManager->SetVectorParameter(EAvatarMaterialSlot::Eyes, TEXT("EmotionTint"), FLinearColor(0.8f, 0.9f, 1.0f));
    }
}

void UPersonalityTraitSystem::SetPersonalityTraits(float InConfidence, float InFlirtiness, float InChaosFactor)
{
    Confidence = FMath::Clamp(InConfidence, 0.0f, 1.0f);
    Flirtiness = FMath::Clamp(InFlirtiness, 0.0f, 1.0f);
    ChaosFactor = FMath::Clamp(InChaosFactor, 0.0f, 1.0f);
}

float UPersonalityTraitSystem::GetConfidence() const
{
    return Confidence;
}

float UPersonalityTraitSystem::GetFlirtiness() const
{
    return Flirtiness;
}

float UPersonalityTraitSystem::GetChaosFactor() const
{
    return ChaosFactor;
}

float UPersonalityTraitSystem::GetEmotionalVolatility() const
{
    return EmotionalVolatility;
}

float UPersonalityTraitSystem::GetIntellectualCuriosity() const
{
    return IntellectualCuriosity;
}
