//=============================================================================
// EchoPulseResonanceChamber.cpp
//
// Implementation of resonance dynamics for the Deep Tree Echo cognitive system.
// Models a damped harmonic oscillator with feedback to simulate echo propagation
// patterns aligned with the 12-step cognitive cycle.
//
// Copyright (c) 2025 Deep Tree Echo Project
//=============================================================================

#include "EchoPulseResonanceChamber.h"

UEchoPulseResonanceChamber::UEchoPulseResonanceChamber()
{
    // Initialize state
    ResonanceLevel = 0.0f;
    ResonanceVelocity = 0.0f;
    DominantFrequency = 0.0f;
    PhaseAlignment = 0.0f;
    PreviousResonanceLevel = 0.0f;
    AccumulatedPhase = 0.0f;
    bWasResonating = false;

    // Configuration defaults
    IntensityFactor = 1.0f;
    DampingCoefficient = 0.15f;      // Moderate damping
    NaturalFrequency = 1.0f;         // 1 Hz natural frequency
    FeedbackGain = 0.3f;             // 30% feedback
    ResonanceThreshold = 50.0f;      // 50% threshold
    bFeedbackEnabled = true;

    // Pre-allocate signal history
    SignalHistory.Reserve(64);
}

void UEchoPulseResonanceChamber::UpdateResonance(float Signal)
{
    // Store previous level for velocity calculation
    PreviousResonanceLevel = ResonanceLevel;

    // Apply intensity factor to input signal
    float ScaledSignal = Signal * IntensityFactor;

    // Update resonance level with signal input
    ResonanceLevel += ScaledSignal;

    // Apply feedback if enabled
    if (bFeedbackEnabled && ResonanceLevel > ResonanceThreshold * 0.5f)
    {
        float FeedbackContribution = ResonanceLevel * FeedbackGain * 0.1f;
        ResonanceLevel += FeedbackContribution;
    }

    // Clamp to valid range
    ResonanceLevel = FMath::Clamp(ResonanceLevel, 0.0f, 100.0f);

    // Calculate velocity
    ResonanceVelocity = ResonanceLevel - PreviousResonanceLevel;

    // Update signal history for frequency analysis
    SignalHistory.Add(ScaledSignal);
    if (SignalHistory.Num() > 64)
    {
        SignalHistory.RemoveAt(0);
    }

    // Update dominant frequency
    DominantFrequency = CalculateDominantFrequency();

    // Check for threshold crossing events
    bool bCurrentlyResonating = ResonanceLevel >= ResonanceThreshold;
    if (bCurrentlyResonating && !bWasResonating)
    {
        // Just crossed into resonating state
        OnResonanceTriggered.Broadcast(ResonanceLevel);
        UE_LOG(LogTemp, Log, TEXT("EchoPulseResonanceChamber: Resonance triggered at level %.1f"), ResonanceLevel);
    }
    else if (!bCurrentlyResonating && bWasResonating)
    {
        // Just crossed out of resonating state
        OnResonanceFaded.Broadcast();
        UE_LOG(LogTemp, Verbose, TEXT("EchoPulseResonanceChamber: Resonance faded"));
    }
    bWasResonating = bCurrentlyResonating;
}

void UEchoPulseResonanceChamber::UpdateResonanceWithStep(float Signal, int32 CognitiveStep)
{
    // Get phase multiplier for this cognitive step
    float PhaseMultiplier = GetStepPhaseMultiplier(CognitiveStep);

    // Apply phase-aligned signal
    float AlignedSignal = Signal * PhaseMultiplier;

    // Update phase alignment based on step
    // Steps 1,5,9 (Stream 1) - Perceiving - phase 0°
    // Steps 2,6,10 (Stream 2) - Acting - phase 120°
    // Steps 3,7,11 (Stream 3) - Reflecting - phase 240°
    int32 StreamIndex = ((CognitiveStep - 1) % 3);
    PhaseAlignment = FMath::Cos(StreamIndex * (2.0f * PI / 3.0f) + AccumulatedPhase);

    // Normalize to 0-1 range
    PhaseAlignment = (PhaseAlignment + 1.0f) / 2.0f;

    // Update accumulated phase
    AccumulatedPhase += (2.0f * PI * NaturalFrequency) / 12.0f;
    if (AccumulatedPhase > 2.0f * PI)
    {
        AccumulatedPhase -= 2.0f * PI;
    }

    // Call base update with aligned signal
    UpdateResonance(AlignedSignal);
}

void UEchoPulseResonanceChamber::ProcessDecay(float DeltaTime)
{
    // Store previous level
    PreviousResonanceLevel = ResonanceLevel;

    // Apply damped exponential decay
    // Using critically damped harmonic oscillator model
    float DecayFactor = FMath::Exp(-DampingCoefficient * NaturalFrequency * DeltaTime);
    ResonanceLevel *= DecayFactor;

    // Apply minimum threshold
    if (ResonanceLevel < 0.01f)
    {
        ResonanceLevel = 0.0f;
    }

    // Update velocity
    ResonanceVelocity = ResonanceLevel - PreviousResonanceLevel;

    // Check for threshold crossing (fading)
    bool bCurrentlyResonating = ResonanceLevel >= ResonanceThreshold;
    if (!bCurrentlyResonating && bWasResonating)
    {
        OnResonanceFaded.Broadcast();
        UE_LOG(LogTemp, Verbose, TEXT("EchoPulseResonanceChamber: Resonance faded through decay"));
    }
    bWasResonating = bCurrentlyResonating;
}

