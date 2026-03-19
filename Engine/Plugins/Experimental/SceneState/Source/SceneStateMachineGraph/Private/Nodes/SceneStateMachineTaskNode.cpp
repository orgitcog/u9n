// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nodes/SceneStateMachineTaskNode.h"
#include "SceneStateBindingDelegates.h"
#include "SceneStateBindingUtils.h"
#include "SceneStateMachineGraphSchema.h"
#include "SceneStateUtils.h"
#include "Tasks/SceneStateBlueprintableTask.h"
#include "Tasks/SceneStateBlueprintableTaskWrapper.h"
#include "Tasks/SceneStateTaskBindingExtension.h"
#include "Tasks/SceneStateTaskDesc.h"
#include "Tasks/SceneStateTaskDescRegistry.h"

#define LOCTEXT_NAMESPACE "SceneStateMachineTaskNode"

USceneStateMachineTaskNode::USceneStateMachineTaskNode()
{
	NodeName = TEXT("Task");
	NodeType = UE::SceneState::Graph::EStateMachineNodeType::Task;

	bCanRenameNode = false;

	UE::SceneState::OnStructIdChanged.AddUObject(this, &USceneStateMachineTaskNode::OnStructIdChanged);
}

bool USceneStateMachineTaskNode::SetTaskStruct(const UScriptStruct* InTaskStruct)
{
	if (Task.GetScriptStruct() != InTaskStruct)
	{
		if (InTaskStruct)
		{
			Task.InitializeAsScriptStruct(InTaskStruct);
		}
		else
		{
			Task.Reset();
		}
		OnTaskTypeChanged();
		return true;
	}
	return false;
}

void USceneStateMachineTaskNode::SetTaskBlueprintClass(TSubclassOf<USceneStateBlueprintableTask> InTaskClass)
{
	bool bTaskChanged = false;

	if (Task.GetScriptStruct() != FSceneStateBlueprintableTaskWrapper::StaticStruct())
	{
		Task.InitializeAsScriptStruct(FSceneStateBlueprintableTaskWrapper::StaticStruct());
		bTaskChanged = true;
	}

	FSceneStateBlueprintableTaskWrapper& TaskWrapper = Task.GetMutable<FSceneStateBlueprintableTaskWrapper>();
	bTaskChanged |= TaskWrapper.SetTaskClass(InTaskClass);

	if (bTaskChanged)
	{
		OnTaskTypeChanged();
	}
}

bool USceneStateMachineTaskNode::FindDataViewById(const FGuid& InStructId, FStructView& OutDataView)
{
	if (TaskId == InStructId)
	{
		OutDataView = GetTaskInstance();
		return true;
	}

	const FSceneStateTask* ResolvedTask = Task.GetPtr<FSceneStateTask>();
	if (!ResolvedTask)
	{
		return false;
	}

	if (const FSceneStateTaskBindingExtension* BindingExtension = ResolvedTask->GetBindingExtension())
	{
		uint16 DataIndex;
		return BindingExtension->FindDataById(GetTaskInstance(), InStructId, OutDataView, /*out*/DataIndex);
	}

	return false;
}

void USceneStateMachineTaskNode::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, USceneStateMachineGraphSchema::PC_Task, USceneStateMachineGraphSchema::PN_In);
	CreatePin(EGPD_Output, USceneStateMachineGraphSchema::PC_Task, USceneStateMachineGraphSchema::PN_Out);

	// Hide pins that should be hidden
	HidePins(MakeArrayView(&USceneStateMachineGraphSchema::PN_In, 1));
}

FText USceneStateMachineTaskNode::GetNodeTitle(ENodeTitleType::Type InTitleType) const
{
	if (CachedNodeTitle.IsEmpty())
	{
		if (Task.IsValid())
		{
			const FSceneStateTaskDesc& TaskDesc = FSceneStateTaskDescRegistry::Get().GetTaskDesc(Task.GetScriptStruct());

			const FSceneStateTaskDescContext Context = MakeContext();

			FText Tooltip = TaskDesc.GetTooltip(Context);
			if (!Tooltip.IsEmpty())
			{
				Tooltip = FText::Format(INVTEXT("\n{0}"), Tooltip);
			}

			CachedNodeTitle = FText::Format(LOCTEXT("NodeTitle", "{0}{1}"), TaskDesc.GetDisplayName(Context), Tooltip);
		}
		else
		{
			CachedNodeTitle = LOCTEXT("NodeTitle_InvalidTaskClass", "Invalid Task Class");
		}
	}

	return CachedNodeTitle;
}

