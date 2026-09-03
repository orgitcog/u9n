#include "GestureSystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"

UGestureSystem::UGestureSystem()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;

    CurrentBodyLanguage = EBodyLanguageState::Neutral;
    TargetBodyLanguage = EBodyLanguageState::Neutral;
    BodyLanguageTransitionProgress = 1.0f;
    BodyLanguageTransitionDuration = 0.5f;

    bGesturePlaying = false;
    GestureTimer = 0.0f;
    GestureDuration = 0.0f;
    GestureIntensity = 1.0f;
}

void UGestureSystem::BeginPlay()
{
    Super::BeginPlay();

    InitializeGestureLibrary();
    InitializeBodyLanguagePresets();

    // Set initial body language data
    CurrentBodyLanguageData = BodyLanguagePresets[EBodyLanguageState::Neutral];
    TargetBodyLanguageData = CurrentBodyLanguageData;

    UE_LOG(LogTemp, Log, TEXT("GestureSystem initialized with %d gestures"), GestureLibrary.Num());
}

void UGestureSystem::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    UpdateGesture(DeltaTime);
    UpdateBodyLanguageTransition(DeltaTime);
    UpdateGestureChain(DeltaTime);
}

void UGestureSystem::InitializeGestureLibrary()
{
    // Wave gesture
    FGestureData Wave;
    Wave.Type = EGestureType::Wave;
    Wave.Duration = 1.5f;
    Wave.BlendInTime = 0.2f;
    Wave.BlendOutTime = 0.3f;
    Wave.bLooping = false;
    GestureLibrary.Add(EGestureType::Wave, Wave);

    // Point gesture
    FGestureData Point;
    Point.Type = EGestureType::Point;
    Point.Duration = 1.0f;
    Point.BlendInTime = 0.15f;
    Point.BlendOutTime = 0.2f;
    Point.bLooping = false;
    GestureLibrary.Add(EGestureType::Point, Point);

    // Thumbs up gesture
    FGestureData ThumbsUp;
    ThumbsUp.Type = EGestureType::ThumbsUp;
    ThumbsUp.Duration = 1.2f;
    ThumbsUp.BlendInTime = 0.2f;
    ThumbsUp.BlendOutTime = 0.2f;
    ThumbsUp.bLooping = false;
    GestureLibrary.Add(EGestureType::ThumbsUp, ThumbsUp);

    // Thumbs down gesture
    FGestureData ThumbsDown;
    ThumbsDown.Type = EGestureType::ThumbsDown;
    ThumbsDown.Duration = 1.2f;
    ThumbsDown.BlendInTime = 0.2f;
    ThumbsDown.BlendOutTime = 0.2f;
    ThumbsDown.bLooping = false;
    GestureLibrary.Add(EGestureType::ThumbsDown, ThumbsDown);

    // Peace sign gesture
    FGestureData Peace;
    Peace.Type = EGestureType::Peace;
    Peace.Duration = 1.5f;
    Peace.BlendInTime = 0.2f;
    Peace.BlendOutTime = 0.3f;
    Peace.bLooping = false;
    GestureLibrary.Add(EGestureType::Peace, Peace);

    // Heart gesture (super-hot-girl aesthetic)
    FGestureData Heart;
    Heart.Type = EGestureType::Heart;
    Heart.Duration = 2.0f;
    Heart.BlendInTime = 0.3f;
    Heart.BlendOutTime = 0.4f;
    Heart.bLooping = false;
    GestureLibrary.Add(EGestureType::Heart, Heart);

    // Clap gesture
    FGestureData Clap;
    Clap.Type = EGestureType::Clap;
    Clap.Duration = 2.0f;
    Clap.BlendInTime = 0.1f;
    Clap.BlendOutTime = 0.2f;
    Clap.bLooping = true;
    GestureLibrary.Add(EGestureType::Clap, Clap);

    // Shrug gesture
    FGestureData Shrug;
    Shrug.Type = EGestureType::Shrug;
    Shrug.Duration = 1.5f;
    Shrug.BlendInTime = 0.2f;
    Shrug.BlendOutTime = 0.3f;
    Shrug.bLooping = false;
    GestureLibrary.Add(EGestureType::Shrug, Shrug);

    // Nod gesture
    FGestureData Nod;
    Nod.Type = EGestureType::Nod;
    Nod.Duration = 1.0f;
    Nod.BlendInTime = 0.1f;
    Nod.BlendOutTime = 0.2f;
    Nod.bLooping = false;
    GestureLibrary.Add(EGestureType::Nod, Nod);

    // Shake head gesture
    FGestureData Shake;
    Shake.Type = EGestureType::Shake;
    Shake.Duration = 1.2f;
    Shake.BlendInTime = 0.1f;
    Shake.BlendOutTime = 0.2f;
    Shake.bLooping = false;
    GestureLibrary.Add(EGestureType::Shake, Shake);

    // Bow gesture
    FGestureData Bow;
    Bow.Type = EGestureType::Bow;
    Bow.Duration = 2.0f;
    Bow.BlendInTime = 0.3f;
    Bow.BlendOutTime = 0.4f;
    Bow.bLooping = false;
    GestureLibrary.Add(EGestureType::Bow, Bow);

    // Salute gesture
    FGestureData Salute;
    Salute.Type = EGestureType::Salute;
    Salute.Duration = 1.5f;
    Salute.BlendInTime = 0.2f;
    Salute.BlendOutTime = 0.3f;
    Salute.bLooping = false;
    GestureLibrary.Add(EGestureType::Salute, Salute);

    // Thinking gesture
    FGestureData Thinking;
    Thinking.Type = EGestureType::Thinking;
    Thinking.Duration = 2.5f;
    Thinking.BlendInTime = 0.4f;
    Thinking.BlendOutTime = 0.4f;
    Thinking.bLooping = false;
    GestureLibrary.Add(EGestureType::Thinking, Thinking);

    // Excited gesture
    FGestureData Excited;
    Excited.Type = EGestureType::Excited;
    Excited.Duration = 2.0f;
    Excited.BlendInTime = 0.2f;
    Excited.BlendOutTime = 0.3f;
    Excited.bLooping = false;
    GestureLibrary.Add(EGestureType::Excited, Excited);

    // Confident pose (super-hot-girl aesthetic)
    FGestureData Confident;
    Confident.Type = EGestureType::Confident;
    Confident.Duration = 3.0f;
    Confident.BlendInTime = 0.5f;
    Confident.BlendOutTime = 0.5f;
    Confident.bLooping = false;
    GestureLibrary.Add(EGestureType::Confident, Confident);

    // Flirty gesture (super-hot-girl aesthetic)
    FGestureData Flirty;
    Flirty.Type = EGestureType::Flirty;
    Flirty.Duration = 2.5f;
    Flirty.BlendInTime = 0.4f;
    Flirty.BlendOutTime = 0.5f;
    Flirty.bLooping = false;
    GestureLibrary.Add(EGestureType::Flirty, Flirty);

    // Playful gesture (super-hot-girl aesthetic)
    FGestureData Playful;
    Playful.Type = EGestureType::Playful;
    Playful.Duration = 2.0f;
    Playful.BlendInTime = 0.3f;
    Playful.BlendOutTime = 0.4f;
    Playful.bLooping = false;
    GestureLibrary.Add(EGestureType::Playful, Playful);

    // Elegant pose (super-hot-girl aesthetic)
    FGestureData Elegant;
    Elegant.Type = EGestureType::Elegant;
    Elegant.Duration = 3.5f;
    Elegant.BlendInTime = 0.6f;
    Elegant.BlendOutTime = 0.6f;
    Elegant.bLooping = false;
    GestureLibrary.Add(EGestureType::Elegant, Elegant);

    // Initialize context-gesture mappings
    ContextGestureMappings.Add(TEXT("greeting"), {EGestureType::Wave, EGestureType::Bow});
    ContextGestureMappings.Add(TEXT("agreement"), {EGestureType::Nod, EGestureType::ThumbsUp});
    ContextGestureMappings.Add(TEXT("disagreement"), {EGestureType::Shake, EGestureType::ThumbsDown});
    ContextGestureMappings.Add(TEXT("celebration"), {EGestureType::Clap, EGestureType::Excited, EGestureType::Peace});
    ContextGestureMappings.Add(TEXT("thinking"), {EGestureType::Thinking});
    ContextGestureMappings.Add(TEXT("flirting"), {EGestureType::Flirty, EGestureType::Heart, EGestureType::Playful});
    ContextGestureMappings.Add(TEXT("confidence"), {EGestureType::Confident, EGestureType::Elegant});
}

