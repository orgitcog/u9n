// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NavigationToolDefines.h"
#include "ScopedTransaction.h"
#include "Templates/UniquePtr.h"
#include "Widgets/SCompoundWidget.h"

namespace UE::SequenceNavigator
{

class INavigationToolView;
class SNavigationToolTreeRow;

class SNavigationToolHBias : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNavigationToolHBias) {}
	SLATE_END_ARGS()

	virtual ~SNavigationToolHBias() override {}

	void Construct(const FArguments& InArgs
		, const FNavigationToolItemRef& InItem
		, const TSharedRef<INavigationToolView>& InView
		, const TSharedRef<SNavigationToolTreeRow>& InRowWidget);

private:
	int32 GetValue() const;
	void OnValueChanged(const int32 InNewValue);
	void OnValueCommitted(const int32 InNewValue, const ETextCommit::Type InCommitType);
	void OnBeginSliderMovement();
	void OnEndSliderMovement(const int32 InNewValue);

	FText GetTransactionText() const;

	FNavigationToolItemWeakPtr WeakItem;

	TWeakPtr<INavigationToolView> WeakView;

	TWeakPtr<SNavigationToolTreeRow> WeakRowWidget;

	TUniquePtr<FScopedTransaction> UndoTransaction;
};

} // namespace UE::SequenceNavigator
