// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetaHumanCharacterEditorAccentRegionsPanel.h"

#include "MetaHumanCharacterEditorStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SOverlay.h"

#define LOCTEXT_NAMESPACE "SMetaHumanCharacterEditorAccentRegionsPanel"

void SMetaHumanCharacterEditorAccentRegionsPanel::Construct(const FArguments& InArgs)
{
	Invalidate(EInvalidateWidget::LayoutAndVolatility);

	RegionClickedDelegate = InArgs._OnRegionClicked;
	SelectedRegion = InArgs._SelectedRegion;

	ChildSlot
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.AutoHeight()
			[
				SNew(SOverlay)

				+ SOverlay::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SConstraintCanvas)
					+ SConstraintCanvas::Slot()
					.Anchors(FAnchors(0.5f, 0.5f))
					.Offset(FMargin(0.f, 0.f))
					.AutoSize(true)
					[
						SNew(SImage)
						.DesiredSizeOverride(FVector2D(200.f, 270.f))
						.Image(FMetaHumanCharacterEditorStyle::Get().GetBrush(TEXT("Skin.Accents.Head")))
					]
				]
				+ SOverlay::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				[
					SNew(SConstraintCanvas)

					+SConstraintCanvas::Slot()
					.Anchors(FAnchors(0.5f, 0.5f))
					.Offset(FMargin(-100.f, -135.f))
					.AutoSize(true)
					[
						SAssignNew(AccentRegionsCanvas, SCanvas)
					]
				]
			]
		];

	const TArray<FMetaHumanCharacterEditorAccentRegionInfo> AccentRegionsInfos = CreateAccentRegionsInfoArray();
	MakeAccentRegionsCanvas(AccentRegionsInfos);
}

EMetaHumanCharacterAccentRegion SMetaHumanCharacterEditorAccentRegionsPanel::GetSelectedRegion() const
{
	return SelectedRegion.IsSet() ? SelectedRegion.Get() : EMetaHumanCharacterAccentRegion::Count;
}

void SMetaHumanCharacterEditorAccentRegionsPanel::MakeAccentRegionsCanvas(const TArray<FMetaHumanCharacterEditorAccentRegionInfo> AccentRegionsInfos)
{
	if (AccentRegionsCanvas.IsValid() && !AccentRegionsInfos.IsEmpty())
	{
		for(const FMetaHumanCharacterEditorAccentRegionInfo& AccentRegionInfo : AccentRegionsInfos)
		{
			const FString StyleNameAsString = TEXT("Skin.Accents.") + AccentRegionInfo.Name.ToString();
			const FName StyleName = *StyleNameAsString;

			AccentRegionsCanvas->AddSlot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Position(AccentRegionInfo.Position)
				.Size(AccentRegionInfo.Size)
				[
					SNew(SCheckBox)
					.Style(FMetaHumanCharacterEditorStyle::Get(), StyleName)
					.ToolTipText(AccentRegionInfo.Label)
					.IsChecked(this, &SMetaHumanCharacterEditorAccentRegionsPanel::IsRegionSelected, AccentRegionInfo.Type)
					.OnCheckStateChanged(this, &SMetaHumanCharacterEditorAccentRegionsPanel::OnRegionCheckedStateChanged, AccentRegionInfo.Type)
				];
		}
	}
}

