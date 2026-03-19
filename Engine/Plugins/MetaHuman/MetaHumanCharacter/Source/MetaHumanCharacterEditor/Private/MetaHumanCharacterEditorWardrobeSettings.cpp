// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacterEditorWardrobeSettings.h"


FText UMetaHumanCharacterEditorWardrobeSettings::SlotNameToCategoryName(const FName& InSlotName, const FText& InTextFallback) const
{
	if (const FText* FoundText = SlotNameToCategoryNameMap.Find(InSlotName))
	{
		return *FoundText;
	}
	else
	{
		return InTextFallback;
	}
}
