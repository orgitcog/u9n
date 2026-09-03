// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NavigationToolDefines.h"
#include "SNavigationToolTime.h"

namespace UE::SequenceNavigator
{

class INavigationToolView;
class SNavigationToolTreeRow;

class SNavigationToolStartFrameOffset : public SNavigationToolTime
{
public:
	SLATE_BEGIN_ARGS(SNavigationToolStartFrameOffset) {}
	SLATE_END_ARGS()

	virtual ~SNavigationToolStartFrameOffset() override {}

	void Construct(const FArguments& InArgs
		, const FNavigationToolItemRef& InItem
		, const TSharedRef<INavigationToolView>& InView
		, const TSharedRef<SNavigationToolTreeRow>& InRowWidget);

private:
	virtual double GetFrameTimeValue() const override;

	virtual void OnFrameTimeValueCommitted(const double InNewValue, const ETextCommit::Type InCommitType) override;

	virtual FText GetTransactionText() const override;
};

} // namespace UE::SequenceNavigator
