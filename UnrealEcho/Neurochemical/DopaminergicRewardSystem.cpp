#include "DopaminergicRewardSystem.h"

UDopaminergicRewardSystem::UDopaminergicRewardSystem()
{
    HabitationDissipationRate = 0.003f;
    PhasicFatigueRecoveryRate = 0.05f;
    NAccToPFCCoupling = 0.6f;
    NAccToStriatalCoupling = 0.8f;
    TonicSetPoint = 0.4f;

    Initialize();
}

void UDopaminergicRewardSystem::Initialize()
{
    State = FDopaminergicRewardState();
    PendingRewardEvents.Empty();
}

void UDopaminergicRewardSystem::Update(float DeltaTime)
{
    if (DeltaTime <= 0.0f)
    {
        return;
    }

    UpdateVTANeurons(DeltaTime);
    UpdateDopamineLevels(DeltaTime);
    UpdateReceptors(DeltaTime);
    UpdateHedonicHabituation(DeltaTime);
    UpdateMotivationalDrive();

    // Consume processed reward events
    PendingRewardEvents.Empty();
}

void UDopaminergicRewardSystem::Reset()
{
    Initialize();
}

// ===== Reward Input =====

void UDopaminergicRewardSystem::RegisterRewardOutcome(float ActualReward, ERewardEventType RewardType)
{
    float ClampedReward = FMath::Clamp(ActualReward, 0.0f, 1.0f);

    // Compute RPE
    float RPE = ClampedReward - State.RewardPrediction.PredictedValue;

    // Update running statistics
    float Alpha = 0.1f;
    State.RewardPrediction.RPERunningMean =
        (1.0f - Alpha) * State.RewardPrediction.RPERunningMean + Alpha * RPE;
    float RPEDeviation = RPE - State.RewardPrediction.RPERunningMean;
    State.RewardPrediction.RPERunningVariance =
        (1.0f - Alpha) * State.RewardPrediction.RPERunningVariance + Alpha * RPEDeviation * RPEDeviation;

    State.RewardPrediction.ActualReward = ClampedReward;
    State.RewardPrediction.RPE = RPE;

    // TD update: V(t) <- V(t) + alpha * (r + gamma * V(t+1) - V(t))
    // In the absence of explicit future-value estimation, V(t+1) ≈ V(t)
    State.RewardPrediction.PredictedValue +=
        State.RewardPrediction.LearningRate * RPE;
    State.RewardPrediction.PredictedValue =
        FMath::Clamp(State.RewardPrediction.PredictedValue, 0.0f, 1.0f);

    // Trigger phasic VTA response proportional to RPE
    ApplyRPEToPhasicResponse(RPE);

    // Queue event for within-frame processing
    PendingRewardEvents.Add(FRewardEvent(ClampedReward, State.RewardPrediction.PredictedValue, RewardType));

    UE_LOG(LogTemp, Log,
           TEXT("DopaminergicReward: Outcome registered (actual=%.2f, predicted=%.2f, RPE=%.2f)"),
           ClampedReward, State.RewardPrediction.PredictedValue - State.RewardPrediction.LearningRate * RPE, RPE);
}

void UDopaminergicRewardSystem::ProcessUnexpectedReward(float Magnitude, ERewardEventType RewardType)
{
    float ClampedMagnitude = FMath::Clamp(Magnitude, 0.0f, 1.0f);

    // Habituation scales down the effective response
    float HabitationScale = 1.0f - State.HedonicHabituation * 0.6f;

    // Phasic burst — the full magnitude is the RPE (prediction was 0)
    float EffectiveRPE = ClampedMagnitude * HabitationScale;
    ApplyRPEToPhasicResponse(EffectiveRPE);

    // Accumulate hedonic habituation for repeated unexpected rewards
    State.HedonicHabituation += ClampedMagnitude * 0.05f;
    State.HedonicHabituation = FMath::Clamp(State.HedonicHabituation, 0.0f, 1.0f);

    PendingRewardEvents.Add(FRewardEvent(ClampedMagnitude, 0.0f, RewardType));

    UE_LOG(LogTemp, Log,
           TEXT("DopaminergicReward: Unexpected reward processed (magnitude=%.2f, habituation=%.2f)"),
           ClampedMagnitude, State.HedonicHabituation);
}

