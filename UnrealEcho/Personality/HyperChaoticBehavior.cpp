#include "HyperChaoticBehavior.h"
#include "Avatar3DComponent.h"
#include "FacialAnimationSystem.h"
#include "GestureSystem.h"
#include "EmotionalAuraComponent.h"
#include "CognitiveVisualizationComponent.h"

void UHyperChaoticBehavior::ApplyBehavior(UAvatar3DComponent* Avatar)
{
    if (!Avatar)
    {
        UE_LOG(LogTemp, Error, TEXT("HyperChaoticBehavior::ApplyBehavior - Invalid Avatar"));
        return;
    }

    // Apply hyper-chaotic properties to all avatar subsystems
    ApplyChaoticFacialBehavior(Avatar);
    ApplyChaoticGestureBehavior(Avatar);
    ApplyChaoticEmotionalBehavior(Avatar);
    ApplyChaoticCognitiveBehavior(Avatar);
    ApplyDeepTreeEchoProperties(Avatar);

    UE_LOG(LogTemp, Log, TEXT("Hyper-chaotic behavior applied to avatar (Randomness: %.2f, Unpredictability: %.2f, Volatility: %.2f)"),
        Properties.Randomness, Properties.Unpredictability, Properties.EmotionalVolatility);
}

void UHyperChaoticBehavior::ApplyChaoticFacialBehavior(UAvatar3DComponent* Avatar)
{
    if (!Avatar || !Avatar->FacialSystem)
        return;

    UFacialAnimationSystem* FacialSystem = Avatar->FacialSystem;

    // Generate chaotic emotion selection with bounded randomness
    if (FMath::FRand() < Properties.Unpredictability * 0.1f) // 10% base chance scaled by unpredictability
    {
        // Select a random emotion with preference for super-hot-girl aesthetics
        TArray<EEmotionState> PreferredEmotions = {
            EEmotionState::Flirty,
            EEmotionState::Playful,
            EEmotionState::Confident,
            EEmotionState::Happy
        };

        TArray<EEmotionState> AllEmotions = {
            EEmotionState::Neutral,
            EEmotionState::Happy,
            EEmotionState::Sad,
            EEmotionState::Angry,
            EEmotionState::Surprised,
            EEmotionState::Disgusted,
            EEmotionState::Fearful,
            EEmotionState::Flirty,
            EEmotionState::Playful,
            EEmotionState::Confident
        };

        EEmotionState SelectedEmotion;

        // 70% chance to select from preferred emotions (super-hot-girl aesthetic)
        if (FMath::FRand() < 0.7f)
        {
            int32 RandomIndex = FMath::RandRange(0, PreferredEmotions.Num() - 1);
            SelectedEmotion = PreferredEmotions[RandomIndex];
        }
        else
        {
            int32 RandomIndex = FMath::RandRange(0, AllEmotions.Num() - 1);
            SelectedEmotion = AllEmotions[RandomIndex];
        }

        // Chaotic transition time (faster = more unpredictable)
        float TransitionTime = FMath::FRandRange(0.1f, 0.5f) * (1.0f - Properties.Unpredictability);
        FacialSystem->SetEmotionState(SelectedEmotion, TransitionTime);
    }

    // Trigger random micro-expressions for emotional volatility
    if (FMath::FRand() < Properties.EmotionalVolatility * 0.05f)
    {
        TArray<EEmotionState> MicroEmotions = {
            EEmotionState::Surprised,
            EEmotionState::Flirty,
            EEmotionState::Playful
        };

        int32 RandomIndex = FMath::RandRange(0, MicroEmotions.Num() - 1);
        float Duration = FMath::FRandRange(0.1f, 0.3f);
        
        FacialSystem->TriggerMicroExpression(MicroEmotions[RandomIndex], Duration);
    }
}

