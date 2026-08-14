#include "ProceduralAnimationComponent.h"

UProceduralAnimationComponent::UProceduralAnimationComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;

    // Enable all by default
    bBlinkingEnabled = true;
    bBreathingEnabled = true;
    bSaccadesEnabled = true;
    bHeadMovementEnabled = true;

    // State flags
    bIsSpeaking = false;
    bIsThinking = false;
    bIsListening = false;

    // Blink state
    BlinkTimer = 0.0f;
    NextBlinkTime = 3.0f;
    bIsBlinking = false;
    BlinkProgress = 0.0f;
    CurrentBlinkDuration = 0.15f;
    bPendingDoubleBlink = false;

    // Breathing state
    BreathingPhase = 0.0f;
    bWasInhaling = true;
    bSighPending = false;

    // Saccade state
    CurrentEyeOffset = FVector2D::ZeroVector;
    TargetEyeOffset = FVector2D::ZeroVector;
    SaccadeTimer = 0.0f;
    FixationTimer = 0.0f;
    bHasForcedEyeOffset = false;
    ForcedEyeOffset = FVector2D::ZeroVector;

    // Head movement state
    HeadSwayPhaseX = 0.0f;
    HeadSwayPhaseY = 0.0f;
    ListeningNodPhase = 0.0f;
}

void UProceduralAnimationComponent::BeginPlay()
{
    Super::BeginPlay();

    // Randomize initial blink time
    NextBlinkTime = FMath::RandRange(BlinkSettings.MinInterval, BlinkSettings.MaxInterval);

    // Randomize initial phases to prevent synchronized movement
    BreathingPhase = FMath::RandRange(0.0f, 1.0f);
    HeadSwayPhaseX = FMath::RandRange(0.0f, 2.0f * PI);
    HeadSwayPhaseY = FMath::RandRange(0.0f, 2.0f * PI);

    UE_LOG(LogTemp, Log, TEXT("ProceduralAnimationComponent initialized"));
}

void UProceduralAnimationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Reset output
    Output.EyeCloseL = 0.0f;
    Output.EyeCloseR = 0.0f;

    // Update all procedural systems
    UpdateBlinking(DeltaTime);
    UpdateBreathing(DeltaTime);
    UpdateSaccades(DeltaTime);
    UpdateHeadMovement(DeltaTime);
}

void UProceduralAnimationComponent::UpdateBlinking(float DeltaTime)
{
    if (!bBlinkingEnabled)
        return;

    if (bIsBlinking)
    {
        // Animate the blink
        BlinkProgress += DeltaTime / CurrentBlinkDuration;

        if (BlinkProgress >= 1.0f)
        {
            // Blink complete
            bIsBlinking = false;
            BlinkProgress = 0.0f;

            // Check for double blink
            if (bPendingDoubleBlink)
            {
                bPendingDoubleBlink = false;
                // Start second blink after tiny delay
                BlinkTimer = -0.05f;
            }
            else
            {
                // Schedule next blink
                NextBlinkTime = GetCurrentBlinkInterval();
                BlinkTimer = 0.0f;
            }
        }
        else
        {
            // Calculate blink curve
            // Fast close (0-30%), slower open (30-100%)
            float BlinkValue;
            if (BlinkProgress < 0.3f)
            {
                // Fast close - ease out
                float t = BlinkProgress / 0.3f;
                BlinkValue = t * t;  // Quadratic ease-in for fast close
            }
            else
            {
                // Slower open - ease in
                float t = (BlinkProgress - 0.3f) / 0.7f;
                BlinkValue = 1.0f - (t * t);  // Quadratic ease-out for slower open
            }

            Output.EyeCloseL = BlinkValue;
            Output.EyeCloseR = BlinkValue;
        }
    }
    else
    {
        // Wait for next blink
        BlinkTimer += DeltaTime;
        if (BlinkTimer >= NextBlinkTime)
        {
            TriggerBlink();

            // Random chance for double blink
            if (FMath::RandRange(0.0f, 1.0f) < BlinkSettings.DoubleBinkProbability)
            {
                bPendingDoubleBlink = true;
            }
        }
    }
}

