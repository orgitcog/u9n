// Copyright Epic Games, Inc. All Rights Reserved.
// Copyright (C) Microsoft. All rights reserved.

/*=============================================================================
GPUSkinCache.cpp: Performs skinning on a compute shader into a buffer to avoid vertex buffer skinning.
=============================================================================*/

#include "GPUSkinCache.h"
#include "CanvasTypes.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/RenderCommandPipes.h"
#include "SkeletalRenderGPUSkin.h"
#include "MeshDrawShaderBindings.h"
#include "ShaderParameterUtils.h"
#include "PipelineStateCache.h"
#include "RenderCaptureInterface.h"
#include "Engine/SkinnedAssetCommon.h"
#include "GPUSkinCacheVisualizationData.h"
#include "RHIContext.h"
#include "ShaderPlatformCachedIniValue.h"
#include "RenderUtils.h"
#include "RendererInterface.h"
#include "RenderingThread.h"
#include "RHIResourceUtils.h"
#include "Stats/StatsTrace.h"
#include "UObject/UObjectIterator.h"
#include "Algo/Sort.h"
#include "ComponentRecreateRenderStateContext.h"
#include "RenderGraphUtils.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "ShaderCompilerCore.h"

DECLARE_DWORD_COUNTER_STAT(TEXT("Num Dispatches"), STAT_GPUSkinCache_NumDispatches, STATGROUP_GPUSkinCache);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num Buffers"), STAT_GPUSkinCache_NumBuffers, STATGROUP_GPUSkinCache);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num RayTracing Dispatches"), STAT_GPUSkinCache_NumRayTracingDispatches, STATGROUP_GPUSkinCache);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num RayTracing Buffers"), STAT_GPUSkinCache_NumRayTracingBuffers, STATGROUP_GPUSkinCache);
DEFINE_STAT(STAT_GPUSkinCache_TotalNumChunks);
DEFINE_STAT(STAT_GPUSkinCache_TotalNumVertices);
DEFINE_STAT(STAT_GPUSkinCache_TotalMemUsed);
DEFINE_STAT(STAT_GPUSkinCache_TangentsIntermediateMemUsed);
DEFINE_STAT(STAT_GPUSkinCache_NumTrianglesForRecomputeTangents);
DEFINE_STAT(STAT_GPUSkinCache_NumSectionsProcessed);
DEFINE_LOG_CATEGORY_STATIC(LogSkinCache, Log, All);

