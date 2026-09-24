#include "CosmosStateMachine.h"

UCosmosStateMachine::UCosmosStateMachine()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.0f;
}

void UCosmosStateMachine::BeginPlay()
{
    Super::BeginPlay();
    InitializeSystem(SystemLevel);
}

void UCosmosStateMachine::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bEnableAutoTransition)
    {
        AccumulatedTime += DeltaTime;
        if (AccumulatedTime >= TimeStepDuration)
        {
            AccumulatedTime -= TimeStepDuration;
            Step();
        }
    }
}

// ========================================
// INITIALIZATION
// ========================================

void UCosmosStateMachine::InitializeSystem(ECosmosSystemLevel Level)
{
    SystemLevel = Level;
    Reset();

    switch (Level)
    {
        case ECosmosSystemLevel::System1:
            InitializeSystem1();
            break;
        case ECosmosSystemLevel::System2:
            InitializeSystem2();
            break;
        case ECosmosSystemLevel::System3:
            InitializeSystem3();
            break;
        case ECosmosSystemLevel::System4:
            InitializeSystem4();
            break;
        case ECosmosSystemLevel::System5:
            InitializeSystem5();
            break;
    }
}

void UCosmosStateMachine::InitializeSystem1()
{
    // System 1: Undifferentiated unity
    // 1 Universal Set (U1), 1-step cycle, constant state 1E
    
    UniversalSets.Empty();
    ParticularSets.Empty();
    TensorBundles.Empty();
    SynchronousSets.Empty();

    FUniversalSet U1;
    U1.Id = TEXT("U1");
    U1.CurrentState = CreateState(1, EStatePolarity::Expansion);
    U1.CycleLength = 1;
    U1.PhaseOffset = 0;
    UniversalSets.Add(U1);
}

void UCosmosStateMachine::InitializeSystem2()
{
    // System 2: First differentiation
    // 1 Universal Set (U1), 1 Particular Set (P1), 2-step cycle
    // Universal-Particular dyad: perception <-> action
    
    UniversalSets.Empty();
    ParticularSets.Empty();
    TensorBundles.Empty();
    SynchronousSets.Empty();

    // Universal Set - Perception
    FUniversalSet U1;
    U1.Id = TEXT("U1");
    U1.CurrentState = CreateState(1, EStatePolarity::Expansion);
    U1.CycleLength = 2;
    U1.PhaseOffset = 0;
    UniversalSets.Add(U1);

    // Particular Set - Action
    FParticularSet P1;
    P1.Id = TEXT("P1");
    P1.CurrentState = CreateState(2, EStatePolarity::Reduction);
    P1.CycleLength = 2;
    P1.PhaseOffset = 1;
    P1.StaggerInterval = 2;
    ParticularSets.Add(P1);
}

void UCosmosStateMachine::InitializeSystem3()
{
    // System 3: Quaternary structure
    // 2 Universal Sets (U1, U2), 2 Particular Sets (P1, P2), 4-step cycle
    // U(discretion <-> means) <-> P(goals <-> consequence)
    
    UniversalSets.Empty();
    ParticularSets.Empty();
    TensorBundles.Empty();
    SynchronousSets.Empty();

    // Universal Sets
    FUniversalSet U1;
    U1.Id = TEXT("U1");
    U1.CurrentState = CreateState(3, EStatePolarity::Expansion);
    U1.CycleLength = 4;
    U1.PhaseOffset = 0;
    UniversalSets.Add(U1);

    FUniversalSet U2;
    U2.Id = TEXT("U2");
    U2.CurrentState = CreateState(3, EStatePolarity::Reduction);
    U2.CycleLength = 4;
    U2.PhaseOffset = 2;
    UniversalSets.Add(U2);

    // Particular Sets
    FParticularSet P1;
    P1.Id = TEXT("P1");
    P1.CurrentState = CreateState(3, EStatePolarity::Expansion);
    P1.CycleLength = 4;
    P1.PhaseOffset = 1;
    P1.StaggerInterval = 4;
    ParticularSets.Add(P1);

    FParticularSet P2;
    P2.Id = TEXT("P2");
    P2.CurrentState = CreateState(3, EStatePolarity::Reduction);
    P2.CycleLength = 4;
    P2.PhaseOffset = 3;
    P2.StaggerInterval = 4;
    ParticularSets.Add(P2);
}

