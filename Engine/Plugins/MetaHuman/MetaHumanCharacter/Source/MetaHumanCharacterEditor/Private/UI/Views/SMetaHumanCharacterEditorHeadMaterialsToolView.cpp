// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetaHumanCharacterEditorHeadMaterialsToolView.h"

#include "Algo/Find.h"
#include "MetaHumanCharacterEditorStyle.h"
#include "Tools/MetaHumanCharacterEditorHeadModelTool.h"
#include "UI/Widgets/SMetaHumanCharacterEditorTeethSlidersPanel.h"
#include "UI/Widgets/SMetaHumanCharacterEditorTileView.h"
#include "UI/Widgets/SMetaHumanCharacterEditorToolPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "SWarningOrErrorBox.h"

#define LOCTEXT_NAMESPACE "SMetaHumanCharacterEditorHeadMaterialsToolView"

void SMetaHumanCharacterEditorHeadMaterialsToolView::Construct(const FArguments& InArgs, UMetaHumanCharacterEditorHeadMaterialsTool* InTool)
{
	SMetaHumanCharacterEditorToolView::Construct(SMetaHumanCharacterEditorToolView::FArguments(), InTool);
}

UInteractiveToolPropertySet* SMetaHumanCharacterEditorHeadMaterialsToolView::GetToolProperties() const
{
	TArray<UObject*> ToolProperties;
	const UMetaHumanCharacterEditorHeadMaterialsTool* HeadModelTool = Cast<UMetaHumanCharacterEditorHeadMaterialsTool>(Tool);
	if (IsValid(HeadModelTool))
	{
		constexpr bool bOnlyEnabled = true;
		ToolProperties = HeadModelTool->GetToolProperties(bOnlyEnabled);
	}

	UObject* const* SubToolProperties = Algo::FindByPredicate(ToolProperties,
		[](UObject* ToolProperty)
		{
			return IsValid(Cast<UMetaHumanCharacterHeadModelSubToolBase>(ToolProperty));
		});

	return SubToolProperties ? Cast<UInteractiveToolPropertySet>(*SubToolProperties) : nullptr;
}

void SMetaHumanCharacterEditorHeadMaterialsToolView::MakeToolView()
{
	if (ToolViewScrollBox.IsValid())
	{
		ToolViewScrollBox->AddSlot()
			.VAlign(VAlign_Top)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(TeethSubToolView, SVerticalBox)
					.Visibility(this, &SMetaHumanCharacterEditorHeadMaterialsToolView::GetTeethSubToolViewVisibility)
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(EyelashesSubToolView, SVerticalBox)
					.Visibility(this, &SMetaHumanCharacterEditorHeadMaterialsToolView::GetEyelashesSubToolViewVisibility)
				]
			];

		MakeTeethSubToolView();
		MakeEyelashesSubToolView();
		// First subtool that is opened does not trigger OnPropertySetsModified so it should be set enabled manually.
		UMetaHumanCharacterHeadModelSubToolBase* EnabledSubToolProperties = Cast<UMetaHumanCharacterHeadModelSubToolBase>(GetToolProperties());
		if (EnabledSubToolProperties)
		{
			UMetaHumanCharacterEditorHeadModelTool* HeadTool = Cast<UMetaHumanCharacterEditorHeadModelTool>(Tool);
			if (HeadTool)
			{
				HeadTool->SetEnabledSubTool(EnabledSubToolProperties, true);
			}
		}

		Tool.Pin()->OnPropertySetsModified.AddSP(this, &SMetaHumanCharacterEditorHeadMaterialsToolView::OnPropertySetsModified);
	}
}

void SMetaHumanCharacterEditorHeadMaterialsToolView::OnPropertySetsModified()
{
	UMetaHumanCharacterHeadModelSubToolBase* EnabledSubToolProperties = Cast<UMetaHumanCharacterHeadModelSubToolBase>(GetToolProperties());
	if (EnabledSubToolProperties)
	{
		UMetaHumanCharacterEditorHeadMaterialsTool* HeadTool = Cast<UMetaHumanCharacterEditorHeadMaterialsTool>(Tool);
		if (HeadTool)
		{
			HeadTool->SetEnabledSubTool(EnabledSubToolProperties, true);

			TArray<UObject*> AllSubToolProperties = Tool->GetToolProperties(false);
			for (int32 Ind = 0; Ind < AllSubToolProperties.Num(); ++Ind)
			{
				if (AllSubToolProperties[Ind] != EnabledSubToolProperties)
				{
					UMetaHumanCharacterHeadModelSubToolBase* SubTool = Cast<UMetaHumanCharacterHeadModelSubToolBase>(AllSubToolProperties[Ind]);
					if (SubTool)
					{
						HeadTool->SetEnabledSubTool(SubTool, false);
					}
				}
			}
		}
	}
}

