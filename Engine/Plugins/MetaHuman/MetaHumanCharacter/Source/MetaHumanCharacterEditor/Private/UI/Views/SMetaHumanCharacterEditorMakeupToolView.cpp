// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetaHumanCharacterEditorMakeupToolView.h"

#include "InteractiveTool.h"
#include "MetaHumanCharacterEditorStyle.h"
#include "Tools/MetaHumanCharacterEditorMakeupTool.h"
#include "UI/Widgets/SMetaHumanCharacterEditorTileView.h"
#include "UI/Widgets/SMetaHumanCharacterEditorToolPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"

#define LOCTEXT_NAMESPACE "SMetaHumanCharacterEditorMakeupToolView"

void SMetaHumanCharacterEditorMakeupToolView::Construct(const FArguments& InArgs, UMetaHumanCharacterEditorMakeupTool* InTool)
{
	SMetaHumanCharacterEditorToolView::Construct(SMetaHumanCharacterEditorToolView::FArguments(), InTool);
}

UInteractiveToolPropertySet* SMetaHumanCharacterEditorMakeupToolView::GetToolProperties() const
{
	const UMetaHumanCharacterEditorMakeupTool* MakeupTool = Cast<UMetaHumanCharacterEditorMakeupTool>(Tool);
	return IsValid(MakeupTool) ? MakeupTool->GetMakeupToolProperties() : nullptr;
}

void SMetaHumanCharacterEditorMakeupToolView::MakeToolView()
{
	if (ToolViewScrollBox.IsValid())
	{
		ToolViewScrollBox->AddSlot()
			.VAlign(VAlign_Top)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.Padding(4.f)
				.AutoHeight()
				[
					CreateMakeupToolViewFoundationSection()
				]

				+ SVerticalBox::Slot()
				.Padding(4.f)
				.AutoHeight()
				[
					CreateMakeupToolViewEyesSection()
				]

				+ SVerticalBox::Slot()
				.Padding(4.f)
				.AutoHeight()
				[
					CreateMakeupToolViewBlushSection()
				]

				+ SVerticalBox::Slot()
				.Padding(4.f)
				.AutoHeight()
				[
					CreateMakeupToolViewLipsSection()
				]
			];
	}
}

TSharedRef<SWidget> SMetaHumanCharacterEditorMakeupToolView::CreateMakeupToolViewFoundationSection()
{
	UMetaHumanCharacterEditorMakeupToolProperties* MakeupToolProperties = Cast<UMetaHumanCharacterEditorMakeupToolProperties>(GetToolProperties());
	void* FoundationProperties = IsValid(MakeupToolProperties) ? &MakeupToolProperties->Foundation : nullptr;
	if (!FoundationProperties)
	{
		return SNullWidget::NullWidget;
	}

	FProperty* ApplyFoundationProperty = FMetaHumanCharacterFoundationMakeupProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterFoundationMakeupProperties, bApplyFoundation));
	FProperty* ColorProperty = FMetaHumanCharacterFoundationMakeupProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterFoundationMakeupProperties, Color));
	FProperty* IntensityProperty = FMetaHumanCharacterFoundationMakeupProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterFoundationMakeupProperties, Intensity));
	FProperty* RoughnessProperty = FMetaHumanCharacterFoundationMakeupProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterFoundationMakeupProperties, Roughness));
	FProperty* ConcealerProperty = FMetaHumanCharacterFoundationMakeupProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterFoundationMakeupProperties, Concealer));

	const TSharedRef<SWidget> FoundationSectionWidget =
		SNew(SMetaHumanCharacterEditorToolPanel)
		.Label(LOCTEXT("FoundationSectionLabel", "Foundation"))
		.Content()
		[
			SNew(SVerticalBox)

			// ApplyFoundation check box section
			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				CreatePropertyCheckBoxWidget(TEXT("Apply Foundation"), ApplyFoundationProperty, FoundationProperties)
			]

			// Color color picker section
			+SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				CreatePropertyColorPickerWidget(TEXT("Color"), ColorProperty, FoundationProperties)
			]

			// Intensity spinbox section
			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				CreatePropertySpinBoxWidget(TEXT("Intensity"), IntensityProperty, FoundationProperties)
			]

			// Roughness spinbox section
			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				CreatePropertySpinBoxWidget(TEXT("Roughness"), RoughnessProperty, FoundationProperties)
			]

			// Concealer spinbox section
			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				CreatePropertySpinBoxWidget(TEXT("Concealer"), ConcealerProperty, FoundationProperties)
			]
		];

	return FoundationSectionWidget;
}