void UGestureSystem::InitializeBodyLanguagePresets()
{
    // Neutral body language
    FBodyLanguageData Neutral;
    Neutral.SpineRotation = FVector(0.0f, 0.0f, 0.0f);
    Neutral.ShoulderOffset = FVector(0.0f, 0.0f, 0.0f);
    Neutral.HipRotation = FVector(0.0f, 0.0f, 0.0f);
    Neutral.ChestExpansion = 0.0f;
    Neutral.ArmOpenness = 0.0f;
    BodyLanguagePresets.Add(EBodyLanguageState::Neutral, Neutral);

    // Open body language
    FBodyLanguageData Open;
    Open.SpineRotation = FVector(0.0f, 0.0f, 0.0f);
    Open.ShoulderOffset = FVector(0.0f, -5.0f, 2.0f);
    Open.HipRotation = FVector(0.0f, 0.0f, 0.0f);
    Open.ChestExpansion = 0.3f;
    Open.ArmOpenness = 0.8f;
    BodyLanguagePresets.Add(EBodyLanguageState::Open, Open);

    // Closed body language
    FBodyLanguageData Closed;
    Closed.SpineRotation = FVector(10.0f, 0.0f, 0.0f);
    Closed.ShoulderOffset = FVector(0.0f, 5.0f, -2.0f);
    Closed.HipRotation = FVector(0.0f, 0.0f, 0.0f);
    Closed.ChestExpansion = -0.2f;
    Closed.ArmOpenness = -0.6f;
    BodyLanguagePresets.Add(EBodyLanguageState::Closed, Closed);

    // Confident body language (super-hot-girl aesthetic)
    FBodyLanguageData Confident;
    Confident.SpineRotation = FVector(-5.0f, 0.0f, 0.0f);
    Confident.ShoulderOffset = FVector(0.0f, -8.0f, 3.0f);
    Confident.HipRotation = FVector(0.0f, 0.0f, 5.0f);
    Confident.ChestExpansion = 0.5f;
    Confident.ArmOpenness = 0.5f;
    BodyLanguagePresets.Add(EBodyLanguageState::Confident, Confident);

    // Shy body language
    FBodyLanguageData Shy;
    Shy.SpineRotation = FVector(8.0f, 0.0f, -3.0f);
    Shy.ShoulderOffset = FVector(0.0f, 6.0f, -3.0f);
    Shy.HipRotation = FVector(0.0f, 0.0f, -2.0f);
    Shy.ChestExpansion = -0.3f;
    Shy.ArmOpenness = -0.4f;
    BodyLanguagePresets.Add(EBodyLanguageState::Shy, Shy);

    // Aggressive body language
    FBodyLanguageData Aggressive;
    Aggressive.SpineRotation = FVector(-8.0f, 0.0f, 0.0f);
    Aggressive.ShoulderOffset = FVector(0.0f, -10.0f, 5.0f);
    Aggressive.HipRotation = FVector(0.0f, 0.0f, 0.0f);
    Aggressive.ChestExpansion = 0.7f;
    Aggressive.ArmOpenness = 0.9f;
    BodyLanguagePresets.Add(EBodyLanguageState::Aggressive, Aggressive);

    // Relaxed body language
    FBodyLanguageData Relaxed;
    Relaxed.SpineRotation = FVector(3.0f, 0.0f, 2.0f);
    Relaxed.ShoulderOffset = FVector(0.0f, 2.0f, -1.0f);
    Relaxed.HipRotation = FVector(0.0f, 0.0f, 3.0f);
    Relaxed.ChestExpansion = 0.1f;
    Relaxed.ArmOpenness = 0.3f;
    BodyLanguagePresets.Add(EBodyLanguageState::Relaxed, Relaxed);

    // Tense body language
    FBodyLanguageData Tense;
    Tense.SpineRotation = FVector(0.0f, 0.0f, 0.0f);
    Tense.ShoulderOffset = FVector(0.0f, 8.0f, 4.0f);
    Tense.HipRotation = FVector(0.0f, 0.0f, 0.0f);
    Tense.ChestExpansion = 0.2f;
    Tense.ArmOpenness = -0.5f;
    BodyLanguagePresets.Add(EBodyLanguageState::Tense, Tense);

    // Playful body language (super-hot-girl aesthetic)
    FBodyLanguageData Playful;
    Playful.SpineRotation = FVector(0.0f, 0.0f, 4.0f);
    Playful.ShoulderOffset = FVector(0.0f, -3.0f, 1.0f);
    Playful.HipRotation = FVector(0.0f, 0.0f, 6.0f);
    Playful.ChestExpansion = 0.3f;
    Playful.ArmOpenness = 0.6f;
    BodyLanguagePresets.Add(EBodyLanguageState::Playful, Playful);

    // Seductive body language (super-hot-girl aesthetic)
    FBodyLanguageData Seductive;
    Seductive.SpineRotation = FVector(-3.0f, 0.0f, 5.0f);
    Seductive.ShoulderOffset = FVector(0.0f, -6.0f, 2.0f);
    Seductive.HipRotation = FVector(0.0f, 0.0f, 8.0f);
    Seductive.ChestExpansion = 0.4f;
    Seductive.ArmOpenness = 0.4f;
    BodyLanguagePresets.Add(EBodyLanguageState::Seductive, Seductive);

    UE_LOG(LogTemp, Log, TEXT("Body language presets initialized: %d states"), BodyLanguagePresets.Num());
}

