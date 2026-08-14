/**
 * Hierarchical Goal Manager Implementation
 * 
 * Implements multi-level goal management for Deep-Tree-Echo.
 */

#include "HierarchicalGoalManager.h"
#include "../Entelechy/EntelechyFramework.h"
#include "../Memory/EpisodicMemorySystem.h"

// ========================================
// CONSTRUCTOR
// ========================================

UHierarchicalGoalManager::UHierarchicalGoalManager()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.0f;
}

// ========================================
// LIFECYCLE
// ========================================

void UHierarchicalGoalManager::BeginPlay()
{
    Super::BeginPlay();
    Initialize();
}

void UHierarchicalGoalManager::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
    AccumulatedTime += DeltaTime;
    
    // Update urgencies
    UpdateUrgencies(DeltaTime);
    
    // Periodic progress update
    if (AccumulatedTime - LastProgressUpdateTime > Config.ProgressUpdateInterval)
    {
        for (auto& Pair : AllGoals)
        {
            if (Pair.Value.Status == EGoalStatus::Active)
            {
                CheckGoalConditions(Pair.Key);
            }
        }
        
        // Detect conflicts
        if (Config.bEnableConflictDetection)
        {
            DetectConflicts();
        }
        
        // Update coherence
        UpdateGoalCoherence();
        
        LastProgressUpdateTime = AccumulatedTime;
    }
    
    // Integrate with cognitive cycle
    if (CognitiveCycleManager)
    {
        int32 GlobalStep = CognitiveCycleManager->CycleState.GlobalStep;
        if (GlobalStep != LastProcessedStep)
        {
            EConsciousnessStream ActiveStream;
            int32 StepInCycle = ((GlobalStep - 1) % 12) + 1;
            
            if (StepInCycle >= 1 && StepInCycle <= 4)
                ActiveStream = EConsciousnessStream::Perception;
            else if (StepInCycle >= 5 && StepInCycle <= 8)
                ActiveStream = EConsciousnessStream::Action;
            else
                ActiveStream = EConsciousnessStream::Simulation;
            
            ProcessCognitiveStep(GlobalStep, ActiveStream);
            LastProcessedStep = GlobalStep;
        }
    }
}

// ========================================
// INITIALIZATION
// ========================================

void UHierarchicalGoalManager::Initialize()
{
    // Initialize state
    State.GoalStack.Empty();
    State.ActiveCountPerLevel.Empty();
    State.FocusGoalID = -1;
    State.ActiveConflicts.Empty();
    State.GoalCoherence = 1.0f;
    
    // Initialize active count per level
    State.ActiveCountPerLevel.Add(EGoalLevel::Life, 0);
    State.ActiveCountPerLevel.Add(EGoalLevel::Strategic, 0);
    State.ActiveCountPerLevel.Add(EGoalLevel::Tactical, 0);
    State.ActiveCountPerLevel.Add(EGoalLevel::Operational, 0);
    
    UE_LOG(LogTemp, Log, TEXT("Hierarchical Goal Manager initialized"));
}

void UHierarchicalGoalManager::Reset()
{
    AllGoals.Empty();
    NextGoalID = 1;
    AccumulatedTime = 0.0f;
    LastProgressUpdateTime = 0.0f;
    
    State.TotalGoalsCreated = 0;
    State.TotalGoalsCompleted = 0;
    
    Initialize();
}

// ========================================
// GOAL CREATION
// ========================================

int32 UHierarchicalGoalManager::CreateGoal(FName GoalName, const FString& Description, EGoalLevel Level, EGoalPriority Priority)
{
    FGoal NewGoal;
    NewGoal.GoalID = NextGoalID++;
    NewGoal.GoalName = GoalName;
    NewGoal.Description = Description;
    NewGoal.Level = Level;
    NewGoal.Priority = Priority;
    NewGoal.Status = EGoalStatus::Pending;
    NewGoal.CreationTime = AccumulatedTime;
    NewGoal.LastUpdateTime = AccumulatedTime;
    
    // Set default type based on level
    switch (Level)
    {
    case EGoalLevel::Life:
        NewGoal.Type = EGoalType::UniversalPrimary;
        NewGoal.Importance = 1.0f;
        break;
    case EGoalLevel::Strategic:
        NewGoal.Type = EGoalType::UniversalSecondary;
        NewGoal.Importance = 0.8f;
        break;
    case EGoalLevel::Tactical:
        NewGoal.Type = EGoalType::Particular3;
        NewGoal.Importance = 0.5f;
        break;
    case EGoalLevel::Operational:
        NewGoal.Type = EGoalType::Particular1;
        NewGoal.Importance = 0.3f;
        break;
    }
    
    AllGoals.Add(NewGoal.GoalID, NewGoal);
    State.TotalGoalsCreated++;
    
    OnGoalCreated.Broadcast(NewGoal.GoalID, GoalName);
    
    UE_LOG(LogTemp, Log, TEXT("Created goal %d: %s at level %d"), NewGoal.GoalID, *GoalName.ToString(), (int32)Level);
    
    return NewGoal.GoalID;
}

