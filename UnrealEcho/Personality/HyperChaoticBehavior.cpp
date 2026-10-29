#include "HyperChaoticBehavior.h"
#include "Avatar3DComponent.h"
#include "FacialAnimationSystem.h"
#include "GestureSystem.h"
#include "EmotionalAuraComponent.h"
#include "CognitiveVisualizationComponent.h"
#include "Neurochemical/NeurochemicalSimulationComponent.h"
#include "DTELanguageModelBridge.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "UObject/Class.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    /** The expression the avatar is asked to show. Every part is optional. */
    struct FExpressionChoice
    {
        bool bHasEmotion = false;
        EEmotionState Emotion = EEmotionState::Neutral;

        bool bHasMicroExpression = false;
        EEmotionState MicroExpression = EEmotionState::Neutral;

        bool bHasGesture = false;
        EGestureType Gesture = EGestureType::Wave;

        bool bHasBodyLanguage = false;
        EBodyLanguageState BodyLanguage = EBodyLanguageState::Neutral;

        bool bHasAura = false;
        EAuraEmotionType Aura = EAuraEmotionType::Neutral;

        bool bHasActivity = false;
        ECognitiveActivityType Activity = ECognitiveActivityType::Idle;
    };

    template <typename TEnum>
    FString EnumMenu()
    {
        const UEnum* Enum = StaticEnum<TEnum>();
        FString Menu;
        // The last entry of a reflected enum is the generated _MAX sentinel.
        for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index)
        {
            if (!Menu.IsEmpty())
            {
                Menu += TEXT(", ");
            }
            Menu += Enum->GetNameStringByIndex(Index);
        }
        return Menu;
    }

    template <typename TEnum>
    FString EnumName(TEnum Value)
    {
        return StaticEnum<TEnum>()->GetNameStringByValue(static_cast<int64>(Value));
    }

    /** Reads an enum name from the model's JSON. "none" or an unknown name leaves OutValue untouched. */
    template <typename TEnum>
    bool ParseEnumField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, TEnum& OutValue)
    {
        FString Text;
        if (!Object->TryGetStringField(FString(Key), Text))
        {
            return false;
        }
        Text.TrimStartAndEndInline();
        if (Text.IsEmpty() || Text.Equals(TEXT("none"), ESearchCase::IgnoreCase))
        {
            return false;
        }
        const int64 Value = StaticEnum<TEnum>()->GetValueByNameString(Text);
        if (Value == INDEX_NONE)
        {
            UE_LOG(LogTemp, Warning, TEXT("HyperChaoticBehavior: model answered \"%s\" for %s, which is not on the menu"), *Text, Key);
            return false;
        }
        OutValue = static_cast<TEnum>(Value);
        return true;
    }

    TSharedPtr<FJsonObject> ParseModelJson(const FString& ModelText)
    {
        int32 Open = INDEX_NONE;
        int32 Close = INDEX_NONE;
        ModelText.FindChar(TEXT('{'), Open);
        ModelText.FindLastChar(TEXT('}'), Close);
        if (Open == INDEX_NONE || Close == INDEX_NONE || Close <= Open)
        {
            return nullptr;
        }

        TSharedPtr<FJsonObject> Object;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ModelText.Mid(Open, Close - Open + 1));
        if (!FJsonSerializer::Deserialize(Reader, Object) || !Object.IsValid())
        {
            return nullptr;
        }
        return Object;
    }

    /** Which affect channel dominates, and how strongly. */
    struct FDominantAffect
    {
        enum EChannel { Happiness, Excitement, Calmness, Anxiety, Affection, Focus, None };
        EChannel First = None;
        float FirstValue = 0.0f;
        EChannel Second = None;
        float SecondValue = 0.0f;
    };

    FDominantAffect RankAffect(const FHyperChaoticAffect& Affect)
    {
        const TPair<FDominantAffect::EChannel, float> Channels[] = {
            { FDominantAffect::Happiness,  Affect.Happiness },
            { FDominantAffect::Excitement, Affect.Excitement },
            { FDominantAffect::Calmness,   Affect.Calmness },
            { FDominantAffect::Anxiety,    Affect.Anxiety },
            { FDominantAffect::Affection,  Affect.Affection },
            { FDominantAffect::Focus,      Affect.Focus },
        };

        FDominantAffect Result;
        for (const auto& Channel : Channels)
        {
            if (Channel.Value > Result.FirstValue)
            {
                Result.Second = Result.First;
                Result.SecondValue = Result.FirstValue;
                Result.First = Channel.Key;
                Result.FirstValue = Channel.Value;
            }
            else if (Channel.Value > Result.SecondValue)
            {
                Result.Second = Channel.Key;
                Result.SecondValue = Channel.Value;
            }
        }

        // Below this floor nothing is felt strongly enough to show.
        const float Floor = 0.35f;
        if (Result.FirstValue < Floor)
        {
            Result.First = FDominantAffect::None;
        }
        if (Result.SecondValue < Floor)
        {
            Result.Second = FDominantAffect::None;
        }
        return Result;
    }

    EEmotionState EmotionForChannel(FDominantAffect::EChannel Channel, const FHyperChaoticAffect& Affect)
    {
        switch (Channel)
        {
            case FDominantAffect::Happiness:  return EEmotionState::Happy;
            case FDominantAffect::Excitement: return EEmotionState::Playful;
            case FDominantAffect::Affection:  return EEmotionState::Flirty;
            case FDominantAffect::Focus:      return EEmotionState::Confident;
            case FDominantAffect::Anxiety:
                // Energised, unstable anxiety reads as anger; otherwise as fear.
                return (Affect.Energy > 0.6f && Affect.Stability < 0.4f) ? EEmotionState::Angry : EEmotionState::Fearful;
            case FDominantAffect::Calmness:   return EEmotionState::Neutral;
            default:
                // Nothing dominant: flat and drained is sadness, otherwise neutral.
                return (Affect.Happiness < 0.3f && Affect.Energy < 0.3f) ? EEmotionState::Sad : EEmotionState::Neutral;
        }
    }

    bool GestureForEmotion(EEmotionState Emotion, EGestureType& OutGesture)
    {
        switch (Emotion)
        {
            case EEmotionState::Happy:     OutGesture = EGestureType::Wave;      return true;
            case EEmotionState::Playful:   OutGesture = EGestureType::Excited;   return true;
            case EEmotionState::Flirty:    OutGesture = EGestureType::Flirty;    return true;
            case EEmotionState::Confident: OutGesture = EGestureType::Confident; return true;
            case EEmotionState::Fearful:   OutGesture = EGestureType::Shrug;     return true;
            case EEmotionState::Angry:     OutGesture = EGestureType::Shake;     return true;
            default:                       return false;
        }
    }

    EBodyLanguageState BodyLanguageForAffect(const FDominantAffect& Rank, const FHyperChaoticAffect& Affect)
    {
        switch (Rank.First)
        {
            case FDominantAffect::Anxiety:    return Affect.Energy < 0.35f ? EBodyLanguageState::Closed : EBodyLanguageState::Tense;
            case FDominantAffect::Calmness:   return EBodyLanguageState::Relaxed;
            case FDominantAffect::Affection:  return Affect.SocialDesire > 0.6f ? EBodyLanguageState::Seductive : EBodyLanguageState::Open;
            case FDominantAffect::Excitement: return EBodyLanguageState::Playful;
            case FDominantAffect::Focus:      return EBodyLanguageState::Confident;
            case FDominantAffect::Happiness:  return EBodyLanguageState::Open;
            default:                          return EBodyLanguageState::Neutral;
        }
    }

    EAuraEmotionType AuraForEmotion(EEmotionState Emotion, const FHyperChaoticAffect& Affect)
    {
        switch (Emotion)
        {
            case EEmotionState::Happy:     return EAuraEmotionType::Joy;
            case EEmotionState::Playful:   return EAuraEmotionType::Excitement;
            case EEmotionState::Flirty:    return EAuraEmotionType::Love;
            case EEmotionState::Confident: return EAuraEmotionType::Confidence;
            case EEmotionState::Fearful:   return EAuraEmotionType::Fear;
            case EEmotionState::Angry:     return EAuraEmotionType::Anger;
            case EEmotionState::Sad:       return EAuraEmotionType::Sadness;
            default:                       return Affect.Calmness > 0.5f ? EAuraEmotionType::Calm : EAuraEmotionType::Neutral;
        }
    }

    ECognitiveActivityType ActivityForAffect(const FDominantAffect& Rank, const FHyperChaoticAffect& Affect)
    {
        if (Rank.First == FDominantAffect::Focus)
        {
            return Affect.Clarity > 0.6f ? ECognitiveActivityType::Analyzing : ECognitiveActivityType::Thinking;
        }
        switch (Rank.First)
        {
            case FDominantAffect::Affection:  return ECognitiveActivityType::Emotional;
            case FDominantAffect::Excitement: return ECognitiveActivityType::Creating;
            case FDominantAffect::Anxiety:    return ECognitiveActivityType::Deciding;
            default:                          return Affect.Focus > 0.5f ? ECognitiveActivityType::Thinking : ECognitiveActivityType::Idle;
        }
    }

    /**
     * Pushes a chosen expression into the avatar subsystems. The chaos
     * parameters and the chaotic driver sample only scale timing and intensity.
     */
    void ApplyChoice(UAvatar3DComponent* Avatar, const FExpressionChoice& Choice, const FHyperChaoticProperties& Props,
                     const FHyperChaoticAffect& Affect, float Chaos)
    {
        if (!Avatar)
        {
            return;
        }

        // Sharper shifts with higher unpredictability; the chaotic driver adds numeric jitter.
        const float ShiftScale = FMath::Lerp(1.0f, 0.2f, Props.Unpredictability) * FMath::Lerp(0.75f, 1.25f, Chaos);

        if (UFacialAnimationSystem* Facial = Avatar->FacialSystem)
        {
            if (Choice.bHasEmotion && Facial->GetCurrentEmotion() != Choice.Emotion)
            {
                Facial->SetEmotionState(Choice.Emotion, 0.5f * ShiftScale);
            }
            if (Choice.bHasMicroExpression && Choice.MicroExpression != EEmotionState::Neutral)
            {
                Facial->TriggerMicroExpression(Choice.MicroExpression, FMath::Lerp(0.3f, 0.1f, Props.EmotionalVolatility));
            }
        }

        if (UGestureSystem* Gestures = Avatar->GestureSystem)
        {
            if (Choice.bHasGesture)
            {
                const float Intensity = FMath::Lerp(0.7f, 1.5f, Props.Randomness) * FMath::Lerp(0.85f, 1.15f, Chaos);
                Gestures->TriggerGesture(Choice.Gesture, Intensity);
            }
            if (Choice.bHasBodyLanguage && Gestures->GetBodyLanguageState() != Choice.BodyLanguage)
            {
                Gestures->SetBodyLanguageState(Choice.BodyLanguage, 0.8f * ShiftScale);
            }
        }

        if (UEmotionalAuraComponent* Aura = Avatar->EmotionalAura)
        {
            if (Choice.bHasAura && Aura->GetEmotionalState() != Choice.Aura)
            {
                Aura->SetEmotionalState(Choice.Aura, FMath::Lerp(1.5f, 0.5f, Props.EmotionalVolatility) * FMath::Lerp(0.8f, 1.2f, Chaos));
            }

            // Aura brightness follows energy and excitement; randomness widens its range.
            const float Drive = (Affect.Energy + Affect.Excitement) * 0.5f;
            Aura->SetAuraIntensity(FMath::Lerp(0.5f, FMath::Lerp(1.0f, 1.8f, Props.Randomness), Drive));

            // A volatile character flares when excitement runs high.
            if (Affect.Excitement * Props.EmotionalVolatility > 0.3f)
            {
                const float PulseIntensity = FMath::Lerp(1.5f, 2.5f, Props.EmotionalVolatility) * FMath::Lerp(0.8f, 1.2f, Chaos);
                Aura->TriggerEmotionalPulse(PulseIntensity, FMath::Lerp(0.3f, 0.7f, Chaos));
            }
        }

        if (UCognitiveVisualizationComponent* Cognitive = Avatar->CognitiveViz)
        {
            if (Choice.bHasActivity)
            {
                Cognitive->SetCognitiveActivity(Choice.Activity, FMath::Lerp(0.5f, 1.5f, Props.Unpredictability * Chaos));

                // Thought streams are drawn whenever the mind is visibly busy; their
                // path through space is visual particle noise, not a reaction.
                if (Choice.Activity != ECognitiveActivityType::Idle)
                {
                    if (AActor* Owner = Avatar->GetOwner())
                    {
                        const FVector Base = Owner->GetActorLocation();
                        const FVector Start = Base + FVector(FMath::FRandRange(-100.0f, 100.0f), FMath::FRandRange(-100.0f, 100.0f), FMath::FRandRange(150.0f, 250.0f));
                        const FVector End = Base + FVector(FMath::FRandRange(-100.0f, 100.0f), FMath::FRandRange(-100.0f, 100.0f), FMath::FRandRange(150.0f, 250.0f));
                        Cognitive->TriggerThoughtProcess(Start, End);
                    }
                }
            }
        }
    }

    FString DescribeCurrentExpression(const UAvatar3DComponent* Avatar)
    {
        FString Out;
        if (Avatar->FacialSystem)
        {
            Out += FString::Printf(TEXT("emotion=%s "), *EnumName(Avatar->FacialSystem->GetCurrentEmotion()));
        }
        if (Avatar->GestureSystem)
        {
            Out += FString::Printf(TEXT("body_language=%s "), *EnumName(Avatar->GestureSystem->GetBodyLanguageState()));
        }
        if (Avatar->EmotionalAura)
        {
            Out += FString::Printf(TEXT("aura=%s "), *EnumName(Avatar->EmotionalAura->GetEmotionalState()));
        }
        if (Avatar->CognitiveViz)
        {
            Out += FString::Printf(TEXT("cognitive_activity=%s"), *EnumName(Avatar->CognitiveViz->GetCognitiveActivity()));
        }
        return Out.TrimStartAndEnd();
    }
}

