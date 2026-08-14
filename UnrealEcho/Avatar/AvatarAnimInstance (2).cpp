// Avatar Animation Instance - Production Implementation
// Deep Tree Echo AGI Avatar System

#include "AvatarAnimInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"

UAvatarAnimInstance::UAvatarAnimInstance()
{
    // Initialize default values
    LocomotionState = ELocomotionState::Idle;
    Speed = 0.0f;
    Direction = 0.0f;
    bIsInAir = false;
    bIsOnGround = true;

    CurrentEmotion = EAvatarEmotionalState::Neutral;
    EmotionIntensity = 0.5f;
    EmotionalValence = 0.0f;
    EmotionalArousal = 0.5f;

    ConfidenceLevel = 0.7f;
    FlirtinessLevel = 0.5f;
    ChaosFactor = 0.3f;
    PlayfulnessLevel = 0.5f;

    CurrentGesture = EGestureState::None;
    GestureBlendWeight = 0.0f;
    GestureTimer = 0.0f;
    GestureDuration = 0.0f;

    bEnableGazeTracking = true;
    EyeLookAtAlpha = 1.0f;
    HeadLookAtAlpha = 0.7f;

    EchoResonance = 0.0f;
    CognitiveLoad = 0.0f;
    GlitchIntensity = 0.0f;

    BreathingCycle = 0.0f;
    BreathingRate = 15.0f; // 15 breaths per minute
    BreathingTime = 0.0f;

    IdleSway = FVector2D::ZeroVector;
    IdleSwayTime = 0.0f;

    FacialBlendSpeed = 5.0f;
    ChaosTimer = 0.0f;
    NextChaoticEventTime = FMath::FRandRange(2.0f, 5.0f);
}

void UAvatarAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();

    // Get owning pawn
    OwningPawn = TryGetPawnOwner();
    
    if (OwningPawn)
    {
        // Get character movement component
        ACharacter* Character = Cast<ACharacter>(OwningPawn);
        if (Character)
        {
            MovementComponent = Character->GetCharacterMovement();
        }
    }
}

void UAvatarAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    if (!OwningPawn)
    {
        return;
    }

    // Update all animation systems
    UpdateLocomotionState(DeltaSeconds);
    UpdateGestureState(DeltaSeconds);
    UpdateFacialAnimation(DeltaSeconds);
    UpdateGazeTracking(DeltaSeconds);
    UpdateProceduralAnimation(DeltaSeconds);
    UpdateChaoticBehavior(DeltaSeconds);
}

void UAvatarAnimInstance::UpdateLocomotionState(float DeltaTime)
{
    if (!OwningPawn || !MovementComponent)
    {
        return;
    }

    // Calculate speed and direction
    FVector Velocity = OwningPawn->GetVelocity();
    Speed = Velocity.Size2D();
    
    FVector Forward = OwningPawn->GetActorForwardVector();
    FVector VelocityNormalized = Velocity.GetSafeNormal2D();
    Direction = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(Forward, VelocityNormalized)));
    
    // Determine if moving left or right
    FVector Right = OwningPawn->GetActorRightVector();
    if (FVector::DotProduct(Right, VelocityNormalized) < 0.0f)
    {
        Direction *= -1.0f;
    }

    // Update air state
    bIsInAir = MovementComponent->IsFalling();
    bIsOnGround = !bIsInAir;

    // Determine locomotion state
    if (bIsInAir)
    {
        if (Velocity.Z < -100.0f)
        {
            LocomotionState = ELocomotionState::Fall;
        }
        else
        {
            LocomotionState = ELocomotionState::Jump;
        }
    }
    else
    {
        if (Speed > 300.0f)
        {
            LocomotionState = ELocomotionState::Run;
        }
        else if (Speed > 10.0f)
        {
            LocomotionState = ELocomotionState::Walk;
        }
        else
        {
            LocomotionState = ELocomotionState::Idle;
        }
    }
}

