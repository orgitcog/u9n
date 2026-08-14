// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TrackEditors/CommonAnimationTrackEditor.h"

/**
 * Tools for animation tracks
 */
class FSkeletalAnimationTrackEditor : public UE::Sequencer::FCommonAnimationTrackEditor
{
public:

	/**
	 * Constructor
	 *
	 * @param InSequencer The sequencer instance to be used by this tool
	 */
	FSkeletalAnimationTrackEditor( TSharedRef<ISequencer> InSequencer );

	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> InSequencer);

	virtual void OnInitialize() override;
	virtual void OnRelease() override;

	virtual void BuildTrackContextMenu(FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track) override;
	virtual void BuildTrackSidebarMenu(FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track) override;
	virtual bool SupportsSequence(UMovieSceneSequence* InSequence) const override;
	virtual bool SupportsType( TSubclassOf<UMovieSceneTrack> Type ) const override;
	virtual TSharedRef<ISequencerSection> MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding) override;
	virtual TSubclassOf<UMovieSceneCommonAnimationTrack> GetTrackClass() const override;

private:

	void OnPostPropertyChanged(UObject* InObject, struct FPropertyChangedEvent& InPropertyChangedEvent);

	/** Common function used to build context and sidebar menus */
	void BuildTrackContextMenu_Internal(FMenuBuilder& MenuBuilder, UMovieSceneTrack* const InTrack, const bool bAddSeparatorAtEnd);
};


/** Class for animation sections */
class FSkeletalAnimationSection
	: public UE::Sequencer::FCommonAnimationSection
{
public:

	/** Constructor. */
	FSkeletalAnimationSection( UMovieSceneSection& InSection, TWeakPtr<ISequencer> InSequencer);
};
