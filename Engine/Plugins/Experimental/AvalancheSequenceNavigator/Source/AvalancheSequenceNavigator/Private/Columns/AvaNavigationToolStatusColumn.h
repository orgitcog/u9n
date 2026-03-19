// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Columns/NavigationToolColumn.h"

namespace UE::SequenceNavigator
{

class FAvaNavigationToolStatusColumn : public FNavigationToolColumn
{
public:
	UE_NAVIGATIONTOOL_INHERITS_WITH_SUPER(FAvaNavigationToolStatusColumn, FNavigationToolColumn);

	static FName StaticColumnId() { return TEXT("Status"); }

protected:
	//~ Begin INavigationToolColumn
	virtual FName GetColumnId() const override { return StaticColumnId(); }
	virtual FText GetColumnDisplayNameText() const override;
	virtual const FSlateBrush* GetIconBrush() const override;
	virtual bool ShouldShowColumnByDefault() const override { return false; }
	virtual float GetFillWidth() const override { return 10.f; }
	virtual SHeaderRow::FColumn::FArguments ConstructHeaderRowColumn(const TSharedRef<INavigationToolView>& InToolView, const float InFillSize) override;
	virtual TSharedRef<SWidget> ConstructRowWidget(const FNavigationToolItemRef& InItem
		, const TSharedRef<INavigationToolView>& InView
		, const TSharedRef<SNavigationToolTreeRow>& InRow) override;
	//~ End INavigationToolColumn
};

} // namespace UE::SequenceNavigator
