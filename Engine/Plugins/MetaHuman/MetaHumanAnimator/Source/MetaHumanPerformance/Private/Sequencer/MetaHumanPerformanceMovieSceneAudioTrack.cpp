// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanPerformanceMovieSceneAudioTrack.h"
#include "MetaHumanPerformanceMovieSceneAudioSection.h"


UMetaHumanPerformanceMovieSceneAudioTrack::UMetaHumanPerformanceMovieSceneAudioTrack(const FObjectInitializer& InObjectInitializer)
	: Super{ InObjectInitializer }
{
	// This disables the "Add Section" entry in the track's context menu
	SupportedBlendTypes = FMovieSceneBlendTypeField::None();
}

UMovieSceneSection* UMetaHumanPerformanceMovieSceneAudioTrack::CreateNewSection()
{
	return NewObject<UMetaHumanPerformanceMovieSceneAudioSection>(this, NAME_None, RF_Transactional);
}