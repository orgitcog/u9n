// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetaHumanCharacterEditorEyesToolView.h"

#include "MetaHumanCharacterEditorStyle.h"
#include "Subsystem/MetaHumanCharacterSkinMaterials.h"
#include "Tools/MetaHumanCharacterEditorEyesTool.h"
#include "UI/Widgets/SMetaHumanCharacterEditorTileView.h"
#include "UI/Widgets/SMetaHumanCharacterEditorToolPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "Engine/Texture2D.h"

#define LOCTEXT_NAMESPACE "SMetaHumanCharacterEditorEyesToolView"

struct FEyePresetItem
{
	FMetaHumanCharacterEyePreset Preset;

	FSlateBrush Brush;

	FEyePresetItem() = default;

	FEyePresetItem(const FMetaHumanCharacterEyePreset& InPreset)
		: Preset(InPreset)
	{
		if (!Preset.Thumbnail.IsNull())
		{
			Brush.SetResourceObject(Preset.Thumbnail.LoadSynchronous());
		}
	}
};

void SMetaHumanCharacterEditorEyesToolView::Construct(const FArguments& InArgs, UMetaHumanCharacterEditorEyesTool* InTool)
{
	SMetaHumanCharacterEditorToolView::Construct(SMetaHumanCharacterEditorToolView::FArguments(), InTool);
}

UInteractiveToolPropertySet* SMetaHumanCharacterEditorEyesToolView::GetToolProperties() const
{
	const UMetaHumanCharacterEditorEyesTool* EyesTool = Cast<UMetaHumanCharacterEditorEyesTool>(Tool);
	return IsValid(EyesTool) ? EyesTool->GetEyesToolProperties() : nullptr;
}

void SMetaHumanCharacterEditorEyesToolView::MakeToolView()
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
					CreateEyesToolViewPresetsSection()
				]

				+ SVerticalBox::Slot()
				.Padding(4.f)
				.AutoHeight()
				[
					CreateEyeSelectionSection()
				]

				+ SVerticalBox::Slot()
				.Padding(4.f)
				.AutoHeight()
				[
					CreateEyesToolViewIrisSection()
				]

				+ SVerticalBox::Slot()
				.Padding(4.f)
				.AutoHeight()
				[
					CreateEyeToolViewPupilSection()
				]

				+ SVerticalBox::Slot()
				.Padding(4.f)
				.AutoHeight()
				[
					CreateEyeCorneaViewSection()
				]

				+ SVerticalBox::Slot()
				.Padding(4.f)
				.AutoHeight()
				[
					CreateEyesToolViewScleraSection()
				]
			];
	}
}

TSharedRef<SWidget> SMetaHumanCharacterEditorEyesToolView::CreateEyesToolViewPresetsSection()
{
	TNotNull<UMetaHumanCharacterEyePresets*> EyePresets = UMetaHumanCharacterEyePresets::Get();

	UMetaHumanCharacterEditorEyesToolProperties* EyesToolProperties = Cast<UMetaHumanCharacterEditorEyesToolProperties>(GetToolProperties());

	Algo::Transform(EyePresets->Presets, PresetItems, [](const FMetaHumanCharacterEyePreset& Preset)
					{
						return MakeShared<FEyePresetItem>(Preset);
					});

	return SNew(SMetaHumanCharacterEditorToolPanel)
		.Label(LOCTEXT("PresetsSectionLabel", "Presets"))
		.Content()
		[
			SNew(SVerticalBox)

			// Presets tile view section
			+ SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.Padding(4.f)
			.AutoHeight()
			[
				SNew(STileView<FEyePresetItemPtr>)
				.ListItemsSource(&PresetItems)
				.SelectionMode(ESelectionMode::None)
				.ItemAlignment(EListItemAlignment::EvenlyDistributed)
				.OnGenerateTile_Lambda([](FEyePresetItemPtr Item, const TSharedRef<STableViewBase>& OwnerTable)
				{
					return SNew(STableRow<FEyePresetItemPtr>, OwnerTable)
						.Padding(4.0f)
						.ToolTipText_Lambda([Item]
						{
							return FText::FromName(Item->Preset.PresetName);
						})
						.Style(FMetaHumanCharacterEditorStyle::Get(), "MetaHumanCharacterEditorTools.TableViewRow")
						[
							SNew(SImage)
								.Image(&Item->Brush)
						];
				})
				.OnMouseButtonDoubleClick_Lambda([this](FEyePresetItemPtr Item)
				{
					if (Item.IsValid())
					{
						UMetaHumanCharacterEditorEyesTool* EyeTool = CastChecked<UMetaHumanCharacterEditorEyesTool>(Tool);
						EyeTool->SetEyesFromPreset(Item->Preset.EyesSettings);
					}
				})
			]
		];
}

