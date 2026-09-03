// Copyright Epic Games, Inc. All Rights Reserved.

#include "SplineMeshSceneProxyDesc.h"

#include "SplineMeshSceneProxy.h"
#include "StaticMeshResources.h"
#include "Components/SplineMeshComponent.h"
#include "HAL/ConsoleManager.h"

static TAutoConsoleVariable<bool> CVarSplineMeshFitToSourceMeshBounds(
	TEXT("r.SplineMesh.FitToSourceMeshBounds"),
	true,
	TEXT("When true, will use the bounds of the LOD 0 source mesh to fit the mesh to the spline, as opposed to the collective ")
	TEXT("mesh bounds of all LODs. This prevents gaps that might occur due bounds being expanded by lower LODs or bounds extensions.")
);

static FVector3f SplineEvalPos(const FVector3f& StartPos, const FVector3f& StartTangent, const FVector3f& EndPos, const FVector3f& EndTangent, float A)
{
	const float A2 = A * A;
	const float A3 = A2 * A;

	return (((2 * A3) - (3 * A2) + 1) * StartPos) + ((A3 - (2 * A2) + A) * StartTangent) + ((A3 - A2) * EndTangent) + (((-2 * A3) + (3 * A2)) * EndPos);
}

static FVector3f SplineEvalPos(const FSplineMeshParams& Params, float A)
{
	// TODO: these don't need to be doubles!
	const FVector3f StartPos = FVector3f(Params.StartPos);
	const FVector3f StartTangent = FVector3f(Params.StartTangent);
	const FVector3f EndPos = FVector3f(Params.EndPos);
	const FVector3f EndTangent = FVector3f(Params.EndTangent);

	return SplineEvalPos(StartPos, StartTangent, EndPos, EndTangent, A);
}

static FVector3f SplineEvalTangent(const FVector3f& StartPos, const FVector3f& StartTangent, const FVector3f& EndPos, const FVector3f& EndTangent, const float A)
{
	const FVector3f C = (6 * StartPos) + (3 * StartTangent) + (3 * EndTangent) - (6 * EndPos);
	const FVector3f D = (-6 * StartPos) - (4 * StartTangent) - (2 * EndTangent) + (6 * EndPos);
	const FVector3f E = StartTangent;

	const float A2 = A * A;

	return (C * A2) + (D * A) + E;
}

static FVector3f SplineEvalTangent(const FSplineMeshParams& Params, const float A)
{
	// TODO: these don't need to be doubles!
	const FVector3f StartPos = FVector3f(Params.StartPos);
	const FVector3f StartTangent = FVector3f(Params.StartTangent);
	const FVector3f EndPos = FVector3f(Params.EndPos);
	const FVector3f EndTangent = FVector3f(Params.EndTangent);

	return SplineEvalTangent(StartPos, StartTangent, EndPos, EndTangent, A);
}

static FVector3f SplineEvalDir(const FSplineMeshParams& Params, const float A)
{
	return SplineEvalTangent(Params, A).GetSafeNormal();
}


FSplineMeshSceneProxyDesc::FSplineMeshSceneProxyDesc(const USplineMeshComponent* InComponent)
{
	InitializeFrom(InComponent);
}

void FSplineMeshSceneProxyDesc::InitializeFrom(const USplineMeshComponent* InComponent)
{
	SplineParams = InComponent->SplineParams;
	SplineUpDir = InComponent->SplineUpDir;
	SplineBoundaryMin = InComponent->SplineBoundaryMin;
	SplineBoundaryMax = InComponent->SplineBoundaryMax;
	bSmoothInterpRollScale = InComponent->bSmoothInterpRollScale;
	ForwardAxis = InComponent->ForwardAxis;

	if (const UStaticMesh* StaticMesh = InComponent->GetStaticMesh())
	{
		SourceMeshBounds = StaticMesh->GetBounds(); // legacy behavior

		if (CVarSplineMeshFitToSourceMeshBounds.GetValueOnAnyThread())
		{
			if (const FStaticMeshRenderData* RenderData = StaticMesh->GetRenderData())
			{
				if (!RenderData->LODResources.IsEmpty())
				{
					SourceMeshBounds = RenderData->LODResources[0].SourceMeshBounds;
				}
			}
		}
	}
}

