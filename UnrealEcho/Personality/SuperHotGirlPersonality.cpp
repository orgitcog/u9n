#include "SuperHotGirlPersonality.h"
#include "Avatar3DComponent.h"
#include "FacialAnimationSystem.h"
#include "GestureSystem.h"
#include "EmotionalAuraComponent.h"
#include "CognitiveVisualizationComponent.h"
#include "GameFramework/Actor.h"

void USuperHotGirlPersonality::ApplyPersonality(UAvatar3DComponent* Avatar)
{
    if (!Avatar)
    {
        UE_LOG(LogTemp, Error, TEXT("SuperHotGirlPersonality::ApplyPersonality - Invalid Avatar"));
        return;
    }

    // Resting states are re-asserted every call; one-shot accents only when the
    // trait vector actually changed.
    const bool bTraitsChanged = HaveTraitsChanged();

    ApplyFacialPersonality(Avatar, bTraitsChanged);
    ApplyGesturePersonality(Avatar, bTraitsChanged);
    ApplyEmotionalPersonality(Avatar, bTraitsChanged);
    ApplyCognitivePersonality(Avatar, bTraitsChanged);

    if (bTraitsChanged)
    {
        LastAppliedTraits = Traits;
        bHasAppliedOnce = true;

        UE_LOG(LogTemp, Log, TEXT("Super-hot-girl personality applied (Confidence: %.2f, Charm: %.2f, Playfulness: %.2f, Wit: %.2f, Sass: %.2f)"),
            Traits.Confidence, Traits.Charm, Traits.Playfulness, Traits.Wit, Traits.Sass);
    }
}

bool USuperHotGirlPersonality::HaveTraitsChanged() const
{
    if (!bHasAppliedOnce)
    {
        return true;
    }
    return !FMath::IsNearlyEqual(Traits.Confidence, LastAppliedTraits.Confidence, 0.001f)
        || !FMath::IsNearlyEqual(Traits.Charm, LastAppliedTraits.Charm, 0.001f)
        || !FMath::IsNearlyEqual(Traits.Playfulness, LastAppliedTraits.Playfulness, 0.001f)
        || !FMath::IsNearlyEqual(Traits.Wit, LastAppliedTraits.Wit, 0.001f)
        || !FMath::IsNearlyEqual(Traits.Sass, LastAppliedTraits.Sass, 0.001f);
}

void USuperHotGirlPersonality::ApplyFacialPersonality(UAvatar3DComponent* Avatar, bool bTraitsChanged)
{
    if (!Avatar || !Avatar->FacialSystem)
        return;

    UFacialAnimationSystem* FacialSystem = Avatar->FacialSystem;

    // Resting emotion from the trait vector
    EEmotionState SelectedEmotion = EEmotionState::Confident;

    if (Traits.Confidence > 0.7f && Traits.Charm > 0.7f)
    {
        SelectedEmotion = EEmotionState::Confident;
    }
    else if (Traits.Playfulness > 0.7f)
    {
        SelectedEmotion = EEmotionState::Playful;
    }
    else if (Traits.Charm > 0.7f && Traits.Playfulness > 0.5f)
    {
        SelectedEmotion = EEmotionState::Flirty;
    }
    else if (Traits.Wit > 0.7f || Traits.Sass > 0.7f)
    {
        SelectedEmotion = EEmotionState::Happy;
    }

    if (FacialSystem->GetCurrentEmotion() != SelectedEmotion)
    {
        const float TransitionTime = 1.0f - (Traits.Confidence * 0.5f); // More confident = faster transitions
        FacialSystem->SetEmotionState(SelectedEmotion, TransitionTime);
    }

    // Micro-expression accents fire once per trait change: wit shows as a
    // playful flicker, sass as a flash of mock surprise.
    if (bTraitsChanged)
    {
        if (Traits.Wit > 0.7f)
        {
            FacialSystem->TriggerMicroExpression(EEmotionState::Playful, 0.2f);
        }
        else if (Traits.Sass > 0.7f)
        {
            FacialSystem->TriggerMicroExpression(EEmotionState::Surprised, 0.15f);
        }
    }
}

