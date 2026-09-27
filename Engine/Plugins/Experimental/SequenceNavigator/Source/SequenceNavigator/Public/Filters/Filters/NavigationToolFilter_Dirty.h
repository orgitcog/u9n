// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Filters/Filters/NavigationToolFilterBase.h"
#include "Filters/NavigationToolFilterData.h"

namespace UE::SequenceNavigator
{

class FNavigationToolFilter_Dirty : public FNavigationToolFilter
{
public:
	static FString StaticName() { return TEXT("Dirty"); }

	FNavigationToolFilter_Dirty(INavigationToolFilterBar& InFilterInterface, TSharedPtr<FFilterCategory> InCategory = nullptr);

	//~ Begin FNavigationToolFilter
	virtual FText GetDefaultToolTipText() const override;
	virtual TSharedPtr<FUICommandInfo> GetToggleCommand() const override;
	//~ End FNavigationToolFilter

	//~ Begin FFilterBase
	virtual FText GetDisplayName() const override;
	virtual FSlateIcon GetIcon() const override;
	//~ End FFilterBase

	//~ Begin IFilter
	virtual FString GetName() const override;
	virtual bool PassesFilter(FNavigationToolFilterType InItem) const override;
	//~ End IFilter

protected:
	static bool IsObjectPackageDirty(const UObject* const InObject);
};

} // namespace UE::SequenceNavigator
