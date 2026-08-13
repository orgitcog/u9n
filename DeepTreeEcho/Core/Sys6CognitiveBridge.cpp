// Sys6CognitiveBridge.cpp
// Implementation of bridge between sys6 operad and 12-step cognitive cycle

#include "Sys6CognitiveBridge.h"
#include "Math/UnrealMathUtility.h"

USys6CognitiveBridge::USys6CognitiveBridge()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void USys6CognitiveBridge::BeginPlay()
{
    Super::BeginPlay();
    FindComponentReferences();
    InitializeBridge();
}

void USys6CognitiveBridge::TickComponent(float DeltaTime, ELevelTick TickType,
                                          FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bEnableAutoSync)
    {
        UpdateBridgeState();
    }
}

void USys6CognitiveBridge::FindComponentReferences()
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        Sys6Engine = Owner->FindComponentByClass<USys6OperadEngine>();
        CognitiveManager = Owner->FindComponentByClass<UCognitiveCycleManager>();

        // Bind to sys6 step events
        if (Sys6Engine)
        {
            Sys6Engine->OnStepAdvanced.AddDynamic(this, &USys6CognitiveBridge::HandleSys6StepAdvanced);
        }

        // Bind to cognitive step events
        if (CognitiveManager)
        {
            CognitiveManager->OnStepChanged.AddDynamic(this, &USys6CognitiveBridge::HandleCognitiveStepChanged);
        }
    }
}

void USys6CognitiveBridge::InitializeBridge()
{
    State.Sys6Step = 1;
    State.CognitiveStep = 1;
    State.MappingMode = MappingMode;
    State.CurrentShellLevel = 1;
    State.BridgeCoherence = 1.0f;
    State.PhaseAlignment = 1.0f;

    InitializeShellConfigs();
    InitializeThreadPermutations();
}

void USys6CognitiveBridge::InitializeShellConfigs()
{
    ShellConfigs.Empty();

    // OEIS A000081 nested shell configuration
    // N=1: 1 term, 1 step apart
    FNestedShellConfig Shell1;
    Shell1.NestingLevel = 1;
    Shell1.TermCount = 1;
    Shell1.StepsApart = 1;
    Shell1.ActiveStreams = {1};
    ShellConfigs.Add(Shell1);

    // N=2: 2 terms, 2 steps apart
    FNestedShellConfig Shell2;
    Shell2.NestingLevel = 2;
    Shell2.TermCount = 2;
    Shell2.StepsApart = 2;
    Shell2.ActiveStreams = {1, 2};
    ShellConfigs.Add(Shell2);

    // N=3: 4 terms, 3 steps apart
    FNestedShellConfig Shell3;
    Shell3.NestingLevel = 3;
    Shell3.TermCount = 4;
    Shell3.StepsApart = 3;
    Shell3.ActiveStreams = {1, 2, 3};
    ShellConfigs.Add(Shell3);

    // N=4: 9 terms, 4 steps apart
    FNestedShellConfig Shell4;
    Shell4.NestingLevel = 4;
    Shell4.TermCount = 9;
    Shell4.StepsApart = 4;
    Shell4.ActiveStreams = {1, 2, 3};
    ShellConfigs.Add(Shell4);
}

void USys6CognitiveBridge::InitializeThreadPermutations()
{
    // Thread pair permutation sequence P(i,j)
    // P(1,2)→P(1,3)→P(1,4)→P(2,3)→P(2,4)→P(3,4)
    ThreadPairSequence.Empty();
    ThreadPairSequence.Add({1, 2});
    ThreadPairSequence.Add({1, 3});
    ThreadPairSequence.Add({1, 4});
    ThreadPairSequence.Add({2, 3});
    ThreadPairSequence.Add({2, 4});
    ThreadPairSequence.Add({3, 4});

    // Complementary triad MP1 sequence
    // P[1,2,3]→P[1,2,4]→P[1,3,4]→P[2,3,4]
    TriadMP1Sequence.Empty();
    TriadMP1Sequence.Add({1, 2, 3});
    TriadMP1Sequence.Add({1, 2, 4});
    TriadMP1Sequence.Add({1, 3, 4});
    TriadMP1Sequence.Add({2, 3, 4});

    // Complementary triad MP2 sequence (offset)
    // P[1,3,4]→P[2,3,4]→P[1,2,3]→P[1,2,4]
    TriadMP2Sequence.Empty();
    TriadMP2Sequence.Add({1, 3, 4});
    TriadMP2Sequence.Add({2, 3, 4});
    TriadMP2Sequence.Add({1, 2, 3});
    TriadMP2Sequence.Add({1, 2, 4});

    // Initialize thread state
    State.ThreadState.PermutationIndex = 0;
    State.ThreadState.CurrentPair = ThreadPairSequence[0];
    State.ThreadState.TriadMP1 = TriadMP1Sequence[0];
    State.ThreadState.TriadMP2 = TriadMP2Sequence[0];
}

