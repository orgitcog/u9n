// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/UnrealConversionUtils.h"

#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/UnrealPortabilityHelpers.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectPrivate.h"
#include "MuCO/CustomizableObjectSystemPrivate.h"
#include "MuCO/MutableMeshBufferUtils.h"
#include "MuR/Skeleton.h"
#include "MuR/Mesh.h"
#include "MuR/MeshBufferSet.h"
#include "MuR/MutableTrace.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "GPUSkinVertexFactory.h"
#include "Rendering/SkeletalMeshRenderData.h"

class USkeleton;

namespace UnrealConversionUtils
{
	// Hidden functions only used internally to aid other functions
	namespace
	{
		/**
		 * Initializes the static mesh vertex buffers object provided with the data found on the mutable buffers
		 * @param OutVertexBuffers - The Unreal's vertex buffers container to be updated with the mutable data.
		 * @param NumVertices - The amount of vertices on the buffer
		 * @param NumTexCoords - The amount of texture coordinates
		 * @param bUseFullPrecisionUVs - Determines if we want to use or not full precision UVs
		 * @param bNeedCPUAccess - Determines if CPU access is required
		 * @param InMutablePositionData - Mutable position data buffer
		 * @param InMutableTangentData - Mutable tangent data buffer
		 * @param InMutableTextureData - Mutable texture data buffer
		 */
		void FStaticMeshVertexBuffers_InitWithMutableData(
			FStaticMeshVertexBuffers& OutVertexBuffers,
			const int32 NumVertices,
			const int32 NumTexCoords,
			const bool bUseFullPrecisionUVs,
			const bool bNeedCPUAccess,
			const void* InMutablePositionData,
			const void* InMutableTangentData,
			const void* InMutableTextureData)
		{
			// positions
			OutVertexBuffers.PositionVertexBuffer.Init(NumVertices, bNeedCPUAccess);
			FMemory::Memcpy(OutVertexBuffers.PositionVertexBuffer.GetVertexData(), InMutablePositionData, NumVertices * OutVertexBuffers.PositionVertexBuffer.GetStride());

			// tangent and texture coords
			OutVertexBuffers.StaticMeshVertexBuffer.SetUseFullPrecisionUVs(bUseFullPrecisionUVs);
			OutVertexBuffers.StaticMeshVertexBuffer.SetUseHighPrecisionTangentBasis(false);
			OutVertexBuffers.StaticMeshVertexBuffer.Init(NumVertices, NumTexCoords, bNeedCPUAccess);
			FMemory::Memcpy(OutVertexBuffers.StaticMeshVertexBuffer.GetTangentData(), InMutableTangentData, OutVertexBuffers.StaticMeshVertexBuffer.GetTangentSize());
			FMemory::Memcpy(OutVertexBuffers.StaticMeshVertexBuffer.GetTexCoordData(), InMutableTextureData, OutVertexBuffers.StaticMeshVertexBuffer.GetTexCoordSize());
		}


		/**
		 * Initializes the color vertex buffers object provided with the data found on the mutable buffers
		 * @param OutVertexBuffers - The Unreal's vertex buffers container to be updated with the mutable data.
		 * @param NumVertices - The amount of vertices on the buffer
		 * @param InMutableColorData - Mutable color data buffer
		 */
		void FColorVertexBuffers_InitWithMutableData(
			FStaticMeshVertexBuffers& OutVertexBuffers,
			const int32 NumVertices,
			const void* InMutableColorData
		)
		{
			// positions
			OutVertexBuffers.ColorVertexBuffer.Init(NumVertices);
			FMemory::Memcpy(OutVertexBuffers.ColorVertexBuffer.GetVertexData(), InMutableColorData, NumVertices * OutVertexBuffers.ColorVertexBuffer.GetStride());
		}


		/**
		 * Initializes the skin vertex buffers object provided with the data found on the mutable buffers
		 * @param OutVertexWeightBuffer - The Unreal's vertex buffers container to be updated with the mutable data.
		   * @param NumVertices - The amount of vertices on the buffer
		 * @param NumBones - The amount of bones to use to init the skin weights buffer
		   * @param NumBoneInfluences - The amount of bone influences on the buffer
		 * @param bNeedCPUAccess - Determines if CPU access is required
		 * @param InMutableData - Mutable data buffer
		 */
		void FSkinWeightVertexBuffer_InitWithMutableData(
			FSkinWeightVertexBuffer& OutVertexWeightBuffer,
			const int32 NumVertices,
			const int32 NumBones,
			const int32 NumBoneInfluences,
			const bool bNeedCPUAccess,
			const void* InMutableData,
			const uint32 MutableDataSize)
		{
			OutVertexWeightBuffer.SetNeedsCPUAccess(bNeedCPUAccess);

			FSkinWeightDataVertexBuffer* VertexBuffer = OutVertexWeightBuffer.GetDataVertexBuffer();
			VertexBuffer->SetMaxBoneInfluences(NumBoneInfluences);

			if (!NumVertices)
			{
				return;
			}

			int32 MaxBoneInfluences = VertexBuffer->GetMaxBoneInfluences();
			if (MaxBoneInfluences == NumBoneInfluences)
			{
				VertexBuffer->Init(NumBones, NumVertices);
				uint32 OutVertexWeightBufferSize = VertexBuffer->GetVertexDataSize();
				ensure(MutableDataSize == OutVertexWeightBufferSize);
				void* Data = VertexBuffer->GetWeightData();
				FMemory::Memcpy(Data, InMutableData, OutVertexWeightBufferSize);
			}
			else if (NumBones>0)
			{
				// We need to expand it with blank data interleaved
				uint32 MutableVertexDataSize = MutableDataSize / NumVertices;
				uint32 FinalVertexDataSize = ( MutableDataSize / NumBones) * MaxBoneInfluences;
				VertexBuffer->Init(NumVertices*MaxBoneInfluences, NumVertices);
				uint32 OutVertexWeightBufferSize = VertexBuffer->GetVertexDataSize();
				ensure(FinalVertexDataSize*NumVertices == OutVertexWeightBufferSize);

				int32 BoneIndexSize = OutVertexWeightBuffer.GetBoneIndexByteSize();
				int32 WeightSize = OutVertexWeightBuffer.GetBoneWeightByteSize();

				const uint8* MutableData = reinterpret_cast<const uint8*>(InMutableData);
				uint8* Data = VertexBuffer->GetWeightData();
				FMemory::Memzero(Data, OutVertexWeightBufferSize);
				for (int32 V=0; V<NumVertices; ++V)
				{
					// Bone indices
					FMemory::Memcpy(Data, MutableData, NumBoneInfluences*BoneIndexSize);
					MutableData += NumBoneInfluences * BoneIndexSize;
					Data += MaxBoneInfluences * BoneIndexSize;

					// Weights
					FMemory::Memcpy(Data, MutableData, NumBoneInfluences * WeightSize);
					MutableData += NumBoneInfluences * WeightSize;
					Data += MaxBoneInfluences * WeightSize;
				}
			}

			bool bIsVariableBonesPerVertex = VertexBuffer->GetVariableBonesPerVertex();
			check(!FGPUBaseSkinVertexFactory::UseUnlimitedBoneInfluences(NumBoneInfluences) || bIsVariableBonesPerVertex);
			if (bIsVariableBonesPerVertex)
			{
				OutVertexWeightBuffer.RebuildLookupVertexBuffer();

				{
					MUTABLE_CPUPROFILER_SCOPE(OptimizeVertexAndLookupBuffers);

					// Everything in this scope is optional and makes extra copies, but will optimize the variable bone
					// influences buffers. Without it, the vertices are assumed to have a constant NumBoneInfluences per vertex.
					TArray<FSkinWeightInfo> TempVertices;
					OutVertexWeightBuffer.GetSkinWeights(TempVertices);

					// The assignment operator actually optimizes the DataVertexBuffer
					OutVertexWeightBuffer = TempVertices;
				}
			}
	
		}
	}