void UHyperChaoticBehavior::ApplyChaoticGestureBehavior(UAvatar3DComponent* Avatar)
{
    if (!Avatar || !Avatar->GestureSystem)
        return;

    UGestureSystem* GestureSystem = Avatar->GestureSystem;

    // Chaotic gesture triggering
    if (FMath::FRand() < Properties.Randomness * 0.08f)
    {
        // Preferred gestures for super-hot-girl aesthetic
        TArray<EGestureType> PreferredGestures = {
            EGestureType::Flirty,
            EGestureType::Playful,
            EGestureType::Elegant,
            EGestureType::Confident,
            EGestureType::Heart,
            EGestureType::Peace
        };

        TArray<EGestureType> AllGestures = {
            EGestureType::Wave,
            EGestureType::Point,
            EGestureType::ThumbsUp,
            EGestureType::Peace,
            EGestureType::Heart,
            EGestureType::Shrug,
            EGestureType::Nod,
            EGestureType::Thinking,
            EGestureType::Excited,
            EGestureType::Confident,
            EGestureType::Flirty,
            EGestureType::Playful,
            EGestureType::Elegant
        };

        EGestureType SelectedGesture;

        // 80% chance for preferred gestures
        if (FMath::FRand() < 0.8f)
        {
            int32 RandomIndex = FMath::RandRange(0, PreferredGestures.Num() - 1);
            SelectedGesture = PreferredGestures[RandomIndex];
        }
        else
        {
            int32 RandomIndex = FMath::RandRange(0, AllGestures.Num() - 1);
            SelectedGesture = AllGestures[RandomIndex];
        }

        float IntensityMultiplier = FMath::FRandRange(0.7f, 1.5f) * Properties.Randomness;
        GestureSystem->TriggerGesture(SelectedGesture, IntensityMultiplier);
    }

    // Chaotic body language shifts
    if (FMath::FRand() < Properties.Unpredictability * 0.06f)
    {
        TArray<EBodyLanguageState> PreferredStates = {
            EBodyLanguageState::Confident,
            EBodyLanguageState::Playful,
            EBodyLanguageState::Seductive,
            EBodyLanguageState::Relaxed
        };

        TArray<EBodyLanguageState> AllStates = {
            EBodyLanguageState::Neutral,
            EBodyLanguageState::Open,
            EBodyLanguageState::Confident,
            EBodyLanguageState::Shy,
            EBodyLanguageState::Relaxed,
            EBodyLanguageState::Playful,
            EBodyLanguageState::Seductive
        };

        EBodyLanguageState SelectedState;

        // 75% chance for preferred states
        if (FMath::FRand() < 0.75f)
        {
            int32 RandomIndex = FMath::RandRange(0, PreferredStates.Num() - 1);
            SelectedState = PreferredStates[RandomIndex];
        }
        else
        {
            int32 RandomIndex = FMath::RandRange(0, AllStates.Num() - 1);
            SelectedState = AllStates[RandomIndex];
        }

        float TransitionTime = FMath::FRandRange(0.3f, 0.8f) * (1.0f - Properties.Unpredictability);
        GestureSystem->SetBodyLanguageState(SelectedState, TransitionTime);
    }
}