FString FHyperChaoticAffect::Describe() const
{
    return FString::Printf(
        TEXT("happiness=%.2f excitement=%.2f calmness=%.2f anxiety=%.2f affection=%.2f focus=%.2f ")
        TEXT("energy=%.2f social_desire=%.2f clarity=%.2f stability=%.2f"),
        Happiness, Excitement, Calmness, Anxiety, Affection, Focus, Energy, SocialDesire, Clarity, Stability);
}

void UHyperChaoticBehavior::ApplyBehavior(UAvatar3DComponent* Avatar)
{
    if (!Avatar)
    {
        UE_LOG(LogTemp, Error, TEXT("HyperChaoticBehavior::ApplyBehavior - Invalid Avatar"));
        return;
    }

    const UWorld* World = Avatar->GetWorld();
    const float Now = World ? World->GetTimeSeconds() : 0.0f;

    // Numeric chaos dynamics run every call; they shape intensity and cadence only.
    UpdateChaosDynamics(Avatar, Now);

    // The expression itself is re-evaluated on the chaos cadence.
    const float Cadence = FMath::Lerp(FMath::Max(SlowestCadenceSeconds, 0.5f), FMath::Max(FastestCadenceSeconds, 0.5f),
                                      FMath::Clamp(Properties.Unpredictability, 0.0f, 1.0f));
    if (LastEvaluationTime >= 0.0f && Now - LastEvaluationTime < Cadence)
    {
        return;
    }
    if (bModelChoicePending)
    {
        return;
    }
    LastEvaluationTime = Now;

    FHyperChaoticAffect Affect;
    const bool bHasAffect = ReadAffect(Avatar, Affect);

    if (!ModelBridge)
    {
        ModelBridge = UDTELanguageModelBridge::FindOrAdd(Avatar->GetOwner());
    }

    if (ModelBridge && ModelBridge->IsModelAvailable())
    {
        RequestModelExpression(Avatar, Affect);
    }
    else if (bHasAffect)
    {
        ApplyAffectMapping(Avatar, Affect);
    }
    else
    {
        // No model and no affect source: the avatar stays as it is.
        UE_LOG(LogTemp, Verbose, TEXT("HyperChaoticBehavior: no model and no neurochemical state on %s; expression left idle"),
               *GetNameSafe(Avatar->GetOwner()));
    }
}