void USuperHotGirlPersonality::ApplyGesturePersonality(UAvatar3DComponent* Avatar, bool bTraitsChanged)
{
    if (!Avatar || !Avatar->GestureSystem)
        return;

    UGestureSystem* GestureSystem = Avatar->GestureSystem;

    if (bTraitsChanged)
    {
        // Signature gesture for the new trait vector. Ties between two fitting
        // gestures are broken by the secondary traits, not by chance.
        EGestureType SelectedGesture = EGestureType::Wave;

        if (Traits.Confidence > 0.8f)
        {
            SelectedGesture = EGestureType::Confident;
        }
        else if (Traits.Playfulness > 0.7f)
        {
            SelectedGesture = EGestureType::Playful;
        }
        else if (Traits.Charm > 0.8f)
        {
            SelectedGesture = (Traits.Playfulness >= Traits.Wit) ? EGestureType::Flirty : EGestureType::Elegant;
        }
        else if (Traits.Wit > 0.7f)
        {
            SelectedGesture = (Traits.Playfulness > Traits.Confidence) ? EGestureType::Peace : EGestureType::Thinking;
        }
        else if (Traits.Sass > 0.7f)
        {
            SelectedGesture = (Traits.Charm >= Traits.Confidence) ? EGestureType::Heart : EGestureType::ThumbsUp;
        }

        const float IntensityMultiplier = FMath::Lerp(0.7f, 1.5f, Traits.Confidence);
        GestureSystem->TriggerGesture(SelectedGesture, IntensityMultiplier);
    }

    // Resting body language
    EBodyLanguageState SelectedBodyLanguage = EBodyLanguageState::Relaxed;

    if (Traits.Confidence > 0.8f)
    {
        SelectedBodyLanguage = (Traits.Charm > 0.7f) ? EBodyLanguageState::Seductive : EBodyLanguageState::Confident;
    }
    else if (Traits.Playfulness > 0.7f)
    {
        SelectedBodyLanguage = EBodyLanguageState::Playful;
    }
    else if (Traits.Charm > 0.7f)
    {
        SelectedBodyLanguage = (Traits.Sass > 0.5f) ? EBodyLanguageState::Seductive : EBodyLanguageState::Open;
    }

    if (GestureSystem->GetBodyLanguageState() != SelectedBodyLanguage)
    {
        const float TransitionTime = FMath::Lerp(0.5f, 1.0f, 1.0f - Traits.Confidence);
        GestureSystem->SetBodyLanguageState(SelectedBodyLanguage, TransitionTime);
    }

    // Gesture context
    if (Traits.Charm > 0.7f && Traits.Playfulness > 0.6f)
    {
        GestureSystem->SetGestureContext(TEXT("flirting"));
    }
    else if (Traits.Confidence > 0.8f)
    {
        GestureSystem->SetGestureContext(TEXT("confidence"));
    }
    else if (Traits.Wit > 0.7f)
    {
        GestureSystem->SetGestureContext(TEXT("thinking"));
    }
}

