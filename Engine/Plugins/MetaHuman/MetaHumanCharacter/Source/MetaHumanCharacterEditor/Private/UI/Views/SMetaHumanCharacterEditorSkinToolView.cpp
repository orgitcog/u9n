// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetaHumanCharacterEditorSkinToolView.h"

#include "IDetailsView.h"
#include "InteractiveTool.h"
#include "IStructureDetailsView.h"
#include "MetaHumanCharacterEditorStyle.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Tools/MetaHumanCharacterEditorSkinTool.h"
#include "UI/Widgets/SMetaHumanCharacterEditorAccentRegionsPanel.h"
#include "UI/Widgets/SMetaHumanCharacterEditorTileView.h"
#include "UI/Widgets/SMetaHumanCharacterEditorToolPanel.h"
#include "UI/Widgets/SMetaHumanCharacterEditorTextComboBox.h"
#include "UObject/StructOnScope.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Text/STextBlock.h"
#include "SWarningOrErrorBox.h"
#include "MetaHumanCharacterEditorSubsystem.h"

#define LOCTEXT_NAMESPACE "SMetaHumanCharacterEditorSkinToolView"

void SMetaHumanCharacterEditorSkinToolView::Construct(const FArguments& InArgs, UMetaHumanCharacterEditorSkinTool* InTool)
{
	SMetaHumanCharacterEditorToolView::Construct(SMetaHumanCharacterEditorToolView::FArguments(), InTool);
}

UInteractiveToolPropertySet* SMetaHumanCharacterEditorSkinToolView::GetToolProperties() const
{
	const UMetaHumanCharacterEditorSkinTool* SkinTool = Cast<UMetaHumanCharacterEditorSkinTool>(Tool);
	return IsValid(SkinTool) ? SkinTool->GetSkinToolProperties() : nullptr;
}

void SMetaHumanCharacterEditorSkinToolView::MakeToolView()
{
	if (ToolViewScrollBox.IsValid())
	{
		ToolViewScrollBox->AddSlot()
			.VAlign(VAlign_Top)
			[
				SNew(SVerticalBox)

				// Build warning label
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox)
						.Padding(4.f)
						[
							SNew(SWarningOrErrorBox)
								.AutoWrapText(true)
								.MessageStyle(EMessageStyle::Warning)
								.Visibility(this, &SMetaHumanCharacterEditorSkinToolView::GetSkinEditWarningVisibility)
								.Message(LOCTEXT("SkinEditDisabledWarningMessage", "Skin editing is disabled. Enable the MetaHuman Content option in the UE installer to enable skin editing."))
						]
				]

				+ SVerticalBox::Slot()
				.Padding(4.f)
				.AutoHeight()
				[
					CreateSkinToolViewSkinSection()
				]

				+ SVerticalBox::Slot()
				.Padding(4.f)
				.AutoHeight()
				[
					CreateSkinToolViewFrecklesSection()
				]

				+ SVerticalBox::Slot()
				.Padding(4.f)
				.AutoHeight()
				[
					CreateSkinToolViewAccentsSection()
				]

				+ SVerticalBox::Slot()
				.Padding(4.f)
				.AutoHeight()
				[
					CreateSkinToolViewTextureOverridesSection()
				]
			];
	}
}

void SMetaHumanCharacterEditorSkinToolView::NotifyPreChange(FProperty* PropertyAboutToChange)
{
	OnPreEditChangeProperty(PropertyAboutToChange, PropertyAboutToChange->GetName());
}

void SMetaHumanCharacterEditorSkinToolView::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged)
{
	const bool bIsInteractive = PropertyChangedEvent.ChangeType & EPropertyChangeType::Interactive;
	OnPostEditChangeProperty(PropertyThatChanged, bIsInteractive);
}

FUNC_DECLARE_DELEGATE(FOnMetaHumanCharacterGetSelectedString, FText, int32)

