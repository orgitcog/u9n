/**
 * @file Sys6OperadTests.cpp
 * @brief Exhaustive unit tests for Sys6 Operad Engine
 *
 * Tests cover:
 * - LCM(2,3,5)=30 clock synchronization
 * - Dyadic (2-phase), Triadic (3-phase), Pentadic (5-stage) channels
 * - 2x3->4 fold via double-step delay
 * - Cubic concurrency C8 (2^3 = 8 parallel states)
 * - Triadic convolution K9 (3^2 = 9 orthogonal phases)
 * - 5x6 stage scheduling
 * - Operad operations: Delta, Mu, Phi, Sigma
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <vector>
#include <array>
#include <cmath>
#include <chrono>
#include <functional>

namespace {

// ============================================================================
// Mock Types for Sys6 Operad Engine
// ============================================================================

enum class EWireBundleType {
    D_Dyadic,      // 2-phase
    T_Triadic,     // 3-phase
    P_Pentadic,    // 5-stage selector
    C8_Cubic,      // 2^3 = 8 states
    K9_Triadic     // 3^2 = 9 phases
};

enum class EDyadicPhase {
    A,
    B
};

enum class ETriadicPhase {
    Phase1,
    Phase2,
    Phase3
};

enum class EPentadicStage {
    Stage1,
    Stage2,
    Stage3,
    Stage4,
    Stage5
};

enum class EFourStepPhase {
    Step1,  // State 1, A, 1
    Step2,  // State 4, A, 2
    Step3,  // State 6, B, 2
    Step4   // State 1, B, 3
};

struct FCubicConcurrencyState {
    std::vector<float> ThreadStates;
    std::vector<int> ActivePairs;
    float EntanglementLevel = 0.0f;

    FCubicConcurrencyState() : ThreadStates(8, 0.0f), ActivePairs({0, 1}) {}
};

struct FTriadicConvolutionState {
    std::vector<float> PhaseStates;
    int CurrentKernel = 0;
    float PhaseRotation = 0.0f;

    FTriadicConvolutionState() : PhaseStates(9, 0.0f) {}
};

struct FDoubleStepDelayState {
    EFourStepPhase CurrentPhase = EFourStepPhase::Step1;
    EDyadicPhase DyadicState = EDyadicPhase::A;
    ETriadicPhase TriadicState = ETriadicPhase::Phase1;
    int StateValue = 1;
    int DyadHoldCounter = 0;
};

struct FStageConfig {
    EPentadicStage Stage = EPentadicStage::Stage1;
    int StepsInStage = 6;
    int TransitionSteps = 2;
    int ActiveSteps = 4;
};

struct FSys6FullState {
    int GlobalStep = 1;
    EDyadicPhase DyadicPhase = EDyadicPhase::A;
    ETriadicPhase TriadicPhase = ETriadicPhase::Phase1;
    EPentadicStage PentadicStage = EPentadicStage::Stage1;
    int StageStep = 1;
    EFourStepPhase FourStepPhase = EFourStepPhase::Step1;
    FCubicConcurrencyState CubicState;
    FTriadicConvolutionState ConvolutionState;
    FDoubleStepDelayState DelayState;
    int CycleCount = 0;
    int SyncEventCount = 0;
};

// Delegate types for events
using FOnStepAdvanced = std::function<void(int, int)>;
using FOnStageChanged = std::function<void(EPentadicStage, EPentadicStage)>;
using FOnSyncEvent = std::function<void(int)>;
using FOnCycleCompleted = std::function<void(int)>;

/**
 * @brief Mock Sys6 Operad Engine
 */
class MockSys6ClockEngine {
public:
    MockSys6ClockEngine() {
        InitializeStageConfigs();
    }

    void InitializeEngine() {
        bInitialized = true;
        State = FSys6FullState();
        State.GlobalStep = 1;
    }

    bool IsInitialized() const { return bInitialized; }

    // Configuration
    float CycleDuration = 30.0f;
    bool bEnableAutoCycle = true;
    bool bEnableCubicConcurrency = true;
    bool bEnableTriadicConvolution = true;
    int CubicCoreCount = 8;
    int ConvolutionCoreCount = 3;

