// Copyright Epic Games, Inc. All Rights Reserved.

#include "BuildGroomSkinningNodes.h"
#include "GroomCollectionFacades.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "SkeletalMeshAttributes.h"
#include "SkeletalMeshLODRenderDataToDynamicMesh.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Dataflow/DataflowObjectInterface.h"
#include "DynamicMesh/MeshTransforms.h"
#include "Operations/TransferBoneWeights.h"
#include "Rendering/SkeletalMeshRenderData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BuildGroomSkinningNodes)

namespace UE::Groom::Private
{
	FORCEINLINE bool SkeletalMeshToDynamicMesh(const USkeletalMesh* SkeletalMesh, const int32 LodIndex, UE::Geometry::FDynamicMesh3& ToDynamicMesh)
	{
		if (SkeletalMesh->HasMeshDescription(LodIndex))
		{
			const FMeshDescription* SourceMesh = SkeletalMesh->GetMeshDescription(LodIndex);
			if (!SourceMesh)
			{
				return false;
			}

			FMeshDescriptionToDynamicMesh Converter;
			Converter.Convert(SourceMesh, ToDynamicMesh);
		}
		else
		{ 
			const FSkeletalMeshRenderData* RenderData = SkeletalMesh->GetResourceForRendering();
			if (!RenderData)
			{
				return false;
			}

			if (!RenderData->LODRenderData.IsValidIndex(LodIndex))
			{
				return false;
			}

			const FSkeletalMeshLODRenderData* SkeletalMeshLODRenderData = &(RenderData->LODRenderData[LodIndex]);

			UE::Geometry::FSkeletalMeshLODRenderDataToDynamicMesh::ConversionOptions ConversionOptions;
			ConversionOptions.bWantUVs = false;
			ConversionOptions.bWantVertexColors = false;
			ConversionOptions.bWantMaterialIDs = false;
			ConversionOptions.bWantSkinWeights = true;

			UE::Geometry::FSkeletalMeshLODRenderDataToDynamicMesh::Convert(SkeletalMeshLODRenderData, SkeletalMesh->GetRefSkeleton(), ConversionOptions, ToDynamicMesh);
		}

		return true;
	}

	template<typename FacadeType>
	static void BuildSkinningData(FManagedArrayCollection& GroomCollection, const int32 GroupIndex, const TObjectPtr<USkeletalMesh> SkeletalMesh, const int32 LODIndex,
		const FTransform& RelativeTransform)
	{
		FacadeType GroomFacade(GroomCollection);

		if(GroomFacade.IsValid())
		{
			TManagedArray<TObjectPtr<UObject>>& ObjectSkeletalMeshes =
				GroomCollection.AddAttribute<TObjectPtr<UObject>>(UE::Groom::FGroomGuidesFacade::ObjectSkeletalMeshesAttribute, FacadeType::ObjectsGroup);

			TManagedArray<int32>& ObjectMeshLODs =
				GroomCollection.AddAttribute<int32>(UE::Groom::FGroomGuidesFacade::ObjectMeshLODsAttribute, FacadeType::ObjectsGroup);

			TManagedArray<FIntVector4>& VertexBoneIndices =
				GroomCollection.AddAttribute<FIntVector4>(UE::Groom::FGroomGuidesFacade::PointBoneIndicesAttribute, FacadeType::VerticesGroup);

			TManagedArray<FVector4f>& VertexBoneWeights =
				GroomCollection.AddAttribute<FVector4f>(UE::Groom::FGroomGuidesFacade::PointBoneWeightsAttribute, FacadeType::VerticesGroup);
			
			if(GroupIndex == INDEX_NONE)
			{
				for(int32 LocalIndex = 0; LocalIndex < ObjectSkeletalMeshes.Num(); ++LocalIndex)
				{
					ObjectSkeletalMeshes[LocalIndex] = SkeletalMesh;
					ObjectMeshLODs[LocalIndex] = LODIndex;
				}
			}
			else if(ObjectSkeletalMeshes.IsValidIndex(GroupIndex) && ObjectMeshLODs.IsValidIndex(GroupIndex))
			{
				ObjectSkeletalMeshes[GroupIndex] = SkeletalMesh;
				ObjectMeshLODs[GroupIndex] = LODIndex;
			}
			if(SkeletalMesh && SkeletalMesh->IsValidLODIndex(LODIndex))
			{
				TMap<FName, FBoneIndexType> TargetBoneToIndex;
				TargetBoneToIndex.Reserve(SkeletalMesh->GetRefSkeleton().GetRawBoneNum());
				for (int32 BoneIdx = 0; BoneIdx < SkeletalMesh->GetRefSkeleton().GetRawBoneNum(); ++BoneIdx)
				{
					TargetBoneToIndex.Add(SkeletalMesh->GetRefSkeleton().GetRawRefBoneInfo()[BoneIdx].Name, BoneIdx);
				}
				UE::Geometry::FDynamicMesh3 DynamicMesh;
				if (UE::Groom::Private::SkeletalMeshToDynamicMesh(SkeletalMesh, LODIndex, DynamicMesh))
				{
					MeshTransforms::ApplyTransform(DynamicMesh, RelativeTransform, true);

					UE::Geometry::FTransferBoneWeights TransferBoneWeights(&DynamicMesh, FSkeletalMeshAttributes::DefaultSkinWeightProfileName);
					TransferBoneWeights.bUseParallel = true;
					TransferBoneWeights.MaxNumInfluences = 4;
					TransferBoneWeights.TransferMethod = UE::Geometry::FTransferBoneWeights::ETransferBoneWeightsMethod::ClosestPointOnSurface;

					if (TransferBoneWeights.Validate() == UE::Geometry::EOperationValidationResult::Ok)
					{
						const int32 LocalIndex = GroupIndex;
						ParallelFor(GroomFacade.GetNumPoints(), [&TransferBoneWeights, &TargetBoneToIndex, &GroomFacade, LocalIndex, &VertexBoneIndices, &VertexBoneWeights](
							int32 PointIndex)
						{
							const int32 CurveIndex = GroomFacade.GetPointCurveIndices()[PointIndex];
							const int32 ObjectIndex = GroomFacade.GetCurveObjectIndices()[CurveIndex];

							if(LocalIndex == INDEX_NONE || ObjectIndex == LocalIndex)
							{
								TArray<int32> BoneIndices;
								TArray<float> BoneWeights;
								TransferBoneWeights.TransferWeightsToPoint(BoneIndices, BoneWeights,
									GroomFacade.GetPointRestPositions()[PointIndex],&TargetBoneToIndex);

								FIntVector4 PointBoneIndices(INDEX_NONE);
								FVector4f PointBoneWeights(0.0f);

								for(int32 BoneIdx = 0; BoneIdx < BoneIndices.Num(); ++BoneIdx)
								{
									PointBoneIndices[BoneIdx] = BoneIndices[BoneIdx];
									PointBoneWeights[BoneIdx] = BoneWeights[BoneIdx];
								}
								VertexBoneIndices[2*PointIndex] = PointBoneIndices;
								VertexBoneIndices[2*PointIndex+1] = PointBoneIndices;
								
								VertexBoneWeights[2*PointIndex] = PointBoneWeights;
								VertexBoneWeights[2*PointIndex+1] = PointBoneWeights;
							}

						}, TransferBoneWeights.bUseParallel  ? EParallelForFlags::None : EParallelForFlags::ForceSingleThread);
					}
				}
			}
		}
	}
	
