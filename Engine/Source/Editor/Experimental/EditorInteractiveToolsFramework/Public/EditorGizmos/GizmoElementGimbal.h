// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseGizmos/GizmoElementGroup.h"
#include "TransformGizmoInterfaces.h"

#include "GizmoElementGimbal.generated.h"

/**
 * Gimbal rotation group object intended to be used as part of 3D Rotation Gizmos
 * This group expects three rotation sub-elements
 */
UCLASS(Transient, MinimalAPI)
class UGizmoElementGimbal : public UGizmoElementGroup
{
	GENERATED_BODY()

public:

	//~ Begin UGizmoElementBase Interface.
	EDITORINTERACTIVETOOLSFRAMEWORK_API virtual void Render(IToolsContextRenderAPI* InRenderAPI, const FRenderTraversalState& InRenderState) override;
	EDITORINTERACTIVETOOLSFRAMEWORK_API virtual FInputRayHit LineTrace(const UGizmoViewContext* InViewContext, const FLineTraceTraversalState& InLineTraceState, const FVector& InRayOrigin, const FVector& InRayDirection) override;
	//~ End UGizmoElementBase Interface.

	// Add object to group.
	EDITORINTERACTIVETOOLSFRAMEWORK_API virtual void Add(UGizmoElementBase* InElement) override;
	
	UPROPERTY()
	FRotationContext RotationContext;
};
