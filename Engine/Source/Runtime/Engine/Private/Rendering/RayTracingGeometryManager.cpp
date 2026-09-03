// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rendering/RayTracingGeometryManager.h"

#include "PrimitiveSceneProxy.h"
#include "SceneInterface.h"
#include "ComponentRecreateRenderStateContext.h"

#include "RHIResources.h"
#include "RHICommandList.h"

#include "RayTracingGeometry.h"
#include "RenderUtils.h"

#include "Rendering/RayTracingStreamableAsset.h"

#include "Math/UnitConversion.h"

#include "ProfilingDebugging/CsvProfiler.h"
#include "ProfilingDebugging/IoStoreTrace.h"

#include "Engine/Engine.h"

#if RHI_RAYTRACING

DECLARE_LOG_CATEGORY_CLASS(LogRayTracingGeometryManager, Log, All);

static bool bHasRayTracingEnableChanged = false;
static TAutoConsoleVariable<int32> CVarRayTracingEnable(
	TEXT("r.RayTracing.Enable"),
	1,
	TEXT("Whether ray tracing is enabled at runtime.\n")
	TEXT("If r.RayTracing.EnableOnDemand is enabled, ray tracing can be toggled on/off at runtime. Otherwise this is only checked during initialization."),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
		{
			FGlobalComponentRecreateRenderStateContext Context;
			ENQUEUE_RENDER_COMMAND(RayTracingToggledCmd)(
				[](FRHICommandListImmediate&)
				{
					bHasRayTracingEnableChanged = true;
				}
			);
		}),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<bool> CVarRayTracingUseReferenceBasedResidency(
	TEXT("r.RayTracing.UseReferenceBasedResidency"),
	true,
	TEXT("Whether raytracing geometries should be resident or evicted based on whether they're referenced in TLAS."),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
		{
			FGlobalComponentRecreateRenderStateContext Context;
			ENQUEUE_RENDER_COMMAND(RayTracingToggledCmd)(
				[](FRHICommandListImmediate&)
				{
					bHasRayTracingEnableChanged = true;
				}
			);
		}),
	ECVF_RenderThreadSafe
);

static int32 GRayTracingStreamingMaxPendingRequests = 128;
static FAutoConsoleVariableRef CVarNaniteStreamingMaxPendingRequests(
	TEXT("r.RayTracing.Streaming.MaxPendingRequests"),
	GRayTracingStreamingMaxPendingRequests,
	TEXT("Maximum number of requests that can be pending streaming."),
	ECVF_ReadOnly
);

static int32 GRayTracingResidentGeometryMemoryPoolSizeInMB = 400;
static FAutoConsoleVariableRef CVarRayTracingResidentGeometryMemoryPoolSizeInMB(
	TEXT("r.RayTracing.ResidentGeometryMemoryPoolSizeInMB"),
	GRayTracingResidentGeometryMemoryPoolSizeInMB,
	TEXT("Size of the ray tracing geometry pool.\n")
	TEXT("If pool size is larger than the requested geometry size, some unreferenced geometries will stay resident to reduce build overhead when they are requested again."),
	ECVF_RenderThreadSafe
);

static float GRayTracingApproximateCompactionRatio = 0.5f;
static FAutoConsoleVariableRef CVarRayTracingApproximateCompactionRatio(
	TEXT("r.RayTracing.ApproximateCompactionRatio"),
	GRayTracingApproximateCompactionRatio,
	TEXT("Ratio used by Ray Tracing Geometry Manager to approximate the ray tracing geometry size after compaction.\n")
	TEXT("This will be removed in a future version once Ray Tracing Geometry Manager tracks the actual compacted sizes."),
	ECVF_RenderThreadSafe
);

static bool bRefreshAlwaysResidentRayTracingGeometries = false;

static int32 GRayTracingNumAlwaysResidentLODs = 1;
static FAutoConsoleVariableRef CVarRayTracingNumAlwaysResidentLODs(
	TEXT("r.RayTracing.NumAlwaysResidentLODs"),
	GRayTracingNumAlwaysResidentLODs,
	TEXT("Number of LODs per ray tracing geometry group to always keep resident (even when not referenced by TLAS).\n")
	TEXT("Doesn't apply when ray tracing is disabled, in which case all ray tracing geometry is evicted."),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
		{
			ENQUEUE_RENDER_COMMAND(RefreshAlwaysResidentRayTracingGeometriesCmd)(
				[](FRHICommandListImmediate&)
				{
					bRefreshAlwaysResidentRayTracingGeometries = true;
				}
			);
		}),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<bool> CVarRayTracingOnDemandGeometryBuffersStreaming(
	TEXT("r.RayTracing.OnDemandGeometryBuffersStreaming"),
	true,
	TEXT("Whether to stream-in VB/IB buffers required to update dynamic geometry on-demand instead of keeping it in memory."),
	ECVF_RenderThreadSafe
);

static int32 GRayTracingMaxBuiltPrimitivesPerFrame = -1;
static FAutoConsoleVariableRef CVarRayTracingMaxBuiltPrimitivesPerFrame(
	TEXT("r.RayTracing.Geometry.MaxBuiltPrimitivesPerFrame"),
	GRayTracingMaxBuiltPrimitivesPerFrame,
	TEXT("Sets the ray tracing acceleration structure build budget in terms of maximum number of triangles per frame (<= 0 then disabled and all acceleration structures are build immediatly - default)"),
	ECVF_RenderThreadSafe
);

static float GRayTracingPendingBuildPriorityBoostPerFrame = 0.001f;
static FAutoConsoleVariableRef CVarRayTracingPendingBuildPriorityBoostPerFrame(
	TEXT("r.RayTracing.Geometry.PendingBuildPriorityBoostPerFrame"),
	GRayTracingPendingBuildPriorityBoostPerFrame,
	TEXT("Increment the priority for all pending build requests which are not scheduled that frame (0.001 - default)"),
	ECVF_RenderThreadSafe
);

static bool GRayTracingShowOnScreenWarnings = true;
static FAutoConsoleVariableRef CVarRayTracingShowOnScreenWarnings(
	TEXT("r.RayTracing.ShowOnScreenWarnings"),
	GRayTracingShowOnScreenWarnings,
	TEXT("Whether to show on-screen warnings related to ray tracing."),
	ECVF_RenderThreadSafe
);

#if DO_CHECK
static bool GRayTracingTestCheckIntegrity = false;
static FAutoConsoleVariableRef CVarRayTracingTestCheckIntegrity(
	TEXT("r.RayTracing.Test.CheckIntegrity"),
	GRayTracingTestCheckIntegrity,
	TEXT("Whether to check integrity of cached state related to ray tracing."),
	ECVF_RenderThreadSafe
);
#endif

DECLARE_STATS_GROUP(TEXT("Ray Tracing Geometry"), STATGROUP_RayTracingGeometry, STATCAT_Advanced);

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Geometry Count"), STAT_RayTracingGeometryCount, STATGROUP_RayTracingGeometry);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Geometry Group Count"), STAT_RayTracingGeometryGroupCount, STATGROUP_RayTracingGeometry);

DECLARE_MEMORY_STAT(TEXT("Resident Memory"), STAT_RayTracingGeometryResidentMemory, STATGROUP_RayTracingGeometry);
DECLARE_MEMORY_STAT(TEXT("Always Resident Memory"), STAT_RayTracingGeometryAlwaysResidentMemory, STATGROUP_RayTracingGeometry);
DECLARE_MEMORY_STAT(TEXT("Referenced Memory"), STAT_RayTracingGeometryReferencedMemory, STATGROUP_RayTracingGeometry);
DECLARE_MEMORY_STAT(TEXT("Requested Memory"), STAT_RayTracingGeometryRequestedMemory, STATGROUP_RayTracingGeometry);

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Pending Builds"), STAT_RayTracingPendingBuilds, STATGROUP_RayTracingGeometry);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Pending Build Primitives"), STAT_RayTracingPendingBuildPrimitives, STATGROUP_RayTracingGeometry);

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Pending Streaming Requests"), STAT_RayTracingPendingStreamingRequests, STATGROUP_RayTracingGeometry);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("In-flight Streaming Requests"), STAT_RayTracingInflightStreamingRequests, STATGROUP_RayTracingGeometry);

CSV_DEFINE_CATEGORY(RayTracingGeometry, true);

FRayTracingGeometryManager::FRayTracingGeometryManager()
{
	StreamingRequests.SetNum(GRayTracingStreamingMaxPendingRequests);

#if CSV_PROFILER_STATS
	if (FCsvProfiler* CSVProfiler = FCsvProfiler::Get())
	{
		CSVProfiler->OnCSVProfileStart().AddLambda([]()
		{
			CSV_METADATA(TEXT("RayTracing"), IsRayTracingEnabled() ? TEXT("1") : TEXT("0"));
		});
	}
#endif
}

FRayTracingGeometryManager::~FRayTracingGeometryManager()
{
	ensure(GeometryBuildRequests.IsEmpty());
	ensure(RegisteredGeometries.IsEmpty());

	ensure(RegisteredGroups.IsEmpty());
}

static float GetInitialBuildPriority(ERTAccelerationStructureBuildPriority InBuildPriority)
{
	switch (InBuildPriority)
	{
	case ERTAccelerationStructureBuildPriority::Immediate:	return 1.0f;
	case ERTAccelerationStructureBuildPriority::High:		return 0.5f;
	case ERTAccelerationStructureBuildPriority::Normal:		return 0.24f;
	case ERTAccelerationStructureBuildPriority::Low:		return 0.01f;
	case ERTAccelerationStructureBuildPriority::Skip:
	default:
	{
		checkNoEntry();
		return 0.0f;
	}
	}
}

FRayTracingGeometryManager::FBuildRequestIndex FRayTracingGeometryManager::RequestBuildAccelerationStructure(FRayTracingGeometry* InGeometry, ERTAccelerationStructureBuildPriority InPriority, EAccelerationStructureBuildMode InBuildMode)
{
	check(InGeometry->RayTracingBuildRequestIndex == INDEX_NONE);

	FBuildRequest Request;
	Request.BuildPriority = GetInitialBuildPriority(InPriority);
	Request.Owner = InGeometry;
	Request.BuildMode = EAccelerationStructureBuildMode::Build;

	FScopeLock ScopeLock(&RequestCS);
	FBuildRequestIndex RequestIndex = GeometryBuildRequests.Add(Request);
	GeometryBuildRequests[RequestIndex].RequestIndex = RequestIndex;

	INC_DWORD_STAT(STAT_RayTracingPendingBuilds);
	INC_DWORD_STAT_BY(STAT_RayTracingPendingBuildPrimitives, InGeometry->Initializer.TotalPrimitiveCount);

	InGeometry->RayTracingBuildRequestIndex = RequestIndex;
	
	return RequestIndex;
}

void FRayTracingGeometryManager::RemoveBuildRequest(FBuildRequestIndex InRequestIndex)
{
	FScopeLock ScopeLock(&RequestCS);

	DEC_DWORD_STAT(STAT_RayTracingPendingBuilds);
	DEC_DWORD_STAT_BY(STAT_RayTracingPendingBuildPrimitives, GeometryBuildRequests[InRequestIndex].Owner->Initializer.TotalPrimitiveCount);

	GeometryBuildRequests.RemoveAt(InRequestIndex);
}

bool FRayTracingGeometryManager::IsAlwaysResidentGeometry(const FRayTracingGeometry* InGeometry, const FRayTracingGeometryGroup& Group)
{
	return InGeometry->LODIndex >= Group.GeometryHandles.Num() - GRayTracingNumAlwaysResidentLODs;
}

RayTracing::FGeometryGroupHandle FRayTracingGeometryManager::RegisterRayTracingGeometryGroup(uint32 NumLODs, uint32 CurrentFirstLODIdx)
{
	FScopeLock ScopeLock(&MainCS);

	FRayTracingGeometryGroup Group;
	Group.GeometryHandles.Init(INDEX_NONE, NumLODs);
	Group.NumReferences = 1;
	Group.CurrentFirstLODIdx = CurrentFirstLODIdx;

	RayTracing::FGeometryGroupHandle Handle = RegisteredGroups.Add(MoveTemp(Group));

	INC_DWORD_STAT(STAT_RayTracingGeometryGroupCount);

	return Handle;
}

void FRayTracingGeometryManager::ReleaseRayTracingGeometryGroup(RayTracing::FGeometryGroupHandle Handle)
{
	FScopeLock ScopeLock(&MainCS);

	check(RegisteredGroups.IsValidIndex(Handle));

	ReleaseRayTracingGeometryGroupReference(Handle);
}

void FRayTracingGeometryManager::ReleaseRayTracingGeometryGroupReference(RayTracing::FGeometryGroupHandle Handle)
{
	FRayTracingGeometryGroup& Group = RegisteredGroups[Handle];

	--Group.NumReferences;

	if (Group.NumReferences == 0)
	{
		for (FGeometryHandle GeometryHandle : Group.GeometryHandles)
		{
			checkf(GeometryHandle == INDEX_NONE, TEXT("All FRayTracingGeometry in a group must be unregistered before releasing the group."));
		}

		check(Group.ProxiesWithCachedRayTracingState.IsEmpty());

		RegisteredGroups.RemoveAt(Handle);
		ReferencedGeometryGroups.Remove(Handle);
		ReferencedGeometryGroupsForDynamicUpdate.Remove(Handle);

		DEC_DWORD_STAT(STAT_RayTracingGeometryGroupCount);
	}
}

FRayTracingGeometryManager::FGeometryHandle FRayTracingGeometryManager::RegisterRayTracingGeometry(FRayTracingGeometry* InGeometry)
{
	check(InGeometry);

	FScopeLock ScopeLock(&MainCS);

	FGeometryHandle Handle = RegisteredGeometries.Add({});

	FRegisteredGeometry& RegisteredGeometry = RegisteredGeometries[Handle];
	RegisteredGeometry.Geometry = InGeometry;
	RegisteredGeometry.LastReferencedFrame = 0;

	if (InGeometry->GroupHandle != INDEX_NONE)
	{
		checkf(RegisteredGroups.IsValidIndex(InGeometry->GroupHandle), TEXT("FRayTracingGeometry.GroupHandle must be valid"));

		FRayTracingGeometryGroup& Group = RegisteredGroups[InGeometry->GroupHandle];

		checkf(InGeometry->LODIndex >= 0 && InGeometry->LODIndex < Group.GeometryHandles.Num(), TEXT("FRayTracingGeometry assigned to a group must have a valid LODIndex"));
		checkf(Group.GeometryHandles[InGeometry->LODIndex] == INDEX_NONE, TEXT("Each LOD inside a FRayTracingGeometryGroup can only be associated with a single FRayTracingGeometry"));

		Group.GeometryHandles[InGeometry->LODIndex] = Handle;
		++Group.NumReferences;

		RegisteredGeometry.bAlwaysResident = IsAlwaysResidentGeometry(InGeometry, Group);

		if (RegisteredGeometry.bAlwaysResident)
		{
			AlwaysResidentGeometries.Add(Handle);
		}

		if (IsRayTracingEnabled() && InGeometry->LODIndex >= Group.CurrentFirstLODIdx && (!IsRayTracingUsingReferenceBasedResidency() || RegisteredGeometry.bAlwaysResident))
		{
			PendingStreamingRequests.Add(Handle);
			INC_DWORD_STAT(STAT_RayTracingPendingStreamingRequests);
		}
	}
		
	INC_DWORD_STAT(STAT_RayTracingGeometryCount);

	GRayTracingGeometryManager->RefreshRegisteredGeometry(Handle);

	return Handle;
}

void FRayTracingGeometryManager::ReleaseRayTracingGeometryHandle(FGeometryHandle Handle)
{
	check(Handle != INDEX_NONE);

	FScopeLock ScopeLock(&MainCS);

	FRegisteredGeometry& RegisteredGeometry = RegisteredGeometries[Handle];

	// Cancel associated streaming request if currently in-flight
	CancelStreamingRequest(RegisteredGeometry);

	if (RegisteredGeometry.Geometry->GroupHandle != INDEX_NONE)
	{
		// if geometry was assigned to a group, clear the relevant entry so another geometry can be registered later

		checkf(RegisteredGroups.IsValidIndex(RegisteredGeometry.Geometry->GroupHandle), TEXT("FRayTracingGeometry.GroupHandle must be valid"));

		FRayTracingGeometryGroup& Group = RegisteredGroups[RegisteredGeometry.Geometry->GroupHandle];

		checkf(RegisteredGeometry.Geometry->LODIndex >= 0 && RegisteredGeometry.Geometry->LODIndex < Group.GeometryHandles.Num(), TEXT("FRayTracingGeometry assigned to a group must have a valid LODIndex"));
		checkf(Group.GeometryHandles[RegisteredGeometry.Geometry->LODIndex] == Handle, TEXT("Unexpected mismatch of FRayTracingGeometry in FRayTracingGeometryGroup"));

		Group.GeometryHandles[RegisteredGeometry.Geometry->LODIndex] = INDEX_NONE;

		ReleaseRayTracingGeometryGroupReference(RegisteredGeometry.Geometry->GroupHandle);
	}

	{
		int32 NumRemoved = ResidentGeometries.Remove(Handle);

		if (NumRemoved > 0)
		{
			TotalResidentSize -= RegisteredGeometry.Size;
		}
	}

	{
		int32 NumRemoved = AlwaysResidentGeometries.Remove(Handle);

		if (NumRemoved > 0)
		{
			checkf(RegisteredGeometry.bAlwaysResident, TEXT("Geometry should have the bAlwaysResident flag enabled since it was in the AlwaysResidentGeometries set."));

			TotalAlwaysResidentSize -= RegisteredGeometry.Size;
		}
	}

	EvictableGeometries.Remove(Handle);

	RegisteredGeometries.RemoveAt(Handle);
	ReferencedGeometryHandles.Remove(Handle);
	if (PendingStreamingRequests.Remove(Handle) > 0)
	{
		DEC_DWORD_STAT(STAT_RayTracingPendingStreamingRequests);
	}

	DEC_DWORD_STAT(STAT_RayTracingGeometryCount);
}

void FRayTracingGeometryManager::SetRayTracingGeometryStreamingData(const FRayTracingGeometry* Geometry, FRayTracingStreamableAsset& StreamableAsset)
{
	FScopeLock ScopeLock(&MainCS);

	checkf(RegisteredGeometries.IsValidIndex(Geometry->RayTracingGeometryHandle), TEXT("SetRayTracingGeometryStreamingData(...) can only be used with FRayTracingGeometry that has been registered with FRayTracingGeometryManager."));

	FRegisteredGeometry& RegisteredGeometry = RegisteredGeometries[Geometry->RayTracingGeometryHandle];
	RegisteredGeometry.StreamableAsset = &StreamableAsset;
	RegisteredGeometry.StreamableBVHSize = StreamableAsset.GetRequestSizeBVH();
	RegisteredGeometry.StreamableBuffersSize = StreamableAsset.GetRequestSizeBuffers();

	checkf(RegisteredGeometry.StreamableBVHSize > 0 || RegisteredGeometry.StreamableBuffersSize > 0, TEXT("FRayTracingStreamableAsset should have data to stream."));
}


void FRayTracingGeometryManager::SetRayTracingGeometryGroupCurrentFirstLODIndex(FRHICommandListBase& RHICmdList, RayTracing::FGeometryGroupHandle Handle, uint8 NewCurrentFirstLODIdx)
{
	FScopeLock ScopeLock(&MainCS);

	FRayTracingGeometryGroup& Group = RegisteredGroups[Handle];

	// immediately release streamed out LODs
	if(NewCurrentFirstLODIdx > Group.CurrentFirstLODIdx)
	{
		FRHIResourceReplaceBatcher Batcher(RHICmdList, NewCurrentFirstLODIdx - Group.CurrentFirstLODIdx);
		for (int32 LODIdx = Group.CurrentFirstLODIdx; LODIdx < NewCurrentFirstLODIdx; ++LODIdx)
		{
			FGeometryHandle GeometryHandle = Group.GeometryHandles[LODIdx];

			// some LODs might be stripped during cook
			// skeletal meshes only create static LOD when rendering as static
			if (GeometryHandle == INDEX_NONE)
			{
				continue;
			}

			FRegisteredGeometry& RegisteredGeometry = RegisteredGeometries[GeometryHandle];

			if (!RegisteredGeometry.Geometry->IsEvicted())
			{
				// Cancel associated streaming request if currently in-flight
				CancelStreamingRequest(RegisteredGeometry);

				StreamOutGeometry(Batcher, RegisteredGeometry);
			}
		}
	}
	else if(IsRayTracingEnabled() && !IsRayTracingUsingReferenceBasedResidency())
	{
		for (int32 LODIdx = NewCurrentFirstLODIdx; LODIdx < Group.CurrentFirstLODIdx; ++LODIdx)
		{
			if (Group.GeometryHandles[LODIdx] != INDEX_NONE)
			{
				// TODO: should do this for always resident mips even when using reference based residency
				PendingStreamingRequests.Add(Group.GeometryHandles[LODIdx]);
				INC_DWORD_STAT(STAT_RayTracingPendingStreamingRequests);
			}
		}
	}

	Group.CurrentFirstLODIdx = NewCurrentFirstLODIdx;
}

static bool ShouldCompactAfterBuild(const FRayTracingGeometryInitializer& Initializer)
{
	return Initializer.bAllowCompaction
		&& !Initializer.bFastBuild
		&& !Initializer.bAllowUpdate
		&& !Initializer.OfflineDataHeader.IsValid()
		&& GRHIGlobals.RayTracing.SupportsAccelerationStructureCompaction;
}

void FRayTracingGeometryManager::RefreshRegisteredGeometry(FGeometryHandle Handle)
{
	FScopeLock ScopeLock(&MainCS);

	if (RegisteredGeometries.IsValidIndex(Handle))
	{
		FRegisteredGeometry& RegisteredGeometry = RegisteredGeometries[Handle];

		const uint32 OldSize = RegisteredGeometry.Size;

		// Update size - Geometry RHI might not be valid yet (evicted or uninitialized), so calculate size using Initializer here
		{
			bool bAllSegmentsAreValid = RegisteredGeometry.Geometry->Initializer.Segments.Num() > 0;
			for (const FRayTracingGeometrySegment& Segment : RegisteredGeometry.Geometry->Initializer.Segments)
			{
				if (!Segment.VertexBuffer)
				{
					bAllSegmentsAreValid = false;
					break;
				}
			}

			if (bAllSegmentsAreValid)
			{
				RegisteredGeometry.Size = RHICalcRayTracingGeometrySize(RegisteredGeometry.Geometry->Initializer).ResultSize;

				if (ShouldCompactAfterBuild(RegisteredGeometry.Geometry->Initializer))
				{
					RegisteredGeometry.Size = uint32(RegisteredGeometry.Size * GRayTracingApproximateCompactionRatio);
				}
			}
			else
			{
				RegisteredGeometry.Size = 0;
			}
		}

		RegisteredGeometry.bEvicted = RegisteredGeometry.Geometry->IsEvicted();

		if (RegisteredGeometry.bAlwaysResident)
		{
			checkf(AlwaysResidentGeometries.Contains(Handle), TEXT("Geometry with bAlwaysResident flag set should be in the AlwaysResidentGeometries set."));

			TotalAlwaysResidentSize -= OldSize;
			TotalAlwaysResidentSize += RegisteredGeometry.Size;
		}

		if (RegisteredGeometry.Geometry->IsValid() && !RegisteredGeometry.bEvicted)
		{
			bool bAlreadyInSet;
			ResidentGeometries.Add(Handle, &bAlreadyInSet);

			if (bAlreadyInSet)
			{
				TotalResidentSize -= OldSize;
			}

			TotalResidentSize += RegisteredGeometry.Size;

			if (!RegisteredGeometry.bAlwaysResident)
			{
				EvictableGeometries.Add(Handle);
			}
		}
		else
		{
			int32 NumRemoved = ResidentGeometries.Remove(Handle);

			if (NumRemoved > 0)
			{
				TotalResidentSize -= OldSize;
			}

			EvictableGeometries.Remove(Handle);
		}

		checkf(!AlwaysResidentGeometries.Contains(Handle) || !RegisteredGeometry.bEvicted || !IsRayTracingEnabled(), TEXT("Always resident geometries can't be evicted"));

		if (RegisteredGeometry.Geometry->Initializer.Type == ERayTracingGeometryInitializerType::StreamingDestination)
		{
			RegisteredGeometry.Status = FRegisteredGeometry::FStatus::StreamedOut;
		}
	}
}

void FRayTracingGeometryManager::PreRender()
{
	bRenderedFrame = true;
}

