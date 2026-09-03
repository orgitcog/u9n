// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimationEvaluation.h"
#include "HAL/Platform.h"

class USceneComponent;
class USkeletalMeshComponent;

namespace TransformableHandleUtils
{
	/** Returns true if the constraints' evaluation scheme shoud skip ticking skeletal meshes. */
	CONSTRAINTS_API bool SkipTicking();

	/** Force ticking all the skeletal meshes related to this component. */
	CONSTRAINTS_API void TickDependantComponents(USceneComponent* InComponent);
	
	/** Force ticking InSkeletalMeshComponent. */
	CONSTRAINTS_API void TickSkeletalMeshComponent(USkeletalMeshComponent* InSkeletalMeshComponent);

	/** Mark InSceneComponent for animation evaluation. */
	CONSTRAINTS_API void MarkComponentForEvaluation(const USceneComponent* InSceneComponent);
	
	/** Returns an updated version of InSceneComponent's animation evaluator. */
	CONSTRAINTS_API const UE::Anim::FAnimationEvaluator& EvaluateComponent(USceneComponent* InSceneComponent);
	
	/** Returns an updated version of InSceneComponent's animation evaluator and adds the input post-evaluation task if not alread added. */
	CONSTRAINTS_API const UE::Anim::FAnimationEvaluator& EvaluateComponent(
		USceneComponent* InSceneComponent,
		const UE::Anim::FAnimationEvaluationTask& InTask);
}
