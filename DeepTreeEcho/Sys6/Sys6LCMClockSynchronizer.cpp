// Sys6LCMClockSynchronizer.cpp
// Implementation of 30-step LCM clock synchronizer for Deep Tree Echo

#include "Sys6LCMClockSynchronizer.h"
#include "../Echobeats/EchobeatsStreamEngine.h"
#include "../Core/Sys6OperadEngine.h"
#include "../Echobeats/TensionalCouplingDynamics.h"
#include "../Learning/PredictiveAdaptationEngine.h"
#include "Math/UnrealMathUtility.h"

// LCM(2,3,5) = 30
static const int32 LCM_CYCLE_LENGTH = 30;

// Double-step delay pattern lookup table
// Step -> (InternalState, DyadPhase, TriadState)
// Pattern repeats every 4 steps: (1,A,1) -> (4,A,2) -> (6,B,2) -> (1,B,3)
static const int32 DELAY_PATTERN_INTERNAL_STATE[4] = { 1, 4, 6, 1 };
static const int32 DELAY_PATTERN_DYAD[4] = { 0, 0, 1, 1 };  // 0=A, 1=B
static const int32 DELAY_PATTERN_TRIAD[4] = { 1, 2, 2, 3 };

USys6LCMClockSynchronizer::USys6LCMClockSynchronizer()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void USys6LCMClockSynchronizer::BeginPlay()
{
    Super::BeginPlay();
    FindComponentReferences();
    InitializeThreadStates();

    // Calculate step duration from tick rate
    StepDuration = 1.0f / ClockTickRate;

    // Initialize clock state
    ClockState.LCMStep = 0;
    ClockState.Timestamp = GetWorld()->GetTimeSeconds();
    UpdateClockState();
}

void USys6LCMClockSynchronizer::TickComponent(float DeltaTime, ELevelTick TickType,
                                               FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bIsPaused || !bAutoAdvance)
    {
        return;
    }

    // Accumulate time
    AccumulatedTime += DeltaTime;

    // Advance steps as needed
    while (AccumulatedTime >= StepDuration)
    {
        AccumulatedTime -= StepDuration;
        AdvanceStep();
    }
}

void USys6LCMClockSynchronizer::FindComponentReferences()
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        EchobeatsEngine = Owner->FindComponentByClass<UEchobeatsStreamEngine>();
        OperadEngine = Owner->FindComponentByClass<USys6OperadEngine>();
        CouplingDynamics = Owner->FindComponentByClass<UTensionalCouplingDynamics>();
        AdaptationEngine = Owner->FindComponentByClass<UPredictiveAdaptationEngine>();
    }
}

void USys6LCMClockSynchronizer::InitializeThreadStates()
{
    ConcurrencyState.ActiveThreads = ConcurrentThreadCount;
    ConcurrencyState.ThreadStates.Empty();

    // Initialize thread states with 120-degree phase separation
    // For 3 threads in 30-step cycle: offsets of 0, 10, 20
    for (int32 i = 0; i < ConcurrentThreadCount; ++i)
    {
        FThreadSyncState ThreadState;
        ThreadState.ThreadID = i;
        ThreadState.PhaseOffset = (i * LCM_CYCLE_LENGTH) / ConcurrentThreadCount;
        ThreadState.CurrentStep = ThreadState.PhaseOffset;
        ThreadState.bIsSynchronized = true;
        ThreadState.Drift = 0.0f;

        ConcurrencyState.ThreadStates.Add(ThreadState);
    }

    // Initialize convolution results
    ConcurrencyState.ConvolutionResults.SetNumZeroed(ConcurrentThreadCount * (ConcurrentThreadCount - 1) / 2);
    ConcurrencyState.TriadicCoherence = 1.0f;

    // Initialize entanglement state
    EntanglementState.Order = DefaultEntanglementOrder;
    EntanglementState.CoherenceLevel = 1.0f;
}

