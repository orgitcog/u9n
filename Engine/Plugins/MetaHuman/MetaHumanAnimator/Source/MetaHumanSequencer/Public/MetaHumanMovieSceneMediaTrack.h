// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneMediaTrack.h"

#include "MetaHumanMovieSceneMediaTrack.generated.h"

/**
 * Implements a MovieSceneMediaTrack customized for the MetaHumanPerformance plugin
 */
UCLASS()
class METAHUMANSEQUENCER_API UMetaHumanMovieSceneMediaTrack
	: public UMovieSceneMediaTrack
{
	GENERATED_BODY()

public:
	UMetaHumanMovieSceneMediaTrack(const FObjectInitializer& InObjectInitializer);

	//~ UMovieSceneMediaTrack interface
	virtual UMovieSceneSection* AddNewMediaSourceOnRow(UMediaSource& MediaSource, FFrameNumber Time, int32 RowIndex) override;
	virtual bool SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const override;
	virtual void RemoveAllAnimationData() override;

#if WITH_EDITORONLY_DATA

public:

	/**
	 * Get the height of this track's rows
	 */
	float GetRowHeight() const;

	/**
	 * Set the height of this track's rows
	 */
	void SetRowHeight(int32 NewRowHeight);

private:
	/** The minimum height for resizable media tracks */
	static constexpr float MinRowHeight = 37.0f;

	/** The height for each row of this track */
	UPROPERTY()
	float RowHeight;

#endif
};
