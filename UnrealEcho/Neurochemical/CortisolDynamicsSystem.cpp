#include "CortisolDynamicsSystem.h"

UCortisolDynamicsSystem::UCortisolDynamicsSystem()
{
    TimeScale = 1.0f;
    ClearanceHalfLife = 30.0f;  // Game-time seconds (~90 min real scaled)
    CRHToACTHGain = 0.8f;
    ACTHToCortisolGain = 0.6f;
    CRHDecayRate = 0.15f;
    ACTHDecayRate = 0.1f;
    ChronicStressAccumulationRate = 0.005f;
    ChronicStressDissipationRate = 0.002f;
    NormalFreeFraction = 0.05f;

    Initialize();
}

void UCortisolDynamicsSystem::Initialize()
{
    State = FCortisolDynamicsState();
    ActiveStressEvents.Empty();
}

void UCortisolDynamicsSystem::Update(float DeltaTime)
{
    if (DeltaTime <= 0.0f)
    {
        return;
    }

    UpdateCircadianRhythm(DeltaTime);
    UpdateStressEvents(DeltaTime);
    UpdateChronicStress(DeltaTime);
    UpdateHPAAxis(DeltaTime);
    UpdateReceptorSensitivity(DeltaTime);

    // Calculate net cortisol change
    float Production = CalculateCortisolProduction(DeltaTime);
    float Clearance = CalculateCortisolClearance(DeltaTime);

    State.TotalCortisol += Production - Clearance;
    State.TotalCortisol = FMath::Clamp(State.TotalCortisol, 0.0f, 1.0f);

    UpdateFreeAndBoundCortisol();
}

void UCortisolDynamicsSystem::Reset()
{
    Initialize();
}

// ===== Stress Input =====

void UCortisolDynamicsSystem::ApplyStressor(float Intensity, float Duration)
{
    float ClampedIntensity = FMath::Clamp(Intensity, 0.0f, 1.0f);
    float ClampedDuration = FMath::Max(Duration, 0.1f);

    FStressEvent Event(ClampedIntensity, ClampedDuration, EStressEventType::Acute);
    ActiveStressEvents.Add(Event);

    // Immediate CRH spike proportional to stressor intensity
    State.HPAAxis.CRHLevel += ClampedIntensity * 0.5f;
    State.HPAAxis.CRHLevel = FMath::Clamp(State.HPAAxis.CRHLevel, 0.0f, 1.0f);

    UE_LOG(LogTemp, Log, TEXT("CortisolDynamics: Acute stressor applied (intensity=%.2f, duration=%.1fs)"),
           ClampedIntensity, ClampedDuration);
}

void UCortisolDynamicsSystem::ApplyChronicStressor(float Intensity)
{
    State.bChronicStressActive = true;
    State.ChronicStressorIntensity = FMath::Clamp(Intensity, 0.0f, 1.0f);

    UE_LOG(LogTemp, Log, TEXT("CortisolDynamics: Chronic stressor applied (intensity=%.2f)"),
           State.ChronicStressorIntensity);
}

void UCortisolDynamicsSystem::RemoveChronicStressor()
{
    State.bChronicStressActive = false;
    State.ChronicStressorIntensity = 0.0f;

    UE_LOG(LogTemp, Log, TEXT("CortisolDynamics: Chronic stressor removed"));
}

// ===== Output Queries =====

float UCortisolDynamicsSystem::GetCortisolLevel() const
{
    return State.TotalCortisol;
}

float UCortisolDynamicsSystem::GetFreeCortisolLevel() const
{
    return State.FreeCortisol;
}

float UCortisolDynamicsSystem::GetStressLoad() const
{
    return State.ChronicStressLoad;
}

// ===== Configuration =====

void UCortisolDynamicsSystem::SetTimeOfDay(float Hours)
{
    State.Circadian.TimeOfDay = FMath::Fmod(Hours, 24.0f);
    if (State.Circadian.TimeOfDay < 0.0f)
    {
        State.Circadian.TimeOfDay += 24.0f;
    }
}

