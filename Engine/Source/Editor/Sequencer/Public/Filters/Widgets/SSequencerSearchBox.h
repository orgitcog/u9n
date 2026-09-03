// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Filters/SequencerFilterSuggestion.h"
#include "Filters/SFilterSearchBox.h"
#include "Framework/Text/TextLayout.h"
#include "Misc/ExpressionParserTypesFwd.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FSequencerFilterSuggestionListEntryBase;
class FTextFilterExpressionEvaluator;
class ISequencerFilterBar;
class ITableRow;
class SMenuAnchor;
class STableViewBase;
struct FFocusEvent;
struct FGeometry;
struct FKeyEvent;

/** A delegate for a callback to filter the given suggestion list, to allow custom filtering behavior */
DECLARE_DELEGATE_ThreeParams(FOnSequencerSearchBoxSuggestionFilter, const FText& /*InSearchText*/, TArray<FSequencerFilterSuggestion>& /*InPossibleSuggestions*/, FText& /*InSuggestionHighlightText*/);

/** A delegate for a callback when a suggestion entry is chosen during a search, to allow custom compositing behavior of the suggestion into the search text */
DECLARE_DELEGATE_RetVal_TwoParams(FText, FOnSequencerSearchBoxSuggestionChosen, const FText& /*InSearchText*/, const FString& /*InSuggestion*/);

/**
 * A wrapper widget around SFilterSearchBox to provide filter text expression suggestions in a dropdown menu.
 */
class SEQUENCER_API SSequencerSearchBox : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSequencerSearchBox)
		: _SuggestionListPlacement(MenuPlacement_BelowAnchor)
		, _PossibleSuggestions(TArray<FSequencerFilterSuggestion>())
		, _DelayChangeNotificationsWhileTyping(true)
		, _MustMatchPossibleSuggestions(false)
		, _ShowSearchHistory(true)
	{}

		/** Where to place the suggestion list */
		SLATE_ARGUMENT(EMenuPlacement, SuggestionListPlacement)

		/** Invoked whenever the text changes */
		SLATE_EVENT(FOnTextChanged, OnTextChanged)

		/** Invoked whenever the text is committed (e.g. user presses enter) */
		SLATE_EVENT(FOnTextCommitted, OnTextCommitted)

		/** Initial text to display for the search text */
		SLATE_ATTRIBUTE(FText, InitialText)

		/** Hint text to display for the search text when there is no value */
		SLATE_ATTRIBUTE(FText, HintText)

		/** All possible suggestions for the search text */
		SLATE_ATTRIBUTE(TArray<FSequencerFilterSuggestion>, PossibleSuggestions)

		/** Whether the SearchBox should delay notifying listeners of text changed events until the user is done typing */
		SLATE_ATTRIBUTE(bool, DelayChangeNotificationsWhileTyping)

		/** Whether the SearchBox allows entries that don't match the possible suggestions */
		SLATE_ATTRIBUTE(bool, MustMatchPossibleSuggestions)

		/** Callback to filter the given suggestion list, to allow custom filtering behavior */
		SLATE_EVENT(FOnSequencerSearchBoxSuggestionFilter, OnSuggestionFilter)

		/** Callback when a suggestion entry is chosen during an asset search, to allow custom compositing behavior of the suggestion into the search text */
		SLATE_EVENT(FOnSequencerSearchBoxSuggestionChosen, OnSuggestionChosen)

		/** Callback delegate to have first chance handling of the OnKeyDown event */
		SLATE_EVENT(FOnKeyDown, OnKeyDownHandler)
	
		/** Whether we should show a dropdown containing the last few searches */
		SLATE_ATTRIBUTE(bool, ShowSearchHistory)

		/** Handler for when the + Button next to a search is clicked */
		SLATE_EVENT(SFilterSearchBox::FOnSaveSearchClicked, OnSaveSearchClicked)

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, const TWeakPtr<ISequencerFilterBar>& InWeakFilterBar);

	/** Get the text string currently being edited */
	FText GetText() const;

	/** Sets the text string currently being edited */
	void SetText(const TAttribute<FText>& InNewText);

	/** Set or clear the current error reporting information for this search box */
	void SetError(const FText& InError);
	void SetError(const FString& InError);

	/** Show a + button next to the current search and set the handler for when that is clicked */
	void SetOnSaveSearchHandler(SFilterSearchBox::FOnSaveSearchClicked InOnSaveSearchHandler);

	//~ Begin SWidget
	virtual FReply OnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual bool SupportsKeyboardFocus() const override;
	virtual bool HasKeyboardFocus() const override;
	virtual FReply OnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent) override;
	//~ End SWidget