int32 UHierarchicalGoalManager::CreateSubgoal(int32 ParentGoalID, FName GoalName, const FString& Description)
{
    FGoal* Parent = GetGoalPtr(ParentGoalID);
    if (!Parent)
    {
        return -1;
    }
    
    // Determine subgoal level
    EGoalLevel SubLevel;
    switch (Parent->Level)
    {
    case EGoalLevel::Life:
        SubLevel = EGoalLevel::Strategic;
        break;
    case EGoalLevel::Strategic:
        SubLevel = EGoalLevel::Tactical;
        break;
    case EGoalLevel::Tactical:
    case EGoalLevel::Operational:
        SubLevel = EGoalLevel::Operational;
        break;
    default:
        SubLevel = EGoalLevel::Operational;
    }
    
    int32 SubgoalID = CreateGoal(GoalName, Description, SubLevel, Parent->Priority);
    
    if (FGoal* Subgoal = GetGoalPtr(SubgoalID))
    {
        Subgoal->ParentGoalID = ParentGoalID;
        Parent->ChildGoalIDs.Add(SubgoalID);
    }
    
    return SubgoalID;
}

void UHierarchicalGoalManager::AddSuccessCondition(int32 GoalID, FName ConditionID, float TargetValue, const FString& ComparisonOp)
{
    if (FGoal* Goal = GetGoalPtr(GoalID))
    {
        FGoalCondition Condition;
        Condition.ConditionID = ConditionID;
        Condition.TargetValue = TargetValue;
        Condition.ComparisonOp = ComparisonOp;
        Condition.CurrentValue = 0.0f;
        Condition.bSatisfied = false;
        Condition.Weight = 1.0f / (Goal->SuccessConditions.Num() + 1);
        
        // Rebalance weights
        for (FGoalCondition& Existing : Goal->SuccessConditions)
        {
            Existing.Weight = 1.0f / (Goal->SuccessConditions.Num() + 1);
        }
        
        Goal->SuccessConditions.Add(Condition);
    }
}

void UHierarchicalGoalManager::AddFailureCondition(int32 GoalID, FName ConditionID, float TargetValue, const FString& ComparisonOp)
{
    if (FGoal* Goal = GetGoalPtr(GoalID))
    {
        FGoalCondition Condition;
        Condition.ConditionID = ConditionID;
        Condition.TargetValue = TargetValue;
        Condition.ComparisonOp = ComparisonOp;
        Condition.CurrentValue = 0.0f;
        Condition.bSatisfied = false;
        
        Goal->FailureConditions.Add(Condition);
    }
}

void UHierarchicalGoalManager::SetGoalDeadline(int32 GoalID, float Deadline)
{
    if (FGoal* Goal = GetGoalPtr(GoalID))
    {
        Goal->Deadline = Deadline;
    }
}

void UHierarchicalGoalManager::AssociateAffordance(int32 GoalID, FName AffordanceName)
{
    if (FGoal* Goal = GetGoalPtr(GoalID))
    {
        if (!Goal->AssociatedAffordances.Contains(AffordanceName))
        {
            Goal->AssociatedAffordances.Add(AffordanceName);
        }
    }
}

void UHierarchicalGoalManager::AssociateSkill(int32 GoalID, FName SkillName)
{
    if (FGoal* Goal = GetGoalPtr(GoalID))
    {
        if (!Goal->RequiredSkills.Contains(SkillName))
        {
            Goal->RequiredSkills.Add(SkillName);
        }
    }
}

// ========================================
// GOAL PURSUIT
// ========================================

bool UHierarchicalGoalManager::ActivateGoal(int32 GoalID)
{
    FGoal* Goal = GetGoalPtr(GoalID);
    if (!Goal)
    {
        return false;
    }
    
    // Check if we can activate at this level
    int32* ActiveCount = State.ActiveCountPerLevel.Find(Goal->Level);
    if (ActiveCount && *ActiveCount >= Config.MaxActivePerLevel)
    {
        return false;
    }
    
    Goal->Status = EGoalStatus::Active;
    Goal->LastUpdateTime = AccumulatedTime;
    
    if (ActiveCount)
    {
        (*ActiveCount)++;
    }
    
    // Auto-generate subgoals if enabled
    if (Config.bEnableAutoSubgoals && Goal->ChildGoalIDs.Num() == 0)
    {
        AutoGenerateSubgoals(GoalID);
    }
    
    OnGoalActivated.Broadcast(GoalID);
    
    return true;
}