void UDopaminergicRewardSystem::ProcessRewardOmission()
{
    // Expected reward did not arrive — dip proportional to prediction
    float PredictedValue = State.RewardPrediction.PredictedValue;

    // RPE is negative: received 0 when PredictedValue > 0
    float RPE = -PredictedValue;
    State.RewardPrediction.ActualReward = 0.0f;
    State.RewardPrediction.RPE = RPE;

    // Update prediction toward 0 (omission)
    State.RewardPrediction.PredictedValue +=
        State.RewardPrediction.LearningRate * RPE;
    State.RewardPrediction.PredictedValue =
        FMath::Clamp(State.RewardPrediction.PredictedValue, 0.0f, 1.0f);

    ApplyRPEToPhasicResponse(RPE);

    UE_LOG(LogTemp, Log,
           TEXT("DopaminergicReward: Reward omission (predicted=%.2f, RPE=%.2f)"),
           PredictedValue, RPE);
}

void UDopaminergicRewardSystem::TriggerCuedAnticipation(float CueValue)
{
    float ClampedCue = FMath::Clamp(CueValue, 0.0f, 1.0f);

    // Update value prediction to reflect cue information
    float PreviousPrediction = State.RewardPrediction.PredictedValue;
    State.RewardPrediction.PredictedValue =
        FMath::Lerp(PreviousPrediction, ClampedCue,
                    State.RewardPrediction.LearningRate * 2.0f);

    // Modest anticipatory VTA elevation (smaller than consummatory burst)
    float AnticipationBurst = ClampedCue * 0.4f * (1.0f - State.HedonicHabituation * 0.3f);
    float FatigueScale = 1.0f - State.VTANeurons.PhasicFatigue;
    AnticipationBurst *= FatigueScale;

    if (AnticipationBurst > State.VTANeurons.PhasicBurstAmplitude)
    {
        State.VTANeurons.PhasicBurstAmplitude = AnticipationBurst;
        State.VTANeurons.PhasicDuration = 1.5f; // Brief anticipatory elevation
        State.VTANeurons.bIsPhasicBurst = true;
    }

    UE_LOG(LogTemp, Log,
           TEXT("DopaminergicReward: Cued anticipation triggered (cueValue=%.2f, burst=%.2f)"),
           ClampedCue, AnticipationBurst);
}

void UDopaminergicRewardSystem::SetValuePrediction(float PredictedValue)
{
    State.RewardPrediction.PredictedValue = FMath::Clamp(PredictedValue, 0.0f, 1.0f);
}

// ===== Output Queries =====

float UDopaminergicRewardSystem::GetNAccDopamine() const
{
    return State.NAccDopamine;
}

float UDopaminergicRewardSystem::GetPFCDopamine() const
{
    return State.PFCDopamine;
}

float UDopaminergicRewardSystem::GetRewardPredictionError() const
{
    return State.RewardPrediction.RPE;
}

float UDopaminergicRewardSystem::GetTonicDopamine() const
{
    return State.VTANeurons.TonicFiringRate;
}

float UDopaminergicRewardSystem::GetPhasicDopamine() const
{
    if (State.VTANeurons.bIsPhasicBurst)
    {
        return State.VTANeurons.PhasicBurstAmplitude;
    }
    return -State.VTANeurons.PhasicDipAmplitude;
}

float UDopaminergicRewardSystem::GetMotivationalDrive() const
{
    return State.MotivationalDrive;
}

float UDopaminergicRewardSystem::GetGoSignal() const
{
    return State.Receptors.GoSignal;
}

float UDopaminergicRewardSystem::GetNoGoSignal() const
{
    return State.Receptors.NoGoSignal;
}

float UDopaminergicRewardSystem::GetHedonicHabituation() const
{
    return State.HedonicHabituation;
}

// ===== Configuration =====

void UDopaminergicRewardSystem::SetLearningRate(float LearningRate)
{
    State.RewardPrediction.LearningRate = FMath::Clamp(LearningRate, 0.001f, 1.0f);
}

void UDopaminergicRewardSystem::SetHabitationDissipationRate(float Rate)
{
    HabitationDissipationRate = FMath::Max(Rate, 0.0f);
}

int32 UDopaminergicRewardSystem::GetPendingRewardEventCount() const
{
    return PendingRewardEvents.Num();
}

// ===== Internal Methods =====

