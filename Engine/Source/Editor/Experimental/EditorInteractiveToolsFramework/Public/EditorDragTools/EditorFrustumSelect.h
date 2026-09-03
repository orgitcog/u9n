// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorDragToolBehaviorTarget.h"


class FCanvas;
class FEditorViewportClient;
class FSceneView;
class UModel;
struct FConvexVolume;

/**
 * Draws a box in the current viewport and when the mouse button is released,
 * it selects/unselects everything inside of it.
 */
class FEditorFrustumSelect : public FEditorDragToolBehaviorTarget
{
public:
	explicit FEditorFrustumSelect(FEditorViewportClient* const InEditorViewportClient)
		: FEditorDragToolBehaviorTarget(InEditorViewportClient)
	{
	}

	virtual void Render(const FSceneView* View, FCanvas* Canvas) override;

	//~ Begin IClickDragBehaviorTarget
	virtual FInputRayHit CanBeginClickDragSequence(const FInputDeviceRay& InPressPos) override;
	virtual void OnClickPress(const FInputDeviceRay& InPressPos) override;
	virtual void OnClickDrag(const FInputDeviceRay& InDragPos) override;
	virtual void OnClickRelease(const FInputDeviceRay& InReleasePos) override;
	virtual void OnTerminateDragSequence() override;
	//~ End IClickDragBehaviorTarget

protected:
	virtual TArray<FEditorModeID> GetUnsupportedModes() override;

private:
	/**
	 * Returns true if the provided BSP node intersects with the provided frustum
	 *
	 * @param InModel				The model containing BSP nodes to check
	 * @param NodeIndex				The index to a BSP node in the model.  This node is used for the bounds check.
	 * @param InFrustum				The frustum to check against.
	 * @param bUseStrictSelection	true if the node must be entirely within the frustum
	 */
	static bool IntersectsFrustum(const UModel& InModel, int32 NodeIndex, const FConvexVolume& InFrustum, bool bUseStrictSelection);

	/**
	 * Calculates a frustum to check actors against
	 *
	 * @param InView			Information about scene projection
	 * @param OutFrustum		The created frustum
	 * @param bUseBoxFrustum	If true a frustum out of the current dragged box will be created.  false will use the view frustum.
	 */
	void CalculateFrustum(const FSceneView* InView, FConvexVolume& OutFrustum, bool bUseBoxFrustum) const;
};
