// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/SlateIMManager.h"
#include "SlateIM.h"

namespace SlateIM
{
	bool IsHovered()
	{
		if (TSharedPtr<SWidget> ChildWidget = FSlateIMManager::Get().GetCurrentChildAsWidget())
		{
			return ChildWidget->IsHovered();
		}

		return false;
	}

	bool IsFocused(EFocusDepth Depth)
	{
		if (TSharedPtr<SWidget> ChildWidget = FSlateIMManager::Get().GetCurrentChildAsWidget())
		{
			switch (Depth)
			{
			case EFocusDepth::SelfOnly:
				return ChildWidget->HasAnyUserFocus().IsSet();
			case EFocusDepth::IncludingDescendants:
				return ChildWidget->HasAnyUserFocusOrFocusedDescendants();
			default:
				checkNoEntry();
				break;
			}
		}

		return false;
	}
}
