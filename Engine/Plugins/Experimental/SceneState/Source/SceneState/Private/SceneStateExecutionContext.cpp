// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneStateExecutionContext.h"
#include "PropertyBindingDataView.h"
#include "PropertyBindingTypes.h"
#include "SceneState.h"
#include "SceneStateExecutionContextRegistry.h"
#include "SceneStateGeneratedClass.h"
#include "SceneStateLog.h"
#include "SceneStateMachine.h"
#include "SceneStateObject.h"
#include "SceneStateUtils.h"
#include "Tasks/SceneStateTaskBindingExtension.h"
#include "Tasks/SceneStateTaskInstance.h"

const FSceneStateExecutionContext FSceneStateExecutionContext::InvalidContext;

FSceneStateExecutionContext::~FSceneStateExecutionContext()
{
	Reset();
}

TWeakPtr<const UE::SceneState::FExecutionContextRegistry> FSceneStateExecutionContext::GetContextRegistry() const
{
	return ContextRegistryWeak;
}

const FSceneStateBindingCollection* FSceneStateExecutionContext::GetBindingCollection() const
{
	if (GeneratedClass)
	{
		return &GeneratedClass->GetBindingCollection();
	}
	return nullptr;
}

FString FSceneStateExecutionContext::GetExecutionContextName() const
{
	if (RootState)
	{
		return RootState->GetContextName();
	}
	return FString();
}

UObject* FSceneStateExecutionContext::GetContextObject() const
{
	if (RootState)
	{
		return RootState->GetContextObject();
	}
	return nullptr;
}

USceneStateEventStream* FSceneStateExecutionContext::GetEventStream() const
{
	if (RootState)
	{
		return RootState->GetEventStream();
	}
	return nullptr;
}

void FSceneStateExecutionContext::Setup(USceneStateObject* InRootState)
{
	Reset();

	check(InRootState);
	RootState = InRootState;
	GeneratedClass = Cast<USceneStateGeneratedClass>(RootState->GetClass());

	const TSharedRef<UE::SceneState::FExecutionContextRegistry> ContextRegistry = InRootState->GetContextRegistry();
	ContextRegistry->RegisterContext(this);
	ContextRegistryWeak = ContextRegistry;
}

void FSceneStateExecutionContext::Reset()
{
	RootState = nullptr;
	GeneratedClass = nullptr;

	StateInstances.Reset();
	StateMachineInstances.Reset();
	TransitionInstances.Reset();

	if (TSharedPtr<UE::SceneState::FExecutionContextRegistry> ContextRegistry = ContextRegistryWeak.Pin())
	{
		ContextRegistry->UnregisterContext(this);
	}

	ContextRegistryWeak.Reset();
}

FPropertyBindingDataView FSceneStateExecutionContext::FindDataView(const FSceneStateBindingDataHandle& InDataHandle) const
{
	const uint16 DataIndex = InDataHandle.GetDataIndex();
	const uint16 DataSubIndex = InDataHandle.GetDataSubIndex();

	if (InDataHandle.IsExternalDataType())
	{
		// Todo: unsupported external sources
		return FPropertyBindingDataView();
	}

	const ESceneStateDataType DataType = static_cast<ESceneStateDataType>(InDataHandle.GetDataType());

	switch (DataType)
	{
	case ESceneStateDataType::Root:
		return FPropertyBindingDataView(RootState);

	case ESceneStateDataType::Task:
		return FindTaskInstance(DataIndex);

	case ESceneStateDataType::EventHandler:
		if (USceneStateEventStream* EventStream = GetEventStream())
		{
			if (const FSceneStateEventHandler* EventHandler = GetEventHandler(DataIndex))
			{
				if (FSceneStateEvent* Event = EventStream->FindCapturedEvent(EventHandler->GetHandlerId()))
				{
					return Event->GetDataViewMutable();
				}
			}
		}
		break;

	case ESceneStateDataType::TaskExtension:
		if (const FSceneStateTask* Task = FindTask(DataIndex).GetPtr<const FSceneStateTask>())
		{
			if (const FSceneStateTaskBindingExtension* BindingExtension = Task->GetBindingExtension())
			{
				FStructView DataView;
				if (BindingExtension->FindDataByIndex(FindTaskInstance(DataIndex), DataSubIndex, DataView))
				{
					return DataView;
				}
			}
		}
		break;

	case ESceneStateDataType::Transition:
		if (FSceneStateTransitionInstance* TransitionInstance = FindTransitionInstance(DataIndex))
		{
			return TransitionInstance->Parameters.GetMutableValue();
		}
		break;

	case ESceneStateDataType::StateMachine:
		if (FSceneStateMachineInstance* StateMachineInstance = FindStateMachineInstance(DataIndex))
		{
			return StateMachineInstance->Parameters.GetMutableValue();
		}
		break;
	}

	return FPropertyBindingDataView();
}

