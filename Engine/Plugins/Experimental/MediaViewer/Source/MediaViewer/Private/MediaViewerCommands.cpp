// Copyright Epic Games, Inc. All Rights Reserved.

#include "MediaViewerCommands.h"

#include "Framework/Commands/InputChord.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "MediaViewerCommands"

namespace UE::MediaViewer::Private
{

FMediaViewerCommands::FMediaViewerCommands()
	: TCommands("MediaViewer", LOCTEXT("ContextDescription", "Media Viewer"), NAME_None, FAppStyle::Get().GetStyleSetName())
{
}

void FMediaViewerCommands::RegisterCommands()
{
	UI_COMMAND(
		ToggleOverlay, 
		"Show Overlays", 
		"Toggles the visibility of the status bar and any custom overlays.", 
		EUserInterfaceActionType::ToggleButton,
		FInputChord(EKeys::O)
	);

	UI_COMMAND(
		MoveLeft,
		"Move Left",
		"Moves the camera focus point to the left.",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::A),
		FInputChord(EKeys::Left)
	);

	UI_COMMAND(
		MoveRight,
		"Move Right",
		"Moves the camera focus point to the right.",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::D),
		FInputChord(EKeys::Right)
	);

	UI_COMMAND(
		MoveUp,
		"Move Up",
		"Moves the camera focus point to the up.",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::W),
		FInputChord(EKeys::Up)
	);

	UI_COMMAND(
		MoveDown,
		"Move Down",
		"Moves the camera focus point to the down.",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::S),
		FInputChord(EKeys::Down)
	);

	UI_COMMAND(
		MoveForward,
		"Move Forward",
		"Moves the camera focus point to the forward.",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::Q),
		FInputChord(EKeys::PageUp)
	);

	UI_COMMAND(
		MoveBackward,
		"Move Backward",
		"Moves the camera focus point to the backward.",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::E),
		FInputChord(EKeys::PageDown)
	);

	UI_COMMAND(
		RotatePlusYaw,
		"Rotate +Yaw",
		"Rotates the camera around the Z axis.",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::NumPadSix)
	);

	UI_COMMAND(
		RotateMinusYaw,
		"Rotate -Yaw",
		"Rotates the camera around the Z axis.",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::NumPadFour)
	);

	UI_COMMAND(
		RotatePlusPitch,
		"Rotate +Pitch",
		"Rotates the camera around the X axis.",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::NumPadEight)
	);

	UI_COMMAND(
		RotateMinusPitch,
		"Rotate -Pitch",
		"Rotates the camera around the X axis.",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::NumPadTwo)
	);

	UI_COMMAND(
		RotatePlusRoll,
		"Rotate +Roll",
		"Rotates the camera around the Y axis.",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::NumPadThree)
	);

	UI_COMMAND(
		RotateMinusRoll,
		"Rotate -Roll",
		"Rotates the camera around the Y axis.",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::NumPadOne)
	);

	UI_COMMAND(
		Scale12, 
		"12.5%", 
		"Set Scale to 12.5%.", 
		EUserInterfaceActionType::RadioButton, 
		FInputChord(EKeys::One)
	);

	UI_COMMAND(
		Scale25,
		"25%",
		"Set Scale to 25%.",
		EUserInterfaceActionType::RadioButton,
		FInputChord(EKeys::Two)
	);

	UI_COMMAND(
		Scale50,
		"50%",
		"Set Scale to 50%.",
		EUserInterfaceActionType::RadioButton,
		FInputChord(EKeys::Three)
	);

	UI_COMMAND(
		Scale100,
		"100%",
		"Set Scale to 100%.",
		EUserInterfaceActionType::RadioButton,
		FInputChord(EKeys::Four)
	);

	UI_COMMAND(
		Scale200,
		"200%",
		"Set Scale to 200%.",
		EUserInterfaceActionType::RadioButton,
		FInputChord(EKeys::Five)
	);

	UI_COMMAND(
		Scale400,
		"400%",
		"Set Scale to 400%.",
		EUserInterfaceActionType::RadioButton,
		FInputChord(EKeys::Six)
	);

	UI_COMMAND(
		Scale800,
		"800%",
		"Set Scale to 800%.",
		EUserInterfaceActionType::RadioButton,
		FInputChord(EKeys::Seven)
	);

	UI_COMMAND(
		ScaleToFit, 
		"Scale to Fit", 
		"Changes the scale of the image so it matches, but does not exceed, the size of the viewport.", 
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::Eight)
	);

	UI_COMMAND(
		CopyTransform,
		"Copy Transform",
		"Copiess the image transform to the other viewer.",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::Insert)
	);

	UI_COMMAND(
		ResetTransform,
		"Reset Transform",
		"Resets the image transform.",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::Home),
		FInputChord(EKeys::NumPadZero)
	);

	UI_COMMAND(
		ResetAllTransforms,
		"Reset All Transforms",
		"Resets all the image transforms.",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::Delete),
		FInputChord(EKeys::Decimal)
	);

	UI_COMMAND(
		ToggleLockedTransform,
		"Sync Transforms",
		"Syncs the transforms of all images so that changes made to one are made to all of them.",
		EUserInterfaceActionType::ToggleButton,
		FInputChord(EKeys::L)
	);

	UI_COMMAND(
		MipMinus, 
		"Lower Mip", 
		"Show next lower mip.", 
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::Hyphen)
	);

	UI_COMMAND(
		MipPlus, 
		"Higher Mip", 
		"Show next higher mip.", 
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::Equals)
	);

	UI_COMMAND(
		SecondImageOpacityMinus,
		"Lower B Image Opacity",
		"Make the B Image more translucent.",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::LeftBracket)
	);

	UI_COMMAND(
		SecondImageOpacityPlus,
		"Higher B Image Opacity",
		"Make the B Image more opaque.",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::RightBracket)
	);

	UI_COMMAND(
		CopyColor,
		"Copy Color",
		"Copy the color under the crosshair to the clipboard.",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::C)
	);

	UI_COMMAND(
		AddToLibrary,
		"Add to Library",
		"Save this image to the default group in the Library.",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::S, EModifierKey::Control)
	);

	UI_COMMAND(
		SwapAB,
		"Swap A and B",
		"Swap A and B images and their transforms.",
		EUserInterfaceActionType::Button,
		FInputChord(EKeys::Slash)
	);
}

} // UE::MediaViewer::Private

#undef LOCTEXT_NAMESPACE