TSharedRef<SWidget> SMetaHumanCharacterEditorSkinToolView::CreateSkinToolViewSkinSection()
{
	UMetaHumanCharacterEditorSkinToolProperties* SkinToolProperties = Cast<UMetaHumanCharacterEditorSkinToolProperties>(GetToolProperties());
	void* SkinProperties = IsValid(SkinToolProperties) ? &SkinToolProperties->Skin : nullptr;
	if (!SkinProperties)
	{
		return SNullWidget::NullWidget;
	}

	void* FaceEvaluationProperties = IsValid(SkinToolProperties) ? &SkinToolProperties->FaceEvaluationSettings : nullptr;
	if (!FaceEvaluationProperties)
	{
		return SNullWidget::NullWidget;
	}

	FProperty* UProperty = FMetaHumanCharacterSkinProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterSkinProperties, U));
	FProperty* VProperty = FMetaHumanCharacterSkinProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterSkinProperties, V));
	FProperty* ShowTopUnderwearProperty = FMetaHumanCharacterSkinProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterSkinProperties, bShowTopUnderwear));
	FProperty* BodyTextureIndexProperty = FMetaHumanCharacterSkinProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterSkinProperties, BodyTextureIndex));
	FProperty* FaceTextureIndexProperty = FMetaHumanCharacterSkinProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterSkinProperties, FaceTextureIndex));
	FProperty* RoughnessProperty = FMetaHumanCharacterSkinProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterSkinProperties, Roughness));
	FProperty* HighFrequencyDeltaProperty = FMetaHumanCharacterFaceEvaluationSettings::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterFaceEvaluationSettings, HighFrequencyDelta));
	FProperty* SkinFilterEnabledProperty = UMetaHumanCharacterEditorSkinToolProperties::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorSkinToolProperties, bIsSkinFilterEnabled));
	FProperty* SkinFilterValuesProperty = UMetaHumanCharacterEditorSkinToolProperties::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorSkinToolProperties, SkinFilterValues));
	FProperty* SkinFilterIndexProperty = UMetaHumanCharacterEditorSkinToolProperties::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorSkinToolProperties, SkinFilterIndex));

	TNotNull<UMetaHumanCharacterEditorSubsystem*> MetaHumanCharacterSubsystem = UMetaHumanCharacterEditorSubsystem::Get();
	int32 NumTextureAttributes = MetaHumanCharacterSubsystem->GetFaceTextureAttributeMap().NumAttributes();
	if (AttributeValueNames.Num() != NumTextureAttributes)
	{
		AttributeValueNames.Reset();
		for (int32 Idx = 0; Idx < NumTextureAttributes; ++Idx)
		{
			AttributeValueNames.Push(TArray<TSharedPtr<FString>>());
			AttributeValueNames.Last().Push(MakeShared<FString>("---"));
			for (int32 NameIdx = 0; NameIdx < MetaHumanCharacterSubsystem->GetFaceTextureAttributeMap().GetAttributeValueNames(Idx).Num(); ++NameIdx)
			{
				AttributeValueNames.Last().Push(MakeShared<FString>(MetaHumanCharacterSubsystem->GetFaceTextureAttributeMap().GetAttributeValueNames(Idx)[NameIdx]));
			}
		}
	}

	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox)
		.IsEnabled(this, &SMetaHumanCharacterEditorSkinToolView::IsSkinEditEnabled);

	VerticalBox->AddSlot()// Skin Tone Picker section
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Fill)
		.AutoHeight()
		[
			CreatePropertyUVColorPickerWidget(UProperty,
											  VProperty,
											  SkinProperties,
											  LOCTEXT("SkinTonePicker", "Skin Tone"),
											  MetaHumanCharacterSubsystem->GetOrCreateSkinToneTexture().Get())
		];

	// Body texture index section
	VerticalBox->AddSlot()
		.AutoHeight()
		[
			CreatePropertySpinBoxWidget(TEXT("Body Texture Index"), BodyTextureIndexProperty, SkinProperties, 4)
		];

	// Texture spinbox section
	VerticalBox->AddSlot()
		.AutoHeight()
		[
			CreatePropertyCheckBoxWidget(TEXT("Use Texture Index Filters"), SkinFilterEnabledProperty, SkinToolProperties)
		];

	for (int32 Idx = 0; Idx < NumTextureAttributes; ++Idx)
	{
		VerticalBox->AddSlot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
					.Visibility_Lambda([this]() -> EVisibility {
							if (UMetaHumanCharacterEditorSkinToolProperties* SkinToolProperties = Cast<UMetaHumanCharacterEditorSkinToolProperties>(GetToolProperties()))
							{
								return SkinToolProperties->bIsSkinFilterEnabled ? EVisibility::Visible : EVisibility::Collapsed;
							}
							return EVisibility::Collapsed;
						})
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					.FillWidth(.3f)
					.Padding(14.f, 0.f)
					[
						SNew(STextBlock)
							.Text(FText::FromString(MetaHumanCharacterSubsystem->GetFaceTextureAttributeMap().GetAttributeName(Idx)))
							.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					]

					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Fill)
					.FillWidth(.7f)
					.Padding(4.f, 2.f, 40.f, 2.f)
					[
						SNew(SMetaHumanCharacterEditorTextComboBox, AttributeValueNames[Idx], AttributeValueNames[Idx][0])
							.OnSelectionChanged_Lambda([this, Idx](int32 ItemIdx) {
							if (UMetaHumanCharacterEditorSkinToolProperties* SkinToolProperties = Cast<UMetaHumanCharacterEditorSkinToolProperties>(GetToolProperties()))
							{
								SkinToolProperties->SkinFilterValues[Idx] = ItemIdx - 1;
								FProperty* SkinFilterProperty = UMetaHumanCharacterEditorSkinToolProperties::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorSkinToolProperties, SkinFilterValues));
								OnPostEditChangeProperty(SkinFilterProperty, /*bIsInteractive*/false);
							}
								})
					]
			];
		VerticalBox->AddSlot()
			.AutoHeight()
			[
				SNew(SSeparator)
					.Thickness(1.f)
					.Visibility_Lambda([this]() -> EVisibility {
							if (UMetaHumanCharacterEditorSkinToolProperties* SkinToolProperties = Cast<UMetaHumanCharacterEditorSkinToolProperties>(GetToolProperties()))
							{
								return SkinToolProperties->bIsSkinFilterEnabled ? EVisibility::Visible : EVisibility::Collapsed;
							}
							return EVisibility::Collapsed;
						})
			];;

	}

	VerticalBox->AddSlot()
		.AutoHeight()
		[
			SNew(SVerticalBox)
				.Visibility_Lambda([this]() -> EVisibility {
					if (UMetaHumanCharacterEditorSkinToolProperties* SkinToolProperties = Cast<UMetaHumanCharacterEditorSkinToolProperties>(GetToolProperties()))
					{
						return SkinToolProperties->bIsSkinFilterEnabled ? EVisibility::Visible : EVisibility::Collapsed;
					}
					return EVisibility::Hidden;
				})
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)

						// SpinBox Label section
						+ SHorizontalBox::Slot()
						.HAlign(HAlign_Left)
						.VAlign(VAlign_Center)
						.FillWidth(.3f)
						.Padding(10.f, 0.f)
						[
							SNew(STextBlock)
								.Text(LOCTEXT("FaceFilterIndexLabel", "Face Filter Index"))
								.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						]

						// SpinBox slider section
						+ SHorizontalBox::Slot()
						.HAlign(HAlign_Fill)
						.FillWidth(.7f)
						.Padding(4.f, 2.f, 40.f, 2.f)
						[
							CreatePropertyNumericEntry(SkinFilterIndexProperty, SkinToolProperties, TEXT("Face Filter Index"), 4)
						]
				]
		];

	VerticalBox->AddSlot()
		.AutoHeight()
		[
			CreatePropertySpinBoxWidget(TEXT("Face Texture Index"), FaceTextureIndexProperty, SkinProperties, 4)
		];

	// Roughness spinbox section
	VerticalBox->AddSlot()
		.AutoHeight()
		[
			CreatePropertySpinBoxWidgetNormalized(RoughnessProperty, SkinProperties, 0.85f, 1.15f)
		];

	// Show underwear
	VerticalBox->AddSlot()
		.AutoHeight()
		[
			CreatePropertyCheckBoxWidget(TEXT("Show Top Underwear"), ShowTopUnderwearProperty, SkinProperties)
		];

	// Geometry HF Delta spinbox section
	VerticalBox->AddSlot()
		.AutoHeight()
		[
			CreatePropertySpinBoxWidget(TEXT("Texture Position Offset"), HighFrequencyDeltaProperty, FaceEvaluationProperties)
		];
	

	TSharedRef<SWidget> SkinSectionWidget =
		SNew(SMetaHumanCharacterEditorToolPanel)
		.Label(LOCTEXT("SkinSectionLabel", "Skin"))
		.Content()
		[
			VerticalBox
		];
		
	return SkinSectionWidget;
}


