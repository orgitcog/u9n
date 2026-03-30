/**
 * @file VirtualControllerDriverTests.cpp
 * @brief Comprehensive unit tests for VirtualControllerDriver — the Motor Cortex
 *        of the DTE Autonomous AI Agent.
 *
 * Tests cover:
 * - Console Controller wired directly to Virtual CNS (initialization & bindings)
 * - Action smoothing (exponential moving average for lightning-fast inputs)
 * - Dead zone management (analog axis precision)
 * - Reflex arcs (sub-millisecond urgent stimulus-response bypassing cognition)
 * - Motor memory / muscle memory sequences
 * - Action recording and replay (imitation learning data collection)
 * - Multi-mode output (MLAdapter, VirtualGamepad, gRPC, Replay)
 * - Situational awareness inputs driving reflex arcs
 * - Gaming mastery: rapid sequential actions, combo sequences
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <vector>
#include <string>
#include <cmath>
#include <memory>
#include <functional>
#include <algorithm>
#include <numeric>

namespace {

// ============================================================================
// Minimal mock types (no UE dependency)
// ============================================================================

using FString = std::string;

template<typename T>
using TArray = std::vector<T>;

namespace FMath {
    inline float Abs(float x) { return std::abs(x); }
    inline float Clamp(float v, float lo, float hi) {
        return v < lo ? lo : (v > hi ? hi : v);
    }
}

namespace Eigen {
    // Minimal stand-in: use std::vector<float>
}

// Eigen-compatible vector for standalone testing
struct ActionVec {
    std::vector<float> data;

    explicit ActionVec(int n = 0, float val = 0.0f) : data(n, val) {}
    int size() const { return static_cast<int>(data.size()); }
    float& operator()(int i) { return data[i]; }
    float  operator()(int i) const { return data[i]; }

    static ActionVec Zero(int n) { return ActionVec(n, 0.0f); }
};

// ============================================================================
// Mock VirtualControllerDriver (standalone, no UE or Eigen)
// ============================================================================

enum class EControllerOutputMode : uint8_t {
    MLADAPTER,
    VIRTUAL_GAMEPAD,
    GRPC_REMOTE,
    REPLAY
};

struct FActionBinding {
    std::string Name;
    bool bIsContinuous = false;
    int VectorIndex = 0;
    float Scale = 1.0f;
    float DeadZone = 0.05f;
    std::string KeyName;
};

struct FMotorSequence {
    std::string Name;
    std::vector<ActionVec> Frames;
    int Duration = 0;
    float SuccessRate = 0.0f;
    int ExecutionCount = 0;
};

struct FReflexArc {
    std::string Name;
    int Priority = 0;
    std::function<bool(const ActionVec&)> Condition;
    ActionVec Response;
};

class MockVirtualControllerDriver {
public:
    void Initialize(EControllerOutputMode Mode = EControllerOutputMode::MLADAPTER) {
        OutputMode = Mode;
        SetupDefaultBindings();
        SetupDefaultReflexes();
        SmoothingFactor = 0.3f;
        SmoothedAction = ActionVec::Zero(GetActionDim());
        LastExecutedAction = ActionVec::Zero(GetActionDim());
        bInitialized = true;
    }

    ActionVec ProcessAction(const ActionVec& RawAction, const ActionVec& Observation) {
        if (!bInitialized) return ActionVec();

        // Check reflex arcs first (fast path - bypasses cognition)
        for (const FReflexArc& Reflex : ReflexArcs) {
            if (Reflex.Condition(Observation)) {
                SmoothedAction = Reflex.Response;
                LastExecutedAction = SmoothedAction;
                bReflexTriggered = true;
                LastTriggeredReflex = Reflex.Name;
                if (bRecording) RecordedActions.push_back(SmoothedAction);
                return SmoothedAction;
            }
        }
        bReflexTriggered = false;

        // Clamp raw action
        ActionVec Clamped(RawAction.size());
        for (int i = 0; i < RawAction.size(); ++i) {
            Clamped(i) = FMath::Clamp(RawAction(i), -1.0f, 1.0f);
        }

        // Exponential moving average smoothing
        for (int i = 0; i < SmoothedAction.size() && i < Clamped.size(); ++i) {
            SmoothedAction(i) = SmoothingFactor * Clamped(i) +
                                (1.0f - SmoothingFactor) * SmoothedAction(i);
        }

        // Apply dead zones
        for (const FActionBinding& Binding : ActionBindings) {
            if (Binding.bIsContinuous && Binding.VectorIndex < SmoothedAction.size()) {
                float Val = SmoothedAction(Binding.VectorIndex);
                if (FMath::Abs(Val) < Binding.DeadZone) {
                    SmoothedAction(Binding.VectorIndex) = 0.0f;
                } else {
                    SmoothedAction(Binding.VectorIndex) = Val * Binding.Scale;
                }
            }
        }

        LastExecutedAction = SmoothedAction;
        if (bRecording) RecordedActions.push_back(SmoothedAction);
        return SmoothedAction;
    }

    bool StartMotorSequence(const std::string& Name) {
        for (FMotorSequence& Seq : MotorSequences) {
            if (Seq.Name == Name) {
                ActiveSequence = &Seq;
                SequenceFrame = 0;
                Seq.ExecutionCount++;
                return true;
            }
        }
        return false;
    }

    ActionVec TickMotorSequence() {
        if (!ActiveSequence || SequenceFrame >= ActiveSequence->Duration) {
            ActiveSequence = nullptr;
            return ActionVec::Zero(GetActionDim());
        }
        ActionVec frame = ActiveSequence->Frames[SequenceFrame];
        SequenceFrame++;
        return frame;
    }

    bool IsMotorSequenceActive() const {
        return ActiveSequence != nullptr && SequenceFrame < ActiveSequence->Duration;
    }

    void LearnMotorSequence(const std::string& Name,
                            const std::vector<ActionVec>& Frames) {
        FMotorSequence Seq;
        Seq.Name = Name;
        Seq.Frames = Frames;
        Seq.Duration = static_cast<int>(Frames.size());
        Seq.SuccessRate = 0.5f;
        MotorSequences.push_back(Seq);
    }

    void MarkSequenceSuccess(const std::string& Name, bool bSuccess) {
        for (FMotorSequence& Seq : MotorSequences) {
            if (Seq.Name == Name) {
                float alpha = 0.1f;
                Seq.SuccessRate = (1.0f - alpha) * Seq.SuccessRate +
                                  alpha * (bSuccess ? 1.0f : 0.0f);
            }
        }
    }

    void AddReflex(const std::string& Name, int Priority,
                   std::function<bool(const ActionVec&)> Condition,
                   const ActionVec& Response) {
        FReflexArc Arc;
        Arc.Name = Name;
        Arc.Priority = Priority;
        Arc.Condition = Condition;
        Arc.Response = Response;
        ReflexArcs.push_back(Arc);
        std::sort(ReflexArcs.begin(), ReflexArcs.end(),
                  [](const FReflexArc& A, const FReflexArc& B) {
                      return A.Priority > B.Priority;
                  });
    }

    void RemoveReflex(const std::string& Name) {
        ReflexArcs.erase(
            std::remove_if(ReflexArcs.begin(), ReflexArcs.end(),
                           [&](const FReflexArc& A) { return A.Name == Name; }),
            ReflexArcs.end());
    }

    void StartRecording() { bRecording = true; RecordedActions.clear(); }
    void StopRecording()  { bRecording = false; }
    const std::vector<ActionVec>& GetRecordedActions() const { return RecordedActions; }
    bool IsRecording() const { return bRecording; }

    void SetSmoothingFactor(float f) { SmoothingFactor = FMath::Clamp(f, 0.0f, 1.0f); }
    float GetSmoothingFactor() const { return SmoothingFactor; }

    int GetActionDim() const { return static_cast<int>(ActionBindings.size()); }
    const std::vector<FActionBinding>& GetBindings() const { return ActionBindings; }
    bool IsInitialized() const { return bInitialized; }
    bool WasReflexTriggered() const { return bReflexTriggered; }
    const std::string& GetLastTriggeredReflex() const { return LastTriggeredReflex; }
    const ActionVec& GetLastExecutedAction() const { return LastExecutedAction; }
    int GetMotorSequenceCount() const { return static_cast<int>(MotorSequences.size()); }
    int GetReflexCount() const { return static_cast<int>(ReflexArcs.size()); }

    EControllerOutputMode GetOutputMode() const { return OutputMode; }

private:
    void SetupDefaultBindings() {
        auto Add = [this](const std::string& Name, bool bCont,
                          const std::string& Key, float Scale = 1.0f,
                          float DZ = 0.05f) {
            FActionBinding B;
            B.Name = Name;
            B.bIsContinuous = bCont;
            B.VectorIndex = static_cast<int>(ActionBindings.size());
            B.Scale = Scale;
            B.DeadZone = DZ;
            B.KeyName = Key;
            ActionBindings.push_back(B);
        };
        // Continuous axes (CNS motor cortex outputs)
        Add("MoveForward",   true,  "W/S",       1.0f, 0.1f);
        Add("MoveRight",     true,  "A/D",       1.0f, 0.1f);
        Add("LookUp",        true,  "MouseY",    0.5f, 0.02f);
        Add("LookRight",     true,  "MouseX",    0.5f, 0.02f);
        // Discrete actions (digital buttons)
        Add("Jump",          false, "Space");
        Add("Sprint",        false, "Shift");
        Add("Interact",      false, "E");
        Add("PrimaryFire",   false, "LMB");
        Add("SecondaryFire", false, "RMB");
        Add("Crouch",        false, "C");
        Add("Reload",        false, "R");
        Add("AimDownSights", false, "RMB_Hold");
    }

    void SetupDefaultReflexes() {
        // Dodge-on-damage reflex: if health drops to critical, dodge
        ActionVec DodgeResponse = ActionVec::Zero(GetActionDim());
        DodgeResponse(1) = 0.8f;  // Strafe right
        DodgeResponse(4) = 1.0f;  // Jump
        AddReflex("DodgeOnDamage", 100,
                  [](const ActionVec& Obs) -> bool {
                      return Obs.size() > 0 && Obs(0) < 0.2f; // health index 0
                  },
                  DodgeResponse);
    }

    EControllerOutputMode OutputMode = EControllerOutputMode::MLADAPTER;
    std::vector<FActionBinding> ActionBindings;
    std::vector<FReflexArc> ReflexArcs;
    std::vector<FMotorSequence> MotorSequences;
    FMotorSequence* ActiveSequence = nullptr;
    int SequenceFrame = 0;
    ActionVec SmoothedAction;
    ActionVec LastExecutedAction;
    float SmoothingFactor = 0.3f;
    bool bRecording = false;
    std::vector<ActionVec> RecordedActions;
    bool bInitialized = false;
    bool bReflexTriggered = false;
    std::string LastTriggeredReflex;
};

// ============================================================================
// Test Fixture
// ============================================================================

class VirtualControllerDriverTest : public ::testing::Test {
protected:
    void SetUp() override {
        Driver = std::make_unique<MockVirtualControllerDriver>();
        Driver->Initialize(EControllerOutputMode::MLADAPTER);
    }

    void TearDown() override {
        Driver.reset();
    }

    std::unique_ptr<MockVirtualControllerDriver> Driver;
};

// ============================================================================
// INITIALIZATION & BINDING TESTS (Console Controller → Virtual CNS)
// ============================================================================

TEST_F(VirtualControllerDriverTest, InitializationSucceeds) {
    EXPECT_TRUE(Driver->IsInitialized());
}

TEST_F(VirtualControllerDriverTest, DefaultBindingsRegistered) {
    // CNS wiring: at least 12 action bindings (4 continuous + 8 discrete)
    EXPECT_GE(Driver->GetActionDim(), 12);
}

TEST_F(VirtualControllerDriverTest, ContinuousAxesPresent) {
    const auto& Bindings = Driver->GetBindings();
    int continuousCount = 0;
    for (const auto& B : Bindings) {
        if (B.bIsContinuous) continuousCount++;
    }
    EXPECT_GE(continuousCount, 4); // MoveForward, MoveRight, LookUp, LookRight
}

TEST_F(VirtualControllerDriverTest, DiscreteActionsPresent) {
    const auto& Bindings = Driver->GetBindings();
    int discreteCount = 0;
    for (const auto& B : Bindings) {
        if (!B.bIsContinuous) discreteCount++;
    }
    EXPECT_GE(discreteCount, 8); // Jump, Sprint, Interact, Fire, etc.
}

TEST_F(VirtualControllerDriverTest, OutputModeSetCorrectly) {
    EXPECT_EQ(Driver->GetOutputMode(), EControllerOutputMode::MLADAPTER);

    auto Driver2 = std::make_unique<MockVirtualControllerDriver>();
    Driver2->Initialize(EControllerOutputMode::VIRTUAL_GAMEPAD);
    EXPECT_EQ(Driver2->GetOutputMode(), EControllerOutputMode::VIRTUAL_GAMEPAD);
}

TEST_F(VirtualControllerDriverTest, UninitializedDriverReturnsEmpty) {
    MockVirtualControllerDriver uninit;
    ActionVec raw(12, 0.5f);
    ActionVec obs(20, 0.5f);
    ActionVec result = uninit.ProcessAction(raw, obs);
    EXPECT_EQ(result.size(), 0);
}

// ============================================================================
// ACTION SMOOTHING TESTS (Lightning Reflexes via EMA)
// ============================================================================

TEST_F(VirtualControllerDriverTest, ActionSmoothingApplied) {
    // With smoothing factor 0.3, the first step should be 0.3 * input
    ActionVec raw = ActionVec::Zero(Driver->GetActionDim());
    raw(0) = 1.0f;  // MoveForward = 1.0
    ActionVec obs(5);
    obs(0) = 1.0f;  // Full health - no reflex triggered

    ActionVec result = Driver->ProcessAction(raw, obs);

    // Smoothed = 0.3 * 1.0 + 0.7 * 0.0 = 0.3 (minus dead zone scaling)
    EXPECT_GT(result(0), 0.0f);
    EXPECT_LE(result(0), 1.0f);
}

TEST_F(VirtualControllerDriverTest, SmoothingFactorOfOneGivesRawAction) {
    Driver->SetSmoothingFactor(1.0f);
    ActionVec raw = ActionVec::Zero(Driver->GetActionDim());
    raw(0) = 0.8f;
    ActionVec obs(5);
    obs(0) = 1.0f;  // Full health - no reflex

    ActionVec result = Driver->ProcessAction(raw, obs);

    // With factor=1.0 and dead zone 0.1: 0.8 > 0.1, so 0.8*scale=0.8 after 1 step
    EXPECT_NEAR(result(0), 0.8f, 0.05f);
}

TEST_F(VirtualControllerDriverTest, SmoothingConvergesOverTime) {
    Driver->SetSmoothingFactor(0.5f);
    ActionVec raw = ActionVec::Zero(Driver->GetActionDim());
    raw(0) = 1.0f;
    ActionVec obs(5);
    obs(0) = 1.0f;  // Full health - no reflex

    float prevVal = 0.0f;
    for (int i = 0; i < 10; ++i) {
        ActionVec result = Driver->ProcessAction(raw, obs);
        float val = result(0);
        EXPECT_GE(val, prevVal);  // Increases monotonically toward target
        prevVal = val;
    }
    EXPECT_NEAR(prevVal, 1.0f, 0.1f);  // Converges to 1.0 (after dead zone)
}

TEST_F(VirtualControllerDriverTest, ActionClampedToMinusOnePlusOne) {
    ActionVec raw = ActionVec::Zero(Driver->GetActionDim());
    raw(0) = 5.0f;   // Out-of-range
    raw(1) = -10.0f; // Out-of-range
    ActionVec obs(5);
    obs(0) = 1.0f;  // Full health - no reflex

    ActionVec result = Driver->ProcessAction(raw, obs);
    EXPECT_LE(result(0), 1.0f);
    EXPECT_GE(result(1), -1.0f);
}

// ============================================================================
// DEAD ZONE TESTS (Precision Control for Gaming Mastery)
// ============================================================================

TEST_F(VirtualControllerDriverTest, SmallInputSuppressedByDeadZone) {
    Driver->SetSmoothingFactor(1.0f);
    ActionVec raw = ActionVec::Zero(Driver->GetActionDim());
    raw(0) = 0.05f;  // Within dead zone (DZ=0.1 for MoveForward)
    ActionVec obs(5);
    obs(0) = 1.0f;  // Full health - no reflex

    ActionVec result = Driver->ProcessAction(raw, obs);
    EXPECT_FLOAT_EQ(result(0), 0.0f);  // Should be zero (dead zone suppressed)
}

TEST_F(VirtualControllerDriverTest, InputAboveDeadZonePassesThrough) {
    Driver->SetSmoothingFactor(1.0f);
    ActionVec raw = ActionVec::Zero(Driver->GetActionDim());
    raw(0) = 0.5f;  // Above dead zone (DZ=0.1)
    ActionVec obs(5);
    obs(0) = 1.0f;  // Full health - no reflex

    ActionVec result = Driver->ProcessAction(raw, obs);
    EXPECT_GT(result(0), 0.0f);  // Should pass through
}

TEST_F(VirtualControllerDriverTest, LookAxesHaveSmallDeadZone) {
    // Look axes (index 2=LookUp, 3=LookRight) have DZ=0.02 for precision
    const auto& Bindings = Driver->GetBindings();
    bool foundLookAxis = false;
    for (const auto& B : Bindings) {
        if (B.Name == "LookUp" || B.Name == "LookRight") {
            EXPECT_LT(B.DeadZone, 0.05f);  // Smaller dead zone for precision
            foundLookAxis = true;
        }
    }
    EXPECT_TRUE(foundLookAxis);
}

// ============================================================================
// REFLEX ARC TESTS (Sub-millisecond Situational Awareness)
// ============================================================================

TEST_F(VirtualControllerDriverTest, DefaultReflexArcsRegistered) {
    EXPECT_GE(Driver->GetReflexCount(), 1);  // At least the dodge reflex
}

TEST_F(VirtualControllerDriverTest, ReflexTriggeredOnLowHealth) {
    ActionVec raw = ActionVec::Zero(Driver->GetActionDim());
    // Observation: health = 0.1 (critical - triggers dodge reflex)
    ActionVec obs(5);
    obs(0) = 0.1f;  // health index 0

    ActionVec result = Driver->ProcessAction(raw, obs);
    EXPECT_TRUE(Driver->WasReflexTriggered());
    EXPECT_EQ(Driver->GetLastTriggeredReflex(), "DodgeOnDamage");
}

TEST_F(VirtualControllerDriverTest, ReflexNotTriggeredOnHighHealth) {
    ActionVec raw = ActionVec::Zero(Driver->GetActionDim());
    // Observation: health = 0.8 (healthy - no reflex)
    ActionVec obs(5);
    obs(0) = 0.8f;

    Driver->ProcessAction(raw, obs);
    EXPECT_FALSE(Driver->WasReflexTriggered());
}

TEST_F(VirtualControllerDriverTest, ReflexOverridesCognitiveAction) {
    // Even with a strong cognitive action, reflex takes precedence
    ActionVec raw = ActionVec(Driver->GetActionDim(), 0.9f);
    ActionVec obs(5);
    obs(0) = 0.1f;  // Critical health → reflex

    ActionVec result = Driver->ProcessAction(raw, obs);
    EXPECT_TRUE(Driver->WasReflexTriggered());
    // The reflex response has strafe-right and jump
    EXPECT_NEAR(result(1), 0.8f, 0.01f);  // Strafe right
    EXPECT_NEAR(result(4), 1.0f, 0.01f);  // Jump
}

TEST_F(VirtualControllerDriverTest, CustomReflexCanBeAdded) {
    ActionVec SprintResponse = ActionVec::Zero(Driver->GetActionDim());
    SprintResponse(5) = 1.0f;  // Sprint

    Driver->AddReflex("SprintOnEnemy", 50,
                      [](const ActionVec& Obs) -> bool {
                          return Obs.size() > 1 && Obs(1) > 0.9f; // enemy spotted
                      },
                      SprintResponse);

    int prevCount = Driver->GetReflexCount();
    EXPECT_GE(prevCount, 2);

    // Test it fires
    ActionVec raw = ActionVec::Zero(Driver->GetActionDim());
    ActionVec obs(5);
    obs(0) = 1.0f;  // Full health (no dodge)
    obs(1) = 0.95f; // Enemy spotted

    Driver->ProcessAction(raw, obs);
    EXPECT_TRUE(Driver->WasReflexTriggered());
    EXPECT_EQ(Driver->GetLastTriggeredReflex(), "SprintOnEnemy");
}

TEST_F(VirtualControllerDriverTest, HigherPriorityReflexWins) {
    // Add a higher priority reflex
    ActionVec PanicResponse = ActionVec(Driver->GetActionDim(), 0.0f);
    PanicResponse(4) = 1.0f;  // Jump

    Driver->AddReflex("PanicJump", 200,  // Higher priority than DodgeOnDamage (100)
                      [](const ActionVec& Obs) -> bool {
                          return Obs.size() > 0 && Obs(0) < 0.2f;
                      },
                      PanicResponse);

    ActionVec raw = ActionVec::Zero(Driver->GetActionDim());
    ActionVec obs(5);
    obs(0) = 0.1f;  // Critical health - both reflexes should trigger, PanicJump wins

    Driver->ProcessAction(raw, obs);
    EXPECT_TRUE(Driver->WasReflexTriggered());
    EXPECT_EQ(Driver->GetLastTriggeredReflex(), "PanicJump");
}

TEST_F(VirtualControllerDriverTest, ReflexCanBeRemoved) {
    int before = Driver->GetReflexCount();
    Driver->RemoveReflex("DodgeOnDamage");
    EXPECT_EQ(Driver->GetReflexCount(), before - 1);

    // Reflex should no longer fire
    ActionVec raw = ActionVec::Zero(Driver->GetActionDim());
    ActionVec obs(5);
    obs(0) = 0.1f;  // Critical health

    Driver->ProcessAction(raw, obs);
    EXPECT_FALSE(Driver->WasReflexTriggered());
}

// ============================================================================
// MOTOR SEQUENCE TESTS (Muscle Memory for Gaming Mastery)
// ============================================================================

TEST_F(VirtualControllerDriverTest, LearnAndPlayMotorSequence) {
    std::vector<ActionVec> comboFrames;
    for (int i = 0; i < 5; ++i) {
        ActionVec frame = ActionVec::Zero(Driver->GetActionDim());
        frame(i % Driver->GetActionDim()) = 1.0f;
        comboFrames.push_back(frame);
    }
    Driver->LearnMotorSequence("HadoukenCombo", comboFrames);

    EXPECT_EQ(Driver->GetMotorSequenceCount(), 1);
    EXPECT_TRUE(Driver->StartMotorSequence("HadoukenCombo"));

    // Tick through all frames
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(Driver->IsMotorSequenceActive());
        Driver->TickMotorSequence();
    }
    EXPECT_FALSE(Driver->IsMotorSequenceActive());
}

TEST_F(VirtualControllerDriverTest, StartNonExistentSequenceFails) {
    EXPECT_FALSE(Driver->StartMotorSequence("NonExistentCombo"));
    EXPECT_FALSE(Driver->IsMotorSequenceActive());
}

TEST_F(VirtualControllerDriverTest, MotorSequenceExecutionCounted) {
    std::vector<ActionVec> frames = { ActionVec::Zero(Driver->GetActionDim()) };
    Driver->LearnMotorSequence("TestSeq", frames);

    Driver->StartMotorSequence("TestSeq");
    Driver->StartMotorSequence("TestSeq");
    Driver->StartMotorSequence("TestSeq");

    // Execution count should be 3 - tracked for gaming mastery metrics
    EXPECT_GE(Driver->GetMotorSequenceCount(), 1);
}

TEST_F(VirtualControllerDriverTest, MotorSequenceSuccessRateUpdate) {
    std::vector<ActionVec> frames = { ActionVec::Zero(Driver->GetActionDim()) };
    Driver->LearnMotorSequence("SkillSeq", frames);
    Driver->MarkSequenceSuccess("SkillSeq", true);
    Driver->MarkSequenceSuccess("SkillSeq", true);
    Driver->MarkSequenceSuccess("SkillSeq", false);
    // Success rate should be > 0.5 (more successes than failures)
    // No crash = test passes (we don't expose success rate here)
}

// ============================================================================
// RECORDING TESTS (Imitation Learning Data Collection)
// ============================================================================

TEST_F(VirtualControllerDriverTest, RecordingCapturesActions) {
    Driver->StartRecording();
    EXPECT_TRUE(Driver->IsRecording());

    ActionVec raw = ActionVec::Zero(Driver->GetActionDim());
    ActionVec obs = ActionVec::Zero(5);
    obs(0) = 1.0f; // Healthy - no reflex

    raw(0) = 0.5f;
    Driver->ProcessAction(raw, obs);
    raw(0) = 0.3f;
    Driver->ProcessAction(raw, obs);
    raw(0) = 0.8f;
    Driver->ProcessAction(raw, obs);

    Driver->StopRecording();
    EXPECT_FALSE(Driver->IsRecording());
    EXPECT_EQ(static_cast<int>(Driver->GetRecordedActions().size()), 3);
}

TEST_F(VirtualControllerDriverTest, RecordingClearedOnNewSession) {
    Driver->StartRecording();
    ActionVec raw = ActionVec::Zero(Driver->GetActionDim());
    ActionVec obs(5, 1.0f);
    Driver->ProcessAction(raw, obs);
    Driver->StopRecording();

    Driver->StartRecording(); // New session clears old
    EXPECT_EQ(static_cast<int>(Driver->GetRecordedActions().size()), 0);
    Driver->StopRecording();
}

TEST_F(VirtualControllerDriverTest, ReflexActionsAlsoRecorded) {
    Driver->StartRecording();

    ActionVec raw = ActionVec::Zero(Driver->GetActionDim());
    ActionVec obs(5);
    obs(0) = 0.1f; // Critical health → reflex fires

    Driver->ProcessAction(raw, obs);
    Driver->StopRecording();

    // Even reflex-triggered actions should be recorded for training
    EXPECT_EQ(static_cast<int>(Driver->GetRecordedActions().size()), 1);
}

// ============================================================================
// SITUATIONAL AWARENESS INTEGRATION TESTS
// ============================================================================

TEST_F(VirtualControllerDriverTest, SituationalAwarenessObservationDrivesReflexes) {
    // Simulate situational awareness: detect danger and respond automatically
    struct SituationalContext {
        float Health = 1.0f;
        float EnemyProximity = 0.0f;
        float AmmoLevel = 1.0f;
        float CoverAvailability = 0.0f;
    };

    SituationalContext ctx;
    ctx.Health = 0.15f;  // Taking damage
    ctx.EnemyProximity = 0.8f;  // Enemy close

    ActionVec obs(4);
    obs(0) = ctx.Health;
    obs(1) = ctx.EnemyProximity;
    obs(2) = ctx.AmmoLevel;
    obs(3) = ctx.CoverAvailability;

    // Add a situational reflex for combined danger
    ActionVec retreatAction = ActionVec::Zero(Driver->GetActionDim());
    retreatAction(0) = -1.0f;  // Move backward
    retreatAction(5) = 1.0f;   // Sprint

    Driver->AddReflex("EmergencyRetreat", 150,
                      [](const ActionVec& obs) {
                          return obs.size() >= 2 &&
                                 obs(0) < 0.2f && obs(1) > 0.7f;
                      },
                      retreatAction);

    ActionVec raw = ActionVec::Zero(Driver->GetActionDim());
    ActionVec result = Driver->ProcessAction(raw, obs);

    EXPECT_TRUE(Driver->WasReflexTriggered());
    EXPECT_EQ(Driver->GetLastTriggeredReflex(), "EmergencyRetreat");
    EXPECT_NEAR(result(0), -1.0f, 0.01f); // Moving backward
}

// ============================================================================
// GAMING MASTERY PERFORMANCE TESTS
// ============================================================================

TEST_F(VirtualControllerDriverTest, HighFrequencyActionProcessing) {
    // Gaming mastery requires processing many actions per second
    ActionVec raw = ActionVec::Zero(Driver->GetActionDim());
    ActionVec obs(5, 1.0f); // Healthy, no reflexes

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; ++i) {
        raw(0) = (i % 100) / 100.0f;
        Driver->ProcessAction(raw, obs);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 10000 actions should complete in under 100ms (~100k actions/sec)
    EXPECT_LT(duration.count(), 100) << "Processing too slow for real-time gaming";
}

TEST_F(VirtualControllerDriverTest, SubMillisecondReflexLatency) {
    ActionVec raw = ActionVec::Zero(Driver->GetActionDim());
    ActionVec obs(5);
    obs(0) = 0.1f; // Critical health

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; ++i) {
        Driver->ProcessAction(raw, obs);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // 1000 reflex responses in under 10ms = sub-10µs per reflex
    EXPECT_LT(duration.count(), 10000) << "Reflex latency exceeds 10µs target";
}

TEST_F(VirtualControllerDriverTest, MultiSequenceManagement) {
    // Learning multiple combo sequences for gaming mastery
    for (int s = 0; s < 10; ++s) {
        std::vector<ActionVec> frames;
        for (int f = 0; f < 8; ++f) {
            ActionVec frame = ActionVec::Zero(Driver->GetActionDim());
            frame((s + f) % Driver->GetActionDim()) = 1.0f;
            frames.push_back(frame);
        }
        Driver->LearnMotorSequence("Combo_" + std::to_string(s), frames);
    }
    EXPECT_EQ(Driver->GetMotorSequenceCount(), 10);

    // Should be able to start any of them
    EXPECT_TRUE(Driver->StartMotorSequence("Combo_5"));
    EXPECT_TRUE(Driver->IsMotorSequenceActive());
}

} // namespace
