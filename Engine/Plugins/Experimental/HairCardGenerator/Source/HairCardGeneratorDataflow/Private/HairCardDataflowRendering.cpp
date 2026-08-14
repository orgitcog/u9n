// Copyright Epic Games, Inc. All Rights Reserved.

#include "HairCardDataflowRendering.h"
#include "GenerateCardsGeometryNode.h"
#include "GenerateCardsClumpsNode.h"
#include "GenerateCardsTexturesNode.h"
#include "GroomCollectionFacades.h"
#include "GeometryCollection/Facades/CollectionRenderingFacade.h"

namespace UE::CardGen::Private
{
	int32 GGroomDataflowCardsLod = 0;
	static FAutoConsoleVariableRef CVarGroomDataflowCardsLod(
		 TEXT("p.Groom.Dataflow.CardsLod"),
		 GGroomDataflowCardsLod,
		 TEXT("Cards Lod we want to display")
	 );

	float GGroomDataflowCardsAlpha = 0.1;
	static FAutoConsoleVariableRef CVarGroomDataflowCardsAlpha(
		 TEXT("p.Groom.Dataflow.CardsAlpha"),
		 GGroomDataflowCardsAlpha,
		 TEXT("Cards Alpha for the rendering")
	 );
	
	Dataflow::FRenderKey FCardsGeometryRenderingCallbacks::RenderKey = { "GeometryRender", FName("FCardsCollection") };
	
