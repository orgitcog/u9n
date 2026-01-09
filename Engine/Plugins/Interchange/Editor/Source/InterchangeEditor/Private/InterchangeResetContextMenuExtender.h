// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Misc/CoreDelegates.h"

class FInterchangeResetContextMenuExtender
{
public:
	static void SetupLevelEditorContextMenuExtender();
	static void RemoveLevelEditorContextMenuExtender();

private:
	static FDelegateHandle LevelEditorExtenderDelegateHandle;
};
