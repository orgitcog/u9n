// Copyright Epic Games, Inc. All Rights Reserved.

#include "GroomDataflowRendering.h"
#include "GroomCollectionFacades.h"
#include "Engine/SkeletalMesh.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/Facades/CollectionRenderingFacade.h"

namespace UE::Groom
{
	int32 GGroomDataflowDebugDraw = 0;
	static FAutoConsoleVariableRef CVarGroomDataflowDebugDraw(
		 TEXT("p.Groom.Dataflow.DebugDraw"),
		 GGroomDataflowDebugDraw,
		 TEXT("Type of information we want to draw for debug (0-DefaultColor, 1-SkinWeights, 2-GuidesLods)")
	 );

	FString GGroomDataflowBoneName = TEXT("");
	static FAutoConsoleVariableRef CVarGroomDataflowBoneName(
		 TEXT("p.Groom.Dataflow.BoneName"),
		 GGroomDataflowBoneName,
		 TEXT("Bone name we want to visualize the skin weights")
	 );

	int32 GGroomDataflowGuidesLod = 0;
	static FAutoConsoleVariableRef CVarGroomDataflowGuidesLod(
		 TEXT("p.Groom.Dataflow.GuidesLod"),
		 GGroomDataflowGuidesLod,
		 TEXT("Guides Lod we want to display")
	 );

	int32 GGroomDataflowStrandsLod = 0;
	static FAutoConsoleVariableRef CVarGroomDataflowStrandsLod(
		 TEXT("p.Groom.Dataflow.StrandsLod"),
		 GGroomDataflowStrandsLod,
		 TEXT("Strands Lod we want to display")
	 );

	float GGroomDataflowRenderingThickness = 0.5f;
	static FAutoConsoleVariableRef CVarRestGuidesRenderingThickness(
		 TEXT("p.Groom.Dataflow.RenderingThickness"),
		 GGroomDataflowRenderingThickness,
		 TEXT("Thickness used to render the groom in dataflow editor.")
	 );

	FORCEINLINE void BuildVertexPositions(const int32 PointIndex, const FVector3f& PointPosition, const FVector3f& SideVector, const float RenderThickness, TArray<FVector3f>& VertexPositions)
	{
		const int32 VertexIndex = 2 * PointIndex;
		
		const FVector3f PointPositionA = PointPosition + SideVector * RenderThickness;
		const FVector3f PointPositionB = PointPosition - SideVector * RenderThickness;
	
		VertexPositions[VertexIndex] = PointPositionA;
		VertexPositions[VertexIndex+1] = PointPositionB;
	}
	
	FORCEINLINE void BuildFaceVertices(const int32 PointIndex, const int32 CurveIndex, TArray<FIntVector>& FaceVertices)
	{
		const int32 VertexIndex = 2 * PointIndex;
		const int32 FaceIndex = 2 * (PointIndex - CurveIndex);
				
		FaceVertices[FaceIndex] = FIntVector(VertexIndex, VertexIndex + 1, VertexIndex + 3);
		FaceVertices[FaceIndex+1] = FIntVector(VertexIndex, VertexIndex + 3, VertexIndex + 2);
	}

	FORCEINLINE void BuildVertexNormals(const int32 PointIndex, const FVector3f& EdgeNormal, TArray<FVector3f>& VertexNormals)
	{
		const int32 VertexIndex = 2 * PointIndex;

		VertexNormals[VertexIndex] = EdgeNormal;
		VertexNormals[VertexIndex+1] = EdgeNormal;
	}

