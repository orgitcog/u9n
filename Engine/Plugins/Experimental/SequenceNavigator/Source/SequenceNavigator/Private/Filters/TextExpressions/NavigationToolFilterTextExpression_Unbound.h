// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Filters/TextExpressions/NavigationToolFilterTextExpressionContext.h"

namespace UE::SequenceNavigator
{

class INavigationToolFilterBar;

class FNavigationToolFilterTextExpression_Unbound : public FNavigationToolFilterTextExpressionContext
{
public:
	FNavigationToolFilterTextExpression_Unbound(INavigationToolFilterBar& InFilterInterface);

	//~ Begin ISequencerTextFilterExpressionContext
	virtual TSet<FName> GetKeys() const override;
	virtual ESequencerTextFilterValueType GetValueType() const override;
	virtual FText GetDescription() const override;
	//~ End ISequencerTextFilterExpressionContext

	//~ Begin ITextFilterExpressionContext
	virtual bool TestComplexExpression(const FName& InKey
		, const FTextFilterString& InValue
		, const ETextFilterComparisonOperation InComparisonOperation
		, const ETextFilterTextComparisonMode InTextComparisonMode) const override;
	//~ End ITextFilterExpressionContext
};

} // namespace UE::SequenceNavigator