void FSceneStateExecutionContext::ForEachTask(const FSceneState& InState, TFunctionRef<UE::SceneState::EIterationResult(const FSceneStateTask&, FStructView)> InCallable) const
{
	const FSceneStateRange TaskRange = InState.GetTaskRange();
	if (TaskRange.Count == 0 || !GeneratedClass)
	{
		return;
	}

	FInstancedStructContainer* TaskInstanceContainer = FindTaskInstanceContainer(InState);
	if (!TaskInstanceContainer)
	{
		return;
	}

	const FInstancedStructContainer& Tasks = GeneratedClass->GetTasks();

	checkfSlow(Tasks.IsValidIndex(TaskRange.Index) && Tasks.IsValidIndex(TaskRange.GetLastIndex())
		, TEXT("[Context: %s] State Task Range [%d, %d] is out of bounds! (Total Task Count: %d)")
		, *GetExecutionContextName()
		, TaskRange.Index
		, TaskRange.GetLastIndex()
		, Tasks.Num());

	for (uint16 TaskIndex = 0; TaskIndex < TaskRange.Count; ++TaskIndex)
	{
		if (!TaskInstanceContainer->IsValidIndex(TaskIndex))
		{
			continue;
		}

		const uint16 AbsoluteIndex = TaskRange.Index + TaskIndex;
		const FSceneStateTask& Task = Tasks[AbsoluteIndex].Get<const FSceneStateTask>();
		const FStructView TaskInstance = (*TaskInstanceContainer)[TaskIndex];

		if (InCallable(Task, TaskInstance) == UE::SceneState::EIterationResult::Break)
		{
			break;
		}
	}
}

TArray<FConstStructView> FSceneStateExecutionContext::GetTemplateTaskInstances(const FSceneState& InState) const
{
	const FSceneStateRange TaskRange = InState.GetTaskRange();
	if (TaskRange.Count == 0 || !GeneratedClass)
	{
		return {};
	}
	return UE::SceneState::GetConstStructViews(GeneratedClass->GetTaskInstances(), TaskRange);
}

TConstArrayView<uint16> FSceneStateExecutionContext::GetTaskPrerequisites(const FSceneStateTask& InTask) const
{
	const FSceneStateRange TaskPrerequisiteRange = InTask.GetPrerequisiteRange();
	if (TaskPrerequisiteRange.Count == 0 || !GeneratedClass)
	{
		return {};
	}

	const TConstArrayView<uint16> TaskPrerequisitesView = GeneratedClass->GetTaskPrerequisites();

	checkfSlow(TaskPrerequisitesView.IsValidIndex(TaskPrerequisiteRange.Index) && TaskPrerequisitesView.IsValidIndex(TaskPrerequisiteRange.GetLastIndex())
		, TEXT("[Context: %s] Task Prerequisite Range [%d, %d] is out of bounds! (Total Prerequisite Count: %d)")
		, *GetExecutionContextName()
		, TaskPrerequisiteRange.Index
		, TaskPrerequisiteRange.GetLastIndex()
		, TaskPrerequisitesView.Num());

	return TaskPrerequisitesView.Slice(TaskPrerequisiteRange.Index, TaskPrerequisiteRange.Count);
}