void USys6CognitiveBridge::UpdateBridgeState()
{
    // Get current steps from components
    if (Sys6Engine)
    {
        State.Sys6Step = Sys6Engine->GetCurrentStep();
    }

    if (CognitiveManager)
    {
        State.CognitiveStep = CognitiveManager->GetCurrentStep();
    }

    // Update shell level
    int32 OldShellLevel = State.CurrentShellLevel;
    State.CurrentShellLevel = ComputeShellLevel(State.CognitiveStep);

    if (OldShellLevel != State.CurrentShellLevel)
    {
        OnShellTransition.Broadcast(OldShellLevel, State.CurrentShellLevel);
    }

    // Compute coherence and alignment
    ComputeBridgeCoherence();
    ComputePhaseAlignment();
}

void USys6CognitiveBridge::Synchronize()
{
    if (!Sys6Engine || !CognitiveManager)
    {
        return;
    }

    // Map sys6 step to cognitive step
    int32 MappedCognitiveStep = MapSys6ToCognitive(State.Sys6Step);

    // Apply synchronization with strength
    int32 CurrentCognitiveStep = CognitiveManager->GetCurrentStep();
    if (MappedCognitiveStep != CurrentCognitiveStep)
    {
        // Blend toward mapped step
        float Diff = static_cast<float>(MappedCognitiveStep - CurrentCognitiveStep);
        int32 AdjustedStep = CurrentCognitiveStep + FMath::RoundToInt(Diff * SyncStrength);
        AdjustedStep = FMath::Clamp(AdjustedStep, 1, 12);

        if (AdjustedStep != CurrentCognitiveStep)
        {
            CognitiveManager->JumpToStep(AdjustedStep);
        }
    }

    OnBridgeSync.Broadcast(State.Sys6Step, State.CognitiveStep);
}

int32 USys6CognitiveBridge::MapSys6ToCognitive(int32 Sys6Step) const
{
    switch (State.MappingMode)
    {
    case ESys6MappingMode::Direct:
        return DirectMapping(Sys6Step);
    case ESys6MappingMode::Interleaved:
        return InterleavedMapping(Sys6Step);
    case ESys6MappingMode::Hierarchical:
    default:
        return HierarchicalMapping(Sys6Step);
    }
}

int32 USys6CognitiveBridge::MapCognitiveToSys6(int32 CognitiveStep) const
{
    // Inverse mapping: 12 → 30
    // For hierarchical mode, use the shell-based mapping
    
    int32 ShellLevel = ComputeShellLevel(CognitiveStep);
    
    if (ShellLevel < ShellConfigs.Num())
    {
        const FNestedShellConfig& Shell = ShellConfigs[ShellLevel];
        // Map based on steps apart
        return ((CognitiveStep - 1) * Shell.StepsApart) % 30 + 1;
    }

    // Default: direct modular mapping
    return ((CognitiveStep - 1) * 5 / 2) % 30 + 1;
}

int32 USys6CognitiveBridge::DirectMapping(int32 Sys6Step) const
{
    // Direct: 30 → 12 via modular arithmetic
    // Map 30 steps to 12 steps: (step * 12 / 30) + 1
    // Simplified: (step * 2 / 5) + 1
    return ((Sys6Step - 1) * 12 / 30) + 1;
}

