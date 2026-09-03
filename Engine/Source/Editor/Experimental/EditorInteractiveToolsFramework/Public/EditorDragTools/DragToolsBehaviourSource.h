// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorViewportClient.h"
#include "InputBehaviorSet.h"
#include "UObject/Object.h"
#include "DragToolsBehaviourSource.generated.h"

class FEditorDuplicateDragSelection;
class FEditorDragToolBehaviorTarget;
class FEditorMoveCameraWithObject;
class UEditorInteractiveToolsContext;

/**
 * Hosts Drag Tools and needed behaviors.
 * Handles an Input Behavior Set, and keeps track of currently active Drag tool.
 */
UCLASS(Transient, MinimalAPI)
class UDragToolsBehaviorSource final : public UObject, public IInputBehaviorSource
{
	GENERATED_BODY()

public:
	static bool IsViewportChangeToolEnabled();

	DECLARE_MULTICAST_DELEGATE(FOnOnViewportChangeToolToggleDelegate)
	static FOnOnViewportChangeToolToggleDelegate& OnViewportChangeToolActivated();
	static FOnOnViewportChangeToolToggleDelegate& OnViewportChangeToolDeactivated();

	//~ Begin IInputBehaviorSource
	EDITORINTERACTIVETOOLSFRAMEWORK_API virtual const UInputBehaviorSet* GetInputBehaviors() const override { return BehaviorSet; }
	//~ End IClickDragBehaviorTarget

	/**
	 * Register this Input Behavior Source to the InputRouter
	 */
	EDITORINTERACTIVETOOLSFRAMEWORK_API void RegisterSource();

	/**
	 * Deregister this Input Behavior Source from the InputRouter
	 */
	EDITORINTERACTIVETOOLSFRAMEWORK_API void DeregisterSource();

	/**
	 * Creates and stores the drag tools.
	 * Instantiates the BehaviorSet hosting all behaviors required by drag tools.
	 *
	 * @param InInteractiveToolsContext: used to retrieve the viewport client used by drag tools. Can be gathered e.g.
	 * from UEdMode::GetInteractiveToolsContext.
	 */
	EDITORINTERACTIVETOOLSFRAMEWORK_API void Initialize(const UEditorInteractiveToolsContext* InInteractiveToolsContext);

	/**
	 * Renders the active tool on the specified View/Canvas
	 */
	EDITORINTERACTIVETOOLSFRAMEWORK_API void RenderTools(const FSceneView* InSceneView, FCanvas* InCanvas) const;

private:
	void OnActivateTool(EDragTool::Type InDragToolType);
	void OnDeactivateTool(EDragTool::Type InDragToolType);

	void ActivateViewportChangeTool();
	void DeactivateViewportChangeTool();

	/**
	 * Returns the drag tool currently being used, if any
	 */
	FEditorDragToolBehaviorTarget* GetActiveTool() const;

	FEditorViewportClient* GetEditorViewportClient() const;

	/**
	 * Hosting drag tools behaviors
	 */
	UPROPERTY(Transient)
	TObjectPtr<UInputBehaviorSet> BehaviorSet;

	TWeakObjectPtr<const UEditorInteractiveToolsContext> EditorInteractiveToolsContextWeak;

	/**
	 * Available Drag Tools, access by using their type (EDragTool::Type)
	 */
	TMap<EDragTool::Type, TSharedPtr<FEditorDragToolBehaviorTarget>> DragTools;

	/**
	 * Duplicate dragged selection tool
	 */
	TSharedPtr<FEditorDuplicateDragSelection> DuplicateDragSelection;

	/**
	 * Move camera together with dragged selection
	 */
	TSharedPtr<FEditorMoveCameraWithObject> MoveCameraWithObject;

	/**
	 * Which type of Drag tool is currently active
	 */
	int ActiveToolType = -1;
};