TConstArrayView<FSceneStateTransition> FSceneStateExecutionContext::GetTransitions(const FSceneState& InState) const
{
	const FSceneStateRange TransitionRange = InState.GetTransitionRange();
	if (TransitionRange.Count == 0 || !GeneratedClass)
	{
		return {};
	}

	const TConstArrayView<FSceneStateTransition> TransitionsView = GeneratedClass->GetTransitions();

	checkfSlow(TransitionsView.IsValidIndex(TransitionRange.Index) && TransitionsView.IsValidIndex(TransitionRange.GetLastIndex())
		, TEXT("[Context: %s] State Transition Range [%d, %d] is out of bounds! (Total Transition Count: %d)")
		, *GetExecutionContextName()
		, TransitionRange.Index
		, TransitionRange.GetLastIndex()
		, TransitionsView.Num());

	return TransitionsView.Slice(TransitionRange.Index, TransitionRange.Count);
}

TConstArrayView<FSceneStateTransition> FSceneStateExecutionContext::GetTransitions(const FSceneStateConduit& InConduit) const
{
	const FSceneStateRange TransitionRange = InConduit.GetTransitionRange();
	if (TransitionRange.Count == 0 || !GeneratedClass)
	{
		return {};
	}

	const TConstArrayView<FSceneStateTransition> TransitionsView = GeneratedClass->GetTransitions();

	checkfSlow(TransitionsView.IsValidIndex(TransitionRange.Index) && TransitionsView.IsValidIndex(TransitionRange.GetLastIndex())
		, TEXT("[Context: %s] Conduit Transition Range [%d, %d] is out of bounds! (Total Transition Count: %d)")
		, *GetExecutionContextName()
		, TransitionRange.Index
		, TransitionRange.GetLastIndex()
		, TransitionsView.Num());

	return TransitionsView.Slice(TransitionRange.Index, TransitionRange.Count);
}

FInstancedPropertyBag FSceneStateExecutionContext::GetTemplateTransitionParameter(const FSceneStateTransition& InTransition) const
{
	uint16 TransitionIndex;
	if (GetTransitionIndex(InTransition, TransitionIndex))
	{
		return GeneratedClass->GetTransitionParameter(TransitionIndex);
	}
	return FInstancedPropertyBag();
}

TConstArrayView<FSceneStateMachine> FSceneStateExecutionContext::GetStateMachines(const FSceneState& InState) const
{
	const FSceneStateRange StateMachineRange = InState.GetStateMachineRange();
	if (StateMachineRange.Count == 0 || !GeneratedClass)
	{
		return {};
	}

	const TConstArrayView<FSceneStateMachine> StateMachinesView = GeneratedClass->GetStateMachines();

	checkfSlow(StateMachinesView.IsValidIndex(StateMachineRange.Index) && StateMachinesView.IsValidIndex(StateMachineRange.GetLastIndex())
		, TEXT("[Context: %s] State State Machine Range [%d, %d] is out of bounds! (Total State Machine Count: %d)")
		, *GetExecutionContextName()
		, StateMachineRange.Index
		, StateMachineRange.GetLastIndex()
		, StateMachinesView.Num());

	return StateMachinesView.Slice(StateMachineRange.Index, StateMachineRange.Count);
}

TConstArrayView<FSceneStateEventHandler> FSceneStateExecutionContext::GetEventHandlers(const FSceneState& InState) const
{
	const FSceneStateRange EventHandlerRange = InState.GetEventHandlerRange();
	if (EventHandlerRange.Count == 0 || !GeneratedClass)
	{
		return {};
	}

	const TConstArrayView<FSceneStateEventHandler> EventHandlersView = GeneratedClass->GetEventHandlers();

	checkfSlow(EventHandlersView.IsValidIndex(EventHandlerRange.Index) && EventHandlersView.IsValidIndex(EventHandlerRange.GetLastIndex())
		, TEXT("[Context: %s] State Event Handler Range [%d, %d] is out of bounds! (Total Event Handler Count: %d)")
		, *GetExecutionContextName()
		, EventHandlerRange.Index
		, EventHandlerRange.GetLastIndex()
		, EventHandlersView.Num());

	return EventHandlersView.Slice(EventHandlerRange.Index, EventHandlerRange.Count);
}

