// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieSceneClock.h"

#include "Engine/World.h"
#include "Evaluation/IMovieSceneCustomClockSource.h"
#include "MovieScene.h"
#include "MovieSceneTimeController.h"

TSharedPtr<FMovieSceneTimeController> UMovieSceneClock::MakeTimeController(UObject* PlaybackContext) const
{
	return MakeShared<FMovieSceneTimeController_Tick>();
}

TSharedPtr<FMovieSceneTimeController> UMovieSceneExternalClock::MakeTimeController(UObject* PlaybackContext) const
{
	return MakeShared<FMovieSceneTimeController_Custom>(CustomClockSourcePath, PlaybackContext);
}