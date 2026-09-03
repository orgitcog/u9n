// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "StaticMeshSceneProxyDesc.h"
#include "Components/SplineMeshComponent.h"

class USplineMeshComponent;

struct FSplineMeshSceneProxyDesc
{
	FSplineMeshSceneProxyDesc() = default;
	ENGINE_API FSplineMeshSceneProxyDesc(const USplineMeshComponent* InComponent);
	
	void InitializeFrom(const USplineMeshComponent* InComponent);
	
	FSplineMeshParams SplineParams{};
	FVector SplineUpDir = FVector::UpVector;
	float SplineBoundaryMin = 0.0f;
	float SplineBoundaryMax = 0.0f;
	TEnumAsByte<ESplineMeshAxis::Type> ForwardAxis = ESplineMeshAxis::X;
	bool bSmoothInterpRollScale = false;
	FBoxSphereBounds SourceMeshBounds { ForceInit };

	FSplineMeshParams GetSplineParams() const { return SplineParams; }

	FSplineMeshShaderParams CalculateShaderParams() const;
	float ComputeRatioAlongSpline(float DistanceAlong) const;
	void ComputeVisualMeshSplineTRange(float& MinT, float& MaxT) const;
	ENGINE_API FBox ComputeDistortedBounds(const FTransform& InLocalToWorld, const FBoxSphereBounds& InMeshBounds, const FBoxSphereBounds* InBoundsToDistort = nullptr) const;
	FTransform CalcSliceTransform(const float DistanceAlong) const;
	FTransform CalcSliceTransformAtSplineOffset(const float Alpha, const float MinT, const float MaxT) const;
	
	static void InitVertexFactory(UStaticMesh* Mesh, const ERHIFeatureLevel::Type FeatureLevel, int32 InLODIndex, FColorVertexBuffer* InOverrideColorVertexBuffer);
	static void InitRayTracingProxyVertexFactory(UStaticMesh* Mesh, const ERHIFeatureLevel::Type FeatureLevel, int32 InLODIndex, FColorVertexBuffer* InOverrideColorVertexBuffer);
};