void UGestureSystem::TriggerGesture(EGestureType Gesture, float IntensityMultiplier)
{
    if (!GestureLibrary.Contains(Gesture))
    {
        UE_LOG(LogTemp, Warning, TEXT("Gesture not found in library: %d"), (int32)Gesture);
        return;
    }

    const FGestureData& GestureData = GestureLibrary[Gesture];

    CurrentGesture = Gesture;
    GestureDuration = GestureData.Duration;
    GestureIntensity = IntensityMultiplier;
    GestureTimer = 0.0f;
    bGesturePlaying = true;

    UE_LOG(LogTemp, Log, TEXT("Gesture triggered: %d (intensity: %.2f)"), (int32)Gesture, IntensityMultiplier);
}

void UGestureSystem::SetBodyLanguageState(EBodyLanguageState NewState, float TransitionTime)
{
    if (NewState == CurrentBodyLanguage && BodyLanguageTransitionProgress >= 1.0f)
        return;

    TargetBodyLanguage = NewState;
    BodyLanguageTransitionDuration = TransitionTime;
    BodyLanguageTransitionProgress = 0.0f;

    if (BodyLanguagePresets.Contains(TargetBodyLanguage))
    {
        TargetBodyLanguageData = BodyLanguagePresets[TargetBodyLanguage];
    }

    UE_LOG(LogTemp, Log, TEXT("Body language transition: %d -> %d (%.2fs)"), 
        (int32)CurrentBodyLanguage, (int32)TargetBodyLanguage, TransitionTime);
}

