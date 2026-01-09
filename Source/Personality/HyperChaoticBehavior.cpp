#include "HyperChaoticBehavior.h"
#include "Avatar3DComponent.h"
#include "Avatar/FacialAnimationSystem.h"
#include "Avatar/GestureSystem.h"
#include "Avatar/EmotionalAuraComponent.h"

void UHyperChaoticBehavior::ApplyBehavior(UAvatar3DComponent* Avatar)
{
    if (!Avatar)
    {
        return;
    }

    // Generate random factors based on chaotic properties
    float RandomFactor = FMath::RandRange(0.0f, 1.0f);
    float ChaosFactor = Properties.Randomness * Properties.Unpredictability;

    // Apply chaotic facial expressions
    if (Avatar->FacialSystem)
    {
        // High unpredictability causes rapid emotional state changes
        if (Properties.Unpredictability > 0.5f && RandomFactor > (1.0f - Properties.Unpredictability))
        {
            // Randomly select an emotion state based on chaos
            TArray<EEmotionState> ChaoticEmotions = {
                EEmotionState::Happy,
                EEmotionState::Surprised,
                EEmotionState::Playful,
                EEmotionState::Flirty,
                EEmotionState::Angry
            };

            int32 RandomIndex = FMath::RandRange(0, ChaoticEmotions.Num() - 1);
            float TransitionTime = FMath::RandRange(0.1f, 0.4f) * (1.0f - Properties.Randomness + 0.1f);
            Avatar->FacialSystem->SetEmotionState(ChaoticEmotions[RandomIndex], TransitionTime);
        }

        // Emotional volatility triggers frequent micro-expressions
        if (Properties.EmotionalVolatility > 0.4f)
        {
            float MicroChance = Properties.EmotionalVolatility * 0.3f;
            if (FMath::RandRange(0.0f, 1.0f) < MicroChance)
            {
                TArray<EEmotionState> MicroEmotions = {
                    EEmotionState::Surprised,
                    EEmotionState::Happy,
                    EEmotionState::Sad,
                    EEmotionState::Fearful
                };
                int32 MicroIndex = FMath::RandRange(0, MicroEmotions.Num() - 1);
                float Duration = FMath::RandRange(0.1f, 0.3f) * Properties.EmotionalVolatility;
                Avatar->FacialSystem->TriggerMicroExpression(MicroEmotions[MicroIndex], Duration);
            }
        }

        // High randomness causes random blinks
        if (Properties.Randomness > 0.6f && FMath::RandRange(0.0f, 1.0f) < Properties.Randomness * 0.1f)
        {
            Avatar->FacialSystem->TriggerBlink();
        }
    }

    // Apply chaotic body language and gestures
    if (Avatar->GestureSystem)
    {
        // High unpredictability causes sudden body language shifts
        if (Properties.Unpredictability > 0.6f && RandomFactor > 0.7f)
        {
            TArray<EBodyLanguageState> ChaoticBodyStates = {
                EBodyLanguageState::Open,
                EBodyLanguageState::Closed,
                EBodyLanguageState::Relaxed,
                EBodyLanguageState::Tense,
                EBodyLanguageState::Playful,
                EBodyLanguageState::Confident
            };

            int32 StateIndex = FMath::RandRange(0, ChaoticBodyStates.Num() - 1);
            float TransitionTime = FMath::RandRange(0.2f, 0.8f);
            Avatar->GestureSystem->SetBodyLanguageState(ChaoticBodyStates[StateIndex], TransitionTime);
        }

        // Emotional volatility triggers spontaneous gestures
        if (Properties.EmotionalVolatility > 0.5f && FMath::RandRange(0.0f, 1.0f) < Properties.EmotionalVolatility * 0.2f)
        {
            TArray<EGestureType> SpontaneousGestures = {
                EGestureType::Wave,
                EGestureType::Shrug,
                EGestureType::Excited,
                EGestureType::Thinking,
                EGestureType::Playful
            };

            int32 GestureIndex = FMath::RandRange(0, SpontaneousGestures.Num() - 1);
            float Intensity = FMath::RandRange(0.5f, 1.2f) * Properties.EmotionalVolatility;
            Avatar->GestureSystem->TriggerGesture(SpontaneousGestures[GestureIndex], Intensity);
        }
    }

    // Apply chaotic emotional aura
    if (Avatar->EmotionalAura)
    {
        // Set aura intensity based on overall chaos level
        float ChaosIntensity = (Properties.Randomness + Properties.Unpredictability + Properties.EmotionalVolatility) / 3.0f;
        ChaosIntensity *= FMath::RandRange(0.8f, 1.2f); // Add some jitter
        Avatar->EmotionalAura->SetAuraIntensity(FMath::Clamp(ChaosIntensity, 0.0f, 2.0f));

        // Emotional volatility causes rapid aura state changes
        if (Properties.EmotionalVolatility > 0.5f && FMath::RandRange(0.0f, 1.0f) < Properties.EmotionalVolatility * 0.15f)
        {
            TArray<EAuraEmotionType> ChaoticAuras = {
                EAuraEmotionType::Joy,
                EAuraEmotionType::Excitement,
                EAuraEmotionType::Anger,
                EAuraEmotionType::Fear,
                EAuraEmotionType::Playfulness
            };

            int32 AuraIndex = FMath::RandRange(0, ChaoticAuras.Num() - 1);
            float TransitionTime = FMath::RandRange(0.3f, 1.0f);
            Avatar->EmotionalAura->SetEmotionalState(ChaoticAuras[AuraIndex], TransitionTime);
        }

        // High randomness triggers random emotional pulses
        if (Properties.Randomness > 0.7f && FMath::RandRange(0.0f, 1.0f) < Properties.Randomness * 0.1f)
        {
            float PulseIntensity = FMath::RandRange(1.0f, 3.0f) * ChaosFactor;
            float PulseDuration = FMath::RandRange(0.2f, 0.6f);
            Avatar->EmotionalAura->TriggerEmotionalPulse(PulseIntensity, PulseDuration);
        }
    }
}