void UHierarchicalGoalManager::SuspendGoal(int32 GoalID)
{
    if (FGoal* Goal = GetGoalPtr(GoalID))
    {
        if (Goal->Status == EGoalStatus::Active)
        {
            Goal->Status = EGoalStatus::Suspended;
            Goal->LastUpdateTime = AccumulatedTime;
            
            if (int32* ActiveCount = State.ActiveCountPerLevel.Find(Goal->Level))
            {
                (*ActiveCount)--;
            }
        }
    }
}

void UHierarchicalGoalManager::ResumeGoal(int32 GoalID)
{
    if (FGoal* Goal = GetGoalPtr(GoalID))
    {
        if (Goal->Status == EGoalStatus::Suspended)
        {
            ActivateGoal(GoalID);
        }
    }
}

void UHierarchicalGoalManager::AbandonGoal(int32 GoalID)
{
    if (FGoal* Goal = GetGoalPtr(GoalID))
    {
        Goal->Status = EGoalStatus::Failed;
        Goal->LastUpdateTime = AccumulatedTime;
        
        if (Goal->Status == EGoalStatus::Active)
        {
            if (int32* ActiveCount = State.ActiveCountPerLevel.Find(Goal->Level))
            {
                (*ActiveCount)--;
            }
        }
        
        // Also abandon child goals
        for (int32 ChildID : Goal->ChildGoalIDs)
        {
            AbandonGoal(ChildID);
        }
        
        OnGoalFailed.Broadcast(GoalID, TEXT("Abandoned"));
    }
}

void UHierarchicalGoalManager::PushGoal(int32 GoalID)
{
    if (!GoalExists(GoalID))
    {
        return;
    }
    
    FGoalStackFrame Frame;
    Frame.GoalID = GoalID;
    Frame.EntryTime = AccumulatedTime;
    Frame.PursuitTime = 0.0f;
    Frame.CurrentSubgoalIndex = 0;
    
    State.GoalStack.Add(Frame);
    
    int32 OldFocus = State.FocusGoalID;
    State.FocusGoalID = GoalID;
    
    OnFocusChanged.Broadcast(OldFocus, GoalID);
}

int32 UHierarchicalGoalManager::PopGoal()
{
    if (State.GoalStack.Num() == 0)
    {
        return -1;
    }
    
    int32 PoppedID = State.GoalStack.Last().GoalID;
    State.GoalStack.RemoveAt(State.GoalStack.Num() - 1);
    
    int32 OldFocus = State.FocusGoalID;
    State.FocusGoalID = State.GoalStack.Num() > 0 ? State.GoalStack.Last().GoalID : -1;
    
    OnFocusChanged.Broadcast(OldFocus, State.FocusGoalID);
    
    return PoppedID;
}

void UHierarchicalGoalManager::SetFocusGoal(int32 GoalID)
{
    if (GoalExists(GoalID) || GoalID == -1)
    {
        int32 OldFocus = State.FocusGoalID;
        State.FocusGoalID = GoalID;
        
        if (OldFocus != GoalID)
        {
            OnFocusChanged.Broadcast(OldFocus, GoalID);
        }
    }
}

// ========================================
// GOAL PROGRESS
// ========================================

void UHierarchicalGoalManager::UpdateConditionValue(int32 GoalID, FName ConditionID, float NewValue)
{
    if (FGoal* Goal = GetGoalPtr(GoalID))
    {
        for (FGoalCondition& Condition : Goal->SuccessConditions)
        {
            if (Condition.ConditionID == ConditionID)
            {
                Condition.CurrentValue = NewValue;
                Condition.bSatisfied = EvaluateCondition(Condition);
                break;
            }
        }
        
        for (FGoalCondition& Condition : Goal->FailureConditions)
        {
            if (Condition.ConditionID == ConditionID)
            {
                Condition.CurrentValue = NewValue;
                Condition.bSatisfied = EvaluateCondition(Condition);
                break;
            }
        }
        
        // Update progress
        Goal->Progress = ComputeProgressFromConditions(*Goal);
        Goal->LastUpdateTime = AccumulatedTime;
        
        // Check for completion or failure
        CheckGoalConditions(GoalID);
    }
}

void UHierarchicalGoalManager::UpdateGoalProgress(int32 GoalID, float Progress)
{
    if (FGoal* Goal = GetGoalPtr(GoalID))
    {
        Goal->Progress = FMath::Clamp(Progress, 0.0f, 1.0f);
        Goal->LastUpdateTime = AccumulatedTime;
        
        // Propagate to parent
        PropagateProgressToParent(GoalID);
        
        // Check for completion
        if (Goal->Progress >= 1.0f)
        {
            CompleteGoal(GoalID);
        }
    }
}

