// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanAudioTrack.h"


UMetaHumanAudioTrack::UMetaHumanAudioTrack(const FObjectInitializer& InObjectInitializer)
	: Super{ InObjectInitializer }
{
	// This disables the "Add Section" entry in the track's context menu
	SupportedBlendTypes = FMovieSceneBlendTypeField::None();
}