void USuperHotGirlPersonality::ApplyEmotionalPersonality(UAvatar3DComponent* Avatar, bool bTraitsChanged)
{
    if (!Avatar || !Avatar->EmotionalAura)
        return;

    UEmotionalAuraComponent* EmotionalAura = Avatar->EmotionalAura;

    // Resting aura from the trait vector
    EAuraEmotionType SelectedAura = EAuraEmotionType::Confidence;

    if (Traits.Confidence > 0.8f && Traits.Charm > 0.7f)
    {
        SelectedAura = EAuraEmotionType::Confidence;
    }
    else if (Traits.Playfulness > 0.7f)
    {
        SelectedAura = EAuraEmotionType::Playfulness;
    }
    else if (Traits.Charm > 0.8f)
    {
        SelectedAura = (Traits.Playfulness > 0.5f) ? EAuraEmotionType::Excitement : EAuraEmotionType::Love;
    }
    else if (Traits.Wit > 0.7f || Traits.Sass > 0.7f)
    {
        SelectedAura = EAuraEmotionType::Joy;
    }

    if (EmotionalAura->GetEmotionalState() != SelectedAura)
    {
        const float TransitionTime = FMath::Lerp(0.8f, 1.5f, 1.0f - Traits.Confidence);
        EmotionalAura->SetEmotionalState(SelectedAura, TransitionTime);
    }

    // Aura intensity from charm and confidence
    float AuraIntensity = (Traits.Charm + Traits.Confidence) / 2.0f;
    AuraIntensity = FMath::Clamp(AuraIntensity, 0.5f, 1.5f);
    EmotionalAura->SetAuraIntensity(AuraIntensity);

    // A playful or sassy trait change flares the aura once
    if (bTraitsChanged && (Traits.Playfulness > 0.7f || Traits.Sass > 0.7f))
    {
        const float PulseIntensity = FMath::Lerp(1.5f, 2.5f, Traits.Playfulness);
        const float Duration = FMath::Lerp(0.3f, 0.6f, Traits.Sass);
        EmotionalAura->TriggerEmotionalPulse(PulseIntensity, Duration);
    }

    // Aura palette
    if (Traits.Charm > 0.8f)
    {
        // Pink/magenta for high charm
        const FLinearColor PrimaryColor = FLinearColor(1.0f, 0.4f, 0.7f, 1.0f);
        const FLinearColor SecondaryColor = FLinearColor(1.0f, 0.6f, 0.8f, 0.8f);
        EmotionalAura->SetCustomAuraColors(PrimaryColor, SecondaryColor);
    }
    else if (Traits.Confidence > 0.8f)
    {
        // Golden for high confidence
        const FLinearColor PrimaryColor = FLinearColor(1.0f, 0.8f, 0.3f, 1.0f);
        const FLinearColor SecondaryColor = FLinearColor(1.0f, 0.9f, 0.6f, 0.8f);
        EmotionalAura->SetCustomAuraColors(PrimaryColor, SecondaryColor);
    }
}

void USuperHotGirlPersonality::ApplyCognitivePersonality(UAvatar3DComponent* Avatar, bool bTraitsChanged)
{
    if (!Avatar || !Avatar->CognitiveViz)
        return;

    UCognitiveVisualizationComponent* CognitiveViz = Avatar->CognitiveViz;

    // Resting cognitive activity from the trait vector
    ECognitiveActivityType SelectedActivity = ECognitiveActivityType::Thinking;

    if (Traits.Wit > 0.7f)
    {
        SelectedActivity = (Traits.Sass > 0.5f) ? ECognitiveActivityType::Analyzing : ECognitiveActivityType::Thinking;
    }
    else if (Traits.Playfulness > 0.7f)
    {
        SelectedActivity = ECognitiveActivityType::Creating;
    }
    else if (Traits.Sass > 0.7f)
    {
        SelectedActivity = ECognitiveActivityType::Deciding;
    }
    else if (Traits.Charm > 0.7f)
    {
        SelectedActivity = ECognitiveActivityType::Emotional;
    }

    const float Intensity = FMath::Lerp(0.6f, 1.4f, Traits.Wit);
    CognitiveViz->SetCognitiveActivity(SelectedActivity, Intensity);

    // A witty or sassy trait change shows one thought stream. Its path through
    // space is visual particle noise, not a reaction.
    AActor* Owner = Avatar->GetOwner();
    if (bTraitsChanged && Owner && (Traits.Wit > 0.7f || Traits.Sass > 0.7f))
    {
        const FVector BasePosition = Owner->GetActorLocation();
        const FVector StartPosition = BasePosition + FVector(
            FMath::FRandRange(-80.0f, 80.0f),
            FMath::FRandRange(-80.0f, 80.0f),
            FMath::FRandRange(180.0f, 220.0f)
        );
        const FVector EndPosition = BasePosition + FVector(
            FMath::FRandRange(-80.0f, 80.0f),
            FMath::FRandRange(-80.0f, 80.0f),
            FMath::FRandRange(180.0f, 220.0f)
        );

        CognitiveViz->TriggerThoughtProcess(StartPosition, EndPosition);
    }
}