TArray<FMetaHumanCharacterEditorAccentRegionInfo> SMetaHumanCharacterEditorAccentRegionsPanel::CreateAccentRegionsInfoArray() const
{
	TArray<FMetaHumanCharacterEditorAccentRegionInfo> AccentRegionsInfos;

	// Scalp region
	FMetaHumanCharacterEditorAccentRegionInfo ScalpRegionInfo;
	ScalpRegionInfo.Type = EMetaHumanCharacterAccentRegion::Scalp;
	ScalpRegionInfo.Name = TEXT("Scalp");
	ScalpRegionInfo.Label = FText::Format(LOCTEXT("ScalpTooltip", "{0}"), FText::FromName(ScalpRegionInfo.Name));
	ScalpRegionInfo.Position = FVector2D(100.f, 15.f);
	ScalpRegionInfo.Size = FVector2D(120.f, 30.f);
	AccentRegionsInfos.Add(ScalpRegionInfo);

	// Forehead region
	FMetaHumanCharacterEditorAccentRegionInfo ForeheadRegionInfo;
	ForeheadRegionInfo.Type = EMetaHumanCharacterAccentRegion::Forehead;
	ForeheadRegionInfo.Name = TEXT("Forehead");
	ForeheadRegionInfo.Label = FText::Format(LOCTEXT("ForeheadTooltip", "{0}"), FText::FromName(ForeheadRegionInfo.Name));
	ForeheadRegionInfo.Position = FVector2D(100.f, 55.f);
	ForeheadRegionInfo.Size = FVector2D(165.f, 59.f);
	AccentRegionsInfos.Add(ForeheadRegionInfo);

	// Nose region
	FMetaHumanCharacterEditorAccentRegionInfo NoseRegionInfo;
	NoseRegionInfo.Type = EMetaHumanCharacterAccentRegion::Nose;
	NoseRegionInfo.Name = TEXT("Nose");
	NoseRegionInfo.Label = FText::Format(LOCTEXT("NoseTooltip", "{0}"), FText::FromName(NoseRegionInfo.Name));
	NoseRegionInfo.Position = FVector2D(100.f, 140.f);
	NoseRegionInfo.Size = FVector2D(50.f, 80.f);
	AccentRegionsInfos.Add(NoseRegionInfo);

	// UnderEye  regions
	FMetaHumanCharacterEditorAccentRegionInfo UnderEyeLeftRegionInfo;
	UnderEyeLeftRegionInfo.Type = EMetaHumanCharacterAccentRegion::UnderEye;
	UnderEyeLeftRegionInfo.Name = TEXT("UnderEyeLeft");
	UnderEyeLeftRegionInfo.Label = FText::Format(LOCTEXT("UnderEyeLeftTooltip", "{0}"), FText::FromName(UnderEyeLeftRegionInfo.Name));
	UnderEyeLeftRegionInfo.Position = FVector2D(52.f, 150.f);
	UnderEyeLeftRegionInfo.Size = FVector2D(55, 40.f);
	AccentRegionsInfos.Add(UnderEyeLeftRegionInfo);

	FMetaHumanCharacterEditorAccentRegionInfo UnderEyeRightRegionInfo;
	UnderEyeRightRegionInfo.Type = EMetaHumanCharacterAccentRegion::UnderEye;
	UnderEyeRightRegionInfo.Name = TEXT("UnderEyeRight");
	UnderEyeRightRegionInfo.Label = FText::Format(LOCTEXT("UnderEyeRightTooltip", "{0}"), FText::FromName(UnderEyeRightRegionInfo.Name));
	UnderEyeRightRegionInfo.Position = FVector2D(150.f, 150.f);
	UnderEyeRightRegionInfo.Size = FVector2D(55, 40.f);
	AccentRegionsInfos.Add(UnderEyeRightRegionInfo);

	// Ears regions
	FMetaHumanCharacterEditorAccentRegionInfo EarLeftRegionInfo;
	EarLeftRegionInfo.Type = EMetaHumanCharacterAccentRegion::Ears;
	EarLeftRegionInfo.Name = TEXT("EarLeft");
	EarLeftRegionInfo.Label = FText::Format(LOCTEXT("EarLeftTooltip", "{0}"), FText::FromName(EarLeftRegionInfo.Name));
	EarLeftRegionInfo.Position = FVector2D(10.f, 155.f);
	EarLeftRegionInfo.Size = FVector2D(21.f, 70.f);
	AccentRegionsInfos.Add(EarLeftRegionInfo);

	FMetaHumanCharacterEditorAccentRegionInfo EarRightRegionInfo;
	EarRightRegionInfo.Type = EMetaHumanCharacterAccentRegion::Ears;
	EarRightRegionInfo.Name = TEXT("EarRight");
	EarRightRegionInfo.Label = FText::Format(LOCTEXT("EarRightTooltip", "{0}"), FText::FromName(EarRightRegionInfo.Name));
	EarRightRegionInfo.Position = FVector2D(190.f, 155.f);
	EarRightRegionInfo.Size = FVector2D(21.f, 70.f);
	AccentRegionsInfos.Add(EarRightRegionInfo);

	// Cheeks regions
	FMetaHumanCharacterEditorAccentRegionInfo CheekLeftRegionInfo;
	CheekLeftRegionInfo.Type = EMetaHumanCharacterAccentRegion::Cheeks;
	CheekLeftRegionInfo.Name = TEXT("CheekLeft");
	CheekLeftRegionInfo.Label = FText::Format(LOCTEXT("CheekLeftTooltip", "{0}"), FText::FromName(CheekLeftRegionInfo.Name));
	CheekLeftRegionInfo.Position = FVector2D(43.5f, 200.f);
	CheekLeftRegionInfo.Size = FVector2D(44.f, 90.f);
	AccentRegionsInfos.Add(CheekLeftRegionInfo);

	FMetaHumanCharacterEditorAccentRegionInfo CheekRightRegionInfo;
	CheekRightRegionInfo.Name = TEXT("CheekRight");
	CheekRightRegionInfo.Label = FText::Format(LOCTEXT("CheekRightTooltip", "{0}"), FText::FromName(CheekRightRegionInfo.Name));
	CheekRightRegionInfo.Type = EMetaHumanCharacterAccentRegion::Cheeks;
	CheekRightRegionInfo.Position = FVector2D(155.f, 200.f);
	CheekRightRegionInfo.Size = FVector2D(44.f, 90.f);
	AccentRegionsInfos.Add(CheekRightRegionInfo);

	// Lips region
	FMetaHumanCharacterEditorAccentRegionInfo LipsRegionInfo;
	LipsRegionInfo.Type = EMetaHumanCharacterAccentRegion::Lips;
	LipsRegionInfo.Name = TEXT("Lips");
	LipsRegionInfo.Label = FText::Format(LOCTEXT("LipsTooltip", "{0}"), FText::FromName(LipsRegionInfo.Name));
	LipsRegionInfo.Position = FVector2D(100.f, 215.f);
	LipsRegionInfo.Size = FVector2D(81.f, 46.f);
	AccentRegionsInfos.Add( LipsRegionInfo);

	// Chin region
	FMetaHumanCharacterEditorAccentRegionInfo ChinRegionInfo;
	ChinRegionInfo.Type = EMetaHumanCharacterAccentRegion::Chin;
	ChinRegionInfo.Position = FVector2D(100.f, 250.f);
	ChinRegionInfo.Size = FVector2D(81.f, 30.f);
	ChinRegionInfo.Name = TEXT("Chin");
	ChinRegionInfo.Label = FText::Format(LOCTEXT("ChinTooltip", "{0}"), FText::FromName(ChinRegionInfo.Name));
	AccentRegionsInfos.Add(ChinRegionInfo);

	return AccentRegionsInfos;
}

ECheckBoxState SMetaHumanCharacterEditorAccentRegionsPanel::IsRegionSelected(EMetaHumanCharacterAccentRegion InRegion) const
{
	return (InRegion == SelectedRegion.Get()) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SMetaHumanCharacterEditorAccentRegionsPanel::OnRegionCheckedStateChanged(ECheckBoxState InState, EMetaHumanCharacterAccentRegion InRegion)
{
	if (InState == ECheckBoxState::Checked)
	{
		SelectedRegion = InRegion;
		RegionClickedDelegate.ExecuteIfBound(InRegion);
	}
}

#undef LOCTEXT_NAMESPACE
