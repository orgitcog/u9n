/**
 * @file CortisolDynamicsTests.cpp
 * @brief Unit tests for the Cortisol Dynamics System (Feature F4.1.1)
 *
 * Tests cover:
 * - Initialization and defaults
 * - Circadian rhythm modeling
 * - Acute stress response dynamics
 * - HPA axis cascade behavior
 * - Chronic stress accumulation
 * - Receptor sensitivity adaptation
 * - Cortisol clearance and homeostasis return
 * - Multiple concurrent stressors
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

// Mock Unreal Engine types for standalone testing
#ifndef WITH_UNREAL_ENGINE
#include <algorithm>
#include <functional>

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

using int32 = int;

namespace FMath
{
    inline float Clamp(float Value, float Min, float Max) {
        return std::max(Min, std::min(Max, Value));
    }
    inline float Max(float A, float B) { return std::max(A, B); }
    inline float Fmod(float A, float B) { return std::fmod(A, B); }
    inline float Cos(float Value) { return std::cos(Value); }
    inline float Lerp(float A, float B, float Alpha) { return A + Alpha * (B - A); }
}

#ifndef PI
#define PI 3.14159265358979323846f
#endif

// Minimal TArray mock
template<typename T>
class TArray : public std::vector<T> {
public:
    int32 Num() const { return static_cast<int32>(this->size()); }
    void Add(const T& Item) { this->push_back(Item); }
    void Empty() { this->clear(); }
    void RemoveAll(std::function<bool(const T&)> Pred) {
        this->erase(std::remove_if(this->begin(), this->end(), Pred), this->end());
    }
};

class UObject {};

#endif // WITH_UNREAL_ENGINE

// ============================================================================
// Include the cortisol dynamics types and re-implement for testing
// ============================================================================

enum class EStressEventType : unsigned char
{
    Acute,
    Chronic,
    Traumatic
};

struct FHPAAxisState
{
    float CRHLevel = 0.1f;
    float ACTHLevel = 0.1f;
    float CortisolOutput = 0.0f;
    float AxisActivation = 0.0f;
    float NegativeFeedbackGain = 1.0f;
};

struct FCircadianRhythm
{
    float TimeOfDay = 8.0f;
    float PeakHour = 8.0f;
    float Amplitude = 0.15f;
    float Baseline = 0.25f;
    float CurrentContribution = 0.0f;
};

struct FStressEvent
{
    float Intensity = 0.0f;
    float Duration = 0.0f;
    float ElapsedTime = 0.0f;
    EStressEventType Type = EStressEventType::Acute;
    bool bActive = false;

    FStressEvent() = default;
    FStressEvent(float InIntensity, float InDuration, EStressEventType InType)
        : Intensity(InIntensity), Duration(InDuration), ElapsedTime(0.0f),
          Type(InType), bActive(true) {}
};

struct FCortisolReceptorState
{
    float Sensitivity = 1.0f;
    float DownregulationRate = 0.02f;
    float RecoveryRate = 0.01f;
    float DownregulationThreshold = 0.7f;
    float RecoveryThreshold = 0.4f;
};

struct FCortisolDynamicsState
{
    float TotalCortisol = 0.3f;
    float FreeCortisol = 0.015f;
    float BoundCortisol = 0.285f;
    FHPAAxisState HPAAxis;
    FCircadianRhythm Circadian;
    FCortisolReceptorState Receptors;
    float ChronicStressLoad = 0.0f;
    bool bChronicStressActive = false;
    float ChronicStressorIntensity = 0.0f;
};

/**
 * Mock implementation of CortisolDynamicsSystem for standalone testing
 * Mirrors the UE implementation logic without UE dependencies
 */
class MockCortisolDynamicsSystem
{
public:
    MockCortisolDynamicsSystem()
        : TimeScale(1.0f)
        , ClearanceHalfLife(30.0f)
        , CRHToACTHGain(0.8f)
        , ACTHToCortisolGain(0.6f)
        , CRHDecayRate(0.15f)
        , ACTHDecayRate(0.1f)
        , ChronicStressAccumulationRate(0.005f)
        , ChronicStressDissipationRate(0.002f)
        , NormalFreeFraction(0.05f)
    {
        Initialize();
    }

