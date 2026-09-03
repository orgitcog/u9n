// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetaHumanCharacterEditorToolPanel.h"

#include "MetaHumanCharacterEditorStyle.h"
#include "Styling/SlateBrush.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SMetaHumanCharacterEditorToolPanel"

void SMetaHumanCharacterEditorArrowButton::Construct(const FArguments& InArgs)
{
	ChildSlot
		[
			SAssignNew(Button, SButton)
			.ButtonStyle(FCoreStyle::Get(), "NoBorder")
			.ClickMethod(EButtonClickMethod::MouseDown)
			.ContentPadding(0.f)
			.ForegroundColor(FSlateColor::UseForeground())
			.IsFocusable(false)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.ToolTipText(InArgs._ToolTipText)
			.OnClicked(this, &SMetaHumanCharacterEditorArrowButton::OnArrowButtonClicked)
			[
				SNew(SImage)
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.Image(this, &SMetaHumanCharacterEditorArrowButton::GetArrowButtonImage)
			]
		];
}

FReply SMetaHumanCharacterEditorArrowButton::OnArrowButtonClicked()
{
	bIsExpanded = !bIsExpanded;

	return FReply::Handled();
}

const FSlateBrush* SMetaHumanCharacterEditorArrowButton::GetArrowButtonImage() const
{
	if (!Button.IsValid())
	{
		return nullptr;
	}

	if (bIsExpanded)
	{
		if (Button->IsHovered())
		{
			return FAppStyle::GetBrush("TreeArrow_Expanded_Hovered");
		}
		else
		{
			return FAppStyle::GetBrush("TreeArrow_Expanded");
		}
	}
	else
	{
		if (Button->IsHovered())
		{
			return FAppStyle::GetBrush("TreeArrow_Collapsed_Hovered");
		}
		else
		{
			return FAppStyle::GetBrush("TreeArrow_Collapsed");
		}
	}
}