/** Exec helper to handle GPU Skin Cache related commands. */
class FSkinCacheExecHelper : public FSelfRegisteringExec
{
	virtual bool Exec_Runtime(class UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override
	{
		/** Command to list all skeletal mesh lods which have the skin cache disabled. */
		if (FParse::Command(&Cmd, TEXT("list skincacheusage")))
		{
			UE_LOG(LogTemp, Display, TEXT("Name, Lod Index, Skin Cache Usage"));

			for (TObjectIterator<USkeletalMesh> It; It; ++It)
			{
				if (USkeletalMesh* SkeletalMesh = *It)
				{
					for (int32 LODIndex = 0; LODIndex < SkeletalMesh->GetLODNum(); ++LODIndex)
					{
						if (FSkeletalMeshLODInfo* LODInfo = SkeletalMesh->GetLODInfo(LODIndex))
						{
							UE_LOG(LogTemp, Display, TEXT("%s, %d, %d"), *SkeletalMesh->GetFullName(), LODIndex, int(LODInfo->SkinCacheUsage));
						}
					}
				}
			}
			return true;
		}
		return false;
	}
};
static FSkinCacheExecHelper GSkelMeshExecHelper;

static int32 GEnableGPUSkinCacheShaders = 0;

static TAutoConsoleVariable<bool> CVarAllowGPUSkinCache(
	TEXT("r.SkinCache.Allow"),
	true,
	TEXT("Whether or not to allow the GPU skin Cache system to be enabled.\n"),
	ECVF_RenderThreadSafe | ECVF_ReadOnly
);

static FAutoConsoleVariableRef CVarEnableGPUSkinCacheShaders(
	TEXT("r.SkinCache.CompileShaders"),
	GEnableGPUSkinCacheShaders,
	TEXT("Whether or not to compile the GPU compute skinning cache shaders.\n")
	TEXT("This will compile the shaders for skinning on a compute job and not skin on the vertex shader.\n")
	TEXT("GPUSkinVertexFactory.usf needs to be touched to cause a recompile if this changes.\n")
	TEXT("0 is off(default), 1 is on"),
	ECVF_RenderThreadSafe | ECVF_ReadOnly
);

static TAutoConsoleVariable<bool> CVarSkipCompilingGPUSkinVF(
	TEXT("r.SkinCache.SkipCompilingGPUSkinVF"),
	false,
	TEXT("Reduce GPU Skin Vertex Factory shader permutations. Cannot be disabled while the skin cache is turned off.\n")
	TEXT(" False ( 0): Compile all GPU Skin Vertex factory variants.\n")
	TEXT(" True  ( 1): Don't compile all GPU Skin Vertex factory variants."),
    ECVF_RenderThreadSafe | ECVF_ReadOnly
);

// 0/1
int32 GEnableGPUSkinCache = 1;
static TAutoConsoleVariable<int32> CVarEnableGPUSkinCache(
	TEXT("r.SkinCache.Mode"),
	1,
	TEXT("Whether or not to use the GPU compute skinning cache.\n")
	TEXT("This will perform skinning on a compute job and not skin on the vertex shader.\n")
	TEXT("Requires r.SkinCache.CompileShaders=1 and r.SkinCache.Allow=1\n")
	TEXT(" 0: off\n")
	TEXT(" 1: on(default)\n"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarDefaultGPUSkinCacheBehavior(
	TEXT("r.SkinCache.DefaultBehavior"),
	(int32)ESkinCacheDefaultBehavior::Inclusive,
	TEXT("Default behavior if all skeletal meshes are included/excluded from the skin cache. If Support Ray Tracing is enabled on a mesh, will force inclusive behavior on that mesh.\n")
	TEXT(" Exclusive ( 0): All skeletal meshes are excluded from the skin cache. Each must opt in individually.\n")
	TEXT(" Inclusive ( 1): All skeletal meshes are included into the skin cache. Each must opt out individually. (default)")
	);

int32 GSkinCacheRecomputeTangents = 2;
TAutoConsoleVariable<int32> CVarGPUSkinCacheRecomputeTangents(
	TEXT("r.SkinCache.RecomputeTangents"),
	2,
	TEXT("This option enables recomputing the vertex tangents on the GPU.\n")
	TEXT("Can be changed at runtime, requires both r.SkinCache.CompileShaders=1, r.SkinCache.Mode=1, r.SkinCache.Allow=1 and r.SkinCache.StoreDuplicatedVerticesForRecomputeTangents\n")
	TEXT(" 0: off\n")
	TEXT(" 1: on, forces all skinned object to Recompute Tangents\n")
	TEXT(" 2: on, only recompute tangents on skinned objects who ticked the Recompute Tangents checkbox(default)\n"),
	ECVF_RenderThreadSafe
);

static int32 GNumTangentIntermediateBuffers = 1;
static TAutoConsoleVariable<float> CVarGPUSkinNumTangentIntermediateBuffers(
	TEXT("r.SkinCache.NumTangentIntermediateBuffers"),
	1,
	TEXT("How many intermediate buffers to use for intermediate results while\n")
	TEXT("doing Recompute Tangents; more may allow the GPU to overlap compute jobs."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarGPUSkinCacheDebug(
	TEXT("r.SkinCache.Debug"),
	1.0f,
	TEXT("A scaling constant passed to the SkinCache shader, useful for debugging"),
	ECVF_RenderThreadSafe
);

static float GSkinCacheSceneMemoryLimitInMB = 128.0f;
static TAutoConsoleVariable<float> CVarGPUSkinCacheSceneMemoryLimitInMB(
	TEXT("r.SkinCache.SceneMemoryLimitInMB"),
	128.0f,
	TEXT("Maximum memory allowed to be allocated per World/Scene in Megs"),
	ECVF_RenderThreadSafe
);

static int32 GStoreDuplicatedVerticesForRecomputeTangents = 0;
FAutoConsoleVariableRef CVarGPUSkinCacheStoreDuplicatedVerticesForRecomputeTangents(
	TEXT("r.SkinCache.StoreDuplicatedVerticesForRecomputeTangents"),
	GStoreDuplicatedVerticesForRecomputeTangents,
	TEXT("0: Don't store duplicated vertices for all skeletal mesh render sections. It will still be stored if the render section has bRecomputeTangent set. (default)\n")
	TEXT("1: Store duplicated vertices for all skeletal mesh render sections.\n"),
	ECVF_ReadOnly
);

static int32 GUseDuplicatedVerticesForRecomputeTangents = 1;
FAutoConsoleVariableRef CVarGPUSkinCacheAllowDupedVertesForRecomputeTangents(
	TEXT("r.SkinCache.UseDuplicatedVerticesForRecomputeTangents"),
	GUseDuplicatedVerticesForRecomputeTangents,
	TEXT("0: Disable usage of duplicated vertices for runtime tangent recomputation/\n")
	TEXT("1: Use stored duplicated vertices if they are available (default).\n"),
	ECVF_RenderThreadSafe
);

int32 GRecomputeTangentsParallelDispatch = 0;
FAutoConsoleVariableRef CVarRecomputeTangentsParallelDispatch(
	TEXT("r.SkinCache.RecomputeTangentsParallelDispatch"),
	GRecomputeTangentsParallelDispatch,
	TEXT("This option enables parallel dispatches for recompute tangents.\n")
	TEXT(" 0: off (default), triangle pass is interleaved with vertex pass, requires resource barriers in between. \n")
	TEXT(" 1: on, batch triangle passes together, resource barrier, followed by vertex passes together, cost more memory. \n"),
	ECVF_RenderThreadSafe
);

static int32 GSkinCachePrintMemorySummary = 0;
FAutoConsoleVariableRef CVarGPUSkinCachePrintMemorySummary(
	TEXT("r.SkinCache.PrintMemorySummary"),
	GSkinCachePrintMemorySummary,
	TEXT("Print break down of memory usage.")
	TEXT(" 0: off (default),")
	TEXT(" 1: print when out of memory,")
	TEXT(" 2: print every frame"),
	ECVF_RenderThreadSafe
);

bool GSkinCacheAsyncCompute = false;
FAutoConsoleVariableRef CVarSkinCacheAsyncCompute(
	TEXT("r.SkinCache.AsyncCompute"),
	GSkinCacheAsyncCompute,
	TEXT(" 0: off\n")
	TEXT(" 1: on\n"),
	ECVF_RenderThreadSafe
);

int32 GNumDispatchesToCapture = 0;
static FAutoConsoleVariableRef CVarGPUSkinCacheNumDispatchesToCapture(
	TEXT("r.SkinCache.Capture"),
	GNumDispatchesToCapture,
	TEXT("Trigger a render capture for the next skin cache dispatches."));

static int32 GGPUSkinCacheFlushCounter = 0;

const float MBSize = 1048576.f; // 1024 x 1024 bytes

static inline bool IsGPUSkinCacheEnable(EShaderPlatform Platform)
{
	static FShaderPlatformCachedIniValue<int32> PerPlatformCVar(TEXT("r.SkinCache.Mode"));
	return (PerPlatformCVar.Get(Platform) != 0);
}

static inline bool IsGPUSkinCacheInclusive(EShaderPlatform Platform)
{
	static FShaderPlatformCachedIniValue<int32> PerPlatformCVar(TEXT("r.SkinCache.DefaultBehavior"));
	return (PerPlatformCVar.Get(Platform) != 0);
}

bool ShouldWeCompileGPUSkinVFShaders(EShaderPlatform Platform, ERHIFeatureLevel::Type FeatureLevel)
{
	// If the skin cache is not available on this platform we need to compile GPU Skin VF shaders.
	if (IsGPUSkinCacheAvailable(Platform) == false)
	{
		return true;
	}

	// If the skin cache is not available on this platform we need to compile GPU Skin VF Shaders.
	if (IsGPUSkinCacheEnable(Platform) == false)
	{
		return true;
	}

	// If the skin cache has been globally disabled for all skeletal meshes we need to compile GPU Skin VF Shaders.
	if (IsGPUSkinCacheInclusive(Platform) == false)
	{
		return true;
	}

	// Some mobile GPUs (MALI) has a 64K elements limitation on texel buffers
	// This results in meshes with more than 64k vertices having their skin cache entries disabled at runtime.
	// We don't have a reliable way of checking this at cook time, so for mobile we must always cache skin cache
	// shaders so we have something to fall back to.
	if (FeatureLevel == ERHIFeatureLevel::ES3_1)
	{
		return true;
	}

	// If the skin cache is enabled and we've been asked to skip GPU Skin VF shaders.
	static FShaderPlatformCachedIniValue<bool> PerPlatformCVar(TEXT("r.SkinCache.SkipCompilingGPUSkinVF"));
	return (PerPlatformCVar.Get(Platform) == false);
}

ESkinCacheDefaultBehavior GetSkinCacheDefaultBehavior()
{
	return ESkinCacheDefaultBehavior(CVarDefaultGPUSkinCacheBehavior->GetInt()) == ESkinCacheDefaultBehavior::Inclusive
		? ESkinCacheDefaultBehavior::Inclusive
		: ESkinCacheDefaultBehavior::Exclusive;
}

ENGINE_API bool GPUSkinCacheNeedsDuplicatedVertices()
{
#if WITH_EDITOR // Duplicated vertices are used in the editor when merging meshes
	return true;
#else
	return GPUSkinCacheStoreDuplicatedVertices();
#endif
}

ENGINE_API bool GPUSkinCacheStoreDuplicatedVertices()
{
	return GStoreDuplicatedVerticesForRecomputeTangents > 0;
}

RDG_REGISTER_BLACKBOARD_STRUCT(FGPUSkinCache::FTaskData);

// determine if during DispatchUpdateSkinning caching should occur
enum class EGPUSkinCacheDispatchFlags : uint8
{
	None                 = 0,
	Position             = 1 << 0,
	PositionPrevious     = 1 << 1,
	RecomputeTangents    = 1 << 2
};
ENUM_CLASS_FLAGS(EGPUSkinCacheDispatchFlags);

enum class EGPUSkinCacheBufferBits : uint8
{
	None                 = 0,
	IntermediateTangents = 1 << 1,
	PositionPrevious     = 1 << 2,
};
ENUM_CLASS_FLAGS(EGPUSkinCacheBufferBits);

struct FGPUSkinCache::FSkinCacheRWBuffer
{
	FRWBuffer	Buffer;
	ERHIAccess	AccessState = ERHIAccess::Unknown;	// Keep track of current access state
	mutable uint32	LastTransitionFence = 0;

	void Release()
	{
		Buffer.Release();
		AccessState = ERHIAccess::Unknown;
	}

	bool UpdateFence(uint32 NextTransitionFence)
	{
		const bool bUpdateRequired = LastTransitionFence != NextTransitionFence;
		LastTransitionFence = NextTransitionFence;
		return bUpdateRequired;
	}

	// Update the access state and return transition info
	FRHITransitionInfo UpdateAccessState(ERHIAccess NewState)
	{
		ERHIAccess OldState = AccessState;
		AccessState = NewState;
		return FRHITransitionInfo(Buffer.UAV.GetReference(), OldState, AccessState);
	}
};

struct FGPUSkinCache::FRWBuffersAllocationInitializer
{
	static const uint32 PositionStride = 4;

	EGPUSkinCacheBufferBits BufferBits = EGPUSkinCacheBufferBits::None;
	uint32 NumVertices = 0;
	uint32 IntermediateAccumulatedTangentsSize = 0;
	EPixelFormat TangentFormat = PF_Unknown;

	static uint32 GetPositionStride()
	{
		return PositionStride;
	}

	uint32 GetTangentStride() const
	{
		return GPixelFormats[TangentFormat].BlockBytes;
	}

	uint32 GetBufferSize() const
	{
		const bool bIntermediateTangents = EnumHasAnyFlags(BufferBits, EGPUSkinCacheBufferBits::IntermediateTangents);
		const bool bPositionPrevious = EnumHasAnyFlags(BufferBits, EGPUSkinCacheBufferBits::PositionPrevious);

		const uint32 TangentStride = GetTangentStride();

		const uint32 PositionBufferSize = PositionStride * NumVertices * 3 * (bPositionPrevious ? NUM_BUFFERS : 1);
		const uint32 TangentBufferSize =  TangentStride * NumVertices * 2;
		const uint32 IntermediateTangentBufferSize = bIntermediateTangents ? TangentStride * NumVertices * 2 : 0;
		const uint32 AccumulatedTangentBufferSize = IntermediateAccumulatedTangentsSize * FGPUSkinCache::IntermediateAccumBufferNumInts * sizeof(int32);

		return TangentBufferSize + IntermediateTangentBufferSize + PositionBufferSize + AccumulatedTangentBufferSize;
	}
};

struct FGPUSkinCache::FRWBuffersAllocation
{
	friend FRWBufferTracker;

	FRWBuffersAllocation(FRHICommandList& RHICmdList, const FRWBuffersAllocationInitializer& InInitializer, const FName& OwnerName)
		: Initializer(InInitializer)
	{
		const static FLazyName PositionsName(TEXT("SkinCachePositions"));
		const static FLazyName TangentsName(TEXT("SkinCacheTangents"));
		const static FLazyName IntermediateTangentsName(TEXT("SkinCacheIntermediateTangents"));
		const static FLazyName IntermediateAccumulatedTangentsName(TEXT("SkinCacheIntermediateAccumulatedTangents"));

		const int32 NumBuffers = EnumHasAnyFlags(Initializer.BufferBits, EGPUSkinCacheBufferBits::PositionPrevious) ? NUM_BUFFERS : 1;
		
		const uint32 PositionStride = InInitializer.GetPositionStride();
		const uint32 TangentStride = InInitializer.GetTangentStride();

		for (int32 Index = 0; Index < NumBuffers; ++Index)
		{
			PositionBuffers[Index].Buffer.ClassName = PositionsName;
			PositionBuffers[Index].Buffer.OwnerName = OwnerName;
			PositionBuffers[Index].Buffer.Initialize(RHICmdList, TEXT("SkinCachePositions"), PositionStride, Initializer.NumVertices * 3, PF_R32_FLOAT, ERHIAccess::SRVMask, BUF_Static);
			PositionBuffers[Index].Buffer.Buffer->SetOwnerName(OwnerName);
			PositionBuffers[Index].AccessState = ERHIAccess::Unknown;
		}

		// Tangents are skinned inside the main skinning compute shader and are always allocated, even if the recompute tangents pass doesn't run.
		Tangents.Buffer.ClassName = TangentsName;
		Tangents.Buffer.OwnerName = OwnerName;
		Tangents.Buffer.Initialize(RHICmdList, TEXT("SkinCacheTangents"), TangentStride, Initializer.NumVertices * 2, Initializer.TangentFormat, BUF_Static);
		Tangents.Buffer.Buffer->SetOwnerName(OwnerName);
		Tangents.AccessState = ERHIAccess::Unknown;

		if (EnumHasAnyFlags(Initializer.BufferBits, EGPUSkinCacheBufferBits::IntermediateTangents))
		{
			IntermediateTangents.Buffer.ClassName = IntermediateTangentsName;
			IntermediateTangents.Buffer.OwnerName = OwnerName;
			IntermediateTangents.Buffer.Initialize(RHICmdList, TEXT("SkinCacheIntermediateTangents"), TangentStride, Initializer.NumVertices * 2, Initializer.TangentFormat, BUF_Static);
			IntermediateTangents.Buffer.Buffer->SetOwnerName(OwnerName);
			IntermediateTangents.AccessState = ERHIAccess::Unknown;
		}

		if (Initializer.IntermediateAccumulatedTangentsSize > 0)
		{
			IntermediateAccumulatedTangents.Buffer.ClassName = IntermediateAccumulatedTangentsName;
			IntermediateAccumulatedTangents.Buffer.OwnerName = OwnerName;
			IntermediateAccumulatedTangents.Buffer.Initialize(RHICmdList, TEXT("SkinCacheIntermediateAccumulatedTangents"), sizeof(int32), Initializer.IntermediateAccumulatedTangentsSize * FGPUSkinCache::IntermediateAccumBufferNumInts, PF_R32_SINT, BUF_UnorderedAccess);
			IntermediateAccumulatedTangents.Buffer.Buffer->SetOwnerName(OwnerName);
			IntermediateAccumulatedTangents.AccessState = ERHIAccess::Unknown;

			// The UAV must be zero-filled. We leave it zeroed after each round (see RecomputeTangentsPerVertexPass.usf), so this is only needed on when the buffer is first created.
			RHICmdList.ClearUAVUint(IntermediateAccumulatedTangents.Buffer.UAV, FUintVector4(0, 0, 0, 0));
		}
	}

	~FRWBuffersAllocation()
	{
		for (int32 Index = 0; Index < NUM_BUFFERS; ++Index)
		{
			PositionBuffers[Index].Release();
		}

		Tangents.Release();
		IntermediateTangents.Release();
		IntermediateAccumulatedTangents.Release();
	}

	uint64 GetBufferSize() const
	{
		return Initializer.GetBufferSize();
	}

	FSkinCacheRWBuffer* GetTangentBuffer()
	{
		return &Tangents;
	}

	FSkinCacheRWBuffer* GetIntermediateTangentBuffer()
	{
		return EnumHasAllFlags(Initializer.BufferBits, EGPUSkinCacheBufferBits::IntermediateTangents) ? &IntermediateTangents : nullptr;
	}

	FSkinCacheRWBuffer* GetIntermediateAccumulatedTangentBuffer()
	{
		return Initializer.IntermediateAccumulatedTangentsSize > 0 ? &IntermediateAccumulatedTangents : nullptr;
	}

	bool HasPreviousBuffer() const
	{
		return EnumHasAllFlags(Initializer.BufferBits, EGPUSkinCacheBufferBits::PositionPrevious);
	}

	FSkinCacheRWBuffer& GetPositionBuffer()
	{
		return PositionBuffers[0];
	}

private:
	// Output of the GPU skinning (ie Pos, Normals)
	FSkinCacheRWBuffer PositionBuffers[NUM_BUFFERS];

	FSkinCacheRWBuffer Tangents;
	FSkinCacheRWBuffer IntermediateTangents;
	FSkinCacheRWBuffer IntermediateAccumulatedTangents;	// Intermediate buffer used to accumulate results of triangle pass to be passed onto vertex pass

	const FRWBuffersAllocationInitializer Initializer;
};

struct FGPUSkinCache::FRWBufferTracker
{
	FRWBuffersAllocation* Allocation;

	FRWBufferTracker()
		: Allocation(nullptr)
	{
		Reset();
	}

	void Reset()
	{
		for (int32 Index = 0; Index < NUM_BUFFERS; ++Index)
		{
			Revisions[Index] = 0;
			BoneBuffers[Index] = nullptr;
		}
	}

	uint32 GetBufferSize() const
	{
		return Allocation->GetBufferSize();
	}

	FSkinCacheRWBuffer* Find(const FVertexBufferAndSRV& BoneBuffer, uint32 Revision)
	{
		for (int32 Index = 0; Index < NUM_BUFFERS; ++Index)
		{
			if (Revisions[Index] == Revision && BoneBuffers[Index] == &BoneBuffer)
			{
				return &Allocation->PositionBuffers[Index];
			}
		}

		return nullptr;
	}

	FSkinCacheRWBuffer* GetTangentBuffer()
	{
		return Allocation ? Allocation->GetTangentBuffer() : nullptr;
	}

	FSkinCacheRWBuffer* GetIntermediateTangentBuffer()
	{
		return Allocation ? Allocation->GetIntermediateTangentBuffer() : nullptr;
	}

	FSkinCacheRWBuffer* GetIntermediateAccumulatedTangentBuffer()
	{
		return Allocation ? Allocation->GetIntermediateAccumulatedTangentBuffer() : nullptr;
	}

	// Allocates an element that's not the "Used" element passed in (or if Used is NULL, allocates any element).
	FSkinCacheRWBuffer* AllocateUnused(const FVertexBufferAndSRV& BoneBuffer, uint32 Revision, const FSkinCacheRWBuffer* Used)
	{
		int32 UnusedIndex = Used == &Allocation->PositionBuffers[0] ? 1 : 0;
		Revisions[UnusedIndex] = Revision;
		BoneBuffers[UnusedIndex] = &BoneBuffer;

		return &Allocation->PositionBuffers[UnusedIndex];
	}

	// On recreate of the render state where the GPU skin cache entry is preserved, the bone buffer will have been reallocated,
	// even though the transforms didn't change.  We need to force the Find() call above to treat the data as up-to-date, which
	// can be accomplished by updating the BoneBuffer pointer for the previous Revision, so it matches again.
	void UpdatePreviousBoneBuffer(const FVertexBufferAndSRV& PreviousBoneBuffer, uint32 PreviousRevision)
	{
		for (int32 Index = 0; Index < NUM_BUFFERS; ++Index)
		{
			if (Revisions[Index] == PreviousRevision)
			{
				BoneBuffers[Index] = &PreviousBoneBuffer;
				break;
			}
		}
	}

private:
	uint32 Revisions[NUM_BUFFERS];
	const FVertexBufferAndSRV* BoneBuffers[NUM_BUFFERS];
};

class FGPUSkinCacheEntry
{
public:
	FGPUSkinCacheEntry(FGPUSkinCache* InSkinCache, FSkeletalMeshObject* InGPUSkin, FGPUSkinCache::FRWBuffersAllocation* InPositionAllocation, int32 InLOD, EGPUSkinCacheEntryMode InMode)
		: Mode(InMode)
		, PositionAllocation(InPositionAllocation)
		, SkinCache(InSkinCache)
		, GPUSkin(InGPUSkin)
		, MorphBuffer(0)
		, LOD(InLOD)
	{
		const TArray<FSkelMeshRenderSection>& Sections = InGPUSkin->GetRenderSections(LOD);
		DispatchData.AddDefaulted(Sections.Num());

		UpdateSkinWeightBuffer();
	}

	~FGPUSkinCacheEntry()
	{
		check(!PositionAllocation);
	}

	struct FSectionDispatchData
	{
		FGPUSkinCache::FRWBufferTracker PositionTracker;

		FGPUBaseSkinVertexFactory* SourceVertexFactory = nullptr;
		FGPUSkinPassthroughVertexFactory* TargetVertexFactory = nullptr;

		// triangle index buffer (input for the RecomputeSkinTangents, might need special index buffer unique to position and normal, not considering UV/vertex color)
		FRHIShaderResourceView* IndexBuffer = nullptr;

		const FSkelMeshRenderSection* Section = nullptr;

		// for debugging / draw events, -1 if not set
		uint32 SectionIndex = -1;

		// 0:normal, 1:with morph target, 2:with APEX cloth (not yet implemented)
		uint16 SkinType = 0;

		EGPUSkinCacheDispatchFlags DispatchFlags = EGPUSkinCacheDispatchFlags::None;

		uint32 UpdatedFrameNumber = 0;
		
		uint32 NumBoneInfluences = 0;

		// in floats (4 bytes)
		uint32 OutputStreamStart = 0;
		uint32 NumVertices = 0;

		// in vertices
		uint32 InputStreamStart = 0;
		uint32 NumTexCoords = 1;
		uint32 SelectedTexCoord = 0;

		FShaderResourceViewRHIRef TangentBufferSRV = nullptr;
		FShaderResourceViewRHIRef UVsBufferSRV = nullptr;
		FShaderResourceViewRHIRef ColorBufferSRV = nullptr;
		FShaderResourceViewRHIRef PositionBufferSRV = nullptr;
		FShaderResourceViewRHIRef ClothPositionsAndNormalsBuffer = nullptr;

		// skin weight input
		uint32 InputWeightStart = 0;

		// morph input
		uint32 MorphBufferOffset = 0;

        // cloth input
		uint32 ClothBufferOffset = 0;
        float ClothBlendWeight = 0.0f;
		uint32 ClothNumInfluencesPerVertex = 1;
        FMatrix44f ClothToLocal = FMatrix44f::Identity;
		FVector3f WorldScale = FVector3f::OneVector;

		// triangle index buffer (input for the RecomputeSkinTangents, might need special index buffer unique to position and normal, not considering UV/vertex color)
		uint32 IndexBufferOffsetValue = 0;
		uint32 NumTriangles = 0;
		uint32 RevisionNumber = 0;
		FGPUSkinCache::FSkinCacheRWBuffer* TangentBuffer = nullptr;
		FGPUSkinCache::FSkinCacheRWBuffer* IntermediateTangentBuffer = nullptr;
		FGPUSkinCache::FSkinCacheRWBuffer* IntermediateAccumulatedTangentBuffer = nullptr;
		uint32 IntermediateAccumulatedTangentBufferOffset = INDEX_NONE;
		FGPUSkinCache::FSkinCacheRWBuffer* PositionBuffer = nullptr;
		FGPUSkinCache::FSkinCacheRWBuffer* PreviousPositionBuffer = nullptr;

        // Handle duplicates
        FShaderResourceViewRHIRef DuplicatedIndicesIndices = nullptr;
        FShaderResourceViewRHIRef DuplicatedIndices = nullptr;

		FSectionDispatchData() = default;

		inline FGPUSkinCache::FSkinCacheRWBuffer* GetPreviousPositionRWBuffer() const
		{
			check(PreviousPositionBuffer);
			return PreviousPositionBuffer;
		}

		inline FGPUSkinCache::FSkinCacheRWBuffer* GetPositionRWBuffer() const
		{
			check(PositionBuffer);
			return PositionBuffer;
		}

		inline FGPUSkinCache::FSkinCacheRWBuffer* GetTangentRWBuffer() const
		{
			check(TangentBuffer);
			return TangentBuffer;
		}

		FGPUSkinCache::FSkinCacheRWBuffer* GetActiveTangentRWBuffer() const
		{
			// This is the buffer containing tangent results from the skinning CS pass
			return (IndexBuffer && IntermediateTangentBuffer) ? IntermediateTangentBuffer : TangentBuffer;
		}

		inline FGPUSkinCache::FSkinCacheRWBuffer* GetIntermediateAccumulatedTangentBuffer() const
		{
			check(IntermediateAccumulatedTangentBuffer);
			return IntermediateAccumulatedTangentBuffer;
		}

		void UpdateVertexFactoryDeclaration(FRHICommandListBase& RHICmdList, EGPUSkinCacheEntryMode InMode)
		{
			FRHIShaderResourceView* CurrentPositionSRV = PositionBuffer->Buffer.SRV;
			FRHIShaderResourceView* PreviousPositionSRV = PreviousPositionBuffer ? PreviousPositionBuffer->Buffer.SRV.GetReference() : CurrentPositionSRV;

			FGPUSkinPassthroughVertexFactory::FAddVertexAttributeDesc Desc;
			Desc.FrameNumber = InMode == EGPUSkinCacheEntryMode::Raster ? SourceVertexFactory->GetShaderData().UpdatedFrameNumber : 0;
			Desc.StreamBuffers[FGPUSkinPassthroughVertexFactory::EVertexAttribute::VertexPosition] = PositionBuffer->Buffer.Buffer;
			Desc.StreamBuffers[FGPUSkinPassthroughVertexFactory::EVertexAttribute::VertexTangent] = TangentBuffer->Buffer.Buffer;
			Desc.SRVs[FGPUSkinPassthroughVertexFactory::EShaderResource::Position] = CurrentPositionSRV;
			Desc.SRVs[FGPUSkinPassthroughVertexFactory::EShaderResource::PreviousPosition] = PreviousPositionSRV;
			Desc.SRVs[FGPUSkinPassthroughVertexFactory::EShaderResource::Tangent] = TangentBuffer->Buffer.SRV;
			TargetVertexFactory->SetVertexAttributes(RHICmdList, SourceVertexFactory, Desc);
		}
	};

	void UpdateVertexFactoryDeclaration(FRHICommandListBase& RHICmdList, int32 Section)
	{
		DispatchData[Section].UpdateVertexFactoryDeclaration(RHICmdList, Mode);
	}

	inline FCachedGeometry::Section GetCachedGeometry(int32 SectionIndex) const
	{
		FCachedGeometry::Section MeshSection;
		if (SectionIndex >= 0 && SectionIndex < DispatchData.Num())
		{
			const FSkelMeshRenderSection& Section = *DispatchData[SectionIndex].Section;
			MeshSection.PositionBuffer = DispatchData[SectionIndex].PositionBuffer->Buffer.SRV;
			MeshSection.PreviousPositionBuffer = DispatchData[SectionIndex].PreviousPositionBuffer->Buffer.SRV;
			MeshSection.UVsBuffer = DispatchData[SectionIndex].UVsBufferSRV;
			MeshSection.TangentBuffer = DispatchData[SectionIndex].TangentBufferSRV;
			MeshSection.TotalVertexCount = DispatchData[SectionIndex].PositionBuffer->Buffer.NumBytes / (sizeof(float) * 3);
			MeshSection.NumPrimitives = Section.NumTriangles;
			MeshSection.NumVertices = Section.NumVertices;
			MeshSection.IndexBaseIndex = Section.BaseIndex;
			MeshSection.VertexBaseIndex = Section.BaseVertexIndex;
			MeshSection.IndexBuffer = nullptr;
			MeshSection.TotalIndexCount = 0;
			MeshSection.LODIndex = 0;
			MeshSection.SectionIndex = SectionIndex;
		}
		return MeshSection;
	}

	bool IsSectionValid(int32 Section) const
	{
		const FSectionDispatchData& SectionData = DispatchData[Section];
		return SectionData.SectionIndex == Section;
	}

	bool IsSourceFactoryValid(int32 Section, FGPUBaseSkinVertexFactory* SourceVertexFactory) const
	{
		const FSectionDispatchData& SectionData = DispatchData[Section];
		return SectionData.SourceVertexFactory == SourceVertexFactory;
	}

	bool IsValid(FSkeletalMeshObject* InSkin, int32 InLOD) const
	{
		return GPUSkin == InSkin && LOD == InLOD;
	}

	void UpdateSkinWeightBuffer()
	{
		const FSkinWeightVertexBuffer* WeightBuffer = GPUSkin->GetSkinWeightVertexBuffer(LOD);
		bUse16BitBoneIndex = WeightBuffer->Use16BitBoneIndex();
		bUse16BitBoneWeight = WeightBuffer->Use16BitBoneWeight();
		InputWeightIndexSize = WeightBuffer->GetBoneIndexByteSize() | (WeightBuffer->GetBoneWeightByteSize() << 8);
		InputWeightStride = WeightBuffer->GetConstantInfluencesVertexStride();
		InputWeightStreamSRV = WeightBuffer->GetDataVertexBuffer()->GetSRV();
		InputWeightLookupStreamSRV = WeightBuffer->GetLookupVertexBuffer()->GetSRV();
				
		if (WeightBuffer->GetBoneInfluenceType() == GPUSkinBoneInfluenceType::DefaultBoneInfluence)
		{
			int32 MaxBoneInfluences = WeightBuffer->GetMaxBoneInfluences();
			BoneInfluenceType = MaxBoneInfluences > MAX_INFLUENCES_PER_STREAM ? 1 : 0;
		}
		else
		{
			BoneInfluenceType = 2;
		}
	}

	void SetupSection(
		int32 SectionIndex,
		FGPUSkinCache::FRWBuffersAllocation* InPositionAllocation,
		FSkelMeshRenderSection* Section,
		const FMorphVertexBuffer* MorphVertexBuffer,
		const FSkeletalMeshVertexClothBuffer* ClothVertexBuffer,
		uint32 NumVertices,
		uint32 InputStreamStart,
		FGPUBaseSkinVertexFactory* InSourceVertexFactory,
		FGPUSkinPassthroughVertexFactory* InTargetVertexFactory,
		uint32 InIntermediateAccumulatedTangentBufferOffset,
		const FClothSimulData* SimData)
	{
		//UE_LOG(LogSkinCache, Warning, TEXT("*** SetupSection E %p Alloc %p Sec %d(%p) LOD %d"), this, InAllocation, SectionIndex, Section, LOD);
		FSectionDispatchData& Data = DispatchData[SectionIndex];
		check(!Data.PositionTracker.Allocation || Data.PositionTracker.Allocation == InPositionAllocation);

		Data.PositionTracker.Allocation = InPositionAllocation;

		Data.SectionIndex = SectionIndex;
		Data.Section = Section;

		FSkeletalMeshRenderData& SkelMeshRenderData = GPUSkin->GetSkeletalMeshRenderData();
		FSkeletalMeshLODRenderData& LodData = SkelMeshRenderData.LODRenderData[LOD];
		check(Data.SectionIndex == LodData.FindSectionIndex(*Section));

		Data.NumVertices = NumVertices;
		const bool bMorph = MorphVertexBuffer && MorphVertexBuffer->SectionIds.Contains(SectionIndex);
		if (bMorph)
		{
			// in bytes
			const uint32 MorphStride = sizeof(FMorphGPUSkinVertex);

			// see GPU code "check(MorphStride == sizeof(float) * 6);"
			check(MorphStride == sizeof(float) * 6);

			Data.MorphBufferOffset = Section->BaseVertexIndex;
		}

		if (ClothVertexBuffer)
		{
			constexpr int32 ClothLODBias0 = 0;  // Use the same cloth LOD mapping (= 0 bias) to get the number of deformer weights
			const uint32 NumWrapDeformerWeights = Data.Section->ClothMappingDataLODs.Num() ? Data.Section->ClothMappingDataLODs[ClothLODBias0].Num() : 0;
			// NumInfluencesPerVertex should be a whole integer
			check(NumWrapDeformerWeights % Data.NumVertices == 0);
			Data.ClothNumInfluencesPerVertex = NumWrapDeformerWeights / Data.NumVertices;

			if (ClothVertexBuffer->GetClothIndexMapping().Num() > SectionIndex)
			{

				const FClothBufferIndexMapping& ClothBufferIndexMapping = ClothVertexBuffer->GetClothIndexMapping()[SectionIndex];

				check(SimData->LODIndex != INDEX_NONE && SimData->LODIndex <= LOD);
				const uint32 ClothLODBias = (uint32)(LOD - SimData->LODIndex);

				const uint32 ClothBufferOffset = ClothBufferIndexMapping.MappingOffset + ClothBufferIndexMapping.LODBiasStride * ClothLODBias;

				// Set the buffer offset depending on whether enough deformer mapping data exists (RaytracingMinLOD/RaytracingLODBias/ClothLODBiasMode settings)
				const uint32 NumInfluences = NumVertices ? ClothBufferIndexMapping.LODBiasStride / NumVertices : 1;
				Data.ClothBufferOffset = (ClothBufferOffset + NumVertices * NumInfluences <= ClothVertexBuffer->GetNumVertices()) ?
					ClothBufferOffset :                     // If the offset is valid, set the calculated LODBias offset
					ClothBufferIndexMapping.MappingOffset;  // Otherwise fallback to a 0 ClothLODBias to prevent from reading pass the buffer (but still raytrace broken shadows/reflections/etc.)
			}
		}

		// SkinType 0:normal, 1:with morph target, 2:with cloth
		Data.SkinType = ClothVertexBuffer ? 2 : (bMorph ? 1 : 0);
		Data.InputStreamStart = InputStreamStart;
		Data.OutputStreamStart = Section->BaseVertexIndex;

		Data.TangentBufferSRV = InSourceVertexFactory->GetTangentsSRV();
		Data.UVsBufferSRV = InSourceVertexFactory->GetTextureCoordinatesSRV();
		Data.ColorBufferSRV = InSourceVertexFactory->GetColorComponentsSRV();
		Data.NumTexCoords = InSourceVertexFactory->GetNumTexCoords();
		Data.PositionBufferSRV = InSourceVertexFactory->GetPositionsSRV();

		Data.NumBoneInfluences = InSourceVertexFactory->GetNumBoneInfluences();
		check(Data.TangentBufferSRV && Data.PositionBufferSRV);

		// weight buffer
		Data.InputWeightStart = (InputWeightStride * Section->BaseVertexIndex) / sizeof(float);
		Data.SourceVertexFactory = InSourceVertexFactory;
		Data.TargetVertexFactory = InTargetVertexFactory;

		int32 RecomputeTangentsMode = GSkinCacheRecomputeTangents;
		if (RecomputeTangentsMode > 0)
		{
			if (Section->bRecomputeTangent || RecomputeTangentsMode == 1)
			{
				FRawStaticIndexBuffer16or32Interface* IndexBuffer = LodData.MultiSizeIndexContainer.GetIndexBuffer();
				Data.IndexBuffer = IndexBuffer->GetSRV();
				if (Data.IndexBuffer)
				{
					Data.NumTriangles = Section->NumTriangles;
					Data.IndexBufferOffsetValue = Section->BaseIndex;
					Data.IntermediateAccumulatedTangentBufferOffset = InIntermediateAccumulatedTangentBufferOffset;
				}
			}
		}
	}

#if RHI_RAYTRACING
	void GetRayTracingSegmentVertexBuffers(TArray<FBufferRHIRef>& OutVertexBuffers) const
	{
		OutVertexBuffers.SetNum(DispatchData.Num());
		for (int32 SectionIdx = 0; SectionIdx < DispatchData.Num(); SectionIdx++)
		{
			FGPUSkinCache::FSkinCacheRWBuffer* PositionBuffer = DispatchData[SectionIdx].PositionBuffer;
			OutVertexBuffers[SectionIdx] = PositionBuffer ? PositionBuffer->Buffer.Buffer : nullptr;
		}
	}
#endif // RHI_RAYTRACING

	TArray<FSectionDispatchData>& GetDispatchData() { return DispatchData; }
	TArray<FSectionDispatchData> const& GetDispatchData() const { return DispatchData; }

protected:
	EGPUSkinCacheEntryMode Mode;
	FGPUSkinCache::FRWBuffersAllocation* PositionAllocation;
	FGPUSkinCache* SkinCache;
	TArray<FSectionDispatchData> DispatchData;
	FSkeletalMeshObject* GPUSkin;
	int BoneInfluenceType;
	bool bUse16BitBoneIndex;
	bool bUse16BitBoneWeight;
	bool bQueuedForDispatch = false;
	uint32 InputWeightIndexSize;
	uint32 InputWeightStride;
	FShaderResourceViewRHIRef InputWeightStreamSRV;
	FShaderResourceViewRHIRef InputWeightLookupStreamSRV;
	FRHIShaderResourceView* MorphBuffer;
	FShaderResourceViewRHIRef ClothBuffer;
	int32 LOD;

	friend class FGPUSkinCache;
	friend class FBaseGPUSkinCacheCS;
	friend class FBaseRecomputeTangentsPerTriangleShader;
};

class FBaseGPUSkinCacheCS : public FGlobalShader
{
	DECLARE_INLINE_TYPE_LAYOUT(FBaseGPUSkinCacheCS, NonVirtual);
public:
	FBaseGPUSkinCacheCS() {}

	FBaseGPUSkinCacheCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		//DebugParameter.Bind(Initializer.ParameterMap, TEXT("DebugParameter"));

		NumVertices.Bind(Initializer.ParameterMap, TEXT("NumVertices"));
		SkinCacheStart.Bind(Initializer.ParameterMap, TEXT("SkinCacheStart"));
		BoneMatrices.Bind(Initializer.ParameterMap, TEXT("BoneMatrices"));
		TangentInputBuffer.Bind(Initializer.ParameterMap, TEXT("TangentInputBuffer"));
		PositionInputBuffer.Bind(Initializer.ParameterMap, TEXT("PositionInputBuffer"));

		InputStreamStart.Bind(Initializer.ParameterMap, TEXT("InputStreamStart"));

		NumBoneInfluences.Bind(Initializer.ParameterMap, TEXT("NumBoneInfluences"));
		InputWeightIndexSize.Bind(Initializer.ParameterMap, TEXT("InputWeightIndexSize"));
		InputWeightStart.Bind(Initializer.ParameterMap, TEXT("InputWeightStart"));
		InputWeightStride.Bind(Initializer.ParameterMap, TEXT("InputWeightStride"));
		InputWeightStream.Bind(Initializer.ParameterMap, TEXT("InputWeightStream"));
		InputWeightLookupStream.Bind(Initializer.ParameterMap, TEXT("InputWeightLookupStream"));

		PositionBufferUAV.Bind(Initializer.ParameterMap, TEXT("PositionBufferUAV"));
		TangentBufferUAV.Bind(Initializer.ParameterMap, TEXT("TangentBufferUAV"));

		MorphBuffer.Bind(Initializer.ParameterMap, TEXT("MorphBuffer"));
		MorphBufferOffset.Bind(Initializer.ParameterMap, TEXT("MorphBufferOffset"));
		SkinCacheDebug.Bind(Initializer.ParameterMap, TEXT("SkinCacheDebug"));

		ClothBuffer.Bind(Initializer.ParameterMap, TEXT("ClothBuffer"));
		ClothPositionsAndNormalsBuffer.Bind(Initializer.ParameterMap, TEXT("ClothPositionsAndNormalsBuffer"));
		ClothBufferOffset.Bind(Initializer.ParameterMap, TEXT("ClothBufferOffset"));
		ClothBlendWeight.Bind(Initializer.ParameterMap, TEXT("ClothBlendWeight"));
		ClothToLocal.Bind(Initializer.ParameterMap, TEXT("ClothToLocal"));
		ClothNumInfluencesPerVertex.Bind(Initializer.ParameterMap, TEXT("ClothNumInfluencesPerVertex"));
		WorldScale.Bind(Initializer.ParameterMap, TEXT("WorldScale"));
	}

	void SetParameters(
		FRHIBatchedShaderParameters& BatchedParameters,
		const FVertexBufferAndSRV& BoneBuffer,
		FGPUSkinCacheEntry* Entry,
		const FGPUSkinCacheEntry::FSectionDispatchData& DispatchData,
		FRHIUnorderedAccessView* PositionUAV,
		FRHIUnorderedAccessView* TangentUAV
		)
	{
		SetShaderValue(BatchedParameters, NumVertices, DispatchData.NumVertices);
		SetShaderValue(BatchedParameters, InputStreamStart, DispatchData.InputStreamStart);

		check(BoneBuffer.VertexBufferSRV);
		SetSRVParameter(BatchedParameters, BoneMatrices, BoneBuffer.VertexBufferSRV);

		SetSRVParameter(BatchedParameters, TangentInputBuffer, DispatchData.TangentBufferSRV);
		SetSRVParameter(BatchedParameters, PositionInputBuffer, DispatchData.PositionBufferSRV);

		SetShaderValue(BatchedParameters, NumBoneInfluences, DispatchData.NumBoneInfluences);
		SetShaderValue(BatchedParameters, InputWeightIndexSize, Entry->InputWeightIndexSize);
		SetShaderValue(BatchedParameters, InputWeightStart, DispatchData.InputWeightStart);
		SetShaderValue(BatchedParameters, InputWeightStride, Entry->InputWeightStride);
		SetSRVParameter(BatchedParameters, InputWeightStream, Entry->InputWeightStreamSRV);
		SetSRVParameter(BatchedParameters, InputWeightLookupStream, Entry->InputWeightLookupStreamSRV);

		// output UAV
		SetUAVParameter(BatchedParameters, PositionBufferUAV, PositionUAV);
		SetUAVParameter(BatchedParameters, TangentBufferUAV, TangentUAV);
		SetShaderValue(BatchedParameters, SkinCacheStart, DispatchData.OutputStreamStart);

		const bool bMorph = DispatchData.SkinType == 1;
		if (bMorph)
		{
			SetSRVParameter(BatchedParameters, MorphBuffer, Entry->MorphBuffer);
			SetShaderValue(BatchedParameters, MorphBufferOffset, DispatchData.MorphBufferOffset);
		}

		const bool bCloth = DispatchData.SkinType == 2;
		if (bCloth)
		{
			SetSRVParameter(BatchedParameters, ClothBuffer, Entry->ClothBuffer);
			SetSRVParameter(BatchedParameters, ClothPositionsAndNormalsBuffer, DispatchData.ClothPositionsAndNormalsBuffer);
			SetShaderValue(BatchedParameters, ClothBufferOffset, DispatchData.ClothBufferOffset);
			SetShaderValue(BatchedParameters, ClothBlendWeight, DispatchData.ClothBlendWeight);
			SetShaderValue(BatchedParameters, ClothToLocal, DispatchData.ClothToLocal);
			SetShaderValue(BatchedParameters, ClothNumInfluencesPerVertex, DispatchData.ClothNumInfluencesPerVertex);
			SetShaderValue(BatchedParameters, WorldScale, DispatchData.WorldScale);
		}

		SetShaderValue(BatchedParameters, SkinCacheDebug, CVarGPUSkinCacheDebug.GetValueOnRenderThread());
	}

	void UnsetParameters(FRHIBatchedShaderUnbinds& BatchedUnbinds)
	{
		UnsetUAVParameter(BatchedUnbinds, PositionBufferUAV);
		UnsetUAVParameter(BatchedUnbinds, TangentBufferUAV);
	}

private:
	
	LAYOUT_FIELD(FShaderParameter, NumVertices)
	LAYOUT_FIELD(FShaderParameter, SkinCacheDebug)
	LAYOUT_FIELD(FShaderParameter, InputStreamStart)
	LAYOUT_FIELD(FShaderParameter, SkinCacheStart)

	//LAYOUT_FIELD(FShaderParameter, DebugParameter)

	LAYOUT_FIELD(FShaderUniformBufferParameter, SkinUniformBuffer)

	LAYOUT_FIELD(FShaderResourceParameter, BoneMatrices)
	LAYOUT_FIELD(FShaderResourceParameter, TangentInputBuffer)
	LAYOUT_FIELD(FShaderResourceParameter, PositionInputBuffer)
	LAYOUT_FIELD(FShaderResourceParameter, PositionBufferUAV)
	LAYOUT_FIELD(FShaderResourceParameter, TangentBufferUAV)

	LAYOUT_FIELD(FShaderParameter, NumBoneInfluences);
	LAYOUT_FIELD(FShaderParameter, InputWeightIndexSize);
	LAYOUT_FIELD(FShaderParameter, InputWeightStart)
	LAYOUT_FIELD(FShaderParameter, InputWeightStride)
	LAYOUT_FIELD(FShaderResourceParameter, InputWeightStream)
	LAYOUT_FIELD(FShaderResourceParameter, InputWeightLookupStream);

	LAYOUT_FIELD(FShaderResourceParameter, MorphBuffer)
	LAYOUT_FIELD(FShaderParameter, MorphBufferOffset)

	LAYOUT_FIELD(FShaderResourceParameter, ClothBuffer)
	LAYOUT_FIELD(FShaderResourceParameter, ClothPositionsAndNormalsBuffer)
	LAYOUT_FIELD(FShaderParameter, ClothBufferOffset)
	LAYOUT_FIELD(FShaderParameter, ClothBlendWeight)
	LAYOUT_FIELD(FShaderParameter, ClothToLocal)
	LAYOUT_FIELD(FShaderParameter, ClothNumInfluencesPerVertex)
	LAYOUT_FIELD(FShaderParameter, WorldScale)
};

/** Compute shader that skins a batch of vertices. */
// @param SkinType 0:normal, 1:with morph targets calculated outside the cache, 2: with cloth, 3:with morph target calculated inside the cache (not yet implemented)
//        BoneInfluenceType 0:normal, 1:extra bone influences, 2:unlimited bone influences
//        BoneIndex16 0: 8-bit indices, 1: 16-bit indices
//        BoneWeights16 0: 8-bit weights, 1: 16-bit weights
template <int Permutation>
class TGPUSkinCacheCS : public FBaseGPUSkinCacheCS
{
	constexpr static bool bBoneWeights16 = (32 == (Permutation & 32));
	constexpr static bool bBoneIndex16 = (16 == (Permutation & 16));
	constexpr static bool bUnlimitedBoneInfluence = (8 == (Permutation & 12));
	constexpr static bool bUseExtraBoneInfluencesT = (4 == (Permutation & 12));
	constexpr static bool bApexCloth = (2 == (Permutation & 3));
    constexpr static bool bMorphBlend = (1 == (Permutation & 3));

	DECLARE_SHADER_TYPE(TGPUSkinCacheCS, Global)
public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsGPUSkinCacheAvailable(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("GPUSKIN_UNLIMITED_BONE_INFLUENCE"), bUnlimitedBoneInfluence);
		OutEnvironment.SetDefine(TEXT("GPUSKIN_USE_EXTRA_INFLUENCES"), bUseExtraBoneInfluencesT);
		OutEnvironment.SetDefine(TEXT("GPUSKIN_MORPH_BLEND"), bMorphBlend);
		OutEnvironment.SetDefine(TEXT("GPUSKIN_APEX_CLOTH"), bApexCloth);
		OutEnvironment.SetDefine(TEXT("GPUSKIN_RWBUFFER_OFFSET_TANGENT_X"), FGPUSkinCache::RWTangentXOffsetInFloats);
		OutEnvironment.SetDefine(TEXT("GPUSKIN_RWBUFFER_OFFSET_TANGENT_Z"), FGPUSkinCache::RWTangentZOffsetInFloats);
		OutEnvironment.SetDefine(TEXT("GPUSKIN_BONE_INDEX_UINT16"), bBoneIndex16);
		OutEnvironment.SetDefine(TEXT("GPUSKIN_BONE_WEIGHTS_UINT16"), bBoneWeights16);

		if (FDataDrivenShaderPlatformInfo::GetSupportsRealTypes(Parameters.Platform) == ERHIFeatureSupport::RuntimeGuaranteed)
		{
			OutEnvironment.CompilerFlags.Add(CFLAG_AllowRealTypes);
		}
	}

	TGPUSkinCacheCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FBaseGPUSkinCacheCS(Initializer)
	{
	}

	TGPUSkinCacheCS()
	{
	}
};

#define SKIN_CACHE_SHADER_IDX(_WEIGHT16_, _INDEX16_, _INFLUENCE_TYPE_, _SKIN_TYPE_) (_WEIGHT16_ * 32 + _INDEX16_ * 16 + _INFLUENCE_TYPE_ * 4 + _SKIN_TYPE_)

#define SKIN_CACHE_SHADER_ALL_SKIN_TYPES(_WEIGHT16_, _INDEX16_, _INFLUENCE_TYPE_) \
	SKIN_CACHE_SHADER(SKIN_CACHE_SHADER_IDX(_WEIGHT16_, _INDEX16_, _INFLUENCE_TYPE_, 0)); \
	SKIN_CACHE_SHADER(SKIN_CACHE_SHADER_IDX(_WEIGHT16_, _INDEX16_, _INFLUENCE_TYPE_, 1)); \
	SKIN_CACHE_SHADER(SKIN_CACHE_SHADER_IDX(_WEIGHT16_, _INDEX16_, _INFLUENCE_TYPE_, 2))

// NOTE: Bone influence type 2 (multiple) does not require a 16-bit index or weight permutation.
#define SKIN_CACHE_SHADER_ALL() \
	SKIN_CACHE_SHADER_ALL_SKIN_TYPES(0, 0, 0); \
	SKIN_CACHE_SHADER_ALL_SKIN_TYPES(0, 0, 1); \
	SKIN_CACHE_SHADER_ALL_SKIN_TYPES(0, 1, 0); \
	SKIN_CACHE_SHADER_ALL_SKIN_TYPES(0, 1, 1); \
	SKIN_CACHE_SHADER_ALL_SKIN_TYPES(1, 0, 0); \
	SKIN_CACHE_SHADER_ALL_SKIN_TYPES(1, 0, 1); \
	SKIN_CACHE_SHADER_ALL_SKIN_TYPES(1, 1, 0); \
	SKIN_CACHE_SHADER_ALL_SKIN_TYPES(1, 1, 1); \
	SKIN_CACHE_SHADER_ALL_SKIN_TYPES(0, 0, 2);

#define SKIN_CACHE_SHADER(_SHADER_IDX_) \
	IMPLEMENT_SHADER_TYPE(template<>, TGPUSkinCacheCS<_SHADER_IDX_>, TEXT("/Engine/Private/GpuSkinCacheComputeShader.usf"), TEXT("SkinCacheUpdateBatchCS"), SF_Compute) 
SKIN_CACHE_SHADER_ALL()
#undef SKIN_CACHE_SHADER

FGPUSkinCache::FGPUSkinCache(ERHIFeatureLevel::Type InFeatureLevel, bool bInRequiresMemoryLimit, UWorld* InWorld)
	: UsedMemoryInBytes(0)
	, ExtraRequiredMemory(0)
	, FlushCounter(0)
	, bRequiresMemoryLimit(bInRequiresMemoryLimit)
	, CurrentStagingBufferIndex(0)
	, FeatureLevel(InFeatureLevel)
	, World(InWorld)
{
	check(World);

	if (GSkinCacheRecomputeTangents == 1 && GStoreDuplicatedVerticesForRecomputeTangents == 0)
	{
		UE_LOG(LogSkinCache, Warning, TEXT("r.SkinCache.RecomputeTangents is set to 1 to update all skinned objects but duplicated vertices are not are not always stored. Set r.SkinCache.RecomputeTangents to 2 or r.SkinCache.StoreDuplicatedVerticesForRecomputeTangents to 1."));
	}
}

FGPUSkinCache::~FGPUSkinCache()
{
	Cleanup();
}

void FGPUSkinCache::Cleanup()
{
	for (int32 Index = 0; Index < StagingBuffers.Num(); ++Index)
	{
		StagingBuffers[Index].Release();
	}

	while (Entries.Num() > 0)
	{
		Release(Entries.Last());
	}
	ensure(Allocations.Num() == 0);
}

/** base of the FRecomputeTangentsPerTrianglePassCS class */
class FBaseRecomputeTangentsPerTriangleShader : public FGlobalShader
{
	DECLARE_INLINE_TYPE_LAYOUT(FBaseRecomputeTangentsPerTriangleShader, NonVirtual);
public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		// currently only implemented and tested on Window SM5 (needs Compute, Atomics, SRV for index buffers, UAV for VertexBuffers)
		return IsGPUSkinCacheAvailable(Parameters.Platform);
	}

	static const uint32 ThreadGroupSizeX = 64;

	FBaseRecomputeTangentsPerTriangleShader()
	{}

	FBaseRecomputeTangentsPerTriangleShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		IntermediateAccumBufferUAV.Bind(Initializer.ParameterMap, TEXT("IntermediateAccumBufferUAV"));
		IntermediateAccumBufferOffset.Bind(Initializer.ParameterMap, TEXT("IntermediateAccumBufferOffset"));
		NumTriangles.Bind(Initializer.ParameterMap, TEXT("NumTriangles"));
		GPUPositionCacheBuffer.Bind(Initializer.ParameterMap, TEXT("GPUPositionCacheBuffer"));
		GPUTangentCacheBuffer.Bind(Initializer.ParameterMap, TEXT("GPUTangentCacheBuffer"));
		SkinCacheStart.Bind(Initializer.ParameterMap, TEXT("SkinCacheStart"));
		IndexBuffer.Bind(Initializer.ParameterMap, TEXT("IndexBuffer"));
		IndexBufferOffset.Bind(Initializer.ParameterMap, TEXT("IndexBufferOffset"));

		InputStreamStart.Bind(Initializer.ParameterMap, TEXT("InputStreamStart"));
		NumTexCoords.Bind(Initializer.ParameterMap, TEXT("NumTexCoords"));
		SelectedTexCoord.Bind(Initializer.ParameterMap, TEXT("SelectedTexCoord"));
		TangentInputBuffer.Bind(Initializer.ParameterMap, TEXT("TangentInputBuffer"));
		UVsInputBuffer.Bind(Initializer.ParameterMap, TEXT("UVsInputBuffer"));

        DuplicatedIndices.Bind(Initializer.ParameterMap, TEXT("DuplicatedIndices"));
        DuplicatedIndicesIndices.Bind(Initializer.ParameterMap, TEXT("DuplicatedIndicesIndices"));
	}

	void SetParameters(FRHIBatchedShaderParameters& BatchedParameters, FGPUSkinCacheEntry* Entry, const FGPUSkinCacheEntry::FSectionDispatchData& DispatchData, const FRWBuffer& StagingBuffer)
	{
//later		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View);

		SetShaderValue(BatchedParameters, NumTriangles, DispatchData.NumTriangles);

		SetSRVParameter(BatchedParameters, GPUPositionCacheBuffer, DispatchData.GetPositionRWBuffer()->Buffer.SRV);
		SetSRVParameter(BatchedParameters, GPUTangentCacheBuffer, DispatchData.GetActiveTangentRWBuffer()->Buffer.SRV);
		SetSRVParameter(BatchedParameters, UVsInputBuffer, DispatchData.UVsBufferSRV);

		SetShaderValue(BatchedParameters, SkinCacheStart, DispatchData.OutputStreamStart);

		SetSRVParameter(BatchedParameters, IndexBuffer, DispatchData.IndexBuffer);
		SetShaderValue(BatchedParameters, IndexBufferOffset, DispatchData.IndexBufferOffsetValue);
		
		SetShaderValue(BatchedParameters, InputStreamStart, DispatchData.InputStreamStart);
		SetShaderValue(BatchedParameters, NumTexCoords, DispatchData.NumTexCoords);
		SetShaderValue(BatchedParameters, SelectedTexCoord, DispatchData.SelectedTexCoord);
		SetSRVParameter(BatchedParameters, TangentInputBuffer, DispatchData.TangentBufferSRV);
		SetSRVParameter(BatchedParameters, TangentInputBuffer, DispatchData.UVsBufferSRV);

		// UAV
		SetUAVParameter(BatchedParameters, IntermediateAccumBufferUAV, StagingBuffer.UAV);
		SetShaderValue(BatchedParameters, IntermediateAccumBufferOffset, GRecomputeTangentsParallelDispatch * DispatchData.IntermediateAccumulatedTangentBufferOffset);

        if (DispatchData.DuplicatedIndices)
        {
		    SetSRVParameter(BatchedParameters, DuplicatedIndices, DispatchData.DuplicatedIndices);
            SetSRVParameter(BatchedParameters, DuplicatedIndicesIndices, DispatchData.DuplicatedIndicesIndices);
        }
	}

	void UnsetParameters(FRHIBatchedShaderUnbinds& BatchedUnbinds)
	{
		UnsetUAVParameter(BatchedUnbinds, IntermediateAccumBufferUAV);
	}

	LAYOUT_FIELD(FShaderResourceParameter, IntermediateAccumBufferUAV);
	LAYOUT_FIELD(FShaderParameter, IntermediateAccumBufferOffset);
	LAYOUT_FIELD(FShaderParameter, NumTriangles);
	LAYOUT_FIELD(FShaderResourceParameter, GPUPositionCacheBuffer);
	LAYOUT_FIELD(FShaderResourceParameter, GPUTangentCacheBuffer);
	LAYOUT_FIELD(FShaderParameter, SkinCacheStart);
	LAYOUT_FIELD(FShaderResourceParameter, IndexBuffer);
	LAYOUT_FIELD(FShaderParameter, IndexBufferOffset);
	LAYOUT_FIELD(FShaderParameter, InputStreamStart);
	LAYOUT_FIELD(FShaderParameter, NumTexCoords);
	LAYOUT_FIELD(FShaderParameter, SelectedTexCoord);
	LAYOUT_FIELD(FShaderResourceParameter, TangentInputBuffer);
	LAYOUT_FIELD(FShaderResourceParameter, UVsInputBuffer);
	LAYOUT_FIELD(FShaderResourceParameter, DuplicatedIndices);
	LAYOUT_FIELD(FShaderResourceParameter, DuplicatedIndicesIndices);
};

/** Encapsulates the RecomputeSkinTangents compute shader. */
template <int Permutation>
class FRecomputeTangentsPerTrianglePassCS : public FBaseRecomputeTangentsPerTriangleShader
{
    constexpr static bool bMergeDuplicatedVerts = (2 == (Permutation & 2));
	constexpr static bool bFullPrecisionUV = (1 == (Permutation & 1));