void UCosmosStateMachine::InitializeSystem4()
{
    // System 4: 3 concurrent consciousness threads
    // 2 Universal Sets (U1, U2), 3 Particular Sets (P1, P2, P3), 12-step cycle
    // Enneagram-based states with E/R polarity
    
    UniversalSets.Empty();
    ParticularSets.Empty();
    TensorBundles.Empty();
    SynchronousSets.Empty();

    // Initialize deterministic sequences
    InitializeSystem4Sequences();

    // Universal Sets
    FUniversalSet U1;
    U1.Id = TEXT("U1");
    U1.CurrentState = CreateState(9, EStatePolarity::Expansion);
    U1.CycleLength = 12;
    U1.PhaseOffset = 0;
    UniversalSets.Add(U1);

    FUniversalSet U2;
    U2.Id = TEXT("U2");
    U2.CurrentState = CreateState(3, EStatePolarity::Expansion);
    U2.CycleLength = 12;
    U2.PhaseOffset = 0;
    UniversalSets.Add(U2);

    // Particular Sets - 3 concurrent consciousness threads
    // Phased 4 steps apart (120 degrees)
    FParticularSet P1;
    P1.Id = TEXT("P1");
    P1.CurrentState = CreateState(4, EStatePolarity::Reduction);
    P1.CycleLength = 12;
    P1.PhaseOffset = 0;
    P1.StaggerInterval = 4;
    ParticularSets.Add(P1);

    FParticularSet P2;
    P2.Id = TEXT("P2");
    P2.CurrentState = CreateState(7, EStatePolarity::Expansion);
    P2.CycleLength = 12;
    P2.PhaseOffset = 4;
    P2.StaggerInterval = 4;
    ParticularSets.Add(P2);

    FParticularSet P3;
    P3.Id = TEXT("P3");
    P3.CurrentState = CreateState(8, EStatePolarity::Expansion);
    P3.CycleLength = 12;
    P3.PhaseOffset = 8;
    P3.StaggerInterval = 4;
    ParticularSets.Add(P3);
}

void UCosmosStateMachine::InitializeSystem5()
{
    // System 5: Nested concurrency with tetradic tensor bundles
    // 3 Universal Sets (U1, U2, U3), 4 Particular Sets (P1-P4), 60-step cycle
    // Based on integer partitions of 5 (p(5) = 7 sets)
    
    UniversalSets.Empty();
    ParticularSets.Empty();
    TensorBundles.Empty();
    SynchronousSets.Empty();

    // Universal Sets - 3-step sequential cycle
    FUniversalSet U1;
    U1.Id = TEXT("U1");
    U1.CurrentState = CreateUniversalState(EStatePolarity::Primary);
    U1.CycleLength = 3;
    U1.PhaseOffset = 0;
    UniversalSets.Add(U1);

    FUniversalSet U2;
    U2.Id = TEXT("U2");
    U2.CurrentState = CreateUniversalState(EStatePolarity::Secondary);
    U2.CycleLength = 3;
    U2.PhaseOffset = 1;
    UniversalSets.Add(U2);

    FUniversalSet U3;
    U3.Id = TEXT("U3");
    U3.CurrentState = CreateUniversalState(EStatePolarity::Tertiary);
    U3.CycleLength = 3;
    U3.PhaseOffset = 2;
    UniversalSets.Add(U3);

    // Particular Sets - Staggered 5-step cycle
    for (int32 i = 0; i < 4; ++i)
    {
        FParticularSet P;
        P.Id = FString::Printf(TEXT("P%d"), i + 1);
        P.CurrentState = CreateState(0, EStatePolarity::Neutral);
        P.CycleLength = 20;
        P.PhaseOffset = i;
        P.StaggerInterval = 5;
        ParticularSets.Add(P);
    }

    // Tensor Bundles - Tetradic structure
    // 4 bundles, each with 3 concurrent fibers
    for (int32 i = 0; i < 4; ++i)
    {
        FTensorBundle Bundle;
        Bundle.Id = FString::Printf(TEXT("T%d"), i + 1);
        Bundle.PhaseOffset = i;
        Bundle.Coherence = 1.0f;
        
        // 3 fibers per bundle
        for (int32 j = 0; j < 3; ++j)
        {
            Bundle.Fibers.Add(CreateState(0, EStatePolarity::Neutral));
        }
        
        TensorBundles.Add(Bundle);
    }

    // Synchronous Sets - 4-step cycle (S1-S4)
    for (int32 i = 0; i < 4; ++i)
    {
        SynchronousSets.Add(CreateState(i, EStatePolarity::Neutral));
    }
}