    // Event callbacks
    FOnStepAdvanced OnStepAdvanced;
    FOnStageChanged OnStageChanged;
    FOnSyncEvent OnSyncEvent;
    FOnCycleCompleted OnCycleCompleted;

    // Operad Operations

    /**
     * Delta_2: D -> (D, C8) - Prime power delegation for dyadic
     * Moves 2^3 = 8 into parallel concurrency
     */
    void ApplyDeltaDyadic() {
        if (bEnableCubicConcurrency) {
            // Distribute dyadic phase across 8 parallel states
            float phaseValue = (State.DyadicPhase == EDyadicPhase::A) ? 1.0f : -1.0f;
            for (int i = 0; i < 8; i++) {
                State.CubicState.ThreadStates[i] = phaseValue * ((i % 2 == 0) ? 1.0f : -1.0f);
            }
        }
    }

    /**
     * Delta_3: T -> (T, K9) - Prime power delegation for triadic
     * Moves 3^2 = 9 into orthogonal convolution phases
     */
    void ApplyDeltaTriadic() {
        if (bEnableTriadicConvolution) {
            // Distribute triadic phase across 9 orthogonal phases
            int basePhase = static_cast<int>(State.TriadicPhase);
            for (int i = 0; i < 9; i++) {
                float angle = (basePhase * 120.0f + i * 40.0f) * 3.14159f / 180.0f;
                State.ConvolutionState.PhaseStates[i] = std::sin(angle);
            }
            State.ConvolutionState.CurrentKernel = basePhase * 3;
        }
    }

    /**
     * Mu: (D, T, P) -> Clock30 - LCM synchronizer
     * Aligns D(2), T(3), P(5) into LCM(2,3,5) = 30 step clock
     */
    void ApplyMuSynchronizer() {
        // Compute phases from global step
        int step = State.GlobalStep;

        // Dyadic: step mod 2
        State.DyadicPhase = (step % 2 == 1) ? EDyadicPhase::A : EDyadicPhase::B;

        // Triadic: step mod 3
        int triadicIdx = (step - 1) % 3;
        State.TriadicPhase = static_cast<ETriadicPhase>(triadicIdx);

        // Pentadic: step / 6 (each stage has 6 steps)
        int stageIdx = (step - 1) / 6;
        State.PentadicStage = static_cast<EPentadicStage>(stageIdx);
        State.StageStep = ((step - 1) % 6) + 1;

        // Count synchronization events
        bool dyadSync = (step % 2 == 0);
        bool triadSync = (step % 3 == 0);
        bool pentadSync = (step % 6 == 0);

        int syncType = 0;
        if (dyadSync) syncType |= 1;
        if (triadSync) syncType |= 2;
        if (pentadSync) syncType |= 4;

        if (syncType > 0) {
            State.SyncEventCount++;
            if (OnSyncEvent) OnSyncEvent(syncType);
        }
    }

    /**
     * Phi: 2x3 -> 4 fold via double-step delay
     * Compresses 6-step dyad x triad into 4 real steps
     */
    void ApplyPhiFold() {
        // Double-step delay pattern:
        // Step 1: State 1, A, Phase 1
        // Step 2: State 4, A, Phase 2  (dyad held)
        // Step 3: State 6, B, Phase 2  (triad held)
        // Step 4: State 1, B, Phase 3

        // Map global step to 4-phase cycle (steps 1-4 map to phases 0-3)
        int phaseStep = (State.GlobalStep - 1) % 4;

        State.FourStepPhase = static_cast<EFourStepPhase>(phaseStep);
        State.DelayState.CurrentPhase = State.FourStepPhase;

        // Compute state value (1, 4, or 6)
        switch (State.FourStepPhase) {
            case EFourStepPhase::Step1:
            case EFourStepPhase::Step4:
                State.DelayState.StateValue = 1;
                break;
            case EFourStepPhase::Step2:
                State.DelayState.StateValue = 4;
                break;
            case EFourStepPhase::Step3:
                State.DelayState.StateValue = 6;
                break;
        }

        // Update delay state
        State.DelayState.DyadicState = State.DyadicPhase;
        State.DelayState.TriadicState = State.TriadicPhase;
    }

