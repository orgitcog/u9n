// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISequencerFilterBar.h"

class FSequencerTrackFilter;
class FSequencerTrackFilter_CustomText;
struct FSequencerFilterData;

class ISequencerTrackFilters : public ISequencerFilterBar
{
public:
	virtual ~ISequencerTrackFilters() override = default;

	SEQUENCER_API virtual bool IsFilterEnabled(TSharedRef<FSequencerTrackFilter> InFilter) const = 0;
	SEQUENCER_API virtual bool SetFilterEnabled(const TSharedRef<FSequencerTrackFilter> InFilter, const bool bInEnabled, const bool bInRequestFilterUpdate = true) = 0;

	SEQUENCER_API virtual bool IsFilterActive(const TSharedRef<FSequencerTrackFilter> InFilter) const = 0;
	SEQUENCER_API virtual bool SetFilterActive(const TSharedRef<FSequencerTrackFilter>& InFilter, const bool bInActive, const bool bInRequestFilterUpdate = true) = 0;

	SEQUENCER_API virtual bool AddCustomTextFilter(const TSharedRef<FSequencerTrackFilter_CustomText>& InFilter, const bool bInAddToConfig) = 0;
	SEQUENCER_API virtual bool RemoveCustomTextFilter(const TSharedRef<FSequencerTrackFilter_CustomText>& InFilter, const bool bInAddToConfig) = 0;

	SEQUENCER_API virtual void HideSelectedTracks() = 0;
	SEQUENCER_API virtual void IsolateSelectedTracks() = 0;

	SEQUENCER_API virtual void ShowOnlyLocationCategoryGroups() = 0;
	SEQUENCER_API virtual void ShowOnlyRotationCategoryGroups() = 0;
	SEQUENCER_API virtual void ShowOnlyScaleCategoryGroups() = 0;

	SEQUENCER_API virtual bool HasSelectedTracks() const = 0;

	SEQUENCER_API virtual FSequencerFilterData& GetFilterData() = 0;
};
