// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Sections/MovieSceneAudioSection.h"

#include "MetaHumanPerformanceMovieSceneAudioSection.generated.h"

/**
 * Implements a MovieSceneAudioSection that holds a reference to a PerformanceShot asset allowing customization of how to display it in Sequencer
 */
UCLASS(MinimalAPI)
class UMetaHumanPerformanceMovieSceneAudioSection
	: public UMovieSceneAudioSection
{
	GENERATED_BODY()

public:
	UMetaHumanPerformanceMovieSceneAudioSection(const FObjectInitializer& InObjectInitializer);

	UPROPERTY()
	TObjectPtr<class UMetaHumanPerformance> PerformanceShot;
};