FSplineMeshShaderParams FSplineMeshSceneProxyDesc::CalculateShaderParams() const
{
	FSplineMeshShaderParams Output;

	Output.StartPos 				= FVector3f(SplineParams.StartPos);
	Output.EndPos 					= FVector3f(SplineParams.EndPos);
	Output.StartTangent 			= FVector3f(SplineParams.StartTangent);
	Output.EndTangent 				= FVector3f(SplineParams.EndTangent);
	Output.StartScale 				= FVector2f(SplineParams.StartScale);
	Output.EndScale 				= FVector2f(SplineParams.EndScale);
	Output.StartOffset 				= FVector2f(SplineParams.StartOffset);
	Output.EndOffset 				= FVector2f(SplineParams.EndOffset);
	Output.StartRoll 				= SplineParams.StartRoll;
	Output.EndRoll 					= SplineParams.EndRoll;
	Output.NaniteClusterBoundsScale	= SplineParams.NaniteClusterBoundsScale;
	Output.bSmoothInterpRollScale 	= bSmoothInterpRollScale;
	Output.SplineUpDir 				= FVector3f(SplineUpDir);
	Output.TextureCoord 			= FUintVector2(INDEX_NONE, INDEX_NONE); // either unused or assigned later

	const uint32 MeshXAxis = (ForwardAxis + 1) % 3;
	const uint32 MeshYAxis = (ForwardAxis + 2) % 3;
	Output.MeshDir = Output.MeshX = Output.MeshY = FVector3f::ZeroVector;
	Output.MeshDir[ForwardAxis] = 1.0f;
	Output.MeshX[MeshXAxis] = 1.0f;
	Output.MeshY[MeshYAxis] = 1.0f;

	Output.MeshZScale = 1.0f;
	Output.MeshZOffset = 0.0f;

	if (SourceMeshBounds.SphereRadius > 0.0f)
	{
		const float BoundsXYRadius = FVector3f(SourceMeshBounds.BoxExtent).Dot((Output.MeshX + Output.MeshY).GetUnsafeNormal());

		const float MeshMinZ = UE::SplineMesh::RealToFloatChecked(USplineMeshComponent::GetAxisValueRef(SourceMeshBounds.Origin - SourceMeshBounds.BoxExtent, ForwardAxis));
		const float MeshZLen = UE::SplineMesh::RealToFloatChecked(2 * USplineMeshComponent::GetAxisValueRef(SourceMeshBounds.BoxExtent, ForwardAxis));
		const float InvMeshZLen = (MeshZLen <= 0.0f) ? 1.0f : 1.0f / MeshZLen;
		constexpr float MeshTexelLen = float(SPLINE_MESH_TEXEL_WIDTH - 1);

		if (FMath::IsNearlyEqual(SplineBoundaryMin, SplineBoundaryMax))
		{
			Output.MeshZScale = InvMeshZLen;
			Output.MeshZOffset = -MeshMinZ * InvMeshZLen;
			Output.SplineDistToTexelScale = MeshTexelLen;
			Output.SplineDistToTexelOffset = 0.0f;
		}
		else
		{
			const float BoundaryLen = SplineBoundaryMax - SplineBoundaryMin;
			const float InvBoundaryLen = 1.0f / BoundaryLen;

			Output.MeshZScale = InvBoundaryLen;
			Output.MeshZOffset = -SplineBoundaryMin * InvBoundaryLen;
			Output.SplineDistToTexelScale = BoundaryLen * InvMeshZLen * MeshTexelLen;
			Output.SplineDistToTexelOffset = (SplineBoundaryMin - MeshMinZ) * InvMeshZLen * MeshTexelLen;
		}

		// Iteratively solve for an approximation of spline length
		float SplineLength = 0.0f;
		{
			static const uint32 NumIterations = 63; // 64 sampled points
			static const float IterStep = 1.0f / float(NumIterations);
			float A = 0.0f;
			FVector3f PrevPoint = SplineEvalPos(SplineParams, A);
			for (uint32 i = 0; i < NumIterations; ++i)
			{
				FVector3f Point = SplineEvalPos(SplineParams, A);
				SplineLength += (Point - PrevPoint).Length();
				PrevPoint = Point;
				A += IterStep;
			}
		}

		// Calculate an approximation of how much the mesh gets scaled in each local axis as a result of spline
		// deformation and take the smallest of the axes. This is important for LOD selection of Nanite spline
		// meshes.
		{
			// Estimate length added due to twisting as well
			const float XYRadius = BoundsXYRadius * FMath::Max(Output.StartScale.GetAbsMax(), Output.EndScale.GetAbsMax());
			const float TwistRadians = FMath::Abs(Output.StartRoll - Output.EndRoll);
			SplineLength += TwistRadians * XYRadius;

			// Take the mid-point scale in X/Y to balance out LOD selection in case either of them are extreme.
			auto AvgAbs = [](float A, float B) { return (FMath::Abs(A) + FMath::Abs(B)) * 0.5f; };
			const FVector3f DeformScale = FVector3f(
				SplineLength * Output.MeshZScale,
				AvgAbs(Output.StartScale.X, Output.EndScale.X),
				AvgAbs(Output.StartScale.Y, Output.EndScale.Y)
			);
			
			Output.MeshDeformScaleMinMax = FVector2f(DeformScale.GetMin(), DeformScale.GetMax());
		}
	}
	return Output;
}