	void FCardsGeometryRenderingCallbacks::Render(GeometryCollection::Facades::FRenderingFacade& RenderCollection, const Dataflow::FGraphRenderingState& State)
	{
		if (State.GetRenderOutputs().Num())
		{
			const FManagedArrayCollection Default;
			checkf(State.GetRenderOutputs().Num() == 1, TEXT("Expected FGraphRenderingState object to have one render output"));
			const FName PrimaryOutput = State.GetRenderOutputs()[0];
			const FManagedArrayCollection& Collection = State.GetValue<FManagedArrayCollection>(PrimaryOutput, Default);

			FString CardsVerticesLODGroup = FGenerateCardsGeometryNode::CardsVerticesGroup.ToString();
			CardsVerticesLODGroup.AppendInt(GGroomDataflowCardsLod);
							
			FString CardsFacesLODGroup = FGenerateCardsGeometryNode::CardsFacesGroup.ToString();
			CardsFacesLODGroup.AppendInt(GGroomDataflowCardsLod);

			if (Collection.HasAttribute(FGenerateCardsGeometryNode::VertexClumpPositionsAttribute, FName(CardsVerticesLODGroup)) &&
				Collection.HasAttribute(FGenerateCardsGeometryNode::FaceVertexIndicesAttribute, FName(CardsFacesLODGroup)) && 
				Collection.HasAttribute(FGenerateCardsGeometryNode::VertexCardIndicesAttribute, FName(CardsVerticesLODGroup)))
			{
				if (State.GetViewMode().GetName() == Dataflow::FDataflowConstruction3DViewMode::Name)
				{
					const TManagedArray<FVector3f>& VertexGlobalPositions = Collection.GetAttribute<FVector3f>(
						FGenerateCardsGeometryNode::VertexClumpPositionsAttribute, FName(CardsVerticesLODGroup));
						
					const TManagedArray<FIntVector3>& FaceGlobalVertices = Collection.GetAttribute<FIntVector3>(
						FGenerateCardsGeometryNode::FaceVertexIndicesAttribute, FName(CardsFacesLODGroup));

					const TManagedArray<int32>& VertexCardIndices = Collection.GetAttribute<int32>(
						FGenerateCardsGeometryNode::VertexCardIndicesAttribute, FName(CardsVerticesLODGroup));

					int32 NumCards = 0;
					for(const int32& CardIndex : VertexCardIndices.GetConstArray())
					{
						NumCards = FMath::Max(NumCards, CardIndex);
					}
					++NumCards;

					TArray<TArray<int32>> CardVertexIndices;
					TArray<TArray<int32>> CardFaceIndices;
					TArray<int32> VertexLocalIndices;

					CardVertexIndices.SetNum(NumCards);
					CardFaceIndices.SetNum(NumCards);
					VertexLocalIndices.SetNum(VertexCardIndices.Num());
					
					for (int32 VertexIndex = 0, NumVertices = VertexCardIndices.Num(); VertexIndex < NumVertices; ++VertexIndex)
					{
						VertexLocalIndices[VertexIndex] = CardVertexIndices[VertexCardIndices[VertexIndex]].Num();
						CardVertexIndices[VertexCardIndices[VertexIndex]].Add(VertexIndex);
					}

					for (int32 FaceIndex = 0, NumFaces = FaceGlobalVertices.Num(); FaceIndex < NumFaces; ++FaceIndex)
					{
						CardFaceIndices[VertexCardIndices[FaceGlobalVertices[FaceIndex][0]]].Add(FaceIndex);
					}

					for(int32 CardIndex = 0; CardIndex < NumCards; ++CardIndex)
					{	
						TArray<FVector3f> VertexLocalPositions;
						TArray<FVector3f> VertexLocalNormals;
						TArray<FIntVector> FaceLocalVertices;
						TArray<int32> VertexNumFaces;

						VertexLocalPositions.Init(FVector3f::Zero(), CardVertexIndices[CardIndex].Num());
						VertexLocalNormals.Init(FVector3f::Zero(), CardVertexIndices[CardIndex].Num());
						VertexNumFaces.Init(0, CardVertexIndices[CardIndex].Num());
						FaceLocalVertices.Init(FIntVector::ZeroValue, CardFaceIndices.Num());

						for (int32 FaceIndex = 0, NumFaces = CardFaceIndices[CardIndex].Num(); FaceIndex < NumFaces; ++FaceIndex)
						{
							const FIntVector3 GlobalVertices = FaceGlobalVertices[CardFaceIndices[CardIndex][FaceIndex]];
							FaceLocalVertices[FaceIndex] = FIntVector(VertexLocalIndices[GlobalVertices[0]], VertexLocalIndices[GlobalVertices[1]], VertexLocalIndices[GlobalVertices[2]]);

							const FVector3f FaceNormal = (VertexGlobalPositions[GlobalVertices[2]] - VertexGlobalPositions[GlobalVertices[0]]).Cross(
								VertexGlobalPositions[GlobalVertices[1]] - VertexGlobalPositions[GlobalVertices[0]]).GetSafeNormal();

							VertexLocalNormals[FaceLocalVertices[FaceIndex][0]] += FaceNormal;
							VertexLocalNormals[FaceLocalVertices[FaceIndex][1]] += FaceNormal;
							VertexLocalNormals[FaceLocalVertices[FaceIndex][2]] += FaceNormal;

							VertexNumFaces[FaceLocalVertices[FaceIndex][0]]++;
							VertexNumFaces[FaceLocalVertices[FaceIndex][1]]++;
							VertexNumFaces[FaceLocalVertices[FaceIndex][2]]++;
						}

						for (int32 VertexIndex = 0, NumVertices = CardVertexIndices[CardIndex].Num(); VertexIndex < NumVertices; ++VertexIndex)
						{
							if (VertexNumFaces[VertexIndex] > 0)
							{
								VertexLocalNormals[VertexIndex] /= VertexNumFaces[VertexIndex];
							}
							VertexLocalPositions[VertexIndex] = VertexGlobalPositions[CardVertexIndices[CardIndex][VertexIndex]];
						}

						const FLinearColor GroupColor = FLinearColor::IntToDistinctColor(CardIndex, 0.75f, 1.0f, 90.0f);

						TArray<FLinearColor> VertexLocalColors;
						VertexLocalColors.Init(GroupColor, VertexLocalPositions.Num());

						ComputeVertexColors(Collection, CardIndex, VertexLocalColors);

						const FString GeometryName = TEXT("Groom_Card_") + FString::FromInt(CardIndex);
						const int32 GeometryIndex = RenderCollection.StartGeometryGroup(GeometryName);
						RenderCollection.AddSurface(MoveTemp(VertexLocalPositions), MoveTemp(FaceLocalVertices), MoveTemp(VertexLocalNormals), MoveTemp(VertexLocalColors));
						RenderCollection.EndGeometryGroup(GeometryIndex);
					}
				}
				else
				{
					checkf(false, TEXT("Invalid View Mode for groom cards geometry rendering"));
				}
			}
		}
	}

	Dataflow::FRenderKey FCardsTextureRenderingCallbacks::RenderKey = { "TextureRender", FName("FCardsCollection") };