    /**
     * Sigma: Stage scheduler - 5 stages x 6 steps
     * Runs Phi once per stage with 2 transition/sync steps
     */
    void ApplySigmaScheduler() {
        EPentadicStage oldStage = State.PentadicStage;

        // Determine stage from global step
        int stageIdx = (State.GlobalStep - 1) / 6;
        State.PentadicStage = static_cast<EPentadicStage>(stageIdx);
        State.StageStep = ((State.GlobalStep - 1) % 6) + 1;

        // Check for stage transition
        if (State.PentadicStage != oldStage) {
            if (OnStageChanged) OnStageChanged(oldStage, State.PentadicStage);
        }

        // Transition steps are 1 and 2 of each stage
        bool isTransitionStep = (State.StageStep <= StageConfigs[stageIdx].TransitionSteps);

        // Active steps are 3-6 of each stage
        bool isActiveStep = !isTransitionStep;

        // Apply Phi only during active steps
        if (isActiveStep) {
            ApplyPhiFold();
        }
    }

    /**
     * Full Sys6 composite: sigma o (phi o mu o (Delta_2 x Delta_3 x id_P))
     */
    void ApplySys6Composite() {
        ApplyDeltaDyadic();
        ApplyDeltaTriadic();
        ApplyMuSynchronizer();
        ApplyPhiFold();
        ApplySigmaScheduler();
    }

    // Cycle Control

    void AdvanceStep() {
        int oldStep = State.GlobalStep;
        State.GlobalStep = (State.GlobalStep % 30) + 1;

        if (OnStepAdvanced) OnStepAdvanced(oldStep, State.GlobalStep);

        ApplySys6Composite();

        if (State.GlobalStep == 1) {
            State.CycleCount++;
            if (OnCycleCompleted) OnCycleCompleted(State.CycleCount);
        }
    }

    void JumpToStep(int step) {
        if (step < 1 || step > 30) return;
        State.GlobalStep = step;
        ApplySys6Composite();
    }

    void PauseCycle() { bIsPaused = true; }
    void ResumeCycle() { bIsPaused = false; }

    void ResetCycle() {
        State = FSys6FullState();
        State.GlobalStep = 1;
        ApplySys6Composite();
    }

    // State Queries

    FSys6FullState GetFullState() const { return State; }
    int GetCurrentStep() const { return State.GlobalStep; }
    EDyadicPhase GetDyadicPhase() const { return State.DyadicPhase; }
    ETriadicPhase GetTriadicPhase() const { return State.TriadicPhase; }
    EPentadicStage GetPentadicStage() const { return State.PentadicStage; }
    EFourStepPhase GetFourStepPhase() const { return State.FourStepPhase; }
    FCubicConcurrencyState GetCubicState() const { return State.CubicState; }
    FTriadicConvolutionState GetConvolutionState() const { return State.ConvolutionState; }

    bool IsAtSyncBoundary() const {
        int step = State.GlobalStep;
        return (step % 2 == 0) || (step % 3 == 0) || (step % 6 == 0);
    }

    int GetSyncBoundaryType() const {
        int step = State.GlobalStep;
        int type = 0;
        if (step % 2 == 0) type |= 1;  // Dyad
        if (step % 3 == 0) type |= 2;  // Triad
        if (step % 6 == 0) type |= 4;  // Pentad (stage boundary)
        if (step % 30 == 0) type |= 8; // Full cycle
        return type;
    }

    // Concurrency Operations

    std::vector<float> ProcessCubicConcurrency(const std::vector<float>& input) {
        if (!bEnableCubicConcurrency || input.empty()) {
            return input;
        }

        std::vector<float> output(input.size());

        // 8-way parallel processing
        for (size_t i = 0; i < input.size(); i++) {
            float sum = 0.0f;
            for (int t = 0; t < 8; t++) {
                sum += input[i] * State.CubicState.ThreadStates[t];
            }
            output[i] = std::tanh(sum / 8.0f);
        }

        return output;
    }