	void SetupRenderSections(
		FSkeletalMeshLODRenderData& LODResource,
		const mu::FMesh* InMutableMesh,
		const TArray<mu::FBoneName>& InBoneMap,
		const TMap<mu::FBoneName, TPair<FName, uint16>>& BoneInfoMap,
		const int32 InFirstBoneMapIndex,
		const TArray<const FMutableSurfaceMetadata*>& SurfacesMetadata)
	{
		check(InMutableMesh);

		const mu::FMeshBufferSet& MutableMeshVertexBuffers = InMutableMesh->GetVertexBuffers();

		// Find the number of influences from this mesh
		int32 NumBoneInfluences = 0;
		int32 boneIndexBuffer = -1;
		int32 boneIndexChannel = -1;
		MutableMeshVertexBuffers.FindChannel(mu::EMeshBufferSemantic::BoneIndices, 0, &boneIndexBuffer, &boneIndexChannel);
		if (boneIndexBuffer >= 0 || boneIndexChannel >= 0)
		{
			MutableMeshVertexBuffers.GetChannel(boneIndexBuffer, boneIndexChannel,
				nullptr, nullptr, nullptr, &NumBoneInfluences, nullptr);
		}

		const int32 SurfaceCount = InMutableMesh->GetSurfaceCount();
		for (int32 SurfaceIndex = 0; SurfaceIndex < SurfaceCount; ++SurfaceIndex)
		{
			MUTABLE_CPUPROFILER_SCOPE(SetupRenderSections);

			int32 FirstIndex;
			int32 IndexCount;
			int32 FirstVertex;
			int32 VertexCount;
			int32 FirstBone;
			int32 BoneCount;
			InMutableMesh->GetSurface(SurfaceIndex, FirstVertex, VertexCount, FirstIndex, IndexCount, FirstBone, BoneCount);
			FSkelMeshRenderSection& Section = LODResource.RenderSections[SurfaceIndex];

			Section.DuplicatedVerticesBuffer.Init(1, TMap<int, TArray<int32>>());

			if (VertexCount == 0 || IndexCount == 0)
			{
				Section.bDisabled = true;
				continue; // Unreal doesn't like empty meshes
			}

			Section.BaseIndex = FirstIndex;
			Section.NumTriangles = IndexCount / 3;
			Section.BaseVertexIndex = FirstVertex;
			Section.MaxBoneInfluences = NumBoneInfluences;
			Section.NumVertices = VertexCount;

			check(SurfacesMetadata.Num() == InMutableMesh->Surfaces.Num());
			if (SurfacesMetadata[SurfaceIndex])
			{
				Section.bCastShadow = SurfacesMetadata[SurfaceIndex]->bCastShadow;
			}

			// InBoneMaps may contain bonemaps from other sections. Copy the bones belonging to this mesh.
			FirstBone += InFirstBoneMapIndex;
			
			Section.BoneMap.Reserve(BoneCount);
			for (int32 BoneMapIndex = 0; BoneMapIndex < BoneCount; ++BoneMapIndex, ++FirstBone)
			{
				if (InBoneMap.IsValidIndex(FirstBone))
				{
					mu::FBoneName FirstBoneName = InBoneMap[FirstBone];
					if (const TPair<FName, uint16>* Found = BoneInfoMap.Find(FirstBoneName))
					{
						Section.BoneMap.Add(Found->Value);
					}
				}
			}
		}
	}


	void InitVertexBuffersWithDummyData(
		FSkeletalMeshLODRenderData& LODResource,
		const mu::FMesh* InMutableMesh,
		const bool bAllowCPUAccess)
	{
		MUTABLE_CPUPROFILER_SCOPE(InitVertexBuffersWithDummyData);

		const mu::FMeshBufferSet& MutableMeshVertexBuffers = InMutableMesh->GetVertexBuffers();
		check(MutableMeshVertexBuffers.GetElementCount() > 0);

		const bool bUseFullPrecisionUVs = true;
		const bool bUseFullPrecisionTangentBasis = false;

		const uint32 NumVertices = MutableMeshVertexBuffers.GetElementCount();
		const uint32 NumTexCoords = MutableMeshVertexBuffers.GetBufferChannelCount(MUTABLE_VERTEXBUFFER_TEXCOORDS);

		const uint32 DummyNumVertices = 1;

		// Static Vertex buffers
		{
			LODResource.StaticVertexBuffers.PositionVertexBuffer.Init(DummyNumVertices, bAllowCPUAccess);
			LODResource.StaticVertexBuffers.PositionVertexBuffer.SetMetaData(LODResource.StaticVertexBuffers.PositionVertexBuffer.GetStride(), NumVertices);

			// tangent and texture coords
			LODResource.StaticVertexBuffers.StaticMeshVertexBuffer.SetUseFullPrecisionUVs(bUseFullPrecisionUVs);
			LODResource.StaticVertexBuffers.StaticMeshVertexBuffer.SetUseHighPrecisionTangentBasis(bUseFullPrecisionTangentBasis);
			LODResource.StaticVertexBuffers.StaticMeshVertexBuffer.Init(DummyNumVertices, NumTexCoords, bAllowCPUAccess);
			LODResource.StaticVertexBuffers.StaticMeshVertexBuffer.SetMetaData(NumTexCoords, NumVertices, bUseFullPrecisionUVs, bUseFullPrecisionTangentBasis);
		}

		mu::EMeshBufferFormat BoneIndexFormat = mu::EMeshBufferFormat::None;
		int32 NumBoneInfluences = 0;
		int32 BoneIndexBuffer = -1;
		int32 BoneIndexChannel = -1;
		MutableMeshVertexBuffers.FindChannel(mu::EMeshBufferSemantic::BoneIndices, 0, &BoneIndexBuffer, &BoneIndexChannel);
		if (BoneIndexBuffer >= 0 || BoneIndexChannel >= 0)
		{
			MutableMeshVertexBuffers.GetChannel(BoneIndexBuffer, BoneIndexChannel,
				nullptr, nullptr, &BoneIndexFormat, &NumBoneInfluences, nullptr);
		}

		mu::EMeshBufferFormat BoneWeightFormat = mu::EMeshBufferFormat::None;
		int32 BoneWeightBuffer = -1;
		int32 BoneWeightChannel = -1;
		MutableMeshVertexBuffers.FindChannel(mu::EMeshBufferSemantic::BoneWeights, 0, &BoneWeightBuffer, &BoneWeightChannel);
		if (BoneWeightBuffer >= 0 || BoneWeightChannel >= 0)
		{
			MutableMeshVertexBuffers.GetChannel(BoneWeightBuffer, BoneWeightChannel,
				nullptr, nullptr, &BoneWeightFormat, nullptr, nullptr);
		}

		// Skin Weights
		{
			const bool bUse16BitBoneIndex = BoneIndexFormat == mu::EMeshBufferFormat::UInt16;
			const bool bUse16BitBoneWeights = BoneWeightFormat == mu::EMeshBufferFormat::NUInt16;

			LODResource.SkinWeightVertexBuffer.SetUse16BitBoneIndex(bUse16BitBoneIndex);
			LODResource.SkinWeightVertexBuffer.SetUse16BitBoneWeight(bUse16BitBoneWeights);
			LODResource.SkinWeightVertexBuffer.SetNeedsCPUAccess(bAllowCPUAccess);

			FSkinWeightDataVertexBuffer* VertexBuffer = LODResource.SkinWeightVertexBuffer.GetDataVertexBuffer();

			// NumBoneInfluences must be equal to MaxBoneInfluences. Set and then GetMaxBoneInfluences to know the number of bone influences to use (at least MAX_INFLUENCES_PER_STREAM).
			VertexBuffer->SetMaxBoneInfluences(NumBoneInfluences);
			NumBoneInfluences = VertexBuffer->GetMaxBoneInfluences();
			
			VertexBuffer->Init(NumBoneInfluences, DummyNumVertices);
			VertexBuffer->SetMetaData(NumVertices, NumBoneInfluences, bUse16BitBoneIndex, bUse16BitBoneWeights);

			if (VertexBuffer->GetVariableBonesPerVertex())
			{
				FSkinWeightLookupVertexBuffer* LookUpVertexBuffer = const_cast<FSkinWeightLookupVertexBuffer*>(LODResource.SkinWeightVertexBuffer.GetLookupVertexBuffer());
				LookUpVertexBuffer->SetMetaData(NumVertices);
			}
		}

		// Optional buffers
		for (int32 Buffer = MUTABLE_VERTEXBUFFER_TEXCOORDS + 1; Buffer < MutableMeshVertexBuffers.GetBufferCount(); ++Buffer)
		{
			if (MutableMeshVertexBuffers.GetBufferChannelCount(Buffer) > 0)
			{
				mu::EMeshBufferSemantic Semantic;
				mu::EMeshBufferFormat Format;
				int32 SemanticIndex;
				int32 ComponentCount;
				int32 Offset;
				MutableMeshVertexBuffers.GetChannel(Buffer, 0, &Semantic, &SemanticIndex, &Format, &ComponentCount, &Offset);

				// colour buffer?
				if (Semantic == mu::EMeshBufferSemantic::Color)
				{
					LODResource.StaticVertexBuffers.ColorVertexBuffer.Init(1, bAllowCPUAccess);
					LODResource.StaticVertexBuffers.ColorVertexBuffer.SetMetaData(LODResource.StaticVertexBuffers.ColorVertexBuffer.GetStride(), NumVertices);
					
					check(LODResource.StaticVertexBuffers.ColorVertexBuffer.GetStride() == MutableMeshVertexBuffers.GetElementSize(Buffer));
				}
			}
		}
	}


	void CopyMutableVertexBuffers(
		FSkeletalMeshLODRenderData& LODResource,
		const mu::FMesh* MutableMesh,
		const bool bAllowCPUAccess)