    void Initialize()
    {
        State = FCortisolDynamicsState();
        ActiveStressEvents.Empty();
    }

    void Update(float DeltaTime)
    {
        if (DeltaTime <= 0.0f) return;

        UpdateCircadianRhythm(DeltaTime);
        UpdateStressEvents(DeltaTime);
        UpdateChronicStress(DeltaTime);
        UpdateHPAAxis(DeltaTime);
        UpdateReceptorSensitivity(DeltaTime);

        float Production = CalculateCortisolProduction(DeltaTime);
        float Clearance = CalculateCortisolClearance(DeltaTime);

        State.TotalCortisol += Production - Clearance;
        State.TotalCortisol = FMath::Clamp(State.TotalCortisol, 0.0f, 1.0f);

        UpdateFreeAndBoundCortisol();
    }

    void Reset() { Initialize(); }

    void ApplyStressor(float Intensity, float Duration)
    {
        float ClampedIntensity = FMath::Clamp(Intensity, 0.0f, 1.0f);
        float ClampedDuration = FMath::Max(Duration, 0.1f);
        FStressEvent Event(ClampedIntensity, ClampedDuration, EStressEventType::Acute);
        ActiveStressEvents.Add(Event);
        State.HPAAxis.CRHLevel += ClampedIntensity * 0.5f;
        State.HPAAxis.CRHLevel = FMath::Clamp(State.HPAAxis.CRHLevel, 0.0f, 1.0f);
    }

    void ApplyChronicStressor(float Intensity)
    {
        State.bChronicStressActive = true;
        State.ChronicStressorIntensity = FMath::Clamp(Intensity, 0.0f, 1.0f);
    }

    void RemoveChronicStressor()
    {
        State.bChronicStressActive = false;
        State.ChronicStressorIntensity = 0.0f;
    }

    float GetCortisolLevel() const { return State.TotalCortisol; }
    float GetFreeCortisolLevel() const { return State.FreeCortisol; }
    float GetStressLoad() const { return State.ChronicStressLoad; }
    FCortisolDynamicsState GetState() const { return State; }
    FHPAAxisState GetHPAAxisState() const { return State.HPAAxis; }
    FCortisolReceptorState GetReceptorState() const { return State.Receptors; }

    void SetTimeOfDay(float Hours)
    {
        State.Circadian.TimeOfDay = std::fmod(Hours, 24.0f);
        if (State.Circadian.TimeOfDay < 0.0f) State.Circadian.TimeOfDay += 24.0f;
    }

    void SetTimeScale(float Scale) { TimeScale = FMath::Max(Scale, 0.0f); }

    int32 GetActiveStressEventCount() const
    {
        int32 Count = 0;
        for (size_t i = 0; i < ActiveStressEvents.size(); i++)
        {
            if (ActiveStressEvents[i].bActive) Count++;
        }
        return Count;
    }

    // Expose state for testing
    FCortisolDynamicsState& GetMutableState() { return State; }

private:
    void UpdateHPAAxis(float DeltaTime)
    {
        FHPAAxisState& HPA = State.HPAAxis;
        float FeedbackStrength = HPA.NegativeFeedbackGain * State.Receptors.Sensitivity;
        float FeedbackSuppression = State.TotalCortisol * FeedbackStrength * 0.5f;
        HPA.NegativeFeedbackGain = FMath::Lerp(1.0f, 0.4f, State.ChronicStressLoad);

        float CRHDrive = 0.0f;
        if (State.bChronicStressActive)
        {
            CRHDrive += State.ChronicStressorIntensity * 0.2f;
        }
        for (size_t i = 0; i < ActiveStressEvents.size(); i++)
        {
            const FStressEvent& Event = ActiveStressEvents[i];
            if (Event.bActive)
            {
                float RemainingFraction = 1.0f - (Event.ElapsedTime / FMath::Max(Event.Duration, 0.1f));
                CRHDrive += Event.Intensity * RemainingFraction * 0.3f;
            }
        }

        HPA.CRHLevel += (CRHDrive - FeedbackSuppression - HPA.CRHLevel * CRHDecayRate) * DeltaTime;
        HPA.CRHLevel = FMath::Clamp(HPA.CRHLevel, 0.0f, 1.0f);

        float ACTHDrive = HPA.CRHLevel * CRHToACTHGain;
        HPA.ACTHLevel += (ACTHDrive - HPA.ACTHLevel * ACTHDecayRate) * DeltaTime;
        HPA.ACTHLevel = FMath::Clamp(HPA.ACTHLevel, 0.0f, 1.0f);

        HPA.CortisolOutput = HPA.ACTHLevel * ACTHToCortisolGain;
        HPA.AxisActivation = (HPA.CRHLevel + HPA.ACTHLevel + HPA.CortisolOutput) / 3.0f;
    }