TSharedRef<SWidget> SMetaHumanCharacterEditorSkinToolView::CreateSkinToolViewFrecklesSection()
{
	UMetaHumanCharacterEditorSkinToolProperties* SkinToolProperties = Cast<UMetaHumanCharacterEditorSkinToolProperties>(GetToolProperties());
	void* FrecklesProperties = IsValid(SkinToolProperties) ? &SkinToolProperties->Freckles : nullptr;
	if (!FrecklesProperties)
	{
		return SNullWidget::NullWidget;
	}

	FProperty* DensityProperty = FMetaHumanCharacterFrecklesProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterFrecklesProperties, Density));
	FProperty* StrengthProperty = FMetaHumanCharacterFrecklesProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterFrecklesProperties, Strength));
	FProperty* SaturationProperty = FMetaHumanCharacterFrecklesProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterFrecklesProperties, Saturation));
	FProperty* ToneShiftProperty = FMetaHumanCharacterFrecklesProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterFrecklesProperties, ToneShift));
	FProperty* MaskProperty = FMetaHumanCharacterFrecklesProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterFrecklesProperties, Mask));

	const TSharedRef<SWidget> FrecklesSectionWidget =
		SNew(SMetaHumanCharacterEditorToolPanel)
		.Label(LOCTEXT("FrecklesSectionLabel", "Freckles"))
		.Content()
		[
			SNew(SVerticalBox)
			.IsEnabled(this, &SMetaHumanCharacterEditorSkinToolView::IsEditEnabled)

			// Density spinbox section
			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				CreatePropertySpinBoxWidget(TEXT("Density"), DensityProperty, FrecklesProperties)
			]

			// Strength spinbox section
			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				CreatePropertySpinBoxWidget(TEXT("Strength"), StrengthProperty, FrecklesProperties)
			]

			// Saturation spinbox section
			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				CreatePropertySpinBoxWidget(TEXT("Saturation"), SaturationProperty, FrecklesProperties)
			]

			// ToneShift spinbox section
			+SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				CreatePropertySpinBoxWidget(TEXT("Tone Shift"), ToneShiftProperty, FrecklesProperties)
			]

			+ SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.Padding(4.f)
			.AutoHeight()
			[
				SNew(SMetaHumanCharacterEditorTileView<EMetaHumanCharacterFrecklesMask>)
				.OnGetSlateBrush(this, &SMetaHumanCharacterEditorSkinToolView::GetFreckesSectionBrush)
				.OnSelectionChanged(this, &SMetaHumanCharacterEditorSkinToolView::OnEnumPropertyValueChanged, MaskProperty, FrecklesProperties)
				.InitiallySelectedItem(SkinToolProperties->Freckles.Mask)
			]
		];

	return FrecklesSectionWidget;
}

