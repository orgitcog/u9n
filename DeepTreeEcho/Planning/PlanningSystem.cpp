// PlanningSystem.cpp
// Implementation of Goal-Directed Planning and Action Selection

#include "PlanningSystem.h"
#include "../Core/CognitiveCycleManager.h"
#include "../Memory/MemorySystems.h"
#include "../Learning/OnlineLearningSystem.h"
#include "Math/UnrealMathUtility.h"

UPlanningSystem::UPlanningSystem()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UPlanningSystem::BeginPlay()
{
    Super::BeginPlay();
    FindComponentReferences();
    InitializePlanningSystem();
}

void UPlanningSystem::TickComponent(float DeltaTime, ELevelTick TickType, 
                                     FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    UpdateGoalProgress(DeltaTime);
    UpdatePlanExecution(DeltaTime);
    CheckGoalConditions();
}

void UPlanningSystem::FindComponentReferences()
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        CycleManager = Owner->FindComponentByClass<UCognitiveCycleManager>();
        MemoryComponent = Owner->FindComponentByClass<UMemorySystems>();
        LearningComponent = Owner->FindComponentByClass<UOnlineLearningSystem>();
    }
}

void UPlanningSystem::InitializePlanningSystem()
{
    Goals.Empty();
    Plans.Empty();
    InitializeDefaultActions();
}

void UPlanningSystem::InitializeDefaultActions()
{
    RegisterActionTemplate(TEXT("Observe"), {}, {TEXT("HasObservation")}, 0.5f, 1.0f);
    RegisterActionTemplate(TEXT("Move"), {TEXT("HasLocation")}, {TEXT("AtLocation")}, 1.0f, 2.0f);
    RegisterActionTemplate(TEXT("Interact"), {TEXT("AtLocation")}, {TEXT("Interacted")}, 1.5f, 3.0f);
    RegisterActionTemplate(TEXT("Communicate"), {TEXT("HasAgent")}, {TEXT("Communicated")}, 1.0f, 2.0f);
    RegisterActionTemplate(TEXT("Think"), {}, {TEXT("HasThought")}, 0.5f, 0.5f);
}

FGoal UPlanningSystem::CreateGoal(const FString& Name, const FString& Description, EGoalPriority Priority,
                                   const TArray<FString>& SuccessConditions)
{
    FGoal Goal;
    Goal.GoalID = GenerateGoalID();
    Goal.GoalName = Name;
    Goal.Description = Description;
    Goal.Priority = Priority;
    Goal.SuccessConditions = SuccessConditions;
    Goal.Status = EGoalStatus::Pending;
    Goal.CreationTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    Goals.Add(Goal);
    OnGoalCreated.Broadcast(Goal);
    return Goal;
}

FGoal UPlanningSystem::CreateSubGoal(const FString& ParentGoalID, const FString& Name, 
                                      const TArray<FString>& SuccessConditions)
{
    int32 ParentIndex = FindGoalIndex(ParentGoalID);
    if (ParentIndex < 0) return FGoal();
    FGoal SubGoal = CreateGoal(Name, TEXT("Subgoal"), Goals[ParentIndex].Priority, SuccessConditions);
    SubGoal.ParentGoalID = ParentGoalID;
    Goals[ParentIndex].SubGoals.Add(SubGoal.GoalID);
    return SubGoal;
}

void UPlanningSystem::ActivateGoal(const FString& GoalID)
{
    int32 Index = FindGoalIndex(GoalID);
    if (Index >= 0)
    {
        Goals[Index].Status = EGoalStatus::Active;
        OnGoalStatusChanged.Broadcast(GoalID, EGoalStatus::Active);
        CreatePlan(GoalID);
    }
}

void UPlanningSystem::SuspendGoal(const FString& GoalID)
{
    int32 Index = FindGoalIndex(GoalID);
    if (Index >= 0)
    {
        Goals[Index].Status = EGoalStatus::Suspended;
        OnGoalStatusChanged.Broadcast(GoalID, EGoalStatus::Suspended);
    }
}

void UPlanningSystem::AbandonGoal(const FString& GoalID)
{
    int32 Index = FindGoalIndex(GoalID);
    if (Index >= 0)
    {
        Goals[Index].Status = EGoalStatus::Abandoned;
        OnGoalStatusChanged.Broadcast(GoalID, EGoalStatus::Abandoned);
    }
}

