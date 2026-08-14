// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanMovieSceneMediaSection.h"

#include "MetaHumanPerformanceMovieSceneMediaSection.generated.h"

/**
 * Implements a MovieSceneMediaSection that holds a reference to a PerformanceShot asset allowing customization of how to display it in Sequencer
 */
UCLASS(MinimalAPI)
class UMetaHumanPerformanceMovieSceneMediaSection
	: public UMetaHumanMovieSceneMediaSection
{
	GENERATED_BODY()

public:
	UMetaHumanPerformanceMovieSceneMediaSection(const FObjectInitializer& InObjectInitializer);

	UPROPERTY()
	TObjectPtr<class UMetaHumanPerformance> PerformanceShot;
};