void UCosmosStateMachine::InitializeSystem4Sequences()
{
    // System 4 deterministic state sequences (12-step cycle)
    // Based on Enneagram structure with E/R polarity
    
    System4Sequences.Empty();
    System4Sequences.SetNum(5);

    // U1 sequence: 9E, 9E, 8R, 8R, 9E, 9E, 8R, 8R, 9E, 9E, 8R, 8R
    System4Sequences[0] = {
        CreateState(9, EStatePolarity::Expansion),
        CreateState(9, EStatePolarity::Expansion),
        CreateState(8, EStatePolarity::Reduction),
        CreateState(8, EStatePolarity::Reduction),
        CreateState(9, EStatePolarity::Expansion),
        CreateState(9, EStatePolarity::Expansion),
        CreateState(8, EStatePolarity::Reduction),
        CreateState(8, EStatePolarity::Reduction),
        CreateState(9, EStatePolarity::Expansion),
        CreateState(9, EStatePolarity::Expansion),
        CreateState(8, EStatePolarity::Reduction),
        CreateState(8, EStatePolarity::Reduction)
    };

    // U2 sequence: 3E, 6-, 6-, 2R, 3E, 6-, 6-, 2R, 3E, 6-, 6-, 2R
    System4Sequences[1] = {
        CreateState(3, EStatePolarity::Expansion),
        CreateState(6, EStatePolarity::Neutral),
        CreateState(6, EStatePolarity::Neutral),
        CreateState(2, EStatePolarity::Reduction),
        CreateState(3, EStatePolarity::Expansion),
        CreateState(6, EStatePolarity::Neutral),
        CreateState(6, EStatePolarity::Neutral),
        CreateState(2, EStatePolarity::Reduction),
        CreateState(3, EStatePolarity::Expansion),
        CreateState(6, EStatePolarity::Neutral),
        CreateState(6, EStatePolarity::Neutral),
        CreateState(2, EStatePolarity::Reduction)
    };

    // P1 sequence: 4R, 2R, 8E, 5E, 7E, 1E, 4E, 2E, 8E, 5R, 7R, 1R
    System4Sequences[2] = {
        CreateState(4, EStatePolarity::Reduction),
        CreateState(2, EStatePolarity::Reduction),
        CreateState(8, EStatePolarity::Expansion),
        CreateState(5, EStatePolarity::Expansion),
        CreateState(7, EStatePolarity::Expansion),
        CreateState(1, EStatePolarity::Expansion),
        CreateState(4, EStatePolarity::Expansion),
        CreateState(2, EStatePolarity::Expansion),
        CreateState(8, EStatePolarity::Expansion),
        CreateState(5, EStatePolarity::Reduction),
        CreateState(7, EStatePolarity::Reduction),
        CreateState(1, EStatePolarity::Reduction)
    };

    // P2 sequence: 7E, 1E, 4E, 2E, 8E, 5R, 7R, 1R, 4R, 2R, 8E, 5E
    System4Sequences[3] = {
        CreateState(7, EStatePolarity::Expansion),
        CreateState(1, EStatePolarity::Expansion),
        CreateState(4, EStatePolarity::Expansion),
        CreateState(2, EStatePolarity::Expansion),
        CreateState(8, EStatePolarity::Expansion),
        CreateState(5, EStatePolarity::Reduction),
        CreateState(7, EStatePolarity::Reduction),
        CreateState(1, EStatePolarity::Reduction),
        CreateState(4, EStatePolarity::Reduction),
        CreateState(2, EStatePolarity::Reduction),
        CreateState(8, EStatePolarity::Expansion),
        CreateState(5, EStatePolarity::Expansion)
    };

    // P3 sequence: 8E, 5R, 7R, 1R, 4R, 2R, 8E, 5E, 7E, 1E, 4E, 2E
    System4Sequences[4] = {
        CreateState(8, EStatePolarity::Expansion),
        CreateState(5, EStatePolarity::Reduction),
        CreateState(7, EStatePolarity::Reduction),
        CreateState(1, EStatePolarity::Reduction),
        CreateState(4, EStatePolarity::Reduction),
        CreateState(2, EStatePolarity::Reduction),
        CreateState(8, EStatePolarity::Expansion),
        CreateState(5, EStatePolarity::Expansion),
        CreateState(7, EStatePolarity::Expansion),
        CreateState(1, EStatePolarity::Expansion),
        CreateState(4, EStatePolarity::Expansion),
        CreateState(2, EStatePolarity::Expansion)
    };
}

