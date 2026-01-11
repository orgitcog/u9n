// Copyright Epic Games, Inc. All Rights Reserved.

#include "SNavigationToolInTime.h"
#include "ScopedTransaction.h"
#include "Extensions/IInTimeExtension.h"
#include "Items/NavigationToolItemUtils.h"
#include "NavigationToolView.h"
#include "Widgets/SNavigationToolTreeRow.h"

#define LOCTEXT_NAMESPACE "SNavigationToolInTime"

namespace UE::SequenceNavigator
{

void SNavigationToolInTime::Construct(const FArguments& InArgs
	, const FNavigationToolItemRef& InItem
	, const TSharedRef<INavigationToolView>& InView
	, const TSharedRef<SNavigationToolTreeRow>& InRowWidget)
{
	WeakItem = InItem;
	WeakView = InView;
	WeakRowWidget = InRowWidget;

	if (!InItem->IsA<IInTimeExtension>())
	{
		return;
	}

	SNavigationToolTime::Construct(SNavigationToolTime::FArguments(), InItem, InView, InRowWidget);
}

IInTimeExtension* SNavigationToolInTime::GetInTimeExtension() const
{
	return WeakItem.IsValid() ? WeakItem.Pin()->CastTo<IInTimeExtension>() : nullptr;
}

FName SNavigationToolInTime::GetStyleName() const
{
	return TEXT("SpinBox.InTime");
}

double SNavigationToolInTime::GetFrameTimeValue() const
{
	if (const IInTimeExtension* const InTimeExtension = GetInTimeExtension())
	{
		return InTimeExtension->GetInTime().Value;
	}
	return 0;
}

void SNavigationToolInTime::OnFrameTimeValueCommitted(const double InNewValue, const ETextCommit::Type InCommitType)
{
	IInTimeExtension* const InTimeExtension = GetInTimeExtension();
	if (!InTimeExtension)
	{
		return;
	}

	const bool bShouldTransact = !UndoTransaction.IsValid() && (InCommitType == ETextCommit::OnEnter);
	const FScopedTransaction Transaction(GetTransactionText(), bShouldTransact);

	return InTimeExtension->SetInTime(static_cast<int32>(InNewValue));
}

FText SNavigationToolInTime::GetTransactionText() const
{
	return LOCTEXT("SetInTimeTransaction", "Set In Time");
}

} // namespace UE::SequenceNavigator

#undef LOCTEXT_NAMESPACE
