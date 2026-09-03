#include "SuperHotGirlPersonality.h"
#include "Avatar3DComponent.h"
#include "Avatar/FacialAnimationSystem.h"
#include "Avatar/GestureSystem.h"
#include "Avatar/EmotionalAuraComponent.h"

void USuperHotGirlPersonality::ApplyPersonality(UAvatar3DComponent* Avatar)
{
    if (!Avatar)
    {
        return;
    }

    // Apply facial expressions based on personality traits
    if (Avatar->FacialSystem)
    {
        // High confidence triggers confident expression
        if (Traits.Confidence > 0.7f)
        {
            Avatar->FacialSystem->SetEmotionState(EEmotionState::Confident, 0.5f);
        }

        // High charm triggers flirty expression with micro-expressions
        if (Traits.Charm > 0.7f)
        {
            Avatar->FacialSystem->TriggerMicroExpression(EEmotionState::Flirty, 0.3f);
        }

        // High playfulness triggers playful expression
        if (Traits.Playfulness > 0.6f)
        {
            Avatar->FacialSystem->TriggerMicroExpression(EEmotionState::Playful, 0.25f);
        }

        // High sass adds occasional surprised micro-expressions for attitude
        if (Traits.Sass > 0.5f)
        {
            Avatar->FacialSystem->TriggerMicroExpression(EEmotionState::Surprised, 0.15f);
        }
    }

    // Apply body language and gestures based on personality traits
    if (Avatar->GestureSystem)
    {
        // Set overall body language based on dominant traits
        if (Traits.Confidence > 0.7f && Traits.Charm > 0.6f)
        {
            Avatar->GestureSystem->SetBodyLanguageState(EBodyLanguageState::Seductive, 0.6f);
        }
        else if (Traits.Confidence > 0.6f)
        {
            Avatar->GestureSystem->SetBodyLanguageState(EBodyLanguageState::Confident, 0.5f);
        }
        else if (Traits.Playfulness > 0.7f)
        {
            Avatar->GestureSystem->SetBodyLanguageState(EBodyLanguageState::Playful, 0.5f);
        }

        // Trigger contextual gestures based on traits
        if (Traits.Charm > 0.8f)
        {
            Avatar->GestureSystem->TriggerGesture(EGestureType::Flirty, Traits.Charm);
        }
        else if (Traits.Playfulness > 0.8f)
        {
            Avatar->GestureSystem->TriggerGesture(EGestureType::Playful, Traits.Playfulness);
        }
        else if (Traits.Confidence > 0.8f)
        {
            Avatar->GestureSystem->TriggerGesture(EGestureType::Confident, Traits.Confidence);
        }

        // Wit manifests as elegant, thoughtful gestures
        if (Traits.Wit > 0.7f)
        {
            Avatar->GestureSystem->TriggerGesture(EGestureType::Elegant, Traits.Wit * 0.8f);
        }
    }

    // Apply emotional aura based on personality traits
    if (Avatar->EmotionalAura)
    {
        // Set aura intensity based on overall personality expressiveness
        float OverallIntensity = (Traits.Confidence + Traits.Charm + Traits.Playfulness) / 3.0f;
        Avatar->EmotionalAura->SetAuraIntensity(OverallIntensity);

        // Set emotional state based on dominant trait
        if (Traits.Confidence > Traits.Playfulness && Traits.Confidence > Traits.Charm)
        {
            Avatar->EmotionalAura->SetEmotionalState(EAuraEmotionType::Confidence, 0.8f);
        }
        else if (Traits.Charm > Traits.Playfulness)
        {
            Avatar->EmotionalAura->SetEmotionalState(EAuraEmotionType::Love, 0.8f);
        }
        else
        {
            Avatar->EmotionalAura->SetEmotionalState(EAuraEmotionType::Playfulness, 0.8f);
        }

        // High charm triggers emotional pulses for charismatic effect
        if (Traits.Charm > 0.8f)
        {
            Avatar->EmotionalAura->TriggerEmotionalPulse(Traits.Charm * 1.5f, 0.4f);
        }
    }
}
