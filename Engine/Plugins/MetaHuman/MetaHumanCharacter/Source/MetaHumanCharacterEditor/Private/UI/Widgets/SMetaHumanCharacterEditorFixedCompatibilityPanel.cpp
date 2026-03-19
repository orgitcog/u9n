// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetaHumanCharacterEditorFixedCompatibilityPanel.h"
#include "MetaHumanCharacterEditorStyle.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SSpinBox.h"

#define LOCTEXT_NAMESPACE "SMetaHumanCharacterEditorFixedCompatibilityPanel"

static TArray<EMetaHumanBodyType> GetBodyTypeSubRangeByHeight(int32 InHeight)
{
	TArray<EMetaHumanBodyType> BodyTypeSubRange;
	if (InHeight == 0)
	{
		BodyTypeSubRange = {
			EMetaHumanBodyType::f_srt_unw, EMetaHumanBodyType::m_srt_unw, EMetaHumanBodyType::f_srt_nrw,
			EMetaHumanBodyType::m_srt_nrw, EMetaHumanBodyType::f_srt_ovw, EMetaHumanBodyType::m_srt_ovw};
	}
	else if (InHeight == 1)
	{
		BodyTypeSubRange = {
			EMetaHumanBodyType::f_med_unw, EMetaHumanBodyType::m_med_unw, EMetaHumanBodyType::f_med_nrw,
			EMetaHumanBodyType::m_med_nrw, EMetaHumanBodyType::f_med_ovw, EMetaHumanBodyType::m_med_ovw};
	}
	else
	{
		BodyTypeSubRange= {
			EMetaHumanBodyType::f_tal_unw, EMetaHumanBodyType::m_tal_unw, EMetaHumanBodyType::f_tal_nrw,
			EMetaHumanBodyType::m_tal_nrw, EMetaHumanBodyType::f_tal_ovw, EMetaHumanBodyType::m_tal_ovw};
	}
	return BodyTypeSubRange;
}

void SMetaHumanCharacterEditorFixedCompatibilityPanel::Construct(const FArguments& InArgs)
{
	FixedCompatabilityProperties = InArgs._FixedCompatabilityProperties;
	OnSelectionChangedDelegate = InArgs._OnSelectionChanged;

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.Padding(4.f)
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(4)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("FixedCompatibilityHeight", "Height"))
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Fill)
			.Padding(4)
			.FillWidth(true)
			[
				SNew(SSlider)
				.MinValue(0)
				.MaxValue(2)
				.StepSize(1)
				.MouseUsesStep(true)
				.Style(&FAppStyle::Get().GetWidgetStyle<FSliderStyle>("AnimBlueprint.AssetPlayerSlider"))
				.Value_Lambda([this]
				{
					return GetHeightValue();
				})
				.OnValueChanged_Lambda([this](float NewHeight)
				{
					int32 HeightValue = static_cast<int32>(NewHeight);
					OnHeightValueChanged(HeightValue);
				})
			]
		]

		+ SVerticalBox::Slot()
		.Padding(4.f)
		[
			SAssignNew(TileView, SMetaHumanCharacterEditorTileView<EMetaHumanBodyType>)
			.OnGetSlateBrush(this, &SMetaHumanCharacterEditorFixedCompatibilityPanel::GetFixedCompatabilityBodyBrush)
			.OnSelectionChanged(InArgs._OnSelectionChanged)
			.InitiallySelectedItem(FixedCompatabilityProperties->MetaHumanBodyType)
			.ExcludedItems({EMetaHumanBodyType::BlendableBody})
		]
	];

	OnHeightValueChanged(GetHeightValue());
}

void SMetaHumanCharacterEditorFixedCompatibilityPanel::UpdateItemListFromProperties()
{
	TileView->SetItemsSource(GetBodyTypeSubRangeByHeight(GetHeightValue()), FixedCompatabilityProperties->MetaHumanBodyType);
}

int32 SMetaHumanCharacterEditorFixedCompatibilityPanel::GetHeightValue() const
{
	if (FixedCompatabilityProperties.IsValid())
	{
		return FixedCompatabilityProperties->GetHeightIndex();
	}
	return 1;
}

void SMetaHumanCharacterEditorFixedCompatibilityPanel::OnHeightValueChanged(int32 HeightValue)
{
	if (FixedCompatabilityProperties.IsValid())
	{
		FixedCompatabilityProperties->Height = static_cast<EMetaHumanCharacterFixedBodyToolHeight>(HeightValue);
		UpdateItemListFromProperties();
	}
}

const FSlateBrush* SMetaHumanCharacterEditorFixedCompatibilityPanel::GetFixedCompatabilityBodyBrush(uint8 InItem)
{
	const FString FixedBodyName = StaticEnum<EMetaHumanBodyType>()->GetAuthoredNameStringByValue(InItem);
	const FString FixedBodyBrushName = FString::Format(TEXT("Legacy.Body.{0}"), { FixedBodyName });
	return FMetaHumanCharacterEditorStyle::Get().GetBrush(*FixedBodyBrushName);
}

#undef LOCTEXT_NAMESPACE
