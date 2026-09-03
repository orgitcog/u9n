#include "FacialAnimationSystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"

UFacialAnimationSystem::UFacialAnimationSystem()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;

    CurrentEmotion = EEmotionState::Neutral;
    TargetEmotion = EEmotionState::Neutral;
    EmotionTransitionProgress = 1.0f;
    EmotionTransitionDuration = 0.3f;

    BlinkTimer = 0.0f;
    NextBlinkTime = 3.0f;
    bIsBlinking = false;
    BlinkProgress = 0.0f;

    bHasGazeTarget = false;
    CurrentEyeRotation = FRotator::ZeroRotator;

    bMicroExpressionActive = false;
    MicroExpressionTimer = 0.0f;
    MicroExpressionDuration = 0.2f;
}

void UFacialAnimationSystem::BeginPlay()
{
    Super::BeginPlay();

    InitializeEmotionPresets();
    InitializePhonemeMappings();

    // Set initial blend shapes to neutral
    CurrentBlendShapes = EmotionPresets[EEmotionState::Neutral];
    TargetBlendShapes = CurrentBlendShapes;

    UE_LOG(LogTemp, Log, TEXT("FacialAnimationSystem initialized"));
}

void UFacialAnimationSystem::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    UpdateEmotionTransition(DeltaTime);
    UpdateAutoBlinking(DeltaTime);
    UpdateGazeTracking(DeltaTime);
    UpdateMicroExpressions(DeltaTime);
}

