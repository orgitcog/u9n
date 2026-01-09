// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NavigationToolDefines.h"
#include "SNavigationToolTime.h"

namespace UE::SequenceNavigator
{

class INavigationToolView;
class SNavigationToolTreeRow;

class SNavigationToolLength : public SNavigationToolTime
{
public:
	SLATE_BEGIN_ARGS(SNavigationToolLength) {}
	SLATE_END_ARGS()

	virtual ~SNavigationToolLength() override {}

	void Construct(const FArguments& InArgs
		, const FNavigationToolItemRef& InItem
		, const TSharedRef<INavigationToolView>& InView
		, const TSharedRef<SNavigationToolTreeRow>& InRowWidget);

private:
	virtual double GetFrameTimeValue() const override;

	virtual FText GetTransactionText() const override;

	virtual TSharedPtr<INumericTypeInterface<double>> GetNumericTypeInterface() const override;
};

} // namespace UE::SequenceNavigator
