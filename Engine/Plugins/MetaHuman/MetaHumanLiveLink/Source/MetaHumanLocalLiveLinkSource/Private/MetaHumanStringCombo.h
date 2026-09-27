// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SComboBox.h"



class SMetaHumanStringCombo : public SCompoundWidget
{
public:

	typedef TSharedPtr<TPair<FString, FString>> FComboItemType;

	DECLARE_DELEGATE_OneParam(FOnItemSelected, FComboItemType InSelectedItem);

	SLATE_BEGIN_ARGS(SMetaHumanStringCombo) {}
		SLATE_EVENT(FOnItemSelected, OnItemSelected)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TArray<FComboItemType>* InItemSource);

	void RefreshOptions();

	FComboItemType CurrentItem = nullptr;

private:

	TSharedPtr<SComboBox<FComboItemType>> Combo;

	TSharedRef<SWidget> MakeWidgetForItem(FComboItemType InItem) const;

	void OnSelectionChanged(FComboItemType InItem, ESelectInfo::Type InSelectType);

	FText GetCurrentItemLabel() const;

	FOnItemSelected OnItemSelected;
};
