// Copyright Epic Games, Inc.All Rights Reserved.

#include "MetaHumanPerformanceControlRigViewportClient.h"

#include "EditorModeManager.h"
#include "Tools/EdModeInteractiveToolsContext.h"
#include "CameraController.h"

//////////////////////////////////////////////////////////////////////////
// FMetaHumanPerformance2DScrollBehaviorTarget

FMetaHumanPerformance2DScrollBehaviorTarget::FMetaHumanPerformance2DScrollBehaviorTarget(FMetaHumanPerformanceControlRigViewportClient* InViewportClient)
	: ViewportClient{ InViewportClient }
{
	check(InViewportClient);
}

FInputRayHit FMetaHumanPerformance2DScrollBehaviorTarget::CanBeginClickDragSequence(const FInputDeviceRay& InPressPos)
{
	// Verify that ray is facing the proper direction
	if (InPressPos.WorldRay.Direction.Y * InPressPos.WorldRay.Origin.Y < 0)
	{
		return FInputRayHit{ static_cast<float>(-InPressPos.WorldRay.Origin.Y / InPressPos.WorldRay.Direction.Y) };
	}
	return FInputRayHit();
}

void FMetaHumanPerformance2DScrollBehaviorTarget::OnClickPress(const FInputDeviceRay& InPressPos)
{
	if (ensure(InPressPos.WorldRay.Direction.Y * InPressPos.WorldRay.Origin.Y < 0))
	{
		// Intersect with XZ plane
		const double DistanceToPlane = -InPressPos.WorldRay.Origin.Y / InPressPos.WorldRay.Direction.Y;
		DragStart = FVector(InPressPos.WorldRay.Origin.X + DistanceToPlane * InPressPos.WorldRay.Direction.X,
							0,
							InPressPos.WorldRay.Origin.Z + DistanceToPlane * InPressPos.WorldRay.Direction.Z);

		OriginalCameraLocation = ViewportClient->GetViewLocation();
	}
}


void FMetaHumanPerformance2DScrollBehaviorTarget::OnClickDrag(const FInputDeviceRay& InDragPos)
{
	if (ensure(InDragPos.WorldRay.Direction.Y * InDragPos.WorldRay.Origin.Y < 0))
	{
		// Intersect a ray starting from the original position and using the new
		// ray direction. I.e., pretend the camera is not moving.

		const double DistanceToPlane = -OriginalCameraLocation.Y / InDragPos.WorldRay.Direction.Y;
		const FVector DragEnd = FVector(OriginalCameraLocation.X + DistanceToPlane * InDragPos.WorldRay.Direction.X,
										0,
										OriginalCameraLocation.Z + DistanceToPlane * InDragPos.WorldRay.Direction.Z);

		// We want to make it look like we are sliding the plane such that DragStart
		// ends up on DragEnd. For that, our camera will be moving the opposite direction.
		const FVector CameraDisplacement = DragStart - DragEnd;
		check(CameraDisplacement.Y == 0);
		ViewportClient->SetViewLocation(OriginalCameraLocation + CameraDisplacement);
	}
}

void FMetaHumanPerformance2DScrollBehaviorTarget::OnClickRelease(const FInputDeviceRay& InReleasePos)
{
}

void FMetaHumanPerformance2DScrollBehaviorTarget::OnTerminateDragSequence()
{
}

//////////////////////////////////////////////////////////////////////////
// FMetaHumanPerformance2DMouseWheelZoomBehaviorTarget

FMetaHumanPerformance2DMouseWheelZoomBehaviorTarget::FMetaHumanPerformance2DMouseWheelZoomBehaviorTarget(FMetaHumanPerformanceControlRigViewportClient* InViewportClient)
	: ViewportClient{ InViewportClient }
{
	check(InViewportClient);

	constexpr double DefaultZoomAmount = 20;
	SetZoomAmount(DefaultZoomAmount);
}

FInputRayHit FMetaHumanPerformance2DMouseWheelZoomBehaviorTarget::ShouldRespondToMouseWheel(const FInputDeviceRay& InCurrentPos)
{
	// Always allowed to zoom with mouse wheel
	FInputRayHit InputRayHit;
	InputRayHit.bHit = true;
	return InputRayHit;
}