bool UHyperChaoticBehavior::ReadAffect(const UAvatar3DComponent* Avatar, FHyperChaoticAffect& OutAffect) const
{
    const AActor* Owner = Avatar ? Avatar->GetOwner() : nullptr;
    const UNeurochemicalSimulationComponent* Neuro = Owner ? Owner->FindComponentByClass<UNeurochemicalSimulationComponent>() : nullptr;
    if (!Neuro)
    {
        return false;
    }

    const FEmotionalChemistry Chemistry = Neuro->GetEmotionalChemistry();
    OutAffect.Happiness = FMath::Clamp(Chemistry.Happiness, 0.0f, 1.0f);
    OutAffect.Excitement = FMath::Clamp(Chemistry.Excitement, 0.0f, 1.0f);
    OutAffect.Calmness = FMath::Clamp(Chemistry.Calmness, 0.0f, 1.0f);
    OutAffect.Anxiety = FMath::Clamp(Chemistry.Anxiety, 0.0f, 1.0f);
    OutAffect.Affection = FMath::Clamp(Chemistry.Affection, 0.0f, 1.0f);
    OutAffect.Focus = FMath::Clamp(Chemistry.Focus, 0.0f, 1.0f);
    OutAffect.Energy = FMath::Clamp(Neuro->GetEnergyLevel(), 0.0f, 1.0f);
    OutAffect.SocialDesire = FMath::Clamp(Neuro->GetSocialDesire(), 0.0f, 1.0f);
    OutAffect.Clarity = FMath::Clamp(Neuro->GetCognitiveClarity(), 0.0f, 1.0f);
    OutAffect.Stability = FMath::Clamp(Neuro->GetEmotionalStability(), 0.0f, 1.0f);
    return true;
}

