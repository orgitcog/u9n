// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetaHumanCharacterEditorHeadModelToolView.h"

#include "Algo/Find.h"
#include "MetaHumanCharacterEditorStyle.h"
#include "Tools/MetaHumanCharacterEditorHeadModelTool.h"
#include "UI/Widgets/SMetaHumanCharacterEditorTeethSlidersPanel.h"
#include "UI/Widgets/SMetaHumanCharacterEditorTileView.h"
#include "UI/Widgets/SMetaHumanCharacterEditorToolPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "SWarningOrErrorBox.h"

#define LOCTEXT_NAMESPACE "SMetaHumanCharacterEditorHeadModelToolView"

void SMetaHumanCharacterEditorHeadModelToolView::Construct(const FArguments& InArgs, UMetaHumanCharacterEditorHeadModelTool* InTool)
{
	SMetaHumanCharacterEditorToolView::Construct(SMetaHumanCharacterEditorToolView::FArguments(), InTool);
}

UInteractiveToolPropertySet* SMetaHumanCharacterEditorHeadModelToolView::GetToolProperties() const
{
	TArray<UObject*> ToolProperties;
	const UMetaHumanCharacterEditorHeadModelTool* HeadModelTool = Cast<UMetaHumanCharacterEditorHeadModelTool>(Tool);
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

void SMetaHumanCharacterEditorHeadModelToolView::MakeToolView()
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
						.Visibility(this, &SMetaHumanCharacterEditorHeadModelToolView::GetTeethSubToolViewVisibility)
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(EyelashesSubToolView, SVerticalBox)
					.Visibility(this, &SMetaHumanCharacterEditorHeadModelToolView::GetEyelashesSubToolViewVisibility)
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

		Tool.Pin()->OnPropertySetsModified.AddSP(this, &SMetaHumanCharacterEditorHeadModelToolView::OnPropertySetsModified);
	}
}

void SMetaHumanCharacterEditorHeadModelToolView::OnPropertySetsModified()
{
	UMetaHumanCharacterHeadModelSubToolBase* EnabledSubToolProperties = Cast<UMetaHumanCharacterHeadModelSubToolBase>(GetToolProperties());
	if (EnabledSubToolProperties)
	{
		UMetaHumanCharacterEditorHeadModelTool* HeadTool = Cast<UMetaHumanCharacterEditorHeadModelTool>(Tool);
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

UInteractiveToolPropertySet* SMetaHumanCharacterEditorHeadModelToolView::GetEyelashesProperties() const
{
	UMetaHumanCharacterHeadModelEyelashesProperties* EyelashesToolProperties = nullptr;
	if (Tool.IsValid())
	{
		constexpr bool bOnlyEnabled = false;
		Tool->GetToolProperties(bOnlyEnabled).FindItemByClass<UMetaHumanCharacterHeadModelEyelashesProperties>(&EyelashesToolProperties);
	}

	return EyelashesToolProperties;
}

UInteractiveToolPropertySet* SMetaHumanCharacterEditorHeadModelToolView::GetTeethProperties() const
{
	UMetaHumanCharacterHeadModelTeethProperties* TeethToolProperties = nullptr;
	if (Tool.IsValid())
	{
		constexpr bool bOnlyEnabled = false;
		Tool->GetToolProperties(bOnlyEnabled).FindItemByClass<UMetaHumanCharacterHeadModelTeethProperties>(&TeethToolProperties);
	}

	return TeethToolProperties;
}

void SMetaHumanCharacterEditorHeadModelToolView::MakeEyelashesSubToolView()
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
					CreateEyelashesSubToolViewStyleSection()
				]

				+ SVerticalBox::Slot()
				.Padding(4.f)
				.AutoHeight()
				[
					CreateEyelashesSubToolViewMaterialSection()
				]
			];
	}
}

void SMetaHumanCharacterEditorHeadModelToolView::MakeTeethSubToolView()
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
					CreateTeethSubToolViewParametersSection()
				]
			];
	}
}