void UHierarchicalGoalManager::CompleteGoal(int32 GoalID)
{
    if (FGoal* Goal = GetGoalPtr(GoalID))
    {
        Goal->Status = EGoalStatus::Completed;
        Goal->Progress = 1.0f;
        Goal->LastUpdateTime = AccumulatedTime;
        
        if (int32* ActiveCount = State.ActiveCountPerLevel.Find(Goal->Level))
        {
            (*ActiveCount)--;
        }
        
        State.TotalGoalsCompleted++;
        
        // Propagate to parent
        PropagateProgressToParent(GoalID);
        
        // Pop from stack if current focus
        if (State.FocusGoalID == GoalID)
        {
            PopGoal();
        }
        
        OnGoalCompleted.Broadcast(GoalID);
        
        UE_LOG(LogTemp, Log, TEXT("Goal %d completed: %s"), GoalID, *Goal->GoalName.ToString());
    }
}

void UHierarchicalGoalManager::FailGoal(int32 GoalID, const FString& Reason)
{
    if (FGoal* Goal = GetGoalPtr(GoalID))
    {
        Goal->Status = EGoalStatus::Failed;
        Goal->LastUpdateTime = AccumulatedTime;
        
        if (int32* ActiveCount = State.ActiveCountPerLevel.Find(Goal->Level))
        {
            (*ActiveCount)--;
        }
        
        // Pop from stack if current focus
        if (State.FocusGoalID == GoalID)
        {
            PopGoal();
        }
        
        OnGoalFailed.Broadcast(GoalID, Reason);
        
        UE_LOG(LogTemp, Log, TEXT("Goal %d failed: %s - %s"), GoalID, *Goal->GoalName.ToString(), *Reason);
    }
}

float UHierarchicalGoalManager::GetGoalProgress(int32 GoalID) const
{
    if (const FGoal* Goal = GetGoalPtr(GoalID))
    {
        return Goal->Progress;
    }
    return 0.0f;
}

// ========================================
// GOAL QUERIES
// ========================================

FGoal UHierarchicalGoalManager::GetGoal(int32 GoalID) const
{
    if (const FGoal* Goal = GetGoalPtr(GoalID))
    {
        return *Goal;
    }
    return FGoal();
}

TArray<FGoal> UHierarchicalGoalManager::GetGoalsAtLevel(EGoalLevel Level) const
{
    TArray<FGoal> Result;
    for (const auto& Pair : AllGoals)
    {
        if (Pair.Value.Level == Level)
        {
            Result.Add(Pair.Value);
        }
    }
    return Result;
}

TArray<FGoal> UHierarchicalGoalManager::GetActiveGoals() const
{
    TArray<FGoal> Result;
    for (const auto& Pair : AllGoals)
    {
        if (Pair.Value.Status == EGoalStatus::Active)
        {
            Result.Add(Pair.Value);
        }
    }
    return Result;
}

FGoal UHierarchicalGoalManager::GetFocusGoal() const
{
    return GetGoal(State.FocusGoalID);
}

TArray<FGoal> UHierarchicalGoalManager::GetChildGoals(int32 ParentGoalID) const
{
    TArray<FGoal> Result;
    if (const FGoal* Parent = GetGoalPtr(ParentGoalID))
    {
        for (int32 ChildID : Parent->ChildGoalIDs)
        {
            if (const FGoal* Child = GetGoalPtr(ChildID))
            {
                Result.Add(*Child);
            }
        }
    }
    return Result;
}

TArray<FGoal> UHierarchicalGoalManager::GetGoalsByPriority(EGoalPriority Priority) const
{
    TArray<FGoal> Result;
    for (const auto& Pair : AllGoals)
    {
        if (Pair.Value.Priority == Priority)
        {
            Result.Add(Pair.Value);
        }
    }
    return Result;
}

TArray<FGoal> UHierarchicalGoalManager::GetGoalsForAffordance(FName AffordanceName) const
{
    TArray<FGoal> Result;
    for (const auto& Pair : AllGoals)
    {
        if (Pair.Value.AssociatedAffordances.Contains(AffordanceName))
        {
            Result.Add(Pair.Value);
        }
    }
    return Result;
}

bool UHierarchicalGoalManager::GoalExists(int32 GoalID) const
{
    return AllGoals.Contains(GoalID);
}

// ========================================
// CONFLICT MANAGEMENT
// ========================================

