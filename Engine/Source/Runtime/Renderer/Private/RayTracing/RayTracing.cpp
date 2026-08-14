// Copyright Epic Games, Inc. All Rights Reserved.

#include "RayTracing.h"

#if RHI_RAYTRACING

#include "RayTracingDynamicGeometryUpdateManager.h"
#include "RayTracingInstanceMask.h"
#include "RayTracingInstanceCulling.h"
#include "RayTracingMaterialHitShaders.h"
#include "RayTracingScene.h"
#include "Nanite/NaniteRayTracing.h"
#include "Rendering/NaniteCoarseMeshStreamingManager.h"
#include "Rendering/RayTracingGeometryManager.h"
#include "ScenePrivate.h"
#include "Materials/MaterialRenderProxy.h"
#include "Lumen/LumenHardwareRayTracingCommon.h"
#include "RayTracingShadows.h"
#include "Experimental/Containers/SherwoodHashTable.h"
#include "RHIShaderBindingLayout.h"
#include "Async/ParallelFor.h"
#include <type_traits>

static int32 GRayTracingParallelMeshBatchSetup = 1;
static FAutoConsoleVariableRef CRayTracingParallelMeshBatchSetup(
	TEXT("r.RayTracing.ParallelMeshBatchSetup"),
	GRayTracingParallelMeshBatchSetup,
	TEXT("Whether to setup ray tracing materials via parallel jobs."),
	ECVF_RenderThreadSafe);

