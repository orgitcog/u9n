// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tasks/StateTreeRunParallelStateTreeTask.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeExecutionTypes.h"
#include "StateTreeInstanceData.h"

#define LOCTEXT_NAMESPACE "StateTree"

void FStateTreeRunParallelStateTreeExecutionExtension::ScheduleNextTick(const FContextParameters& Context)
{
	const FStateTreeMinimalExecutionContext ExecutionContext(&Context.Owner, &Context.StateTree, Context.InstanceData);
	const FStateTreeScheduledTick ScheduledTick = ExecutionContext.GetNextScheduledTick();
	WeakExecutionContext.UpdateScheduledTickRequest(ScheduledTickHandle, ScheduledTick);
}

FStateTreeRunParallelStateTreeTask::FStateTreeRunParallelStateTreeTask()
{
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
	bShouldAffectTransitions = true;
	bConsideredForScheduling = false;
}

EStateTreeRunStatus FStateTreeRunParallelStateTreeTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transitions) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const FStateTreeReference& StateTreeToRun = GetStateTreeToRun(Context, InstanceData);
	if (!StateTreeToRun.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	// Find if it's a recursive call. The detection is not perfect. For example: StateTrees with a parallel task that links to each other cannot be detected.
	const bool bInParentContext = Context.GetActiveFrames().ContainsByPredicate([NewTree = StateTreeToRun.GetStateTree()](const FStateTreeExecutionFrame& Frame){ return Frame.StateTree == NewTree; });
	if (bInParentContext)
	{
		UE_LOG(LogStateTree, Warning, TEXT("Trying to start a new parallel tree from the same tree '%s'"), *StateTreeToRun.GetStateTree()->GetName());
		return EStateTreeRunStatus::Failed;
	}

	if (InstanceData.ScheduledTickHandle.IsValid())
	{
		Context.RemoveScheduledTickRequest(InstanceData.ScheduledTickHandle);
	}

	// Share event queue with parent tree.
	if (FStateTreeInstanceData* OuterInstanceData = Context.GetMutableInstanceData())
	{
		InstanceData.TreeInstanceData.SetSharedEventQueue(OuterInstanceData->GetSharedMutableEventQueue());
	}
	
	InstanceData.RunningStateTree = StateTreeToRun.GetStateTree();
	FStateTreeExecutionContext ParallelTreeContext(Context, *InstanceData.RunningStateTree, InstanceData.TreeInstanceData);
	if (!ParallelTreeContext.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	FStateTreeRunParallelStateTreeExecutionExtension Extension;
	Extension.WeakExecutionContext = Context.MakeWeakExecutionContext();
	const EStateTreeRunStatus RunStatus = ParallelTreeContext.Start(FStateTreeExecutionContext::FStartParameters
		{
			.GlobalParameters = &StateTreeToRun.GetParameters(),
			.ExecutionExtension = TInstancedStruct<FStateTreeRunParallelStateTreeExecutionExtension>::Make(MoveTemp(Extension))
		});

	InstanceData.ScheduledTickHandle = Context.AddScheduledTickRequest(ParallelTreeContext.GetNextScheduledTick());
	InstanceData.TreeInstanceData.GetMutableExecutionState()->ExecutionExtension.GetMutable<FStateTreeRunParallelStateTreeExecutionExtension>().ScheduledTickHandle = InstanceData.ScheduledTickHandle;

	return RunStatus;
}

EStateTreeRunStatus FStateTreeRunParallelStateTreeTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.RunningStateTree)
	{
		return EStateTreeRunStatus::Failed;
	}

	FStateTreeExecutionContext ParallelTreeContext(Context, *InstanceData.RunningStateTree, InstanceData.TreeInstanceData);
	if (!ParallelTreeContext.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	const EStateTreeRunStatus RunStatus = ParallelTreeContext.TickUpdateTasks(DeltaTime);
	Context.UpdateScheduledTickRequest(InstanceData.ScheduledTickHandle, ParallelTreeContext.GetNextScheduledTick());
	return RunStatus;
}

void FStateTreeRunParallelStateTreeTask::TriggerTransitions(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.RunningStateTree)
	{
		return;
	}

	FStateTreeExecutionContext ParallelTreeContext(Context, *InstanceData.RunningStateTree, InstanceData.TreeInstanceData);
	if (!ParallelTreeContext.IsValid())
	{
		return;
	}

	ParallelTreeContext.TickTriggerTransitions();
	Context.UpdateScheduledTickRequest(InstanceData.ScheduledTickHandle, ParallelTreeContext.GetNextScheduledTick());
}

void FStateTreeRunParallelStateTreeTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!InstanceData.RunningStateTree)
	{
		return;
	}

	FStateTreeExecutionContext ParallelTreeContext(Context, *InstanceData.RunningStateTree, InstanceData.TreeInstanceData);
	if (!ParallelTreeContext.IsValid())
	{
		return;
	}

	ParallelTreeContext.Stop();
	Context.RemoveScheduledTickRequest(InstanceData.ScheduledTickHandle);
}

const FStateTreeReference& FStateTreeRunParallelStateTreeTask::GetStateTreeToRun(FStateTreeExecutionContext& Context, FInstanceDataType& InstanceData) const
{
	if (StateTreeOverrideTag.IsValid())
	{
		if (const FStateTreeReference* Override = Context.GetLinkedStateTreeOverrideForTag(StateTreeOverrideTag))
		{
			return *Override;
		}
	}

	return InstanceData.StateTree;
}

#if WITH_EDITOR
EDataValidationResult FStateTreeRunParallelStateTreeTask::Compile(UE::StateTree::ICompileNodeContext& Context)
{
	TransitionHandlingPriority = EventHandlingPriority;

	return EDataValidationResult::Valid;
}

void FStateTreeRunParallelStateTreeTask::PostEditInstanceDataChangeChainProperty(const FPropertyChangedChainEvent& PropertyChangedEvent, FStateTreeDataView InstanceDataView)
{
	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(FStateTreeRunParallelStateTreeTaskInstanceData, StateTree))
	{
		InstanceDataView.GetMutable<FInstanceDataType>().StateTree.SyncParameters();
	}
}

void FStateTreeRunParallelStateTreeTask::PostLoad(FStateTreeDataView InstanceDataView)
{
	if (FInstanceDataType* DataType = InstanceDataView.GetMutablePtr<FInstanceDataType>())
	{
		DataType->StateTree.SyncParameters();
	}
}

FText FStateTreeRunParallelStateTreeTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();
	check(InstanceData);

	FText StateTreeValue = BindingLookup.GetBindingSourceDisplayName(FPropertyBindingPath(ID, GET_MEMBER_NAME_CHECKED(FInstanceDataType, StateTree)), Formatting);
	if (StateTreeValue.IsEmpty())
	{
		StateTreeValue = FText::FromString(GetNameSafe(InstanceData->StateTree.GetStateTree()));
	}

	const FText Format = (Formatting == EStateTreeNodeFormatting::RichText)
		? LOCTEXT("RunParallelRich", "<b>Run Parallel</> {Asset}")
		: LOCTEXT("RunParallel", "Run Parallel {Asset}");

	return FText::FormatNamed(Format,
		TEXT("Asset"), StateTreeValue);
}
#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