void UProceduralAnimationComponent::UpdateBreathing(float DeltaTime)
{
    if (!bBreathingEnabled)
        return;

    float BreathRate = GetCurrentBreathRate();
    float CycleTime = 60.0f / BreathRate;

    // Handle sigh (deep breath)
    if (bSighPending && BreathingPhase < 0.1f)
    {
        bSighPending = false;
        // Sigh is a deeper, slower breath
        CycleTime *= 1.5f;
    }

    // Advance phase
    BreathingPhase += DeltaTime / CycleTime;
    if (BreathingPhase >= 1.0f)
    {
        BreathingPhase -= 1.0f;
    }

    Output.BreathPhase = BreathingPhase;

    // Determine if inhaling or exhaling
    bool bInhaling = (BreathingPhase < BreathingSettings.InhaleRatio);

    // Fire event on phase change
    if (bInhaling != bWasInhaling)
    {
        bWasInhaling = bInhaling;
        OnBreathCycle.Broadcast(bInhaling);
    }

    // Calculate breath curve
    float BreathValue;
    if (bInhaling)
    {
        // Inhale: 0 -> 1
        float t = BreathingPhase / BreathingSettings.InhaleRatio;
        // Smooth step for natural feel
        BreathValue = t * t * (3.0f - 2.0f * t);
    }
    else
    {
        // Exhale: 1 -> 0
        float t = (BreathingPhase - BreathingSettings.InhaleRatio) / (1.0f - BreathingSettings.InhaleRatio);
        // Slightly faster exhale
        BreathValue = 1.0f - (t * t);
    }

    // Apply to output
    Output.ChestExpansion = BreathValue * BreathingSettings.ChestAmplitude;
    Output.ShoulderRise = BreathValue * BreathingSettings.ShoulderAmplitude;
}

void UProceduralAnimationComponent::UpdateSaccades(float DeltaTime)
{
    if (!bSaccadesEnabled)
    {
        Output.EyeRotationOffset = FRotator::ZeroRotator;
        return;
    }

    // Handle forced eye offset
    if (bHasForcedEyeOffset)
    {
        CurrentEyeOffset = FMath::Vector2DInterpTo(CurrentEyeOffset, ForcedEyeOffset, DeltaTime, 10.0f);
        Output.EyeRotationOffset = FRotator(CurrentEyeOffset.Y, CurrentEyeOffset.X, 0.0f);
        return;
    }

    // Update fixation timer
    FixationTimer -= DeltaTime;

    if (FixationTimer <= 0.0f)
    {
        // Time for a new saccade
        SaccadeTimer += DeltaTime;

        if (SaccadeTimer >= 1.0f / SaccadeSettings.Frequency)
        {
            SaccadeTimer = 0.0f;

            // Decide if returning to center or new random position
            if (FMath::RandRange(0.0f, 1.0f) < SaccadeSettings.CenterReturnProbability)
            {
                TargetEyeOffset = FVector2D::ZeroVector;
            }
            else
            {
                // Generate new random target
                float Angle = FMath::RandRange(0.0f, 2.0f * PI);
                float Distance = FMath::RandRange(0.0f, SaccadeSettings.MaxAmplitude);
                TargetEyeOffset.X = FMath::Cos(Angle) * Distance;
                TargetEyeOffset.Y = FMath::Sin(Angle) * Distance;
            }

            // Set fixation duration
            FixationTimer = FMath::RandRange(SaccadeSettings.FixationDuration.X, SaccadeSettings.FixationDuration.Y);
        }
    }

    // Interpolate towards target (saccades are fast)
    float InterpSpeed = SaccadeSettings.Speed;
    CurrentEyeOffset = FMath::Vector2DInterpTo(CurrentEyeOffset, TargetEyeOffset, DeltaTime, InterpSpeed);

    // Apply to output
    Output.EyeRotationOffset = FRotator(CurrentEyeOffset.Y, CurrentEyeOffset.X, 0.0f);
}

void UProceduralAnimationComponent::UpdateHeadMovement(float DeltaTime)
{
    if (!bHeadMovementEnabled)
    {
        Output.HeadRotationOffset = FRotator::ZeroRotator;
        return;
    }

    // Base head sway (always active)
    HeadSwayPhaseX += DeltaTime * HeadMovementSettings.SwayFrequency * 2.0f * PI;
    HeadSwayPhaseY += DeltaTime * HeadMovementSettings.SwayFrequency * 0.7f * 2.0f * PI;  // Different frequency for Y

    if (HeadSwayPhaseX > 2.0f * PI) HeadSwayPhaseX -= 2.0f * PI;
    if (HeadSwayPhaseY > 2.0f * PI) HeadSwayPhaseY -= 2.0f * PI;

    float SwayX = FMath::Sin(HeadSwayPhaseX) * HeadMovementSettings.SwayAmplitude;
    float SwayY = FMath::Sin(HeadSwayPhaseY) * HeadMovementSettings.SwayAmplitude * 0.5f;

    // Listening nods (when listening)
    float NodOffset = 0.0f;
    if (bIsListening)
    {
        ListeningNodPhase += DeltaTime * HeadMovementSettings.ListeningNodFrequency * 2.0f * PI;
        if (ListeningNodPhase > 2.0f * PI) ListeningNodPhase -= 2.0f * PI;

        // Asymmetric nod (faster down, slower up)
        float NodValue = FMath::Sin(ListeningNodPhase);
        if (NodValue > 0.0f)
        {
            NodOffset = NodValue * HeadMovementSettings.ListeningNodAmplitude;
        }
        else
        {
            NodOffset = NodValue * HeadMovementSettings.ListeningNodAmplitude * 0.5f;
        }
    }

    // Combine movements
    Output.HeadRotationOffset = FRotator(
        SwayY + NodOffset,  // Pitch (up/down)
        SwayX,              // Yaw (left/right)
        SwayX * 0.3f        // Roll (slight tilt)
    );
}