	template<typename FacadeType>
	FORCEINLINE void BuildRenderingDatas(const FacadeType& GroomFacade, TArray<FVector3f>& VertexPositions, TArray<FVector3f>& VertexNormals, TArray<FIntVector>& FaceVertices)
	{
		const int32 NumPoints = GroomFacade.GetNumPoints();
		const int32 NumEdges = GroomFacade.GetNumEdges();
		const int32 NumCurves = GroomFacade.GetNumCurves();

		VertexPositions.SetNum(NumPoints * 2);
		VertexNormals.SetNum(NumPoints * 2);
		FaceVertices.SetNum(NumEdges * 2);

		int32 PointOffset = 0;
		for(int32 CurveIndex = 0; CurveIndex < NumCurves; ++CurveIndex)
		{
			for (int32 PointIndex = PointOffset, PointEnd = GroomFacade.GetCurvePointOffsets()[CurveIndex];
					   PointIndex < PointEnd; ++PointIndex)
			{
				// Build 2 vertices for each points
				const int32 EdgeIndex = (PointIndex == (PointEnd-1)) ? PointIndex-CurveIndex-1 : PointIndex-CurveIndex;
								
				const FVector3f SideVector = GroomFacade.GetEdgeRestOrientations()[EdgeIndex].GetAxisX();
				BuildVertexPositions(PointIndex, GroomFacade.GetPointRestPositions()[PointIndex], SideVector, GGroomDataflowRenderingThickness, VertexPositions);

				const FVector3f NormalVector = GroomFacade.GetEdgeRestOrientations()[EdgeIndex].GetAxisZ();
				BuildVertexNormals(PointIndex, NormalVector, VertexNormals);
					
				// Build 2 faces for each edges perpendicular to the up-vector curve
				if(PointIndex < (PointEnd-1))
				{
					BuildFaceVertices(PointIndex, CurveIndex, FaceVertices);
				}
			}
			PointOffset = GroomFacade.GetCurvePointOffsets()[CurveIndex];
		}
	}

	template<typename FacadeType>
	static void BuildRenderingGroups(const FacadeType& GroomFacade, TArray<TArray<FVector3f>>& GroupVertexPositions,
		TArray<TArray<FVector3f>>& GroupVertexNormals, TArray<TArray<FIntVector>>& GroupFaceVertices, const FString& GroupAttribute)
	{
		const int32 NumCurves = GroomFacade.GetNumCurves();

		const FManagedArrayCollection& GroomCollection = GroomFacade.GetManagedArrayCollection();
		if(GroomCollection.HasAttribute(FName(GroupAttribute), FacadeType::CurvesGroup))
		{
			const TManagedArray<int32>& CurveGroups = GroomCollection.GetAttribute<int32>(FName(GroupAttribute), FacadeType::CurvesGroup);

			int32 NumGroups = INDEX_NONE;
			for(int32 CurveIndex = 0; CurveIndex < NumCurves; ++CurveIndex)
			{
				NumGroups = FMath::Max(NumGroups, CurveGroups[CurveIndex]);
			}
			++NumGroups;

			if(NumGroups > 0)
			{ 
				GroupVertexPositions.SetNum(NumGroups);
				GroupVertexNormals.SetNum(NumGroups);
				GroupFaceVertices.SetNum(NumGroups);

				TArray<int32> GroupsPoints;
				GroupsPoints.Init(0, NumGroups);
			
				TArray<int32> GroupsCurves;
				GroupsCurves.Init(0, NumGroups);

				int32 PointOffset = 0;
				for(int32 CurveIndex = 0; CurveIndex < NumCurves; ++CurveIndex)
				{
					const int32 GroupIndex = CurveGroups[CurveIndex];
					if(GroupsPoints.IsValidIndex(GroupIndex) && GroupsCurves.IsValidIndex(GroupIndex))
					{ 
						const int32 CurvePoints = GroomFacade.GetCurvePointOffsets()[CurveIndex] - PointOffset;
						GroupsPoints[GroupIndex] += CurvePoints;
						++GroupsCurves[GroupIndex];
					}
					PointOffset = GroomFacade.GetCurvePointOffsets()[CurveIndex];
				}

				for(int32 GroupIndex = 0; GroupIndex < NumGroups; ++GroupIndex)
				{
					GroupVertexPositions[GroupIndex].SetNum(GroupsPoints[GroupIndex] * 2);
					GroupVertexNormals[GroupIndex].SetNum(GroupsPoints[GroupIndex] * 2);
					GroupFaceVertices[GroupIndex].SetNum((GroupsPoints[GroupIndex]-GroupsCurves[GroupIndex]) * 2);
				}

				GroupsPoints.Init(0, NumGroups);
				GroupsCurves.Init(0, NumGroups);
			
				PointOffset = 0;
				for(int32 CurveIndex = 0; CurveIndex < NumCurves; ++CurveIndex)
				{
					const int32 GroupIndex = CurveGroups[CurveIndex];
					if (GroupsPoints.IsValidIndex(GroupIndex) && GroupsCurves.IsValidIndex(GroupIndex))
					{
						TArray<FVector3f>& VertexPositions = GroupVertexPositions[GroupIndex];
						TArray<FVector3f>& VertexNormals = GroupVertexNormals[GroupIndex];
						TArray<FIntVector>& FaceVertices = GroupFaceVertices[GroupIndex];
				
						for (int32 PointIndex = PointOffset, PointEnd = GroomFacade.GetCurvePointOffsets()[CurveIndex];
								   PointIndex < PointEnd; ++PointIndex)
						{
							// Build 2 vertices for each points
							const int32 EdgeIndex = (PointIndex == (PointEnd-1)) ? PointIndex-CurveIndex-1 : PointIndex-CurveIndex;
										
							const FVector3f SideVector = GroomFacade.GetEdgeRestOrientations()[EdgeIndex].GetAxisX();
							BuildVertexPositions(GroupsPoints[GroupIndex], GroomFacade.GetPointRestPositions()[PointIndex], SideVector, GGroomDataflowRenderingThickness, VertexPositions);

							const FVector3f NormalVector = GroomFacade.GetEdgeRestOrientations()[EdgeIndex].GetAxisZ();
							BuildVertexNormals(GroupsPoints[GroupIndex], NormalVector, VertexNormals);
							
							// Build 2 faces for each edges perpendicular to the up-vector curve
							if(PointIndex < (PointEnd-1))
							{
								BuildFaceVertices(GroupsPoints[GroupIndex], GroupsCurves[GroupIndex], FaceVertices);
							}

							++GroupsPoints[GroupIndex];
					
						}
						++GroupsCurves[GroupIndex];
					}
					PointOffset = GroomFacade.GetCurvePointOffsets()[CurveIndex];
				}
			}
		}
	}