FText USceneStateMachineTaskNode::GetTooltipText() const
{
	const FSceneStateTaskDesc& TaskDesc = FSceneStateTaskDescRegistry::Get().GetTaskDesc(Task.GetScriptStruct());
	return TaskDesc.GetTooltip(MakeContext());
}

bool USceneStateMachineTaskNode::CanDuplicateNode() const
{
	return true;
}

void USceneStateMachineTaskNode::PostPasteNode()
{
	Super::PostPasteNode();
	GenerateNewTaskId();
}

UObject* USceneStateMachineTaskNode::GetJumpTargetForDoubleClick() const
{
	const FSceneStateTaskDesc& TaskDesc = FSceneStateTaskDescRegistry::Get().GetTaskDesc(Task.GetScriptStruct());
	return TaskDesc.GetJumpTarget(MakeContext());
}

void USceneStateMachineTaskNode::PostLoad()
{
	Super::PostLoad();

	// Hide pins that should be hidden
	HidePins(MakeArrayView(&USceneStateMachineGraphSchema::PN_In, 1));
}

void USceneStateMachineTaskNode::BeginDestroy()
{
	Super::BeginDestroy();
	UE::SceneState::OnStructIdChanged.RemoveAll(this);
}

void USceneStateMachineTaskNode::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	CachedNodeTitle = FText::GetEmpty();

	if (InPropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_CHECKED(USceneStateMachineTaskNode, Task))
	{
		OnTaskTypeChanged();
	}

	OnPostEditTaskDelegate.Broadcast();
}

void USceneStateMachineTaskNode::PostDuplicate(bool bInDuplicateForPIE)
{
	Super::PostDuplicate(bInDuplicateForPIE);

	if (!bInDuplicateForPIE)
	{
		GenerateNewTaskId();
		RebuildTaskInstance();
	}
}

void USceneStateMachineTaskNode::PostEditImport()
{
	Super::PostEditImport();
	GenerateNewTaskId();
	RebuildTaskInstance();
}

FSceneStateTaskDescContext USceneStateMachineTaskNode::MakeContext() const
{
	FSceneStateTaskDescContext Context;
	Context.Task = Task;
	Context.TaskInstance = TaskInstance;
	Context.ContextObject = this;
	return Context;
}

FSceneStateTaskDescMutableContext USceneStateMachineTaskNode::MakeMutableContext()
{
	FSceneStateTaskDescMutableContext Context;
	Context.Task = Task;
	Context.TaskInstance = TaskInstance;
	Context.ContextObject = this;
	return Context;
}

void USceneStateMachineTaskNode::RebuildTaskInstance()
{
	if (const FSceneStateTask* ResolvedTask = Task.GetPtr<FSceneStateTask>())
	{
		FStructView TaskInstanceView(TaskInstance.GetScriptStruct(), TaskInstance.GetMutableMemory());
		ResolvedTask->BuildTaskInstance(this, TaskInstanceView);
	}
}

void USceneStateMachineTaskNode::OnTaskTypeChanged()
{
	if (const FSceneStateTask* ResolvedTask = Task.GetPtr<FSceneStateTask>())
	{
		if (const UScriptStruct* InstanceDataType = ResolvedTask->GetTaskInstanceType())
		{
			if (TaskInstance.GetScriptStruct() != InstanceDataType)
			{
				TaskInstance.InitializeAsScriptStruct(InstanceDataType);

				FStructView TaskInstanceView(TaskInstance.GetScriptStruct(), TaskInstance.GetMutableMemory());
				ResolvedTask->BuildTaskInstance(this, TaskInstanceView);
			}
		}
		else
		{
			TaskInstance.Reset();
		}
	}
	else
	{
		TaskInstance.Reset();
	}
}

void USceneStateMachineTaskNode::OnStructIdChanged(const UE::SceneState::FStructIdChange& InChange)
{
	// Only consider changes if they come from a binding owner that contains this task node
	if (IsIn(InChange.BindingOwner))
	{
		const FSceneStateTaskDesc& TaskDesc = FSceneStateTaskDescRegistry::Get().GetTaskDesc(Task.GetScriptStruct());
		TaskDesc.NotifyStructIdsChanged(MakeMutableContext(), InChange);
	}
}

void USceneStateMachineTaskNode::GenerateNewTaskId()
{
	const FGuid OldTaskId = TaskId;
	TaskId = FGuid::NewGuid();

	UE::SceneState::HandleStructIdChanged(*this, OldTaskId, TaskId);
}

#undef LOCTEXT_NAMESPACE
