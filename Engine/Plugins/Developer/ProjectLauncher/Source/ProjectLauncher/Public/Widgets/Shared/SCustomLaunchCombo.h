// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateOptMacros.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SComboButton.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

// helper class to create a combo box that can select an item
template<typename T>
class SCustomLaunchCombo
	: public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnSelectionChanged, T );
	DECLARE_DELEGATE_RetVal_OneParam(FText, FGetItemText, T)

	SLATE_BEGIN_ARGS(SCustomLaunchCombo<T>)
		{}
		SLATE_EVENT(FOnSelectionChanged, OnSelectionChanged)
		SLATE_EVENT(FGetItemText, GetDisplayName)
		SLATE_EVENT(FGetItemText, GetItemToolTip)
		SLATE_ATTRIBUTE(TArray<T>, Items)
		SLATE_ATTRIBUTE(T, SelectedItem)
	SLATE_END_ARGS()

public:
	void Construct(	const FArguments& InArgs)
	{
		OnSelectionChanged = InArgs._OnSelectionChanged;
		GetDisplayName = InArgs._GetDisplayName;
		GetItemToolTip = InArgs._GetItemToolTip;
		Items = InArgs._Items;
		SelectedItem = InArgs._SelectedItem;

		ChildSlot
		[
			SNew(SComboButton)
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text(this, &SCustomLaunchCombo<T>::GetSelectedItemDisplayName)
				.Font(FCoreStyle::Get().GetFontStyle("SmallFont"))
			]
			.OnGetMenuContent(this, &SCustomLaunchCombo<T>::MakeWidget)
		];
	}

protected:
	TAttribute<T> SelectedItem;
	TAttribute<TArray<T>> Items;
	FOnSelectionChanged OnSelectionChanged;
	FGetItemText GetDisplayName;
	FGetItemText GetItemToolTip;

	FText GetSelectedItemDisplayName() const
	{
		T Item = SelectedItem.Get();
		return ToText(Item);
	}

	void SetSelectedItem(T Value)
	{
		OnSelectionChanged.ExecuteIfBound(Value);
	}

	FText ToText(T Value) const
	{
		if (GetDisplayName.IsBound())
		{
			return GetDisplayName.Execute(Value);
		}

		return ToTextOverride(Value);
	}


	virtual FText ToTextOverride(T Value) const
	{
		return FText::FromString("ERROR: GetDisplayName or ToTextOverride not bound");
	}


BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	TSharedRef<SWidget> MakeWidget()
	{
		FMenuBuilder MenuBuilder(true, nullptr);
		{
			TArray<T> MenuItems = Items.Get();

			for (const T& Item : MenuItems)
			{
				FText ItemText = ToText(Item);
				FText ToolTipText = GetItemToolTip.IsBound() ? GetItemToolTip.Execute(Item) : FText::GetEmpty();

				MenuBuilder.AddMenuEntry(
					ItemText, 
					ToolTipText, 
					FSlateIcon(), 
					FUIAction(
						FExecuteAction::CreateSP(this, &SCustomLaunchCombo<T>::SetSelectedItem, Item),
						FCanExecuteAction(),
						FGetActionCheckState::CreateLambda( [this, Item]() { return (SelectedItem.Get() == Item) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					),
					NAME_None,
					EUserInterfaceActionType::Check);
			}
		}

		return MenuBuilder.MakeWidget();
	}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION


};


// helper class that can display a list of items, if those items have a LexToString function available
template<typename T>
class SCustomLaunchLexToStringCombo : public SCustomLaunchCombo<T>
{
public:
	virtual FText ToTextOverride(T Value) const override
	{
		return FText::FromString(LexToString(Value));
	}
};

// helper class that can display a list of strings
class SCustomLaunchStringCombo : public SCustomLaunchCombo<FString>
{
public:
	virtual FText ToTextOverride(FString Value) const override
	{
		return FText::FromString(Value);
	}
};