void FRayTracingGeometryManager::Tick(FRHICommandList& RHICmdList)
{
	if (IsRunningCommandlet())
	{
		return;
	}

	check(IsInRenderingThread());

	TRACE_CPUPROFILER_EVENT_SCOPE(FRayTracingGeometryManager::Tick);
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FRayTracingGeometryManager_Tick);
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(RayTracingGeometryManager_Tick);

	// TODO: investigate fine grained locking to minimize blocking progress on render command pipes
	// - Don't touch registered geometry/group arrays from render command pipes
	//   - Separate arrays of free geometry/group handles + HandleAllocationCS
	//   - delay actual registration until PreRender() which happens on Render Thread
	//	 - Tick() doesn't need to lock at all
	// - Refresh requests could be queued and processed during Tick()
	FScopeLock ScopeLock(&MainCS);

#if DO_CHECK
	static uint64 PreviousFrameCounter = GFrameCounterRenderThread - 1;
	checkf(GFrameCounterRenderThread != PreviousFrameCounter, TEXT("FRayTracingGeometryManager::Tick() should only be called once per frame"));
	PreviousFrameCounter = GFrameCounterRenderThread;
#endif

	const bool bUsingReferenceBasedResidency = IsRayTracingUsingReferenceBasedResidency();

	checkf(bUsingReferenceBasedResidency || (ReferencedGeometryHandles.IsEmpty() && ReferencedGeometryGroups.IsEmpty() && ReferencedGeometryGroupsForDynamicUpdate.IsEmpty()),
		TEXT("ReferencedGeometryHandles, ReferencedGeometryGroups and ReferencedGeometryGroupsForDynamicUpdate are expected to be empty when not using reference based residency"));

	if (bRefreshAlwaysResidentRayTracingGeometries)
	{
		bRefreshAlwaysResidentRayTracingGeometries = false;

		AlwaysResidentGeometries.Empty();
		TotalAlwaysResidentSize = 0;

		for (FRegisteredGeometry& RegisteredGeometry : RegisteredGeometries)
		{
			const RayTracing::FGeometryGroupHandle GroupHandle = RegisteredGeometry.Geometry->GroupHandle;

			if (GroupHandle == INDEX_NONE)
			{
				RegisteredGeometry.bAlwaysResident = false;
				continue;
			}

			const FRayTracingGeometryGroup& Group = RegisteredGroups[GroupHandle];

			RegisteredGeometry.bAlwaysResident = IsAlwaysResidentGeometry(RegisteredGeometry.Geometry, Group);

			if (RegisteredGeometry.bAlwaysResident)
			{
				AlwaysResidentGeometries.Add(RegisteredGeometry.Geometry->RayTracingGeometryHandle);
				TotalAlwaysResidentSize += RegisteredGeometry.Size;

				if (RegisteredGeometry.Geometry->IsEvicted())
				{
					MakeGeometryResident(RHICmdList, RegisteredGeometry);
				}

				if (!RequestRayTracingGeometryStreamIn(RHICmdList, RegisteredGeometry.Geometry->RayTracingGeometryHandle))
				{
					PendingStreamingRequests.Add(RegisteredGeometry.Geometry->RayTracingGeometryHandle);

					INC_DWORD_STAT(STAT_RayTracingPendingStreamingRequests);
				}

				EvictableGeometries.Remove(RegisteredGeometry.Geometry->RayTracingGeometryHandle);
			}
			else if (bUsingReferenceBasedResidency && RegisteredGeometry.Geometry->GetRHI() != nullptr)
			{
				EvictGeometry(RHICmdList, RegisteredGeometry);
			}
		}
	}

#if DO_CHECK
	if (GRayTracingTestCheckIntegrity)
	{
		for (FRegisteredGeometry& RegisteredGeometry : RegisteredGeometries)
		{
			const FGeometryHandle GeometryHandle = RegisteredGeometry.Geometry->RayTracingGeometryHandle;
			const RayTracing::FGeometryGroupHandle GroupHandle = RegisteredGeometry.Geometry->GroupHandle;

			bool bAlwaysResident = false;

			if (GroupHandle != INDEX_NONE)
			{
				const FRayTracingGeometryGroup& Group = RegisteredGroups[GroupHandle];
				bAlwaysResident = IsAlwaysResidentGeometry(RegisteredGeometry.Geometry, Group);
			}
			else
			{
				// geometries not assigned to a group (eg: dynamic geometry) are always evictable
			}

			checkf(RegisteredGeometry.bEvicted == RegisteredGeometry.Geometry->IsEvicted(), TEXT("Cached bEvicted flag in FRegisteredGeometry is stale"));
			checkf(RegisteredGeometry.bAlwaysResident == bAlwaysResident, TEXT("Cached bAlwaysResident flag in FRegisteredGeometry is stale"));
			checkf(RegisteredGeometry.bAlwaysResident == AlwaysResidentGeometries.Contains(GeometryHandle), TEXT("Geometry with bAlwaysResident flag set should be in the AlwaysResidentGeometries set."));
		}
	}
