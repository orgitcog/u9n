// EmotionalSystem.cpp
// Implementation of Affective Computing and Emotional Processing

#include "EmotionalSystem.h"
#include "../Core/CognitiveCycleManager.h"
#include "../Memory/MemorySystems.h"
#include "../Attention/AttentionSystem.h"
#include "Math/UnrealMathUtility.h"

UEmotionalSystem::UEmotionalSystem()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UEmotionalSystem::BeginPlay()
{
    Super::BeginPlay();
    FindComponentReferences();
    InitializeEmotionalSystem();
}

void UEmotionalSystem::TickComponent(float DeltaTime, ELevelTick TickType, 
                                      FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    UpdateEmotionDecay(DeltaTime);
    UpdateMood(DeltaTime);
}

void UEmotionalSystem::FindComponentReferences()
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        CycleManager = Owner->FindComponentByClass<UCognitiveCycleManager>();
        MemoryComponent = Owner->FindComponentByClass<UMemorySystems>();
        AttentionComponent = Owner->FindComponentByClass<UAttentionSystem>();
    }
}

void UEmotionalSystem::InitializeEmotionalSystem()
{
    // Initialize dimensional state
    EmotionalState.Dimensional.Pleasure = BaselineValence;
    EmotionalState.Dimensional.Arousal = BaselineArousal;
    EmotionalState.Dimensional.Dominance = 0.5f;

    // Initialize categorical state
    EmotionalState.Categorical.PrimaryEmotion = EBasicEmotion::Neutral;
    EmotionalState.Categorical.PrimaryIntensity = 0.0f;
    EmotionalState.Categorical.EmotionProfile.Add(EBasicEmotion::Joy, 0.0f);
    EmotionalState.Categorical.EmotionProfile.Add(EBasicEmotion::Sadness, 0.0f);
    EmotionalState.Categorical.EmotionProfile.Add(EBasicEmotion::Fear, 0.0f);
    EmotionalState.Categorical.EmotionProfile.Add(EBasicEmotion::Anger, 0.0f);
    EmotionalState.Categorical.EmotionProfile.Add(EBasicEmotion::Surprise, 0.0f);
    EmotionalState.Categorical.EmotionProfile.Add(EBasicEmotion::Disgust, 0.0f);
    EmotionalState.Categorical.EmotionProfile.Add(EBasicEmotion::Trust, 0.0f);
    EmotionalState.Categorical.EmotionProfile.Add(EBasicEmotion::Anticipation, 0.0f);

    EmotionalState.CurrentMood = EMoodState::Calm;
    EmotionalState.MoodStability = 1.0f;
    EmotionalState.EmotionalReactivity = 0.5f;
    EmotionalState.RegulationCapacity = 0.5f;

    SomaticMarkers.Empty();
    EmotionalEvents.Empty();
}

// ========================================
// EMOTION TRIGGERING
// ========================================

void UEmotionalSystem::TriggerEmotion(EBasicEmotion Emotion, float Intensity, const FString& Trigger)
{
    Intensity = FMath::Clamp(Intensity, 0.0f, 1.0f);

    // Apply reactivity
    Intensity *= EmotionalState.EmotionalReactivity;

    // Update categorical emotion
    if (EmotionalState.Categorical.EmotionProfile.Contains(Emotion))
    {
        EmotionalState.Categorical.EmotionProfile[Emotion] = 
            FMath::Max(EmotionalState.Categorical.EmotionProfile[Emotion], Intensity);
    }

    // Update primary/secondary
    if (Intensity > EmotionalState.Categorical.PrimaryIntensity)
    {
        EmotionalState.Categorical.SecondaryEmotion = EmotionalState.Categorical.PrimaryEmotion;
        EmotionalState.Categorical.SecondaryIntensity = EmotionalState.Categorical.PrimaryIntensity;
        EmotionalState.Categorical.PrimaryEmotion = Emotion;
        EmotionalState.Categorical.PrimaryIntensity = Intensity;
    }
    else if (Intensity > EmotionalState.Categorical.SecondaryIntensity)
    {
        EmotionalState.Categorical.SecondaryEmotion = Emotion;
        EmotionalState.Categorical.SecondaryIntensity = Intensity;
    }

    // Update dimensional from categorical
    UpdateDimensionalFromCategorical();

    // Record event
    RecordEmotionalEvent(Emotion, Intensity, Trigger);

    OnEmotionTriggered.Broadcast(Emotion, Intensity);
}