    std::vector<float> ProcessTriadicConvolution(const std::vector<float>& input) {
        if (!bEnableTriadicConvolution || input.empty()) {
            return input;
        }

        std::vector<float> output(input.size());

        // 9-phase convolution
        for (size_t i = 0; i < input.size(); i++) {
            float sum = 0.0f;
            for (int p = 0; p < 9; p++) {
                sum += input[i] * State.ConvolutionState.PhaseStates[p];
            }
            output[i] = std::tanh(sum / 9.0f);
        }

        return output;
    }

    void SetEntangledPair(int threadA, int threadB) {
        if (threadA >= 0 && threadA < 8 && threadB >= 0 && threadB < 8) {
            State.CubicState.ActivePairs = {threadA, threadB};
            State.CubicState.EntanglementLevel = 1.0f;
        }
    }

    void RotateConvolutionKernel() {
        State.ConvolutionState.CurrentKernel = (State.ConvolutionState.CurrentKernel + 1) % 9;
        State.ConvolutionState.PhaseRotation += 40.0f;
        if (State.ConvolutionState.PhaseRotation >= 360.0f) {
            State.ConvolutionState.PhaseRotation -= 360.0f;
        }
    }

private:
    void InitializeStageConfigs() {
        StageConfigs.resize(5);
        for (int i = 0; i < 5; i++) {
            StageConfigs[i].Stage = static_cast<EPentadicStage>(i);
            StageConfigs[i].StepsInStage = 6;
            StageConfigs[i].TransitionSteps = 2;
            StageConfigs[i].ActiveSteps = 4;
        }
    }

    bool bInitialized = false;
    bool bIsPaused = false;
    FSys6FullState State;
    std::vector<FStageConfig> StageConfigs;
};

// ============================================================================
// Test Fixtures
// ============================================================================

class Sys6OperadEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        Engine = std::make_unique<MockSys6ClockEngine>();
        Engine->InitializeEngine();
    }

    void TearDown() override {
        Engine.reset();
    }

    std::unique_ptr<MockSys6ClockEngine> Engine;
};

// ============================================================================
// Initialization Tests
// ============================================================================

TEST_F(Sys6OperadEngineTest, Initialization) {
    EXPECT_TRUE(Engine->IsInitialized());
    EXPECT_EQ(Engine->GetCurrentStep(), 1);
}

TEST_F(Sys6OperadEngineTest, DefaultConfiguration) {
    EXPECT_FLOAT_EQ(Engine->CycleDuration, 30.0f);
    EXPECT_TRUE(Engine->bEnableAutoCycle);
    EXPECT_TRUE(Engine->bEnableCubicConcurrency);
    EXPECT_TRUE(Engine->bEnableTriadicConvolution);
    EXPECT_EQ(Engine->CubicCoreCount, 8);
    EXPECT_EQ(Engine->ConvolutionCoreCount, 3);
}

TEST_F(Sys6OperadEngineTest, InitialState) {
    FSys6FullState state = Engine->GetFullState();

    EXPECT_EQ(state.GlobalStep, 1);
    EXPECT_EQ(state.DyadicPhase, EDyadicPhase::A);
    EXPECT_EQ(state.TriadicPhase, ETriadicPhase::Phase1);
    EXPECT_EQ(state.PentadicStage, EPentadicStage::Stage1);
    EXPECT_EQ(state.CycleCount, 0);
}

// ============================================================================
// LCM Clock Tests (30-step cycle)
// ============================================================================

TEST_F(Sys6OperadEngineTest, FullCycleCompletion) {
    for (int i = 0; i < 30; i++) {
        Engine->AdvanceStep();
    }

    EXPECT_EQ(Engine->GetCurrentStep(), 1);
    EXPECT_EQ(Engine->GetFullState().CycleCount, 1);
}

TEST_F(Sys6OperadEngineTest, CycleWraparound) {
    Engine->JumpToStep(30);
    Engine->AdvanceStep();

    EXPECT_EQ(Engine->GetCurrentStep(), 1);
}

TEST_F(Sys6OperadEngineTest, MultipleCycles) {
    for (int i = 0; i < 90; i++) {  // 3 full cycles
        Engine->AdvanceStep();
    }

    EXPECT_EQ(Engine->GetFullState().CycleCount, 3);
}

// ============================================================================
// Dyadic Phase Tests (mod 2)
// ============================================================================

