// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/LowLevelMemTracker.h"
#include "GrowOnlySpanAllocator.h"
#include "IO/IoHash.h"
#include "UnifiedBuffer.h"
#include "RenderGraphDefinitions.h"
#include "SceneManagement.h"
#include "Materials/MaterialInterface.h"
#include "Serialization/BulkData.h"
#include "Misc/MemoryReadStream.h"
#include "NaniteDefinitions.h"
#include "NaniteInterface.h"
#include "Templates/DontCopy.h"
#include "VertexFactory.h"
#include "Matrix3x4.h"
#include "SkeletalMeshTypes.h"

/** Whether Nanite::FSceneProxy should store data and enable codepaths needed for debug rendering. */
#if PLATFORM_WINDOWS
#define NANITE_ENABLE_DEBUG_RENDERING (!(UE_BUILD_SHIPPING || UE_BUILD_TEST) || WITH_EDITOR)
#else
#define NANITE_ENABLE_DEBUG_RENDERING 0
#endif

DECLARE_STATS_GROUP( TEXT("Nanite"), STATGROUP_Nanite, STATCAT_Advanced );

DECLARE_GPU_STAT_NAMED_EXTERN(NaniteStreaming, TEXT("Nanite Streaming"));
DECLARE_GPU_STAT_NAMED_EXTERN(NaniteReadback, TEXT("Nanite Readback"));

LLM_DECLARE_TAG_API(Nanite, ENGINE_API);

struct FMeshNaniteSettings;
class FNaniteVertexFactory;

class FStaticMeshSectionArray;
class FSkelMeshSectionArray;

struct FStaticMeshSection;
struct FSkelMeshSection;

namespace UE::DerivedData { class FRequestOwner; }

namespace Nanite
{

struct FPackedHierarchyNode
{
	FVector4f		LODBounds[NANITE_MAX_BVH_NODE_FANOUT];
	
	struct
	{
		FVector3f	BoxBoundsCenter;
		uint32		MinLODError_MaxParentLODError;
	} Misc0[NANITE_MAX_BVH_NODE_FANOUT];

	struct
	{
		FVector3f	BoxBoundsExtent;
		uint32		ChildStartReference;
	} Misc1[NANITE_MAX_BVH_NODE_FANOUT];
	
	struct
	{
		uint32		ResourcePageIndex_NumPages_GroupPartSize;
	#if NANITE_ASSEMBLY_DATA
		uint32 		AssemblyPartIndex;
	#endif
	} Misc2[NANITE_MAX_BVH_NODE_FANOUT];
};

// These are expected to match up
static_assert(NANITE_HIERARCHY_NODE_SLICE_SIZE_DWORDS == sizeof(FPackedHierarchyNode) / 4);

FORCEINLINE uint32 GetBits(uint32 Value, uint32 NumBits, uint32 Offset)
{
	uint32 Mask = (1u << NumBits) - 1u;
	return (Value >> Offset) & Mask;
}

FORCEINLINE void SetBits(uint32& Value, uint32 Bits, uint32 NumBits, uint32 Offset)
{
	uint32 Mask = (1u << NumBits) - 1u;
	check(Bits <= Mask);
	Mask <<= Offset;
	Value = (Value & ~Mask) | (Bits << Offset);
}


// Packed Cluster as it is used by the GPU
struct FPackedCluster
{
	// TODO: Repack. Assuming we don't want to support larger page sizes than 128KB, we can encode offsets as 15bit dword offsets.

	// Members needed for rasterization
	uint32		NumVerts_PositionOffset;					// NumVerts:14, PositionOffset:18
	uint32		NumTris_IndexOffset;						// NumTris:8, IndexOffset: 24
	uint32		ColorMin;
	uint32		ColorBits_GroupIndex;						// R:4, G:4, B:4, A:4. (GroupIndex&0xFFFF) is for debug visualization only.

	FIntVector	PosStart;
	uint32		BitsPerIndex_PosPrecision_PosBits_NormalPrecision_TangentPrecision;	// BitsPerIndex:4, PosPrecision: 5, PosBits:5.5.5, NormalPrecision: 4, TangentPrecision: 4
	
