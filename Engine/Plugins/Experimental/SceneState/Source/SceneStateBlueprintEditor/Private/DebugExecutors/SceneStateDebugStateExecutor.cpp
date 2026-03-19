// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneStateDebugStateExecutor.h"
#include "SceneStateGeneratedClass.h"

namespace UE::SceneState::Editor
{

const FSceneState* FDebugStateExecutor::GetState(const FSceneStateExecutionContext& InExecutionContext) const
{
	if (const USceneStateGeneratedClass* GeneratedClass = InExecutionContext.GetGeneratedClass())
	{
		return GeneratedClass->FindStateFromNode(GetNodeKey());
	}
	return nullptr;
}

void FDebugStateExecutor::OnStart(const FSceneStateExecutionContext& InExecutionContext)
{
	if (const FSceneState* State = GetState(InExecutionContext))
	{
		State->Enter(InExecutionContext);
		ConditionallyExit(InExecutionContext, *State);
	}
}

void FDebugStateExecutor::OnTick(const FSceneStateExecutionContext& InExecutionContext, float InDeltaSeconds)
{
	if (const FSceneState* State = GetState(InExecutionContext))
	{
		State->Tick(InExecutionContext, InDeltaSeconds);
		ConditionallyExit(InExecutionContext, *State);
	}
}

void FDebugStateExecutor::OnExit(const FSceneStateExecutionContext& InExecutionContext)
{
	if (const FSceneState* State = GetState(InExecutionContext))
	{
		State->Exit(InExecutionContext);
	}
}

void FDebugStateExecutor::ConditionallyExit(const FSceneStateExecutionContext& InExecutionContext, const FSceneState& InState)
{
	if (!InState.HasPendingTasks(InExecutionContext))
	{
		Exit();
	}
}

} // UE::SceneState::Editor