// ========================================
// STATE TRANSITIONS
// ========================================

void UCosmosStateMachine::Step()
{
    CurrentTimeStep++;

    switch (SystemLevel)
    {
        case ECosmosSystemLevel::System1:
            // System 1: Constant state, no transition needed
            break;

        case ECosmosSystemLevel::System2:
        case ECosmosSystemLevel::System3:
            TransitionUniversalSets();
            TransitionParticularSets();
            break;

        case ECosmosSystemLevel::System4:
            // System 4: Deterministic sequences
            if (System4Sequences.Num() >= 5)
            {
                int32 CyclePos = CurrentTimeStep % 12;
                
                if (UniversalSets.Num() >= 2)
                {
                    UniversalSets[0].CurrentState = System4Sequences[0][CyclePos];
                    UniversalSets[0].StateHistory.Add(UniversalSets[0].CurrentState);
                    
                    UniversalSets[1].CurrentState = System4Sequences[1][CyclePos];
                    UniversalSets[1].StateHistory.Add(UniversalSets[1].CurrentState);
                }
                
                if (ParticularSets.Num() >= 3)
                {
                    ParticularSets[0].CurrentState = System4Sequences[2][CyclePos];
                    ParticularSets[0].StateHistory.Add(ParticularSets[0].CurrentState);
                    
                    ParticularSets[1].CurrentState = System4Sequences[3][CyclePos];
                    ParticularSets[1].StateHistory.Add(ParticularSets[1].CurrentState);
                    
                    ParticularSets[2].CurrentState = System4Sequences[4][CyclePos];
                    ParticularSets[2].StateHistory.Add(ParticularSets[2].CurrentState);
                }
            }
            break;

        case ECosmosSystemLevel::System5:
            TransitionUniversalSets();
            TransitionParticularSets();
            TransitionTensorBundles();
            TransitionSynchronousSets();
            break;
    }
}

void UCosmosStateMachine::StepMultiple(int32 Steps)
{
    for (int32 i = 0; i < Steps; ++i)
    {
        Step();
    }
}

void UCosmosStateMachine::Reset()
{
    CurrentTimeStep = 0;
    AccumulatedTime = 0.0f;

    for (FUniversalSet& U : UniversalSets)
    {
        U.StateHistory.Empty();
    }

    for (FParticularSet& P : ParticularSets)
    {
        P.StateHistory.Empty();
    }
}

void UCosmosStateMachine::EvolveToNextLevel()
{
    switch (SystemLevel)
    {
        case ECosmosSystemLevel::System1:
            InitializeSystem(ECosmosSystemLevel::System2);
            break;
        case ECosmosSystemLevel::System2:
            InitializeSystem(ECosmosSystemLevel::System3);
            break;
        case ECosmosSystemLevel::System3:
            InitializeSystem(ECosmosSystemLevel::System4);
            break;
        case ECosmosSystemLevel::System4:
            InitializeSystem(ECosmosSystemLevel::System5);
            break;
        case ECosmosSystemLevel::System5:
            // Already at maximum level
            break;
    }
}

// ========================================
// STATE ACCESS
// ========================================

