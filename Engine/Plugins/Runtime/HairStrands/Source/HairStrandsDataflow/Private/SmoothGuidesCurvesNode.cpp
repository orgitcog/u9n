// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmoothGuidesCurvesNode.h"
#include "GroomCollectionFacades.h"
#include "Dataflow/DataflowObjectInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SmoothGuidesCurvesNode)

namespace UE::Groom::Private
{
	FORCEINLINE void SmoothGuidesPoints(FGroomGuidesFacade& GroomFacade, const float SmoothingFactor)
	{
		if(GroomFacade.IsValid())
		{
			const int32 NumPoints = GroomFacade.GetNumPoints();
			const int32 NumCurves = GroomFacade.GetNumCurves();

			TArray<FVector3f> SmoothedPositions;
			SmoothedPositions.Init(FVector3f::Zero(), NumPoints);

			int32 PointOffset = 0;
			for(int32 CurveIndex = 0; CurveIndex < NumCurves; ++CurveIndex)
			{
				FVector3f DirM1 = GroomFacade.GetPointRestPositions()[PointOffset+1] - GroomFacade.GetPointRestPositions()[PointOffset];
				FVector3f DirM2 = DirM1;

				const float Gamma1 = 2.0 * (1.0-SmoothingFactor);
				const float Gamma2 = - (1.0-SmoothingFactor)*(1.0-SmoothingFactor);
				const float Gamma3 = SmoothingFactor*SmoothingFactor;
				
				SmoothedPositions[PointOffset] = GroomFacade.GetPointRestPositions()[PointOffset];
				
				for(int32 PointIndex = PointOffset; PointIndex < GroomFacade.GetCurvePointOffsets()[CurveIndex]-1; ++PointIndex)
				{
					const FVector3f DirM3 = GroomFacade.GetPointRestPositions()[PointIndex+1] - GroomFacade.GetPointRestPositions()[PointIndex];
					const FVector3f DirMi = Gamma1 * DirM1 + Gamma2 * DirM2 + Gamma3 * DirM3;

					SmoothedPositions[PointIndex+1] = SmoothedPositions[PointIndex] + DirMi;
					
					DirM2 = DirM1;
					DirM1 = DirMi;
				}
				PointOffset = GroomFacade.GetCurvePointOffsets()[CurveIndex];
			}

			// Set the point positions and kinematic weights
			GroomFacade.SetPointRestPositions(SmoothedPositions);
		}
	}
}

void FSmoothGuidesCurvesDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		FManagedArrayCollection GroomCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		
		UE::Groom::FGroomGuidesFacade GuidesFacade(GroomCollection);

		if(GuidesFacade.IsValid())
		{
			UE::Groom::Private::SmoothGuidesPoints(GuidesFacade, 1.0-SmoothingFactor);
		}
		
		SetValue(Context, MoveTemp(GroomCollection), &Collection);
	}
}

