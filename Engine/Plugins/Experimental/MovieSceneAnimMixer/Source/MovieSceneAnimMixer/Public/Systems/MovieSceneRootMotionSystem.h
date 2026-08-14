// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EntitySystem/MovieSceneEntitySystem.h"
#include "EntitySystem/MovieSceneEntityIDs.h"
#include "MovieSceneAnimationMixerTrack.h"
#include "AnimSequencerInstanceProxy.h"
#include "EvaluationVM/EvaluationTask.h"
#include "Tickable.h"
#include "MovieSceneRootMotionSystem.generated.h"

enum class EMovieSceneRootMotionDestination : uint8;

class UMovieSceneAnimMixerSystem;
class UMovieSceneRootMotionSystem;


USTRUCT()
struct FSkeletalAnimationRootMotionOverride
{
	GENERATED_BODY()
	
	UPROPERTY()
	FTransform RootMotion = FTransform::Identity;

	UPROPERTY()
	int32 ChildBoneIndex = INDEX_NONE;

	/** If true we use the ChildBoneIndex otherwise we use the root*/
	UPROPERTY()
	bool bBlendFirstChildOfRoot = false;
};

// Structure used for animation tracks to communicate to the mixer how they would like root motion handled if at all.
struct FMovieSceneRootMotionSettings
{
	FVector RootLocation = FVector(EForceInit::ForceInit);
	FVector RootOriginLocation = FVector(EForceInit::ForceInit);
	FVector RootOverrideLocation = FVector(EForceInit::ForceInit);

	FQuat   RootRotation = FQuat(EForceInit::ForceInit);
	FQuat   RootOverrideRotation = FQuat(EForceInit::ForceInit);

	int32 ChildBoneIndex = INDEX_NONE;

	// What space to apply root motion in. Defaults to animation space.
	EMovieSceneRootMotionSpace RootMotionSpace = EMovieSceneRootMotionSpace::AnimationSpace;
	EMovieSceneRootMotionTransformMode TransformMode = EMovieSceneRootMotionTransformMode::Offset;
	ESwapRootBone LegacySwapRootBone = ESwapRootBone::SwapRootBone_None;

	uint8 bHasRootMotionOverride : 1 = 0;
	/** If true we use the ChildBoneIndex otherwise we use the root*/
	uint8 bBlendFirstChildOfRoot : 1 = 0;
};

// Structure that is shared between entities for handling the mixer's root motion result. 
// As this can get read/written from multiple threads, access is threadsafe.
struct FMovieSceneMixerRootMotionComponentData
{
public:

	TOptional<FQuat> GetInverseMeshToActorRotation() const;

	void Initialize();

public:

	TWeakObjectPtr<USceneComponent> OriginalBoundObject;

	// Where to apply the root motion
	TWeakObjectPtr<USceneComponent> Target;

	// EntityID for the anim mixer
	UE::MovieScene::FMovieSceneEntityID MixerEntityID;

	EMovieSceneRootMotionDestination RootDestination;

	FTransform ActorTransform;
	FTransform ComponentToActorTransform;

private:

	// Optional inverse mesh component to actor rotation used to offset any mesh component rotation where applicable.
	TOptional<FQuat> InverseMeshToActorRotation;

	mutable FTransactionallySafeRWLock RootMotionLock;

public:

	bool bComponentSpaceRoot = false;
};

// Task that converts the root motion attribute on the top pose of the pose stack to world space by adding on the actor transformation, root base transform, and/or transform origin.
USTRUCT()
struct FAnimNextConvertRootMotionToWorldSpaceTask : public FAnimNextEvaluationTask
{
	GENERATED_BODY()

	DECLARE_ANIM_EVALUATION_TASK(FAnimNextConvertRootMotionToWorldSpaceTask)

