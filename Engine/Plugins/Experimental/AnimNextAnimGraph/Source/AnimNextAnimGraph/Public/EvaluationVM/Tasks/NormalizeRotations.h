// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EvaluationVM/EvaluationTask.h"

#include "NormalizeRotations.generated.h"

#define UE_API ANIMNEXTANIMGRAPH_API

/*
 * Normalize Keyframe Rotations Task
 *
 * This pop the top keyframe from the VM keyframe stack, it normalizes its rotations, and pushes
 * back the result onto the stack.
 */
USTRUCT()
struct FAnimNextNormalizeKeyframeRotationsTask : public FAnimNextEvaluationTask
{
	GENERATED_BODY()

	DECLARE_ANIM_EVALUATION_TASK(FAnimNextNormalizeKeyframeRotationsTask)

	// Task entry point
	UE_API virtual void Execute(UE::AnimNext::FEvaluationVM& VM) const override;
};

#undef UE_API