TSharedRef<SWidget> SMetaHumanCharacterEditorHeadModelToolView::CreateEyelashesSubToolViewStyleSection()
{
	UMetaHumanCharacterHeadModelEyelashesProperties* HeadModelProperties = Cast<UMetaHumanCharacterHeadModelEyelashesProperties>(GetEyelashesProperties());
	FMetaHumanCharacterEyelashesProperties* EyelashesProperties = IsValid(HeadModelProperties) ? &HeadModelProperties->Eyelashes : nullptr;
	if (!EyelashesProperties)
	{
		return SNullWidget::NullWidget;
	}

	FProperty* TypeProperty = FMetaHumanCharacterEyelashesProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterEyelashesProperties, Type));
	const TSharedRef<SWidget> EyelashesStyleWidget =
		SNew(SMetaHumanCharacterEditorToolPanel)
		.Label(LOCTEXT("EyelashesStyleSectionLabel", "Style"))
		.Content()
		[
			SNew(SVerticalBox)

			// Type combo box section
			+ SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.Padding(4.f)
			.AutoHeight()
			[
				SNew(SMetaHumanCharacterEditorTileView<EMetaHumanCharacterEyelashesType>)
				.OnGetSlateBrush(this, &SMetaHumanCharacterEditorHeadModelToolView::GetEyelashesSectionBrush)
				.OnSelectionChanged(this, &SMetaHumanCharacterEditorHeadModelToolView::OnEnumPropertyValueChanged, TypeProperty, static_cast<void*>(EyelashesProperties))
				.InitiallySelectedItem(EyelashesProperties->Type)
			]
		];

	return EyelashesStyleWidget;
}

TSharedRef<SWidget> SMetaHumanCharacterEditorHeadModelToolView::CreateEyelashesSubToolViewMaterialSection()
{
	UMetaHumanCharacterHeadModelEyelashesProperties* HeadModelProperties = Cast<UMetaHumanCharacterHeadModelEyelashesProperties>(GetEyelashesProperties());
	FMetaHumanCharacterEyelashesProperties* EyelashesProperties = IsValid(HeadModelProperties) ? &HeadModelProperties->Eyelashes : nullptr;
	if (!EyelashesProperties)
	{
		return SNullWidget::NullWidget;
	}

	FProperty* EnableGroomsProperty = FMetaHumanCharacterEyelashesProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterEyelashesProperties, bEnableGrooms));
	const TSharedRef<SWidget> EyelashesMaterialWidget =
		SNew(SMetaHumanCharacterEditorToolPanel)
		.Label(LOCTEXT("EyelashesMaterialSectionLabel", "Grooms"))
		.Content()
		[
			SNew(SVerticalBox)

			// Toggle eyelashes grooming
			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[				
				CreatePropertyCheckBoxWidget(TEXT("Enable Grooms"), EnableGroomsProperty, EyelashesProperties)
			]
	];

	return EyelashesMaterialWidget;
}


TSharedRef<SWidget> SMetaHumanCharacterEditorHeadModelToolView::CreateTeethSubToolViewParametersSection()
{
	UMetaHumanCharacterHeadModelTeethProperties* HeadModelProperties = Cast<UMetaHumanCharacterHeadModelTeethProperties>(GetTeethProperties());
	FMetaHumanCharacterTeethProperties* TeethProperties = IsValid(HeadModelProperties) ? &HeadModelProperties->Teeth : nullptr;
	if (!TeethProperties)
	{
		return SNullWidget::NullWidget;
	}

	FProperty* EditablePropertyProperty = UMetaHumanCharacterHeadModelTeethProperties::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterHeadModelTeethProperties, EditableProperty));
	FProperty* VariationProperty = FMetaHumanCharacterTeethProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterTeethProperties, Variation));
	FProperty* JawOpenProperty = FMetaHumanCharacterTeethProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterTeethProperties, JawOpen));

	const FString EditableTeethPropertyName = StaticEnum<EMetaHumanCharacterTeethPropertyType>()->GetAuthoredNameStringByValue(static_cast<int64>(HeadModelProperties->EditableProperty));
	FProperty* TeethProperty = FMetaHumanCharacterTeethProperties::StaticStruct()->FindPropertyByName(*EditableTeethPropertyName);

	const TSharedRef<SWidget> TeetParametersWidget =
		SNew(SMetaHumanCharacterEditorToolPanel)
		.Label(LOCTEXT("TeethParametersSectionLabel", "Parameters"))
		.Content()
		[
			SNew(SVerticalBox)

			// Teeth panel section
			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(4.f)
			.AutoHeight()
			[
				SNew(SMetaHumanCharacterEditorTeethSlidersPanel)
				.OnTeethSliderPropertyEdited(this, &SMetaHumanCharacterEditorHeadModelToolView::OnTeethSliderPropertyEdited)
				.OnTeethSliderValueChanged(this, &SMetaHumanCharacterEditorHeadModelToolView::OnTeethSliderValueChanged, static_cast<void*>(TeethProperties))
				.OnGetTeethSliderValue(this, &SMetaHumanCharacterEditorHeadModelToolView::GetFloatPropertyValue, static_cast<void*>(TeethProperties))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSeparator)
				.Thickness(1.f)
			]

			// Teeth Editable Property section
			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
				.FillWidth(.3f)
				.VAlign(VAlign_Center)
				.Padding(4.f, 2.f)
				[
					SAssignNew(TeethEditablePropertyComboBox, SMetaHumanCharacterEditorComboBox<EMetaHumanCharacterTeethPropertyType>)
					.InitiallySelectedItem(HeadModelProperties->EditableProperty)
					.OnSelectionChanged(this, &SMetaHumanCharacterEditorHeadModelToolView::OnTeethEditablePropertyValueChanged, EditablePropertyProperty, static_cast<void*>(HeadModelProperties))
				]

				+ SHorizontalBox::Slot()
				.FillWidth(.7f)
				.VAlign(VAlign_Center)
				.Padding(20.f, 2.f, 40.f, 2.f)
				[
					SAssignNew(TeethEditablePropertyBox, SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						CreatePropertyNumericEntry(TeethProperty, TeethProperties)
					]
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSeparator)
				.Thickness(1.f)
			]


			// Variation spin box section
			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				CreatePropertySpinBoxWidget(TEXT("Variation"), VariationProperty, TeethProperties)
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

