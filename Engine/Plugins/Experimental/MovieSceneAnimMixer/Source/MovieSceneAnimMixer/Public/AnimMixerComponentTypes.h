// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EntitySystem/BuiltInComponentTypes.h"
#include "EntitySystem/MovieSceneEntityIDs.h"
#include "EntitySystem/MovieScenePropertyMetaDataTraits.h"
#include "EntitySystem/MovieScenePropertySystemTypes.h"
#include "EntitySystem/MovieScenePropertyTraits.h"
#include "MovieSceneMixedAnimationTarget.h"
#include "StructUtils/InstancedStruct.h"
#include "EvaluationVM/EvaluationTask.h"
#include "AnimSequencerInstanceProxy.h"
#include "Systems/MovieSceneRootMotionSystem.h"

enum class EMovieSceneRootMotionDestination : uint8;

struct FMovieSceneAnimMixerEntry;

namespace UE::MovieScene
{
	struct FAnimMixerComponentTypes
	{
		MOVIESCENEANIMMIXER_API static FAnimMixerComponentTypes* Get();
		MOVIESCENEANIMMIXER_API static void Destroy();

		// A root motion transform in animation space to be applied (after space conversion)
		MOVIESCENEANIMMIXER_API static const UE::Anim::FAttributeId RootTransformAttributeId;
		MOVIESCENEANIMMIXER_API static const UE::Anim::FAttributeId RootTransformWeightAttributeId;
		// Internal flag marking a section as authoritative source of root motion.
		// Some sections, e.g. stitch sections, should not have their root motion blended with others, since it's using motion matching to blend into the animation already.
		MOVIESCENEANIMMIXER_API static const UE::Anim::FAttributeId RootTransformIsAuthoritativeAttributeId;

		TComponentTypeID<int32> Priority;
		TComponentTypeID<TInstancedStruct<FMovieSceneMixedAnimationTarget>> Target;
		TComponentTypeID<TSharedPtr<FAnimNextEvaluationTask>> Task;
		TComponentTypeID<TSharedPtr<FAnimNextEvaluationTask>> MixerTask;
		TComponentTypeID<TSharedPtr<FMovieSceneAnimMixerEntry>> MixerEntry;
		TComponentTypeID<FMovieSceneRootMotionSettings> RootMotionSettings;
		TComponentTypeID<EMovieSceneRootMotionDestination> RootDestination;
		TComponentTypeID<FObjectComponent> MeshComponent;
		TComponentTypeID<TSharedPtr<FMovieSceneMixerRootMotionComponentData>> MixerRootMotion;

		struct
		{
			FComponentTypeID RequiresBlending;
		} Tags;


	private:
		FAnimMixerComponentTypes();
	};

}