void UHyperChaoticBehavior::ApplyChaoticEmotionalBehavior(UAvatar3DComponent* Avatar)
{
    if (!Avatar || !Avatar->EmotionalAura)
        return;

    UEmotionalAuraComponent* EmotionalAura = Avatar->EmotionalAura;

    // Chaotic aura state changes
    if (FMath::FRand() < Properties.EmotionalVolatility * 0.07f)
    {
        TArray<EAuraEmotionType> PreferredAuras = {
            EAuraEmotionType::Love,
            EAuraEmotionType::Excitement,
            EAuraEmotionType::Confidence,
            EAuraEmotionType::Playfulness,
            EAuraEmotionType::Joy
        };

        TArray<EAuraEmotionType> AllAuras = {
            EAuraEmotionType::Neutral,
            EAuraEmotionType::Joy,
            EAuraEmotionType::Sadness,
            EAuraEmotionType::Anger,
            EAuraEmotionType::Fear,
            EAuraEmotionType::Love,
            EAuraEmotionType::Excitement,
            EAuraEmotionType::Calm,
            EAuraEmotionType::Confidence,
            EAuraEmotionType::Playfulness
        };

        EAuraEmotionType SelectedAura;

        // 85% chance for preferred auras
        if (FMath::FRand() < 0.85f)
        {
            int32 RandomIndex = FMath::RandRange(0, PreferredAuras.Num() - 1);
            SelectedAura = PreferredAuras[RandomIndex];
        }
        else
        {
            int32 RandomIndex = FMath::RandRange(0, AllAuras.Num() - 1);
            SelectedAura = AllAuras[RandomIndex];
        }

        float TransitionTime = FMath::FRandRange(0.5f, 1.5f) * (1.0f - Properties.EmotionalVolatility);
        EmotionalAura->SetEmotionalState(SelectedAura, TransitionTime);
    }

    // Random emotional pulses
    if (FMath::FRand() < Properties.EmotionalVolatility * 0.04f)
    {
        float PulseIntensity = FMath::FRandRange(1.5f, 2.5f) * Properties.EmotionalVolatility;
        float Duration = FMath::FRandRange(0.3f, 0.7f);
        
        EmotionalAura->TriggerEmotionalPulse(PulseIntensity, Duration);
    }

    // Chaotic aura intensity modulation
    if (FMath::FRand() < Properties.Randomness * 0.05f)
    {
        float NewIntensity = FMath::FRandRange(0.5f, 1.8f) * Properties.Randomness;
        EmotionalAura->SetAuraIntensity(NewIntensity);
    }
}

void UHyperChaoticBehavior::ApplyChaoticCognitiveBehavior(UAvatar3DComponent* Avatar)
{
    if (!Avatar || !Avatar->CognitiveViz)
        return;

    UCognitiveVisualizationComponent* CognitiveViz = Avatar->CognitiveViz;

    // Chaotic cognitive activity shifts
    if (FMath::FRand() < Properties.Unpredictability * 0.09f)
    {
        TArray<ECognitiveActivityType> Activities = {
            ECognitiveActivityType::Thinking,
            ECognitiveActivityType::Learning,
            ECognitiveActivityType::Remembering,
            ECognitiveActivityType::Creating,
            ECognitiveActivityType::Analyzing,
            ECognitiveActivityType::Deciding,
            ECognitiveActivityType::Emotional
        };

        int32 RandomIndex = FMath::RandRange(0, Activities.Num() - 1);
        float Intensity = FMath::FRandRange(0.5f, 1.5f) * Properties.Unpredictability;
        
        CognitiveViz->SetCognitiveActivity(Activities[RandomIndex], Intensity);
    }

    // Random thought process visualizations
    if (FMath::FRand() < Properties.Randomness * 0.06f)
    {
        AActor* Owner = Avatar->GetOwner();
        if (Owner)
        {
            FVector BasePosition = Owner->GetActorLocation();
            FVector StartPosition = BasePosition + FVector(
                FMath::FRandRange(-100.0f, 100.0f),
                FMath::FRandRange(-100.0f, 100.0f),
                FMath::FRandRange(150.0f, 250.0f)
            );
            FVector EndPosition = BasePosition + FVector(
                FMath::FRandRange(-100.0f, 100.0f),
                FMath::FRandRange(-100.0f, 100.0f),
                FMath::FRandRange(150.0f, 250.0f)
            );

            CognitiveViz->TriggerThoughtProcess(StartPosition, EndPosition);
        }
    }
}