    void UpdateCircadianRhythm(float DeltaTime)
    {
        FCircadianRhythm& C = State.Circadian;
        C.TimeOfDay += DeltaTime * TimeScale / 60.0f;
        if (C.TimeOfDay >= 24.0f) C.TimeOfDay -= 24.0f;

        float PhaseRadians = 2.0f * static_cast<float>(PI) * (C.TimeOfDay - C.PeakHour) / 24.0f;
        C.CurrentContribution = C.Baseline + C.Amplitude * std::cos(PhaseRadians);
        C.CurrentContribution = FMath::Max(C.CurrentContribution, 0.0f);
    }

    void UpdateStressEvents(float DeltaTime)
    {
        for (size_t i = 0; i < ActiveStressEvents.size(); i++)
        {
            FStressEvent& Event = ActiveStressEvents[i];
            if (!Event.bActive) continue;
            Event.ElapsedTime += DeltaTime;
            if (Event.Type == EStressEventType::Acute && Event.ElapsedTime >= Event.Duration)
            {
                Event.bActive = false;
            }
        }
        ActiveStressEvents.RemoveAll([](const FStressEvent& E) { return !E.bActive; });
    }

    void UpdateReceptorSensitivity(float DeltaTime)
    {
        FCortisolReceptorState& R = State.Receptors;
        if (State.TotalCortisol > R.DownregulationThreshold)
        {
            float Excess = State.TotalCortisol - R.DownregulationThreshold;
            R.Sensitivity -= Excess * R.DownregulationRate * DeltaTime;
        }
        else if (State.TotalCortisol < R.RecoveryThreshold)
        {
            float Deficit = R.RecoveryThreshold - State.TotalCortisol;
            R.Sensitivity += Deficit * R.RecoveryRate * DeltaTime;
        }
        R.Sensitivity = FMath::Clamp(R.Sensitivity, 0.1f, 1.0f);
    }

    void UpdateChronicStress(float DeltaTime)
    {
        if (State.bChronicStressActive)
        {
            State.ChronicStressLoad += State.ChronicStressorIntensity * ChronicStressAccumulationRate * DeltaTime;
        }
        else
        {
            State.ChronicStressLoad -= ChronicStressDissipationRate * DeltaTime;
        }
        State.ChronicStressLoad = FMath::Clamp(State.ChronicStressLoad, 0.0f, 1.0f);
    }

    float CalculateCortisolProduction(float DeltaTime)
    {
        float HPA = State.HPAAxis.CortisolOutput * DeltaTime;
        float Circadian = State.Circadian.CurrentContribution * 0.1f * DeltaTime;
        float Chronic = State.ChronicStressLoad * 0.05f * DeltaTime;
        return HPA + Circadian + Chronic;
    }

    float CalculateCortisolClearance(float DeltaTime)
    {
        float DecayConstant = 0.693f / FMath::Max(ClearanceHalfLife, 0.1f);
        float EffectiveDecay = DecayConstant * State.Receptors.Sensitivity;
        return State.TotalCortisol * EffectiveDecay * DeltaTime;
    }

