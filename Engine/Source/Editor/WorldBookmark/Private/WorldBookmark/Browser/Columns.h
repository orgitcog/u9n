// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Array.h"
#include "Internationalization/Text.h"
#include "UObject/NameTypes.h"

namespace UE::WorldBookmark::Browser::Columns
{
	struct FColumnDefinition
	{
		FName Id;
		FText DisplayText;
		FText ToolTipText;
	};

	extern const FColumnDefinition Favorite;
	extern const FColumnDefinition RecentlyUsed;
	extern const FColumnDefinition Label;
	extern const FColumnDefinition World;
	extern const FColumnDefinition Category;

	extern const TArray<FName> DefaultHiddenColumns;
}