FGoal UPlanningSystem::GetGoal(const FString& GoalID) const
{
    int32 Index = FindGoalIndex(GoalID);
    return Index >= 0 ? Goals[Index] : FGoal();
}

TArray<FGoal> UPlanningSystem::GetAllGoals() const { return Goals; }

TArray<FGoal> UPlanningSystem::GetActiveGoals() const
{
    TArray<FGoal> Active;
    for (const FGoal& G : Goals) if (G.Status == EGoalStatus::Active) Active.Add(G);
    return Active;
}

FGoal UPlanningSystem::GetHighestPriorityGoal() const
{
    FGoal Highest;
    float Best = -1;
    for (const FGoal& G : Goals)
    {
        float U = ComputeGoalUtility(G);
        if (U > Best) { Best = U; Highest = G; }
    }
    return Highest;
}

bool UPlanningSystem::IsGoalAchieved(const FString& GoalID) const
{
    int32 Index = FindGoalIndex(GoalID);
    if (Index < 0) return false;
    for (const FString& C : Goals[Index].SuccessConditions)
        if (!IsConditionSatisfied(C)) return false;
    return true;
}

FPlan UPlanningSystem::CreatePlan(const FString& GoalID)
{
    int32 GI = FindGoalIndex(GoalID);
    if (GI < 0) return FPlan();
    FPlan Plan;
    Plan.PlanID = GeneratePlanID();
    Plan.TargetGoalID = GoalID;
    Plan.Actions = GenerateActionSequence(Goals[GI]);
    Plan.Status = Plan.Actions.Num() > 0 ? EPlanStatus::Ready : EPlanStatus::Failed;
    Plans.Add(Plan);
    OnPlanCreated.Broadcast(Plan);
    return Plan;
}

void UPlanningSystem::ExecutePlan(const FString& PlanID)
{
    int32 Index = FindPlanIndex(PlanID);
    if (Index >= 0)
    {
        Plans[Index].Status = EPlanStatus::Executing;
        CurrentPlanID = PlanID;
        OnPlanStatusChanged.Broadcast(PlanID, EPlanStatus::Executing);
    }
}

void UPlanningSystem::PausePlan(const FString& PlanID)
{
    int32 Index = FindPlanIndex(PlanID);
    if (Index >= 0) Plans[Index].Status = EPlanStatus::Ready;
}

void UPlanningSystem::ResumePlan(const FString& PlanID)
{
    int32 Index = FindPlanIndex(PlanID);
    if (Index >= 0) Plans[Index].Status = EPlanStatus::Executing;
}

void UPlanningSystem::CancelPlan(const FString& PlanID)
{
    int32 Index = FindPlanIndex(PlanID);
    if (Index >= 0) Plans[Index].Status = EPlanStatus::Failed;
}

FPlan UPlanningSystem::Replan(const FString& GoalID) { return CreatePlan(GoalID); }

FPlan UPlanningSystem::GetPlan(const FString& PlanID) const
{
    int32 Index = FindPlanIndex(PlanID);
    return Index >= 0 ? Plans[Index] : FPlan();
}

FPlan UPlanningSystem::GetPlanForGoal(const FString& GoalID) const
{
    for (const FPlan& P : Plans) if (P.TargetGoalID == GoalID) return P;
    return FPlan();
}

TArray<FPlan> UPlanningSystem::GetAllPlans() const { return Plans; }

FPlannedAction UPlanningSystem::GetCurrentAction() const
{
    int32 Index = FindPlanIndex(CurrentPlanID);
    if (Index >= 0 && Plans[Index].CurrentActionIndex < Plans[Index].Actions.Num())
        return Plans[Index].Actions[Plans[Index].CurrentActionIndex];
    return FPlannedAction();
}

void UPlanningSystem::RegisterActionTemplate(const FString& ActionName, const TArray<FString>& Preconditions,
                                              const TArray<FString>& Effects, float Cost, float Duration)
{
    FActionTemplate T;
    T.ActionName = ActionName;
    T.Preconditions = Preconditions;
    T.Effects = Effects;
    T.BaseCost = Cost;
    T.BaseDuration = Duration;
    ActionTemplates.Add(T);
}