	// Members needed for culling
	FSphere3f	LODBounds;

	FVector3f	BoxBoundsCenter;
	uint32		LODErrorAndEdgeLength;
	
	FVector3f	BoxBoundsExtent;
	uint32		Flags_NumClusterBoneInfluences;							// ClusterFlags:4, NumClusterBoneInfluences: 5

	// Members needed by materials
	uint32		AttributeOffset_BitsPerAttribute;						// AttributeOffset: 22, BitsPerAttribute: 10
	uint32		DecodeInfoOffset_HasTangents_Skinning_NumUVs_ColorMode;	// DecodeInfoOffset: 22, bTangents: 1, bSkinning: 1, NumUVs: 3, ColorMode: 2
	uint32		UVBitOffsets;											// Bit offsets of UV sets relative to beginning of UV data.
																		// UV0 Offset: 8, UV1 Offset: 8, UV2 Offset: 8, UV3 Offset: 8
	uint32		PackedMaterialInfo;

	uint32		ExtendedDataOffset_Num;									// ExtendedDataOffset: 22, Num: 10
	uint32		BrickDataOffset_Num;									// BrickDataOffset: 22, Num: 10, VOXELTODO: Reuse PositionOffset for BrickDataOffset?
	uint32		Dummy0;
	uint32		Dummy1;

	uint32		VertReuseBatchInfo[4];

	uint32		GetNumVerts() const						{ return GetBits(NumVerts_PositionOffset, 14, 0); }
	uint32		GetPositionOffset() const				{ return GetBits(NumVerts_PositionOffset, 18, 14); }

	uint32		GetNumTris() const						{ return GetBits(NumTris_IndexOffset, 8, 0); }
	uint32		GetIndexOffset() const					{ return GetBits(NumTris_IndexOffset, 24, 8); }

	uint32		GetBitsPerIndex() const					{ return GetBits(BitsPerIndex_PosPrecision_PosBits_NormalPrecision_TangentPrecision, 3, 0) + 1; }
	int32		GetPosPrecision() const					{ return (int32)GetBits(BitsPerIndex_PosPrecision_PosBits_NormalPrecision_TangentPrecision, 6, 3) + NANITE_MIN_POSITION_PRECISION; }
	uint32		GetPosBitsX() const						{ return GetBits(BitsPerIndex_PosPrecision_PosBits_NormalPrecision_TangentPrecision, 5, 9); }
	uint32		GetPosBitsY() const						{ return GetBits(BitsPerIndex_PosPrecision_PosBits_NormalPrecision_TangentPrecision, 5, 14); }
	uint32		GetPosBitsZ() const						{ return GetBits(BitsPerIndex_PosPrecision_PosBits_NormalPrecision_TangentPrecision, 5, 19); }
	uint32		GetNormalPrecision() const				{ return GetBits(BitsPerIndex_PosPrecision_PosBits_NormalPrecision_TangentPrecision, 4, 24); }
	uint32		GetTangentPrecision() const				{ return GetBits(BitsPerIndex_PosPrecision_PosBits_NormalPrecision_TangentPrecision, 4, 28); }

	uint32		GetFlags() const						{ return GetBits(Flags_NumClusterBoneInfluences, 4, 0); }
	uint32		GetNumClusterBoneInfluences() const		{ return GetBits(Flags_NumClusterBoneInfluences, 5, 4); }


	uint32		GetAttributeOffset() const				{ return GetBits(AttributeOffset_BitsPerAttribute, 22, 0); }
	uint32		GetBitsPerAttribute() const				{ return GetBits(AttributeOffset_BitsPerAttribute, 10, 22); }
	
	void		SetNumVerts(uint32 NumVerts)			{ SetBits(NumVerts_PositionOffset, NumVerts, 14, 0); }
	void		SetPositionOffset(uint32 Offset)		{ SetBits(NumVerts_PositionOffset, Offset, 18, 14); }