#endif

	if (!IsRayTracingEnabled())
	{
		if (bHasRayTracingEnableChanged)
		{
			// evict all geometries
			for (FRegisteredGeometry& RegisteredGeometry : RegisteredGeometries)
			{
				if (RegisteredGeometry.Geometry->GetRHI() != nullptr)
				{
					EvictGeometry(RHICmdList, RegisteredGeometry);
				}
			}

			PendingStreamingRequests.Empty();

			SET_DWORD_STAT(STAT_RayTracingPendingStreamingRequests, 0);
		}
		else
		{
#if DO_CHECK
			// otherwise just check that everything is evicted
			for (FRegisteredGeometry& RegisteredGeometry : RegisteredGeometries)
			{
				checkf(RegisteredGeometry.Geometry->IsEvicted() || RegisteredGeometry.Geometry->GetRHI() == nullptr, TEXT("Ray tracing geometry should be evicted when ray tracing is disabled."));
			}
#endif
		}

		checkf(TotalResidentSize == 0,
			TEXT("TotalResidentSize should be 0 when ray tracing is disabled but is currently %lld.\n")
			TEXT("There's likely some issue tracking resident geometries or not all geometries have been evicted."),
			TotalResidentSize
		);

		check(PendingStreamingRequests.IsEmpty());

		SET_MEMORY_STAT(STAT_RayTracingGeometryReferencedMemory, 0);
		SET_MEMORY_STAT(STAT_RayTracingGeometryRequestedMemory, 0);
		CSV_CUSTOM_STAT(RayTracingGeometry, ReferencedSizeMB, 0, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(RayTracingGeometry, RequestedSizeMB, 0, ECsvCustomStatOp::Set);
	}
	else if (bUsingReferenceBasedResidency)
	{
		check(IsRayTracingEnabled());

		if (!bRenderedFrame)
		{
			ensureMsgf(ReferencedGeometryHandles.IsEmpty() && ReferencedGeometryGroups.IsEmpty() && ReferencedGeometryGroupsForDynamicUpdate.IsEmpty(),
				TEXT("Unexpected entries in ReferencedGeometryHandles/ReferencedGeometryGroups/ReferencedGeometryGroupsForDynamicUpdate. ")
				TEXT("Missing a call to PreRender() or didn't clear the arrays in the last frame?"));
			return;
		}

		bRenderedFrame = false;

		if (bHasRayTracingEnableChanged)
		{
			// make always resident geometries actually resident

			for (FGeometryHandle GeometryHandle : AlwaysResidentGeometries)
			{
				FRegisteredGeometry& RegisteredGeometry = RegisteredGeometries[GeometryHandle];

				if (RegisteredGeometry.Geometry->IsEvicted())
				{
					MakeGeometryResident(RHICmdList, RegisteredGeometry);
				}
				if (!RequestRayTracingGeometryStreamIn(RHICmdList, GeometryHandle))
				{
					PendingStreamingRequests.Add(GeometryHandle);

					INC_DWORD_STAT(STAT_RayTracingPendingStreamingRequests);
				}
			}
		}

		TSet<FGeometryHandle> NotReferencedResidentGeometries = EvictableGeometries;

		TArray<FGeometryHandle> ReferencedGeometries;

		uint64 ReferencedSize = 0;
		uint64 RequestedSize = TotalAlwaysResidentSize;
		uint64 RequestedButEvictedSize = 0;

		// Step 1
		// - update LastReferencedFrame of referenced geometries and calculate memory required to make evicted geometries resident
		for (FGeometryHandle GeometryHandle : ReferencedGeometryHandles)
		{
			FRegisteredGeometry& RegisteredGeometry = RegisteredGeometries[GeometryHandle];
			RegisteredGeometry.LastReferencedFrame = GFrameCounterRenderThread;

			ReferencedGeometries.Add(GeometryHandle);
			NotReferencedResidentGeometries.Remove(GeometryHandle);

			ReferencedSize += RegisteredGeometry.Size;

			if (!RegisteredGeometry.bAlwaysResident)
			{
				RequestedSize += RegisteredGeometry.Size;
			}

			if (RegisteredGeometry.bEvicted)
			{
				RequestedButEvictedSize += RegisteredGeometry.Size;
			}
		}

		// Step 2
		// - add all geometries in referenced groups to ReferencedGeometries
		//		- need to make all geometries in group resident otherwise might not have valid geometry when reducing LOD
		//		- TODO: Could track TargetLOD and only make [TargetLOD ... LastLOD] range resident
		// - also update LastReferencedFrame and calculate memory required to make evicted geometries resident
		for (RayTracing::FGeometryGroupHandle GroupHandle : ReferencedGeometryGroups)
		{
			checkf(RegisteredGroups.IsValidIndex(GroupHandle), TEXT("RayTracingGeometryGroupHandle must be valid"));

			const FRayTracingGeometryGroup& Group = RegisteredGroups[GroupHandle];

			for (uint8 LODIndex = Group.CurrentFirstLODIdx; LODIndex < Group.GeometryHandles.Num(); ++LODIndex)
			{
				FGeometryHandle GeometryHandle = Group.GeometryHandles[LODIndex];

				if (GeometryHandle != INDEX_NONE) // some LODs might be stripped during cook
				{
					FRegisteredGeometry& RegisteredGeometry = RegisteredGeometries[GeometryHandle];
					RegisteredGeometry.LastReferencedFrame = GFrameCounterRenderThread;

					ReferencedSize += RegisteredGeometry.Size;

					if (RegisteredGeometry.bAlwaysResident)
					{
						checkf(!RegisteredGeometry.bEvicted, TEXT("Always resident ray tracing geometry was unexpectely evicted."));
					}
					else
					{
						ReferencedGeometries.Add(GeometryHandle);
						NotReferencedResidentGeometries.Remove(GeometryHandle);

						RequestedSize += RegisteredGeometry.Size;

						if (RegisteredGeometry.bEvicted)
						{
							RequestedButEvictedSize += RegisteredGeometry.Size;
						}
					}
				}
			}
		}

#if DO_CHECK
		// ensure(ReferencedGeometries.Num() == TSet(ReferencedGeometries).Num());
#endif

		const uint64 ResidentGeometryMemoryPoolSize = FUnitConversion::Convert(GRayTracingResidentGeometryMemoryPoolSizeInMB, EUnit::Megabytes, EUnit::Bytes);

		// Step 3
		// - if making requested geometries resident will put us over budget -> evict some geometry not referenced by TLAS
		if (TotalResidentSize + TotalStreamingSize + RequestedButEvictedSize > ResidentGeometryMemoryPoolSize)
		{
			TArray<FGeometryHandle> NotReferencedResidentGeometriesArray = NotReferencedResidentGeometries.Array();

			// Step 3.1
			// - sort to evict geometries in the following order:
			//		- least recently used
			//		- largest geometries
			Algo::Sort(NotReferencedResidentGeometriesArray, [this](FGeometryHandle& LHSHandle, FGeometryHandle& RHSHandle)
				{
					FRegisteredGeometry& LHS = RegisteredGeometries[LHSHandle];
					FRegisteredGeometry& RHS = RegisteredGeometries[RHSHandle];

					// TODO: evict unreferenced dynamic geometries using shared buffers first since they need to be rebuild anyway
					// (and then dynamic geometries requiring update?

					// 1st - last referenced frame
					if (LHS.LastReferencedFrame != RHS.LastReferencedFrame)
					{
						return LHS.LastReferencedFrame < RHS.LastReferencedFrame;
					}

					// 2nd - size
					return LHS.Size > RHS.Size;
				});

			// Step 3.2
			// - evict geometries until we are in budget
			int32 Index = 0;
			while (TotalResidentSize + TotalStreamingSize + RequestedButEvictedSize > ResidentGeometryMemoryPoolSize && Index < NotReferencedResidentGeometriesArray.Num())
			{
				FGeometryHandle GeometryHandle = NotReferencedResidentGeometriesArray[Index];
				FRegisteredGeometry& RegisteredGeometry = RegisteredGeometries[GeometryHandle];

				check(RegisteredGeometry.Geometry->IsValid() && !RegisteredGeometry.Geometry->IsEvicted());

				EvictGeometry(RHICmdList, RegisteredGeometry);

				++Index;
			}
		}

		// Step 4
		// - make referenced geometries resident until we go over budget
		if (TotalResidentSize + TotalStreamingSize < ResidentGeometryMemoryPoolSize)
		{
			// Step 4.1
			//	- sort by size to prioritize smaller geometries
			Algo::Sort(ReferencedGeometries, [this](FGeometryHandle& LHSHandle, FGeometryHandle& RHSHandle)
				{
					FRegisteredGeometry& LHS = RegisteredGeometries[LHSHandle];
					FRegisteredGeometry& RHS = RegisteredGeometries[RHSHandle];

					return LHS.Size < RHS.Size;
				});

			// Step 4.2
			// - make geometries resident until we go over budget
			int32 Index = 0;
			while (TotalResidentSize + TotalStreamingSize < ResidentGeometryMemoryPoolSize && Index < ReferencedGeometries.Num())
			{
				FGeometryHandle GeometryHandle = ReferencedGeometries[Index];
				FRegisteredGeometry& RegisteredGeometry = RegisteredGeometries[GeometryHandle];

				if (RegisteredGeometry.Geometry->IsEvicted())
				{
					MakeGeometryResident(RHICmdList, RegisteredGeometry);
				}

				RequestRayTracingGeometryStreamIn(RHICmdList, GeometryHandle);

				++Index;
			}
		}

		SET_MEMORY_STAT(STAT_RayTracingGeometryReferencedMemory, ReferencedSize);
		SET_MEMORY_STAT(STAT_RayTracingGeometryRequestedMemory, RequestedSize);
		CSV_CUSTOM_STAT(RayTracingGeometry, ReferencedSizeMB, ReferencedSize / 1024.0f / 1024.0f, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(RayTracingGeometry, RequestedSizeMB, RequestedSize / 1024.0f / 1024.0f, ECsvCustomStatOp::Set);

#if !UE_BUILD_SHIPPING
		if (GRayTracingShowOnScreenWarnings)
		{
			if (TotalAlwaysResidentSize > ResidentGeometryMemoryPoolSize)
			{
				GEngine->AddOnScreenDebugMessage(uint64(this), 1.f, FColor::Red,
					*FString::Printf(TEXT("RAY TRACING GEOMETRY - ALWAYS RESIDENT MEMORY OVER BUDGET %s / %s"),
						*FText::AsMemory(TotalAlwaysResidentSize).ToString(),
						*FText::AsMemory(ResidentGeometryMemoryPoolSize).ToString()));
			}
			else if (RequestedSize > ResidentGeometryMemoryPoolSize)
			{
				GEngine->AddOnScreenDebugMessage(uint64(this), 1.f, FColor::Yellow,
					*FString::Printf(TEXT("RAY TRACING GEOMETRY - REQUESTED MEMORY OVER BUDGET %s / %s"),
						*FText::AsMemory(RequestedSize).ToString(),
						*FText::AsMemory(ResidentGeometryMemoryPoolSize).ToString()));
			}
		}
#endif
	}
	else
	{
		check(IsRayTracingEnabled());

		if (bHasRayTracingEnableChanged)
		{
			// make all geometries resident
			for (FRegisteredGeometry& RegisteredGeometry : RegisteredGeometries)
			{
				if (RegisteredGeometry.Geometry->IsEvicted())
				{
					MakeGeometryResident(RHICmdList, RegisteredGeometry);
				}

				if (!RequestRayTracingGeometryStreamIn(RHICmdList, RegisteredGeometry.Geometry->RayTracingGeometryHandle))
				{
					PendingStreamingRequests.Add(RegisteredGeometry.Geometry->RayTracingGeometryHandle);

					INC_DWORD_STAT(STAT_RayTracingPendingStreamingRequests);
				}
			}
		}
		else
		{
#if DO_CHECK
			// otherwise just check that all geometries are resident
			for (FRegisteredGeometry& RegisteredGeometry : RegisteredGeometries)
			{
				checkf(!RegisteredGeometry.Geometry->IsEvicted(), TEXT("Ray tracing geometry should not be evicted when ray tracing is enabled."));
			}
#endif
		}

		SET_MEMORY_STAT(STAT_RayTracingGeometryReferencedMemory, 0);
		SET_MEMORY_STAT(STAT_RayTracingGeometryRequestedMemory, TotalResidentSize);
		CSV_CUSTOM_STAT(RayTracingGeometry, ReferencedSizeMB, 0, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(RayTracingGeometry, RequestedSizeMB, TotalResidentSize / 1024.0f / 1024.0f, ECsvCustomStatOp::Set);
	}

	{
		TSet<FGeometryHandle> CurrentPendingStreamingRequests;
		Swap(CurrentPendingStreamingRequests, PendingStreamingRequests);
		PendingStreamingRequests.Reserve(CurrentPendingStreamingRequests.Num());

		for (FGeometryHandle GeometryHandle : CurrentPendingStreamingRequests)
		{
			if (!RequestRayTracingGeometryStreamIn(RHICmdList, GeometryHandle))
			{
				PendingStreamingRequests.Add(GeometryHandle);
			}
		}
	}

	SET_DWORD_STAT(STAT_RayTracingPendingStreamingRequests, PendingStreamingRequests.Num());

	ProcessCompletedStreamingRequests(RHICmdList);

	ReferencedGeometryHandles.Reset();
	ReferencedGeometryGroups.Reset();
	ReferencedGeometryGroupsForDynamicUpdate.Reset();

	bHasRayTracingEnableChanged = false;

	SET_MEMORY_STAT(STAT_RayTracingGeometryResidentMemory, TotalResidentSize);
	SET_MEMORY_STAT(STAT_RayTracingGeometryAlwaysResidentMemory, TotalAlwaysResidentSize);

	CSV_CUSTOM_STAT(RayTracingGeometry, TotalResidentSizeMB, TotalResidentSize / 1024.0f / 1024.0f, ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(RayTracingGeometry, TotalAlwaysResidentSizeMB, TotalAlwaysResidentSize / 1024.0f / 1024.0f, ECsvCustomStatOp::Set);
}

bool FRayTracingGeometryManager::RequestRayTracingGeometryStreamIn(FRHICommandList& RHICmdList, FGeometryHandle GeometryHandle)
{
	FRegisteredGeometry& RegisteredGeometry = RegisteredGeometries[GeometryHandle];
	FRayTracingGeometry* Geometry = RegisteredGeometry.Geometry;

	if (RegisteredGeometry.Status == FRegisteredGeometry::FStatus::Streaming)
	{
		// skip if there's already a streaming request in-flight for this geometry
		return true;
	}

	const bool bStreamBVH = Geometry->Initializer.Type == ERayTracingGeometryInitializerType::StreamingDestination;
	const bool bStreamBuffers = bStreamBVH || (RegisteredGeometry.StreamableBuffersSize > 0 && !RegisteredGeometry.StreamableAsset->AreBuffersStreamedIn() && ReferencedGeometryGroupsForDynamicUpdate.Contains(RegisteredGeometry.Geometry->GroupHandle));

	if (!bStreamBuffers && !bStreamBVH)
	{
		// no streaming required
		return true;
	}

	if (Geometry->GroupHandle != INDEX_NONE)
	{
		const FRayTracingGeometryGroup& Group = RegisteredGroups[Geometry->GroupHandle];

		if (Geometry->LODIndex < Group.CurrentFirstLODIdx)
		{
			// streaming request no longer necessary
			return true;
		}
	}

	// TODO: Support DDC streaming

	if (RegisteredGeometry.StreamableBuffersSize == 0 && RegisteredGeometry.StreamableBVHSize == 0)
	{
		// no offline data -> build from VB/IB at runtime

		RegisteredGeometry.Status = FRegisteredGeometry::FStatus::StreamedIn;
	}
	else
	{
		if (NumStreamingRequests >= GRayTracingStreamingMaxPendingRequests)
		{
			return false;
		}

		checkf(RegisteredGeometry.StreamingRequestIndex == INDEX_NONE, TEXT("Ray Tracing Geometry already has a streaming request in-flight"));
		RegisteredGeometry.StreamingRequestIndex = NextStreamingRequestIndex;

		FStreamingRequest& StreamingRequest = StreamingRequests[NextStreamingRequestIndex];
		checkf(!StreamingRequest.IsValid(), TEXT("Unused streaming request are expected to be in invalid state."));
		NextStreamingRequestIndex = (NextStreamingRequestIndex + 1) % GRayTracingStreamingMaxPendingRequests;
		++NumStreamingRequests;

		INC_DWORD_STAT(STAT_RayTracingInflightStreamingRequests);

		uint32 StreamableDataSize = 0;

		if (bStreamBuffers)
		{
			StreamableDataSize += RegisteredGeometry.StreamableBuffersSize;
		}

		if (bStreamBVH)
		{
			check(!RegisteredGeometry.StreamableAsset->IsBVHStreamedIn());
			StreamableDataSize += RegisteredGeometry.StreamableBVHSize;
		}

		StreamingRequest.GeometryHandle = GeometryHandle;
		StreamingRequest.GeometrySize = RegisteredGeometry.Size;
		StreamingRequest.bBuffersOnly = !bStreamBVH;
		StreamingRequest.RequestBuffer = FIoBuffer(StreamableDataSize); // TODO: Use FIoBuffer::Wrap with preallocated memory

		RegisteredGeometry.StreamableAsset->IssueRequest(StreamingRequest.Request, StreamingRequest.RequestBuffer, StreamingRequest.bBuffersOnly);

		RegisteredGeometry.Status = FRegisteredGeometry::FStatus::Streaming;

		TotalStreamingSize += StreamingRequest.GeometrySize;
	}

	if (RegisteredGeometry.Status == FRegisteredGeometry::FStatus::StreamedIn)
	{
		{
			FRHIResourceReplaceBatcher Batcher(RHICmdList, 1);
			FRayTracingGeometryInitializer IntermediateInitializer = Geometry->Initializer;
			IntermediateInitializer.Type = ERayTracingGeometryInitializerType::StreamingSource;
			IntermediateInitializer.OfflineData = nullptr;

			FRayTracingGeometryRHIRef IntermediateRayTracingGeometry = RHICmdList.CreateRayTracingGeometry(IntermediateInitializer);

			Geometry->SetRequiresBuild(IntermediateInitializer.OfflineData == nullptr || IntermediateRayTracingGeometry->IsCompressed());

			Geometry->InitRHIForStreaming(IntermediateRayTracingGeometry, Batcher);

			// When Batcher goes out of scope it will add commands to copy the BLAS buffers on RHI thread.
			// We need to do it before we build the current geometry (also on RHI thread).
		}

		Geometry->RequestBuildIfNeeded(RHICmdList, ERTAccelerationStructureBuildPriority::Normal);
	}

	return true;
}

void FRayTracingGeometryManager::ProcessCompletedStreamingRequests(FRHICommandList& RHICmdList)
{
	const bool bOnDemandGeometryBuffersStreaming = IsRayTracingUsingReferenceBasedResidency() && CVarRayTracingOnDemandGeometryBuffersStreaming.GetValueOnRenderThread();

	const int32 StartPendingRequestIndex = (NextStreamingRequestIndex + GRayTracingStreamingMaxPendingRequests - NumStreamingRequests) % GRayTracingStreamingMaxPendingRequests;

	int32 NumCompletedRequests = 0;

	for (int32 Index = 0; Index < NumStreamingRequests; ++Index)
	{
		const int32 PendingRequestIndex = (StartPendingRequestIndex + Index) % GRayTracingStreamingMaxPendingRequests;
		FStreamingRequest& PendingRequest = StreamingRequests[PendingRequestIndex];

		checkf(PendingRequest.IsValid(), TEXT("Pending streaming request should be valid."));

		if (PendingRequest.Request.IsCompleted())
		{
			++NumCompletedRequests;

			TotalStreamingSize -= PendingRequest.GeometrySize;
			check(TotalStreamingSize >= 0);

			if (PendingRequest.bCancelled)
			{
				PendingRequest.Reset();
				continue;
			}

			FRegisteredGeometry& RegisteredGeometry = RegisteredGeometries[PendingRequest.GeometryHandle];

			RegisteredGeometry.StreamingRequestIndex = INDEX_NONE;

			const FRayTracingGeometryGroup& Group = RegisteredGroups[RegisteredGeometry.Geometry->GroupHandle];

			if (RegisteredGeometry.Geometry->IsEvicted() || RegisteredGeometry.Geometry->LODIndex < Group.CurrentFirstLODIdx)
			{
				// do nothing since geometry was evicted while streaming request was being processed
			}
			else if (!PendingRequest.Request.IsOk())
			{
				UE_LOG(LogRayTracingGeometryManager, Warning, TEXT("Ray Tracing Geometry IO Request failed (%s)"), *RegisteredGeometry.Geometry->Initializer.DebugName.ToString());

				// Manager will retry again if still necessary on the next frame
			}
			else
			{
				RegisteredGeometry.StreamableAsset->InitWithStreamedData(RHICmdList, PendingRequest.RequestBuffer.GetView(), PendingRequest.bBuffersOnly);

				// if VB/IB are not being used for dynamic BLAS updates (eg: WPO)
				// and the RHI doesn't need them either (hit shaders not supported / inline SBT not required)
				// then we can stream-out the buffers after BLAS is built
				if (!GRHIGlobals.RayTracing.SupportsShaders
					&& !GRHIGlobals.RayTracing.RequiresInlineRayTracingSBT
					&& bOnDemandGeometryBuffersStreaming
					&& !ReferencedGeometryGroupsForDynamicUpdate.Contains(RegisteredGeometry.Geometry->GroupHandle))
				{
					if (RegisteredGeometry.Geometry->HasPendingBuildRequest())
					{
						// need to delay releasing buffers until build is dispatched
						GeometryBuildRequests[RegisteredGeometry.Geometry->RayTracingBuildRequestIndex].bReleaseBuffersAfterBuild = true;
					}
					else
					{
						FRHIResourceReplaceBatcher Batcher(RHICmdList, 1);
						RegisteredGeometry.StreamableAsset->ReleaseBuffersForStreaming(Batcher);
					}
				}

				RegisteredGeometry.Status = FRegisteredGeometry::FStatus::StreamedIn;
			}

			PendingRequest.Reset();
		}
		else
		{
			// TODO: Could other requests already be completed?
			break;
		}
	}

	NumStreamingRequests -= NumCompletedRequests;

	SET_DWORD_STAT(STAT_RayTracingInflightStreamingRequests, NumStreamingRequests);
}

void FRayTracingGeometryManager::CancelStreamingRequest(FRegisteredGeometry& RegisteredGeometry)
{
	if (RegisteredGeometry.StreamingRequestIndex != INDEX_NONE)
	{
		FStreamingRequest& StreamingRequest = StreamingRequests[RegisteredGeometry.StreamingRequestIndex];
		checkf(StreamingRequest.GeometryHandle == RegisteredGeometry.Geometry->RayTracingGeometryHandle,
			TEXT("Ray tracing geometry streaming request owner mismatch (expected %d, got %d)."), RegisteredGeometry.Geometry->RayTracingGeometryHandle, StreamingRequest.GeometryHandle);

		StreamingRequest.Cancel();

		RegisteredGeometry.StreamingRequestIndex = INDEX_NONE;
	}
}

void FRayTracingGeometryManager::StreamOutGeometry(FRHIResourceReplaceBatcher& Batcher, FRegisteredGeometry& RegisteredGeometry)
{
	if (EnumHasAllFlags(RegisteredGeometry.Geometry->GetGeometryState(), FRayTracingGeometry::EGeometryStateFlags::StreamedIn))
	{
		if (RegisteredGeometry.StreamableAsset != nullptr)
		{
			RegisteredGeometry.StreamableAsset->ReleaseForStreaming(Batcher);
		}
		else
		{
			RegisteredGeometry.Geometry->ReleaseRHIForStreaming(Batcher);
		}
	}
}

void FRayTracingGeometryManager::MakeGeometryResident(FRHICommandList& RHICmdList, FRegisteredGeometry& RegisteredGeometry)
{
	RegisteredGeometry.Geometry->MakeResident(RHICmdList);
	RegisteredGeometry.bEvicted = false;
}

void FRayTracingGeometryManager::EvictGeometry(FRHICommandListBase& RHICmdList, FRegisteredGeometry& RegisteredGeometry)
{
	// Cancel associated streaming request if currently in-flight
	CancelStreamingRequest(RegisteredGeometry);

	// Both FRayTracingGeometry::ReleaseRHIForStreaming(...) and FRayTracingGeometry::Evict()
	// call FRayTracingGeometryManager:::RefreshRegisteredGeometry(...) which is unecessary
	// however there's no straightforward way to avoid that
	// TODO: investigate possible improvements

	FRHIResourceReplaceBatcher Batcher(RHICmdList, 1);
	StreamOutGeometry(Batcher, RegisteredGeometry);

	RegisteredGeometry.Geometry->Evict();
	RegisteredGeometry.bEvicted = true;
}

void FRayTracingGeometryManager::BoostPriority(FBuildRequestIndex InRequestIndex, float InBoostValue)
{
	FScopeLock ScopeLock(&RequestCS);
	GeometryBuildRequests[InRequestIndex].BuildPriority += InBoostValue;
}

void FRayTracingGeometryManager::ForceBuildIfPending(FRHIComputeCommandList& InCmdList, const TArrayView<const FRayTracingGeometry*> InGeometries)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FRayTracingGeometryManager::ForceBuildIfPending);

	FScopeLock ScopeLock(&RequestCS);

	TArray<FRayTracingGeometry*> ReleaseBuffers;

	BuildParams.Empty(FMath::Max(BuildParams.Max(), InGeometries.Num()));
	for (const FRayTracingGeometry* Geometry : InGeometries)
	{
		if (Geometry->HasPendingBuildRequest())
		{
			SetupBuildParams(GeometryBuildRequests[Geometry->RayTracingBuildRequestIndex], BuildParams, ReleaseBuffers);
		}
	}

	if (BuildParams.Num())
	{
		InCmdList.BuildAccelerationStructures(BuildParams);
	}

	BuildParams.Reset();

	for (FRayTracingGeometry* Geometry : ReleaseBuffers)
	{
		FRegisteredGeometry& RegisteredGeometry = RegisteredGeometries[Geometry->RayTracingGeometryHandle];

		FRHIResourceReplaceBatcher Batcher(InCmdList, 1);
		RegisteredGeometry.StreamableAsset->ReleaseBuffersForStreaming(Batcher);
	}
}

void FRayTracingGeometryManager::ProcessBuildRequests(FRHIComputeCommandList& InCmdList, bool bInBuildAll)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FRayTracingGeometryManager::ProcessBuildRequests);

	FScopeLock ScopeLock(&RequestCS);

	if (GeometryBuildRequests.Num() == 0)
	{
		return;
	}

	checkf(BuildParams.IsEmpty(), TEXT("Unexpected entries in BuildParams. The array should've been reset at the end of the previous call."));
	checkf(SortedRequests.IsEmpty(), TEXT("Unexpected entries in SortedRequests. The array should've been reset at the end of the previous call."));

	TArray<FRayTracingGeometry*> ReleaseBuffers;

	BuildParams.Empty(FMath::Max(BuildParams.Max(), GeometryBuildRequests.Num()));

	if (GRayTracingMaxBuiltPrimitivesPerFrame <= 0)
	{
		// no limit -> no need to sort

		SortedRequests.Empty(); // free potentially allocated memory

		for (FBuildRequest& Request : GeometryBuildRequests)
		{
			const bool bRemoveFromRequestArray = false; // can't modify array while iterating over it
			SetupBuildParams(Request, BuildParams, ReleaseBuffers, bRemoveFromRequestArray);
		}

		// after setting up build params can clear the whole array
		GeometryBuildRequests.Reset();
	}
	else
	{
		SortedRequests.Empty(FMath::Max(SortedRequests.Max(), GeometryBuildRequests.Num()));

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(SortRequests);

			// Is there a fast way to extract all entries from sparse array?
			for (const FBuildRequest& Request : GeometryBuildRequests)
			{
				SortedRequests.Add(Request);
			}

			SortedRequests.Sort([](const FBuildRequest& InLHS, const FBuildRequest& InRHS)
				{
					return InLHS.BuildPriority > InRHS.BuildPriority;
				});
		}

		// process n requests each 'frame'
		uint64 PrimitivesBuild = 0;
		bool bAddBuildRequest = true;
		for (FBuildRequest& Request : SortedRequests)
		{
			if (bAddBuildRequest || Request.BuildPriority >= 1.0f) // always build immediate requests
			{
				SetupBuildParams(Request, BuildParams, ReleaseBuffers);

				// Requested enough?
				PrimitivesBuild += Request.Owner->Initializer.TotalPrimitiveCount;
				if (!bInBuildAll && (PrimitivesBuild > GRayTracingMaxBuiltPrimitivesPerFrame))
				{
					bAddBuildRequest = false;
				}
			}
			else
			{
				// Increment priority to make sure requests don't starve
				Request.BuildPriority += GRayTracingPendingBuildPriorityBoostPerFrame;
			}
		}

		SortedRequests.Reset();
	}

	// kick actual build request to RHI command list
	InCmdList.BuildAccelerationStructures(BuildParams);

	BuildParams.Reset();

	for (FRayTracingGeometry* Geometry : ReleaseBuffers)
	{
		FRegisteredGeometry& RegisteredGeometry = RegisteredGeometries[Geometry->RayTracingGeometryHandle];

		FRHIResourceReplaceBatcher Batcher(InCmdList, 1);
		RegisteredGeometry.StreamableAsset->ReleaseBuffersForStreaming(Batcher);
	}
}