	DECLARE_SHADER_TYPE(FRecomputeTangentsPerTrianglePassCS, Global);

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("MERGE_DUPLICATED_VERTICES"), bMergeDuplicatedVerts);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), ThreadGroupSizeX);
		OutEnvironment.SetDefine(TEXT("INTERMEDIATE_ACCUM_BUFFER_NUM_INTS"), FGPUSkinCache::IntermediateAccumBufferNumInts);
		OutEnvironment.SetDefine(TEXT("FULL_PRECISION_UV"), bFullPrecisionUV);
	}

	FRecomputeTangentsPerTrianglePassCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FBaseRecomputeTangentsPerTriangleShader(Initializer)
	{
	}

	FRecomputeTangentsPerTrianglePassCS()
	{}
};

IMPLEMENT_SHADER_TYPE(template<>, FRecomputeTangentsPerTrianglePassCS<0>, TEXT("/Engine/Private/RecomputeTangentsPerTrianglePass.usf"), TEXT("MainCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FRecomputeTangentsPerTrianglePassCS<1>, TEXT("/Engine/Private/RecomputeTangentsPerTrianglePass.usf"), TEXT("MainCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FRecomputeTangentsPerTrianglePassCS<2>, TEXT("/Engine/Private/RecomputeTangentsPerTrianglePass.usf"), TEXT("MainCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FRecomputeTangentsPerTrianglePassCS<3>, TEXT("/Engine/Private/RecomputeTangentsPerTrianglePass.usf"), TEXT("MainCS"), SF_Compute);

/** Encapsulates the RecomputeSkinTangentsResolve compute shader. */
class FBaseRecomputeTangentsPerVertexShader : public FGlobalShader
{
	DECLARE_INLINE_TYPE_LAYOUT(FBaseRecomputeTangentsPerVertexShader, NonVirtual);

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		// currently only implemented and tested on Window SM5 (needs Compute, Atomics, SRV for index buffers, UAV for VertexBuffers)
		return IsGPUSkinCacheAvailable(Parameters.Platform);
	}

	static const uint32 ThreadGroupSizeX = 64;

	LAYOUT_FIELD(FShaderResourceParameter, IntermediateAccumBufferUAV);
	LAYOUT_FIELD(FShaderParameter, IntermediateAccumBufferOffset);
	LAYOUT_FIELD(FShaderResourceParameter, TangentBufferUAV);
	LAYOUT_FIELD(FShaderResourceParameter, TangentInputBuffer);
	LAYOUT_FIELD(FShaderResourceParameter, ColorInputBuffer);
	LAYOUT_FIELD(FShaderParameter, SkinCacheStart);
	LAYOUT_FIELD(FShaderParameter, NumVertices);
	LAYOUT_FIELD(FShaderParameter, InputStreamStart);
	LAYOUT_FIELD(FShaderParameter, VertexColorChannel); // which channel to use to read mask colors (0-R, 1-G, 2-B)

	FBaseRecomputeTangentsPerVertexShader() {}

	FBaseRecomputeTangentsPerVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		IntermediateAccumBufferUAV.Bind(Initializer.ParameterMap, TEXT("IntermediateAccumBufferUAV"));
		IntermediateAccumBufferOffset.Bind(Initializer.ParameterMap, TEXT("IntermediateAccumBufferOffset"));
		TangentBufferUAV.Bind(Initializer.ParameterMap, TEXT("TangentBufferUAV"));
		TangentInputBuffer.Bind(Initializer.ParameterMap, TEXT("TangentInputBuffer"));
		ColorInputBuffer.Bind(Initializer.ParameterMap, TEXT("ColorInputBuffer"));
		SkinCacheStart.Bind(Initializer.ParameterMap, TEXT("SkinCacheStart"));
		NumVertices.Bind(Initializer.ParameterMap, TEXT("NumVertices"));
		InputStreamStart.Bind(Initializer.ParameterMap, TEXT("InputStreamStart"));
		VertexColorChannel.Bind(Initializer.ParameterMap, TEXT("VertexColorChannel"));
	}

	void SetParameters(FRHIBatchedShaderParameters& BatchedParameters, FGPUSkinCacheEntry* Entry, const FGPUSkinCacheEntry::FSectionDispatchData& DispatchData, const FRWBuffer& StagingBuffer)
	{
		check(StagingBuffer.UAV);

		//later		FGlobalShader::SetParameters<FViewUniformShaderParameters>(BatchedParameters, View);

		SetShaderValue(BatchedParameters, SkinCacheStart, DispatchData.OutputStreamStart);
		SetShaderValue(BatchedParameters, NumVertices, DispatchData.NumVertices);
		SetShaderValue(BatchedParameters, InputStreamStart, DispatchData.InputStreamStart);
		SetShaderValue(BatchedParameters, VertexColorChannel, uint32(DispatchData.Section->RecomputeTangentsVertexMaskChannel));

		// UAVs
		SetUAVParameter(BatchedParameters, IntermediateAccumBufferUAV, StagingBuffer.UAV);
		SetShaderValue(BatchedParameters, IntermediateAccumBufferOffset, GRecomputeTangentsParallelDispatch * DispatchData.IntermediateAccumulatedTangentBufferOffset);
		SetUAVParameter(BatchedParameters, TangentBufferUAV, DispatchData.GetTangentRWBuffer()->Buffer.UAV);

		SetSRVParameter(BatchedParameters, TangentInputBuffer, DispatchData.IntermediateTangentBuffer ? DispatchData.IntermediateTangentBuffer->Buffer.SRV : nullptr);

		SetSRVParameter(BatchedParameters, ColorInputBuffer, DispatchData.ColorBufferSRV);
	}

	void UnsetParameters(FRHIBatchedShaderUnbinds& BatchedUnbinds)
	{
		UnsetUAVParameter(BatchedUnbinds, TangentBufferUAV);
		UnsetUAVParameter(BatchedUnbinds, IntermediateAccumBufferUAV);
	}
};

template <int Permutation>
class FRecomputeTangentsPerVertexPassCS : public FBaseRecomputeTangentsPerVertexShader
{
	DECLARE_SHADER_TYPE(FRecomputeTangentsPerVertexPassCS, Global);

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		// this pass cannot read the input as it doesn't have the permutation
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), ThreadGroupSizeX);
		OutEnvironment.SetDefine(TEXT("GPUSKIN_RWBUFFER_OFFSET_TANGENT_X"), FGPUSkinCache::RWTangentXOffsetInFloats);
		OutEnvironment.SetDefine(TEXT("GPUSKIN_RWBUFFER_OFFSET_TANGENT_Z"), FGPUSkinCache::RWTangentZOffsetInFloats);
		OutEnvironment.SetDefine(TEXT("INTERMEDIATE_ACCUM_BUFFER_NUM_INTS"), FGPUSkinCache::IntermediateAccumBufferNumInts);
		OutEnvironment.SetDefine(TEXT("BLEND_USING_VERTEX_COLOR"), Permutation);
	}

	FRecomputeTangentsPerVertexPassCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FBaseRecomputeTangentsPerVertexShader(Initializer)
	{
	}

	FRecomputeTangentsPerVertexPassCS()
	{}
};

