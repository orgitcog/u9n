// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/NavigationToolSubTrack.h"
#include "INavigationTool.h"
#include "Items/NavigationToolItemUtils.h"
#include "Items/NavigationToolSequence.h"
#include "MovieSceneSequence.h"
#include "MovieSceneTrack.h"
#include "Sections/MovieSceneSubSection.h"
#include "Tracks/MovieSceneSubTrack.h"
#include "Utils/NavigationToolMovieSceneUtils.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "NavigationToolSubTrack"

namespace UE::SequenceNavigator
{

using namespace UE::Sequencer;

FNavigationToolSubTrack::FNavigationToolSubTrack(INavigationTool& InTool
	, const FNavigationToolItemPtr& InParentItem
	, UMovieSceneSubTrack* const InSubTrack
	, const TWeakObjectPtr<UMovieSceneSequence>& InSequence
	, const TWeakObjectPtr<UMovieSceneSection>& InSection
	, const int32 InSubSectionIndex)
	: Super(InTool
	, InParentItem
	, InSubTrack
	, InSequence
	, InSection
	, InSubSectionIndex)
{
	OnTrackObjectChanged();
}

void FNavigationToolSubTrack::FindChildren(TArray<FNavigationToolItemPtr>& OutChildren, const bool bInRecursive)
{
	Super::FindChildren(OutChildren, bInRecursive);

	if (UMovieSceneSubTrack* const SubTrack = GetSubTrack())
	{
		const TSharedRef<FNavigationToolSubTrack> SharedThisRef = SharedThis(this);
		const TSharedPtr<FNavigationToolProvider> Provider = GetProvider();
		const TArray<UMovieSceneSection*>& AllSections = SubTrack->GetAllSections();

		for (int32 Index = 0; Index < AllSections.Num(); ++Index)
		{
			if (UMovieSceneSubSection* const SubSection = Cast<UMovieSceneSubSection>(AllSections[Index]))
			{
				if (UMovieSceneSequence* const Sequence = SubSection->GetSequence())
				{
					const FNavigationToolItemPtr NewItem = Tool.FindOrAdd<FNavigationToolSequence>(Provider
						, SharedThisRef, Sequence, SubSection, Index);
					OutChildren.Add(NewItem);
					if (bInRecursive)
					{
						NewItem->FindChildren(OutChildren, bInRecursive);
					}
				}
			}
		}
	}
}

TOptional<EItemDropZone> FNavigationToolSubTrack::CanAcceptDrop(const FDragDropEvent& InDragDropEvent, EItemDropZone InDropZone)
{
	return Super::CanAcceptDrop(InDragDropEvent, InDropZone);
}

FReply FNavigationToolSubTrack::AcceptDrop(const FDragDropEvent& InDragDropEvent, EItemDropZone InDropZone)
{
	return Super::AcceptDrop(InDragDropEvent, InDropZone);
}

UMovieSceneSubTrack* FNavigationToolSubTrack::GetSubTrack() const
{
	return Cast<UMovieSceneSubTrack>(GetTrack());
}

EItemSequenceInactiveState FNavigationToolSubTrack::GetInactiveState() const
{
	if (UMovieSceneSubSection* const ThisSubSection = Cast<UMovieSceneSubSection>(WeakSection.Get()))
	{
		return ThisSubSection->IsActive() ? EItemSequenceInactiveState::None : EItemSequenceInactiveState::Inactive;
	}

	const ENavigationToolCompareState State = CompareChildrenItemState<ISequenceInactivableExtension>(this,
		[](const ISequenceInactivableExtension* const InInactivableItem)
			{
				return InInactivableItem->GetInactiveState() == EItemSequenceInactiveState::Inactive;
			},
		[](const ISequenceInactivableExtension* const InInactivableItem)
			{
				return InInactivableItem->GetInactiveState() == EItemSequenceInactiveState::None;
			});

	return static_cast<EItemSequenceInactiveState>(State);
}

void FNavigationToolSubTrack::SetIsInactive(const bool bInIsInactive)
{
	const bool bNewActiveState = !bInIsInactive;

	if (UMovieSceneSubSection* const ThisSubSection = Cast<UMovieSceneSubSection>(WeakSection.Get()))
	{
		if (ThisSubSection->IsActive() != bNewActiveState)
		{
			ThisSubSection->Modify();
			ThisSubSection->SetIsActive(bNewActiveState);
		}
	}

	for (ISequenceInactivableExtension* const InactivableItem : GetChildrenOfType<ISequenceInactivableExtension>())
	{
		InactivableItem->SetIsInactive(bInIsInactive);
	}
}

EItemMarkerVisibility FNavigationToolSubTrack::GetMarkerVisibility() const
{
	const ENavigationToolCompareState State = CompareChildrenItemState<IMarkerVisibilityExtension>(this,
		[](const IMarkerVisibilityExtension* const InInactivableItem)
			{
				return InInactivableItem->GetMarkerVisibility() == EItemMarkerVisibility::Visible;
			},
		[](const IMarkerVisibilityExtension* const InInactivableItem)
			{
				return InInactivableItem->GetMarkerVisibility() == EItemMarkerVisibility::None;
			});

	return static_cast<EItemMarkerVisibility>(State);
}

void FNavigationToolSubTrack::SetMarkerVisibility(const bool bInVisible)
{
	const TSharedPtr<ISequencer> Sequencer = Tool.GetSequencer();
	if (!Sequencer.IsValid())
	{
		return;
	}

	if (UMovieSceneSubSection* const ThisSubSection = Cast<UMovieSceneSubSection>(WeakSection.Get()))
	{
		UMovieSceneSequence* const Sequence = ThisSubSection->GetSequence();
		const bool bIsVisible = IsGloballyMarkedFramesForSequence(Sequence);
		if (bIsVisible != bInVisible)
		{
			ModifySequenceAndMovieScene(Sequence);
			ShowGloballyMarkedFramesForSequence(*Sequencer, Sequence, bInVisible);
		}
	}

	for (IMarkerVisibilityExtension* const MarkerVisibilityItem : GetChildrenOfType<IMarkerVisibilityExtension>())
	{
		MarkerVisibilityItem->SetMarkerVisibility(bInVisible);
	}
}

} // namespace UE::SequenceNavigator

#undef LOCTEXT_NAMESPACE
