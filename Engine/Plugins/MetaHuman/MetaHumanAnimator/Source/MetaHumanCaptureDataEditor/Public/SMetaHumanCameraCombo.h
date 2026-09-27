// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SComboBox.h"
#include "Templates/SharedPointer.h"
#include "Containers/UnrealString.h"
#include "PropertyHandle.h"
#include "UObject/Object.h"



class METAHUMANCAPTUREDATAEDITOR_API SMetaHumanCameraCombo : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMetaHumanCameraCombo)
	{
	}
	SLATE_END_ARGS()

	typedef TSharedPtr<FString> FComboItemType;

	void Construct(const FArguments& InArgs, const TArray<TSharedPtr<FString>>* InOptionsSource, const FString* InCamera, TObjectPtr<UObject> InPropertyOwner, TSharedPtr<IPropertyHandle> InProperty);

	void HandleSourceDataChanged(class UFootageCaptureData* InFootageCaptureData, class USoundWave* InAudio, bool bInResetRanges);
	void HandleSourceDataChanged(bool bInResetRanges);

	TSharedRef<SWidget> MakeWidgetForOption(FComboItemType InOption);

	void OnSelectionChanged(FComboItemType InNewValue, ESelectInfo::Type);

	FText GetCurrentItemLabel() const;

	bool IsEnabled() const;

private:

	const FString* Camera = nullptr;
	TObjectPtr<UObject> PropertyOwner;
	TSharedPtr<IPropertyHandle> Property;
	TSharedPtr<SComboBox<FComboItemType>> Combo;
};