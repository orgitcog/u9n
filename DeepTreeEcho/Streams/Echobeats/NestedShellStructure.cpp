// NestedShellStructure.cpp
// Implementation of OEIS A000081 nested shell structure

#include "NestedShellStructure.h"
#include "Math/UnrealMathUtility.h"

UNestedShellStructure::UNestedShellStructure()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UNestedShellStructure::BeginPlay()
{
    Super::BeginPlay();
    InitializeShellStructure();
}

void UNestedShellStructure::TickComponent(float DeltaTime, ELevelTick TickType,
                                           FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bEnableAutoPropagation)
    {
        return;
    }

    PropagationTimer += DeltaTime;
    float PropagationInterval = 1.0f / PropagationRate;

    if (PropagationTimer >= PropagationInterval)
    {
        PropagationTimer -= PropagationInterval;

        // Propagate based on current direction
        switch (State.PropagationDirection)
        {
        case EShellPropagationDirection::OuterToInner:
            PropagateOuterToInner();
            break;
        case EShellPropagationDirection::InnerToOuter:
            PropagateInnerToOuter();
            break;
        case EShellPropagationDirection::Bidirectional:
            PropagateBidirectional();
            break;
        }

        ComputeShellCoherence();
    }
}

void UNestedShellStructure::InitializeShellStructure()
{
    State.Terms.Empty();
    State.ShellCoherence = 1.0f;
    State.CurrentEchobeatStep = 1;
    State.ActiveLevel = EShellNestingLevel::Level1;
    State.PropagationDirection = EShellPropagationDirection::Bidirectional;

    BuildTreeStructure();
}

void UNestedShellStructure::BuildTreeStructure()
{
    // OEIS A000081 structure:
    // Level 1: 1 term (root) - ID 0
    // Level 2: 2 terms - IDs 1, 2
    // Level 3: 4 terms - IDs 3, 4, 5, 6
    // Level 4: 9 terms - IDs 7, 8, 9, 10, 11, 12, 13, 14, 15
    // Total: 16 terms

    int32 TermID = 0;

    // Level 1: Root term
    FShellTermState RootTerm;
    RootTerm.TermID = TermID++;
    RootTerm.NestingLevel = EShellNestingLevel::Level1;
    RootTerm.ParentTermID = -1;
    RootTerm.ChildTermIDs = {1, 2};
    RootTerm.Activation = 1.0f;
    RootTerm.StepsFromParent = 0;
    RootTerm.StateVector.SetNum(StateVectorSize);
    State.Terms.Add(RootTerm);

    // Level 2: 2 terms
    for (int32 i = 0; i < 2; ++i)
    {
        FShellTermState Term;
        Term.TermID = TermID++;
        Term.NestingLevel = EShellNestingLevel::Level2;
        Term.ParentTermID = 0;
        Term.ChildTermIDs = {3 + i * 2, 4 + i * 2};
        Term.Activation = 0.0f;
        Term.StepsFromParent = 1; // 1 step apart
        Term.StateVector.SetNum(StateVectorSize);
        State.Terms.Add(Term);
    }

    // Level 3: 4 terms
    for (int32 i = 0; i < 4; ++i)
    {
        FShellTermState Term;
        Term.TermID = TermID++;
        Term.NestingLevel = EShellNestingLevel::Level3;
        Term.ParentTermID = 1 + (i / 2);
        
        // Level 3 terms have 2-3 children each to reach 9 total at level 4
        if (i < 2)
        {
            Term.ChildTermIDs = {7 + i * 2, 8 + i * 2};
        }
        else if (i == 2)
        {
            Term.ChildTermIDs = {11, 12, 13};
        }
        else
        {
            Term.ChildTermIDs = {14, 15};
        }
        
        Term.Activation = 0.0f;
        Term.StepsFromParent = 2; // 2 steps apart
        Term.StateVector.SetNum(StateVectorSize);
        State.Terms.Add(Term);
    }

    // Level 4: 9 terms
    TArray<int32> Level4Parents = {3, 3, 4, 4, 5, 5, 5, 6, 6};
    for (int32 i = 0; i < 9; ++i)
    {
        FShellTermState Term;
        Term.TermID = TermID++;
        Term.NestingLevel = EShellNestingLevel::Level4;
        Term.ParentTermID = Level4Parents[i];
        Term.ChildTermIDs.Empty(); // Leaf nodes
        Term.Activation = 0.0f;
        Term.StepsFromParent = 3; // 3 steps apart (but 4 steps from root)
        Term.StateVector.SetNum(StateVectorSize);
        State.Terms.Add(Term);
    }

    // Initialize state vectors with small random values
    for (FShellTermState& Term : State.Terms)
    {
        for (int32 i = 0; i < StateVectorSize; ++i)
        {
            Term.StateVector[i] = FMath::FRandRange(-0.1f, 0.1f);
        }
    }
}