	void FCardsTextureRenderingCallbacks::Render(GeometryCollection::Facades::FRenderingFacade& RenderCollection, const Dataflow::FGraphRenderingState& State)
	{
		if (State.GetViewMode().GetName() == Dataflow::FDataflowConstruction3DViewMode::Name)
		{
			FCardsGeometryRenderingCallbacks::Render(RenderCollection, State);
		}
		else if (State.GetViewMode().GetName() == Dataflow::FDataflowConstructionUVViewMode::Name)
		{
			if (State.GetRenderOutputs().Num())
			{
				const FManagedArrayCollection Default;
				checkf(State.GetRenderOutputs().Num() == 1, TEXT("Expected FGraphRenderingState object to have one render output"));
				const FName PrimaryOutput = State.GetRenderOutputs()[0];
				const FManagedArrayCollection& Collection = State.GetValue<FManagedArrayCollection>(PrimaryOutput, Default);

				FString CardsVerticesLODGroup = FGenerateCardsGeometryNode::CardsVerticesGroup.ToString();
				CardsVerticesLODGroup.AppendInt(GGroomDataflowCardsLod);
							
				FString CardsFacesLODGroup = FGenerateCardsGeometryNode::CardsFacesGroup.ToString();
				CardsFacesLODGroup.AppendInt(GGroomDataflowCardsLod);

				if (Collection.HasAttribute(FGenerateCardsTexturesNode::VertexTextureUVsAttribute, FName(CardsVerticesLODGroup)) &&
					Collection.HasAttribute(FGenerateCardsGeometryNode::FaceVertexIndicesAttribute, FName(CardsFacesLODGroup)))
				{
					const TManagedArray<FVector2f>& VertexUVsArray = Collection.GetAttribute<FVector2f>(
							FGenerateCardsTexturesNode::VertexTextureUVsAttribute, FName(CardsVerticesLODGroup));
						
					const TManagedArray<FIntVector3>& FaceIndicesArray = Collection.GetAttribute<FIntVector3>(
						FGenerateCardsGeometryNode::FaceVertexIndicesAttribute, FName(CardsFacesLODGroup));
					
					TArray<FIntVector> FaceIndices = FaceIndicesArray.GetConstArray();
					
					TArray<FVector3f> VertexUVs;
					VertexUVs.Init(FVector3f(0.0, 0.0, 1.0), VertexUVsArray.Num());

					for (int32 VertexIndex = 0; VertexIndex < VertexUVsArray.Num(); ++VertexIndex)
					{
						VertexUVs[VertexIndex] = FVector3f(VertexUVsArray[VertexIndex][0], VertexUVsArray[VertexIndex][1], 0.0f);
					}

					TArray<FVector3f> VertexNormals;
					VertexNormals.Init(FVector3f(0.0, 0.0, 1.0), VertexUVs.Num());

					TArray<FLinearColor> VertexColors;
					VertexColors.Init(FLinearColor(0,0,0,0), VertexUVs.Num());

					const int32 GeometryIndex = RenderCollection.StartGeometryGroup(State.GetGuid().ToString());
					RenderCollection.AddSurface(MoveTemp(VertexUVs), MoveTemp(FaceIndices), MoveTemp(VertexNormals), MoveTemp(VertexColors));
					RenderCollection.EndGeometryGroup(GeometryIndex);
				}	
			}
		}
	}

	void FCardsTextureRenderingCallbacks::ComputeVertexColors(const FManagedArrayCollection& Collection, const int32 CardIndex, TArray<FLinearColor>& VertexColors) const
	{
		FString CardsObjectsLODGroup = FGenerateCardsTexturesNode::CardsObjectsGroup.ToString();
		CardsObjectsLODGroup.AppendInt(GGroomDataflowCardsLod);

		if (Collection.HasAttribute(FGenerateCardsTexturesNode::ObjectTextureIndicesAttribute, FName(CardsObjectsLODGroup)))
		{
			const TManagedArray<int32>& CardsTextureArray = Collection.GetAttribute<int32>(
				FGenerateCardsTexturesNode::ObjectTextureIndicesAttribute, FName(CardsObjectsLODGroup));

			const int32 TextureIndex = (CardIndex < CardsTextureArray.Num()) ? CardsTextureArray[CardIndex] : INDEX_NONE;
			FLinearColor CardColor = (TextureIndex != INDEX_NONE) ? FLinearColor::IntToDistinctColor(TextureIndex, 0.75f, 1.0f, 90.0f) : FLinearColor::Black;
			CardColor *= (TextureIndex == CardIndex) ? 1.0 : GGroomDataflowCardsAlpha;

			const int32 NumVertices = VertexColors.Num();
			VertexColors.Init(CardColor, NumVertices);
		}
	}

