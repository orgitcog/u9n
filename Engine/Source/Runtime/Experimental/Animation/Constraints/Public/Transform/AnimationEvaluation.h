// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/SkeletalMeshComponent.h"
#include "Containers/Map.h"
#include "Delegates/IDelegateInstance.h"
#include "Math/Transform.h"
#include "UObject/WeakObjectPtrTemplates.h"

namespace UE::Anim
{

/**
 * FAnimationEvaluationTask allows post-evaluation functions to be added to an evaluator, so that data not cached in FAnimationEvaluationContext
 * can be handled by external mechanisms.
*/
	
struct FAnimationEvaluationTask
{
	bool IsValid(const USkeletalMeshComponent* InSkeletalMeshComponent) const
	{
		return Guid.IsValid() && PostEvaluationFunction.IsSet() && InSkeletalMeshComponent && InSkeletalMeshComponent == SkeletalMeshComponent;
	}

	FGuid Guid;
	TWeakObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent = nullptr;
	TFunction<void()> PostEvaluationFunction;
};
	
/**
 * FAnimationEvaluator allows evaluating the animation of a skeletal mesh without having to tick it directly.
 * USkeletalMeshComponent::TickAnimation & USkeletalMeshComponent::RefreshBoneTransforms update internal data, in particular bones transforms,
 * which leads to several side effects (motion blur for example) as well as poor performance.
 * This structure provides a cached equivalent of USkeletalMeshComponent::RefreshBoneTransforms, which is only updated on demand.
 * It's also bound to USkeletalMeshComponent::OnBoneTransformsFinalizedMC so that it's updated with the real values once the skeletal mesh is 
 * finally up to date.
*/
	
struct FAnimationEvaluator
{
	FAnimationEvaluator(USkeletalMeshComponent* InSkeletalMeshComponent);
	~FAnimationEvaluator();

	/** Clears the context and refresh the bones transforms if bRefreshBoneTransforms is true. */ 
	void Update(const bool bRefreshBoneTransforms);
	
	/** Returns true if the skeletal mesh component, its skeletal mesh and the transforms are valid for use. */
	bool IsValid() const;
	
	/** Returns InSocketName component space transform composed with the skeletal mesh component's global transform. */
	FTransform GetGlobalTransform(const FName InSocketName) const;

	/** Adds a post-evaluation task to this evaluator if it has not already been added. */
	void AddPostEvaluationTask(const FAnimationEvaluationTask& InTask);
	
private:

	FAnimationEvaluator() = default;

	/** Prepares the animation context for evaluation. */
	void UpdateContext();
	
	/** Evaluates the animation using the Context. */
	void EvaluateAnimation();

	/** Evaluates the animation and store the bones transforms. */
	void RefreshBoneTransforms();

	/** Updates the bones transforms once the skeletal mesh is finally up to date. */
	void BoneTransformsFinalized();
	
	TWeakObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent = nullptr;
	FAnimationEvaluationContext Context;
	FDelegateHandle OnBoneTransformsFinalizedHandle;
	TMap<FGuid, FAnimationEvaluationTask> PostEvaluationTasks;
};

/**
 * FAnimationEvaluationCache provides a cache mechanism connecting skeletal mesh component and its animation evaluator to avoid multiplying
 * FAnimationEvaluator instances when they all refer to the same skeletal mesh component. The animation evaluator is built lazily and dirtied
 * manually on demand  that the next call to GetEvaluator actually evaluates the animation if needed.  
*/
	
struct FAnimationEvaluationCache
{
public:
	static FAnimationEvaluationCache& Get();
	~FAnimationEvaluationCache();

	/** Marks the evaluator related to InSkeletalMeshComponent for evaluation. */
	void MarkForEvaluation(const USkeletalMeshComponent* InSkeletalMeshComponent);
	
	/**
	 * Returns and up-to-date evaluator to be queried.
	 * Note that calling this function will actually evaluate the animation if the evaluator has been previously dirtied.
	 */
	const FAnimationEvaluator& GetEvaluator(USkeletalMeshComponent* InSkeletalMeshComponent);
	
	/**
	 * Returns and up-to-date evaluator to be queried and adds the post-evaluation task if it has not already been added.
	 * Note that calling this function will actually evaluate the animation and the post-evaluation tasks if the evaluator has been previously dirtied.
	 */
	const FAnimationEvaluator& GetEvaluator(USkeletalMeshComponent* InSkeletalMeshComponent, const FAnimationEvaluationTask& InTask);

private:
	FAnimationEvaluationCache() = default;

	/** Listen to the constraint manager notification to avoid keeping useless or invalid data. */
	void RegisterNotifications();

	/** Stop listening to the constraint manager notification system. */
	void UnregisterNotifications();

	/** List of connected skeletal mesh components and animation evaluators. */
	TMap<USkeletalMeshComponent*, FAnimationEvaluator> PerSkeletalMeshEvaluator;

	FDelegateHandle ConstraintsNotificationHandle;
};

}