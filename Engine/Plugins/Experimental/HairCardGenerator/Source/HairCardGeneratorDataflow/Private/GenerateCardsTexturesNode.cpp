// Copyright Epic Games, Inc. All Rights Reserved.

#include "GenerateCardsTexturesNode.h"

#include "GenerateCardsGeometryNode.h"
#include "GroomCollectionFacades.h"
#include "HairCardGeneratorEditorModule.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GenerateCardsTexturesNode)

// Texture Attributes
const FName FGenerateCardsTexturesNode::ObjectTextureIndicesAttribute("ObjectTextureIndices");
const FName FGenerateCardsTexturesNode::CardsObjectsGroup("CardsObjects_LOD");
const FName FGenerateCardsTexturesNode::VertexTextureUVsAttribute("VertexTextureUVs");

void FGenerateCardsTexturesNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection) || Out->IsA<TArray<FGroomCardsSettings>>(&CardsSettings))
	{
		TArray<FGroomCardsSettings> OutputSettings = GetValue<TArray<FGroomCardsSettings>>(Context, &CardsSettings);
		for(FGroomCardsSettings& LODSettings : OutputSettings)
		{
			// Override the generation settings if matching the LOD index and card group
			if(TObjectPtr<UHairCardGeneratorPluginSettings> GenerationSettings = LODSettings.GenerationSettings)
			{
				for(const FCardsTextureSettings& OverideSettings : TextureSettings)
				{
					for(TObjectPtr<UHairCardGeneratorGroupSettings> FilterSettings : GenerationSettings->GetFilterGroupSettings())
					{
						if (FilterSettings.Get())
						{
							if ((OverideSettings.FilterName != NAME_None && FilterSettings->GetFilterName() == OverideSettings.FilterName) || OverideSettings.FilterName == NAME_None)
							{
								FilterSettings->NumberOfTexturesInAtlas = OverideSettings.NumTextures;
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
		else if (Out->IsA<FManagedArrayCollection>(&Collection))
		{
			FManagedArrayCollection GroomCollection = GetValue<FManagedArrayCollection>(Context, &Collection);

			for(FGroomCardsSettings& LODSettings : OutputSettings)
			{
				if(TObjectPtr<UHairCardGeneratorPluginSettings> GenerationSettings = LODSettings.GenerationSettings)
				{
					if(FHairCardGeneratorUtils::LoadGenerationSettings(GenerationSettings))
					{
						TArray<int32> CardsTextures;
						TArray<FVector2f> VertexUVs;
						
						bool bHasTextures = FHairCardGeneratorUtils::RunCardsGeneration(GenerationSettings, LODSettings.PipelineFlags,
							[&CardsTextures, &VertexUVs](const TObjectPtr<const UHairCardGeneratorPluginSettings>& GenerationSettings, const int32 FilterIndex, const uint8 GenFlags)
						{
							TArray<int32> FilterTextures;
							int32 TexturesCount = 0;
							if(FHairCardGeneratorUtils::GenerateCardsTexturesClusters(GenerationSettings, FilterIndex, GenFlags, FilterTextures, TexturesCount))
							{
								const int32 CardCount = FilterTextures.Num() - 1 - TexturesCount;
								const int32 CardOffset = CardsTextures.Num();

								CardsTextures.Reserve(CardOffset + CardCount);
								for (int32 CardIndex = 0; CardIndex < CardCount; ++CardIndex)
								{
									CardsTextures.Add((FilterTextures[CardIndex] < TexturesCount) ? FilterTextures[CardCount + FilterTextures[CardIndex]] + CardOffset : INDEX_NONE);
								}
								return true;
							}
							return false;
						}, false);
			
						if(bHasTextures)
						{
							TArray<float> FilterUVs;
							if(FHairCardGeneratorUtils::GenerateTexturesLayoutAndAtlases(GenerationSettings, LODSettings.PipelineFlags, FilterUVs))
							{
								VertexUVs.Reserve(FilterUVs.Num());
								int32 UVsIndex = 0;
								while (UVsIndex < FilterUVs.Num())
								{
									if (FilterUVs[UVsIndex] != -1.0)
									{
										VertexUVs.Add(FVector2f(FilterUVs[UVsIndex], FilterUVs[UVsIndex + 1]));
										UVsIndex += 2;
									}
									else
									{
										UVsIndex += 1;
									}
								}
								FString CardsObjectsLODGroup = CardsObjectsGroup.ToString();
								CardsObjectsLODGroup.AppendInt(GenerationSettings->GetLODIndex());
			
								FString CardsVerticesLODGroup = FGenerateCardsGeometryNode::CardsVerticesGroup.ToString();
								CardsVerticesLODGroup.AppendInt(GenerationSettings->GetLODIndex());
							
								TManagedArray<int32>& ObjectTextureIndices = GroomCollection.AddAttribute<int32>(ObjectTextureIndicesAttribute, FName(CardsObjectsLODGroup));
								TManagedArray<FVector2f>& VertexTextureUvs = GroomCollection.AddAttribute<FVector2f>(VertexTextureUVsAttribute, FName(CardsVerticesLODGroup));
							
								GroomCollection.EmptyGroup(FName(CardsObjectsLODGroup));
								GroomCollection.AddElements(CardsTextures.Num(), FName(CardsObjectsLODGroup));
							
								for(int32 CardIndex = 0, NumCards = CardsTextures.Num(); CardIndex < NumCards; ++CardIndex)
								{
									ObjectTextureIndices[CardIndex] = CardsTextures[CardIndex];
								}

								if(VertexUVs.Num() == VertexTextureUvs.Num())
								{
									for(int32 VertexIndex = 0, NumVertices = VertexUVs.Num(); VertexIndex < NumVertices; ++VertexIndex)
									{
										VertexTextureUvs[VertexIndex] = VertexUVs[VertexIndex];
									}
								}
							}
						}
					}
				}
			}
			SetValue(Context, MoveTemp(GroomCollection), &Collection);
		}
	}
}

