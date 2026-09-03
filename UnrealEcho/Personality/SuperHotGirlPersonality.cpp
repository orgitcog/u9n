#include "SuperHotGirlPersonality.h"
#include "Avatar3DComponent.h"
#include "FacialAnimationSystem.h"
#include "GestureSystem.h"
#include "EmotionalAuraComponent.h"
#include "CognitiveVisualizationComponent.h"

void USuperHotGirlPersonality::ApplyPersonality(UAvatar3DComponent* Avatar)
{
    if (!Avatar)
    {
        UE_LOG(LogTemp, Error, TEXT("SuperHotGirlPersonality::ApplyPersonality - Invalid Avatar"));
        return;
    }

    // Apply personality traits to all avatar subsystems
    ApplyFacialPersonality(Avatar);
    ApplyGesturePersonality(Avatar);
    ApplyEmotionalPersonality(Avatar);
    ApplyCognitivePersonality(Avatar);

    UE_LOG(LogTemp, Log, TEXT("Super-hot-girl personality applied (Confidence: %.2f, Charm: %.2f, Playfulness: %.2f, Wit: %.2f, Sass: %.2f)"),
        Traits.Confidence, Traits.Charm, Traits.Playfulness, Traits.Wit, Traits.Sass);
}

void USuperHotGirlPersonality::ApplyFacialPersonality(UAvatar3DComponent* Avatar)
{
    if (!Avatar || !Avatar->FacialSystem)
        return;

    UFacialAnimationSystem* FacialSystem = Avatar->FacialSystem;

    // Select emotion based on personality traits
    EEmotionState SelectedEmotion = EEmotionState::Neutral;

    // High confidence + high charm = Confident
    if (Traits.Confidence > 0.7f && Traits.Charm > 0.7f)
    {
        SelectedEmotion = EEmotionState::Confident;
    }
    // High playfulness = Playful
    else if (Traits.Playfulness > 0.7f)
    {
        SelectedEmotion = EEmotionState::Playful;
    }
    // High charm + moderate playfulness = Flirty
    else if (Traits.Charm > 0.7f && Traits.Playfulness > 0.5f)
    {
        SelectedEmotion = EEmotionState::Flirty;
    }
    // High wit or sass = Happy (with a smirk)
    else if (Traits.Wit > 0.7f || Traits.Sass > 0.7f)
    {
        SelectedEmotion = EEmotionState::Happy;
    }
    else
    {
        SelectedEmotion = EEmotionState::Confident; // Default to confident
    }

    // Apply emotion with smooth transition
    float TransitionTime = 1.0f - (Traits.Confidence * 0.5f); // More confident = faster transitions
    FacialSystem->SetEmotionState(SelectedEmotion, TransitionTime);

    // Adjust eye blink frequency based on charm
    float BlinkFrequency = FMath::Lerp(2.0f, 4.0f, Traits.Charm);
    // Note: Would call FacialSystem->SetBlinkFrequency(BlinkFrequency) if that method existed

    // Trigger micro-expressions based on wit and sass
    if (Traits.Wit > 0.7f && FMath::FRand() < 0.3f)
    {
        FacialSystem->TriggerMicroExpression(EEmotionState::Playful, 0.2f);
    }

    if (Traits.Sass > 0.7f && FMath::FRand() < 0.2f)
    {
        FacialSystem->TriggerMicroExpression(EEmotionState::Surprised, 0.15f);
    }
}

