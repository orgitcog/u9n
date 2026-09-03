// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseGizmos/TransformProxy.h"
#include "EditorGizmos/TransformGizmo.h"

class FLevelEditorViewportClient;
class UEditorTransformGizmo;
class UInteractiveToolManager;
class UTransformProxy;

/**
 * Listens for key press of a specified modifier key, and if Level Viewport Gizmo is dragging, it duplicates current selection.
 * Currently used with Alt (see UDragToolsBehaviorSource)
 */
class FEditorDuplicateDragSelection
	: public IModifierToggleBehaviorTarget
	, public FGCObject
{
public:
	FEditorDuplicateDragSelection(UInteractiveToolManager* InToolManager);

	virtual ~FEditorDuplicateDragSelection() override;

protected:
	//~Begin IModifierToggleBehaviorTarget
	virtual void OnUpdateModifierState(int InModifierID, bool bInIsOn) override;
	virtual void OnForceEndCapture() override;
	//~End IModifierToggleBehaviorTarget

	//~Begin FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& InCollector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FEditorDuplicateDragSelection");
	}
	//~End FGCObject

	void OnGizmoMovementBegin(UTransformProxy* InTransformProxy);
	void OnGizmoMovementEnd(UTransformProxy* InTransformProxy);
	void OnGizmoTransformChanged(UTransformProxy* InTransformProxy, UE::Math::TTransform<double> InTransform);

	void Initialize();
	void Reset();

	void OnGizmoCreatedDelegate(UTransformGizmo* InTransformGizmo);
	void OnUsesNewTRSGizmosChanged(bool bInUseNewTRSGizmos);

	/**
	 * Duplicate the current selection
	 * @param bInSelectNewElements if newly created elements should be selected right after duplication
	 */
	void DuplicateSelection(bool bInSelectNewElements) const;

private:
	TObjectPtr<UInteractiveToolManager> ToolManager;
	TObjectPtr<UEditorTransformGizmo> TransformGizmo;

	bool bGizmoIsDragged = false;
	bool bModifierKeyIsPressed = false;
	bool bGizmoTransformHasChanged = false;
};