void UDopaminergicRewardSystem::UpdateVTANeurons(float DeltaTime)
{
    FVTANeuronState& VTA = State.VTANeurons;

    // Recover phasic fatigue
    VTA.PhasicFatigue -= PhasicFatigueRecoveryRate * DeltaTime;
    VTA.PhasicFatigue = FMath::Max(VTA.PhasicFatigue, 0.0f);

    // Decay active phasic response toward zero
    if (VTA.PhasicDuration > 0.0f)
    {
        VTA.PhasicDuration -= DeltaTime;

        if (VTA.bIsPhasicBurst)
        {
            VTA.PhasicBurstAmplitude -= VTA.PhasicDecayRate * DeltaTime;
            VTA.PhasicBurstAmplitude = FMath::Max(VTA.PhasicBurstAmplitude, 0.0f);
        }
        else
        {
            VTA.PhasicDipAmplitude -= VTA.PhasicDecayRate * DeltaTime;
            VTA.PhasicDipAmplitude = FMath::Max(VTA.PhasicDipAmplitude, 0.0f);
        }

        if (VTA.PhasicDuration <= 0.0f)
        {
            VTA.PhasicDuration = 0.0f;
            VTA.PhasicBurstAmplitude = 0.0f;
            VTA.PhasicDipAmplitude = 0.0f;
        }
    }

    // Tonic rate drifts toward TonicSetPoint (influenced by chronic reward history)
    float TonicTarget = TonicSetPoint * (1.0f - State.HedonicHabituation * 0.4f);
    VTA.TonicFiringRate = FMath::Lerp(VTA.TonicFiringRate, TonicTarget, 0.02f * DeltaTime);
    VTA.TonicFiringRate = FMath::Clamp(VTA.TonicFiringRate, 0.1f, 1.0f);
}

void UDopaminergicRewardSystem::UpdateDopamineLevels(float DeltaTime)
{
    float Production = CalculateDopamineProduction(DeltaTime);
    float Clearance = CalculateDopamineClearance(DeltaTime);

    State.NAccDopamine += Production - Clearance;
    State.NAccDopamine = FMath::Clamp(State.NAccDopamine, 0.0f, 1.0f);

    // PFC and striatal dopamine track NAcc with coupling factors and slower dynamics
    float PFCTarget = State.NAccDopamine * NAccToPFCCoupling;
    State.PFCDopamine = FMath::Lerp(State.PFCDopamine, PFCTarget, 0.15f * DeltaTime);
    State.PFCDopamine = FMath::Clamp(State.PFCDopamine, 0.0f, 1.0f);

    float StriatalTarget = State.NAccDopamine * NAccToStriatalCoupling;
    State.StriatalDopamine = FMath::Lerp(State.StriatalDopamine, StriatalTarget, 0.2f * DeltaTime);
    State.StriatalDopamine = FMath::Clamp(State.StriatalDopamine, 0.0f, 1.0f);
}

void UDopaminergicRewardSystem::UpdateReceptors(float DeltaTime)
{
    FDopamineReceptorState& Receptors = State.Receptors;
    float DA = State.NAccDopamine;

    // D1 and D2 receptor sensitivity adapt to chronic dopamine levels
    if (DA > Receptors.DownregulationThreshold)
    {
        float Excess = DA - Receptors.DownregulationThreshold;
        Receptors.D1Sensitivity -= Excess * Receptors.DownregulationRate * DeltaTime;
        Receptors.D2Sensitivity -= Excess * Receptors.DownregulationRate * DeltaTime;
    }
    else if (DA < Receptors.RecoveryThreshold)
    {
        float Deficit = Receptors.RecoveryThreshold - DA;
        Receptors.D1Sensitivity += Deficit * Receptors.RecoveryRate * DeltaTime;
        Receptors.D2Sensitivity += Deficit * Receptors.RecoveryRate * DeltaTime;
    }

    Receptors.D1Sensitivity = FMath::Clamp(Receptors.D1Sensitivity, 0.1f, 1.5f);
    Receptors.D2Sensitivity = FMath::Clamp(Receptors.D2Sensitivity, 0.1f, 1.5f);

    // D1 (direct / Go pathway): high DA + high D1 sensitivity → approach
    Receptors.GoSignal = FMath::Clamp(DA * Receptors.D1Sensitivity, 0.0f, 1.0f);

    // D2 (indirect / NoGo pathway): D2 is inhibited by high DA (inverse relationship)
    // High DA suppresses D2, reducing the NoGo signal → approach wins
    float D2Activation = FMath::Max(0.0f, 1.0f - DA) * Receptors.D2Sensitivity;
    Receptors.NoGoSignal = FMath::Clamp(D2Activation, 0.0f, 1.0f);
}

void UDopaminergicRewardSystem::UpdateHedonicHabituation(float DeltaTime)
{
    // Habituation slowly dissipates during baseline (low-reward) periods
    if (State.NAccDopamine < 0.5f)
    {
        float Recovery = (0.5f - State.NAccDopamine) * HabitationDissipationRate * DeltaTime;
        State.HedonicHabituation -= Recovery;
        State.HedonicHabituation = FMath::Max(State.HedonicHabituation, 0.0f);
    }
}

