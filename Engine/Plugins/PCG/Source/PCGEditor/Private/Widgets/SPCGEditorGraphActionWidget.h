// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SGraphActionMenu.h"
#include "EdGraph/EdGraphSchema.h"
#include "Widgets/Images/SLayeredImage.h"

class SPCGGraphActionWidget : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SPCGGraphActionWidget) {}
		SLATE_ARGUMENT(TSharedPtr<SWidget>, NameWidget)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const FCreateWidgetForActionData* const InCreateData)
	{
		check(InCreateData->Action.IsValid());

		TSharedRef<SWidget> IconWidget = SNullWidget::NullWidget;
		ActionPtr = InCreateData->Action;
		MouseButtonDownDelegate = InCreateData->MouseButtonDownDelegate;

		if (const FSlateBrush* const PrimaryIcon = InCreateData->Action->GetPaletteIcon())
		{
			// @todo_pcg: Support secondary icon/colors for more complex type icons (like Maps)
			const FSlateBrush* const SecondaryIcon = nullptr;
			const FLinearColor SecondaryColor = FLinearColor::White;

			SAssignNew(IconWidget, SLayeredImage, SecondaryIcon, SecondaryColor)
			.DesiredSizeOverride(FVector2D(16))
			.Image(PrimaryIcon);
		}

		TSharedRef<SWidget> NameSlotWidget =
			InArgs._NameWidget
				? InArgs._NameWidget.ToSharedRef()
				: SNew(STextBlock)
				.Text(InCreateData->Action->GetMenuDescription());

		this->ChildSlot
		[
			SNew(SHorizontalBox)
			.ToolTipText(InCreateData->Action->GetTooltipDescription())
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				IconWidget
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4, 0, 0, 0)
			[
				std::move(NameSlotWidget)
			]
		];
	}

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (MouseButtonDownDelegate.Execute(ActionPtr))
		{
			return FReply::Handled();
		}

		return FReply::Unhandled();
	}

	/** The action that we want to display with this widget */
	TWeakPtr<FEdGraphSchemaAction> ActionPtr;
	/** Delegate executed when mouse button goes down */
	FCreateWidgetMouseButtonDown MouseButtonDownDelegate;
};
