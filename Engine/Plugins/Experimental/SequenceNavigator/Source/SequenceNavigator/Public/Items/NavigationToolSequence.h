// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Extensions/IInTimeExtension.h"
#include "Extensions/IMarkerVisibilityExtension.h"
#include "Extensions/IOutTimeExtension.h"
#include "Extensions/IPlayheadExtension.h"
#include "Extensions/IRenameableExtension.h"
#include "Extensions/IRevisionControlExtension.h"
#include "Extensions/ISequenceInactivableExtension.h"
#include "Extensions/ISequenceLockableExtension.h"
#include "NavigationToolItem.h"
#include "SequencerCoreFwd.h"

class UMovieScene;
class UMovieSceneSequence;
class UMovieSceneSubSection;
struct FMovieSceneBinding;

namespace UE::Sequencer
{
class FSectionModel;
}

namespace UE::SequenceNavigator
{

/**
 * Item in Navigation Tool representing a Sequence.
 */
class FNavigationToolSequence
	: public FNavigationToolItem
	, public IRenameableExtension
	, public ISequenceInactivableExtension
	, public ISequenceLockableExtension
	, public IMarkerVisibilityExtension
	, public IRevisionControlExtension
	, public IPlayheadExtension
	, public IInTimeExtension
	, public IOutTimeExtension
{
public:
	UE_NAVIGATIONTOOL_INHERITS_WITH_SUPER(FNavigationToolSequence
		, FNavigationToolItem
		, IRenameableExtension
		, ISequenceInactivableExtension
		, ISequenceLockableExtension
		, IMarkerVisibilityExtension
		, IRevisionControlExtension
		, IPlayheadExtension
		, IInTimeExtension
		, IOutTimeExtension);

	SEQUENCENAVIGATOR_API FNavigationToolSequence(INavigationTool& InTool
		, const FNavigationToolItemPtr& InParentItem
		, UMovieSceneSequence* const InSequence
		, UMovieSceneSubSection* const InSubSection
		, const int32 InSubSectionIndex);

	//~ Begin INavigationToolItem

	SEQUENCENAVIGATOR_API virtual bool IsItemValid() const override;
	SEQUENCENAVIGATOR_API virtual UObject* GetItemObject() const override;
	SEQUENCENAVIGATOR_API virtual bool IsAllowedInTool() const override;

	SEQUENCENAVIGATOR_API virtual void FindChildren(TArray<FNavigationToolItemPtr>& OutChildren, const bool bInRecursive) override;
	SEQUENCENAVIGATOR_API virtual void GetItemProxies(TArray<TSharedPtr<FNavigationToolItemProxy>>& OutItemProxies) override;

	SEQUENCENAVIGATOR_API virtual bool AddChild(const FNavigationToolAddItemParams& InAddItemParams) override;
	SEQUENCENAVIGATOR_API virtual bool RemoveChild(const FNavigationToolRemoveItemParams& InRemoveItemParams) override;

	SEQUENCENAVIGATOR_API virtual ENavigationToolItemViewMode GetSupportedViewModes(const INavigationToolView& InToolView) const override;
	virtual bool CanBeTopLevel() const override { return true; }

	virtual FText GetDisplayName() const override;
	SEQUENCENAVIGATOR_API virtual FText GetClassName() const override;
	virtual FSlateIcon GetIcon() const override;
	SEQUENCENAVIGATOR_API virtual FText GetIconTooltipText() const override;

	SEQUENCENAVIGATOR_API virtual bool IsSelected(const FNavigationToolScopedSelection& InSelection) const override;
	SEQUENCENAVIGATOR_API virtual void Select(FNavigationToolScopedSelection& InSelection) const override;

	SEQUENCENAVIGATOR_API virtual void OnSelect() override;
	SEQUENCENAVIGATOR_API virtual void OnDoubleClick() override;

	SEQUENCENAVIGATOR_API virtual void OnObjectsReplaced(const TMap<UObject*, UObject*>& InReplacementMap, bool bInRecursive) override;

	//~ End INavigationToolItem

	//~ Begin IRenameableExtension
	SEQUENCENAVIGATOR_API virtual bool CanRename() const override;
	SEQUENCENAVIGATOR_API virtual bool Rename(const FString& InName) override;
	//~ End IRenameableExtension

	//~ Begin ISequenceDeactivatableExtension
	SEQUENCENAVIGATOR_API virtual EItemSequenceInactiveState GetInactiveState() const override;
	SEQUENCENAVIGATOR_API virtual void SetIsInactive(const bool bInIsInactive) override;
	//~ End ISequenceDeactivatableExtension

	//~ Begin IMarkerVisibilityExtension
	SEQUENCENAVIGATOR_API virtual EItemMarkerVisibility GetMarkerVisibility() const override;
	SEQUENCENAVIGATOR_API virtual void SetMarkerVisibility(const bool bInVisible) override;
	//~ End IMarkerVisibilityExtension

	//~ Begin ISequenceLockableExtension
	SEQUENCENAVIGATOR_API virtual EItemSequenceLockState GetLockState() const override;
	SEQUENCENAVIGATOR_API virtual void SetIsLocked(const bool bInIsLocked) override;
	//~ End ISequenceLockableExtension

	//~ Begin IColorExtension
	SEQUENCENAVIGATOR_API virtual TOptional<FColor> GetColor() const override;
	SEQUENCENAVIGATOR_API virtual void SetColor(const TOptional<FColor>& InColor) override;
	//~ End IColorExtension

	//~ Begin IRevisionControlExtension
	SEQUENCENAVIGATOR_API virtual EItemRevisionControlState GetRevisionControlState() const override;
	SEQUENCENAVIGATOR_API virtual const FSlateBrush* GetRevisionControlStatusIcon() const override;
	SEQUENCENAVIGATOR_API virtual FText GetRevisionControlStatusText() const override;
	//~ End IRevisionControlExtension

	//~ Begin IPlayheadExtension
	SEQUENCENAVIGATOR_API virtual EItemContainsPlayhead ContainsPlayhead() const override;
	//~ End IPlayheadExtension

	//~ Begin IInTimeExtension
	SEQUENCENAVIGATOR_API virtual FFrameNumber GetInTime() const override;
	SEQUENCENAVIGATOR_API virtual void SetInTime(const FFrameNumber& InTime) override;
	//~ End IInTimeExtension

	//~ Begin IOutTimeExtension
	SEQUENCENAVIGATOR_API virtual FFrameNumber GetOutTime() const override;
	SEQUENCENAVIGATOR_API virtual void SetOutTime(const FFrameNumber& InTime) override;
	//~ End IOutTimeExtension

	SEQUENCENAVIGATOR_API UMovieSceneSequence* GetSequence() const;

	SEQUENCENAVIGATOR_API UMovieSceneSubSection* GetSubSection() const;
	SEQUENCENAVIGATOR_API int32 GetSubSectionIndex() const;

	SEQUENCENAVIGATOR_API UMovieScene* GetSequenceMovieScene() const;

	SEQUENCENAVIGATOR_API TArray<FMovieSceneBinding> GetSortedBindings() const;

	UE::Sequencer::TViewModelPtr<UE::Sequencer::FSectionModel> GetViewModel() const;

protected:
	//~Begin FNavigationToolItem
	SEQUENCENAVIGATOR_API virtual FNavigationToolItemId CalculateItemId() const override;
	//~End FNavigationToolItem

	TWeakObjectPtr<UMovieSceneSubSection> WeakSubSection;
	int32 SubSectionIndex = 0;
	TWeakObjectPtr<UMovieSceneSequence> WeakSequence;
};

} // namespace UE::SequenceNavigator
