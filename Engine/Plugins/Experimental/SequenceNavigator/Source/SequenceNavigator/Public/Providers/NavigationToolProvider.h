// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NavigationToolDefines.h"
#include "Providers/INavigationToolProvider.h"

class UMovieSceneSequence;
struct FNavigationToolBuiltInFilterParams;
struct FNavigationToolSerializedTree;
struct FNavigationToolViewSaveState;

namespace UE::SequenceNavigator
{

class FNavigationTool;
class INavigationTool;

/** Base Navigation Tool Provider to extend from */
class FNavigationToolProvider : public INavigationToolProvider
{
public:
	//~ Begin INavigationToolProvider

	SEQUENCENAVIGATOR_API virtual void OnExtendColumnViews(TSet<FNavigationToolColumnView>& OutColumnViews) override;
	SEQUENCENAVIGATOR_API virtual void OnExtendBuiltInFilters(TArray<FNavigationToolBuiltInFilterParams>& OutFilterParams) override;

	SEQUENCENAVIGATOR_API virtual bool ShouldLockTool() const override { return false; }
	SEQUENCENAVIGATOR_API virtual bool ShouldHideItem(const FNavigationToolItemPtr& InItem) const override { return false; }

	SEQUENCENAVIGATOR_API virtual void UpdateItemIdContexts(const INavigationTool& InTool) final override;

	SEQUENCENAVIGATOR_API virtual TOptional<EItemDropZone> OnToolItemCanAcceptDrop(const FDragDropEvent& InDragDropEvent, EItemDropZone InDropZone, const FNavigationToolItemPtr& InTargetItem) const override;
	SEQUENCENAVIGATOR_API virtual FReply OnToolItemAcceptDrop(const FDragDropEvent& InDragDropEvent, EItemDropZone InDropZone, const FNavigationToolItemPtr& InTargetItem) override;

	//~ End INavigationToolProvider

	/** @return Editor only saved data for a specific view */
	FNavigationToolViewSaveState* GetViewSaveState(const INavigationTool& InTool, const int32 InToolViewId) const;

	/** Ensure saved editor only data contains an entry for a specific view, creating it if necessary */
	void EnsureToolViewCount(const INavigationTool& InTool, const int32 InToolViewId);

	SEQUENCENAVIGATOR_API bool IsSequenceSupported(UMovieSceneSequence* const InSequence) const;

private:
	friend class FNavigationToolExtender;
	friend class FNavigationTool;

	void Activate(FNavigationTool& InTool);
	void Deactivate(FNavigationTool& InTool);

	void SaveState(FNavigationTool& InTool);
	void LoadState(FNavigationTool& InTool);

	void SaveSerializedTree(FNavigationTool& InTool, const bool bInResetTree);

	void SaveSerializedTreeRecursive(const FNavigationToolItemPtr& InParentItem
		, FNavigationToolSerializedTree& InSerializedTree);

	void LoadSerializedTree(const FNavigationToolItemPtr& InParentItem
		, FNavigationToolSerializedTree* const InSerializedTree);

	void CleanupExtendedColumnViews();

	TArray<FText> ExtendedColumnViewNames;
	TArray<FName> ExtendedBuiltInFilterNames;
};

} // namespace UE::SequenceNavigator