void UNestedShellStructure::ActivateTerm(int32 TermID, float Activation)
{
    if (TermID >= 0 && TermID < State.Terms.Num())
    {
        float OldActivation = State.Terms[TermID].Activation;
        State.Terms[TermID].Activation = FMath::Clamp(Activation, 0.0f, 1.0f);

        if (FMath::Abs(State.Terms[TermID].Activation - OldActivation) > 0.01f)
        {
            OnTermActivated.Broadcast(TermID, State.Terms[TermID].Activation);
        }
    }
}

void UNestedShellStructure::SetTermState(int32 TermID, const TArray<float>& StateVector)
{
    if (TermID >= 0 && TermID < State.Terms.Num())
    {
        State.Terms[TermID].StateVector = StateVector;
        State.Terms[TermID].StateVector.SetNum(StateVectorSize);
    }
}

FShellTermState UNestedShellStructure::GetTermState(int32 TermID) const
{
    if (TermID >= 0 && TermID < State.Terms.Num())
    {
        return State.Terms[TermID];
    }
    return FShellTermState();
}

TArray<FShellTermState> UNestedShellStructure::GetTermsAtLevel(EShellNestingLevel Level) const
{
    TArray<FShellTermState> LevelTerms;
    for (const FShellTermState& Term : State.Terms)
    {
        if (Term.NestingLevel == Level)
        {
            LevelTerms.Add(Term);
        }
    }
    return LevelTerms;
}

TArray<FShellTermState> UNestedShellStructure::GetChildTerms(int32 ParentTermID) const
{
    TArray<FShellTermState> Children;
    if (ParentTermID >= 0 && ParentTermID < State.Terms.Num())
    {
        for (int32 ChildID : State.Terms[ParentTermID].ChildTermIDs)
        {
            if (ChildID >= 0 && ChildID < State.Terms.Num())
            {
                Children.Add(State.Terms[ChildID]);
            }
        }
    }
    return Children;
}

void UNestedShellStructure::PropagateOuterToInner()
{
    // Propagate from root to leaves
    for (int32 Level = 0; Level < 4; ++Level)
    {
        EShellNestingLevel NestLevel = static_cast<EShellNestingLevel>(Level);
        TArray<FShellTermState> LevelTerms = GetTermsAtLevel(NestLevel);

        for (const FShellTermState& Term : LevelTerms)
        {
            PropagateToChildren(Term.TermID);
        }
    }

    OnPropagationComplete.Broadcast(EShellPropagationDirection::OuterToInner, State.ShellCoherence);
}

void UNestedShellStructure::PropagateInnerToOuter()
{
    // Propagate from leaves to root
    for (int32 Level = 3; Level >= 0; --Level)
    {
        EShellNestingLevel NestLevel = static_cast<EShellNestingLevel>(Level);
        TArray<FShellTermState> LevelTerms = GetTermsAtLevel(NestLevel);

        for (const FShellTermState& Term : LevelTerms)
        {
            if (Term.ParentTermID >= 0)
            {
                PropagateToParent(Term.TermID);
            }
        }
    }

    OnPropagationComplete.Broadcast(EShellPropagationDirection::InnerToOuter, State.ShellCoherence);
}

void UNestedShellStructure::PropagateBidirectional()
{
    PropagateOuterToInner();
    PropagateInnerToOuter();

    OnPropagationComplete.Broadcast(EShellPropagationDirection::Bidirectional, State.ShellCoherence);
}

