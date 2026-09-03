// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/MovieSceneFadeTrackTests.h"

#include "Camera/PlayerCameraManager.h"

float UMovieSceneFadeTrackTestLibrary::GetManualFadeAmount(APlayerCameraManager* PlayerCameraManager)
{
	return PlayerCameraManager ? PlayerCameraManager->FadeAmount : 0.f;
}

