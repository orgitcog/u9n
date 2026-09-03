// Copyright Epic Games, Inc. All Rights Reserved.

#include "SequencerTextFilterExpression_Name.h"
#include "MVVM/Extensions/IOutlinerExtension.h"
#include "MVVM/ViewModelPtr.h"
#include "MVVM/ViewModels/ViewModelIterators.h"

using namespace UE::Sequencer;

#define LOCTEXT_NAMESPACE "SequencerTextFilterExpression_Name"

FSequencerTextFilterExpression_Name::FSequencerTextFilterExpression_Name(ISequencerTrackFilters& InFilterInterface)
	: FSequencerTextFilterExpressionContext(InFilterInterface)
{
}

TSet<FName> FSequencerTextFilterExpression_Name::GetKeys() const
{
	return { TEXT("Name") };
}

ESequencerTextFilterValueType FSequencerTextFilterExpression_Name::GetValueType() const
{
	return ESequencerTextFilterValueType::String;
}

FText FSequencerTextFilterExpression_Name::GetDescription() const
{
	return LOCTEXT("ExpressionDescription_Name", "Filter by track name");
}

bool FSequencerTextFilterExpression_Name::TestComplexExpression(const FName& InKey
	, const FTextFilterString& InValue
	, const ETextFilterComparisonOperation InComparisonOperation
	, const ETextFilterTextComparisonMode InTextComparisonMode) const
{
	if (!FSequencerTextFilterExpressionContext::TestComplexExpression(InKey, InValue, InComparisonOperation, InTextComparisonMode))
	{
		return true;
	}

	for (const TViewModelPtr<IOutlinerExtension>& OutlinerExtension : FilterItem->GetAncestorsOfType<IOutlinerExtension>())
	{
		const FString Label = OutlinerExtension->GetLabel().ToString();
		if (TextFilterUtils::TestComplexExpression(Label, InValue, InComparisonOperation, InTextComparisonMode))
		{
			return true;
		}
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