void UHyperChaoticBehavior::RequestModelExpression(UAvatar3DComponent* Avatar, const FHyperChaoticAffect& Affect)
{
    if (!ModelBridge || !Avatar)
    {
        return;
    }

    const FString Instruction =
        TEXT("Choose the non-verbal expression this character shows for the next few seconds. ")
        TEXT("Reply with one JSON object with the keys \"emotion\", \"micro_expression\", \"gesture\", ")
        TEXT("\"body_language\", \"aura\" and \"cognitive_activity\". Each value must be exactly one name ")
        TEXT("from the matching menu in the context; \"micro_expression\" and \"gesture\" may be \"none\" ")
        TEXT("when nothing brief should happen. Choose what this character would genuinely show given the ")
        TEXT("affect readings and the chaos profile. A high unpredictability profile means sharper, less ")
        TEXT("expected shifts are in character; it never means the choice is arbitrary.");

    const FString Context = FString::Printf(
        TEXT("chaos profile: randomness=%.2f unpredictability=%.2f emotional_volatility=%.2f\n")
        TEXT("affect readings: %s\n")
        TEXT("currently showing: %s\n")
        TEXT("emotion menu: %s\n")
        TEXT("micro_expression menu: %s, none\n")
        TEXT("gesture menu: %s, none\n")
        TEXT("body_language menu: %s\n")
        TEXT("aura menu: %s\n")
        TEXT("cognitive_activity menu: %s"),
        Properties.Randomness, Properties.Unpredictability, Properties.EmotionalVolatility,
        *Affect.Describe(),
        *DescribeCurrentExpression(Avatar),
        *EnumMenu<EEmotionState>(),
        *EnumMenu<EEmotionState>(),
        *EnumMenu<EGestureType>(),
        *EnumMenu<EBodyLanguageState>(),
        *EnumMenu<EAuraEmotionType>(),
        *EnumMenu<ECognitiveActivityType>());

    bModelChoicePending = true;

    TWeakObjectPtr<UHyperChaoticBehavior> WeakThis(this);
    TWeakObjectPtr<UAvatar3DComponent> WeakAvatar(Avatar);
    const FHyperChaoticProperties PropsAtRequest = Properties;
    const float ChaosAtRequest = SampleChaos();

    ModelBridge->RequestJson(Instruction, Context,
        [WeakThis, WeakAvatar, Affect, PropsAtRequest, ChaosAtRequest](bool bSuccess, const FString& TextOrError)
        {
            if (!WeakThis.IsValid())
            {
                return;
            }
            WeakThis->bModelChoicePending = false;

            if (!bSuccess)
            {
                UE_LOG(LogTemp, Warning, TEXT("HyperChaoticBehavior: expression not changed, model unavailable: %s"), *TextOrError);
                return;
            }

            TSharedPtr<FJsonObject> Object = ParseModelJson(TextOrError);
            if (!Object.IsValid())
            {
                UE_LOG(LogTemp, Warning, TEXT("HyperChaoticBehavior: expression not changed, model reply was not the requested JSON object"));
                return;
            }

            FExpressionChoice Choice;
            Choice.bHasEmotion = ParseEnumField(Object, TEXT("emotion"), Choice.Emotion);
            Choice.bHasMicroExpression = ParseEnumField(Object, TEXT("micro_expression"), Choice.MicroExpression);
            Choice.bHasGesture = ParseEnumField(Object, TEXT("gesture"), Choice.Gesture);
            Choice.bHasBodyLanguage = ParseEnumField(Object, TEXT("body_language"), Choice.BodyLanguage);
            Choice.bHasAura = ParseEnumField(Object, TEXT("aura"), Choice.Aura);
            Choice.bHasActivity = ParseEnumField(Object, TEXT("cognitive_activity"), Choice.Activity);

            if (!WeakAvatar.IsValid())
            {
                return;
            }
            ApplyChoice(WeakAvatar.Get(), Choice, PropsAtRequest, Affect, ChaosAtRequest);
        });
}

