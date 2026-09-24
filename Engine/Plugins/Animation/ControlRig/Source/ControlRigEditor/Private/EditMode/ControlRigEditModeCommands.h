// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "ControlRigEditorStyle.h"

class FControlRigEditModeCommands : public TCommands<FControlRigEditModeCommands>
{

public:
	FControlRigEditModeCommands() : TCommands<FControlRigEditModeCommands>
	(
		"ControlRigEditMode",
		NSLOCTEXT("Contexts", "RigAnimation", "Rig Animation"),
		NAME_None, // "MainFrame" // @todo Fix this crash
		FControlRigEditorStyle::Get().GetStyleSetName() // Icon Style Set
	)
	{}
	

	/** Toggles hiding all manipulators on active control rig in the viewport */
	TSharedPtr< FUICommandInfo > ToggleManipulators;

	/** Toggles hiding all manipulators belonging to the selected module on active control rig in the viewport */
    TSharedPtr< FUICommandInfo > ToggleModuleManipulators;

	/** Toggles hiding all manipulators on all control rigs in the viewport */
	TSharedPtr< FUICommandInfo > ToggleAllManipulators;

	/** Toggles show controls as an overlay in the viewport */
	TSharedPtr< FUICommandInfo > ToggleControlsAsOverlay;

	/** Invert Transforms and channels to Rest Pose */
	TSharedPtr< FUICommandInfo > InvertTransformsAndChannels;

	/** Invert All Transforms and channels  to Rest Pose */
	TSharedPtr< FUICommandInfo > InvertAllTransformsAndChannels;

	/** Invert Transforms to Rest Pose */
	TSharedPtr< FUICommandInfo > InvertTransforms;

	/** Invert All Transforms to Rest Pose */
	TSharedPtr< FUICommandInfo > InvertAllTransforms;

	/** Set Transforms to Zero */
	TSharedPtr< FUICommandInfo > ZeroTransforms;

	/** Set All Transforms to Zero */
	TSharedPtr< FUICommandInfo > ZeroAllTransforms;

	/** Clear Selection*/
	TSharedPtr< FUICommandInfo > ClearSelection;

	/** Frame selected elements */
	TSharedPtr<FUICommandInfo> FrameSelection;

	/** Increase Shape Size */
	TSharedPtr< FUICommandInfo > IncreaseControlShapeSize;

	/** Decrease Shape Size */
	TSharedPtr< FUICommandInfo > DecreaseControlShapeSize;

	/** Reset Shape Size */
	TSharedPtr< FUICommandInfo > ResetControlShapeSize;

	/** Toggle Shape Transform Edit*/
	TSharedPtr< FUICommandInfo > ToggleControlShapeTransformEdit;

	/** Sets Passthrough Key on selected anim layers */
	TSharedPtr< FUICommandInfo > SetAnimLayerPassthroughKey;

	/** Select mirrored Controls on current selection*/
	TSharedPtr< FUICommandInfo > SelectMirroredControls;

	/** Select mirrored Controls on current selection, keeping current selection*/
	TSharedPtr< FUICommandInfo > AddMirroredControlsToSelection;

	/** Put selected Controls to mirrored Pose*/
	TSharedPtr< FUICommandInfo > MirrorSelectedControls;

	/** Put unselected Controls to mirrored selected Controls*/
	TSharedPtr< FUICommandInfo > MirrorUnselectedControls;

	/** Select all Controls*/
	TSharedPtr< FUICommandInfo > SelectAllControls;

	/** Save a pose of selected Controls*/
	TSharedPtr< FUICommandInfo > SavePose;

	/** Select Controls in saved Pose*/
	TSharedPtr< FUICommandInfo > SelectPose;

	/** Paste saved Pose */
	TSharedPtr< FUICommandInfo > PastePose;

	/** Select Controls in saved Pose*/
	TSharedPtr< FUICommandInfo > SelectMirrorPose;

	/** Paste saved mirror Pose */
	TSharedPtr< FUICommandInfo > PasteMirrorPose;



	/** Opens up the space picker widget */
	TSharedPtr< FUICommandInfo > OpenSpacePickerWidget;

	/**
	 * Initialize commands
	 */
	virtual void RegisterCommands() override;
};
