// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataHierarchyEditorCommands.h"

#define LOCTEXT_NAMESPACE "DataHierarchyEditorCommands"

void FDataHierarchyEditorCommands::RegisterCommands()
{
	UI_COMMAND(FindInHierarchy, "Find in Hierarchy", "Find the element in the hierarchy", EUserInterfaceActionType::Button, FInputChord(EKeys::F));
}

#undef LOCTEXT_NAMESPACE