    void UpdateFreeAndBoundCortisol()
    {
        float FreeFraction = NormalFreeFraction;
        if (State.TotalCortisol > 0.6f)
        {
            float Excess = State.TotalCortisol - 0.6f;
            FreeFraction += Excess * 0.5f;
        }
        FreeFraction = FMath::Clamp(FreeFraction, NormalFreeFraction, 0.5f);
        State.FreeCortisol = State.TotalCortisol * FreeFraction;
        State.BoundCortisol = State.TotalCortisol - State.FreeCortisol;
    }

    FCortisolDynamicsState State;
    TArray<FStressEvent> ActiveStressEvents;
    float TimeScale;
    float ClearanceHalfLife;
    float CRHToACTHGain;
    float ACTHToCortisolGain;
    float CRHDecayRate;
    float ACTHDecayRate;
    float ChronicStressAccumulationRate;
    float ChronicStressDissipationRate;
    float NormalFreeFraction;
};

// ============================================================================
// Helper: simulate multiple time steps
// ============================================================================

static void SimulateSeconds(MockCortisolDynamicsSystem& System, float TotalSeconds, float StepSize = 0.1f)
{
    float Elapsed = 0.0f;
    while (Elapsed < TotalSeconds)
    {
        float Step = std::min(StepSize, TotalSeconds - Elapsed);
        System.Update(Step);
        Elapsed += Step;
    }
}

// ============================================================================
// Test Fixture
// ============================================================================

class CortisolDynamicsTest : public ::testing::Test {
protected:
    void SetUp() override {
        System = std::make_unique<MockCortisolDynamicsSystem>();
    }

    void TearDown() override {
        System.reset();
    }

    std::unique_ptr<MockCortisolDynamicsSystem> System;
};

// ============================================================================
// Initialization Tests
// ============================================================================

TEST_F(CortisolDynamicsTest, Initialization) {
    FCortisolDynamicsState State = System->GetState();

    EXPECT_FLOAT_EQ(State.TotalCortisol, 0.3f);
    EXPECT_NEAR(State.FreeCortisol, 0.015f, 0.001f);
    EXPECT_FLOAT_EQ(State.Receptors.Sensitivity, 1.0f);
    EXPECT_FLOAT_EQ(State.ChronicStressLoad, 0.0f);
    EXPECT_FALSE(State.bChronicStressActive);
    EXPECT_EQ(System->GetActiveStressEventCount(), 0);
}

TEST_F(CortisolDynamicsTest, ResetRestoresDefaults) {
    System->ApplyStressor(1.0f, 10.0f);
    SimulateSeconds(*System, 5.0f);
    System->Reset();

    FCortisolDynamicsState State = System->GetState();
    EXPECT_FLOAT_EQ(State.TotalCortisol, 0.3f);
    EXPECT_FLOAT_EQ(State.HPAAxis.CRHLevel, 0.1f);
    EXPECT_EQ(System->GetActiveStressEventCount(), 0);
}

// ============================================================================
// Circadian Rhythm Tests
// ============================================================================

TEST_F(CortisolDynamicsTest, CircadianRhythmMorningPeak) {
    // At morning peak (8:00), circadian contribution should be at maximum
    System->SetTimeOfDay(8.0f);
    System->SetTimeScale(0.0f); // Freeze time so it stays at 8:00
    SimulateSeconds(*System, 1.0f);

    FCortisolDynamicsState State = System->GetState();
    // Baseline(0.25) + Amplitude(0.15) * cos(0) = 0.40
    EXPECT_NEAR(State.Circadian.CurrentContribution, 0.40f, 0.02f);
}

TEST_F(CortisolDynamicsTest, CircadianRhythmEveningTrough) {
    // At 20:00 (12 hours from peak), circadian contribution should be at minimum
    System->SetTimeOfDay(20.0f);
    System->SetTimeScale(0.0f);
    SimulateSeconds(*System, 1.0f);

    FCortisolDynamicsState State = System->GetState();
    // Baseline(0.25) + Amplitude(0.15) * cos(PI) = 0.25 - 0.15 = 0.10
    EXPECT_NEAR(State.Circadian.CurrentContribution, 0.10f, 0.02f);
}

