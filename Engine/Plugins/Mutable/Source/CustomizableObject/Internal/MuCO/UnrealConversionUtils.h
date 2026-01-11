// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

//! Order of the unreal vertex buffers when in mutable data
#define MUTABLE_VERTEXBUFFER_POSITION	0
#define MUTABLE_VERTEXBUFFER_TANGENT	1
#define MUTABLE_VERTEXBUFFER_TEXCOORDS	2

#include "MuR/Ptr.h"
#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/Map.h"
#include "Tasks/Task.h"

struct FClothBufferIndexMapping;
struct FMeshToMeshVertData;
struct FCustomizableObjectMeshToMeshVertData;
struct FClothingMeshData;
struct FMorphTargetMeshData;

namespace mu
{
	class FMesh;
	class FMeshBufferSet;
	struct FBoneName;
}
struct FReferenceSkeleton;
struct FMutableSurfaceMetadata;
class UModelResources;
class FSkeletalMeshLODRenderData;
class USkeletalMesh;
class USkeleton;

namespace UnrealConversionUtils
{
	struct FSectionClothData
	{
		int32 SectionIndex;
		int32 LODIndex;
		int32 BaseVertex;
		TArrayView<const uint16> SectionIndex16View;
		TArrayView<const uint32> SectionIndex32View;
		TArrayView<const int32> ClothingDataIndicesView;
		TArrayView<const FCustomizableObjectMeshToMeshVertData> ClothingDataView;
		TArray<FMeshToMeshVertData> MappingData;
	};
	
	/*
	 * Shared methods between mutable instance update and viewport mesh generation
	 * These are the result of stripping up parts of the reference code to be able to share it on the two
	 * current pipelines (instance update and USkeletal mesh generation for a mesh viewport)
	 */
	
	/**
	 * Prepares the render sections found on the InSkeletalMesh and sets them up accordingly what the InMutableMesh requires
	 * @param LODResource - LODRenderData whose sections are ought to be updated
	 * @param InMutableMesh - Mutable mesh to be used as reference for the section data update on the Skeletal Mesh
	 * @param InBoneMap - Bones to be set as part of the sections.
	 * @param InFirstBoneMapIndex - Index to the first BoneMap bone that belongs to this LODResource.
	 * @param SectionMetadata - Section metadata for each surface in InMutableMesh. 
	 */
	CUSTOMIZABLEOBJECT_API void SetupRenderSections(
		FSkeletalMeshLODRenderData& LODResource,
		const mu::FMesh* InMutableMesh,
		const TArray<mu::FBoneName>& InBoneMap,
		const TMap<mu::FBoneName, TPair<FName, uint16>>& BoneInfoMap,
		const int32 InFirstBoneMapIndex,
		const TArray<const FMutableSurfaceMetadata*>& SectionMetadata);


	/** Initializes the LODResource's VertexBuffers with dummy data to prepare it for streaming.
	 * @param LODResource - LODRenderData to update
	 * @param InMutableMesh - Mutable mesh to be used as reference for the section data update on the Skeletal Mesh
	 * @param bAllowCPUAccess - Keeps this LODs data on the CPU so it can be used for things such as sampling in FX.
	 */
	CUSTOMIZABLEOBJECT_API void InitVertexBuffersWithDummyData(
		FSkeletalMeshLODRenderData& LODResource,
		const mu::FMesh* InMutableMesh,
		const bool bAllowCPUAccess);

	/** Performs a copy of the data found on the vertex buffers on the mutable mesh to the buffers of the skeletal mesh
	 * @param LODResource - LODRenderData to update
	 * @param InMutableMesh - Mutable mesh to be used as reference for the vertex data update on the Skeletal Mesh
	 * @param bAllowCPUAccess - Keeps this LODs data on the CPU so it can be used for things such as sampling in FX.
	 */
	CUSTOMIZABLEOBJECT_API void CopyMutableVertexBuffers(
		FSkeletalMeshLODRenderData& LODResource,
		const mu::FMesh* InMutableMesh,
		const bool bAllowCPUAccess);

	
	/**
	 * Initializes the LODResource's IndexBuffers with dummy data to prepare it for streaming.
	 * @param LODResource - LODRenderData to be updated.
	 * @param InMutableMesh - The mutable mesh whose index buffers you want to work with
	 */
	CUSTOMIZABLEOBJECT_API void InitIndexBuffersWithDummyData(
		FSkeletalMeshLODRenderData& LODResource,
		const mu::FMesh* InMutableMesh);