void UNestedShellStructure::PropagateToChildren(int32 ParentTermID)
{
    if (ParentTermID < 0 || ParentTermID >= State.Terms.Num())
    {
        return;
    }

    const FShellTermState& Parent = State.Terms[ParentTermID];

    for (int32 ChildID : Parent.ChildTermIDs)
    {
        if (ChildID >= 0 && ChildID < State.Terms.Num())
        {
            FShellTermState& Child = State.Terms[ChildID];

            // Propagate activation with decay
            float PropagatedActivation = Parent.Activation * PropagationDecay;
            Child.Activation = FMath::Max(Child.Activation, PropagatedActivation);

            // Propagate state vector
            for (int32 i = 0; i < FMath::Min(Parent.StateVector.Num(), Child.StateVector.Num()); ++i)
            {
                Child.StateVector[i] = FMath::Lerp(Child.StateVector[i], Parent.StateVector[i], PropagationDecay * 0.5f);
            }
        }
    }
}

void UNestedShellStructure::PropagateToParent(int32 ChildTermID)
{
    if (ChildTermID < 0 || ChildTermID >= State.Terms.Num())
    {
        return;
    }

    const FShellTermState& Child = State.Terms[ChildTermID];
    int32 ParentID = Child.ParentTermID;

    if (ParentID >= 0 && ParentID < State.Terms.Num())
    {
        FShellTermState& Parent = State.Terms[ParentID];

        // Average child states for parent
        TArray<float> AvgChildState = AverageChildStates(ParentID);

        // Update parent state with child average
        for (int32 i = 0; i < FMath::Min(Parent.StateVector.Num(), AvgChildState.Num()); ++i)
        {
            Parent.StateVector[i] = FMath::Lerp(Parent.StateVector[i], AvgChildState[i], PropagationDecay * 0.3f);
        }
    }
}

TArray<float> UNestedShellStructure::AverageChildStates(int32 ParentTermID)
{
    TArray<float> AvgState;
    AvgState.SetNum(StateVectorSize);
    for (float& Val : AvgState)
    {
        Val = 0.0f;
    }

    if (ParentTermID < 0 || ParentTermID >= State.Terms.Num())
    {
        return AvgState;
    }

    const FShellTermState& Parent = State.Terms[ParentTermID];
    int32 ChildCount = Parent.ChildTermIDs.Num();

    if (ChildCount == 0)
    {
        return AvgState;
    }

    for (int32 ChildID : Parent.ChildTermIDs)
    {
        if (ChildID >= 0 && ChildID < State.Terms.Num())
        {
            const FShellTermState& Child = State.Terms[ChildID];
            for (int32 i = 0; i < FMath::Min(StateVectorSize, Child.StateVector.Num()); ++i)
            {
                AvgState[i] += Child.StateVector[i];
            }
        }
    }

    for (float& Val : AvgState)
    {
        Val /= ChildCount;
    }

    return AvgState;
}

void UNestedShellStructure::SetPropagationDirection(EShellPropagationDirection Direction)
{
    State.PropagationDirection = Direction;
}

void UNestedShellStructure::MapEchobeatStep(int32 Step)
{
    State.CurrentEchobeatStep = FMath::Clamp(Step, 1, 12);

    // Map step to nesting level based on triadic groups
    // Triadic groups: {1,5,9}, {2,6,10}, {3,7,11}, {4,8,12}
    // Twin primes 5/7 relationship with mean 6 (3x2 triad-of-dyads)

    EShellNestingLevel OldLevel = State.ActiveLevel;

    // Steps 1, 5, 9 (group 0) -> Level 1 (root activation)
    // Steps 2, 6, 10 (group 1) -> Level 2
    // Steps 3, 7, 11 (group 2) -> Level 3
    // Steps 4, 8, 12 (group 3) -> Level 4 (triadic sync)

    int32 Group = (Step - 1) % 4;
    State.ActiveLevel = static_cast<EShellNestingLevel>(Group);

    if (State.ActiveLevel != OldLevel)
    {
        OnLevelChanged.Broadcast(OldLevel, State.ActiveLevel);
    }

    // Activate terms at the active level
    TArray<int32> ActiveTerms = GetActiveTermsForStep(Step);
    for (int32 TermID : ActiveTerms)
    {
        // Compute activation based on step position in cycle
        float StepPhase = (float)(Step - 1) / 12.0f;
        float Activation = 0.5f + 0.5f * FMath::Sin(StepPhase * 2.0f * PI);
        ActivateTerm(TermID, Activation);
    }
}

