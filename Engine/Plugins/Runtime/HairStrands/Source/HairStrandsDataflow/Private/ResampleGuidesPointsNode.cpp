// Copyright Epic Games, Inc. All Rights Reserved.

#include "ResampleGuidesPointsNode.h"
#include "GroomCollectionFacades.h"
#include "Dataflow/DataflowObjectInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ResampleGuidesPointsNode)

namespace UE::Groom::Private
{
	FORCEINLINE int32 ComputeNumPoints(const FGroomGuidesFacade& GroomFacade, const int32 GuidePoints) 
	{
		int32 CurveOffset = 0, NumPoints = 0;
		for(int32 ObjectIndex = 0, NumObjects = GroomFacade.GetNumObjects(); ObjectIndex < NumObjects; ++ObjectIndex)
		{
			const uint32 SamplesCount = (GuidePoints != 0) ? GuidePoints : GroomFacade.GetObjectPointSamples()[ObjectIndex];
			NumPoints +=  (GroomFacade.GetObjectCurveOffsets()[ObjectIndex] - CurveOffset) * SamplesCount;
			CurveOffset = GroomFacade.GetObjectCurveOffsets()[ObjectIndex];
		}
		return NumPoints;
	}

	FORCEINLINE void BuildGuidePoints(const FGroomGuidesFacade& GroomFacade, const int32 CurveIndex, const int32 PointOffset, const int32 SampleOffset,
		TArray<FVector3f>& SamplePositions, const int32 GuidePoints) 
	{
		const int32 ObjectIndex = GroomFacade.GetCurveObjectIndices()[CurveIndex];
		const int32 PointsCount = (GroomFacade.GetCurvePointOffsets()[CurveIndex]-PointOffset)-1;
		const int32 SamplesCount = (GuidePoints != 0) ? GuidePoints-1 : GroomFacade.GetObjectPointSamples()[ObjectIndex]-1;

		float CurveLength = 0.0;
		TArray<float> EdgeLengths; EdgeLengths.Init(0.0f, PointsCount);
		for(int32 EdgeIndex = 0; EdgeIndex < PointsCount; ++EdgeIndex)
		{
			EdgeLengths[EdgeIndex] = (GroomFacade.GetPointRestPositions()[EdgeIndex+PointOffset+1] -
				GroomFacade.GetPointRestPositions()[EdgeIndex+PointOffset]).Length();
			CurveLength += EdgeLengths[EdgeIndex];
		}

		SamplePositions[SampleOffset + 1] = GroomFacade.GetPointRestPositions()[PointOffset];
		for(int32 SampleIndex = 1; SampleIndex < SamplesCount-1; ++SampleIndex)
		{
			const float SampleCoord = static_cast<float>(SampleIndex) / (SamplesCount - 1.0f);
			const float SampleLength = CurveLength * SampleCoord;

			float LocalLength = 0.0;
			for(int32 EdgeIndex = 0; EdgeIndex < PointsCount; ++EdgeIndex)
			{
				LocalLength += EdgeLengths[EdgeIndex];
				if(LocalLength >= SampleLength)
				{
					const int32 PrevPoint = EdgeIndex+PointOffset;
					const int32 NextPoint = PrevPoint+1;
					
					const float SampleAlpha = (LocalLength - SampleLength) / EdgeLengths[EdgeIndex];
					SamplePositions[SampleOffset + SampleIndex + 1] = GroomFacade.GetPointRestPositions()[PrevPoint] * SampleAlpha +
						GroomFacade.GetPointRestPositions()[NextPoint] * (1.0-SampleAlpha);
					break;
				}
			}
		}
		SamplePositions[SampleOffset+SamplesCount] = GroomFacade.GetPointRestPositions()[PointOffset+PointsCount];
		SamplePositions[SampleOffset] = 2 * SamplePositions[SampleOffset+1] - SamplePositions[SampleOffset+2];
	}
	
	FORCEINLINE void ResampleGuidesPoints(FGroomGuidesFacade& GroomFacade, const int32 GuidePoints)
	{
		if(GroomFacade.IsValid())
		{
			const int32 NumSamples = ComputeNumPoints(GroomFacade, GuidePoints);
			const int32 NumCurves = GroomFacade.GetNumCurves();

			TArray<FVector3f> SamplePositions;
			SamplePositions.Init(FVector3f::Zero(), NumSamples);

			TArray<int32> CurveOffsets;
			CurveOffsets.Init(0, NumCurves);

			int32 PointOffset = 0;
			int32 SampleOffset = 0;
			for(int32 CurveIndex = 0; CurveIndex < NumCurves; ++CurveIndex)
			{
				BuildGuidePoints(GroomFacade, CurveIndex, PointOffset, SampleOffset, SamplePositions, GuidePoints);
				const uint32 SamplesCount = (GuidePoints != 0) ? GuidePoints : GroomFacade.GetObjectPointSamples()[GroomFacade.GetCurveObjectIndices()[CurveIndex]];
					
				PointOffset = GroomFacade.GetCurvePointOffsets()[CurveIndex];
				SampleOffset += SamplesCount;

				CurveOffsets[CurveIndex] = SampleOffset;
			}
			// Resize the points groups
			GroomFacade.ResizePointsGroups(NumSamples);

			// Set the curve offsets
			GroomFacade.SetCurvePointOffsets(CurveOffsets);

			// Set the point positions and kinematic weights
			GroomFacade.SetPointRestPositions(SamplePositions);
		}
	}
}

void FResampleGuidesPointsDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		FManagedArrayCollection GroomCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		
		UE::Groom::FGroomGuidesFacade GuidesFacade(GroomCollection);

		if(GuidesFacade.IsValid())
		{
			UE::Groom::Private::ResampleGuidesPoints(GuidesFacade, static_cast<uint8>(PointsCount));
		}
		
		SetValue(Context, MoveTemp(GroomCollection), &Collection);
	}
}