void UAvatarAnimInstance::UpdateGestureState(float DeltaTime)
{
    // Update gesture timer
    if (CurrentGesture != EGestureState::None)
    {
        GestureTimer += DeltaTime;
        
        // Calculate blend weight (fade in, hold, fade out)
        float FadeInTime = 0.3f;
        float FadeOutTime = 0.3f;
        float HoldTime = GestureDuration - FadeInTime - FadeOutTime;
        
        if (GestureTimer < FadeInTime)
        {
            // Fade in
            GestureBlendWeight = GestureTimer / FadeInTime;
        }
        else if (GestureTimer < FadeInTime + HoldTime)
        {
            // Hold
            GestureBlendWeight = 1.0f;
        }
        else if (GestureTimer < GestureDuration)
        {
            // Fade out
            float FadeOutProgress = (GestureTimer - FadeInTime - HoldTime) / FadeOutTime;
            GestureBlendWeight = 1.0f - FadeOutProgress;
        }
        else
        {
            // Gesture complete
            CurrentGesture = EGestureState::None;
            GestureBlendWeight = 0.0f;
            GestureTimer = 0.0f;
        }
    }
    
    // Personality-driven automatic gestures
    if (CurrentGesture == EGestureState::None && FMath::FRand() < 0.01f) // 1% chance per frame
    {
        // Choose gesture based on personality
        if (ConfidenceLevel > 0.7f && FMath::FRand() < 0.5f)
        {
            TriggerGesture(EGestureState::Confident, 2.0f);
        }
        else if (FlirtinessLevel > 0.6f && FMath::FRand() < 0.5f)
        {
            TriggerGesture(EGestureState::Flirty, 2.5f);
        }
        else if (PlayfulnessLevel > 0.6f && FMath::FRand() < 0.5f)
        {
            TriggerGesture(EGestureState::Excited, 1.5f);
        }
    }
}

void UAvatarAnimInstance::UpdateFacialAnimation(float DeltaTime)
{
    // Calculate target blend shapes based on current emotion
    TargetFacialBlendShapes = CalculateEmotionalBlendShapes(CurrentEmotion);
    
    // Apply emotion intensity
    TargetFacialBlendShapes.BrowInnerUp *= EmotionIntensity;
    TargetFacialBlendShapes.BrowOuterUp *= EmotionIntensity;
    TargetFacialBlendShapes.BrowDown *= EmotionIntensity;
    TargetFacialBlendShapes.EyeWide *= EmotionIntensity;
    TargetFacialBlendShapes.EyeSquint *= EmotionIntensity;
    TargetFacialBlendShapes.MouthSmile *= EmotionIntensity;
    TargetFacialBlendShapes.MouthFrown *= EmotionIntensity;
    TargetFacialBlendShapes.MouthOpen *= EmotionIntensity;
    
    // Smooth blend to target
    BlendFacialShapes(TargetFacialBlendShapes, DeltaTime);
}

void UAvatarAnimInstance::UpdateGazeTracking(float DeltaTime)
{
    if (!bEnableGazeTracking || !OwningPawn)
    {
        return;
    }

    // Smooth interpolation of look-at alpha
    // This would be used by animation blueprint to drive eye and head bones
    EyeLookAtAlpha = FMath::FInterpTo(EyeLookAtAlpha, 1.0f, DeltaTime, 5.0f);
    HeadLookAtAlpha = FMath::FInterpTo(HeadLookAtAlpha, 0.7f, DeltaTime, 3.0f);
}

void UAvatarAnimInstance::UpdateProceduralAnimation(float DeltaTime)
{
    // Update breathing cycle
    BreathingTime += DeltaTime;
    float BreathsPerSecond = BreathingRate / 60.0f;
    BreathingCycle = (FMath::Sin(BreathingTime * BreathsPerSecond * 2.0f * PI) + 1.0f) * 0.5f;
    
    // Update idle sway
    IdleSwayTime += DeltaTime;
    IdleSway.X = FMath::Sin(IdleSwayTime * 0.3f) * 2.0f;
    IdleSway.Y = FMath::Cos(IdleSwayTime * 0.25f) * 1.5f;
}

void UAvatarAnimInstance::UpdateChaoticBehavior(float DeltaTime)
{
    if (ChaosFactor < 0.1f)
    {
        return; // Not chaotic enough to trigger events
    }

    ChaosTimer += DeltaTime;
    
    if (ChaosTimer >= NextChaoticEventTime)
    {
        TriggerRandomChaoticEvent();
        ChaosTimer = 0.0f;
        NextChaoticEventTime = FMath::FRandRange(1.0f / ChaosFactor, 5.0f / ChaosFactor);
    }
}

