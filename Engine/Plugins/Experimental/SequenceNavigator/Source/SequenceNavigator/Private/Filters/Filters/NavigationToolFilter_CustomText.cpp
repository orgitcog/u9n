// Copyright Epic Games, Inc. All Rights Reserved.

#include "Filters/Filters/NavigationToolFilter_CustomText.h"

namespace UE::SequenceNavigator
{

using namespace UE::Sequencer;

FNavigationToolFilter_CustomText::FNavigationToolFilter_CustomText(INavigationToolFilterBar& InFilterInterface)
	: FNavigationToolFilter_Text(InFilterInterface)
{
}

void FNavigationToolFilter_CustomText::SetFromCustomTextFilterData(const FCustomTextFilterData& InFilterData)
{
	Color = InFilterData.FilterColor;
	DisplayName = InFilterData.FilterLabel;
	SetRawFilterText(InFilterData.FilterString);
}

FCustomTextFilterData FNavigationToolFilter_CustomText::CreateCustomTextFilterData() const
{
	FCustomTextFilterData CustomTextFilterData;
	CustomTextFilterData.FilterColor = Color;
	CustomTextFilterData.FilterLabel = DisplayName;
	CustomTextFilterData.FilterString = GetRawFilterText();
	return CustomTextFilterData;
}

TSharedPtr<FFilterBase<FNavigationToolItemPtr>> FNavigationToolFilter_CustomText::GetFilter()
{
	return AsShared();
}

bool FNavigationToolFilter_CustomText::IsCustomTextFilter() const
{
	return true;
}

FText FNavigationToolFilter_CustomText::GetDefaultToolTipText() const
{
	return GetDisplayName(); 
}

TSharedPtr<FUICommandInfo> FNavigationToolFilter_CustomText::GetToggleCommand() const
{
	return nullptr;
}

FText FNavigationToolFilter_CustomText::GetDisplayName() const
{
	return DisplayName;
}
FText FNavigationToolFilter_CustomText::GetToolTipText() const
{
	return GetRawFilterText();
}

FLinearColor FNavigationToolFilter_CustomText::GetColor() const
{
	return Color;
}

FString FNavigationToolFilter_CustomText::GetName() const
{
	return FCustomTextFilter<FNavigationToolItemPtr>::GetFilterTypeName().ToString();
}

} // namespace UE::SequenceNavigator
