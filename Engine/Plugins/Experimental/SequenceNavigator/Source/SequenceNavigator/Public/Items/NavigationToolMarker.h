// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Extensions/IInTimeExtension.h"
#include "Extensions/IRenameableExtension.h"
#include "NavigationToolItem.h"

class UMovieScene;
struct FMovieSceneMarkedFrame;

namespace UE::SequenceNavigator
{

class FNavigationToolSequence;

/**
 * Navigation Tool Item representing a sequence marker
 */
class SEQUENCENAVIGATOR_API FNavigationToolMarker
	: public FNavigationToolItem
	, public IRenameableExtension
	, public IInTimeExtension
{
public:
	UE_NAVIGATIONTOOL_INHERITS_WITH_SUPER(FNavigationToolMarker
		, FNavigationToolItem
		, IRenameableExtension
		, IInTimeExtension);

	FNavigationToolMarker(INavigationTool& InTool
		, const FNavigationToolItemPtr& InParentItem
		, const TSharedPtr<FNavigationToolSequence>& InParentSequenceItem
		, const int32 InMarkedFrameIndex);

	//~ Begin INavigationToolItem

	virtual bool IsItemValid() const override;
	virtual bool IsAllowedInTool() const override;

	virtual ENavigationToolItemViewMode GetSupportedViewModes(const INavigationToolView& InToolView) const override;

	virtual FText GetDisplayName() const override;
	virtual FText GetClassName() const override;
	virtual FSlateIcon GetIcon() const override;
	virtual const FSlateBrush* GetIconBrush() const override;
	virtual FSlateColor GetIconColor() const override;
	virtual FText GetIconTooltipText() const override;

	virtual bool IsSelected(const FNavigationToolScopedSelection& InSelection) const override;
	virtual void Select(FNavigationToolScopedSelection& InSelection) const override;
	virtual void OnSelect() override;
	virtual void OnDoubleClick() override;

	virtual bool CanDelete() const override;
	virtual bool Delete() override;

	//~ End INavigationToolItem

	//~ Begin IRenameableExtension
	virtual bool CanRename() const override;
	virtual bool Rename(const FString& InName) override;
	//~ End IRenameableExtension

	//~ Begin IInTimeExtension
	virtual FFrameNumber GetInTime() const override;
	virtual void SetInTime(const FFrameNumber& InTime) override;
	//~ End IInTimeExtension

	int32 GetMarkedFrameIndex() const;

	FMovieSceneMarkedFrame* GetMarkedFrame() const;

protected:
	//~ Begin INavigationToolItem
	virtual FNavigationToolItemId CalculateItemId() const override;
	//~ End INavigationToolItem

	UMovieSceneSequence* GetParentSequence() const;
	UMovieScene* GetParentMovieScene() const;

	TWeakPtr<FNavigationToolSequence> WeakParentSequenceItem;

	int32 MarkedFrameIndex = INDEX_NONE;
};

} // namespace UE::SequenceNavigator