void USys6LCMClockSynchronizer::AdvanceStep()
{
    FLCMClockState OldState = ClockState;

    // Advance LCM step
    ClockState.LCMStep = (ClockState.LCMStep + 1) % LCM_CYCLE_LENGTH;
    ClockState.Timestamp = GetWorld()->GetTimeSeconds();

    // Check for cycle completion
    if (ClockState.LCMStep == 0 && OldState.LCMStep == LCM_CYCLE_LENGTH - 1)
    {
        OnLCMCycleComplete.Broadcast();
    }

    // Update all state
    UpdateClockState();
    UpdateDelayPattern();
    UpdateEntanglement();
    UpdateConcurrency();

    // Broadcast changes
    BroadcastStateChanges(OldState);

    // Notify connected components
    if (AdaptationEngine)
    {
        AdaptationEngine->ProcessSys6Step(ClockState.LCMStep);
    }
}

void USys6LCMClockSynchronizer::AdvanceSteps(int32 Count)
{
    for (int32 i = 0; i < Count; ++i)
    {
        AdvanceStep();
    }
}

void USys6LCMClockSynchronizer::SetStep(int32 Step)
{
    FLCMClockState OldState = ClockState;

    ClockState.LCMStep = Step % LCM_CYCLE_LENGTH;
    ClockState.Timestamp = GetWorld()->GetTimeSeconds();

    UpdateClockState();
    UpdateDelayPattern();
    UpdateEntanglement();
    UpdateConcurrency();

    BroadcastStateChanges(OldState);
}

void USys6LCMClockSynchronizer::ResetClock()
{
    SetStep(0);
    AccumulatedTime = 0.0f;
}

void USys6LCMClockSynchronizer::PauseClock()
{
    bIsPaused = true;
}

void USys6LCMClockSynchronizer::ResumeClock()
{
    bIsPaused = false;
}

bool USys6LCMClockSynchronizer::IsClockPaused() const
{
    return bIsPaused;
}

FLCMClockState USys6LCMClockSynchronizer::GetClockState() const
{
    return ClockState;
}

int32 USys6LCMClockSynchronizer::GetCurrentStep() const
{
    return ClockState.LCMStep;
}

ESys6DyadPhase USys6LCMClockSynchronizer::GetDyadPhase() const
{
    return ClockState.DyadPhase;
}

ESys6TriadState USys6LCMClockSynchronizer::GetTriadState() const
{
    return ClockState.TriadState;
}

ESys6PentadStage USys6LCMClockSynchronizer::GetPentadStage() const
{
    return ClockState.PentadStage;
}

FDoubleStepDelayState USys6LCMClockSynchronizer::GetDelayState() const
{
    return ClockState.DelayState;
}

FDoubleStepDelayState USys6LCMClockSynchronizer::GetDelayPatternForStep(int32 Step) const
{
    return ComputeDelayState(Step);
}

bool USys6LCMClockSynchronizer::ValidateDelayPattern(int32 Step, ESys6DyadPhase ExpectedDyad, ESys6TriadState ExpectedTriad) const
{
    FDoubleStepDelayState State = ComputeDelayState(Step);
    return (State.DyadPhase == ExpectedDyad && State.TriadState == ExpectedTriad);
}

TArray<FDoubleStepDelayState> USys6LCMClockSynchronizer::GetFullDelayPattern() const
{
    TArray<FDoubleStepDelayState> Pattern;

    for (int32 i = 0; i < 4; ++i)
    {
        FDoubleStepDelayState State;
        State.PatternStep = i + 1;
        State.InternalState = DELAY_PATTERN_INTERNAL_STATE[i];
        State.DyadPhase = (DELAY_PATTERN_DYAD[i] == 0) ? ESys6DyadPhase::PhaseA : ESys6DyadPhase::PhaseB;

        switch (DELAY_PATTERN_TRIAD[i])
        {
        case 1:
            State.TriadState = ESys6TriadState::State1;
            break;
        case 2:
            State.TriadState = ESys6TriadState::State2;
            break;
        case 3:
            State.TriadState = ESys6TriadState::State3;
            break;
        }

        Pattern.Add(State);
    }

    return Pattern;
}

