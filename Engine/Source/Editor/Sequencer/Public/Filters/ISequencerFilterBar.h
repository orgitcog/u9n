// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Filters/SBasicFilterBar.h"
#include "Framework/Commands/UICommandList.h"

class FSequencerTrackFilter;
class FSequencerTrackFilter_CustomText;
class FString;
class FText;
class FTextFilterExpressionEvaluator;
class ISequencer;
class ISequencerTextFilterExpressionContext;

/** Generic interface for all Sequencer filter bar implementations. */
class ISequencerFilterBar : public TSharedFromThis<ISequencerFilterBar>
{
public:
	DECLARE_EVENT_TwoParams(ISequencerFilterBar, FOnFilterBarStateChanged, const bool /*InIsVisible*/, const EFilterBarLayout /*InNewLayout*/);

	virtual ~ISequencerFilterBar() = default;

	SEQUENCER_API virtual FName GetIdentifier() const = 0;

	SEQUENCER_API virtual ISequencer& GetSequencer() const = 0;

	SEQUENCER_API virtual TSharedPtr<FUICommandList> GetCommandList() const = 0;

	SEQUENCER_API virtual FString GetTextFilterString() const = 0;
	SEQUENCER_API virtual void SetTextFilterString(const FString& InText) = 0;

	/** Returns true if the current filter bar text filter string contains the specified text expression.
	 * The text expression must have key, operator, and value tokens. */
	SEQUENCER_API virtual bool DoesTextFilterStringContainExpressionPair(const ISequencerTextFilterExpressionContext& InExpression) const = 0;

	SEQUENCER_API virtual bool AreFiltersMuted() const = 0;
	SEQUENCER_API virtual void MuteFilters(const bool bInMute) = 0;

	SEQUENCER_API virtual bool CanResetFilters() const = 0;
	SEQUENCER_API virtual void ResetFilters() = 0;

	SEQUENCER_API virtual bool HasAnyFilterActive(const bool bCheckTextFilter = true
		, const bool bInCheckHideIsolateFilter = true
		, const bool bInCheckCommonFilters = true
		, const bool bInCheckInternalFilters = true
		, const bool bInCheckCustomTextFilters = true) const = 0;

	SEQUENCER_API virtual bool HasAnyFilterEnabled() const = 0;

	SEQUENCER_API virtual void RequestFilterUpdate() = 0;

	SEQUENCER_API virtual void EnableAllFilters(const bool bInEnable, const TArray<FString>& InExceptionFilterNames) = 0;

	SEQUENCER_API virtual void ActivateCommonFilters(const bool bInActivate, const TArray<FString>& InExceptionFilterNames) = 0;

	SEQUENCER_API virtual bool AreAllEnabledFiltersActive(const bool bInActive, const TArray<FString> InExceptionFilterNames) const = 0;
	SEQUENCER_API virtual void ActivateAllEnabledFilters(const bool bInActivate, const TArray<FString> InExceptionFilterNames) = 0;

	SEQUENCER_API virtual bool IsFilterActiveByDisplayName(const FString& InFilterName) const = 0;
	SEQUENCER_API virtual bool IsFilterEnabledByDisplayName(const FString& InFilterName) const = 0;
	SEQUENCER_API virtual bool SetFilterActiveByDisplayName(const FString& InFilterName, const bool bInActive, const bool bInRequestFilterUpdate = true) = 0;
	SEQUENCER_API virtual bool SetFilterEnabledByDisplayName(const FString& InFilterName, const bool bInEnabled, const bool bInRequestFilterUpdate = true) = 0;

	SEQUENCER_API virtual TArray<FText> GetFilterDisplayNames() const = 0;
	SEQUENCER_API virtual TArray<FText> GetCustomTextFilterNames() const = 0;

	SEQUENCER_API virtual int32 GetTotalDisplayNodeCount() const = 0;
	SEQUENCER_API virtual int32 GetFilteredDisplayNodeCount() const  = 0;

	SEQUENCER_API virtual const FTextFilterExpressionEvaluator& GetTextFilterExpressionEvaluator() const = 0;
	SEQUENCER_API virtual TArray<TSharedRef<ISequencerTextFilterExpressionContext>> GetTextFilterExpressionContexts() const = 0;

	SEQUENCER_API virtual void OpenTextExpressionHelp() = 0;
	SEQUENCER_API virtual void SaveCurrentFilterSetAsCustomTextFilter() = 0;
	SEQUENCER_API virtual void CreateNewTextFilter() = 0;

	/** Event called when the visibility or layout state has changed */
	SEQUENCER_API virtual FOnFilterBarStateChanged& OnStateChanged() = 0;

	/** Event called to request that the subscribe update its view since the filters state has changed */
	SEQUENCER_API virtual FSimpleMulticastDelegate& OnRequestUpdate() = 0;

	SEQUENCER_API virtual bool ShouldShowFilterBarWidget() const = 0;

	SEQUENCER_API virtual bool IsFilterBarVisible() const = 0;
	SEQUENCER_API virtual void ToggleFilterBarVisibility() = 0;

	SEQUENCER_API virtual bool IsFilterBarLayout(const EFilterBarLayout InLayout) const = 0;
	SEQUENCER_API virtual void SetToVerticalLayout() = 0;
	SEQUENCER_API virtual void SetToHorizontalLayout() = 0;
	SEQUENCER_API virtual void ToggleFilterBarLayout() = 0;
};