void UCortisolDynamicsSystem::SetTimeScale(float Scale)
{
    TimeScale = FMath::Max(Scale, 0.0f);
}

int32 UCortisolDynamicsSystem::GetActiveStressEventCount() const
{
    int32 Count = 0;
    for (const FStressEvent& Event : ActiveStressEvents)
    {
        if (Event.bActive)
        {
            Count++;
        }
    }
    return Count;
}

// ===== Internal Methods =====

void UCortisolDynamicsSystem::UpdateHPAAxis(float DeltaTime)
{
    FHPAAxisState& HPA = State.HPAAxis;

    // Negative feedback: high cortisol suppresses CRH release
    // Reduced by chronic stress and receptor downregulation
    float FeedbackStrength = HPA.NegativeFeedbackGain * State.Receptors.Sensitivity;
    float FeedbackSuppression = State.TotalCortisol * FeedbackStrength * 0.5f;

    // Chronic stress reduces negative feedback effectiveness
    HPA.NegativeFeedbackGain = FMath::Lerp(1.0f, 0.4f, State.ChronicStressLoad);

    // CRH dynamics: stress drives production, feedback and decay reduce it
    float CRHDrive = 0.0f;
    if (State.bChronicStressActive)
    {
        CRHDrive += State.ChronicStressorIntensity * 0.2f;
    }
    for (const FStressEvent& Event : ActiveStressEvents)
    {
        if (Event.bActive)
        {
            // Stress events drive CRH production, tapering over their duration
            float RemainingFraction = 1.0f - (Event.ElapsedTime / FMath::Max(Event.Duration, 0.1f));
            CRHDrive += Event.Intensity * RemainingFraction * 0.3f;
        }
    }

    HPA.CRHLevel += (CRHDrive - FeedbackSuppression - HPA.CRHLevel * CRHDecayRate) * DeltaTime;
    HPA.CRHLevel = FMath::Clamp(HPA.CRHLevel, 0.0f, 1.0f);

    // ACTH dynamics: driven by CRH with slight delay (modeled via rate)
    float ACTHDrive = HPA.CRHLevel * CRHToACTHGain;
    HPA.ACTHLevel += (ACTHDrive - HPA.ACTHLevel * ACTHDecayRate) * DeltaTime;
    HPA.ACTHLevel = FMath::Clamp(HPA.ACTHLevel, 0.0f, 1.0f);

    // Cortisol output from adrenal: driven by ACTH
    HPA.CortisolOutput = HPA.ACTHLevel * ACTHToCortisolGain;

    // Overall axis activation metric
    HPA.AxisActivation = (HPA.CRHLevel + HPA.ACTHLevel + HPA.CortisolOutput) / 3.0f;
}

void UCortisolDynamicsSystem::UpdateCircadianRhythm(float DeltaTime)
{
    FCircadianRhythm& Circadian = State.Circadian;

    // Advance time of day
    Circadian.TimeOfDay += DeltaTime * TimeScale / 60.0f; // 1 game-second = 1 minute of simulated day
    if (Circadian.TimeOfDay >= 24.0f)
    {
        Circadian.TimeOfDay -= 24.0f;
    }

    // Cosine-based circadian rhythm: peak at PeakHour, trough 12 hours later
    float PhaseRadians = 2.0f * PI * (Circadian.TimeOfDay - Circadian.PeakHour) / 24.0f;
    Circadian.CurrentContribution = Circadian.Baseline + Circadian.Amplitude * FMath::Cos(PhaseRadians);
    Circadian.CurrentContribution = FMath::Max(Circadian.CurrentContribution, 0.0f);
}

