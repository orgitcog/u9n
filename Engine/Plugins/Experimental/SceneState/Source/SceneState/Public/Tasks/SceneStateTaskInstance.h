// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SceneStateEnums.h"
#include "SceneStateTaskInstance.generated.h"

struct FPropertyBindingDataView;

/** Represents the Instance Data of a Task */
USTRUCT()
struct FSceneStateTaskInstance
{
	GENERATED_BODY()

	SCENESTATE_API FSceneStateTaskInstance();

	/** Elapsed Time */
	float ElapsedTime = 0.f;

	/** Current status of the Task */
	UE::SceneState::EExecutionStatus Status = UE::SceneState::EExecutionStatus::NotStarted;

	/** Result of the Task, when finished, otherwise undetermined */
	ESceneStateTaskResult Result = ESceneStateTaskResult::Undetermined;

	uint16 GetInstanceId() const
	{
		return InstanceId;
	}

private:
	/** The id for this instance. This is used to differentiate instances for the same task */
	uint16 InstanceId;
};
