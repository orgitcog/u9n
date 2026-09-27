// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NavigationToolDefines.h"
#include "SNavigationToolTime.h"

namespace UE::SequenceNavigator
{

class IInTimeExtension;
class INavigationToolView;
class SNavigationToolTreeRow;

class SNavigationToolInTime : public SNavigationToolTime
{
public:
	SLATE_BEGIN_ARGS(SNavigationToolInTime) {}
	SLATE_END_ARGS()

	virtual ~SNavigationToolInTime() override {}

	void Construct(const FArguments& InArgs
		, const FNavigationToolItemRef& InItem
		, const TSharedRef<INavigationToolView>& InView
		, const TSharedRef<SNavigationToolTreeRow>& InRowWidget);

private:
	IInTimeExtension* GetInTimeExtension() const;

	virtual FName GetStyleName() const override;

	virtual double GetFrameTimeValue() const override;

	virtual void OnFrameTimeValueCommitted(const double InNewValue, const ETextCommit::Type InCommitType) override;

	virtual FText GetTransactionText() const override;
};

} // namespace UE::SequenceNavigator
