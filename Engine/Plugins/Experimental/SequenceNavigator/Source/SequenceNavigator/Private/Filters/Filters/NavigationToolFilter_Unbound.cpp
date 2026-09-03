// Copyright Epic Games, Inc. All Rights Reserved.

#include "Filters/Filters/NavigationToolFilter_Unbound.h"
#include "Filters/NavigationToolFilterCommands.h"
#include "Items/NavigationToolBinding.h"

#define LOCTEXT_NAMESPACE "NavigationToolFilter_Unbound"

namespace UE::SequenceNavigator
{

using namespace UE::Sequencer;

FNavigationToolFilter_Unbound::FNavigationToolFilter_Unbound(INavigationToolFilterBar& InFilterInterface, TSharedPtr<FFilterCategory> InCategory)
	: FNavigationToolFilter(InFilterInterface, MoveTemp(InCategory))
{
}

FText FNavigationToolFilter_Unbound::GetDefaultToolTipText() const
{
	return LOCTEXT("NavigationToolFilter_UnboundToolTip", "Show only sequences with Unbound tracks");
}

TSharedPtr<FUICommandInfo> FNavigationToolFilter_Unbound::GetToggleCommand() const
{
	return FNavigationToolFilterCommands::Get().ToggleFilter_Unbound;
}

FText FNavigationToolFilter_Unbound::GetDisplayName() const
{
	return LOCTEXT("NavigationToolFilter_Unbound", "Unbound");
}

FSlateIcon FNavigationToolFilter_Unbound::GetIcon() const
{
	return FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("BTEditor.Graph.BTNode.Decorator.DoesPathExist.Icon"));
}

FString FNavigationToolFilter_Unbound::GetName() const
{
	return StaticName();
}

bool FNavigationToolFilter_Unbound::PassesFilter(FNavigationToolFilterType InItem) const
{
	FNavigationToolBinding* const BindingItem = InItem->CastTo<FNavigationToolBinding>();
	if (!BindingItem)
	{
		return false;
	}

	const UObject* const BoundObject = BindingItem->GetCachedBoundObject();
	return !BoundObject;
}

} // namespace UE::SequenceNavigator

#undef LOCTEXT_NAMESPACE
