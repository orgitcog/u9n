// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tasks/SceneStateTask.h"
#include "SceneStateBindingUtils.h"
#include "SceneStateExecutionContext.h"
#include "SceneStateObject.h"
#include "SceneStateUtils.h"
#include "Tasks/SceneStateTaskBindingExtension.h"
#include "Tasks/SceneStateTaskInstance.h"

namespace UE::SceneState::Private
{

EExecutionStatus GetTaskStatus(ESceneStateTaskStopReason InStopReason)
{
	switch (InStopReason)
	{
	case ESceneStateTaskStopReason::State:
		return EExecutionStatus::NotStarted;

	case ESceneStateTaskStopReason::Finished:
		return EExecutionStatus::Finished;
	}

	checkNoEntry();
	return EExecutionStatus::Finished;
}

} // UE::SceneState::Private

#if WITH_EDITOR
const UScriptStruct* FSceneStateTask::GetTaskInstanceType() const
{
	return OnGetTaskInstanceType();
}

void FSceneStateTask::BuildTaskInstance(UObject* InOuter, FStructView InTaskInstance) const
{
	if (InTaskInstance.IsValid())
	{
		OnBuildTaskInstance(InOuter, InTaskInstance);
	}
}
#endif

const FSceneStateTaskBindingExtension* FSceneStateTask::GetBindingExtension() const
{
	if (EnumHasAllFlags(TaskFlags, ESceneStateTaskFlags::HasBindingExtension))
	{
		return OnGetBindingExtension();
	}
	return nullptr;
}

FStructView FSceneStateTask::FindTaskInstance(const FSceneStateExecutionContext& InContext) const
{
	return InContext.FindTaskInstance(GetTaskIndex());
}

void FSceneStateTask::Setup(const FSceneStateExecutionContext& InContext, FStructView InTaskInstance) const
{
	using namespace UE::SceneState;

	QUICK_SCOPE_CYCLE_COUNTER(SceneState_OnSetup);

	FSceneStateTaskInstance* Instance = InTaskInstance.GetPtr<FSceneStateTaskInstance>();
	if (!Instance)
	{
		return;
	}

	Instance->Status = EExecutionStatus::NotStarted;
	OnSetup(InContext, InTaskInstance);
}

void FSceneStateTask::Start(const FSceneStateExecutionContext& InContext, FStructView InTaskInstance) const
{
	using namespace UE::SceneState;

	QUICK_SCOPE_CYCLE_COUNTER(SceneState_OnStart);

	FSceneStateTaskInstance* Instance = InTaskInstance.GetPtr<FSceneStateTaskInstance>();
	if (!Instance || Instance->Status != EExecutionStatus::NotStarted)
	{
		return;
	}

	Instance->Status = EExecutionStatus::Running;
	Instance->Result = ESceneStateTaskResult::Undetermined;
	Instance->ElapsedTime = 0.f;

	ApplyBindings(InContext, InTaskInstance);

	OnStart(InContext, InTaskInstance);
}

void FSceneStateTask::Tick(const FSceneStateExecutionContext& InContext, FStructView InTaskInstance, float InDeltaSeconds) const
{
	using namespace UE::SceneState;

	QUICK_SCOPE_CYCLE_COUNTER(SceneState_OnTick);

	FSceneStateTaskInstance* Instance = InTaskInstance.GetPtr<FSceneStateTaskInstance>();
	if (!Instance || Instance->Status != EExecutionStatus::Running)
	{
		return;
	}

	// Even if Task doesn't tick the virtual/bp functions, keep elapsed time tracked
	Instance->ElapsedTime += InDeltaSeconds;

	if (EnumHasAllFlags(TaskFlags, ESceneStateTaskFlags::Ticks))
	{
		OnTick(InContext, InTaskInstance, InDeltaSeconds);
	}
}

void FSceneStateTask::Stop(const FSceneStateExecutionContext& InContext, FStructView InTaskInstance, ESceneStateTaskStopReason InStopReason) const
{
	using namespace UE::SceneState;

	QUICK_SCOPE_CYCLE_COUNTER(SceneState_OnStop);

	FSceneStateTaskInstance* Instance = InTaskInstance.GetPtr<FSceneStateTaskInstance>();
	if (!Instance)
	{
		return;
	}

	if (Instance->Status == EExecutionStatus::Running)
	{
		OnStop(InContext, InTaskInstance, InStopReason);
	}

	Instance->ElapsedTime = 0.f;
	Instance->Status = Private::GetTaskStatus(InStopReason);
}

void FSceneStateTask::Finish(const FSceneStateExecutionContext& InContext, FStructView InTaskInstance) const
{
	Stop(InContext, InTaskInstance, ESceneStateTaskStopReason::Finished);
}

void FSceneStateTask::SetFlags(ESceneStateTaskFlags InFlags)
{
	TaskFlags |= InFlags;
}

void FSceneStateTask::ClearFlags(ESceneStateTaskFlags InFlags)
{
	TaskFlags &= ~InFlags;
}

bool FSceneStateTask::ApplyBindings(const FSceneStateExecutionContext& InContext, FStructView InTaskInstance) const
{
	QUICK_SCOPE_CYCLE_COUNTER(SceneStateTask_ApplyBindings);

	const FSceneStateBindingCollection* const BindingCollection = InContext.GetBindingCollection();
	if (!BindingCollection)
	{
		return false;
	}

	auto FindDataView =
		[&InContext](const FSceneStateBindingDataHandle& InDataHandle)
		{
			return InContext.FindDataView(InDataHandle);
		};

	UE::SceneState::FApplyBatchParams ApplyBatchParams
		{
			.BatchIndex = BindingsBatch.Get(),
			.TargetDataView = InTaskInstance,
			.BindingCollection = *BindingCollection,
			.FindSourceDataViewFunctor = FindDataView,
		};

	bool bResult = ApplyBatch(ApplyBatchParams);

	if (const FSceneStateTaskBindingExtension* BindingExtension = GetBindingExtension())
	{
		BindingExtension->VisitBindingBatches(InTaskInstance,
			[&bResult, &ApplyBatchParams](uint16 InBatchIndex, FStructView InTargetDataView)
			{
				ApplyBatchParams.BatchIndex = InBatchIndex;
				ApplyBatchParams.TargetDataView = InTargetDataView;
				bResult &= ApplyBatch(ApplyBatchParams);
			});
	}

	return bResult;
}