void UEmotionalSystem::TriggerComplexEmotion(EComplexEmotion Emotion, float Intensity, const FString& Trigger)
{
    // Map complex emotions to basic emotion combinations
    switch (Emotion)
    {
        case EComplexEmotion::Pride:
            TriggerEmotion(EBasicEmotion::Joy, Intensity * 0.7f, Trigger);
            TriggerEmotion(EBasicEmotion::Trust, Intensity * 0.3f, Trigger);
            break;
        case EComplexEmotion::Shame:
            TriggerEmotion(EBasicEmotion::Sadness, Intensity * 0.5f, Trigger);
            TriggerEmotion(EBasicEmotion::Fear, Intensity * 0.5f, Trigger);
            break;
        case EComplexEmotion::Guilt:
            TriggerEmotion(EBasicEmotion::Sadness, Intensity * 0.6f, Trigger);
            TriggerEmotion(EBasicEmotion::Fear, Intensity * 0.4f, Trigger);
            break;
        case EComplexEmotion::Envy:
            TriggerEmotion(EBasicEmotion::Sadness, Intensity * 0.4f, Trigger);
            TriggerEmotion(EBasicEmotion::Anger, Intensity * 0.6f, Trigger);
            break;
        case EComplexEmotion::Gratitude:
            TriggerEmotion(EBasicEmotion::Joy, Intensity * 0.6f, Trigger);
            TriggerEmotion(EBasicEmotion::Trust, Intensity * 0.4f, Trigger);
            break;
        case EComplexEmotion::Compassion:
            TriggerEmotion(EBasicEmotion::Sadness, Intensity * 0.3f, Trigger);
            TriggerEmotion(EBasicEmotion::Trust, Intensity * 0.7f, Trigger);
            break;
        case EComplexEmotion::Awe:
            TriggerEmotion(EBasicEmotion::Surprise, Intensity * 0.5f, Trigger);
            TriggerEmotion(EBasicEmotion::Fear, Intensity * 0.3f, Trigger);
            TriggerEmotion(EBasicEmotion::Joy, Intensity * 0.2f, Trigger);
            break;
        case EComplexEmotion::Contempt:
            TriggerEmotion(EBasicEmotion::Disgust, Intensity * 0.6f, Trigger);
            TriggerEmotion(EBasicEmotion::Anger, Intensity * 0.4f, Trigger);
            break;
        case EComplexEmotion::Love:
            TriggerEmotion(EBasicEmotion::Joy, Intensity * 0.5f, Trigger);
            TriggerEmotion(EBasicEmotion::Trust, Intensity * 0.5f, Trigger);
            break;
        case EComplexEmotion::Hope:
            TriggerEmotion(EBasicEmotion::Anticipation, Intensity * 0.6f, Trigger);
            TriggerEmotion(EBasicEmotion::Joy, Intensity * 0.4f, Trigger);
            break;
    }
}

void UEmotionalSystem::SetDimensionalEmotion(float Pleasure, float Arousal, float Dominance)
{
    EmotionalState.Dimensional.Pleasure = FMath::Clamp(Pleasure, -1.0f, 1.0f);
    EmotionalState.Dimensional.Arousal = FMath::Clamp(Arousal, 0.0f, 1.0f);
    EmotionalState.Dimensional.Dominance = FMath::Clamp(Dominance, 0.0f, 1.0f);

    UpdateCategoricalFromDimensional();
}

