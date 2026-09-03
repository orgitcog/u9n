// Copyright Epic Games, Inc. All Rights Reserved.

#include "GetGroomAssetNode.h"
#include "GroomEdit.h"
#include "GroomInstance.h"
#include "GroomCollectionFacades.h"
#include "Dataflow/DataflowInputOutput.h"
#include "Dataflow/DataflowObjectInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GetGroomAssetNode)

namespace UE::Groom::Private
{
	FORCEINLINE const UGroomAsset* FillEditableAsset(Dataflow::FContext& Context, const UGroomAsset* NodeGroom, FEditableGroom& EditGroom)
	{
		const UGroomAsset* GroomAsset = NodeGroom;
		if (!GroomAsset)
		{
			if (const Dataflow::FEngineContext* EngineContext = Context.AsType<Dataflow::FEngineContext>())
			{
				GroomAsset = Cast<UGroomAsset>(EngineContext->Owner);
			}
		}

		if (GroomAsset)
		{
			ConvertFromGroomAsset(const_cast<UGroomAsset*>(GroomAsset), &EditGroom, false, false, false);
		}
		return GroomAsset;
	}
	
	template<typename FacadeType>
	FORCEINLINE void BuildGroomCollection(FManagedArrayCollection& GroomCollection, FEditableGroom& EditGroom, const FString& GroomName)
	{
		TArray<FVector3f> PointRestPositions;
		TArray<int32> ObjectCurveOffsets;
		TArray<int32> CurvePointOffsets;
		TArray<FString> ObjectGroupNames;

		uint32 GroupIndex = 0;
		for(FEditableGroomGroup& GroomGroup : EditGroom.Groups)
		{
			for( const typename FacadeType::FEditableType& EditType : FacadeType::GetEditableGroom(GroomGroup))
			{
				for(auto& GuidePoint : EditType.ControlPoints)
				{
					PointRestPositions.Add(GuidePoint.Position);
				}
				CurvePointOffsets.Add(PointRestPositions.Num());
			}
			ObjectCurveOffsets.Add(CurvePointOffsets.Num());
			
			const FString GroupName = GroomName + TEXT("_") + FacadeType::GroupPrefix.ToString();
			ObjectGroupNames.Add(GroupName);
			++GroupIndex;
		}
		FacadeType CurvesFacade(GroomCollection);
		CurvesFacade.InitGroomCollection(PointRestPositions, CurvePointOffsets, ObjectCurveOffsets, ObjectGroupNames);
	}
}

void FGetGroomAssetDataflowNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		FManagedArrayCollection GroomCollection;
		FEditableGroom EditGroom;
		if(const UGroomAsset* LocalGroom = UE::Groom::Private::FillEditableAsset(Context, GroomAsset, EditGroom))
		{
			// Fill the guides facade
			UE::Groom::Private::BuildGroomCollection<UE::Groom::FGroomGuidesFacade>(
					GroomCollection, EditGroom, LocalGroom->GetName());

			// Fill the strands facade
			UE::Groom::Private::BuildGroomCollection<UE::Groom::FGroomStrandsFacade>(
					GroomCollection, EditGroom, LocalGroom->GetName());

			// Add guides points samples for future resampling
			TArray<int32> ObjectPointSamples, CurveStrandIndices;
			const TArray<FHairGroupsPhysics>& GroupsPhysics = LocalGroom->GetHairGroupsPhysics();
			for(const FHairGroupsPhysics& GroupPhysics : GroupsPhysics)
			{
				ObjectPointSamples.Add(static_cast<uint8>(GroupPhysics.StrandsParameters.StrandsSize));
			}
			
			UE::Groom::FGroomGuidesFacade GuidesFacade(GroomCollection);
			if(GroomCollection.NumElements(UE::Groom::FGroomGuidesFacade::ObjectsGroup) == 0)
			{
				GroomCollection.AddElements(ObjectPointSamples.Num(), UE::Groom::FGroomGuidesFacade::ObjectsGroup);
			}
			GuidesFacade.SetObjectPointSamples(ObjectPointSamples);
		}

		SetValue(Context, MoveTemp(GroomCollection), &Collection);
	}
}

TArray<UE::Dataflow::FRenderingParameter> FGetGroomAssetDataflowNode::GetRenderParametersImpl() const
{
	if(CurvesType == EGroomCollectionType::Guides)
	{
		return { {TEXT("GuidesRender"), FName("FGroomCollection"), {TEXT("Collection")}}};
	}
	else
	{
		return { {TEXT("StrandsRender"), FName("FGroomCollection"), {TEXT("Collection")}}};
	}
}