void UHyperChaoticBehavior::ApplyAffectMapping(UAvatar3DComponent* Avatar, const FHyperChaoticAffect& Affect)
{
    const FDominantAffect Rank = RankAffect(Affect);

    FExpressionChoice Choice;
    Choice.bHasEmotion = true;
    Choice.Emotion = EmotionForChannel(Rank.First, Affect);

    // A volatile, unstable character briefly lets the runner-up feeling through.
    if (Rank.Second != FDominantAffect::None && Properties.EmotionalVolatility * (1.0f - Affect.Stability) > 0.25f)
    {
        const EEmotionState Runner = EmotionForChannel(Rank.Second, Affect);
        if (Runner != EEmotionState::Neutral && Runner != Choice.Emotion)
        {
            Choice.bHasMicroExpression = true;
            Choice.MicroExpression = Runner;
        }
    }

    // Gesturing takes energy.
    if (Affect.Energy > 0.5f)
    {
        Choice.bHasGesture = GestureForEmotion(Choice.Emotion, Choice.Gesture);
    }

    Choice.bHasBodyLanguage = true;
    Choice.BodyLanguage = BodyLanguageForAffect(Rank, Affect);

    Choice.bHasAura = true;
    Choice.Aura = AuraForEmotion(Choice.Emotion, Affect);

    Choice.bHasActivity = true;
    Choice.Activity = ActivityForAffect(Rank, Affect);

    ApplyChoice(Avatar, Choice, Properties, Affect, SampleChaos());
}

