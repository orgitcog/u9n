// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneMediaSection.h"
#include "MetaHumanMovieSceneChannel.h"

#include "MetaHumanMovieSceneMediaSection.generated.h"

/**
 * Implements a MovieSceneMediaSection
 */

UCLASS()
class METAHUMANSEQUENCER_API UMetaHumanMovieSceneMediaSection
	: public UMovieSceneMediaSection
{
	GENERATED_BODY()

public:
	UMetaHumanMovieSceneMediaSection(const FObjectInitializer& ObjectInitializer);

	FMovieSceneChannelDataKeyAddedEvent& OnKeyAddedEventDelegate();
	FMovieSceneChannelDataKeyDeletedEvent& OnKeyDeletedEventDelegate();
	
	FMetaHumanMovieSceneChannel& GetMetaHumanChannelRef();
	
	void AddChannelToMovieSceneSection();

private:

	UPROPERTY()
	FMetaHumanMovieSceneChannel MetaHumanChannel;
};
