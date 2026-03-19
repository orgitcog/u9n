// Copyright Epic Games, Inc.All Rights Reserved.

#pragma once

#include "AssetEditorViewportLayout.h"

/**
 * A utility class that holds all the information required to render ControlRig controls in a separate viewport tab.
 * This also manages the ControlRig shape Actors that represent the controls in the world
 */
class FMetaHumanPerformanceControlRigViewportManager
{
public:

	/** Performs the initializations for things required to drive the control rig viewport */
	FMetaHumanPerformanceControlRigViewportManager();

	/** Sets which control rig to use */
	void SetControlRig(class UControlRig* InControlRig);

	void SetFaceBoardShapeColor(struct FLinearColor InColor);

	/** Updates the control rig shapes based on the current values from the actual control rig used to drive the animation */
	void UpdateControlRigShapes();

	/** Initializes the viewport tab contents to display the preview scene where the control rig shapes will be rendered */
	void InitializeControlRigTabContents(TSharedRef<class SDockTab> InControlRigTab);

private:

	/** A mode manager used in the control rig viewport client */
	TSharedPtr<class FAssetEditorModeManager> EditorModeManager;

	/** The control rig component responsible for rendering the control rig controls */
	TObjectPtr<class UMetaHumanPerformanceControlRigComponent> ControlRigComponent;

	/** The viewport tab content where the control rig viewport is displayed. */
	TSharedPtr<class FEditorViewportTabContent> ViewportTabContent;

	/** The viewport delegate used to initialize the control rig viewport widget. */
	AssetEditorViewportFactoryFunction ViewportDelegate;

	/** The preview scene displayed in the control rig viewport */
	TUniquePtr<class FPreviewScene> PreviewScene;

	/** The viewport client that controls the control rig viewport */
	TSharedPtr<class FMetaHumanPerformanceControlRigViewportClient> ViewportClient;

	FDelegateHandle ControlRigOnExecuteDelegateHandle;
};