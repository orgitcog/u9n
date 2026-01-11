// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EvaluationVM/EvaluationTask.h"
#include "TransformArray.h"

#include "StoreKeyframe.generated.h"

#define UE_API ANIMNEXTANIMGRAPH_API

/**
 * Swap the two given Transform Arrays
 */
USTRUCT()
struct FAnimNextSwapTransformsTask : public FAnimNextEvaluationTask
{
	GENERATED_BODY()

	DECLARE_ANIM_EVALUATION_TASK(FAnimNextSwapTransformsTask)

	static UE_API FAnimNextSwapTransformsTask Make(UE::AnimNext::FTransformArraySoAHeap* A, UE::AnimNext::FTransformArraySoAHeap* B);

	// Task entry point
	UE_API virtual void Execute(UE::AnimNext::FEvaluationVM& VM) const override;

	UE::AnimNext::FTransformArraySoAHeap* A = nullptr;
	UE::AnimNext::FTransformArraySoAHeap* B = nullptr;
};

/*
 * Store the pose on top of the stack in the given Transform Array
 */
USTRUCT()
struct FAnimNextStoreKeyframeTransformsTask : public FAnimNextEvaluationTask
{
	GENERATED_BODY()

	DECLARE_ANIM_EVALUATION_TASK(FAnimNextStoreKeyframeTransformsTask)

	static UE_API FAnimNextStoreKeyframeTransformsTask Make(UE::AnimNext::FTransformArraySoAHeap* Dest);

	// Task entry point
	UE_API virtual void Execute(UE::AnimNext::FEvaluationVM& VM) const override;

	UE::AnimNext::FTransformArraySoAHeap* Dest = nullptr;
};

/*
 * Duplicates the pose on top of the stack and pushes it as new top
 */
USTRUCT()
struct FAnimNextDuplicateTopKeyframeTask : public FAnimNextEvaluationTask
{
	GENERATED_BODY()

	DECLARE_ANIM_EVALUATION_TASK(FAnimNextDuplicateTopKeyframeTask)

	static UE_API FAnimNextDuplicateTopKeyframeTask Make();

	// Task entry point
	UE_API virtual void Execute(UE::AnimNext::FEvaluationVM& VM) const override;
};

#undef UE_API