FEntanglementState USys6LCMClockSynchronizer::GetEntanglementState() const
{
    return EntanglementState;
}

void USys6LCMClockSynchronizer::SetEntanglementOrder(EEntanglementOrder Order)
{
    EntanglementState.Order = Order;
    OnEntanglementChanged.Broadcast(EntanglementState);
}

void USys6LCMClockSynchronizer::CreateEntangledPair(int32 Thread1, int32 Thread2, int32 SharedAddress)
{
    if (!bEnableEntanglement)
    {
        return;
    }

    // Check if already entangled
    if (IsPairEntangled(Thread1, Thread2))
    {
        return;
    }

    // Add entangled pair
    EntanglementState.EntangledPairs.Add(FIntPoint(Thread1, Thread2));
    EntanglementState.SharedAddresses.Add(SharedAddress);

    // Set order to 2 (entangled)
    EntanglementState.Order = EEntanglementOrder::Order2_Entangled;

    OnEntanglementChanged.Broadcast(EntanglementState);
}

void USys6LCMClockSynchronizer::BreakEntanglement(int32 Thread1, int32 Thread2)
{
    for (int32 i = EntanglementState.EntangledPairs.Num() - 1; i >= 0; --i)
    {
        const FIntPoint& Pair = EntanglementState.EntangledPairs[i];
        if ((Pair.X == Thread1 && Pair.Y == Thread2) ||
            (Pair.X == Thread2 && Pair.Y == Thread1))
        {
            EntanglementState.EntangledPairs.RemoveAt(i);
            if (i < EntanglementState.SharedAddresses.Num())
            {
                EntanglementState.SharedAddresses.RemoveAt(i);
            }
        }
    }

    // Update order if no more entanglements
    if (EntanglementState.EntangledPairs.Num() == 0)
    {
        EntanglementState.Order = EEntanglementOrder::Order1_Normal;
    }

    OnEntanglementChanged.Broadcast(EntanglementState);
}

bool USys6LCMClockSynchronizer::IsPairEntangled(int32 Thread1, int32 Thread2) const
{
    for (const FIntPoint& Pair : EntanglementState.EntangledPairs)
    {
        if ((Pair.X == Thread1 && Pair.Y == Thread2) ||
            (Pair.X == Thread2 && Pair.Y == Thread1))
        {
            return true;
        }
    }
    return false;
}

FCubicConcurrencyState USys6LCMClockSynchronizer::GetConcurrencyState() const
{
    return ConcurrencyState;
}

FThreadSyncState USys6LCMClockSynchronizer::GetThreadState(int32 ThreadID) const
{
    if (ThreadID >= 0 && ThreadID < ConcurrencyState.ThreadStates.Num())
    {
        return ConcurrencyState.ThreadStates[ThreadID];
    }
    return FThreadSyncState();
}

void USys6LCMClockSynchronizer::SynchronizeThread(int32 ThreadID)
{
    if (ThreadID >= 0 && ThreadID < ConcurrencyState.ThreadStates.Num())
    {
        FThreadSyncState& State = ConcurrencyState.ThreadStates[ThreadID];
        State.CurrentStep = (ClockState.LCMStep + State.PhaseOffset) % LCM_CYCLE_LENGTH;
        State.bIsSynchronized = true;
        State.Drift = 0.0f;
    }
}

void USys6LCMClockSynchronizer::SynchronizeAllThreads()
{
    for (int32 i = 0; i < ConcurrencyState.ThreadStates.Num(); ++i)
    {
        SynchronizeThread(i);
    }
}