	{
		MUTABLE_CPUPROFILER_SCOPE(CopyMutableVertexBuffers);

		const mu::FMeshBufferSet& MutableMeshVertexBuffers = MutableMesh->GetVertexBuffers();
		const int32 NumVertices = MutableMeshVertexBuffers.IsDescriptor()
				? 0 
				: MutableMeshVertexBuffers.GetElementCount();

		//const ESkeletalMeshVertexFlags BuildFlags = SkeletalMesh->GetVertexBufferFlags();
		//const bool bUseFullPrecisionUVs = EnumHasAllFlags(BuildFlags, ESkeletalMeshVertexFlags::UseFullPrecisionUVs);
		bool bUseFullPrecisionUVs = true;
		{
			int32 TexCoordsBufferIndex = -1;
			int32 TexCoordsChannelIndex = -1;
			MutableMesh->VertexBuffers.FindChannel(mu::EMeshBufferSemantic::TexCoords, 0, &TexCoordsBufferIndex, &TexCoordsChannelIndex);
			if (TexCoordsBufferIndex >= 0 && TexCoordsChannelIndex >= 0)
			{
				bUseFullPrecisionUVs = MutableMesh->VertexBuffers.Buffers[TexCoordsBufferIndex].Channels[TexCoordsChannelIndex].Format == mu::EMeshBufferFormat::Float32;
			}
		}

		const int NumTexCoords = MutableMeshVertexBuffers.GetBufferChannelCount(MUTABLE_VERTEXBUFFER_TEXCOORDS);

		FStaticMeshVertexBuffers_InitWithMutableData(
			LODResource.StaticVertexBuffers,
			NumVertices,
			NumTexCoords,
			bUseFullPrecisionUVs,
			bAllowCPUAccess,
			MutableMeshVertexBuffers.GetBufferData(MUTABLE_VERTEXBUFFER_POSITION),
			MutableMeshVertexBuffers.GetBufferData(MUTABLE_VERTEXBUFFER_TANGENT),
			MutableMeshVertexBuffers.GetBufferData(MUTABLE_VERTEXBUFFER_TEXCOORDS)
		);

		mu::EMeshBufferFormat BoneIndexFormat = mu::EMeshBufferFormat::None;
		int32 NumBoneInfluences = 0;
		int32 BoneIndexBuffer = -1;
		int32 BoneIndexChannel = -1;
		MutableMeshVertexBuffers.FindChannel(mu::EMeshBufferSemantic::BoneIndices, 0, &BoneIndexBuffer, &BoneIndexChannel);
		if (BoneIndexBuffer >= 0 || BoneIndexChannel >= 0)
		{
			MutableMeshVertexBuffers.GetChannel(BoneIndexBuffer, BoneIndexChannel,
				nullptr, nullptr, &BoneIndexFormat, &NumBoneInfluences, nullptr);
		}

		mu::EMeshBufferFormat BoneWeightFormat = mu::EMeshBufferFormat::None;
		int32 BoneWeightBuffer = -1;
		int32 BoneWeightChannel = -1;
		MutableMeshVertexBuffers.FindChannel(mu::EMeshBufferSemantic::BoneWeights, 0, &BoneWeightBuffer, &BoneWeightChannel);
		if (BoneWeightBuffer >= 0 || BoneWeightChannel >= 0)
		{
			MutableMeshVertexBuffers.GetChannel(BoneWeightBuffer, BoneWeightChannel,
				nullptr, nullptr, &BoneWeightFormat, nullptr, nullptr);
		}

		if (BoneIndexFormat == mu::EMeshBufferFormat::UInt16)
		{
			LODResource.SkinWeightVertexBuffer.SetUse16BitBoneIndex(true);
		}

		if (BoneWeightFormat == mu::EMeshBufferFormat::NUInt16)
		{
			LODResource.SkinWeightVertexBuffer.SetUse16BitBoneWeight(true);
		}

		// Init skin weight buffer
		FSkinWeightVertexBuffer_InitWithMutableData(
			LODResource.SkinWeightVertexBuffer,
			NumVertices,
			NumBoneInfluences * NumVertices,
			NumBoneInfluences,
			bAllowCPUAccess,
			MutableMeshVertexBuffers.GetBufferData(BoneIndexBuffer),
			MutableMeshVertexBuffers.GetBufferDataSize(BoneIndexBuffer)
		);

		// Optional buffers
		for (int32 Buffer = MUTABLE_VERTEXBUFFER_TEXCOORDS + 1; Buffer < MutableMeshVertexBuffers.GetBufferCount(); ++Buffer)
		{
			if (MutableMeshVertexBuffers.GetBufferChannelCount(Buffer) > 0)
			{
				mu::EMeshBufferSemantic Semantic;
				mu::EMeshBufferFormat Format;
				int32 SemanticIndex;
				int32 ComponentCount;
				int32 Offset;
				MutableMeshVertexBuffers.GetChannel(Buffer, 0, &Semantic, &SemanticIndex, &Format, &ComponentCount, &Offset);

				// colour buffer?
				if (Semantic == mu::EMeshBufferSemantic::Color)
				{
					const void* DataPtr = MutableMeshVertexBuffers.GetBufferData(Buffer);
					FColorVertexBuffers_InitWithMutableData(LODResource.StaticVertexBuffers, NumVertices, DataPtr);
					check(LODResource.StaticVertexBuffers.ColorVertexBuffer.GetStride() == MutableMeshVertexBuffers.GetElementSize(Buffer));
				}
			}
		}
	}

		
	void InitIndexBuffersWithDummyData(FSkeletalMeshLODRenderData& LODResource, const mu::FMesh* InMutableMesh)
	{
		MUTABLE_CPUPROFILER_SCOPE(InitIndexBuffersWithDummyData);

		check(InMutableMesh->GetIndexBuffers().GetElementCount() > 0);
		
		const int32 NumIndices = InMutableMesh->GetIndexBuffers().GetElementCount();
		const int32 ElementSize = InMutableMesh->GetIndexBuffers().GetElementSize(0);

		LODResource.MultiSizeIndexContainer.CreateIndexBuffer(ElementSize);
		LODResource.MultiSizeIndexContainer.GetIndexBuffer()->SetMetaData(NumIndices);
	}


	bool CopyMutableIndexBuffers(
		FSkeletalMeshLODRenderData& LODResource,
		const mu::FMesh* InMutableMesh,
		const TArray<uint32>& SurfaceIDs,
		bool& bOutMarkRenderStateDirty)
	{
		MUTABLE_CPUPROFILER_SCOPE(CopyMutableIndexBuffers);

		const int32 IndexCount = InMutableMesh->GetIndexBuffers().GetElementCount();

		if (IndexCount == 0)
		{
			// Copy indices from an empty buffer
			UE_LOG(LogMutable, Error, TEXT("UCustomizableInstancePrivateData::BuildSkeletalMeshRenderData is converting an empty mesh."));
			return false;
		}

		if (InMutableMesh->GetIndexBuffers().IsDescriptor())
		{
			UE_LOG(LogMutable, Error, TEXT("UCustomizableInstancePrivateData::BuildSkeletalMeshRenderData is converting a mesh descriptor."));
			return false;
		}

		const uint8* DataPtr = InMutableMesh->GetIndexBuffers().GetBufferData(0);
		const int32 ElementSize = InMutableMesh->GetIndexBuffers().GetElementSize(0);

		if (!LODResource.MultiSizeIndexContainer.IsIndexBufferValid())
		{
			LODResource.MultiSizeIndexContainer.CreateIndexBuffer(ElementSize);
		}
		
		check(LODResource.MultiSizeIndexContainer.GetDataTypeSize() == ElementSize);
		
		// Don't assume the buffer is empty, otherwise we may add extra elements using Insert(). 
		LODResource.MultiSizeIndexContainer.GetIndexBuffer()->Empty(IndexCount);
		LODResource.MultiSizeIndexContainer.GetIndexBuffer()->Insert(0, IndexCount);

		FMemory::Memcpy(LODResource.MultiSizeIndexContainer.GetIndexBuffer()->GetPointerTo(0), DataPtr, IndexCount * ElementSize);
		
		for (int32 SectionIndex = 0; SectionIndex < LODResource.RenderSections.Num(); ++SectionIndex)
		{
			FSkelMeshRenderSection& Section = LODResource.RenderSections[SectionIndex];
			
			const int32 SurfaceID = SurfaceIDs[SectionIndex];
			const mu::FMeshSurface* Surface = InMutableMesh->Surfaces.FindByPredicate([SurfaceID](const mu::FMeshSurface& Surface)
			{
				return Surface.Id == SurfaceID;
			});
			
			if (Surface)
			{
				const int32 NumVertices = Surface->SubMeshes.Last().VertexEnd - Surface->SubMeshes[0].VertexBegin;

				bOutMarkRenderStateDirty |= Section.NumVertices != NumVertices;
				Section.NumVertices = NumVertices;

				const int32 IndexBegin = Surface->SubMeshes[0].IndexBegin; 
				const int32 IndexEnd = Surface->SubMeshes.Last().IndexEnd; 
				
				const int32 NumTriangles = (IndexEnd - IndexBegin) / 3;

				bOutMarkRenderStateDirty |= Section.NumTriangles != NumTriangles;
				Section.NumTriangles = NumTriangles;

				bOutMarkRenderStateDirty |= Section.BaseIndex != IndexBegin;
				Section.BaseIndex = IndexBegin;

				const int32 VertexBegin = Surface->SubMeshes[0].VertexBegin;

				bOutMarkRenderStateDirty |= Section.BaseVertexIndex != VertexBegin;
				Section.BaseVertexIndex = VertexBegin;
			}
			else
			{
				Section.bDisabled = true;
				Section.NumTriangles = 0;
				Section.NumVertices = 0;
				Section.BaseIndex = 0;
				Section.BaseVertexIndex = 0;

				bOutMarkRenderStateDirty = true;	
			}
		}
		
		return true;
	}


	struct FAuxMutableSkinWeightVertexKey
	{
		FAuxMutableSkinWeightVertexKey(const uint8* InKey, uint8 InKeySize, uint32 InHash)
			: Key(InKey), KeySize(InKeySize), Hash(InHash)
		{
		};

		const uint8* Key;
		uint8 KeySize;
		uint32 Hash;

		friend uint32 GetTypeHash(const FAuxMutableSkinWeightVertexKey& InKey)
		{
			return InKey.Hash;
		}