void UPlanningSystem::CompleteCurrentAction(bool bSuccess)
{
    int32 Index = FindPlanIndex(CurrentPlanID);
    if (Index < 0) return;
    int32 AI = Plans[Index].CurrentActionIndex;
    if (AI < Plans[Index].Actions.Num())
    {
        Plans[Index].Actions[AI].Status = bSuccess ? EActionStatus::Completed : EActionStatus::Failed;
        OnActionCompleted.Broadcast(Plans[Index].Actions[AI], bSuccess);
        if (bSuccess) ApplyActionEffects(Plans[Index].Actions[AI]);
        Plans[Index].CurrentActionIndex++;
    }
}

void UPlanningSystem::SkipCurrentAction()
{
    int32 Index = FindPlanIndex(CurrentPlanID);
    if (Index >= 0) Plans[Index].CurrentActionIndex++;
}

TArray<FActionTemplate> UPlanningSystem::GetAvailableActions() const { return ActionTemplates; }

TArray<FActionTemplate> UPlanningSystem::GetApplicableActions() const
{
    TArray<FActionTemplate> A;
    for (const FActionTemplate& T : ActionTemplates) if (CanApplyAction(T)) A.Add(T);
    return A;
}

void UPlanningSystem::SetBooleanFact(const FString& FactName, bool Value) { WorldState.BooleanFacts.Add(FactName, Value); }
bool UPlanningSystem::GetBooleanFact(const FString& FactName) const { return WorldState.BooleanFacts.Contains(FactName) ? WorldState.BooleanFacts[FactName] : false; }
void UPlanningSystem::SetNumericFact(const FString& FactName, float Value) { WorldState.NumericFacts.Add(FactName, Value); }
float UPlanningSystem::GetNumericFact(const FString& FactName) const { return WorldState.NumericFacts.Contains(FactName) ? WorldState.NumericFacts[FactName] : 0.0f; }
void UPlanningSystem::SetStringFact(const FString& FactName, const FString& Value) { WorldState.StringFacts.Add(FactName, Value); }
FString UPlanningSystem::GetStringFact(const FString& FactName) const { return WorldState.StringFacts.Contains(FactName) ? WorldState.StringFacts[FactName] : TEXT(""); }
FWorldState UPlanningSystem::GetWorldState() const { return WorldState; }

bool UPlanningSystem::IsConditionSatisfied(const FString& Condition) const
{
    if (Condition.StartsWith(TEXT("!")))
        return !WorldState.BooleanFacts.Contains(Condition.RightChop(1)) || !WorldState.BooleanFacts[Condition.RightChop(1)];
    return WorldState.BooleanFacts.Contains(Condition) && WorldState.BooleanFacts[Condition];
}

void UPlanningSystem::UpdateGoalProgress(float DeltaTime)
{
    for (FGoal& G : Goals)
    {
        if (G.Status == EGoalStatus::Active)
        {
            int32 Sat = 0;
            for (const FString& C : G.SuccessConditions) if (IsConditionSatisfied(C)) Sat++;
            G.Progress = G.SuccessConditions.Num() > 0 ? float(Sat) / G.SuccessConditions.Num() : 0;
        }
    }
}