	/** Enum specifying which conversions to perform */
	enum class ESpaceConversions : uint8
	{
		None = 0,
		/** Convert the root motion from animation space to world space */
		AnimationToWorld = 1 << 0,
		/** Convert the root motion from transform origin space to world space (used when there is no transform track in Sequencer) */
		TransformOriginToWorld = 1 << 1,
		/** Convert the root motion from component -> actor space using the inverse component rotation only */
		ComponentToActorRotation = 1 << 2,
		/** Compensate for component rotation and translation offsets when applying root motion in world space */
		WorldSpaceComponentTransformCompensation = 1 << 3,
		/** Apply RootBaseTransform as an offset around RootOffsetOrigin where Root = Root * RootBaseTransform */
		RootTransformOffset = 1 << 4,
		/** Completely override the root transform with RootTransform */
		RootTransformOverride = 1 << 5,
	};

	FAnimNextConvertRootMotionToWorldSpaceTask() = default;
	FAnimNextConvertRootMotionToWorldSpaceTask(const TSharedPtr<FMovieSceneMixerRootMotionComponentData>& InRootMotionData, const FTransform& InTransformOrigin, const FTransform& InRootTransform, const FVector& InRootOffsetOrigin, ESpaceConversions InConversions);

	static FAnimNextConvertRootMotionToWorldSpaceTask Make(const TSharedPtr<FMovieSceneMixerRootMotionComponentData>& InRootMotionData, const FTransform& InTransformOrigin, const FTransform& InRootTransform, const FVector& InRootOffsetOrigin, ESpaceConversions InConversions);

	virtual void Execute(UE::AnimNext::FEvaluationVM& VM) const override;

	/** Weak pointer to the root motion data for all mixed animations. May be null if only a transform origin transformation is required. */
	TWeakPtr<FMovieSceneMixerRootMotionComponentData> WeakRootMotionData;
	/** Base transformation to apply to the root in Actor space, before transform origins. Can be used in place of a transform track. */
	FTransform RootTransform;
	/** Transform origin to apply to the root, if Conversions & ESpaceConversions::TransformOriginToWorld */
	FTransform TransformOrigin;
	/** Origin around which to apply RootTransform when space conversion is RootTransformOffset. */
	FVector RootOffsetOrigin;
	/** Bitmask specifying the conversions to perform */
	ESpaceConversions Conversions;
};
ENUM_CLASS_FLAGS(FAnimNextConvertRootMotionToWorldSpaceTask::ESpaceConversions);

// Task that gets the final mixed root transform and stores it in the root motion data for later application.
// TODO: it's not ideal that we're writing things outside of the animation system during an evaluation task. 
// Consider refactoring this once we have a way to hook into anim next post execution
USTRUCT()
struct FAnimNextStoreRootTransformTask : public FAnimNextEvaluationTask
{
	GENERATED_BODY()

	DECLARE_ANIM_EVALUATION_TASK(FAnimNextStoreRootTransformTask)

	FAnimNextStoreRootTransformTask() = default;
	FAnimNextStoreRootTransformTask(const TSharedPtr<FMovieSceneMixerRootMotionComponentData>& InRootMotionData, bool bInComponentHasKeyedTransform, bool bInRootComponentHasKeyedTransform);

	static FAnimNextStoreRootTransformTask Make(const TSharedPtr<FMovieSceneMixerRootMotionComponentData>& InRootMotionData, bool bInComponentHasKeyedTransform, bool bInRootComponentHasKeyedTransform);

	virtual void Execute(UE::AnimNext::FEvaluationVM& VM) const override;

	TWeakPtr<FMovieSceneMixerRootMotionComponentData> WeakRootMotionData;
	bool bComponentHasKeyedTransform = false;
	bool bRootComponentHasKeyedTransform = false;
};


// Takes in evaluation tasks on mixers.
// Mixes just the root motion attributes.
// Converts it from animation space to either additive actor or component space (based on which attribute used).
// Writes it out as an additive transform to be mixed alongside other transform track values.
UCLASS(MinimalAPI)
class UMovieSceneRootMotionSystem
	: public UMovieSceneEntitySystem
{
public:

	GENERATED_BODY()

	UMovieSceneRootMotionSystem(const FObjectInitializer& ObjInit);

	bool IsTransformKeyed(const FObjectKey& Object) const;

private:

	virtual void OnLink() override;
	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;
	virtual void OnSchedulePersistentTasks(UE::MovieScene::IEntitySystemScheduler* TaskScheduler) override;

	TSet<FObjectKey> ObjectsWithTransforms;
};