		bool operator==(const FAuxMutableSkinWeightVertexKey& o) const
		{
			return FMemory::Memcmp(o.Key, Key, KeySize) == 0;
		}
	};

	
	void CopyMutableSkinWeightProfilesBuffers(
		FSkeletalMeshLODRenderData& LODResource,
		USkeletalMesh& SkeletalMesh,
		int32 LODIndex,
		const mu::FMesh* InMutableMesh,
		const TArray<TPair<uint32, FName>>& ActiveSkinWeightProfiles)
	{
		MUTABLE_CPUPROFILER_SCOPE(CopyMutableSkinWeightProfilesBuffers);

		const uint8 NumInfluences = LODResource.GetVertexBufferMaxBoneInfluences();
		const bool b16BitBoneIndices = LODResource.DoesVertexBufferUse16BitBoneIndex();

		const int32 BoneIndexTypeByteSize = LODResource.SkinWeightVertexBuffer.GetBoneIndexByteSize();
		const uint8 BoneIndicesDataSize = NumInfluences * BoneIndexTypeByteSize;

		const int32 BoneWeightTypeByteSize = LODResource.SkinWeightVertexBuffer.GetBoneWeightByteSize();
		const uint8 BoneWeightsDataSize = NumInfluences * BoneWeightTypeByteSize;

		TMap<FAuxMutableSkinWeightVertexKey, int32> HashToUniqueWeightIndexMap;

		const mu::FMeshBufferSet& MutableMeshVertexBuffers = InMutableMesh->GetVertexBuffers();
		for (const TPair<uint32, FName>& Profile : ActiveSkinWeightProfiles)
		{
			int32 BufferIndex, ChannelIndex;
			mu::EMeshBufferFormat Format;
			int32 MutNumInfluences;

			MutableMeshVertexBuffers.FindChannel(mu::EMeshBufferSemantic::AltSkinWeight, Profile.Key, &BufferIndex, &ChannelIndex);
			
			if (BufferIndex == INDEX_NONE)
			{
				continue;
			}

			// Basic Buffer override settings
			FRuntimeSkinWeightProfileData& Override = LODResource.SkinWeightProfilesData.AddOverrideData(Profile.Value);
			Override.NumWeightsPerVertex = NumInfluences;
			Override.b16BitBoneIndices = b16BitBoneIndices;

			// BoneIndices channel info
			MutableMeshVertexBuffers.GetChannel(BufferIndex, 1, nullptr, nullptr, &Format, &MutNumInfluences, nullptr);
			const uint8 MutBoneIndexTypeByteSize = Format == mu::EMeshBufferFormat::UInt16 ? 2 : 1;
			const uint8 MutBoneIndicesDataSize = MutBoneIndexTypeByteSize * MutNumInfluences;
			check(BoneIndexTypeByteSize == MutBoneIndexTypeByteSize);

			// BoneWeights channel info
			MutableMeshVertexBuffers.GetChannel(BufferIndex, 2, nullptr, nullptr, &Format, &MutNumInfluences, nullptr);
			const uint8 MutBoneWeightTypeByteSize = Format == mu::EMeshBufferFormat::NUInt16 ? 2 : 1;
			const uint8 MutBoneWeightsDataSize = MutBoneWeightTypeByteSize * MutNumInfluences;
			check(BoneWeightTypeByteSize == MutBoneWeightTypeByteSize);

			const uint8 MutSkinWeightVertexSize = MutBoneIndicesDataSize + MutBoneWeightsDataSize;

			const int32 ElementCount = MutableMeshVertexBuffers.GetElementCount();
			Override.BoneIDs.Reserve(ElementCount * BoneIndicesDataSize);
			Override.BoneWeights.Reserve(ElementCount * BoneWeightsDataSize);
			Override.VertexIndexToInfluenceOffset.Reserve(ElementCount);

			HashToUniqueWeightIndexMap.Empty(ElementCount);
			int32 UniqueWeightsCount = 0;

			const uint8* SkinWeightsBuffer = reinterpret_cast<const uint8*>(MutableMeshVertexBuffers.GetBufferData(BufferIndex));
			for (int32 ElementIndex = 0; ElementIndex < ElementCount; ++ElementIndex)
			{
				uint32 ElementHash;
				FMemory::Memcpy(&ElementHash, SkinWeightsBuffer, sizeof(int32));
				SkinWeightsBuffer += sizeof(int32);

				if (ElementHash > 0)
				{
					if (int32* OverrideIndex = HashToUniqueWeightIndexMap.Find({ SkinWeightsBuffer, MutSkinWeightVertexSize, ElementHash }))
					{
						Override.VertexIndexToInfluenceOffset.Add(ElementIndex, *OverrideIndex);
						SkinWeightsBuffer += MutSkinWeightVertexSize;
					}
					else
					{
						Override.VertexIndexToInfluenceOffset.Add(ElementIndex, UniqueWeightsCount);
						HashToUniqueWeightIndexMap.Add({ SkinWeightsBuffer, MutSkinWeightVertexSize, ElementHash }, UniqueWeightsCount);

						Override.BoneIDs.SetNumUninitialized((UniqueWeightsCount + 1) * BoneIndicesDataSize, EAllowShrinking::No);
						FMemory::Memcpy(&Override.BoneIDs[UniqueWeightsCount * BoneIndicesDataSize], SkinWeightsBuffer, BoneIndicesDataSize);
						SkinWeightsBuffer += MutBoneIndicesDataSize;

						Override.BoneWeights.SetNumUninitialized((UniqueWeightsCount + 1) * BoneWeightsDataSize, EAllowShrinking::No);
						FMemory::Memcpy(&Override.BoneWeights[UniqueWeightsCount * BoneWeightsDataSize], SkinWeightsBuffer, BoneWeightsDataSize);
						SkinWeightsBuffer += MutBoneWeightsDataSize;
						++UniqueWeightsCount;
					}
				}
				else
				{
					SkinWeightsBuffer += MutSkinWeightVertexSize;
				}
			}

			Override.BoneIDs.Shrink();
			Override.BoneWeights.Shrink();
			Override.VertexIndexToInfluenceOffset.Shrink();
		}

		LODResource.SkinWeightProfilesData.Init(&LODResource.SkinWeightVertexBuffer);
		SkeletalMesh.SetSkinWeightProfilesData(LODIndex, LODResource.SkinWeightProfilesData);
	}

	
	 void CopySkeletalMeshLODRenderData(
		 FSkeletalMeshLODRenderData& LODResource,
		 FSkeletalMeshLODRenderData& SourceLODResource,
		 USkeletalMesh& SkeletalMesh,
		 int32 LODIndex,
		 const bool bAllowCPUAccess
	 )
	 {
		 MUTABLE_CPUPROFILER_SCOPE(CopySkeletalMeshLODRenderData);

		 // Copying render sections
		 {
			 const int32 SurfaceCount = SourceLODResource.RenderSections.Num();
			 for (int32 SurfaceIndex = 0; SurfaceIndex < SurfaceCount; ++SurfaceIndex)
			 {
				 const FSkelMeshRenderSection& SrcSection = SourceLODResource.RenderSections[SurfaceIndex];
				 FSkelMeshRenderSection* DestSection = new(LODResource.RenderSections) FSkelMeshRenderSection();

				 DestSection->DuplicatedVerticesBuffer.Init(1, TMap<int, TArray<int32>>());
				 DestSection->bDisabled = SrcSection.bDisabled;

				 if (!DestSection->bDisabled)
				 {
					 DestSection->BaseIndex = SrcSection.BaseIndex;
					 DestSection->NumTriangles = SrcSection.NumTriangles;
					 DestSection->BaseVertexIndex = SrcSection.BaseVertexIndex;
					 DestSection->MaxBoneInfluences = SrcSection.MaxBoneInfluences;
					 DestSection->NumVertices = SrcSection.NumVertices;
					 DestSection->BoneMap = SrcSection.BoneMap;
					 DestSection->bCastShadow = SrcSection.bCastShadow;
				 }
			 }
		 }

		 const FStaticMeshVertexBuffers& SrcStaticVertexBuffer = SourceLODResource.StaticVertexBuffers;
		 FStaticMeshVertexBuffers& DestStaticVertexBuffer = LODResource.StaticVertexBuffers;

		 const int32 NumVertices = SrcStaticVertexBuffer.PositionVertexBuffer.GetNumVertices();
		 const int32 NumTexCoords = SrcStaticVertexBuffer.StaticMeshVertexBuffer.GetNumTexCoords();

		 // Copying Static Vertex Buffers
		 {
			 // Position buffer
			 DestStaticVertexBuffer.PositionVertexBuffer.Init(NumVertices, bAllowCPUAccess);
			 FMemory::Memcpy(DestStaticVertexBuffer.PositionVertexBuffer.GetVertexData(), SrcStaticVertexBuffer.PositionVertexBuffer.GetVertexData(), NumVertices * DestStaticVertexBuffer.PositionVertexBuffer.GetStride());

			 // Tangent and Texture coords buffers
			 DestStaticVertexBuffer.StaticMeshVertexBuffer.SetUseFullPrecisionUVs(true);
			 DestStaticVertexBuffer.StaticMeshVertexBuffer.SetUseHighPrecisionTangentBasis(false);
			 DestStaticVertexBuffer.StaticMeshVertexBuffer.Init(NumVertices, NumTexCoords, bAllowCPUAccess);
			 FMemory::Memcpy(DestStaticVertexBuffer.StaticMeshVertexBuffer.GetTangentData(), SrcStaticVertexBuffer.StaticMeshVertexBuffer.GetTangentData(), DestStaticVertexBuffer.StaticMeshVertexBuffer.GetTangentSize());
			 FMemory::Memcpy(DestStaticVertexBuffer.StaticMeshVertexBuffer.GetTexCoordData(), SrcStaticVertexBuffer.StaticMeshVertexBuffer.GetTexCoordData(), DestStaticVertexBuffer.StaticMeshVertexBuffer.GetTexCoordSize());

			 // Color buffer
			 if (LODResource.StaticVertexBuffers.ColorVertexBuffer.GetNumVertices() > 0)
			 {
				 DestStaticVertexBuffer.ColorVertexBuffer.Init(NumVertices);
				 FMemory::Memcpy(DestStaticVertexBuffer.ColorVertexBuffer.GetVertexData(), SrcStaticVertexBuffer.ColorVertexBuffer.GetVertexData(), NumVertices * DestStaticVertexBuffer.ColorVertexBuffer.GetStride());
			 }
		 }

		 // Copying Skin Buffers
		 {
			 const FSkinWeightVertexBuffer& SrcSkinWeightBuffer = SourceLODResource.SkinWeightVertexBuffer;
			 FSkinWeightVertexBuffer& DestSkinWeightBuffer = LODResource.SkinWeightVertexBuffer;

			 int32 NumBoneInfluences = SrcSkinWeightBuffer.GetDataVertexBuffer()->GetMaxBoneInfluences();
			 int32 NumBones = SrcSkinWeightBuffer.GetDataVertexBuffer()->GetNumBoneWeights();

			 DestSkinWeightBuffer.SetUse16BitBoneIndex(SrcSkinWeightBuffer.Use16BitBoneIndex());
			 FSkinWeightDataVertexBuffer* SkinWeightDataVertexBuffer = DestSkinWeightBuffer.GetDataVertexBuffer();
			 SkinWeightDataVertexBuffer->SetMaxBoneInfluences(NumBoneInfluences);
			 SkinWeightDataVertexBuffer->Init(NumBones, NumVertices);

			 if (NumVertices)
			 {
				 DestSkinWeightBuffer.SetNeedsCPUAccess(bAllowCPUAccess);

				 const void* SrcData = SrcSkinWeightBuffer.GetDataVertexBuffer()->GetWeightData();
				 void* Data = SkinWeightDataVertexBuffer->GetWeightData();
				 check(SrcData);
				 check(Data);

				 FMemory::Memcpy(Data, SrcData, DestSkinWeightBuffer.GetVertexDataSize());
			 }
		 }

		 // Copying Skin Weight Profiles Buffers
		 {
			 const int32 NumSkinWeightProfiles = SkeletalMesh.GetSkinWeightProfiles().Num();
			 for (int32 ProfileIndex = 0; ProfileIndex < NumSkinWeightProfiles; ++ProfileIndex)
			 {
				 const FName& ProfileName = SkeletalMesh.GetSkinWeightProfiles()[ProfileIndex].Name;
				 
				 const FRuntimeSkinWeightProfileData* SourceProfile = SourceLODResource.SkinWeightProfilesData.GetOverrideData(ProfileName);
				 FRuntimeSkinWeightProfileData& DestProfile = LODResource.SkinWeightProfilesData.AddOverrideData(ProfileName);
				 
				 DestProfile = *SourceProfile;
			 }

			 LODResource.SkinWeightProfilesData.Init(&LODResource.SkinWeightVertexBuffer);
			 SkeletalMesh.SetSkinWeightProfilesData(LODIndex, LODResource.SkinWeightProfilesData);
		 }

		 // Copying Indices
		 {
			 if (SourceLODResource.MultiSizeIndexContainer.IsIndexBufferValid())
			 {
				 int32 IndexCount = SourceLODResource.MultiSizeIndexContainer.GetIndexBuffer()->Num();
				 int32 ElementSize = SourceLODResource.MultiSizeIndexContainer.GetDataTypeSize();

				 const void* Data = SourceLODResource.MultiSizeIndexContainer.GetIndexBuffer()->GetPointerTo(0);

				 LODResource.MultiSizeIndexContainer.CreateIndexBuffer(ElementSize);
				 LODResource.MultiSizeIndexContainer.GetIndexBuffer()->Insert(0, IndexCount);
				 FMemory::Memcpy(LODResource.MultiSizeIndexContainer.GetIndexBuffer()->GetPointerTo(0), Data, IndexCount * ElementSize);
			 }
		 }

		 LODResource.ActiveBoneIndices.Append(SourceLODResource.ActiveBoneIndices);
		 LODResource.RequiredBones.Append(SourceLODResource.RequiredBones);
		 LODResource.bIsLODOptional = SourceLODResource.bIsLODOptional;
		 LODResource.bStreamedDataInlined = SourceLODResource.bStreamedDataInlined;
		 LODResource.BuffersSize = SourceLODResource.BuffersSize;
	}