void UEmotionalSystem::BlendEmotion(EBasicEmotion Emotion, float Intensity, float BlendFactor)
{
    BlendFactor = FMath::Clamp(BlendFactor, 0.0f, 1.0f);

    if (EmotionalState.Categorical.EmotionProfile.Contains(Emotion))
    {
        float Current = EmotionalState.Categorical.EmotionProfile[Emotion];
        EmotionalState.Categorical.EmotionProfile[Emotion] = 
            FMath::Lerp(Current, Intensity, BlendFactor);
    }

    // Find new primary
    float MaxIntensity = 0.0f;
    EBasicEmotion MaxEmotion = EBasicEmotion::Neutral;
    for (const auto& Pair : EmotionalState.Categorical.EmotionProfile)
    {
        if (Pair.Value > MaxIntensity)
        {
            MaxIntensity = Pair.Value;
            MaxEmotion = Pair.Key;
        }
    }

    EmotionalState.Categorical.PrimaryEmotion = MaxEmotion;
    EmotionalState.Categorical.PrimaryIntensity = MaxIntensity;

    UpdateDimensionalFromCategorical();
}

// ========================================
// APPRAISAL
// ========================================

FAppraisalResult UEmotionalSystem::AppraiseSituation(const FString& Situation, float Relevance, 
                                                      float Congruence, float Controllability)
{
    FAppraisalResult Result;
    Result.Relevance = FMath::Clamp(Relevance, 0.0f, 1.0f);
    Result.Congruence = FMath::Clamp(Congruence, -1.0f, 1.0f);
    Result.Controllability = FMath::Clamp(Controllability, 0.0f, 1.0f);
    Result.Novelty = 0.5f;
    Result.Certainty = 0.5f;

    // Determine resulting emotion based on appraisal dimensions
    if (Result.Relevance < 0.3f)
    {
        Result.ResultingEmotion = EBasicEmotion::Neutral;
        Result.ResultingIntensity = 0.1f;
    }
    else if (Result.Congruence > 0.5f)
    {
        // Goal congruent - positive emotions
        if (Result.Controllability > 0.5f)
        {
            Result.ResultingEmotion = EBasicEmotion::Joy;
        }
        else
        {
            Result.ResultingEmotion = EBasicEmotion::Trust;
        }
        Result.ResultingIntensity = Result.Relevance * Result.Congruence;
    }
    else if (Result.Congruence < -0.3f)
    {
        // Goal incongruent - negative emotions
        if (Result.Controllability > 0.5f)
        {
            Result.ResultingEmotion = EBasicEmotion::Anger;
        }
        else
        {
            Result.ResultingEmotion = EBasicEmotion::Fear;
        }
        Result.ResultingIntensity = Result.Relevance * FMath::Abs(Result.Congruence);
    }
    else
    {
        Result.ResultingEmotion = EBasicEmotion::Anticipation;
        Result.ResultingIntensity = Result.Relevance * 0.5f;
    }

    // Trigger the resulting emotion
    TriggerEmotion(Result.ResultingEmotion, Result.ResultingIntensity, Situation);

    OnAppraisalComplete.Broadcast(Result);

    return Result;
}

FAppraisalResult UEmotionalSystem::AppraiseEvent(const FString& EventDescription, 
                                                  const TArray<FString>& ContextTags)
{
    // Simple heuristic appraisal based on tags
    float Relevance = 0.5f;
    float Congruence = 0.0f;
    float Controllability = 0.5f;

    for (const FString& Tag : ContextTags)
    {
        if (Tag.Contains(TEXT("important")) || Tag.Contains(TEXT("urgent")))
        {
            Relevance += 0.3f;
        }
        if (Tag.Contains(TEXT("success")) || Tag.Contains(TEXT("good")))
        {
            Congruence += 0.4f;
        }
        if (Tag.Contains(TEXT("failure")) || Tag.Contains(TEXT("bad")))
        {
            Congruence -= 0.4f;
        }
        if (Tag.Contains(TEXT("control")) || Tag.Contains(TEXT("power")))
        {
            Controllability += 0.2f;
        }
        if (Tag.Contains(TEXT("helpless")) || Tag.Contains(TEXT("trapped")))
        {
            Controllability -= 0.3f;
        }
    }

    return AppraiseSituation(EventDescription, Relevance, Congruence, Controllability);
}

// ========================================
// SOMATIC MARKERS
// ========================================

FSomaticMarker UEmotionalSystem::CreateSomaticMarker(const FString& Context, float Valence, 
                                                      EBasicEmotion Emotion)
{
    FSomaticMarker Marker;
    Marker.MarkerID = GenerateMarkerID();
    Marker.AssociatedContext = Context;
    Marker.Valence = FMath::Clamp(Valence, -1.0f, 1.0f);
    Marker.Intensity = FMath::Abs(Valence);
    Marker.AssociatedEmotion = Emotion;
    Marker.CreationTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    Marker.ActivationCount = 0;

    SomaticMarkers.Add(Marker);

    // Limit markers
    while (SomaticMarkers.Num() > MaxSomaticMarkers)
    {
        SomaticMarkers.RemoveAt(0);
    }

    return Marker;
}

FSomaticMarker UEmotionalSystem::ActivateSomaticMarker(const FString& Context)
{
    int32 Index = FindMarkerIndex(Context);
    if (Index >= 0)
    {
        SomaticMarkers[Index].ActivationCount++;

        // Trigger associated emotion
        TriggerEmotion(SomaticMarkers[Index].AssociatedEmotion, 
                       SomaticMarkers[Index].Intensity * 0.5f, 
                       FString::Printf(TEXT("Somatic: %s"), *Context));

        OnSomaticMarkerActivated.Broadcast(SomaticMarkers[Index]);

        return SomaticMarkers[Index];
    }

    return FSomaticMarker();
}

FSomaticMarker UEmotionalSystem::GetSomaticMarker(const FString& Context) const
{
    int32 Index = FindMarkerIndex(Context);
    if (Index >= 0)
    {
        return SomaticMarkers[Index];
    }
    return FSomaticMarker();
}

TArray<FSomaticMarker> UEmotionalSystem::GetAllSomaticMarkers() const
{
    return SomaticMarkers;
}

float UEmotionalSystem::GetSomaticValence(const FString& Context) const
{
    int32 Index = FindMarkerIndex(Context);
    if (Index >= 0)
    {
        return SomaticMarkers[Index].Valence;
    }
    return 0.0f;
}

// ========================================
// STATE QUERIES
// ========================================

FEmotionalState UEmotionalSystem::GetEmotionalState() const
{
    return EmotionalState;
}

FDimensionalEmotion UEmotionalSystem::GetDimensionalEmotion() const
{
    return EmotionalState.Dimensional;
}

FCategoricalEmotion UEmotionalSystem::GetCategoricalEmotion() const
{
    return EmotionalState.Categorical;
}

EMoodState UEmotionalSystem::GetCurrentMood() const
{
    return EmotionalState.CurrentMood;
}

EBasicEmotion UEmotionalSystem::GetPrimaryEmotion() const
{
    return EmotionalState.Categorical.PrimaryEmotion;
}

float UEmotionalSystem::GetPrimaryEmotionIntensity() const
{
    return EmotionalState.Categorical.PrimaryIntensity;
}

float UEmotionalSystem::GetValence() const
{
    return EmotionalState.Dimensional.Pleasure;
}

float UEmotionalSystem::GetArousal() const
{
    return EmotionalState.Dimensional.Arousal;
}

TArray<FEmotionalEvent> UEmotionalSystem::GetRecentEvents(int32 Count) const
{
    TArray<FEmotionalEvent> Recent;
    int32 StartIndex = FMath::Max(0, EmotionalEvents.Num() - Count);
    for (int32 i = StartIndex; i < EmotionalEvents.Num(); ++i)
    {
        Recent.Add(EmotionalEvents[i]);
    }
    return Recent;
}

// ========================================
// EMOTION REGULATION
// ========================================

void UEmotionalSystem::RegulateEmotion(float SuppressionFactor)
{
    SuppressionFactor = FMath::Clamp(SuppressionFactor, 0.0f, 1.0f);

    // Apply regulation capacity
    float EffectiveSuppression = SuppressionFactor * EmotionalState.RegulationCapacity;

    // Reduce all emotion intensities
    for (auto& Pair : EmotionalState.Categorical.EmotionProfile)
    {
        Pair.Value *= (1.0f - EffectiveSuppression);
    }

    EmotionalState.Categorical.PrimaryIntensity *= (1.0f - EffectiveSuppression);
    EmotionalState.Categorical.SecondaryIntensity *= (1.0f - EffectiveSuppression);

    // Move dimensional toward baseline
    EmotionalState.Dimensional.Pleasure = FMath::Lerp(EmotionalState.Dimensional.Pleasure, 
                                                       BaselineValence, EffectiveSuppression);
    EmotionalState.Dimensional.Arousal = FMath::Lerp(EmotionalState.Dimensional.Arousal, 
                                                      BaselineArousal, EffectiveSuppression);
}

void UEmotionalSystem::Reappraise(const FString& NewInterpretation)
{
    // Reappraisal shifts valence toward neutral
    float ShiftAmount = 0.2f * EmotionalState.RegulationCapacity;

    if (EmotionalState.Dimensional.Pleasure < 0.0f)
    {
        EmotionalState.Dimensional.Pleasure += ShiftAmount;
    }
    else
    {
        EmotionalState.Dimensional.Pleasure -= ShiftAmount * 0.5f;
    }

    UpdateCategoricalFromDimensional();
}

void UEmotionalSystem::AttentionalDeployment(bool bTowardsPositive)
{
    float ShiftAmount = 0.1f * EmotionalState.RegulationCapacity;

    if (bTowardsPositive)
    {
        EmotionalState.Dimensional.Pleasure += ShiftAmount;
        BlendEmotion(EBasicEmotion::Joy, 0.2f, 0.3f);
    }
    else
    {
        EmotionalState.Dimensional.Arousal -= ShiftAmount;
    }
}

void UEmotionalSystem::ResetToBaseline()
{
    EmotionalState.Dimensional.Pleasure = BaselineValence;
    EmotionalState.Dimensional.Arousal = BaselineArousal;
    EmotionalState.Dimensional.Dominance = 0.5f;

    for (auto& Pair : EmotionalState.Categorical.EmotionProfile)
    {
        Pair.Value = 0.0f;
    }

    EmotionalState.Categorical.PrimaryEmotion = EBasicEmotion::Neutral;
    EmotionalState.Categorical.PrimaryIntensity = 0.0f;
    EmotionalState.Categorical.SecondaryEmotion = EBasicEmotion::Neutral;
    EmotionalState.Categorical.SecondaryIntensity = 0.0f;

    EmotionalState.CurrentMood = EMoodState::Calm;
}

// ========================================
// INTERNAL METHODS
// ========================================