void UGestureSystem::ChainGestures(const TArray<EGestureType>& GestureChain)
{
    GestureChainQueue = GestureChain;

    if (GestureChainQueue.Num() > 0 && !bGesturePlaying)
    {
        EGestureType FirstGesture = GestureChainQueue[0];
        GestureChainQueue.RemoveAt(0);
        TriggerGesture(FirstGesture);
    }

    UE_LOG(LogTemp, Log, TEXT("Gesture chain queued: %d gestures"), GestureChain.Num());
}

void UGestureSystem::ApplyProceduralAnimation(USkeletalMeshComponent* TargetMesh)
{
    if (!TargetMesh || !bGesturePlaying)
        return;

    // Calculate gesture progress
    float Progress = FMath::Clamp(GestureTimer / GestureDuration, 0.0f, 1.0f);
    
    // Apply gesture-specific procedural animations
    switch (CurrentGesture)
    {
        case EGestureType::Wave:
        {
            float WaveAngle = FMath::Sin(Progress * PI * 4.0f) * 45.0f * GestureIntensity;
            // Apply to right arm bones
            break;
        }
        case EGestureType::Nod:
        {
            float NodAngle = FMath::Sin(Progress * PI * 2.0f) * 15.0f * GestureIntensity;
            // Apply to head bone
            break;
        }
        case EGestureType::Shake:
        {
            float ShakeAngle = FMath::Sin(Progress * PI * 3.0f) * 20.0f * GestureIntensity;
            // Apply to head bone rotation
            break;
        }
        default:
            break;
    }
}