void UAvatarAnimInstance::UpdateEmotionalState(EAvatarEmotionalState Emotion, float Intensity)
{
    CurrentEmotion = Emotion;
    EmotionIntensity = FMath::Clamp(Intensity, 0.0f, 1.0f);
    
    // Update emotional dimensions
    switch (Emotion)
    {
        case EAvatarEmotionalState::Happy:
        case EAvatarEmotionalState::Excited:
            EmotionalValence = 0.8f;
            EmotionalArousal = 0.7f;
            break;
            
        case EAvatarEmotionalState::Sad:
            EmotionalValence = -0.6f;
            EmotionalArousal = 0.3f;
            break;
            
        case EAvatarEmotionalState::Angry:
            EmotionalValence = -0.7f;
            EmotionalArousal = 0.9f;
            break;
            
        case EAvatarEmotionalState::Surprised:
            EmotionalValence = 0.2f;
            EmotionalArousal = 0.9f;
            break;
            
        case EAvatarEmotionalState::Flirty:
            EmotionalValence = 0.7f;
            EmotionalArousal = 0.6f;
            break;
            
        case EAvatarEmotionalState::Chaotic:
            EmotionalValence = 0.0f;
            EmotionalArousal = 1.0f;
            break;
            
        default:
            EmotionalValence = 0.0f;
            EmotionalArousal = 0.5f;
            break;
    }
    
    // Update breathing rate based on arousal
    BreathingRate = CalculateBreathingRate();
}

void UAvatarAnimInstance::SetPersonalityTraits(float Confidence, float Flirtiness, float Chaos)
{
    ConfidenceLevel = FMath::Clamp(Confidence, 0.0f, 1.0f);
    FlirtinessLevel = FMath::Clamp(Flirtiness, 0.0f, 1.0f);
    ChaosFactor = FMath::Clamp(Chaos, 0.0f, 1.0f);
}

void UAvatarAnimInstance::TriggerGesture(EGestureState Gesture, float Duration)
{
    CurrentGesture = Gesture;
    GestureDuration = Duration;
    GestureTimer = 0.0f;
    GestureBlendWeight = 0.0f;
}

void UAvatarAnimInstance::SetGazeTarget(const FVector& Target)
{
    GazeTargetLocation = Target;
}

void UAvatarAnimInstance::ApplyChaoticBehavior(float Intensity)
{
    ChaosFactor = FMath::Clamp(Intensity, 0.0f, 1.0f);
}

FFacialBlendShapes UAvatarAnimInstance::CalculateEmotionalBlendShapes(EAvatarEmotionalState Emotion) const
{
    FFacialBlendShapes Shapes;

    switch (Emotion)
    {
        case EAvatarEmotionalState::Happy:
            Shapes.BrowInnerUp = 0.2f;
            Shapes.BrowOuterUp = 0.3f;
            Shapes.EyeWide = 0.3f;
            Shapes.EyeSquint = 0.2f;
            Shapes.MouthSmile = 0.9f;
            Shapes.MouthOpen = 0.2f;
            Shapes.CheekPuff = 0.3f;
            break;

        case EAvatarEmotionalState::Sad:
            Shapes.BrowInnerUp = 0.6f;
            Shapes.BrowDown = 0.4f;
            Shapes.EyeSquint = 0.3f;
            Shapes.MouthFrown = 0.8f;
            break;

        case EAvatarEmotionalState::Angry:
            Shapes.BrowDown = 0.8f;
            Shapes.EyeWide = 0.2f;
            Shapes.EyeSquint = 0.6f;
            Shapes.MouthFrown = 0.5f;
            Shapes.MouthOpen = 0.3f;
            break;

        case EAvatarEmotionalState::Surprised:
            Shapes.BrowInnerUp = 0.8f;
            Shapes.BrowOuterUp = 0.9f;
            Shapes.EyeWide = 0.9f;
            Shapes.MouthOpen = 0.7f;
            Shapes.JawOpen = 0.5f;
            break;

        case EAvatarEmotionalState::Flirty:
            Shapes.BrowInnerUp = 0.3f;
            Shapes.BrowOuterUp = 0.2f;
            Shapes.EyeWide = 0.4f;
            Shapes.EyeSquint = 0.3f;
            Shapes.MouthSmile = 0.7f;
            Shapes.MouthOpen = 0.2f;
            break;

        case EAvatarEmotionalState::Excited:
            Shapes.BrowOuterUp = 0.5f;
            Shapes.EyeWide = 0.7f;
            Shapes.MouthSmile = 0.8f;
            Shapes.MouthOpen = 0.4f;
            break;

        case EAvatarEmotionalState::Chaotic:
            // Random blend shapes for chaotic expression
            Shapes.BrowInnerUp = FMath::FRand();
            Shapes.BrowOuterUp = FMath::FRand();
            Shapes.BrowDown = FMath::FRand() * 0.5f;
            Shapes.EyeWide = FMath::FRand();
            Shapes.EyeSquint = FMath::FRand() * 0.5f;
            Shapes.MouthSmile = FMath::FRand();
            Shapes.MouthFrown = FMath::FRand() * 0.5f;
            Shapes.MouthOpen = FMath::FRand() * 0.7f;
            break;

        default: // Neutral
            // All values remain at 0.0f
            break;
    }

    return Shapes;
}

