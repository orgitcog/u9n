// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneStateEditorUtils.h"
#include "UObject/Class.h"

namespace UE::SceneState::Editor
{

FText GetStructTooltip(const UStruct& InStruct)
{
	return InStruct.GetMetaDataText(TEXT("Tooltip"), TEXT("UObjectToolTips"), InStruct.GetFullGroupName(/*bStartWithOuter*/false));
}

} // UE::SceneState::Editor
