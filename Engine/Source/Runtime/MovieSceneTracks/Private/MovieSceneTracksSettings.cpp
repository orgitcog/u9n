// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneTracksSettings)

UMovieSceneTracksSettings::UMovieSceneTracksSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bPreviewCameraCutsInSimulate = true;
}

void UMovieSceneTracksSettings::SetPreviewCameraCutsInSimulate(bool bInPreviewCameraCutsInSimulate)
{
	if (bInPreviewCameraCutsInSimulate != bPreviewCameraCutsInSimulate)
	{
		bPreviewCameraCutsInSimulate = bInPreviewCameraCutsInSimulate;
		SaveConfig();
	}
}