void SMetaHumanCharacterEditorToolPanel::Construct(const FArguments& InArgs)
{
	bRoundedBorders = InArgs._RoundedBorders;
	HierarchyLevel = InArgs._HierarchyLevel;
	IconBrushAttribute = InArgs._IconBrush;

	const FLinearColor TopLevelBorderLabelColor = FLinearColor(.03f, .03f, .03f, 1.f);
	const FLinearColor MidLevelBorderLabelColor = FLinearColor(.015f, .015f, .015f, 1.f);
	const FLinearColor LowLevelBorderLabelColor = FLinearColor(.005f, .005f, .005f, 1.f);

	const FLinearColor TopLevelBorderBackgroundColor = FLinearColor(.02f, .02f, .02f, 1.f);
	const FLinearColor MidLevelBorderBackgroundColor = FLinearColor(.005f, .005f, .005f, 1.f);
	const FLinearColor LowLevelBorderBackgroundColor = FLinearColor(.0f, .0f, .0f, 1.f);
	
	FLinearColor BorderLabelColor;
	FLinearColor BorderBackgroundColor;
	switch (HierarchyLevel)
	{
	case EMetaHumanCharacterEditorPanelHierarchyLevel::Top:
		BorderLabelColor = TopLevelBorderLabelColor;
		BorderBackgroundColor = TopLevelBorderBackgroundColor;
		break;
	case EMetaHumanCharacterEditorPanelHierarchyLevel::Middle:
		BorderLabelColor = MidLevelBorderLabelColor;
		BorderBackgroundColor = MidLevelBorderBackgroundColor;
		break;
	case EMetaHumanCharacterEditorPanelHierarchyLevel::Low:
		BorderLabelColor = LowLevelBorderLabelColor;
		BorderBackgroundColor = LowLevelBorderBackgroundColor;
		break;
	default:
		BorderLabelColor = TopLevelBorderLabelColor;
		BorderBackgroundColor = TopLevelBorderBackgroundColor;
		break;
	}

	ChildSlot
		.Padding(InArgs._Padding)
		[
			SNew(SBorder)
			.BorderImage(this, &SMetaHumanCharacterEditorToolPanel::GetPanelBorderBrush)
			.BorderBackgroundColor(BorderBackgroundColor)
			.Visibility(InArgs._Visibility)
			[
				SNew(SVerticalBox)

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(-2.f)
				[
					SNew(SBox)
					.HeightOverride(24.f)
					[
						SNew(SOverlay)

						+ SOverlay::Slot()
						[
							SNew(SVerticalBox)

							+ SVerticalBox::Slot()
							.FillHeight(.5f)
							[
								SNew(SBorder)
								.BorderImage(this, &SMetaHumanCharacterEditorToolPanel::GetPanelBorderBrush)
								.BorderBackgroundColor(BorderLabelColor)
							]

							+ SVerticalBox::Slot()
							.FillHeight(.5f)
							[
								SNew(SBorder)
								.BorderImage(FMetaHumanCharacterEditorStyle::Get().GetBrush("MetaHumanCharacterEditorTools.WhiteBrush"))
								.BorderBackgroundColor(BorderLabelColor)
								.Visibility(this, &SMetaHumanCharacterEditorToolPanel::GetContentSlotVisibility)
							]
						]

						+SOverlay::Slot()
						[
							SNew(SBox)
							.HeightOverride(24.f)
							[
								SNew(SBorder)
								.BorderImage(this, &SMetaHumanCharacterEditorToolPanel::GetPanelBorderBrush)
								.BorderBackgroundColor(BorderLabelColor)
								.Padding(2.f, 0.f)
								.VAlign(VAlign_Center)
								[
									SNew(SHorizontalBox)

									+SHorizontalBox::Slot()
									.HAlign(HAlign_Left)
									.AutoWidth()
									[
										SAssignNew(ArrowButton, SMetaHumanCharacterEditorArrowButton)
									]

									+ SHorizontalBox::Slot()
									.Padding(2.f, 0.f)
									.AutoWidth()
									[
										SNew(SImage)
										.Image(IconBrushAttribute)
										.DesiredSizeOverride(FVector2D(20.f, 20.f))
										.Visibility(IconBrushAttribute.IsSet() ? EVisibility::Visible : EVisibility::Collapsed)
									]

									+ SHorizontalBox::Slot()
									.Padding(4.f, 0.f)
									.VAlign(VAlign_Center)
									.HAlign(HAlign_Left)
									.AutoWidth()
									[
										SNew(STextBlock)
										.Clipping(EWidgetClipping::ClipToBoundsAlways)
										.Text(InArgs._Label)
										.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.BoldFont")))
										.OverflowPolicy(ETextOverflowPolicy::MiddleEllipsis)
									]
									+ SHorizontalBox::Slot()
									.HAlign(HAlign_Right)
									.Padding(2.f, 0.f)
									[
										SNew(SBox)
										[
											InArgs._HeaderContent.Widget
										]
									]
								]
							]
						]
					]
				]

				+ SVerticalBox::Slot()
				.Padding(0.f, 2.f, 0.f, 0.f)
				.AutoHeight()
				[
					SNew(SBox)
					.Visibility(this, &SMetaHumanCharacterEditorToolPanel::GetContentSlotVisibility)
					[
						InArgs._Content.Widget
					]
				]
			]
		];
}

const FSlateBrush* SMetaHumanCharacterEditorToolPanel::GetPanelBorderBrush() const
{
	return
		bRoundedBorders ?
		FMetaHumanCharacterEditorStyle::Get().GetBrush("MetaHumanCharacterEditorTools.Rounded.WhiteBrush") :
		FMetaHumanCharacterEditorStyle::Get().GetBrush("MetaHumanCharacterEditorTools.WhiteBrush");
}

EVisibility SMetaHumanCharacterEditorToolPanel::GetContentSlotVisibility() const
{
	return ArrowButton.IsValid() && ArrowButton->IsExpanded() ? EVisibility::Visible : EVisibility::Collapsed;
}

#undef LOCTEXT_NAMESPACE
