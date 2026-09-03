// Copyright Epic Games, Inc. All Rights Reserved.

#include "SNavigationToolLength.h"
#include "ISequencer.h"
#include "Items/NavigationToolItemUtils.h"
#include "Items/NavigationToolSequence.h"
#include "NavigationToolView.h"
#include "Sections/MovieSceneSubSection.h"
#include "Widgets/SNavigationToolTreeRow.h"

#define LOCTEXT_NAMESPACE "SNavigationToolLength"

namespace UE::SequenceNavigator
{

void SNavigationToolLength::Construct(const FArguments& InArgs
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

double SNavigationToolLength::GetFrameTimeValue() const
{
	if (UMovieSceneSubSection* const SubSection = Cast<UMovieSceneSubSection>(GetSequenceItemSubSection(WeakItem.Pin())))
	{
		return SubSection->GetRange().Size<FFrameNumber>().Value;
	}
	return 0.;
}

FText SNavigationToolLength::GetTransactionText() const
{
	return LOCTEXT("SetLengthTransaction", "Set Length");
}

TSharedPtr<INumericTypeInterface<double>> SNavigationToolLength::GetNumericTypeInterface() const
{
	if (const TSharedPtr<ISequencer> Sequencer = GetSequencer())
	{
		return Sequencer->GetNumericTypeInterface(UE::Sequencer::ENumericIntent::Duration);
	}
	return nullptr;
}

} // namespace UE::SequenceNavigator

#undef LOCTEXT_NAMESPACE
