// Sys6OperadEngine.cpp
// Implementation of sys6 operad architecture for Deep Tree Echo

#include "Sys6OperadEngine.h"
#include "Math/UnrealMathUtility.h"

USys6OperadEngine::USys6OperadEngine()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void USys6OperadEngine::BeginPlay()
{
    Super::BeginPlay();
    InitializeEngine();
}

void USys6OperadEngine::TickComponent(float DeltaTime, ELevelTick TickType,
                                       FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bIsPaused || !bEnableAutoCycle)
    {
        return;
    }

    // Update step timer
    StepTimer += DeltaTime;
    float StepDuration = CycleDuration / 30.0f;

    if (StepTimer >= StepDuration)
    {
        StepTimer -= StepDuration;
        AdvanceStep();
    }
}

void USys6OperadEngine::InitializeEngine()
{
    // Initialize state
    State.GlobalStep = 1;
    State.DyadicPhase = EDyadicPhase::A;
    State.TriadicPhase = ETriadicPhase::Phase1;
    State.PentadicStage = EPentadicStage::Stage1;
    State.StageStep = 1;
    State.FourStepPhase = EFourStepPhase::Step1;
    State.CycleCount = 0;
    State.SyncEventCount = 0;

    // Initialize cubic concurrency state (C8)
    State.CubicState.ThreadStates.SetNum(8);
    for (int32 i = 0; i < 8; ++i)
    {
        State.CubicState.ThreadStates[i] = 0.0f;
    }
    State.CubicState.ActivePairs = {0, 1};
    State.CubicState.EntanglementLevel = 0.0f;

    // Initialize triadic convolution state (K9)
    State.ConvolutionState.PhaseStates.SetNum(9);
    for (int32 i = 0; i < 9; ++i)
    {
        State.ConvolutionState.PhaseStates[i] = 0.0f;
    }
    State.ConvolutionState.CurrentKernel = 0;
    State.ConvolutionState.PhaseRotation = 0.0f;

    // Initialize double-step delay state
    State.DelayState.CurrentPhase = EFourStepPhase::Step1;
    State.DelayState.DyadicState = EDyadicPhase::A;
    State.DelayState.TriadicState = ETriadicPhase::Phase1;
    State.DelayState.StateValue = 1;
    State.DelayState.DyadHoldCounter = 0;

    // Initialize stage configurations
    InitializeStageConfigs();

    StepTimer = 0.0f;
    bIsPaused = false;
}

void USys6OperadEngine::InitializeStageConfigs()
{
    StageConfigs.Empty();

    // 5 stages, each with 6 steps (4 active + 2 transition)
    for (int32 i = 0; i < 5; ++i)
    {
        FStageConfig Config;
        Config.Stage = static_cast<EPentadicStage>(i);
        Config.StepsInStage = 6;
        Config.TransitionSteps = 2;
        Config.ActiveSteps = 4;
        StageConfigs.Add(Config);
    }
}

void USys6OperadEngine::AdvanceStep()
{
    int32 OldStep = State.GlobalStep;
    
    // Advance global step (1-30)
    State.GlobalStep = (State.GlobalStep % 30) + 1;

    // Check for cycle completion
    if (State.GlobalStep == 1 && OldStep == 30)
    {
        State.CycleCount++;
        State.SyncEventCount = 0;
        OnCycleCompleted.Broadcast(State.CycleCount);
    }

    // Apply the full sys6 composite operation
    ApplySys6Composite();

    // Fire step advanced event
    OnStepAdvanced.Broadcast(OldStep, State.GlobalStep);

    // Check and fire sync events
    CheckAndFireSyncEvents();
}

void USys6OperadEngine::ApplySys6Composite()
{
    // Sys6 := σ ∘ (φ ∘ μ ∘ (Δ_2 ⊗ Δ_3 ⊗ id_P))
    // Reading left-to-right: build concurrency + convolution, sync clocks, 
    // fold 2×3 into 4, then stage into 5×6 across 30

    // Step 1: Apply prime-power delegation (Δ_2 ⊗ Δ_3)
    ApplyDeltaDyadic();
    ApplyDeltaTriadic();

    // Step 2: Apply LCM synchronizer (μ)
    ApplyMuSynchronizer();

    // Step 3: Apply 2×3→4 fold (φ)
    ApplyPhiFold();

    // Step 4: Apply stage scheduler (σ)
    ApplySigmaScheduler();
}