IMPLEMENT_SHADER_TYPE(template<>, FRecomputeTangentsPerVertexPassCS<0>, TEXT("/Engine/Private/RecomputeTangentsPerVertexPass.usf"), TEXT("MainCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FRecomputeTangentsPerVertexPassCS<1>, TEXT("/Engine/Private/RecomputeTangentsPerVertexPass.usf"), TEXT("MainCS"), SF_Compute);

void FGPUSkinCache::DispatchUpdateSkinTangentsVertexPass(FRHICommandList& RHICmdList, FGPUSkinCacheEntry* Entry, int32 SectionIndex, FSkinCacheRWBuffer*& StagingBuffer)
{
	FGPUSkinCacheEntry::FSectionDispatchData& DispatchData = Entry->DispatchData[SectionIndex];

	SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, SkinTangents_PerVertexPass, GetEmitDrawEvents(), TEXT("%sTangentsVertex Mesh=%s, LOD=%d, Chunk=%d, InputStreamStart=%d, OutputStreamStart=%d, Vert=%d")
		, RHI_BREADCRUMB_FORCE_STRING_LITERAL(Entry->Mode == EGPUSkinCacheEntryMode::RayTracing ? TEXT("[RT]") : TEXT(""))
		, GetSkeletalMeshObjectDebugName(Entry->GPUSkin)
		, Entry->LOD
		, SectionIndex
		, DispatchData.InputStreamStart
		, DispatchData.OutputStreamStart
		, DispatchData.NumVertices
	);

	//#todo-gpuskin Feature level?
	auto* GlobalShaderMap = GetGlobalShaderMap(GetFeatureLevel());
	TShaderMapRef<FRecomputeTangentsPerVertexPassCS<0>> ComputeShader0(GlobalShaderMap);
	TShaderMapRef<FRecomputeTangentsPerVertexPassCS<1>> ComputeShader1(GlobalShaderMap);
	TShaderRef<FBaseRecomputeTangentsPerVertexShader> ComputeShader;
	if (DispatchData.Section->RecomputeTangentsVertexMaskChannel < ESkinVertexColorChannel::None)
		ComputeShader = ComputeShader1;
	else
		ComputeShader = ComputeShader0;

	uint32 VertexCount = DispatchData.NumVertices;
	uint32 ThreadGroupCountValue = FMath::DivideAndRoundUp(VertexCount, ComputeShader->ThreadGroupSizeX);

	if (!GRecomputeTangentsParallelDispatch)
	{
		// When triangle & vertex passes are interleaved, resource transition is needed in between.
		RHICmdList.Transition({
			DispatchData.GetTangentRWBuffer()->UpdateAccessState(ERHIAccess::UAVCompute),
			StagingBuffer->UpdateAccessState(ERHIAccess::UAVCompute)
		}, ERHITransitionCreateFlags::AllowDecayPipelines);
	}

	SetComputePipelineState(RHICmdList, ComputeShader.GetComputeShader());

	SetShaderParametersLegacyCS(RHICmdList, ComputeShader, Entry, DispatchData, GRecomputeTangentsParallelDispatch ? DispatchData.GetIntermediateAccumulatedTangentBuffer()->Buffer : StagingBuffer->Buffer);
	DispatchComputeShader(RHICmdList, ComputeShader.GetShader(), ThreadGroupCountValue, 1, 1);
	UnsetShaderParametersLegacyCS(RHICmdList, ComputeShader);
}

void FGPUSkinCache::DispatchUpdateSkinTangentsTrianglePass(FRHICommandList& RHICmdList, FGPUSkinCacheEntry* Entry, int32 SectionIndex, FSkinCacheRWBuffer*& StagingBuffer)
{
	FGPUSkinCacheEntry::FSectionDispatchData& DispatchData = Entry->DispatchData[SectionIndex];

	FSkeletalMeshRenderData& SkelMeshRenderData = Entry->GPUSkin->GetSkeletalMeshRenderData();
	const int32 LODIndex = Entry->LOD;
	FSkeletalMeshLODRenderData& LodData = SkelMeshRenderData.LODRenderData[LODIndex];

	if (!GRecomputeTangentsParallelDispatch)
	{
		if (StagingBuffers.Num() != GNumTangentIntermediateBuffers)
		{
			// Release extra buffers if shrinking
			for (int32 Index = GNumTangentIntermediateBuffers; Index < StagingBuffers.Num(); ++Index)
			{
				StagingBuffers[Index].Release();
			}
			StagingBuffers.SetNum(GNumTangentIntermediateBuffers, EAllowShrinking::No);
		}

		// no need to clear the staging buffer because we create it cleared and clear it after each usage in the per vertex pass
		uint32 NumIntsPerBuffer = DispatchData.NumVertices * FGPUSkinCache::IntermediateAccumBufferNumInts;
		CurrentStagingBufferIndex = (CurrentStagingBufferIndex + 1) % StagingBuffers.Num();
		StagingBuffer = &StagingBuffers[CurrentStagingBufferIndex];
		if (StagingBuffer->Buffer.NumBytes < NumIntsPerBuffer * sizeof(uint32))
		{
			StagingBuffer->Release();
			StagingBuffer->Buffer.Initialize(RHICmdList, TEXT("SkinTangentIntermediate"), sizeof(int32), NumIntsPerBuffer, PF_R32_SINT, BUF_UnorderedAccess);
			RHICmdList.BindDebugLabelName(StagingBuffer->Buffer.UAV, TEXT("SkinTangentIntermediate"));

			const uint32 MemSize = NumIntsPerBuffer * sizeof(uint32);
			SET_MEMORY_STAT(STAT_GPUSkinCache_TangentsIntermediateMemUsed, MemSize);

			// The UAV must be zero-filled. We leave it zeroed after each round (see RecomputeTangentsPerVertexPass.usf), so this is only needed on when the buffer is first created.
			RHICmdList.ClearUAVUint(StagingBuffer->Buffer.UAV, FUintVector4(0, 0, 0, 0));
		}
	}

	auto* GlobalShaderMap = GetGlobalShaderMap(GetFeatureLevel());
	TShaderMapRef<FRecomputeTangentsPerTrianglePassCS<0>> ComputeShader00(GlobalShaderMap);
	TShaderMapRef<FRecomputeTangentsPerTrianglePassCS<1>> ComputeShader01(GlobalShaderMap);
	TShaderMapRef<FRecomputeTangentsPerTrianglePassCS<2>> ComputeShader10(GlobalShaderMap);
	TShaderMapRef<FRecomputeTangentsPerTrianglePassCS<3>> ComputeShader11(GlobalShaderMap);

	bool bFullPrecisionUV = LodData.StaticVertexBuffers.StaticMeshVertexBuffer.GetUseFullPrecisionUVs();

	bool bUseDuplicatedVertices = GUseDuplicatedVerticesForRecomputeTangents && LodData.RenderSections[SectionIndex].DuplicatedVerticesBuffer.DuplicatedVerticesIndexBuffer.VertexBufferSRV;

	TShaderRef<FBaseRecomputeTangentsPerTriangleShader> Shader;

	if (bFullPrecisionUV)
	{
		if (bUseDuplicatedVertices) Shader = ComputeShader11;
		else Shader = ComputeShader01;
	}
	else
	{
		if (bUseDuplicatedVertices) Shader = ComputeShader10;
		else Shader = ComputeShader00;
	}

	check(Shader.IsValid());

	uint32 NumTriangles = DispatchData.NumTriangles;
	uint32 ThreadGroupCountValue = FMath::DivideAndRoundUp(NumTriangles, FBaseRecomputeTangentsPerTriangleShader::ThreadGroupSizeX);

	SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, SkinTangents_PerTrianglePass, GetEmitDrawEvents(), TEXT("%sTangentsTri  Mesh=%s, LOD=%d, Chunk=%d, IndexStart=%d Tri=%d BoneInfluenceType=%d UVPrecision=%d")
		, RHI_BREADCRUMB_FORCE_STRING_LITERAL(Entry->Mode == EGPUSkinCacheEntryMode::RayTracing ? TEXT("[RT]") : TEXT(""))
		, GetSkeletalMeshObjectDebugName(Entry->GPUSkin)
		, LODIndex
		, SectionIndex
		, DispatchData.IndexBufferOffsetValue
		, DispatchData.NumTriangles
		, Entry->BoneInfluenceType
		, bFullPrecisionUV
	);

	if (bUseDuplicatedVertices)
	{
#if WITH_EDITOR
		check(LodData.RenderSections[SectionIndex].DuplicatedVerticesBuffer.DupVertData.Num() && LodData.RenderSections[SectionIndex].DuplicatedVerticesBuffer.DupVertIndexData.Num());
#endif
		DispatchData.DuplicatedIndices = LodData.RenderSections[SectionIndex].DuplicatedVerticesBuffer.DuplicatedVerticesIndexBuffer.VertexBufferSRV;
		DispatchData.DuplicatedIndicesIndices = LodData.RenderSections[SectionIndex].DuplicatedVerticesBuffer.LengthAndIndexDuplicatedVerticesIndexBuffer.VertexBufferSRV;
	}

	if (!GRecomputeTangentsParallelDispatch)
	{
		// When triangle & vertex passes are interleaved, resource transition is needed in between.
		RHICmdList.Transition({
			DispatchData.GetActiveTangentRWBuffer()->UpdateAccessState(ERHIAccess::SRVCompute),
			StagingBuffer->UpdateAccessState(ERHIAccess::UAVCompute)
		}, ERHITransitionCreateFlags::AllowDecayPipelines);
	}

	INC_DWORD_STAT_BY(STAT_GPUSkinCache_NumTrianglesForRecomputeTangents, NumTriangles);

	const FRWBuffer& ShaderStagingBuffer = GRecomputeTangentsParallelDispatch ? DispatchData.GetIntermediateAccumulatedTangentBuffer()->Buffer : StagingBuffer->Buffer;

	FRHIComputeShader* ShaderRHI = Shader.GetComputeShader();
	SetComputePipelineState(RHICmdList, Shader.GetComputeShader());

	SetShaderParametersLegacyCS(RHICmdList, Shader, Entry, DispatchData, ShaderStagingBuffer);
	DispatchComputeShader(RHICmdList, Shader.GetShader(), ThreadGroupCountValue, 1, 1);
	UnsetShaderParametersLegacyCS(RHICmdList, Shader);
}

