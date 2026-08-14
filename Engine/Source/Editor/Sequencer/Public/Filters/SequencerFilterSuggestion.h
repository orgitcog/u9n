// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "Internationalization/Text.h"

struct FSequencerFilterSuggestion
{
	/** The raw suggestion string that should be used with the search box */
	FString Suggestion;

	/** The user-facing display name of this suggestion */
	FText DisplayName;

	/** The user-facing category name of this suggestion (if any) */
	FText CategoryName;

	/** Describes what this suggestion will do */
	FText Description;

	static FSequencerFilterSuggestion MakeSimpleSuggestion(FString InSuggestionString)
	{
		FSequencerFilterSuggestion Suggestion;
		Suggestion.Suggestion = MoveTemp(InSuggestionString);
		Suggestion.DisplayName = FText::FromString(Suggestion.Suggestion);
		return MoveTemp(Suggestion);
	}
};
