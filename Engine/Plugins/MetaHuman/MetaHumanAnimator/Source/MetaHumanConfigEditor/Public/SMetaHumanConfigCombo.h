// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanConfig.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SComboBox.h"
#include "PropertyHandle.h"



class METAHUMANCONFIGEDITOR_API SMetaHumanConfigCombo : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SMetaHumanConfigCombo)
	{
	}
	SLATE_END_ARGS()

	typedef TSharedPtr<FAssetData> FComboItemType;

	void Construct(const FArguments& InArgs, EMetaHumanConfigType InMetaHumanConfigType, TObjectPtr<UObject> InPropertyOwner, TSharedPtr<IPropertyHandle> InProperty);

	TSharedRef<SWidget> MakeWidgetForOption(FComboItemType InOption);

	void OnSelectionChanged(FComboItemType InNewValue, ESelectInfo::Type);

	FText GetCurrentItemLabel() const;

	bool IsEnabled() const;

private:

	TArray<TSharedPtr<FAssetData>> OptionsSource;
	TObjectPtr<UObject> PropertyOwner;
	TSharedPtr<IPropertyHandle> Property;
	TSharedPtr<SComboBox<FComboItemType>> Combo;
};