DECLARE_GPU_STAT(GPUSkinCache);

void FGPUSkinCache::TransitionBuffers(FRHICommandList& RHICmdList, TArray<FSkinCacheRWBuffer*>& Buffers, ERHIAccess ToState)
{
	if (!Buffers.IsEmpty())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(TransitionBuffers);
		const uint32 NextTransitionFence = GetNextTransitionFence();

		TArray<FRHITransitionInfo, SceneRenderingAllocator> UAVs;
		UAVs.Reserve(Buffers.Num());

		FSkinCacheRWBuffer* LastBuffer = nullptr;
		for (FSkinCacheRWBuffer* Buffer : Buffers)
		{
			if (!Buffer->UpdateFence(NextTransitionFence))
			{
				continue;
			}

			LastBuffer = Buffer;
			if (EnumHasAnyFlags(ToState, ERHIAccess::UAVMask) || Buffer->AccessState != ToState)
			{
				UAVs.Add(Buffer->UpdateAccessState(ToState));
			}
		}

		// The NoFence flag is necessary to silence the validator for transitioning from All pipes to Graphics.
		RHICmdList.Transition(MakeArrayView(UAVs.GetData(), UAVs.Num()), ERHITransitionCreateFlags::AllowDecayPipelines);
	}
}

void FGPUSkinCache::TransitionBufferUAVs(FRHICommandList& RHICmdList, const TArray<FSkinCacheRWBuffer*>& Buffers, TArray<FRHIUnorderedAccessView*>& OutUAVs)
{
	if (!Buffers.IsEmpty())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(TransitionBufferUAVs);
		const uint32 NextTransitionFence = GetNextTransitionFence();

		TArray<FRHITransitionInfo, SceneRenderingAllocator> UAVs;
		UAVs.Reserve(Buffers.Num());
		OutUAVs.Reset(Buffers.Num());

		for (FSkinCacheRWBuffer* Buffer : Buffers)
		{
			if (!Buffer->UpdateFence(NextTransitionFence))
			{
				continue;
			}

			UAVs.Add(Buffer->UpdateAccessState(ERHIAccess::UAVCompute));
			OutUAVs.Add(Buffer->Buffer.UAV);
		}

		// The NoFence flag is necessary to silence the validator for transitioning from All pipes to Graphics.
		RHICmdList.Transition(MakeArrayView(UAVs.GetData(), UAVs.Num()), ERHITransitionCreateFlags::AllowDecayPipelines);
	}
}

ERHIPipeline FGPUSkinCache::GetDispatchPipeline(FRDGBuilder& GraphBuilder)
{
	// Morph targets require the skeletal mesh updater to be able to support async compute.
	extern bool GUseSkeletalMeshUpdater;
	return GUseSkeletalMeshUpdater && GSkinCacheAsyncCompute && GraphBuilder.IsAsyncComputeEnabled() ? ERHIPipeline::AsyncCompute : ERHIPipeline::Graphics;
}

struct FGPUSkinCache::FTaskData
{
	FTaskData(FRDGBuilder& GraphBuilder, ERHIPipeline InPipeline)
		: Pipeline(InPipeline)
		, bAsyncCommandList(GraphBuilder.IsParallelSetupEnabled() && !GNumDispatchesToCapture)
	{
		if (bAsyncCommandList)
		{
			RHICmdList = new FRHICommandList;

			FRHICommandListScopedPipeline ScopedPipeline(GraphBuilder.RHICmdList, Pipeline);
			GraphBuilder.RHICmdList.QueueAsyncCommandListSubmit(RHICmdList);
		}
		else
		{
			RHICmdList = &GraphBuilder.RHICmdList;
		}
	}

	void Begin()
	{
		OriginalPipeline = RHICmdList->SwitchPipeline(Pipeline);

		RHICmdListScopedFence.Emplace(*RHICmdList);
	}

	void End()
	{
		RHICmdListScopedFence.Reset();

		if (bAsyncCommandList)
		{
			RHICmdList->FinishRecording();
		}
		else
		{
			RHICmdList->SwitchPipeline(OriginalPipeline);
		}
	}

	TArray<FDispatchEntry> TangentDispatches;
	TArray<FSortedDispatchEntry> SortedDispatches;
	TOptional<FRHICommandListScopedFence> RHICmdListScopedFence;
	FRHICommandList* RHICmdList = nullptr;

	struct
	{
		TArray<FSkinCacheRWBuffer*> FinalRead;

		struct
		{
			TArray<FSkinCacheRWBuffer*> Write;
			TArray<FRHIUnorderedAccessView*> Overlap;

		} Skinning;

		struct
		{
			TArray<FSkinCacheRWBuffer*> Write;
			TArray<FSkinCacheRWBuffer*> Read;

		} RecomputeTangents;

	} Transitions;

	UE::Tasks::FTask SetupTask;
	ERHIPipeline Pipeline = ERHIPipeline::Graphics;
	ERHIPipeline OriginalPipeline = ERHIPipeline::Graphics;
	const FRHITransition* AsyncComputeTransition = nullptr;
	bool bAsyncCommandList = false;
	bool bWaitPassAdded = false;
	bool bSignalPassAdded = false;
};

UE::Tasks::FTask FGPUSkinCache::Dispatch(FRDGBuilder& GraphBuilder, const UE::Tasks::FTask& PrerequisitesTask, ERHIPipeline InPipeline)
{
	FTaskData* TaskData = &GraphBuilder.Blackboard.Create<FTaskData>(GraphBuilder, InPipeline);

	TaskData->SetupTask = GraphBuilder.AddSetupTask([this, TaskData]
	{
		TaskData->Begin();
		DispatchPassSetup(*TaskData);

	}, PrerequisitesTask, UE::Tasks::ETaskPriority::High, TaskData->bAsyncCommandList);

	GraphBuilder.AddSetupTask([this, TaskData]
	{
		FTaskTagScope TagScope(ETaskTag::EParallelRenderingThread);
		DispatchPassExecute(*TaskData);
		TaskData->End();

	}, TaskData->SetupTask, UE::Tasks::ETaskPriority::BackgroundHigh, TaskData->bAsyncCommandList);

	if (TaskData->Pipeline == ERHIPipeline::AsyncCompute)
	{
		// Tell the builder that we will manually sync async compute work back to graphics.
		GraphBuilder.SkipInitialAsyncComputeFence();

		GraphBuilder.AddPostExecuteCallback([TaskData, &RHICmdList = GraphBuilder.RHICmdList]
		{
			checkf(TaskData->bWaitPassAdded, TEXT("FGPUSkinCache::AddAsyncComputeWait was never called!"));
		});
	}

	return TaskData->SetupTask;
}

void FGPUSkinCache::AddAsyncComputeSignal(FRDGBuilder& GraphBuilder)
{
	FTaskData* TaskData = GraphBuilder.Blackboard.GetMutable<FTaskData>();

	if (TaskData && !TaskData->bSignalPassAdded && TaskData->Pipeline == ERHIPipeline::AsyncCompute)
	{
		AddPass(GraphBuilder, RDG_EVENT_NAME("GPUSkinCache_AsyncComputeSignal"), [] (FRDGAsyncTask, FRHICommandList& RHICmdList)
		{
			RHICmdList.Transition({}, ERHIPipeline::Graphics, ERHIPipeline::AsyncCompute);
		});
		TaskData->bSignalPassAdded = true;
	}
}

void FGPUSkinCache::AddAsyncComputeWait(FRDGBuilder& GraphBuilder)
{
	FTaskData* TaskData = GraphBuilder.Blackboard.GetMutable<FTaskData>();

	if (!TaskData)
	{
		return;
	}

	if (TaskData->SetupTask.IsValid())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FGPUSkinCache::EndDispatch);
		TaskData->SetupTask.Wait();
		TaskData->SetupTask = {};
	}

	if (!TaskData->bWaitPassAdded && TaskData->Pipeline == ERHIPipeline::AsyncCompute)
	{
		AddPass(GraphBuilder, RDG_EVENT_NAME("GPUSkinCache_AsyncComputeWait"), [TaskData](FRDGAsyncTask, FRHICommandList& RHICmdList)
		{
			check(TaskData->AsyncComputeTransition);
			RHICmdList.EndTransition(TaskData->AsyncComputeTransition);
		});
		TaskData->bWaitPassAdded = true;
	}
}