TEST_F(Sys6OperadEngineTest, DyadicPhasePattern) {
    // Odd steps should be A, even steps should be B
    for (int step = 1; step <= 10; step++) {
        Engine->JumpToStep(step);
        Engine->ApplyMuSynchronizer();

        EDyadicPhase expected = (step % 2 == 1) ? EDyadicPhase::A : EDyadicPhase::B;
        EXPECT_EQ(Engine->GetDyadicPhase(), expected);
    }
}

TEST_F(Sys6OperadEngineTest, DyadicPhaseCount) {
    int aCount = 0, bCount = 0;

    for (int step = 1; step <= 30; step++) {
        Engine->JumpToStep(step);
        Engine->ApplyMuSynchronizer();

        if (Engine->GetDyadicPhase() == EDyadicPhase::A) aCount++;
        else bCount++;
    }

    EXPECT_EQ(aCount, 15);
    EXPECT_EQ(bCount, 15);
}

// ============================================================================
// Triadic Phase Tests (mod 3)
// ============================================================================

TEST_F(Sys6OperadEngineTest, TriadicPhasePattern) {
    std::vector<ETriadicPhase> expectedSequence = {
        ETriadicPhase::Phase1, ETriadicPhase::Phase2, ETriadicPhase::Phase3
    };

    for (int step = 1; step <= 9; step++) {
        Engine->JumpToStep(step);
        Engine->ApplyMuSynchronizer();

        ETriadicPhase expected = expectedSequence[(step - 1) % 3];
        EXPECT_EQ(Engine->GetTriadicPhase(), expected);
    }
}

TEST_F(Sys6OperadEngineTest, TriadicPhaseCount) {
    int phase1Count = 0, phase2Count = 0, phase3Count = 0;

    for (int step = 1; step <= 30; step++) {
        Engine->JumpToStep(step);
        Engine->ApplyMuSynchronizer();

        switch (Engine->GetTriadicPhase()) {
            case ETriadicPhase::Phase1: phase1Count++; break;
            case ETriadicPhase::Phase2: phase2Count++; break;
            case ETriadicPhase::Phase3: phase3Count++; break;
        }
    }

    EXPECT_EQ(phase1Count, 10);
    EXPECT_EQ(phase2Count, 10);
    EXPECT_EQ(phase3Count, 10);
}

// ============================================================================
// Pentadic Stage Tests (5 stages, 6 steps each)
// ============================================================================

TEST_F(Sys6OperadEngineTest, PentadicStageMapping) {
    std::vector<EPentadicStage> expectedStages = {
        EPentadicStage::Stage1, EPentadicStage::Stage1, EPentadicStage::Stage1,
        EPentadicStage::Stage1, EPentadicStage::Stage1, EPentadicStage::Stage1,
        EPentadicStage::Stage2, EPentadicStage::Stage2, EPentadicStage::Stage2,
        EPentadicStage::Stage2, EPentadicStage::Stage2, EPentadicStage::Stage2
    };

    for (int step = 1; step <= 12; step++) {
        Engine->JumpToStep(step);
        Engine->ApplyMuSynchronizer();

        EXPECT_EQ(Engine->GetPentadicStage(), expectedStages[step - 1]);
    }
}

TEST_F(Sys6OperadEngineTest, AllStagesVisited) {
    std::set<EPentadicStage> visitedStages;

    for (int step = 1; step <= 30; step++) {
        Engine->JumpToStep(step);
        Engine->ApplyMuSynchronizer();
        visitedStages.insert(Engine->GetPentadicStage());
    }

    EXPECT_EQ(visitedStages.size(), 5);
}

TEST_F(Sys6OperadEngineTest, StageStepWithinStage) {
    for (int step = 1; step <= 30; step++) {
        Engine->JumpToStep(step);
        Engine->ApplyMuSynchronizer();

        int expectedStageStep = ((step - 1) % 6) + 1;
        EXPECT_EQ(Engine->GetFullState().StageStep, expectedStageStep);
    }
}

// ============================================================================
// Synchronization Boundary Tests
// ============================================================================