void UEchoPulseResonanceChamber::Reset()
{
    ResonanceLevel = 0.0f;
    ResonanceVelocity = 0.0f;
    DominantFrequency = 0.0f;
    PhaseAlignment = 0.0f;
    PreviousResonanceLevel = 0.0f;
    AccumulatedPhase = 0.0f;
    bWasResonating = false;
    SignalHistory.Empty();

    UE_LOG(LogTemp, Verbose, TEXT("EchoPulseResonanceChamber: Reset complete"));
}

void UEchoPulseResonanceChamber::ApplyFeedback(float FeedbackAmount)
{
    if (!bFeedbackEnabled)
    {
        return;
    }

    // Apply feedback with gain
    float ScaledFeedback = FeedbackAmount * FeedbackGain;
    ResonanceLevel += ScaledFeedback;
    ResonanceLevel = FMath::Clamp(ResonanceLevel, 0.0f, 100.0f);
}

void UEchoPulseResonanceChamber::SetFeedbackGain(float Gain)
{
    FeedbackGain = FMath::Clamp(Gain, 0.0f, 2.0f);
}

void UEchoPulseResonanceChamber::SetFeedbackEnabled(bool bEnabled)
{
    bFeedbackEnabled = bEnabled;
}

void UEchoPulseResonanceChamber::SyncWithStream(int32 StreamIndex, float StreamActivation)
{
    // Validate stream index (0, 1, or 2 for the three consciousness streams)
    int32 ValidStreamIndex = FMath::Clamp(StreamIndex, 0, 2);

    // Calculate phase offset for this stream (120° apart)
    float StreamPhase = ValidStreamIndex * (2.0f * PI / 3.0f);

    // Calculate alignment between resonance phase and stream phase
    float PhaseDifference = FMath::Abs(AccumulatedPhase - StreamPhase);
    if (PhaseDifference > PI)
    {
        PhaseDifference = 2.0f * PI - PhaseDifference;
    }

    // Update phase alignment (1.0 = perfect alignment, 0.0 = opposite phase)
    PhaseAlignment = 1.0f - (PhaseDifference / PI);

    // Apply stream activation as modulated signal
    // Higher alignment = stronger contribution
    float ModulatedSignal = StreamActivation * PhaseAlignment;
    UpdateResonance(ModulatedSignal);

    UE_LOG(LogTemp, Verbose, TEXT("EchoPulseResonanceChamber: Synced with stream %d, alignment %.2f"),
           ValidStreamIndex, PhaseAlignment);
}

float UEchoPulseResonanceChamber::CalculateDominantFrequency() const
{
    if (SignalHistory.Num() < 8)
    {
        return 0.0f;
    }

    // Simple zero-crossing frequency estimation
    int32 ZeroCrossings = 0;
    float PreviousValue = SignalHistory[0];
    float Mean = 0.0f;

    // Calculate mean
    for (float Value : SignalHistory)
    {
        Mean += Value;
    }
    Mean /= SignalHistory.Num();

    // Count zero crossings (relative to mean)
    for (int32 i = 1; i < SignalHistory.Num(); i++)
    {
        float CurrentValue = SignalHistory[i] - Mean;
        float PrevValue = SignalHistory[i - 1] - Mean;

        if ((CurrentValue >= 0 && PrevValue < 0) || (CurrentValue < 0 && PrevValue >= 0))
        {
            ZeroCrossings++;
        }
    }

    // Estimate frequency from zero crossings
    // Each complete cycle has 2 zero crossings
    // Assuming 20Hz sampling rate (50ms tick)
    float SamplingRate = 20.0f;
    float EstimatedFrequency = (ZeroCrossings * SamplingRate) / (2.0f * SignalHistory.Num());

    return EstimatedFrequency;
}

float UEchoPulseResonanceChamber::GetStepPhaseMultiplier(int32 CognitiveStep) const
{
    // Validate step (1-12)
    int32 ValidStep = FMath::Clamp(CognitiveStep, 1, 12);

    // Triadic synchronization points have boosted response
    // {1,5,9}, {2,6,10}, {3,7,11}, {4,8,12} are triadic sync points
    bool bIsTriadicSync = (ValidStep == 1 || ValidStep == 5 || ValidStep == 9 ||
                          ValidStep == 2 || ValidStep == 6 || ValidStep == 10 ||
                          ValidStep == 3 || ValidStep == 7 || ValidStep == 11 ||
                          ValidStep == 4 || ValidStep == 8 || ValidStep == 12);

    // All steps are triadic sync points in the 12-step model
    // But primary sync points (1,5,9) get highest boost
    float BaseMultiplier = 1.0f;

    if (ValidStep == 1 || ValidStep == 5 || ValidStep == 9)
    {
        // Primary triadic sync - Perceiving stream pivots
        BaseMultiplier = 1.5f;
    }
    else if (ValidStep == 2 || ValidStep == 6 || ValidStep == 10)
    {
        // Secondary triadic sync - Acting stream pivots
        BaseMultiplier = 1.3f;
    }
    else if (ValidStep == 3 || ValidStep == 7 || ValidStep == 11)
    {
        // Tertiary triadic sync - Reflecting stream pivots
        BaseMultiplier = 1.4f;
    }
    else
    {
        // Integration/transition steps
        BaseMultiplier = 1.1f;
    }

    // Add phase-based modulation
    float StepPhase = (ValidStep - 1) * (2.0f * PI / 12.0f);
    float PhaseModulation = 0.5f * (1.0f + FMath::Cos(StepPhase - AccumulatedPhase));

    return BaseMultiplier * (0.7f + 0.3f * PhaseModulation);
}
