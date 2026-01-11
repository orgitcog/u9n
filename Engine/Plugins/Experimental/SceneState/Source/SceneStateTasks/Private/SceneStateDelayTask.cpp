// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneStateDelayTask.h"
#include "Tasks/SceneStateTaskExecutionContext.h"

FSceneStateDelayTask::FSceneStateDelayTask()
{
	SetFlags(ESceneStateTaskFlags::Ticks);
}

#if WITH_EDITOR
const UScriptStruct* FSceneStateDelayTask::OnGetTaskInstanceType() const
{
	return FInstanceDataType::StaticStruct();
}
#endif

void FSceneStateDelayTask::OnStart(const FSceneStateExecutionContext& InContext, FStructView InTaskInstance) const
{
	const FInstanceDataType& Instance = InTaskInstance.Get<FInstanceDataType>();
	if (Instance.Delay < UE_SMALL_NUMBER)
	{
		Finish(InContext, InTaskInstance);
	}
}

void FSceneStateDelayTask::OnTick(const FSceneStateExecutionContext& InContext, FStructView InTaskInstance, float InDeltaSeconds) const
{
	const FInstanceDataType& Instance = InTaskInstance.Get<FInstanceDataType>();
	if (Instance.ElapsedTime >= Instance.Delay)
	{
		Finish(InContext, InTaskInstance);
	}
}