TEST_F(CortisolDynamicsTest, CircadianTimeAdvances) {
    float InitialTime = System->GetState().Circadian.TimeOfDay;
    System->SetTimeScale(1.0f);
    SimulateSeconds(*System, 60.0f); // 60 game-seconds = 1 simulated hour

    float NewTime = System->GetState().Circadian.TimeOfDay;
    EXPECT_GT(NewTime, InitialTime);
}

// ============================================================================
// Acute Stress Response Tests
// ============================================================================

TEST_F(CortisolDynamicsTest, AcuteStressRaisesCortisolLevel) {
    System->SetTimeScale(0.0f); // Freeze circadian
    float BaselineCortisol = System->GetCortisolLevel();

    System->ApplyStressor(0.8f, 5.0f);
    SimulateSeconds(*System, 3.0f);

    EXPECT_GT(System->GetCortisolLevel(), BaselineCortisol);
}

TEST_F(CortisolDynamicsTest, AcuteStressEventExpires) {
    System->ApplyStressor(0.5f, 2.0f);
    EXPECT_EQ(System->GetActiveStressEventCount(), 1);

    SimulateSeconds(*System, 3.0f);
    EXPECT_EQ(System->GetActiveStressEventCount(), 0);
}

TEST_F(CortisolDynamicsTest, StrongerStressorProducesHigherCortisol) {
    System->SetTimeScale(0.0f);

    // Apply weak stressor
    MockCortisolDynamicsSystem WeakSystem;
    WeakSystem.SetTimeScale(0.0f);
    WeakSystem.ApplyStressor(0.3f, 5.0f);
    SimulateSeconds(WeakSystem, 3.0f);

    // Apply strong stressor
    MockCortisolDynamicsSystem StrongSystem;
    StrongSystem.SetTimeScale(0.0f);
    StrongSystem.ApplyStressor(0.9f, 5.0f);
    SimulateSeconds(StrongSystem, 3.0f);

    EXPECT_GT(StrongSystem.GetCortisolLevel(), WeakSystem.GetCortisolLevel());
}

// ============================================================================
// HPA Axis Cascade Tests
// ============================================================================

TEST_F(CortisolDynamicsTest, HPACascadeCRHRisesFirst) {
    System->SetTimeScale(0.0f);
    float InitialCRH = System->GetHPAAxisState().CRHLevel;

    System->ApplyStressor(1.0f, 10.0f);

    // CRH should spike immediately on stressor application
    float PostStressCRH = System->GetHPAAxisState().CRHLevel;
    EXPECT_GT(PostStressCRH, InitialCRH);
}

TEST_F(CortisolDynamicsTest, HPACascadeACTHFollowsCRH) {
    System->SetTimeScale(0.0f);
    System->ApplyStressor(1.0f, 10.0f);

    // After a few ticks, ACTH should be driven by CRH
    SimulateSeconds(*System, 2.0f);

    FHPAAxisState HPA = System->GetHPAAxisState();
    EXPECT_GT(HPA.ACTHLevel, 0.1f); // Should have risen from baseline
}

TEST_F(CortisolDynamicsTest, NegativeFeedbackSuppressesCRH) {
    System->SetTimeScale(0.0f);

    // Manually set high cortisol to test feedback
    System->GetMutableState().TotalCortisol = 0.9f;

    FHPAAxisState InitialHPA = System->GetHPAAxisState();
    SimulateSeconds(*System, 5.0f);

    // CRH should be suppressed by high cortisol negative feedback
    FHPAAxisState FinalHPA = System->GetHPAAxisState();
    EXPECT_LT(FinalHPA.CRHLevel, InitialHPA.CRHLevel);
}

// ============================================================================
// Chronic Stress Tests
// ============================================================================