void FGPUSkinCache::DispatchPassSetup(FTaskData& TaskData)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FGPUSkinCache::DispatchPassSetup);

	const int32 NumDispatches = BatchDispatches.Num();

	TaskData.TangentDispatches.Reserve(NumDispatches);
	TaskData.Transitions.FinalRead.Reserve(NumDispatches * NUM_BUFFERS);
	TaskData.Transitions.Skinning.Write.Reserve(NumDispatches * NUM_BUFFERS);
	TaskData.Transitions.RecomputeTangents.Read.Reserve(NumDispatches * 2);

	if (GRecomputeTangentsParallelDispatch)
	{
		TaskData.Transitions.RecomputeTangents.Write.Reserve(NumDispatches);
	}

	struct
	{
		int32 NumRayTracingDispatches = 0;
		int32 NumRayTracingBuffers = 0;
		int32 NumBuffers = 0;

	} Stats;

	for (const FDispatchEntry& DispatchItem : BatchDispatches)
	{
		FGPUSkinCacheEntry* Entry = DispatchItem.SkinCacheEntry;
		Entry->bQueuedForDispatch = false;

		FGPUSkinCacheEntry::FSectionDispatchData& DispatchData = Entry->DispatchData[DispatchItem.Section];
		DispatchData.DispatchFlags = EGPUSkinCacheDispatchFlags::None;
		DispatchData.PreviousPositionBuffer = nullptr;
		DispatchData.RevisionNumber = 0;

		if (DispatchData.PositionTracker.Allocation->HasPreviousBuffer())
		{
			const FGPUBaseSkinVertexFactory::FShaderDataType& ShaderData = DispatchData.SourceVertexFactory->GetShaderData();

			const FVertexBufferAndSRV& BoneBuffer = ShaderData.GetBoneBufferForReading(false);
			const uint32 CurrentRevision = ShaderData.GetRevisionNumber(false);
			DispatchData.PositionBuffer = DispatchData.PositionTracker.Find(BoneBuffer, CurrentRevision);

			const FVertexBufferAndSRV& PreviousBoneBuffer = ShaderData.GetBoneBufferForReading(true);
			const uint32 PreviousRevision = ShaderData.GetRevisionNumber(true);
			DispatchData.PreviousPositionBuffer = DispatchData.PositionTracker.Find(PreviousBoneBuffer, PreviousRevision);

			// Allocate buffers if not found, excluding buffers already in use.  Or make the current buffer distinct if it happens to equal previous.
			if (!DispatchData.PositionBuffer || DispatchData.PositionBuffer == DispatchData.PreviousPositionBuffer)
			{
				DispatchData.PositionBuffer = DispatchData.PositionTracker.AllocateUnused(BoneBuffer, CurrentRevision, DispatchData.PreviousPositionBuffer);
				DispatchData.DispatchFlags |= EGPUSkinCacheDispatchFlags::Position;

				TaskData.Transitions.Skinning.Write.Emplace(DispatchData.PositionBuffer);
				TaskData.Transitions.FinalRead.Emplace(DispatchData.PositionBuffer);
			}

			if (!DispatchData.PreviousPositionBuffer)
			{
				DispatchData.PreviousPositionBuffer = DispatchData.PositionTracker.AllocateUnused(PreviousBoneBuffer, PreviousRevision, DispatchData.PositionBuffer);
				DispatchData.DispatchFlags |= EGPUSkinCacheDispatchFlags::PositionPrevious;

				TaskData.Transitions.Skinning.Write.Emplace(DispatchData.PreviousPositionBuffer);
				TaskData.Transitions.FinalRead.Emplace(DispatchData.PreviousPositionBuffer);
			}
		}
		else
		{
			DispatchData.PositionBuffer = &DispatchData.PositionTracker.Allocation->GetPositionBuffer();
			DispatchData.DispatchFlags |= EGPUSkinCacheDispatchFlags::Position;

			TaskData.Transitions.Skinning.Write.Emplace(DispatchData.PositionBuffer);
			TaskData.Transitions.FinalRead.Emplace(DispatchData.PositionBuffer);
		}

		check(DispatchData.PreviousPositionBuffer != DispatchData.PositionBuffer);

		DispatchData.IntermediateTangentBuffer = DispatchData.PositionTracker.GetIntermediateTangentBuffer();
		DispatchData.IntermediateAccumulatedTangentBuffer = DispatchData.PositionTracker.GetIntermediateAccumulatedTangentBuffer();
		DispatchData.TangentBuffer = DispatchData.PositionTracker.GetTangentBuffer();

		if (EnumHasAnyFlags(DispatchData.DispatchFlags, EGPUSkinCacheDispatchFlags::Position))
		{
			if (DispatchData.IntermediateTangentBuffer && DispatchData.IntermediateAccumulatedTangentBufferOffset != INDEX_NONE)
			{
				DispatchData.DispatchFlags |= EGPUSkinCacheDispatchFlags::RecomputeTangents;

				TaskData.Transitions.Skinning.Write.Emplace(DispatchData.IntermediateTangentBuffer);
				TaskData.Transitions.RecomputeTangents.Read.Emplace(DispatchData.IntermediateTangentBuffer);
				TaskData.Transitions.RecomputeTangents.Read.Emplace(DispatchData.PositionBuffer);

				if (GRecomputeTangentsParallelDispatch)
				{
					TaskData.Transitions.RecomputeTangents.Write.Add(DispatchData.GetIntermediateAccumulatedTangentBuffer());
				}

				TaskData.TangentDispatches.Emplace(DispatchItem);
			}

			TaskData.Transitions.Skinning.Write.Emplace(DispatchData.TangentBuffer);
			TaskData.Transitions.FinalRead.Emplace(DispatchData.TangentBuffer);
		}

		int32 NumBuffers = 0;
		NumBuffers += DispatchData.PositionBuffer ? 1 : 0;
		NumBuffers += DispatchData.PreviousPositionBuffer ? 1 : 0;
		NumBuffers += DispatchData.TangentBuffer ? 1 : 0;
		NumBuffers += DispatchData.IntermediateTangentBuffer ? 1 : 0;
		NumBuffers += DispatchData.IntermediateAccumulatedTangentBuffer ? 1 : 0;

		Stats.NumBuffers += NumBuffers;
		Stats.NumRayTracingBuffers    += Entry->Mode == EGPUSkinCacheEntryMode::RayTracing ? NumBuffers : 0;
		Stats.NumRayTracingDispatches += Entry->Mode == EGPUSkinCacheEntryMode::RayTracing ? 1 : 0;
	}

	SET_DWORD_STAT(STAT_GPUSkinCache_NumBuffers, Stats.NumBuffers);
	SET_DWORD_STAT(STAT_GPUSkinCache_NumRayTracingBuffers, Stats.NumRayTracingBuffers);
	SET_DWORD_STAT(STAT_GPUSkinCache_NumRayTracingDispatches, Stats.NumRayTracingDispatches);
	SET_DWORD_STAT(STAT_GPUSkinCache_NumDispatches, BatchDispatches.Num());

#if RHI_RAYTRACING
	if (IsGPUSkinCacheRayTracingSupported() && IsRayTracingEnabled())
	{
		for (FGPUSkinCacheEntry* SkinCacheEntry : PendingProcessRTGeometryEntries)
		{
			ProcessRayTracingGeometryToUpdate(*TaskData.RHICmdList, SkinCacheEntry);
		}
	}

	PendingProcessRTGeometryEntries.Reset();
#endif
}

void FGPUSkinCache::DispatchPassExecute(FTaskData& TaskData)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FGPUSkinCache::DispatchPassExecute);

	const int32 BatchCount = BatchDispatches.Num();
	INC_DWORD_STAT_BY(STAT_GPUSkinCache_TotalNumChunks, BatchCount);
	FRHICommandList& RHICmdList = *TaskData.RHICmdList;

	bool bCapture = BatchCount > 0 && GNumDispatchesToCapture > 0;
	RenderCaptureInterface::FScopedCapture RenderCapture(bCapture, &RHICmdList);
	GNumDispatchesToCapture -= bCapture ? 1 : 0;
	TaskData.SortedDispatches.Reserve(BatchCount);

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(BuildSortedDispatchList);
		int32 NumRayTracingDispatches = 0;

		for (int32 BatchIndex = 0; BatchIndex < BatchCount; ++BatchIndex)
		{
			FDispatchEntry& DispatchItem = BatchDispatches[BatchIndex];
			FGPUSkinCacheEntry* Entry = DispatchItem.SkinCacheEntry;
			int32 Section = DispatchItem.Section;
			FGPUSkinCacheEntry::FSectionDispatchData& DispatchData = Entry->DispatchData[Section];

			if (EnumHasAnyFlags(DispatchData.DispatchFlags, EGPUSkinCacheDispatchFlags::PositionPrevious | EGPUSkinCacheDispatchFlags::Position))
			{
				// For 'unlimited' bone indexes, we pass in the index and weight sizes via a shader parameter and so we
				// can re-use the same shader permutation as for 8-bit indexes.
				bool bUse16BitBoneIndex = Entry->bUse16BitBoneIndex;
				bool bUse16BitBoneWeight = Entry->bUse16BitBoneWeight;
				if (Entry->BoneInfluenceType == 2)
				{
					bUse16BitBoneIndex = bUse16BitBoneWeight = false;
				}

				FSortedDispatchEntry SortedEntry;

				SortedEntry.ShaderIndex = SKIN_CACHE_SHADER_IDX(
					static_cast<int32>(bUse16BitBoneWeight),
					static_cast<int32>(bUse16BitBoneIndex),
					Entry->BoneInfluenceType,
					DispatchData.SkinType);
				SortedEntry.BatchIndex = BatchIndex;

				TaskData.SortedDispatches.Add(SortedEntry);
			}
		}

		Algo::Sort(TaskData.SortedDispatches, [](const FSortedDispatchEntry& A, const FSortedDispatchEntry& B)
		{
			if (A.ShaderIndex != B.ShaderIndex)
			{
				return A.ShaderIndex < B.ShaderIndex;
			}
			return A.BatchIndex < B.BatchIndex;
		});
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UpdateVertexFactoryDeclarations);

		for (FDispatchEntry& DispatchItem : BatchDispatches)
		{
			DispatchItem.SkinCacheEntry->UpdateVertexFactoryDeclaration(*TaskData.RHICmdList, DispatchItem.Section);
		}
	}

	TransitionBufferUAVs(*TaskData.RHICmdList, TaskData.Transitions.Skinning.Write, TaskData.Transitions.Skinning.Overlap);
	RHICmdList.BeginUAVOverlap(TaskData.Transitions.Skinning.Overlap);

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(GPUSkinCache_UpdateSkinningBatches);
		SCOPED_DRAW_EVENT(RHICmdList, GPUSkinCache_UpdateSkinningBatches);

		auto* GlobalShaderMap = GetGlobalShaderMap(GetFeatureLevel());

		int32 LastShaderIndex = -1;
		TShaderRef<FBaseGPUSkinCacheCS> Shader;

		TShaderRef<FBaseGPUSkinCacheCS> AllShaders[64];
#define SKIN_CACHE_SHADER(_SHADER_IDX_) static_assert(_SHADER_IDX_ < 64); AllShaders[_SHADER_IDX_] = TShaderMapRef<TGPUSkinCacheCS<_SHADER_IDX_>>(GlobalShaderMap);
		SKIN_CACHE_SHADER_ALL();
#undef SKIN_CACHE_SHADER

		int32 SortedCount = TaskData.SortedDispatches.Num();
		for (const FSortedDispatchEntry& SortedEntry : TaskData.SortedDispatches)
		{
			if (SortedEntry.ShaderIndex != LastShaderIndex)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(ChangeShader);

				if (Shader.IsValid())
				{
					UnsetShaderParametersLegacyCS(RHICmdList, Shader);
				}

				LastShaderIndex = SortedEntry.ShaderIndex;
				Shader = AllShaders[SortedEntry.ShaderIndex];

				check(Shader.IsValid());

				SetComputePipelineState(RHICmdList, Shader.GetComputeShader());
			}

			// This is pulled from FGPUSkinCache::DispatchUpdateSkinning() below, but inlined so we can set the
			// shader only when it changes. Not sure if it's worth pulling out the common bits into a shared function.

			FDispatchEntry& DispatchEntry = BatchDispatches[SortedEntry.BatchIndex];
			FGPUSkinCacheEntry* Entry = DispatchEntry.SkinCacheEntry;
			FGPUSkinCacheEntry::FSectionDispatchData& DispatchData = Entry->DispatchData[DispatchEntry.Section];
			FGPUBaseSkinVertexFactory::FShaderDataType& ShaderData = DispatchData.SourceVertexFactory->GetShaderData();

			SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, SkinCacheDispatch, GetEmitDrawEvents(), TEXT("%sSkinning%d%d%d%d Mesh=%s LOD=%d Chunk=%d InStreamStart=%d OutStart=%d Vert=%d Morph=%d/%d")
				, RHI_BREADCRUMB_FORCE_STRING_LITERAL(Entry->Mode == EGPUSkinCacheEntryMode::RayTracing ? TEXT("[RT]") : TEXT(""))
				, (int32)Entry->bUse16BitBoneIndex
				, (int32)Entry->bUse16BitBoneWeight
				, (int32)Entry->BoneInfluenceType
				, DispatchData.SkinType
				, GetSkeletalMeshObjectDebugName(Entry->GPUSkin)
				, Entry->LOD
				, DispatchData.SectionIndex
				, DispatchData.InputStreamStart
				, DispatchData.OutputStreamStart
				, DispatchData.NumVertices
				, Entry->MorphBuffer != 0
				, DispatchData.MorphBufferOffset
			);

			uint32 VertexCountAlign64 = FMath::DivideAndRoundUp(DispatchData.NumVertices, (uint32)64);

			FSkinCacheRWBuffer* TangentBuffer = DispatchData.GetActiveTangentRWBuffer();
			check(TangentBuffer);

			if (EnumHasAnyFlags(DispatchData.DispatchFlags, EGPUSkinCacheDispatchFlags::PositionPrevious))
			{
				const FVertexBufferAndSRV& PrevBoneBuffer = ShaderData.GetBoneBufferForReading(true);

				SetShaderParametersLegacyCS(
					RHICmdList,
					Shader,
					PrevBoneBuffer,
					Entry,
					DispatchData,
					DispatchData.GetPreviousPositionRWBuffer()->Buffer.UAV,
					TangentBuffer->Buffer.UAV
				);

				INC_DWORD_STAT_BY(STAT_GPUSkinCache_TotalNumVertices, VertexCountAlign64 * 64);
				RHICmdList.DispatchComputeShader(VertexCountAlign64, 1, 1);
			}

			if (EnumHasAnyFlags(DispatchData.DispatchFlags, EGPUSkinCacheDispatchFlags::Position))
			{
				const FVertexBufferAndSRV& BoneBuffer = ShaderData.GetBoneBufferForReading(false);

				SetShaderParametersLegacyCS(
					RHICmdList,
					Shader,
					BoneBuffer,
					Entry,
					DispatchData,
					DispatchData.GetPositionRWBuffer()->Buffer.UAV,
					TangentBuffer->Buffer.UAV
				);

				INC_DWORD_STAT_BY(STAT_GPUSkinCache_TotalNumVertices, VertexCountAlign64 * 64);
				RHICmdList.DispatchComputeShader(VertexCountAlign64, 1, 1);
			}

			check(DispatchData.PreviousPositionBuffer != DispatchData.PositionBuffer);
		}

		if (Shader.IsValid())
		{
			UnsetShaderParametersLegacyCS(RHICmdList, Shader);
		}
	}

	RHICmdList.EndUAVOverlap(TaskData.Transitions.Skinning.Overlap);

	if (!TaskData.Transitions.RecomputeTangents.Read.IsEmpty())
	{
		TArray<FRHIUnorderedAccessView*> IntermediateAccumulatedTangentBuffersToOverlap;
		TransitionBuffers(RHICmdList, TaskData.Transitions.RecomputeTangents.Read, ERHIAccess::SRVCompute);
		TransitionBufferUAVs(RHICmdList, TaskData.Transitions.RecomputeTangents.Write, IntermediateAccumulatedTangentBuffersToOverlap);
		RHICmdList.BeginUAVOverlap(IntermediateAccumulatedTangentBuffersToOverlap);

		{
			SCOPED_DRAW_EVENT(RHICmdList, GPUSkinCache_RecomputeTangentsBatches);
			FSkinCacheRWBuffer* StagingBuffer = nullptr;
			TArray<FSkinCacheRWBuffer*> TangentBuffers;

			if (GRecomputeTangentsParallelDispatch)
			{
				TangentBuffers.Reserve(TaskData.TangentDispatches.Num());
			}

			for (const FDispatchEntry& DispatchItem : TaskData.TangentDispatches)
			{
				DispatchUpdateSkinTangentsTrianglePass(RHICmdList, DispatchItem.SkinCacheEntry, DispatchItem.Section, StagingBuffer);

				if (GRecomputeTangentsParallelDispatch)
				{
					TangentBuffers.Add(DispatchItem.SkinCacheEntry->DispatchData[DispatchItem.Section].GetTangentRWBuffer());
				}
				else
				{
					DispatchUpdateSkinTangentsVertexPass(RHICmdList, DispatchItem.SkinCacheEntry, DispatchItem.Section, StagingBuffer);
				}
			}

			if (GRecomputeTangentsParallelDispatch)
			{
				TArray<FRHIUnorderedAccessView*> TangentBuffersToOverlap;
				TransitionBuffers(RHICmdList, TaskData.Transitions.RecomputeTangents.Write, ERHIAccess::UAVCompute);
				TransitionBufferUAVs(RHICmdList, TangentBuffers, TangentBuffersToOverlap);
				RHICmdList.BeginUAVOverlap(TangentBuffersToOverlap);

				for (const FDispatchEntry& DispatchItem : TaskData.TangentDispatches)
				{
					DispatchUpdateSkinTangentsVertexPass(RHICmdList, DispatchItem.SkinCacheEntry, DispatchItem.Section, StagingBuffer);
				}

				RHICmdList.EndUAVOverlap(TangentBuffersToOverlap);
			}
		}

		RHICmdList.EndUAVOverlap(IntermediateAccumulatedTangentBuffersToOverlap);
	}

	TArray<FRHITransitionInfo, SceneRenderingAllocator> TransitionInfos;

	if (!TaskData.Transitions.FinalRead.IsEmpty())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(TransitionAllToReadable);
		const uint32 NextTransitionFence = GetNextTransitionFence();
		const ERHIAccess ReadState = ERHIAccess::VertexOrIndexBuffer | ERHIAccess::SRVMask;

		TransitionInfos.Reserve(TaskData.Transitions.FinalRead.Num());
		for (FSkinCacheRWBuffer* Buffer : TaskData.Transitions.FinalRead)
		{
			if (!Buffer->UpdateFence(NextTransitionFence))
			{
				continue;
			}

			if (Buffer->AccessState != ReadState)
			{
				TransitionInfos.Add(Buffer->UpdateAccessState(ReadState));
			}
		}

		if (TaskData.Pipeline == ERHIPipeline::Graphics)
		{
			RHICmdList.Transition(TransitionInfos, ERHIPipeline::Graphics, IsGPUSkinCacheRayTracingSupported() && !FRDGBuilder::IsImmediateMode() ? ERHIPipeline::All : ERHIPipeline::Graphics);
		}
	}

	// When async compute is enabled the transition also acts as the fence back to the graphics pipe.
	if (TaskData.Pipeline == ERHIPipeline::AsyncCompute)
	{
		TaskData.AsyncComputeTransition = RHICreateTransition({ ERHIPipeline::AsyncCompute, ERHIPipeline::All, ERHITransitionCreateFlags::None, TransitionInfos });
		RHICmdList.BeginTransition(TaskData.AsyncComputeTransition);
		RHICmdList.EndTransition(TaskData.AsyncComputeTransition);
		RHICmdList.SetTrackedAccess(TransitionInfos, ERHIPipeline::All);
	}

	BatchDispatches.Reset();
}