	/**
	 *Performs a copy of the data found on the index buffers on the mutable mesh to the buffers of the skeletal mesh
	 * @param LODResource - LODRenderData to be updated.
	 * @param InMutableMesh - The mutable mesh whose index buffers you want to work with
	 * @return True if the operation could be performed successfully, false if not.
	 */
	CUSTOMIZABLEOBJECT_API bool CopyMutableIndexBuffers(
		FSkeletalMeshLODRenderData& LODResource,
		const mu::FMesh* InMutableMesh,
		const TArray<uint32>& SurfaceIDs,
		bool& bOutMarkRenderStateDirty);
	

	/**
	 *Performs a copy of the data found on the index buffers on the mutable mesh to the buffers of the skeletal mesh
	 * @param LODResource - LODRenderData to be updated.
	 * @param Owner - Outer mesh of the LODResource. 
	 * @param LODIndex - LOD Index of the LODResource.
	 * @param InMutableMesh - Mutable mesh to be used to initialize the SkinWeightProfilesBuffers.
	 * @param InActiveProfiles - Profiles to initialize. Values are ProfileID and ProfileName.
	 */
	CUSTOMIZABLEOBJECT_API void CopyMutableSkinWeightProfilesBuffers(
		FSkeletalMeshLODRenderData& LODResource,
		USkeletalMesh& Owner,
		int32 LODIndex,
		const mu::FMesh* InMutableMesh,
		const TArray<TPair<uint32, FName>>& InActiveProfiles
		);


	/**
	 *Performs a copy of the render data of a specific Skeletal Mesh LOD to another Skeletal Mesh
	 * @param LODResource - LODRenderData to copy to.
	 * @param SourceLODResource - LODRenderData to copy from.
	 * @param Owner - Outer mesh of the LODResource and SourceLODResource. 
	 * @param LODIndex - LOD Index to copy to.
	 * @param bAllowCPUAccess - Keeps this LODs data on the CPU so it can be used for things such as sampling in FX.
	 */
	CUSTOMIZABLEOBJECT_API void CopySkeletalMeshLODRenderData(
		FSkeletalMeshLODRenderData& LODResource,
		FSkeletalMeshLODRenderData& SourceLODResource,
		USkeletalMesh& Owner,
		int32 LODIndex,
		const bool bAllowCPUAccess
	);

	/**
	 * Update SkeletalMeshLODRenderData buffers size.
	 * @param LODResource - LODRenderData to be updated.
	 */
	CUSTOMIZABLEOBJECT_API void UpdateSkeletalMeshLODRenderDataBuffersSize(
		FSkeletalMeshLODRenderData& LODResource
	);

	void MorphTargetVertexInfoBuffers(FSkeletalMeshLODRenderData& LODResource, const USkeletalMesh& Owner, const mu::FMesh& MutableMesh, const TMap<uint32, FMorphTargetMeshData>& MorphTargetMeshData, int32 LODIndex);

	CUSTOMIZABLEOBJECT_API UE::Tasks::FTask ConvertSkeletalMeshFromRuntimeData(USkeletalMesh* SkeletalMesh, int32 LODIndex, int32 SectionIndex, UModelResources* ModelResources, mu::FMesh* Result);

	void GetSectionClothData(const mu::FMesh& MutableMesh, int32 LODIndex, const TMap<uint32, FClothingMeshData>& ClothingMeshData, TArray<FSectionClothData>& SectionsClothData, int32& NumClothingDataNotFound);
	
	void CopyMeshToMeshClothData(TArray<FSectionClothData>& SectionsClothData);

	/** Based on FSkeletalMeshLODModel::GetClothMappingData() */
	void CreateClothMapping(const FSectionClothData& SectionClothData, TArray<FMeshToMeshVertData>& MappingData, TArray<FClothBufferIndexMapping>& ClothIndexMapping);
	
	void ClothVertexBuffers(FSkeletalMeshLODRenderData& LODResource, const mu::FMesh& MutableMesh, const TMap<uint32, FClothingMeshData>& ClothingMeshData, int32 LODIndex);
}
