#include "Avatar3DComponent.h"
#include "FacialAnimationSystem.h"
#include "GestureSystem.h"
#include "EmotionalAuraComponent.h"
#include "CognitiveVisualizationComponent.h"

UAvatar3DComponent::UAvatar3DComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.016f; // ~60 FPS updates

    // Initialize subsystem pointers
    FacialSystem = nullptr;
    GestureSystem = nullptr;
    EmotionalAura = nullptr;
    CognitiveViz = nullptr;
}

void UAvatar3DComponent::BeginPlay()
{
    Super::BeginPlay();

    // Create and initialize sub-components if not already set
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        UE_LOG(LogTemp, Error, TEXT("Avatar3DComponent: No owning actor"));
        return;
    }

    // Initialize FacialSystem
    if (!FacialSystem)
    {
        FacialSystem = Owner->FindComponentByClass<UFacialAnimationSystem>();
        if (!FacialSystem)
        {
            FacialSystem = NewObject<UFacialAnimationSystem>(Owner, TEXT("FacialSystem"));
            FacialSystem->RegisterComponent();
        }
    }
    else
    {
        FacialSystem->RegisterComponent();
    }

    // Initialize GestureSystem
    if (!GestureSystem)
    {
        GestureSystem = Owner->FindComponentByClass<UGestureSystem>();
        if (!GestureSystem)
        {
            GestureSystem = NewObject<UGestureSystem>(Owner, TEXT("GestureSystem"));
            GestureSystem->RegisterComponent();
        }
    }
    else
    {
        GestureSystem->RegisterComponent();
    }

    // Initialize EmotionalAura
    if (!EmotionalAura)
    {
        EmotionalAura = Owner->FindComponentByClass<UEmotionalAuraComponent>();
        if (!EmotionalAura)
        {
            EmotionalAura = NewObject<UEmotionalAuraComponent>(Owner, TEXT("EmotionalAura"));
            EmotionalAura->RegisterComponent();
        }
    }
    else
    {
        EmotionalAura->RegisterComponent();
    }

    // Initialize CognitiveViz
    if (!CognitiveViz)
    {
        CognitiveViz = Owner->FindComponentByClass<UCognitiveVisualizationComponent>();
        if (!CognitiveViz)
        {
            CognitiveViz = NewObject<UCognitiveVisualizationComponent>(Owner, TEXT("CognitiveViz"));
            CognitiveViz->RegisterComponent();
        }
    }
    else
    {
        CognitiveViz->RegisterComponent();
    }

    UE_LOG(LogTemp, Log, TEXT("Avatar3DComponent: All subsystems initialized for %s"), *Owner->GetName());
}

void UAvatar3DComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Coordinate subsystem updates
    UpdateFacialAnimation(DeltaTime);
    UpdateGestures(DeltaTime);
    UpdateEmotionalAura(DeltaTime);
    UpdateCognitiveVisualization(DeltaTime);
}

void UAvatar3DComponent::SetEmotionalState(EEmotionState Emotion, float Intensity, float TransitionTime)
{
    if (FacialSystem)
    {
        FacialSystem->SetEmotionState(Emotion, TransitionTime);
    }

    // Map EEmotionState to aura emotions
    if (EmotionalAura)
    {
        EAuraEmotionType AuraEmotion = EAuraEmotionType::Neutral;
        switch (Emotion)
        {
            case EEmotionState::Happy:
                AuraEmotion = EAuraEmotionType::Joy;
                break;
            case EEmotionState::Sad:
                AuraEmotion = EAuraEmotionType::Sadness;
                break;
            case EEmotionState::Angry:
                AuraEmotion = EAuraEmotionType::Anger;
                break;
            case EEmotionState::Fearful:
                AuraEmotion = EAuraEmotionType::Fear;
                break;
            case EEmotionState::Flirty:
                AuraEmotion = EAuraEmotionType::Love;
                break;
            case EEmotionState::Playful:
                AuraEmotion = EAuraEmotionType::Playfulness;
                break;
            case EEmotionState::Confident:
                AuraEmotion = EAuraEmotionType::Confidence;
                break;
            case EEmotionState::Surprised:
                AuraEmotion = EAuraEmotionType::Excitement;
                break;
            default:
                AuraEmotion = EAuraEmotionType::Neutral;
                break;
        }
        EmotionalAura->SetEmotionalState(AuraEmotion, TransitionTime);
        EmotionalAura->SetAuraIntensity(Intensity);
    }

    CurrentEmotionalIntensity = Intensity;
}

void UAvatar3DComponent::TriggerGesture(EGestureType Gesture, float Intensity)
{
    if (GestureSystem)
    {
        GestureSystem->TriggerGesture(Gesture, Intensity);
    }
}

void UAvatar3DComponent::SetBodyLanguage(EBodyLanguageState BodyLanguage, float TransitionTime)
{
    if (GestureSystem)
    {
        GestureSystem->SetBodyLanguageState(BodyLanguage, TransitionTime);
    }
}