	template<typename FacadeType>
	FORCEINLINE void ExtractSkinningData(FManagedArrayCollection& GroomCollection, const FCollectionAttributeKey& BoneIndicesKey, const FCollectionAttributeKey& BoneWeightsKey)
	{
		if (!BoneIndicesKey.Attribute.IsEmpty() && !BoneWeightsKey.Attribute.IsEmpty())
		{
			FacadeType GroomFacade(GroomCollection);

			if(GroomFacade.IsValid())
			{
				const TManagedArray<FIntVector4>* BoneIndices = GroomCollection.FindAttributeTyped<FIntVector4>(FName(UE::Groom::FGroomGuidesFacade::PointBoneIndicesAttribute), FacadeType::VerticesGroup);
				const TManagedArray<FVector4f>* BoneWeights = GroomCollection.FindAttributeTyped<FVector4f>(FName(UE::Groom::FGroomGuidesFacade::PointBoneWeightsAttribute), FacadeType::VerticesGroup);
				
				if(BoneIndices && BoneWeights)
				{
					TManagedArray<TArray<int32>>& IndicesArray = GroomCollection.AddAttribute<TArray<int32>>(FName(BoneIndicesKey.Attribute), FName(BoneIndicesKey.Group));
					TManagedArray<TArray<float>>& WeightsArray = GroomCollection.AddAttribute<TArray<float>>(FName(BoneWeightsKey.Attribute), FName(BoneWeightsKey.Group));

					const int32 NumVertices = GroomFacade.GetNumVertices();
					for(int32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
					{
						const FIntVector4& PointIndices = (*BoneIndices)[VertexIndex];
						const FVector4f& PointWeights = (*BoneWeights)[VertexIndex];

						IndicesArray[VertexIndex].Reset(4);
						WeightsArray[VertexIndex].Reset(4);
						for(int32 WeightIndex = 0; WeightIndex < 4; ++WeightIndex)
						{
							if(PointIndices[WeightIndex] != INDEX_NONE)
							{
								IndicesArray[VertexIndex].Add(PointIndices[WeightIndex]);
								WeightsArray[VertexIndex].Add(PointWeights[WeightIndex]);
							}
						}
					}
				}
			}
		}
	}

	template<typename FacadeType>
	FORCEINLINE void ReportSkinningData(FManagedArrayCollection& GroomCollection, const FCollectionAttributeKey& BoneIndicesKey, const FCollectionAttributeKey& BoneWeightsKey)
	{
		if (!BoneIndicesKey.Attribute.IsEmpty() && !BoneWeightsKey.Attribute.IsEmpty())
		{
			FacadeType GroomFacade(GroomCollection);

			if(GroomFacade.IsValid())
			{
				const TManagedArray<TArray<int32>>* IndicesArray = GroomCollection.FindAttributeTyped<TArray<int32>>(FName(BoneIndicesKey.Attribute), FName(BoneIndicesKey.Group));
				const TManagedArray<TArray<float>>* WeightsArray = GroomCollection.FindAttributeTyped<TArray<float>>(FName(BoneWeightsKey.Attribute), FName(BoneWeightsKey.Group));

				if(IndicesArray && WeightsArray)
				{
					TManagedArray<FIntVector4>& BoneIndices = GroomCollection.AddAttribute<FIntVector4>(FName(UE::Groom::FGroomGuidesFacade::PointBoneIndicesAttribute), FacadeType::VerticesGroup);
					TManagedArray<FVector4f>& BoneWeights = GroomCollection.AddAttribute<FVector4f>(FName(UE::Groom::FGroomGuidesFacade::PointBoneWeightsAttribute), FacadeType::VerticesGroup);
				
					const int32 NumVertices = GroomFacade.GetNumVertices();
					for(int32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
					{
						FIntVector4 PointIndices(INDEX_NONE);
						FVector4f PointWeights(0.0f);

						const int32 NumWeights = FMath::Min(4, (*IndicesArray)[VertexIndex].Num());
						float TotalWeight = 0.0;
						for(int32 BoneIdx = 0; BoneIdx < NumWeights; ++BoneIdx)
						{
							PointIndices[BoneIdx] = (*IndicesArray)[VertexIndex][BoneIdx];
							PointWeights[BoneIdx] = (*WeightsArray)[VertexIndex][BoneIdx];
							TotalWeight += PointWeights[BoneIdx];
						}
						if(TotalWeight != 0.0)
						{
							for(int32 BoneIdx = 0; BoneIdx < NumWeights; ++BoneIdx)
							{
								PointWeights[BoneIdx] /= TotalWeight;
							}
						}
						BoneIndices[VertexIndex] = PointIndices;
						BoneWeights[VertexIndex] = PointWeights;
					}
				}
			}
		}
	}
}

void FTransferSkinWeightsGroomNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		FManagedArrayCollection GroomCollection = GetValue<FManagedArrayCollection>(Context, &Collection);

		if(CurvesType == EGroomCollectionType::Guides)
		{
			UE::Groom::Private::BuildSkinningData<UE::Groom::FGroomGuidesFacade>(GroomCollection, GroupIndex, SkeletalMesh, LODIndex, RelativeTransform);
		}
		else
		{
			UE::Groom::Private::BuildSkinningData<UE::Groom::FGroomStrandsFacade>(GroomCollection, GroupIndex, SkeletalMesh, LODIndex, RelativeTransform);
		}
		SetValue(Context, MoveTemp(GroomCollection), &Collection);
	}
	else if (Out->IsA<FCollectionAttributeKey>(&BoneIndicesKey))
	{
		SetValue(Context, GetBoneIndicesKey(), &BoneIndicesKey);
	}
	else if (Out->IsA<FCollectionAttributeKey>(&BoneWeightsKey))
	{
		SetValue(Context, GetBoneWeightsKey(), &BoneWeightsKey);
	}
}

FCollectionAttributeKey FTransferSkinWeightsGroomNode::GetBoneIndicesKey() const
{
	FCollectionAttributeKey Key;
	Key.Group = (CurvesType == EGroomCollectionType::Guides) ? UE::Groom::FGroomGuidesFacade::VerticesGroup.ToString() :
		UE::Groom::FGroomStrandsFacade::VerticesGroup.ToString();
	Key.Attribute = UE::Groom::FGroomGuidesFacade::PointBoneIndicesAttribute.ToString();
	return Key;
}

FCollectionAttributeKey FTransferSkinWeightsGroomNode::GetBoneWeightsKey() const
{
	FCollectionAttributeKey Key;
	Key.Group = (CurvesType == EGroomCollectionType::Guides) ? UE::Groom::FGroomGuidesFacade::VerticesGroup.ToString() :
		UE::Groom::FGroomStrandsFacade::VerticesGroup.ToString();
	Key.Attribute = UE::Groom::FGroomGuidesFacade::PointBoneWeightsAttribute.ToString();
	return Key;
}

TArray<UE::Dataflow::FRenderingParameter> FTransferSkinWeightsGroomNode::GetRenderParametersImpl() const
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