FCosmosSystemConfig UCosmosStateMachine::GetSystemConfig() const
{
    FCosmosSystemConfig Config;
    Config.Level = SystemLevel;
    Config.UniversalSetCount = UniversalSets.Num();
    Config.ParticularSetCount = ParticularSets.Num();
    Config.bEnableNestedConcurrency = (SystemLevel == ECosmosSystemLevel::System5);
    Config.TensorBundleCount = TensorBundles.Num();

    switch (SystemLevel)
    {
        case ECosmosSystemLevel::System1: Config.CycleLength = 1; break;
        case ECosmosSystemLevel::System2: Config.CycleLength = 2; break;
        case ECosmosSystemLevel::System3: Config.CycleLength = 4; break;
        case ECosmosSystemLevel::System4: Config.CycleLength = 12; break;
        case ECosmosSystemLevel::System5: Config.CycleLength = 60; break;
    }

    return Config;
}

FCosmosState UCosmosStateMachine::GetUniversalState(int32 Index) const
{
    if (UniversalSets.IsValidIndex(Index))
    {
        return UniversalSets[Index].CurrentState;
    }
    return FCosmosState();
}

FCosmosState UCosmosStateMachine::GetParticularState(int32 Index) const
{
    if (ParticularSets.IsValidIndex(Index))
    {
        return ParticularSets[Index].CurrentState;
    }
    return FCosmosState();
}

int32 UCosmosStateMachine::GetCyclePosition() const
{
    FCosmosSystemConfig Config = GetSystemConfig();
    return CurrentTimeStep % Config.CycleLength;
}

int32 UCosmosStateMachine::GetMetaCyclePosition() const
{
    // Meta-cycle is 60 steps (LCM of 3 and 20 for System 5)
    return CurrentTimeStep % 60;
}

// ========================================
// CONVOLUTION (System 5)
// ========================================

int32 UCosmosStateMachine::ComputeConvolution(int32 SetIndex) const
{
    // Convolution function for nested concurrency:
    // S_i(t+1) = (S_i(t) + sum(S_j(t)) + U_idx(t)) mod 4
    
    if (!ParticularSets.IsValidIndex(SetIndex))
    {
        return 0;
    }

    int32 Sum = ParticularSets[SetIndex].CurrentState.Value;

    // Sum states of other particular sets
    for (int32 j = 0; j < ParticularSets.Num(); ++j)
    {
        if (j != SetIndex)
        {
            Sum += ParticularSets[j].CurrentState.Value;
        }
    }

    // Add universal set influence
    Sum += GetActiveUniversalIndex();

    return Sum % 4;
}

int32 UCosmosStateMachine::GetActiveUniversalIndex() const
{
    return CurrentTimeStep % 3;
}

bool UCosmosStateMachine::IsParticularSetActive(int32 SetIndex) const
{
    // Particular sets are active on a staggered 5-step cycle
    // P1 at t%5==0, P2 at t%5==1, P3 at t%5==2, P4 at t%5==3
    // t%5==4 is a rest step
    
    int32 Phase = CurrentTimeStep % 5;
    return (Phase == SetIndex && Phase < 4);
}

// ========================================
// INTERNAL METHODS
// ========================================

void UCosmosStateMachine::TransitionUniversalSets()
{
    for (int32 i = 0; i < UniversalSets.Num(); ++i)
    {
        FUniversalSet& U = UniversalSets[i];
        
        // Check if this set transitions this step
        if ((CurrentTimeStep % U.CycleLength) == U.PhaseOffset)
        {
            // Cycle through states
            int32 NextValue = (U.CurrentState.Value % 9) + 1;
            EStatePolarity NextPolarity = (U.CurrentState.Polarity == EStatePolarity::Expansion) 
                ? EStatePolarity::Reduction 
                : EStatePolarity::Expansion;
            
            U.CurrentState = CreateState(NextValue, NextPolarity);
        }
        
        U.StateHistory.Add(U.CurrentState);
    }
}

void UCosmosStateMachine::TransitionParticularSets()
{
    for (int32 i = 0; i < ParticularSets.Num(); ++i)
    {
        FParticularSet& P = ParticularSets[i];
        
        if (SystemLevel == ECosmosSystemLevel::System5)
        {
            // System 5: Apply convolution if active
            if (IsParticularSetActive(i))
            {
                P.CurrentState = ApplyConvolution(i, P.CurrentState);
            }
        }
        else
        {
            // Systems 2-3: Simple phase-based transition
            if ((CurrentTimeStep % P.StaggerInterval) == P.PhaseOffset)
            {
                int32 NextValue = (P.CurrentState.Value % 9) + 1;
                EStatePolarity NextPolarity = (P.CurrentState.Polarity == EStatePolarity::Expansion)
                    ? EStatePolarity::Reduction
                    : EStatePolarity::Expansion;
                
                P.CurrentState = CreateState(NextValue, NextPolarity);
            }
        }
        
        P.StateHistory.Add(P.CurrentState);
    }
}