	void		SetNumTris(uint32 NumTris)				{ SetBits(NumTris_IndexOffset, NumTris, 8, 0); }
	void		SetIndexOffset(uint32 Offset)			{ SetBits(NumTris_IndexOffset, Offset, 24, 8); }

	void		SetBitsPerIndex(uint32 BitsPerIndex)	{ SetBits(BitsPerIndex_PosPrecision_PosBits_NormalPrecision_TangentPrecision, BitsPerIndex - 1, 3, 0); }
	void		SetPosPrecision(int32 Precision)		{ SetBits(BitsPerIndex_PosPrecision_PosBits_NormalPrecision_TangentPrecision, uint32(Precision - NANITE_MIN_POSITION_PRECISION), 6, 3); }
	void		SetPosBitsX(uint32 NumBits)				{ SetBits(BitsPerIndex_PosPrecision_PosBits_NormalPrecision_TangentPrecision, NumBits, 5, 9); }
	void		SetPosBitsY(uint32 NumBits)				{ SetBits(BitsPerIndex_PosPrecision_PosBits_NormalPrecision_TangentPrecision, NumBits, 5, 14); }
	void		SetPosBitsZ(uint32 NumBits)				{ SetBits(BitsPerIndex_PosPrecision_PosBits_NormalPrecision_TangentPrecision, NumBits, 5, 19); }
	void		SetNormalPrecision(uint32 NumBits)		{ SetBits(BitsPerIndex_PosPrecision_PosBits_NormalPrecision_TangentPrecision, NumBits, 4, 24); }
	void		SetTangentPrecision(uint32 NumBits)		{ SetBits(BitsPerIndex_PosPrecision_PosBits_NormalPrecision_TangentPrecision, NumBits, 4, 28); }

	void		SetFlags(uint32 Flags)					{ SetBits(Flags_NumClusterBoneInfluences, Flags, 4, 0); }
	void		SetNumClusterBoneInfluences(uint32 N)	{ SetBits(Flags_NumClusterBoneInfluences, N,	 5, 4); }

	void		SetAttributeOffset(uint32 Offset)		{ SetBits(AttributeOffset_BitsPerAttribute, Offset, 22, 0); }
	void		SetBitsPerAttribute(uint32 Bits)		{ SetBits(AttributeOffset_BitsPerAttribute, Bits, 10, 22); }

	void		SetDecodeInfoOffset(uint32 Offset)		{ SetBits(DecodeInfoOffset_HasTangents_Skinning_NumUVs_ColorMode, Offset, 22, 0); }
	void		SetHasTangents(bool bHasTangents)		{ SetBits(DecodeInfoOffset_HasTangents_Skinning_NumUVs_ColorMode, bHasTangents, 1, 22); }
	void		SetHasSkinning(bool bSkinning)			{ SetBits(DecodeInfoOffset_HasTangents_Skinning_NumUVs_ColorMode, bSkinning, 1, 23); }
	void		SetNumUVs(uint32 Num)					{ SetBits(DecodeInfoOffset_HasTangents_Skinning_NumUVs_ColorMode, Num, 3, 24); }
	void		SetColorMode(uint32 Mode)				{ SetBits(DecodeInfoOffset_HasTangents_Skinning_NumUVs_ColorMode, Mode, 1, 27); }

	void		SetColorBitsR(uint32 NumBits)			{ SetBits(ColorBits_GroupIndex, NumBits, 4, 0); }
	void		SetColorBitsG(uint32 NumBits)			{ SetBits(ColorBits_GroupIndex, NumBits, 4, 4); }
	void		SetColorBitsB(uint32 NumBits)			{ SetBits(ColorBits_GroupIndex, NumBits, 4, 8); }
	void		SetColorBitsA(uint32 NumBits)			{ SetBits(ColorBits_GroupIndex, NumBits, 4, 12); }

	void		SetGroupIndex(uint32 GroupIndex)		{ SetBits(ColorBits_GroupIndex, GroupIndex & 0xFFFFu, 16, 16); }

	void		SetExtendedDataOffset(uint32 Offset)	{ SetBits(ExtendedDataOffset_Num, Offset, 22, 0); }
	void		SetExtendedDataNum(uint32 Num)			{ SetBits(ExtendedDataOffset_Num, Num,	  10, 22); }