float USys6LCMClockSynchronizer::ComputePairwiseConvolution(int32 Thread1, int32 Thread2)
{
    if (Thread1 < 0 || Thread1 >= ConcurrencyState.ThreadStates.Num() ||
        Thread2 < 0 || Thread2 >= ConcurrencyState.ThreadStates.Num())
    {
        return 0.0f;
    }

    const FThreadSyncState& State1 = ConcurrencyState.ThreadStates[Thread1];
    const FThreadSyncState& State2 = ConcurrencyState.ThreadStates[Thread2];

    // Compute phase difference
    int32 PhaseDiff = FMath::Abs(State1.CurrentStep - State2.CurrentStep);
    PhaseDiff = FMath::Min(PhaseDiff, LCM_CYCLE_LENGTH - PhaseDiff);

    // Expected phase difference for orthogonal convolution
    int32 ExpectedDiff = LCM_CYCLE_LENGTH / ConcurrentThreadCount;

    // Convolution strength based on phase alignment
    float PhaseAlignment = 1.0f - (float)FMath::Abs(PhaseDiff - ExpectedDiff) / (float)ExpectedDiff;
    PhaseAlignment = FMath::Clamp(PhaseAlignment, 0.0f, 1.0f);

    // Boost if entangled
    if (IsPairEntangled(Thread1, Thread2))
    {
        PhaseAlignment = FMath::Min(1.0f, PhaseAlignment * 1.5f);
    }

    return PhaseAlignment;
}

float USys6LCMClockSynchronizer::ComputeTriadicConvolution()
{
    if (ConcurrencyState.ThreadStates.Num() < 3)
    {
        return 0.0f;
    }

    // Compute all pairwise convolutions
    float Conv01 = ComputePairwiseConvolution(0, 1);
    float Conv02 = ComputePairwiseConvolution(0, 2);
    float Conv12 = ComputePairwiseConvolution(1, 2);

    // Store results
    if (ConcurrencyState.ConvolutionResults.Num() >= 3)
    {
        ConcurrencyState.ConvolutionResults[0] = Conv01;
        ConcurrencyState.ConvolutionResults[1] = Conv02;
        ConcurrencyState.ConvolutionResults[2] = Conv12;
    }

    // Triadic coherence is geometric mean of pairwise convolutions
    float TriadicCoherence = FMath::Pow(Conv01 * Conv02 * Conv12, 1.0f / 3.0f);

    ConcurrencyState.TriadicCoherence = TriadicCoherence;

    return TriadicCoherence;
}

int32 USys6LCMClockSynchronizer::MapLCMToEchobeat(int32 LCMStep) const
{
    // 30 LCM steps map to 12 echobeat steps
    // Each echobeat step spans 2.5 LCM steps
    // LCM 0-2 -> Echobeat 1
    // LCM 3-4 -> Echobeat 2
    // etc.

    // Using floor division with rounding
    int32 EchobeatStep = ((LCMStep * 12) / LCM_CYCLE_LENGTH) + 1;
    return FMath::Clamp(EchobeatStep, 1, 12);
}

int32 USys6LCMClockSynchronizer::MapEchobeatToLCM(int32 EchobeatStep) const
{
    // Map echobeat step (1-12) to LCM step (0-29)
    // Returns the first LCM step of the echobeat
    int32 ClampedStep = FMath::Clamp(EchobeatStep, 1, 12);
    return ((ClampedStep - 1) * LCM_CYCLE_LENGTH) / 12;
}

int32 USys6LCMClockSynchronizer::GetStreamForLCMStep(int32 LCMStep) const
{
    // Determine which stream is primary for this LCM step
    // Streams are phased 10 steps apart (120 degrees)
    // Stream 0: steps 0-9
    // Stream 1: steps 10-19
    // Stream 2: steps 20-29

    return (LCMStep / 10) % ConcurrentThreadCount;
}

void USys6LCMClockSynchronizer::SynchronizeWithEchobeats()
{
    if (EchobeatsEngine)
    {
        int32 EchobeatStep = MapLCMToEchobeat(ClockState.LCMStep);
        // Would call EchobeatsEngine->SetStep(EchobeatStep);
    }
}