void FRayTracingGeometryManager::SetupBuildParams(const FBuildRequest& InBuildRequest, TArray<FRayTracingGeometryBuildParams>& InBuildParams, TArray<FRayTracingGeometry*>& InReleaseBuffers, bool bRemoveFromRequestArray)
{
	check(InBuildRequest.RequestIndex != INDEX_NONE && InBuildRequest.RequestIndex == InBuildRequest.Owner->RayTracingBuildRequestIndex);
	checkf(InBuildRequest.Owner->GetRHI() != nullptr, TEXT("Build request for FRayTracingGeometry without valid RHI. Was the FRayTracingGeometry evicted or released without calling RemoveBuildRequest()?"));

	FRayTracingGeometryBuildParams BuildParam;
	BuildParam.Geometry = InBuildRequest.Owner->GetRHI();
	BuildParam.BuildMode = InBuildRequest.BuildMode;
	InBuildParams.Add(BuildParam);

	InBuildRequest.Owner->RayTracingBuildRequestIndex = INDEX_NONE;

	if (InBuildRequest.Owner->GroupHandle != INDEX_NONE)
	{
		RequestUpdateCachedRenderState(InBuildRequest.Owner->GroupHandle);
	}

	if (InBuildRequest.bReleaseBuffersAfterBuild)
	{
		InReleaseBuffers.Add(InBuildRequest.Owner);
	}

	DEC_DWORD_STAT(STAT_RayTracingPendingBuilds);
	DEC_DWORD_STAT_BY(STAT_RayTracingPendingBuildPrimitives, InBuildRequest.Owner->Initializer.TotalPrimitiveCount);

	if (bRemoveFromRequestArray)
	{
		GeometryBuildRequests.RemoveAt(InBuildRequest.RequestIndex);
	}
}

