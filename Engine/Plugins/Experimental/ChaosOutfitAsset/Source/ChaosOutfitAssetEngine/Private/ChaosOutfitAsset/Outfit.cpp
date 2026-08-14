// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosOutfitAsset/Outfit.h"
#include "Chaos/CollectionPropertyFacade.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "ChaosClothAsset/ClothSimulationModel.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "ChaosOutfitAsset/BodyUserData.h"
#include "ChaosOutfitAsset/CollectionOutfitFacade.h"
#include "ChaosOutfitAsset/OutfitAsset.h"
#include "ChaosOutfitAsset/OutfitAssetPrivate.h"
#include "ChaosOutfitAsset/OutfitCollection.h"
#include "ChaosOutfitAsset/SizedOutfitSource.h"
#include "Engine/SkeletalMesh.h"
#include "MeshResizing/RBFInterpolation.h"
#include "Misc/ScopedSlowTask.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "StaticMeshResources.h"
#include "Tasks/Task.h"
#include "Async/TaskGraphInterfaces.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(Outfit)

#define LOCTEXT_NAMESPACE "ChaosOutfit"

struct UChaosOutfit::FLODRenderData
{
	// Half edge buffers are always regenerated on the last merge, a dummy buffer is used until then
	struct FDummyHalfEdgeBuffer
	{
		bool bHasHalfEdges = false;
		
		bool IsCPUDataValid() const
		{
			return bHasHalfEdges;
		}
		void CleanUp()
		{
			bHasHalfEdges = false;
		}
		void Init(UChaosOutfit::FLODRenderData&)
		{
			bHasHalfEdges = true;
		}
	};

	TArray<FSkelMeshRenderSection> RenderSections;
	FMultiSizeIndexContainer MultiSizeIndexContainer;
	FStaticMeshVertexBuffers StaticVertexBuffers;
	FSkinWeightVertexBuffer SkinWeightVertexBuffer;
	FSkeletalMeshVertexClothBuffer ClothVertexBuffer;
	FSkinWeightProfilesData SkinWeightProfilesData;
	TArray<FBoneIndexType> ActiveBoneIndices;
	TArray<FBoneIndexType> RequiredBones;
	FDummyHalfEdgeBuffer HalfEdgeBuffer;

	uint32 GetNumVertices() const
	{
		return StaticVertexBuffers.PositionVertexBuffer.GetNumVertices();
	}

	bool HasClothData() const
	{
		for (int32 SectionIdx = 0; SectionIdx < RenderSections.Num(); SectionIdx++)
		{
			if (RenderSections[SectionIdx].HasClothingData())
			{
				return true;
			}
		}
		return false;	
	}

	friend FArchive& operator<<(FArchive& Ar, UChaosOutfit::FLODRenderData& LODRenderData)
	{
		static const FName MultiSizeIndexContainerName(TEXT("UChaosOutfit::FLODRenderData.MultiSizeIndexContainer"));
		static const FName StaticPositionVertexBufferName(TEXT("UChaosOutfit::FLODRenderData.StaticVertexBuffers.PositionVertexBuffer"));
		static const FName StaticMeshVertexBufferName(TEXT("UChaosOutfit::FLODRenderData.StaticVertexBuffers.StaticMeshVertexBuffer"));
		static const FName StaticColorVertexBufferName(TEXT("UChaosOutfit::FLODRenderData.StaticVertexBuffers.ColorVertexBuffer"));
		static const FName ClothVertexBufferName(TEXT("UChaosOutfit::FLODRenderData.ClothVertexBuffer"));
		static const FName SourceRayTracingGeometryName(TEXT("UChaosOutfit::FLODRenderData.SourceRayTracingGeometry"));

		constexpr bool bNeedsCPUAccess = true;

		Ar << LODRenderData.RenderSections;

		LODRenderData.MultiSizeIndexContainer.Serialize(Ar, bNeedsCPUAccess);
		LODRenderData.MultiSizeIndexContainer.GetIndexBuffer()->SetResourceName(MultiSizeIndexContainerName);

		LODRenderData.StaticVertexBuffers.PositionVertexBuffer.Serialize(Ar, bNeedsCPUAccess);
		LODRenderData.StaticVertexBuffers.PositionVertexBuffer.SetResourceName(StaticPositionVertexBufferName);
		LODRenderData.StaticVertexBuffers.StaticMeshVertexBuffer.Serialize(Ar, bNeedsCPUAccess);
		LODRenderData.StaticVertexBuffers.StaticMeshVertexBuffer.SetResourceName(StaticMeshVertexBufferName);

		Ar << LODRenderData.SkinWeightVertexBuffer;
		if (LODRenderData.HasClothData()) // This requires that RenderSections have already been serialized/deserialized
		{
			Ar << LODRenderData.ClothVertexBuffer;
			LODRenderData.ClothVertexBuffer.SetResourceName(ClothVertexBufferName);
		}

		Ar << LODRenderData.ActiveBoneIndices;
		Ar << LODRenderData.RequiredBones;
		Ar << LODRenderData.HalfEdgeBuffer.bHasHalfEdges;

		return Ar;
	}
};

struct UChaosOutfit::FRenderData
{
	TIndirectArray<FLODRenderData> LODRenderData;
	uint8 NumInlinedLODs = 0;
	uint8 NumNonOptionalLODs = 0;
	bool bSupportRayTracing = false;

	friend FArchive& operator<<(FArchive& Ar, UChaosOutfit::FRenderData& InRenderData)
	{
		Ar << InRenderData.LODRenderData;
		Ar << InRenderData.NumInlinedLODs;
		Ar << InRenderData.NumNonOptionalLODs;
		Ar << InRenderData.bSupportRayTracing;

		return Ar;
	}
};

namespace UE::Chaos::OutfitAsset::Private
{
	// Merge bone to a new skeleton and return its new index in the new skeleton
	// returns INDEX_NONE oif the parent don't match
	static int32 MergeBoneToSkeleton(const int32 BoneIndex, const FReferenceSkeleton& InSkeleton, FReferenceSkeleton& InOutSkeleton)
	{
		// Check if it exists in the new skeleton with the same parent
		const FName BoneName = InSkeleton.GetBoneName(BoneIndex);
		const int32 ParentBoneIndex = InSkeleton.GetParentIndex(BoneIndex);
		const int32 ExistingBoneIndex = InOutSkeleton.FindBoneIndex(BoneName);
		if (ExistingBoneIndex != INDEX_NONE ||  // Already exists or
			ParentBoneIndex == INDEX_NONE)      // it is the root bone, update the bone map
		{
			// Check if the parents matches
			const int32 ExistingParentBoneIndex = (ExistingBoneIndex != INDEX_NONE) ? InOutSkeleton.GetParentIndex(ExistingBoneIndex) : INDEX_NONE;
			if (!ensureMsgf((ParentBoneIndex == INDEX_NONE && ExistingParentBoneIndex == INDEX_NONE) ||
				(InSkeleton.GetBoneName(ParentBoneIndex) == InOutSkeleton.GetBoneName(ExistingParentBoneIndex)),
				TEXT("Bone parent must match")))
			{
				// TODO Better error management than ensure
			}
			// TODO: Warn if the refposes don't match

			// Update the bone map
			return ExistingBoneIndex;
		}
		else  // Doesn't already exists, add the bone
		{
			// Find its parent in the new skeleton
			const FName ParentBoneName = (ParentBoneIndex == INDEX_NONE) ? NAME_None : InSkeleton.GetBoneName(ParentBoneIndex);
			const int32 ExistingParentBoneIndex = InOutSkeleton.FindBoneIndex(ParentBoneName);
			checkf(ParentBoneIndex == INDEX_NONE || ExistingParentBoneIndex != INDEX_NONE, TEXT("Either root or a valid parent should exist at this point."));
			FReferenceSkeletonModifier ReferenceSkeletonModifier(InOutSkeleton, nullptr);

			FMeshBoneInfo MeshBoneInfo;
#if WITH_EDITORONLY_DATA
			MeshBoneInfo.ExportName = BoneName.ToString();
#endif
			MeshBoneInfo.Name = BoneName;
			MeshBoneInfo.ParentIndex = ExistingParentBoneIndex;
			ReferenceSkeletonModifier.Add(MeshBoneInfo, InSkeleton.GetRefBonePose()[BoneIndex]);

			// Update the bone map
			return ReferenceSkeletonModifier.FindBoneIndex(BoneName);
		}
	}

