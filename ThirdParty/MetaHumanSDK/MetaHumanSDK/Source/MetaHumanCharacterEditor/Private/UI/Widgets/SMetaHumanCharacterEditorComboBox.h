// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Widgets/Input/SComboBox.h"

/*
* This class is used as a custom ComboBox for MetaHuman Character purposes
* Requirements for using it are having an Enum for options which the ComboBox will represent
*/
template<typename TEnum>
class SMetaHumanCharacterEditorComboBox : public SCompoundWidget
{
public:
	using FEnumType = typename std::underlying_type<TEnum>::type;

	DECLARE_DELEGATE_OneParam(FOnSelectionChanged, uint8)
	
	SLATE_BEGIN_ARGS(SMetaHumanCharacterEditorComboBox<TEnum>) {}

		/** The initially selected item of the Combo Box. */
		SLATE_ATTRIBUTE(TEnum, InitiallySelectedItem)

		/** Called when the selection of the Combo Box has changed. */
		SLATE_EVENT(FOnSelectionChanged, OnSelectionChanged)

	SLATE_END_ARGS()

	/** Constructs the widget. */
	void Construct(const typename SMetaHumanCharacterEditorComboBox<TEnum>::FArguments& InArgs)
	{
		OnSelectionChanged = InArgs._OnSelectionChanged;
		ComboBoxOptions = GetEnumOptions();

		check(OnSelectionChanged.IsBound());
		check(!ComboBoxOptions.IsEmpty());

		const TEnum InitiallySelectedItem = InArgs._InitiallySelectedItem.IsSet() ? InArgs._InitiallySelectedItem.Get() : static_cast<TEnum>(0);

		TSharedPtr<TEnum> InitialItem = nullptr;
		for (const TSharedPtr<TEnum>& ItemPtr : ComboBoxOptions)
		{
			if (!ItemPtr.IsValid())
			{
				continue;
			}

			const TEnum Item = *ItemPtr.Get();
			if (Item == InitiallySelectedItem)
			{
				InitialItem = ItemPtr;
			}
		}

		if (!InitialItem)
		{
			InitialItem = ComboBoxOptions[0];
		}

		ChildSlot
			[
				SAssignNew(ComboBox, SComboBox<TSharedPtr<TEnum>>)
				.OptionsSource(&ComboBoxOptions)
				.InitiallySelectedItem(InitialItem)
				.OnGenerateWidget(this, &SMetaHumanCharacterEditorComboBox::OnGenerateWidget)
				.OnSelectionChanged(this, &SMetaHumanCharacterEditorComboBox::OnComboBoxSelectionChanged)
				.IsEnabled(InArgs._IsEnabled)
				.Content()
				[
					SNew(STextBlock)
					.Text(this, &SMetaHumanCharacterEditorComboBox::GetSelectedEnumNameAsText)
					.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
			];
	}

	/** Gets the Enum name by the given index. */
	void SetSelectedItem(TEnum InItem) const
	{
		TSharedPtr<TEnum> ItemToSelect = nullptr;
		for (const TSharedPtr<TEnum>& ItemPtr : ComboBoxOptions)
		{
			if (!ItemPtr.IsValid())
			{
				continue;
			}

			const TEnum Item = *ItemPtr.Get();
			if (Item == InItem)
			{
				ItemToSelect = ItemPtr;
			}
		}

		if (ComboBox.IsValid() && ItemToSelect.IsValid())
		{
			if (ComboBox->GetSelectedItem() != ItemToSelect)
			{
				ComboBox->SetSelectedItem(ItemToSelect);
				OnSelectionChanged.ExecuteIfBound(static_cast<FEnumType>(InItem));
			}
		}
	}

private:
	/** Gets the Enum name by the given index. */
	FString GetEnumName(int64 EnumIndex) const
	{
		const UEnum* EnumPtr = StaticEnum<TEnum>();
		return EnumPtr ? EnumPtr->GetNameStringByValue(EnumIndex) : TEXT("");
	}

	/** Gets the name of the currently selected Enum as text. */
	FText GetSelectedEnumNameAsText() const
	{
		const TSharedPtr<TEnum> SelectedItem = ComboBox.IsValid() ? ComboBox->GetSelectedItem() : nullptr;
		if (SelectedItem.IsValid())
		{
			const int64 ItemValue = static_cast<int64>(static_cast<FEnumType>(*SelectedItem.Get()));
			const FString EnumName = GetEnumName(ItemValue);
			return FText::FromString(EnumName);
		}

		return FText::GetEmpty();
	}

	/** Gets the array of all Enum options from the given UEnum. */
	TArray<TSharedPtr<TEnum>> GetEnumOptions()
	{
		TArray<TSharedPtr<TEnum>> Options;

		UEnum* EnumPtr = StaticEnum<TEnum>();
		if (!EnumPtr)
		{
			return Options;
		}

		const int32 NumEnums = EnumPtr->NumEnums();
		for (int32 Index = 0; Index < NumEnums; ++Index)
		{
			const FString EnumName = EnumPtr->GetNameStringByIndex(Index);
			if (EnumName.EndsWith(TEXT("MAX")))
			{
				continue;
			}

			TEnum EnumValue = static_cast<TEnum>(EnumPtr->GetValueByIndex(Index));
			Options.Add(MakeShared<TEnum>(EnumValue));
		}

		return Options;
	}

	/** Generated the Combo Box widget for the given item. */
	TSharedRef<SWidget> OnGenerateWidget(TSharedPtr<TEnum>InItem)
	{
		check(InItem.IsValid());

		const int64 ItemValue = static_cast<int64>(static_cast<FEnumType>(*InItem.Get()));
		const FString EnumName = GetEnumName(ItemValue);
			
		return 
			SNew(STextBlock)
			.Text(FText::FromString(EnumName))
			.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));
	}

	/** Called when the combo box selection has changed. */
	void OnComboBoxSelectionChanged(TSharedPtr<TEnum> InItem, ESelectInfo::Type InSelectInfo)
	{
		if (InItem.IsValid() && InSelectInfo != ESelectInfo::Direct)
		{
			const uint8 Item = static_cast<FEnumType>(*InItem);
			OnSelectionChanged.ExecuteIfBound(Item);
		}
	}

	/** The delegate to execute when the selection of the Combo Box has changed. */
	FOnSelectionChanged OnSelectionChanged;

	/** The array of Combo Box options. */
	TArray<TSharedPtr<TEnum>> ComboBoxOptions;

	/** Reference to the Combo Box widget. */
	TSharedPtr<SComboBox<TSharedPtr<TEnum>>> ComboBox;
};