void FRayTracingGeometryManager::RegisterProxyWithCachedRayTracingState(FPrimitiveSceneProxy* Proxy, RayTracing::FGeometryGroupHandle InRayTracingGeometryGroupHandle)
{
	checkf(IsInRenderingThread(), TEXT("Can only access RegisteredGroups on render thread otherwise need a critical section"));
	checkf(IsRayTracingAllowed(), TEXT("Should only register proxies with FRayTracingGeometryManager when ray tracing is allowed"));
	checkf(RegisteredGroups.IsValidIndex(InRayTracingGeometryGroupHandle), TEXT("InRayTracingGeometryGroupHandle must be valid"));

	FRayTracingGeometryGroup& Group = RegisteredGroups[InRayTracingGeometryGroupHandle];

	TSet<FPrimitiveSceneProxy*>& ProxiesSet = Group.ProxiesWithCachedRayTracingState;
	check(!ProxiesSet.Contains(Proxy));

	ProxiesSet.Add(Proxy);

	++Group.NumReferences;
}

void FRayTracingGeometryManager::UnregisterProxyWithCachedRayTracingState(FPrimitiveSceneProxy* Proxy, RayTracing::FGeometryGroupHandle InRayTracingGeometryGroupHandle)
{
	checkf(IsInRenderingThread(), TEXT("Can only access RegisteredGroups on render thread otherwise need a critical section"));
	checkf(IsRayTracingAllowed(), TEXT("Should only register proxies with FRayTracingGeometryManager when ray tracing is allowed"));
	checkf(RegisteredGroups.IsValidIndex(InRayTracingGeometryGroupHandle), TEXT("InRayTracingGeometryGroupHandle must be valid"));

	FRayTracingGeometryGroup& Group = RegisteredGroups[InRayTracingGeometryGroupHandle];

	TSet<FPrimitiveSceneProxy*>& ProxiesSet = Group.ProxiesWithCachedRayTracingState;

	verify(ProxiesSet.Remove(Proxy) == 1);

	ReleaseRayTracingGeometryGroupReference(InRayTracingGeometryGroupHandle);
}