void UAvatarAnimInstance::BlendFacialShapes(const FFacialBlendShapes& Target, float DeltaTime)
{
    // Smooth interpolation to target blend shapes
    FacialBlendShapes.BrowInnerUp = FMath::FInterpTo(FacialBlendShapes.BrowInnerUp, Target.BrowInnerUp, DeltaTime, FacialBlendSpeed);
    FacialBlendShapes.BrowOuterUp = FMath::FInterpTo(FacialBlendShapes.BrowOuterUp, Target.BrowOuterUp, DeltaTime, FacialBlendSpeed);
    FacialBlendShapes.BrowDown = FMath::FInterpTo(FacialBlendShapes.BrowDown, Target.BrowDown, DeltaTime, FacialBlendSpeed);
    FacialBlendShapes.EyeWide = FMath::FInterpTo(FacialBlendShapes.EyeWide, Target.EyeWide, DeltaTime, FacialBlendSpeed);
    FacialBlendShapes.EyeSquint = FMath::FInterpTo(FacialBlendShapes.EyeSquint, Target.EyeSquint, DeltaTime, FacialBlendSpeed);
    FacialBlendShapes.MouthSmile = FMath::FInterpTo(FacialBlendShapes.MouthSmile, Target.MouthSmile, DeltaTime, FacialBlendSpeed);
    FacialBlendShapes.MouthFrown = FMath::FInterpTo(FacialBlendShapes.MouthFrown, Target.MouthFrown, DeltaTime, FacialBlendSpeed);
    FacialBlendShapes.MouthOpen = FMath::FInterpTo(FacialBlendShapes.MouthOpen, Target.MouthOpen, DeltaTime, FacialBlendSpeed);
    FacialBlendShapes.CheekPuff = FMath::FInterpTo(FacialBlendShapes.CheekPuff, Target.CheekPuff, DeltaTime, FacialBlendSpeed);
    FacialBlendShapes.JawOpen = FMath::FInterpTo(FacialBlendShapes.JawOpen, Target.JawOpen, DeltaTime, FacialBlendSpeed);
}

float UAvatarAnimInstance::CalculateBreathingRate() const
{
    // Base breathing rate: 15 breaths per minute
    float BaseRate = 15.0f;
    
    // Arousal increases breathing rate
    float ArousalModifier = EmotionalArousal * 10.0f; // Up to +10 BPM
    
    // Chaos adds variability
    float ChaosModifier = FMath::FRandRange(-2.0f, 2.0f) * ChaosFactor;
    
    return FMath::Clamp(BaseRate + ArousalModifier + ChaosModifier, 10.0f, 30.0f);
}

void UAvatarAnimInstance::TriggerRandomChaoticEvent()
{
    // Random chaotic behaviors
    int32 EventType = FMath::RandRange(0, 3);
    
    switch (EventType)
    {
        case 0: // Random gesture
            {
                EGestureState RandomGesture = static_cast<EGestureState>(FMath::RandRange(1, 6));
                TriggerGesture(RandomGesture, FMath::FRandRange(0.5f, 1.5f));
            }
            break;
            
        case 1: // Sudden emotion change
            {
                EAvatarEmotionalState RandomEmotion = static_cast<EAvatarEmotionalState>(FMath::RandRange(0, 9));
                UpdateEmotionalState(RandomEmotion, FMath::FRandRange(0.3f, 0.8f));
            }
            break;
            
        case 2: // Glitch effect
            GlitchIntensity = FMath::FRandRange(0.5f, 1.0f);
            break;
            
        case 3: // Random head movement (would be implemented in animation blueprint)
            // This would trigger a custom animation node
            break;
    }
}
