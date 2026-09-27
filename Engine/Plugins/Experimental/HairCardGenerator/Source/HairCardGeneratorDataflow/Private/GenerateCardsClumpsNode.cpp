// Copyright Epic Games, Inc. All Rights Reserved.

#include "GenerateCardsClumpsNode.h"
#include "GroomCollectionFacades.h"
#include "HairCardGeneratorEditorModule.h"
#include "Dataflow/DataflowDebugDrawInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GenerateCardsClumpsNode)

// Clumps Attributes
const FName FGenerateCardsClumpsNode::CurveClumpIndicesAttribute("CurveClumpIndices_LOD");
const FName FGenerateCardsClumpsNode::ObjectNumClumpsAttribute("ObjectNumClumps_LOD");

void FGenerateCardsClumpsNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection) || Out->IsA<TArray<FGroomCardsSettings>>(&CardsSettings))
	{
		TArray<FGroomCardsSettings> OutputSettings = GetValue<TArray<FGroomCardsSettings>>(Context, &CardsSettings);
		for(FGroomCardsSettings& LODSettings : OutputSettings)
		{
			// Override the generation settings if matching the LOD index and card group
			if(TObjectPtr<UHairCardGeneratorPluginSettings>
				GenerationSettings = LODSettings.GenerationSettings)
			{
				for(const FCardsClumpsSettings& OverideSettings : ClumpsSettings)
				{
					for(TObjectPtr<UHairCardGeneratorGroupSettings> FilterSettings : GenerationSettings->GetFilterGroupSettings())
					{
						if(FilterSettings.Get())
						{
							if((OverideSettings.FilterName != NAME_None && FilterSettings->GetFilterName() == OverideSettings.FilterName) || OverideSettings.FilterName == NAME_None)
							{
								FilterSettings->TargetNumberOfCards = OverideSettings.NumCards;
								FilterSettings->MaxNumberOfFlyaways = OverideSettings.NumFlyaways;
							}
						}
					}
				}
			}
		}
		if(Out->IsA<TArray<FGroomCardsSettings>>(&CardsSettings))
		{
			SetValue(Context, MoveTemp(OutputSettings), &CardsSettings);
		}
		else if(Out->IsA<FManagedArrayCollection>(&Collection))
		{
			FManagedArrayCollection GroomCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
			
			for(FGroomCardsSettings& LODSettings : OutputSettings)
			{
				if(TObjectPtr<UHairCardGeneratorPluginSettings> GenerationSettings = LODSettings.GenerationSettings)
				{
					if(FHairCardGeneratorUtils::LoadGenerationSettings(GenerationSettings))
					{
						TArray<int32> StrandsClumps;
						int32 NumClumps = 0;

						StrandsClumps.Init(INDEX_NONE, GroomCollection.NumElements(UE::Groom::FGroomStrandsFacade::CurvesGroup));
						
						bool bHasClumps = FHairCardGeneratorUtils::RunCardsGeneration(GenerationSettings, LODSettings.PipelineFlags,
				[&StrandsClumps, &NumClumps](const TObjectPtr<const UHairCardGeneratorPluginSettings>& GenerationSettings, const int32 FilterIndex, const uint8 GenFlags)
						{
							TArray<int32> FilterClumps; int32 ClumpCount = 0;
							if(FHairCardGeneratorUtils::GenerateCardsClumps(GenerationSettings, FilterIndex, GenFlags, FilterClumps, ClumpCount))
							{ 
								for (int32 CurveIndex = 0, NumCurves = FilterClumps.Num()-1; CurveIndex < NumCurves; ++CurveIndex)
								{
									if (StrandsClumps.IsValidIndex(CurveIndex) && (FilterClumps[CurveIndex] != INDEX_NONE))
									{
										StrandsClumps[CurveIndex] = FilterClumps[CurveIndex] + NumClumps;
									}
								}
								NumClumps += ClumpCount;
								return true;
							}
							return false;
						}, false);
			
 						if(bHasClumps)
						{
							FString ClumpIndicesLOD = CurveClumpIndicesAttribute.ToString();
							ClumpIndicesLOD.AppendInt(GenerationSettings->GetLODIndex());
			
							FString NumClumpsLOD = ObjectNumClumpsAttribute.ToString();
							NumClumpsLOD.AppendInt(GenerationSettings->GetLODIndex());
					
							TManagedArray<int32>& CurveClumpIndices = GroomCollection.AddAttribute<int32>(FName(ClumpIndicesLOD), UE::Groom::FGroomStrandsFacade::CurvesGroup);
							TManagedArray<int32>& ObjectNumClumps = GroomCollection.AddAttribute<int32>(FName(NumClumpsLOD), UE::Groom::FGroomStrandsFacade::ObjectsGroup);
							
							for(int32 CurveIndex = 0, NumCurves = CurveClumpIndices.Num(); CurveIndex < NumCurves; ++CurveIndex)
							{
								CurveClumpIndices[CurveIndex] = StrandsClumps[CurveIndex];
							}
							for(int32 ObjectIndex = 0, NumObjects = ObjectNumClumps.Num(); ObjectIndex < NumObjects; ++ObjectIndex)
							{
								ObjectNumClumps[ObjectIndex] = NumClumps;
							}
						}
					}
				}
			}
			SetValue(Context, MoveTemp(GroomCollection), &Collection);
		}
	}
}