#if WITH_EDITOR
const FSceneStateMetadata* FSceneStateExecutionContext::GetStateMetadata(const FSceneState& InState) const
{
	if (!GeneratedClass)
	{
		return nullptr;
	}

	uint16 StateIndex;
	if (!GetStateIndex(InState, StateIndex))
	{
		UE_LOG(LogSceneState, Error, TEXT("[Context: %s] State Index was not able to be retrieved. State Data is invalid!"), *GetExecutionContextName());
		return nullptr;
	}

	const TConstArrayView<FSceneStateMetadata> StateMetadataView = GeneratedClass->GetStateMetadata();

	if (!StateMetadataView.IsValidIndex(StateIndex))
	{
		UE_LOG(LogSceneState, Error, TEXT("[Context: %s] Metadata for State Index %d is not available!"
			"Be sure to have compiled the Scene State Object. Metadata count: %d")
			, *GetExecutionContextName()
			, StateIndex
			, StateMetadataView.Num());
		return nullptr;
	}

	return &StateMetadataView[StateIndex];
}
#endif

const FSceneStateMachine* FSceneStateExecutionContext::GetStateMachine(const FGuid& InStateMachineId) const
{
	if (GeneratedClass)
	{
		return GeneratedClass->FindStateMachine(InStateMachineId);
	}
	return nullptr;
}

const FSceneState* FSceneStateExecutionContext::GetActiveState(const FSceneStateMachine& InStateMachine) const
{
	if (const FSceneStateMachineInstance* Instance = FindStateMachineInstance(InStateMachine))
	{
		return GetState(InStateMachine, Instance->ActiveIndex);
	}
	return nullptr;
}

const FSceneState* FSceneStateExecutionContext::GetState(uint16 InAbsoluteIndex) const
{
	if (!GeneratedClass)
	{
		return nullptr;
	}

	const TConstArrayView<FSceneState> StatesView = GeneratedClass->GetStates();

	checkfSlow(StatesView.IsValidIndex(InAbsoluteIndex)
		, TEXT("[Context: %s] Absolute Index %d is out of bounds! (Total State Count: %d)")
		, *GetExecutionContextName()
		, InAbsoluteIndex
		, StatesView.Num());

	return &StatesView[InAbsoluteIndex];
}

const FSceneStateEventHandler* FSceneStateExecutionContext::GetEventHandler(uint16 InAbsoluteIndex) const
{
	if (!GeneratedClass)
	{
		return nullptr;
	}

	const TConstArrayView<FSceneStateEventHandler> EventHandlers = GeneratedClass->GetEventHandlers();

	checkfSlow(EventHandlers.IsValidIndex(InAbsoluteIndex)
		, TEXT("[Context: %s] Absolute Index %d is out of bounds! (Total Event Handlers Count: %d)")
		, *GetExecutionContextName()
		, InAbsoluteIndex
		, EventHandlers.Num());

	return &EventHandlers[InAbsoluteIndex];
}

const FSceneState* FSceneStateExecutionContext::GetState(const FSceneStateMachine& InStateMachine, uint16 InRelativeIndex) const
{
	const FSceneStateRange StateRange = InStateMachine.GetStateRange();
	if (StateRange.Count == 0 || !GeneratedClass)
	{
		return nullptr;
	}

	const TConstArrayView<FSceneState> StatesView = GeneratedClass->GetStates();

	const uint16 AbsoluteIndex = StateRange.Index + InRelativeIndex;

	checkfSlow(StatesView.IsValidIndex(AbsoluteIndex)
		, TEXT("[Context: %s] State Machine Relative Active Index %d from [%d, %d] is out of bounds! (Total State Count: %d)")
		, *GetExecutionContextName()
		, InRelativeIndex
		, StateRange.Index
		, StateRange.GetLastIndex()
		, StatesView.Num());

	return &StatesView[AbsoluteIndex];
}

const FSceneStateConduit* FSceneStateExecutionContext::GetConduit(const FSceneStateMachine& InStateMachine, uint16 InRelativeIndex) const
{
	const FSceneStateRange ConduitRange = InStateMachine.GetConduitRange();
	if (ConduitRange.Count == 0 || !GeneratedClass)
	{
		return nullptr;
	}

	const TConstArrayView<FSceneStateConduit> ConduitsView = GeneratedClass->GetConduits();

	const uint16 AbsoluteIndex = ConduitRange.Index + InRelativeIndex;

	checkfSlow(ConduitsView.IsValidIndex(AbsoluteIndex)
		, TEXT("[Context: %s] State machine conduit relative index %d from [%d, %d] is out of bounds! (Total conduit count: %d)")
		, *GetExecutionContextName()
		, InRelativeIndex
		, ConduitRange.Index
		, ConduitRange.GetLastIndex()
		, ConduitsView.Num());

	return &ConduitsView[AbsoluteIndex];
}

