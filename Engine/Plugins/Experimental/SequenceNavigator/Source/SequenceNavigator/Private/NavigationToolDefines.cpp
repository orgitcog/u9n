// Copyright Epic Games, Inc. All Rights Reserved.

#include "NavigationToolDefines.h"
#include "Menus/NavigationToolItemContextMenu.h"
#include "Menus/NavigationToolToolbarMenu.h"

namespace UE::SequenceNavigator
{

FName GetToolBarMenuName()
{
	return FNavigationToolToolbarMenu::GetMenuName();
}

FName GetItemContextMenuName()
{
	return FNavigationToolItemContextMenu::GetMenuName();
}

} // namespace UE::SequenceNavigator
