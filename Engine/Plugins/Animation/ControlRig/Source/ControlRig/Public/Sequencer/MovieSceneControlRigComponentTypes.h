// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Sequencer/MovieSceneControlRigParameterBuffer.h"
#include "EntitySystem/MovieSceneEntityIDs.h"

struct FMovieSceneControlRigSpaceChannel;
struct FMovieSceneControlRigSpaceBaseKey;

class UMovieSceneControlRigParameterTrack;
class UMovieSceneControlRigParameterSection;


namespace UE::MovieScene
{

struct FAccumulatedControlEntryIndex;

/** Component data present aon all base and parameter Control Rig entities */
struct FControlRigSourceData
{
	UMovieSceneControlRigParameterTrack* Track = nullptr;
};

/**
 * Component that exists for base-eval control rig entities
 */
struct FBaseControlRigEvalData
{
	UMovieSceneControlRigParameterSection* Section = nullptr;

	TWeakObjectPtr<UControlRig> WeakControlRig;
	uint8 bIsActive : 1 = true;
	uint8 bHasWeight : 1 = false;
	uint8 bWasDoNotKey : 1 = false;
};

/**
 * Singleton Control Rig component types
 */
struct FControlRigComponentTypes
{
public:
	static FControlRigComponentTypes* Get();
	static void Destroy();

	TComponentTypeID<FControlRigSourceData> ControlRigSource;

	TComponentTypeID<FBaseControlRigEvalData> BaseControlRigEvalData;

	TComponentTypeID<FAccumulatedControlEntryIndex> AccumulatedControlEntryIndex;

	TComponentTypeID<const FMovieSceneControlRigSpaceChannel*> SpaceChannel;
	TComponentTypeID<FMovieSceneControlRigSpaceBaseKey> SpaceResult;

	struct
	{
		FComponentTypeID BaseControlRig;
		FComponentTypeID ControlRigParameter;
		FComponentTypeID Space;

		FComponentTypeID IgnoredBaseControlRig;
	} Tags;

private:
	FControlRigComponentTypes();
};


} // namespace UE::MovieScene