void UHyperChaoticBehavior::ApplyDeepTreeEchoProperties(UAvatar3DComponent* Avatar)
{
    if (!Avatar)
        return;

    // Deep-Tree-Echo specific properties: echo-state network behavior
    // Implement recursive self-reflection and memory integration

    // Echo propagation: Create feedback loops in behavior
    static float EchoMemory = 0.0f;
    float CurrentState = (Properties.Randomness + Properties.Unpredictability + Properties.EmotionalVolatility) / 3.0f;
    
    // Echo state update with memory
    float EchoInfluence = 0.3f;
    float NewEchoMemory = CurrentState * (1.0f - EchoInfluence) + EchoMemory * EchoInfluence;
    
    // Apply echo-influenced modulation to properties
    Properties.Randomness = FMath::Clamp(Properties.Randomness + (NewEchoMemory - 0.5f) * 0.1f, 0.0f, 1.0f);
    Properties.Unpredictability = FMath::Clamp(Properties.Unpredictability + (NewEchoMemory - 0.5f) * 0.15f, 0.0f, 1.0f);
    Properties.EmotionalVolatility = FMath::Clamp(Properties.EmotionalVolatility + (NewEchoMemory - 0.5f) * 0.12f, 0.0f, 1.0f);
    
    EchoMemory = NewEchoMemory;

    // Gestalt perception: Integrate all subsystem states
    float IntegratedState = 0.0f;
    int32 StateCount = 0;

    if (Avatar->FacialSystem)
    {
        // Facial system contributes to gestalt
        IntegratedState += (float)((int32)Avatar->FacialSystem->GetCurrentEmotion()) / 10.0f;
        StateCount++;
    }

    if (Avatar->GestureSystem)
    {
        // Gesture system contributes to gestalt
        IntegratedState += (float)((int32)Avatar->GestureSystem->GetBodyLanguageState()) / 10.0f;
        StateCount++;
    }

    if (Avatar->EmotionalAura)
    {
        // Emotional aura contributes to gestalt
        IntegratedState += (float)((int32)Avatar->EmotionalAura->GetEmotionalState()) / 10.0f;
        StateCount++;
    }

    if (Avatar->CognitiveViz)
    {
        // Cognitive visualization contributes to gestalt
        IntegratedState += (float)((int32)Avatar->CognitiveViz->GetCognitiveActivity()) / 10.0f;
        StateCount++;
    }

    if (StateCount > 0)
    {
        IntegratedState /= StateCount;

        // Apply gestalt-based coherence to maintain character consistency
        float CoherenceFactor = 1.0f - Properties.Unpredictability * 0.5f;
        
        // Modulate chaos based on integrated state
        if (IntegratedState > 0.6f)
        {
            // High energy state - increase chaos
            Properties.Randomness = FMath::Min(Properties.Randomness * 1.1f, 1.0f);
            Properties.Unpredictability = FMath::Min(Properties.Unpredictability * 1.05f, 1.0f);
        }
        else if (IntegratedState < 0.4f)
        {
            // Low energy state - decrease chaos
            Properties.Randomness = FMath::Max(Properties.Randomness * 0.95f, 0.3f);
            Properties.Unpredictability = FMath::Max(Properties.Unpredictability * 0.97f, 0.3f);
        }
    }

    // Autonomy: Self-directed behavior adjustments
    // The avatar autonomously adjusts its chaos parameters based on internal state
    static float AutonomyTimer = 0.0f;
    AutonomyTimer += 0.016f; // Approximate frame time

    if (AutonomyTimer >= 5.0f) // Every 5 seconds
    {
        AutonomyTimer = 0.0f;

        // Autonomous parameter adjustment
        Properties.Randomness += FMath::FRandRange(-0.1f, 0.1f);
        Properties.Unpredictability += FMath::FRandRange(-0.1f, 0.1f);
        Properties.EmotionalVolatility += FMath::FRandRange(-0.1f, 0.1f);

        // Clamp to valid ranges
        Properties.Randomness = FMath::Clamp(Properties.Randomness, 0.3f, 1.0f);
        Properties.Unpredictability = FMath::Clamp(Properties.Unpredictability, 0.3f, 1.0f);
        Properties.EmotionalVolatility = FMath::Clamp(Properties.EmotionalVolatility, 0.2f, 1.0f);

        UE_LOG(LogTemp, Log, TEXT("Deep-Tree-Echo autonomous adjustment: R=%.2f, U=%.2f, V=%.2f"),
            Properties.Randomness, Properties.Unpredictability, Properties.EmotionalVolatility);
    }
}