int32 USys6CognitiveBridge::InterleavedMapping(int32 Sys6Step) const
{
    // Interleaved: 5 stages × 6 steps → 12 cognitive steps
    // Each stage maps to ~2.4 cognitive steps
    // Stage step (1-6) maps to cognitive step within stage

    int32 Stage = ((Sys6Step - 1) / 6); // 0-4
    int32 StageStep = ((Sys6Step - 1) % 6) + 1; // 1-6

    // Map stage step to cognitive step
    // Steps 1-4 are active (map to cognitive steps)
    // Steps 5-6 are transition (hold cognitive step)
    
    if (StageStep <= 4)
    {
        // Active step: map to cognitive step
        int32 BaseStep = (Stage * 2) + 1; // Stages contribute 2 steps each
        int32 Offset = (StageStep - 1) / 2; // 0 or 1
        return FMath::Clamp(BaseStep + Offset, 1, 12);
    }
    else
    {
        // Transition step: hold at stage boundary
        int32 BaseStep = (Stage * 2) + 2;
        return FMath::Clamp(BaseStep, 1, 12);
    }
}

int32 USys6CognitiveBridge::HierarchicalMapping(int32 Sys6Step) const
{
    // Hierarchical: Nested shell mapping based on OEIS A000081
    // Uses the 5/7 twin prime relationship with mean of 6
    
    // Determine which shell level based on step
    int32 ShellLevel = 1;
    
    // Shell boundaries based on term counts (1, 2, 4, 9)
    if (Sys6Step <= 1) ShellLevel = 1;
    else if (Sys6Step <= 3) ShellLevel = 2;  // 1 + 2 = 3
    else if (Sys6Step <= 7) ShellLevel = 3;  // 3 + 4 = 7
    else if (Sys6Step <= 16) ShellLevel = 4; // 7 + 9 = 16
    else ShellLevel = 4; // Wrap around

    // Map within shell
    if (ShellLevel <= ShellConfigs.Num())
    {
        const FNestedShellConfig& Shell = ShellConfigs[ShellLevel - 1];
        
        // Cognitive step based on shell's steps apart
        int32 ShellOffset = 0;
        for (int32 i = 0; i < ShellLevel - 1 && i < ShellConfigs.Num(); ++i)
        {
            ShellOffset += ShellConfigs[i].TermCount;
        }
        
        int32 StepWithinShell = Sys6Step - ShellOffset;
        int32 CognitiveStep = ((StepWithinShell - 1) * Shell.StepsApart) % 12 + 1;
        
        return FMath::Clamp(CognitiveStep, 1, 12);
    }

    // Fallback to direct mapping
    return DirectMapping(Sys6Step);
}

FNestedShellConfig USys6CognitiveBridge::GetCurrentShell() const
{
    if (State.CurrentShellLevel >= 1 && State.CurrentShellLevel <= ShellConfigs.Num())
    {
        return ShellConfigs[State.CurrentShellLevel - 1];
    }
    return FNestedShellConfig();
}

void USys6CognitiveBridge::AdvanceThreadPermutation()
{
    // Advance through permutation sequence
    State.ThreadState.PermutationIndex = (State.ThreadState.PermutationIndex + 1) % ThreadPairSequence.Num();
    
    // Update current pair
    State.ThreadState.CurrentPair = ThreadPairSequence[State.ThreadState.PermutationIndex];

    // Update triads (cycle through 4 triads)
    int32 TriadIndex = State.ThreadState.PermutationIndex % TriadMP1Sequence.Num();
    State.ThreadState.TriadMP1 = TriadMP1Sequence[TriadIndex];
    State.ThreadState.TriadMP2 = TriadMP2Sequence[TriadIndex];

    // Update sys6 engine entanglement if available
    if (Sys6Engine && State.ThreadState.CurrentPair.Num() >= 2)
    {
        Sys6Engine->SetEntangledPair(
            State.ThreadState.CurrentPair[0] - 1,  // Convert to 0-indexed
            State.ThreadState.CurrentPair[1] - 1
        );
    }

    OnThreadPermutation.Broadcast(State.ThreadState.PermutationIndex);
}

