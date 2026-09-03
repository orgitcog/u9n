// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneStateGeneratedClass.h"
#include "Misc/EnumerateRange.h"
#include "SceneStateBindingUtils.h"
#include "SceneStateEventSchema.h"
#include "SceneStateExecutionContext.h"
#include "SceneStateExecutionContextRegistry.h"
#include "SceneStateLog.h"
#include "SceneStateMachine.h"
#include "SceneStateObject.h"
#include "SceneStateUtils.h"
#include "StructUtils/UserDefinedStruct.h"
#include "Tasks/SceneStateTask.h"
#include "Tasks/SceneStateTaskBindingExtension.h"
#include "Tasks/SceneStateTaskInstance.h"

#if WITH_EDITOR
#include "StructUtils/UserDefinedStruct.h"
#include "StructUtilsDelegates.h"
#endif

USceneStateGeneratedClass::USceneStateGeneratedClass()
{
#if WITH_EDITOR
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		OnObjectsReinstancedHandle = FCoreUObjectDelegates::OnObjectsReinstanced.AddUObject(this, &USceneStateGeneratedClass::OnObjectsReinstanced);
		OnStructsReinstancedHandle = UE::StructUtils::Delegates::OnUserDefinedStructReinstanced.AddUObject(this, &USceneStateGeneratedClass::OnStructsReinstanced);
	}
#endif
}

const FSceneState* USceneStateGeneratedClass::GetRootState() const
{
	if (States.IsValidIndex(RootStateIndex))
	{
		return &States[RootStateIndex];	
	}
	return nullptr;
}

const FSceneStateMachine* USceneStateGeneratedClass::FindStateMachine(const FGuid& InStateMachineId) const
{
	const uint16* StateMachineIndex = StateMachineIdToIndex.Find(InStateMachineId);
	if (StateMachineIndex && StateMachines.IsValidIndex(*StateMachineIndex))
	{
		return &StateMachines[*StateMachineIndex];
	}
	return nullptr;
}

FInstancedPropertyBag USceneStateGeneratedClass::GetTransitionParameter(uint16 InAbsoluteIndex) const
{
	return TransitionParameters.FindRef(InAbsoluteIndex);
}

void USceneStateGeneratedClass::Link(FArchive& Ar, bool bInRelinkExistingProperties)
{
	Super::Link(Ar, bInRelinkExistingProperties);

	if (IsFullClass())
	{
		for (TEnumerateRef<FSceneStateTransition> Transition : EnumerateRange(Transitions))
		{
			if (TransitionLinks.IsValidIndex(Transition.GetIndex()))
			{
				Transition->Link(TransitionLinks[Transition.GetIndex()], this);
			}
		}

		for (TEnumerateRef<FSceneStateConduit> Conduit : EnumerateRange(Conduits))
		{
			if (ConduitLinks.IsValidIndex(Conduit.GetIndex()))
			{
				Conduit->Link(ConduitLinks[Conduit.GetIndex()], this);
			}
		}
	}
}

void USceneStateGeneratedClass::PostLoad()
{
	Super::PostLoad();
	ResolveBindings();
}

void USceneStateGeneratedClass::BeginDestroy()
{
	Super::BeginDestroy();
	Reset();
}

#if WITH_EDITOR
const FSceneState* USceneStateGeneratedClass::FindStateFromNode(FObjectKey InStateNode) const
{
	const uint16* StateIndex = StateNodeToIndex.Find(InStateNode);
	if (StateIndex && States.IsValidIndex(*StateIndex))
	{
		return &States[*StateIndex];
	}
	return nullptr;
}

const FSceneStateTask* USceneStateGeneratedClass::FindTaskFromNode(FObjectKey InTaskNode) const
{
	const uint16* TaskIndex = TaskNodeToIndex.Find(InTaskNode);
	if (TaskIndex && Tasks.IsValidIndex(*TaskIndex))
	{
		return Tasks[*TaskIndex].GetPtr<const FSceneStateTask>();
	}
	return nullptr;
}

void USceneStateGeneratedClass::ForEachStateInstance(USceneStateObject& InRootState, FObjectKey InStateNode, TFunctionRef<void(const FSceneStateInstance&)> InFunctor) const
{
	const uint16* StateIndex = StateNodeToIndex.Find(InStateNode);
	if (!StateIndex)
	{
		return;
	}

	const TSharedRef<UE::SceneState::FExecutionContextRegistry> ContextRegistry = InRootState.GetContextRegistry();

	ContextRegistry->ForEachExecutionContext(
		[StateIndex, &InFunctor](const FSceneStateExecutionContext& InExecutionContext)
		{
			if (const FSceneStateInstance* StateInstance = InExecutionContext.FindStateInstance(*StateIndex))
			{
				InFunctor(*StateInstance);
			}
		});
}