TArray<FGoalConflict> UHierarchicalGoalManager::DetectConflicts()
{
    State.ActiveConflicts.Empty();
    
    TArray<FGoal> ActiveGoals = GetActiveGoals();
    
    for (int32 i = 0; i < ActiveGoals.Num(); ++i)
    {
        for (int32 j = i + 1; j < ActiveGoals.Num(); ++j)
        {
            const FGoal& A = ActiveGoals[i];
            const FGoal& B = ActiveGoals[j];
            
            // Check for explicit conflicts
            if (A.ConflictingGoals.Contains(B.GoalID) || B.ConflictingGoals.Contains(A.GoalID))
            {
                FGoalConflict Conflict;
                Conflict.GoalA = A.GoalID;
                Conflict.GoalB = B.GoalID;
                Conflict.Severity = 0.8f;
                Conflict.ConflictType = TEXT("Explicit");
                Conflict.RecommendedResolution = Config.DefaultConflictResolution;
                
                State.ActiveConflicts.Add(Conflict);
                OnConflictDetected.Broadcast(A.GoalID, B.GoalID);
            }
            // Check for resource conflicts (same affordances)
            else
            {
                int32 SharedAffordances = 0;
                for (const FName& Aff : A.AssociatedAffordances)
                {
                    if (B.AssociatedAffordances.Contains(Aff))
                    {
                        SharedAffordances++;
                    }
                }
                
                if (SharedAffordances > 0 && A.Priority == B.Priority)
                {
                    FGoalConflict Conflict;
                    Conflict.GoalA = A.GoalID;
                    Conflict.GoalB = B.GoalID;
                    Conflict.Severity = 0.3f * SharedAffordances;
                    Conflict.ConflictType = TEXT("Resource");
                    Conflict.RecommendedResolution = EConflictResolution::UrgencyBased;
                    
                    State.ActiveConflicts.Add(Conflict);
                }
            }
        }
    }
    
    return State.ActiveConflicts;
}

void UHierarchicalGoalManager::ResolveConflict(int32 GoalA, int32 GoalB, EConflictResolution Strategy)
{
    FGoal* A = GetGoalPtr(GoalA);
    FGoal* B = GetGoalPtr(GoalB);
    
    if (!A || !B)
    {
        return;
    }
    
    int32 WinnerID = -1;
    int32 LoserID = -1;
    
    switch (Strategy)
    {
    case EConflictResolution::PriorityBased:
        if ((int32)A->Priority < (int32)B->Priority)
        {
            WinnerID = GoalA;
            LoserID = GoalB;
        }
        else
        {
            WinnerID = GoalB;
            LoserID = GoalA;
        }
        break;
        
    case EConflictResolution::HierarchyBased:
        if ((int32)A->Level < (int32)B->Level)
        {
            WinnerID = GoalA;
            LoserID = GoalB;
        }
        else
        {
            WinnerID = GoalB;
            LoserID = GoalA;
        }
        break;
        
    case EConflictResolution::UrgencyBased:
        if (A->Urgency > B->Urgency)
        {
            WinnerID = GoalA;
            LoserID = GoalB;
        }
        else
        {
            WinnerID = GoalB;
            LoserID = GoalA;
        }
        break;
        
    case EConflictResolution::Compromise:
        // Both goals continue but with reduced priority
        A->Priority = (EGoalPriority)FMath::Min((int32)A->Priority + 1, (int32)EGoalPriority::Background);
        B->Priority = (EGoalPriority)FMath::Min((int32)B->Priority + 1, (int32)EGoalPriority::Background);
        break;
        
    case EConflictResolution::Defer:
        // Do nothing, let it resolve naturally
        break;
    }
    
    if (LoserID >= 0)
    {
        SuspendGoal(LoserID);
    }
    
    // Mark conflict as resolved
    for (FGoalConflict& Conflict : State.ActiveConflicts)
    {
        if ((Conflict.GoalA == GoalA && Conflict.GoalB == GoalB) ||
            (Conflict.GoalA == GoalB && Conflict.GoalB == GoalA))
        {
            Conflict.bResolved = true;
        }
    }
}

void UHierarchicalGoalManager::MarkConflicting(int32 GoalA, int32 GoalB)
{
    if (FGoal* A = GetGoalPtr(GoalA))
    {
        if (!A->ConflictingGoals.Contains(GoalB))
        {
            A->ConflictingGoals.Add(GoalB);
        }
    }
    
    if (FGoal* B = GetGoalPtr(GoalB))
    {
        if (!B->ConflictingGoals.Contains(GoalA))
        {
            B->ConflictingGoals.Add(GoalA);
        }
    }
}

void UHierarchicalGoalManager::MarkSupporting(int32 GoalA, int32 GoalB)
{
    if (FGoal* A = GetGoalPtr(GoalA))
    {
        if (!A->SupportingGoals.Contains(GoalB))
        {
            A->SupportingGoals.Add(GoalB);
        }
    }
    
    if (FGoal* B = GetGoalPtr(GoalB))
    {
        if (!B->SupportingGoals.Contains(GoalA))
        {
            B->SupportingGoals.Add(GoalA);
        }
    }
}

