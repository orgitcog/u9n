// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/GeometryCollectionSamplingNodes.h"

#include "UDynamicMesh.h"
#include "Spatial/FastWinding.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GeometryCollectionSamplingNodes)

namespace UE::Dataflow
{

	void GeometryCollectionSamplingNodes()
	{
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FUniformPointSamplingDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FNonUniformPointSamplingDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FVertexWeightedPointSamplingDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FFilterPointSetWithMeshDataflowNode);
	}
}

FFilterPointSetWithMeshDataflowNode::FFilterPointSetWithMeshDataflowNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterInputConnection(&TargetMesh);
	RegisterInputConnection(&SamplePoints);
	RegisterInputConnection(&bKeepInside).SetCanHidePin(true).SetPinIsHidden(true);
	RegisterInputConnection(&WindingThreshold).SetCanHidePin(true).SetPinIsHidden(true);

	RegisterOutputConnection(&SamplePoints);
}

void FFilterPointSetWithMeshDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA(&SamplePoints))
	{
		TArray<FVector> InSamplePoints = GetValue(Context, &SamplePoints);
		double InWindingThreshold = (double)GetValue(Context, &WindingThreshold);
		bool bInKeepInside = GetValue(Context, &bKeepInside);
		if (TObjectPtr<const UDynamicMesh> InTargetMesh = GetValue(Context, &TargetMesh))
		{
			using namespace UE::Geometry;
			InTargetMesh->ProcessMesh([&InSamplePoints, InWindingThreshold, bInKeepInside](const FDynamicMesh3& Mesh)
				{
					//  set up AABBTree and FWNTree lists
					TMeshAABBTree3<FDynamicMesh3> Spatial(&Mesh);
					TFastWindingTree<FDynamicMesh3> FastWinding(&Spatial);

					// Filter points w/ the fast winding tree
					TArray<bool> PointInside;
					PointInside.SetNumZeroed(InSamplePoints.Num());
					ParallelFor(PointInside.Num(), 
						[&PointInside, &FastWinding, &InSamplePoints, InWindingThreshold]
						(int32 PointIdx)
						{
							FVector Pt = InSamplePoints[PointIdx];
							PointInside[PointIdx] = FastWinding.IsInside(Pt, InWindingThreshold);
						}
					);

					// Move the points we're keeping to the front of the array, and trim the array
					int32 FoundPoints = 0;
					for (int32 Idx = 0; ; ++Idx)
					{
						while (Idx < InSamplePoints.Num() && PointInside[Idx] != bInKeepInside)
						{
							Idx++;
						}
						if (Idx < InSamplePoints.Num())
						{
							if (Idx != FoundPoints)
							{
								InSamplePoints[FoundPoints] = InSamplePoints[Idx];
							}
							FoundPoints++;
						}
						else
						{
							break;
						}
					}
					InSamplePoints.SetNum(FoundPoints);
				}
			);
		}
		SetValue(Context, MoveTemp(InSamplePoints), &SamplePoints);
	}
}

void FUniformPointSamplingDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA(&SamplePoints) || 
		Out->IsA(&SampleTriangleIDs) ||
		Out->IsA(&SampleBarycentricCoords) ||
		Out->IsA(&NumSamplePoints))
	{
		TArray<FVector> OutPoints;
		TArray<FTransform> OutSamples;
		TArray<int32> OutTriangleIDs;
		TArray<FVector> OutBarycentricCoords;

		if (TObjectPtr<UDynamicMesh> InTargetMesh = GetValue(Context, &TargetMesh))
		{
			const UE::Geometry::FDynamicMesh3& InDynTargetMesh = InTargetMesh->GetMeshRef();

			if (InDynTargetMesh.VertexCount() > 0)
			{
				FFractureEngineSampling::ComputeUniformPointSampling(InDynTargetMesh,
					GetValue(Context, &SamplingRadius),
					GetValue(Context, &MaxNumSamples),
					GetValue(Context, &SubSampleDensity),
					GetValue(Context, &RandomSeed),
					OutSamples, 
					OutTriangleIDs, 
					OutBarycentricCoords);

				const int32 NumSamples = OutSamples.Num();

				OutPoints.AddUninitialized(NumSamples);

				for (int32 Idx = 0; Idx < NumSamples; ++Idx)
				{
					OutPoints[Idx] = OutSamples[Idx].GetTranslation();
				}
			}
		}

		SetValue(Context, MoveTemp(OutPoints), &SamplePoints);
		SetValue(Context, MoveTemp(OutTriangleIDs), &SampleTriangleIDs);
		SetValue(Context, MoveTemp(OutBarycentricCoords), &SampleBarycentricCoords);
		SetValue(Context, OutPoints.Num(), &NumSamplePoints);
	}
}

void FNonUniformPointSamplingDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA(&SamplePoints) ||
		Out->IsA(&SampleRadii) ||
		Out->IsA(&SampleTriangleIDs) ||
		Out->IsA(&SampleBarycentricCoords) ||
		Out->IsA(&NumSamplePoints))
	{
		TArray<FVector> OutPoints;
		TArray<FTransform> OutSamples;
		TArray<float> OutSampleRadii;
		TArray<int32> OutTriangleIDs;
		TArray<FVector> OutBarycentricCoords;

		if (TObjectPtr<UDynamicMesh> InTargetMesh = GetValue(Context, &TargetMesh))
		{
			const UE::Geometry::FDynamicMesh3& InDynTargetMesh = InTargetMesh->GetMeshRef();

			if (InDynTargetMesh.VertexCount() > 0)
			{
				FFractureEngineSampling::ComputeNonUniformPointSampling(InDynTargetMesh,
					GetValue(Context, &SamplingRadius),
					GetValue(Context, &MaxNumSamples),
					GetValue(Context, &SubSampleDensity),
					GetValue(Context, &RandomSeed),
					GetValue(Context, &MaxSamplingRadius),
					SizeDistribution,
					GetValue(Context, &SizeDistributionPower),
					OutSamples,
					OutSampleRadii,
					OutTriangleIDs, 
					OutBarycentricCoords);

				const int32 NumSamples = OutSamples.Num();

				OutPoints.AddUninitialized(NumSamples);

				for (int32 Idx = 0; Idx < NumSamples; ++Idx)
				{
					OutPoints[Idx] = OutSamples[Idx].GetTranslation();
				}
			}
		}

		SetValue(Context, MoveTemp(OutPoints), &SamplePoints);
		SetValue(Context, MoveTemp(OutSampleRadii), &SampleRadii);
		SetValue(Context, MoveTemp(OutTriangleIDs), &SampleTriangleIDs);
		SetValue(Context, MoveTemp(OutBarycentricCoords), &SampleBarycentricCoords);
		SetValue(Context, OutPoints.Num(), &NumSamplePoints);
	}
}

void FVertexWeightedPointSamplingDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA(&SamplePoints) ||
		Out->IsA(&SampleRadii) ||
		Out->IsA(&SampleTriangleIDs) ||
		Out->IsA(&SampleBarycentricCoords) ||
		Out->IsA(&NumSamplePoints))
	{
		TArray<FVector> OutPoints;
		TArray<FTransform> OutSamples;
		TArray<float> OutSampleRadii;
		TArray<int32> OutTriangleIDs;
		TArray<FVector> OutBarycentricCoords;

		if (TObjectPtr<UDynamicMesh> InTargetMesh = GetValue(Context, &TargetMesh))
		{
			const UE::Geometry::FDynamicMesh3& InDynTargetMesh = InTargetMesh->GetMeshRef();

			if (InDynTargetMesh.VertexCount() > 0)
			{
				if (IsConnected(&VertexWeights))
				{
					FFractureEngineSampling::ComputeVertexWeightedPointSampling(InDynTargetMesh,
						GetValue(Context, &VertexWeights),
						GetValue(Context, &SamplingRadius),
						GetValue(Context, &MaxNumSamples),
						GetValue(Context, &SubSampleDensity),
						GetValue(Context, &RandomSeed),
						GetValue(Context, &MaxSamplingRadius),
						SizeDistribution,
						GetValue(Context, &SizeDistributionPower),
						WeightMode,
						bInvertWeights,
						OutSamples,
						OutSampleRadii,
						OutTriangleIDs,
						OutBarycentricCoords);

					const int32 NumSamples = OutSamples.Num();

					OutPoints.AddUninitialized(NumSamples);

					for (int32 Idx = 0; Idx < NumSamples; ++Idx)
					{
						OutPoints[Idx] = OutSamples[Idx].GetTranslation();
					}

				}
			}
		}

		SetValue(Context, MoveTemp(OutPoints), &SamplePoints);
		SetValue(Context, MoveTemp(OutSampleRadii), &SampleRadii);
		SetValue(Context, MoveTemp(OutTriangleIDs), &SampleTriangleIDs);
		SetValue(Context, MoveTemp(OutBarycentricCoords), &SampleBarycentricCoords);
		SetValue(Context, OutPoints.Num(), &NumSamplePoints);
	}
}