extern void InitSplineMeshVertexFactoryComponents(
	const FStaticMeshVertexBuffers& VertexBuffers, 
	const FSplineMeshVertexFactory* VertexFactory, 
	int32 LightMapCoordinateIndex, 
	bool bOverrideColorVertexBuffer,
	FLocalVertexFactory::FDataType& OutData);

static void InitSplineVertexFactory_Internal(FStaticMeshVertexFactories& VertexFactories, const FStaticMeshVertexBuffers& VertexBuffers, int32 LightMapCoordinateIndex, const ERHIFeatureLevel::Type FeatureLevel, FColorVertexBuffer* InOverrideColorVertexBuffer)
{
	// Skip LODs that have their render data stripped (eg. platform MinLod settings)
	if (VertexBuffers.StaticMeshVertexBuffer.GetNumVertices() == 0)
	{
		return;
	}

	bool bOverrideColorVertexBuffer = !!InOverrideColorVertexBuffer;

	if ((VertexFactories.SplineVertexFactory && !bOverrideColorVertexBuffer) || (VertexFactories.SplineVertexFactoryOverrideColorVertexBuffer && bOverrideColorVertexBuffer))
	{
		// we already have it
		return;
	}

	FSplineMeshVertexFactory* VertexFactory = new FSplineMeshVertexFactory(FeatureLevel);
	if (bOverrideColorVertexBuffer)
	{
		VertexFactories.SplineVertexFactoryOverrideColorVertexBuffer = VertexFactory;
	}
	else
	{
		VertexFactories.SplineVertexFactory = VertexFactory;
	}

	// Initialize the static mesh's vertex factory.
	ENQUEUE_RENDER_COMMAND(InitSplineMeshVertexFactory)(
		[VertexFactory, &VertexBuffers, bOverrideColorVertexBuffer, LightMapCoordinateIndex](FRHICommandListBase& RHICmdList)
		{
			FLocalVertexFactory::FDataType Data;
			InitSplineMeshVertexFactoryComponents(VertexBuffers, VertexFactory, LightMapCoordinateIndex, bOverrideColorVertexBuffer, Data);
			VertexFactory->SetData(RHICmdList, Data);
			VertexFactory->InitResource(RHICmdList);
		});
}

void FSplineMeshSceneProxyDesc::InitVertexFactory(UStaticMesh* Mesh, const ERHIFeatureLevel::Type FeatureLevel, int32 InLODIndex, FColorVertexBuffer* InOverrideColorVertexBuffer)
{	
	if (Mesh == nullptr)
	{
		return;
	}

	FStaticMeshRenderData* RenderData = Mesh->GetRenderData();

	InitSplineVertexFactory_Internal(RenderData->LODVertexFactories[InLODIndex], RenderData->LODResources[InLODIndex].VertexBuffers, Mesh->GetLightMapCoordinateIndex(), FeatureLevel, InOverrideColorVertexBuffer);
}