void USys6OperadEngine::ApplyDeltaDyadic()
{
    // Δ_2: D → (D, C8)
    // Move 2³ = 8 into parallel concurrency state

    if (!bEnableCubicConcurrency)
    {
        return;
    }

    // Update cubic concurrency based on dyadic phase
    int32 BaseIndex = (State.DyadicPhase == EDyadicPhase::A) ? 0 : 4;

    // Activate 4 threads based on current dyadic phase
    for (int32 i = 0; i < 4; ++i)
    {
        int32 ThreadIndex = BaseIndex + i;
        if (ThreadIndex < State.CubicState.ThreadStates.Num())
        {
            State.CubicState.ThreadStates[ThreadIndex] = 1.0f;
        }
    }

    // Deactivate other threads
    for (int32 i = 0; i < 4; ++i)
    {
        int32 ThreadIndex = (BaseIndex == 0) ? (4 + i) : i;
        if (ThreadIndex < State.CubicState.ThreadStates.Num())
        {
            State.CubicState.ThreadStates[ThreadIndex] *= 0.9f; // Decay
        }
    }

    // Update entanglement based on thread pair overlap
    UpdateCubicConcurrency();
}

void USys6OperadEngine::ApplyDeltaTriadic()
{
    // Δ_3: T → (T, K9)
    // Move 3² = 9 into orthogonal convolution phases

    if (!bEnableTriadicConvolution)
    {
        return;
    }

    // Map triadic phase to convolution kernel bank
    int32 PhaseIndex = static_cast<int32>(State.TriadicPhase);
    int32 BaseKernel = PhaseIndex * 3; // Each phase activates 3 kernels

    // Activate kernels for current triadic phase
    for (int32 i = 0; i < 3; ++i)
    {
        int32 KernelIndex = (BaseKernel + i) % 9;
        if (KernelIndex < State.ConvolutionState.PhaseStates.Num())
        {
            State.ConvolutionState.PhaseStates[KernelIndex] = 1.0f;
        }
    }

    // Decay other kernels
    for (int32 i = 0; i < 9; ++i)
    {
        if (i < BaseKernel || i >= BaseKernel + 3)
        {
            if (i < State.ConvolutionState.PhaseStates.Num())
            {
                State.ConvolutionState.PhaseStates[i] *= 0.9f;
            }
        }
    }

    // Update convolution state
    UpdateTriadicConvolution();
}

void USys6OperadEngine::ApplyMuSynchronizer()
{
    // μ: (D, T, P) → Clock30
    // Align D, T, P into single clock domain based on LCM(2,3,5)=30

    int32 t = State.GlobalStep;

    // Compute phases from global step
    // dyadicPhase = t mod 2
    EDyadicPhase OldDyad = State.DyadicPhase;
    State.DyadicPhase = ((t - 1) % 2 == 0) ? EDyadicPhase::A : EDyadicPhase::B;
    
    if (OldDyad != State.DyadicPhase)
    {
        OnDyadChanged.Broadcast(OldDyad, State.DyadicPhase);
    }

    // triadicPhase = t mod 3
    ETriadicPhase OldTriad = State.TriadicPhase;
    int32 TriadIndex = (t - 1) % 3;
    State.TriadicPhase = static_cast<ETriadicPhase>(TriadIndex);
    
    if (OldTriad != State.TriadicPhase)
    {
        OnTriadChanged.Broadcast(OldTriad, State.TriadicPhase);
    }

    // pentadicStage = ceil(t/6)
    EPentadicStage OldStage = State.PentadicStage;
    int32 StageIndex = ((t - 1) / 6);
    State.PentadicStage = static_cast<EPentadicStage>(FMath::Clamp(StageIndex, 0, 4));
    
    if (OldStage != State.PentadicStage)
    {
        OnStageChanged.Broadcast(OldStage, State.PentadicStage);
    }

    // stageStep = ((t-1) mod 6) + 1
    State.StageStep = ((t - 1) % 6) + 1;
}

void USys6OperadEngine::ApplyPhiFold()
{
    // φ: 2×3 → 4 fold via double-step delay
    // Compresses naive 6-step dyad×triad multiplex into 4 real steps
    // by holding dyad for two consecutive steps while triad advances

    // Compute four-step phase: ((t-1) mod 4) + 1
    int32 t = State.GlobalStep;
    int32 FourStepIndex = ((t - 1) % 4);
    State.FourStepPhase = static_cast<EFourStepPhase>(FourStepIndex);

    // Apply the double-step delay pattern
    ComputeDoubleStepDelayState();
}