void UEmotionalSystem::UpdateEmotionDecay(float DeltaTime)
{
    // Decay all emotion intensities toward zero
    for (auto& Pair : EmotionalState.Categorical.EmotionProfile)
    {
        Pair.Value = FMath::Max(0.0f, Pair.Value - EmotionDecayRate * DeltaTime);
    }

    EmotionalState.Categorical.PrimaryIntensity = 
        FMath::Max(0.0f, EmotionalState.Categorical.PrimaryIntensity - EmotionDecayRate * DeltaTime);
    EmotionalState.Categorical.SecondaryIntensity = 
        FMath::Max(0.0f, EmotionalState.Categorical.SecondaryIntensity - EmotionDecayRate * DeltaTime);

    // Decay dimensional toward baseline
    float DecayFactor = EmotionDecayRate * DeltaTime;
    EmotionalState.Dimensional.Pleasure = FMath::Lerp(EmotionalState.Dimensional.Pleasure, 
                                                       BaselineValence, DecayFactor);
    EmotionalState.Dimensional.Arousal = FMath::Lerp(EmotionalState.Dimensional.Arousal, 
                                                      BaselineArousal, DecayFactor);

    // Find new primary if current decayed
    if (EmotionalState.Categorical.PrimaryIntensity < 0.1f)
    {
        float MaxIntensity = 0.0f;
        EBasicEmotion MaxEmotion = EBasicEmotion::Neutral;
        for (const auto& Pair : EmotionalState.Categorical.EmotionProfile)
        {
            if (Pair.Value > MaxIntensity)
            {
                MaxIntensity = Pair.Value;
                MaxEmotion = Pair.Key;
            }
        }
        EmotionalState.Categorical.PrimaryEmotion = MaxEmotion;
        EmotionalState.Categorical.PrimaryIntensity = MaxIntensity;
    }
}

void UEmotionalSystem::UpdateMood(float DeltaTime)
{
    EMoodState NewMood = ComputeMoodFromDimensional();

    if (NewMood != EmotionalState.CurrentMood)
    {
        // Apply mood inertia
        float InertiaFactor = MoodInertia * EmotionalState.MoodStability;
        if (FMath::FRand() > InertiaFactor)
        {
            EMoodState OldMood = EmotionalState.CurrentMood;
            EmotionalState.CurrentMood = NewMood;
            OnMoodChanged.Broadcast(OldMood, NewMood);
        }
    }
}

void UEmotionalSystem::UpdateCategoricalFromDimensional()
{
    EBasicEmotion MappedEmotion = MapDimensionalToBasic(EmotionalState.Dimensional.Pleasure, 
                                                         EmotionalState.Dimensional.Arousal);

    float Intensity = FMath::Abs(EmotionalState.Dimensional.Pleasure) * EmotionalState.Dimensional.Arousal;

    if (EmotionalState.Categorical.EmotionProfile.Contains(MappedEmotion))
    {
        EmotionalState.Categorical.EmotionProfile[MappedEmotion] = 
            FMath::Max(EmotionalState.Categorical.EmotionProfile[MappedEmotion], Intensity);
    }
}

void UEmotionalSystem::UpdateDimensionalFromCategorical()
{
    FDimensionalEmotion Mapped = MapBasicToDimensional(EmotionalState.Categorical.PrimaryEmotion, 
                                                        EmotionalState.Categorical.PrimaryIntensity);

    // Blend with current state
    EmotionalState.Dimensional.Pleasure = FMath::Lerp(EmotionalState.Dimensional.Pleasure, 
                                                       Mapped.Pleasure, 0.5f);
    EmotionalState.Dimensional.Arousal = FMath::Lerp(EmotionalState.Dimensional.Arousal, 
                                                      Mapped.Arousal, 0.5f);
}

EMoodState UEmotionalSystem::ComputeMoodFromDimensional() const
{
    float P = EmotionalState.Dimensional.Pleasure;
    float A = EmotionalState.Dimensional.Arousal;

    if (P > 0.5f && A > 0.7f) return EMoodState::Elated;
    if (P > 0.3f && A > 0.5f) return EMoodState::Happy;
    if (P > 0.1f && A < 0.5f) return EMoodState::Content;
    if (P > -0.1f && A < 0.3f) return EMoodState::Calm;
    if (P < 0.0f && A < 0.3f) return EMoodState::Bored;
    if (P < -0.3f && A < 0.5f) return EMoodState::Sad;
    if (P < -0.1f && A > 0.5f) return EMoodState::Anxious;
    if (P < -0.3f && A > 0.7f) return EMoodState::Stressed;
    if (P < -0.5f && A > 0.5f) return EMoodState::Angry;

    return EMoodState::Calm;
}