// ========================================
// COGNITIVE CYCLE INTEGRATION
// ========================================

void UHierarchicalGoalManager::ProcessCognitiveStep(int32 GlobalStep, EConsciousnessStream ActiveStream)
{
    int32 StepInCycle = ((GlobalStep - 1) % 12) + 1;
    
    switch (ActiveStream)
    {
    case EConsciousnessStream::Perception:
        // Steps 1-4: Goal-relevant perception
        if (StepInCycle == 1)
        {
            // T1R: Assess needs vs capacity
            // Check if current goals are achievable
        }
        else if (StepInCycle == 4)
        {
            // T4E: Sensory input for goal monitoring
            // Update goal conditions based on perception
        }
        break;
        
    case EConsciousnessStream::Action:
        // Steps 5-8: Goal-directed action
        if (StepInCycle == 5)
        {
            // Select action based on current focus goal
            FName Action = SelectGoalDirectedAction();
        }
        else if (StepInCycle == 7)
        {
            // T5E: Execute action sequence
            // Update goal stack frame pursuit time
            if (State.GoalStack.Num() > 0)
            {
                State.GoalStack.Last().PursuitTime += 0.1f;
            }
        }
        break;
        
    case EConsciousnessStream::Simulation:
        // Steps 9-12: Goal planning and evaluation
        if (StepInCycle == 9)
        {
            // T8E: Balanced response - evaluate goal progress
            for (auto& Pair : AllGoals)
            {
                if (Pair.Value.Status == EGoalStatus::Active)
                {
                    CheckGoalConditions(Pair.Key);
                }
            }
        }
        else if (StepInCycle == 12)
        {
            // Planning step - consider goal hierarchy
            // Auto-generate subgoals if needed
            if (State.FocusGoalID >= 0)
            {
                if (FGoal* Focus = GetGoalPtr(State.FocusGoalID))
                {
                    if (Config.bEnableAutoSubgoals && Focus->ChildGoalIDs.Num() == 0 && Focus->Progress < 0.5f)
                    {
                        AutoGenerateSubgoals(State.FocusGoalID);
                    }
                }
            }
        }
        break;
    }
}

TArray<float> UHierarchicalGoalManager::GetGoalDirectedAttention(const TArray<float>& CurrentPerception) const
{
    TArray<float> Attention;
    Attention.SetNum(CurrentPerception.Num());
    
    // Initialize with uniform attention
    for (float& W : Attention)
    {
        W = 1.0f / CurrentPerception.Num();
    }
    
    // Modulate by active goals
    // (In a full implementation, this would use goal-specific attention patterns)
    
    return Attention;
}

FName UHierarchicalGoalManager::SelectGoalDirectedAction() const
{
    if (State.FocusGoalID < 0)
    {
        return NAME_None;
    }
    
    const FGoal* Focus = GetGoalPtr(State.FocusGoalID);
    if (!Focus)
    {
        return NAME_None;
    }
    
    // Return first associated affordance as action
    if (Focus->AssociatedAffordances.Num() > 0)
    {
        return Focus->AssociatedAffordances[0];
    }
    
    return NAME_None;
}

// ========================================
// SYSTEM 5 INTEGRATION
// ========================================

TArray<FGoal> UHierarchicalGoalManager::GetGoalsByType(EGoalType Type) const
{
    TArray<FGoal> Result;
    for (const auto& Pair : AllGoals)
    {
        if (Pair.Value.Type == Type)
        {
            Result.Add(Pair.Value);
        }
    }
    return Result;
}

TArray<FGoal> UHierarchicalGoalManager::GetUniversalGoals() const
{
    TArray<FGoal> Result;
    for (const auto& Pair : AllGoals)
    {
        if (Pair.Value.Type == EGoalType::UniversalPrimary ||
            Pair.Value.Type == EGoalType::UniversalSecondary ||
            Pair.Value.Type == EGoalType::UniversalTertiary)
        {
            Result.Add(Pair.Value);
        }
    }
    return Result;
}

TArray<FGoal> UHierarchicalGoalManager::GetParticularGoals() const
{
    TArray<FGoal> Result;
    for (const auto& Pair : AllGoals)
    {
        if (Pair.Value.Type == EGoalType::Particular1 ||
            Pair.Value.Type == EGoalType::Particular2 ||
            Pair.Value.Type == EGoalType::Particular3 ||
            Pair.Value.Type == EGoalType::Particular4)
        {
            Result.Add(Pair.Value);
        }
    }
    return Result;
}

