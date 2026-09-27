// Copyright Epic Games, Inc. All Rights Reserved.

#include "GenerateCardsGeometryNode.h"
#include "GenerateCardsClumpsNode.h"
#include "GroomCollectionFacades.h"
#include "HairCardGeneratorEditorModule.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GenerateCardsGeometryNode)

// Geometry Attributes
const FName FGenerateCardsGeometryNode::VertexClumpPositionsAttribute("VertexClumpPositions");
const FName FGenerateCardsGeometryNode::FaceVertexIndicesAttribute("FaceVertexIndices");
const FName FGenerateCardsGeometryNode::VertexCardIndicesAttribute("VertexCardIndices");

const FName FGenerateCardsGeometryNode::CardsVerticesGroup("CardsVertices_LOD");
const FName FGenerateCardsGeometryNode::CardsFacesGroup("CardsFaces_LOD");

void FGenerateCardsGeometryNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection) || Out->IsA<TArray<FGroomCardsSettings>>(&CardsSettings))
	{
		TArray<FGroomCardsSettings> OutputSettings = GetValue<TArray<FGroomCardsSettings>>(Context, &CardsSettings);
		for(FGroomCardsSettings& LODSettings : OutputSettings)
		{
			// Override the generation settings if matching the LOD index and card group
			if(TObjectPtr<UHairCardGeneratorPluginSettings> GenerationSettings = LODSettings.GenerationSettings)
			{
				for(const FCardsGeometrySettings& OverideSettings : GeometrySettings)
				{
					for(TObjectPtr<UHairCardGeneratorGroupSettings> FilterSettings : GenerationSettings->GetFilterGroupSettings())
					{
						if(FilterSettings.Get())
                        {
							if((OverideSettings.FilterName != NAME_None && FilterSettings->GetFilterName() == OverideSettings.FilterName) || OverideSettings.FilterName == NAME_None)
							{
								FilterSettings->TargetTriangleCount = OverideSettings.NumTriangles;
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
						TArray<TArray<FVector3f>> ClumpsGeometry;
						
						bool bHasGeometry = FHairCardGeneratorUtils::RunCardsGeneration(GenerationSettings, LODSettings.PipelineFlags,
                        	[&ClumpsGeometry](const TObjectPtr<const UHairCardGeneratorPluginSettings>& GenerationSettings, const int32 FilterIndex, const uint8 GenFlags)
						{
                        	TArray<TArray<FVector3f>> FilterGeometry;
                        	if(FHairCardGeneratorUtils::GenerateCardsGeometry(GenerationSettings, FilterIndex, GenFlags, FilterGeometry))
                        	{
								ClumpsGeometry.Append(FilterGeometry);
                        		return true;
                        	}
							return false;
						}, false);
   
						if(bHasGeometry)
						{
							TArray<FIntVector3> ClumpsFaces;
							TArray<FVector3f> ClumpsVertices;
							TArray<int32> CardIndices;
							int32 VertexOffset = 0;
   
							int32 CardIndex = 0;
							for(TArray<FVector3f>& GeometryVertices : ClumpsGeometry)
							{
								for(int32 CurveVertex = 0, NumVertices = GeometryVertices.Num()/2; CurveVertex < (NumVertices-1); ++CurveVertex)
								{
									const int32 VertexIndex = CurveVertex * 2 + VertexOffset;
									ClumpsFaces.Add(FIntVector3(VertexIndex, VertexIndex + 2, VertexIndex + 3));
									ClumpsFaces.Add(FIntVector3(VertexIndex + 3, VertexIndex + 1, VertexIndex));
								}
								for(FVector3f& VertexPosition : GeometryVertices)
                                {
									ClumpsVertices.Add(VertexPosition);
									CardIndices.Add(CardIndex);
								}
								VertexOffset = ClumpsVertices.Num();
								++CardIndex;
							}
							
							FString CardsVerticesLODGroup = CardsVerticesGroup.ToString();
							CardsVerticesLODGroup.AppendInt(GenerationSettings->GetLODIndex());
							
							FString CardsFacesLODGroup = CardsFacesGroup.ToString();
							CardsFacesLODGroup.AppendInt(GenerationSettings->GetLODIndex());
							
							TManagedArray<FVector3f>& VertexClumpPositions = GroomCollection.AddAttribute<FVector3f>(VertexClumpPositionsAttribute, FName(CardsVerticesLODGroup));
							TManagedArray<FIntVector3>& FaceVertexIndices = GroomCollection.AddAttribute<FIntVector3>(FaceVertexIndicesAttribute, FName(CardsFacesLODGroup));
							TManagedArray<int32>& VertexCardIndices = GroomCollection.AddAttribute<int32>(VertexCardIndicesAttribute, FName(CardsVerticesLODGroup));
							
							GroomCollection.EmptyGroup(FName(CardsVerticesLODGroup));
							GroomCollection.AddElements(ClumpsVertices.Num(), FName(CardsVerticesLODGroup));
							
							GroomCollection.EmptyGroup(FName(CardsFacesLODGroup));
                            GroomCollection.AddElements(ClumpsFaces.Num(), FName(CardsFacesLODGroup));
							
							for(int32 VertexIndex = 0, NumVertices = ClumpsVertices.Num(); VertexIndex < NumVertices; ++VertexIndex)
							{
								VertexClumpPositions[VertexIndex] = ClumpsVertices[VertexIndex];
								VertexCardIndices[VertexIndex] = CardIndices[VertexIndex];
							}
							for(int32 FaceIndex = 0, NumFaces = ClumpsFaces.Num(); FaceIndex < NumFaces; ++FaceIndex)
							{
								FaceVertexIndices[FaceIndex] = ClumpsFaces[FaceIndex];
							}
						}
					}
				}
			}
			SetValue(Context, MoveTemp(GroomCollection), &Collection);
		}
	}
}

