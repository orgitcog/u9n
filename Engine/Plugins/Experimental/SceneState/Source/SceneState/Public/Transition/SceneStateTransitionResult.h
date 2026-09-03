// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SceneStateTransitionResult.generated.h"

USTRUCT(BlueprintInternalUseOnly)
struct FSceneStateTransitionResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scene State Transition")
	bool bCanTransition = true;
};