UInteractiveToolPropertySet* SMetaHumanCharacterEditorHeadMaterialsToolView::GetEyelashesProperties() const
{
	UMetaHumanCharacterHeadModelEyelashesProperties* EyelashesToolProperties = nullptr;
	if (Tool.IsValid())
	{
		constexpr bool bOnlyEnabled = false;
		Tool->GetToolProperties(bOnlyEnabled).FindItemByClass<UMetaHumanCharacterHeadModelEyelashesProperties>(&EyelashesToolProperties);
	}

	return EyelashesToolProperties;
}

UInteractiveToolPropertySet* SMetaHumanCharacterEditorHeadMaterialsToolView::GetTeethProperties() const
{
	UMetaHumanCharacterHeadModelTeethProperties* TeethToolProperties = nullptr;
	if (Tool.IsValid())
	{
		constexpr bool bOnlyEnabled = false;
		Tool->GetToolProperties(bOnlyEnabled).FindItemByClass<UMetaHumanCharacterHeadModelTeethProperties>(&TeethToolProperties);
	}

	return TeethToolProperties;
}

void SMetaHumanCharacterEditorHeadMaterialsToolView::MakeEyelashesSubToolView()
{
	if (EyelashesSubToolView.IsValid())
	{
		EyelashesSubToolView->AddSlot()
			.AutoHeight()
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.Padding(4.f)
				.AutoHeight()
				[
					CreateEyelashesSubToolViewMaterialSection()
				]
			];
	}
}

void SMetaHumanCharacterEditorHeadMaterialsToolView::MakeTeethSubToolView()
{
	if (TeethSubToolView.IsValid())
	{
		TeethSubToolView->AddSlot()
			.AutoHeight()
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.Padding(4.f)
				.AutoHeight()
				[
					CreateTeethSubToolViewMaterialsSection()
				]
			];
	}
}

TSharedRef<SWidget> SMetaHumanCharacterEditorHeadMaterialsToolView::CreateEyelashesSubToolViewMaterialSection()
{
	UMetaHumanCharacterHeadModelEyelashesProperties* HeadModelProperties = Cast<UMetaHumanCharacterHeadModelEyelashesProperties>(GetEyelashesProperties());
	FMetaHumanCharacterEyelashesProperties* EyelashesProperties = IsValid(HeadModelProperties) ? &HeadModelProperties->Eyelashes : nullptr;
	if (!EyelashesProperties)
	{
		return SNullWidget::NullWidget;
	}

	FProperty* DyeColorProperty = FMetaHumanCharacterEyelashesProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterEyelashesProperties, DyeColor));
	FProperty* MelaninProperty = FMetaHumanCharacterEyelashesProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterEyelashesProperties, Melanin));
	FProperty* RednessProperty = FMetaHumanCharacterEyelashesProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterEyelashesProperties, Redness));
	FProperty* RoughnessProperty = FMetaHumanCharacterEyelashesProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterEyelashesProperties, Roughness));
	//FProperty* SaltAndPepperProperty = FMetaHumanCharacterEyelashesProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterEyelashesProperties, SaltAndPepper));
	//FProperty* LightnessProperty = FMetaHumanCharacterEyelashesProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterEyelashesProperties, Lightness));

	const TSharedRef<SWidget> EyelashesMaterialWidget =
		SNew(SMetaHumanCharacterEditorToolPanel)
		.Label(LOCTEXT("EyelashesMaterialSectionLabel", "Material"))
		.Content()
		[
			SNew(SVerticalBox)

			// Color color picker section
			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				CreatePropertyColorPickerWidget(TEXT("Dye Color"), DyeColorProperty, EyelashesProperties)
			]

			// TODO: Replace Melanin and Redness with a color picker similar to the Skin Tone one
			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				CreatePropertySpinBoxWidget(TEXT("Melanin"), MelaninProperty, EyelashesProperties)
			]

			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				CreatePropertySpinBoxWidget(TEXT("Redness"), RednessProperty, EyelashesProperties)
			]

			// Roughness spin box section
			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				CreatePropertySpinBoxWidget(TEXT("Roughness"), RoughnessProperty, EyelashesProperties)
			]

			// TODO: Salt&Pepper and Lightness are only used in the Groom materials, not in the cards
			// Reenable this when groom material overrides are supported
			// Salt&Pepper spin box section
			// + SVerticalBox::Slot()
			// .MinHeight(24.f)
			// .Padding(2.f, 0.f)
			// .AutoHeight()
			// [
			// 	CreatePropertySpinBoxWidget(TEXT("Salt & Pepper"), SaltAndPepperProperty, EyelashesProperties)
			// ]

			// Lightness spin box section
			// + SVerticalBox::Slot()
			// .MinHeight(24.f)
			// .Padding(2.f, 0.f)
			// .AutoHeight()
			// [
			// 	CreatePropertySpinBoxWidget(TEXT("Lightness"), LightnessProperty, EyelashesProperties)
			// ]
	];

	return EyelashesMaterialWidget;
}