	void		SetBrickDataOffset(uint32 Offset)		{ SetBits(BrickDataOffset_Num, Offset, 22, 0); }
	void		SetBrickDataNum(uint32 Num)				{ SetBits(BrickDataOffset_Num, Num,	   10, 22); }

	void SetVertResourceBatchInfo(TArray<uint32>& BatchInfo, uint32 GpuPageOffset, uint32 NumMaterialRanges)
	{
		FMemory::Memzero(VertReuseBatchInfo, sizeof(VertReuseBatchInfo));
		if (NumMaterialRanges <= 3)
		{
			check(BatchInfo.Num() <= 4);
			FMemory::Memcpy(VertReuseBatchInfo, BatchInfo.GetData(), BatchInfo.Num() * sizeof(uint32));
		}
		else
		{
			check((GpuPageOffset & 0x3) == 0);
			VertReuseBatchInfo[0] = GpuPageOffset >> 2;
			VertReuseBatchInfo[1] = NumMaterialRanges;
		}
	}
};

struct FPageStreamingState
{
	uint32			BulkOffset;
	uint32			BulkSize;
	uint32			PageSize;
	uint32			DependenciesStart;
	uint16			DependenciesNum;
	uint8			MaxHierarchyDepth;
	uint8			Flags;
};

class FHierarchyFixup
{
public:
	FHierarchyFixup() {}

	FHierarchyFixup( uint32 InPageIndex, uint32 NodeIndex, uint32 ChildIndex, uint32 InClusterGroupPartStartIndex, uint32 PageDependencyStart, uint32 PageDependencyNum )
	{
		check(InPageIndex < NANITE_MAX_RESOURCE_PAGES);
		PageIndex = InPageIndex;

		check( NodeIndex < ( 1 << ( 32 - NANITE_MAX_HIERACHY_CHILDREN_BITS ) ) );
		check( ChildIndex < NANITE_MAX_HIERACHY_CHILDREN );
		check( InClusterGroupPartStartIndex < ( 1 << ( 32 - NANITE_MAX_CLUSTERS_PER_GROUP_BITS ) ) );
		HierarchyNodeAndChildIndex = ( NodeIndex << NANITE_MAX_HIERACHY_CHILDREN_BITS ) | ChildIndex;
		ClusterGroupPartStartIndex = InClusterGroupPartStartIndex;

		check(PageDependencyStart < NANITE_MAX_RESOURCE_PAGES);
		check(PageDependencyNum <= NANITE_MAX_GROUP_PARTS_MASK);
		PageDependencyStartAndNum = (PageDependencyStart << NANITE_MAX_GROUP_PARTS_BITS) | PageDependencyNum;
	}

	uint32 GetPageIndex() const						{ return PageIndex; }
	uint32 GetNodeIndex() const						{ return HierarchyNodeAndChildIndex >> NANITE_MAX_HIERACHY_CHILDREN_BITS; }
	uint32 GetChildIndex() const					{ return HierarchyNodeAndChildIndex & (NANITE_MAX_HIERACHY_CHILDREN - 1); }
	uint32 GetClusterGroupPartStartIndex() const	{ return ClusterGroupPartStartIndex; }
	uint32 GetPageDependencyStart() const			{ return PageDependencyStartAndNum >> NANITE_MAX_GROUP_PARTS_BITS; }
	uint32 GetPageDependencyNum() const				{ return PageDependencyStartAndNum & NANITE_MAX_GROUP_PARTS_MASK; }

	uint32 PageIndex;
	uint32 HierarchyNodeAndChildIndex;
	uint32 ClusterGroupPartStartIndex;
	uint32 PageDependencyStartAndNum;
};

class FClusterFixup
{
public:
	FClusterFixup() {}

	FClusterFixup( uint32 PageIndex, uint32 ClusterIndex, uint32 PageDependencyStart, uint32 PageDependencyNum )
	{
		check( PageIndex < ( 1 << ( 32 - NANITE_MAX_CLUSTERS_PER_GROUP_BITS ) ) );
		check(ClusterIndex < NANITE_MAX_CLUSTERS_PER_PAGE);
		PageAndClusterIndex = ( PageIndex << NANITE_MAX_CLUSTERS_PER_PAGE_BITS ) | ClusterIndex;

		check(PageDependencyStart < NANITE_MAX_RESOURCE_PAGES);
		check(PageDependencyNum <= NANITE_MAX_GROUP_PARTS_MASK);
		PageDependencyStartAndNum = (PageDependencyStart << NANITE_MAX_GROUP_PARTS_BITS) | PageDependencyNum;
	}
	
	uint32 GetPageIndex() const				{ return PageAndClusterIndex >> NANITE_MAX_CLUSTERS_PER_PAGE_BITS; }
	uint32 GetClusterIndex() const			{ return PageAndClusterIndex & (NANITE_MAX_CLUSTERS_PER_PAGE - 1u); }
	uint32 GetPageDependencyStart() const	{ return PageDependencyStartAndNum >> NANITE_MAX_GROUP_PARTS_BITS; }
	uint32 GetPageDependencyNum() const		{ return PageDependencyStartAndNum & NANITE_MAX_GROUP_PARTS_MASK; }

	uint32 PageAndClusterIndex;
	uint32 PageDependencyStartAndNum;
};

class FFixupChunk	//TODO: rename to something else
{
public:
	struct FHeader
	{
		uint16 Magic = 0;
		uint16 NumClusters = 0;
		uint16 NumHierarchyFixups = 0;
		uint16 NumClusterFixups = 0;
	} Header;
	
	static constexpr uint32 GetSize(uint32 NumHierarchyFixups, uint32 NumClusterFixups) { return sizeof( FHeader ) + NumHierarchyFixups * sizeof( FHierarchyFixup ) + NumClusterFixups * sizeof( FClusterFixup ); }

	FClusterFixup&		GetClusterFixup( uint32 Index ) const { check( Index < Header.NumClusterFixups );  return ( (FClusterFixup*)( GetData() + Header.NumHierarchyFixups * sizeof( FHierarchyFixup ) ) )[ Index ]; }
	FHierarchyFixup&	GetHierarchyFixup( uint32 Index ) const { check( Index < Header.NumHierarchyFixups ); return ((FHierarchyFixup*)GetData())[ Index ]; }
	uint32				GetSize() const { return GetSize(Header.NumHierarchyFixups, Header.NumClusterFixups); }

private:
	uint8*	GetData() const { return (uint8*)(&Header + 1); }
};

class FFixupChunkBuffer	//TODO: rename when you rename FFixupChunk
{
public:
	void Reserve(uint32 NumPages)
	{
		constexpr uint32 PreallocSizePerChunk = FFixupChunk::GetSize(NANITE_MAX_CLUSTERS_PER_PAGE, NANITE_MAX_CLUSTERS_PER_PAGE);
		constexpr uint32 PreallocDwordsPerChunk = ((PreallocSizePerChunk - 1u) / 4u + 1u);
		DwordOffsets.Reserve(NumPages);
		ChunkData.Reserve(NumPages * PreallocDwordsPerChunk);
	}

	uint32 Add(uint32 NumClusters, uint32 NumHierarchyFixups, uint32 NumClusterFixups)
	{
		check(NumClusters <= MAX_uint16);
		check(NumHierarchyFixups <= MAX_uint16);
		check(NumClusterFixups <= MAX_uint16);
		uint32 PageIndex = DwordOffsets.Add(ChunkData.Num());
		ChunkData.AddUninitialized(((FFixupChunk::GetSize(NumHierarchyFixups, NumClusterFixups) - 1u) / 4u) + 1u);
		FFixupChunk& Chunk = (*this)[PageIndex];
		Chunk.Header.Magic 				= NANITE_FIXUP_MAGIC;
		Chunk.Header.NumClusters		= uint16(NumClusters);
		Chunk.Header.NumHierarchyFixups	= uint16(NumHierarchyFixups);
		Chunk.Header.NumClusterFixups	= uint16(NumClusterFixups);
		return PageIndex;
	}
	
