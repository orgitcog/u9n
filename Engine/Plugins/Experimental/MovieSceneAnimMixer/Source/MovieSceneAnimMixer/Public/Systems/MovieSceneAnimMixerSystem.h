// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EntitySystem/MovieSceneEntitySystem.h"
#include "MovieSceneMixedAnimationTarget.h"
#include "StructUtils/InstancedStruct.h"
#include "Containers/ContainersFwd.h"
#include "Containers/Map.h"
#include "Templates/SharedPointer.h"
#include "AnimSequencerInstanceProxy.h"
#include "Systems/MovieSceneRootMotionSystem.h"
#include "EvaluationVM/Tasks/BlendKeyframes.h"
#include "MovieSceneAnimMixerSystem.generated.h"

struct FAnimNextEvaluationTask;

namespace UE
{
	namespace AnimNext
	{
		struct FEvaluationProgram;
	}
}

struct FMovieSceneAnimMixer;

struct FMovieSceneAnimMixerEntry
{
	
	TWeakPtr<FMovieSceneAnimMixer> WeakMixer;
	TSharedPtr<FAnimNextEvaluationTask> EvalTask;
	/** Shared pointer to the root motion for this entry if it came from a FMovieSceneRootMotionSettings on a mixer
	 * @note: Only to be used for lifetime management to keep FMovieSceneAnimMixer::WeakRootMotion alive!
	 *        This may be null even if the result of the mix still has root motion */
	TSharedPtr<FMovieSceneMixerRootMotionComponentData> RootMotionLifetimeReference;
	TOptional<FMovieSceneRootMotionSettings> RootMotionSettings;
	double PoseWeight = 1.0;
	int32 Priority = 0;
	bool bAdditive = false;
	bool bRequiresBlend = true;
	UE::MovieScene::FMovieSceneEntityID EntityID;
	UE::MovieScene::FInstanceHandle InstanceHandle;


	FORCEINLINE bool operator<(const FMovieSceneAnimMixerEntry& RHS) const
	{
		if (Priority == RHS.Priority)
		{
			// Sort additives after absolutes so they are applied on top
			if (bAdditive != RHS.bAdditive)
			{
				return RHS.bAdditive;
			}
			return false;
		}
		return Priority < RHS.Priority;
	}
};

// Key into the mixer map- one mixer per bound object per animation target

struct FMovieSceneAnimMixerKey
{
	FObjectKey BoundObjectKey;
	TInstancedStruct<FMovieSceneMixedAnimationTarget> Target;

	/** Equality operator */
	friend bool operator==(const FMovieSceneAnimMixerKey& A, const FMovieSceneAnimMixerKey& B)
	{
		return A.BoundObjectKey == B.BoundObjectKey && A.Target == B.Target;
	}

	/** Generate a type hash from this component */
	friend uint32 GetTypeHash(const FMovieSceneAnimMixerKey& AnimMixerKey)
	{
		return HashCombineFast(GetTypeHash(AnimMixerKey.BoundObjectKey), AnimMixerKey.Target.IsValid() ? AnimMixerKey.Target.GetScriptStruct()->GetStructTypeHash(&AnimMixerKey.Target.Get()) : INDEX_NONE);
	}
};

// A single anim mixer, containing an array of mixer entries sorted by priority.

struct FMovieSceneAnimMixer
{
	UE::MovieScene::FMovieSceneEntityID MixerEntityID;
	TArray<TWeakPtr<FMovieSceneAnimMixerEntry>> WeakEntries;
	TSharedPtr<UE::AnimNext::FEvaluationProgram> EvaluationProgram;
	TWeakPtr<FMovieSceneMixerRootMotionComponentData> WeakRootMotion;

	bool bNeedsResort = false;
};


/*
 * Sequencer weighted average addition blend task
 *
 * This happens to be identical to FAnimNextBlendAddKeyframeWithScaleTask except the operatnds are reversed,
 *     so the resulting stack state is:

 * Top = (Top-1) + (Top * ScaleFactor)
 * 
 * Note that rotations will not be normalized after this task.
 */
USTRUCT()
struct FMovieSceneAccumulateAbsoluteBlendTask : public FAnimNextEvaluationTask
{
	GENERATED_BODY()

	DECLARE_ANIM_EVALUATION_TASK(FMovieSceneAccumulateAbsoluteBlendTask)

	static FMovieSceneAccumulateAbsoluteBlendTask Make(float ScaleFactor);

	// Task entry point
	virtual void Execute(UE::AnimNext::FEvaluationVM& VM) const override;

	// The scale factor to apply to the keyframe
	UPROPERTY()
	float ScaleFactor = 0.0f;
};

USTRUCT()
struct FAnimNextBlendTwoKeyframesPreserveRootMotionTask : public FAnimNextBlendTwoKeyframesTask
{
	GENERATED_BODY()

	DECLARE_ANIM_EVALUATION_TASK(FAnimNextBlendTwoKeyframesPreserveRootMotionTask)

	static FAnimNextBlendTwoKeyframesPreserveRootMotionTask Make(float InterpolationAlpha);

	// Task entry point
	virtual void Execute(UE::AnimNext::FEvaluationVM& VM) const override;
};



// Takes in evaluation tasks with optional pose weight, masks, priority and a given animation target.
// Constructs a hierarchical 'mixer' per bound object per target. 
// Similar to blender systems, in a 'many to one' operation, each mixer will create an entity with a single evaluation task
// wrapping the full blend operation, with the target component. 
// This entity is then consumed by the appropriate target animation system in order to produce the result on the mesh.
UCLASS(MinimalAPI)
class UMovieSceneAnimMixerSystem
	: public UMovieSceneEntitySystem
{
public:

	GENERATED_BODY()

	UMovieSceneAnimMixerSystem(const FObjectInitializer& ObjInit);

	static TInstancedStruct<FMovieSceneMixedAnimationTarget> ResolveAnimationTarget(FObjectKey ObjectKey, const TInstancedStruct<FMovieSceneMixedAnimationTarget>& InTarget);

	TSharedPtr<FMovieSceneMixerRootMotionComponentData> FindRootMotion(FMovieSceneAnimMixerKey InMixerKey) const;
	void AssignRootMotion(FMovieSceneAnimMixerKey InMixerKey, TSharedPtr<FMovieSceneMixerRootMotionComponentData> RootMotion);

	void InitializeAllRootMotion();

private:

	// Map of animation mixers
	TMap<FMovieSceneAnimMixerKey, TSharedPtr<FMovieSceneAnimMixer>> Mixers;

	virtual void OnLink() override;
	virtual bool IsRelevantImpl(UMovieSceneEntitySystemLinker* InLinker) const override;
	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;
	virtual void OnSchedulePersistentTasks(UE::MovieScene::IEntitySystemScheduler* TaskScheduler) override;
	virtual void OnCleanTaggedGarbage() override;

	UPROPERTY()
	TObjectPtr<UMovieSceneRootMotionSystem> RootMotionSystem;
};