	template<typename FacadeType>
	FORCEINLINE void BuildGeometryDatas(const FacadeType& GroomFacade, TArray<float>& VertexPositions, TArray<int32>& FaceVertices, TArray<int32>& ObjectFaceOffsets, TArray<int32>& ObjectVertexOffsets)
	{
		TArray<FVector3f> RenderingPositions, RenderingNormals;
		TArray<FIntVector> RenderingVertices;

		BuildRenderingDatas(GroomFacade, RenderingPositions, RenderingNormals, RenderingVertices);

		VertexPositions.SetNum(RenderingPositions.Num() * 3);
		FaceVertices.SetNum(RenderingVertices.Num() * 3);

		int32 VertexIndex = 0;
		for(const FVector3f& VertexPosition : RenderingPositions)
		{
			VertexPositions[VertexIndex++] = VertexPosition.X;
			VertexPositions[VertexIndex++] = VertexPosition.Y;
			VertexPositions[VertexIndex++] = VertexPosition.Z;
		}

		int32 FaceIndex = 0;
		for(const FIntVector& FaceVertex : RenderingVertices)
		{
			FaceVertices[FaceIndex++] = FaceVertex.X;
			FaceVertices[FaceIndex++] = FaceVertex.Y;
			FaceVertices[FaceIndex++] = FaceVertex.Z;
		}

		const int32 NumObjects = GroomFacade.GetNumObjects();
		ObjectFaceOffsets.SetNum(NumObjects);
		ObjectVertexOffsets.SetNum(NumObjects);
		
		for(int32 ObjectIndex = 0; ObjectIndex < NumObjects; ++ObjectIndex)
		{
			const int32 CurveOffset = GroomFacade.GetObjectCurveOffsets()[ObjectIndex]-1;
			ObjectVertexOffsets[ObjectIndex] = 2 * GroomFacade.GetCurvePointOffsets()[CurveOffset];
			ObjectFaceOffsets[ObjectIndex] = ObjectVertexOffsets[ObjectIndex] - 2 * CurveOffset;
		}
	}