	FFixupChunk&		Add_GetRef(uint32 NumClusters, uint32 NumHierarchyFixups, uint32 NumClusterFixups) { return (*this)[Add(NumClusters, NumHierarchyFixups, NumClusterFixups)]; }
	void				Reset()	{ DwordOffsets.Reset(); ChunkData.Reset(); }
	int32 				Num() const { return DwordOffsets.Num(); }
	FFixupChunk&		operator[](int32 Index) { return *(FFixupChunk*)(&ChunkData[DwordOffsets[Index]]); }
	const FFixupChunk&	operator[](int32 Index) const { return (*const_cast<FFixupChunkBuffer*>(this))[Index]; }

private:
	TArray<uint32> DwordOffsets;
	TArray<uint32> ChunkData;
};

struct FInstanceDraw
{
	uint32 InstanceId;
	uint32 ViewId;
};

enum class EMeshDataSectionFlags : uint32
{
	None = 0x0,

	// If set, collision is enabled for this section.
	EnableCollision = 1 << 1,

	// If set, this section will cast a shadow.
	CastShadow = 1 << 2, // Shared

	// If set, this section will be visible in ray tracing effects.
	VisibleInRayTracing = 1 << 3, // Shared

	// If set, this section will affect lighting methods that use distance fields.
	AffectDistanceFieldLighting = 1 << 4,

	// If set, this section will be considered opaque in ray tracing effects.
	ForceOpaque = 1 << 5,

	// If set, this section is selected.
	Selected = 1 << 6, // Skel

	// If set, this selection will recompute tangents at runtime.
	RecomputeTangents = 1 << 7, // Skel

	// If set, this section will store bone indices as 16 bit (as opposed to 8 bit).
	Use16BitBoneIndices = 1 << 8, // Skel

	// If set, this section will not be rendered.
	Disabled = 1 << 9, // Skel
};
ENUM_CLASS_FLAGS(EMeshDataSectionFlags)

// Note: Must match MAX_STATIC_TEXCOORDS
#define MAX_MESH_DATA_TEXCOORDS 8

#if WITH_EDITOR

struct FMeshSkinningData
{
	// Max # of bones used to skin the vertices in this section 
	uint32 MaxBoneInfluences;

	/** Vertex color channel to mask recompute tangents. R=0,G=1,B=2,A=None=3 */
	ESkinVertexColorChannel RecomputeTangentsVertexMaskChannel;

	// The soft vertices of this section.
	TArray<FSoftSkinVertex> SoftVertices;

	/** Map between a vertex index and all vertices that share the same position **/
	TMap<int32, TArray<int32>> OverlappingVertices;

	// The bones which are used by the vertices of this section. Indices of bones in the USkeletalMesh::RefSkeleton array
	TArray<uint16> BoneMap;

	/**
	 * The cloth deformer mapping data to each required cloth LOD.
	 * Raytracing may require a different deformer LOD to the one being simulated/rendered.
	 * The outer array indexes the LOD bias to this LOD. The inner array indexes the vertex mapping data.
	 * For example, if this LODModel is LOD3, then ClothMappingDataLODs[1] will point to defomer data that are using cloth LOD2.
	 * Then ClothMappingDataLODs[2] will point to defomer data that are using cloth LOD1, ...etc.
	 * ClothMappingDataLODs[0] will always point to defomer data that are using the same cloth LOD as this section LOD,
	 * this is convenient for cases where the cloth LOD bias is not known/required.
	 */
	TArray<TArray<FMeshToMeshVertData>> ClothMappingDataLODs;

	/** Clothing data for this section, clothing is only present if ClothingData.IsValid() returns true */
	FClothingSectionData ClothingData;

	// INDEX_NONE if not set
	int16 CorrespondClothAssetIndex;

	/*
	 * The LOD index at which any generated lower quality LODs will include this section.
	 * A value of -1 mean the section will always be include when generating a LOD
	 */
	int32 GenerateUpToLodIndex;