TSharedRef<SWidget> SMetaHumanCharacterEditorSkinToolView::CreateSkinToolViewAccentsSection()
{
	FProperty* RednessProperty = FMetaHumanCharacterAccentRegionProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterAccentRegionProperties, Redness));
	FProperty* SaturationProperty = FMetaHumanCharacterAccentRegionProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterAccentRegionProperties, Saturation));
	FProperty* LightnessProperty = FMetaHumanCharacterAccentRegionProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterAccentRegionProperties, Lightness));

	const TSharedRef<SWidget> AccentsSectionWidget =
		SNew(SMetaHumanCharacterEditorToolPanel)
		.Label(LOCTEXT("AccentsSectionLabel", "Accents"))
		.Content()
		[
			SNew(SVerticalBox)
			.IsEnabled(this, &SMetaHumanCharacterEditorSkinToolView::IsEditEnabled)

			//Accent Regions panel section
			+ SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.Padding(8.f)
			.AutoHeight()
			[
				SAssignNew(AccentRegionsPanel, SMetaHumanCharacterEditorAccentRegionsPanel)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSeparator)
				.Thickness(1.f)
			]

			// Redness spinbox section
			+SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				CreateAccentRegionPropertySpinBoxWidget(TEXT("Redness"), RednessProperty)
			]

			// Saturation spinbox section
			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				CreateAccentRegionPropertySpinBoxWidget(TEXT("Saturation"), SaturationProperty)
			]

			// Lightness spinbox section
			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				CreateAccentRegionPropertySpinBoxWidget(TEXT("Lightness"), LightnessProperty)
			]
		];

	return AccentsSectionWidget;
}