FSys6BridgeState USys6CognitiveBridge::GetBridgeState() const
{
    return State;
}

int32 USys6CognitiveBridge::GetCurrentShellLevel() const
{
    return State.CurrentShellLevel;
}

float USys6CognitiveBridge::GetBridgeCoherence() const
{
    return State.BridgeCoherence;
}

bool USys6CognitiveBridge::IsAtShellBoundary() const
{
    // Shell boundaries at steps 1, 3, 7 (cumulative term counts)
    return (State.CognitiveStep == 1 || State.CognitiveStep == 3 || 
            State.CognitiveStep == 7 || State.CognitiveStep == 12);
}

int32 USys6CognitiveBridge::ComputeShellLevel(int32 Step) const
{
    // Determine shell level based on cognitive step
    // Using triadic groupings: {1,5,9}, {2,6,10}, {3,7,11}, {4,8,12}
    
    int32 TriadicGroup = ((Step - 1) % 4);
    
    // Map triadic group to shell level
    // Group 0 (steps 1,5,9): Shell 1
    // Group 1 (steps 2,6,10): Shell 2
    // Group 2 (steps 3,7,11): Shell 3
    // Group 3 (steps 4,8,12): Shell 4
    
    return TriadicGroup + 1;
}

void USys6CognitiveBridge::ComputeBridgeCoherence()
{
    // Coherence based on alignment between sys6 and cognitive cycles
    
    int32 ExpectedCognitive = MapSys6ToCognitive(State.Sys6Step);
    int32 ActualCognitive = State.CognitiveStep;

    // Compute distance (circular)
    int32 Diff = FMath::Abs(ExpectedCognitive - ActualCognitive);
    Diff = FMath::Min(Diff, 12 - Diff);

    // Coherence decreases with distance
    State.BridgeCoherence = 1.0f - (static_cast<float>(Diff) / 6.0f);
    State.BridgeCoherence = FMath::Clamp(State.BridgeCoherence, 0.0f, 1.0f);
}

void USys6CognitiveBridge::ComputePhaseAlignment()
{
    // Phase alignment based on sys6 phases matching cognitive mode
    
    if (!Sys6Engine || !CognitiveManager)
    {
        State.PhaseAlignment = 0.5f;
        return;
    }

    // Get sys6 phases
    EDyadicPhase DyadPhase = Sys6Engine->GetDyadicPhase();
    ETriadicPhase TriadPhase = Sys6Engine->GetTriadicPhase();

    // Get cognitive mode
    ECognitiveModeType CogMode = CognitiveManager->GetCurrentMode();

    // Alignment rules:
    // Dyad A + Expressive = good alignment
    // Dyad B + Reflective = good alignment
    // Triad Phase 1 + Perceive/Orient = good
    // Triad Phase 2 + Integrate/Decide = good
    // Triad Phase 3 + Act/Observe = good

    float DyadAlignment = 0.5f;
    if ((DyadPhase == EDyadicPhase::A && CogMode == ECognitiveModeType::Expressive) ||
        (DyadPhase == EDyadicPhase::B && CogMode == ECognitiveModeType::Reflective))
    {
        DyadAlignment = 1.0f;
    }

    // Simplified triad alignment
    float TriadAlignment = 0.5f + (static_cast<int32>(TriadPhase) * 0.1f);

    State.PhaseAlignment = (DyadAlignment + TriadAlignment) / 2.0f;
}

void USys6CognitiveBridge::HandleSys6StepAdvanced(int32 OldStep, int32 NewStep)
{
    State.Sys6Step = NewStep;

    // Check for shell transition
    if (IsAtShellBoundary())
    {
        AdvanceThreadPermutation();
    }

    // Auto-sync if enabled
    if (bEnableAutoSync)
    {
        Synchronize();
    }
}

void USys6CognitiveBridge::HandleCognitiveStepChanged(int32 OldStep, int32 NewStep)
{
    State.CognitiveStep = NewStep;
    UpdateBridgeState();
}