void FSplineMeshSceneProxyDesc::InitRayTracingProxyVertexFactory(UStaticMesh* Mesh, const ERHIFeatureLevel::Type FeatureLevel, int32 InLODIndex, FColorVertexBuffer* InOverrideColorVertexBuffer)
{
	if (Mesh == nullptr || Mesh->GetRenderData()->RayTracingProxy->bUsingRenderingLODs)
	{
		return;
	}

	FStaticMeshRenderData* RenderData = Mesh->GetRenderData();

	InitSplineVertexFactory_Internal((*RenderData->RayTracingProxy->LODVertexFactories)[InLODIndex], *RenderData->RayTracingProxy->LODs[InLODIndex].VertexBuffers, Mesh->GetLightMapCoordinateIndex(), FeatureLevel, InOverrideColorVertexBuffer);
}

FBox FSplineMeshSceneProxyDesc::ComputeDistortedBounds(const FTransform& InLocalToWorld, const FBoxSphereBounds& InMeshBounds, const FBoxSphereBounds* InBoundsToDistort) const
{
	float MinT = 0.0f;
	float MaxT = 1.0f;
	ComputeVisualMeshSplineTRange(MinT, MaxT);
	const FBoxSphereBounds& BoundsToDistort = InBoundsToDistort ? *InBoundsToDistort : InMeshBounds;

	auto ForwardAxisType = static_cast<ESplineMeshAxis::Type>(ForwardAxis);
	const FVector AxisMask = USplineMeshComponent::GetAxisMask(ForwardAxisType);
	const FVector FlattenedBoundsOrigin = BoundsToDistort.Origin * AxisMask;
	const FVector FlattenedBoundsExtent = BoundsToDistort.BoxExtent * AxisMask;
	const FBox FlattenedBounds = FBox(FlattenedBoundsOrigin - FlattenedBoundsExtent, FlattenedBoundsOrigin + FlattenedBoundsExtent);

	FBox BoundingBox(ForceInit);
	BoundingBox += FlattenedBounds.TransformBy(CalcSliceTransformAtSplineOffset(MinT, MinT, MaxT));
	BoundingBox += FlattenedBounds.TransformBy(CalcSliceTransformAtSplineOffset(MaxT, MinT, MaxT));

	// Work out coefficients of the cubic spline derivative equation dx/dt
	const FVector A(6 * SplineParams.StartPos + 3 * SplineParams.StartTangent + 3 * SplineParams.EndTangent - 6 * SplineParams.EndPos);
	const FVector B(-6 * SplineParams.StartPos - 4 * SplineParams.StartTangent - 2 * SplineParams.EndTangent + 6 * SplineParams.EndPos);
	const FVector C(SplineParams.StartTangent);

	auto AppendAxisExtrema = [this, &BoundingBox, &FlattenedBounds, MinT, MaxT](const double Discriminant, const double A, const double B)
		{
			// Negative discriminant means no solution; A == 0 implies coincident start/end points
			if (Discriminant > 0 && !FMath::IsNearlyZero(A))
			{
				const double SqrtDiscriminant = FMath::Sqrt(Discriminant);
				const double Denominator = 0.5 / A;
				const double T0 = (-B + SqrtDiscriminant) * Denominator;
				const double T1 = (-B - SqrtDiscriminant) * Denominator;

				if (T0 >= MinT && T0 <= MaxT)
				{
					BoundingBox += FlattenedBounds.TransformBy(CalcSliceTransformAtSplineOffset(UE::SplineMesh::RealToFloatChecked(T0), MinT, MaxT));
				}

				if (T1 >= MinT && T1 <= MaxT)
				{
					BoundingBox += FlattenedBounds.TransformBy(CalcSliceTransformAtSplineOffset(UE::SplineMesh::RealToFloatChecked(T1), MinT, MaxT));
				}
			}
		};


	// Minima/maxima happen where dx/dt == 0, calculate t values
	const FVector Discriminant = B * B - 4 * A * C;

	// Work out minima/maxima component-by-component.
	AppendAxisExtrema(Discriminant.X, A.X, B.X);
	AppendAxisExtrema(Discriminant.Y, A.Y, B.Y);
	AppendAxisExtrema(Discriminant.Z, A.Z, B.Z);

	// Applying extrapolation if bounds to apply on spline are different than the mesh bounds used
	// to define the spline range [0,1]
	if (InBoundsToDistort != nullptr && InBoundsToDistort != &InMeshBounds)
	{
		const double BoundsMin = USplineMeshComponent::GetAxisValueRef(BoundsToDistort.Origin - BoundsToDistort.BoxExtent, ForwardAxisType);
		const double BoundsMax = USplineMeshComponent::GetAxisValueRef(BoundsToDistort.Origin + BoundsToDistort.BoxExtent, ForwardAxisType);

		float Alpha = ComputeRatioAlongSpline(UE::SplineMesh::RealToFloatChecked(BoundsMin));
		if (Alpha < MinT)
		{
			BoundingBox += FlattenedBounds.TransformBy(CalcSliceTransformAtSplineOffset(Alpha, MinT, MaxT));
		}

		Alpha = ComputeRatioAlongSpline(UE::SplineMesh::RealToFloatChecked(BoundsMax));
		if (Alpha > MaxT)
		{
			BoundingBox += FlattenedBounds.TransformBy(CalcSliceTransformAtSplineOffset(Alpha, MinT, MaxT));
		}
	}

	return BoundingBox.TransformBy(InLocalToWorld);
}