void FMetaHumanPerformance2DMouseWheelZoomBehaviorTarget::OnMouseWheelScrollUp(const FInputDeviceRay& InCurrentPos)
{
	const FVector OriginalLocation = ViewportClient->GetViewLocation();
	const double DistanceToPlane = -OriginalLocation.Y / InCurrentPos.WorldRay.Direction.Y;

	const FVector NewLocation = OriginalLocation + (ZoomInProportion * DistanceToPlane * InCurrentPos.WorldRay.Direction);

	ViewportClient->OverrideFarClipPlane(static_cast<float>(NewLocation.Y - CameraFarPlaneWorldY));
	ViewportClient->OverrideNearClipPlane(static_cast<float>(NewLocation.Y * (1.0 - CameraNearPlaneProportionY)));

	// Don't zoom in so far that the XY plane lies in front of our near clipping plane, or else everything
	// will suddenly disappear.
	if (NewLocation.Y > ViewportClient->GetNearClipPlane() && NewLocation.Y > ZoomInLimit)
	{
		ViewportClient->SetViewLocation(NewLocation);
	}
}

void FMetaHumanPerformance2DMouseWheelZoomBehaviorTarget::OnMouseWheelScrollDown(const FInputDeviceRay& InCurrentPos)
{
	const FVector OriginalLocation = ViewportClient->GetViewLocation();
	const double DistanceToPlane = -OriginalLocation.Y / InCurrentPos.WorldRay.Direction.Y;
	const FVector NewLocation = OriginalLocation - (ZoomOutProportion * DistanceToPlane * InCurrentPos.WorldRay.Direction);

	ViewportClient->OverrideFarClipPlane(static_cast<float>(NewLocation.Y - CameraFarPlaneWorldY));
	ViewportClient->OverrideNearClipPlane(static_cast<float>(NewLocation.Y * (1.0 - CameraNearPlaneProportionY)));

	if (NewLocation.Y < ZoomOutLimit)
	{
		ViewportClient->SetViewLocation(NewLocation);
	}
}

void FMetaHumanPerformance2DMouseWheelZoomBehaviorTarget::SetZoomAmount(double InPercentZoomIn)
{
	check(InPercentZoomIn < 100 && InPercentZoomIn >= 0);

	ZoomInProportion = InPercentZoomIn / 100;

	// Set the zoom out proportion such that (1 + ZoomOutProportion)(1 - ZoomInProportion) = 1
	// so that zooming in and then zooming out will return to the same zoom level.
	ZoomOutProportion = ZoomInProportion / (1.0 - ZoomInProportion);
}

void FMetaHumanPerformance2DMouseWheelZoomBehaviorTarget::SetZoomLimits(double InZoomInLimit, double InZoomOutLimit)
{
	ZoomInLimit = InZoomInLimit;
	ZoomOutLimit = InZoomOutLimit;
}

void FMetaHumanPerformance2DMouseWheelZoomBehaviorTarget::SetCameraFarPlaneWorldY(double InCameraFarPlaneWorldYIn)
{
	CameraFarPlaneWorldY = InCameraFarPlaneWorldYIn;
}

void FMetaHumanPerformance2DMouseWheelZoomBehaviorTarget::SetCameraNearPlaneProportionY(double InCameraNearPlaneProportionY)
{
	CameraNearPlaneProportionY = InCameraNearPlaneProportionY;
}

//////////////////////////////////////////////////////////////////////////
// FMetaHumanPerformanceControlRigViewportClient

