// Copyright Epic Games, Inc. All Rights Reserved.

#include "SNavigationToolOutTime.h"
#include "Items/NavigationToolItemUtils.h"
#include "Items/NavigationToolSequence.h"
#include "NavigationToolView.h"
#include "ScopedTransaction.h"
#include "Widgets/SNavigationToolTreeRow.h"

#define LOCTEXT_NAMESPACE "SNavigationToolOutTime"

namespace UE::SequenceNavigator
{

void SNavigationToolOutTime::Construct(const FArguments& InArgs
	, const FNavigationToolItemRef& InItem
	, const TSharedRef<INavigationToolView>& InView
	, const TSharedRef<SNavigationToolTreeRow>& InRowWidget)
{
	WeakItem = InItem;
	WeakView = InView;
	WeakRowWidget = InRowWidget;

	if (!InItem->IsA<IOutTimeExtension>())
	{
		return;
	}

	SNavigationToolTime::Construct(SNavigationToolTime::FArguments(), InItem, InView, InRowWidget);
}

IOutTimeExtension* SNavigationToolOutTime::GetOutTimeExtension() const
{
	return WeakItem.IsValid() ? WeakItem.Pin()->CastTo<IOutTimeExtension>() : nullptr;
}

FName SNavigationToolOutTime::GetStyleName() const
{
	return TEXT("SpinBox.OutTime");
}

double SNavigationToolOutTime::GetFrameTimeValue() const
{
	if (const IOutTimeExtension* const OutTimeExtension = GetOutTimeExtension())
	{
		return OutTimeExtension->GetOutTime().Value;
	}
	return 0;
}

void SNavigationToolOutTime::OnFrameTimeValueCommitted(const double InNewValue, const ETextCommit::Type InCommitType)
{
	IOutTimeExtension* const OutTimeExtension = GetOutTimeExtension();
	if (!OutTimeExtension)
	{
		return;
	}

	const bool bShouldTransact = !UndoTransaction.IsValid() && (InCommitType == ETextCommit::OnEnter);
	const FScopedTransaction Transaction(GetTransactionText(), bShouldTransact);

	return OutTimeExtension->SetOutTime(static_cast<int32>(InNewValue));
}

FText SNavigationToolOutTime::GetTransactionText() const
{
	return LOCTEXT("SetOutTimeTransaction", "Set Out Time");
}

} // namespace UE::SequenceNavigator

#undef LOCTEXT_NAMESPACE