void UFacialAnimationSystem::InitializeEmotionPresets()
{
    // Neutral expression
    FFacialBlendShapes Neutral;
    Neutral.EyeBlinkLeft = 0.0f;
    Neutral.EyeBlinkRight = 0.0f;
    Neutral.EyeWideLeft = 0.0f;
    Neutral.EyeWideRight = 0.0f;
    Neutral.BrowInnerUp = 0.0f;
    Neutral.BrowOuterUpLeft = 0.0f;
    Neutral.BrowOuterUpRight = 0.0f;
    Neutral.MouthSmileLeft = 0.0f;
    Neutral.MouthSmileRight = 0.0f;
    Neutral.MouthOpen = 0.0f;
    Neutral.JawOpen = 0.0f;
    Neutral.CheekPuff = 0.0f;
    EmotionPresets.Add(EEmotionState::Neutral, Neutral);

    // Happy expression (super-hot-girl aesthetic)
    FFacialBlendShapes Happy;
    Happy.EyeBlinkLeft = 0.0f;
    Happy.EyeBlinkRight = 0.0f;
    Happy.EyeWideLeft = 0.3f;
    Happy.EyeWideRight = 0.3f;
    Happy.BrowInnerUp = 0.2f;
    Happy.BrowOuterUpLeft = 0.4f;
    Happy.BrowOuterUpRight = 0.4f;
    Happy.MouthSmileLeft = 0.8f;
    Happy.MouthSmileRight = 0.8f;
    Happy.MouthOpen = 0.3f;
    Happy.JawOpen = 0.2f;
    Happy.CheekPuff = 0.5f;
    EmotionPresets.Add(EEmotionState::Happy, Happy);

    // Sad expression
    FFacialBlendShapes Sad;
    Sad.EyeBlinkLeft = 0.3f;
    Sad.EyeBlinkRight = 0.3f;
    Sad.EyeWideLeft = 0.0f;
    Sad.EyeWideRight = 0.0f;
    Sad.BrowInnerUp = 0.8f;
    Sad.BrowOuterUpLeft = 0.0f;
    Sad.BrowOuterUpRight = 0.0f;
    Sad.MouthSmileLeft = -0.4f;
    Sad.MouthSmileRight = -0.4f;
    Sad.MouthOpen = 0.0f;
    Sad.JawOpen = 0.0f;
    Sad.CheekPuff = 0.0f;
    EmotionPresets.Add(EEmotionState::Sad, Sad);

    // Angry expression
    FFacialBlendShapes Angry;
    Angry.EyeBlinkLeft = 0.0f;
    Angry.EyeBlinkRight = 0.0f;
    Angry.EyeWideLeft = 0.6f;
    Angry.EyeWideRight = 0.6f;
    Angry.BrowInnerUp = 0.0f;
    Angry.BrowOuterUpLeft = -0.6f;
    Angry.BrowOuterUpRight = -0.6f;
    Angry.MouthSmileLeft = -0.3f;
    Angry.MouthSmileRight = -0.3f;
    Angry.MouthOpen = 0.2f;
    Angry.JawOpen = 0.1f;
    Angry.CheekPuff = 0.0f;
    EmotionPresets.Add(EEmotionState::Angry, Angry);

    // Surprised expression
    FFacialBlendShapes Surprised;
    Surprised.EyeBlinkLeft = 0.0f;
    Surprised.EyeBlinkRight = 0.0f;
    Surprised.EyeWideLeft = 1.0f;
    Surprised.EyeWideRight = 1.0f;
    Surprised.BrowInnerUp = 0.9f;
    Surprised.BrowOuterUpLeft = 0.9f;
    Surprised.BrowOuterUpRight = 0.9f;
    Surprised.MouthSmileLeft = 0.0f;
    Surprised.MouthSmileRight = 0.0f;
    Surprised.MouthOpen = 0.8f;
    Surprised.JawOpen = 0.7f;
    Surprised.CheekPuff = 0.0f;
    EmotionPresets.Add(EEmotionState::Surprised, Surprised);

    // Disgusted expression
    FFacialBlendShapes Disgusted;
    Disgusted.EyeBlinkLeft = 0.4f;
    Disgusted.EyeBlinkRight = 0.4f;
    Disgusted.EyeWideLeft = 0.0f;
    Disgusted.EyeWideRight = 0.0f;
    Disgusted.BrowInnerUp = 0.0f;
    Disgusted.BrowOuterUpLeft = -0.4f;
    Disgusted.BrowOuterUpRight = -0.4f;
    Disgusted.MouthSmileLeft = -0.6f;
    Disgusted.MouthSmileRight = -0.6f;
    Disgusted.MouthOpen = 0.0f;
    Disgusted.JawOpen = 0.0f;
    Disgusted.CheekPuff = 0.0f;
    EmotionPresets.Add(EEmotionState::Disgusted, Disgusted);

    // Fearful expression
    FFacialBlendShapes Fearful;
    Fearful.EyeBlinkLeft = 0.0f;
    Fearful.EyeBlinkRight = 0.0f;
    Fearful.EyeWideLeft = 0.9f;
    Fearful.EyeWideRight = 0.9f;
    Fearful.BrowInnerUp = 0.8f;
    Fearful.BrowOuterUpLeft = 0.3f;
    Fearful.BrowOuterUpRight = 0.3f;
    Fearful.MouthSmileLeft = -0.2f;
    Fearful.MouthSmileRight = -0.2f;
    Fearful.MouthOpen = 0.4f;
    Fearful.JawOpen = 0.3f;
    Fearful.CheekPuff = 0.0f;
    EmotionPresets.Add(EEmotionState::Fearful, Fearful);

    // Flirty expression (super-hot-girl aesthetic)
    FFacialBlendShapes Flirty;
    Flirty.EyeBlinkLeft = 0.5f;
    Flirty.EyeBlinkRight = 0.0f;
    Flirty.EyeWideLeft = 0.0f;
    Flirty.EyeWideRight = 0.6f;
    Flirty.BrowInnerUp = 0.3f;
    Flirty.BrowOuterUpLeft = 0.2f;
    Flirty.BrowOuterUpRight = 0.5f;
    Flirty.MouthSmileLeft = 0.6f;
    Flirty.MouthSmileRight = 0.7f;
    Flirty.MouthOpen = 0.2f;
    Flirty.JawOpen = 0.1f;
    Flirty.CheekPuff = 0.6f;
    EmotionPresets.Add(EEmotionState::Flirty, Flirty);

    // Playful expression (super-hot-girl aesthetic)
    FFacialBlendShapes Playful;
    Playful.EyeBlinkLeft = 0.0f;
    Playful.EyeBlinkRight = 0.0f;
    Playful.EyeWideLeft = 0.5f;
    Playful.EyeWideRight = 0.5f;
    Playful.BrowInnerUp = 0.4f;
    Playful.BrowOuterUpLeft = 0.6f;
    Playful.BrowOuterUpRight = 0.6f;
    Playful.MouthSmileLeft = 0.7f;
    Playful.MouthSmileRight = 0.5f;
    Playful.MouthOpen = 0.4f;
    Playful.JawOpen = 0.3f;
    Playful.CheekPuff = 0.4f;
    EmotionPresets.Add(EEmotionState::Playful, Playful);

    // Confident expression (super-hot-girl aesthetic)
    FFacialBlendShapes Confident;
    Confident.EyeBlinkLeft = 0.0f;
    Confident.EyeBlinkRight = 0.0f;
    Confident.EyeWideLeft = 0.4f;
    Confident.EyeWideRight = 0.4f;
    Confident.BrowInnerUp = 0.1f;
    Confident.BrowOuterUpLeft = 0.3f;
    Confident.BrowOuterUpRight = 0.3f;
    Confident.MouthSmileLeft = 0.5f;
    Confident.MouthSmileRight = 0.5f;
    Confident.MouthOpen = 0.1f;
    Confident.JawOpen = 0.05f;
    Confident.CheekPuff = 0.3f;
    EmotionPresets.Add(EEmotionState::Confident, Confident);

    UE_LOG(LogTemp, Log, TEXT("Emotion presets initialized: %d emotions"), EmotionPresets.Num());
}