	/*
	 * This represent the original section index in the imported data. The original data is chunk per material,
	 * we use this index to store user section modification. The user cannot change a BONE chunked section data,
	 * since the BONE chunk can be per-platform. Do not use this value to index the Sections array, only the user
	 * section data should be index by this value.
	 */
	int32 OriginalDataSectionIndex;

	/*
	 * If this section was produce because of BONE chunking, the parent section index will be valid.
	 * If the section is not the result of skin vertex chunking, this value will be INDEX_NONE.
	 * Use this value to know if the section was BONE chunked:
	 * if(ChunkedParentSectionIndex != INDEX_NONE) will be true if the section is BONE chunked
	 */
	int32 ChunkedParentSectionIndex;
};

#endif // WITH_EDITOR

struct FMeshDataSection
{
	/** The index of the material with which to render this section. */
	int32 MaterialIndex; // Shared

	/** Range of vertices and indices used when rendering this section. */
	uint32 FirstIndex; // BaseIndex
	uint32 NumTriangles; // Shared
	uint32 MinVertexIndex; // BaseVertexIndex
	uint32 MaxVertexIndex;

	EMeshDataSectionFlags Flags = EMeshDataSectionFlags::None;

#if WITH_EDITOR
	FMeshSkinningData Skinning;
#endif

#if WITH_EDITORONLY_DATA
	// The UV channel density in LocalSpaceUnit / UV Unit.
	float UVDensities[MAX_MESH_DATA_TEXCOORDS];

	// The weights to apply to the UV density, based on the area.
	float Weights[MAX_MESH_DATA_TEXCOORDS];
#endif
};

class FMeshDataSectionArray : public TArray<FMeshDataSection, TInlineAllocator<1>>
{
	using Super = TArray<FMeshDataSection, TInlineAllocator<1>>;
public:
	using Super::Super;
};

ENGINE_API FMeshDataSectionArray BuildMeshSections(const FStaticMeshSectionArray& InSections);
ENGINE_API FStaticMeshSectionArray BuildStaticMeshSections(const FMeshDataSectionArray& InSections);

#if WITH_EDITOR

ENGINE_API FMeshDataSectionArray BuildMeshSections(const TConstArrayView<const FSkelMeshSection> InSections);
ENGINE_API FSkelMeshSectionArray BuildSkeletalMeshSections(const FMeshDataSectionArray& InSections);

#endif // WITH_EDITOR

struct FResources
{
	// Persistent State
	TArray< uint8 >					RootData;			// Root pages are loaded on resource load, so we always have something to draw.
	FByteBulkData					StreamablePages;	// Remaining pages are streamed on demand.
	TArray< uint16 >				ImposterAtlas;
	TArray< FPackedHierarchyNode >	HierarchyNodes;
	TArray< uint32 >				HierarchyRootOffsets;
	TArray< FPageStreamingState >	PageStreamingStates;
	TArray< uint32 >				PageDependencies;
	TArray< FMatrix3x4 >			AssemblyTransforms;
	FBoxSphereBounds3f 				MeshBounds			= FBoxSphereBounds3f(ForceInit);
	uint32							NumRootPages		= 0;
	int32							PositionPrecision	= 0;
	int32							NormalPrecision		= 0;
	int32							TangentPrecision	= 0;
	uint32							NumInputTriangles	= 0;
	uint32							NumInputVertices	= 0;
	uint32							NumClusters			= 0;
	uint32							ResourceFlags		= 0;

	// Runtime State
	uint32	RuntimeResourceID		= MAX_uint32;
	uint32	HierarchyOffset			= MAX_uint32;
	uint32	AssemblyTransformOffset	= MAX_uint32;
	int32	RootPageIndex			= INDEX_NONE;
	int32	ImposterIndex			= INDEX_NONE;
	uint32	NumHierarchyNodes		= 0;
	uint32	NumHierarchyDwords		= 0;
	uint32	NumResidentClusters		= 0;
	uint32	PersistentHash			= NANITE_INVALID_PERSISTENT_HASH;

#if WITH_EDITOR
	FString							ResourceName;
	FIoHash							DDCKeyHash;
	FIoHash							DDCRawHash;
private:
	TDontCopy<TPimplPtr<UE::DerivedData::FRequestOwner>> DDCRequestOwner;

