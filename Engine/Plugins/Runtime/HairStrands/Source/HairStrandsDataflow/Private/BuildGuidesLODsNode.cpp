// Copyright Epic Games, Inc. All Rights Reserved.

#include "BuildGuidesLODsNode.h"
#include "GroomCollectionFacades.h"
#include "Dataflow/DataflowObjectInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BuildGuidesLODsNode)

namespace UE::Groom::Private
{
	FORCEINLINE float ComputeGuidesMetric(const FGroomGuidesFacade& GuidesFacade, const uint32 GuideIndexA, const uint32 GuideIndexB, const float& GuideLengthA, const float& GuideLengthB,
										 const float ShapeWeight, const float ProximityWeight)
	{
		const uint32 PointOffsetA = (GuideIndexA > 0) ? GuidesFacade.GetCurvePointOffsets()[GuideIndexA-1] : 0;
		const uint32 PointOffsetB = (GuideIndexB > 0) ? GuidesFacade.GetCurvePointOffsets()[GuideIndexB-1] : 0;
		
		const uint32 NumPointsA = GuidesFacade.GetCurvePointOffsets()[GuideIndexA] - PointOffsetA;
		const uint32 NumPointsB = GuidesFacade.GetCurvePointOffsets()[GuideIndexB] - PointOffsetB;

		if(NumPointsA == NumPointsB)
		{
			float ProximityMetric = 0.0f;
			float ShapeMetric = 0.0f;
			for(uint32 PointIndex = 0; PointIndex < NumPointsA; ++PointIndex)
			{
				const FVector3f& GuidePositionA = GuidesFacade.GetPointRestPositions()[PointIndex+PointOffsetA];
				const FVector3f& GuidePositionB = GuidesFacade.GetPointRestPositions()[PointIndex+PointOffsetB];
				
				ProximityMetric += (GuidePositionB - GuidePositionA).Size();
				ShapeMetric += (GuidePositionB - GuidesFacade.GetPointRestPositions()[PointOffsetB] - GuidePositionA + GuidesFacade.GetPointRestPositions()[PointOffsetA]).Size();
			}

			const float MetricScale = 1.0f / (NumPointsA * 0.5f * (GuideLengthA+GuideLengthB));
			return FMath::Exp(-ShapeWeight * ShapeMetric * MetricScale) * FMath::Exp(-ProximityWeight * ProximityMetric * MetricScale);
		}
		return 0.0f;
	}

	FORCEINLINE void BuildGuidesLODs(FGroomGuidesFacade& GuidesFacade)
	{
		const uint32 NumObjects = GuidesFacade.GetNumObjects();
		
		TArray<int32> CurveParentIndices; CurveParentIndices.Init(INDEX_NONE, GuidesFacade.GetNumCurves());
		TArray<int32> CurveLodIndices; CurveLodIndices.Init(INDEX_NONE, GuidesFacade.GetNumCurves());
		
		uint32 CurveOffset = 0, PointOffset = 0;
		for(uint32 ObjectIndex = 0; ObjectIndex < NumObjects; ++ObjectIndex)
		{ 
			TArray<float> GuidesLengths; GuidesLengths.Init(0.0f, GuidesFacade.GetObjectCurveOffsets()[ObjectIndex] - CurveOffset);
			for(int32 GuideIndex = CurveOffset; GuideIndex < GuidesFacade.GetObjectCurveOffsets()[ObjectIndex]; ++GuideIndex)
			{
				for(int32 PointIndex = PointOffset+1; PointIndex < GuidesFacade.GetCurvePointOffsets()[GuideIndex]; ++PointIndex)
				{
					GuidesLengths[GuideIndex-CurveOffset] += (GuidesFacade.GetPointRestPositions()[PointIndex]-GuidesFacade.GetPointRestPositions()[PointIndex-1]).Size();
				}
				PointOffset = GuidesFacade.GetCurvePointOffsets()[GuideIndex];
			}
			const int32 NumObjectLODs = FMath::CeilLogTwo(GuidesLengths.Num());
			ParallelFor(GuidesLengths.Num(), [CurveOffset, &GuidesLengths, &GuidesFacade, &CurveParentIndices, &CurveLodIndices, &NumObjectLODs](int32 CurveIndex)
			{
				const uint32 GuideIndex = CurveIndex + CurveOffset;
				const uint32 GuideLod = NumObjectLODs - 1 - FMath::FloorLog2(CurveIndex);
				const uint32 LodOffset = (GuideLod == NumObjectLODs - 1) ? 0 : 1 << FMath::FloorLog2(CurveIndex);

				float MinMetric = FLT_MAX;
				int32 MinParent = INDEX_NONE;
				for(uint32 ParentIndex = CurveOffset; ParentIndex < CurveOffset+LodOffset; ++ParentIndex)
				{
					const float ParentMetric = 1.0f - ComputeGuidesMetric(GuidesFacade,
						GuideIndex, ParentIndex, GuidesLengths[CurveIndex], GuidesLengths[ParentIndex-CurveOffset],  1.0f, 1.0f);
					if (ParentMetric < MinMetric)
					{
						MinMetric = ParentMetric;
						MinParent = ParentIndex;
					}
				}
				CurveParentIndices[GuideIndex] = MinParent;
				CurveLodIndices[GuideIndex] = GuideLod;

				//UE_LOG(LogTemp, Log, TEXT("Guide Index = %d, Num Guides = %d, Num LODs = %d, Guide LOD = %d, LOD Offset = %d, Parent Index = %d"),
				//	CurveIndex, GuidesLengths.Num(), NumObjectLODs, GuideLod, LodOffset, MinParent);
			}, EParallelForFlags::None);

			GuidesFacade.SetCurveParentIndices(CurveParentIndices);
			GuidesFacade.SetCurveLodIndices(CurveLodIndices);
			CurveOffset = GuidesFacade.GetObjectCurveOffsets()[ObjectIndex];
		}
	}

	FCollectionAttributeKey GetCurveParentsKey()
    {
    	FCollectionAttributeKey Key;
    	Key.Group =  UE::Groom::FGroomGuidesFacade::CurvesGroup.ToString();
    	Key.Attribute = UE::Groom::FGroomGuidesFacade::CurveParentIndicesAttribute.ToString();
    	return Key;
    }

	FCollectionAttributeKey GetCurveLodsKey()
	{
		FCollectionAttributeKey Key;
		Key.Group =  UE::Groom::FGroomGuidesFacade::CurvesGroup.ToString();
		Key.Attribute = UE::Groom::FGroomGuidesFacade::CurveLodIndicesAttribute.ToString();
		return Key;
	}
}

void FBuildGuidesLODsDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		FManagedArrayCollection GroomCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		
		UE::Groom::FGroomGuidesFacade GuidesFacade(GroomCollection);

		if(GuidesFacade.IsValid())
		{
			UE::Groom::Private::BuildGuidesLODs(GuidesFacade);
		}
		
		SetValue(Context, MoveTemp(GroomCollection), &Collection);
	}
	else if (Out->IsA<FCollectionAttributeKey>(&CurveParentsKey))
	{
		SetValue(Context, UE::Groom::Private::GetCurveParentsKey(), &CurveParentsKey);
	}
	else if (Out->IsA<FCollectionAttributeKey>(&CurveLodsKey))
	{
		SetValue(Context, UE::Groom::Private::GetCurveLodsKey(), &CurveLodsKey);
	}
}