bool FGPUSkinCache::ProcessEntry(
	EGPUSkinCacheEntryMode Mode,
	FRHICommandList& RHICmdList, 
	FGPUBaseSkinVertexFactory* VertexFactory,
	FGPUSkinPassthroughVertexFactory* TargetVertexFactory, 
	const FSkelMeshRenderSection& BatchElement, 
	FSkeletalMeshObject* Skin,
	const FMorphVertexBuffer* MorphVertexBuffer,
	const FSkeletalMeshVertexClothBuffer* ClothVertexBuffer, 
	const FClothSimulData* SimData,
	const FMatrix44f& ClothToLocal,
	float ClothBlendWeight,
	FVector3f WorldScale,
	uint32 RevisionNumber, 
	int32 Section,
	int32 LODIndex,
	bool& bRecreating,
	FGPUSkinCacheEntry*& InOutEntry
	)
{
	INC_DWORD_STAT(STAT_GPUSkinCache_NumSectionsProcessed);

	const int32 NumVertices = BatchElement.GetNumVertices();
	const uint32 InputStreamStart = BatchElement.BaseVertexIndex;

	FSkeletalMeshRenderData& SkelMeshRenderData = Skin->GetSkeletalMeshRenderData();
	FSkeletalMeshLODRenderData& LodData = SkelMeshRenderData.LODRenderData[LODIndex];

	if (FlushCounter < GGPUSkinCacheFlushCounter)
	{
		FlushCounter = GGPUSkinCacheFlushCounter;
		InvalidateAllEntries();
	}

	EGPUSkinCacheBufferBits BufferBits = EGPUSkinCacheBufferBits::None;

	// IntermediateAccumulatedTangents buffer is needed if mesh has at least one section needing recomputing tangents.
	uint32 IntermediateAccumulatedTangentBufferSize = 0;
	uint32 IntermediateAccumulatedTangentBufferOffset = 0;

	if (Mode == EGPUSkinCacheEntryMode::Raster)
	{
		if (GSkinCacheRecomputeTangents > 0)
		{
			for (int32 Index = 0; Index < LodData.RenderSections.Num(); ++Index)
			{
				const FSkelMeshRenderSection& RenderSection = LodData.RenderSections[Index];
				if (GSkinCacheRecomputeTangents == 1 || RenderSection.bRecomputeTangent)
				{
					if (Index < Section)
					{
						IntermediateAccumulatedTangentBufferOffset += RenderSection.GetNumVertices();
					}

					if (RenderSection.RecomputeTangentsVertexMaskChannel < ESkinVertexColorChannel::None && LodData.MultiSizeIndexContainer.GetIndexBuffer())
					{
						BufferBits |= EGPUSkinCacheBufferBits::IntermediateTangents;
					}

					IntermediateAccumulatedTangentBufferSize += RenderSection.GetNumVertices();
				}
			}
		}

		BufferBits |= EGPUSkinCacheBufferBits::PositionPrevious;
	}

	// Don't allocate the accumulated tangents buffer if none of the sections are using it.
	if (!EnumHasAnyFlags(BufferBits, EGPUSkinCacheBufferBits::IntermediateTangents))
	{
		IntermediateAccumulatedTangentBufferSize = 0;
	}

	if (InOutEntry)
	{
		// If the LOD changed, the entry has to be invalidated
		if (!InOutEntry->IsValid(Skin, LODIndex))
		{
			Release(InOutEntry);
			InOutEntry = nullptr;
		}
		else
		{
			if (!InOutEntry->IsSectionValid(Section) || !InOutEntry->IsSourceFactoryValid(Section, VertexFactory))
			{
				// This section might not be valid yet, so set it up
				InOutEntry->SetupSection(
					Section,
					InOutEntry->PositionAllocation,
					&LodData.RenderSections[Section],
					MorphVertexBuffer,
					ClothVertexBuffer,
					NumVertices,
					InputStreamStart,
					VertexFactory,
					TargetVertexFactory,
					IntermediateAccumulatedTangentBufferOffset,
					SimData);
			}
		}
	}

	// Try to allocate a new entry
	if (!InOutEntry)
	{
		// If something caused the existing entry to be invalid, disable recreate logic for the rest of the function
		bRecreating = false;

		FRWBuffersAllocationInitializer Initializer
		{
			  .BufferBits = BufferBits
			, .NumVertices = VertexFactory->GetNumVertices()
			, .IntermediateAccumulatedTangentsSize = IntermediateAccumulatedTangentBufferSize
		};

		// OpenGL ES does not support writing to RGBA16_SNORM images, so use the packed format instead.
		if (IsOpenGLPlatform(GMaxRHIShaderPlatform))
		{
			Initializer.TangentFormat = PF_R16G16B16A16_SINT;
		}
		else if (GPixelFormats[TargetVertexFactory->GetTangentFormat()].BlockBytes == 4)
		{
			Initializer.TangentFormat = PF_R8G8B8A8_SNORM;
		}
		else
		{
			Initializer.TangentFormat = PF_R16G16B16A16_SNORM;
		}

		const uint32 BufferSize = Initializer.GetBufferSize();
		const uint32 MaxSizeInBytes = (GSkinCacheSceneMemoryLimitInMB * 1024.0f * 1024.0f);

		if (bRequiresMemoryLimit && UsedMemoryInBytes + BufferSize >= MaxSizeInBytes)
		{
			ExtraRequiredMemory += BufferSize;

			if (GSkinCachePrintMemorySummary > 0)
			{
				UE_LOG(LogSkinCache, Warning, TEXT("FGPUSkinCache::ProcessEntry%s failed to allocate %.3fMB for mesh %s LOD%d, extra required memory increased to %.3fMB")
					, Mode == EGPUSkinCacheEntryMode::RayTracing ? TEXT("[RT]") : TEXT("")
					, BufferSize / MBSize
					, *GetSkeletalMeshObjectName(Skin)
					, LODIndex
					, ExtraRequiredMemory / MBSize
				);
			}

			// Couldn't fit; caller will notify OOM
			return false;
		}

		FRWBuffersAllocation* BufferAllocation = new FRWBuffersAllocation(RHICmdList, Initializer, Skin->GetAssetPathName(LODIndex));
		Allocations.Add(BufferAllocation);

		UsedMemoryInBytes += BufferSize;
		INC_MEMORY_STAT_BY(STAT_GPUSkinCache_TotalMemUsed, BufferSize);

		InOutEntry = new FGPUSkinCacheEntry(this, Skin, BufferAllocation, LODIndex, Mode);
		InOutEntry->GPUSkin = Skin;

		InOutEntry->SetupSection(
			Section,
			BufferAllocation,
			&LodData.RenderSections[Section],
			MorphVertexBuffer,
			ClothVertexBuffer,
			NumVertices,
			InputStreamStart,
			VertexFactory,
			TargetVertexFactory,
			IntermediateAccumulatedTangentBufferOffset,
			SimData);

		Entries.Add(InOutEntry);
	}

	FGPUSkinCacheEntry::FSectionDispatchData& SectionDispatchData = InOutEntry->DispatchData[Section];

	const bool bMorph = MorphVertexBuffer && MorphVertexBuffer->SectionIds.Contains(Section);
	if (bMorph)
	{
		InOutEntry->MorphBuffer = MorphVertexBuffer->GetSRV();
		check(InOutEntry->MorphBuffer);

		const uint32 MorphStride = sizeof(FMorphGPUSkinVertex);

		// see GPU code "check(MorphStride == sizeof(float) * 6);"
		check(MorphStride == sizeof(float) * 6);

		SectionDispatchData.MorphBufferOffset = BatchElement.BaseVertexIndex;

		// weight buffer
		const FSkinWeightVertexBuffer* WeightBuffer = Skin->GetSkinWeightVertexBuffer(LODIndex);
		uint32 WeightStride = WeightBuffer->GetConstantInfluencesVertexStride();
		SectionDispatchData.InputWeightStart = (WeightStride * BatchElement.BaseVertexIndex) / sizeof(float);
		InOutEntry->InputWeightStride = WeightStride;
		InOutEntry->InputWeightStreamSRV = WeightBuffer->GetDataVertexBuffer()->GetSRV();
	}

    if (ClothVertexBuffer)
    {
		TRACE_CPUPROFILER_EVENT_SCOPE(SetupCloth);
		FVertexBufferAndSRV ClothPositionAndNormalsBuffer;
		TSkeletalMeshVertexData<FVector3f> VertexAndNormalData(true);
        InOutEntry->ClothBuffer = ClothVertexBuffer->GetSRV();
        check(InOutEntry->ClothBuffer);

		if (SimData->Positions.Num() > 0)
		{
	        check(SimData->Positions.Num() == SimData->Normals.Num());
	        VertexAndNormalData.ResizeBuffer( 2 * SimData->Positions.Num() );

			FVector3f* Data = (FVector3f*)VertexAndNormalData.GetDataPointer();
	        uint32 Stride = VertexAndNormalData.GetStride();

	        // Copy the vertices into the buffer.
	        checkSlow(Stride*VertexAndNormalData.GetNumVertices() == sizeof(FVector3f) * 2 * SimData->Positions.Num());

			if (ClothVertexBuffer && ClothVertexBuffer->GetClothIndexMapping().Num() > Section)
			{
				const FClothBufferIndexMapping& ClothBufferIndexMapping = ClothVertexBuffer->GetClothIndexMapping()[Section];

				check(SimData->LODIndex != INDEX_NONE && SimData->LODIndex <= LODIndex);
				const uint32 ClothLODBias = (uint32)(LODIndex - SimData->LODIndex);

				const uint32 ClothBufferOffset = ClothBufferIndexMapping.MappingOffset + ClothBufferIndexMapping.LODBiasStride * ClothLODBias;

				// Set the buffer offset depending on whether enough deformer mapping data exists (RaytracingMinLOD/RaytracingLODBias/ClothLODBiasMode settings)
				const uint32 NumInfluences = NumVertices ? ClothBufferIndexMapping.LODBiasStride / NumVertices : 1;
				SectionDispatchData.ClothBufferOffset = (ClothBufferOffset + NumVertices * NumInfluences <= ClothVertexBuffer->GetNumVertices()) ?
					ClothBufferOffset :                     // If the offset is valid, set the calculated LODBias offset
					ClothBufferIndexMapping.MappingOffset;  // Otherwise fallback to a 0 ClothLODBias to prevent from reading pass the buffer (but still raytrace broken shadows/reflections/etc.)
			}

			for (int32 Index = 0; Index < SimData->Positions.Num(); Index++)
			{
				*(Data + Index * 2) = SimData->Positions[Index];
				*(Data + Index * 2 + 1) = SimData->Normals[Index];
			}

	        FResourceArrayInterface* ResourceArray = VertexAndNormalData.GetResourceArray();
	        check(ResourceArray->GetResourceDataSize() > 0);

			const FRHIBufferCreateDesc CreateDesc =
				FRHIBufferCreateDesc::CreateVertex(TEXT("ClothPositionAndNormalsBuffer"), ResourceArray->GetResourceDataSize())
				.AddUsage(BUF_Static | BUF_ShaderResource)
				.SetInitActionResourceArray(ResourceArray)
				.DetermineInitialState();

	        ClothPositionAndNormalsBuffer.VertexBufferRHI = RHICmdList.CreateBuffer(CreateDesc);
	        ClothPositionAndNormalsBuffer.VertexBufferSRV = RHICmdList.CreateShaderResourceView(
				ClothPositionAndNormalsBuffer.VertexBufferRHI, 
				FRHIViewDesc::CreateBufferSRV()
					.SetType(FRHIViewDesc::EBufferType::Typed)
					.SetFormat(PF_G32R32F));
			SectionDispatchData.ClothPositionsAndNormalsBuffer = ClothPositionAndNormalsBuffer.VertexBufferSRV;
		}
		else
		{
			UE_LOG(LogSkinCache, Error, TEXT("Cloth sim data is missing on mesh %s"), *GetSkeletalMeshObjectName(Skin));
		}

		SectionDispatchData.ClothBlendWeight = ClothBlendWeight;
		SectionDispatchData.ClothToLocal = ClothToLocal;
		SectionDispatchData.WorldScale = WorldScale;
	}
	SectionDispatchData.SkinType = ClothVertexBuffer && SectionDispatchData.ClothPositionsAndNormalsBuffer ? 2 : (bMorph ? 1 : 0);

	// Need to update the previous bone buffer pointer, so logic that checks if the bone buffers changed (FGPUSkinCache::FRWBufferTracker::Find)
	// doesn't invalidate the previous frame position data.  Recreating the render state will have generated new bone buffers.
	if (bRecreating && Mode == EGPUSkinCacheEntryMode::Raster)
	{
		FGPUBaseSkinVertexFactory::FShaderDataType& ShaderData = VertexFactory->GetShaderData();
		if (ShaderData.HasBoneBufferForReading(true))
		{
			SectionDispatchData.PositionTracker.UpdatePreviousBoneBuffer(ShaderData.GetBoneBufferForReading(true), ShaderData.GetRevisionNumber(true));
		}
	}

	InOutEntry->bQueuedForDispatch = true;

	// Check if the combo of skin cache entry and section index already exists, if so use the entry and update to latest revision number.
	if (SectionDispatchData.RevisionNumber != 0)
	{
		SectionDispatchData.RevisionNumber = FMath::Max(InOutEntry->DispatchData[Section].RevisionNumber, RevisionNumber);
	}
	else
	{
		SectionDispatchData.RevisionNumber = RevisionNumber;
		BatchDispatches.Add({ InOutEntry, uint32(Section) });
	}

#if RHI_RAYTRACING
	if (!Skin->ShouldUseSeparateSkinCacheEntryForRayTracing() || Mode == EGPUSkinCacheEntryMode::RayTracing)
	{
		// This is a RT skin cache entry
		PendingProcessRTGeometryEntries.Add(InOutEntry);
	}
#endif

	return true;
}

bool FGPUSkinCache::IsGPUSkinCacheRayTracingSupported()
{
#if RHI_RAYTRACING
	static const auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing.Geometry.SupportSkeletalMeshes"));
	static const bool SupportSkeletalMeshes = CVar->GetInt() != 0;
	return IsRayTracingAllowed() && SupportSkeletalMeshes && GEnableGPUSkinCache;
#else
	return false;
#endif
}

#if RHI_RAYTRACING

void FGPUSkinCache::ProcessRayTracingGeometryToUpdate(FRHICommandList& RHICmdList, FGPUSkinCacheEntry* SkinCacheEntry)
{
	check(IsGPUSkinCacheRayTracingSupported() && IsRayTracingEnabled());

	if (SkinCacheEntry && SkinCacheEntry->GPUSkin && SkinCacheEntry->GPUSkin->bSupportRayTracing)
	{
 		TArray<FBufferRHIRef> VertexBuffers;
 		SkinCacheEntry->GetRayTracingSegmentVertexBuffers(VertexBuffers);

		const int32 LODIndex = SkinCacheEntry->LOD;
		FSkeletalMeshRenderData& SkelMeshRenderData = SkinCacheEntry->GPUSkin->GetSkeletalMeshRenderData();
		check(LODIndex < SkelMeshRenderData.LODRenderData.Num());
		FSkeletalMeshLODRenderData& LODModel = SkelMeshRenderData.LODRenderData[LODIndex];

 		SkinCacheEntry->GPUSkin->UpdateRayTracingGeometry(RHICmdList, LODModel, LODIndex, VertexBuffers);
	}
}

#endif

void FGPUSkinCache::Dequeue(FGPUSkinCacheEntry* SkinCacheEntry)
{
	if (!SkinCacheEntry)
	{
		return;
	}
	
	FGPUSkinCache* SkinCache = SkinCacheEntry->SkinCache;
	checkf(SkinCache, TEXT("Attempting to dequeue a skin cache entry not linked to a parent skin cache"));
	
	SkinCache->PendingProcessRTGeometryEntries.Remove(SkinCacheEntry);

	if (SkinCacheEntry->bQueuedForDispatch)
	{
		for (int32 Index = 0; Index < SkinCache->BatchDispatches.Num(); )
		{
			FDispatchEntry& Dispatch = SkinCache->BatchDispatches[Index];
			
			if (Dispatch.SkinCacheEntry == SkinCacheEntry)
			{
				// Reset the revision, may not kick off the update otherwise
				SkinCacheEntry->DispatchData[Dispatch.Section].RevisionNumber = 0;
				
				SkinCache->BatchDispatches.RemoveAtSwap(Index);

				// Continue to search for other sections associated with this skin cache entry.
			}
			else
			{
				++Index;
			}
		}
		
		SkinCacheEntry->bQueuedForDispatch = false;
	}
}

void FGPUSkinCache::Release(FGPUSkinCacheEntry*& SkinCacheEntry)
{
	if (SkinCacheEntry)
	{
		Dequeue(SkinCacheEntry);

		ReleaseSkinCacheEntry(SkinCacheEntry);
		SkinCacheEntry = nullptr;
	}
}

void FGPUSkinCache::ReleaseSkinCacheEntry(FGPUSkinCacheEntry* SkinCacheEntry)
{
	FGPUSkinCache* SkinCache = SkinCacheEntry->SkinCache;

	FRWBuffersAllocation* PositionAllocation = SkinCacheEntry->PositionAllocation;
	if (PositionAllocation)
	{
		uint32 BufferSize = PositionAllocation->GetBufferSize();
		SkinCache->UsedMemoryInBytes -= BufferSize;
		DEC_MEMORY_STAT_BY(STAT_GPUSkinCache_TotalMemUsed, BufferSize);

		SkinCache->Allocations.Remove(PositionAllocation);

		delete PositionAllocation;

		SkinCacheEntry->PositionAllocation = nullptr;
	}

	SkinCache->Entries.RemoveSingleSwap(SkinCacheEntry, EAllowShrinking::No);
	delete SkinCacheEntry;
}

bool FGPUSkinCache::IsEntryValid(FGPUSkinCacheEntry* SkinCacheEntry, int32 Section)
{
	return SkinCacheEntry && SkinCacheEntry->IsSectionValid(Section);
}

void FGPUSkinCache::InvalidateAllEntries()
{
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		Entries[Index]->LOD = -1;
	}

	for (int32 Index = 0; Index < StagingBuffers.Num(); ++Index)
	{
		StagingBuffers[Index].Release();
	}
	StagingBuffers.SetNum(0, EAllowShrinking::No);
	SET_MEMORY_STAT(STAT_GPUSkinCache_TangentsIntermediateMemUsed, 0);
}

FRWBuffer* FGPUSkinCache::GetPositionBuffer(FRDGBuilder& GraphBuilder, FGPUSkinCacheEntry const* Entry, uint32 SectionIndex)
{
	if (Entry)
	{
		Entry->SkinCache->AddAsyncComputeWait(GraphBuilder);
		FGPUSkinCacheEntry::FSectionDispatchData const& DispatchData = Entry->GetDispatchData()[SectionIndex];
		FSkinCacheRWBuffer* SkinCacheRWBuffer = DispatchData.PositionBuffer;
		return SkinCacheRWBuffer != nullptr ? &SkinCacheRWBuffer->Buffer : nullptr;
	}
	return nullptr;
}