	static TArray<int32> GetChildren(const TArray<FMeshBoneInfo>& MeshBoneInfos, const int32 ParentIndex)
	{
		TArray<int32> Children;
		for (int32 Index = 0; Index < MeshBoneInfos.Num(); ++Index)
		{
			const FMeshBoneInfo& MeshBoneInfo = MeshBoneInfos[Index];
			if (MeshBoneInfo.ParentIndex == ParentIndex)
			{
				Children.Emplace(Index);
			}
		}
		return Children;
	}

	static void LogHierachy(const TArray<FMeshBoneInfo>& MeshBoneInfos, const int32 ParentIndex = INDEX_NONE, int32 Indent = 0)
	{
		auto MakeIndentString = [](const int32 Indent) -> FString
			{
				FString IndentString;
				for (int32 Index = 0; Index < Indent; ++Index)
				{
					IndentString += TEXT("   ");
				}
				return IndentString;
			};

		const FString IndentString = MakeIndentString(Indent);

		for (const int32 ChildIndex : GetChildren(MeshBoneInfos, ParentIndex))
		{
			const FString IndentedName = IndentString + MeshBoneInfos[ChildIndex].Name.ToString();

			UE_LOG(LogChaosOutfitAsset, VeryVerbose, TEXT("%d - %s"), ChildIndex, *IndentedName);
			
			LogHierachy(MeshBoneInfos, ChildIndex, Indent + 1);
		}
	}

	static void MergeSkeletons(const FReferenceSkeleton& InSkeleton, FReferenceSkeleton& InOutSkeleton, TArray<int32>& OutBoneMap)
	{
		const int32 NumBones = InSkeleton.GetNum();

		OutBoneMap.SetNumZeroed(NumBones);

		TBitArray BonesToProcess(true, NumBones);

		for (;;)
		{
			// Find the first unprocessed bone
			int32 BoneIndex = BonesToProcess.Find(true);
			if (BoneIndex == INDEX_NONE)
			{
				break;
			}

			// Replace by its parent in case they're not already processed (this assumes the bones aren't sorted to be on the safe side)
			for (int32 ParentBoneIndex = InSkeleton.GetParentIndex(BoneIndex);
				ParentBoneIndex != INDEX_NONE && BonesToProcess[ParentBoneIndex];
				ParentBoneIndex = InSkeleton.GetParentIndex(ParentBoneIndex))
			{
				BoneIndex = ParentBoneIndex;
			}

			// Merge the bone
			const int32 NewBoneIndex = MergeBoneToSkeleton(BoneIndex, InSkeleton, InOutSkeleton);
			OutBoneMap[BoneIndex] = NewBoneIndex;

			// Marked it as processed
			BonesToProcess[BoneIndex] = false;
		}

		UE_LOG(LogChaosOutfitAsset, VeryVerbose, TEXT("------- Outfit Reference Skseleton Merging --------"));
		LogHierachy(InOutSkeleton.GetRefBoneInfo());
		UE_LOG(LogChaosOutfitAsset, VeryVerbose, TEXT("---------------------------------------------------"));
		for (int32 Index = 0; Index < OutBoneMap.Num(); ++Index)
		{
			UE_LOG(LogChaosOutfitAsset, VeryVerbose, TEXT("%d -> %d"), Index, OutBoneMap[Index]);
		}
		UE_LOG(LogChaosOutfitAsset, VeryVerbose, TEXT("---------------------------------------------------"));
	}

	static TArray<uint32> GetIndices(const FMultiSizeIndexContainer& MultiSizeIndexContainer, int32 VertexOffset = 0)
	{
		TArray<uint32> IndexBuffer;
		if (MultiSizeIndexContainer.IsIndexBufferValid())
		{
			MultiSizeIndexContainer.GetIndexBuffer(IndexBuffer);
			if (VertexOffset)
			{
				for (uint32& Index : IndexBuffer)
				{
					Index += VertexOffset;
				}
			}
		}
		return IndexBuffer;
	}