FSceneStateInstance* FSceneStateExecutionContext::FindOrAddStateInstance(const FSceneState& InState) const
{
	uint16 StateIndex;
	if (GetStateIndex(InState, /*out*/StateIndex))
	{
		return &StateInstances.FindOrAdd(StateIndex);
	}
	return nullptr;
}

FSceneStateInstance* FSceneStateExecutionContext::FindStateInstance(uint16 InAbsoluteIndex) const
{
	return StateInstances.Find(InAbsoluteIndex);
}

FSceneStateInstance* FSceneStateExecutionContext::FindStateInstance(const FSceneState& InState) const
{
	uint16 StateIndex;
	if (GetStateIndex(InState, /*out*/StateIndex))
	{
		return FindStateInstance(StateIndex);
	}
	return nullptr;
}

void FSceneStateExecutionContext::RemoveStateInstance(const FSceneState& InState) const
{
	uint16 StateIndex;
	if (GetStateIndex(InState, /*out*/StateIndex))
	{
		StateInstances.Remove(StateIndex);
	}
}

FConstStructView FSceneStateExecutionContext::FindTask(uint16 InAbsoluteIndex) const
{
	if (!GeneratedClass)
	{
		return {};
	}

	const FInstancedStructContainer& Tasks = GeneratedClass->GetTasks();

	if (!Tasks.IsValidIndex(InAbsoluteIndex))
	{
		return {};
	}

	return Tasks[InAbsoluteIndex];
}

FInstancedStructContainer* FSceneStateExecutionContext::FindOrAddTaskInstanceContainer(const FSceneState& InState) const
{
	uint16 StateIndex;
	if (GetStateIndex(InState, /*out*/StateIndex))
	{
		return &TaskInstanceContainers.FindOrAdd(StateIndex);
	}
	return nullptr;
}

FInstancedStructContainer* FSceneStateExecutionContext::FindTaskInstanceContainer(const FSceneState& InState) const
{
	uint16 StateIndex;
	if (GetStateIndex(InState, /*out*/StateIndex))
	{
		return FindTaskInstanceContainer(StateIndex);
	}
	return nullptr;
}

FInstancedStructContainer* FSceneStateExecutionContext::FindTaskInstanceContainer(uint16 InAbsoluteIndex) const
{
	return TaskInstanceContainers.Find(InAbsoluteIndex);
}

FStructView FSceneStateExecutionContext::FindTaskInstance(uint16 InAbsoluteIndex) const
{
	if (!GeneratedClass)
	{
		return {};
	}

	const FInstancedStructContainer& Tasks = GeneratedClass->GetTasks();

	if (!Tasks.IsValidIndex(InAbsoluteIndex))
	{
		return {};
	}

	const FSceneStateTask& Task = Tasks[InAbsoluteIndex].Get<const FSceneStateTask>();

	const uint16 StateIndex = Task.GetParentStateIndex();

	FInstancedStructContainer* const TaskInstanceContainer = FindTaskInstanceContainer(StateIndex);
	if (!TaskInstanceContainer)
	{
		return {};
	}

	const TConstArrayView<FSceneState> StatesView = GeneratedClass->GetStates();

	checkf(StatesView.IsValidIndex(StateIndex)
		, TEXT("[Context: %s] Task Parent State Index %d is out of bounds! (Total State Count: %d)")
		, *GetExecutionContextName()
		, StateIndex
		, StatesView.Num());

	const FSceneState& State = StatesView[StateIndex];

	const uint16 TaskRelativeIndex = InAbsoluteIndex - State.GetTaskRange().Index;
	if (!TaskInstanceContainer->IsValidIndex(TaskRelativeIndex))
	{
		return {};
	}

	return (*TaskInstanceContainer)[TaskRelativeIndex];
}

void FSceneStateExecutionContext::RemoveTaskInstanceContainer(const FSceneState& InState) const
{
	uint16 StateIndex;
	if (GetStateIndex(InState, /*out*/StateIndex))
	{
		TaskInstanceContainers.Remove(StateIndex);
	}
}

