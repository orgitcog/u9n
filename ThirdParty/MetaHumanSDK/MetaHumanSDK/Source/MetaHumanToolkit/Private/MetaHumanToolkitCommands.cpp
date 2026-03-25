// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanToolkitCommands.h"
#include "MetaHumanToolkitStyle.h"

#define LOCTEXT_NAMESPACE "MetaHumanToolkitCommands"

FMetaHumanToolkitCommands::FMetaHumanToolkitCommands()
	: TCommands<FMetaHumanToolkitCommands>(TEXT("MetaHuman Toolkit"),
										   LOCTEXT("MetaHumanToolkitCommandsContext", "MetaHuman Toolkit Context"),
										   NAME_None,
										   FMetaHumanToolkitStyle::Get().GetStyleSetName())
{}

void FMetaHumanToolkitCommands::RegisterCommands()
{
	UI_COMMAND(ToggleSingleViewToA, "A", "View A parameters", EUserInterfaceActionType::ToggleButton, FInputChord(EKeys::One));
	UI_COMMAND(ToggleSingleViewToB, "B", "View B parameters", EUserInterfaceActionType::ToggleButton, FInputChord(EKeys::Two));

	// View mix
	UI_COMMAND(ViewMixToSingle, "Single", "View Mix Mode: Single\nToggle A and B views using buttons on the left\nTracking curves can be viewed only in this mode", EUserInterfaceActionType::RadioButton, FInputChord(EKeys::Three));
	UI_COMMAND(ViewMixToWipe, "Wipe", "View Mix Mode: Wipe\nWipe A and B views\nTracking curves can be viewed only in Single View Mix Mode", EUserInterfaceActionType::RadioButton, FInputChord(EKeys::Four));
	UI_COMMAND(ViewMixToDual, "Dual", "View Mix Mode: Dual\nShow A and B views side by side\nTracking curves can be viewed only in Single View Mix Mode", EUserInterfaceActionType::RadioButton, FInputChord(EKeys::Five));

	UI_COMMAND(ToggleRGBChannel, "Footage", "Toggle Footage visibility", EUserInterfaceActionType::ToggleButton, FInputChord{ EKeys::R });
	UI_COMMAND(ToggleCurves, "Curves", "Toggle the visibility of landmark curves", EUserInterfaceActionType::ToggleButton, FInputChord{EKeys::Six});
	UI_COMMAND(ToggleControlVertices, "Control Vertices", "Toggle the visibility of landmark control vertices", EUserInterfaceActionType::ToggleButton, FInputChord{ EKeys::Seven });
	UI_COMMAND(ToggleDepthMesh, "Depth Mesh", "Toggle the display of the depth mesh in this view", EUserInterfaceActionType::ToggleButton, FInputChord{ EKeys::Zero });
	UI_COMMAND(ToggleUndistortion, "Undistort", "Toggle the display of undistorted footage", EUserInterfaceActionType::ToggleButton, FInputChord{ EKeys::U });
}

#undef LOCTEXT_NAMESPACE