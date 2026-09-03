// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetaHumanConfigCombo.h"
#include "DetailLayoutBuilder.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetData.h"



void SMetaHumanConfigCombo::Construct(const FArguments& InArgs, EMetaHumanConfigType InMetaHumanConfigType, TObjectPtr<UObject> InPropertyOwner, TSharedPtr<IPropertyHandle> InProperty)
{
	TArray<FAssetData> Configs;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	AssetRegistryModule.Get().GetAssetsByClass(UMetaHumanConfig::StaticClass()->GetClassPathName(), Configs);

	for (int32 Index = Configs.Num() - 1; Index >= 0; --Index)
	{
		const UMetaHumanConfig* Config = Cast<UMetaHumanConfig>(Configs[Index].GetAsset());
		if (!Config || Config->Type != InMetaHumanConfigType)
		{
			Configs.RemoveAt(Index);
		}
	}

	Configs.Sort();
	Configs.Insert(FAssetData(), 0);

	for (const FAssetData& AssetData : Configs)
	{
		OptionsSource.Add(MakeShared<FAssetData>(AssetData));
	}

	PropertyOwner = InPropertyOwner;
	Property = InProperty;

	Combo = SNew(SComboBox<FComboItemType>)
			.OptionsSource(&OptionsSource)
			.OnSelectionChanged(this, &SMetaHumanConfigCombo::OnSelectionChanged)
			.OnGenerateWidget(this, &SMetaHumanConfigCombo::MakeWidgetForOption)
			.IsEnabled(this, &SMetaHumanConfigCombo::IsEnabled)
			[
				SNew(STextBlock)
				.Text(this, &SMetaHumanConfigCombo::GetCurrentItemLabel)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			];

	ChildSlot
	[
		Combo.ToSharedRef()
	];
}

TSharedRef<SWidget> SMetaHumanConfigCombo::MakeWidgetForOption(FComboItemType InOption)
{
	return SNew(STextBlock).Text(FText::FromString(InOption->AssetName.ToString()));
}

void SMetaHumanConfigCombo::OnSelectionChanged(FComboItemType InNewValue, ESelectInfo::Type)
{
	if (InNewValue.IsValid())
	{
		Property->SetValue(*InNewValue);
	}

	Combo->ClearSelection();
}

FText SMetaHumanConfigCombo::GetCurrentItemLabel() const
{
	FAssetData AssetData;
	Property->GetValue(AssetData);
	return FText::FromString(AssetData.AssetName.ToString());
}

bool SMetaHumanConfigCombo::IsEnabled() const
{
	return PropertyOwner && Property && PropertyOwner->CanEditChange(Property->GetProperty());
}