void UGestureSystem::SetGestureContext(const FString& Context)
{
    CurrentContext = Context.ToLower();
    UE_LOG(LogTemp, Log, TEXT("Gesture context set: %s"), *CurrentContext);
}

EGestureType UGestureSystem::SelectContextualGesture()
{
    if (CurrentContext.IsEmpty() || !ContextGestureMappings.Contains(CurrentContext))
    {
        return EGestureType::Wave; // Default gesture
    }

    const TArray<EGestureType>& ContextGestures = ContextGestureMappings[CurrentContext];
    
    if (ContextGestures.Num() == 0)
    {
        return EGestureType::Wave;
    }

    // Randomly select from appropriate gestures
    int32 RandomIndex = FMath::RandRange(0, ContextGestures.Num() - 1);
    return ContextGestures[RandomIndex];
}

void UGestureSystem::UpdateGesture(float DeltaTime)
{
    if (!bGesturePlaying)
        return;

    GestureTimer += DeltaTime;

    if (GestureTimer >= GestureDuration)
    {
        bGesturePlaying = false;
        GestureTimer = 0.0f;

        UE_LOG(LogTemp, Log, TEXT("Gesture completed: %d"), (int32)CurrentGesture);
    }
}

void UGestureSystem::UpdateBodyLanguageTransition(float DeltaTime)
{
    if (BodyLanguageTransitionProgress >= 1.0f)
        return;

    BodyLanguageTransitionProgress += DeltaTime / BodyLanguageTransitionDuration;
    BodyLanguageTransitionProgress = FMath::Clamp(BodyLanguageTransitionProgress, 0.0f, 1.0f);

    // Smooth interpolation
    float Alpha = FMath::SmoothStep(0.0f, 1.0f, BodyLanguageTransitionProgress);

    // Interpolate body language data
    FBodyLanguageData BaseData = BodyLanguagePresets.Contains(CurrentBodyLanguage) ?
        BodyLanguagePresets[CurrentBodyLanguage] : FBodyLanguageData();
    
    CurrentBodyLanguageData = InterpolateBodyLanguage(BaseData, TargetBodyLanguageData, Alpha);

    if (BodyLanguageTransitionProgress >= 1.0f)
    {
        CurrentBodyLanguage = TargetBodyLanguage;
        UE_LOG(LogTemp, Log, TEXT("Body language transition completed: %d"), (int32)CurrentBodyLanguage);
    }
}

void UGestureSystem::UpdateGestureChain(float DeltaTime)
{
    if (GestureChainQueue.Num() == 0 || bGesturePlaying)
        return;

    // Trigger next gesture in chain
    EGestureType NextGesture = GestureChainQueue[0];
    GestureChainQueue.RemoveAt(0);
    TriggerGesture(NextGesture);
}

FBodyLanguageData UGestureSystem::InterpolateBodyLanguage(const FBodyLanguageData& From, const FBodyLanguageData& To, float Alpha)
{
    FBodyLanguageData Result;

    Result.SpineRotation = FMath::Lerp(From.SpineRotation, To.SpineRotation, Alpha);
    Result.ShoulderOffset = FMath::Lerp(From.ShoulderOffset, To.ShoulderOffset, Alpha);
    Result.HipRotation = FMath::Lerp(From.HipRotation, To.HipRotation, Alpha);
    Result.ChestExpansion = FMath::Lerp(From.ChestExpansion, To.ChestExpansion, Alpha);
    Result.ArmOpenness = FMath::Lerp(From.ArmOpenness, To.ArmOpenness, Alpha);

    return Result;
}