	void UpdateSkeletalMeshLODRenderDataBuffersSize(FSkeletalMeshLODRenderData& LODResource)
	{
		LODResource.BuffersSize = 0;
		
		// Add VertexBuffers' size
		LODResource.BuffersSize += LODResource.StaticVertexBuffers.PositionVertexBuffer.GetAllocatedSize();
		LODResource.BuffersSize += LODResource.StaticVertexBuffers.StaticMeshVertexBuffer.GetResourceSize();
		LODResource.BuffersSize += LODResource.StaticVertexBuffers.ColorVertexBuffer.GetAllocatedSize();
		LODResource.BuffersSize += LODResource.SkinWeightVertexBuffer.GetVertexDataSize();

		// Add Optional VertexBuffers' size
		LODResource.BuffersSize += LODResource.ClothVertexBuffer.GetVertexDataSize();
		LODResource.BuffersSize += LODResource.SkinWeightProfilesData.GetResourcesSize();
		LODResource.BuffersSize += LODResource.MorphTargetVertexInfoBuffers.GetMorphDataSizeInBytes();

		// Add IndexBuffer's size
		if (LODResource.MultiSizeIndexContainer.IsIndexBufferValid())
		{
			LODResource.BuffersSize += LODResource.MultiSizeIndexContainer.GetIndexBuffer()->GetResourceDataSize();
		}
	}


	void MorphTargetVertexInfoBuffers(FSkeletalMeshLODRenderData& LODResource, const USkeletalMesh& Owner, const mu::FMesh& MutableMesh, const TMap<uint32, FMorphTargetMeshData>& MorphTargetMeshData, int32 LODIndex)
	{
		if (Owner.GetMorphTargets().IsEmpty())
		{
			return;
		}
		
		// Get the global names.
		const TMap<FName, int32>& IndexMap = Owner.GetMorphTargetIndexMap();
	
		int32 MaxIndex = 0;
		for (const TPair<FName, int32>& Pair : IndexMap)
		{
			MaxIndex = FMath::Max(MaxIndex, Pair.Value);
		}

		TArray<FName> GlobalNames;
		GlobalNames.SetNum(MaxIndex + 1);

		for (const TPair<FName, int32>& Pair : IndexMap)
		{
			GlobalNames[Pair.Value] = Pair.Key;
		}

		// Map the local names to the global names.
		TMap<uint32, FMappedMorphTargetMeshData> GlobalMorphTargets; // Key is the block id. Value data needed to reconstruct the Morp Targets.
		for (const TPair<uint32, FMorphTargetMeshData>& Pair : MorphTargetMeshData)
		{
			FMappedMorphTargetMeshData& MappedMorphsData = GlobalMorphTargets.FindOrAdd(Pair.Key);

			// Remapping to global names.
			MappedMorphsData.NameResolutionMap.SetNum(Pair.Value.NameResolutionMap.Num());
			for (int32 NameIndex = 0; NameIndex < Pair.Value.NameResolutionMap.Num(); ++NameIndex)
			{
				MappedMorphsData.NameResolutionMap[NameIndex] = GlobalNames.Find(Pair.Value.NameResolutionMap[NameIndex]);
			}

			// Pointer to the serialized data
			MappedMorphsData.DataView = &Pair.Value.Data;
		}

		// Reconstruct the final Morph Targets using the global names.
		TArray<FMorphTargetLODModel> MorphTargetLODs;
		ReconstructMorphTargets(MutableMesh, GlobalNames, GlobalMorphTargets, MorphTargetLODs); // FMorphTargetLODModel::SectionIndices not initialized here

		TArray<const FMorphTargetLODModel*> MorphsToInit;
		MorphsToInit.Reserve(MorphTargetLODs.Num());
		for (FMorphTargetLODModel& MorphTargetLOD : MorphTargetLODs)
		{
			MorphsToInit.Add(&MorphTargetLOD);
		}

		const TBitArray UsesBuiltinMorphTargetCompression(true, MorphTargetLODs.Num());
		const float ErrorTolerance = Owner.GetLODInfo(LODIndex)->MorphTargetPositionErrorTolerance;
		LODResource.MorphTargetVertexInfoBuffers.InitMorphResourcesStreaming(LODResource.RenderSections, MorphsToInit, LODResource.StaticVertexBuffers.StaticMeshVertexBuffer.GetNumVertices(), ErrorTolerance);
	}