void UFacialAnimationSystem::InitializePhonemeMappings()
{
    // Phoneme mappings for lip sync
    FPhonemeData PhonemeA;
    PhonemeA.PhonemeName = TEXT("A");
    PhonemeA.MouthOpenAmount = 0.8f;
    PhonemeA.LipShapeValue = 0.0f;
    PhonemeMappings.Add(TEXT("A"), PhonemeA);

    FPhonemeData PhonemeE;
    PhonemeE.PhonemeName = TEXT("E");
    PhonemeE.MouthOpenAmount = 0.5f;
    PhonemeE.LipShapeValue = 0.3f;
    PhonemeMappings.Add(TEXT("E"), PhonemeE);

    FPhonemeData PhonemeI;
    PhonemeI.PhonemeName = TEXT("I");
    PhonemeI.MouthOpenAmount = 0.3f;
    PhonemeI.LipShapeValue = 0.6f;
    PhonemeMappings.Add(TEXT("I"), PhonemeI);

    FPhonemeData PhonemeO;
    PhonemeO.PhonemeName = TEXT("O");
    PhonemeO.MouthOpenAmount = 0.7f;
    PhonemeO.LipShapeValue = -0.5f;
    PhonemeMappings.Add(TEXT("O"), PhonemeO);

    FPhonemeData PhonemeU;
    PhonemeU.PhonemeName = TEXT("U");
    PhonemeU.MouthOpenAmount = 0.4f;
    PhonemeU.LipShapeValue = -0.8f;
    PhonemeMappings.Add(TEXT("U"), PhonemeU);

    UE_LOG(LogTemp, Log, TEXT("Phoneme mappings initialized: %d phonemes"), PhonemeMappings.Num());
}

void UFacialAnimationSystem::SetEmotionState(EEmotionState NewEmotion, float TransitionTime)
{
    if (NewEmotion == CurrentEmotion && EmotionTransitionProgress >= 1.0f)
        return;

    TargetEmotion = NewEmotion;
    EmotionTransitionDuration = TransitionTime;
    EmotionTransitionProgress = 0.0f;

    if (EmotionPresets.Contains(TargetEmotion))
    {
        TargetBlendShapes = EmotionPresets[TargetEmotion];
    }

    UE_LOG(LogTemp, Log, TEXT("Emotion transition started: %d -> %d (%.2fs)"), 
        (int32)CurrentEmotion, (int32)TargetEmotion, TransitionTime);
}

void UFacialAnimationSystem::TriggerMicroExpression(EEmotionState MicroEmotion, float Duration)
{
    if (bMicroExpressionActive)
        return;

    BaseEmotionBeforeMicro = CurrentEmotion;
    MicroExpressionState = MicroEmotion;
    MicroExpressionDuration = Duration;
    MicroExpressionTimer = 0.0f;
    bMicroExpressionActive = true;

    // Immediately set to micro-expression
    SetEmotionState(MicroEmotion, 0.1f);

    UE_LOG(LogTemp, Log, TEXT("Micro-expression triggered: %d for %.2fs"), (int32)MicroEmotion, Duration);
}

void UFacialAnimationSystem::SetGazeTarget(const FVector& WorldTarget)
{
    GazeTarget = WorldTarget;
    bHasGazeTarget = true;
}

void UFacialAnimationSystem::SetAutoBlinkEnabled(bool bEnabled)
{
    bAutoBlinkEnabled = bEnabled;
    
    if (!bEnabled && bIsBlinking)
    {
        // Reset blink state
        bIsBlinking = false;
        BlinkProgress = 0.0f;
        CurrentBlendShapes.EyeBlinkLeft = 0.0f;
        CurrentBlendShapes.EyeBlinkRight = 0.0f;
    }
}

void UFacialAnimationSystem::TriggerBlink()
{
    if (!bIsBlinking)
    {
        bIsBlinking = true;
        BlinkProgress = 0.0f;
    }
}

void UFacialAnimationSystem::SetPhoneme(const FName& PhonemeName, float Intensity)
{
    if (PhonemeMappings.Contains(PhonemeName))
    {
        const FPhonemeData& Phoneme = PhonemeMappings[PhonemeName];
        
        CurrentBlendShapes.MouthOpen = Phoneme.MouthOpenAmount * Intensity;
        CurrentBlendShapes.JawOpen = Phoneme.MouthOpenAmount * 0.8f * Intensity;
        
        // Apply lip shape based on phoneme
        if (Phoneme.LipShapeValue > 0.0f)
        {
            CurrentBlendShapes.MouthSmileLeft = Phoneme.LipShapeValue * Intensity;
            CurrentBlendShapes.MouthSmileRight = Phoneme.LipShapeValue * Intensity;
        }
    }
}

void UFacialAnimationSystem::ApplyBlendShapesToMesh(USkeletalMeshComponent* TargetMesh)
{
    if (!TargetMesh)
        return;

    // Apply blend shapes to morph targets
    TargetMesh->SetMorphTarget(TEXT("EyeBlinkLeft"), CurrentBlendShapes.EyeBlinkLeft);
    TargetMesh->SetMorphTarget(TEXT("EyeBlinkRight"), CurrentBlendShapes.EyeBlinkRight);
    TargetMesh->SetMorphTarget(TEXT("EyeWideLeft"), CurrentBlendShapes.EyeWideLeft);
    TargetMesh->SetMorphTarget(TEXT("EyeWideRight"), CurrentBlendShapes.EyeWideRight);
    TargetMesh->SetMorphTarget(TEXT("BrowInnerUp"), CurrentBlendShapes.BrowInnerUp);
    TargetMesh->SetMorphTarget(TEXT("BrowOuterUpLeft"), CurrentBlendShapes.BrowOuterUpLeft);
    TargetMesh->SetMorphTarget(TEXT("BrowOuterUpRight"), CurrentBlendShapes.BrowOuterUpRight);
    TargetMesh->SetMorphTarget(TEXT("MouthSmileLeft"), CurrentBlendShapes.MouthSmileLeft);
    TargetMesh->SetMorphTarget(TEXT("MouthSmileRight"), CurrentBlendShapes.MouthSmileRight);
    TargetMesh->SetMorphTarget(TEXT("MouthOpen"), CurrentBlendShapes.MouthOpen);
    TargetMesh->SetMorphTarget(TEXT("JawOpen"), CurrentBlendShapes.JawOpen);
    TargetMesh->SetMorphTarget(TEXT("CheekPuff"), CurrentBlendShapes.CheekPuff);
}

void UFacialAnimationSystem::UpdateEmotionTransition(float DeltaTime)
{
    if (EmotionTransitionProgress >= 1.0f)
        return;

    EmotionTransitionProgress += DeltaTime / EmotionTransitionDuration;
    EmotionTransitionProgress = FMath::Clamp(EmotionTransitionProgress, 0.0f, 1.0f);

    // Smooth interpolation curve
    float Alpha = FMath::SmoothStep(0.0f, 1.0f, EmotionTransitionProgress);

    // Interpolate blend shapes
    FFacialBlendShapes BaseShapes = EmotionPresets.Contains(CurrentEmotion) ? 
        EmotionPresets[CurrentEmotion] : FFacialBlendShapes();
    
    CurrentBlendShapes = InterpolateBlendShapes(BaseShapes, TargetBlendShapes, Alpha);

    if (EmotionTransitionProgress >= 1.0f)
    {
        CurrentEmotion = TargetEmotion;
        UE_LOG(LogTemp, Log, TEXT("Emotion transition completed: %d"), (int32)CurrentEmotion);
    }
}

