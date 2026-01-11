// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Extensions/IMarkerVisibilityExtension.h"
#include "Extensions/ISequenceInactivableExtension.h"
#include "NavigationToolTrack.h"

class UMovieSceneSequence;
class UMovieSceneSection;
class UMovieSceneSubTrack;

namespace UE::SequenceNavigator
{

class SEQUENCENAVIGATOR_API FNavigationToolSubTrack
	: public FNavigationToolTrack
	, public ISequenceInactivableExtension
	, public IMarkerVisibilityExtension
{
public:
	UE_NAVIGATIONTOOL_INHERITS_WITH_SUPER(FNavigationToolSubTrack
		, FNavigationToolTrack
		, ISequenceInactivableExtension
		, IMarkerVisibilityExtension);

	FNavigationToolSubTrack(INavigationTool& InTool
		, const FNavigationToolItemPtr& InParentItem
		, UMovieSceneSubTrack* const InSubTrack
		, const TWeakObjectPtr<UMovieSceneSequence>& InSequence
		, const TWeakObjectPtr<UMovieSceneSection>& InSection
        , const int32 InSubSectionIndex);

	//~ Begin INavigationToolItem
	virtual void FindChildren(TArray<FNavigationToolItemPtr>& OutChildren, const bool bInRecursive) override;
	virtual TOptional<EItemDropZone> CanAcceptDrop(const FDragDropEvent& InDragDropEvent, EItemDropZone InDropZone) override;
	virtual FReply AcceptDrop(const FDragDropEvent& InDragDropEvent, EItemDropZone InDropZone) override;
	//~ End INavigationToolItem

	UMovieSceneSubTrack* GetSubTrack() const;

	//~ Begin ISequenceDeactivatableExtension
	virtual EItemSequenceInactiveState GetInactiveState() const override;
	virtual void SetIsInactive(const bool bInIsInactive) override;
	//~ End ISequenceDeactivatableExtension

	//~ Begin IMarkerVisibilityExtension
	virtual EItemMarkerVisibility GetMarkerVisibility() const override;
	virtual void SetMarkerVisibility(const bool bInVisible) override;
	//~ End IMarkerVisibilityExtension
};

} // namespace UE::SequenceNavigator
