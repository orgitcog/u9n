// Copyright Epic Games, Inc. All Rights Reserved.

#include "AttachGuidesRootsNode.h"
#include "GroomCollectionFacades.h"
#include "Dataflow/DataflowObjectInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AttachGuidesRootsNode)

namespace UE::Groom::Private
{
	FCollectionAttributeKey GetKinematicWeightsKey()
	{
		FCollectionAttributeKey Key;
		Key.Group =  UE::Groom::FGroomGuidesFacade::VerticesGroup.ToString();
		Key.Attribute = UE::Groom::FGroomGuidesFacade::PointKinematicWeightsAttribute.ToString();
		return Key;
	}
}

void FAttachGuidesRootsDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		FManagedArrayCollection GroomCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		
		UE::Groom::FGroomGuidesFacade GuidesFacade(GroomCollection);

		if(GuidesFacade.IsValid())
		{
			TArray<float> KinematicWeights = GuidesFacade.GetPointKinematicWeights();
			
			const int32 LocalIndex = GroupIndex;
			ParallelFor(GuidesFacade.GetNumPoints(), [&KinematicWeights, &GuidesFacade, LocalIndex](int32 PointIndex)
			{
				const int32 CurveIndex = GuidesFacade.GetPointCurveIndices()[PointIndex];
				const int32 ObjectIndex = GuidesFacade.GetCurveObjectIndices()[CurveIndex];

				if(LocalIndex == INDEX_NONE || ObjectIndex == LocalIndex)
				{
					const int32 PointOffset = (CurveIndex == 0) ? 0 : GuidesFacade.GetCurvePointOffsets()[CurveIndex-1];
					const int32 LocalPoint = PointIndex - PointOffset;

					if(LocalPoint < 2)
					{
						KinematicWeights[2*PointIndex] = 1.0f;
						KinematicWeights[2*PointIndex+1] = 1.0f;
					}
				}
			}, EParallelForFlags::None);
			GuidesFacade.SetPointKinematicWeights(KinematicWeights);
		}
		
		SetValue(Context, MoveTemp(GroomCollection), &Collection);
	}
	else if (Out->IsA<FCollectionAttributeKey>(&KinematicWeightsKey))
	{
		SetValue(Context, UE::Groom::Private::GetKinematicWeightsKey(), &KinematicWeightsKey);
	}
}


