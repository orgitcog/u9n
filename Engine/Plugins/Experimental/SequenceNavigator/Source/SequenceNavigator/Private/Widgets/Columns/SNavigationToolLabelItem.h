// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NavigationToolDefines.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SCompoundWidget.h"

class SInlineEditableTextBlock;
struct FSlateColor;

namespace UE::SequenceNavigator
{

class FNavigationToolItem;
class FNavigationTool;
class INavigationToolView;
class SNavigationToolTreeRow;

class SNavigationToolLabelItem : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNavigationToolLabelItem) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs
		, const FNavigationToolItemRef& InItem
		, const TSharedRef<SNavigationToolTreeRow>& InRowWidget);

	bool IsReadOnly() const;

	bool IsItemEnabled() const;

	FText GetItemText() const;

	bool OnVerifyItemLabelChanged(const FText& InLabel, FText& OutErrorMessage);

	void OnLabelTextCommitted(const FText& InLabel, ETextCommit::Type InCommitInfo);
	void RenameItem(const FText& InLabel);

	void OnRenameAction(ENavigationToolRenameAction InRenameAction, const TSharedPtr<INavigationToolView>& InToolView) const;

	void OnEnterEditingMode();
	void OnExitEditingMode();

	virtual const FInlineEditableTextBlockStyle* GetTextBlockStyle() const;

	virtual FSlateColor GetForegroundColor() const;

protected:
	FNavigationToolItemWeakPtr WeakItem;

	TWeakPtr<INavigationToolView> WeakView;

	TSharedPtr<SInlineEditableTextBlock> InlineTextBlock;

	bool bInEditingMode = false;
};

} // namespace UE::SequenceNavigator