static int32 GRayTracingParallelMeshBatchSize = 1024;
static FAutoConsoleVariableRef CRayTracingParallelMeshBatchSize(
	TEXT("r.RayTracing.ParallelMeshBatchSize"),
	GRayTracingParallelMeshBatchSize,
	TEXT("Batch size for ray tracing materials parallel jobs."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarRayTracingDynamicGeometryLastRenderTimeUpdateDistance(
	TEXT("r.RayTracing.DynamicGeometryLastRenderTimeUpdateDistance"),
	5000.0f,
	TEXT("Dynamic geometries within this distance will have their LastRenderTime updated, so that visibility based ticking (like skeletal mesh) can work when the component is not directly visible in the view (but reflected)."));

static TAutoConsoleVariable<int32> CVarRayTracingAutoInstance(
	TEXT("r.RayTracing.AutoInstance"),
	1,
	TEXT("Whether to auto instance static meshes\n"),
	ECVF_RenderThreadSafe
);

static int32 GRayTracingExcludeTranslucent = 0;
static FAutoConsoleVariableRef CRayTracingExcludeTranslucent(
	TEXT("r.RayTracing.ExcludeTranslucent"),
	GRayTracingExcludeTranslucent,
	TEXT("A toggle that modifies the inclusion of translucent objects in the ray tracing scene.\n")
	TEXT(" 0: Translucent objects included in the ray tracing scene (default)\n")
	TEXT(" 1: Translucent objects excluded from the ray tracing scene"),
	ECVF_RenderThreadSafe);

static int32 GRayTracingExcludeSky = 1;
static FAutoConsoleVariableRef CRayTracingExcludeSky(
	TEXT("r.RayTracing.ExcludeSky"),
	GRayTracingExcludeSky,
	TEXT("A toggle that controls inclusion of sky geometry in the ray tracing scene (excluding sky can make ray tracing faster). This setting is ignored for the Path Tracer.\n")
	TEXT(" 0: Sky objects included in the ray tracing scene\n")
	TEXT(" 1: Sky objects excluded from the ray tracing scene (default)"),
	ECVF_RenderThreadSafe);

static int32 GRayTracingExcludeDecals = 0;
static FAutoConsoleVariableRef CRayTracingExcludeDecals(
	TEXT("r.RayTracing.ExcludeDecals"),
	GRayTracingExcludeDecals,
	TEXT("A toggle that modifies the inclusion of decals in the ray tracing BVH.\n")
	TEXT(" 0: Decals included in the ray tracing BVH (default)\n")
	TEXT(" 1: Decals excluded from the ray tracing BVH"),
	ECVF_RenderThreadSafe);

static int32 GRayTracingDebugDisableTriangleCull = 0;
static FAutoConsoleVariableRef CVarRayTracingDebugDisableTriangleCull(
	TEXT("r.RayTracing.DebugDisableTriangleCull"),
	GRayTracingDebugDisableTriangleCull,
	TEXT("Forces all ray tracing geometry instances to be double-sided by disabling back-face culling. This is useful for debugging and profiling. (default = 0)")
);

static int32 GRayTracingDebugForceOpaque = 0;
static FAutoConsoleVariableRef CVarRayTracingDebugForceOpaque(
	TEXT("r.RayTracing.DebugForceOpaque"),
	GRayTracingDebugForceOpaque,
	TEXT("Forces all ray tracing geometry instances to be opaque, effectively disabling any-hit shaders. This is useful for debugging and profiling. (default = 0)")
);

static bool bUpdateCachedRayTracingState = false;

static FAutoConsoleCommand UpdateCachedRayTracingStateCmd(
	TEXT("r.RayTracing.UpdateCachedState"),
	TEXT("Update cached ray tracing state (mesh commands and instances)."),
	FConsoleCommandDelegate::CreateStatic([] { bUpdateCachedRayTracingState = true; }));

static bool bRefreshRayTracingInstances = false;

static void RefreshRayTracingInstancesSinkFunction()
{
	static const auto RayTracingStaticMeshesCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.RayTracing.Geometry.StaticMeshes"));
	static const auto RayTracingHISMCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.RayTracing.Geometry.HierarchicalInstancedStaticMesh"));
	static const auto RayTracingNaniteProxiesCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.RayTracing.Geometry.NaniteProxies"));
	static const auto RayTracingSkeletalMeshesCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.RayTracing.Geometry.SkeletalMeshes"));
	static const auto RayTracingLandscapeGrassCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.RayTracing.Geometry.LandscapeGrass"));

	static int32 CachedRayTracingStaticMeshes = RayTracingStaticMeshesCVar->GetValueOnGameThread();
	static int32 CachedRayTracingHISM = RayTracingHISMCVar->GetValueOnGameThread();
	static int32 CachedRayTracingNaniteProxies = RayTracingNaniteProxiesCVar->GetValueOnGameThread();
	static int32 CachedRayTracingSkeletalMeshes = RayTracingSkeletalMeshesCVar->GetValueOnGameThread();
	static int32 CachedRayTracingLandscapeGrass = RayTracingLandscapeGrassCVar->GetValueOnGameThread();

	const int32 RayTracingStaticMeshes = RayTracingStaticMeshesCVar->GetValueOnGameThread();
	const int32 RayTracingHISM = RayTracingHISMCVar->GetValueOnGameThread();
	const int32 RayTracingNaniteProxies = RayTracingNaniteProxiesCVar->GetValueOnGameThread();
	const int32 RayTracingSkeletalMeshes = RayTracingSkeletalMeshesCVar->GetValueOnGameThread();
	const int32 RayTracingLandscapeGrass = RayTracingLandscapeGrassCVar->GetValueOnGameThread();

	if (RayTracingStaticMeshes != CachedRayTracingStaticMeshes
		|| RayTracingHISM != CachedRayTracingHISM
		|| RayTracingNaniteProxies != CachedRayTracingNaniteProxies
		|| RayTracingSkeletalMeshes != CachedRayTracingSkeletalMeshes
		|| RayTracingLandscapeGrass != CachedRayTracingLandscapeGrass)
	{
		ENQUEUE_RENDER_COMMAND(RefreshRayTracingInstancesCmd)(
			[](FRHICommandListImmediate&)
			{
				bRefreshRayTracingInstances = true;
			}
		);

		CachedRayTracingStaticMeshes = RayTracingStaticMeshes;
		CachedRayTracingHISM = RayTracingHISM;
		CachedRayTracingNaniteProxies = RayTracingNaniteProxies;
		CachedRayTracingSkeletalMeshes = RayTracingSkeletalMeshes;
		CachedRayTracingLandscapeGrass = RayTracingLandscapeGrass;
	}
}

static FAutoConsoleVariableSink CVarRefreshRayTracingInstancesSink(FConsoleCommandDelegate::CreateStatic(&RefreshRayTracingInstancesSinkFunction));

namespace RayTracing
{
	static void AddRayTracingInstanceFlagOverrides(ERayTracingInstanceFlags& InOutFlags, bool bUsesLightingChannels)
	{
		if (bUsesLightingChannels)
		{
			// AHS is needed for lighting channels since the Raytracing API's work off an "inclusion" mask that's not compatible with lighting channels
			InOutFlags &= ~ERayTracingInstanceFlags::ForceOpaque;
		}

		if (GRayTracingDebugForceOpaque)
		{
			InOutFlags |= ERayTracingInstanceFlags::ForceOpaque;
		}

		if (GRayTracingDebugDisableTriangleCull)
		{
			InOutFlags |= ERayTracingInstanceFlags::TriangleCullDisable;
		}
	}

	// Configure ray tracing scene options based on currently enabled features and their needs
	FSceneOptions::FSceneOptions(
		const FScene& Scene,
		const FViewFamilyInfo& ViewFamily,
		const FViewInfo& View,
		EDiffuseIndirectMethod DiffuseIndirectMethod,
		EReflectionsMethod ReflectionsMethod)
	{
		bTranslucentGeometry = false;
		LumenHardwareRayTracing::SetRayTracingSceneOptions(View, DiffuseIndirectMethod, ReflectionsMethod, *this);
		RayTracingShadows::SetRayTracingSceneOptions(View.bHasRayTracingShadows, *this);

		if (ShouldRenderRayTracingTranslucency(View))
		{
			bTranslucentGeometry = true;
		}

		if (ViewFamily.EngineShowFlags.RayTracingDebug)
		{
			bTranslucentGeometry = true; // could check r.RayTracing.Visualize.OpaqueOnly, but not critical as this is only for debugging purposes
		}

		if (ViewFamily.EngineShowFlags.PathTracing
			&& FDataDrivenShaderPlatformInfo::GetSupportsPathTracing(Scene.GetShaderPlatform()))
		{
			bTranslucentGeometry = true;
		}

		if (GRayTracingExcludeTranslucent != 0)
		{
			bTranslucentGeometry = false;
		}

		bIncludeSky = GRayTracingExcludeSky == 0 || ViewFamily.EngineShowFlags.PathTracing;

		bLightingChannelsUsingAHS = MegaLights::IsEnabled(ViewFamily) && MegaLights::IsUsingLightingChannels();
	}

	struct FRelevantPrimitive
	{
		const FRayTracingGeometry* RayTracingGeometry = nullptr;
		FRayTracingCachedMeshCommandFlags CachedMeshCommandFlags;
		int32 PrimitiveIndex = -1;
		FPersistentPrimitiveIndex PersistentPrimitiveIndex;
		int8 LODIndex = -1;
		bool bUsesLightingChannels : 1 = false;

		const FRayTracingGeometryInstance* CachedRayTracingInstance = nullptr;

		// Offsets relative to FRelevantPrimitiveContext offsets
		int32 RelativeInstanceOffset = INDEX_NONE;
		int32 RelativeDecalInstanceOffset = INDEX_NONE;
		int32 RelativeFarFieldInstanceOffset = INDEX_NONE;
		int32 RelativeVisibleShaderBindingsOffset = INDEX_NONE;
		int32 ContextIndex = INDEX_NONE;

		uint64 InstancingKey() const
		{
			uint64 Key = CachedMeshCommandFlags.CachedMeshCommandHash;
			Key ^= uint64(CachedMeshCommandFlags.InstanceMask) << 32;
			Key ^= CachedMeshCommandFlags.bAllSegmentsOpaque ? 0x1ull << 40 : 0x0;
			Key ^= CachedMeshCommandFlags.bAllSegmentsCastShadow ? 0x1ull << 41 : 0x0;
			Key ^= CachedMeshCommandFlags.bAnySegmentsCastShadow ? 0x1ull << 42 : 0x0;
			Key ^= CachedMeshCommandFlags.bAnySegmentsDecal ? 0x1ull << 43 : 0x0;
			Key ^= CachedMeshCommandFlags.bAllSegmentsDecal ? 0x1ull << 44 : 0x0;
			Key ^= CachedMeshCommandFlags.bTwoSided ? 0x1ull << 45 : 0x0;
			Key ^= CachedMeshCommandFlags.bIsSky ? 0x1ull << 46 : 0x0;
			Key ^= CachedMeshCommandFlags.bAllSegmentsTranslucent ? 0x1ull << 47 : 0x0;
			Key ^= CachedMeshCommandFlags.bAllSegmentsReverseCulling ? 0x1ull << 48 : 0x0;
			return Key ^ reinterpret_cast<uint64>(RayTracingGeometry->GetRHI());
		}
	};

	struct FRelevantPrimitiveGatherContext
	{
		int32 InstanceOffset = -1;
		int32 DecalInstanceOffset = -1;
		int32 FarFieldInstanceOffset = -1;
		int32 VisibleShaderBindingsOffset = -1;
	};

	struct FDynamicRayTracingPrimitive
	{
		int32 PrimitiveIndex;
		TRange<int32> InstancesRange;
		TRange<int32> GeometriesToUpdateRange;
	};

	class FDynamicRayTracingInstancesContext
	{
	public:
		FDynamicRayTracingInstancesContext(FScene& Scene, FViewInfo& InView, FSceneRenderingBulkObjectAllocator& InBulkAllocator, const FSceneOptions& InSceneOptions);

		void GatherDynamicRayTracingInstances(TConstArrayView<int32> InDynamicPrimitives);

		void GatherDynamicRayTracingInstances_RenderThread();

		void Finish(FRHICommandListImmediate& InRHICmdList);

		void AddInstancesToScene(FRayTracingScene& RayTracingScene, FRayTracingShaderBindingTable& RayTracingSBT, int64 SharedBufferGenerationID);

		void CollectRDGResources(FRDGBuilder& RDGBuilder);

	private:

		void GatherDynamicRayTracingInstances_Internal(int32 PrimitiveIndex);

		FScene& Scene;
		FViewInfo& View;
		FSceneRenderingBulkObjectAllocator& BulkAllocator;
		const FSceneOptions& SceneOptions;

		bool bTrackReferencedGeometryGroups;

		FRHICommandList* RHICmdList;
		FGlobalDynamicVertexBuffer DynamicVertexBuffer;
		FGlobalDynamicIndexBuffer DynamicIndexBuffer;

		FRayTracingInstanceCollector RayTracingInstanceCollector;

		TArray<FDynamicRayTracingPrimitive> DynamicRayTracingPrimitives;

		TArray<int32> RenderThreadDynamicPrimitives;
	};

	struct FGatherInstancesTaskData
	{
		UE_NONCOPYABLE(FGatherInstancesTaskData)

		FGatherInstancesTaskData(FScene& InScene, FViewInfo& InView, FSceneRenderingBulkObjectAllocator& InBulkAllocator, FSceneOptions InSceneOptions)
			: Scene(InScene)
			, View(InView)
			, SceneOptions(MoveTemp(InSceneOptions))
			, DynamicRayTracingInstancesContext(Scene, View, InBulkAllocator, SceneOptions)
		{

		}

		FScene& Scene;
		FViewInfo& View;
		FSceneOptions SceneOptions;

		// Filtered lists of relevant primitives
		TArray<int32> StaticPrimitivesIndices;
		TArray<int32> DynamicPrimitivesIndices;

		TArray<FRelevantPrimitive> StaticPrimitives;
		TArray<FRelevantPrimitive> CachedStaticPrimitives;

		TArray<FRelevantPrimitiveGatherContext> GatherContexts;

		FDynamicRayTracingInstancesContext DynamicRayTracingInstancesContext;

		// This task must complete before accessing StaticPrimitivesIndices/DynamicPrimitivesIndices.
		UE::Tasks::FTask GatherRelevantPrimitivesTask;

		// This task must complete before accessing StaticPrimitives/CachedStaticPrimitives.
		UE::Tasks::FTask GatherRelevantStaticPrimitivesTask;

		// This task must complete before accessing DynamicRayTracingInstancesContext.
		UE::Tasks::FTask GatherDynamicRayTracingInstancesTask;

		// This task must complete before PostRenderAllViewports().
		UE::Tasks::FTask AddUsedStreamingHandlesTask;

		UE::Tasks::FTaskEvent GatherDynamicRayTracingInstancesPrerequisites{ UE_SOURCE_LOCATION };
		bool bGatherDynamicRayTracingInstancesPrerequisitesTriggered = false;

		// Array of primitives that should update their cached ray tracing instances via FPrimitiveSceneInfo::UpdateCachedRaytracingData()
		TArray<FPrimitiveSceneInfo*> DirtyCachedRayTracingPrimitives; // TODO: remove this since it seems to be transient

		// Used coarse mesh streaming handles during the last TLAS build
		TArray<Nanite::CoarseMeshStreamingHandle> UsedCoarseMeshStreamingHandles; // TODO: Should be a set

		int32 NumCachedStaticInstances = 0;
		int32 NumCachedStaticDecalInstances = 0;
		int32 NumCachedStaticFarFieldInstances = 0;
		int32 NumCachedStaticVisibleShaderBindings = 0;

		bool bUsesLightingChannels = false;

		// Indicates that this object has been fully produced (for validation)
		bool bValid = false;
	};

	FGatherInstancesTaskData* CreateGatherInstancesTaskData(
		FSceneRenderingBulkObjectAllocator& InAllocator,
		FScene& Scene,
		FViewInfo& View,
		const FViewFamilyInfo& ViewFamily,
		EDiffuseIndirectMethod DiffuseIndirectMethod,
		EReflectionsMethod ReflectionsMethod)
	{
		return InAllocator.Create<FGatherInstancesTaskData>(Scene, View, InAllocator, FSceneOptions(Scene, ViewFamily, View, DiffuseIndirectMethod, ReflectionsMethod));
	}

	void OnRenderBegin(const FSceneRenderUpdateInputs& SceneUpdateInputs)
	{
		const ERayTracingType CurrentType = EnumHasAnyFlags(SceneUpdateInputs.CommonShowFlags, ESceneRenderCommonShowFlags::PathTracing) ? ERayTracingType::PathTracing : ERayTracingType::RayTracing;

		bool bNaniteCoarseMeshStreamingModeChanged = false;
#if WITH_EDITOR
		bNaniteCoarseMeshStreamingModeChanged = Nanite::FCoarseMeshStreamingManager::CheckStreamingMode();
#endif // WITH_EDITOR
		const bool bNaniteRayTracingModeChanged = Nanite::GRayTracingManager.CheckModeChanged();

		FScene& Scene = *SceneUpdateInputs.Scene;

		if (CurrentType != Scene.CachedRayTracingMeshCommandsType
			|| bNaniteCoarseMeshStreamingModeChanged
			|| bNaniteRayTracingModeChanged
			|| bUpdateCachedRayTracingState)
		{
			Scene.WaitForCacheRayTracingPrimitivesTask();

			// In some situations, we need to refresh the cached ray tracing mesh commands because they contain data about the currently bound shader. 
			// This operation is a bit expensive but only happens once as we transition between RT types which should be rare.
			Scene.CachedRayTracingMeshCommandsType = CurrentType;
			Scene.RefreshRayTracingMeshCommandCache();
			bUpdateCachedRayTracingState = false;
		}

		if (bRefreshRayTracingInstances)
		{
			Scene.WaitForCacheRayTracingPrimitivesTask();

			// In some situations, we need to refresh the cached ray tracing instance.
			// This assumes that cached instances will keep using the same LOD since CachedRayTracingMeshCommands is not recalculated
			// eg: Need to update PrimitiveRayTracingFlags
			// This operation is a bit expensive but only happens once as we transition between RT types which should be rare.
			Scene.RefreshRayTracingInstances();
			bRefreshRayTracingInstances = false;
		}

		if (bNaniteRayTracingModeChanged)
		{
			for (FViewInfo* View : SceneUpdateInputs.Views)
			{
				if (View->ViewState != nullptr && !View->bIsOfflineRender)
				{
					// don't invalidate in the offline case because we only get one attempt at rendering each sample
					View->ViewState->PathTracingInvalidate();
				}
			}
		}
	}	

	class FRaytracingShaderBindingLayout : public FShaderBindingLayoutContainer
	{
	public:
		static const FShaderBindingLayout& GetInstance(EBindingType BindingType)
		{
			static FRaytracingShaderBindingLayout Instance;
			return Instance.GetLayout(BindingType);
		}
	private:

		FRaytracingShaderBindingLayout()
		{
			// No special binding layout flags required
			EShaderBindingLayoutFlags ShaderBindingLayoutFlags = EShaderBindingLayoutFlags::None;

			// Add scene, view and nanite ray tracing as global/static uniform buffers
			TArray<FShaderParametersMetadata*> StaticUniformBuffers;
			StaticUniformBuffers.Add(FindUniformBufferStructByName(TEXT("Scene")));
			StaticUniformBuffers.Add(FindUniformBufferStructByName(TEXT("View")));
			StaticUniformBuffers.Add(FindUniformBufferStructByName(TEXT("NaniteRayTracing")));
			StaticUniformBuffers.Add(FindUniformBufferStructByName(TEXT("LumenHardwareRayTracingUniformBuffer")));

			BuildShaderBindingLayout(StaticUniformBuffers, ShaderBindingLayoutFlags, *this);
		}
	};

	const FShaderBindingLayout* GetShaderBindingLayout(EShaderPlatform ShaderPlatform)
	{
		if (RHIGetStaticShaderBindingLayoutSupport(ShaderPlatform) != ERHIStaticShaderBindingLayoutSupport::Unsupported)
		{
			// Should support bindless for raytracing at least
			// NOTE: checks disable checks because GConfig which is used to check 
			// runtime binding config can be modified in another thread at the same time
			//check(RHIGetRuntimeBindlessResourcesConfiguration(ShaderPlatform) != ERHIBindlessConfiguration::Disabled);
			//check(RHIGetRuntimeBindlessSamplersConfiguration(ShaderPlatform) != ERHIBindlessConfiguration::Disabled);

			// Retrieve the bindless shader binding table
			return &FRaytracingShaderBindingLayout::GetInstance(FShaderBindingLayoutContainer::EBindingType::Bindless);
		}

		// No binding table supported
		return nullptr;
	}

	TOptional<FScopedUniformBufferStaticBindings> BindStaticUniformBufferBindings(const FViewInfo& View, FRHIUniformBuffer* SceneUniformBuffer, FRHIUniformBuffer* NaniteRayTracingUniformBuffer, FRHICommandList& RHICmdList)
	{
		TOptional<FScopedUniformBufferStaticBindings> StaticUniformBufferScope;

		// Setup the static uniform buffers used by the RTPSO if enabled
		const FShaderBindingLayout* ShaderBindingLayout = GetShaderBindingLayout(View.GetShaderPlatform());
		if (ShaderBindingLayout)
		{			
			FUniformBufferStaticBindings StaticUniformBuffers(&ShaderBindingLayout->RHILayout);
			StaticUniformBuffers.AddUniformBuffer(View.ViewUniformBuffer.GetReference());
			StaticUniformBuffers.AddUniformBuffer(SceneUniformBuffer);
			StaticUniformBuffers.AddUniformBuffer(NaniteRayTracingUniformBuffer);
			StaticUniformBuffers.AddUniformBuffer(View.LumenHardwareRayTracingUniformBuffer.GetReference());

			StaticUniformBufferScope.Emplace(RHICmdList, StaticUniformBuffers);
		}

		return StaticUniformBufferScope;
	}

	struct FRayTracingMeshBatchWorkItem
	{
		const FPrimitiveSceneProxy* SceneProxy = nullptr;
		const FRHIRayTracingGeometry* RayTracingGeometry = nullptr;
		TArray<FMeshBatch> MeshBatchesOwned;
		TArrayView<const FMeshBatch> MeshBatchesView;
		FRayTracingSBTAllocation* SBTAllocation;

		TArrayView<const FMeshBatch> GetMeshBatches() const
		{
			if (MeshBatchesOwned.Num())
			{
				check(MeshBatchesView.Num() == 0);
				return TArrayView<const FMeshBatch>(MeshBatchesOwned);
			}
			else
			{
				check(MeshBatchesOwned.Num() == 0);
				return MeshBatchesView;
			}
		}
	};

	struct FRayTracingMeshBatchTaskPage
	{
		static constexpr uint32 MaxWorkItems = 128; // Try to keep individual pages small to avoid slow-path memory allocations

		FRayTracingMeshBatchWorkItem WorkItems[MaxWorkItems];
		uint32 NumWorkItems = 0;
		FRayTracingMeshBatchTaskPage* Next = nullptr;
	};

	void DispatchRayTracingMeshBatchTask(FSceneRenderingBulkObjectAllocator& InBulkAllocator, FScene& Scene, FViewInfo& View, FRayTracingMeshBatchTaskPage* MeshBatchTaskHead, uint32 NumPendingMeshBatches)
	{
		FDynamicRayTracingMeshCommandStorage* TaskDynamicCommandStorage = InBulkAllocator.Create<FDynamicRayTracingMeshCommandStorage>();
		View.DynamicRayTracingMeshCommandStoragePerTask.Add(TaskDynamicCommandStorage);

		FRayTracingShaderBindingDataOneFrameArray* TaskVisibleShaderBindings = InBulkAllocator.Create<FRayTracingShaderBindingDataOneFrameArray>();
		TaskVisibleShaderBindings->Reserve(NumPendingMeshBatches);
		View.DynamicRayTracingShaderBindingsPerTask.Add(TaskVisibleShaderBindings);

		View.AddDynamicRayTracingMeshBatchTaskList.Add(UE::Tasks::Launch(UE_SOURCE_LOCATION,
			[TaskDataHead = MeshBatchTaskHead, &View, &Scene, TaskDynamicCommandStorage, TaskVisibleShaderBindings]()
			{
				FTaskTagScope TaskTagScope(ETaskTag::EParallelRenderingThread);
				TRACE_CPUPROFILER_EVENT_SCOPE(RayTracingMeshBatchTask);
				FRayTracingMeshBatchTaskPage* Page = TaskDataHead;
				const int32 ExpectedMaxVisibleCommands = TaskVisibleShaderBindings->Max();
				while (Page)
				{
					for (uint32 ItemIndex = 0; ItemIndex < Page->NumWorkItems; ++ItemIndex)
					{
						const FRayTracingMeshBatchWorkItem& WorkItem = Page->WorkItems[ItemIndex];
						TArrayView<const FMeshBatch> MeshBatches = WorkItem.GetMeshBatches();
						for (int32 SegmentIndex = 0; SegmentIndex < MeshBatches.Num(); SegmentIndex++)
						{
							const FMeshBatch& MeshBatch = MeshBatches[SegmentIndex];
							FDynamicRayTracingMeshCommandContext CommandContext(
								*TaskDynamicCommandStorage, *TaskVisibleShaderBindings,
								WorkItem.RayTracingGeometry, SegmentIndex, WorkItem.SBTAllocation);
							FRayTracingMeshProcessor RayTracingMeshProcessor(&CommandContext, &Scene, &View, Scene.CachedRayTracingMeshCommandsType);
							RayTracingMeshProcessor.AddMeshBatch(MeshBatch, 1, WorkItem.SceneProxy);
						}
					}
					FRayTracingMeshBatchTaskPage* NextPage = Page->Next;
					Page = NextPage;
				}
				check(ExpectedMaxVisibleCommands <= TaskVisibleShaderBindings->Max());
			}, UE::Tasks::ETaskPriority::High));
	};

	FDynamicRayTracingInstancesContext::FDynamicRayTracingInstancesContext(FScene& Scene, FViewInfo& InView, FSceneRenderingBulkObjectAllocator& InBulkAllocator, const FSceneOptions& InSceneOptions)
		: Scene(Scene)
		, View(InView)
		, BulkAllocator(InBulkAllocator)
		, SceneOptions(InSceneOptions)
		, bTrackReferencedGeometryGroups(IsRayTracingUsingReferenceBasedResidency())
		, RHICmdList(new FRHICommandList(FRHIGPUMask::All()))
		, DynamicVertexBuffer(*RHICmdList)
		, DynamicIndexBuffer(*RHICmdList)
		, RayTracingInstanceCollector(Scene.GetFeatureLevel(), InBulkAllocator, &View, bTrackReferencedGeometryGroups)
	{
		RHICmdList->SwitchPipeline(ERHIPipeline::Graphics);

		RayTracingInstanceCollector.Start(
			*RHICmdList,
			DynamicVertexBuffer,
			DynamicIndexBuffer,
			FSceneRenderer::DynamicReadBufferForRayTracing
		);

		RayTracingInstanceCollector.AddViewMeshArrays(
			&View,
			nullptr,
			nullptr,
			&View.RayTracingDynamicPrimitiveCollector
#if UE_ENABLE_DEBUG_DRAWING
			, nullptr
#endif
		);
	}

	void FDynamicRayTracingInstancesContext::GatherDynamicRayTracingInstances_Internal(int32 PrimitiveIndex)
	{
		FPrimitiveSceneProxy* SceneProxy = Scene.PrimitiveSceneProxies[PrimitiveIndex];
		FPrimitiveSceneInfo* SceneInfo = Scene.Primitives[PrimitiveIndex];
		const FPersistentPrimitiveIndex PersistentPrimitiveIndex = SceneInfo->GetPersistentIndex();

		RayTracingInstanceCollector.SetPrimitive(SceneProxy, FHitProxyId::InvisibleHitProxyId);

		int32 BaseRayTracingInstance = RayTracingInstanceCollector.RayTracingInstances.Num();
		int32 BaseGeometryToUpdate = RayTracingInstanceCollector.RayTracingGeometriesToUpdate.Num();

		if (SceneOptions.bTranslucentGeometry || SceneProxy->IsOpaqueOrMasked())
		{
			SceneProxy->GetDynamicRayTracingInstances(RayTracingInstanceCollector);
		}

		FDynamicRayTracingPrimitive Tmp;
		Tmp.PrimitiveIndex = PrimitiveIndex;
		Tmp.InstancesRange = TRange<int32>(BaseRayTracingInstance, RayTracingInstanceCollector.RayTracingInstances.Num());
		Tmp.GeometriesToUpdateRange = TRange<int32>(BaseGeometryToUpdate, RayTracingInstanceCollector.RayTracingGeometriesToUpdate.Num());

		DynamicRayTracingPrimitives.Add(MoveTemp(Tmp));
	}

	void FDynamicRayTracingInstancesContext::GatherDynamicRayTracingInstances(TConstArrayView<int32> InDynamicPrimitives)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(RayTracing_GatherDynamicRayTracingInstances);

		DynamicRayTracingPrimitives.Reserve(DynamicRayTracingPrimitives.Num() + InDynamicPrimitives.Num());

		if (!IsParallelGatherDynamicRayTracingInstancesEnabled())
		{
			RenderThreadDynamicPrimitives = InDynamicPrimitives;
			return;
		}

		// TODO: Could filter primitives whose proxy supports ParallelGDRTI during GatherRayTracingRelevantPrimitives_Parallel

		for (int32 PrimitiveIndex : InDynamicPrimitives)
		{
			FPrimitiveSceneProxy* SceneProxy = Scene.PrimitiveSceneProxies[PrimitiveIndex];

			if (SceneProxy->SupportsParallelGDRTI())
			{
				GatherDynamicRayTracingInstances_Internal(PrimitiveIndex);
			}
			else
			{
				RenderThreadDynamicPrimitives.Add(PrimitiveIndex);
			}
		}
	}

	void FDynamicRayTracingInstancesContext::GatherDynamicRayTracingInstances_RenderThread()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(RayTracing_GatherDynamicRayTracingInstances_RenderThread);

		check(IsInRenderingThread());

		for (int32 PrimitiveIndex : RenderThreadDynamicPrimitives)
		{
			GatherDynamicRayTracingInstances_Internal(PrimitiveIndex);
		}

		RenderThreadDynamicPrimitives.Empty();
	}

	void FDynamicRayTracingInstancesContext::Finish(FRHICommandListImmediate& InRHICmdList)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(RayTracing_GatherDynamicRayTracingInstances_Finish);

		// TODO: Could process RayTracingGeometriesToUpdate in parallel thread after merging multiple tasks

		FRayTracingDynamicGeometryUpdateManager* DynamicGeometryUpdateManager = Scene.GetRayTracingDynamicGeometryUpdateManager();

		// Can't use RayTracingGeometriesToUpdate directly because need SceneProxy and PersistentPrimitiveIndex
		// TODO: Move those parameters into FRayTracingDynamicGeometryUpdateParams
		for (const FDynamicRayTracingPrimitive& DynamicRayTracingPrimitive : DynamicRayTracingPrimitives)
		{
			FPrimitiveSceneProxy* SceneProxy = Scene.PrimitiveSceneProxies[DynamicRayTracingPrimitive.PrimitiveIndex];
			FPrimitiveSceneInfo* SceneInfo = Scene.Primitives[DynamicRayTracingPrimitive.PrimitiveIndex];
			const FPersistentPrimitiveIndex PersistentPrimitiveIndex = SceneInfo->GetPersistentIndex();

			for (int32 Index = DynamicRayTracingPrimitive.GeometriesToUpdateRange.GetLowerBoundValue(); Index < DynamicRayTracingPrimitive.GeometriesToUpdateRange.GetUpperBoundValue(); ++Index)
			{				
				DynamicGeometryUpdateManager->AddDynamicGeometryToUpdate(
					*RHICmdList,
					&Scene,
					&View,
					SceneProxy,
					RayTracingInstanceCollector.RayTracingGeometriesToUpdate[Index],
					PersistentPrimitiveIndex.Index
				);
			}
		}

		RayTracingInstanceCollector.RayTracingGeometriesToUpdate.Empty();

		if (bTrackReferencedGeometryGroups)
		{
			// TODO: Could run in parallel thread if properly synchronized with static ray tracing instances tasks
			((FRayTracingGeometryManager*)GRayTracingGeometryManager)->AddReferencedGeometryGroups(RayTracingInstanceCollector.ReferencedGeometryGroups);
			((FRayTracingGeometryManager*)GRayTracingGeometryManager)->AddReferencedGeometryGroupsForDynamicUpdate(RayTracingInstanceCollector.ReferencedGeometryGroupsForDynamicUpdate);

			RayTracingInstanceCollector.ReferencedGeometryGroups.Empty();
			RayTracingInstanceCollector.ReferencedGeometryGroupsForDynamicUpdate.Empty();
		}
		else
		{
			check(RayTracingInstanceCollector.ReferencedGeometryGroups.IsEmpty());
			check(RayTracingInstanceCollector.ReferencedGeometryGroupsForDynamicUpdate.IsEmpty());
		}

		RayTracingInstanceCollector.Finish();

		DynamicVertexBuffer.Commit();
		DynamicIndexBuffer.Commit();
		RHICmdList->FinishRecording();

		FSceneRenderer::DynamicReadBufferForRayTracing.Commit(InRHICmdList);

		InRHICmdList.QueueAsyncCommandListSubmit(RHICmdList);
	}

	void FDynamicRayTracingInstancesContext::CollectRDGResources(FRDGBuilder& RDGBuilder)
	{
		for (FRDGPooledBuffer* PooledBuffer : RayTracingInstanceCollector.RDGPooledBuffers)
		{
			FRDGBufferRef RDGBuffer = RDGBuilder.RegisterExternalBuffer(PooledBuffer);
			View.DynamicRayTracingRDGBuffers.Add(RDGBuffer);
		}
	}

	void FDynamicRayTracingInstancesContext::AddInstancesToScene(FRayTracingScene& RayTracingScene, FRayTracingShaderBindingTable& RayTracingSBT, int64 SharedBufferGenerationID)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(RayTracing_AddDynamicInstancesToScene);

		const float CurrentWorldTime = View.Family->Time.GetWorldTimeSeconds();

		const bool bParallelMeshBatchSetup = GRayTracingParallelMeshBatchSetup && FApp::ShouldUseThreadingForPerformance();

		FRayTracingMeshBatchTaskPage* MeshBatchTaskHead = nullptr;
		FRayTracingMeshBatchTaskPage* MeshBatchTaskPage = nullptr;
		uint32 NumPendingMeshBatches = 0;
		const uint32 RayTracingParallelMeshBatchSize = GRayTracingParallelMeshBatchSize;

		auto KickRayTracingMeshBatchTask = [this, &MeshBatchTaskHead, &MeshBatchTaskPage, &NumPendingMeshBatches]()
			{
				if (MeshBatchTaskHead)
				{
					DispatchRayTracingMeshBatchTask(BulkAllocator, Scene, View, MeshBatchTaskHead, NumPendingMeshBatches);
				}

				MeshBatchTaskHead = nullptr;
				MeshBatchTaskPage = nullptr;
				NumPendingMeshBatches = 0;
			};

		const int32 ViewDynamicPrimitiveId = View.RayTracingDynamicPrimitiveCollector.GetPrimitiveIdRange().GetLowerBoundValue();
		const int32 ViewInstanceSceneDataOffset = View.RayTracingDynamicPrimitiveCollector.GetInstanceSceneDataOffset();

		for (const FDynamicRayTracingPrimitive& DynamicRayTracingPrimitive : DynamicRayTracingPrimitives)
		{
			const int32 PrimitiveIndex = DynamicRayTracingPrimitive.PrimitiveIndex;
			FPrimitiveSceneProxy* SceneProxy = Scene.PrimitiveSceneProxies[PrimitiveIndex];
			FPrimitiveSceneInfo* SceneInfo = Scene.Primitives[PrimitiveIndex];
			const FPersistentPrimitiveIndex PersistentPrimitiveIndex = SceneInfo->GetPersistentIndex();

			TArrayView<FRayTracingInstance> TempRayTracingInstances = MakeArrayView(
				RayTracingInstanceCollector.RayTracingInstances.GetData() + DynamicRayTracingPrimitive.InstancesRange.GetLowerBoundValue(),
				DynamicRayTracingPrimitive.InstancesRange.Size<int32>());

			if (TempRayTracingInstances.Num() > 0)
			{
				for (FRayTracingInstance& Instance : TempRayTracingInstances)
				{
					const FRayTracingGeometry* Geometry = Instance.Geometry;

					if (!ensureMsgf(Geometry->DynamicGeometrySharedBufferGenerationID == FRayTracingGeometry::NonSharedVertexBuffers
						|| Geometry->DynamicGeometrySharedBufferGenerationID == SharedBufferGenerationID,
						TEXT("GenerationID %lld, but expected to be %lld or %lld. Geometry debug name: '%s'. ")
						TEXT("When shared vertex buffers are used, the contents is expected to be written every frame. ")
						TEXT("Possibly AddDynamicMeshBatchForGeometryUpdate() was not called for this geometry."),
						Geometry->DynamicGeometrySharedBufferGenerationID, SharedBufferGenerationID, FRayTracingGeometry::NonSharedVertexBuffers,
						*Geometry->Initializer.DebugName.ToString()))
					{
						continue;
					}

					((FRayTracingGeometryManager*)GRayTracingGeometryManager)->AddReferencedGeometry(Geometry);

					if (Geometry->IsEvicted())
					{
						continue;
					}

					// If geometry still has pending build request then add to list which requires a force build
					if (Geometry->HasPendingBuildRequest())
					{
						RayTracingScene.GeometriesToBuild.Add(Geometry);
					}

					// Validate the material/segment counts
					if (!ensureMsgf(Instance.GetMaterials().Num() == Geometry->Initializer.Segments.Num() ||
						(Geometry->Initializer.Segments.Num() == 0 && Instance.GetMaterials().Num() == 1),
						TEXT("Ray tracing material assignment validation failed for geometry '%s'. "
							"Instance.GetMaterials().Num() = %d, Geometry->Initializer.Segments.Num() = %d."),
						*Geometry->Initializer.DebugName.ToString(), Instance.GetMaterials().Num(),
						Geometry->Initializer.Segments.Num()))
					{
						continue;
					}

					if (Instance.bInstanceMaskAndFlagsDirty || SceneInfo->bCachedRayTracingInstanceMaskAndFlagsDirty)
					{
						// Build InstanceMaskAndFlags since the data in SceneInfo is not up to date

						FRayTracingMaskAndFlags InstanceMaskAndFlags;

						if (Instance.GetMaterials().IsEmpty())
						{
							// If the material list is empty, explicitly set the mask to 0 so it will not be added in the raytracing scene
							InstanceMaskAndFlags.Mask = 0;
						}
						else
						{
							InstanceMaskAndFlags = BuildRayTracingInstanceMaskAndFlags(Instance, *SceneProxy);
						}

						SceneInfo->UpdateCachedRayTracingInstanceMaskAndFlags(InstanceMaskAndFlags);
					}

					if (!SceneOptions.bTranslucentGeometry && SceneInfo->bCachedRayTracingInstanceAllSegmentsTranslucent)
					{
						continue;
					}

					// TODO: Do we want to support dynamic instances in far field?
					const bool bNeedMainInstance = !SceneInfo->bCachedRayTracingInstanceAllSegmentsDecal;

					// if primitive has mixed decal and non-decal segments we need to have two ray tracing instances
					// one containing non-decal segments and the other with decal segments
					// masking of segments is done using "hidden" hitgroups
					// TODO: Debug Visualization to highlight primitives using this?
					const bool bNeedDecalInstance = SceneInfo->bCachedRayTracingInstanceAnySegmentsDecal && !ShouldExcludeDecals();

					if (ShouldExcludeDecals() && SceneInfo->bCachedRayTracingInstanceAllSegmentsDecal)
					{
						continue;
					}

					int32 PrimitiveId = PersistentPrimitiveIndex.Index;
					int32 InstanceSceneDataOffset = SceneInfo->GetInstanceSceneDataOffset();

					if (Instance.Materials.Num() > 0 && Instance.Materials[0].Elements.Num() > 0 && Instance.Materials[0].Elements[0].DynamicPrimitiveData != nullptr)
					{
						check(Instance.NumTransforms == Instance.Materials[0].Elements[0].NumInstances);
						PrimitiveId = ViewDynamicPrimitiveId + Instance.Materials[0].Elements[0].DynamicPrimitiveIndex;
						InstanceSceneDataOffset = ViewInstanceSceneDataOffset + Instance.Materials[0].Elements[0].DynamicPrimitiveInstanceSceneDataOffset;
					}

					const bool bUsesLightingChannels = SceneOptions.bLightingChannelsUsingAHS && SceneInfo->bUsesLightingChannels;

					FRayTracingGeometryInstance RayTracingInstance;
					RayTracingInstance.GeometryRHI = Geometry->GetRHI();
					checkf(RayTracingInstance.GeometryRHI, TEXT("Ray tracing instance must have a valid geometry."));
					RayTracingInstance.DefaultUserData = InstanceSceneDataOffset;
					RayTracingInstance.bIncrementUserDataPerInstance = true;
					RayTracingInstance.bApplyLocalBoundsTransform = Instance.bApplyLocalBoundsTransform;
					RayTracingInstance.Mask = SceneInfo->GetCachedRayTracingInstance().Mask;
					RayTracingInstance.Flags = SceneInfo->GetCachedRayTracingInstance().Flags;
					AddRayTracingInstanceFlagOverrides(RayTracingInstance.Flags, bUsesLightingChannels);

					if (!Instance.GetPrimitiveInstanceIndices().IsEmpty())
					{
						TConstArrayView<uint32> PrimitiveInstanceIndices = Instance.GetPrimitiveInstanceIndices();

						// Convert from instance indices to InstanceSceneDataOffsets
						TArrayView<uint32> InstanceSceneDataOffsets = RayTracingScene.Allocate<uint32>(PrimitiveInstanceIndices.Num());
						for (int32 InstanceIndex = 0; InstanceIndex < PrimitiveInstanceIndices.Num(); ++InstanceIndex)
						{
							InstanceSceneDataOffsets[InstanceIndex] = SceneInfo->GetInstanceSceneDataOffset() + PrimitiveInstanceIndices[InstanceIndex];
						}

						RayTracingInstance.InstanceSceneDataOffsets = InstanceSceneDataOffsets;
						RayTracingInstance.UserData = InstanceSceneDataOffsets;
						RayTracingInstance.NumTransforms = PrimitiveInstanceIndices.Num();
					}
					else if (!Instance.GetTransforms().IsEmpty())
					{
						TConstArrayView<FMatrix> TransformsView;
						if (Instance.OwnsTransforms())
						{
							// Slow path: copy transforms to the owned storage
							checkf(Instance.InstanceTransformsView.Num() == 0, TEXT("InstanceTransformsView is expected to be empty if using InstanceTransforms"));
							TArrayView<FMatrix> SceneOwnedTransforms = RayTracingScene.Allocate<FMatrix>(Instance.InstanceTransforms.Num());
							FMemory::Memcpy(SceneOwnedTransforms.GetData(), Instance.InstanceTransforms.GetData(), Instance.InstanceTransforms.Num() * sizeof(RayTracingInstance.Transforms[0]));
							static_assert(std::is_same_v<decltype(SceneOwnedTransforms[0]), decltype(Instance.InstanceTransforms[0])>, "Unexpected transform type");

							TransformsView = SceneOwnedTransforms;
						}
						else
						{
							// Fast path: just reference persistently-allocated transforms and avoid a copy
							checkf(Instance.InstanceTransforms.Num() == 0, TEXT("InstanceTransforms is expected to be empty if using InstanceTransformsView"));
							TransformsView = Instance.InstanceTransformsView;
						}

						RayTracingInstance.NumTransforms = TransformsView.Num();
						RayTracingInstance.Transforms = TransformsView;
					}
					else
					{
						// If array of transforms was not provided, get the instance transforms from GPU Scene
						RayTracingInstance.NumTransforms = Instance.NumTransforms;
						RayTracingInstance.BaseInstanceSceneDataOffset = InstanceSceneDataOffset;
					}

					ERayTracingShaderBindingLayerMask ActiveLayers = ERayTracingShaderBindingLayerMask::None;
					if (bNeedMainInstance)
					{
						EnumAddFlags(ActiveLayers, ERayTracingShaderBindingLayerMask::Base);
					}
					if (bNeedDecalInstance)
					{
						EnumAddFlags(ActiveLayers, ERayTracingShaderBindingLayerMask::Decals);
					}

					FRayTracingSBTAllocation* SBTAllocation = RayTracingSBT.AllocateDynamicRange(ActiveLayers, Instance.GetMaterials().Num());
					if (bNeedMainInstance)
					{
						RayTracingInstance.InstanceContributionToHitGroupIndex = SBTAllocation->GetInstanceContributionToHitGroupIndex(ERayTracingShaderBindingLayer::Base);

						ERayTracingPrimitiveFlags Flags = Scene.PrimitiveRayTracingFlags[PrimitiveIndex];
						RayTracingScene.AddInstance(RayTracingInstance, EnumHasAllFlags(Flags, ERayTracingPrimitiveFlags::FarField) ? ERayTracingSceneLayer::FarField : ERayTracingSceneLayer::Base, SceneProxy, true, Geometry->GetGeometryHandle());
					}

					if (bNeedDecalInstance)
					{
						FRayTracingGeometryInstance DecalRayTracingInstance = RayTracingInstance;
						DecalRayTracingInstance.InstanceContributionToHitGroupIndex = SBTAllocation->GetInstanceContributionToHitGroupIndex(ERayTracingShaderBindingLayer::Decals);
						RayTracingScene.AddInstance(MoveTemp(DecalRayTracingInstance), ERayTracingSceneLayer::Decals, SceneProxy, true, Geometry->GetGeometryHandle());
					}

					if (bNeedMainInstance || bNeedDecalInstance)
					{
						RayTracingScene.bUsesLightingChannels |= bUsesLightingChannels;
					}

					if (bParallelMeshBatchSetup)
					{
						if (NumPendingMeshBatches >= RayTracingParallelMeshBatchSize)
						{
							KickRayTracingMeshBatchTask();
						}

						if (MeshBatchTaskPage == nullptr || MeshBatchTaskPage->NumWorkItems == FRayTracingMeshBatchTaskPage::MaxWorkItems)
						{
							FRayTracingMeshBatchTaskPage* NextPage = BulkAllocator.Create<FRayTracingMeshBatchTaskPage>();
							if (MeshBatchTaskHead == nullptr)
							{
								MeshBatchTaskHead = NextPage;
							}
							if (MeshBatchTaskPage)
							{
								MeshBatchTaskPage->Next = NextPage;
							}
							MeshBatchTaskPage = NextPage;
						}

						FRayTracingMeshBatchWorkItem& WorkItem = MeshBatchTaskPage->WorkItems[MeshBatchTaskPage->NumWorkItems];
						MeshBatchTaskPage->NumWorkItems++;

						NumPendingMeshBatches += Instance.GetMaterials().Num();

						if (Instance.OwnsMaterials())
						{
							Swap(WorkItem.MeshBatchesOwned, Instance.Materials);
						}
						else
						{
							WorkItem.MeshBatchesView = Instance.MaterialsView;
						}

						WorkItem.SceneProxy = SceneProxy;
						WorkItem.RayTracingGeometry = Geometry->GetRHI();
						WorkItem.SBTAllocation = SBTAllocation;
					}
					else
					{
						TArrayView<const FMeshBatch> InstanceMaterials = Instance.GetMaterials();
						for (int32 SegmentIndex = 0; SegmentIndex < InstanceMaterials.Num(); SegmentIndex++)
						{
							const FMeshBatch& MeshBatch = InstanceMaterials[SegmentIndex];
							FDynamicRayTracingMeshCommandContext CommandContext(View.DynamicRayTracingMeshCommandStorage, View.VisibleRayTracingShaderBindings, Geometry->GetRHI(), SegmentIndex, SBTAllocation);
							FRayTracingMeshProcessor RayTracingMeshProcessor(&CommandContext, &Scene, &View, Scene.CachedRayTracingMeshCommandsType);
							RayTracingMeshProcessor.AddMeshBatch(MeshBatch, 1, SceneProxy);
						}
					}
				}

				if (CVarRayTracingDynamicGeometryLastRenderTimeUpdateDistance.GetValueOnRenderThread() > 0.0f)
				{
					if (FVector::Distance(SceneProxy->GetActorPosition(), View.ViewMatrices.GetViewOrigin()) < CVarRayTracingDynamicGeometryLastRenderTimeUpdateDistance.GetValueOnRenderThread())
					{
						// Update LastRenderTime for components so that visibility based ticking (like skeletal meshes) can get updated
						// We are only doing this for dynamic geometries now
						SceneInfo->LastRenderTime = CurrentWorldTime;
						SceneInfo->UpdateComponentLastRenderTime(CurrentWorldTime, /*bUpdateLastRenderTimeOnScreen=*/true);
					}
				}
			}
		}

		KickRayTracingMeshBatchTask();

		RayTracingInstanceCollector.RayTracingInstances.Empty();
	}

	void GatherRelevantPrimitives(FGatherInstancesTaskData& TaskData, bool bUsingReferenceBasedResidency)
	{
		FScene& Scene = TaskData.Scene;
		FViewInfo& View = TaskData.View;

		const bool bGameView = View.bIsGameView || View.Family->EngineShowFlags.Game;

		bool bPerformRayTracing = View.State != nullptr && !View.bIsReflectionCapture && View.IsRayTracingAllowedForView();
		if (bPerformRayTracing)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(GatherRayTracingRelevantPrimitives);

			struct FGatherRelevantPrimitivesContext
			{
				TChunkedArray<int32> StaticPrimitives;
				TChunkedArray<int32> DynamicPrimitives;
				TChunkedArray<Nanite::CoarseMeshStreamingHandle> UsedCoarseMeshStreamingHandles;
				TChunkedArray<FPrimitiveSceneInfo*> DirtyCachedRayTracingPrimitives;

				TSet<RayTracing::FGeometryGroupHandle> ReferencedGeometryGroups;
			};

			TArray<FGatherRelevantPrimitivesContext> Contexts;
			const int32 MinBatchSize = 128;
			ParallelForWithTaskContext(
				TEXT("GatherRayTracingRelevantPrimitives_Parallel"),
				Contexts,
				Scene.PrimitiveSceneProxies.Num(),
				MinBatchSize,
				[&Scene, &View, bGameView, bUsingReferenceBasedResidency](FGatherRelevantPrimitivesContext& Context, int32 PrimitiveIndex)
			{
				// Get primitive visibility state from culling
				if (!View.PrimitiveRayTracingVisibilityMap[PrimitiveIndex])
				{
					return;
				}

				const ERayTracingPrimitiveFlags Flags = Scene.PrimitiveRayTracingFlags[PrimitiveIndex];

				check(!EnumHasAnyFlags(Flags, ERayTracingPrimitiveFlags::Exclude));

				const FPrimitiveSceneInfo* SceneInfo = Scene.Primitives[PrimitiveIndex];

				const bool bRetainWhileHidden = SceneInfo->bCastHiddenShadow || SceneInfo->bAffectIndirectLightingWhileHidden;

				// #dxr_todo: ray tracing in scene captures should re-use the persistent RT scene. (UE-112448)
				if (View.bIsSceneCapture && !SceneInfo->bIsVisibleInSceneCaptures && !bRetainWhileHidden)
				{
					return;
				}

				if (!View.bIsSceneCapture && SceneInfo->bIsVisibleInSceneCapturesOnly)
				{
					return;
				}

				// Some primitives should only be visible editor mode, however far field geometry 
				// and geometry that retains visibility while hidden (affect indirect while hidden
				// or hidden shadow casters) must still always be added to the RT scene.
				if (bGameView && !SceneInfo->bDrawInGame && !SceneInfo->bRayTracingFarField && !bRetainWhileHidden)
				{
					return;
				}

				// Marked visible and used after point, check if streaming then mark as used in the TLAS (so it can be streamed in)
				if (EnumHasAnyFlags(Flags, ERayTracingPrimitiveFlags::Streaming))
				{
					check(SceneInfo->CoarseMeshStreamingHandle != INDEX_NONE);
					Context.UsedCoarseMeshStreamingHandles.AddElement(SceneInfo->CoarseMeshStreamingHandle);
				}

				if (bUsingReferenceBasedResidency && SceneInfo->RayTracingGeometryGroupHandle != INDEX_NONE)
				{
					Context.ReferencedGeometryGroups.Add(SceneInfo->RayTracingGeometryGroupHandle);
				}

				// Is the cached data dirty?
				// eg: mesh was streamed in/out
				if (SceneInfo->bCachedRaytracingDataDirty)
				{
					Context.DirtyCachedRayTracingPrimitives.AddElement(Scene.Primitives[PrimitiveIndex]);
				}

				if (EnumHasAnyFlags(Flags, ERayTracingPrimitiveFlags::Skip))
				{
					return;
				}

				if (EnumHasAnyFlags(Flags, ERayTracingPrimitiveFlags::Dynamic))
				{
					checkf(!EnumHasAllFlags(Flags, ERayTracingPrimitiveFlags::CacheInstances), TEXT("Only static primitives are expected to use CacheInstances flag."));

					if (View.Family->EngineShowFlags.SkeletalMeshes) // TODO: Fix this check
					{
						Context.DynamicPrimitives.AddElement(PrimitiveIndex);
					}
				}
				else if (View.Family->EngineShowFlags.StaticMeshes)
				{
					Context.StaticPrimitives.AddElement(PrimitiveIndex);
				}
			});

			if (Contexts.Num() > 0)
			{
				SCOPED_NAMED_EVENT(GatherRayTracingRelevantPrimitives_Merge, FColor::Emerald);

				int32 NumStaticPrimitives = 0;
				int32 NumDynamicPrimitives = 0;
				int32 NumUsedCoarseMeshStreamingHandles = 0;
				int32 NumDirtyCachedRayTracingPrimitives = 0;

				for (auto& Context : Contexts)
				{
					NumStaticPrimitives += Context.StaticPrimitives.Num();
					NumDynamicPrimitives += Context.DynamicPrimitives.Num();
					NumUsedCoarseMeshStreamingHandles += Context.UsedCoarseMeshStreamingHandles.Num();
					NumDirtyCachedRayTracingPrimitives += Context.DirtyCachedRayTracingPrimitives.Num();
				}

				TaskData.StaticPrimitivesIndices.Reserve(NumStaticPrimitives);
				TaskData.DynamicPrimitivesIndices.Reserve(NumDynamicPrimitives);
				TaskData.UsedCoarseMeshStreamingHandles.Reserve(NumUsedCoarseMeshStreamingHandles);
				TaskData.DirtyCachedRayTracingPrimitives.Reserve(NumDirtyCachedRayTracingPrimitives);

				for (auto& Context : Contexts)
				{
					Context.StaticPrimitives.CopyToLinearArray(TaskData.StaticPrimitivesIndices);
					Context.DynamicPrimitives.CopyToLinearArray(TaskData.DynamicPrimitivesIndices);
					Context.UsedCoarseMeshStreamingHandles.CopyToLinearArray(TaskData.UsedCoarseMeshStreamingHandles);
					Context.DirtyCachedRayTracingPrimitives.CopyToLinearArray(TaskData.DirtyCachedRayTracingPrimitives);

					if(bUsingReferenceBasedResidency)
					{
						((FRayTracingGeometryManager*)GRayTracingGeometryManager)->AddReferencedGeometryGroups(Context.ReferencedGeometryGroups);
					}
				}
			}
		}

		// TODO: could be moved to GatherRelevantStaticPrimitivesTask so that launching GatherDynamicRayTracingInstancesTask isn't delayed
		FPrimitiveSceneInfo::UpdateCachedRaytracingData(&TaskData.Scene, TaskData.DirtyCachedRayTracingPrimitives);
	}

	void GatherRelevantStaticPrimitives(FGatherInstancesTaskData& TaskData, float GlobalLODScale, int32 ForcedLODLevel, bool bUsingReferenceBasedResidency)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(GatherRayTracingRelevantStaticPrimitives);

		struct FRelevantStaticPrimitivesContext
		{
			FRelevantStaticPrimitivesContext(int32 InContextIndex) : ContextIndex(InContextIndex) {}

			TChunkedArray<FRelevantPrimitive> StaticPrimitives;
			TChunkedArray<FRelevantPrimitive> CachedStaticPrimitives;
			TChunkedArray<const FPrimitiveSceneInfo*> VisibleNaniteRayTracingPrimitives;

			int32 NumCachedStaticInstances = 0;
			int32 NumCachedStaticDecalInstances = 0;
			int32 NumCachedStaticFarFieldInstances = 0;
			int32 NumCachedStaticVisibleShaderBindings = 0;

			bool bUsesLightingChannels = false;

			int32 ContextIndex = INDEX_NONE;
		};

		TArray<FRelevantStaticPrimitivesContext> Contexts;
		ParallelForWithTaskContext(
			TEXT("GatherRayTracingRelevantStaticPrimitives_Parallel"),
			Contexts,
			TaskData.StaticPrimitivesIndices.Num(),
			[](int32 ContextIndex, int32 NumContexts) { return ContextIndex; },
			[&Scene = TaskData.Scene, &View = TaskData.View, GlobalLODScale, ForcedLODLevel, &StaticPrimitivesIndices = TaskData.StaticPrimitivesIndices,
			bUsingReferenceBasedResidency, bLightingChannelsUsingAHS = TaskData.SceneOptions.bLightingChannelsUsingAHS](FRelevantStaticPrimitivesContext& Context, int32 ItemIndex)
			{
				const int32 PrimitiveIndex = StaticPrimitivesIndices[ItemIndex];

				const FPrimitiveSceneProxy* SceneProxy = Scene.PrimitiveSceneProxies[PrimitiveIndex];
				const FPrimitiveSceneInfo* SceneInfo = Scene.Primitives[PrimitiveIndex];
				const ERayTracingPrimitiveFlags Flags = Scene.PrimitiveRayTracingFlags[PrimitiveIndex];

				const bool bUsingNaniteRayTracing = (Nanite::GetRayTracingMode() != Nanite::ERayTracingMode::Fallback) && SceneProxy->IsNaniteMesh();

				if (bUsingNaniteRayTracing)
				{
					Context.VisibleNaniteRayTracingPrimitives.AddElement(SceneInfo);
				}

				int8 LODIndex = 0;

				if (EnumHasAnyFlags(Flags, ERayTracingPrimitiveFlags::ComputeLOD))
				{
					const FPrimitiveBounds& Bounds = Scene.PrimitiveBounds[PrimitiveIndex];

					const int8 CurFirstLODIdx = SceneProxy->GetCurrentFirstLODIdx_RenderThread();
					check(CurFirstLODIdx >= 0);

					float MeshScreenSizeSquared = 0;
					float LODScale = GlobalLODScale * View.LODDistanceFactor;
					FLODMask LODToRender = ComputeLODForMeshes(SceneInfo->StaticMeshRelevances, View, Bounds.BoxSphereBounds.Origin, Bounds.BoxSphereBounds.SphereRadius, ForcedLODLevel, MeshScreenSizeSquared, CurFirstLODIdx, LODScale, true);

					LODIndex = LODToRender.GetRayTracedLOD();

					// TODO: Handle !RayTracingProxy->bUsingRenderingLODs
				}

				if (EnumHasAnyFlags(Flags, ERayTracingPrimitiveFlags::CacheInstances))
				{
					const FRayTracingGeometryInstance& CachedRayTracingInstance = SceneInfo->GetCachedRayTracingInstance();
					if (bUsingNaniteRayTracing)
					{
						if (CachedRayTracingInstance.GeometryRHI == nullptr)
						{
							// Nanite ray tracing geometry not ready yet, doesn't include primitive in ray tracing scene
							return;
						}
					}
					else
					{
						// Currently IsCachedRayTracingGeometryValid() can only be called for non-nanite geometries
						checkf(SceneInfo->IsCachedRayTracingGeometryValid(), TEXT("Cached ray tracing instance is expected to be valid. Was mesh LOD streamed but cached data was not invalidated?"));
						checkf(CachedRayTracingInstance.GeometryRHI, TEXT("Ray tracing instance must have a valid geometry."));
					}

					if (ShouldExcludeDecals() && SceneInfo->bCachedRayTracingInstanceAllSegmentsDecal)
					{
						return;
					}

					checkf(CachedRayTracingInstance.GeometryRHI, TEXT("Ray tracing instance must have a valid geometry."));

					// For primitives with ERayTracingPrimitiveFlags::CacheInstances flag we only cache the instance/mesh commands of the current LOD
					// (see FPrimitiveSceneInfo::UpdateCachedRayTracingInstance(...) and CacheRayTracingPrimitive(...))
					check(!EnumHasAnyFlags(Flags, ERayTracingPrimitiveFlags::ComputeLOD));
					LODIndex = 0;

					const FPrimitiveSceneInfo::FRayTracingLODData& RTLODData = SceneInfo->GetRayTracingLODData(LODIndex);
					ensure(RTLODData.SBTAllocation);
					if (RTLODData.SBTAllocation == nullptr)
					{
						return;
					}

					const bool bUsesLightingChannels = bLightingChannelsUsingAHS && SceneInfo->bUsesLightingChannels;
					Context.bUsesLightingChannels |= bUsesLightingChannels;

					FRelevantPrimitive* RelevantPrimitive = new (Context.CachedStaticPrimitives) FRelevantPrimitive();
					RelevantPrimitive->PrimitiveIndex = PrimitiveIndex;
					RelevantPrimitive->PersistentPrimitiveIndex = SceneInfo->GetPersistentIndex();
					RelevantPrimitive->RayTracingGeometry = SceneInfo->GetCachedRayTracingGeometry();
					RelevantPrimitive->bUsesLightingChannels = bUsesLightingChannels;

					ensureMsgf(!SceneInfo->bCachedRaytracingDataDirty, TEXT("Cached ray tracing instances must be up-to-date at this point"));

					RelevantPrimitive->CachedRayTracingInstance = &CachedRayTracingInstance;
					RelevantPrimitive->CachedMeshCommandFlags.bAnySegmentsDecal = SceneInfo->bCachedRayTracingInstanceAnySegmentsDecal;
					RelevantPrimitive->CachedMeshCommandFlags.bAllSegmentsDecal = SceneInfo->bCachedRayTracingInstanceAllSegmentsDecal;

					// CacheInstances expects to have one ray tracing mesh command per BLAS segment.
					// If that's not the case in the future, other logic such as NumCachedStaticVisibleMeshCommands calculation needs to be updated.
					if (!bUsingNaniteRayTracing)
					{
						checkf(RTLODData.CachedMeshCommandIndices.Num() == RelevantPrimitive->RayTracingGeometry->Initializer.Segments.Num(),
							TEXT("Expected to have one ray tracing mesh command per BLAS segment (primitive has %d cached mesh commands but BLAS has %d segments)."),
							RTLODData.CachedMeshCommandIndices.Num(), RelevantPrimitive->RayTracingGeometry->Initializer.Segments.Num());
					}

					const bool bNeedFarFieldInstance = EnumHasAllFlags(Flags, ERayTracingPrimitiveFlags::FarField);

					const bool bNeedMainInstance = !bNeedFarFieldInstance && !RelevantPrimitive->CachedMeshCommandFlags.bAllSegmentsDecal;

					// if primitive has mixed decal and non-decal segments we need to have two ray tracing instances
					// one containing non-decal segments and the other with decal segments
					// masking of segments is done using "hidden" hitgroups
					// TODO: Debug Visualization to highlight primitives using this?
					const bool bNeedDecalInstance = !bNeedFarFieldInstance && RelevantPrimitive->CachedMeshCommandFlags.bAnySegmentsDecal && !ShouldExcludeDecals();
					checkf(bNeedMainInstance || bNeedDecalInstance || bNeedFarFieldInstance, TEXT("FRelevantPrimitive is expected to have a main instance, decal instance or both.")); // TODO

					// For now store offsets relative to current context
					// Will be patched later to be a global offset
					RelevantPrimitive->RelativeInstanceOffset = Context.NumCachedStaticInstances;
					RelevantPrimitive->RelativeDecalInstanceOffset = Context.NumCachedStaticDecalInstances;
					RelevantPrimitive->RelativeFarFieldInstanceOffset = Context.NumCachedStaticFarFieldInstances;
					RelevantPrimitive->ContextIndex = Context.ContextIndex;

					if (bNeedMainInstance)
					{
						++Context.NumCachedStaticInstances;
					}

					if (bNeedDecalInstance)
					{
						++Context.NumCachedStaticDecalInstances;
					}

					if (bNeedFarFieldInstance)
					{
						++Context.NumCachedStaticFarFieldInstances;
					}

					// Setup the shader bindings write offset which is computed here for all cached instances so the writing can happen in parallel to the visible bindings array
					RelevantPrimitive->RelativeVisibleShaderBindingsOffset = Context.NumCachedStaticVisibleShaderBindings;
					Context.NumCachedStaticVisibleShaderBindings += RTLODData.SBTAllocationSegmentCount;
					checkSlow(RTLODData.SBTAllocationSegmentCount == RTLODData.SBTAllocation->GetSegmentCount());
				}
				// - DirtyCachedRayTracingPrimitives are only processed after StaticPrimitiveIndices is filled
				// so we can end up with primitives that should be skipped here
				// - once we update flags of primitive with dirty raytracing state before `GatherRayTracingRelevantPrimitives_Parallel`
				// we should replace this condition with an assert instead
				else if (!EnumHasAnyFlags(Flags, ERayTracingPrimitiveFlags::Skip))
				{
#if DO_CHECK
					if (bUsingReferenceBasedResidency)
					{
						FRayTracingGeometry* TargetRayTracingGeometry = SceneInfo->GetStaticRayTracingGeometry(LODIndex);
						if (!ensure(TargetRayTracingGeometry != nullptr))
						{
							return;
						}

						ensure(((FRayTracingGeometryManager*)GRayTracingGeometryManager)->IsGeometryGroupReferenced(TargetRayTracingGeometry->GroupHandle));

					}
#endif

					FRayTracingGeometry* RayTracingGeometry = SceneInfo->GetValidStaticRayTracingGeometry(LODIndex);

					if (RayTracingGeometry == nullptr)
					{
						return;
					}

					check(RayTracingGeometry->LODIndex == LODIndex);

					// Sometimes LODIndex is out of range because it is clamped by ClampToFirstLOD, like the requested LOD is being streamed in and hasn't been available
					// According to InitViews, we should hide the static mesh instance
					if (SceneInfo->GetRayTracingLODDataNum() > uint32(LODIndex))
					{
						const bool bUsesLightingChannels = bLightingChannelsUsingAHS && SceneInfo->bUsesLightingChannels;
						Context.bUsesLightingChannels |= bUsesLightingChannels;

						FRelevantPrimitive* RelevantPrimitive = new (Context.StaticPrimitives) FRelevantPrimitive();
						RelevantPrimitive->PrimitiveIndex = PrimitiveIndex;
						RelevantPrimitive->PersistentPrimitiveIndex = SceneInfo->GetPersistentIndex();

						RelevantPrimitive->LODIndex = LODIndex;
						RelevantPrimitive->RayTracingGeometry = RayTracingGeometry;
						RelevantPrimitive->bUsesLightingChannels = bUsesLightingChannels;

						const FPrimitiveSceneInfo::FRayTracingLODData& RTLODData = SceneInfo->GetRayTracingLODData(LODIndex);
						RelevantPrimitive->CachedMeshCommandFlags = RTLODData.CachedMeshCommandFlags;
					}
				}
			});

		if (Contexts.Num() > 0)
		{
			SCOPED_NAMED_EVENT(GatherRayTracingRelevantStaticPrimitives__Merge, FColor::Emerald);

			uint32 NumStaticPrimitives = 0;
			uint32 NumCachedStaticPrimitives = 0;

			for (auto& Context : Contexts)
			{
				NumStaticPrimitives += Context.StaticPrimitives.Num();
				NumCachedStaticPrimitives += Context.CachedStaticPrimitives.Num();
			}

			TaskData.StaticPrimitives.Reserve(NumStaticPrimitives);
			TaskData.CachedStaticPrimitives.Reserve(NumCachedStaticPrimitives);

			TaskData.GatherContexts.SetNum(Contexts.Num());

			for (int32 ContextIndex = 0; ContextIndex < Contexts.Num(); ++ContextIndex)
			{
				FRelevantStaticPrimitivesContext& Context = Contexts[ContextIndex];
				FRelevantPrimitiveGatherContext& GatherContext = TaskData.GatherContexts[ContextIndex];

				Context.StaticPrimitives.CopyToLinearArray(TaskData.StaticPrimitives);
				Context.CachedStaticPrimitives.CopyToLinearArray(TaskData.CachedStaticPrimitives);

				GatherContext.InstanceOffset = TaskData.NumCachedStaticInstances;
				GatherContext.DecalInstanceOffset = TaskData.NumCachedStaticDecalInstances;
				GatherContext.FarFieldInstanceOffset = TaskData.NumCachedStaticFarFieldInstances;
				GatherContext.VisibleShaderBindingsOffset = TaskData.NumCachedStaticVisibleShaderBindings;

				TaskData.NumCachedStaticInstances += Context.NumCachedStaticInstances;
				TaskData.NumCachedStaticDecalInstances += Context.NumCachedStaticDecalInstances;
				TaskData.NumCachedStaticFarFieldInstances += Context.NumCachedStaticFarFieldInstances;
				TaskData.NumCachedStaticVisibleShaderBindings += Context.NumCachedStaticVisibleShaderBindings;

				TaskData.bUsesLightingChannels |= Context.bUsesLightingChannels;

				for (const FPrimitiveSceneInfo* SceneInfo : Context.VisibleNaniteRayTracingPrimitives)
				{
					Nanite::GRayTracingManager.AddVisiblePrimitive(SceneInfo);
				}
			}
		}
	}

	struct FAutoInstanceBatch
	{
		FRayTracingScene::FInstanceHandle InstanceHandle = FRayTracingScene::INVALID_INSTANCE_HANDLE;
		FRayTracingScene::FInstanceHandle DecalInstanceHandle = FRayTracingScene::INVALID_INSTANCE_HANDLE;

		// Copies the next InstanceSceneDataOffset and user data into the current batch, returns true if arrays were re-allocated.
		bool Add(FRayTracingScene& InRayTracingScene, uint32 InInstanceSceneDataOffset)
		{
			// Adhoc TArray-like resize behavior, in lieu of support for using a custom FMemStackBase in TArray.
			// Idea for future: if batch becomes large enough, we could actually split it into multiple instances to avoid memory waste.

			const bool bNeedReallocation = Cursor == InstanceSceneDataOffsets.Num();

			if (bNeedReallocation)
			{
				int32 PrevCount = InstanceSceneDataOffsets.Num();
				int32 NextCount = FMath::Max(PrevCount * 2, 1);

				TArrayView<uint32> NewInstanceSceneDataOffsets = InRayTracingScene.Allocate<uint32>(NextCount);
				if (PrevCount)
				{
					FMemory::Memcpy(NewInstanceSceneDataOffsets.GetData(), InstanceSceneDataOffsets.GetData(), InstanceSceneDataOffsets.GetTypeSize() * InstanceSceneDataOffsets.Num());
				}
				InstanceSceneDataOffsets = NewInstanceSceneDataOffsets;
			}

			InstanceSceneDataOffsets[Cursor] = InInstanceSceneDataOffset;

			++Cursor;

			return bNeedReallocation;
		}

		bool IsValid() const
		{
			return InstanceSceneDataOffsets.Num() != 0;
		}

		TArrayView<uint32> InstanceSceneDataOffsets;
		uint32 Cursor = 0;
	}; 
	
	void AddStaticInstancesToRayTracingScene(
		const FScene& Scene,
		const RayTracing::FSceneOptions& SceneOptions,
		TConstArrayView<FRelevantPrimitive> RelevantStaticPrimitives,
		FRayTracingScene& RayTracingScene,
		TArray<FRayTracingShaderBindingData>& VisibleShaderBindingData)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(RayTracingScene_AddStaticInstances);

		VisibleShaderBindingData.Reserve(VisibleShaderBindingData.Num() + VisibleShaderBindingData.Num());

		const bool bAutoInstance = CVarRayTracingAutoInstance.GetValueOnRenderThread() != 0;

		// Instance batches by FRelevantPrimitive::InstancingKey()
		Experimental::TSherwoodMap<uint64, FAutoInstanceBatch> InstanceBatches;

		// scan relevant primitives computing hash data to look for duplicate instances
		for (const FRelevantPrimitive& RelevantPrimitive : RelevantStaticPrimitives)
		{
			const int32 PrimitiveIndex = RelevantPrimitive.PrimitiveIndex;
			FPrimitiveSceneInfo* SceneInfo = Scene.Primitives[PrimitiveIndex];
			FPrimitiveSceneProxy* SceneProxy = Scene.PrimitiveSceneProxies[PrimitiveIndex];
			ERayTracingPrimitiveFlags Flags = Scene.PrimitiveRayTracingFlags[PrimitiveIndex];
			const FPersistentPrimitiveIndex PersistentPrimitiveIndex = RelevantPrimitive.PersistentPrimitiveIndex;

			check(!EnumHasAnyFlags(Flags, ERayTracingPrimitiveFlags::CacheInstances));

			const int8 LODIndex = RelevantPrimitive.LODIndex;

			if (LODIndex < 0)
			{
				// TODO: Filter these primitives earlier
				continue;
			}

			const bool bNeedMainInstance = !RelevantPrimitive.CachedMeshCommandFlags.bAllSegmentsDecal;

			// if primitive has mixed decal and non-decal segments we need to have two ray tracing instances
			// one containing non-decal segments and the other with decal segments
			// masking of segments is done using "hidden" hitgroups
			// TODO: Debug Visualization to highlight primitives using this?
			const bool bNeedDecalInstance = RelevantPrimitive.CachedMeshCommandFlags.bAnySegmentsDecal && !ShouldExcludeDecals();

			// skip if not needed for main or decal - default values for bAllSegmentsDecal is true because it's updated with & op for added cached segments
			// but if there are no cached command indices then default value of true is kept but bAnySegmentsDecal will false as well then.
			if (!bNeedMainInstance && !bNeedDecalInstance)
			{
				continue;
			}

			if (ShouldExcludeDecals() && RelevantPrimitive.CachedMeshCommandFlags.bAllSegmentsDecal)
			{
				continue;
			}

			if (!SceneOptions.bTranslucentGeometry && RelevantPrimitive.CachedMeshCommandFlags.bAllSegmentsTranslucent)
			{
				continue;
			}

			if (!SceneOptions.bIncludeSky && RelevantPrimitive.CachedMeshCommandFlags.bIsSky)
			{
				continue;
			}

			// location if this is a new entry
			const uint64 InstanceKey = RelevantPrimitive.InstancingKey();

			FAutoInstanceBatch DummyInstanceBatch = { };
			FAutoInstanceBatch& InstanceBatch = bAutoInstance ? InstanceBatches.FindOrAdd(InstanceKey, DummyInstanceBatch) : DummyInstanceBatch;

			if (InstanceBatch.IsValid())
			{
				// Reusing a previous entry, just append to the instance list.

				bool bReallocated = InstanceBatch.Add(RayTracingScene, SceneInfo->GetInstanceSceneDataOffset());

				if (InstanceBatch.InstanceHandle.IsValid())
				{
					FRayTracingGeometryInstance& RayTracingInstance = RayTracingScene.GetInstance(InstanceBatch.InstanceHandle);
					++RayTracingInstance.NumTransforms;
					check(RayTracingInstance.NumTransforms == InstanceBatch.Cursor); // sanity check

					if (bReallocated)
					{
						RayTracingInstance.InstanceSceneDataOffsets = InstanceBatch.InstanceSceneDataOffsets;
						RayTracingInstance.UserData = InstanceBatch.InstanceSceneDataOffsets;
					}
				}

				if (InstanceBatch.DecalInstanceHandle.IsValid())
				{
					FRayTracingGeometryInstance& RayTracingInstance = RayTracingScene.GetInstance(InstanceBatch.DecalInstanceHandle);
					++RayTracingInstance.NumTransforms;
					check(RayTracingInstance.NumTransforms == InstanceBatch.Cursor); // sanity check

					if (bReallocated)
					{
						RayTracingInstance.InstanceSceneDataOffsets = InstanceBatch.InstanceSceneDataOffsets;
						RayTracingInstance.UserData = InstanceBatch.InstanceSceneDataOffsets;
					}
				}
			}
			else
			{
				// Starting new instance batch

				InstanceBatch.Add(RayTracingScene, SceneInfo->GetInstanceSceneDataOffset());

				FRayTracingGeometryInstance RayTracingInstance;
				RayTracingInstance.GeometryRHI = RelevantPrimitive.RayTracingGeometry->GetRHI();
				checkf(RayTracingInstance.GeometryRHI, TEXT("Ray tracing instance must have a valid geometry."));
				RayTracingInstance.InstanceSceneDataOffsets = InstanceBatch.InstanceSceneDataOffsets;
				RayTracingInstance.UserData = InstanceBatch.InstanceSceneDataOffsets;
				RayTracingInstance.NumTransforms = 1;

				RayTracingInstance.Mask = RelevantPrimitive.CachedMeshCommandFlags.InstanceMask; // When no cached command is found, InstanceMask == 0 and the instance is effectively filtered out

				// Run AHS for alpha masked and meshes with only some sections casting shadows, which require per mesh section filtering in AHS
				if (RelevantPrimitive.CachedMeshCommandFlags.bAllSegmentsOpaque && (RelevantPrimitive.CachedMeshCommandFlags.bAllSegmentsCastShadow || !RelevantPrimitive.CachedMeshCommandFlags.bAnySegmentsCastShadow))
				{
					RayTracingInstance.Flags |= ERayTracingInstanceFlags::ForceOpaque;
				}
				if (RelevantPrimitive.CachedMeshCommandFlags.bTwoSided)
				{
					RayTracingInstance.Flags |= ERayTracingInstanceFlags::TriangleCullDisable;
				}
				if (RelevantPrimitive.CachedMeshCommandFlags.bAllSegmentsReverseCulling)
				{
					RayTracingInstance.Flags |= ERayTracingInstanceFlags::TriangleCullReverse;
				}
				AddRayTracingInstanceFlagOverrides(RayTracingInstance.Flags, RelevantPrimitive.bUsesLightingChannels);

				InstanceBatch.InstanceHandle = FRayTracingScene::INVALID_INSTANCE_HANDLE;

				const FPrimitiveSceneInfo::FRayTracingLODData& RTLODData = SceneInfo->GetRayTracingLODData(LODIndex);
				ensure(RTLODData.SBTAllocation);
				if (RTLODData.SBTAllocation == nullptr)
				{
					continue;
				}

				uint32 SegmentCount = RelevantPrimitive.RayTracingGeometry->Initializer.Segments.Num();
				if (bNeedMainInstance)
				{
					RayTracingInstance.InstanceContributionToHitGroupIndex = RTLODData.SBTAllocation->GetInstanceContributionToHitGroupIndex(ERayTracingShaderBindingLayer::Base);
					InstanceBatch.InstanceHandle = RayTracingScene.AddInstance(RayTracingInstance, EnumHasAllFlags(Flags, ERayTracingPrimitiveFlags::FarField) ? ERayTracingSceneLayer::FarField : ERayTracingSceneLayer::Base, SceneProxy, false, RelevantPrimitive.RayTracingGeometry->GetGeometryHandle());
				}

				InstanceBatch.DecalInstanceHandle = FRayTracingScene::INVALID_INSTANCE_HANDLE;
				if (bNeedDecalInstance)
				{
					FRayTracingGeometryInstance DecalRayTracingInstance = RayTracingInstance;
					DecalRayTracingInstance.InstanceContributionToHitGroupIndex = RTLODData.SBTAllocation->GetInstanceContributionToHitGroupIndex(ERayTracingShaderBindingLayer::Decals);
					InstanceBatch.DecalInstanceHandle = RayTracingScene.AddInstance(MoveTemp(DecalRayTracingInstance), ERayTracingSceneLayer::Decals, SceneProxy, false, RelevantPrimitive.RayTracingGeometry->GetGeometryHandle());
				}

				for (int32 CommandIndex : RTLODData.CachedMeshCommandIndices)
				{
					if (CommandIndex >= 0)
					{
						const FRayTracingMeshCommand& MeshCommand = Scene.CachedRayTracingMeshCommands[CommandIndex];

						ERayTracingLocalShaderBindingType BindingType = MeshCommand.bCanBeCached ? ERayTracingLocalShaderBindingType::Persistent : ERayTracingLocalShaderBindingType::Transient;

						if (bNeedMainInstance)
						{
							const bool bHidden = MeshCommand.bDecal;
							const uint32 RecordIndex = RTLODData.SBTAllocation->GetRecordIndex(ERayTracingShaderBindingLayer::Base, MeshCommand.GeometrySegmentIndex);
							FRayTracingShaderBindingData RTShaderBindingData(&MeshCommand, RelevantPrimitive.RayTracingGeometry->GetRHI(), RecordIndex, BindingType, bHidden);
							VisibleShaderBindingData.Add(RTShaderBindingData);
						}
						if (bNeedDecalInstance)
						{
							const bool bHidden = !MeshCommand.bDecal;
							const uint32 RecordIndex = RTLODData.SBTAllocation->GetRecordIndex(ERayTracingShaderBindingLayer::Decals, MeshCommand.GeometrySegmentIndex);
							FRayTracingShaderBindingData RTShaderBindingData(&MeshCommand, RelevantPrimitive.RayTracingGeometry->GetRHI(), RecordIndex, BindingType, bHidden);
							VisibleShaderBindingData.Add(RTShaderBindingData);
						}
					}
					else
					{
						// CommandIndex == -1 indicates that the mesh batch has been filtered by FRayTracingMeshProcessor (like the shadow depth pass batch)
						// Do nothing in this case
					}
				}
			}
		}
	}

	void AddCachedStaticInstancesToRayTracingScene(
		const FScene& Scene,
		const RayTracing::FSceneOptions& SceneOptions,
		TConstArrayView<FRelevantPrimitive> RelevantCachedStaticPrimitives,
		TConstArrayView<FRelevantPrimitiveGatherContext> GatherContexts,
		const int32 NumCachedStaticInstances,
		const int32 NumCachedStaticDecalInstances,
		const int32 NumCachedStaticFarFieldInstances,
		const int32 NumCachedStaticVisibleShaderBindings,
		FRayTracingScene& RayTracingScene,
		TArray<FRayTracingShaderBindingData>& VisibleShaderBindingData)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(RayTracingScene_AddCachedStaticInstances);

		const FRayTracingScene::FInstanceRange CachedStaticInstanceRange = RayTracingScene.AllocateInstanceRangeUninitialized(NumCachedStaticInstances, ERayTracingSceneLayer::Base);
		const FRayTracingScene::FInstanceRange CachedStaticDecalInstanceRange = RayTracingScene.AllocateInstanceRangeUninitialized(NumCachedStaticDecalInstances, ERayTracingSceneLayer::Decals);
		const FRayTracingScene::FInstanceRange CachedStaticFarFieldInstanceRange = RayTracingScene.AllocateInstanceRangeUninitialized(NumCachedStaticFarFieldInstances, ERayTracingSceneLayer::FarField);
		const uint32 BaseCachedVisibleShaderBindingsIndex = VisibleShaderBindingData.AddUninitialized(NumCachedStaticVisibleShaderBindings);

		const int32 MinBatchSize = 128;
		ParallelFor(
			TEXT("RayTracingScene_AddCachedStaticInstances_ParallelFor"),
			RelevantCachedStaticPrimitives.Num(),
			MinBatchSize,
			[&Scene, &RayTracingScene, &VisibleShaderBindingData, &RelevantCachedStaticPrimitives, &GatherContexts, CachedStaticInstanceRange, CachedStaticDecalInstanceRange, CachedStaticFarFieldInstanceRange, BaseCachedVisibleShaderBindingsIndex](int32 Index)
			{
				const FRelevantPrimitive& RelevantPrimitive = RelevantCachedStaticPrimitives[Index];
				const int32 PrimitiveIndex = RelevantPrimitive.PrimitiveIndex;
				FPrimitiveSceneInfo* SceneInfo = Scene.Primitives[PrimitiveIndex];
				FPrimitiveSceneProxy* SceneProxy = Scene.PrimitiveSceneProxies[PrimitiveIndex];
				ERayTracingPrimitiveFlags Flags = Scene.PrimitiveRayTracingFlags[PrimitiveIndex];
				const FPersistentPrimitiveIndex PersistentPrimitiveIndex = RelevantPrimitive.PersistentPrimitiveIndex;

				check(EnumHasAnyFlags(Flags, ERayTracingPrimitiveFlags::CacheInstances));

				const bool bUsingNaniteRayTracing = (Nanite::GetRayTracingMode() != Nanite::ERayTracingMode::Fallback) && SceneProxy->IsNaniteMesh();

				if (bUsingNaniteRayTracing)
				{
					check(RelevantPrimitive.CachedRayTracingInstance->GeometryRHI != nullptr);
				}

				const bool bNeedFarFieldInstance = EnumHasAllFlags(Flags, ERayTracingPrimitiveFlags::FarField);

				const bool bNeedMainInstance = !bNeedFarFieldInstance && !RelevantPrimitive.CachedMeshCommandFlags.bAllSegmentsDecal;

				// if primitive has mixed decal and non-decal segments we need to have two ray tracing instances
				// one containing non-decal segments and the other with decal segments
				// masking of segments is done using "hidden" hitgroups
				// TODO: Debug Visualization to highlight primitives using this?
				const bool bNeedDecalInstance = !bNeedFarFieldInstance && RelevantPrimitive.CachedMeshCommandFlags.bAnySegmentsDecal && !RelevantPrimitive.CachedMeshCommandFlags.bAllSegmentsDecal && !ShouldExcludeDecals();

				check(!ShouldExcludeDecals() || !RelevantPrimitive.CachedMeshCommandFlags.bAllSegmentsDecal);
				check(RelevantPrimitive.CachedRayTracingInstance);

				// Cached instances always use single LOD 0 - see above
				const int8 LODIndex = 0;
				const FPrimitiveSceneInfo::FRayTracingLODData& RTLODData = SceneInfo->GetRayTracingLODData(LODIndex);

				if (bNeedMainInstance)
				{
					const int32 InstanceIndexInRange = GatherContexts[RelevantPrimitive.ContextIndex].InstanceOffset + RelevantPrimitive.RelativeInstanceOffset;

					FRayTracingGeometryInstance RayTracingInstance = *RelevantPrimitive.CachedRayTracingInstance;
					RayTracingInstance.InstanceContributionToHitGroupIndex = RTLODData.SBTAllocation->GetInstanceContributionToHitGroupIndex(ERayTracingShaderBindingLayer::Base);
					AddRayTracingInstanceFlagOverrides(RayTracingInstance.Flags, RelevantPrimitive.bUsesLightingChannels);

					const int32 GeometryHandle = RelevantPrimitive.RayTracingGeometry ? RelevantPrimitive.RayTracingGeometry->GetGeometryHandle() : INDEX_NONE;
					RayTracingScene.SetInstance(CachedStaticInstanceRange, InstanceIndexInRange, MoveTemp(RayTracingInstance), SceneProxy, false, GeometryHandle);
				}

				if (bNeedDecalInstance)
				{
					const int32 DecalInstanceIndexInRange = GatherContexts[RelevantPrimitive.ContextIndex].DecalInstanceOffset + RelevantPrimitive.RelativeDecalInstanceOffset;

					FRayTracingGeometryInstance DecalRayTracingInstance = *RelevantPrimitive.CachedRayTracingInstance;
					DecalRayTracingInstance.InstanceContributionToHitGroupIndex = RTLODData.SBTAllocation->GetInstanceContributionToHitGroupIndex(ERayTracingShaderBindingLayer::Decals);
					AddRayTracingInstanceFlagOverrides(DecalRayTracingInstance.Flags, RelevantPrimitive.bUsesLightingChannels);
					
					const int32 GeometryHandle = RelevantPrimitive.RayTracingGeometry ? RelevantPrimitive.RayTracingGeometry->GetGeometryHandle() : INDEX_NONE;
					RayTracingScene.SetInstance(CachedStaticDecalInstanceRange, DecalInstanceIndexInRange, MoveTemp(DecalRayTracingInstance), SceneProxy, false, GeometryHandle);
				}

				if (bNeedFarFieldInstance)
				{
					const int32 FarFieldInstanceIndexInRange = GatherContexts[RelevantPrimitive.ContextIndex].FarFieldInstanceOffset + RelevantPrimitive.RelativeFarFieldInstanceOffset;

					FRayTracingGeometryInstance FarFieldRayTracingInstance = *RelevantPrimitive.CachedRayTracingInstance;
					FarFieldRayTracingInstance.InstanceContributionToHitGroupIndex = RTLODData.SBTAllocation->GetInstanceContributionToHitGroupIndex(ERayTracingShaderBindingLayer::Base);
					AddRayTracingInstanceFlagOverrides(FarFieldRayTracingInstance.Flags, RelevantPrimitive.bUsesLightingChannels);

					RayTracingScene.SetInstance(CachedStaticFarFieldInstanceRange, FarFieldInstanceIndexInRange, MoveTemp(FarFieldRayTracingInstance), SceneProxy, false);
				}

				if (RelevantPrimitive.RelativeVisibleShaderBindingsOffset != INDEX_NONE)
				{
					const int32 VisibleShadingBindingOffset = BaseCachedVisibleShaderBindingsIndex + GatherContexts[RelevantPrimitive.ContextIndex].VisibleShaderBindingsOffset + RelevantPrimitive.RelativeVisibleShaderBindingsOffset;

					int32 MainCommandOffset = VisibleShadingBindingOffset;
					int32 DecalCommandOffset = MainCommandOffset + (bNeedMainInstance ? RTLODData.CachedMeshCommandIndices.Num() : 0);
					int32 FarFieldCommandOffset = DecalCommandOffset + (bNeedDecalInstance ? RTLODData.CachedMeshCommandIndices.Num() : 0);
					
					for (int32 CommandIndex : RTLODData.CachedMeshCommandIndices)
					{
						const FRayTracingMeshCommand& MeshCommand = Scene.CachedRayTracingMeshCommands[CommandIndex];
						ERayTracingLocalShaderBindingType BindingType = MeshCommand.bCanBeCached ? ERayTracingLocalShaderBindingType::Persistent : ERayTracingLocalShaderBindingType::Transient;

						if (bNeedMainInstance)
						{
							const bool bHidden = MeshCommand.bDecal;
							const uint32 RecordIndex = RTLODData.SBTAllocation->GetRecordIndex(ERayTracingShaderBindingLayer::Base, MeshCommand.GeometrySegmentIndex);
							VisibleShaderBindingData[MainCommandOffset] = FRayTracingShaderBindingData(&MeshCommand, RelevantPrimitive.CachedRayTracingInstance->GeometryRHI, RecordIndex, BindingType, bHidden);
							++MainCommandOffset;
						}

						if (bNeedDecalInstance)
						{
							const bool bHidden = !MeshCommand.bDecal;
							const uint32 RecordIndex = RTLODData.SBTAllocation->GetRecordIndex(ERayTracingShaderBindingLayer::Decals, MeshCommand.GeometrySegmentIndex);
							VisibleShaderBindingData[DecalCommandOffset] = FRayTracingShaderBindingData(&MeshCommand, RelevantPrimitive.CachedRayTracingInstance->GeometryRHI, RecordIndex, BindingType, bHidden);
							++DecalCommandOffset;
						}

						if (bNeedFarFieldInstance)
						{
							const bool bHidden = MeshCommand.bDecal;
							const uint32 RecordIndex = RTLODData.SBTAllocation->GetRecordIndex(ERayTracingShaderBindingLayer::Base, MeshCommand.GeometrySegmentIndex);
							VisibleShaderBindingData[FarFieldCommandOffset] = FRayTracingShaderBindingData(&MeshCommand, RelevantPrimitive.CachedRayTracingInstance->GeometryRHI, RecordIndex, BindingType, bHidden);
							++FarFieldCommandOffset;
						}
					}
				}
			});
	}

	void AddDynamicInstancesToRayTracingScene(
		FGatherInstancesTaskData& TaskData,
		FRayTracingScene& RayTracingScene,
		FRayTracingShaderBindingTable& RayTracingSBT,
		int64 SharedBufferGenerationID)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(RayTracingScene_AddDynamicInstances);

		TaskData.DynamicRayTracingInstancesContext.AddInstancesToScene(RayTracingScene, RayTracingSBT, SharedBufferGenerationID);
	}

	void BeginGatherInstances(FRDGBuilder& GraphBuilder, FGatherInstancesTaskData& TaskData, UE::Tasks::FTask FrustumCullTask)
	{
		const bool bUsingReferenceBasedResidency = IsRayTracingUsingReferenceBasedResidency();

		UE::Tasks::FTask CacheRayTracingPrimitivesTask = TaskData.Scene.GetCacheRayTracingPrimitivesTask();

		TaskData.GatherRelevantPrimitivesTask = GraphBuilder.AddSetupTask([&TaskData, bUsingReferenceBasedResidency]
			{
				FTaskTagScope Scope(ETaskTag::EParallelRenderingThread);
				GatherRelevantPrimitives(TaskData, bUsingReferenceBasedResidency);
			}, MakeArrayView({ CacheRayTracingPrimitivesTask, FrustumCullTask }), UE::Tasks::ETaskPriority::High);

		static const auto ICVarStaticMeshLODDistanceScale = IConsoleManager::Get().FindConsoleVariable(TEXT("r.StaticMeshLODDistanceScale"));
		const float LODScaleCVarValue = ICVarStaticMeshLODDistanceScale->GetFloat();
		const int32 ForcedLODLevel = GetCVarForceLOD();

		TaskData.GatherRelevantStaticPrimitivesTask = GraphBuilder.AddSetupTask([&TaskData, LODScaleCVarValue, ForcedLODLevel, bUsingReferenceBasedResidency]
			{
				FTaskTagScope Scope(ETaskTag::EParallelRenderingThread);
				GatherRelevantStaticPrimitives(TaskData, LODScaleCVarValue, ForcedLODLevel, bUsingReferenceBasedResidency);
			}, TaskData.GatherRelevantPrimitivesTask);

		TaskData.GatherDynamicRayTracingInstancesPrerequisites.AddPrerequisites(TaskData.GatherRelevantPrimitivesTask);
		TaskData.GatherDynamicRayTracingInstancesPrerequisites.AddPrerequisites(TaskData.Scene.GetGPUSkinCacheTask());

		// TODO: Could gather dynamic ray tracing instances using multiple tasks / FDynamicRayTracingInstancesContext
		TaskData.GatherDynamicRayTracingInstancesTask = GraphBuilder.AddSetupTask([&TaskData]
			{
				FTaskTagScope Scope(ETaskTag::EParallelRenderingThread);
				TaskData.DynamicRayTracingInstancesContext.GatherDynamicRayTracingInstances(TaskData.DynamicPrimitivesIndices);
			}, TaskData.GatherDynamicRayTracingInstancesPrerequisites);


		TaskData.AddUsedStreamingHandlesTask = GraphBuilder.AddSetupTask([&TaskData, LODScaleCVarValue, ForcedLODLevel, bUsingReferenceBasedResidency]
			{
				FTaskTagScope Scope(ETaskTag::EParallelRenderingThread);

				// Inform the coarse mesh streaming manager about all the used streamable render assets in the scene
				Nanite::FCoarseMeshStreamingManager* CoarseMeshSM = IStreamingManager::Get().GetNaniteCoarseMeshStreamingManager();
				if (CoarseMeshSM)
				{
					CoarseMeshSM->AddUsedStreamingHandles(TaskData.UsedCoarseMeshStreamingHandles);
				}
			}, TaskData.GatherRelevantPrimitivesTask);

		TaskData.bValid = true;
	}

	void BeginGatherDynamicRayTracingInstances(FGatherInstancesTaskData& TaskData)
	{
		if (!TaskData.bGatherDynamicRayTracingInstancesPrerequisitesTriggered)
		{
			TaskData.GatherDynamicRayTracingInstancesPrerequisites.Trigger();
			TaskData.bGatherDynamicRayTracingInstancesPrerequisitesTriggered = true;
		}
	}

	bool FinishGatherInstances(
		FRDGBuilder& GraphBuilder,
		FGatherInstancesTaskData& TaskData,
		FRayTracingScene& RayTracingScene,
		FRayTracingShaderBindingTable& RayTracingSBT,
		FGlobalDynamicReadBuffer& InDynamicReadBuffer,
		FSceneRenderingBulkObjectAllocator& InBulkAllocator)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(RayTracing_FinishGatherInstances);
		SCOPE_CYCLE_COUNTER(STAT_RayTracing_FinishGatherInstances);
		RDG_CSV_STAT_EXCLUSIVE_SCOPE(GraphBuilder, RayTracing_FinishGatherInstances);

		TaskData.GatherRelevantPrimitivesTask.Wait();

		FScene& Scene = TaskData.Scene;
		FViewInfo& View = TaskData.View;

		// Prepare ray tracing scene instance list
		checkf(TaskData.bValid, TEXT("Ray tracing relevant primitive list is expected to have been created before GatherRayTracingWorldInstancesForView() is called."));

		// Check that any invalidated cached uniform expressions have been updated on the rendering thread.
		// Normally this work is done through FMaterialRenderProxy::UpdateUniformExpressionCacheIfNeeded,
		// however ray tracing material processing (FMaterialShader::GetShaderBindings, which accesses UniformExpressionCache)
		// is done on task threads, therefore all work must be done here up-front as UpdateUniformExpressionCacheIfNeeded is not free-threaded.
		check(!FMaterialRenderProxy::HasDeferredUniformExpressionCacheRequests());

		RayTracingSBT.ResetDynamicAllocationData();

		INC_DWORD_STAT_BY(STAT_VisibleRayTracingPrimitives, TaskData.StaticPrimitives.Num() + TaskData.CachedStaticPrimitives.Num() + TaskData.DynamicPrimitivesIndices.Num());

		FRayTracingDynamicGeometryUpdateManager* DynamicGeometryUpdateManager = Scene.GetRayTracingDynamicGeometryUpdateManager();
		const int64 SharedBufferGenerationID = DynamicGeometryUpdateManager->BeginUpdate();

		TaskData.GatherDynamicRayTracingInstancesTask.Wait();
		TaskData.DynamicRayTracingInstancesContext.GatherDynamicRayTracingInstances_RenderThread();
		TaskData.DynamicRayTracingInstancesContext.Finish(GraphBuilder.RHICmdList);

		Scene.GPUScene.UploadDynamicPrimitiveShaderDataForView(GraphBuilder, View, /*bRayTracing*/ true);

		TaskData.DynamicRayTracingInstancesContext.CollectRDGResources(GraphBuilder);

		// This adds final instances to the ray tracing scene and must be done before FRayTracingScene::BuildInitializationData().
		UE::Tasks::FTask AddInstancesTask = GraphBuilder.AddSetupTask(
			[&TaskData, &Scene, &View, &RayTracingScene, &RayTracingSBT, SharedBufferGenerationID]
			{
				FTaskTagScope TaskTagScope(ETaskTag::EParallelRenderingThread);

				AddDynamicInstancesToRayTracingScene(TaskData, RayTracingScene, RayTracingSBT, SharedBufferGenerationID);

				RayTracingScene.bUsesLightingChannels |= TaskData.bUsesLightingChannels;

				View.VisibleRayTracingShaderBindings.Reserve(TaskData.StaticPrimitives.Num() + TaskData.NumCachedStaticVisibleShaderBindings);

				AddStaticInstancesToRayTracingScene(
					Scene,
					TaskData.SceneOptions,
					TaskData.StaticPrimitives,
					RayTracingScene,
					View.VisibleRayTracingShaderBindings);

				AddCachedStaticInstancesToRayTracingScene(
					Scene,
					TaskData.SceneOptions,
					TaskData.CachedStaticPrimitives,
					TaskData.GatherContexts,
					TaskData.NumCachedStaticInstances,
					TaskData.NumCachedStaticDecalInstances,
					TaskData.NumCachedStaticFarFieldInstances,
					TaskData.NumCachedStaticVisibleShaderBindings,
					RayTracingScene,
					View.VisibleRayTracingShaderBindings);

			}, TaskData.GatherRelevantStaticPrimitivesTask);

		// Scene init task can run only when all pre-init tasks are complete (including culling tasks that are spawned while adding instances)
		View.RayTracingSceneInitTask = GraphBuilder.AddSetupTask(
			[&RayTracingScene]
			{
				FTaskTagScope TaskTagScope(ETaskTag::EParallelRenderingThread);

				TRACE_CPUPROFILER_EVENT_SCOPE(RayTracingSceneInitTask);
				RayTracingScene.BuildInitializationData();
			}, AddInstancesTask);

		// Finalizing VisibleRayTracingShaderBindings can run only when all pre-init tasks are complete (including culling tasks that are spawned while adding instances)
		View.VisibleRayTracingShaderBindingsFinalizeTask = GraphBuilder.AddSetupTask(
			[&View, &RayTracingSBT]
			{
				FTaskTagScope TaskTagScope(ETaskTag::EParallelRenderingThread);

				{
					TRACE_CPUPROFILER_EVENT_SCOPE(DeduplicateVisibleShaderBindings);

					// Deduplicate all the written SBT record indices by using bit array and checking the written indices into the SBT table
					TBitArray<> ProcessedSBTAllocations(false, RayTracingSBT.GetNumGeometrySegments() * RAY_TRACING_NUM_SHADER_SLOTS);
					TArray<FRayTracingShaderBindingData> DeduplicatedVisibleShaderBindingData;
					DeduplicatedVisibleShaderBindingData.Reserve(View.VisibleRayTracingShaderBindings.Num());
					for (FRayTracingShaderBindingData& VisibleBinding : View.VisibleRayTracingShaderBindings)
					{
						FBitReference BitReference = ProcessedSBTAllocations[VisibleBinding.SBTRecordIndex];
						if (!BitReference)
						{
							BitReference = true;
							DeduplicatedVisibleShaderBindingData.Add(VisibleBinding);
						}
					}
					View.VisibleRayTracingShaderBindings = MoveTemp(DeduplicatedVisibleShaderBindingData);
				}

				// reset static allocation lock again now that the final array of dirty shader bindings is build
				RayTracingSBT.ResetStaticAllocationLock();
			}, AddInstancesTask);

		// wait for this task here, although it could be done later in the frame
		// since it's only consumed by FCoarseMeshStreamingManager::UpdateResourceStates() during PostRenderAllViewports_RenderThread
		TaskData.AddUsedStreamingHandlesTask.Wait();

		return true;
	}

	bool ShouldExcludeDecals()
	{
		return GRayTracingExcludeDecals != 0;
	}
}

static_assert(std::is_trivially_destructible_v<RayTracing::FRelevantPrimitive>, "FRelevantPrimitive must be trivially destructible");
template <> struct TIsPODType<RayTracing::FRelevantPrimitive> { enum { Value = true }; }; // Necessary to use TChunkedArray::CopyToLinearArray

#endif //RHI_RAYTRACING