TEST_F(CortisolDynamicsTest, ChronicStressAccumulation) {
    EXPECT_FLOAT_EQ(System->GetStressLoad(), 0.0f);

    System->ApplyChronicStressor(0.7f);
    SimulateSeconds(*System, 20.0f);

    EXPECT_GT(System->GetStressLoad(), 0.0f);
}

TEST_F(CortisolDynamicsTest, ChronicStressDissipates) {
    // Build up some chronic stress
    System->ApplyChronicStressor(0.8f);
    SimulateSeconds(*System, 30.0f);
    float PeakStress = System->GetStressLoad();
    EXPECT_GT(PeakStress, 0.0f);

    // Remove stressor and let it dissipate
    System->RemoveChronicStressor();
    SimulateSeconds(*System, 30.0f);

    EXPECT_LT(System->GetStressLoad(), PeakStress);
}

TEST_F(CortisolDynamicsTest, ChronicStressReducesNegativeFeedback) {
    // Build up chronic stress load
    System->ApplyChronicStressor(1.0f);
    SimulateSeconds(*System, 50.0f);

    FHPAAxisState HPA = System->GetHPAAxisState();
    // Negative feedback gain should be reduced by chronic stress
    EXPECT_LT(HPA.NegativeFeedbackGain, 1.0f);
}

// ============================================================================
// Receptor Sensitivity Tests
// ============================================================================

TEST_F(CortisolDynamicsTest, ReceptorDownregulationUnderHighCortisol) {
    System->SetTimeScale(0.0f);
    EXPECT_FLOAT_EQ(System->GetReceptorState().Sensitivity, 1.0f);

    // Force high cortisol
    System->GetMutableState().TotalCortisol = 0.9f;
    SimulateSeconds(*System, 20.0f);

    EXPECT_LT(System->GetReceptorState().Sensitivity, 1.0f);
}

TEST_F(CortisolDynamicsTest, ReceptorRecoveryUnderLowCortisol) {
    System->SetTimeScale(0.0f);
    // Set circadian to trough to minimize cortisol production
    System->SetTimeOfDay(20.0f);

    // Set sensitivity below normal and low cortisol to trigger recovery
    System->GetMutableState().Receptors.Sensitivity = 0.5f;
    System->GetMutableState().TotalCortisol = 0.1f;

    float InitialSensitivity = System->GetReceptorState().Sensitivity;

    // Short simulation - cortisol stays low because circadian is at trough
    SimulateSeconds(*System, 10.0f);

    EXPECT_GT(System->GetReceptorState().Sensitivity, InitialSensitivity);
}

TEST_F(CortisolDynamicsTest, ReceptorSensitivityHasFloor) {
    System->SetTimeScale(0.0f);

    // Extreme high cortisol for a long time
    System->GetMutableState().TotalCortisol = 1.0f;
    SimulateSeconds(*System, 200.0f);

    // Sensitivity should not go below minimum (0.1)
    EXPECT_GE(System->GetReceptorState().Sensitivity, 0.1f);
}

// ============================================================================
// Cortisol Clearance and Homeostasis Tests
// ============================================================================

TEST_F(CortisolDynamicsTest, HomeostasisReturnAfterStress) {
    System->SetTimeScale(0.0f);

    // Disable circadian production to isolate stress recovery dynamics
    System->GetMutableState().Circadian.Baseline = 0.0f;
    System->GetMutableState().Circadian.Amplitude = 0.0f;

    // Apply moderate acute stressor
    System->ApplyStressor(0.5f, 2.0f);
    SimulateSeconds(*System, 2.0f);

    // Let the stress event expire, then wait a bit for HPA axis to wind down
    SimulateSeconds(*System, 5.0f);
    float PostStressCortisol = System->GetCortisolLevel();
    EXPECT_GT(PostStressCortisol, 0.3f); // Should be above initial baseline

    // Wait for clearance to bring cortisol back down
    SimulateSeconds(*System, 120.0f);
    float RecoveredCortisol = System->GetCortisolLevel();

    EXPECT_LT(RecoveredCortisol, PostStressCortisol);
}