	UE::Tasks::FTask ConvertSkeletalMeshFromRuntimeData(USkeletalMesh* SkeletalMesh, int32 LODIndex, int32 SectionIndex, UModelResources* ModelResources, mu::FMesh* Result)
	{
		MUTABLE_CPUPROFILER_SCOPE(ConvertSkeletalMeshFromRuntimeData);

		if (!SkeletalMesh || !Result)
		{
			ensure(false);
			return UE::Tasks::MakeCompletedTask<void>();
		}

		FSkeletalMeshRenderData* Data = SkeletalMesh->GetResourceForRendering();

		if (!Data->LODRenderData.IsValidIndex(LODIndex))
		{
			// This may happen if we are requesting a LOD that is not present in the provided mesh.
			// TODO: an empty mesh is returned, but should we use the last LOD instead?
			UE_LOG(LogMutable, Warning, TEXT("Trying to convert mesh [%s] LOD %d but it only has %d LODs"), 
				*SkeletalMesh->GetName(), LODIndex, Data->LODRenderData.Num() );

			return UE::Tasks::MakeCompletedTask<void>();
		}

		FSkeletalMeshLODRenderData* LOD = &Data->LODRenderData[LODIndex];

		if (!LOD->RenderSections.IsValidIndex(SectionIndex))
		{
			// This may happen in the provided mesh in the parameter value doesn't have the expected number of sections
			UE_LOG(LogMutable, Warning, TEXT("Trying to convert mesh [%s] section %d in LOD %d but it only has %d sections"),
				*SkeletalMesh->GetName(), SectionIndex, LODIndex, LOD->RenderSections.Num());

			return UE::Tasks::MakeCompletedTask<void>();
		}

		const FSkelMeshRenderSection& Section = LOD->RenderSections[SectionIndex];

		// Skeleton data
		bool bBoneMapModified = false;
		TArray<FBoneIndexType> BoneMap;
		TArray<FBoneIndexType> RemappedBoneMapIndices;

		bool bIgnoreSkeleton = false;
		if (!bIgnoreSkeleton)
		{
			USkeleton* Skeleton = SkeletalMesh->GetSkeleton();
			if (!Skeleton)
			{
				ensure(false);
				return UE::Tasks::MakeCompletedTask<void>();
			}

			// Add the skeleton to the list of referenced skeletons and add its index to the mesh
			if (ModelResources)
			{
				const int32 SkeletonID = ModelResources->Skeletons.AddUnique(Skeleton);
				Result->AddSkeletonID(SkeletonID);
			}

			const TArray<uint16>& SourceRequiredBones = LOD->RequiredBones;

			// Build RequiredBones array
			TArray<FBoneIndexType> RequiredBones;
			RequiredBones.Reserve(SourceRequiredBones.Num());

			for (const FBoneIndexType& RequiredBoneIndex : SourceRequiredBones)
			{
				RequiredBones.AddUnique(RequiredBoneIndex);
			}

			// Rebuild BoneMap
			const TArray<uint16>& SourceBoneMap = Section.BoneMap;
			const int32 NumBonesInBoneMap = SourceBoneMap.Num();

			for (int32 BoneIndex = 0; BoneIndex < NumBonesInBoneMap; ++BoneIndex)
			{
				const FBoneIndexType FinalBoneIndex = SourceBoneMap[BoneIndex];

				const int32 BoneMapIndex = BoneMap.AddUnique(FinalBoneIndex);
				RemappedBoneMapIndices.Add(BoneMapIndex);

				bBoneMapModified = bBoneMapModified || SourceBoneMap[BoneIndex] != FinalBoneIndex;
			}

			// Create the skeleton, poses, and BoneMap for this mesh
			TSharedPtr<mu::FSkeleton> MutableSkeleton = MakeShared<mu::FSkeleton>();
			Result->SetSkeleton(MutableSkeleton);

			const int32 NumRequiredBones = RequiredBones.Num();
			Result->SetBonePoseCount(NumRequiredBones);
			MutableSkeleton->SetBoneCount(NumRequiredBones);

			// MutableBoneMap will not keep an index to the Skeleton, but to the BoneName
			TArray<mu::FBoneName> MutableBoneMap;
			MutableBoneMap.SetNum(BoneMap.Num());

			TArray<FMatrix> ComposedRefPoseMatrices;
			ComposedRefPoseMatrices.SetNum(NumRequiredBones);

			const TArray<FMeshBoneInfo>& RefBoneInfo = SkeletalMesh->GetRefSkeleton().GetRefBoneInfo();
			for (int32 BoneIndex = 0; BoneIndex < NumRequiredBones; ++BoneIndex)
			{
				const int32 RefSkeletonBoneIndex = RequiredBones[BoneIndex];

				const FMeshBoneInfo& BoneInfo = RefBoneInfo[RefSkeletonBoneIndex];
				const int32 ParentBoneIndex = RequiredBones.Find(BoneInfo.ParentIndex);

				// Set bone hierarchy

				// Get the bone id
				mu::FBoneName BoneName;
				if (ModelResources)
				{
					// See if the bone exists in the compiled CO data
					const FString BoneNameString = BoneInfo.Name.ToString().ToLower();
					uint32* BoneId = ModelResources->BoneNamesMap.Find(BoneNameString);

					if (BoneId)
					{
						BoneName = *BoneId;
					}
					else
					{
						// Go the slow way and add it to get a unique ID that the core can work with.
						uint32 NewBoneId = CityHash32(reinterpret_cast<const char*>(*BoneNameString), BoneNameString.Len() * sizeof(FString::ElementType));

						// See if the hash collides with an existing bone
						bool bUnique = false;
						while (!bUnique)
						{
							bUnique = true;
							TMap<FString, uint32>::TConstIterator MapIterator = ModelResources->BoneNamesMap.CreateConstIterator();
							while (MapIterator)
							{
								if (MapIterator.Value() == NewBoneId)
								{
									bUnique = false;
									break;
								}
								++MapIterator;
							}

							if (!bUnique)
							{
								NewBoneId++;
							}
						}

						ModelResources->BoneNamesMap.Add(BoneNameString, NewBoneId);
						BoneName = NewBoneId;
					}
				}

				MutableSkeleton->SetBoneName(BoneIndex, BoneName);
				MutableSkeleton->SetBoneParent(BoneIndex, ParentBoneIndex);

				// Debug. Will not be serialized
				MutableSkeleton->SetDebugName(BoneIndex, BoneInfo.Name);

				// BoneMap: Convert RefSkeletonBoneIndex to BoneId
				const int32 BoneMapIndex = BoneMap.Find(RefSkeletonBoneIndex);
				if (BoneMapIndex != INDEX_NONE)
				{
					MutableBoneMap[BoneMapIndex] = BoneName;
				}

				if (ParentBoneIndex >= 0)
				{
					ComposedRefPoseMatrices[BoneIndex] = SkeletalMesh->GetRefPoseMatrix(RefSkeletonBoneIndex) * ComposedRefPoseMatrices[ParentBoneIndex];
				}
				else
				{
					ComposedRefPoseMatrices[BoneIndex] = SkeletalMesh->GetRefPoseMatrix(RefSkeletonBoneIndex);
				}

				// Set bone pose
				FTransform3f BoneTransform;
				BoneTransform.SetFromMatrix(FMatrix44f(ComposedRefPoseMatrices[BoneIndex]));

				mu::EBoneUsageFlags BoneUsageFlags = mu::EBoneUsageFlags::None;
				EnumAddFlags(BoneUsageFlags, BoneMapIndex != INDEX_NONE ? mu::EBoneUsageFlags::Skinning : mu::EBoneUsageFlags::None);
				EnumAddFlags(BoneUsageFlags, ParentBoneIndex == INDEX_NONE ? mu::EBoneUsageFlags::Root : mu::EBoneUsageFlags::None);

				Result->SetBonePose(BoneIndex, BoneName, BoneTransform, BoneUsageFlags);
			}

			Result->SetBoneMap(MutableBoneMap);
		}


		const bool bUseStreaming = !LOD->bStreamedDataInlined;
		
		UE::Tasks::FTaskEvent LoadedEvent(TEXT("MutableMeshParamLoadCompleted"));

		// This context stores the information that travels between tasks.
		struct FMeshConversionContext
		{
			FSkeletalMeshLODRenderData StreamedLOD;
			FSkeletalMeshLODRenderData* OriginalLOD = nullptr;
			FSkeletalMeshLODRenderData* DataLOD = nullptr;
			TUniquePtr<IBulkDataIORequest> Request;

			FMeshConversionContext()
				: StreamedLOD(false)
			{
			}
		};

		TSharedPtr<FMeshConversionContext> Context = MakeShared<FMeshConversionContext>();
		Context->OriginalLOD = LOD;
		Context->DataLOD = LOD;
		
		if (!bUseStreaming)
		{
			LoadedEvent.Trigger();
		}
		else
		{
			MUTABLE_CPUPROFILER_SCOPE(MeshStreaming);

			FBulkDataIORequestCallBack RequestCallback = [LoadedEvent, SkeletalMesh, Context, LODIndex](bool bWasCancelled, IBulkDataIORequest* IORequest) mutable
				{
					if (!bWasCancelled)
					{
						uint8* BulkData = IORequest->GetReadResults();
						check(BulkData != nullptr);

						{
							MUTABLE_CPUPROFILER_SCOPE(MeshStreamingSerialization);

							FMemoryReaderView Ar(TArrayView<uint8>(BulkData, IORequest->GetSize()), true);

							constexpr uint8 DummyStripFlags = 0;
							const bool bForceKeepCPUResources = true;
							const bool bNeedsCPUAccess = true;
							Context->StreamedLOD.SerializeStreamedData(Ar, const_cast<USkeletalMesh*>(SkeletalMesh), LODIndex, DummyStripFlags, bNeedsCPUAccess, bForceKeepCPUResources);

							Context->DataLOD = &Context->StreamedLOD;
						}
					}
					else
					{
						// Can this happen even if we don't cancel ourselves?
						check(false);
					}

					LoadedEvent.Trigger();
				};

			 Context->Request.Reset( LOD->StreamingBulkData.CreateStreamingRequest( EAsyncIOPriorityAndFlags::AIOP_High, &RequestCallback, nullptr) );
		}

		UE::Tasks::FTask FinalConversionTask = UE::Tasks::Launch( TEXT("MutableRuntimeMeshConversionFinal"),
			[Context, SectionIndex, Result]()
			{
				MUTABLE_CPUPROFILER_SCOPE(MeshConversion);

				const FSkelMeshRenderSection& Section = Context->OriginalLOD->RenderSections[SectionIndex];

				// Mesh data
				int32 FirstVertexIndex = Context->OriginalLOD->RenderSections[SectionIndex].BaseVertexIndex;
				int32 VertexCount = Section.GetNumVertices();

				mu::FMeshBufferSet& Vertices = Result->GetVertexBuffers();
				Vertices.SetElementCount(VertexCount, mu::EMemoryInitPolicy::Zeroed);
				Vertices.SetBufferCount(5);

				// Position buffer
				int32 CurrentVertexBuffer = 0;
				{
					MutableMeshBufferUtils::SetupVertexPositionsBuffer(CurrentVertexBuffer, Vertices);

					int32 ElementSize = Vertices.Buffers[CurrentVertexBuffer].ElementSize;
					const uint8* SourceVertexData = ((const uint8*)Context->DataLOD->StaticVertexBuffers.PositionVertexBuffer.GetVertexData()) + FirstVertexIndex * ElementSize;

					check(ElementSize * (FirstVertexIndex + VertexCount) <= Context->DataLOD->StaticVertexBuffers.PositionVertexBuffer.GetAllocatedSize());
					FMemory::Memcpy(Vertices.GetBufferData(CurrentVertexBuffer), SourceVertexData, ElementSize * VertexCount);

					++CurrentVertexBuffer;
				}

				// Tangent buffer
				{
					bool bIsHighPrecision = Context->DataLOD->StaticVertexBuffers.StaticMeshVertexBuffer.GetUseHighPrecisionTangentBasis();

					const mu::EMeshBufferSemantic Semantics[2] = { mu::EMeshBufferSemantic::Tangent, mu::EMeshBufferSemantic::Normal };
					const int32 SemanticIndices[2] = { 0, 0 };
					const int32 Components[2] = { 4, 4 };
					mu::EMeshBufferFormat Formats[2] = { mu::EMeshBufferFormat::PackedDirS8, mu::EMeshBufferFormat::PackedDirS8_W_TangentSign };
					int32 Offsets[2] = { 0, 4 };
					int32 ElementSize = 8;

					if (bIsHighPrecision)
					{
						// Not really supported
						ensure(false);
						Formats[0] = mu::EMeshBufferFormat::Int16;
						Formats[1] = mu::EMeshBufferFormat::Int16;
						Offsets[1] = 8;
						ElementSize = 16;
					}
					Vertices.SetBuffer(CurrentVertexBuffer, ElementSize, 2, Semantics, SemanticIndices, Formats, Components, Offsets);

					const uint8* SourceVertexData = ((const uint8*)Context->DataLOD->StaticVertexBuffers.StaticMeshVertexBuffer.GetTangentData()) + FirstVertexIndex * ElementSize;

					check((int32)ElementSize * (FirstVertexIndex + VertexCount) <= Context->DataLOD->StaticVertexBuffers.StaticMeshVertexBuffer.GetTangentSize());
					FMemory::Memcpy(Vertices.GetBufferData(CurrentVertexBuffer), SourceVertexData, ElementSize * VertexCount);

					++CurrentVertexBuffer;
				}

				// Texture coords buffer
				{
					int32 NumTexCoords = Context->OriginalLOD->GetNumTexCoords();

					constexpr int32 MaxChannelCount = 4;
					mu::EMeshBufferSemantic Semantics[MaxChannelCount];
					int32 SemanticIndices[MaxChannelCount];
					mu::EMeshBufferFormat Formats[MaxChannelCount];
					int32 Components[MaxChannelCount];
					int32 Offsets[MaxChannelCount];

					int32 UVSize = Context->DataLOD->StaticVertexBuffers.StaticMeshVertexBuffer.GetUseFullPrecisionUVs() ? 8 : 4;
					const int32 ElementSize = UVSize * NumTexCoords;

					for (int32 UV = 0; UV < NumTexCoords; ++UV)
					{
						Semantics[UV] = mu::EMeshBufferSemantic::TexCoords;
						SemanticIndices[UV] = UV;
						Formats[UV] = (UVSize == 8) ? mu::EMeshBufferFormat::Float32 : mu::EMeshBufferFormat::Float16;
						Components[UV] = 2;
						Offsets[UV] = UVSize * UV;
					}
					Vertices.SetBuffer(CurrentVertexBuffer, ElementSize, NumTexCoords, Semantics, SemanticIndices, Formats, Components, Offsets);

					const uint8* SourceVertexData = ((const uint8*)Context->DataLOD->StaticVertexBuffers.StaticMeshVertexBuffer.GetTexCoordData()) + FirstVertexIndex * UVSize * NumTexCoords;
					check(UVSize * NumTexCoords * (FirstVertexIndex + VertexCount) <= Context->DataLOD->StaticVertexBuffers.StaticMeshVertexBuffer.GetTexCoordSize());

					FMemory::Memcpy(Vertices.GetBufferData(CurrentVertexBuffer), SourceVertexData, Vertices.Buffers[CurrentVertexBuffer].ElementSize * VertexCount);

					++CurrentVertexBuffer;
				}

				// Skin buffer
				if (Section.MaxBoneInfluences > 0)
				{
					const FSkinWeightVertexBuffer* SkinBuffer = Context->DataLOD->GetSkinWeightVertexBuffer();
					const int32 MaxBoneIndexTypeSizeBytes = SkinBuffer->GetBoneIndexByteSize();
					const int32 MaxBoneWeightTypeSizeBytes = SkinBuffer->GetBoneWeightByteSize();
					const int32 MaxBonesPerVertex = SkinBuffer->GetMaxBoneInfluences();
					MutableMeshBufferUtils::SetupSkinBuffer(CurrentVertexBuffer, MaxBoneIndexTypeSizeBytes, MaxBoneWeightTypeSizeBytes, MaxBonesPerVertex, Vertices);

					int32 ElementSize = Vertices.Buffers[CurrentVertexBuffer].ElementSize;
					const uint8* SourceVertexData = ((const uint8*)SkinBuffer->GetDataVertexBuffer()->GetWeightData()) + FirstVertexIndex * ElementSize;

					check(ElementSize * (FirstVertexIndex + VertexCount) <= (int32)SkinBuffer->GetVertexDataSize());

					FMemory::Memcpy(Vertices.GetBufferData(CurrentVertexBuffer), SourceVertexData, ElementSize * VertexCount);

					++CurrentVertexBuffer;
				}

				// Colour buffer
				if (Context->DataLOD->StaticVertexBuffers.ColorVertexBuffer.GetNumVertices() > 0)
				{
					const FColorVertexBuffer& ColorBuffer = Context->DataLOD->StaticVertexBuffers.ColorVertexBuffer;

					MutableMeshBufferUtils::SetupVertexColorBuffer(CurrentVertexBuffer, Vertices);

					int32 ElementSize = Vertices.Buffers[CurrentVertexBuffer].ElementSize;
					const uint8* SourceVertexData = ((const uint8*)ColorBuffer.GetVertexData()) + FirstVertexIndex * ElementSize;

					check(ElementSize * (FirstVertexIndex + VertexCount) <= (int32)ColorBuffer.GetAllocatedSize());
					check(ElementSize == ColorBuffer.GetStride());

					FMemory::Memcpy(Vertices.GetBufferData(CurrentVertexBuffer), SourceVertexData, ElementSize * VertexCount);

					++CurrentVertexBuffer;
				}

				// Indices
				{
					int32 FirstIndexIndex = Section.BaseIndex;
					int32 IndexCount = Section.NumTriangles * 3;
					int32 ElementSize = Context->DataLOD->MultiSizeIndexContainer.GetDataTypeSize();

					mu::FMeshBufferSet& Indices = Result->GetIndexBuffers();
					Indices.SetBufferCount(1);
					Indices.SetElementCount(IndexCount);
					constexpr int32 ChannelCount = 1;
					const mu::EMeshBufferSemantic Semantics[ChannelCount] = { mu::EMeshBufferSemantic::VertexIndex };
					const int32 SemanticIndices[ChannelCount] = { 0 };
					mu::EMeshBufferFormat Formats[ChannelCount] = { ElementSize == 4 ? mu::EMeshBufferFormat::UInt32 : mu::EMeshBufferFormat::UInt16 };
					const int32 Components[ChannelCount] = { 1 };
					const int32 Offsets[ChannelCount] = { 0 };
					Indices.SetBuffer(0, ElementSize, ChannelCount, Semantics, SemanticIndices, Formats, Components, Offsets);

					const uint8* SourceData = (const uint8*)Context->DataLOD->MultiSizeIndexContainer.GetIndexBuffer()->GetPointerTo(FirstIndexIndex);
					uint8* TargetData = Indices.GetBufferData(0);

					if (FirstVertexIndex == 0)
					{
						FMemory::Memcpy(TargetData, SourceData, IndexCount * ElementSize);
					}
					else
					{
						// Apply vertex offset
						switch (ElementSize)
						{
						case 2:
							for (int32 Index = 0; Index < IndexCount; ++Index)
							{
								const uint16* Source = ((const uint16*)SourceData) + Index;
								uint16* Target = ((uint16*)TargetData) + Index;
								check(*Source >= FirstVertexIndex);
								*Target = *Source - FirstVertexIndex;
							}
							break;
						case 4:
							for (int32 Index = 0; Index < IndexCount; ++Index)
							{
								const uint32* Source = ((const uint32*)SourceData) + Index;
								uint32* Target = ((uint32*)TargetData) + Index;
								check(*Source >= uint32(FirstVertexIndex));
								*Target = *Source - FirstVertexIndex;
							}
							break;
						default:
							// Index size not implemented
							break;
						}
					}
				}

				Result->EnsureSurfaceData();
			},

			// Dependencies
			LoadedEvent
			);

		return FinalConversionTask;
	}