TSharedRef<SWidget> SMetaHumanCharacterEditorSkinToolView::CreateSkinToolViewTextureOverridesSection()
{
	UMetaHumanCharacterEditorSkinToolProperties* SkinToolProperties = Cast<UMetaHumanCharacterEditorSkinToolProperties>(GetToolProperties());
	if (!SkinToolProperties)
	{
		return SNullWidget::NullWidget;
	}

	FProperty* EnableTexturesProperty = UMetaHumanCharacterEditorSkinToolProperties::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorSkinToolProperties, bEnableTextureOverrides));

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bUpdatesFromSelection = false;
	DetailsViewArgs.bLockable = false;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsViewArgs.NotifyHook = this;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	FStructureDetailsViewArgs StructureViewArgs;
	StructureViewArgs.bShowObjects = true;
	StructureViewArgs.bShowAssets = true;
	StructureViewArgs.bShowClasses = true;
	StructureViewArgs.bShowInterfaces = true;

	TSharedRef<FStructOnScope> StructOnScope = MakeShared<FStructOnScope>(FMetaHumanCharacterSkinTextureSoftSet::StaticStruct(), (uint8*)&SkinToolProperties->TextureOverrides);
	TSharedRef<IStructureDetailsView> StructDetailsView = PropertyEditorModule.CreateStructureDetailView(DetailsViewArgs, StructureViewArgs, StructOnScope);

	const TSharedRef<SWidget> TextureOverridesSectionWidget =
		SNew(SMetaHumanCharacterEditorToolPanel)
		.Label(LOCTEXT("TextureOverrideSectionLabel", "Texture Override"))
		.Content()
		[
			SNew(SVerticalBox)

			// Density spinbox section
			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				CreatePropertyCheckBoxWidget(TEXT("Enable Texture Overrides"), EnableTexturesProperty, SkinToolProperties)
			]

			// Strength spinbox section
			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.AutoHeight()
			[
				SNew(SBox)
				.IsEnabled(this, &SMetaHumanCharacterEditorSkinToolView::GetBoolPropertyValue, EnableTexturesProperty, static_cast<void*>(SkinToolProperties))
				[
					StructDetailsView->GetWidget().ToSharedRef()
				]
			]
		];

	return TextureOverridesSectionWidget;
}

