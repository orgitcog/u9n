// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NavigationToolDefines.h"
#include "ScopedTransaction.h"
#include "Templates/UniquePtr.h"
#include "Widgets/Input/NumericTypeInterface.h"
#include "Widgets/SCompoundWidget.h"

class FName;
class FText;
class ISequencer;

namespace UE::SequenceNavigator
{

class INavigationToolView;
class SNavigationToolTreeRow;

class SNavigationToolTime : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNavigationToolTime) {}
	SLATE_END_ARGS()

	virtual ~SNavigationToolTime() override {}

	void Construct(const FArguments& InArgs
		, const FNavigationToolItemRef& InItem
		, const TSharedRef<INavigationToolView>& InView
		, const TSharedRef<SNavigationToolTreeRow>& InRowWidget);

	virtual bool IsReadOnly() const;
	virtual void SetIsReadOnly(const bool bInIsReadOnly);

protected:
	virtual FName GetStyleName() const;

	virtual double GetFrameTimeValue() const = 0;

	virtual void OnFrameTimeValueChanged(const double InNewValue);
	virtual void OnFrameTimeValueCommitted(const double InNewValue, const ETextCommit::Type InCommitType);

	virtual void OnBeginSliderMovement();
	virtual void OnEndSliderMovement(const double InNewValue);

	virtual TSharedPtr<INumericTypeInterface<double>> GetNumericTypeInterface() const;

	virtual double GetDisplayRateDeltaFrameCount() const;

	virtual FText GetTransactionText() const = 0;

	TSharedPtr<ISequencer> GetSequencer() const;

	FNavigationToolItemWeakPtr WeakItem;

	TWeakPtr<INavigationToolView> WeakView;

	TWeakPtr<SNavigationToolTreeRow> WeakRowWidget;

	TUniquePtr<FScopedTransaction> UndoTransaction;

	bool bIsReadOnly = false;
};

} // namespace UE::SequenceNavigator