TEST_F(CortisolDynamicsTest, FreeBoundCortisolPartitioning) {
    // Under normal cortisol levels, free fraction should be ~5%
    FCortisolDynamicsState State = System->GetState();
    EXPECT_NEAR(State.FreeCortisol / State.TotalCortisol, 0.05f, 0.01f);
    EXPECT_NEAR(State.FreeCortisol + State.BoundCortisol, State.TotalCortisol, 0.001f);
}

TEST_F(CortisolDynamicsTest, CBGSaturationIncreasesFreeFraction) {
    System->SetTimeScale(0.0f);

    // Force very high cortisol to saturate CBG
    System->GetMutableState().TotalCortisol = 0.9f;
    System->Update(0.01f); // Single tick to recalculate partitioning

    FCortisolDynamicsState State = System->GetState();
    float FreeFraction = State.FreeCortisol / State.TotalCortisol;
    EXPECT_GT(FreeFraction, 0.05f); // Should be higher than normal 5%
}

// ============================================================================
// Multiple Concurrent Stressors Tests
// ============================================================================

TEST_F(CortisolDynamicsTest, MultipleConcurrentStressorsAreAdditive) {
    System->SetTimeScale(0.0f);

    // Single stressor baseline
    MockCortisolDynamicsSystem SingleSystem;
    SingleSystem.SetTimeScale(0.0f);
    SingleSystem.ApplyStressor(0.5f, 5.0f);
    SimulateSeconds(SingleSystem, 2.0f);

    // Multiple stressors
    MockCortisolDynamicsSystem MultiSystem;
    MultiSystem.SetTimeScale(0.0f);
    MultiSystem.ApplyStressor(0.5f, 5.0f);
    MultiSystem.ApplyStressor(0.5f, 5.0f);
    SimulateSeconds(MultiSystem, 2.0f);

    EXPECT_GT(MultiSystem.GetCortisolLevel(), SingleSystem.GetCortisolLevel());
}

TEST_F(CortisolDynamicsTest, StressEventCountTracking) {
    EXPECT_EQ(System->GetActiveStressEventCount(), 0);

    System->ApplyStressor(0.5f, 10.0f);
    EXPECT_EQ(System->GetActiveStressEventCount(), 1);

    System->ApplyStressor(0.3f, 10.0f);
    EXPECT_EQ(System->GetActiveStressEventCount(), 2);

    // Expire all events
    SimulateSeconds(*System, 15.0f);
    EXPECT_EQ(System->GetActiveStressEventCount(), 0);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(CortisolDynamicsTest, ZeroDeltaTimeNoOp) {
    FCortisolDynamicsState Before = System->GetState();
    System->Update(0.0f);
    FCortisolDynamicsState After = System->GetState();

    EXPECT_FLOAT_EQ(Before.TotalCortisol, After.TotalCortisol);
}

TEST_F(CortisolDynamicsTest, NegativeDeltaTimeNoOp) {
    FCortisolDynamicsState Before = System->GetState();
    System->Update(-1.0f);
    FCortisolDynamicsState After = System->GetState();

    EXPECT_FLOAT_EQ(Before.TotalCortisol, After.TotalCortisol);
}

TEST_F(CortisolDynamicsTest, CortisolStaysInRange) {
    System->SetTimeScale(0.0f);

    // Apply extreme stress
    for (int i = 0; i < 10; i++)
    {
        System->ApplyStressor(1.0f, 100.0f);
    }
    SimulateSeconds(*System, 50.0f);

    EXPECT_LE(System->GetCortisolLevel(), 1.0f);
    EXPECT_GE(System->GetCortisolLevel(), 0.0f);
}

TEST_F(CortisolDynamicsTest, SetTimeOfDayWrapsCorrectly) {
    System->SetTimeOfDay(25.0f);
    EXPECT_NEAR(System->GetState().Circadian.TimeOfDay, 1.0f, 0.01f);

    System->SetTimeOfDay(-2.0f);
    EXPECT_NEAR(System->GetState().Circadian.TimeOfDay, 22.0f, 0.01f);
}

// ============================================================================
// Main Entry Point
// ============================================================================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