private:
	struct FParsedExpression
	{
		TOptional<FString> Key;
		TOptional<FString> Value;
		int32 SuggestionIndex = 0;
	};

	void ExtractSearchFilterTerms(const FText& InSearchText, FParsedExpression& OutParsedPair) const;

	TSharedRef<SWidget> GetSuggestionListMenuContent();

	/** First chance handler for key down events to the editable text widget */
	FReply HandleKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent);

	/** Handler for when text in the editable text box changed */
	void HandleTextChanged(const FText& InNewText);

	/** Handler for when text in the editable text box changed */
	void HandleTextCommitted(const FText& InNewText, const ETextCommit::Type InCommitType);

	/** Called by SListView when the selection changes in the suggestion list */
	void OnSelectionChanged(TSharedPtr<FSequencerFilterSuggestionListEntryBase> InNewValue, const ESelectInfo::Type InSelectInfo);

	/** Makes the widget for a suggestion message in the list view */
	TSharedRef<ITableRow> MakeSuggestionListItemWidget(const TSharedPtr<FSequencerFilterSuggestionListEntryBase> InSuggestion, const TSharedRef<STableViewBase>& InOwnerTable);

	/** Gets the text to highlight in the suggestion list */
	FText GetHighlightText() const;

	/** Updates and shows or hides the suggestion list */
	void UpdateSuggestionList();

	/** Sets the focus to the InputText box */
	void FocusEditBox();

	/** Returns the currently selected suggestion */
	TSharedPtr<FSequencerFilterSuggestionListEntryBase> GetSelectedSuggestion() const;

	/** Default implementation of OnSuggestionFilter, if no external implementation is provided */
	void DefaultSuggestionFilterImpl(const FText& InSearchText, TArray<FSequencerFilterSuggestion>& OutPossibleSuggestions, FText& OutSuggestionHighlightText);

	/** Default implementation of OnSuggestionChosen, if no external implementation is provided */
	FText DefaultSuggestionChosenImpl(const FText& InSearchText, const FString& InSuggestion);

	/** Provides default key suggestions, if no suggestions are provided */
	void DefaultKeySuggestions(const FString& InKeyValue, TArray<FSequencerFilterSuggestion>& OutPossibleSuggestions);

	/** Provides default value suggestions, if no suggestions are provided */
	void DefaultValueSuggestions(const FString& InKeyValue, TArray<FSequencerFilterSuggestion>& OutPossibleSuggestions);

	static bool IsOperatorToken(const TExpressionToken<TCHAR>& InToken);
	static bool IsLogicalOperatorToken(const TExpressionToken<TCHAR>& InToken);

	static int32 FindTokenIndex(const TArray<TExpressionToken<TCHAR>>& ExpressionTokens, const int32 InIndexToFind);

	const FTextFilterExpressionEvaluator* GetTextFilterExpressionEvaluator() const;

	TWeakPtr<ISequencerFilterBar> WeakFilterBar;

	TSharedPtr<SFilterSearchBox> SearchBox;

	TSharedPtr<SListView<TSharedPtr<FSequencerFilterSuggestionListEntryBase>>> SuggestionListView;

	TSharedPtr<SMenuAnchor> SuggestionMenuAnchor;

	TArray<TSharedPtr<FSequencerFilterSuggestionListEntryBase>> SuggestionItems;

	/** The state of the text prior to being committed */
	FText PreCommittedText;

	/** The highlight text to use for the suggestions list */
	FText SuggestionHighlightText;

	FOnTextChanged OnTextChanged;
	FOnTextCommitted OnTextCommitted;

	/** Delegate to filter the given suggestion list, to allow custom filtering behavior */
	FOnSequencerSearchBoxSuggestionFilter OnSuggestionFilter;

	/** Delegate when a suggestion entry is chosen during a search, to allow custom compositing behavior of the suggestion into the search text */
	FOnSequencerSearchBoxSuggestionChosen OnSuggestionChosen;

	/** Delegate for first chance handling for key down events */
	FOnKeyDown OnKeyDownHandler;

	/** All possible suggestions for the search text */
	TAttribute<TArray<FSequencerFilterSuggestion>> PossibleSuggestions;

	/** Determines whether the committed text should match a suggestion */
	bool bMustMatchPossibleSuggestions = false;

	bool bDisableOpeningSuggestions = true;

	FTextLocation LastCursorLocation = FTextLocation(0, 0);

	int32 LastChosenCursorOffset = INDEX_NONE;
};
