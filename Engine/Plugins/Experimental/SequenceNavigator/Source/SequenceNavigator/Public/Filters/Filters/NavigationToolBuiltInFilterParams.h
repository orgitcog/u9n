// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NavigationToolDefines.h"
#include "NavigationToolFilterBase.h"
#include "Styling/SlateBrush.h"
#include "NavigationToolBuiltInFilterParams.generated.h"

class UObject;

namespace UE::SequenceNavigator
{
	class FNavigationToolItemTypeId;
}

USTRUCT()
struct FNavigationToolBuiltInFilterParams
{
	GENERATED_BODY()

	SEQUENCENAVIGATOR_API static FNavigationToolBuiltInFilterParams CreateSequenceFilter();
	SEQUENCENAVIGATOR_API static FNavigationToolBuiltInFilterParams CreateTrackFilter();
	SEQUENCENAVIGATOR_API static FNavigationToolBuiltInFilterParams CreateBindingFilter();
	SEQUENCENAVIGATOR_API static FNavigationToolBuiltInFilterParams CreateMarkerFilter();

	FNavigationToolBuiltInFilterParams() = default;

	SEQUENCENAVIGATOR_API FNavigationToolBuiltInFilterParams(const FName InFilterId
		, const TSet<UE::SequenceNavigator::FNavigationToolItemTypeId>& InItemClasses
		, const TArray<TSubclassOf<UObject>>& InFilterClasses
		, const ENavigationToolFilterMode InMode = ENavigationToolFilterMode::MatchesType
		, const FSlateBrush* const InIconBrush = nullptr
		, const FText& InDisplayName = FText::GetEmpty()
		, const FText& InTooltipText = FText::GetEmpty()
		, const TSharedPtr<FUICommandInfo>& InToggleCommand = nullptr
		, const bool InEnabledByDefault = true
		, const EClassFlags InRequiredClassFlags = CLASS_None
		, const EClassFlags InRestrictedClassFlags = CLASS_None);

	bool HasValidFilterData() const;

	FName GetFilterId() const;

	FText GetDisplayName() const;

	FText GetTooltipText() const;

	const FSlateBrush* GetIconBrush() const;

	ENavigationToolFilterMode GetFilterMode() const;

	bool IsEnabledByDefault() const;

	TSharedPtr<FUICommandInfo> GetToggleCommand() const;

	void SetOverrideIconColor(const FSlateColor& InNewIconColor);

	bool IsValidItemClass(const UE::SequenceNavigator::FNavigationToolItemTypeId InClassTypeId) const;
	bool IsValidObjectClass(const UClass* const InClass) const;

	void SetFilterText(const FText& InText);

	bool PassesFilterText(const UE::SequenceNavigator::FNavigationToolItemPtr& InItem) const;

private:
	UPROPERTY(EditAnywhere, Category = "Filter")
	FName FilterId = NAME_None;

	TSet<UE::SequenceNavigator::FNavigationToolItemTypeId> ItemClasses;

	UPROPERTY(EditAnywhere, Category = "Filter")
	TArray<TSubclassOf<UObject>> ObjectClasses;

	UPROPERTY(EditAnywhere, Category = "Filter")
	ENavigationToolFilterMode FilterMode = ENavigationToolFilterMode::MatchesType;

	UPROPERTY(EditAnywhere, Category = "Filter|Advanced")
	FText FilterText;

	UPROPERTY(EditAnywhere, Category = "Filter")
	FText DisplayName;

	UPROPERTY(EditAnywhere, Category = "Filter")
	FText TooltipText;

	UPROPERTY(EditAnywhere, Category = "Filter", meta=(EditCondition="bUseOverrideIcon"))
	FSlateBrush OverrideIcon;

	UPROPERTY(EditAnywhere, Category = "Filter", meta=(InlineEditConditionToggle))
	bool bUseOverrideIcon = true;

	UPROPERTY(EditAnywhere, Category = "Filter", meta=(InlineEditConditionToggle))
	bool bEnabledByDefault = true;

	TSharedPtr<FUICommandInfo> ToggleCommand;

	const FSlateBrush* IconBrush = nullptr;

	EClassFlags RequiredClassFlags = CLASS_None;
	EClassFlags RestrictedClassFlags = CLASS_None;
};