float UHierarchicalGoalManager::ComputeGoalBalance() const
{
    TArray<FGoal> Universal = GetUniversalGoals();
    TArray<FGoal> Particular = GetParticularGoals();
    
    float UniversalWeight = 0.0f;
    for (const FGoal& G : Universal)
    {
        UniversalWeight += G.Importance * (G.Status == EGoalStatus::Active ? 1.0f : 0.5f);
    }
    
    float ParticularWeight = 0.0f;
    for (const FGoal& G : Particular)
    {
        ParticularWeight += G.Importance * (G.Status == EGoalStatus::Active ? 1.0f : 0.5f);
    }
    
    float Total = UniversalWeight + ParticularWeight;
    if (Total < 0.001f)
    {
        return 0.5f; // Balanced by default
    }
    
    // Return ratio of Universal to total (0.5 = balanced)
    return UniversalWeight / Total;
}

// ========================================
// INTERNAL METHODS
// ========================================

void UHierarchicalGoalManager::UpdateUrgencies(float DeltaTime)
{
    for (auto& Pair : AllGoals)
    {
        if (Pair.Value.Status == EGoalStatus::Active)
        {
            // Base urgency increase
            Pair.Value.Urgency += Config.UrgencyIncreaseRate * DeltaTime;
            
            // Deadline-based urgency
            if (Pair.Value.Deadline > 0.0f)
            {
                float TimeRemaining = Pair.Value.Deadline - AccumulatedTime;
                if (TimeRemaining > 0.0f)
                {
                    float DeadlineUrgency = 1.0f - (TimeRemaining / Pair.Value.Deadline);
                    Pair.Value.Urgency = FMath::Max(Pair.Value.Urgency, DeadlineUrgency);
                }
                else
                {
                    // Past deadline
                    Pair.Value.Urgency = 1.0f;
                }
            }
            
            Pair.Value.Urgency = FMath::Clamp(Pair.Value.Urgency, 0.0f, 1.0f);
        }
    }
}

void UHierarchicalGoalManager::CheckGoalConditions(int32 GoalID)
{
    FGoal* Goal = GetGoalPtr(GoalID);
    if (!Goal || Goal->Status != EGoalStatus::Active)
    {
        return;
    }
    
    // Check failure conditions first
    for (FGoalCondition& Condition : Goal->FailureConditions)
    {
        if (EvaluateCondition(Condition))
        {
            FailGoal(GoalID, FString::Printf(TEXT("Failure condition met: %s"), *Condition.ConditionID.ToString()));
            return;
        }
    }
    
    // Check success conditions
    bool AllSatisfied = Goal->SuccessConditions.Num() > 0;
    for (FGoalCondition& Condition : Goal->SuccessConditions)
    {
        Condition.bSatisfied = EvaluateCondition(Condition);
        if (!Condition.bSatisfied)
        {
            AllSatisfied = false;
        }
    }
    
    // Update progress
    Goal->Progress = ComputeProgressFromConditions(*Goal);
    
    if (AllSatisfied)
    {
        CompleteGoal(GoalID);
    }
}

float UHierarchicalGoalManager::ComputeProgressFromConditions(const FGoal& Goal) const
{
    if (Goal.SuccessConditions.Num() == 0)
    {
        // If no conditions, use child goal progress
        if (Goal.ChildGoalIDs.Num() > 0)
        {
            float TotalProgress = 0.0f;
            for (int32 ChildID : Goal.ChildGoalIDs)
            {
                if (const FGoal* Child = GetGoalPtr(ChildID))
                {
                    TotalProgress += Child->Progress;
                }
            }
            return TotalProgress / Goal.ChildGoalIDs.Num();
        }
        return 0.0f;
    }
    
    float WeightedProgress = 0.0f;
    float TotalWeight = 0.0f;
    
    for (const FGoalCondition& Condition : Goal.SuccessConditions)
    {
        float ConditionProgress = 0.0f;
        if (Condition.TargetValue != 0.0f)
        {
            ConditionProgress = FMath::Clamp(Condition.CurrentValue / Condition.TargetValue, 0.0f, 1.0f);
        }
        else if (Condition.bSatisfied)
        {
            ConditionProgress = 1.0f;
        }
        
        WeightedProgress += ConditionProgress * Condition.Weight;
        TotalWeight += Condition.Weight;
    }
    
    if (TotalWeight > 0.0f)
    {
        return WeightedProgress / TotalWeight;
    }
    
    return 0.0f;
}

void UHierarchicalGoalManager::PropagateProgressToParent(int32 GoalID)
{
    const FGoal* Goal = GetGoalPtr(GoalID);
    if (!Goal || Goal->ParentGoalID < 0)
    {
        return;
    }
    
    if (FGoal* Parent = GetGoalPtr(Goal->ParentGoalID))
    {
        // Recompute parent progress from children
        float TotalProgress = 0.0f;
        int32 ChildCount = 0;
        
        for (int32 ChildID : Parent->ChildGoalIDs)
        {
            if (const FGoal* Child = GetGoalPtr(ChildID))
            {
                TotalProgress += Child->Progress;
                ChildCount++;
            }
        }
        
        if (ChildCount > 0)
        {
            Parent->Progress = TotalProgress / ChildCount;
            Parent->LastUpdateTime = AccumulatedTime;
            
            // Check if parent is complete
            if (Parent->Progress >= 1.0f)
            {
                CompleteGoal(Parent->GoalID);
            }
            else
            {
                // Continue propagation
                PropagateProgressToParent(Parent->GoalID);
            }
        }
    }
}

