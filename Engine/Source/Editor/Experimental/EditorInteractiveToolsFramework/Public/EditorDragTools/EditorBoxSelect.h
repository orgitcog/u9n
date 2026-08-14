// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorDragToolBehaviorTarget.h"

class FCanvas;
class UModel;

class FEditorBoxSelect : public FEditorDragToolBehaviorTarget
{
public:
	explicit FEditorBoxSelect(FEditorViewportClient* const InEditorViewportClient)
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
	 * @param InNodeIndex				The index to a BSP node in the model.  This node is used for the bounds check.
	 * @param InBox			The frustum to check against.
	 * @param bInUseStrictSelection	true if the node must be entirely within the frustum
	 */
	static bool IntersectsBox(const UModel& InModel, int32 InNodeIndex, const FBox& InBox, bool bInUseStrictSelection);

	/** Adds a hover effect to the passed in actor */
	static void AddHoverEffect(AActor& InActor);

	/** Adds a hover effect to the passed in bsp surface */
	static void AddHoverEffect(UModel& InModel, int32 InSurfIndex);

	/** Removes a hover effect from the passed in actor */
	static void RemoveHoverEffect(AActor& InActor);

	/** Removes a hover effect from the passed in bsp surface */
	static void RemoveHoverEffect(UModel& InModel, int32 InSurfIndex);

	/**
	 * Calculates a box to check actors against
	 *
	 * @param OutBox	The created box.
	 */
	void CalculateBox(FBox& OutBox) const;

	/** List of BSP models to check for selection */
	TArray<UModel*> ModelsToCheck;
};