void UFacialAnimationSystem::UpdateAutoBlinking(float DeltaTime)
{
    if (!bAutoBlinkEnabled)
        return;

    BlinkTimer += DeltaTime;

    if (bIsBlinking)
    {
        BlinkProgress += DeltaTime * 10.0f; // Blink speed

        if (BlinkProgress >= 1.0f)
        {
            bIsBlinking = false;
            BlinkProgress = 0.0f;
            NextBlinkTime = FMath::RandRange(2.0f, 5.0f);
            BlinkTimer = 0.0f;

            CurrentBlendShapes.EyeBlinkLeft = 0.0f;
            CurrentBlendShapes.EyeBlinkRight = 0.0f;
        }
        else
        {
            // Smooth blink curve
            float BlinkCurve = FMath::Sin(BlinkProgress * PI);
            CurrentBlendShapes.EyeBlinkLeft = BlinkCurve;
            CurrentBlendShapes.EyeBlinkRight = BlinkCurve;
        }
    }
    else if (BlinkTimer >= NextBlinkTime)
    {
        bIsBlinking = true;
        BlinkProgress = 0.0f;
    }
}

void UFacialAnimationSystem::UpdateGazeTracking(float DeltaTime)
{
    if (!bHasGazeTarget)
        return;

    // Calculate eye rotation towards gaze target
    AActor* Owner = GetOwner();
    if (!Owner)
        return;

    FVector EyeLocation = Owner->GetActorLocation() + FVector(0.0f, 0.0f, 160.0f); // Approximate eye height
    FVector DirectionToTarget = (GazeTarget - EyeLocation).GetSafeNormal();
    
    FRotator TargetRotation = DirectionToTarget.Rotation();
    
    // Smooth interpolation
    CurrentEyeRotation = FMath::RInterpTo(CurrentEyeRotation, TargetRotation, DeltaTime, 5.0f);
}

void UFacialAnimationSystem::UpdateMicroExpressions(float DeltaTime)
{
    if (!bMicroExpressionActive)
        return;

    MicroExpressionTimer += DeltaTime;

    if (MicroExpressionTimer >= MicroExpressionDuration)
    {
        // Return to base emotion
        SetEmotionState(BaseEmotionBeforeMicro, 0.2f);
        bMicroExpressionActive = false;
        MicroExpressionTimer = 0.0f;

        UE_LOG(LogTemp, Log, TEXT("Micro-expression ended, returning to: %d"), (int32)BaseEmotionBeforeMicro);
    }
}

FFacialBlendShapes UFacialAnimationSystem::InterpolateBlendShapes(const FFacialBlendShapes& From, const FFacialBlendShapes& To, float Alpha)
{
    FFacialBlendShapes Result;

    Result.EyeBlinkLeft = FMath::Lerp(From.EyeBlinkLeft, To.EyeBlinkLeft, Alpha);
    Result.EyeBlinkRight = FMath::Lerp(From.EyeBlinkRight, To.EyeBlinkRight, Alpha);
    Result.EyeWideLeft = FMath::Lerp(From.EyeWideLeft, To.EyeWideLeft, Alpha);
    Result.EyeWideRight = FMath::Lerp(From.EyeWideRight, To.EyeWideRight, Alpha);
    Result.BrowInnerUp = FMath::Lerp(From.BrowInnerUp, To.BrowInnerUp, Alpha);
    Result.BrowOuterUpLeft = FMath::Lerp(From.BrowOuterUpLeft, To.BrowOuterUpLeft, Alpha);
    Result.BrowOuterUpRight = FMath::Lerp(From.BrowOuterUpRight, To.BrowOuterUpRight, Alpha);
    Result.MouthSmileLeft = FMath::Lerp(From.MouthSmileLeft, To.MouthSmileLeft, Alpha);
    Result.MouthSmileRight = FMath::Lerp(From.MouthSmileRight, To.MouthSmileRight, Alpha);
    Result.MouthOpen = FMath::Lerp(From.MouthOpen, To.MouthOpen, Alpha);
    Result.JawOpen = FMath::Lerp(From.JawOpen, To.JawOpen, Alpha);
    Result.CheekPuff = FMath::Lerp(From.CheekPuff, To.CheekPuff, Alpha);

    return Result;
}