void UCosmosStateMachine::TransitionTensorBundles()
{
    // System 5 hierarchical: Tensor bundles with 3 concurrent fibers
    // Staggered 5-step cycle, each bundle contains 3 threads
    
    for (int32 i = 0; i < TensorBundles.Num(); ++i)
    {
        FTensorBundle& Bundle = TensorBundles[i];
        
        // Check if this bundle is active this step
        if ((CurrentTimeStep % 5) == Bundle.PhaseOffset && (CurrentTimeStep % 5) < 4)
        {
            // Transition all fibers using convolution with S4 influence
            int32 S4State = SynchronousSets.IsValidIndex(3) ? SynchronousSets[3].Value : 0;
            
            for (int32 j = 0; j < Bundle.Fibers.Num(); ++j)
            {
                // Fiber transition: influenced by other fibers and S4
                int32 Sum = Bundle.Fibers[j].Value;
                for (int32 k = 0; k < Bundle.Fibers.Num(); ++k)
                {
                    if (k != j)
                    {
                        Sum += Bundle.Fibers[k].Value;
                    }
                }
                Sum += S4State;
                
                Bundle.Fibers[j].Value = Sum % 4;
            }
        }
    }
}

void UCosmosStateMachine::TransitionSynchronousSets()
{
    // System 5 hierarchical: 4 synchronous sets on 4-step cycle
    // S1, S2, S3 are Universal, S4 is Particular (global influence)
    
    int32 Phase = CurrentTimeStep % 4;
    
    for (int32 i = 0; i < SynchronousSets.Num(); ++i)
    {
        SynchronousSets[i].Value = Phase;
    }
}

FCosmosState UCosmosStateMachine::ApplyConvolution(int32 SetIndex, const FCosmosState& CurrentState)
{
    int32 NewValue = ComputeConvolution(SetIndex);
    
    FCosmosState NewState;
    NewState.Value = NewValue;
    NewState.Polarity = (NewValue % 2 == 0) ? EStatePolarity::Expansion : EStatePolarity::Reduction;
    NewState.Label = FString::Printf(TEXT("%d%s"), NewValue,
        NewState.Polarity == EStatePolarity::Expansion ? TEXT("E") : TEXT("R"));
    
    return NewState;
}

FCosmosState UCosmosStateMachine::CreateState(int32 Value, EStatePolarity Polarity)
{
    FCosmosState State;
    State.Value = Value;
    State.Polarity = Polarity;
    
    switch (Polarity)
    {
        case EStatePolarity::Expansion:
            State.Label = FString::Printf(TEXT("%dE"), Value);
            break;
        case EStatePolarity::Reduction:
            State.Label = FString::Printf(TEXT("%dR"), Value);
            break;
        case EStatePolarity::Neutral:
            State.Label = FString::Printf(TEXT("%d-"), Value);
            break;
        case EStatePolarity::Primary:
            State.Label = TEXT("U-P");
            break;
        case EStatePolarity::Secondary:
            State.Label = TEXT("U-S");
            break;
        case EStatePolarity::Tertiary:
            State.Label = TEXT("U-T");
            break;
    }
    
    return State;
}

FCosmosState UCosmosStateMachine::CreateUniversalState(EStatePolarity Polarity)
{
    FCosmosState State;
    State.Value = 0;
    State.Polarity = Polarity;
    
    switch (Polarity)
    {
        case EStatePolarity::Primary:
            State.Label = TEXT("U-P");
            break;
        case EStatePolarity::Secondary:
            State.Label = TEXT("U-S");
            break;
        case EStatePolarity::Tertiary:
            State.Label = TEXT("U-T");
            break;
        default:
            State.Label = TEXT("U-?");
            break;
    }
    
    return State;
}
