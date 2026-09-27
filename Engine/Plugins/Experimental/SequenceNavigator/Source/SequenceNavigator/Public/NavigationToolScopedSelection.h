// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Set.h"
#include "MovieSceneSequence.h"

class ISequencer;
class UMovieSceneSection;
class UMovieSceneTrack;
class UObject;

namespace UE::SequenceNavigator
{

enum class ENavigationToolScopedSelectionPurpose
{
	/** At the end of the Scope, it will set whatever has been added to the Selected List to be the new Selection */
	Sync,
	/** Used only to check for whether an Object is Selected or not. Cannot execute "Select" */
	Read,
};

/** Handler to Sync Selection from Navigation Tool to the Sequencer */
class FNavigationToolScopedSelection
{
public:
	explicit FNavigationToolScopedSelection(ISequencer& InSequencer
		, ENavigationToolScopedSelectionPurpose InPurpose = ENavigationToolScopedSelectionPurpose::Read);

	~FNavigationToolScopedSelection();

	SEQUENCENAVIGATOR_API void Select(const FGuid& InObjectGuid);
	SEQUENCENAVIGATOR_API void Select(UMovieSceneSection* const InSection);
	SEQUENCENAVIGATOR_API void Select(UMovieSceneTrack* const InTrack);
	SEQUENCENAVIGATOR_API void Select(UMovieSceneSequence* const InSequence
		, const int32 InMarkedFrameIndex);

	SEQUENCENAVIGATOR_API bool IsSelected(const UObject* const InObject) const;
	SEQUENCENAVIGATOR_API bool IsSelected(const FGuid& InObjectGuid) const;
	SEQUENCENAVIGATOR_API bool IsSelected(UMovieSceneSection* const InSection) const;
	SEQUENCENAVIGATOR_API bool IsSelected(UMovieSceneTrack* const InTrack) const;
	SEQUENCENAVIGATOR_API bool IsSelected(UMovieSceneSequence* const InSequence
		, const int32 InMarkedFrameIndex) const;

	SEQUENCENAVIGATOR_API ISequencer& GetSequencer() const;

private:
	void SyncSelections();

	ISequencer& Sequencer;

	/** All Objects Selected (Sections, Tracks, Objects) */
	TSet<const UObject*> ObjectsSet;

	TArray<FGuid> SelectedObjectGuids;
	TArray<UMovieSceneSection*> SelectedSections;
	TArray<UMovieSceneTrack*> SelectedTracks;
	TMap<UMovieSceneSequence*, TSet<int32>> SelectedMarkedFrames;

	ENavigationToolScopedSelectionPurpose Purpose;
};

} // namespace UE::SequenceNavigator
