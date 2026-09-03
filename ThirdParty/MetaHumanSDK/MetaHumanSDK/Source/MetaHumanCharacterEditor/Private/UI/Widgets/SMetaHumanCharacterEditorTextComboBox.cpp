// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetaHumanCharacterEditorTextComboBox.h"

void SMetaHumanCharacterEditorTextComboBox::Construct(const typename SMetaHumanCharacterEditorTextComboBox::FArguments& InArgs, const TArray<TSharedPtr<FString>>& InComboBoxOptions, const TSharedPtr<FString>& InInitiallySelectedItem)
{
	OnSelectionChanged = InArgs._OnSelectionChanged;
	ComboBoxOptions = InComboBoxOptions;

	check(OnSelectionChanged.IsBound());
	check(!ComboBoxOptions.IsEmpty());

	ChildSlot
		[
			SAssignNew(ComboBox, SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&ComboBoxOptions)
				.InitiallySelectedItem(InInitiallySelectedItem)
				.OnGenerateWidget(this, &SMetaHumanCharacterEditorTextComboBox::OnGenerateWidget)
				.OnSelectionChanged(this, &SMetaHumanCharacterEditorTextComboBox::OnComboBoxSelectionChanged)
				.IsEnabled(InArgs._IsEnabled)
				.Content()
				[
					SNew(STextBlock)
						.Text(this, &SMetaHumanCharacterEditorTextComboBox::GetSelectedItemAsText)
						.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
		];
}

void SMetaHumanCharacterEditorTextComboBox::SetSelectedItem(const int8 InIdx)
{
	if (ComboBox.IsValid() && InIdx >= 0 && InIdx < ComboBoxOptions.Num())
	{
		if (ComboBox->GetSelectedItem() != ComboBoxOptions[InIdx])
		{
			ComboBox->SetSelectedItem(ComboBoxOptions[InIdx]);
			OnSelectionChanged.ExecuteIfBound(InIdx);
		}
	}
}

TSharedRef<SWidget> SMetaHumanCharacterEditorTextComboBox::OnGenerateWidget(TSharedPtr<FString> InItem)
{
	check(InItem.IsValid());

	return
		SNew(STextBlock)
		.Text(FText::FromString(*InItem))
		.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));
}

void SMetaHumanCharacterEditorTextComboBox::OnComboBoxSelectionChanged(TSharedPtr<FString> InItem, ESelectInfo::Type InSelectInfo)
{
	if (InItem.IsValid() && InSelectInfo != ESelectInfo::Direct)
	{
		int32 SelectedIdx = -1;
		for (int32 Idx = 0; Idx < ComboBoxOptions.Num(); ++Idx)
		{
			if (ComboBoxOptions[Idx] == InItem)
			{
				SelectedIdx = Idx;
			}
		}
		OnSelectionChanged.ExecuteIfBound(SelectedIdx);
	}
}

FText SMetaHumanCharacterEditorTextComboBox::GetSelectedItemAsText() const
{
	const TSharedPtr<FString> SelectedItem = ComboBox.IsValid() ? ComboBox->GetSelectedItem() : nullptr;
	if (SelectedItem.IsValid())
	{
		return FText::FromString(*SelectedItem.Get());
	}

	return FText::GetEmpty();
}
