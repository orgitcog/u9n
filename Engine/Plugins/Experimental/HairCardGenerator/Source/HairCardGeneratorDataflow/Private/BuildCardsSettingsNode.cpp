// Copyright Epic Games, Inc. All Rights Reserved.

#include "BuildCardsSettingsNode.h"

#include "GroomCollectionFacades.h"
#include "HairCardGeneratorEditorModule.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BuildCardsSettingsNode)

namespace UE::CardGen::Private
{
	static void BuildStrandsPositions(const FManagedArrayCollection& GroomCollection, TArray<TArray<FVector>>& StrandsPositions)
	{
		const UE::Groom::FGroomStrandsFacade StrandsFacade(GroomCollection);
		if (StrandsFacade.IsValid())
		{
			const int32 NumStrands = StrandsFacade.GetNumCurves();
			StrandsPositions.SetNum(NumStrands);

			int32 PointOffset = 0;
			for(int32 StrandIndex = 0; StrandIndex < NumStrands; ++StrandIndex)
			{
				const int32 NumPoints = StrandsFacade.GetCurvePointOffsets()[StrandIndex]-PointOffset;
				StrandsPositions[StrandIndex].SetNum(NumPoints);
				
				for(int32 PointIndex = 0; PointIndex < NumPoints; ++PointIndex)
				{
					StrandsPositions[StrandIndex][PointIndex] = FVector(StrandsFacade.GetPointRestPositions()[PointIndex + PointOffset]);
				}
				PointOffset = StrandsFacade.GetCurvePointOffsets()[StrandIndex];
			}
		}
	}
}
void FBuildCardsSettingsNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		FManagedArrayCollection GroomCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		SetValue(Context, MoveTemp(GroomCollection), &Collection);
	}
	else if (Out->IsA<TArray<FGroomCardsSettings>>(&CardsSettings))
	{
		UGroomAsset* LocalAsset = const_cast<UGroomAsset*>(GroomAsset.Get());
		if (!LocalAsset)
		{
			if (const UE::Dataflow::FEngineContext* EngineContext = Context.AsType<UE::Dataflow::FEngineContext>())
			{
				LocalAsset = Cast<UGroomAsset>(EngineContext->Owner);
			}
		}
		TArray<FGroomCardsSettings> OutputSettings;
		if(LocalAsset && !LocalAsset->GetHairGroupsCards().IsEmpty())
		{
			FManagedArrayCollection GroomCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
			for(FHairGroupsCardsSourceDescription& HairCardsDescription : LocalAsset->GetHairGroupsCards())
			{
				if(HairCardsDescription.LODIndex != INDEX_NONE)
				{ 
					HairCardsDescription.Textures = FHairGroupCardsTextures();
				
					OutputSettings.AddDefaulted();
					FHairCardGeneratorUtils::BuildGenerationSettings(false, LocalAsset, HairCardsDescription,
						OutputSettings.Last().GenerationSettings, OutputSettings.Last().GenerationFlags, OutputSettings.Last().PipelineFlags);
				
					if(!FilterSettings.IsEmpty())
					{
						TArray<TArray<FName>> FilterCardGroups;
						TArray<FName> FilterGroupNames;
						for(const FGroomFilterSettings& LocalSettings : FilterSettings)
						{
							if((LocalSettings.GroupIndex == HairCardsDescription.GroupIndex) &&
								(LocalSettings.LODIndex == HairCardsDescription.LODIndex) && (HairCardsDescription.LODIndex != INDEX_NONE))
							{
								FilterCardGroups.Add(LocalSettings.CardGroups);
								FilterGroupNames.Add(LocalSettings.FilterName);
							}
						}
						OutputSettings.Last().GenerationSettings->BuildFilterGroupSettings(FilterCardGroups, FilterGroupNames);
					}

					FHairCardGeneratorUtils::LoadGroomStrands(LocalAsset, [&GroomCollection](TArray<TArray<FVector>>& StrandsPositions)
					{
						UE::CardGen::Private::BuildStrandsPositions(GroomCollection, StrandsPositions);
					});
					OutputSettings.Last().GenerationSettings->UpdateStrandFilterAssignment();
				}
			}
		}
		SetValue(Context, MoveTemp(OutputSettings), &CardsSettings);
	}
}

