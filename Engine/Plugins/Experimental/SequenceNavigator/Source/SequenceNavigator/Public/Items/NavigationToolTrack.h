// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Extensions/IPlayheadExtension.h"
#include "Extensions/ISequenceLockableExtension.h"
#include "NavigationToolItem.h"
#include "SequencerCoreFwd.h"
#include "Textures/SlateIcon.h"
#include "UObject/WeakObjectPtrTemplates.h"

class UMovieSceneSequence;
class UMovieSceneSection;
class UMovieSceneTrack;

namespace UE::Sequencer
{
class FTrackModel;
}

namespace UE::SequenceNavigator
{

class FNavigationToolTrack
	: public FNavigationToolItem
	, public ISequenceLockableExtension
	, public IPlayheadExtension
{
public:
	UE_NAVIGATIONTOOL_INHERITS_WITH_SUPER(FNavigationToolTrack
		, FNavigationToolItem
		, ISequenceLockableExtension
		, IPlayheadExtension);

	FNavigationToolTrack(INavigationTool& InTool
		, const FNavigationToolItemPtr& InParentItem
		, UMovieSceneTrack* const InTrack
		, const TWeakObjectPtr<UMovieSceneSequence>& InSequence
		, const TWeakObjectPtr<UMovieSceneSection>& InSection
		, const int32 InSectionIndex);

	//~ Begin INavigationToolItem

	virtual bool IsItemValid() const override;
	virtual UObject* GetItemObject() const override;
	virtual bool IsAllowedInTool() const override;

	virtual void FindChildren(TArray<FNavigationToolItemPtr>& OutChildren, const bool bInRecursive) override;

	virtual bool CanBeTopLevel() const override { return false; }

	virtual FText GetDisplayName() const override;
	virtual FText GetClassName() const override;
	virtual const FSlateBrush* GetDefaultIconBrush() const override;
	virtual FSlateIcon GetIcon() const override;
	virtual FText GetIconTooltipText() const override;

	virtual bool IsSelected(const FNavigationToolScopedSelection& InSelection) const override;
	virtual void Select(FNavigationToolScopedSelection& InSelection) const override;

	virtual void OnSelect() override;
	virtual void OnDoubleClick() override;

	void OnObjectsReplaced(const TMap<UObject*, UObject*>& InReplacementMap, bool bInRecursive);

	//~ End INavigationToolItem

	//~ Begin ISequenceLockableExtension
	virtual EItemSequenceLockState GetLockState() const override;
	virtual void SetIsLocked(const bool bInIsLocked) override;
	//~ End ISequenceLockableExtension

	//~ Begin IPlayheadExtension
	virtual EItemContainsPlayhead ContainsPlayhead() const override;
	//~ End IPlayheadExtension

	UMovieSceneTrack* GetTrack() const;

	UMovieSceneSequence* GetSequence() const;
	UMovieSceneSection* GetSection() const;
	int32 GetSectionIndex() const;

	UE::Sequencer::TViewModelPtr<UE::Sequencer::FTrackModel> GetViewModel() const;

protected:
	//~ Begin FNavigationToolItem
	virtual FNavigationToolItemId CalculateItemId() const override;
	//~ End FNavigationToolItem

	void OnTrackObjectChanged();

	TWeakObjectPtr<UMovieSceneSequence> WeakSequence;
	TWeakObjectPtr<UMovieSceneSection> WeakSection;
	int32 SectionIndex = 0;
	TWeakObjectPtr<UMovieSceneTrack> WeakTrack;

	FSlateIcon Icon;
};

} // namespace UE::SequenceNavigator
