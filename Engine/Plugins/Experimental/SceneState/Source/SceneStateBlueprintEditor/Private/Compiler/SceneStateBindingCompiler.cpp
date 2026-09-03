// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneStateBindingCompiler.h"
#include "SceneStateBlueprint.h"
#include "SceneStateBlueprintCompilerContext.h"
#include "SceneStateEventSchema.h"
#include "SceneStateGeneratedClass.h"
#include "StructUtils/UserDefinedStruct.h"
#include "Tasks/SceneStateBlueprintableTask.h"
#include "Tasks/SceneStateTaskBindingExtension.h"

#define LOCTEXT_NAMESPACE "SceneStateBindingCompiler"

namespace UE::SceneState::Editor
{

void FBindingCompiler::Compile()
{
	check(Blueprint && Class);

	// Initial estimate count of the binding descs
	const int32 BindingDescInitialCount = 1 + Class->Tasks.Num() + Class->EventHandlers.Num() + Class->TransitionParameters.Num();

	// Copy Editor Bindings and reserve for the binding descs to add
	Class->BindingCollection.Bindings = Blueprint->BindingCollection.Bindings;
	Class->BindingCollection.BindingDescs.Empty(BindingDescInitialCount);

	ValidBindingMap.Reserve(BindingDescInitialCount);

	AddRootBindingDesc();
	AddStateMachineBindingDescs();
	AddTransitionBindingDescs();
	AddTaskBindingDescs();
	AddEventHandlerBindingDescs();

	RemoveInvalidBindings();

	ResolveBindingDataHandles();
	BatchCopies();
}

void FBindingCompiler::AddBindingDesc(FSceneStateBindingDesc&& InBindingDesc)
{
	// Record this binding in the valid binding map. This will be used to then remove all the invalid bindings from the collection
	// The binding data view can be filled in with null memory, as only the struct is required for resolution
	ValidBindingMap.Add(InBindingDesc.ID, FPropertyBindingDataView(InBindingDesc.Struct, nullptr));

	Class->BindingCollection.BindingDescs.Add(MoveTemp(InBindingDesc));
}

void FBindingCompiler::AddRootBindingDesc()
{
	FSceneStateBindingDesc BindingDesc = Blueprint->CreateRootBinding();

	// Set the Struct to the New Class during compilation
	BindingDesc.Struct = Class;
	BindingDesc.DataHandle = FSceneStateBindingDataHandle(ESceneStateDataType::Root);

	AddBindingDesc(MoveTemp(BindingDesc));
}

void FBindingCompiler::AddStateMachineBindingDescs()
{
	for (const TPair<FGuid, uint16>& Pair : Class->StateMachineIdToIndex)
	{
		const uint16 StateMachineIndex = Pair.Value;

		const FSceneStateMachine& StateMachine = Class->StateMachines[StateMachineIndex];
		if (!StateMachine.Parameters.IsValid())
		{
			continue;
		}

		FSceneStateBindingDesc BindingDesc;
		BindingDesc.ID = Pair.Key;
		BindingDesc.Name = TEXT("State Machine Parameters");
		BindingDesc.Struct = StateMachine.Parameters.GetPropertyBagStruct();
		BindingDesc.DataHandle = FSceneStateBindingDataHandle(ESceneStateDataType::StateMachine, StateMachineIndex);

		AddBindingDesc(MoveTemp(BindingDesc));
	}
}

void FBindingCompiler::AddTransitionBindingDescs()
{
	for (const TPair<uint16, FInstancedPropertyBag>& Pair : Class->TransitionParameters)
	{
		const uint16 TransitionIndex = Pair.Key;

		const FSceneStateTransitionMetadata& TransitionMetadata = Class->TransitionMetadata[TransitionIndex];

		FSceneStateBindingDesc BindingDesc;
		BindingDesc.ID = TransitionMetadata.ParametersId;
		BindingDesc.Name = TEXT("Transition Parameters");
		BindingDesc.Struct = Pair.Value.GetPropertyBagStruct();
		BindingDesc.DataHandle = FSceneStateBindingDataHandle(ESceneStateDataType::Transition, TransitionIndex);

		AddBindingDesc(MoveTemp(BindingDesc));
	}
}

void FBindingCompiler::AddTaskBindingDescs()
{
	check(Class->Tasks.Num() == Class->TaskMetadata.Num());

	TArray<FSceneStateBindingDesc>& BindingDescs = Class->BindingCollection.BindingDescs;

	for (int32 TaskIndex = 0; TaskIndex < Class->Tasks.Num(); ++TaskIndex)
	{
		const FSceneStateTask& Task = Class->Tasks[TaskIndex].Get<FSceneStateTask>();
		const FSceneStateTaskMetadata& TaskMetadata = Class->TaskMetadata[TaskIndex];
		const FConstStructView TaskInstance = Class->TaskInstances[TaskIndex];

		// Task Instance Binding
		{
			FSceneStateBindingDesc BindingDesc;
			BindingDesc.ID = TaskMetadata.TaskId;
			BindingDesc.Name = FName(TEXT("Task"), TaskIndex);
			BindingDesc.Struct = TaskInstance.GetScriptStruct();
			BindingDesc.DataHandle = FSceneStateBindingDataHandle(ESceneStateDataType::Task, TaskIndex);

			AddBindingDesc(MoveTemp(BindingDesc));
		}

		const FSceneStateTaskBindingExtension* BindingExtension = Task.GetBindingExtension();
		if (!BindingExtension)
		{
			continue;
		}

		TSet<uint16> DataIndices;
		BindingExtension->VisitBindingDescs(TaskInstance,
			[&BindingDescs, TaskIndex, &DataIndices, This = this](const FTaskBindingDesc& InBindingDesc)
			{
				bool bAlreadyInSet = false;
				DataIndices.Add(InBindingDesc.DataIndex, &bAlreadyInSet);
				checkf(!bAlreadyInSet, TEXT("Data Index already being used by another Desc in this Task!"));

				FSceneStateBindingDesc BindingDesc;
				BindingDesc.ID = InBindingDesc.Id;
				BindingDesc.Name = InBindingDesc.Name;
				BindingDesc.Struct = InBindingDesc.Struct;
				BindingDesc.DataHandle = FSceneStateBindingDataHandle(ESceneStateDataType::TaskExtension, TaskIndex, InBindingDesc.DataIndex);

				This->AddBindingDesc(MoveTemp(BindingDesc));
			});
	}
}

void FBindingCompiler::AddEventHandlerBindingDescs()
{
	for (int32 EventHandlerIndex = 0; EventHandlerIndex < Class->EventHandlers.Num(); ++EventHandlerIndex)
	{
		const FSceneStateEventHandler& EventHandler = Class->EventHandlers[EventHandlerIndex];

		const USceneStateEventSchemaObject* EventSchema = EventHandler.GetEventSchemaHandle().GetEventSchema();
		if (!EventSchema)
		{
			continue;
		}

		FSceneStateBindingDesc BindingDesc;
		BindingDesc.ID = EventHandler.GetHandlerId();
		BindingDesc.Name = EventSchema->Name;
		BindingDesc.Struct = EventSchema->Struct;
		BindingDesc.DataHandle = FSceneStateBindingDataHandle(ESceneStateDataType::EventHandler, EventHandlerIndex);

		AddBindingDesc(MoveTemp(BindingDesc));
	}
}

bool FBindingCompiler::ValidateBinding(const FSceneStateBinding& InBinding) const
{
	// Source must be accessible by the target struct.
	// This mismatch could happen if copying an object bound to a scoped parameter and pasting it outside such scope
	TArray<TInstancedStruct<FPropertyBindingBindableStructDescriptor>> AccessibleStructs;
	Blueprint->GetBindableStructs(InBinding.GetTargetPath().GetStructID(), AccessibleStructs);

	const FGuid& SourceStructId = InBinding.GetSourcePath().GetStructID();
	const bool bSourceAccessible = AccessibleStructs.ContainsByPredicate(
		[&SourceStructId](TConstStructView<FPropertyBindingBindableStructDescriptor> InBindableStruct)
		{
			return InBindableStruct.Get().ID == SourceStructId;
		});

	if (!bSourceAccessible)
	{
		const FText ErrorMessageFormat = FText::Format(LOCTEXT("InaccessibleSourceError"
			, "Source '{0}' cannot be bound to target '{1}' because it's inaccessible (in another scope)")
			, FText::FromString(InBinding.GetSourcePath().ToString())
			, FText::FromString(InBinding.GetTargetPath().ToString()));

		Context.MessageLog.Error(*ErrorMessageFormat.ToString());
		return false;
	}

	if (!InBinding.SourceDataHandle.IsValid())
	{
		const FText ErrorMessageFormat = FText::Format(LOCTEXT("InvalidSourceHandleError"
			, "Source '{0}' data handle was not found")
			, FText::FromString(InBinding.GetSourcePath().ToString()));

		Context.MessageLog.Error(*ErrorMessageFormat.ToString());
		return false;
	}

	if (!InBinding.TargetDataHandle.IsValid())
	{
		const FText ErrorMessageFormat = FText::Format(LOCTEXT("InvalidTargetHandleError"
			, "Target '{0}' data handle was not found")
			, FText::FromString(InBinding.GetTargetPath().ToString()));

		Context.MessageLog.Error(*ErrorMessageFormat.ToString());
		return false;
	}

	return true;
}

void FBindingCompiler::ResolveBindingDataHandles()
{
	for (TArray<FSceneStateBinding>::TIterator Iter(Class->BindingCollection.Bindings); Iter; ++Iter)
	{
		FSceneStateBinding& Binding = *Iter;
		Binding.SourceDataHandle = GetDataHandleById(Binding.GetSourcePath().GetStructID());
		Binding.TargetDataHandle = GetDataHandleById(Binding.GetTargetPath().GetStructID());

		if (!ValidateBinding(Binding))
		{
			Iter.RemoveCurrentSwap();
		}
	}
}

void FBindingCompiler::RemoveInvalidBindings()
{
	// Remove all the bindings not present in the valid binding map
	Blueprint->BindingCollection.RemoveInvalidBindings(ValidBindingMap);
	Class->BindingCollection.RemoveInvalidBindings(ValidBindingMap);
}

void FBindingCompiler::BatchCopies()
{
	// To batch first sort the bindings so all the same target data handles are together
	Class->BindingCollection.Bindings.StableSort(
		[](const FSceneStateBinding& InLeft, const FSceneStateBinding& InRight)
		{
			return InLeft.TargetDataHandle.AsNumber() < InRight.TargetDataHandle.AsNumber();
		});

	// Add 1 batch per group of bindings that all have the same target data handle
	for (TArray<FSceneStateBinding>::TIterator BindingIter(Class->BindingCollection.Bindings); BindingIter;)
	{
		const FSceneStateBindingDataHandle TargetDataHandle = BindingIter->TargetDataHandle;

		const FSceneStateBindingDesc* TargetDesc = Class->BindingCollection.FindBindingDesc(TargetDataHandle);
		if (!ensure(TargetDesc))
		{
			++BindingIter;
			continue;
		}

		const FPropertyBindingIndex16 BatchBindings(Class->BindingCollection.GetNumCopyBatches());

		FPropertyBindingCopyInfoBatch& CopyBatch = Class->BindingCollection.AddCopyBatch();
		CopyBatch.TargetStruct = TInstancedStruct<FSceneStateBindingDesc>::Make(*TargetDesc);
		CopyBatch.BindingsBegin = FPropertyBindingIndex16(BindingIter.GetIndex());

		// Get the last index that matches the current target data handle
		for (; BindingIter && BindingIter->TargetDataHandle == TargetDataHandle; ++BindingIter)
		{
		}

		// Binding end is one past the last binding.
		CopyBatch.BindingsEnd = FPropertyBindingIndex16(BindingIter.GetIndex());
		OnBindingsBatchCompiled(BatchBindings, TargetDataHandle);
	}
}

void FBindingCompiler::OnBindingsBatchCompiled(FPropertyBindingIndex16 InBindingsBatch, const FSceneStateBindingDataHandle& InTargetDataHandle)
{
	// External Data Types as Targets are not supported by design.
	check(!InTargetDataHandle.IsExternalDataType());

	const ESceneStateDataType DataType = static_cast<ESceneStateDataType>(InTargetDataHandle.GetDataType());
	const uint16 DataIndex = InTargetDataHandle.GetDataIndex();

	if (DataType == ESceneStateDataType::Task)
	{
		FSceneStateTask& Task = Class->Tasks[DataIndex].Get<FSceneStateTask>();
		Task.BindingsBatch = InBindingsBatch;
	}
	else if (DataType == ESceneStateDataType::TaskExtension)
	{
		FSceneStateTask& Task = Class->Tasks[DataIndex].Get<FSceneStateTask>();

		FSceneStateTaskBindingExtension* BindingExtension = const_cast<FSceneStateTaskBindingExtension*>(Task.GetBindingExtension());
		check(BindingExtension);
		BindingExtension->SetBindingBatch(InTargetDataHandle.GetDataSubIndex(), InBindingsBatch.Get());
	}
	else if (DataType == ESceneStateDataType::StateMachine)
	{
		FSceneStateMachine& StateMachine = Class->StateMachines[DataIndex];
		StateMachine.BindingsBatch = InBindingsBatch;
	}
	else if (DataType == ESceneStateDataType::Transition)
	{
		FSceneStateTransition& Transition = Class->Transitions[DataIndex];
		Transition.BindingsBatch = InBindingsBatch;
	}
	else
	{
		// No other Data Types are supported as Targets at the moment
		checkNoEntry();
	}
}

FSceneStateBindingDataHandle FBindingCompiler::GetDataHandleById(const FGuid& InStructId)
{
	if (Blueprint->GetRootId() == InStructId)
	{
		return FSceneStateBindingDataHandle(ESceneStateDataType::Root);
	}

	// Tasks
	for (int32 TaskIndex = 0; TaskIndex < Class->TaskMetadata.Num(); ++TaskIndex)
	{
		const FSceneStateTaskMetadata& TaskMetadata = Class->TaskMetadata[TaskIndex];
		if (TaskMetadata.TaskId == InStructId)
		{
			return FSceneStateBindingDataHandle(ESceneStateDataType::Task, TaskIndex);
		}
	}

	// State Machines
	for (const TPair<FGuid, uint16>& Pair : Class->StateMachineIdToIndex)
	{
		if (Pair.Key == InStructId)
		{
			return FSceneStateBindingDataHandle(ESceneStateDataType::StateMachine, Pair.Value);
		}
	}

	// Transitions
	for (const TPair<uint16, FInstancedPropertyBag>& Pair : Class->TransitionParameters)
	{
		const uint16 TransitionIndex = Pair.Key;

		const FSceneStateTransitionMetadata& TransitionMetadata = Class->TransitionMetadata[TransitionIndex];
		if (TransitionMetadata.ParametersId == InStructId)
		{
			return FSceneStateBindingDataHandle(ESceneStateDataType::Transition, TransitionIndex);
		}
	}

	// Event Handlers
	for (int32 EventHandlerIndex = 0; EventHandlerIndex < Class->EventHandlers.Num(); ++EventHandlerIndex)
	{
		const FSceneStateEventHandler& EventHandler = Class->EventHandlers[EventHandlerIndex];
		if (EventHandler.GetHandlerId() == InStructId)
		{
			return FSceneStateBindingDataHandle(ESceneStateDataType::EventHandler, EventHandlerIndex);
		}
	}

	// Task Custom Data 
	for (int32 TaskIndex = 0; TaskIndex < Class->Tasks.Num(); ++TaskIndex)
	{
		const FSceneStateTask& Task = Class->Tasks[TaskIndex].Get<const FSceneStateTask>();
		const FStructView TaskInstance = Class->TaskInstances[TaskIndex];

		FStructView DataView;
		uint16 DataIndex;

		const FSceneStateTaskBindingExtension* BindingExtension = Task.GetBindingExtension();
		if (BindingExtension && BindingExtension->FindDataById(TaskInstance, InStructId, DataView, DataIndex))
		{
			return FSceneStateBindingDataHandle(ESceneStateDataType::TaskExtension, TaskIndex, DataIndex);
		}
	}

	return FSceneStateBindingDataHandle();
}

} // UE::SceneState::Editor

#undef LOCTEXT_NAMESPACE