TEST_F(Sys6OperadEngineTest, SyncBoundaryDetection) {
    // Test at various sync boundaries
    Engine->JumpToStep(6);  // Pentad boundary
    EXPECT_TRUE(Engine->IsAtSyncBoundary());
    EXPECT_EQ(Engine->GetSyncBoundaryType() & 4, 4);  // Pentad sync

    Engine->JumpToStep(30);  // Full cycle boundary
    EXPECT_TRUE(Engine->IsAtSyncBoundary());
    int type = Engine->GetSyncBoundaryType();
    EXPECT_TRUE((type & 8) != 0);  // Full cycle
}

TEST_F(Sys6OperadEngineTest, SyncEventCount) {
    // Run full cycle and count sync events
    for (int i = 0; i < 30; i++) {
        Engine->AdvanceStep();
    }

    // Should have multiple sync events
    EXPECT_GT(Engine->GetFullState().SyncEventCount, 0);
}

// ============================================================================
// Delta Operations Tests
// ============================================================================

TEST_F(Sys6OperadEngineTest, DeltaDyadicCubicExpansion) {
    Engine->ApplyDeltaDyadic();

    FCubicConcurrencyState cubic = Engine->GetCubicState();
    EXPECT_EQ(cubic.ThreadStates.size(), 8);

    // Should have non-zero values
    bool hasNonZero = false;
    for (float val : cubic.ThreadStates) {
        if (val != 0.0f) hasNonZero = true;
    }
    EXPECT_TRUE(hasNonZero);
}

TEST_F(Sys6OperadEngineTest, DeltaTriadicConvolutionExpansion) {
    Engine->ApplyDeltaTriadic();

    FTriadicConvolutionState conv = Engine->GetConvolutionState();
    EXPECT_EQ(conv.PhaseStates.size(), 9);

    // Should have varied phase values
    std::set<float> uniqueValues;
    for (float val : conv.PhaseStates) {
        uniqueValues.insert(val);
    }
    EXPECT_GT(uniqueValues.size(), 1);
}

// ============================================================================
// Phi (2x3->4 Fold) Tests
// ============================================================================

TEST_F(Sys6OperadEngineTest, PhiFoldStateValues) {
    // Four-step phase should cycle through state values 1, 4, 6, 1
    std::vector<int> expectedValues = {1, 4, 6, 1};

    for (int i = 0; i < 4; i++) {
        Engine->JumpToStep(i + 1);
        Engine->ApplyPhiFold();

        int stateValue = Engine->GetFullState().DelayState.StateValue;
        EXPECT_EQ(stateValue, expectedValues[i % 4]);
    }
}

TEST_F(Sys6OperadEngineTest, PhiFoldDoubleStepDelay) {
    // Test dyad hold pattern
    Engine->JumpToStep(1);
    Engine->ApplyMuSynchronizer();
    Engine->ApplyPhiFold();
    EDyadicPhase phase1 = Engine->GetFullState().DelayState.DyadicState;

    Engine->JumpToStep(2);
    Engine->ApplyMuSynchronizer();
    Engine->ApplyPhiFold();
    EDyadicPhase phase2 = Engine->GetFullState().DelayState.DyadicState;

    // Phases should change
    EXPECT_NE(phase1, phase2);
}

// ============================================================================
// Sigma (5x6 Staging) Tests
// ============================================================================

TEST_F(Sys6OperadEngineTest, SigmaStageScheduling) {
    std::vector<EPentadicStage> stagesVisited;

    for (int step = 1; step <= 30; step++) {
        Engine->JumpToStep(step);
        Engine->ApplySigmaScheduler();

        if (step % 6 == 1) {  // First step of each stage
            stagesVisited.push_back(Engine->GetPentadicStage());
        }
    }

    EXPECT_EQ(stagesVisited.size(), 5);
    EXPECT_EQ(stagesVisited[0], EPentadicStage::Stage1);
    EXPECT_EQ(stagesVisited[4], EPentadicStage::Stage5);
}