void USceneStateGeneratedClass::ForEachTaskInstance(USceneStateObject& InRootState, FObjectKey InTaskNode, TFunctionRef<void(const FSceneStateTaskInstance&)> InFunctor) const
{
	const uint16* TaskIndex = TaskNodeToIndex.Find(InTaskNode);
	if (!TaskIndex)
	{
		return;
	}

	const TSharedRef<UE::SceneState::FExecutionContextRegistry> ContextRegistry = InRootState.GetContextRegistry();

	ContextRegistry->ForEachExecutionContext(
		[TaskIndex, &InFunctor](const FSceneStateExecutionContext& InExecutionContext)
		{
			if (const FSceneStateTaskInstance* TaskInstance = InExecutionContext.FindTaskInstance(*TaskIndex).GetPtr<const FSceneStateTaskInstance>())
			{
				InFunctor(*TaskInstance);
			}
		});
}
#endif

const UStruct* USceneStateGeneratedClass::FindDataStruct(const FSceneStateBindingDataHandle& InDataHandle)
{
	const uint16 DataIndex = InDataHandle.GetDataIndex();
	const uint16 DataSubIndex = InDataHandle.GetDataSubIndex();

	if (InDataHandle.IsExternalDataType())
	{
		// Todo: unsupported external sources
		return nullptr;
	}

	const ESceneStateDataType DataType = static_cast<ESceneStateDataType>(InDataHandle.GetDataType());

	switch (DataType)
	{
	case ESceneStateDataType::Root:
		return this;

	case ESceneStateDataType::Task:
		return TaskInstances[DataIndex].GetScriptStruct();

	case ESceneStateDataType::EventHandler:
		return EventHandlers[DataIndex].GetEventSchemaHandle().GetEventStruct();

	case ESceneStateDataType::TaskExtension:
		if (const FSceneStateTask* Task = Tasks[DataIndex].GetPtr<const FSceneStateTask>())
		{
			if (const FSceneStateTaskBindingExtension* BindingExtension = Task->GetBindingExtension())
			{
				FStructView DataView;
				if (BindingExtension->FindDataByIndex(TaskInstances[DataIndex], DataSubIndex, DataView))
				{
					return DataView.GetScriptStruct();
				}
			}
		}
		break;

	case ESceneStateDataType::Transition:
		return TransitionParameters[DataIndex].GetPropertyBagStruct();

	case ESceneStateDataType::StateMachine:
		return StateMachines[DataIndex].GetParametersStruct();
	}

	return nullptr;
}

void USceneStateGeneratedClass::ResolveBindings()
{
	if (!IsFullClass())
	{
		return;	
	}

	UE::SceneState::PatchBindingCollection({ .BindingCollection = BindingCollection, .FindDataStructFunctor =
		[this](const FSceneStateBindingDataHandle& InDataHandle)->const UStruct*
		{
			return FindDataStruct(InDataHandle);
		}});

	// Resolves property paths used by bindings a store property pointers
	if (!BindingCollection.ResolvePaths())
	{
		UE_LOG(LogSceneState, Warning, TEXT("%s failed to resolve bindings. Try compiling Scene State Blueprint again"), *GetName());
	}
}

void USceneStateGeneratedClass::Reset()
{
	Tasks.Reset();
	TaskPrerequisites.Reset();
	TaskInstances.Reset();
	States.Reset();
	Conduits.Reset();
	ConduitLinks.Reset();
	EventHandlers.Reset();
	Transitions.Reset();
	TransitionLinks.Reset();
	TransitionParameters.Reset();
	StateMachines.Reset();
	BindingCollection.Reset();
	StateMachineIdToIndex.Reset();

#if WITH_EDITOR
	TaskMetadata.Reset();
	StateMetadata.Reset();
	TransitionMetadata.Reset();
	StateNodeToIndex.Reset();
	StateMachineGraphToIndex.Reset();
	TaskNodeToIndex.Reset();
#endif
}

bool USceneStateGeneratedClass::IsFullClass() const
{
#if WITH_EDITOR
	FNameBuilder ClassName(GetFName());
	FStringView ClassNameView = ClassName.ToView();
	return !ClassNameView.StartsWith(TEXT("SKEL_")) && !ClassNameView.StartsWith(TEXT("REINST_"));
#else
	return true;
#endif
}

#if WITH_EDITOR
void USceneStateGeneratedClass::OnObjectsReinstanced(const TMap<UObject*, UObject*>& InReplacementMap)
{
	TSet<const UStruct*> Structs;
	Structs.Reserve(InReplacementMap.Num());

	bool bRequiresResolve = false;

	for (const TPair<UObject*, UObject*>& Pair : InReplacementMap)
	{
		if (const UObject* Replacement = Pair.Value)
		{
			if (Replacement->IsIn(this))
			{
				bRequiresResolve = true;
				break;
			}
			Structs.Add(Replacement->GetClass());
		}
	}

	if (bRequiresResolve || BindingCollection.ContainsAnyStruct(Structs))
	{
		ResolveBindings();
	}
}

void USceneStateGeneratedClass::OnStructsReinstanced(const UUserDefinedStruct& InStruct)
{
	TSet<const UStruct*> Structs;
	Structs.Add(&InStruct);

	if (BindingCollection.ContainsAnyStruct(Structs))
	{
		ResolveBindings();
	}
}
#endif
