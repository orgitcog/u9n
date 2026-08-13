/**
 * @file DopaminergicRewardSystemTests.cpp
 * @brief Unit tests for the Dopaminergic Reward System (Feature F4.1.2)
 *
 * Tests cover:
 * - Initialization and defaults
 * - Reward Prediction Error (RPE) computation and TD learning
 * - Phasic burst on positive RPE
 * - Phasic dip on negative RPE / reward omission
 * - Unexpected reward processing
 * - Cued anticipatory dopamine
 * - Receptor (D1/D2) downregulation and recovery
 * - DAT Michaelis–Menten reuptake kinetics
 * - Hedonic habituation
 * - Motivational drive integration
 * - Boundary conditions (zero DeltaTime, clamped inputs)
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <functional>
#include <algorithm>
#include <limits>

namespace {

// ============================================================================
// Minimal Unreal Engine mock for standalone compilation
// ============================================================================
#ifndef WITH_UNREAL_ENGINE

#define UPROPERTY(...)
#define UFUNCTION(...)
#define UCLASS(...)
#define USTRUCT(...)
#define UENUM(...)
#define UMETA(...)
#define GENERATED_BODY()
#define BlueprintType
#define BlueprintReadWrite
#define BlueprintCallable
#define EditAnywhere
#define TEXT(x) x
#define UNREALENGINE_API
#define UE_LOG(...)

#define KINDA_SMALL_NUMBER 1e-4f

using int32 = int;

namespace FMath
{
    inline float Clamp(float Value, float Min, float Max) {
        return std::max(Min, std::min(Max, Value));
    }
    inline float Max(float A, float B) { return std::max(A, B); }
    inline float Min(float A, float B) { return std::min(A, B); }
    inline float Abs(float V) { return std::fabs(V); }
    inline float Lerp(float A, float B, float Alpha) { return A + Alpha * (B - A); }
}

#ifndef PI
#define PI 3.14159265358979323846f
#endif

template<typename T>
class TArray : public std::vector<T> {
public:
    int32 Num() const { return static_cast<int32>(this->size()); }
    void Add(const T& Item) { this->push_back(Item); }
    void Empty() { this->clear(); }
    void RemoveAll(std::function<bool(const T&)> Pred) {
        this->erase(
            std::remove_if(this->begin(), this->end(), Pred),
            this->end());
    }
};

class UObject {};

#endif // WITH_UNREAL_ENGINE

// ============================================================================
// Inline type definitions (mirror the production headers)
// ============================================================================

enum class ERewardEventType : unsigned char
{
    Primary,
    Secondary,
    Social,
    Achievement
};

struct FVTANeuronState
{
    float TonicFiringRate = 0.4f;
    float PhasicBurstAmplitude = 0.0f;
    float PhasicDipAmplitude = 0.0f;
    float PhasicDuration = 0.0f;
    bool  bIsPhasicBurst = true;
    float PhasicDecayRate = 0.8f;
    float PhasicFatigue = 0.0f;
};

struct FRewardPredictionState
{
    float PredictedValue = 0.5f;
    float ActualReward = 0.0f;
    float RPE = 0.0f;
    float LearningRate = 0.1f;
    float DiscountFactor = 0.9f;
    float RPERunningMean = 0.0f;
    float RPERunningVariance = 0.1f;
};

struct FDopamineReceptorState
{
    float D1Sensitivity = 1.0f;
    float D2Sensitivity = 1.0f;
    float DownregulationRate = 0.015f;
    float RecoveryRate = 0.008f;
    float DownregulationThreshold = 0.75f;
    float RecoveryThreshold = 0.35f;
    float GoSignal = 0.5f;
    float NoGoSignal = 0.5f;
};

struct FDopamineTransporterState
{
    float TransporterActivity = 1.0f;
    float MaxReuptakeRate = 0.4f;
    float KmReuptake = 0.3f;
    float MAODegradationRate = 0.05f;
};

struct FRewardEvent
{
    float Magnitude = 0.0f;
    float PredictedMagnitude = 0.0f;
    ERewardEventType Type = ERewardEventType::Primary;
    bool bActive = false;

    FRewardEvent() = default;
    FRewardEvent(float InMagnitude, float InPredicted, ERewardEventType InType)
        : Magnitude(InMagnitude), PredictedMagnitude(InPredicted),
          Type(InType), bActive(true) {}
};

struct FDopaminergicRewardState
{
    float NAccDopamine = 0.5f;
    float PFCDopamine  = 0.45f;
    float StriatalDopamine = 0.5f;
    FVTANeuronState VTANeurons;
    FRewardPredictionState RewardPrediction;
    FDopamineReceptorState Receptors;
    FDopamineTransporterState Transporter;
    float HedonicHabituation = 0.0f;
    float MotivationalDrive  = 0.5f;
};

// ============================================================================
// Testable mock implementation (mirrors UDopaminergicRewardSystem logic)
// ============================================================================

class MockDopaminergicRewardSystem
{
public:
    MockDopaminergicRewardSystem()
    {
        HabitationDissipationRate  = 0.003f;
        PhasicFatigueRecoveryRate  = 0.05f;
        NAccToPFCCoupling          = 0.6f;
        NAccToStriatalCoupling     = 0.8f;
        TonicSetPoint              = 0.4f;
        Initialize();
    }

    void Initialize()
    {
        State = FDopaminergicRewardState();
        PendingRewardEvents.Empty();
    }

    void Reset() { Initialize(); }

    void Update(float DeltaTime)
    {
        if (DeltaTime <= 0.0f) return;
        UpdateVTANeurons(DeltaTime);
        UpdateDopamineLevels(DeltaTime);
        UpdateReceptors(DeltaTime);
        UpdateHedonicHabituation(DeltaTime);
        UpdateMotivationalDrive();
        PendingRewardEvents.Empty();
    }

    void RegisterRewardOutcome(float ActualReward,
                               ERewardEventType RewardType = ERewardEventType::Primary)
    {
        float ClampedReward = FMath::Clamp(ActualReward, 0.0f, 1.0f);
        float RPE = ClampedReward - State.RewardPrediction.PredictedValue;

        float Alpha = 0.1f;
        State.RewardPrediction.RPERunningMean =
            (1.0f - Alpha) * State.RewardPrediction.RPERunningMean + Alpha * RPE;
        float Dev = RPE - State.RewardPrediction.RPERunningMean;
        State.RewardPrediction.RPERunningVariance =
            (1.0f - Alpha) * State.RewardPrediction.RPERunningVariance + Alpha * Dev * Dev;

        State.RewardPrediction.ActualReward = ClampedReward;
        State.RewardPrediction.RPE = RPE;

        State.RewardPrediction.PredictedValue +=
            State.RewardPrediction.LearningRate * RPE;
        State.RewardPrediction.PredictedValue =
            FMath::Clamp(State.RewardPrediction.PredictedValue, 0.0f, 1.0f);

        ApplyRPEToPhasicResponse(RPE);
        PendingRewardEvents.Add(
            FRewardEvent(ClampedReward, State.RewardPrediction.PredictedValue, RewardType));
    }

    void ProcessUnexpectedReward(float Magnitude,
                                 ERewardEventType RewardType = ERewardEventType::Primary)
    {
        float ClampedMagnitude = FMath::Clamp(Magnitude, 0.0f, 1.0f);
        float HabitationScale = 1.0f - State.HedonicHabituation * 0.6f;
        float EffectiveRPE = ClampedMagnitude * HabitationScale;
        ApplyRPEToPhasicResponse(EffectiveRPE);
        State.HedonicHabituation += ClampedMagnitude * 0.05f;
        State.HedonicHabituation = FMath::Clamp(State.HedonicHabituation, 0.0f, 1.0f);
        PendingRewardEvents.Add(FRewardEvent(ClampedMagnitude, 0.0f, RewardType));
    }

    void ProcessRewardOmission()
    {
        float PredictedValue = State.RewardPrediction.PredictedValue;
        float RPE = -PredictedValue;
        State.RewardPrediction.ActualReward = 0.0f;
        State.RewardPrediction.RPE = RPE;
        State.RewardPrediction.PredictedValue +=
            State.RewardPrediction.LearningRate * RPE;
        State.RewardPrediction.PredictedValue =
            FMath::Clamp(State.RewardPrediction.PredictedValue, 0.0f, 1.0f);
        ApplyRPEToPhasicResponse(RPE);
    }

    void TriggerCuedAnticipation(float CueValue)
    {
        float ClampedCue = FMath::Clamp(CueValue, 0.0f, 1.0f);
        float PreviousPrediction = State.RewardPrediction.PredictedValue;
        State.RewardPrediction.PredictedValue =
            FMath::Lerp(PreviousPrediction, ClampedCue,
                        State.RewardPrediction.LearningRate * 2.0f);

        float AnticipationBurst =
            ClampedCue * 0.4f * (1.0f - State.HedonicHabituation * 0.3f);
        float FatigueScale = 1.0f - State.VTANeurons.PhasicFatigue;
        AnticipationBurst *= FatigueScale;

        if (AnticipationBurst > State.VTANeurons.PhasicBurstAmplitude)
        {
            State.VTANeurons.PhasicBurstAmplitude = AnticipationBurst;
            State.VTANeurons.PhasicDuration = 1.5f;
            State.VTANeurons.bIsPhasicBurst = true;
        }
    }

    void SetValuePrediction(float PredictedValue)
    {
        State.RewardPrediction.PredictedValue =
            FMath::Clamp(PredictedValue, 0.0f, 1.0f);
    }

    void SetLearningRate(float LearningRate)
    {
        State.RewardPrediction.LearningRate =
            FMath::Clamp(LearningRate, 0.001f, 1.0f);
    }

    void SetHabitationDissipationRate(float Rate)
    {
        HabitationDissipationRate = FMath::Max(Rate, 0.0f);
    }

    float GetNAccDopamine()           const { return State.NAccDopamine; }
    float GetPFCDopamine()            const { return State.PFCDopamine; }
    float GetRewardPredictionError()  const { return State.RewardPrediction.RPE; }
    float GetTonicDopamine()          const { return State.VTANeurons.TonicFiringRate; }
    float GetPhasicDopamine()         const {
        return State.VTANeurons.bIsPhasicBurst
            ?  State.VTANeurons.PhasicBurstAmplitude
            : -State.VTANeurons.PhasicDipAmplitude;
    }
    float GetMotivationalDrive()      const { return State.MotivationalDrive; }
    float GetGoSignal()               const { return State.Receptors.GoSignal; }
    float GetNoGoSignal()             const { return State.Receptors.NoGoSignal; }
    float GetHedonicHabituation()     const { return State.HedonicHabituation; }
    int32 GetPendingRewardEventCount() const { return PendingRewardEvents.Num(); }

    FDopaminergicRewardState& GetMutableState() { return State; }
    FDopaminergicRewardState  GetState()  const { return State; }

private:
    FDopaminergicRewardState State;
    TArray<FRewardEvent>     PendingRewardEvents;
    float HabitationDissipationRate;
    float PhasicFatigueRecoveryRate;
    float NAccToPFCCoupling;
    float NAccToStriatalCoupling;
    float TonicSetPoint;

    void UpdateVTANeurons(float DeltaTime)
    {
        FVTANeuronState& VTA = State.VTANeurons;
        VTA.PhasicFatigue -= PhasicFatigueRecoveryRate * DeltaTime;
        VTA.PhasicFatigue = FMath::Max(VTA.PhasicFatigue, 0.0f);

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

        float TonicTarget = TonicSetPoint * (1.0f - State.HedonicHabituation * 0.4f);
        VTA.TonicFiringRate =
            FMath::Lerp(VTA.TonicFiringRate, TonicTarget, 0.02f * DeltaTime);
        VTA.TonicFiringRate = FMath::Clamp(VTA.TonicFiringRate, 0.1f, 1.0f);
    }

    void UpdateDopamineLevels(float DeltaTime)
    {
        float Production = CalculateDopamineProduction(DeltaTime);
        float Clearance  = CalculateDopamineClearance(DeltaTime);
        State.NAccDopamine += Production - Clearance;
        State.NAccDopamine = FMath::Clamp(State.NAccDopamine, 0.0f, 1.0f);

        float PFCTarget = State.NAccDopamine * NAccToPFCCoupling;
        State.PFCDopamine =
            FMath::Lerp(State.PFCDopamine, PFCTarget, 0.15f * DeltaTime);
        State.PFCDopamine = FMath::Clamp(State.PFCDopamine, 0.0f, 1.0f);

        float StriatalTarget = State.NAccDopamine * NAccToStriatalCoupling;
        State.StriatalDopamine =
            FMath::Lerp(State.StriatalDopamine, StriatalTarget, 0.2f * DeltaTime);
        State.StriatalDopamine = FMath::Clamp(State.StriatalDopamine, 0.0f, 1.0f);
    }

    void UpdateReceptors(float DeltaTime)
    {
        FDopamineReceptorState& R = State.Receptors;
        float DA = State.NAccDopamine;
        if (DA > R.DownregulationThreshold)
        {
            float Excess = DA - R.DownregulationThreshold;
            R.D1Sensitivity -= Excess * R.DownregulationRate * DeltaTime;
            R.D2Sensitivity -= Excess * R.DownregulationRate * DeltaTime;
        }
        else if (DA < R.RecoveryThreshold)
        {
            float Deficit = R.RecoveryThreshold - DA;
            R.D1Sensitivity += Deficit * R.RecoveryRate * DeltaTime;
            R.D2Sensitivity += Deficit * R.RecoveryRate * DeltaTime;
        }
        R.D1Sensitivity = FMath::Clamp(R.D1Sensitivity, 0.1f, 1.5f);
        R.D2Sensitivity = FMath::Clamp(R.D2Sensitivity, 0.1f, 1.5f);
        R.GoSignal   = FMath::Clamp(DA * R.D1Sensitivity, 0.0f, 1.0f);
        float D2Act  = FMath::Max(0.0f, 1.0f - DA) * R.D2Sensitivity;
        R.NoGoSignal = FMath::Clamp(D2Act, 0.0f, 1.0f);
    }

    void UpdateHedonicHabituation(float DeltaTime)
    {
        if (State.NAccDopamine < 0.5f)
        {
            float Recovery = (0.5f - State.NAccDopamine) *
                             HabitationDissipationRate * DeltaTime;
            State.HedonicHabituation -= Recovery;
            State.HedonicHabituation = FMath::Max(State.HedonicHabituation, 0.0f);
        }
    }

    void UpdateMotivationalDrive()
    {
        float TonicComponent = State.VTANeurons.TonicFiringRate * 0.4f;
        float PhasicContribution = 0.0f;
        if (State.VTANeurons.bIsPhasicBurst)
            PhasicContribution =  State.VTANeurons.PhasicBurstAmplitude * 0.3f;
        else
            PhasicContribution = -State.VTANeurons.PhasicDipAmplitude * 0.3f;
        float PredictionComponent = State.RewardPrediction.PredictedValue * 0.2f;
        float GoNoGoBalance =
            (State.Receptors.GoSignal - State.Receptors.NoGoSignal) * 0.1f;
        State.MotivationalDrive = FMath::Clamp(
            TonicComponent + PhasicContribution + PredictionComponent +
                GoNoGoBalance + 0.5f,
            0.0f, 1.0f);
    }

    float CalculateDopamineProduction(float DeltaTime) const
    {
        const FVTANeuronState& VTA = State.VTANeurons;
        float Tonic  = VTA.TonicFiringRate * 0.08f * DeltaTime;
        float Phasic = 0.0f;
        if (VTA.bIsPhasicBurst && VTA.PhasicBurstAmplitude > 0.0f)
            Phasic = VTA.PhasicBurstAmplitude * 0.5f * DeltaTime;
        return Tonic + Phasic;
    }

    float CalculateDopamineClearance(float DeltaTime) const
    {
        const FDopamineTransporterState& DAT = State.Transporter;
        float DA = State.NAccDopamine;
        float Reuptake = (DAT.MaxReuptakeRate * DA) /
                         (DAT.KmReuptake + DA + KINDA_SMALL_NUMBER);
        Reuptake *= DAT.TransporterActivity * DeltaTime;
        float MAO = DA * DAT.MAODegradationRate * DeltaTime;
        float Dip = 0.0f;
        if (!State.VTANeurons.bIsPhasicBurst &&
            State.VTANeurons.PhasicDipAmplitude > 0.0f)
            Dip = State.VTANeurons.PhasicDipAmplitude * 0.3f * DeltaTime;
        return Reuptake + MAO + Dip;
    }

    void ApplyRPEToPhasicResponse(float RPE)
    {
        FVTANeuronState& VTA = State.VTANeurons;
        float FatigueScale = 1.0f - VTA.PhasicFatigue;
        if (RPE > 0.0f)
        {
            float BurstMagnitude = RPE * FatigueScale;
            if (BurstMagnitude > VTA.PhasicBurstAmplitude)
            {
                VTA.PhasicBurstAmplitude = BurstMagnitude;
                VTA.PhasicDipAmplitude   = 0.0f;
                VTA.PhasicDuration       = 0.5f + RPE * 1.5f;
                VTA.bIsPhasicBurst       = true;
            }
            VTA.PhasicFatigue += FMath::Abs(RPE) * 0.1f;
            VTA.PhasicFatigue = FMath::Clamp(VTA.PhasicFatigue, 0.0f, 1.0f);
        }
        else if (RPE < 0.0f)
        {
            float DipMagnitude = FMath::Abs(RPE) * FatigueScale;
            if (DipMagnitude > VTA.PhasicDipAmplitude)
            {
                VTA.PhasicDipAmplitude   = DipMagnitude;
                VTA.PhasicBurstAmplitude = 0.0f;
                VTA.PhasicDuration       = 0.5f + DipMagnitude * 1.0f;
                VTA.bIsPhasicBurst       = false;
            }
            VTA.PhasicFatigue += FMath::Abs(RPE) * 0.05f;
            VTA.PhasicFatigue = FMath::Clamp(VTA.PhasicFatigue, 0.0f, 1.0f);
        }
    }
};

// ============================================================================
// Helper
// ============================================================================

static void SimulateSeconds(MockDopaminergicRewardSystem& Sys, float TotalTime,
                             float StepSize = 0.05f)
{
    for (float T = 0.0f; T < TotalTime; T += StepSize)
    {
        Sys.Update(StepSize);
    }
}

// ============================================================================
// Test fixture
// ============================================================================

class DopaminergicRewardSystemTest : public ::testing::Test
{
protected:
    MockDopaminergicRewardSystem* System = nullptr;

    void SetUp() override
    {
        System = new MockDopaminergicRewardSystem();
    }

    void TearDown() override
    {
        delete System;
        System = nullptr;
    }
};

// ============================================================================
// Initialization Tests
// ============================================================================

TEST_F(DopaminergicRewardSystemTest, DefaultInitializationIsValid)
{
    FDopaminergicRewardState State = System->GetState();
    EXPECT_GT(State.NAccDopamine, 0.0f);
    EXPECT_LE(State.NAccDopamine, 1.0f);
    EXPECT_GT(State.PFCDopamine, 0.0f);
    EXPECT_LE(State.PFCDopamine, 1.0f);
    EXPECT_GE(State.HedonicHabituation, 0.0f);
    EXPECT_EQ(State.VTANeurons.PhasicBurstAmplitude, 0.0f);
    EXPECT_EQ(State.VTANeurons.PhasicDipAmplitude, 0.0f);
}

TEST_F(DopaminergicRewardSystemTest, ResetRestoresDefaults)
{
    System->ProcessUnexpectedReward(1.0f);
    SimulateSeconds(*System, 2.0f);
    System->Reset();

    FDopaminergicRewardState State = System->GetState();
    EXPECT_NEAR(State.NAccDopamine, 0.5f, 0.01f);
    EXPECT_NEAR(State.HedonicHabituation, 0.0f, 0.01f);
    EXPECT_EQ(State.VTANeurons.PhasicBurstAmplitude, 0.0f);
}

// ============================================================================
// RPE and TD Learning Tests
// ============================================================================

TEST_F(DopaminergicRewardSystemTest, PositiveRPEProducesPhasicBurst)
{
    // Prediction at default 0.5; give reward 0.9 → RPE = +0.4
    System->SetValuePrediction(0.5f);
    System->RegisterRewardOutcome(0.9f);

    float RPE = System->GetRewardPredictionError();
    EXPECT_NEAR(RPE, 0.4f, 0.001f);
    EXPECT_GT(System->GetPhasicDopamine(), 0.0f);
}

TEST_F(DopaminergicRewardSystemTest, NegativeRPEProducesPhasicDip)
{
    // Prediction at 0.8; give reward 0.1 → RPE = -0.7
    System->SetValuePrediction(0.8f);
    System->RegisterRewardOutcome(0.1f);

    float RPE = System->GetRewardPredictionError();
    EXPECT_NEAR(RPE, -0.7f, 0.001f);
    EXPECT_LT(System->GetPhasicDopamine(), 0.0f);
}

TEST_F(DopaminergicRewardSystemTest, ZeroRPENoSignificantPhasicResponse)
{
    // Prediction matches actual: RPE = 0
    System->SetValuePrediction(0.6f);
    System->RegisterRewardOutcome(0.6f);

    EXPECT_NEAR(System->GetRewardPredictionError(), 0.0f, 0.001f);
    // Phasic amplitude should not rise from zero RPE
    EXPECT_NEAR(System->GetPhasicDopamine(), 0.0f, 0.001f);
}

TEST_F(DopaminergicRewardSystemTest, TDLearningUpdatesValuePrediction)
{
    float InitialPrediction = System->GetState().RewardPrediction.PredictedValue;
    float LearningRate = System->GetState().RewardPrediction.LearningRate;

    System->RegisterRewardOutcome(1.0f); // RPE = 1.0 - InitialPrediction

    float Expected = InitialPrediction + LearningRate * (1.0f - InitialPrediction);
    EXPECT_NEAR(System->GetState().RewardPrediction.PredictedValue, Expected, 0.001f);
}

TEST_F(DopaminergicRewardSystemTest, RepeatedHighRewardsConvergePredictionUpward)
{
    System->SetValuePrediction(0.0f);
    for (int i = 0; i < 50; i++)
    {
        System->RegisterRewardOutcome(1.0f);
    }
    EXPECT_GT(System->GetState().RewardPrediction.PredictedValue, 0.8f);
}

TEST_F(DopaminergicRewardSystemTest, RepeatedLowRewardsConvergePredictionDownward)
{
    System->SetValuePrediction(1.0f);
    for (int i = 0; i < 50; i++)
    {
        System->RegisterRewardOutcome(0.0f);
    }
    EXPECT_LT(System->GetState().RewardPrediction.PredictedValue, 0.2f);
}

// ============================================================================
// Unexpected Reward Tests
// ============================================================================

TEST_F(DopaminergicRewardSystemTest, UnexpectedRewardRaisesPhasicBurst)
{
    System->ProcessUnexpectedReward(0.8f);

    EXPECT_GT(System->GetPhasicDopamine(), 0.0f);
    EXPECT_EQ(System->GetPendingRewardEventCount(), 1);
}

TEST_F(DopaminergicRewardSystemTest, UnexpectedRewardAccumulatesHabituation)
{
    float HabBefore = System->GetHedonicHabituation();
    System->ProcessUnexpectedReward(1.0f);
    EXPECT_GT(System->GetHedonicHabituation(), HabBefore);
}

TEST_F(DopaminergicRewardSystemTest, HabitationReducesBurstMagnitude)
{
    // First reward (no habituation)
    System->Reset();
    System->ProcessUnexpectedReward(1.0f);
    float FirstBurst = System->GetPhasicDopamine();

    // Force high habituation
    System->Reset();
    System->GetMutableState().HedonicHabituation = 0.9f;
    System->ProcessUnexpectedReward(1.0f);
    float HabituatedBurst = System->GetPhasicDopamine();

    EXPECT_GT(FirstBurst, HabituatedBurst);
}

// ============================================================================
// Reward Omission Tests
// ============================================================================

TEST_F(DopaminergicRewardSystemTest, OmissionProducesPhasicDip)
{
    System->SetValuePrediction(0.8f);
    System->ProcessRewardOmission();

    EXPECT_LT(System->GetPhasicDopamine(), 0.0f);
    EXPECT_LT(System->GetRewardPredictionError(), 0.0f);
}

TEST_F(DopaminergicRewardSystemTest, OmissionDecreasesValuePrediction)
{
    System->SetValuePrediction(0.8f);
    float Before = System->GetState().RewardPrediction.PredictedValue;
    System->ProcessRewardOmission();
    float After = System->GetState().RewardPrediction.PredictedValue;

    EXPECT_LT(After, Before);
}

// ============================================================================
// Cued Anticipation Tests
// ============================================================================

TEST_F(DopaminergicRewardSystemTest, CuedAnticipationRaisesPhasicBurst)
{
    System->TriggerCuedAnticipation(0.8f);
    EXPECT_GT(System->GetPhasicDopamine(), 0.0f);
}

TEST_F(DopaminergicRewardSystemTest, CuedAnticipationUpdatesValuePrediction)
{
    float Before = System->GetState().RewardPrediction.PredictedValue;
    System->TriggerCuedAnticipation(1.0f);
    float After = System->GetState().RewardPrediction.PredictedValue;
    EXPECT_NE(Before, After);
}

TEST_F(DopaminergicRewardSystemTest, CuedAnticipationLessThanConsummatory)
{
    // Anticipatory burst should be less than consummatory burst for same value
    System->Reset();
    System->TriggerCuedAnticipation(1.0f);
    float AnticipatoryBurst = System->GetPhasicDopamine();

    System->Reset();
    System->SetValuePrediction(0.0f);
    System->RegisterRewardOutcome(1.0f); // RPE = 1.0 → full burst
    float ConsummatoryBurst = System->GetPhasicDopamine();

    EXPECT_LT(AnticipatoryBurst, ConsummatoryBurst);
}

// ============================================================================
// Receptor Dynamics Tests
// ============================================================================

TEST_F(DopaminergicRewardSystemTest, HighDopamineCausesReceptorDownregulation)
{
    // Force high NAcc dopamine above downregulation threshold (0.75)
    System->GetMutableState().NAccDopamine = 0.9f;
    float D1Before = System->GetState().Receptors.D1Sensitivity;
    float D2Before = System->GetState().Receptors.D2Sensitivity;

    // Use short simulation time (0.3s) - DA stays above threshold before clearance
    // brings it below, avoiding the subsequent recovery phase
    SimulateSeconds(*System, 0.3f);

    EXPECT_LT(System->GetState().Receptors.D1Sensitivity, D1Before);
    EXPECT_LT(System->GetState().Receptors.D2Sensitivity, D2Before);
}

TEST_F(DopaminergicRewardSystemTest, LowDopamineAllowsReceptorRecovery)
{
    // Start with low receptor sensitivity from prior downregulation
    System->GetMutableState().Receptors.D1Sensitivity = 0.3f;
    System->GetMutableState().Receptors.D2Sensitivity = 0.3f;
    // Low dopamine state
    System->GetMutableState().NAccDopamine = 0.1f;

    SimulateSeconds(*System, 10.0f);

    EXPECT_GT(System->GetState().Receptors.D1Sensitivity, 0.3f);
}

TEST_F(DopaminergicRewardSystemTest, HighDopamineElevatesGoSignal)
{
    System->GetMutableState().NAccDopamine = 0.9f;
    System->Update(0.1f);

    // D1 (Go) activated by high dopamine
    EXPECT_GT(System->GetGoSignal(), 0.5f);
}

TEST_F(DopaminergicRewardSystemTest, HighDopamineSuppressesNoGoSignal)
{
    System->GetMutableState().NAccDopamine = 0.9f;
    System->Update(0.1f);

    // D2 (NoGo) inhibited by high dopamine
    EXPECT_LT(System->GetNoGoSignal(), 0.5f);
}

// ============================================================================
// Phasic Dynamics Tests
// ============================================================================

TEST_F(DopaminergicRewardSystemTest, PhasicBurstDecaysOverTime)
{
    System->ProcessUnexpectedReward(1.0f);
    float InitialBurst = System->GetPhasicDopamine();
    EXPECT_GT(InitialBurst, 0.0f);

    SimulateSeconds(*System, 3.0f);
    float FinalBurst = System->GetPhasicDopamine();
    EXPECT_LT(FinalBurst, InitialBurst);
}

TEST_F(DopaminergicRewardSystemTest, PhasicFatigueReducesRepeatedBursts)
{
    System->GetMutableState().VTANeurons.PhasicFatigue = 0.0f;
    System->SetValuePrediction(0.0f);
    System->RegisterRewardOutcome(1.0f);
    float FirstBurst = System->GetPhasicDopamine();

    // Reset burst amplitude so the second burst can be compared cleanly,
    // then accumulate fatigue and trigger another burst
    System->GetMutableState().VTANeurons.PhasicBurstAmplitude = 0.0f;
    System->GetMutableState().VTANeurons.PhasicFatigue = 0.8f;
    System->SetValuePrediction(0.0f);
    System->RegisterRewardOutcome(1.0f);
    float FatiguedBurst = System->GetPhasicDopamine();

    EXPECT_LT(FatiguedBurst, FirstBurst);
}

TEST_F(DopaminergicRewardSystemTest, PhasicFatigueRecovery)
{
    // Force fatigue
    System->GetMutableState().VTANeurons.PhasicFatigue = 1.0f;
    SimulateSeconds(*System, 20.0f);
    EXPECT_LT(System->GetState().VTANeurons.PhasicFatigue, 1.0f);
}

// ============================================================================
// Motivational Drive Tests
// ============================================================================

TEST_F(DopaminergicRewardSystemTest, MotivationalDriveIsInRange)
{
    SimulateSeconds(*System, 5.0f);
    float Drive = System->GetMotivationalDrive();
    EXPECT_GE(Drive, 0.0f);
    EXPECT_LE(Drive, 1.0f);
}

TEST_F(DopaminergicRewardSystemTest, PhasicBurstIncreasesMotivationalDrive)
{
    System->Update(0.1f);
    float DriveBefore = System->GetMotivationalDrive();

    System->SetValuePrediction(0.0f);
    System->RegisterRewardOutcome(1.0f); // Large positive RPE triggers phasic burst
    System->Update(0.001f); // Advance by minimal time to apply motivational update

    float DriveAfter = System->GetMotivationalDrive();
    EXPECT_GT(DriveAfter, DriveBefore);
}

// ============================================================================
// Hedonic Habituation Dissipation Tests
// ============================================================================

TEST_F(DopaminergicRewardSystemTest, HabitationDissipatesDuringLowRewardPeriod)
{
    System->GetMutableState().HedonicHabituation = 0.9f;
    System->GetMutableState().NAccDopamine = 0.1f; // Low dopamine state

    float HabBefore = System->GetHedonicHabituation();
    SimulateSeconds(*System, 30.0f);
    EXPECT_LT(System->GetHedonicHabituation(), HabBefore);
}

// ============================================================================
// Boundary Condition Tests
// ============================================================================

TEST_F(DopaminergicRewardSystemTest, ZeroDeltaTimeIsNoOp)
{
    FDopaminergicRewardState Before = System->GetState();
    System->Update(0.0f);
    FDopaminergicRewardState After = System->GetState();

    EXPECT_FLOAT_EQ(Before.NAccDopamine, After.NAccDopamine);
    EXPECT_FLOAT_EQ(Before.HedonicHabituation, After.HedonicHabituation);
}

TEST_F(DopaminergicRewardSystemTest, NegativeDeltaTimeIsNoOp)
{
    FDopaminergicRewardState Before = System->GetState();
    System->Update(-1.0f);
    FDopaminergicRewardState After = System->GetState();

    EXPECT_FLOAT_EQ(Before.NAccDopamine, After.NAccDopamine);
}

TEST_F(DopaminergicRewardSystemTest, DopamineStaysInRange)
{
    // Flood with maximum phasic stimulation
    for (int i = 0; i < 20; i++)
    {
        System->SetValuePrediction(0.0f);
        System->RegisterRewardOutcome(1.0f);
        System->Update(0.05f);
    }
    EXPECT_LE(System->GetNAccDopamine(), 1.0f);
    EXPECT_GE(System->GetNAccDopamine(), 0.0f);
}

TEST_F(DopaminergicRewardSystemTest, RewardOutcomeClampedToValidRange)
{
    System->RegisterRewardOutcome(5.0f); // Over-limit
    EXPECT_LE(System->GetState().RewardPrediction.ActualReward, 1.0f);

    System->RegisterRewardOutcome(-3.0f); // Under-limit
    EXPECT_GE(System->GetState().RewardPrediction.ActualReward, 0.0f);
}

TEST_F(DopaminergicRewardSystemTest, SetLearningRateClamped)
{
    System->SetLearningRate(10.0f);
    EXPECT_LE(System->GetState().RewardPrediction.LearningRate, 1.0f);

    System->SetLearningRate(-1.0f);
    EXPECT_GE(System->GetState().RewardPrediction.LearningRate, 0.001f);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(DopaminergicRewardSystemTest, PredictedRewardElicitsNoPhasicOnDelivery)
{
    // After full convergence, delivering predicted reward should not cause burst
    System->SetValuePrediction(0.0f);
    for (int i = 0; i < 40; i++)
    {
        System->RegisterRewardOutcome(0.7f);
    }
    // Now prediction ≈ 0.7; delivering 0.7 again
    System->RegisterRewardOutcome(0.7f);
    EXPECT_NEAR(System->GetRewardPredictionError(), 0.0f, 0.05f);
}

TEST_F(DopaminergicRewardSystemTest, ExtendedSimulationDoesNotDiverge)
{
    for (int i = 0; i < 20; i++)
    {
        System->ProcessUnexpectedReward(0.5f);
        SimulateSeconds(*System, 5.0f);
    }
    EXPECT_LE(System->GetNAccDopamine(), 1.0f);
    EXPECT_GE(System->GetNAccDopamine(), 0.0f);
    EXPECT_LE(System->GetHedonicHabituation(), 1.0f);
}



} // namespace