TSharedRef<SWidget> SMetaHumanCharacterEditorEyesToolView::CreateEyeSelectionSection()
{	
	UMetaHumanCharacterEditorEyesToolProperties* EyeToolProperties = CastChecked<UMetaHumanCharacterEditorEyesToolProperties>(GetToolProperties());

	TSharedRef<SSegmentedControl<EMetaHumanCharacterEyeEditSelection>> EyeSelectionWidget =
		SNew(SSegmentedControl<EMetaHumanCharacterEyeEditSelection>)
		.Value_Lambda([EyeToolProperties]
					  {
						  return EyeToolProperties->EyeSelection;
					  })
		.OnValueChanged_Lambda([this, EyeToolProperties](EMetaHumanCharacterEyeEditSelection Selection)
							   {
									CastChecked<UMetaHumanCharacterEditorEyesTool>(GetTool())->SetEyeSelection(Selection);
							   });

	for (EMetaHumanCharacterEyeEditSelection EyeSelection : TEnumRange<EMetaHumanCharacterEyeEditSelection>())
	{
		EyeSelectionWidget->AddSlot(EyeSelection)
			.Text(UEnum::GetDisplayValueAsText(EyeSelection));
	}

	return SNew(SMetaHumanCharacterEditorToolPanel)
		.Label(LOCTEXT("EyeSelectionLabel", "Eyes"))
		.Content()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.Padding(2.0f, 2.0f)
			[
				EyeSelectionWidget
			]
		];
}

TSharedRef<SWidget> SMetaHumanCharacterEditorEyesToolView::CreateEyesToolViewIrisSection()
{
	UMetaHumanCharacterEditorEyesToolProperties* EyesToolProperties = Cast<UMetaHumanCharacterEditorEyesToolProperties>(GetToolProperties());
	FMetaHumanCharacterEyeIrisProperties* IrisProperties = IsValid(EyesToolProperties) ? &EyesToolProperties->Eye.Iris : nullptr;
	if (!IrisProperties)
	{
		return SNullWidget::NullWidget;
	}

	UStruct* Struct = FMetaHumanCharacterEyeIrisProperties::StaticStruct();

	FProperty* IrisRotationProperty = Struct->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterEyeIrisProperties, IrisRotation));
	FProperty* PrimaryColorUProperty = Struct->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterEyeIrisProperties, PrimaryColorU));
	FProperty* PrimaryColorVProperty = Struct->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterEyeIrisProperties, PrimaryColorV));
	FProperty* SecondaryColorUProperty = Struct->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterEyeIrisProperties, SecondaryColorU));
	FProperty* SecondaryColorVProperty = Struct->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterEyeIrisProperties, SecondaryColorV));
	FProperty* ColorBlendProperty = Struct->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterEyeIrisProperties, ColorBlend));
	FProperty* ColorBlendSoftnessProperty = Struct->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterEyeIrisProperties, ColorBlendSoftness));
	FProperty* BlendMethodProperty = Struct->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterEyeIrisProperties, BlendMethod));
	FProperty* ShadowDetailsProperty = Struct->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterEyeIrisProperties, ShadowDetails));
	FProperty* LimbalRingSizeProperty = Struct->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterEyeIrisProperties, LimbalRingSize));
	FProperty* LimbalRingSoftnessProperty = Struct->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterEyeIrisProperties, LimbalRingSoftness));
	FProperty* LimbalRingColorProperty = Struct->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterEyeIrisProperties, LimbalRingColor));
	FProperty* GlobalSaturationProperty = Struct->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterEyeIrisProperties, GlobalSaturation));
	FProperty* GlobalTintProperty = Struct->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterEyeIrisProperties, GlobalTint));
	FProperty* IrisPatternProperty = Struct->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterEyeIrisProperties, IrisPattern));

	UTexture2D* IrisColorPicker = LoadObject<UTexture2D>(nullptr, TEXT("/Script/Engine.Texture2D'/MetaHumanCharacter/Lookdev_UHM/Eye/Textures/T_iris_color_picker.T_iris_color_picker'"));
	check(IrisColorPicker);

	const TSharedRef<SWidget> IrisSectionWidget =
		SNew(SMetaHumanCharacterEditorToolPanel)
		.Label(LOCTEXT("IrisSectionLabel", "Iris"))
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
				SNew(SMetaHumanCharacterEditorTileView<EMetaHumanCharacterEyesIrisPattern>)
					.OnGetSlateBrush(this, &SMetaHumanCharacterEditorEyesToolView::GetIrisSectionBrush)
					.OnSelectionChanged(this, &SMetaHumanCharacterEditorEyesToolView::OnEnumPropertyValueChanged, IrisPatternProperty, (void*) IrisProperties)
					.InitiallySelectedItem(IrisProperties->IrisPattern)
			]

			// Iris Rotation
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				CreatePropertySpinBoxWidget(IrisRotationProperty->GetDisplayNameText().ToString(), IrisRotationProperty, IrisProperties)
			]

			// TODO: Replace with a UV color picker

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				CreatePropertyUVColorPickerWidget(PrimaryColorUProperty, PrimaryColorVProperty, IrisProperties, LOCTEXT("IrisPrimaryColor", "Primary Color"), IrisColorPicker)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				CreatePropertyUVColorPickerWidget(SecondaryColorUProperty, SecondaryColorVProperty, IrisProperties, LOCTEXT("IrisSecondaryColor", "Secondary Color"), IrisColorPicker)
			]

			// Color Blend
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				CreatePropertySpinBoxWidget(ColorBlendProperty->GetDisplayNameText().ToString(), ColorBlendProperty, IrisProperties)
			]

			// BlendMethod combo box section
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				CreatePropertyComboBoxWidget<EMetaHumanCharacterEyesBlendMethod>(BlendMethodProperty->GetDisplayNameText().ToString(), IrisProperties->BlendMethod, BlendMethodProperty, IrisProperties)
			]

			// ShadowDetails spinbox section
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				CreatePropertySpinBoxWidget(ShadowDetailsProperty->GetDisplayNameText().ToString(), ShadowDetailsProperty, IrisProperties)
			]

			// LimbalRingSize spinbox section
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				CreatePropertySpinBoxWidgetNormalized(LimbalRingSizeProperty, IrisProperties, 0.6f, 0.85f)
			]

			// LimbalRingSoftness spinbox section
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				CreatePropertySpinBoxWidgetNormalized(LimbalRingSoftnessProperty, IrisProperties, 0.02f, 0.15f)
			]

			// LimbalRingColor color picker
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				CreatePropertyColorPickerWidget(LimbalRingColorProperty->GetDisplayNameText().ToString(), LimbalRingColorProperty, IrisProperties)
			]

			// Global Saturation
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				CreatePropertySpinBoxWidgetNormalized(GlobalSaturationProperty, IrisProperties, 0.0f, 4.0f)
			]

			// Global Tint
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				CreatePropertyColorPickerWidget(GlobalTintProperty->GetDisplayNameText().ToString(), GlobalTintProperty, IrisProperties)
			]
		];

	return IrisSectionWidget;
}