void UHierarchicalGoalManager::AutoGenerateSubgoals(int32 GoalID)
{
    FGoal* Goal = GetGoalPtr(GoalID);
    if (!Goal)
    {
        return;
    }
    
    // Generate subgoals based on goal level
    switch (Goal->Level)
    {
    case EGoalLevel::Life:
        // Life goals generate strategic subgoals
        CreateSubgoal(GoalID, FName(*FString::Printf(TEXT("%s_Strategy1"), *Goal->GoalName.ToString())), TEXT("Strategic step 1"));
        CreateSubgoal(GoalID, FName(*FString::Printf(TEXT("%s_Strategy2"), *Goal->GoalName.ToString())), TEXT("Strategic step 2"));
        break;
        
    case EGoalLevel::Strategic:
        // Strategic goals generate tactical subgoals
        CreateSubgoal(GoalID, FName(*FString::Printf(TEXT("%s_Tactic1"), *Goal->GoalName.ToString())), TEXT("Tactical step 1"));
        CreateSubgoal(GoalID, FName(*FString::Printf(TEXT("%s_Tactic2"), *Goal->GoalName.ToString())), TEXT("Tactical step 2"));
        break;
        
    case EGoalLevel::Tactical:
        // Tactical goals generate operational subgoals
        CreateSubgoal(GoalID, FName(*FString::Printf(TEXT("%s_Op1"), *Goal->GoalName.ToString())), TEXT("Operational step 1"));
        CreateSubgoal(GoalID, FName(*FString::Printf(TEXT("%s_Op2"), *Goal->GoalName.ToString())), TEXT("Operational step 2"));
        break;
        
    case EGoalLevel::Operational:
        // Operational goals don't generate subgoals
        break;
    }
}

void UHierarchicalGoalManager::UpdateGoalCoherence()
{
    // Coherence decreases with conflicts and increases with supporting goals
    float ConflictPenalty = State.ActiveConflicts.Num() * 0.1f;
    
    float SupportBonus = 0.0f;
    for (const auto& Pair : AllGoals)
    {
        SupportBonus += Pair.Value.SupportingGoals.Num() * 0.05f;
    }
    
    State.GoalCoherence = FMath::Clamp(1.0f - ConflictPenalty + SupportBonus, 0.0f, 1.0f);
}

FGoal* UHierarchicalGoalManager::GetGoalPtr(int32 GoalID)
{
    return AllGoals.Find(GoalID);
}

const FGoal* UHierarchicalGoalManager::GetGoalPtr(int32 GoalID) const
{
    return AllGoals.Find(GoalID);
}

bool UHierarchicalGoalManager::EvaluateCondition(const FGoalCondition& Condition) const
{
    if (Condition.ComparisonOp == TEXT(">="))
    {
        return Condition.CurrentValue >= Condition.TargetValue;
    }
    else if (Condition.ComparisonOp == TEXT(">"))
    {
        return Condition.CurrentValue > Condition.TargetValue;
    }
    else if (Condition.ComparisonOp == TEXT("<="))
    {
        return Condition.CurrentValue <= Condition.TargetValue;
    }
    else if (Condition.ComparisonOp == TEXT("<"))
    {
        return Condition.CurrentValue < Condition.TargetValue;
    }
    else if (Condition.ComparisonOp == TEXT("=="))
    {
        return FMath::IsNearlyEqual(Condition.CurrentValue, Condition.TargetValue, 0.001f);
    }
    
    return false;
}

float UHierarchicalGoalManager::ComputeGoalUtility(const FGoal& Goal) const
{
    // Utility = Importance * (1 - Progress) * Urgency * Priority_Weight
    float PriorityWeight = 1.0f;
    switch (Goal.Priority)
    {
    case EGoalPriority::Critical: PriorityWeight = 2.0f; break;
    case EGoalPriority::High: PriorityWeight = 1.5f; break;
    case EGoalPriority::Medium: PriorityWeight = 1.0f; break;
    case EGoalPriority::Low: PriorityWeight = 0.5f; break;
    case EGoalPriority::Background: PriorityWeight = 0.25f; break;
    }
    
    return Goal.Importance * (1.0f - Goal.Progress) * (0.5f + 0.5f * Goal.Urgency) * PriorityWeight;
}
