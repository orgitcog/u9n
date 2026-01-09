// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneTrackEditor.h"

#include "ContentBrowserDelegates.h"

struct FGuid;
class FReply;
class UMovieSceneTrack;
class UMovieSceneSubtitlesTrack;

/**
 * Tools for subtitles tracks
 */
class FSubtitlesTrackEditor
	: public FMovieSceneTrackEditor
{
public:

	/**
	 * Constructor
	 *
	 * @param InSequencer The sequencer instance to be used by this tool
	 */
	FSubtitlesTrackEditor(TSharedRef<ISequencer> InSequencer);

	/**
	 * Creates an instance of this class.  Called by a sequencer
	 *
	 * @param OwningSequencer The sequencer instance to be used by this tool
	 * @return The new instance of this class
	 */
	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> OwningSequencer);

public:

	// ISequencerTrackEditor interface
	virtual FText GetDisplayName() const override;
	virtual void BuildAddTrackMenu(FMenuBuilder& MenuBuilder) override;
	virtual TSharedPtr<SWidget> BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params) override;
	virtual TSharedRef<ISequencerSection> MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding) override;
	virtual bool SupportsType(TSubclassOf<UMovieSceneTrack> Type) const override;
	virtual bool SupportsSequence(UMovieSceneSequence* InSequence) const override;
	virtual bool OnAllowDrop(const FDragDropEvent& DragDropEvent, FSequencerDragDropParams& DragDropParams) override;
	virtual FReply OnDrop(const FDragDropEvent& DragDropEvent, const FSequencerDragDropParams& DragDropParams) override;

private:

	TSharedRef<SWidget> BuildSubMenu(FOnAssetSelected OnAssetSelected, FOnAssetEnterPressed OnAssetEnterPressed);
	void OnAssetSelected(const FAssetData& AssetData, UMovieSceneTrack* Track);
	void OnAssetEnterPressed(const TArray<FAssetData>& AssetData, UMovieSceneTrack* Track);
	void HandleAddMenuEntryExecute();
};