FRWBuffer* FGPUSkinCache::GetPreviousPositionBuffer(FRDGBuilder& GraphBuilder, FGPUSkinCacheEntry const* Entry, uint32 SectionIndex)
{
	if (Entry)
	{
		Entry->SkinCache->AddAsyncComputeWait(GraphBuilder);
		FGPUSkinCacheEntry::FSectionDispatchData const& DispatchData = Entry->GetDispatchData()[SectionIndex];
		FSkinCacheRWBuffer* SkinCacheRWBuffer = DispatchData.PreviousPositionBuffer;
		return SkinCacheRWBuffer != nullptr ? &SkinCacheRWBuffer->Buffer : nullptr;
	}
	return nullptr;
}

FRWBuffer* FGPUSkinCache::GetTangentBuffer(FRDGBuilder& GraphBuilder, FGPUSkinCacheEntry const* Entry, uint32 SectionIndex)
{
	if (Entry)
	{
		Entry->SkinCache->AddAsyncComputeWait(GraphBuilder);
		FGPUSkinCacheEntry::FSectionDispatchData const& DispatchData = Entry->GetDispatchData()[SectionIndex];
		FSkinCacheRWBuffer* SkinCacheRWBuffer = DispatchData.TangentBuffer;
		return SkinCacheRWBuffer != nullptr ? &SkinCacheRWBuffer->Buffer : nullptr;
	}
	return nullptr;
}

void FGPUSkinCache::UpdateSkinWeightBuffer(FGPUSkinCacheEntry* Entry)
{
	if (Entry)
	{
		// Dequeue any pending updates to the entries
		//   Skin weight updates reinitialize the vertex factories RHI state, which will in turn invalidate the bone data
		//   for any pending update in the dispatch list.
		FGPUSkinCache::Dequeue(Entry);
		
		Entry->UpdateSkinWeightBuffer();
	}
}

void FGPUSkinCache::SetEntryGPUSkin(FGPUSkinCacheEntry* Entry, FSkeletalMeshObject* Skin)
{
	if (Entry)
	{
		// Dequeue any pending updates to the entries
		//   When transferring owner there is a small window in which we may still reference the original vertex factory
		//   before the new owner has updated the entry. If the entry is pending an update in the dispatch list, we risk
		//   accessing invalid bone data if the original owner is released. The original owner *does* dequeue on release,
		//   however, the transfer nulls the old entry.
		FGPUSkinCache::Dequeue(Entry);

		for (FGPUSkinCacheEntry::FSectionDispatchData& DispatchData : Entry->DispatchData)
		{
			// Reset VF pointers to ensure IsSourceFactoryValid returns false when entry will get updated in next ProcessEntry call
			DispatchData.SourceVertexFactory = nullptr;
			DispatchData.TargetVertexFactory = nullptr;
		}

		Entry->GPUSkin = Skin;
	}
}

void FGPUSkinCache::CVarSinkFunction()
{
	int32 NewGPUSkinCacheValue = CVarEnableGPUSkinCache.GetValueOnAnyThread() != 0;
	int32 NewRecomputeTangentsValue = CVarGPUSkinCacheRecomputeTangents.GetValueOnAnyThread();
	const float NewSceneMaxSizeInMb = CVarGPUSkinCacheSceneMemoryLimitInMB.GetValueOnAnyThread();
	const int32 NewNumTangentIntermediateBuffers = CVarGPUSkinNumTangentIntermediateBuffers.GetValueOnAnyThread();
	const bool NewSkipCompilingGPUSkinVF = CVarSkipCompilingGPUSkinVF.GetValueOnAnyThread();

	if (GEnableGPUSkinCacheShaders)
	{
		if (GIsRHIInitialized && IsGPUSkinCacheRayTracingSupported() && IsRayTracingEnabled())
		{
			// Skin cache is *required* for ray tracing.
			NewGPUSkinCacheValue = 1;
		}
	}
	else
	{
		NewGPUSkinCacheValue = 0;
		NewRecomputeTangentsValue = 0;
	}

	// We don't have GPU Skin VF shaders at all so we can't fallback to using GPU Skinning.
	if (NewSkipCompilingGPUSkinVF)
	{
		// If we had the skin cache enabled and we are turning it off.
		if (GEnableGPUSkinCache && (NewGPUSkinCacheValue == 0))
		{
			NewGPUSkinCacheValue = 1;
			UE_LOG(LogSkinCache, Warning, TEXT("Attemping to turn off the GPU Skin Cache, but we don't have GPU Skin VF shaders to fallback to (r.SkinCache.SkipCompilingGPUSkinVF=1).  Leaving skin cache turned on."));
		}
	}

	if (NewGPUSkinCacheValue != GEnableGPUSkinCache || NewRecomputeTangentsValue != GSkinCacheRecomputeTangents
		|| NewSceneMaxSizeInMb != GSkinCacheSceneMemoryLimitInMB || NewNumTangentIntermediateBuffers != GNumTangentIntermediateBuffers)
	{		
		if (NewRecomputeTangentsValue == 1 && GStoreDuplicatedVerticesForRecomputeTangents == 0)
		{
			UE_LOG(LogSkinCache, Warning, TEXT("r.SkinCache.RecomputeTangents is set to 1 to update all skinned objects but duplicated vertices are not are not always stored. Set r.SkinCache.RecomputeTangents to 2 or r.SkinCache.StoreDuplicatedVerticesForRecomputeTangents to 1."));
		}

		ENQUEUE_RENDER_COMMAND(DoEnableSkinCaching)(UE::RenderCommandPipe::SkeletalMesh,
			[NewRecomputeTangentsValue, NewGPUSkinCacheValue, NewSceneMaxSizeInMb, NewNumTangentIntermediateBuffers](FRHICommandList& RHICmdList)
		{
			GNumTangentIntermediateBuffers = FMath::Max(NewNumTangentIntermediateBuffers, 1);
			GEnableGPUSkinCache = NewGPUSkinCacheValue;
			GSkinCacheRecomputeTangents = NewRecomputeTangentsValue;
			GSkinCacheSceneMemoryLimitInMB = NewSceneMaxSizeInMb;
			++GGPUSkinCacheFlushCounter;
		});

		TArray<UActorComponent*> Components;

		for (USkinnedMeshComponent* Component : TObjectRange<USkinnedMeshComponent>())
		{
			if (Component->IsRegistered() && Component->IsRenderStateCreated())
			{
				Components.Emplace(Component);
			}
		}

		FGlobalComponentRecreateRenderStateContext Context(Components);
	}
}

FAutoConsoleVariableSink FGPUSkinCache::CVarSink(FConsoleCommandDelegate::CreateStatic(&CVarSinkFunction));

uint64 FGPUSkinCache::GetExtraRequiredMemoryAndReset()
{
	if (GSkinCachePrintMemorySummary == 2 || (GSkinCachePrintMemorySummary == 1 && ExtraRequiredMemory > 0))
	{
		PrintMemorySummary();
	}

	uint64 OriginalValue = ExtraRequiredMemory;
	ExtraRequiredMemory = 0;
	return OriginalValue;
}

void FGPUSkinCache::PrintMemorySummary() const
{
	UE_LOG(LogSkinCache, Display, TEXT("======= Skin Cache Memory Usage Summary ======="));

	uint64 TotalMemInBytes = 0;
	for (int32 i = 0; i < Entries.Num(); ++i)
	{
		FGPUSkinCacheEntry* Entry = Entries[i];
		if (Entry)
		{
			FString RecomputeTangentSections = TEXT("");
			for (int32 DispatchIdx = 0; DispatchIdx < Entry->DispatchData.Num(); ++DispatchIdx)
			{
				const FGPUSkinCacheEntry::FSectionDispatchData& Data = Entry->DispatchData[DispatchIdx];
				if (Data.IndexBuffer)
				{
					if (RecomputeTangentSections.IsEmpty())
					{
						RecomputeTangentSections = TEXT("[Section]") + FString::FromInt(Data.SectionIndex);
					}
					else
					{
						RecomputeTangentSections = RecomputeTangentSections + TEXT("/") + FString::FromInt(Data.SectionIndex);
					}
				}
			}
			if (RecomputeTangentSections.IsEmpty())
			{
				RecomputeTangentSections = TEXT("Off");
			}

			uint64 MemInBytes = Entry->PositionAllocation ? Entry->PositionAllocation->GetBufferSize() : 0;
			uint64 TangentsInBytes = (Entry->PositionAllocation && Entry->PositionAllocation->GetTangentBuffer()) ? Entry->PositionAllocation->GetTangentBuffer()->Buffer.NumBytes : 0;
			uint64 IntermediateTangentsInBytes = (Entry->PositionAllocation && Entry->PositionAllocation->GetIntermediateTangentBuffer()) ? Entry->PositionAllocation->GetIntermediateTangentBuffer()->Buffer.NumBytes : 0;
			uint64 IntermediateAccumulatedTangentsInBytes = (Entry->PositionAllocation && Entry->PositionAllocation->GetIntermediateAccumulatedTangentBuffer()) ? Entry->PositionAllocation->GetIntermediateAccumulatedTangentBuffer()->Buffer.NumBytes : 0;

			UE_LOG(LogSkinCache, Display, TEXT("   SkinCacheEntry_%d: %sMesh=%s, LOD=%d, RecomputeTangent=%s, Mem=%.3fKB (Tangents=%.3fKB, InterTangents=%.3fKB, InterAccumTangents=%.3fKB)")
				, i
				, Entry->Mode == EGPUSkinCacheEntryMode::RayTracing ? TEXT("[RT]") : TEXT("")
				, *GetSkeletalMeshObjectName(Entry->GPUSkin)
				, Entry->LOD
				, *RecomputeTangentSections
				, MemInBytes / 1024.f
				, TangentsInBytes / 1024.f
				, IntermediateTangentsInBytes / 1024.f
				, IntermediateAccumulatedTangentsInBytes / 1024.f
			);

			TotalMemInBytes += MemInBytes;
		}
	}
	ensure(TotalMemInBytes == UsedMemoryInBytes);

	uint64 MaxSizeInBytes = (uint64)(GSkinCacheSceneMemoryLimitInMB * MBSize);
	uint64 UnusedSizeInBytes = MaxSizeInBytes - UsedMemoryInBytes;

	UE_LOG(LogSkinCache, Display, TEXT("Used: %.3fMB"), UsedMemoryInBytes / MBSize);
	UE_LOG(LogSkinCache, Display, TEXT("Available: %.3fMB"), UnusedSizeInBytes / MBSize);
	UE_LOG(LogSkinCache, Display, TEXT("Total limit: %.3fMB"), GSkinCacheSceneMemoryLimitInMB);
	UE_LOG(LogSkinCache, Display, TEXT("Extra required: %.3fMB"), ExtraRequiredMemory / MBSize);
	UE_LOG(LogSkinCache, Display, TEXT("==============================================="));
}

FString FGPUSkinCache::GetSkeletalMeshObjectName(const FSkeletalMeshObject* GPUSkin) const
{
	FString Name = TEXT("None");
	if (GPUSkin)
	{
#if !UE_BUILD_SHIPPING
		Name = GPUSkin->DebugName.ToString();
#endif // !UE_BUILD_SHIPPING
	}
	return Name;
}

FDebugName FGPUSkinCache::GetSkeletalMeshObjectDebugName(const FSkeletalMeshObject* GPUSkin) const
{
	if (!GPUSkin)
		return {};

	return GPUSkin->GetDebugName();
}

FColor FGPUSkinCache::GetVisualizationDebugColor(const FName& GPUSkinCacheVisualizationMode, FGPUSkinCacheEntry* Entry, FGPUSkinCacheEntry* RayTracingEntry, uint32 SectionIndex)
{
	const FGPUSkinCacheVisualizationData& VisualizationData = GetGPUSkinCacheVisualizationData();
	if (VisualizationData.IsActive())
	{
		// Color coding should match DrawVisualizationInfoText function
		FGPUSkinCacheVisualizationData::FModeType ModeType = VisualizationData.GetActiveModeType();

		if (ModeType == FGPUSkinCacheVisualizationData::FModeType::Overview)
		{
			bool bRecomputeTangent = Entry && Entry->DispatchData[SectionIndex].IndexBuffer;
			return Entry ? 
				   (bRecomputeTangent ? GEngine->GPUSkinCacheVisualizationRecomputeTangentsColor.QuantizeRound() : GEngine->GPUSkinCacheVisualizationIncludedColor.QuantizeRound()) : 
				   GEngine->GPUSkinCacheVisualizationExcludedColor.QuantizeRound();
		}
		else if (ModeType == FGPUSkinCacheVisualizationData::FModeType::Memory)
		{
			uint64 MemoryInBytes = (Entry && Entry->PositionAllocation) ? Entry->PositionAllocation->GetBufferSize() : 0;
#if RHI_RAYTRACING
			if (RayTracingEntry && RayTracingEntry != Entry)
			{
				// Separate ray tracing entry
				MemoryInBytes += RayTracingEntry->PositionAllocation ? RayTracingEntry->PositionAllocation->GetBufferSize() : 0;
			}
#endif
			float MemoryInMB = MemoryInBytes / MBSize;

			return MemoryInMB < GEngine->GPUSkinCacheVisualizationLowMemoryThresholdInMB ? GEngine->GPUSkinCacheVisualizationLowMemoryColor.QuantizeRound() :
				  (MemoryInMB < GEngine->GPUSkinCacheVisualizationHighMemoryThresholdInMB ? GEngine->GPUSkinCacheVisualizationMidMemoryColor.QuantizeRound() : GEngine->GPUSkinCacheVisualizationHighMemoryColor.QuantizeRound());
		}
		else if (ModeType == FGPUSkinCacheVisualizationData::FModeType::RayTracingLODOffset)
		{
	#if RHI_RAYTRACING
			int32 LODOffset = (Entry && RayTracingEntry) ? (RayTracingEntry->LOD - Entry->LOD) : 0;
			check (LODOffset >= 0);
			const TArray<FLinearColor>& VisualizationColors = GEngine->GPUSkinCacheVisualizationRayTracingLODOffsetColors;
			if (VisualizationColors.Num() > 0)
			{
				int32 Index = VisualizationColors.IsValidIndex(LODOffset) ? LODOffset : (VisualizationColors.Num()-1);
				return VisualizationColors[Index].QuantizeRound();
			}
	#endif
		}
	}

	return FColor::White;
}

void FGPUSkinCache::DrawVisualizationInfoText(const FName& GPUSkinCacheVisualizationMode, FScreenMessageWriter& ScreenMessageWriter) const
{
	const FGPUSkinCacheVisualizationData& VisualizationData = GetGPUSkinCacheVisualizationData();
	if (VisualizationData.IsActive())
	{
		FGPUSkinCacheVisualizationData::FModeType ModeType = VisualizationData.GetActiveModeType();

		// Color coding should match GetVisualizationDebugColor function
		auto DrawText = [&ScreenMessageWriter](const FString& Message, const FColor& Color)
		{
			ScreenMessageWriter.DrawLine(FText::FromString(Message), 10, Color);
		};

		if (ModeType == FGPUSkinCacheVisualizationData::FModeType::Overview)
		{
			DrawText(TEXT("Skin Cache Visualization - Overview"), FColor::White);
			DrawText(TEXT("Non SK mesh"), FColor::White);
			DrawText(TEXT("SK Skin Cache Excluded"), GEngine->GPUSkinCacheVisualizationExcludedColor.QuantizeRound());
			DrawText(TEXT("SK Skin Cache Included"), GEngine->GPUSkinCacheVisualizationIncludedColor.QuantizeRound());
			DrawText(TEXT("SK Recompute Tangent ON"), GEngine->GPUSkinCacheVisualizationRecomputeTangentsColor.QuantizeRound());
		}
		else if (ModeType == FGPUSkinCacheVisualizationData::FModeType::Memory)
		{
			float UsedMemoryInMB = UsedMemoryInBytes / MBSize;
			float AvailableMemoryInMB = GSkinCacheSceneMemoryLimitInMB - UsedMemoryInMB;

			FString LowMemoryText = FString::Printf(TEXT("0 - %fMB"), GEngine->GPUSkinCacheVisualizationLowMemoryThresholdInMB);
			DrawText(TEXT("Skin Cache Visualization - Memory"), FColor::White);
			DrawText(FString::Printf(TEXT("Total Limit: %.2fMB"), GSkinCacheSceneMemoryLimitInMB), FColor::White);
			DrawText(FString::Printf(TEXT("Total Used: %.2fMB"), UsedMemoryInMB), FColor::White);
			DrawText(FString::Printf(TEXT("Total Available: %.2fMB"), AvailableMemoryInMB), FColor::White);
			DrawText(FString::Printf(TEXT("Low: < %.2fMB"), GEngine->GPUSkinCacheVisualizationLowMemoryThresholdInMB), GEngine->GPUSkinCacheVisualizationLowMemoryColor.QuantizeRound());
			DrawText(FString::Printf(TEXT("Mid: %.2f - %.2fMB"), GEngine->GPUSkinCacheVisualizationLowMemoryThresholdInMB, GEngine->GPUSkinCacheVisualizationHighMemoryThresholdInMB), GEngine->GPUSkinCacheVisualizationMidMemoryColor.QuantizeRound());
			DrawText(FString::Printf(TEXT("High: > %.2fMB"), GEngine->GPUSkinCacheVisualizationHighMemoryThresholdInMB), GEngine->GPUSkinCacheVisualizationHighMemoryColor.QuantizeRound());
		}
		else if (ModeType == FGPUSkinCacheVisualizationData::FModeType::RayTracingLODOffset)
		{
	#if RHI_RAYTRACING
			DrawText(TEXT("Skin Cache Visualization - RayTracingLODOffset"), FColor::White);
			const TArray<FLinearColor>& VisualizationColors = GEngine->GPUSkinCacheVisualizationRayTracingLODOffsetColors;
			for (int32 i = 0; i < VisualizationColors.Num(); ++i)
			{
				DrawText(FString::Printf(TEXT("RT_LOD == Raster_LOD %s %d"), (i > 0 ? TEXT("+") : TEXT("")), i), VisualizationColors[i].QuantizeRound());
			}
	#endif
		}
	}
}

#undef IMPLEMENT_SKIN_CACHE_SHADER_CLOTH
#undef IMPLEMENT_SKIN_CACHE_SHADER_ALL_SKIN_TYPES
#undef IMPLEMENT_SKIN_CACHE_SHADER
