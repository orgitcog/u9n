// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SceneStateEnums.h"
#include "SceneStateInstance.generated.h"

/** Instance data of a State */
USTRUCT()
struct FSceneStateInstance
{
	GENERATED_BODY()

	/** Time elapsed since the state entered */
	float ElapsedTime = 0.f;

	/** Current status of this state instance */
	UE::SceneState::EExecutionStatus Status = UE::SceneState::EExecutionStatus::NotStarted;
};
