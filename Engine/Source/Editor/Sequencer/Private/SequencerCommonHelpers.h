// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Input/SNumericDropDown.h"
#include "Containers/ArrayView.h"
#include "Misc/FrameNumber.h"
#include "MVVM/ViewModels/ViewModel.h"
#include "MVVM/ViewModels/ViewModelHierarchy.h"

#define LOCTEXT_NAMESPACE "SequencerHelpers"

class FSequencer;
class IKeyArea;
class UMovieSceneSection;
class UMovieSceneTrack;

namespace UE
{
namespace Sequencer
{
	class FChannelModel;
	class IOutlinerExtension;
	struct FViewModelVariantIterator;

} // namespace Sequencer
} // namespace UE


class SequencerHelpers
{
public:
	using FViewModel = UE::Sequencer::FViewModel;

	/**
	 * Gets the key areas from the requested node
	 */
	static void GetAllKeyAreas(TSharedPtr<FViewModel> DataModel, TSet<TSharedPtr<IKeyArea>>& KeyAreas);

	/**
	 * Gets the channels from the requested node
	 */
	static void GetAllChannels(TSharedPtr<FViewModel> DataModel, TSet<TSharedPtr<UE::Sequencer::FChannelModel>>& Channels);

	/**
	 * Get the section index that relates to the specified time
	 * @return the index corresponding to the highest overlapping section, or nearest section where no section overlaps the current time
	 */
	static int32 GetSectionFromTime(TArrayView<UMovieSceneSection* const> InSections, FFrameNumber Time);

	/**
	 * Get descendant nodes
	 */
	static void GetDescendantNodes(TSharedRef<FViewModel> DataModel, TSet<TSharedRef<FViewModel> >& Nodes);

	/**
	 * Gets all sections from the requested node
	 */
	static void GetAllSections(TSharedPtr<FViewModel> DataModel, TSet<TWeakObjectPtr<UMovieSceneSection>>& Sections);

	/**
	 * Perform default selection for the specified mouse event, based on the current hotspot
	 */
	static void PerformDefaultSelection(FSequencer& Sequencer, const FPointerEvent& MouseEvent);
	
	/**
	 * Attempt to summon a context menu for the current hotspot
	 */
	static TSharedPtr<SWidget> SummonContextMenu(FSequencer& Sequencer, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

	/*
	 * Build a sub menu for adding a new track section
	 */
	static void BuildNewSectionMenu(const TWeakPtr<FSequencer>& InWeakSequencer
		, const int32 InRowIndex
		, const TWeakObjectPtr<UMovieSceneTrack>& InTrackWeak
		, FMenuBuilder& MenuBuilder);

	/*
	 * Build an inline menu or sub menu for editing track section(s)
	 */
	static void BuildEditSectionMenu(const TWeakPtr<FSequencer>& InWeakSequencer
		, const TArray<TWeakObjectPtr<>>& InWeakSections
		, FMenuBuilder& MenuBuilder
		, const bool bInSubMenu);

	/*
	* Build an inline menu or sub menu for editing track(s)
	*/
	static void BuildEditTrackMenu(const TWeakPtr<FSequencer>& InWeakSequencer
		, const TArray<TWeakObjectPtr<>>& InWeakTracks
		, FMenuBuilder& MenuBuilder
		, const bool bInSubMenu);



	/*
	 * Build a menu for selection the blend algorithm
	 */
	static void BuildBlendingMenu(const TWeakPtr<FSequencer>& InWeakSequencer
		, const TWeakObjectPtr<UMovieSceneTrack>& InTrackWeak
		, FMenuBuilder& MenuBuilder);

	/**
	 * Gets all section objects from track area models
	 */
	static TArray<TWeakObjectPtr<>> GetSectionObjectsFromTrackAreaModels(const UE::Sequencer::FViewModelVariantIterator& InTrackAreaModels);

	/**
	 * Sorts an array of outliner items by start time of the first layer bar or selection order
	 * @param Sequencer The Sequencer that the outliner items belong to
	 * @param InItems The list of outliner items to sort
	 * @param bInSortByItemOrder If true, sorts by the order of the item in the array instead of by start time of the layer bar
	 * @param bInDescending If true, sorts in reverse order instead of ascending
	 * @param bInTransact If true, creates an editor transaction that can be undone
	 */
	static void SortOutlinerItems(FSequencer& Sequencer
		, const TArray<UE::Sequencer::TViewModelPtr<UE::Sequencer::IOutlinerExtension>>& InItems
		, const bool bInSortByItemOrder
		, const bool bInDescending
		, const bool bInTransact = true);
};

#undef LOCTEXT_NAMESPACE