void UAvatar3DComponent::ApplyNeurochemicalState(float Dopamine, float Serotonin, float Cortisol, float Oxytocin)
{
    // Map neurochemical levels to visual feedback

    // Dopamine affects happiness and energy
    float HappinessLevel = FMath::Clamp(Dopamine + Serotonin * 0.5f, 0.0f, 1.0f);
    if (HappinessLevel > 0.6f && FacialSystem)
    {
        FacialSystem->TriggerMicroExpression(EEmotionState::Happy, 0.2f);
    }

    // Cortisol affects stress/tension
    float StressLevel = FMath::Clamp(Cortisol, 0.0f, 1.0f);
    if (StressLevel > 0.7f && GestureSystem)
    {
        GestureSystem->SetBodyLanguageState(EBodyLanguageState::Tense, 0.5f);
    }
    else if (StressLevel < 0.3f && GestureSystem)
    {
        GestureSystem->SetBodyLanguageState(EBodyLanguageState::Relaxed, 0.5f);
    }

    // Oxytocin affects warmth and openness
    float WarmthLevel = FMath::Clamp(Oxytocin, 0.0f, 1.0f);
    if (WarmthLevel > 0.5f && GestureSystem)
    {
        GestureSystem->SetBodyLanguageState(EBodyLanguageState::Open, 0.5f);
    }

    // Update aura based on overall neurochemical balance
    if (EmotionalAura)
    {
        // Calculate overall mood from neurochemical mix
        float Valence = (Dopamine + Serotonin - Cortisol) / 3.0f;
        float Arousal = (Dopamine + Cortisol) / 2.0f;

        // Set aura color based on valence
        FLinearColor AuraColor;
        if (Valence > 0.3f)
        {
            // Positive: warm golden/pink
            AuraColor = FLinearColor::LerpUsingHSV(
                FLinearColor(1.0f, 0.8f, 0.3f),  // Warm gold
                FLinearColor(1.0f, 0.5f, 0.7f),  // Pink
                Oxytocin
            );
        }
        else if (Valence < -0.3f)
        {
            // Negative: cool blue/purple
            AuraColor = FLinearColor::LerpUsingHSV(
                FLinearColor(0.3f, 0.3f, 0.8f),  // Blue
                FLinearColor(0.5f, 0.2f, 0.6f),  // Purple
                Cortisol
            );
        }
        else
        {
            // Neutral: cyan
            AuraColor = FLinearColor(0.3f, 0.7f, 0.9f);
        }

        EmotionalAura->SetCustomAuraColors(AuraColor, AuraColor * 0.5f);
        EmotionalAura->SetAuraIntensity(0.3f + Arousal * 0.7f);
    }

    // Store for cognitive visualization
    LastNeurochemicalBalance = FVector4(Dopamine, Serotonin, Cortisol, Oxytocin);
}

void UAvatar3DComponent::SetCognitiveLoad(float Load)
{
    CurrentCognitiveLoad = FMath::Clamp(Load, 0.0f, 1.0f);

    if (CognitiveViz)
    {
        // Visualize cognitive load through particle intensity
        // High load = more visible thought particles
        CognitiveViz->SetVisualizationIntensity(CurrentCognitiveLoad);
    }

    // High cognitive load affects facial expression
    if (CurrentCognitiveLoad > 0.7f && FacialSystem)
    {
        // Thoughtful/concentrated expression
        FacialSystem->TriggerMicroExpression(EEmotionState::Neutral, 0.3f);
    }
}

void UAvatar3DComponent::TriggerEchoResonance(float Intensity, float Duration)
{
    if (EmotionalAura)
    {
        EmotionalAura->TriggerEmotionalPulse(Intensity, Duration);
    }

    if (CognitiveViz)
    {
        // Trigger memory constellation visualization
        CognitiveViz->TriggerResonanceEffect(Intensity, Duration);
    }
}

void UAvatar3DComponent::UpdateFacialAnimation(float DeltaTime)
{
    // Facial system handles its own tick, but we can add coordination logic here
    if (!FacialSystem)
    {
        return;
    }

    // Apply micro-expression variations based on cognitive load
    if (CurrentCognitiveLoad > 0.5f && FMath::RandRange(0.0f, 1.0f) < 0.01f * CurrentCognitiveLoad)
    {
        // Occasional thoughtful micro-expressions during high cognitive load
        FacialSystem->TriggerMicroExpression(EEmotionState::Neutral, 0.15f);
    }
}

void UAvatar3DComponent::UpdateGestures(float DeltaTime)
{
    // Gesture system handles its own tick
    if (!GestureSystem)
    {
        return;
    }
}

void UAvatar3DComponent::UpdateEmotionalAura(float DeltaTime)
{
    // Aura system handles its own tick
    if (!EmotionalAura)
    {
        return;
    }
}

void UAvatar3DComponent::UpdateCognitiveVisualization(float DeltaTime)
{
    // Cognitive visualization handles its own tick
    if (!CognitiveViz)
    {
        return;
    }
}
