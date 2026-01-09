// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanStringCombo.h"

#include "Widgets/Text/STextBlock.h"

#if WITH_EDITOR
#include "DetailLayoutBuilder.h"
#endif



void SMetaHumanStringCombo::Construct(const FArguments& InArgs, const TArray<FComboItemType>* InItemSource)
{
	OnItemSelected = InArgs._OnItemSelected;

	Combo = SNew(SComboBox<FComboItemType>)
			.OptionsSource(InItemSource)
			.OnGenerateWidget(this, &SMetaHumanStringCombo::MakeWidgetForItem)
			.OnSelectionChanged(this, &SMetaHumanStringCombo::OnSelectionChanged)
			[
				SNew(STextBlock)
#if WITH_EDITOR
				.Font(IDetailLayoutBuilder::GetDetailFont())
#endif
				.Text(this, &SMetaHumanStringCombo::GetCurrentItemLabel)
			];

	ChildSlot
	[
		Combo.ToSharedRef()
	];
}

void SMetaHumanStringCombo::RefreshOptions()
{
	Combo->ClearSelection();
	Combo->RefreshOptions();
}

TSharedRef<SWidget> SMetaHumanStringCombo::MakeWidgetForItem(FComboItemType InItem) const
{
	return SNew(STextBlock).Text(FText::FromString(InItem->Key));
}

void SMetaHumanStringCombo::OnSelectionChanged(FComboItemType InItem, ESelectInfo::Type InSelectType)
{
	CurrentItem = InItem;

	OnItemSelected.ExecuteIfBound(InItem);
}

FText SMetaHumanStringCombo::GetCurrentItemLabel() const
{
	return FText::FromString(CurrentItem.IsValid() ? CurrentItem->Key : TEXT(""));
}