void USys6LCMClockSynchronizer::GetPrimeFactors(int32 Step, int32& OutPower2, int32& OutPower3, int32& OutPower5) const
{
    // Decompose step position into prime power components
    // For LCM(2,3,5)=30, we track position within each prime cycle

    OutPower2 = Step % 2;  // Position in 2-cycle (0 or 1)
    OutPower3 = Step % 3;  // Position in 3-cycle (0, 1, or 2)
    OutPower5 = Step % 5;  // Position in 5-cycle (0, 1, 2, 3, or 4)
}

void USys6LCMClockSynchronizer::DelegateToNestedConcurrency(int32 Power2Level)
{
    // Delegate to nested concurrency based on power of 2
    // This handles entanglement (2 parallel processes)

    if (Power2Level == 0)
    {
        // Single process mode
        EntanglementState.Order = EEntanglementOrder::Order1_Normal;
    }
    else
    {
        // Entangled mode
        EntanglementState.Order = EEntanglementOrder::Order2_Entangled;
    }
}

void USys6LCMClockSynchronizer::DelegateToNestedConvolution(int32 Power3Level)
{
    // Delegate to nested convolution based on power of 3
    // This handles triadic operations

    if (CouplingDynamics)
    {
        // Would call CouplingDynamics->SetTriadicLevel(Power3Level);
    }
}

void USys6LCMClockSynchronizer::DelegateToNestedTransformation(int32 Power5Level)
{
    // Delegate to nested transformation based on power of 5
    // This handles pentad stage operations

    if (OperadEngine)
    {
        // Would call OperadEngine->SetTransformationStage(Power5Level);
    }
}

void USys6LCMClockSynchronizer::UpdateClockState()
{
    int32 Step = ClockState.LCMStep;

    // Update cycle positions
    ClockState.DyadCycle = Step / 2;
    ClockState.TriadCycle = Step / 3;
    ClockState.PentadCycle = Step / 5;

    // Update phases
    ClockState.DyadPhase = ComputeDyadPhase(Step);
    ClockState.TriadState = ComputeTriadState(Step);
    ClockState.PentadStage = ComputePentadStage(Step);

    // Update delay state
    ClockState.DelayState = ComputeDelayState(Step);
}

void USys6LCMClockSynchronizer::UpdateDelayPattern()
{
    // The double-step delay pattern follows:
    // Step 1: (A, 1) - Dyad A, Triad 1
    // Step 2: (A, 2) - Dyad A, Triad 2 (Triad advances)
    // Step 3: (B, 2) - Dyad B, Triad 2 (Dyad advances)
    // Step 4: (B, 3) - Dyad B, Triad 3 (Triad advances)
    // Then repeats...

    // This creates the alternating double-step delay where
    // Dyad and Triad take turns advancing
}

void USys6LCMClockSynchronizer::UpdateEntanglement()
{
    if (!bEnableEntanglement)
    {
        return;
    }

    // Update coherence based on phase alignment
    float TotalCoherence = 0.0f;
    int32 PairCount = 0;

    for (const FIntPoint& Pair : EntanglementState.EntangledPairs)
    {
        float Convolution = ComputePairwiseConvolution(Pair.X, Pair.Y);
        TotalCoherence += Convolution;
        PairCount++;
    }

    if (PairCount > 0)
    {
        EntanglementState.CoherenceLevel = TotalCoherence / PairCount;
    }
    else
    {
        EntanglementState.CoherenceLevel = 1.0f;
    }
}

