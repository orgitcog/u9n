// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NavigationToolDefines.h"
#include "Templates/UniquePtr.h"
#include "Widgets/SCompoundWidget.h"

namespace UE::SequenceNavigator
{

class INavigationToolView;
class SNavigationToolTreeRow;

class SNavigationToolComment : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNavigationToolComment) {}
	SLATE_END_ARGS()

	virtual ~SNavigationToolComment() override;

	void Construct(const FArguments& InArgs
		, const FNavigationToolItemRef& InItem
		, const TSharedRef<INavigationToolView>& InView
		, const TSharedRef<SNavigationToolTreeRow>& InRowWidget);

private:
	FString GetMetaDataComment() const;

	FText GetCommentText() const;
	void OnCommentTextChanged(const FText& InNewText);
	void OnCommentTextCommitted(const FText& InNewText, const ETextCommit::Type InCommitType);

	FText GetTransactionText() const;

	FNavigationToolItemWeakPtr WeakItem;

	TWeakPtr<INavigationToolView> WeakView;

	TWeakPtr<SNavigationToolTreeRow> WeakRowWidget;
};

} // namespace UE::SequenceNavigator