void FRayTracingGeometryManager::RequestUpdateCachedRenderState(RayTracing::FGeometryGroupHandle InRayTracingGeometryGroupHandle)
{
	checkf(IsInRenderingThread(), TEXT("Can only access RegisteredGroups on render thread otherwise need a critical section"));
	checkf(IsRayTracingAllowed(), TEXT("Should only register proxies with FRayTracingGeometryManager when ray tracing is allowed"));
	checkf(RegisteredGroups.IsValidIndex(InRayTracingGeometryGroupHandle), TEXT("InRayTracingGeometryGroupHandle must be valid"));

	const TSet<FPrimitiveSceneProxy*>& ProxiesSet = RegisteredGroups[InRayTracingGeometryGroupHandle].ProxiesWithCachedRayTracingState;

	for (FPrimitiveSceneProxy* Proxy : ProxiesSet)
	{
		Proxy->GetScene().UpdateCachedRayTracingState(Proxy);
	}
}

void FRayTracingGeometryManager::AddReferencedGeometry(const FRayTracingGeometry* Geometry)
{
	check(IsInRenderingThread() || IsInParallelRenderingThread());

	if (IsRayTracingUsingReferenceBasedResidency())
	{
		if (RegisteredGeometries.IsValidIndex(Geometry->RayTracingGeometryHandle))
		{
			ReferencedGeometryHandles.Add(Geometry->RayTracingGeometryHandle);
		}
	}
}

void FRayTracingGeometryManager::AddReferencedGeometryGroups(const TSet<RayTracing::FGeometryGroupHandle>& GeometryGroups)
{
	check(IsInRenderingThread() || IsInParallelRenderingThread());

	if (IsRayTracingUsingReferenceBasedResidency())
	{
		ReferencedGeometryGroups.Append(GeometryGroups);
	}
	else
	{
		ensureMsgf(GeometryGroups.IsEmpty(), TEXT("Should only track ReferencedGeometryGroups when using using reference based residency"));
	}
}

void FRayTracingGeometryManager::AddReferencedGeometryGroupsForDynamicUpdate(const TSet<RayTracing::FGeometryGroupHandle>& GeometryGroups)
{
	check(IsInRenderingThread() || IsInParallelRenderingThread());

	if (IsRayTracingUsingReferenceBasedResidency())
	{
		ReferencedGeometryGroupsForDynamicUpdate.Append(GeometryGroups);
	}
	else
	{
		ensureMsgf(GeometryGroups.IsEmpty(), TEXT("Should only track ReferencedGeometryGroupsForDynamic when using using reference based residency"));
	}
}

bool FRayTracingGeometryManager::IsGeometryVisible(FGeometryHandle GeometryHandle) const
{
	return VisibleGeometryHandles.Contains(GeometryHandle);
}


void FRayTracingGeometryManager::AddVisibleGeometry(FGeometryHandle GeometryHandle)
{
	VisibleGeometryHandles.Add(GeometryHandle);
}

void FRayTracingGeometryManager::ResetVisibleGeometries()
{
	// Reset the previous frame handles
	VisibleGeometryHandles.Empty(VisibleGeometryHandles.Num());
}

#if DO_CHECK
bool FRayTracingGeometryManager::IsGeometryReferenced(const FRayTracingGeometry* Geometry) const
{
	return ReferencedGeometryHandles.Contains(Geometry->RayTracingGeometryHandle);
}

bool FRayTracingGeometryManager::IsGeometryGroupReferenced(RayTracing::FGeometryGroupHandle GeometryGroup) const
{
	return ReferencedGeometryGroups.Contains(GeometryGroup);
}
#endif

#endif // RHI_RAYTRACING