FTransform FSplineMeshSceneProxyDesc::CalcSliceTransform(const float DistanceAlong) const
{
	const float Alpha = ComputeRatioAlongSpline(DistanceAlong);
	

	float MinT = 0.f;
	float MaxT = 1.f;
	ComputeVisualMeshSplineTRange(MinT, MaxT);

	return CalcSliceTransformAtSplineOffset(Alpha, MinT, MaxT);
}

/**
* Functions used for transforming a static mesh component based on a spline.
* This needs to be updated if the spline functionality changes!
*/
static float SmoothStep(float A, float B, float X)
{
	if (X < A)
	{
		return 0.0f;
	}
	else if (X >= B)
	{
		return 1.0f;
	}
	const float InterpFraction = (X - A) / (B - A);
	return InterpFraction * InterpFraction * (3.0f - 2.0f * InterpFraction);
}

FTransform FSplineMeshSceneProxyDesc::CalcSliceTransformAtSplineOffset(const float Alpha, const float MinT, const float MaxT) const
{
	// Apply hermite interp to Alpha if desired
	const float HermiteAlpha = bSmoothInterpRollScale ? SmoothStep(0.0, 1.0, Alpha) : Alpha;


	// Then find the point and direction of the spline at this point along
	FVector3f SplinePos;
	FVector3f SplineDir;

	// Use linear extrapolation
	if (Alpha < MinT)
	{
		const FVector3f StartTangent(SplineEvalTangent(SplineParams, MinT));
		SplinePos = SplineEvalPos(SplineParams, MinT) + (StartTangent * (Alpha - MinT));
		SplineDir = StartTangent.GetSafeNormal();
	}
	else if (Alpha > MaxT)
	{
		const FVector3f EndTangent(SplineEvalTangent(SplineParams, MaxT));
		SplinePos = SplineEvalPos(SplineParams, MaxT) + (EndTangent * (Alpha - MaxT));
		SplineDir = EndTangent.GetSafeNormal();
	}
	else
	{
		SplinePos = SplineEvalPos(SplineParams, Alpha);
		SplineDir = SplineEvalDir(SplineParams, Alpha);
	}

	// Find base frenet frame
	const FVector3f BaseXVec = (FVector3f(SplineUpDir) ^ SplineDir).GetSafeNormal();
	const FVector3f BaseYVec = (FVector3f(SplineDir) ^ BaseXVec).GetSafeNormal();

	// Offset the spline by the desired amount
	const FVector2f SliceOffset = FMath::Lerp(FVector2f(SplineParams.StartOffset), FVector2f(SplineParams.EndOffset), HermiteAlpha);
	SplinePos += SliceOffset.X * BaseXVec;
	SplinePos += SliceOffset.Y * BaseYVec;

	// Apply roll to frame around spline
	const float UseRoll = FMath::Lerp(SplineParams.StartRoll, SplineParams.EndRoll, HermiteAlpha);
	const float CosAng = FMath::Cos(UseRoll);
	const float SinAng = FMath::Sin(UseRoll);
	const FVector3f XVec = (CosAng * BaseXVec) - (SinAng * BaseYVec);
	const FVector3f YVec = (CosAng * BaseYVec) + (SinAng * BaseXVec);

	// Find scale at this point along spline
	const FVector2f UseScale = FMath::Lerp(FVector2f(SplineParams.StartScale), FVector2f(SplineParams.EndScale), HermiteAlpha);

	// Build overall transform
	FTransform SliceTransform;
	
	switch (static_cast<ESplineMeshAxis::Type>(ForwardAxis))
	{
	case ESplineMeshAxis::X:
		SliceTransform = FTransform(FVector(SplineDir), FVector(XVec), FVector(YVec), FVector(SplinePos));
		SliceTransform.SetScale3D(FVector(1, UseScale.X, UseScale.Y));
		break;
	case ESplineMeshAxis::Y:
		SliceTransform = FTransform(FVector(YVec), FVector(SplineDir), FVector(XVec), FVector(SplinePos));
		SliceTransform.SetScale3D(FVector(UseScale.Y, 1, UseScale.X));
		break;
	case ESplineMeshAxis::Z:
		SliceTransform = FTransform(FVector(XVec), FVector(YVec), FVector(SplineDir), FVector(SplinePos));
		SliceTransform.SetScale3D(FVector(UseScale.X, UseScale.Y, 1));
		break;
	default:
		check(0);
		break;
	}

	return SliceTransform;
}