	Dataflow::FRenderKey FCardsClumpsRenderingCallbacks::RenderKey = { "ClumpsRender", FName("FCardsCollection") };
	
	int32 FCardsClumpsRenderingCallbacks::GetGroupAttribute(const UE::Groom::FGroomStrandsFacade& StrandsFacade, FString& GroupAttribute, FString& GroupName) const
	{
		GroupAttribute = FGenerateCardsClumpsNode::CurveClumpIndicesAttribute.ToString();
		GroupAttribute.AppendInt(GGroomDataflowCardsLod);
		GroupName = FString("Clump");

		const FManagedArrayCollection& GroomCollection = StrandsFacade.GetManagedArrayCollection();
		if (GroomCollection.HasAttribute(FName(GroupAttribute), UE::Groom::FGroomStrandsFacade::CurvesGroup))
		{
			const TManagedArray<int32>& ClumpIndices = GroomCollection.GetAttribute<int32>(FName(GroupAttribute), UE::Groom::FGroomStrandsFacade::CurvesGroup);

			int32 NumClumps = INDEX_NONE;
			for(int32 ElementIndex = 0, NumElements = GroomCollection.NumElements(UE::Groom::FGroomStrandsFacade::CurvesGroup); ElementIndex < NumElements; ++ElementIndex)
			{	
				NumClumps = FMath::Max(NumClumps, ClumpIndices[ElementIndex]);
			}
			++NumClumps;
			return NumClumps;
		}
		return 0;
	}
	
	void FCardsClumpsRenderingCallbacks::ComputeVertexColors(const UE::Groom::FGroomStrandsFacade& StrandsFacade, TArray<FLinearColor>& VertexColors) const
	{
		const FManagedArrayCollection& GroomCollection = StrandsFacade.GetManagedArrayCollection();

		FString ClumpIndicesLOD = FGenerateCardsClumpsNode::CurveClumpIndicesAttribute.ToString();
		ClumpIndicesLOD.AppendInt(GGroomDataflowCardsLod);

		FString NumClumpsLOD = FGenerateCardsClumpsNode::ObjectNumClumpsAttribute.ToString();
		NumClumpsLOD.AppendInt(GGroomDataflowCardsLod);
		
		if(GroomCollection.HasAttribute(FName(ClumpIndicesLOD), UE::Groom::FGroomStrandsFacade::CurvesGroup) &&
		   GroomCollection.HasAttribute(FName(NumClumpsLOD), UE::Groom::FGroomStrandsFacade::ObjectsGroup))
		{
			const TManagedArray<int32>& ClumpIndices = GroomCollection.GetAttribute<int32>(FName(ClumpIndicesLOD), UE::Groom::FGroomStrandsFacade::CurvesGroup);
			const TManagedArray<int32>& NumClumps = GroomCollection.GetAttribute<int32>(FName(NumClumpsLOD), UE::Groom::FGroomStrandsFacade::ObjectsGroup);

			TArray<FLinearColor> ClumpsColors;
			ClumpsColors.Init(FLinearColor::Black, NumClumps[0]);

			for(int32 ClumpIndex = 0; ClumpIndex < NumClumps[0]; ++ClumpIndex)
			{
				ClumpsColors[ClumpIndex] = FLinearColor::IntToDistinctColor(ClumpIndex, 0.75f, 1.0f, 90.0f);
			}

			TArray<FLinearColor> StrandsColors;
			StrandsColors.Init(FLinearColor::Black, StrandsFacade.GetNumCurves());
			for(int32 CurveIndex = 0, CurveEnd = StrandsFacade.GetNumCurves(); CurveIndex < CurveEnd; ++CurveIndex)
			{
				if(ClumpIndices[CurveIndex] < ClumpsColors.Num())
				{
					StrandsColors[CurveIndex] = ClumpsColors[ClumpIndices[CurveIndex]];
				}
			}
			
			for(int32 PointIndex = 0, PointEnd = StrandsFacade.GetNumPoints(); PointIndex < PointEnd; ++PointIndex)
			{
				const int32 CurveIndex = StrandsFacade.GetPointCurveIndices()[PointIndex];

				VertexColors[2*PointIndex] = StrandsColors[CurveIndex];
				VertexColors[2*PointIndex+1] = StrandsColors[CurveIndex];
			}
		}
	}
}