void USys6OperadEngine::ComputeDoubleStepDelayState()
{
    // The corrected alternating pattern:
    // Step 1: State 1, A, 1
    // Step 2: State 4, A, 2  (Dyad held, Triad advances)
    // Step 3: State 6, B, 2  (Dyad advances, Triad held)
    // Step 4: State 1, B, 3  (Dyad held, Triad advances)

    switch (State.FourStepPhase)
    {
    case EFourStepPhase::Step1:
        State.DelayState.StateValue = 1;
        State.DelayState.DyadicState = EDyadicPhase::A;
        State.DelayState.TriadicState = ETriadicPhase::Phase1;
        State.DelayState.DyadHoldCounter = 0;
        break;

    case EFourStepPhase::Step2:
        State.DelayState.StateValue = 4;
        State.DelayState.DyadicState = EDyadicPhase::A; // Held
        State.DelayState.TriadicState = ETriadicPhase::Phase2; // Advanced
        State.DelayState.DyadHoldCounter = 1;
        break;

    case EFourStepPhase::Step3:
        State.DelayState.StateValue = 6;
        State.DelayState.DyadicState = EDyadicPhase::B; // Advanced
        State.DelayState.TriadicState = ETriadicPhase::Phase2; // Held
        State.DelayState.DyadHoldCounter = 0;
        break;

    case EFourStepPhase::Step4:
        State.DelayState.StateValue = 1;
        State.DelayState.DyadicState = EDyadicPhase::B; // Held
        State.DelayState.TriadicState = ETriadicPhase::Phase3; // Advanced
        State.DelayState.DyadHoldCounter = 1;
        break;
    }

    State.DelayState.CurrentPhase = State.FourStepPhase;
}

int32 USys6OperadEngine::GetStateValueForPhase(EFourStepPhase Phase) const
{
    switch (Phase)
    {
    case EFourStepPhase::Step1: return 1;
    case EFourStepPhase::Step2: return 4;
    case EFourStepPhase::Step3: return 6;
    case EFourStepPhase::Step4: return 1;
    default: return 1;
    }
}

void USys6OperadEngine::ApplySigmaScheduler()
{
    // σ: Maps 30-step clock into 5 stages × 6 steps
    // Runs φ once per stage with 2 transition/sync steps

    // Check if we're in a transition step (steps 5-6 of each stage)
    bool bIsTransitionStep = (State.StageStep >= 5);

    if (bIsTransitionStep)
    {
        // During transition, synchronize between stages
        // This is where stage handoff occurs
        
        // Increase sync event count
        State.SyncEventCount++;
    }
}

void USys6OperadEngine::UpdateCubicConcurrency()
{
    // Update entanglement level based on active thread pairs
    if (State.CubicState.ActivePairs.Num() >= 2)
    {
        int32 ThreadA = State.CubicState.ActivePairs[0];
        int32 ThreadB = State.CubicState.ActivePairs[1];

        if (ThreadA < State.CubicState.ThreadStates.Num() &&
            ThreadB < State.CubicState.ThreadStates.Num())
        {
            // Entanglement = product of thread activations
            float EntanglementA = State.CubicState.ThreadStates[ThreadA];
            float EntanglementB = State.CubicState.ThreadStates[ThreadB];
            State.CubicState.EntanglementLevel = EntanglementA * EntanglementB;
        }
    }
}

void USys6OperadEngine::UpdateTriadicConvolution()
{
    // Update current kernel based on triadic phase
    int32 PhaseIndex = static_cast<int32>(State.TriadicPhase);
    State.ConvolutionState.CurrentKernel = PhaseIndex * 3;

    // Update phase rotation (120 degrees per phase)
    State.ConvolutionState.PhaseRotation = PhaseIndex * 120.0f;
}

void USys6OperadEngine::CheckAndFireSyncEvents()
{
    int32 t = State.GlobalStep;

    // Check for sync boundaries
    // Dyad boundary: every 2 steps
    bool bDyadBoundary = (t % 2 == 0);
    
    // Triad boundary: every 3 steps
    bool bTriadBoundary = (t % 3 == 0);
    
    // Pentad boundary: every 6 steps
    bool bPentadBoundary = (t % 6 == 0);

    // Fire sync events
    if (bDyadBoundary)
    {
        State.SyncEventCount++;
        OnSyncEvent.Broadcast(1); // Dyad sync
    }

    if (bTriadBoundary)
    {
        State.SyncEventCount++;
        OnSyncEvent.Broadcast(2); // Triad sync
    }

    if (bPentadBoundary)
    {
        State.SyncEventCount++;
        OnSyncEvent.Broadcast(3); // Pentad sync
    }

    // Multiple sync (LCM boundaries)
    if (bDyadBoundary && bTriadBoundary)
    {
        OnSyncEvent.Broadcast(4); // Dyad-Triad sync (every 6 steps)
    }

    if (bDyadBoundary && bPentadBoundary)
    {
        OnSyncEvent.Broadcast(5); // Dyad-Pentad sync
    }

    if (bTriadBoundary && bPentadBoundary)
    {
        OnSyncEvent.Broadcast(6); // Triad-Pentad sync
    }

    if (t == 30)
    {
        OnSyncEvent.Broadcast(7); // Full cycle sync
    }
}

