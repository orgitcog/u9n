// Copyright Epic Games, Inc.All Rights Reserved.

#pragma once

#include "EditorViewportClient.h"
#include "BaseBehaviors/BehaviorTargetInterfaces.h"
#include "BaseBehaviors/ClickDragBehavior.h"
#include "BaseBehaviors/MouseWheelBehavior.h"
#include "InputBehaviorSet.h"

/////////////////////////////////////////////////////
// FMetaHumanPerformance2DScrollBehaviorTarget

/**
 * Allows click-dragging to move the camera in the XZ plane.
 * Heavily based on FUVEditor2DScrollBehaviorTarget but not public on UE5.1
 */
class FMetaHumanPerformance2DScrollBehaviorTarget
	: public IClickDragBehaviorTarget
{
public:

	FMetaHumanPerformance2DScrollBehaviorTarget(class FMetaHumanPerformanceControlRigViewportClient* InViewportClient);

	//~Begin IClickDragBehaviorTarget
	virtual FInputRayHit CanBeginClickDragSequence(const FInputDeviceRay& InPressPos) override;
	virtual void OnClickPress(const FInputDeviceRay& InPressPos) override;
	virtual void OnClickDrag(const FInputDeviceRay& InDragPos) override;
	virtual void OnClickRelease(const FInputDeviceRay& InReleasePos) override;
	virtual void OnTerminateDragSequence() override;
	//~End IClickDragBehaviorTarget

protected:
	class FMetaHumanPerformanceControlRigViewportClient* ViewportClient = nullptr;
	FVector OriginalCameraLocation;
	FVector DragStart;
};

/////////////////////////////////////////////////////
// FMetaHumanPerformance2DMouseWheelZoomBehaviorTarget

/**
 * Allows the mouse wheel to move the camera forwards/backwards relative to the XZ plane,
 * in the direction pointed to by the mouse.
 * * Heavily based on FUVEditor2DMouseWheelZoomBehaviorTarget but not public on UE5.1
 */
class FMetaHumanPerformance2DMouseWheelZoomBehaviorTarget
	: public IMouseWheelBehaviorTarget
{
public:

	FMetaHumanPerformance2DMouseWheelZoomBehaviorTarget(class FMetaHumanPerformanceControlRigViewportClient* InViewportClient);

	//~Begin IMouseWheelBehaviorTarget
	virtual FInputRayHit ShouldRespondToMouseWheel(const FInputDeviceRay& InCurrentPos) override;
	virtual void OnMouseWheelScrollUp(const FInputDeviceRay& InCurrentPos) override;
	virtual void OnMouseWheelScrollDown(const FInputDeviceRay& InCurrentPos) override;
	//~End IMouseWheelBehaviorTarget

	/**
	 * @param PercentZoomIn How much to move forward on each mouse wheel forward scroll. For instance,
	 *  passing 20 here will decrease the distance to the XZ plane by 20% each time. The zoom out amount
	 *  will be set in such a way that it undoes the same effect. For instance if 20 was passed here,
	 *  zoom out amount will be 25% since zooming in by 20% and then out by 25% of the result gets you
	 *  back to where you were.
	 */
	void SetZoomAmount(double InPercentZoom);
	void SetZoomLimits(double InZoomInLimit, double InZoomOutLimit);

	void SetCameraFarPlaneWorldY(double InCameraFarPlaneWorldY);
	void SetCameraNearPlaneProportionY(double InCameraFarPlaneProportionY);

protected:
	class FMetaHumanPerformanceControlRigViewportClient* ViewportClient = nullptr;
	double ZoomInProportion;
	double ZoomOutProportion;
	double ZoomInLimit;
	double ZoomOutLimit;
	double CameraFarPlaneWorldY;
	double CameraNearPlaneProportionY;
};

/////////////////////////////////////////////////////
// FMetaHumanPerformanceControlRigViewportClient

/**
 * @brief The viewport client used to display the face board control rig in the performance editor
 *
 * This viewport client implements the IInputBehaviorSource which allows arbitrary camera behaviors to be added to the client.
 * This implementation is heavily based on FUVEditor2DViewportClient.
 */
class FMetaHumanPerformanceControlRigViewportClient
	: public FEditorViewportClient
	, public IInputBehaviorSource
{
public:
	FMetaHumanPerformanceControlRigViewportClient(FEditorModeTools* InModeTools, FPreviewScene* InPreviewScene);

	//~Begin FMetaHumanEditorViewportClient interface
	virtual bool InputKey(const FInputKeyEventArgs& InEventArgs) override;
	virtual bool ShouldOrbitCamera() const override;
	//~End FMetaHumanEditorViewportClient interface

	//~Begin FGCObject
	virtual FString GetReferencerName() const override;
	virtual void AddReferencedObjects(FReferenceCollector& InCollector) override;
	//~End FGCObject

	//~Begin IInputBehaviorSource
	virtual const UInputBehaviorSet* GetInputBehaviors() const override;
	//~End IInputBehaviorSource

private:

	// These get added in AddReferencedObjects for memory management
	TObjectPtr<class UInputBehaviorSet> BehaviorSet;

	// Implements the zoom behavior using the mouse wheel
	TUniquePtr<FMetaHumanPerformance2DMouseWheelZoomBehaviorTarget> ZoomBehaviourTarget;

	// Implements the scroll behavior that allows dragging the camera using the right mouse button
	TUniquePtr<FMetaHumanPerformance2DScrollBehaviorTarget> ScrollBehaviorTarget;
};