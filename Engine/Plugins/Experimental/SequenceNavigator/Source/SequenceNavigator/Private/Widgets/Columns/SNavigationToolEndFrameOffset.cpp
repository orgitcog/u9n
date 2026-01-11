// Copyright Epic Games, Inc. All Rights Reserved.

#include "SNavigationToolEndFrameOffset.h"
#include "Items/NavigationToolItemUtils.h"
#include "Items/NavigationToolSequence.h"
#include "NavigationToolView.h"
#include "Sections/MovieSceneSubSection.h"
#include "Widgets/SNavigationToolTreeRow.h"

#define LOCTEXT_NAMESPACE "SNavigationToolEndFrameOffset"

namespace UE::SequenceNavigator
{

void SNavigationToolEndFrameOffset::Construct(const FArguments& InArgs
	, const FNavigationToolItemRef& InItem
	, const TSharedRef<INavigationToolView>& InView
	, const TSharedRef<SNavigationToolTreeRow>& InRowWidget)
{
	WeakItem = InItem;
	WeakView = InView;
	WeakRowWidget = InRowWidget;

	const FNavigationToolSequence* const SequenceItem = InItem->CastTo<FNavigationToolSequence>();
	if (!SequenceItem || !SequenceItem->GetSubSection())
	{
		return;
	}

	SNavigationToolTime::Construct(SNavigationToolTime::FArguments(), InItem, InView, InRowWidget);
}

double SNavigationToolEndFrameOffset::GetFrameTimeValue() const
{
	if (UMovieSceneSubSection* const SubSection = Cast<UMovieSceneSubSection>(GetSequenceItemSubSection(WeakItem.Pin())))
	{
		return SubSection->Parameters.EndFrameOffset.Value;
	}
	return 0.;
}

void SNavigationToolEndFrameOffset::OnFrameTimeValueCommitted(const double InNewValue, const ETextCommit::Type InCommitType)
{
	UMovieSceneSubSection* const SubSection = Cast<UMovieSceneSubSection>(GetSequenceItemSubSection(WeakItem.Pin()));
	if (!SubSection)
	{
		return;
	}

	if (InNewValue == SubSection->Parameters.EndFrameOffset.Value)
	{
		return;
	}

	const bool bShouldTransact = !UndoTransaction.IsValid() && (InCommitType == ETextCommit::OnEnter);
	const FScopedTransaction Transaction(GetTransactionText(), bShouldTransact);

	SubSection->Modify();
	SubSection->Parameters.EndFrameOffset = static_cast<int32>(InNewValue);
}

FText SNavigationToolEndFrameOffset::GetTransactionText() const
{
	return LOCTEXT("SetEndFrameOffsetTransaction", "Set End Frame Offset");
}

} // namespace UE::SequenceNavigator

#undef LOCTEXT_NAMESPACE
