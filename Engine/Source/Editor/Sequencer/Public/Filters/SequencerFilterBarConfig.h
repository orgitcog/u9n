// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Filters/SBasicFilterBar.h"
#include "SequencerFilterBarConfig.generated.h"

USTRUCT()
struct FSequencerFilterSet
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FString Label;

	/** Enabled and active states of common filters. Enabled if in the map. Active if the value of the key is true. */
	UPROPERTY()
	TMap<FString, bool> EnabledStates;

	UPROPERTY()
	FString TextFilterString;
};

USTRUCT()
struct FSequencerFilterBarConfig
{
	GENERATED_BODY()

public:
	/** Common Filters */

	SEQUENCER_API bool IsFilterEnabled(const FString& InFilterName) const;
	SEQUENCER_API bool SetFilterEnabled(const FString& InFilterName, const bool bInActive);

	SEQUENCER_API bool IsFilterActive(const FString& InFilterName) const;
	SEQUENCER_API bool SetFilterActive(const FString& InFilterName, const bool bInActive);

	const FSequencerFilterSet& GetCommonActiveSet() const;

	/** Custom Text Filters */

	SEQUENCER_API TArray<FCustomTextFilterData>& GetCustomTextFilters();
	SEQUENCER_API bool HasCustomTextFilter(const FString& InFilterName) const;
	SEQUENCER_API FCustomTextFilterData* FindCustomTextFilter(const FString& InFilterName);
	SEQUENCER_API bool AddCustomTextFilter(FCustomTextFilterData InFilterData);
	SEQUENCER_API bool RemoveCustomTextFilter(const FString& InFilterName);

	/** Filter Bar Layout */

	SEQUENCER_API EFilterBarLayout GetFilterBarLayout() const;
	SEQUENCER_API void SetFilterBarLayout(const EFilterBarLayout InLayout);

protected:
	/** The currently active set of common and custom text filters that should be restored on editor load */
	UPROPERTY()
	FSequencerFilterSet ActiveFilters;

	/** User created custom text filters */
	UPROPERTY()
	TArray<FCustomTextFilterData> CustomTextFilters;

	/** The layout style for the filter bar widget */
	UPROPERTY()
	EFilterBarLayout FilterBarLayout = EFilterBarLayout::Vertical;
};