void UHyperChaoticBehavior::UpdateChaosDynamics(UAvatar3DComponent* Avatar, float Now)
{
    // Echo propagation: the chaos parameters feed back on themselves with memory.
    const float CurrentState = (Properties.Randomness + Properties.Unpredictability + Properties.EmotionalVolatility) / 3.0f;
    const float EchoInfluence = 0.3f;
    const float NewEchoMemory = CurrentState * (1.0f - EchoInfluence) + EchoMemory * EchoInfluence;

    Properties.Randomness = FMath::Clamp(Properties.Randomness + (NewEchoMemory - 0.5f) * 0.1f, 0.0f, 1.0f);
    Properties.Unpredictability = FMath::Clamp(Properties.Unpredictability + (NewEchoMemory - 0.5f) * 0.15f, 0.0f, 1.0f);
    Properties.EmotionalVolatility = FMath::Clamp(Properties.EmotionalVolatility + (NewEchoMemory - 0.5f) * 0.12f, 0.0f, 1.0f);

    EchoMemory = NewEchoMemory;

    // Gestalt perception: integrate the subsystem states into one energy reading.
    float IntegratedState = 0.0f;
    int32 StateCount = 0;

    if (Avatar->FacialSystem)
    {
        IntegratedState += (float)((int32)Avatar->FacialSystem->GetCurrentEmotion()) / 10.0f;
        StateCount++;
    }
    if (Avatar->GestureSystem)
    {
        IntegratedState += (float)((int32)Avatar->GestureSystem->GetBodyLanguageState()) / 10.0f;
        StateCount++;
    }
    if (Avatar->EmotionalAura)
    {
        IntegratedState += (float)((int32)Avatar->EmotionalAura->GetEmotionalState()) / 10.0f;
        StateCount++;
    }
    if (Avatar->CognitiveViz)
    {
        IntegratedState += (float)((int32)Avatar->CognitiveViz->GetCognitiveActivity()) / 10.0f;
        StateCount++;
    }

    if (StateCount > 0)
    {
        IntegratedState /= StateCount;

        if (IntegratedState > 0.6f)
        {
            Properties.Randomness = FMath::Min(Properties.Randomness * 1.1f, 1.0f);
            Properties.Unpredictability = FMath::Min(Properties.Unpredictability * 1.05f, 1.0f);
        }
        else if (IntegratedState < 0.4f)
        {
            Properties.Randomness = FMath::Max(Properties.Randomness * 0.95f, 0.3f);
            Properties.Unpredictability = FMath::Max(Properties.Unpredictability * 0.97f, 0.3f);
        }
    }

    // Autonomy: every five seconds the chaos parameters drift. This is numeric
    // parameter noise, never an expression.
    if (LastAutonomyTime < 0.0f)
    {
        LastAutonomyTime = Now;
    }
    if (Now - LastAutonomyTime >= 5.0f)
    {
        LastAutonomyTime = Now;

        Properties.Randomness += FMath::FRandRange(-0.1f, 0.1f);
        Properties.Unpredictability += FMath::FRandRange(-0.1f, 0.1f);
        Properties.EmotionalVolatility += FMath::FRandRange(-0.1f, 0.1f);

        Properties.Randomness = FMath::Clamp(Properties.Randomness, 0.3f, 1.0f);
        Properties.Unpredictability = FMath::Clamp(Properties.Unpredictability, 0.3f, 1.0f);
        Properties.EmotionalVolatility = FMath::Clamp(Properties.EmotionalVolatility, 0.2f, 1.0f);

        UE_LOG(LogTemp, Log, TEXT("Deep-Tree-Echo autonomous adjustment: R=%.2f, U=%.2f, V=%.2f"),
            Properties.Randomness, Properties.Unpredictability, Properties.EmotionalVolatility);
    }
}

float UHyperChaoticBehavior::SampleChaos()
{
    // Logistic map in its chaotic regime: a deterministic chaotic driver for
    // timing and intensity jitter. Kept away from the fixed points.
    ChaosState = 3.99f * ChaosState * (1.0f - ChaosState);
    if (ChaosState < 0.01f || ChaosState > 0.99f)
    {
        ChaosState = 0.37f;
    }
    return FMath::Clamp(ChaosState, 0.0f, 1.0f);
}