TSharedRef<SWidget> SMetaHumanCharacterEditorSkinToolView::CreateAccentRegionPropertySpinBoxWidget(const FString Label, FProperty* Property, const int32 FractionalDigits)
{
	if (!Property)
	{
		return SNullWidget::NullWidget;
	}

	return
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			// Slider Label section
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.FillWidth(.3f)
			.Padding(10.f, 0.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Label))
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]

			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Fill)
			.FillWidth(.7f)
			.Padding(4.f, 2.f, 40.f, 2.f)
			[
				SNew(SNumericEntryBox<float>)
				.AllowSpin(true)
				.EditableTextBoxStyle(&FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox"))
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.MinValue(this, &SMetaHumanCharacterEditorSkinToolView::GetFloatPropertyMinValue, Property)
				.MaxValue(this, &SMetaHumanCharacterEditorSkinToolView::GetFloatPropertyMaxValue, Property)
				.MinSliderValue(this, &SMetaHumanCharacterEditorSkinToolView::GetFloatPropertyMinValue, Property)
				.MaxSliderValue(this, &SMetaHumanCharacterEditorSkinToolView::GetFloatPropertyMaxValue, Property)
				.SpinBoxStyle(&FCoreStyle::Get().GetWidgetStyle<FSpinBoxStyle>("SpinBox"))
				.Value(this, &SMetaHumanCharacterEditorSkinToolView::GetAccentRegionFloatPropertyValue, Property)
				.OnBeginSliderMovement(this, &SMetaHumanCharacterEditorSkinToolView::OnPreEditChangeProperty, Property, Label)
				.OnEndSliderMovement(this, &SMetaHumanCharacterEditorSkinToolView::OnAccentRegionFloatPropertyValueChanged, /* bIsInteractive */ false, Property)
				.OnValueChanged(this, &SMetaHumanCharacterEditorSkinToolView::OnAccentRegionFloatPropertyValueChanged, /* bIsDragging */ true, Property)
				.PreventThrottling(true)
				.MaxFractionalDigits(FractionalDigits)
				.LinearDeltaSensitivity(1.0)
				.Delta(.001f)
				.LabelPadding(FMargin(3))
				.LabelLocation(SNumericEntryBox<float>::ELabelLocation::Inside)
				.Label()
				[
					SNumericEntryBox<float>::BuildNarrowColorLabel(FLinearColor::Transparent)
				]
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SSeparator)
			.Thickness(1.f)
		];
}

void* SMetaHumanCharacterEditorSkinToolView::GetAccentRegionPropertyContainerFromSelection() const
{
	UMetaHumanCharacterEditorSkinToolProperties* SkinToolProperties = Cast<UMetaHumanCharacterEditorSkinToolProperties>(GetToolProperties());
	if (!SkinToolProperties || !AccentRegionsPanel.IsValid())
	{
		return nullptr;
	}

	const EMetaHumanCharacterAccentRegion SelectedRegion = AccentRegionsPanel->GetSelectedRegion();
	switch (SelectedRegion)
	{
	case EMetaHumanCharacterAccentRegion::Scalp:
		return &SkinToolProperties->Accents.Scalp;
		break;
	case EMetaHumanCharacterAccentRegion::Forehead:
		return &SkinToolProperties->Accents.Forehead;
		break;
	case EMetaHumanCharacterAccentRegion::Nose:
		return &SkinToolProperties->Accents.Nose;
		break;
	case EMetaHumanCharacterAccentRegion::UnderEye:
		return &SkinToolProperties->Accents.UnderEye;
		break;
	case EMetaHumanCharacterAccentRegion::Ears:
		return &SkinToolProperties->Accents.Ears;
		break;
	case EMetaHumanCharacterAccentRegion::Cheeks:
		return &SkinToolProperties->Accents.Cheeks;
		break;
	case EMetaHumanCharacterAccentRegion::Lips:
		return &SkinToolProperties->Accents.Lips;
		break;
	case EMetaHumanCharacterAccentRegion::Chin:
		return &SkinToolProperties->Accents.Chin;
		break;
	default:
		return nullptr;
		break;
	}
}

TOptional<float> SMetaHumanCharacterEditorSkinToolView::GetAccentRegionFloatPropertyValue(FProperty* Property) const
{
	TOptional<float> PropertyValue;
	void* PropertyContainerPtr = GetAccentRegionPropertyContainerFromSelection();
	if (PropertyContainerPtr)
	{
		PropertyValue = GetFloatPropertyValue(Property, PropertyContainerPtr);
	}

	return PropertyValue;
}

void SMetaHumanCharacterEditorSkinToolView::OnAccentRegionFloatPropertyValueChanged(const float Value, bool bIsDragging, FProperty* Property)
{
	void* PropertyContainerPtr = GetAccentRegionPropertyContainerFromSelection();
	if (PropertyContainerPtr)
	{
		OnFloatPropertyValueChanged(Value, bIsDragging, Property, PropertyContainerPtr);
	}
}