	void GetSectionClothData(const mu::FMesh& MutableMesh, int32 LODIndex, const TMap<uint32, FClothingMeshData>& ClothingMeshData, TArray<FSectionClothData>& SectionsClothData, int32& NumClothingDataNotFound)
	{
		MUTABLE_CPUPROFILER_SCOPE(GetSectionClothData);
		
		const mu::FMeshBufferSet& MeshSet = MutableMesh.GetVertexBuffers();

		int32 ClothingIndexBuffer, ClothingIndexChannel;
		MeshSet.FindChannel(mu::EMeshBufferSemantic::Other, 2, &ClothingIndexBuffer, &ClothingIndexChannel);

		int32 ClothingResourceBuffer, ClothingResourceChannel;
		MeshSet.FindChannel(mu::EMeshBufferSemantic::Other, 3, &ClothingResourceBuffer, &ClothingResourceChannel);

		if (ClothingIndexBuffer >= 0 && ClothingResourceBuffer >= 0)
		{
			const int32* const ClothingDataBuffer = reinterpret_cast<const int32*>(MeshSet.GetBufferData(ClothingIndexBuffer));
			const uint32* const ClothingDataResource = reinterpret_cast<const uint32*>(MeshSet.GetBufferData(ClothingResourceBuffer));

			const int32 SurfaceCount = MutableMesh.GetSurfaceCount();
			for (int32 SectionIndex = 0; SectionIndex < SurfaceCount; ++SectionIndex)
			{
				int32 FirstVertex, VerticesCount, FirstIndex, IndicesCount, UnusedBoneIndex, UnusedBoneCount;
				MutableMesh.GetSurface(SectionIndex, FirstVertex, VerticesCount, FirstIndex, IndicesCount, UnusedBoneIndex, UnusedBoneCount);

				if (VerticesCount == 0 || IndicesCount == 0)
				{
					continue;
				}
				
				// A Section has cloth data on all its vertices or it does not have it in any.
				// It can be determined if this section has clothing data just looking at the 
				// first vertex of the section.
				TArrayView<const int32> ClothingDataView(ClothingDataBuffer + FirstVertex, VerticesCount);
				TArrayView<const uint32> ClothingResourceView(ClothingDataResource + FirstVertex, VerticesCount);

				const int32 IndexCount = MutableMesh.GetIndexBuffers().GetElementCount();

				TArrayView<const uint16> IndicesView16Bits;
				TArrayView<const uint32> IndicesView32Bits;
				
				if (IndexCount)
				{
					if (MutableMesh.GetIndexBuffers().GetElementSize(0) == 2)
					{
						const uint16* IndexPtr = (const uint16*)MutableMesh.GetIndexBuffers().GetBufferData(0);
						IndicesView16Bits = TArrayView<const uint16>(IndexPtr + FirstIndex, IndicesCount);
					}
					else
					{
						const uint32* IndexPtr = (const uint32*)MutableMesh.GetIndexBuffers().GetBufferData(0);
						IndicesView32Bits = TArrayView<const uint32>(IndexPtr + FirstIndex, IndicesCount);
					}
				}
			
				if (!ClothingDataView.Num())
				{
					continue;
				}

				const uint32 ClothResourceId = ClothingResourceView[0];
				if (ClothResourceId == 0)
				{
					continue;
				}

				const FClothingMeshData* SectionClothingData = ClothingMeshData.Find(ClothResourceId);

				if (!SectionClothingData)
				{
					++NumClothingDataNotFound;
					continue;
				}

				check(SectionClothingData->Data.Num());

				SectionsClothData.Add(FSectionClothData {
					SectionIndex,
					LODIndex,
					FirstVertex,
					IndicesView16Bits,
					IndicesView32Bits, 
					ClothingDataView, 
					MakeArrayView(SectionClothingData->Data.GetData(), SectionClothingData->Data.Num()),
					TArray<FMeshToMeshVertData>() });
			}
		}
	}