TArray<int32> UNestedShellStructure::GetActiveTermsForStep(int32 Step) const
{
    TArray<int32> ActiveTerms;
    EShellNestingLevel Level = GetNestingLevelForStep(Step);

    for (const FShellTermState& Term : State.Terms)
    {
        if (Term.NestingLevel == Level)
        {
            ActiveTerms.Add(Term.TermID);
        }
    }

    return ActiveTerms;
}

EShellNestingLevel UNestedShellStructure::GetNestingLevelForStep(int32 Step) const
{
    int32 Group = (Step - 1) % 4;
    return static_cast<EShellNestingLevel>(Group);
}

void UNestedShellStructure::ComputeShellCoherence()
{
    // Compute coherence as average pairwise similarity between adjacent levels
    float TotalCoherence = 0.0f;
    int32 PairCount = 0;

    for (int32 Level = 0; Level < 3; ++Level)
    {
        EShellNestingLevel CurrentLevel = static_cast<EShellNestingLevel>(Level);
        EShellNestingLevel NextLevel = static_cast<EShellNestingLevel>(Level + 1);

        TArray<FShellTermState> CurrentTerms = GetTermsAtLevel(CurrentLevel);
        TArray<FShellTermState> NextTerms = GetTermsAtLevel(NextLevel);

        for (const FShellTermState& Current : CurrentTerms)
        {
            for (const FShellTermState& Next : NextTerms)
            {
                if (Next.ParentTermID == Current.TermID)
                {
                    // Compute cosine similarity
                    float DotProduct = 0.0f;
                    float NormA = 0.0f;
                    float NormB = 0.0f;

                    int32 VecSize = FMath::Min(Current.StateVector.Num(), Next.StateVector.Num());
                    for (int32 i = 0; i < VecSize; ++i)
                    {
                        DotProduct += Current.StateVector[i] * Next.StateVector[i];
                        NormA += Current.StateVector[i] * Current.StateVector[i];
                        NormB += Next.StateVector[i] * Next.StateVector[i];
                    }

                    if (NormA > 0.0001f && NormB > 0.0001f)
                    {
                        float Similarity = DotProduct / (FMath::Sqrt(NormA) * FMath::Sqrt(NormB));
                        TotalCoherence += (Similarity + 1.0f) * 0.5f; // Normalize to 0-1
                        PairCount++;
                    }
                }
            }
        }
    }

    State.ShellCoherence = (PairCount > 0) ? (TotalCoherence / PairCount) : 1.0f;
}

FNestedShellState UNestedShellStructure::GetShellState() const
{
    return State;
}

float UNestedShellStructure::GetShellCoherence() const
{
    return State.ShellCoherence;
}

int32 UNestedShellStructure::GetTermCountAtLevel(EShellNestingLevel Level) const
{
    return GetA000081Value(static_cast<int32>(Level) + 1);
}

int32 UNestedShellStructure::GetTotalTermCount() const
{
    return 16; // 1 + 2 + 4 + 9
}

int32 UNestedShellStructure::GetA000081Value(int32 Level)
{
    // OEIS A000081: Number of rooted trees with n nodes
    // a(1) = 1, a(2) = 1, a(3) = 2, a(4) = 4, a(5) = 9, ...
    // For our purposes, we use:
    // Level 1 -> 1 term
    // Level 2 -> 2 terms
    // Level 3 -> 4 terms
    // Level 4 -> 9 terms

    switch (Level)
    {
    case 1: return 1;
    case 2: return 2;
    case 3: return 4;
    case 4: return 9;
    default: return 0;
    }
}

int32 UNestedShellStructure::GetStepsApart(EShellNestingLevel Level)
{
    // Steps apart from parent based on nesting level
    // 1 nest: 1 step apart
    // 2 nests: 2 steps apart
    // 3 nests: 3 steps apart
    // 4 nests: 4 steps apart

    return static_cast<int32>(Level) + 1;
}