void UDopaminergicRewardSystem::UpdateMotivationalDrive()
{
    // Motivational drive integrates:
    // 1. Tonic dopamine (baseline readiness to engage)
    // 2. Phasic burst (current reward signal)
    // 3. Value prediction (expected future reward)
    // 4. Go vs. NoGo balance

    float TonicComponent = State.VTANeurons.TonicFiringRate * 0.4f;

    float PhasicContribution = 0.0f;
    if (State.VTANeurons.bIsPhasicBurst)
    {
        PhasicContribution = State.VTANeurons.PhasicBurstAmplitude * 0.3f;
    }
    else
    {
        PhasicContribution = -State.VTANeurons.PhasicDipAmplitude * 0.3f;
    }

    float PredictionComponent = State.RewardPrediction.PredictedValue * 0.2f;

    float GoNoGoBalance = (State.Receptors.GoSignal - State.Receptors.NoGoSignal) * 0.1f;

    State.MotivationalDrive = FMath::Clamp(
        TonicComponent + PhasicContribution + PredictionComponent + GoNoGoBalance + 0.5f,
        0.0f, 1.0f);
}

float UDopaminergicRewardSystem::CalculateDopamineProduction(float DeltaTime) const
{
    const FVTANeuronState& VTA = State.VTANeurons;

    // Tonic component: slow, steady release proportional to tonic firing rate
    float TonicProduction = VTA.TonicFiringRate * 0.08f * DeltaTime;

    // Phasic burst component
    float PhasicProduction = 0.0f;
    if (VTA.bIsPhasicBurst && VTA.PhasicBurstAmplitude > 0.0f)
    {
        PhasicProduction = VTA.PhasicBurstAmplitude * 0.5f * DeltaTime;
    }

    return TonicProduction + PhasicProduction;
}

float UDopaminergicRewardSystem::CalculateDopamineClearance(float DeltaTime) const
{
    const FDopamineTransporterState& DAT = State.Transporter;

    // Michaelis–Menten reuptake kinetics:
    // v = Vmax * [DA] / (Km + [DA])
    float DA = State.NAccDopamine;
    float ReuptakeVelocity = (DAT.MaxReuptakeRate * DA) / (DAT.KmReuptake + DA + KINDA_SMALL_NUMBER);
    float Reuptake = ReuptakeVelocity * DAT.TransporterActivity * DeltaTime;

    // MAO-mediated enzymatic degradation (first-order)
    float MAODegradation = DA * DAT.MAODegradationRate * DeltaTime;

    // Phasic dip reduces production (net effect: more clearance than production)
    float DipClearance = 0.0f;
    if (!State.VTANeurons.bIsPhasicBurst && State.VTANeurons.PhasicDipAmplitude > 0.0f)
    {
        DipClearance = State.VTANeurons.PhasicDipAmplitude * 0.3f * DeltaTime;
    }

    return Reuptake + MAODegradation + DipClearance;
}

void UDopaminergicRewardSystem::ApplyRPEToPhasicResponse(float RPE)
{
    FVTANeuronState& VTA = State.VTANeurons;
    float FatigueScale = 1.0f - VTA.PhasicFatigue;

    if (RPE > 0.0f)
    {
        // Positive RPE → phasic burst; proportional to RPE magnitude
        float BurstMagnitude = RPE * FatigueScale;

        if (BurstMagnitude > VTA.PhasicBurstAmplitude)
        {
            VTA.PhasicBurstAmplitude = BurstMagnitude;
            VTA.PhasicDipAmplitude = 0.0f;
            VTA.PhasicDuration = 0.5f + RPE * 1.5f; // Larger RPE → longer burst
            VTA.bIsPhasicBurst = true;
        }

        // Fatigue accumulates with each phasic activation
        VTA.PhasicFatigue += FMath::Abs(RPE) * 0.1f;
        VTA.PhasicFatigue = FMath::Clamp(VTA.PhasicFatigue, 0.0f, 1.0f);
    }
    else if (RPE < 0.0f)
    {
        // Negative RPE → phasic dip; proportional to RPE magnitude
        float DipMagnitude = FMath::Abs(RPE) * FatigueScale;

        if (DipMagnitude > VTA.PhasicDipAmplitude)
        {
            VTA.PhasicDipAmplitude = DipMagnitude;
            VTA.PhasicBurstAmplitude = 0.0f;
            VTA.PhasicDuration = 0.5f + DipMagnitude * 1.0f;
            VTA.bIsPhasicBurst = false;
        }

        VTA.PhasicFatigue += FMath::Abs(RPE) * 0.05f;
        VTA.PhasicFatigue = FMath::Clamp(VTA.PhasicFatigue, 0.0f, 1.0f);
    }
    // RPE ≈ 0 → no phasic response (correctly predicted reward)
}
