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
 * Currently used with Shift (see UDragToolsBehaviorSource)
 */
class EDITORINTERACTIVETOOLSFRAMEWORK_API FEditorMoveCameraWithObject
	: public IModifierToggleBehaviorTarget
	, public FGCObject
{
public:
	FEditorMoveCameraWithObject(UInteractiveToolManager* InToolManager);

	virtual ~FEditorMoveCameraWithObject() override;

protected:
	//~Begin IModifierToggleBehaviorTarget
	virtual void OnUpdateModifierState(int InModifierID, bool bInIsOn) override;
	//~End IModifierToggleBehaviorTarget

	//~Begin FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& InCollector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FEditorMoveCameraWithObject");
	}
	//~End FGCObject

	void OnGizmoMovementBegin(UTransformProxy* InTransformProxy);
	void OnGizmoMovementEnd(UTransformProxy* InTransformProxy);
	void OnGizmoTransformChanged(UTransformProxy* InTransformProxy, UE::Math::TTransform<double> InTransform);

	void Initialize();
	void Reset();

	void OnGizmoCreatedDelegate(UTransformGizmo* InTransformGizmo);
	void OnUsesNewTRSGizmosChanged(bool bInUseNewTRSGizmos);

private:
	TObjectPtr<UInteractiveToolManager> ToolManager;
	TObjectPtr<UEditorTransformGizmo> TransformGizmo;

	bool bGizmoIsDragged = false;
	bool bModifierKeyIsPressed = false;

	FDelegateHandle OnBeginPivotEditDelegate;
	FDelegateHandle OnEndPivotEditDelegate;
	FDelegateHandle OnTransformChangedDelegate;
};