	void CopyMeshToMeshClothData(TArray<FSectionClothData>& SectionsClothData)
	{
		MUTABLE_CPUPROFILER_SCOPE(ClothCopyMeshToMeshData)
		
		// Copy MeshToMeshData.
		for (FSectionClothData& SectionWithCloth : SectionsClothData)
		{
			const int32 NumVertices = SectionWithCloth.ClothingDataIndicesView.Num();

			TArray<FMeshToMeshVertData>& ClothMappingData = SectionWithCloth.MappingData;
			ClothMappingData.SetNum(NumVertices);

			// Copy mesh to mesh data indexed by the index stored per vertex at compile time. 
			for (int32 VertexIdx = 0; VertexIdx < NumVertices; ++VertexIdx)
			{
				// Possible Optimization: Gather consecutive indices in ClothingDataView and Memcpy the whole range.
				// FMeshToMeshVertData and FCustomizableObjectMeshToMeshVertData have the same memory footprint and
				// bytes in a FCustomizableObjectMeshToMeshVertData form a valid FMeshToMeshVertData (not the other way around).

				static_assert(sizeof(FCustomizableObjectMeshToMeshVertData) == sizeof(FMeshToMeshVertData));
				static_assert(TIsTrivial<FCustomizableObjectMeshToMeshVertData>::Value);
				static_assert(TIsTrivial<FMeshToMeshVertData>::Value);

				const int32 VertexDataIndex = SectionWithCloth.ClothingDataIndicesView[VertexIdx];
				check(VertexDataIndex >= 0);

				const FCustomizableObjectMeshToMeshVertData& SrcData = SectionWithCloth.ClothingDataView[VertexDataIndex];

				FMeshToMeshVertData& DstData = ClothMappingData[VertexIdx];
				FMemory::Memcpy(&DstData, &SrcData, sizeof(FMeshToMeshVertData));
			}
		}
	}


	void CreateClothMapping(const FSectionClothData& SectionClothData, TArray<FMeshToMeshVertData>& MappingData, TArray<FClothBufferIndexMapping>& ClothIndexMapping)
	{
		ClothIndexMapping[SectionClothData.SectionIndex] = FClothBufferIndexMapping {
			static_cast<uint32>(SectionClothData.BaseVertex),
			static_cast<uint32>(MappingData.Num()),
			static_cast<uint32>(SectionClothData.MappingData.Num()) };
		
		MappingData.Append(SectionClothData.MappingData);
	}


	void ClothVertexBuffers(FSkeletalMeshLODRenderData& LODResource, const mu::FMesh& MutableMesh, const TMap<uint32, FClothingMeshData>& ClothingMeshData, int32 LODIndex)
	{
		TArray<FSectionClothData> SectionsClothData;
		SectionsClothData.Reserve(32);

		int32 NumClothingDataNotFound = 0;

		GetSectionClothData(MutableMesh, LODIndex, ClothingMeshData, SectionsClothData, NumClothingDataNotFound);
	
		if (NumClothingDataNotFound > 0)
		{
			UE_LOG(LogMutable, Error, TEXT("Some clothing data could not be loaded properly, clothing assets may not behave as expected."));
		}

		if (!SectionsClothData.Num())
		{
			return; // Nothing to do.
		}

		CopyMeshToMeshClothData(SectionsClothData);
		
		TArray<FMeshToMeshVertData> MappingData;
		MappingData.Reserve(Algo::Accumulate(SectionsClothData, 0, [](int32 Sum, const FSectionClothData& Element){	return Sum + Element.MappingData.Num(); })); // Optimization.
		
		TArray<FClothBufferIndexMapping> ClothIndexMapping;
		ClothIndexMapping.SetNumZeroed(LODResource.RenderSections.Num());

		for (const FSectionClothData& Data : SectionsClothData)
		{
			CreateClothMapping(Data, MappingData, ClothIndexMapping);
		}
		
		LODResource.ClothVertexBuffer.Init(MappingData, ClothIndexMapping);
	}
}