void USys6LCMClockSynchronizer::UpdateConcurrency()
{
    if (!bEnableCubicConcurrency)
    {
        return;
    }

    // Update thread states
    for (FThreadSyncState& State : ConcurrencyState.ThreadStates)
    {
        // Advance thread step
        State.CurrentStep = (ClockState.LCMStep + State.PhaseOffset) % LCM_CYCLE_LENGTH;

        // Check synchronization
        int32 ExpectedStep = (ClockState.LCMStep + State.PhaseOffset) % LCM_CYCLE_LENGTH;
        State.Drift = (float)(State.CurrentStep - ExpectedStep) / (float)LCM_CYCLE_LENGTH;
        State.bIsSynchronized = FMath::Abs(State.Drift) <= SyncTolerance;
    }

    // Compute triadic convolution
    ComputeTriadicConvolution();
}

void USys6LCMClockSynchronizer::BroadcastStateChanges(const FLCMClockState& OldState)
{
    // Broadcast step advance
    OnLCMStepAdvanced.Broadcast(ClockState);

    // Broadcast phase changes
    if (OldState.DyadPhase != ClockState.DyadPhase)
    {
        OnDyadPhaseChanged.Broadcast(OldState.DyadPhase, ClockState.DyadPhase);
    }

    if (OldState.TriadState != ClockState.TriadState)
    {
        OnTriadStateChanged.Broadcast(OldState.TriadState, ClockState.TriadState);
    }

    if (OldState.PentadStage != ClockState.PentadStage)
    {
        OnPentadStageChanged.Broadcast(OldState.PentadStage, ClockState.PentadStage);
    }
}

void USys6LCMClockSynchronizer::ComputePhaseFromStep(int32 Step)
{
    ClockState.DyadPhase = ComputeDyadPhase(Step);
    ClockState.TriadState = ComputeTriadState(Step);
    ClockState.PentadStage = ComputePentadStage(Step);
}

ESys6DyadPhase USys6LCMClockSynchronizer::ComputeDyadPhase(int32 Step) const
{
    // Use delay pattern for dyad phase
    int32 PatternStep = Step % 4;
    return (DELAY_PATTERN_DYAD[PatternStep] == 0) ? ESys6DyadPhase::PhaseA : ESys6DyadPhase::PhaseB;
}

ESys6TriadState USys6LCMClockSynchronizer::ComputeTriadState(int32 Step) const
{
    // Use delay pattern for triad state
    int32 PatternStep = Step % 4;
    int32 TriadValue = DELAY_PATTERN_TRIAD[PatternStep];

    switch (TriadValue)
    {
    case 1:
        return ESys6TriadState::State1;
    case 2:
        return ESys6TriadState::State2;
    case 3:
        return ESys6TriadState::State3;
    default:
        return ESys6TriadState::State1;
    }
}

ESys6PentadStage USys6LCMClockSynchronizer::ComputePentadStage(int32 Step) const
{
    // Pentad stage cycles every 5 steps
    int32 StageValue = (Step % 5) + 1;

    switch (StageValue)
    {
    case 1:
        return ESys6PentadStage::Stage1;
    case 2:
        return ESys6PentadStage::Stage2;
    case 3:
        return ESys6PentadStage::Stage3;
    case 4:
        return ESys6PentadStage::Stage4;
    case 5:
        return ESys6PentadStage::Stage5;
    default:
        return ESys6PentadStage::Stage1;
    }
}

FDoubleStepDelayState USys6LCMClockSynchronizer::ComputeDelayState(int32 Step) const
{
    FDoubleStepDelayState State;

    int32 PatternStep = Step % 4;
    State.PatternStep = PatternStep + 1;
    State.InternalState = DELAY_PATTERN_INTERNAL_STATE[PatternStep];
    State.DyadPhase = (DELAY_PATTERN_DYAD[PatternStep] == 0) ? ESys6DyadPhase::PhaseA : ESys6DyadPhase::PhaseB;

    switch (DELAY_PATTERN_TRIAD[PatternStep])
    {
    case 1:
        State.TriadState = ESys6TriadState::State1;
        break;
    case 2:
        State.TriadState = ESys6TriadState::State2;
        break;
    case 3:
        State.TriadState = ESys6TriadState::State3;
        break;
    }

    return State;
}