	enum class EDDCRebuildState : uint8
	{
		Initial,
		InitialAfterFailed,
		Pending,
		Succeeded,
		Failed,
	};
	static bool IsInitialState(EDDCRebuildState State)
	{
		return State == EDDCRebuildState::Initial || State == EDDCRebuildState::InitialAfterFailed;
	}

	struct FDDCRebuildState
	{
		std::atomic<EDDCRebuildState> State = EDDCRebuildState::Initial;

		FDDCRebuildState() = default;
		FDDCRebuildState(const FDDCRebuildState&) {}
		FDDCRebuildState& operator=(const FDDCRebuildState&) { check(IsInitialState(EDDCRebuildState::Initial)); return *this; }
	};

	FDDCRebuildState		DDCRebuildState;

	/** Begins an async rebuild of the bulk data from the cache. Must be paired with EndRebuildBulkDataFromCache. */
	ENGINE_API void BeginRebuildBulkDataFromCache(const UObject* Owner);
	/** Ends an async rebuild of the bulk data from the cache. May block if poll has not returned true. */
	ENGINE_API void EndRebuildBulkDataFromCache();
public:
	ENGINE_API void DropBulkData();

	UE_DEPRECATED(5.1, "Use RebuildBulkDataFromCacheAsync instead.")
	ENGINE_API void RebuildBulkDataFromDDC(const UObject* Owner);

	ENGINE_API bool HasBuildFromDDCError() const;
	ENGINE_API void SetHasBuildFromDDCError(bool bHasError);

	/** Requests (or polls) an async operation that rebuilds the streaming bulk data from the cache.
		If a rebuild is already in progress, the call will just poll the pending operation.
		If true is returned, the operation is complete and it is safe to access the streaming data.
		If false is returned, the operation has not yet completed.
		The operation can fail, which is indicated by the value of bFailed. */
	ENGINE_API bool RebuildBulkDataFromCacheAsync(const UObject* Owner, bool& bFailed);
#endif

	ENGINE_API void InitResources(const UObject* Owner);
	ENGINE_API bool ReleaseResources();

	ENGINE_API void Serialize(FArchive& Ar, UObject* Owner, bool bCooked);
	ENGINE_API bool HasStreamingData() const;

	void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) const;
	bool IsRootPage(uint32 PageIndex) const { return PageIndex < NumRootPages; }

private:
	ENGINE_API void SerializeInternal(FArchive& Ar, UObject* Owner, bool bCooked);
};

class FVertexFactoryResource : public FRenderResource
{
public:
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
	virtual void ReleaseRHI() override;

	FNaniteVertexFactory* GetVertexFactory() { return VertexFactory; }

private:
	FNaniteVertexFactory* VertexFactory = nullptr;
};

} // namespace Nanite

ENGINE_API void ClearNaniteResources(TPimplPtr<Nanite::FResources>& InResources);
ENGINE_API void InitNaniteResources(TPimplPtr<Nanite::FResources>& InResources, bool bRecreate = false);

ENGINE_API uint64 GetNaniteResourcesSize(const TPimplPtr<Nanite::FResources>& InResources);
ENGINE_API void GetNaniteResourcesSizeEx(const TPimplPtr<Nanite::FResources>& InResources, FResourceSizeEx& CumulativeResourceSize);

ENGINE_API uint64 GetNaniteResourcesSize(const Nanite::FResources& InResources);
ENGINE_API void GetNaniteResourcesSizeEx(const Nanite::FResources& InResources, FResourceSizeEx& CumulativeResourceSize);

template <>
struct TIsZeroConstructType<Nanite::FMeshDataSectionArray> : TIsZeroConstructType<TArray<Nanite::FMeshDataSection, TInlineAllocator<1>>>
{
};

template <>
struct TIsContiguousContainer<Nanite::FMeshDataSectionArray> : TIsContiguousContainer<TArray<Nanite::FMeshDataSection, TInlineAllocator<1>>>
{
};
