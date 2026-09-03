// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetaHumanCameraCombo.h"
#include "DetailLayoutBuilder.h"



void SMetaHumanCameraCombo::Construct(const FArguments& InArgs, const TArray<TSharedPtr<FString>>* InOptionsSource, const FString* InCamera, TObjectPtr<UObject> InPropertyOwner, TSharedPtr<IPropertyHandle> InProperty)
{
	Camera = InCamera;
	PropertyOwner = InPropertyOwner;
	Property = InProperty;

	Combo = SNew(SComboBox<FComboItemType>)
		.OptionsSource(InOptionsSource)
		.OnSelectionChanged(this, &SMetaHumanCameraCombo::OnSelectionChanged)
		.OnGenerateWidget(this, &SMetaHumanCameraCombo::MakeWidgetForOption)
		.IsEnabled(this, &SMetaHumanCameraCombo::IsEnabled)
		[
			SNew(STextBlock)
			.Text(this, &SMetaHumanCameraCombo::GetCurrentItemLabel)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		];

	ChildSlot
		[
			Combo.ToSharedRef()
		];
}

void SMetaHumanCameraCombo::HandleSourceDataChanged(UFootageCaptureData* InFootageCaptureData, class USoundWave* InAudio, bool bInResetRanges)
{
	Combo->RefreshOptions();
}

void SMetaHumanCameraCombo::HandleSourceDataChanged(bool bInResetRanges)
{
	Combo->RefreshOptions();
}

TSharedRef<SWidget> SMetaHumanCameraCombo::MakeWidgetForOption(FComboItemType InOption)
{
	return SNew(STextBlock).Text(FText::FromString(*InOption));
}

void SMetaHumanCameraCombo::OnSelectionChanged(FComboItemType InNewValue, ESelectInfo::Type)
{
	if (InNewValue.IsValid())
	{
		Property->SetValue(*InNewValue);
	}

	Combo->ClearSelection();
}

FText SMetaHumanCameraCombo::GetCurrentItemLabel() const
{
	return FText::FromString(*Camera);
}

bool SMetaHumanCameraCombo::IsEnabled() const
{
	return PropertyOwner && Property && PropertyOwner->CanEditChange(Property->GetProperty());
}