void UPlanningSystem::UpdatePlanExecution(float DeltaTime)
{
    // Find the currently executing plan
    int32 PlanIndex = FindPlanIndex(CurrentPlanID);
    if (PlanIndex < 0) return;

    FPlan& CurrentPlan = Plans[PlanIndex];
    if (CurrentPlan.Status != EPlanStatus::Executing) return;

    // Check if we've completed all actions
    if (CurrentPlan.CurrentActionIndex >= CurrentPlan.Actions.Num())
    {
        CurrentPlan.Status = EPlanStatus::Completed;
        OnPlanStatusChanged.Broadcast(CurrentPlanID, EPlanStatus::Completed);
        CurrentPlanID.Empty();
        return;
    }

    FPlannedAction& CurrentAction = CurrentPlan.Actions[CurrentPlan.CurrentActionIndex];

    // Start action if pending
    if (CurrentAction.Status == EActionStatus::Pending)
    {
        // Check preconditions before starting
        bool bCanStart = true;
        for (const FString& Precondition : CurrentAction.Preconditions)
        {
            if (!IsConditionSatisfied(Precondition))
            {
                bCanStart = false;
                break;
            }
        }

        if (bCanStart)
        {
            CurrentAction.Status = EActionStatus::Executing;
            CurrentActionStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
            OnActionStarted.Broadcast(CurrentAction);
        }
        else
        {
            // Preconditions not met - attempt replan
            CurrentPlan.Status = EPlanStatus::Replanning;
            OnPlanStatusChanged.Broadcast(CurrentPlanID, EPlanStatus::Replanning);
            CurrentPlan.ReplanCount++;

            // Generate new action sequence
            int32 GoalIndex = FindGoalIndex(CurrentPlan.TargetGoalID);
            if (GoalIndex >= 0)
            {
                TArray<FPlannedAction> NewActions = GenerateActionSequence(Goals[GoalIndex]);
                if (NewActions.Num() > 0)
                {
                    CurrentPlan.Actions = NewActions;
                    CurrentPlan.CurrentActionIndex = 0;
                    CurrentPlan.Status = EPlanStatus::Executing;
                    OnPlanStatusChanged.Broadcast(CurrentPlanID, EPlanStatus::Executing);
                }
                else
                {
                    CurrentPlan.Status = EPlanStatus::Failed;
                    OnPlanStatusChanged.Broadcast(CurrentPlanID, EPlanStatus::Failed);
                }
            }
        }
    }
    else if (CurrentAction.Status == EActionStatus::Executing)
    {
        // Track execution time
        float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
        CurrentAction.ActualDuration = CurrentTime - CurrentActionStartTime;

        // Check for action timeout
        if (ActionTimeout > 0.0f && CurrentAction.ActualDuration > ActionTimeout)
        {
            CompleteCurrentAction(false);  // Action timed out - mark as failed
        }
    }
}
void UPlanningSystem::CheckGoalConditions()
{
    for (FGoal& G : Goals)
        if (G.Status == EGoalStatus::Active && IsGoalAchieved(G.GoalID))
        {
            G.Status = EGoalStatus::Achieved;
            OnGoalStatusChanged.Broadcast(G.GoalID, EGoalStatus::Achieved);
        }
}

TArray<FPlannedAction> UPlanningSystem::GenerateActionSequence(const FGoal& Goal)
{
    TArray<FPlannedAction> Actions;
    int32 Seq = 0;
    for (const FString& C : Goal.SuccessConditions)
    {
        if (!IsConditionSatisfied(C))
        {
            for (const FActionTemplate& T : ActionTemplates)
            {
                if (T.Effects.Contains(C))
                {
                    FPlannedAction A;
                    A.ActionID = GenerateActionID();
                    A.ActionName = T.ActionName;
                    A.Effects = T.Effects;
                    A.SequenceIndex = Seq++;
                    A.TargetGoalID = Goal.GoalID;
                    Actions.Add(A);
                    break;
                }
            }
        }
    }
    return Actions;
}

bool UPlanningSystem::CanApplyAction(const FActionTemplate& Action) const
{
    for (const FString& P : Action.Preconditions) if (!IsConditionSatisfied(P)) return false;
    return true;
}

void UPlanningSystem::ApplyActionEffects(const FPlannedAction& Action)
{
    for (const FString& E : Action.Effects) SetBooleanFact(E, true);
}

float UPlanningSystem::ComputeGoalUtility(const FGoal& Goal) const
{
    return float(uint8(Goal.Priority)) * 0.5f + Goal.Urgency * 0.3f + Goal.Importance * 0.2f;
}

void UPlanningSystem::PrioritizeGoals()
{
    Goals.Sort([this](const FGoal& A, const FGoal& B) { return ComputeGoalUtility(A) > ComputeGoalUtility(B); });
}

FString UPlanningSystem::GenerateGoalID() { return FString::Printf(TEXT("GOAL_%d"), ++GoalIDCounter); }
FString UPlanningSystem::GeneratePlanID() { return FString::Printf(TEXT("PLAN_%d"), ++PlanIDCounter); }
FString UPlanningSystem::GenerateActionID() { return FString::Printf(TEXT("ACT_%d"), ++ActionIDCounter); }

int32 UPlanningSystem::FindGoalIndex(const FString& GoalID) const
{
    for (int32 i = 0; i < Goals.Num(); ++i) if (Goals[i].GoalID == GoalID) return i;
    return -1;
}

int32 UPlanningSystem::FindPlanIndex(const FString& PlanID) const
{
    for (int32 i = 0; i < Plans.Num(); ++i) if (Plans[i].PlanID == PlanID) return i;
    return -1;
}