TSharedRef<SWidget> SMetaHumanCharacterEditorMakeupToolView::CreateMakeupToolViewEyesSection()
{
	UMetaHumanCharacterEditorMakeupToolProperties* MakeupToolProperties = Cast<UMetaHumanCharacterEditorMakeupToolProperties>(GetToolProperties());
	void* EyesProperties = IsValid(MakeupToolProperties) ? &MakeupToolProperties->Eyes : nullptr;
	if (!EyesProperties)
	{
		return SNullWidget::NullWidget;
	}

	FProperty* TypeProperty = FMetaHumanCharacterEyeMakeupProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterEyeMakeupProperties, Type));
	FProperty* PrimaryColorProperty = FMetaHumanCharacterEyeMakeupProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterEyeMakeupProperties, PrimaryColor));
	FProperty* SecondaryColorProperty = FMetaHumanCharacterEyeMakeupProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterEyeMakeupProperties, SecondaryColor));
	FProperty* RoughnessProperty = FMetaHumanCharacterEyeMakeupProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterEyeMakeupProperties, Roughness));
	FProperty* OpacityProperty = FMetaHumanCharacterEyeMakeupProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterEyeMakeupProperties, Opacity));
	FProperty* MetalnessProperty = FMetaHumanCharacterEyeMakeupProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterEyeMakeupProperties, Metalness));

	const TSharedRef<SWidget> EyesSectionWidget =
		SNew(SMetaHumanCharacterEditorToolPanel)
		.Label(LOCTEXT("EyesSectionLabel", "Eyes"))
		.Content()
		[
			SNew(SVerticalBox)

			// Type tile view section
			+ SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.Padding(4.f)
			.AutoHeight()
			[
				SNew(SMetaHumanCharacterEditorTileView<EMetaHumanCharacterEyeMakeupType>)
				.OnGetSlateBrush(this, &SMetaHumanCharacterEditorMakeupToolView::GetEyesSectionBrush)
				.OnSelectionChanged(this, &SMetaHumanCharacterEditorMakeupToolView::OnEnumPropertyValueChanged, TypeProperty, EyesProperties)
				.InitiallySelectedItem(MakeupToolProperties->Eyes.Type)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSeparator)
				.Thickness(1.f)
			]

			// Primary Color color picker section
			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				CreatePropertyColorPickerWidget(TEXT("Primary Color"), PrimaryColorProperty, EyesProperties)
			]

			// Secondary Color color picker section
			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				CreatePropertyColorPickerWidget(TEXT("Secondary Color"), SecondaryColorProperty, EyesProperties)
			]

			// Roughness spinbox section
			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				CreatePropertySpinBoxWidget(TEXT("Roughness"), RoughnessProperty, EyesProperties)
			]

			// Opacity spinbox section
			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				CreatePropertySpinBoxWidget(TEXT("Opacity"), OpacityProperty, EyesProperties)
			]

			// Metalness spinbox section
			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				CreatePropertySpinBoxWidget(TEXT("Metalness"), MetalnessProperty, EyesProperties)
			]
		];

	return EyesSectionWidget;
}

TSharedRef<SWidget> SMetaHumanCharacterEditorMakeupToolView::CreateMakeupToolViewBlushSection()
{
	UMetaHumanCharacterEditorMakeupToolProperties* MakeupToolProperties = Cast<UMetaHumanCharacterEditorMakeupToolProperties>(GetToolProperties());
	void* BlushProperties = IsValid(MakeupToolProperties) ? &MakeupToolProperties->Blush : nullptr;
	if (!BlushProperties)
	{
		return SNullWidget::NullWidget;
	}

	FProperty* TypeProperty = FMetaHumanCharacterBlushMakeupProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterBlushMakeupProperties, Type));
	FProperty* ColorProperty = FMetaHumanCharacterBlushMakeupProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterBlushMakeupProperties, Color));
	FProperty* IntensityProperty = FMetaHumanCharacterBlushMakeupProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterBlushMakeupProperties, Intensity));
	FProperty* RoughnessProperty = FMetaHumanCharacterBlushMakeupProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterBlushMakeupProperties, Roughness));

	const TSharedRef<SWidget> BlushSectionWidget =
		SNew(SMetaHumanCharacterEditorToolPanel)
		.Label(LOCTEXT("BlushSectionLabel", "Blush"))
		.Content()
		[
			SNew(SVerticalBox)

			// Type tile view section
			+ SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.Padding(4.f)
			.AutoHeight()
			[
				SNew(SMetaHumanCharacterEditorTileView<EMetaHumanCharacterBlushMakeupType>)
				.OnGetSlateBrush(this, &SMetaHumanCharacterEditorMakeupToolView::GetBlushSectionBrush)
				.OnSelectionChanged(this, &SMetaHumanCharacterEditorMakeupToolView::OnEnumPropertyValueChanged, TypeProperty, BlushProperties)
				.InitiallySelectedItem(MakeupToolProperties->Blush.Type)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSeparator)
				.Thickness(1.f)
			]

			// Color color picker section
			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				CreatePropertyColorPickerWidget(TEXT("Color"), ColorProperty, BlushProperties)
			]

			// Intensity spinbox section
			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				CreatePropertySpinBoxWidget(TEXT("Intensity"), IntensityProperty, BlushProperties)
			]

			// Roughness spinbox section
			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				CreatePropertySpinBoxWidget(TEXT("Roughness"), RoughnessProperty, BlushProperties)
			]
		];

	return BlushSectionWidget;
}