void USuperHotGirlPersonality::ApplyGesturePersonality(UAvatar3DComponent* Avatar)
{
    if (!Avatar || !Avatar->GestureSystem)
        return;

    UGestureSystem* GestureSystem = Avatar->GestureSystem;

    // Select gesture based on personality traits
    EGestureType SelectedGesture = EGestureType::Wave;

    // High confidence = Confident pose
    if (Traits.Confidence > 0.8f)
    {
        SelectedGesture = EGestureType::Confident;
    }
    // High playfulness = Playful gesture
    else if (Traits.Playfulness > 0.7f)
    {
        SelectedGesture = EGestureType::Playful;
    }
    // High charm = Flirty or Elegant
    else if (Traits.Charm > 0.8f)
    {
        SelectedGesture = (FMath::FRand() < 0.5f) ? EGestureType::Flirty : EGestureType::Elegant;
    }
    // High wit = Thinking or Peace
    else if (Traits.Wit > 0.7f)
    {
        SelectedGesture = (FMath::FRand() < 0.5f) ? EGestureType::Thinking : EGestureType::Peace;
    }
    // High sass = Heart or ThumbsUp
    else if (Traits.Sass > 0.7f)
    {
        SelectedGesture = (FMath::FRand() < 0.5f) ? EGestureType::Heart : EGestureType::ThumbsUp;
    }

    // Apply gesture with intensity based on confidence
    float IntensityMultiplier = FMath::Lerp(0.7f, 1.5f, Traits.Confidence);
    GestureSystem->TriggerGesture(SelectedGesture, IntensityMultiplier);

    // Select body language based on personality
    EBodyLanguageState SelectedBodyLanguage = EBodyLanguageState::Neutral;

    // High confidence = Confident or Seductive
    if (Traits.Confidence > 0.8f)
    {
        SelectedBodyLanguage = (Traits.Charm > 0.7f) ? EBodyLanguageState::Seductive : EBodyLanguageState::Confident;
    }
    // High playfulness = Playful
    else if (Traits.Playfulness > 0.7f)
    {
        SelectedBodyLanguage = EBodyLanguageState::Playful;
    }
    // High charm = Seductive or Open
    else if (Traits.Charm > 0.7f)
    {
        SelectedBodyLanguage = (FMath::FRand() < 0.6f) ? EBodyLanguageState::Seductive : EBodyLanguageState::Open;
    }
    // Default to relaxed
    else
    {
        SelectedBodyLanguage = EBodyLanguageState::Relaxed;
    }

    // Apply body language with smooth transition
    float TransitionTime = FMath::Lerp(0.5f, 1.0f, 1.0f - Traits.Confidence);
    GestureSystem->SetBodyLanguageState(SelectedBodyLanguage, TransitionTime);

    // Set gesture context based on personality
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

void USuperHotGirlPersonality::ApplyEmotionalPersonality(UAvatar3DComponent* Avatar)
{
    if (!Avatar || !Avatar->EmotionalAura)
        return;

    UEmotionalAuraComponent* EmotionalAura = Avatar->EmotionalAura;

    // Select aura based on personality traits
    EAuraEmotionType SelectedAura = EAuraEmotionType::Neutral;

    // High confidence + high charm = Confidence aura
    if (Traits.Confidence > 0.8f && Traits.Charm > 0.7f)
    {
        SelectedAura = EAuraEmotionType::Confidence;
    }
    // High playfulness = Playfulness aura
    else if (Traits.Playfulness > 0.7f)
    {
        SelectedAura = EAuraEmotionType::Playfulness;
    }
    // High charm = Love or Excitement
    else if (Traits.Charm > 0.8f)
    {
        SelectedAura = (FMath::FRand() < 0.6f) ? EAuraEmotionType::Love : EAuraEmotionType::Excitement;
    }
    // High wit or sass = Joy
    else if (Traits.Wit > 0.7f || Traits.Sass > 0.7f)
    {
        SelectedAura = EAuraEmotionType::Joy;
    }
    // Default to confidence
    else
    {
        SelectedAura = EAuraEmotionType::Confidence;
    }

    // Apply aura with smooth transition
    float TransitionTime = FMath::Lerp(0.8f, 1.5f, 1.0f - Traits.Confidence);
    EmotionalAura->SetEmotionalState(SelectedAura, TransitionTime);

    // Set aura intensity based on charm and confidence
    float AuraIntensity = (Traits.Charm + Traits.Confidence) / 2.0f;
    AuraIntensity = FMath::Clamp(AuraIntensity, 0.5f, 1.5f);
    EmotionalAura->SetAuraIntensity(AuraIntensity);

    // Trigger emotional pulse based on playfulness and sass
    if ((Traits.Playfulness > 0.7f || Traits.Sass > 0.7f) && FMath::FRand() < 0.2f)
    {
        float PulseIntensity = FMath::Lerp(1.5f, 2.5f, Traits.Playfulness);
        float Duration = FMath::Lerp(0.3f, 0.6f, Traits.Sass);
        EmotionalAura->TriggerEmotionalPulse(PulseIntensity, Duration);
    }

    // Set custom aura colors for super-hot-girl aesthetic
    if (Traits.Charm > 0.8f)
    {
        // Pink/magenta for high charm
        FLinearColor PrimaryColor = FLinearColor(1.0f, 0.4f, 0.7f, 1.0f);
        FLinearColor SecondaryColor = FLinearColor(1.0f, 0.6f, 0.8f, 0.8f);
        EmotionalAura->SetCustomAuraColors(PrimaryColor, SecondaryColor);
    }
    else if (Traits.Confidence > 0.8f)
    {
        // Golden for high confidence
        FLinearColor PrimaryColor = FLinearColor(1.0f, 0.8f, 0.3f, 1.0f);
        FLinearColor SecondaryColor = FLinearColor(1.0f, 0.9f, 0.6f, 0.8f);
        EmotionalAura->SetCustomAuraColors(PrimaryColor, SecondaryColor);
    }
}

void USuperHotGirlPersonality::ApplyCognitivePersonality(UAvatar3DComponent* Avatar)
{
    if (!Avatar || !Avatar->CognitiveViz)
        return;

    UCognitiveVisualizationComponent* CognitiveViz = Avatar->CognitiveViz;

    // Select cognitive activity based on personality traits
    ECognitiveActivityType SelectedActivity = ECognitiveActivityType::Idle;

    // High wit = Thinking or Analyzing
    if (Traits.Wit > 0.7f)
    {
        SelectedActivity = (FMath::FRand() < 0.6f) ? ECognitiveActivityType::Thinking : ECognitiveActivityType::Analyzing;
    }
    // High playfulness = Creating
    else if (Traits.Playfulness > 0.7f)
    {
        SelectedActivity = ECognitiveActivityType::Creating;
    }
    // High sass = Deciding
    else if (Traits.Sass > 0.7f)
    {
        SelectedActivity = ECognitiveActivityType::Deciding;
    }
    // High charm = Emotional processing
    else if (Traits.Charm > 0.7f)
    {
        SelectedActivity = ECognitiveActivityType::Emotional;
    }
    // Default to thinking
    else
    {
        SelectedActivity = ECognitiveActivityType::Thinking;
    }

    // Apply cognitive activity with intensity based on wit
    float Intensity = FMath::Lerp(0.6f, 1.4f, Traits.Wit);
    CognitiveViz->SetCognitiveActivity(SelectedActivity, Intensity);

    // Trigger thought processes based on personality
    AActor* Owner = Avatar->GetOwner();
    if (Owner && (Traits.Wit > 0.7f || Traits.Sass > 0.7f) && FMath::FRand() < 0.15f)
    {
        FVector BasePosition = Owner->GetActorLocation();
        FVector StartPosition = BasePosition + FVector(
            FMath::FRandRange(-80.0f, 80.0f),
            FMath::FRandRange(-80.0f, 80.0f),
            FMath::FRandRange(180.0f, 220.0f)
        );
        FVector EndPosition = BasePosition + FVector(
            FMath::FRandRange(-80.0f, 80.0f),
            FMath::FRandRange(-80.0f, 80.0f),
            FMath::FRandRange(180.0f, 220.0f)
        );

        CognitiveViz->TriggerThoughtProcess(StartPosition, EndPosition);
    }
}