void SMetaHumanCharacterEditorHeadModelToolView::OnTeethSliderPropertyEdited(FProperty* Property)
{
	const UEnum* EnumPtr = StaticEnum<EMetaHumanCharacterTeethPropertyType>();
	if (TeethEditablePropertyComboBox.IsValid() && EnumPtr)
	{
		const int64 EnumValue = EnumPtr->GetValueByName(*Property->GetName());
		const EMetaHumanCharacterTeethPropertyType TeethPropertyType = static_cast<EMetaHumanCharacterTeethPropertyType>(EnumValue);
		TeethEditablePropertyComboBox->SetSelectedItem(TeethPropertyType);

		OnPreEditChangeProperty(Property, *Property->GetName());
	}
}

void SMetaHumanCharacterEditorHeadModelToolView::OnTeethEditablePropertyValueChanged(uint8 Value, FProperty* Property, void* PropertyContainerPtr)
{
	UMetaHumanCharacterHeadModelTeethProperties* HeadModelProperties = Cast<UMetaHumanCharacterHeadModelTeethProperties>(GetTeethProperties());
	FMetaHumanCharacterTeethProperties* TeethProperties = IsValid(HeadModelProperties) ? &HeadModelProperties->Teeth : nullptr;
	if (!TeethProperties)
	{
		return;
	}

	if (TeethEditablePropertyBox.IsValid())
	{
		const FString TeethPropertyName = StaticEnum<EMetaHumanCharacterTeethPropertyType>()->GetAuthoredNameStringByValue(Value);
		FProperty* TeethProperty = FMetaHumanCharacterTeethProperties::StaticStruct()->FindPropertyByName(*TeethPropertyName);

		TeethEditablePropertyBox->ClearChildren();
		TeethEditablePropertyBox->AddSlot()
			.AutoHeight()
			[
				CreatePropertyNumericEntry(TeethProperty, TeethProperties)
			];
	}

	OnEnumPropertyValueChanged(Value, Property, PropertyContainerPtr);
}

void SMetaHumanCharacterEditorHeadModelToolView::OnTeethSliderValueChanged(const float Value, bool bIsInteractive, FProperty* Property, void* PropertyContainerPtr)
{
	OnFloatPropertyValueChanged(Value, bIsInteractive, Property, PropertyContainerPtr);
}

const FSlateBrush* SMetaHumanCharacterEditorHeadModelToolView::GetEyelashesSectionBrush(uint8 InItem)
{
	const FString EyelashesMaskName = StaticEnum<EMetaHumanCharacterEyelashesType>()->GetAuthoredNameStringByValue(InItem);
	const FString EyelashesMaskBrushName = FString::Format(TEXT("Eyelashes.{0}"), { EyelashesMaskName });
	return FMetaHumanCharacterEditorStyle::Get().GetBrush(*EyelashesMaskBrushName);
}

const FSlateBrush* SMetaHumanCharacterEditorHeadModelToolView::GetTeethSectionBrush(uint8 InItem)
{
	// TODO this doesn't exist yet
	const FString TeethMaskName = StaticEnum<EMetaHumanCharacterTeethType>()->GetAuthoredNameStringByValue(InItem);
	const FString TeethMaskBrushName = FString::Format(TEXT("Teeth.{0}"), { TeethMaskName });
	return FMetaHumanCharacterEditorStyle::Get().GetBrush(*TeethMaskBrushName);
}

EVisibility SMetaHumanCharacterEditorHeadModelToolView::GetEyelashesSubToolViewVisibility() const
{
	const bool bIsVisible = IsValid(Cast<UMetaHumanCharacterHeadModelEyelashesProperties>(GetToolProperties()));
	return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SMetaHumanCharacterEditorHeadModelToolView::GetTeethSubToolViewVisibility() const
{
	const bool bIsVisible = IsValid(Cast<UMetaHumanCharacterHeadModelTeethProperties>(GetToolProperties()));
	return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
}

#undef LOCTEXT_NAMESPACE
