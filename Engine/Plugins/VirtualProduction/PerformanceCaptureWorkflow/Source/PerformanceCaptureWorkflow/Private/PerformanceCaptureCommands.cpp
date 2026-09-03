// Copyright Epic Games, Inc. All Rights Reserved.

#include "PerformanceCaptureCommands.h"

#define LOCTEXT_NAMESPACE "FPerformanceCaptureModule"

void FPerformanceCaptureCommands::RegisterCommands()
{
	UI_COMMAND(OpenPluginWindow, "Mocap Manager", "Open Mocap Manager", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE