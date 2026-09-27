// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Extensions/IPlayheadExtension.h"
#include "Extensions/IRenameableExtension.h"
#include "Extensions/ISequenceLockableExtension.h"
#include "MovieSceneBinding.h"
#include "MVVM/ViewModels/ObjectBindingModel.h"
#include "NavigationToolItem.h"
#include "SequencerCoreFwd.h"
#include "Styling/SlateColor.h"
#include "Textures/SlateIcon.h"

namespace UE::Sequencer
{
class FObjectBindingModel;
}

namespace UE::SequenceNavigator
{

class FNavigationToolSequence;

/**
 * Navigation Tool Item representing a Sequence binding
 */
class SEQUENCENAVIGATOR_API FNavigationToolBinding
	: public FNavigationToolItem
	, public IRenameableExtension
	//, public ISequenceLockableExtension
	, public IPlayheadExtension
{
public:
	UE_NAVIGATIONTOOL_INHERITS_WITH_SUPER(FNavigationToolBinding
		, FNavigationToolItem
		, IRenameableExtension
		/*, ISequenceLockableExtension*/
		, IPlayheadExtension);

	FNavigationToolBinding(INavigationTool& InTool
		, const FNavigationToolItemPtr& InParentItem
		, const TSharedPtr<FNavigationToolSequence>& InParentSequenceItem
		, const FMovieSceneBinding& InBinding);

	//~ Begin INavigationToolItem

	virtual bool IsItemValid() const override;
	virtual UObject* GetItemObject() const override;
	virtual bool IsAllowedInTool() const override;

	virtual void FindChildren(TArray<FNavigationToolItemPtr>& OutChildren, const bool bInRecursive) override;

	virtual bool CanBeTopLevel() const override { return false; }
	virtual bool ShouldSort() const override { return false; }

	virtual FText GetDisplayName() const override;
	virtual FText GetClassName() const override;
	virtual FSlateColor GetItemLabelColor() const override;
	virtual FSlateIcon GetIcon() const override;
	virtual FText GetIconTooltipText() const override;
	virtual FSlateColor GetIconColor() const override;

	virtual bool IsSelected(const FNavigationToolScopedSelection& InSelection) const override;
	virtual void Select(FNavigationToolScopedSelection& InSelection) const override;
	virtual void OnSelect() override;
	virtual void OnDoubleClick() override;

	virtual bool CanDelete() const override;
	virtual bool Delete() override;

	virtual FNavigationToolItemId CalculateItemId() const override;

	//~ End INavigationToolItem

	//~ Begin IRenameableExtension
	virtual bool CanRename() const override;
	virtual bool Rename(const FString& InName) override;
	//~ End IRenameableExtension

	//~ Begin ISequenceLockableExtension
	//virtual EItemSequenceLockState GetLockState() const override;
	//virtual void SetIsLocked(const bool bInIsLocked) override;
	//~ End ISequenceLockableExtension

	//~ Begin IPlayheadExtension
	virtual EItemContainsPlayhead ContainsPlayhead() const override;
	//~ End IPlayheadExtension

	const FMovieSceneBinding& GetBinding() const;

	/** @return The Sequence this binding belongs to */
	UMovieSceneSequence* GetSequence() const;

	/** @return The cached object that is bound in sequencer. */
	UObject* GetCachedBoundObject() const;

	/** Caches the object if it's not already been cached and returns the result. */
	UObject* CacheBoundObject();

	UE::Sequencer::TViewModelPtr<UE::Sequencer::FObjectBindingModel> GetViewModel() const;

protected:
	TWeakPtr<FNavigationToolSequence> WeakParentSequenceItem;

	FMovieSceneBinding Binding;

	TWeakObjectPtr<const UClass> WeakBoundObjectClass;
	TWeakObjectPtr<> WeakBoundObject;

	FSlateIcon Icon;
	FSlateColor IconColor;
};

} // namespace UE::SequenceNavigator