TSharedRef<SWidget> SMetaHumanCharacterEditorEyesToolView::CreateEyeToolViewPupilSection()
{
	UMetaHumanCharacterEditorEyesToolProperties* EyesToolProperties = Cast<UMetaHumanCharacterEditorEyesToolProperties>(GetToolProperties());
	FMetaHumanCharacterEyePupilProperties* PupilProperties = IsValid(EyesToolProperties) ? &EyesToolProperties->Eye.Pupil: nullptr;
	if (!PupilProperties)
	{
		return SNullWidget::NullWidget;
	}

	UStruct* Struct = FMetaHumanCharacterEyePupilProperties::StaticStruct();

	FProperty* DilationProperty = Struct->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterEyePupilProperties, Dilation));
	FProperty* FeatherProperty = Struct->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterEyePupilProperties, Feather));

	TSharedRef<SWidget> PupilSectionWidget =
		SNew(SMetaHumanCharacterEditorToolPanel)
		.Label(LOCTEXT("PupilSectionLabel", "Pupil"))
		.Content()
		[
			SNew(SVerticalBox)

			// Dilation
			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				CreatePropertySpinBoxWidgetNormalized(DilationProperty, PupilProperties, 0.85f, 1.2f)
			]

			// Feather
			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				CreatePropertySpinBoxWidgetNormalized(FeatherProperty, PupilProperties, 0.6f, 1.0f)
			]
		];
	
	return PupilSectionWidget;
}

TSharedRef<SWidget> SMetaHumanCharacterEditorEyesToolView::CreateEyeCorneaViewSection()
{
	UMetaHumanCharacterEditorEyesToolProperties* EyesToolProperties = Cast<UMetaHumanCharacterEditorEyesToolProperties>(GetToolProperties());
	FMetaHumanCharacterEyeCorneaProperties* CorneaProperties = IsValid(EyesToolProperties) ? &EyesToolProperties->Eye.Cornea : nullptr;
	if (!CorneaProperties)
	{
		return SNullWidget::NullWidget;
	}

	UStruct* Struct = FMetaHumanCharacterEyeCorneaProperties::StaticStruct();

	FProperty* SizeProperty = Struct->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterEyeCorneaProperties, Size));
	FProperty* LimbusSoftnessProperty = Struct->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterEyeCorneaProperties, LimbusSoftness));
	FProperty* LimbusColorProperty = Struct->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterEyeCorneaProperties, LimbusColor));

	TSharedRef<SWidget> CorneaSectionWidget =
		SNew(SMetaHumanCharacterEditorToolPanel)
		.Label(LOCTEXT("CorneaSectionLabel", "Cornea"))
		.Content()
		[
			SNew(SVerticalBox)

			// Size
			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				CreatePropertySpinBoxWidgetNormalized(SizeProperty, CorneaProperties, 0.145f, 0.185f)
			]

			// LimbusSoftness
			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				CreatePropertySpinBoxWidgetNormalized(LimbusSoftnessProperty, CorneaProperties, 0.03f, 0.15f)
			]

			// LimbusColor
			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				CreatePropertyColorPickerWidget(LimbusColorProperty->GetDisplayNameText().ToString(), LimbusColorProperty, CorneaProperties)
			]
		];

	return CorneaSectionWidget;
}