	template<typename FacadeType>
	FORCEINLINE void SetupGeometryCollection(const FacadeType& GroomFacade, FGeometryCollection& GeometryCollection)
	{
		TArray<float> RawVertexPositions;
		TArray<int32> RawFaceIndices;
			
		TArray<int32> ObjectFaceOffsets;
		TArray<int32> ObjectVertexOffsets;

		BuildGeometryDatas(GroomFacade, RawVertexPositions, RawFaceIndices, ObjectFaceOffsets, ObjectVertexOffsets);
		FGeometryCollection::Init(&GeometryCollection, RawVertexPositions, RawFaceIndices, false);

		// add a material section
		TManagedArray<FGeometryCollectionSection>& Sections = GeometryCollection.Sections;
		GeometryCollection.Resize(GroomFacade.GetNumObjects(), FGeometryCollection::MaterialGroup);
			
		int32 FaceOffset = 0;
		int32 VertexOffset = 0;
		for(int32 SectionIndex = 0; SectionIndex < GroomFacade.GetNumObjects(); ++SectionIndex)
		{
			Sections[SectionIndex].MaterialID = SectionIndex;
			Sections[SectionIndex].FirstIndex = FaceOffset;
			Sections[SectionIndex].NumTriangles = ObjectFaceOffsets[SectionIndex] - FaceOffset;
			Sections[SectionIndex].MinVertexIndex = VertexOffset;
			Sections[SectionIndex].MaxVertexIndex = ObjectVertexOffsets[SectionIndex] - 1;

			FaceOffset = ObjectFaceOffsets[SectionIndex];
			VertexOffset = ObjectVertexOffsets[SectionIndex];
		}
	}
	
	template<typename FacadeType>
	FORCEINLINE void RenderGroomCollection(GeometryCollection::Facades::FRenderingFacade& RenderCollection, const Dataflow::FGraphRenderingState& State,
		const TFunction<void(const FacadeType&, TArray<FLinearColor>&)>& ColorLambda)
	{
		if (State.GetRenderOutputs().Num())
		{
			const FManagedArrayCollection Default;
			checkf(State.GetRenderOutputs().Num() == 1, TEXT("Expected FGraphRenderingState object to have one render output"));
			const FName PrimaryOutput = State.GetRenderOutputs()[0];
			const FManagedArrayCollection& Collection = State.GetValue<FManagedArrayCollection>(PrimaryOutput, Default);

			const FacadeType GroomFacade(Collection);
			if (GroomFacade.IsValid())
			{
				TArray<FVector3f> VertexPositions;
				TArray<FVector3f> VertexNormals;
				TArray<FIntVector> FaceIndices;

				if (State.GetViewMode().GetName() == Dataflow::FDataflowConstruction3DViewMode::Name)
				{
					BuildRenderingDatas(GroomFacade, VertexPositions, VertexNormals, FaceIndices);
				}
				else
				{
					checkf(false, TEXT("Invalid View Mode for FClothCollection rendering"));
				}

				TArray<FLinearColor> Colors;
				Colors.Init(FLinearColor(0.2,0.6,1.0), VertexPositions.Num());
				ColorLambda(GroomFacade, Colors);

				const FString GeometryName = GroomFacade.GetObjectGroupNames().Last();
				const int32 GeometryIndex = RenderCollection.StartGeometryGroup(GeometryName);
				RenderCollection.AddSurface(MoveTemp(VertexPositions), MoveTemp(FaceIndices), MoveTemp(VertexNormals), MoveTemp(Colors));
				RenderCollection.EndGeometryGroup(GeometryIndex);
			}
		}
	}

