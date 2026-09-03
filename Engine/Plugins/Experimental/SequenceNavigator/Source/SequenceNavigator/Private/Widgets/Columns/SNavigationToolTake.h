// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Columns/NavigationToolTakeColumn.h"
#include "NavigationToolDefines.h"
#include "ScopedTransaction.h"
#include "Templates/UniquePtr.h"
#include "Widgets/SCompoundWidget.h"

class UMovieSceneSequence;

namespace UE::SequenceNavigator
{

class INavigationTool;
class INavigationToolView;
class SNavigationToolTreeRow;

class SNavigationToolTake : public SCompoundWidget
{
public:
	struct FTakeItemInfo
	{
		uint32 TakeIndex = 0;
		uint32 TakeNumber = 0;
		FString DisplayName;
		TWeakObjectPtr<UMovieSceneSequence> WeakSequence;
	};

	SLATE_BEGIN_ARGS(SNavigationToolTake) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs
		, const FNavigationToolItemRef& InItem
		, const TSharedRef<INavigationToolView>& InView
		, const TSharedRef<SNavigationToolTreeRow>& InRowWidget);

	void RemoveItemColor() const;

	FSlateColor GetBorderColor() const;

	FReply OnTakeEntrySelected(const TSharedPtr<FTakeItemInfo> InTakeInfo);

	void Press();
	void Release();

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnFocusLost(const FFocusEvent& InFocusEvent) override;

	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;

private:
	TSharedRef<SWidget> GenerateTakeWidget(const TSharedPtr<FTakeItemInfo> InTakeInfo);

	void OnSelectionChanged(const TSharedPtr<FTakeItemInfo> InTakeInfo, const ESelectInfo::Type InSelectType);

	void CacheTakes();
	void SetActiveTake(UMovieSceneSequence* const InSequence);

	FNavigationToolItemWeakPtr WeakItem;

	TWeakPtr<INavigationToolView> WeakView;

	TWeakPtr<SNavigationToolTreeRow> WeakRowWidget;

	TWeakPtr<INavigationTool> WeakTool;

	TArray<TSharedPtr<FTakeItemInfo>> CachedTakes;

	TSharedPtr<FTakeItemInfo> ActiveTakeInfo;

	bool bIsPressed = false;

	TUniquePtr<FScopedTransaction> UndoTransaction;
};

} // namespace UE::SequenceNavigator
