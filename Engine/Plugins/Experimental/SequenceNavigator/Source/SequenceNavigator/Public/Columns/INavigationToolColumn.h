// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NavigationToolDefines.h"
#include "NavigationToolItemType.h"
#include "Widgets/Views/SHeaderRow.h"

namespace UE::SequenceNavigator
{

class INavigationToolView;
class SNavigationToolTreeRow;

class INavigationToolColumn : public INavigationToolItemTypeCastable, public TSharedFromThis<INavigationToolColumn>
{
public:
	virtual FName GetColumnId() const { return GetTypeId().ToName(); }

	virtual FText GetColumnDisplayNameText() const = 0;

	virtual const FSlateBrush* GetIconBrush() const { return nullptr; }

	virtual SHeaderRow::FColumn::FArguments ConstructHeaderRowColumn(const TSharedRef<INavigationToolView>& InToolView, const float InFillSize) = 0;

	virtual float GetFillWidth() const { return 0.f; }

	/*
	 * Determines whether the Column should be Showing by Default while still be able to toggle it on/off.
	 * Used when calling SHeaderRow::SetShowGeneratedColumn (requires ShouldGenerateWidget to not be set).
	 */
	virtual bool ShouldShowColumnByDefault() const { return false; }

	virtual bool CanHideColumn(const FName InColumnId) const { return true; }

	virtual TSharedRef<SWidget> ConstructRowWidget(const FNavigationToolItemRef& InItem
		, const TSharedRef<INavigationToolView>& InView
		, const TSharedRef<SNavigationToolTreeRow>& InRow) = 0;

	virtual void Tick(const float InDeltaTime) {}
};

} // namespace UE::SequenceNavigator
