// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorGizmos/GizmoElementGimbal.h"

#include "EditorGizmos/GizmoRotationUtil.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GizmoElementGimbal)

void UGizmoElementGimbal::Render(IToolsContextRenderAPI* InRenderAPI, const FRenderTraversalState& InRenderState)
{
	using namespace UE::GizmoRotationUtil;
	
	if (Elements.Num() != 3)
	{
		return;
	}
	
	FRenderTraversalState CurrentRenderState(InRenderState);
	const bool bVisible = UpdateRenderState(InRenderAPI, FVector::ZeroVector, CurrentRenderState);
	if (!bVisible)
	{
		return;
	}

	// decompose rotations
	FRotationDecomposition Decomposition;
	DecomposeRotations(CurrentRenderState.LocalToWorldTransform, RotationContext, Decomposition);
	
	int32 Index = 0;
	for (UGizmoElementBase* Element : Elements)
	{
		CurrentRenderState.LocalToWorldTransform.SetRotation(Decomposition.R[Index++]);

		if (Element)
		{
			Element->Render(InRenderAPI, CurrentRenderState);
		}
	}
}

FInputRayHit UGizmoElementGimbal::LineTrace(const UGizmoViewContext* InViewContext,
	const FLineTraceTraversalState& InLineTraceState, const FVector& InRayOrigin, const FVector& InRayDirection)
{
	using namespace UE::GizmoRotationUtil;
	
	FInputRayHit Hit;
	if (Elements.Num() != 3)
	{
		return Hit;
	}

	FLineTraceTraversalState CurrentLineTraceState(InLineTraceState);
	const bool bHittable = UpdateLineTraceState(InViewContext, FVector::ZeroVector, CurrentLineTraceState);

	if (!bHittable)
	{
		return Hit;
	}
	
	// decompose rotations
    FRotationDecomposition Decomposition;
    DecomposeRotations(CurrentLineTraceState.LocalToWorldTransform, RotationContext, Decomposition);

	int32 Index = 0;
	for (UGizmoElementBase* Element : Elements)
	{
		CurrentLineTraceState.LocalToWorldTransform.SetRotation(Decomposition.R[Index++]);
		if (Element)
		{
			FInputRayHit NewHit = Element->LineTrace(InViewContext, CurrentLineTraceState, InRayOrigin, InRayDirection);
			if (!Hit.bHit || NewHit.HitDepth < Hit.HitDepth)
			{
				Hit = NewHit;
			}
		}
	}
	
	return Hit;
}

void UGizmoElementGimbal::Add(UGizmoElementBase* InElement)
{
	if (Elements.Num() < 3)
	{
		UGizmoElementGroup::Add(InElement);
	}
}