FSceneStateMachineInstance* FSceneStateExecutionContext::FindOrAddStateMachineInstance(const FSceneStateMachine& InStateMachine) const
{
	uint16 StateMachineIndex;
	if (GetStateMachineIndex(InStateMachine, /*out*/StateMachineIndex))
	{
		return &StateMachineInstances.FindOrAdd(StateMachineIndex);
	}
	return nullptr;
}

FSceneStateMachineInstance* FSceneStateExecutionContext::FindStateMachineInstance(uint16 InAbsoluteIndex) const
{
	return StateMachineInstances.Find(InAbsoluteIndex);
}

FSceneStateMachineInstance* FSceneStateExecutionContext::FindStateMachineInstance(const FSceneStateMachine& InStateMachine) const
{
	uint16 StateMachineIndex;
	if (GetStateMachineIndex(InStateMachine, /*out*/StateMachineIndex))
	{
		return FindStateMachineInstance(StateMachineIndex);
	}
	return nullptr;
}

void FSceneStateExecutionContext::RemoveStateMachineInstance(const FSceneStateMachine& InStateMachine) const
{
	uint16 StateMachineIndex;
	if (GetStateMachineIndex(InStateMachine, /*out*/StateMachineIndex))
	{
		StateMachineInstances.Remove(StateMachineIndex);
	}
}

FSceneStateTransitionInstance* FSceneStateExecutionContext::FindOrAddTransitionInstance(const FSceneStateTransition& InTransition) const
{
	uint16 TransitionIndex;
	if (GetTransitionIndex(InTransition, /*out*/TransitionIndex))
	{
		return &TransitionInstances.FindOrAdd(TransitionIndex);
	}
	return nullptr;
}

FSceneStateTransitionInstance* FSceneStateExecutionContext::FindTransitionInstance(uint16 InAbsoluteIndex) const
{
	return TransitionInstances.Find(InAbsoluteIndex);
}

FSceneStateTransitionInstance* FSceneStateExecutionContext::FindTransitionInstance(const FSceneStateTransition& InTransition) const
{
	uint16 TransitionIndex;
	if (GetTransitionIndex(InTransition, /*out*/TransitionIndex))
	{
		return FindTransitionInstance(TransitionIndex);
	}
	return nullptr;
}

void FSceneStateExecutionContext::RemoveTransitionInstance(const FSceneStateTransition& InTransition) const
{
	uint16 TransitionIndex;
	if (GetTransitionIndex(InTransition, /*out*/TransitionIndex))
	{
		TransitionInstances.Remove(TransitionIndex);
	}
}

bool FSceneStateExecutionContext::GetStateIndex(const FSceneState& InState, uint16& OutIndex) const
{
	if (GeneratedClass)
	{
		TConstArrayView<FSceneState> StatesView = GeneratedClass->GetStates();
		const int32 Index = static_cast<int32>(&InState - StatesView.GetData());
		checkSlow(StatesView.IsValidIndex(Index) && Index >= 0 && Index <= TNumericLimits<uint16>::Max());
		OutIndex = static_cast<uint16>(Index);
		return true;
	}
	return false;
}

bool FSceneStateExecutionContext::GetStateMachineIndex(const FSceneStateMachine& InStateMachine, uint16& OutIndex) const
{
	if (GeneratedClass)
	{
		TConstArrayView<FSceneStateMachine> StateMachinesView = GeneratedClass->GetStateMachines();
		const int32 Index = static_cast<int32>(&InStateMachine - StateMachinesView.GetData());
		checkSlow(StateMachinesView.IsValidIndex(Index) && Index >= 0 && Index <= TNumericLimits<uint16>::Max());
		OutIndex = static_cast<uint16>(Index);
		return true;
	}
	return false;
}

bool FSceneStateExecutionContext::GetTransitionIndex(const FSceneStateTransition& InTransition, uint16& OutIndex) const
{
	if (GeneratedClass)
	{
		TConstArrayView<FSceneStateTransition> TransitionsView = GeneratedClass->GetTransitions();
		const int32 Index = static_cast<int32>(&InTransition - TransitionsView.GetData());
		checkSlow(TransitionsView.IsValidIndex(Index) && Index >= 0 && Index <= TNumericLimits<uint16>::Max());
		OutIndex = static_cast<uint16>(Index);
		return true;
	}
	return false;
}