float FSplineMeshSceneProxyDesc::ComputeRatioAlongSpline(float DistanceAlong) const
{
	// Find how far 'along' mesh (or custom boundaries) we are
	float Alpha = 0.f;

	const bool bHasCustomBoundary = !FMath::IsNearlyEqual(SplineBoundaryMin, SplineBoundaryMax);
	if (bHasCustomBoundary)
	{
		Alpha = (DistanceAlong - SplineBoundaryMin) / (SplineBoundaryMax - SplineBoundaryMin);
	}
	else if (SourceMeshBounds.SphereRadius > 0.0f)
	{
		const double MeshMinZ = USplineMeshComponent::GetAxisValueRef(SourceMeshBounds.Origin, ForwardAxis) - USplineMeshComponent::GetAxisValueRef(SourceMeshBounds.BoxExtent, ForwardAxis);
		const double MeshRangeZ = 2 * USplineMeshComponent::GetAxisValueRef(SourceMeshBounds.BoxExtent, ForwardAxis);
		if (MeshRangeZ > UE_SMALL_NUMBER)
		{
			Alpha = UE::SplineMesh::RealToFloatChecked((DistanceAlong - MeshMinZ) / MeshRangeZ);
		}
	}
	return Alpha;
}

void FSplineMeshSceneProxyDesc::ComputeVisualMeshSplineTRange(float& MinT, float& MaxT) const
{
	MinT = 0.0;
	MaxT = 1.0;	
	const bool bHasCustomBoundary = !FMath::IsNearlyEqual(SplineBoundaryMin, SplineBoundaryMax);
	if (bHasCustomBoundary)
	{
		// If there's a custom boundary, alter the min/max of the spline we need to evaluate
		ESplineMeshAxis::Type ForwardAxisType = static_cast<ESplineMeshAxis::Type>(ForwardAxis);
		const float BoundsMin = UE::SplineMesh::RealToFloatChecked(USplineMeshComponent::GetAxisValueRef(SourceMeshBounds.Origin - SourceMeshBounds.BoxExtent, ForwardAxisType));
		const float BoundsMax = UE::SplineMesh::RealToFloatChecked(USplineMeshComponent::GetAxisValueRef(SourceMeshBounds.Origin + SourceMeshBounds.BoxExtent, ForwardAxisType));
		const float BoundsMinT = (BoundsMin - SplineBoundaryMin) / (SplineBoundaryMax - SplineBoundaryMin);
		const float BoundsMaxT = (BoundsMax - SplineBoundaryMin) / (SplineBoundaryMax - SplineBoundaryMin);

		// Disallow extrapolation beyond a certain value; enormous bounding boxes cause the render thread to crash
		constexpr float MaxSplineExtrapolation = 4.0f;
		MinT = FMath::Max(-MaxSplineExtrapolation, BoundsMinT);
		MaxT = FMath::Min(BoundsMaxT, MaxSplineExtrapolation);
	}
}