TSharedRef<SWidget> SMetaHumanCharacterEditorEyesToolView::CreateEyesToolViewScleraSection()
{
	UMetaHumanCharacterEditorEyesToolProperties* EyesToolProperties = Cast<UMetaHumanCharacterEditorEyesToolProperties>(GetToolProperties());
	FMetaHumanCharacterEyeScleraProperties* ScleraProperties = IsValid(EyesToolProperties) ? &EyesToolProperties->Eye.Sclera : nullptr;
	if (!ScleraProperties)
	{
		return SNullWidget::NullWidget;
	}


	UStruct* Struct = FMetaHumanCharacterEyeScleraProperties::StaticStruct();

	FProperty* RotationProperty = Struct->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterEyeScleraProperties, Rotation));
	FBoolProperty* UseCustomTintProperty = CastFieldChecked<FBoolProperty>(Struct->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterEyeScleraProperties, bUseCustomTint)));
	FProperty* TintProperty = Struct->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterEyeScleraProperties, Tint));
	FProperty* TransmissionSpreadProperty = Struct->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterEyeScleraProperties, TransmissionSpread));
	FProperty* TransmissionColorProperty = Struct->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterEyeScleraProperties, TransmissionColor));
	FProperty* VascularityInstensityProperty = Struct->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterEyeScleraProperties, VascularityIntensity));
	FProperty* VascularityCoverageProperty = Struct->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FMetaHumanCharacterEyeScleraProperties, VascularityCoverage));


	const TSharedRef<SWidget> ScleraSectionWidget =
		SNew(SMetaHumanCharacterEditorToolPanel)
		.Label(LOCTEXT("ScleraSectionLabel", "Sclera"))
		.Content()
		[
			SNew(SVerticalBox)

			// Tint color picker section
			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				CreatePropertySpinBoxWidget(RotationProperty->GetDisplayNameText().ToString(), RotationProperty, ScleraProperties)
			]

			// Use Custom Sclera Tint
			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				CreatePropertyCheckBoxWidget(UseCustomTintProperty->GetDisplayNameText().ToString(), UseCustomTintProperty, ScleraProperties)
			]

			// Tint color picker
			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				SNew(SBox)
				.IsEnabled_Lambda([UseCustomTintProperty, ScleraProperties]
								  {
									return UseCustomTintProperty->GetPropertyValue_InContainer(ScleraProperties);
								  })
				[
					CreatePropertyColorPickerWidget(TintProperty->GetDisplayNameText().ToString(), TintProperty, ScleraProperties)
				]
			]

			// TransmissionSpread section
			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				CreatePropertySpinBoxWidgetNormalized(TransmissionSpreadProperty, ScleraProperties, 0.03f, 0.2f)
			]

			// TransmissionColorProperty picker
			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				CreatePropertyColorPickerWidget(TransmissionColorProperty->GetDisplayNameText().ToString(), TransmissionColorProperty, ScleraProperties)
			]

			// VascularityIntensity section
			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				CreatePropertySpinBoxWidgetNormalized(VascularityInstensityProperty, ScleraProperties, 0.0f, 2.0f)
			]

			// VascularityCoverage section
			+ SVerticalBox::Slot()
			.MinHeight(24.f)
			.Padding(2.f, 0.f)
			.AutoHeight()
			[
				CreatePropertySpinBoxWidgetNormalized(VascularityCoverageProperty, ScleraProperties, 0.1f, 0.4f)
			]
		];

	return ScleraSectionWidget;
}

const FSlateBrush* SMetaHumanCharacterEditorEyesToolView::GetIrisSectionBrush(uint8 InItem)
{
	const FString IrisMaskName = StaticEnum<EMetaHumanCharacterEyesIrisPattern>()->GetAuthoredNameStringByValue(InItem);
	const FString IrisMaskBrushName = FString::Format(TEXT("Eyes.Iris.{0}"), { IrisMaskName });
	return FMetaHumanCharacterEditorStyle::Get().GetBrush(*IrisMaskBrushName);
}

#undef LOCTEXT_NAMESPACE