FMetaHumanPerformanceControlRigViewportClient::FMetaHumanPerformanceControlRigViewportClient(FEditorModeTools* InModeTools, FPreviewScene* InPreviewScene)
	: FEditorViewportClient{ InModeTools, InPreviewScene }
{
	ShowWidget(false);
	OverrideNearClipPlane(KINDA_SMALL_NUMBER);

	// Don't draw the axis in the bottom left corner of the viewport.
	// This viewport is locked to display the XZ plane as this is the default plane where the face board control rig is rendered
	bDrawAxes = false;

	EngineShowFlags.SetAntiAliasing(false);
	EngineShowFlags.SetTemporalAA(false);
	EngineShowFlags.SetOpaqueCompositeEditorPrimitives(true);
	EngineShowFlags.SetDisableOcclusionQueries(true);
	EngineShowFlags.DisableAdvancedFeatures();

	// As the viewport is mostly black, prevents auto exposure logic to run
	ExposureSettings.bFixed = true;

	// Setting this rates to zero makes the 3D camera behaves as a 2D image viewer
	FEditorCameraController* CameraControllerPtr = GetCameraController();
	CameraController->GetConfig().MovementAccelerationRate = 0.0;
	CameraController->GetConfig().RotationAccelerationRate = 0.0;
	CameraController->GetConfig().FOVAccelerationRate = 0.0;

	// Set the camera to look down the Y axis as the face board control rig is rendered in the XZ plane by default
	SetViewLocation(FVector(0, 10, 0));
	SetViewRotation(FRotator(0, -90, 0));

	// We'll have the priority of our viewport manipulation behaviors be lower (i.e. higher
	// numerically) than both the gizmo default and the tool default.
	constexpr int DefaultViewportBehaviourPriority = 150;

	constexpr double CameraFarPlane = -10.0;
	constexpr double CameraNearPlaneProportionY = 0.8;
	constexpr double CameraZoomMin = 0.001;
	constexpr double CameraZoomMax = 10000.0;

	// Create and install the behavior targets to customize the camera controls
	ZoomBehaviourTarget = MakeUnique<FMetaHumanPerformance2DMouseWheelZoomBehaviorTarget>(this);
	ZoomBehaviourTarget->SetCameraFarPlaneWorldY(CameraFarPlane);
	ZoomBehaviourTarget->SetCameraNearPlaneProportionY(CameraNearPlaneProportionY);
	ZoomBehaviourTarget->SetZoomLimits(CameraZoomMin, CameraZoomMax);

	ScrollBehaviorTarget = MakeUnique<FMetaHumanPerformance2DScrollBehaviorTarget>(this);

	UClickDragInputBehavior* ScrollBehavior = NewObject<UClickDragInputBehavior>();
	ScrollBehavior->Initialize(ScrollBehaviorTarget.Get());
	ScrollBehavior->SetDefaultPriority(DefaultViewportBehaviourPriority);
	ScrollBehavior->SetUseRightMouseButton();

	UMouseWheelInputBehavior* ZoomBehaviour = NewObject<UMouseWheelInputBehavior>();
	ZoomBehaviour->Initialize(ZoomBehaviourTarget.Get());
	ZoomBehaviour->SetDefaultPriority(DefaultViewportBehaviourPriority);

	BehaviorSet = NewObject<UInputBehaviorSet>();
	BehaviorSet->Add(ZoomBehaviour);
	BehaviorSet->Add(ScrollBehavior);

	// Register this class as the input source, this will redirect user input to the behavior classes
	ModeTools->GetInteractiveToolsContext()->InputRouter->RegisterSource(this);
}

bool FMetaHumanPerformanceControlRigViewportClient::InputKey(const FInputKeyEventArgs& InEventArgs)
{
	if (bDisableInput)
	{
		return true;
	}

	return ModeTools->InputKey(this, InEventArgs.Viewport, InEventArgs.Key, InEventArgs.Event);
}

bool FMetaHumanPerformanceControlRigViewportClient::ShouldOrbitCamera() const
{
	// This is a 2D view, so should never orbit the camera
	return false;
}

FString FMetaHumanPerformanceControlRigViewportClient::GetReferencerName() const
{
	return TEXT("FMetaHumanPerformanceControlRigViewportClient");
}

void FMetaHumanPerformanceControlRigViewportClient::AddReferencedObjects(FReferenceCollector& InCollector)
{
	if (BehaviorSet != nullptr)
	{
		InCollector.AddReferencedObject(BehaviorSet);
	}
}

const UInputBehaviorSet* FMetaHumanPerformanceControlRigViewportClient::GetInputBehaviors() const
{
	return BehaviorSet;
}