	template<typename FacadeType>
	static void RenderGroupCollection(GeometryCollection::Facades::FRenderingFacade& RenderCollection, const Dataflow::FGraphRenderingState& State,
		const TFunction<int32(const FacadeType&, FString&, FString&)>& GroupLambda)
	{
		if (State.GetRenderOutputs().Num())
		{
			const FManagedArrayCollection Default;
			checkf(State.GetRenderOutputs().Num() == 1, TEXT("Expected FGraphRenderingState object to have one render output"));
			const FName PrimaryOutput = State.GetRenderOutputs()[0];
			const FManagedArrayCollection& Collection = State.GetValue<FManagedArrayCollection>(PrimaryOutput, Default);

			const FacadeType GroomFacade(Collection);
			if (GroomFacade.IsValid())
			{
				FString GroupAttribute, GroupName;
				const int32 NumGroups = GroupLambda(GroomFacade, GroupAttribute, GroupName);
				
				TArray<TArray<FVector3f>> VertexPositions;
				TArray<TArray<FVector3f>> VertexNormals;
				TArray<TArray<FIntVector>> FaceIndices;

				if (State.GetViewMode().GetName() == Dataflow::FDataflowConstruction3DViewMode::Name)
				{
					BuildRenderingGroups(GroomFacade, VertexPositions, VertexNormals, FaceIndices, GroupAttribute);
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("Invalid View Mode for Groom dataflow rendering"));
				}
				if(VertexPositions.Num() == NumGroups && VertexNormals.Num() == NumGroups && FaceIndices.Num() == NumGroups)
				{ 
					for (int32 GroupIndex = 0; GroupIndex < NumGroups; ++GroupIndex)
					{
						const FLinearColor GroupColor = FLinearColor::IntToDistinctColor(GroupIndex, 0.75f, 1.0f, 90.0f);

						TArray<FLinearColor> VertexColors;
						VertexColors.Init(GroupColor, VertexPositions[GroupIndex].Num());

						const FString GeometryName = GroomFacade.GetObjectGroupNames().Last() + TEXT("_") + GroupName + TEXT("_") + FString::FromInt(GroupIndex);
						const int32 GeometryIndex = RenderCollection.StartGeometryGroup(GeometryName);
						RenderCollection.AddSurface(MoveTemp(VertexPositions[GroupIndex]),
							MoveTemp(FaceIndices[GroupIndex]), MoveTemp(VertexNormals[GroupIndex]), MoveTemp(VertexColors));
						RenderCollection.EndGeometryGroup(GeometryIndex);
					}
				}
			}
		}
	}
	
	Dataflow::FRenderKey FGroomGuidesRenderingCallbacks::RenderKey = { "GuidesRender", FName("FGroomCollection") };

	void RenderBoneWeights(const FGroomGuidesFacade& GuidesFacade, TArray<FLinearColor>& VertexColors)
	{
		if(GuidesFacade.GetManagedArrayCollection().HasAttribute(
						UE::Groom::FGroomGuidesFacade::ObjectSkeletalMeshesAttribute, UE::Groom::FGroomGuidesFacade::ObjectsGroup))
		{
			const TManagedArray<TObjectPtr<UObject>>& ObjectSkeletalMeshes = GuidesFacade.GetManagedArrayCollection().GetAttribute<TObjectPtr<UObject>>(
				UE::Groom::FGroomGuidesFacade::ObjectSkeletalMeshesAttribute, UE::Groom::FGroomGuidesFacade::ObjectsGroup);
				
			for(int32 PointIndex = 0, PointEnd = GuidesFacade.GetNumPoints(); PointIndex < PointEnd; ++PointIndex)
			{
				const FIntVector4& BoneIndices = GuidesFacade.GetPointBoneIndices(PointIndex);
				const FVector4f& BoneWeights = GuidesFacade.GetPointBoneWeights(PointIndex);

				const int32 CurveIndex = GuidesFacade.GetPointCurveIndices()[PointIndex];
				const int32 ObjectIndex = GuidesFacade.GetCurveObjectIndices()[CurveIndex];

				if(USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(ObjectSkeletalMeshes[ObjectIndex]))
				{
					for(int32 BoneIndex = 0, BoneEnd = BoneIndices.Num(); BoneIndex < BoneEnd; ++BoneIndex)
					{
						const int32 SkeletonBone = BoneIndices[BoneIndex];
						if(SkeletonBone != INDEX_NONE)
						{
							const FName BoneName = SkeletalMesh->GetRefSkeleton().GetRawRefBoneInfo()[SkeletonBone].Name;
							if(BoneName == GGroomDataflowBoneName)
							{
								VertexColors[2*PointIndex] = FLinearColor::LerpUsingHSV(
									FLinearColor::Black, FLinearColor::Yellow, BoneWeights[BoneIndex]);
								VertexColors[2*PointIndex+1] = VertexColors[2*PointIndex];
							}
						}
					}
				}
			}
		}
	}
	
	void RenderGuidesLods(const FGroomGuidesFacade& GuidesFacade, TArray<FLinearColor>& VertexColors)
	{
		const TArray<int32>& ParentIndices = GuidesFacade.GetCurveParentIndices();
		const TArray<int32>& LodIndices = GuidesFacade.GetCurveLodIndices();

		TArray<FLinearColor> LodColors;
		LodColors.Init(FLinearColor::Black, GuidesFacade.GetNumCurves());
		for(int32 CurveIndex = 0, CurveEnd = GuidesFacade.GetNumCurves(); CurveIndex < CurveEnd; ++CurveIndex)
		{
			if(LodIndices[CurveIndex] >= GGroomDataflowGuidesLod)
			{
				LodColors[CurveIndex] = FLinearColor::IntToDistinctColor(CurveIndex, 0.75f, 1.0f, 90.0f);
			}
		}
		for(int32 CurveIndex = 0, CurveEnd = GuidesFacade.GetNumCurves(); CurveIndex < CurveEnd; ++CurveIndex)
		{
			if((LodIndices[CurveIndex] != INDEX_NONE) && (LodIndices[CurveIndex] < GGroomDataflowGuidesLod))
			{
				int32 ParentIndex = CurveIndex;
				int32 ParentLod = LodIndices[ParentIndex];
				while((ParentLod < GGroomDataflowGuidesLod) && ParentIndex != INDEX_NONE)
				{
					ParentIndex = ParentIndices[ParentIndex];
					ParentLod = LodIndices[ParentIndex];
				}
				LodColors[CurveIndex] = LodColors[ParentIndex];
			}
		}

		for(int32 PointIndex = 0, PointEnd = GuidesFacade.GetNumPoints(); PointIndex < PointEnd; ++PointIndex)
		{
			const int32 CurveIndex = GuidesFacade.GetPointCurveIndices()[PointIndex];

			VertexColors[2*PointIndex] = LodColors[CurveIndex];
			VertexColors[2*PointIndex+1] = LodColors[CurveIndex];
		}
	}
	
	void FGroomGuidesRenderingCallbacks::Render(GeometryCollection::Facades::FRenderingFacade& RenderCollection, const Dataflow::FGraphRenderingState& State)
	{
		RenderGroupCollection<FGroomGuidesFacade>(RenderCollection, State,
			[this](const FGroomGuidesFacade& GuidesFacade, FString& GroupAttribute, FString& GroupName) -> int32
			{
				return GetGroupAttribute(GuidesFacade, GroupAttribute, GroupName);
			});
	}
	
	int32 FGroomGuidesRenderingCallbacks::GetGroupAttribute(const FGroomGuidesFacade& StrandsFacade, FString& GroupAttribute, FString& GroupName) const
	{
		GroupAttribute = FGroomGuidesFacade::CurveObjectIndicesAttribute.ToString();
		GroupName = FString("Group");
		return StrandsFacade.GetNumObjects();
	}

	void FGroomGuidesRenderingCallbacks::ComputeVertexColors(const FGroomGuidesFacade& GuidesFacade, TArray<FLinearColor>& VertexColors) const
	{
		if(GGroomDataflowDebugDraw == 1)
		{
			RenderBoneWeights(GuidesFacade, VertexColors);
		}
		if(GGroomDataflowDebugDraw == 2)
		{
			RenderGuidesLods(GuidesFacade, VertexColors);
		}
	}

	Dataflow::FRenderKey FGroomStrandsRenderingCallbacks::RenderKey = { "StrandsRender", FName("FGroomCollection") };
	
	void FGroomStrandsRenderingCallbacks::Render(GeometryCollection::Facades::FRenderingFacade& RenderCollection, const Dataflow::FGraphRenderingState& State)
	{
		RenderGroupCollection<FGroomStrandsFacade>(RenderCollection, State,
			[this](const FGroomStrandsFacade& StrandsFacade, FString& GroupAttribute, FString& GroupName) -> int32
			{
				return GetGroupAttribute(StrandsFacade, GroupAttribute, GroupName);
			});
	}

	int32 FGroomStrandsRenderingCallbacks::GetGroupAttribute(const FGroomStrandsFacade& StrandsFacade, FString& GroupAttribute, FString& GroupName) const
	{
		GroupAttribute = FGroomStrandsFacade::CurveObjectIndicesAttribute.ToString();
		GroupName = FString("Group");
		return StrandsFacade.GetNumObjects();
	}

	void FGroomStrandsRenderingCallbacks::ComputeVertexColors(const FGroomStrandsFacade& StrandsFacade, TArray<FLinearColor>& VertexColors) const
	{}

	void RegisterRenderingCallbacks()
	{
		Dataflow::FRenderingFactory::GetInstance()->RegisterCallbacks(MakeUnique<FGroomGuidesRenderingCallbacks>());
		Dataflow::FRenderingFactory::GetInstance()->RegisterCallbacks(MakeUnique<FGroomStrandsRenderingCallbacks>());
	}

	void DeregisterRenderingCallbacks()
	{
		Dataflow::FRenderingFactory::GetInstance()->DeregisterCallbacks(FGroomGuidesRenderingCallbacks::RenderKey);
		Dataflow::FRenderingFactory::GetInstance()->DeregisterCallbacks(FGroomStrandsRenderingCallbacks::RenderKey);
	}
}
