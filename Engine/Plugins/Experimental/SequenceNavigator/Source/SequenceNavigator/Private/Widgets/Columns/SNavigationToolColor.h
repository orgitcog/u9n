// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NavigationToolDefines.h"
#include "Widgets/SCompoundWidget.h"

class SMenuAnchor;

namespace UE::SequenceNavigator
{

class INavigationTool;
class INavigationToolView;
class SNavigationToolTreeRow;

class SNavigationToolColor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNavigationToolColor) {}
	SLATE_END_ARGS()

	virtual ~SNavigationToolColor() override {}

	void Construct(const FArguments& InArgs
		, const FNavigationToolItemRef& InItem
		, const TSharedRef<INavigationToolView>& InView
		, const TSharedRef<SNavigationToolTreeRow>& InRowWidget);

	FLinearColor GetColorBlockColor() const;
	void RemoveItemColor() const;

	FSlateColor GetBorderColor() const;
	FLinearColor GetStateColorAndOpacity() const;

	FReply OnColorEntrySelected(const FColor& InColor) const;

	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;

protected:
	FReply OnColorMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InEvent);

	void OpenColorPickerDialog();

	FNavigationToolItemWeakPtr WeakItem;

	TWeakPtr<INavigationToolView> WeakView;

	TWeakPtr<SNavigationToolTreeRow> WeakRowWidget;

	TWeakPtr<INavigationTool> WeakTool;

	TSharedPtr<SMenuAnchor> ColorOptions;

	mutable FColor ItemColor = FColor();

	bool bIsPressed = false;
};

} // namespace UE::SequenceNavigator
