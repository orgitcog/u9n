// Copyright Epic Games, Inc. All Rights Reserved.

#include "Filters/Filters/NavigationToolFilterBase.h"
#include "Filters/INavigationToolFilterBar.h"
#include "Filters/ISequencerFilterBar.h"

namespace UE::SequenceNavigator
{

using namespace UE::Sequencer;

FNavigationToolFilter::FNavigationToolFilter(INavigationToolFilterBar& InOutFilterInterface, TSharedPtr<FFilterCategory>&& InCategory)
	: FSequencerFilterBase<FNavigationToolItemPtr>(InOutFilterInterface, MoveTemp(InCategory))
{
}

INavigationToolFilterBar& FNavigationToolFilter::GetFilterInterface() const
{
	return static_cast<INavigationToolFilterBar&>(FilterInterface);
}

} // namespace UE::SequenceNavigator
