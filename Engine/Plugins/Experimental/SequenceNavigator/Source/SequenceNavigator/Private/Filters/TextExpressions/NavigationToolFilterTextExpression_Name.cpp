// Copyright Epic Games, Inc. All Rights Reserved.

#include "NavigationToolFilterTextExpression_Name.h"
#include "Items/INavigationToolItem.h"

#define LOCTEXT_NAMESPACE "NavigationToolFilterTextExpression_Name"

namespace UE::SequenceNavigator
{

using namespace UE::Sequencer;

FNavigationToolFilterTextExpression_Name::FNavigationToolFilterTextExpression_Name(INavigationToolFilterBar& InFilterInterface)
	: FNavigationToolFilterTextExpressionContext(InFilterInterface)
{
}

TSet<FName> FNavigationToolFilterTextExpression_Name::GetKeys() const
{
	return { TEXT("Name") };
}

ESequencerTextFilterValueType FNavigationToolFilterTextExpression_Name::GetValueType() const
{
	return ESequencerTextFilterValueType::String;
}

FText FNavigationToolFilterTextExpression_Name::GetDescription() const
{
	return LOCTEXT("ExpressionDescription", "Filter by item name");
}

bool FNavigationToolFilterTextExpression_Name::TestComplexExpression(const FName& InKey
	, const FTextFilterString& InValue
	, const ETextFilterComparisonOperation InComparisonOperation
	, const ETextFilterTextComparisonMode InTextComparisonMode) const
{
	if (!FNavigationToolFilterTextExpressionContext::TestComplexExpression(InKey, InValue, InComparisonOperation, InTextComparisonMode))
	{
		return true;
	}

	if (const TSharedPtr<INavigationToolItem> FilterItem = WeakFilterItem.Pin())
	{
		const FString Label = FilterItem->GetDisplayName().ToString();
		const bool bPassed = TextFilterUtils::TestComplexExpression(Label, InValue, InComparisonOperation, InTextComparisonMode);
		return bPassed;
	}

	return false;
}

} // namespace UE::SequenceNavigator

#undef LOCTEXT_NAMESPACE