EBasicEmotion UEmotionalSystem::MapDimensionalToBasic(float Pleasure, float Arousal) const
{
    if (Pleasure > 0.3f)
    {
        if (Arousal > 0.6f) return EBasicEmotion::Joy;
        if (Arousal > 0.3f) return EBasicEmotion::Trust;
        return EBasicEmotion::Anticipation;
    }
    else if (Pleasure < -0.3f)
    {
        if (Arousal > 0.6f) return EBasicEmotion::Anger;
        if (Arousal > 0.4f) return EBasicEmotion::Fear;
        return EBasicEmotion::Sadness;
    }
    else
    {
        if (Arousal > 0.7f) return EBasicEmotion::Surprise;
        return EBasicEmotion::Neutral;
    }
}

FDimensionalEmotion UEmotionalSystem::MapBasicToDimensional(EBasicEmotion Emotion, float Intensity) const
{
    FDimensionalEmotion Result;
    Result.Dominance = 0.5f;

    switch (Emotion)
    {
        case EBasicEmotion::Joy:
            Result.Pleasure = 0.8f * Intensity;
            Result.Arousal = 0.7f * Intensity;
            Result.Dominance = 0.7f;
            break;
        case EBasicEmotion::Sadness:
            Result.Pleasure = -0.7f * Intensity;
            Result.Arousal = 0.2f;
            Result.Dominance = 0.3f;
            break;
        case EBasicEmotion::Fear:
            Result.Pleasure = -0.6f * Intensity;
            Result.Arousal = 0.8f * Intensity;
            Result.Dominance = 0.2f;
            break;
        case EBasicEmotion::Anger:
            Result.Pleasure = -0.5f * Intensity;
            Result.Arousal = 0.9f * Intensity;
            Result.Dominance = 0.8f;
            break;
        case EBasicEmotion::Surprise:
            Result.Pleasure = 0.0f;
            Result.Arousal = 0.9f * Intensity;
            Result.Dominance = 0.5f;
            break;
        case EBasicEmotion::Disgust:
            Result.Pleasure = -0.6f * Intensity;
            Result.Arousal = 0.5f * Intensity;
            Result.Dominance = 0.6f;
            break;
        case EBasicEmotion::Trust:
            Result.Pleasure = 0.5f * Intensity;
            Result.Arousal = 0.3f;
            Result.Dominance = 0.5f;
            break;
        case EBasicEmotion::Anticipation:
            Result.Pleasure = 0.3f * Intensity;
            Result.Arousal = 0.6f * Intensity;
            Result.Dominance = 0.6f;
            break;
        default:
            Result.Pleasure = 0.0f;
            Result.Arousal = BaselineArousal;
            break;
    }

    return Result;
}

void UEmotionalSystem::RecordEmotionalEvent(EBasicEmotion Emotion, float Intensity, const FString& Trigger)
{
    FEmotionalEvent Event;
    Event.EventID = GenerateEventID();
    Event.Trigger = Trigger;
    Event.Emotion = Emotion;
    Event.Intensity = Intensity;
    Event.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    EmotionalEvents.Add(Event);

    // Limit events
    while (EmotionalEvents.Num() > MaxEmotionalEvents)
    {
        EmotionalEvents.RemoveAt(0);
    }

    OnEmotionalEvent.Broadcast(Event);
}

FString UEmotionalSystem::GenerateMarkerID()
{
    return FString::Printf(TEXT("SM_%d_%d"), ++MarkerIDCounter, FMath::RandRange(1000, 9999));
}

FString UEmotionalSystem::GenerateEventID()
{
    return FString::Printf(TEXT("EE_%d_%d"), ++EventIDCounter, FMath::RandRange(1000, 9999));
}

int32 UEmotionalSystem::FindMarkerIndex(const FString& Context) const
{
    for (int32 i = 0; i < SomaticMarkers.Num(); ++i)
    {
        if (SomaticMarkers[i].AssociatedContext.Contains(Context) || 
            Context.Contains(SomaticMarkers[i].AssociatedContext))
        {
            return i;
        }
    }
    return -1;
}