float UProceduralAnimationComponent::GetCurrentBlinkInterval() const
{
    float BaseInterval = FMath::RandRange(BlinkSettings.MinInterval, BlinkSettings.MaxInterval);

    if (bIsSpeaking)
    {
        BaseInterval /= BlinkSettings.SpeakingBlinkMultiplier;
    }
    else if (bIsThinking)
    {
        BaseInterval /= BlinkSettings.ThinkingBlinkMultiplier;
    }

    return BaseInterval;
}

float UProceduralAnimationComponent::GetCurrentBreathRate() const
{
    if (bIsSpeaking)
    {
        return BreathingSettings.SpeakingBreathsPerMinute;
    }
    return BreathingSettings.RestingBreathsPerMinute;
}

// ========== Enable/Disable ==========

void UProceduralAnimationComponent::SetBlinkingEnabled(bool bEnabled)
{
    bBlinkingEnabled = bEnabled;
    if (!bEnabled)
    {
        bIsBlinking = false;
        BlinkProgress = 0.0f;
        Output.EyeCloseL = 0.0f;
        Output.EyeCloseR = 0.0f;
    }
}

void UProceduralAnimationComponent::SetBreathingEnabled(bool bEnabled)
{
    bBreathingEnabled = bEnabled;
}

void UProceduralAnimationComponent::SetSaccadesEnabled(bool bEnabled)
{
    bSaccadesEnabled = bEnabled;
    if (!bEnabled)
    {
        CurrentEyeOffset = FVector2D::ZeroVector;
        TargetEyeOffset = FVector2D::ZeroVector;
    }
}

void UProceduralAnimationComponent::SetHeadMovementEnabled(bool bEnabled)
{
    bHeadMovementEnabled = bEnabled;
}

void UProceduralAnimationComponent::SetAllEnabled(bool bEnabled)
{
    SetBlinkingEnabled(bEnabled);
    SetBreathingEnabled(bEnabled);
    SetSaccadesEnabled(bEnabled);
    SetHeadMovementEnabled(bEnabled);
}

// ========== Manual Triggers ==========

void UProceduralAnimationComponent::TriggerBlink()
{
    if (!bIsBlinking)
    {
        bIsBlinking = true;
        BlinkProgress = 0.0f;
        CurrentBlinkDuration = BlinkSettings.BlinkDuration;
        OnBlinkTriggered.Broadcast();
    }
}

void UProceduralAnimationComponent::TriggerDoubleBlink()
{
    TriggerBlink();
    bPendingDoubleBlink = true;
}

void UProceduralAnimationComponent::TriggerDeliberateBlink(float Duration)
{
    if (!bIsBlinking)
    {
        bIsBlinking = true;
        BlinkProgress = 0.0f;
        CurrentBlinkDuration = Duration;
        OnBlinkTriggered.Broadcast();
    }
}

void UProceduralAnimationComponent::TriggerSigh()
{
    bSighPending = true;
}

void UProceduralAnimationComponent::SetEyeOffset(FVector2D Offset)
{
    bHasForcedEyeOffset = true;
    ForcedEyeOffset = Offset;
}

void UProceduralAnimationComponent::ClearEyeOffset()
{
    bHasForcedEyeOffset = false;
}

// ========== State Modifiers ==========

void UProceduralAnimationComponent::SetSpeakingState(bool bSpeaking)
{
    bIsSpeaking = bSpeaking;
}

void UProceduralAnimationComponent::SetThinkingState(bool bThinking)
{
    bIsThinking = bThinking;
}

void UProceduralAnimationComponent::SetListeningState(bool bListening)
{
    bIsListening = bListening;
    if (!bListening)
    {
        ListeningNodPhase = 0.0f;
    }
}