void SMetaHumanCharacterEditorSkinToolView::OnSkinUVChanged(const FVector2f& UV, bool bIsDragging)
{
	UMetaHumanCharacterEditorSkinToolProperties* ToolProperties = Cast<UMetaHumanCharacterEditorSkinToolProperties>(GetToolProperties());
	void* SkinProperties = IsValid(ToolProperties) ? &ToolProperties->Skin : nullptr;
	if (ToolProperties)
	{
		FProperty* UProperty = FMetaHumanCharacterSkinProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterSkinProperties, U));
		FProperty* VProperty = FMetaHumanCharacterSkinProperties::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterSkinProperties, V));

		OnFloatPropertyValueChanged(UV.X, bIsDragging, UProperty, SkinProperties);
		OnFloatPropertyValueChanged(UV.Y, bIsDragging, VProperty, SkinProperties);
	}
}

const FSlateBrush* SMetaHumanCharacterEditorSkinToolView::GetFreckesSectionBrush(uint8 InItem)
{
	const FString FrecklesMaskName = StaticEnum<EMetaHumanCharacterFrecklesMask>()->GetAuthoredNameStringByValue(InItem);
	const FString FrecklesMaskBrushName = FString::Format(TEXT("Skin.Freckles.{0}"), { FrecklesMaskName });
	return FMetaHumanCharacterEditorStyle::Get().GetBrush(*FrecklesMaskBrushName);
}

bool SMetaHumanCharacterEditorSkinToolView::IsEditEnabled() const
{
	const UMetaHumanCharacterEditorSkinToolProperties* SkinToolProperties = Cast<UMetaHumanCharacterEditorSkinToolProperties>(GetToolProperties());
	return SkinToolProperties != nullptr;
}

bool SMetaHumanCharacterEditorSkinToolView::IsSkinEditEnabled() const
{
	TNotNull<UMetaHumanCharacterEditorSubsystem*> MetaHumanCharacterSubsystem = UMetaHumanCharacterEditorSubsystem::Get();
	return MetaHumanCharacterSubsystem->IsTextureSynthesisEnabled();
}

EVisibility SMetaHumanCharacterEditorSkinToolView::GetSkinEditWarningVisibility() const
{
	return IsSkinEditEnabled() ? EVisibility::Collapsed : EVisibility::HitTestInvisible;
}

EVisibility SMetaHumanCharacterEditorSkinToolView::GetPropertyVisibility(FProperty* Property, void* PropertyContainerPtr) const
{
	return SMetaHumanCharacterEditorToolView::GetPropertyVisibility(Property, PropertyContainerPtr);
}

bool SMetaHumanCharacterEditorSkinToolView::IsPropertyEnabled(FProperty* Property, void* PropertyContainerPtr) const
{
	if (Property->GetName() == GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorSkinToolProperties, SkinFilterIndex))
	{
		UMetaHumanCharacterEditorSkinToolProperties* ToolProperties = Cast<UMetaHumanCharacterEditorSkinToolProperties>(GetToolProperties());
		if (IsValid(ToolProperties) && ToolProperties->bIsSkinFilterEnabled)
		{
			const UMetaHumanCharacterEditorSkinTool* SkinTool = Cast<UMetaHumanCharacterEditorSkinTool>(Tool);
			return SkinTool->IsFilteredFaceTextureIndicesValid();
		}
		return false;
	}
	else if (Property->GetName() == GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterSkinProperties, FaceTextureIndex))
	{
		UMetaHumanCharacterEditorSkinToolProperties* ToolProperties = Cast<UMetaHumanCharacterEditorSkinToolProperties>(GetToolProperties());
		return IsValid(ToolProperties) ? !ToolProperties->bIsSkinFilterEnabled : true;
	}
	return SMetaHumanCharacterEditorToolView::IsPropertyEnabled(Property, PropertyContainerPtr);
}


#undef LOCTEXT_NAMESPACE
