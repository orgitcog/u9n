// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneStateDebugTaskExecutor.h"
#include "Nodes/SceneStateMachineStateNode.h"
#include "Nodes/SceneStateMachineTaskNode.h"
#include "SceneStateGeneratedClass.h"
#include "SceneStateMachineGraphSchema.h"
#include "Tasks/SceneStateTaskInstance.h"

namespace UE::SceneState::Editor
{

const FSceneStateTask* FDebugTaskExecutor::GetTask(const FSceneStateExecutionContext& InExecutionContext) const
{
	if (const USceneStateGeneratedClass* GeneratedClass = InExecutionContext.GetGeneratedClass())
	{
		return GeneratedClass->FindTaskFromNode(GetNodeKey());
	}
	return nullptr;
}

const FSceneState* FDebugTaskExecutor::GetState(const USceneStateGeneratedClass* InGeneratedClass) const
{
	if (const USceneStateMachineTaskNode* const TaskNode = Cast<USceneStateMachineTaskNode>(GetNodeKey().ResolveObjectPtr()))
	{
		if (const USceneStateMachineStateNode* const StateNode = USceneStateMachineGraphSchema::FindConnectedStateNode(TaskNode))
		{
			return InGeneratedClass->FindStateFromNode(StateNode);
		}
	}
	return nullptr;
}

void FDebugTaskExecutor::Setup(const FSceneStateExecutionContext& InExecutionContext, const FSceneStateTask& InTask)
{
	const USceneStateGeneratedClass* GeneratedClass = InExecutionContext.GetGeneratedClass();
	if (!GeneratedClass)
	{
		return;
	}

	const FSceneState* ParentState = GetState(GeneratedClass);
	if (!ParentState)
	{
		return;
	}

	const FSceneStateRange TaskRange = ParentState->GetTaskRange(); 

	const uint16 TaskAbsoluteIndex = InTask.GetTaskIndex();
	const uint16 TaskRelativeIndex = TaskAbsoluteIndex - TaskRange.Index;

	// Make an empty task instance array that matches the state task count,
	// and only set the task instance template for the task this executor cares about
	TArray<FConstStructView> TemplateTaskInstances;
	TemplateTaskInstances.SetNum(TaskRange.Count);
	TemplateTaskInstances[TaskRelativeIndex] = GeneratedClass->GetTaskInstances()[TaskAbsoluteIndex];

	ParentState->AllocateTaskInstances(InExecutionContext, TemplateTaskInstances);
}

void FDebugTaskExecutor::OnStart(const FSceneStateExecutionContext& InExecutionContext)
{
	if (const FSceneStateTask* Task = GetTask(InExecutionContext))
	{
		Setup(InExecutionContext, *Task);

		const FStructView TaskInstance = Task->FindTaskInstance(InExecutionContext);

		Task->Setup(InExecutionContext, TaskInstance);
		Task->Start(InExecutionContext, TaskInstance);
		ConditionallyExit(TaskInstance);
	}
}

void FDebugTaskExecutor::OnTick(const FSceneStateExecutionContext& InExecutionContext, float InDeltaSeconds)
{
	if (const FSceneStateTask* Task = GetTask(InExecutionContext))
	{
		const FStructView TaskInstance = Task->FindTaskInstance(InExecutionContext);

		Task->Tick(InExecutionContext, TaskInstance, InDeltaSeconds);
		ConditionallyExit(TaskInstance);
	}
}

void FDebugTaskExecutor::OnExit(const FSceneStateExecutionContext& InExecutionContext)
{
	if (const FSceneStateTask* Task = GetTask(InExecutionContext))
	{
		const FStructView TaskInstance = Task->FindTaskInstance(InExecutionContext);
		Task->Stop(InExecutionContext, TaskInstance, ESceneStateTaskStopReason::State);
	}
}

void FDebugTaskExecutor::ConditionallyExit(FStructView InTaskInstance)
{
	const FSceneStateTaskInstance* TaskInstance = InTaskInstance.GetPtr<FSceneStateTaskInstance>();
	if (TaskInstance && TaskInstance->Status == EExecutionStatus::Finished)
	{
		Exit();
	}
}

} // UE::SceneState::Editor