	static TArray<FVector3f> GetPositions(const FPositionVertexBuffer& PositionVertexBuffer)
	{
		const uint32 NumVertices = PositionVertexBuffer.GetNumVertices();
		TArray<FVector3f> Positions;
		Positions.SetNumUninitialized(NumVertices);
		for (uint32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
		{
			Positions[VertexIndex] = PositionVertexBuffer.VertexPosition(VertexIndex);
		}
		return Positions;
	}

	static TArray<FVector4f> GetTangents(const FStaticMeshVertexBuffer& StaticMeshVertexBuffer, int Axis)
	{
		const uint32 NumVertices = StaticMeshVertexBuffer.GetNumVertices();
		TArray<FVector4f> Tangents;
		Tangents.SetNumUninitialized(NumVertices);
		for (uint32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
		{
			switch (Axis)
			{
			default: checkNoEntry();
			case 0: Tangents[VertexIndex] = StaticMeshVertexBuffer.VertexTangentX(VertexIndex); break;
			case 1: Tangents[VertexIndex] = StaticMeshVertexBuffer.VertexTangentY(VertexIndex); break;
			case 2: Tangents[VertexIndex] = StaticMeshVertexBuffer.VertexTangentZ(VertexIndex); break;
			}
		}
		return Tangents;
	}

	static TArray<FVector2f> GetVertexUVs(const FStaticMeshVertexBuffer& StaticMeshVertexBuffer, const uint32 MaxTexCoords)
	{
		const uint32 NumVertices = StaticMeshVertexBuffer.GetNumVertices();
		const uint32 NumTexCoords = StaticMeshVertexBuffer.GetNumTexCoords();
		check(NumTexCoords <= MaxTexCoords);

		TArray<FVector2f> VertexUVs;
		VertexUVs.SetNumUninitialized(NumVertices * MaxTexCoords);

		for (uint32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
		{
			for (uint32 UVIndex = 0; UVIndex < NumTexCoords; ++UVIndex)
			{
				VertexUVs[VertexIndex * MaxTexCoords + UVIndex] = StaticMeshVertexBuffer.GetVertexUV(VertexIndex, UVIndex);
			}
			for (uint32 UVIndex = NumTexCoords; UVIndex < MaxTexCoords; ++UVIndex)
			{
				VertexUVs[VertexIndex * MaxTexCoords + UVIndex] = FVector2f::ZeroVector;
			}
		}
		return VertexUVs;
	}

	static TArray<FSkinWeightInfo> GetSkinWeights(const FSkinWeightVertexBuffer& SkinWeightVertexBuffer, const bool bUse16BitBoneWeight, const TArray<int32>* const BoneMap = nullptr)
	{
		checkf(SkinWeightVertexBuffer.Use16BitBoneWeight() || !bUse16BitBoneWeight, TEXT("Weights can only be read from 8bit to 8bit, or 8bit to 16bit, but not 16bit to 8bit."));
		const bool bRenormalizeTo16BitBoneWeight = bUse16BitBoneWeight && !SkinWeightVertexBuffer.Use16BitBoneWeight();
		const uint32 MaxBoneInfluences = SkinWeightVertexBuffer.GetMaxBoneInfluences();
		const uint32 NumVertices = SkinWeightVertexBuffer.GetNumVertices();
		TArray<FSkinWeightInfo> SkinWeights;
		SkinWeights.SetNumUninitialized(NumVertices);
		for (uint32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
		{
			FSkinWeightInfo& SkinWeight = SkinWeights[VertexIndex];
			if (!bRenormalizeTo16BitBoneWeight)
			{
				for (uint32 InfluenceIndex = 0; InfluenceIndex < MaxBoneInfluences; ++InfluenceIndex)
				{
					SkinWeight.InfluenceBones[InfluenceIndex] = SkinWeightVertexBuffer.GetBoneIndex(VertexIndex, InfluenceIndex);
					SkinWeight.InfluenceWeights[InfluenceIndex] = SkinWeightVertexBuffer.GetBoneWeight(VertexIndex, InfluenceIndex);
				}
			}
			else
			{
				uint16 TotalInfluenceWeight = 0;
				for (uint32 InfluenceIndex = 0; InfluenceIndex < MaxBoneInfluences; ++InfluenceIndex)
				{
					SkinWeight.InfluenceBones[InfluenceIndex] = SkinWeightVertexBuffer.GetBoneIndex(VertexIndex, InfluenceIndex) << 8;
					SkinWeight.InfluenceWeights[InfluenceIndex] = SkinWeightVertexBuffer.GetBoneWeight(VertexIndex, InfluenceIndex);
					TotalInfluenceWeight += SkinWeight.InfluenceWeights[InfluenceIndex];
				}
				SkinWeight.InfluenceWeights[0] += TNumericLimits<uint16>::Max() - TotalInfluenceWeight;
			}
			// Clear the extra influences in case MaxBoneInfluences is changed
			for (uint32 InfluenceIndex = MaxBoneInfluences; InfluenceIndex < MAX_TOTAL_INFLUENCES; ++InfluenceIndex)
			{
				SkinWeight.InfluenceBones[InfluenceIndex] = 0;
				SkinWeight.InfluenceWeights[InfluenceIndex] = 0;
			}
		}
		if (BoneMap)
		{
			for (FSkinWeightInfo& SkinWeight : SkinWeights)
			{
				for (uint32 InfluenceIndex = 0; InfluenceIndex < MaxBoneInfluences; ++InfluenceIndex)
				{
					SkinWeight.InfluenceBones[InfluenceIndex] = (*BoneMap)[SkinWeight.InfluenceBones[InfluenceIndex]];
				}
			}
		}
		return SkinWeights;
	}

	static TArray<FColor> GetVertexColors(const FStaticMeshVertexBuffers& StaticMeshVertexBuffers)
	{
		const uint32 NumVertices = StaticMeshVertexBuffers.PositionVertexBuffer.GetNumVertices();
		const FColorVertexBuffer& ColorVertexBuffer = StaticMeshVertexBuffers.ColorVertexBuffer;
		TArray<FColor> VertexColors;
		VertexColors.SetNumUninitialized(NumVertices);
		const bool bHasVertexColors = ColorVertexBuffer.GetAllocatedSize() != 0;
		if (bHasVertexColors)
		{
			check(ColorVertexBuffer.GetNumVertices() == NumVertices);
			for (uint32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
			{
				VertexColors[VertexIndex] = ColorVertexBuffer.VertexColor(VertexIndex);
			}
		}
		else
		{
			for (FColor& VertexColor : VertexColors)
			{
				VertexColor = FColor::White;
			}
		}
		return VertexColors;
	}

	static TArray<FMeshToMeshVertData> GetClothMappingData(const TArray<FSkelMeshRenderSection>& RenderSections, const TArray<FGuid>* AssetGuids = nullptr)
	{
		TArray<FMeshToMeshVertData> MappingData;
		int32 NumMappingData = 0;
		for (const FSkelMeshRenderSection& RenderSection : RenderSections)
		{
			if (!AssetGuids ||
				!RenderSection.ClothingData.AssetGuid.IsValid() ||
				AssetGuids->Find(RenderSection.ClothingData.AssetGuid) != INDEX_NONE)
			{
				for (const TArray<FMeshToMeshVertData>& ClothMappingDataLOD : RenderSection.ClothMappingDataLODs)
				{
					NumMappingData += ClothMappingDataLOD.Num();
				}
			}
		}
		MappingData.Reserve(NumMappingData);
		for (const FSkelMeshRenderSection& RenderSection : RenderSections)
		{
			if (!AssetGuids ||
				!RenderSection.ClothingData.AssetGuid.IsValid() ||
				AssetGuids->Find(RenderSection.ClothingData.AssetGuid) != INDEX_NONE)
			{
				for (const TArray<FMeshToMeshVertData>& ClothMappingDataLOD : RenderSection.ClothMappingDataLODs)
				{
					MappingData.Append(ClothMappingDataLOD);
				}
			}
		}
		return MappingData;
	}

	static TArray<FClothBufferIndexMapping> GetClothBufferIndexMappings(
		const FSkeletalMeshVertexClothBuffer& ClothVertexBuffer,
		const TArray<FSkelMeshRenderSection>& RenderSections,
		const int32 VertexOffset = 0,
		const TArray<FGuid>* AssetGuids = nullptr)
	{
		int32 NumRenderSections = 0;
		for (const FSkelMeshRenderSection& RenderSection : RenderSections)
		{
			if (!AssetGuids ||
				!RenderSection.ClothingData.AssetGuid.IsValid() ||
				AssetGuids->Find(RenderSection.ClothingData.AssetGuid) != INDEX_NONE)
			{
				++NumRenderSections;
			}
		}

		TArray<FClothBufferIndexMapping> ClothBufferIndexMappings;
		if (!ClothVertexBuffer.GetClothIndexMapping().Num())
		{
			// Add empty mapping as some of the LOD sections have clothing
			ClothBufferIndexMappings.SetNumZeroed(NumRenderSections);  // FClothBufferIndexMapping has no default constructor
		}
		else
		{
			// Update the mappings index/offset
			for (int32 SectionIndex = 0; SectionIndex < RenderSections.Num(); ++SectionIndex)
			{
				const FSkelMeshRenderSection& RenderSection = RenderSections[SectionIndex];
				if (!AssetGuids ||
					!RenderSection.ClothingData.AssetGuid.IsValid() ||
					AssetGuids->Find(RenderSection.ClothingData.AssetGuid) != INDEX_NONE)
				{
					FClothBufferIndexMapping& ClothBufferIndexMapping = ClothBufferIndexMappings.Add_GetRef(ClothVertexBuffer.GetClothIndexMapping()[SectionIndex]);
					ClothBufferIndexMapping.BaseVertexIndex += VertexOffset;
					ClothBufferIndexMapping.MappingOffset += VertexOffset;
					// LODBiasStride stays the same since the number of mapping for this section hasn't changed
				}
			}
		}
		return ClothBufferIndexMappings;
	}

	static void MergeBones(const FReferenceSkeleton& ReferenceSkeleton, const TArray<int32> &BoneMap, const TArray<FBoneIndexType>& BoneIndices, TArray<FBoneIndexType>& OutBoneIndices)
	{
		OutBoneIndices.Reserve(OutBoneIndices.Num() + BoneIndices.Num());
		for (const uint16 BoneIndex : BoneIndices)
		{
			OutBoneIndices.AddUnique(BoneMap[BoneIndex]);
		}
		ReferenceSkeleton.EnsureParentsExistAndSort(OutBoneIndices);
		OutBoneIndices.Shrink();
	}

	template<typename T UE_REQUIRES(std::is_same_v<T, UChaosClothAssetBase> || std::is_same_v<T, TArray<FChaosOutfitPiece>>)>
	FChaosOutfitPiece Clone(const T& Pieces, int32 ModelIndex)
	{
		return FChaosOutfitPiece(Pieces, ModelIndex);
	}

	template<typename T UE_REQUIRES(std::is_same_v<T, UChaosClothAssetBase> || std::is_same_v<T, TArray<FChaosOutfitPiece>>)>
	FGuid GetAssetGuid(const T& Pieces, int32 ModelIndex);

	template<>
	FGuid GetAssetGuid<UChaosClothAssetBase>(const UChaosClothAssetBase& Pieces, int32 ModelIndex)
	{
		return Pieces.GetAssetGuid(ModelIndex);
	}

	template<>
	FGuid GetAssetGuid<TArray<FChaosOutfitPiece>>(const TArray<FChaosOutfitPiece>& Pieces, int32 ModelIndex)
	{
		return Pieces[ModelIndex].AssetGuid;
	}
}  // namespace UE::Chaos::OutfitAsset::Private

FChaosOutfitPiece::FChaosOutfitPiece()
	: ClothSimulationModel(MakeShared<FChaosClothSimulationModel>())
{
}

FChaosOutfitPiece::~FChaosOutfitPiece() = default;

FChaosOutfitPiece::FChaosOutfitPiece(
	FName InName,
	FGuid InAssetGuid,
	const UPhysicsAsset* InPhysicsAsset,
	const FChaosClothSimulationModel& InClothSimulationModel,
	const TArray<TSharedRef<const FManagedArrayCollection>>& InCollections)
	: Name(InName)
	, AssetGuid(InAssetGuid)
	, PhysicsAsset(InPhysicsAsset)
	, ClothSimulationModel(MakeShared<FChaosClothSimulationModel>(InClothSimulationModel))
{
	DeepCopyCollections(InCollections);
}

FChaosOutfitPiece::FChaosOutfitPiece(const UChaosClothAssetBase& ClothAssetBase, int32 ModelIndex)
	: FChaosOutfitPiece(
		ClothAssetBase.GetClothSimulationModelName(ModelIndex),
		ClothAssetBase.GetAssetGuid(ModelIndex),
		ClothAssetBase.GetPhysicsAssetForModel(ModelIndex),
		*ClothAssetBase.GetClothSimulationModel(ModelIndex),
		ClothAssetBase.GetCollections(ModelIndex))
{}

FChaosOutfitPiece::FChaosOutfitPiece(const FChaosOutfitPiece& Other)
	: FChaosOutfitPiece(
		Other.Name,
		Other.AssetGuid,
		Other.PhysicsAsset,
		*Other.ClothSimulationModel,
		Other.Collections)
{}

FChaosOutfitPiece::FChaosOutfitPiece(FChaosOutfitPiece&& Other)
	: Name(MoveTemp(Other.Name))
	, AssetGuid(MoveTemp(Other.AssetGuid))
	, PhysicsAsset(MoveTemp(Other.PhysicsAsset))
	, ClothSimulationModel(MoveTemp(Other.ClothSimulationModel))
	, Collections(MoveTemp(Other.Collections))
{}

FChaosOutfitPiece& FChaosOutfitPiece::operator=(const FChaosOutfitPiece& Other)
{
	if (&Other != this)
	{
		Name = Other.Name;
		PhysicsAsset = Other.PhysicsAsset;
		AssetGuid = Other.AssetGuid;
		*ClothSimulationModel = *Other.ClothSimulationModel;
		DeepCopyCollections(Other.Collections);
	}
	return *this;
}

FChaosOutfitPiece& FChaosOutfitPiece::operator=(FChaosOutfitPiece&& Other)
{
	if (&Other != this)
	{
		Name = MoveTemp(Other.Name);
		PhysicsAsset = MoveTemp(Other.PhysicsAsset);
		AssetGuid = MoveTemp(Other.AssetGuid);
		ClothSimulationModel = MoveTemp(Other.ClothSimulationModel);
		Collections = MoveTemp(Other.Collections);
	}
	return *this;
}

bool FChaosOutfitPiece::Serialize(FArchive& Ar)
{
	// Serialize what can be done with tagged properties
	{
		UScriptStruct* const Struct = FChaosOutfitPiece::StaticStruct();
		Struct->SerializeTaggedProperties(Ar, (uint8*)this, Struct, nullptr);
	}
	// Custom serialize the model since TSharedRef can't be declared as UPROPERTY
	{
		UScriptStruct* const Struct = FChaosClothSimulationModel::StaticStruct();
		Struct->SerializeTaggedProperties(Ar, (uint8*)&ClothSimulationModel.Get(), Struct, nullptr);
	}
	// Custom serialize the collections since TArray<TSharedRef> can't be declared as UPROPERTY
	int32 NumCollections = Collections.Num();
	Ar << NumCollections;

	if (Ar.IsLoading())
	{
		Collections.Reset(NumCollections);
		for (int32 Index = 0; Index < NumCollections; ++Index)
		{
			TSharedRef<FManagedArrayCollection> NewCollection = MakeShared<FManagedArrayCollection>();
			NewCollection->Serialize(Ar);
			Collections.Emplace(MoveTemp(NewCollection));
		}
	}
	else
	{
		for (int32 Index = 0; Index < NumCollections; ++Index)
		{
			ConstCastSharedRef<FManagedArrayCollection>(Collections[Index])->Serialize(Ar);
		}
	}
	return true;
}

void FChaosOutfitPiece::DeepCopyCollections(const TArray<TSharedRef<const FManagedArrayCollection>>& Other)
{
	Collections.Reserve(Other.Num());
	for (const TSharedRef<const FManagedArrayCollection>& Collection : Other)
	{
		Collections.Emplace(MakeShared<const FManagedArrayCollection>(*Collection));
	}
}

void FChaosOutfitPiece::RemapBoneIndices(const TArray<int32>& BoneMap)
{
	ClothSimulationModel->RemapBoneIndices(BoneMap);
}

UChaosOutfit::UChaosOutfit(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Init(
		Pieces,
		ReferenceSkeleton,
		RenderData,
		Materials,
		OutfitCollection);
}

UChaosOutfit::UChaosOutfit(FVTableHelper& Helper)
	: Super(Helper)
{}

UChaosOutfit::~UChaosOutfit() = default;

void UChaosOutfit::Append(const UChaosOutfit& Other, const FString& BodySizeNameFilter)
{
	Merge(
		Other.GetReferenceSkeleton(),
		Other.RenderData.Get(),
		Other.Materials,
		Other.OutfitCollection,
		Other.Pieces,
		Other.Pieces.Num(),
		BodySizeNameFilter,
		Pieces,
		ReferenceSkeleton,
		RenderData.Get(),
		Materials,
		OutfitCollection);
}

void UChaosOutfit::Add(const UChaosClothAssetBase& ClothAssetBase)
{
	using namespace UE::Chaos::OutfitAsset;

	// Retrieve the input outfit collection
	TOptional<FManagedArrayCollection> InOutfitCollection;
	const FManagedArrayCollection* InOutfitCollectionPtr = nullptr;

	if (const UChaosClothAsset* ClothAsset = ExactCast<const UChaosClothAsset>(&ClothAssetBase))
	{
		// Build some outfit metadata for this cloth asset
		FCollectionOutfitFacade InOutfitFacade(InOutfitCollection.Emplace());
		InOutfitFacade.DefineSchema();

		const FGuid OutfitGuid = FGuid::NewGuid();
		const int32 BodySize = InOutfitFacade.FindOrAddBodySize(DefaultBodySize.ToString());  // TODO: Should the default body size be part of the schema?
		InOutfitFacade.AddOutfit(OutfitGuid, BodySize, ClothAssetBase);

		InOutfitCollectionPtr = &InOutfitCollection.GetValue();
	}
	else if(const UChaosOutfitAsset* const OutfitAsset = ExactCast<const UChaosOutfitAsset>(&ClothAssetBase))
	{
		InOutfitCollectionPtr = &OutfitAsset->GetOutfitCollection();
	}
	check(InOutfitCollectionPtr);

	// Add cloth/outfit asset
	Merge(
		ClothAssetBase.GetRefSkeleton(),
		ClothAssetBase.GetResourceForRendering(),
		ClothAssetBase.GetMaterials(),
		*InOutfitCollectionPtr,
		ClothAssetBase,
		ClothAssetBase.GetNumClothSimulationModels(),
		{},
		Pieces,
		ReferenceSkeleton,
		RenderData.Get(),
		Materials,
		OutfitCollection);
}

void UChaosOutfit::Add(const FChaosSizedOutfitSource& SizedOutfitSource, const FGuid& OutfitGuid)
{
	using namespace UE::Chaos::OutfitAsset;

	// Add body size
	FManagedArrayCollection InOutfitCollection;
	FCollectionOutfitFacade InOutfitFacade(InOutfitCollection);
	InOutfitFacade.DefineSchema();

	int32 BodySize = INDEX_NONE;

	const FString BodySizeName = SizedOutfitSource.GetBodySizeName();
	if (!BodySizeName.IsEmpty())
	{
		TArray<FString> BodyPartsSkeletalMeshes;
		TArray<TArray<int32>> InterpolationData_SampleIndices;
		TArray<TArray<FVector3f>> InterpolationData_SampleRestPositions;
		TArray<TArray<float>> InterpolationData_InterpolationWeights;
		BodyPartsSkeletalMeshes.Reserve(SizedOutfitSource.SourceBodyParts.Num());
		TMap<FString, float> Measurements;
		for (const TObjectPtr<const USkeletalMesh>& SourceBodyPart : SizedOutfitSource.SourceBodyParts)
		{
			if (SourceBodyPart)
			{
				if (SourceBodyPart->GetAssetUserDataArray())
				{
					for (const UAssetUserData* const AssetUserData : *SourceBodyPart->GetAssetUserDataArray())  // Not using the simpler GetAssetUserDataOfClass here because it is not const
					{
						if (const UChaosOutfitAssetBodyUserData* const BodyAssetUserData =
							Cast<const UChaosOutfitAssetBodyUserData>(AssetUserData))
						{
							Measurements.Append(BodyAssetUserData->Measurements);
							break;
						}
					}
				}
				BodyPartsSkeletalMeshes.Emplace(SourceBodyPart->GetPathName());
#if WITH_EDITORONLY_DATA
				if (SizedOutfitSource.NumResizingInterpolationPoints > 0)
				{
					FMeshResizingRBFInterpolationData InterpData;
					if (const FMeshDescription* const MeshDescription = SourceBodyPart->GetMeshDescription(0))
					{
						constexpr float NumSteps = 1.f;
						FScopedSlowTask SlowTask(NumSteps);
						SlowTask.MakeDialog();  // Can't delay the dialog or it won't show up
						SlowTask.EnterProgressFrame(NumSteps, LOCTEXT("GeneratingRBFInterpolationWeights", "Generating RBF interpolation weights (please wait, this can take several minutes)..."));
						SlowTask.TickProgress();
						SlowTask.ForceRefresh();
						UE::MeshResizing::FRBFInterpolation::GenerateWeights(*MeshDescription, SizedOutfitSource.NumResizingInterpolationPoints, InterpData);
					}
					InterpolationData_SampleIndices.Emplace(MoveTemp(InterpData.SampleIndices));
					InterpolationData_SampleRestPositions.Emplace(MoveTemp(InterpData.SampleRestPositions));
					InterpolationData_InterpolationWeights.Emplace(MoveTemp(InterpData.InterpolationWeights));
				}
#endif
			}
		}
		FCollectionOutfitConstFacade::FRBFInterpolationDataWrapper InterpolationData;
		InterpolationData.SampleIndices = TConstArrayView<TArray<int32>>(InterpolationData_SampleIndices);
		InterpolationData.SampleRestPositions = TConstArrayView<TArray<FVector3f>>(InterpolationData_SampleRestPositions);
		InterpolationData.InterpolationWeights = TConstArrayView<TArray<float>>(InterpolationData_InterpolationWeights);
		BodySize = InOutfitFacade.AddBodySize(BodySizeName, BodyPartsSkeletalMeshes, Measurements, InterpolationData);
	}
	else
	{
		BodySize = InOutfitFacade.FindOrAddBodySize(DefaultBodySize.ToString());
	}

	// Add source asset pieces
	if (const UChaosClothAssetBase* const ClothAssetBase = SizedOutfitSource.SourceAsset)
	{
		// Move the entire set of outfit pieces under the new size/GUID
		InOutfitFacade.AddOutfit(OutfitGuid, BodySize, *ClothAssetBase);

		// Add cloth/outfit asset
		Merge(
			ClothAssetBase->GetRefSkeleton(),
			ClothAssetBase->GetResourceForRendering(),
			ClothAssetBase->GetMaterials(),
			InOutfitCollection,
			*ClothAssetBase,
			ClothAssetBase->GetNumClothSimulationModels(),
			{},
			Pieces,
			ReferenceSkeleton,
			RenderData.Get(),
			Materials,
			OutfitCollection);
	}
	else
	{
		// Only add the size
		FCollectionOutfitFacade OutfitFacade(OutfitCollection);
		OutfitFacade.Append(InOutfitFacade);
	}
}

void UChaosOutfit::CopyTo(
	TArray<FChaosOutfitPiece>& OutPieces,
	FReferenceSkeleton& OutReferenceSkeleton,
	TUniquePtr<FSkeletalMeshRenderData>& OutSkeletalMeshRenderData,
	TArray<FSkeletalMaterial>& OutMaterials,
	FManagedArrayCollection& OutOutfitCollection) const
{
	Init(
		OutPieces,
		OutReferenceSkeleton,
		OutSkeletalMeshRenderData,
		OutMaterials,
		OutOutfitCollection);

	Merge(
		ReferenceSkeleton,
		RenderData.Get(),
		Materials,
		OutfitCollection,
		Pieces,
		Pieces.Num(),
		{},
		OutPieces,
		OutReferenceSkeleton,
		OutSkeletalMeshRenderData.Get(),
		OutMaterials,
		OutOutfitCollection);
}

int32 UChaosOutfit::GetNumLods() const
{
	int32 NumLods = 0;
	for (const FChaosOutfitPiece& Piece : Pieces)
	{
		NumLods = FMath::Max(Piece.Collections.Num(), NumLods);
	}
	return NumLods;
}

TArray<TSharedRef<const FManagedArrayCollection>> UChaosOutfit::GetClothCollections(int32 StartLodIndex) const
{
	int32 NumLods;
	if (StartLodIndex == INDEX_NONE)
	{
		// Get all LODs
		NumLods = GetNumLods();
		StartLodIndex = 0;
	}
	else
	{
		NumLods = 1;
	}

	TArray<TSharedRef<const FManagedArrayCollection>> ClothCollections;
	ClothCollections.Reserve(NumLods * Pieces.Num());

	TSharedRef<FManagedArrayCollection> EmptyClothCollection = MakeShared<FManagedArrayCollection>();
	UE::Chaos::ClothAsset::FCollectionClothFacade ClothFacade(EmptyClothCollection);
	ClothFacade.DefineSchema();

	for (const FChaosOutfitPiece& Piece : Pieces)
	{
		for (int32 LodIndex = StartLodIndex; LodIndex < StartLodIndex + NumLods; ++LodIndex)
		{
			ClothCollections.Emplace(Piece.Collections.IsValidIndex(LodIndex) ?
				Piece.Collections[LodIndex] :
				EmptyClothCollection);
		}
	}
	return ClothCollections;
}

bool UChaosOutfit::HasBodySize(const FString& SizeName) const
{
	UE::Chaos::OutfitAsset::FCollectionOutfitConstFacade OutfitFacade(OutfitCollection);
	return OutfitFacade.IsValid() && OutfitFacade.HasBodySize(SizeName);
}

void UChaosOutfit::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar << ReferenceSkeleton;
	Ar << *RenderData;
}

template<typename RenderDataType UE_REQUIRES_DEFINITION(std::is_same_v<RenderDataType, FSkeletalMeshRenderData> || std::is_same_v<RenderDataType, UChaosOutfit::FRenderData>)>
void UChaosOutfit::Init(
	TArray<FChaosOutfitPiece>& OutPieces,
	FReferenceSkeleton& OutReferenceSkeleton,
	TUniquePtr<RenderDataType>& OutSkeletalMeshRenderData,
	TArray<FSkeletalMaterial>& OutMaterials,
	FManagedArrayCollection& OutOutfitCollection)
{
	OutPieces.Reset();
	OutReferenceSkeleton.Empty();
	OutSkeletalMeshRenderData = MakeUnique<RenderDataType>();
	OutMaterials.Reset();
	OutOutfitCollection.Reset();

	// Create a default valid reference skeleton
	OutReferenceSkeleton.Empty(1);
	FReferenceSkeletonModifier ReferenceSkeletonModifier(OutReferenceSkeleton, nullptr);
	FMeshBoneInfo MeshBoneInfo;
	constexpr const TCHAR* RootName = TEXT("Root");
	MeshBoneInfo.ParentIndex = INDEX_NONE;
#if WITH_EDITORONLY_DATA
	MeshBoneInfo.ExportName = RootName;
#endif
	MeshBoneInfo.Name = FName(RootName);
	ReferenceSkeletonModifier.Add(MeshBoneInfo, FTransform::Identity);

	// Set default values for the empty render data that can be usefully used in a merge
	OutSkeletalMeshRenderData->bSupportRayTracing = false;

	// Init outfit collection
	UE::Chaos::OutfitAsset::FCollectionOutfitFacade OutfitFacade(OutOutfitCollection);
	OutfitFacade.DefineSchema();
}

template<typename InLODRenderDataType, typename OutLODRenderDataType>
void UChaosOutfit::MergeLODRenderDatas(
	const InLODRenderDataType& LODRenderData,
	const TArray<FGuid>& AssetGuids,
	const int32 MaterialOffset,
	const FReferenceSkeleton& ReferenceSkeleton,
	const TArray<int32>& BoneMap,
	OutLODRenderDataType& OutLODRenderData)
{
	using namespace UE::Chaos::OutfitAsset::Private;

	const uint32 VertexOffset = OutLODRenderData.GetNumVertices();
	const uint32 NumVertices = VertexOffset + LODRenderData.GetNumVertices();

	// Merge index buffer
	TArray<uint32> IndexBuffer = GetIndices(OutLODRenderData.MultiSizeIndexContainer);
	const uint32 IndexOffset = IndexBuffer.Num();

	IndexBuffer.Append(GetIndices(LODRenderData.MultiSizeIndexContainer, VertexOffset));
	const uint32 NumIndices = IndexBuffer.Num();

	const uint8 DataTypeSize = (NumVertices < (uint32)TNumericLimits<uint16>::Max()) ? sizeof(uint16) : sizeof(uint32);
	OutLODRenderData.MultiSizeIndexContainer.RebuildIndexBuffer(DataTypeSize, IndexBuffer);

	// Merge positions
	TArray<FVector3f> Positions = GetPositions(OutLODRenderData.StaticVertexBuffers.PositionVertexBuffer);
	Positions.Append(GetPositions(LODRenderData.StaticVertexBuffers.PositionVertexBuffer));
	check(Positions.Num() == NumVertices);

	// Merge tangents
	const bool bUseHighPrecisionTangentBasis =
		OutLODRenderData.StaticVertexBuffers.StaticMeshVertexBuffer.GetUseHighPrecisionTangentBasis() ||
		LODRenderData.StaticVertexBuffers.StaticMeshVertexBuffer.GetUseHighPrecisionTangentBasis();

	TArray<FVector4f> Tangents[3];
	for (int Axis = 0; Axis < 3; ++Axis)
	{
		Tangents[Axis] = GetTangents(OutLODRenderData.StaticVertexBuffers.StaticMeshVertexBuffer, Axis);
		Tangents[Axis].Append(GetTangents(LODRenderData.StaticVertexBuffers.StaticMeshVertexBuffer, Axis));
		check(Tangents[Axis].Num() == NumVertices);
	}

	// Merge UVs
	const uint32 MaxTexCoords = FMath::Max(
		LODRenderData.StaticVertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords(),
		OutLODRenderData.StaticVertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords());
	const bool bUseFullPrecisionUVs =
		OutLODRenderData.StaticVertexBuffers.StaticMeshVertexBuffer.GetUseFullPrecisionUVs() ||
		LODRenderData.StaticVertexBuffers.StaticMeshVertexBuffer.GetUseFullPrecisionUVs();

	TArray<FVector2f> VertexUVs = GetVertexUVs(OutLODRenderData.StaticVertexBuffers.StaticMeshVertexBuffer, MaxTexCoords);
	VertexUVs.Append(GetVertexUVs(LODRenderData.StaticVertexBuffers.StaticMeshVertexBuffer, MaxTexCoords));
	check(VertexUVs.Num() == NumVertices * MaxTexCoords);

	// Merge Vertex attributes

	// TODO: 
	//     The Cloth Asset doesn't have any vertex attributes,
	//     but if it had, merging them is non trivial due to no access to the names

	// Merge Vertex colors
	const bool bHasVertexColors = NumVertices && (
		OutLODRenderData.StaticVertexBuffers.ColorVertexBuffer.GetAllocatedSize() != 0 ||
		LODRenderData.StaticVertexBuffers.ColorVertexBuffer.GetAllocatedSize() != 0);

	TArray<FColor> VertexColors;
	if (bHasVertexColors)
	{
		VertexColors = GetVertexColors(OutLODRenderData.StaticVertexBuffers);
		VertexColors.Append(GetVertexColors(LODRenderData.StaticVertexBuffers));
		check(VertexColors.Num() == NumVertices);
	}

	// Init vertex buffers
	FPositionVertexBuffer& PositionVertexBuffer = OutLODRenderData.StaticVertexBuffers.PositionVertexBuffer;
	FStaticMeshVertexBuffer& StaticMeshVertexBuffer = OutLODRenderData.StaticVertexBuffers.StaticMeshVertexBuffer;

	StaticMeshVertexBuffer.SetUseFullPrecisionUVs(bUseFullPrecisionUVs);
	StaticMeshVertexBuffer.SetUseHighPrecisionTangentBasis(bUseHighPrecisionTangentBasis);

	PositionVertexBuffer.Init(NumVertices);
	StaticMeshVertexBuffer.Init(NumVertices, MaxTexCoords);

	for (uint32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
	{
		PositionVertexBuffer.VertexPosition(VertexIndex) = Positions[VertexIndex];
		StaticMeshVertexBuffer.SetVertexTangents(VertexIndex, Tangents[0][VertexIndex], Tangents[1][VertexIndex], Tangents[2][VertexIndex]);
		for (uint32 UVIndex = 0; UVIndex < MaxTexCoords; ++UVIndex)
		{
			StaticMeshVertexBuffer.SetVertexUV(VertexIndex, UVIndex, VertexUVs[VertexIndex * MaxTexCoords + UVIndex]);
		}
	}
	if (bHasVertexColors)
	{
		FColorVertexBuffer& ColorVertexBuffer = OutLODRenderData.StaticVertexBuffers.ColorVertexBuffer;
		ColorVertexBuffer.InitFromColorArray(VertexColors.GetData(), NumVertices);
	}

	// Skinweight buffer
	const bool bNeedsCPUAccess =
		OutLODRenderData.SkinWeightVertexBuffer.GetNeedsCPUAccess() ||
		LODRenderData.SkinWeightVertexBuffer.GetNeedsCPUAccess();
	const uint32 MaxBoneInfluences = FMath::Max(
		OutLODRenderData.SkinWeightVertexBuffer.GetMaxBoneInfluences(),
		LODRenderData.SkinWeightVertexBuffer.GetMaxBoneInfluences());
	const bool bUse16BitBoneIndex =
		OutLODRenderData.SkinWeightVertexBuffer.Use16BitBoneIndex() ||
		LODRenderData.SkinWeightVertexBuffer.Use16BitBoneIndex();
	const bool bUse16BitBoneWeight =
		OutLODRenderData.SkinWeightVertexBuffer.Use16BitBoneWeight() ||
		LODRenderData.SkinWeightVertexBuffer.Use16BitBoneWeight();

	TArray<FSkinWeightInfo> SkinWeights = GetSkinWeights(OutLODRenderData.SkinWeightVertexBuffer, bUse16BitBoneWeight);
	SkinWeights.Append(GetSkinWeights(LODRenderData.SkinWeightVertexBuffer, bUse16BitBoneWeight, &BoneMap));

	OutLODRenderData.SkinWeightVertexBuffer.SetNeedsCPUAccess(bNeedsCPUAccess);
	OutLODRenderData.SkinWeightVertexBuffer.SetMaxBoneInfluences(MaxBoneInfluences);
	OutLODRenderData.SkinWeightVertexBuffer.SetUse16BitBoneIndex(bUse16BitBoneIndex);
	OutLODRenderData.SkinWeightVertexBuffer.SetUse16BitBoneWeight(bUse16BitBoneWeight);
	OutLODRenderData.SkinWeightVertexBuffer = SkinWeights;  // Assigns the skinweights

	// Skinweight profiles
	OutLODRenderData.SkinWeightProfilesData.Init(&OutLODRenderData.SkinWeightVertexBuffer);

	// TODO: 
	//     The Cloth Asset doesn't have SkinWeightProfilesData yet, 
	//     but the merging code will have to be implemented once it does.

	// Half edges
	const bool bHasHalfEdges = NumVertices && (
		OutLODRenderData.HalfEdgeBuffer.IsCPUDataValid() ||
		LODRenderData.HalfEdgeBuffer.IsCPUDataValid());

	if (bHasHalfEdges)
	{
		OutLODRenderData.HalfEdgeBuffer.CleanUp();
		OutLODRenderData.HalfEdgeBuffer.Init(OutLODRenderData);
	}

	// Merge cloth data
	const bool bHasClothingData = NumVertices && (
		OutLODRenderData.ClothVertexBuffer.GetNumVertices() != 0 ||  // Note GetNumVertices() doesn't return NumVertices because
		LODRenderData.ClothVertexBuffer.GetNumVertices() != 0);      // of the LODBias there could be several mappings per vertex

	if (bHasClothingData)
	{
		TArray<FMeshToMeshVertData> ClothMappingData = GetClothMappingData(OutLODRenderData.RenderSections);
		ClothMappingData.Append(GetClothMappingData(LODRenderData.RenderSections, &AssetGuids));

		TArray<FClothBufferIndexMapping> ClothIndexMappings =
			GetClothBufferIndexMappings(OutLODRenderData.ClothVertexBuffer, OutLODRenderData.RenderSections);
		ClothIndexMappings.Append(
			GetClothBufferIndexMappings(LODRenderData.ClothVertexBuffer, LODRenderData.RenderSections, VertexOffset, &AssetGuids));

		OutLODRenderData.ClothVertexBuffer.Init(ClothMappingData, ClothIndexMappings);
	}

	// Merge sections
	const int32 SectionOffset = OutLODRenderData.RenderSections.Num();  // The index of the first merged section
	const int32 NumSections = SectionOffset + LODRenderData.RenderSections.Num();
	OutLODRenderData.RenderSections.Reserve(NumSections);

	for (const FSkelMeshRenderSection& RenderSection : LODRenderData.RenderSections)
	{
		const int32 PieceIndex = AssetGuids.Find(RenderSection.ClothingData.AssetGuid);
		if (PieceIndex != INDEX_NONE || !RenderSection.ClothingData.AssetGuid.IsValid())
		{
			FSkelMeshRenderSection& OutRenderSection = OutLODRenderData.RenderSections.AddDefaulted_GetRef();

			OutRenderSection.MaterialIndex = RenderSection.MaterialIndex + MaterialOffset;
			OutRenderSection.BaseIndex = RenderSection.BaseIndex + IndexOffset;
			OutRenderSection.NumTriangles = RenderSection.NumTriangles;
			OutRenderSection.bRecomputeTangent = RenderSection.bRecomputeTangent;
			OutRenderSection.bCastShadow = RenderSection.bCastShadow;
			OutRenderSection.bVisibleInRayTracing = RenderSection.bVisibleInRayTracing;
			OutRenderSection.RecomputeTangentsVertexMaskChannel = RenderSection.RecomputeTangentsVertexMaskChannel;
			OutRenderSection.BaseVertexIndex = RenderSection.BaseVertexIndex + VertexOffset;
			OutRenderSection.ClothMappingDataLODs = RenderSection.ClothMappingDataLODs;
			OutRenderSection.BoneMap = RenderSection.BoneMap;
			OutRenderSection.NumVertices = RenderSection.NumVertices;
			OutRenderSection.MaxBoneInfluences = RenderSection.MaxBoneInfluences;
			OutRenderSection.CorrespondClothAssetIndex = PieceIndex;
			OutRenderSection.ClothingData = RenderSection.ClothingData;
			OutRenderSection.DuplicatedVerticesBuffer.Init(0, TMap<int32, TArray<int32>>());
			OutRenderSection.bDisabled = RenderSection.bDisabled;

			for (FBoneIndexType& BoneIndex : OutRenderSection.BoneMap)
			{
				BoneIndex = BoneMap[BoneIndex];
			}
		}
	}

	// Update used bone indices
	MergeBones(ReferenceSkeleton, BoneMap, LODRenderData.ActiveBoneIndices, OutLODRenderData.ActiveBoneIndices);
	MergeBones(ReferenceSkeleton, BoneMap, LODRenderData.RequiredBones, OutLODRenderData.RequiredBones);
}

template<typename InRenderDataType, typename OutRenderDataType, typename PiecesType>
void UChaosOutfit::Merge(
	const FReferenceSkeleton& InReferenceSkeleton,
	const InRenderDataType* const InSkeletalMeshRenderData,
	const TArray<FSkeletalMaterial>& InMaterials,
	const FManagedArrayCollection& InOutfitCollection,
	const PiecesType& Pieces,
	const int32 NumPieces,
	const FString& BodySizeNameFilter,
	TArray<FChaosOutfitPiece>& OutPieces,
	FReferenceSkeleton& OutReferenceSkeleton,
	OutRenderDataType* const OutSkeletalMeshRenderData,
	TArray<FSkeletalMaterial>& OutMaterials,
	FManagedArrayCollection& OutOutfitCollection)
{
	using namespace UE::Chaos::OutfitAsset;

	// Filter the body size pieces to merge, or select all pieces
	TArray<FGuid> AssetGuidsFilter;

	const FCollectionOutfitConstFacade InOutfitFacade(InOutfitCollection);
	FCollectionOutfitFacade OutOutfitFacade(OutOutfitCollection);
	check(InOutfitFacade.IsValid());
	check(OutOutfitFacade.IsValid());
	
	if (!BodySizeNameFilter.IsEmpty())
	{
		// Filter to the requested body size
		const int32 BodySize = InOutfitFacade.FindBodySize(BodySizeNameFilter);
		if (BodySize == INDEX_NONE)
		{
			UE_LOG(LogChaosOutfitAsset, Display, TEXT("The requested body size [%s] isn't available to merge."), *BodySizeNameFilter);
			return;
		}

		// Find all pieces assigned to this body size
		TMap<FGuid, FString> OutfitPieces;
		for (const FGuid& Guid : InOutfitFacade.GetOutfitGuids())
		{
			OutfitPieces.Append(InOutfitFacade.GetOutfitPieces(Guid, BodySize));
		}
		OutfitPieces.GetKeys(AssetGuidsFilter);

		// Merge collection, but only the requested body size
		OutOutfitFacade.Append(InOutfitFacade, BodySize);
	}
	else
	{
		// Add all pieces to the merge
		AssetGuidsFilter = InOutfitFacade.GetOutfitPiecesGuids();

		// Merge the entire collection
		OutOutfitFacade.Append(InOutfitFacade);
	}

	// Merge skeletons
	TArray<int32> BoneMap;
	Private::MergeSkeletons(InReferenceSkeleton, OutReferenceSkeleton, BoneMap);

	// Merge pieces
	TArray<FGuid> AssetGuids;
	AssetGuids.Reserve(NumPieces);
	OutPieces.Reserve(OutPieces.Num() + NumPieces);

	for (int32 PieceIndex = 0; PieceIndex < NumPieces; ++PieceIndex)
	{
		if (AssetGuidsFilter.Contains(Private::GetAssetGuid(Pieces, PieceIndex)))
		{
			FChaosOutfitPiece& Piece = OutPieces.Add_GetRef(Private::Clone(Pieces, PieceIndex));
			Piece.RemapBoneIndices(BoneMap);
			// TODO: Fix case where a piece already exists in the outfit as to not mess up the LODRenderData piece indices
			UE_CLOG(AssetGuids.Contains(Piece.AssetGuid), LogChaosOutfitAsset, Warning, TEXT("Piece [%s] already exists in this Outfit!."), *Piece.Name.ToString());
			AssetGuids.Add(Piece.AssetGuid);
		}
	}

	// Merge materials
	const int32 MaterialOffset = OutMaterials.Num();
	OutMaterials.Append(InMaterials);  // TODO: Merge duplicate material slots. But when doing so, don't forget to consider and update the fix up code in GetOutiftClothCollectionsNode.cpp.

	// Merge render data, only for existing LODs
	// Note: This does not create LODs for the added sections when they are missing
	//       as it would require to duplicate data and add new LODBias clothing data.
	if (InSkeletalMeshRenderData)
	{
		using InLODRenderDataType = decltype(InSkeletalMeshRenderData->LODRenderData);
		using OutLODRenderDataType = decltype(OutSkeletalMeshRenderData->LODRenderData);
		using LODRenderDataElementType = typename OutLODRenderDataType::ElementType;

		const InLODRenderDataType& InLODRenderData = InSkeletalMeshRenderData->LODRenderData;
		OutLODRenderDataType& LODRenderData = OutSkeletalMeshRenderData->LODRenderData;

		const int32 NumLODs = InLODRenderData.Num();
		LODRenderData.Reserve(NumLODs);

		for (int32 LODIndex = 0; LODIndex < NumLODs; ++LODIndex)
		{
			if (LODIndex >= LODRenderData.Num())
			{
				LODRenderData.Add(new LODRenderDataElementType());
			}
		}

		if (NumLODs > 1)
		{

			TArray<UE::Tasks::FTask> PendingTasks;
			PendingTasks.Reserve(NumLODs);
			for (int32 LODIndex = 0; LODIndex < NumLODs; ++LODIndex)
			{
				UE::Tasks::FTask PendingTask = UE::Tasks::Launch(UE_SOURCE_LOCATION, [LODIndex , &InLODRenderData, &AssetGuids, MaterialOffset, &OutReferenceSkeleton, &BoneMap, &LODRenderData]()
				{
					MergeLODRenderDatas(
						InLODRenderData[LODIndex],
						AssetGuids,
						MaterialOffset,
						OutReferenceSkeleton,
						BoneMap,
						LODRenderData[LODIndex]);
				});
				PendingTasks.Add(PendingTask);
			}
			UE::Tasks::Wait(PendingTasks);
		}
		else if (NumLODs == 1)
		{
			const int32 LODIndex = 0;

			MergeLODRenderDatas(
				InLODRenderData[LODIndex],
				AssetGuids,
				MaterialOffset,
				OutReferenceSkeleton,
				BoneMap,
				LODRenderData[LODIndex]);
		}

		OutSkeletalMeshRenderData->NumInlinedLODs = NumLODs;
		OutSkeletalMeshRenderData->NumNonOptionalLODs = NumLODs;
		OutSkeletalMeshRenderData->bSupportRayTracing = OutSkeletalMeshRenderData->bSupportRayTracing || InSkeletalMeshRenderData->bSupportRayTracing;
	}
}

#undef LOCTEXT_NAMESPACE