TSharedRef<SWidget> SMetaHumanCharacterEditorMakeupToolView::CreateMakeupToolViewLipsSection()
{
	UMetaHumanCharacterEditorMakeupToolProperties* MakeupToolProperties = Cast<UMetaHumanCharacterEditorMakeupToolProperties>(GetToolProperties());
	void* LipsProperties = IsValid(MakeupToolProperties) ? &MakeupToolProperties->Lips : nullptr;
	if (!LipsProperties)
	{
		return SNullWidget::NullWidget;
	}

	FProperty* TypeProperty = FMetaHumanCharacterLipsMakeupProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterLipsMakeupProperties, Type));
	FProperty* ColorProperty = FMetaHumanCharacterLipsMakeupProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterLipsMakeupProperties, Color));
	FProperty* RoughnessProperty = FMetaHumanCharacterLipsMakeupProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterLipsMakeupProperties, Roughness));
	FProperty* OpacityProperty = FMetaHumanCharacterLipsMakeupProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterLipsMakeupProperties, Opacity));
	FProperty* MetalnessProperty = FMetaHumanCharacterLipsMakeupProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterLipsMakeupProperties, Metalness));

	const TSharedRef<SWidget> LipsSectionWidget =
		SNew(SMetaHumanCharacterEditorToolPanel)
		.Label(LOCTEXT("LipsSectionLabel", "Lips"))
		.Content()
		[
			SNew(SVerticalBox)

			// Type tile view section
			+ SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.Padding(4.f)
			.AutoHeight()
			[
				SNew(SMetaHumanCharacterEditorTileView<EMetaHumanCharacterLipsMakeupType>)
				.OnGetSlateBrush(this, &SMetaHumanCharacterEditorMakeupToolView::GetLipsSectionBrush)
				.OnSelectionChanged(this, &SMetaHumanCharacterEditorMakeupToolView::OnEnumPropertyValueChanged, TypeProperty, LipsProperties)
				.InitiallySelectedItem(MakeupToolProperties->Lips.Type)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSeparator)
				.Thickness(1.f)
			]

			// Color color picker section
			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				CreatePropertyColorPickerWidget(TEXT("Color"), ColorProperty, LipsProperties)
			]

			// Roughness spinbox section
			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				CreatePropertySpinBoxWidget(TEXT("Roughness"), RoughnessProperty, LipsProperties)
			]

			// Opacity spinbox section
			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				CreatePropertySpinBoxWidget(TEXT("Opacity"), OpacityProperty, LipsProperties)
			]

			// Metalness spinbox
			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				CreatePropertySpinBoxWidget(MetalnessProperty->GetDisplayNameText().ToString(), MetalnessProperty, LipsProperties)
			]
		];

	return LipsSectionWidget;
}

const FSlateBrush* SMetaHumanCharacterEditorMakeupToolView::GetEyesSectionBrush(uint8 InItem)
{
	const FString EyesMaskName = StaticEnum<EMetaHumanCharacterEyeMakeupType>()->GetAuthoredNameStringByValue(InItem);
	const FString EyesMaskBrushName = FString::Format(TEXT("Makeup.Eyes.{0}"), { EyesMaskName });
	return FMetaHumanCharacterEditorStyle::Get().GetBrush(*EyesMaskBrushName);
}

const FSlateBrush* SMetaHumanCharacterEditorMakeupToolView::GetBlushSectionBrush(uint8 InItem)
{
	const FString BlushMaskName = StaticEnum<EMetaHumanCharacterBlushMakeupType>()->GetAuthoredNameStringByValue(InItem);
	const FString BlushMaskBrushName = FString::Format(TEXT("Makeup.Blush.{0}"), { BlushMaskName });
	return FMetaHumanCharacterEditorStyle::Get().GetBrush(*BlushMaskBrushName);
}

const FSlateBrush* SMetaHumanCharacterEditorMakeupToolView::GetLipsSectionBrush(uint8 InItem)
{
	const FString LipsMaskName = StaticEnum<EMetaHumanCharacterLipsMakeupType>()->GetAuthoredNameStringByValue(InItem);
	const FString LipsMaskBrushName = FString::Format(TEXT("Makeup.Lips.{0}"), { LipsMaskName });
	return FMetaHumanCharacterEditorStyle::Get().GetBrush(*LipsMaskBrushName);
}

#undef LOCTEXT_NAMESPACE
