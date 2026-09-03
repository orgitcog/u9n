// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimatedPropertyKey.h"
#include "TrackEditors/AudioTrackEditor.h"

class FMetaHumanAudioTrackEditor
	: public FAudioTrackEditor
{
public:
	/**
	 * Create a new track editor instance. This is called by ISequencerModule::RegisterPropertyTrackEditor when
	 * registering this editor
	 */
	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> InOwningSequencer)
	{
		return MakeShareable(new FMetaHumanAudioTrackEditor{ InOwningSequencer });
	}

	/**
	* Retrieve a list of all property types that this track editor animates, which in the case of this track editor
	* it is just an empty list
	*/
	static TArray<FAnimatedPropertyKey, TInlineAllocator<1>> GetAnimatedPropertyTypes()
	{
		return TArray<FAnimatedPropertyKey, TInlineAllocator<1>>{};
	}

	FMetaHumanAudioTrackEditor(TSharedRef<ISequencer> InSequencer);

	// ISequencerTrackEditor interface
	virtual bool SupportsSequence(class UMovieSceneSequence* InSequence) const override;
	virtual bool SupportsType(TSubclassOf<UMovieSceneTrack> TrackClass) const override;
	virtual void BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const TArray<FGuid>& ObjectBindings, const UClass* ObjectClass) override;
	virtual void BuildAddTrackMenu(FMenuBuilder& MenuBuilder) override;
	virtual TSharedPtr<SWidget> BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params) override;
	virtual TSharedRef<ISequencerSection> MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding) override;
};
