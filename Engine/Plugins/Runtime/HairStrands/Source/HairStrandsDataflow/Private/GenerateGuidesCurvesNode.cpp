// Copyright Epic Games, Inc. All Rights Reserved.

#include "GenerateGuidesCurvesNode.h"

#include "GroomBindingBuilder.h"
#include "GroomCollectionFacades.h"
#include "Dataflow/DataflowObjectInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GenerateGuidesCurvesNode)

namespace UE::Groom::Private
{
	FORCEINLINE uint32 SampleStrandsCurves(const UE::Groom::FGroomStrandsFacade& StrandsFacade, const uint32 GuidesCount, const uint32 GuidesOffset,
		const uint32 ObjectIndex, const uint32 PrevStrands, const uint32 NextStrands, TArray<uint32>& SampleIndices)
	{
		const uint32 NumStrands = NextStrands-PrevStrands;
		uint32 NumGuides = (ObjectIndex == StrandsFacade.GetNumObjects()-1) ? GuidesCount - GuidesOffset :
			GuidesCount * static_cast<float>(NumStrands)/StrandsFacade.GetNumCurves();
		NumGuides = FMath::Max(1u, NumGuides);

		TArray<FVector3f> RootPositions;
		RootPositions.Init(FVector3f::ZeroVector, NumStrands);
		for(uint32 StrandIndex = 0; StrandIndex < NumStrands; ++StrandIndex)
		{
			const uint32 RootIndex = (StrandIndex+PrevStrands == 0) ? 0 : StrandsFacade.GetCurvePointOffsets()[StrandIndex+PrevStrands-1];
			RootPositions[StrandIndex] = StrandsFacade.GetPointRestPositions()[RootIndex];
		}

		TArray<bool> ValidPoints;
		ValidPoints.Init(true, NumStrands);
				
		GroomBinding_RBFWeighting::FPointsSampler PointsSampler(ValidPoints, RootPositions.GetData(), NumGuides);

		SampleIndices = PointsSampler.SampleIndices;
		return NumGuides;
	}

	FORCEINLINE void BuildGuidesCurves(const UE::Groom::FGroomStrandsFacade& StrandsFacade, const uint32 PrevStrands, const TArray<uint32>& SampleIndices,
		TArray<FVector3f>& PointRestPositions, TArray<int32>& ObjectCurveOffsets, TArray<int32>& CurvePointOffsets, TArray<int32>& CurveStrandIndices)
	{
		for(uint32 SampleIndex : SampleIndices)
		{
			const uint32 GuideIndex = SampleIndex + PrevStrands;
			const int32 PointBegin = (GuideIndex == 0) ? 0 : StrandsFacade.GetCurvePointOffsets()[GuideIndex-1];
			const int32 PointEnd = StrandsFacade.GetCurvePointOffsets()[GuideIndex];

			for(int32 PointIndex = PointBegin; PointIndex < PointEnd; ++PointIndex)
			{
				PointRestPositions.Add(StrandsFacade.GetPointRestPositions()[PointIndex]);
			}
			CurvePointOffsets.Add(PointRestPositions.Num());
			CurveStrandIndices.Add(SampleIndex);
		}
		ObjectCurveOffsets.Add(CurvePointOffsets.Num());
	}
}

void FGenerateGuidesCurvesDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		FManagedArrayCollection GroomCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		
		UE::Groom::FGroomStrandsFacade StrandsFacade(GroomCollection);
		if(StrandsFacade.IsValid())
		{
			TArray<FVector3f> PointRestPositions;
			TArray<int32> ObjectCurveOffsets, CurvePointOffsets, CurveStrandIndices;
			TArray<FString> ObjectGroupNames;
			
			uint32 PrevOffset = 0, GuidesOffset = 0;
			for (int32 ObjectIndex = 0; ObjectIndex < StrandsFacade.GetNumObjects(); ++ObjectIndex)
			{
				const uint32 NextOffset = StrandsFacade.GetObjectCurveOffsets()[ObjectIndex];

				TArray<uint32> SampleIndices;
				
				// Sample guides among the input strands
				const uint32 NumGuides = UE::Groom::Private::SampleStrandsCurves(StrandsFacade, GuidesCount, GuidesOffset, ObjectIndex, PrevOffset, NextOffset, SampleIndices);

				// Build the sampled guides
				UE::Groom::Private::BuildGuidesCurves(StrandsFacade, PrevOffset, SampleIndices, PointRestPositions, ObjectCurveOffsets, CurvePointOffsets, CurveStrandIndices);
				
				const FString GroupName = StrandsFacade.GetObjectGroupNames()[ObjectIndex].Replace(
					*UE::Groom::FGroomStrandsFacade::GroupPrefix.ToString(), *UE::Groom::FGroomGuidesFacade::GroupPrefix.ToString());
				
				ObjectGroupNames.Add(GroupName);
				
				GuidesOffset += NumGuides;
				PrevOffset = NextOffset;
			}
			
			UE::Groom::FGroomGuidesFacade GuidesFacade(GroomCollection);
			const TArray<int32> ObjectPointSamples = GuidesFacade.GetObjectPointSamples();

			// Init the groom collection
			GuidesFacade.InitGroomCollection(PointRestPositions, CurvePointOffsets, ObjectCurveOffsets, ObjectGroupNames);

			// Set the curve strand indices
			GuidesFacade.SetCurveStrandIndices(CurveStrandIndices);

			// Set the point samples if already defined and if the size is matching
			if(ObjectPointSamples.Num() == GuidesFacade.GetNumObjects())
			{
				GuidesFacade.SetObjectPointSamples(ObjectPointSamples);
			}
		}
		
		SetValue(Context, MoveTemp(GroomCollection), &Collection);
	}
}

