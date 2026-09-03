// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Filters/ISequencerTextFilterExpressionContext.h"
#include "Filters/INavigationToolFilterBar.h"
#include "Filters/SequencerTrackFilterBase.h"
#include "NavigationToolDefines.h"

class UMovieScene;
class UMovieSceneSequence;

namespace UE::SequenceNavigator
{

class FNavigationToolFilterTextExpressionContext : public ISequencerTextFilterExpressionContext
{ 
public:
	SEQUENCENAVIGATOR_API FNavigationToolFilterTextExpressionContext(INavigationToolFilterBar& InFilterInterface);

	void SetFilterItem(const FNavigationToolItemPtr& InFilterItem);

	//~ Begin ITextFilterExpressionContext

	SEQUENCENAVIGATOR_API virtual bool TestBasicStringExpression(const FTextFilterString& InValue
		, const ETextFilterTextComparisonMode InTextComparisonMode) const override;

	SEQUENCENAVIGATOR_API virtual bool TestComplexExpression(const FName& InKey
		, const FTextFilterString& InValue
		, const ETextFilterComparisonOperation InComparisonOperation
		, const ETextFilterTextComparisonMode InTextComparisonMode) const override;

	//~ End ITextFilterExpressionContext

protected:
	SEQUENCENAVIGATOR_API UMovieSceneSequence* GetFocusedMovieSceneSequence() const;
	SEQUENCENAVIGATOR_API UMovieScene* GetFocusedGetMovieScene() const;

	INavigationToolFilterBar& FilterInterface;

	FNavigationToolItemWeakPtr WeakFilterItem;
};

} // namespace UE::SequenceNavigator