TSharedRef<SWidget> SMetaHumanCharacterEditorHeadMaterialsToolView::CreateTeethSubToolViewMaterialsSection()
{
	UMetaHumanCharacterHeadModelTeethProperties* HeadModelProperties = Cast<UMetaHumanCharacterHeadModelTeethProperties>(GetTeethProperties());
	FMetaHumanCharacterTeethProperties* TeethProperties = IsValid(HeadModelProperties) ? &HeadModelProperties->Teeth : nullptr;
	if (!TeethProperties)
	{
		return SNullWidget::NullWidget;
	}

	FProperty* TeethColorProperty = FMetaHumanCharacterTeethProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterTeethProperties, TeethColor));
	FProperty* GumColorProperty = FMetaHumanCharacterTeethProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterTeethProperties, GumColor));
	FProperty* PlaqueColorProperty = FMetaHumanCharacterTeethProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterTeethProperties, PlaqueColor));
	FProperty* PlaqueAmountProperty = FMetaHumanCharacterTeethProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterTeethProperties, PlaqueAmount));
	FProperty* JawOpenProperty = FMetaHumanCharacterTeethProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterTeethProperties, JawOpen));

	const TSharedRef<SWidget> TeetParametersWidget =
		SNew(SMetaHumanCharacterEditorToolPanel)
		.Label(LOCTEXT("TeethParametersSectionLabel", "Material"))
		.Content()
		[
			SNew(SVerticalBox)

			// Teeth color section
			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				CreatePropertyColorPickerWidget(TEXT("Teeth Color"), TeethColorProperty, TeethProperties)
			]

			// Gum color section
			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				CreatePropertyColorPickerWidget(TEXT("Gum Color"), GumColorProperty, TeethProperties)
			]

			// Plaque color section
			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				CreatePropertyColorPickerWidget(TEXT("Plaque Color"), PlaqueColorProperty, TeethProperties)
			]

			// Plaque amount spin box section
			+SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				CreatePropertySpinBoxWidget(TEXT("Plaque Amount"), PlaqueAmountProperty, TeethProperties)
			]

			// JawOpen spin box section
			+SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				CreatePropertySpinBoxWidget(TEXT("Jaw Open"), JawOpenProperty, TeethProperties)
			]
		];

	return TeetParametersWidget;
}

const FSlateBrush* SMetaHumanCharacterEditorHeadMaterialsToolView::GetEyelashesSectionBrush(uint8 InItem)
{
	const FString EyelashesMaskName = StaticEnum<EMetaHumanCharacterEyelashesType>()->GetAuthoredNameStringByValue(InItem);
	const FString EyelashesMaskBrushName = FString::Format(TEXT("Eyelashes.{0}"), { EyelashesMaskName });
	return FMetaHumanCharacterEditorStyle::Get().GetBrush(*EyelashesMaskBrushName);
}

const FSlateBrush* SMetaHumanCharacterEditorHeadMaterialsToolView::GetTeethSectionBrush(uint8 InItem)
{
	// TODO this doesn't exist yet
	const FString TeethMaskName = StaticEnum<EMetaHumanCharacterTeethType>()->GetAuthoredNameStringByValue(InItem);
	const FString TeethMaskBrushName = FString::Format(TEXT("Teeth.{0}"), { TeethMaskName });
	return FMetaHumanCharacterEditorStyle::Get().GetBrush(*TeethMaskBrushName);
}

EVisibility SMetaHumanCharacterEditorHeadMaterialsToolView::GetEyelashesSubToolViewVisibility() const
{
	const bool bIsVisible = IsValid(Cast<UMetaHumanCharacterHeadModelEyelashesProperties>(GetToolProperties()));
	return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SMetaHumanCharacterEditorHeadMaterialsToolView::GetTeethSubToolViewVisibility() const
{
	const bool bIsVisible = IsValid(Cast<UMetaHumanCharacterHeadModelTeethProperties>(GetToolProperties()));
	return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
}

#undef LOCTEXT_NAMESPACE