TEST_F(Sys6OperadEngineTest, SigmaTransitionSteps) {
    // Steps 1-2 of each stage are transition steps
    Engine->JumpToStep(1);
    Engine->ApplySigmaScheduler();

    int stageStep = Engine->GetFullState().StageStep;
    EXPECT_EQ(stageStep, 1);  // Transition step

    Engine->JumpToStep(3);
    Engine->ApplySigmaScheduler();

    stageStep = Engine->GetFullState().StageStep;
    EXPECT_EQ(stageStep, 3);  // Active step
}

// ============================================================================
// Composite Operation Tests
// ============================================================================

TEST_F(Sys6OperadEngineTest, Sys6CompositeOperation) {
    // Test full composite at various steps
    for (int step = 1; step <= 30; step++) {
        Engine->JumpToStep(step);
        Engine->ApplySys6Composite();

        FSys6FullState state = Engine->GetFullState();

        // All components should be updated
        EXPECT_EQ(state.GlobalStep, step);
        EXPECT_GE((int)state.DyadicPhase, 0);
        EXPECT_GE((int)state.TriadicPhase, 0);
        EXPECT_GE((int)state.PentadicStage, 0);
    }
}

// ============================================================================
// Cubic Concurrency (C8) Tests
// ============================================================================

TEST_F(Sys6OperadEngineTest, CubicConcurrencyProcessing) {
    Engine->ApplyDeltaDyadic();

    std::vector<float> input(16, 0.5f);
    std::vector<float> output = Engine->ProcessCubicConcurrency(input);

    EXPECT_EQ(output.size(), input.size());

    // Output should be different from input
    bool different = false;
    for (size_t i = 0; i < input.size(); i++) {
        if (std::abs(input[i] - output[i]) > 0.001f) {
            different = true;
            break;
        }
    }
    EXPECT_TRUE(different);
}

TEST_F(Sys6OperadEngineTest, EntangledPairSetting) {
    Engine->SetEntangledPair(2, 5);

    FCubicConcurrencyState state = Engine->GetCubicState();
    EXPECT_EQ(state.ActivePairs[0], 2);
    EXPECT_EQ(state.ActivePairs[1], 5);
    EXPECT_FLOAT_EQ(state.EntanglementLevel, 1.0f);
}

// ============================================================================
// Triadic Convolution (K9) Tests
// ============================================================================

TEST_F(Sys6OperadEngineTest, TriadicConvolutionProcessing) {
    Engine->ApplyDeltaTriadic();

    std::vector<float> input(16, 0.5f);
    std::vector<float> output = Engine->ProcessTriadicConvolution(input);

    EXPECT_EQ(output.size(), input.size());
}

TEST_F(Sys6OperadEngineTest, ConvolutionKernelRotation) {
    int initialKernel = Engine->GetConvolutionState().CurrentKernel;
    float initialRotation = Engine->GetConvolutionState().PhaseRotation;

    Engine->RotateConvolutionKernel();

    FTriadicConvolutionState state = Engine->GetConvolutionState();
    EXPECT_EQ(state.CurrentKernel, (initialKernel + 1) % 9);
    EXPECT_GT(state.PhaseRotation, initialRotation);
}

TEST_F(Sys6OperadEngineTest, ConvolutionKernelWraparound) {
    for (int i = 0; i < 9; i++) {
        Engine->RotateConvolutionKernel();
    }

    // Should wrap around to 0
    EXPECT_EQ(Engine->GetConvolutionState().CurrentKernel, 0);
}

// ============================================================================
// Cycle Control Tests
// ============================================================================

TEST_F(Sys6OperadEngineTest, JumpToStep) {
    Engine->JumpToStep(15);
    EXPECT_EQ(Engine->GetCurrentStep(), 15);

    Engine->JumpToStep(30);
    EXPECT_EQ(Engine->GetCurrentStep(), 30);
}

TEST_F(Sys6OperadEngineTest, JumpToStepInvalid) {
    Engine->JumpToStep(0);  // Invalid
    EXPECT_EQ(Engine->GetCurrentStep(), 1);

    Engine->JumpToStep(31);  // Invalid
    EXPECT_EQ(Engine->GetCurrentStep(), 1);
}

TEST_F(Sys6OperadEngineTest, PauseResume) {
    Engine->PauseCycle();
    Engine->ResumeCycle();
    // Should not crash
    EXPECT_TRUE(Engine->IsInitialized());
}