void UCortisolDynamicsSystem::UpdateStressEvents(float DeltaTime)
{
    for (int32 i = ActiveStressEvents.Num() - 1; i >= 0; i--)
    {
        FStressEvent& Event = ActiveStressEvents[i];
        if (!Event.bActive)
        {
            continue;
        }

        Event.ElapsedTime += DeltaTime;

        // Expire finite-duration events
        if (Event.Type == EStressEventType::Acute && Event.ElapsedTime >= Event.Duration)
        {
            Event.bActive = false;
        }
    }

    // Clean up expired events
    ActiveStressEvents.RemoveAll([](const FStressEvent& Event) {
        return !Event.bActive;
    });
}

void UCortisolDynamicsSystem::UpdateReceptorSensitivity(float DeltaTime)
{
    FCortisolReceptorState& Receptors = State.Receptors;

    if (State.TotalCortisol > Receptors.DownregulationThreshold)
    {
        // High cortisol causes receptor downregulation
        float Excess = State.TotalCortisol - Receptors.DownregulationThreshold;
        Receptors.Sensitivity -= Excess * Receptors.DownregulationRate * DeltaTime;
    }
    else if (State.TotalCortisol < Receptors.RecoveryThreshold)
    {
        // Low cortisol allows receptor recovery
        float Deficit = Receptors.RecoveryThreshold - State.TotalCortisol;
        Receptors.Sensitivity += Deficit * Receptors.RecoveryRate * DeltaTime;
    }

    Receptors.Sensitivity = FMath::Clamp(Receptors.Sensitivity, 0.1f, 1.0f);
}

void UCortisolDynamicsSystem::UpdateChronicStress(float DeltaTime)
{
    if (State.bChronicStressActive)
    {
        // Chronic stress load accumulates while stressor is active
        State.ChronicStressLoad += State.ChronicStressorIntensity * ChronicStressAccumulationRate * DeltaTime;
    }
    else
    {
        // Chronic stress load slowly dissipates
        State.ChronicStressLoad -= ChronicStressDissipationRate * DeltaTime;
    }

    State.ChronicStressLoad = FMath::Clamp(State.ChronicStressLoad, 0.0f, 1.0f);
}

float UCortisolDynamicsSystem::CalculateCortisolProduction(float DeltaTime)
{
    // Three sources of cortisol production:
    // 1. HPA axis output (stress-driven)
    // 2. Circadian rhythm (baseline diurnal pattern)
    // 3. Chronic stress tonic elevation

    float HPAContribution = State.HPAAxis.CortisolOutput * DeltaTime;
    float CircadianContribution = State.Circadian.CurrentContribution * 0.1f * DeltaTime;
    float ChronicContribution = State.ChronicStressLoad * 0.05f * DeltaTime;

    return HPAContribution + CircadianContribution + ChronicContribution;
}

float UCortisolDynamicsSystem::CalculateCortisolClearance(float DeltaTime)
{
    // First-order clearance kinetics (hepatic metabolism)
    // Half-life determines the decay constant: k = ln(2) / t_half
    float DecayConstant = 0.693f / FMath::Max(ClearanceHalfLife, 0.1f);

    // Receptor sensitivity modulates effective clearance
    // Lower sensitivity = body is less responsive = slower effective clearance
    float EffectiveDecay = DecayConstant * State.Receptors.Sensitivity;

    return State.TotalCortisol * EffectiveDecay * DeltaTime;
}

void UCortisolDynamicsSystem::UpdateFreeAndBoundCortisol()
{
    // Under normal conditions, ~5% of cortisol is free (bioactive)
    // Under high cortisol, CBG saturates and free fraction increases
    float FreeFraction = NormalFreeFraction;

    // CBG saturation effect: above ~0.6 total cortisol, free fraction increases
    if (State.TotalCortisol > 0.6f)
    {
        float Excess = State.TotalCortisol - 0.6f;
        FreeFraction += Excess * 0.5f; // Free fraction rises sharply with saturation
    }

    FreeFraction = FMath::Clamp(FreeFraction, NormalFreeFraction, 0.5f);

    State.FreeCortisol = State.TotalCortisol * FreeFraction;
    State.BoundCortisol = State.TotalCortisol - State.FreeCortisol;
}