void USys6OperadEngine::JumpToStep(int32 Step)
{
    if (Step < 1 || Step > 30)
    {
        return;
    }

    int32 OldStep = State.GlobalStep;
    State.GlobalStep = Step;
    ApplySys6Composite();
    OnStepAdvanced.Broadcast(OldStep, State.GlobalStep);
}

void USys6OperadEngine::PauseCycle()
{
    bIsPaused = true;
}

void USys6OperadEngine::ResumeCycle()
{
    bIsPaused = false;
}

void USys6OperadEngine::ResetCycle()
{
    InitializeEngine();
}

// State query implementations
FSys6FullState USys6OperadEngine::GetFullState() const
{
    return State;
}

int32 USys6OperadEngine::GetCurrentStep() const
{
    return State.GlobalStep;
}

EDyadicPhase USys6OperadEngine::GetDyadicPhase() const
{
    return State.DyadicPhase;
}

ETriadicPhase USys6OperadEngine::GetTriadicPhase() const
{
    return State.TriadicPhase;
}

EPentadicStage USys6OperadEngine::GetPentadicStage() const
{
    return State.PentadicStage;
}

EFourStepPhase USys6OperadEngine::GetFourStepPhase() const
{
    return State.FourStepPhase;
}

FCubicConcurrencyState USys6OperadEngine::GetCubicState() const
{
    return State.CubicState;
}

FTriadicConvolutionState USys6OperadEngine::GetConvolutionState() const
{
    return State.ConvolutionState;
}

bool USys6OperadEngine::IsAtSyncBoundary() const
{
    return GetSyncBoundaryType() > 0;
}

int32 USys6OperadEngine::GetSyncBoundaryType() const
{
    int32 t = State.GlobalStep;
    
    bool bDyad = (t % 2 == 0);
    bool bTriad = (t % 3 == 0);
    bool bPentad = (t % 6 == 0);

    if (bDyad && bTriad && bPentad) return 7; // All three
    if (bTriad && bPentad) return 6;
    if (bDyad && bPentad) return 5;
    if (bDyad && bTriad) return 4;
    if (bPentad) return 3;
    if (bTriad) return 2;
    if (bDyad) return 1;
    return 0;
}

TArray<float> USys6OperadEngine::ProcessCubicConcurrency(const TArray<float>& Input)
{
    TArray<float> Output;
    Output.SetNum(Input.Num());

    if (!bEnableCubicConcurrency)
    {
        Output = Input;
        return Output;
    }

    // Process through 8-way parallel (SIMD-like)
    int32 ChunkSize = FMath::Max(1, Input.Num() / CubicCoreCount);

    for (int32 Core = 0; Core < CubicCoreCount; ++Core)
    {
        int32 StartIdx = Core * ChunkSize;
        int32 EndIdx = (Core == CubicCoreCount - 1) ? Input.Num() : StartIdx + ChunkSize;

        float ThreadWeight = (Core < State.CubicState.ThreadStates.Num()) 
            ? State.CubicState.ThreadStates[Core] : 0.0f;

        for (int32 i = StartIdx; i < EndIdx && i < Input.Num(); ++i)
        {
            Output[i] = Input[i] * ThreadWeight;
        }
    }

    return Output;
}

TArray<float> USys6OperadEngine::ProcessTriadicConvolution(const TArray<float>& Input)
{
    TArray<float> Output;
    Output.SetNum(Input.Num());

    if (!bEnableTriadicConvolution)
    {
        Output = Input;
        return Output;
    }

    // Process through 9-phase convolution kernel bank
    // Use 3 rotating cores
    int32 CurrentKernel = State.ConvolutionState.CurrentKernel;

    for (int32 i = 0; i < Input.Num(); ++i)
    {
        float Sum = 0.0f;
        for (int32 k = 0; k < ConvolutionCoreCount; ++k)
        {
            int32 KernelIdx = (CurrentKernel + k) % 9;
            float KernelWeight = (KernelIdx < State.ConvolutionState.PhaseStates.Num())
                ? State.ConvolutionState.PhaseStates[KernelIdx] : 0.0f;
            Sum += Input[i] * KernelWeight;
        }
        Output[i] = Sum / ConvolutionCoreCount;
    }

    return Output;
}

void USys6OperadEngine::SetEntangledPair(int32 ThreadA, int32 ThreadB)
{
    if (ThreadA >= 0 && ThreadA < 8 && ThreadB >= 0 && ThreadB < 8 && ThreadA != ThreadB)
    {
        State.CubicState.ActivePairs = {ThreadA, ThreadB};
        UpdateCubicConcurrency();
    }
}

void USys6OperadEngine::RotateConvolutionKernel()
{
    State.ConvolutionState.CurrentKernel = (State.ConvolutionState.CurrentKernel + 1) % 9;
    State.ConvolutionState.PhaseRotation = FMath::Fmod(State.ConvolutionState.PhaseRotation + 40.0f, 360.0f);
}