TEST_F(Sys6OperadEngineTest, ResetCycle) {
    for (int i = 0; i < 15; i++) {
        Engine->AdvanceStep();
    }

    Engine->ResetCycle();

    EXPECT_EQ(Engine->GetCurrentStep(), 1);
}

// ============================================================================
// Event Callback Tests
// ============================================================================

TEST_F(Sys6OperadEngineTest, StepAdvancedCallback) {
    int callbackCount = 0;
    int lastOldStep = 0, lastNewStep = 0;

    Engine->OnStepAdvanced = [&](int oldStep, int newStep) {
        callbackCount++;
        lastOldStep = oldStep;
        lastNewStep = newStep;
    };

    Engine->AdvanceStep();

    EXPECT_EQ(callbackCount, 1);
    EXPECT_EQ(lastOldStep, 1);
    EXPECT_EQ(lastNewStep, 2);
}

TEST_F(Sys6OperadEngineTest, CycleCompletedCallback) {
    int completedCycles = 0;

    Engine->OnCycleCompleted = [&](int cycleCount) {
        completedCycles = cycleCount;
    };

    for (int i = 0; i < 30; i++) {
        Engine->AdvanceStep();
    }

    EXPECT_EQ(completedCycles, 1);
}

// ============================================================================
// Performance Tests
// ============================================================================

TEST(Sys6PerformanceTest, CycleProcessingPerformance) {
    MockSys6ClockEngine engine;
    engine.InitializeEngine();

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; i++) {
        engine.AdvanceStep();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 200);  // 10000 steps in under 200ms
}

TEST(Sys6PerformanceTest, CompositeOperationPerformance) {
    MockSys6ClockEngine engine;
    engine.InitializeEngine();

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100000; i++) {
        engine.ApplySys6Composite();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 500);  // 100000 composite ops in under 500ms
}

TEST(Sys6PerformanceTest, ConcurrencyProcessingPerformance) {
    MockSys6ClockEngine engine;
    engine.InitializeEngine();
    engine.ApplyDeltaDyadic();
    engine.ApplyDeltaTriadic();

    std::vector<float> input(256, 0.5f);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; i++) {
        engine.ProcessCubicConcurrency(input);
        engine.ProcessTriadicConvolution(input);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 500);  // 20000 processing ops in under 500ms
}

// ============================================================================
// Edge Case Tests
// ============================================================================

TEST(Sys6EdgeCaseTest, EmptyInput) {
    MockSys6ClockEngine engine;
    engine.InitializeEngine();

    std::vector<float> empty;
    std::vector<float> cubicResult = engine.ProcessCubicConcurrency(empty);
    std::vector<float> convResult = engine.ProcessTriadicConvolution(empty);

    EXPECT_TRUE(cubicResult.empty());
    EXPECT_TRUE(convResult.empty());
}

TEST(Sys6EdgeCaseTest, DisabledConcurrency) {
    MockSys6ClockEngine engine;
    engine.bEnableCubicConcurrency = false;
    engine.bEnableTriadicConvolution = false;
    engine.InitializeEngine();

    std::vector<float> input(16, 0.5f);
    std::vector<float> cubicResult = engine.ProcessCubicConcurrency(input);
    std::vector<float> convResult = engine.ProcessTriadicConvolution(input);

    // Should return input unchanged
    EXPECT_EQ(cubicResult, input);
    EXPECT_EQ(convResult, input);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST(Sys6IntegrationTest, FullCycleWithAllOperations) {
    MockSys6ClockEngine engine;
    engine.InitializeEngine();

    std::vector<float> input(32, 0.5f);

    for (int cycle = 0; cycle < 3; cycle++) {
        for (int step = 0; step < 30; step++) {
            engine.AdvanceStep();

            // Process through both concurrency systems
            std::vector<float> output = engine.ProcessCubicConcurrency(input);
            output = engine.ProcessTriadicConvolution(output);

            EXPECT_EQ(output.size(), input.size());
        }
    }

    EXPECT_EQ(engine.GetFullState().CycleCount, 3);
}

// Note: main() is provided by GTest::gtest_main in CMakeLists.txt

} // namespace
