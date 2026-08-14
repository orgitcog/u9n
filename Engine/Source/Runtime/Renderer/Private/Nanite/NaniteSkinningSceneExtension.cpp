// Copyright Epic Games, Inc. All Rights Reserved.

#include "NaniteSkinningSceneExtension.h"
#include "ViewDefinitions.h"
#include "ScenePrivate.h"
#include "RenderUtils.h"
#include "SkeletalRenderPublic.h"
#include "SkinningDefinitions.h"
#include "ViewData.h"

// TODO: these are prototype macros for how we might expose SceneUB for direct binding. 
//       If this becomes the way we want to expose this, then we should move this to shared headers.
//       There's still some machinery we _could_ add to make it work nicely as an API, e.g., interface to get the associated sub-UB & register a provider (or something).

#define IMPLEMENT_STATIC_UNIFORM_BUFFER_SCENE_UB(StructType, MangledName) \
	IMPLEMENT_STATIC_UNIFORM_BUFFER_SLOT(MangledName)	\
	IMPLEMENT_STATIC_UNIFORM_BUFFER_STRUCT(StructType, #MangledName, MangledName);

/**
 * Implement a Scene UB sub-struct _with_ a global UB definition for binding stand-alone.
 */
#define IMPLEMENT_SCENE_UB_STRUCT_EX(StructType, FieldName, DefaultValueFactoryType) \
	TSceneUniformBufferMemberRegistration<StructType> SceneUB::FieldName { TEXT(#FieldName), DefaultValueFactoryType }; \
	IMPLEMENT_STATIC_UNIFORM_BUFFER_SCENE_UB(StructType, SceneUbEx##FieldName)

static TAutoConsoleVariable<int32> CVarNaniteTransformDataBufferMinSizeBytes(
	TEXT("r.Nanite.SkinningBuffers.TransformDataMinSizeBytes"),
	4 * 1024,
	TEXT("The smallest size (in bytes) of the Nanite bone transform data buffer."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarNanitePrimitiveSkinningDataBufferMinSizeBytes(
	TEXT("r.Nanite.SkinningBuffers.HeaderDataMinSizeBytes"),
	4 * 1024,
	TEXT("The smallest size (in bytes) of the Nanite per-primitive skinning header data buffer."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<bool> CVarNaniteTransformBufferAsyncUpdates(
	TEXT("r.Nanite.SkinningBuffers.AsyncUpdates"),
	true,
	TEXT("When non-zero, Nanite transform data buffer updates are updated asynchronously."),
	ECVF_RenderThreadSafe
);

static int32 GNaniteTransformBufferForceFullUpload = 0;
static FAutoConsoleVariableRef CVarNaniteTransformBufferForceFullUpload(
	TEXT("r.Nanite.SkinningBuffers.ForceFullUpload"),
	GNaniteTransformBufferForceFullUpload,
	TEXT("0: Do not force a full upload.\n")
	TEXT("1: Force one full upload on the next update.\n")
	TEXT("2: Force a full upload every frame."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<bool> CVarNaniteTransformBufferDefrag(
	TEXT("r.Nanite.SkinningBuffers.Defrag"),
	true,
	TEXT("Whether or not to allow defragmentation of the Nanite skinning buffers."),
	ECVF_RenderThreadSafe
);

static int32 GNaniteTransformBufferForceDefrag = 0;
static FAutoConsoleVariableRef CVarNaniteTransformBufferDefragForce(
	TEXT("r.Nanite.SkinningBuffers.Defrag.Force"),
	GNaniteTransformBufferForceDefrag,
	TEXT("0: Do not force a full defrag.\n")
	TEXT("1: Force one full defrag on the next update.\n")
	TEXT("2: Force a full defrag every frame."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarNaniteTransformBufferDefragLowWaterMark(
	TEXT("r.Nanite.SkinningBuffers.Defrag.LowWaterMark"),
	0.375f,
	TEXT("Ratio of used to allocated memory at which to decide to defrag the Nanite skinning buffers."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<bool> CVarSkinningTransformProviders(
	TEXT("r.Skinning.TransformProviders"),
	true,
	TEXT("When set, transform providers are enabled (if registered)."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarNaniteSkinningDefaultAnimationMinScreenSize(
	TEXT("r.Nanite.Skinning.DefaultAnimationMinScreenSize"),
	0.1f,
	TEXT("Default animation screen size to stop animating at, applies when the per-component value is 0.0."),
	ECVF_RenderThreadSafe
);
BEGIN_UNIFORM_BUFFER_STRUCT(FNaniteSkinningParameters, RENDERER_API)
	SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, SkinningHeaders)
	SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, BoneHierarchy)
	SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, BoneObjectSpace)
	SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, BoneTransforms)
END_UNIFORM_BUFFER_STRUCT()

DECLARE_SCENE_UB_STRUCT(FNaniteSkinningParameters, NaniteSkinning, RENDERER_API)

using FLoadBalancer = TInstanceCullingLoadBalancer<SceneRenderingAllocator>;

// Reference pose transform provider
struct FTransformBlockHeader
{
	uint32 BlockLocalIndex;
	uint32 BlockTransformCount;
	uint32 BlockTransformOffset;
};

class FRefPoseTransformProviderCS : public FGlobalShader
{
public:
	static constexpr uint32 TransformsPerGroup = 64u;

private:
	DECLARE_GLOBAL_SHADER(FRefPoseTransformProviderCS);
	SHADER_USE_PARAMETER_STRUCT(FRefPoseTransformProviderCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneUniformParameters, Scene)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, TransformBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FTransformBlockHeader>, HeaderBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportNanite(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.CompilerFlags.Add(CFLAG_WarningsAsErrors);
		OutEnvironment.CompilerFlags.Add(CFLAG_HLSL2021);

		OutEnvironment.SetDefine(TEXT("TRANSFORMS_PER_GROUP"), TransformsPerGroup);
	}
};

IMPLEMENT_GLOBAL_SHADER(FRefPoseTransformProviderCS, "/Engine/Private/Skinning/TransformProviders.usf", "RefPoseProviderCS", SF_Compute);

static FGuid RefPoseProviderId(REF_POSE_TRANSFORM_PROVIDER_GUID);
static FGuid AnimRuntimeProviderId(ANIM_RUNTIME_TRANSFORM_PROVIDER_GUID);

namespace Nanite
{

static void GetDefaultSkinningParameters(FNaniteSkinningParameters& OutParameters, FRDGBuilder& GraphBuilder)
{
	auto DefaultBuffer = GraphBuilder.CreateSRV(GSystemTextures.GetDefaultByteAddressBuffer(GraphBuilder, 4u));
	OutParameters.SkinningHeaders	= DefaultBuffer;
	OutParameters.BoneHierarchy		= DefaultBuffer;
	OutParameters.BoneObjectSpace	= DefaultBuffer;
	OutParameters.BoneTransforms	= DefaultBuffer;
}

IMPLEMENT_SCENE_EXTENSION(FSkinningSceneExtension);

bool FSkinningSceneExtension::ShouldCreateExtension(FScene& InScene)
{
	return NaniteSkinnedMeshesSupported() && DoesRuntimeSupportNanite(GetFeatureLevelShaderPlatform(InScene.GetFeatureLevel()), true, true);
}

FSkinningSceneExtension::FSkinningSceneExtension(FScene& InScene)
:	ISceneExtension(InScene)
{
	WorldRef = InScene.GetWorld();
	UpdateTimerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FSkinningSceneExtension::Tick));
}

FSkinningSceneExtension::~FSkinningSceneExtension()
{
	FTSTicker::GetCoreTicker().RemoveTicker(UpdateTimerHandle);
}

void FSkinningSceneExtension::InitExtension(FScene& InScene)
{
	// Determine if we want to be initially enabled or disabled
	const bool bNaniteEnabled = UseNanite(GetFeatureLevelShaderPlatform(InScene.GetFeatureLevel()));
	SetEnabled(bNaniteEnabled);

	// Register animation runtime and reference pose transform providers
	if (auto TransformProvider = Scene.GetExtensionPtr<FSkinningTransformProvider>())
	{
		TransformProvider->RegisterProvider(
			GetRefPoseProviderId(),
			FSkinningTransformProvider::FOnProvideTransforms::CreateStatic(&FSkinningSceneExtension::ProvideRefPoseTransforms)
		);

		TransformProvider->RegisterProvider(
			GetAnimRuntimeProviderId(),
			FSkinningTransformProvider::FOnProvideTransforms::CreateStatic(&FSkinningSceneExtension::ProvideAnimRuntimeTransforms)
		);
	}
}

ISceneExtensionUpdater* FSkinningSceneExtension::CreateUpdater()
{
	return new FUpdater(*this);
}

ISceneExtensionRenderer* FSkinningSceneExtension::CreateRenderer(FSceneRendererBase& InSceneRenderer, const FEngineShowFlags& EngineShowFlags)
{
	// We only need to create renderers when we're enabled
	if (!IsEnabled())
	{
		return nullptr;
	}

	return new FRenderer(InSceneRenderer, *this);
}

void FSkinningSceneExtension::SetEnabled(bool bEnabled)
{
	if (bEnabled != IsEnabled())
	{
		if (bEnabled)
		{
			Buffers = MakeUnique<FBuffers>();
		}
		else
		{
			Buffers = nullptr;
			HierarchyAllocator.Reset();
			TransformAllocator.Reset();
			HeaderData.Reset();
		}
	}
}

void FSkinningSceneExtension::FinishSkinningBufferUpload(
	FRDGBuilder& GraphBuilder,
	FNaniteSkinningParameters* OutParams
)
{
	if (!IsEnabled())
	{
		return;
	}

	FRDGBufferRef HeaderBuffer = nullptr;
	FRDGBufferRef BoneHierarchyBuffer = nullptr;
	FRDGBufferRef BoneObjectSpaceBuffer = nullptr;
	FRDGBufferRef TransformBuffer = nullptr;

	// Sync on upload tasks
	UE::Tasks::Wait(
		MakeArrayView(
			{
				TaskHandles[UploadHeaderDataTask],
				TaskHandles[UploadHierarchyDataTask],
				TaskHandles[UploadTransformDataTask]
			}
		)
	);

	const uint32 MinHeaderDataSize = (HeaderData.GetMaxIndex() + 1);
	const uint32 MinTransformDataSize = TransformAllocator.GetMaxSize();
	const uint32 MinHierarchyDataSize = HierarchyAllocator.GetMaxSize();
	const uint32 MinObjectSpaceDataSize = ObjectSpaceAllocator.GetMaxSize();

	RDG_GPU_MASK_SCOPE(GraphBuilder, FRHIGPUMask::All());

	if (Uploader.IsValid())
	{
		HeaderBuffer = Uploader->HeaderDataUploader.ResizeAndUploadTo(
			GraphBuilder,
			Buffers->HeaderDataBuffer,
			MinHeaderDataSize
		);

		BoneHierarchyBuffer = Uploader->BoneHierarchyUploader.ResizeAndUploadTo(
			GraphBuilder,
			Buffers->BoneHierarchyBuffer,
			MinHierarchyDataSize
		);

		BoneObjectSpaceBuffer = Uploader->BoneObjectSpaceUploader.ResizeAndUploadTo(
			GraphBuilder,
			Buffers->BoneObjectSpaceBuffer,
			MinObjectSpaceDataSize
		);

		TransformBuffer = Uploader->TransformDataUploader.ResizeAndUploadTo(
			GraphBuilder,
			Buffers->TransformDataBuffer,
			MinTransformDataSize
		);

		Uploader = nullptr;
	}
	else
	{
		HeaderBuffer			= Buffers->HeaderDataBuffer.ResizeBufferIfNeeded(GraphBuilder, MinHeaderDataSize);
		BoneHierarchyBuffer		= Buffers->BoneHierarchyBuffer.ResizeBufferIfNeeded(GraphBuilder, MinHierarchyDataSize);
		BoneObjectSpaceBuffer	= Buffers->BoneObjectSpaceBuffer.ResizeBufferIfNeeded(GraphBuilder, MinObjectSpaceDataSize);
		TransformBuffer			= Buffers->TransformDataBuffer.ResizeBufferIfNeeded(GraphBuilder, MinTransformDataSize);
	}

	if (OutParams != nullptr)
	{
		OutParams->SkinningHeaders	= GraphBuilder.CreateSRV(HeaderBuffer);
		OutParams->BoneHierarchy	= GraphBuilder.CreateSRV(BoneHierarchyBuffer);
		OutParams->BoneObjectSpace	= GraphBuilder.CreateSRV(BoneObjectSpaceBuffer);
		OutParams->BoneTransforms	= GraphBuilder.CreateSRV(TransformBuffer);
	}
}

void FSkinningSceneExtension::PerformSkinning(
	FNaniteSkinningParameters& Parameters,
	FRDGBuilder& GraphBuilder
)
{
	RDG_EVENT_SCOPE(GraphBuilder, "NaniteSkinning");

	const float CurrentDeltaTime = TickState->DeltaTime;
	TickState->DeltaTime = 0.0f;

	if (auto TransformProvider = Scene.GetExtensionPtr<FSkinningTransformProvider>())
	{
		if (HeaderData.Num() > 0 && CVarSkinningTransformProviders.GetValueOnRenderThread())
		{
			FPrimitiveSceneInfo** Primitives = GraphBuilder.AllocPODArray<FPrimitiveSceneInfo*>(HeaderData.Num());
			uint32* TransformOffsets = GraphBuilder.AllocPODArray<uint32>(HeaderData.Num());
			TArray<uint8, FConcurrentLinearArrayAllocator> PrimitivesToRangeIndex;
			PrimitivesToRangeIndex.AddUninitialized(HeaderData.Num());

			uint32 TotalOffset = 0;

			// TODO: Optimize further (incremental tracking of primitives within provider extension?)
			// The current assumption is that skinned primitive counts should be fairly low, and heavy
			// instancing would be used. If we need a ton of primitives, revisit this algorithm.

			const TArray<FGuid> ProviderIds = TransformProvider->GetProviderIds();
			checkf(ProviderIds.Num() < 256, TEXT("The number of provider ids exceeds storage capacity for PrimitivesToRangeIndex."));

			TArray<FSkinningTransformProvider::FProviderRange, TInlineAllocator<8>> Ranges;
			Ranges.Reserve(ProviderIds.Num());
			for (const FGuid& ProviderId : ProviderIds)
			{
				FSkinningTransformProvider::FProviderRange& Range = Ranges.Emplace_GetRef();
				Range.Id = ProviderId;
				Range.Count = 0;
				Range.Offset = 0;
			}

			uint32 PrimitiveCount = 0;
			for (typename TSparseArray<FHeaderData>::TConstIterator It(HeaderData); It; ++It)
			{
				const FHeaderData& Header = *It;
				int32 RangeIndex = 0;

				for (; RangeIndex < Ranges.Num(); ++RangeIndex)
				{
					FSkinningTransformProvider::FProviderRange& Range = Ranges[RangeIndex];

					if (Header.ProviderId == Range.Id)
					{
						++Range.Count;
						break;
					}
				}

				check(RangeIndex != Ranges.Num());

				PrimitivesToRangeIndex[PrimitiveCount] = RangeIndex;
				Primitives[PrimitiveCount] = Header.PrimitiveSceneInfo;
				TransformOffsets[PrimitiveCount] = Header.TransformBufferOffset;

				++PrimitiveCount;
			}

			uint32 IndirectionCount = 0;

			for (FSkinningTransformProvider::FProviderRange& Range : Ranges)
			{
				Range.Offset = IndirectionCount;
				IndirectionCount += Range.Count;
				Range.Count = 0;
			}

			FUintVector2* PrimitiveIndices = GraphBuilder.AllocPODArray<FUintVector2>(IndirectionCount);
			for (uint32 PrimitiveIndex = 0; PrimitiveIndex < PrimitiveCount; ++PrimitiveIndex)
			{
				FSkinningTransformProvider::FProviderRange& Range = Ranges[PrimitivesToRangeIndex[PrimitiveIndex]];
				PrimitiveIndices[Range.Offset + Range.Count] = FUintVector2(PrimitiveIndex, TransformOffsets[PrimitiveIndex] * sizeof(FCompressedBoneTransform));
				++Range.Count;
			}

			TConstArrayView<FPrimitiveSceneInfo*> PrimitivesView(Primitives, PrimitiveCount);
			TConstArrayView<FUintVector2> IndiciesView(PrimitiveIndices, IndirectionCount);

			FSkinningTransformProvider::FProviderContext Context(
				PrimitivesView,
				IndiciesView,
				CurrentDeltaTime,
				GraphBuilder,
 				Parameters.BoneTransforms->GetParent()
			);

			TransformProvider->Broadcast(Ranges, Context);
		}
	}
}

bool FSkinningSceneExtension::ProcessBufferDefragmentation()
{
	// Consolidate spans
	ObjectSpaceAllocator.Consolidate();
	HierarchyAllocator.Consolidate();
	TransformAllocator.Consolidate();

	// Decide to defragment the buffer when the used size dips below a certain multiple of the max used size.
	// Since the buffer allocates in powers of two, we pick the mid point between 1/4 and 1/2 in hopes to prevent
	// thrashing when usage is close to a power of 2.
	//
	// NOTES:
	//	* We only currently use the state of the transform buffer's fragmentation to decide to defrag all buffers
	//	* Rather than trying to minimize number of moves/uploads, we just realloc and re-upload everything. This
	//	  could be implemented in a more efficient manner if the current method proves expensive.

	const bool bAllowDefrag = CVarNaniteTransformBufferDefrag.GetValueOnRenderThread();
	static const int32 MinTransformBufferCount = CVarNaniteTransformDataBufferMinSizeBytes.GetValueOnRenderThread() / sizeof(FCompressedBoneTransform);
	const float LowWaterMarkRatio = CVarNaniteTransformBufferDefragLowWaterMark.GetValueOnRenderThread();
	const int32 EffectiveMaxSize = FMath::RoundUpToPowerOfTwo(TransformAllocator.GetMaxSize());
	const int32 LowWaterMark = uint32(EffectiveMaxSize * LowWaterMarkRatio);
	const int32 UsedSize = TransformAllocator.GetSparselyAllocatedSize();
	
	if (!bAllowDefrag)
	{
		return false;
	}

	// Check to force a defrag
	const bool bForceDefrag = GNaniteTransformBufferForceDefrag != 0;
	if (GNaniteTransformBufferForceDefrag == 1)
	{
		GNaniteTransformBufferForceDefrag = 0;
	}
	
	if (!bForceDefrag && (EffectiveMaxSize <= MinTransformBufferCount || UsedSize > LowWaterMark))
	{
		// No need to defragment
		return false;
	}

	ObjectSpaceAllocator.Reset();
	HierarchyAllocator.Reset();
	TransformAllocator.Reset();

	for (auto& Data : HeaderData)
	{
		if (Data.TransformBufferOffset != INDEX_NONE)
		{
			Data.TransformBufferOffset = INDEX_NONE;
			Data.TransformBufferCount = 0;
		}

		if (Data.HierarchyBufferOffset != INDEX_NONE)
		{
			Data.HierarchyBufferOffset = INDEX_NONE;
			Data.HierarchyBufferCount = 0;
		}

		if (Data.ObjectSpaceBufferOffset != INDEX_NONE)
		{
			Data.ObjectSpaceBufferOffset = INDEX_NONE;
			Data.ObjectSpaceBufferCount = 0;
		}
	}

	return true;
}

bool FSkinningSceneExtension::Tick(float InDeltaTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FSkinningSceneExtension::Tick);

	FVector NewCameraLocation = FVector::ZeroVector;
	if (UWorld* World = GetWorld())
	{
		if (auto PlayerController = World->GetFirstPlayerController<APlayerController>())
		{
			FRotator CameraRotation;
			PlayerController->GetPlayerViewPoint(NewCameraLocation, CameraRotation);
		}
		else
		{
			FVector LocationSum = FVector::Zero();
			if (World->ViewLocationsRenderedLastFrame.Num() > 0)
			{
				for (const auto& Location : World->ViewLocationsRenderedLastFrame)
				{
					LocationSum += Location;
				}

				NewCameraLocation = LocationSum / World->ViewLocationsRenderedLastFrame.Num();
			}
		}
	}

	// Takes a reference to keep the timer around since the update happens on the GT timeline.
	ENQUEUE_RENDER_COMMAND(FTickSkinningSceneExtension)
	([TickState = TickState, InDeltaTime, NewCameraLocation](FRHICommandListImmediate& RHICmdList)
	{
		TickState->DeltaTime += InDeltaTime;
		TickState->CameraLocation = NewCameraLocation;
	});
	return true;
}

UWorld* FSkinningSceneExtension::GetWorld() const
{
	return WorldRef.Get();
}

void FSkinningSceneExtension::WaitForHeaderDataUpdateTasks() const
{
	UE::Tasks::Wait(MakeArrayView( { TaskHandles[FreeBufferSpaceTask], TaskHandles[InitHeaderDataTask] } ));
}

FSkinningSceneExtension::FBuffers::FBuffers()
: HeaderDataBuffer(CVarNanitePrimitiveSkinningDataBufferMinSizeBytes.GetValueOnAnyThread() >> 2u, TEXT("Nanite.SkinningHeaders"))
, BoneHierarchyBuffer(CVarNaniteTransformDataBufferMinSizeBytes.GetValueOnAnyThread() >> 2u, TEXT("Nanite.BoneHierarchy"))
, BoneObjectSpaceBuffer(CVarNaniteTransformDataBufferMinSizeBytes.GetValueOnAnyThread() >> 2u, TEXT("Nanite.BoneObjectSpace"))
, TransformDataBuffer(CVarNaniteTransformDataBufferMinSizeBytes.GetValueOnAnyThread() >> 2u, TEXT("Nanite.BoneTransforms"))
{
}

FSkinningSceneExtension::FUpdater::FUpdater(FSkinningSceneExtension& InSceneData)
: SceneData(&InSceneData)
, bEnableAsync(CVarNaniteTransformBufferAsyncUpdates.GetValueOnRenderThread())
{
}

void FSkinningSceneExtension::FUpdater::End()
{
	// Ensure these tasks finish before we fall out of scope.
	// NOTE: This should be unnecessary if the updater shares the graph builder's lifetime but we don't enforce that
	SceneData->SyncAllTasks();
}

void FSkinningSceneExtension::FUpdater::PreSceneUpdate(FRDGBuilder& GraphBuilder, const FScenePreUpdateChangeSet& ChangeSet, FSceneUniformBuffer& SceneUniforms)
{
	// If there was a pending upload from a prior update (due to the buffer never being used), finish the upload now.
	// This keeps the upload entries from growing unbounded and prevents any undefined behavior caused by any
	// updates that overlap primitives.
	SceneData->FinishSkinningBufferUpload(GraphBuilder);

	// Update whether or not we are enabled based on in Nanite is enabled
	const bool bNaniteEnabled = UseNanite(GetFeatureLevelShaderPlatform(SceneData->Scene.GetFeatureLevel()));
	SceneData->SetEnabled(bNaniteEnabled);

	if (!SceneData->IsEnabled())
	{
		return;
	}

	SceneData->TaskHandles[FreeBufferSpaceTask] = GraphBuilder.AddSetupTask(
		[this, RemovedList = ChangeSet.RemovedPrimitiveIds]
		{
			// Remove and free transform data for removed primitives
			// NOTE: Using the ID list instead of the primitive list since we're in an async task
			for (const auto& PersistentIndex : RemovedList)
			{
				if (SceneData->HeaderData.IsValidIndex(PersistentIndex.Index))
				{
					FSkinningSceneExtension::FHeaderData& Data = SceneData->HeaderData[PersistentIndex.Index];
					SceneData->LoadBalancer.SubReservedInstances(Data.NumInstanceSceneDataEntries);

					if (Data.ObjectSpaceBufferOffset != INDEX_NONE)
					{
						SceneData->ObjectSpaceAllocator.Free(Data.ObjectSpaceBufferOffset, Data.ObjectSpaceBufferCount);
					}

					if (Data.HierarchyBufferOffset != INDEX_NONE)
					{
						SceneData->HierarchyAllocator.Free(Data.HierarchyBufferOffset, Data.HierarchyBufferCount);
					}

					if (Data.TransformBufferOffset != INDEX_NONE)
					{
						SceneData->TransformAllocator.Free(Data.TransformBufferOffset, Data.TransformBufferCount);
					}
		
					SceneData->HeaderData.RemoveAt(PersistentIndex.Index);
				}
			}

			// Check to force a full upload by CVar
			// NOTE: Doesn't currently discern which scene to affect
			bForceFullUpload = GNaniteTransformBufferForceFullUpload != 0;
			if (GNaniteTransformBufferForceFullUpload == 1)
			{
				GNaniteTransformBufferForceFullUpload = 0;
			}

			bDefragging = SceneData->ProcessBufferDefragmentation();
			bForceFullUpload |= bDefragging;
		},
		UE::Tasks::ETaskPriority::Normal,
		bEnableAsync
	);
}

void FSkinningSceneExtension::FUpdater::PostSceneUpdate(FRDGBuilder& GraphBuilder, const FScenePostUpdateChangeSet& ChangeSet)
{
	if (!SceneData->IsEnabled())
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(FSkinningSceneExtension::FUpdater::PostSceneUpdate);

	// Cache the updated PrimitiveSceneInfos (this is safe as long as we only access it in updater funcs and RDG setup tasks)
	AddedList = ChangeSet.AddedPrimitiveSceneInfos;

	// Kick off a task to initialize added transform ranges
	if (AddedList.Num() > 0)
	{
		SceneData->TaskHandles[InitHeaderDataTask] = GraphBuilder.AddSetupTask(
			[this]
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FSkinningSceneExtension::FUpdater::PostSceneUpdate::AddPrimitives);

				// Skip any non-Nanite primitives, or rigid Nanite primitives
				for (auto PrimitiveSceneInfo : AddedList)
				{
					if (!PrimitiveSceneInfo->Proxy->IsNaniteMesh())
					{
						continue;
					}

					auto* NaniteProxy = static_cast<Nanite::FSceneProxyBase*>(PrimitiveSceneInfo->Proxy);
					if (!NaniteProxy->IsSkinnedMesh())
					{
						continue;
					}

					auto* SkinnedProxy = static_cast<Nanite::FSkinnedSceneProxy*>(NaniteProxy);

					const int32 PersistentIndex = PrimitiveSceneInfo->GetPersistentIndex().Index;

					FHeaderData NewHeader;
					NewHeader.InstanceSceneDataOffset = PrimitiveSceneInfo->GetInstanceSceneDataOffset();
					NewHeader.NumInstanceSceneDataEntries = PrimitiveSceneInfo->GetNumInstanceSceneDataEntries();
					NewHeader.ProviderId = SkinnedProxy->GetTransformProviderId();
					NewHeader.PrimitiveSceneInfo = PrimitiveSceneInfo;
					NewHeader.MaxTransformCount = SkinnedProxy->GetMaxBoneTransformCount();
					NewHeader.MaxInfluenceCount = SkinnedProxy->GetMaxBoneInfluenceCount();
					NewHeader.UniqueAnimationCount = SkinnedProxy->GetUniqueAnimationCount();
					NewHeader.bHasScale = SkinnedProxy->HasScale();
					NewHeader.AnimationMinScreenSize = FMath::Clamp(SkinnedProxy->GetAnimationMinScreenSize(), -1.0f, 1.0f); 

					SceneData->LoadBalancer.AddReservedInstances(NewHeader.NumInstanceSceneDataEntries);
					SceneData->HeaderData.EmplaceAt(PersistentIndex, NewHeader);

					if (!bForceFullUpload)
					{
						DirtyPrimitiveList.Add(PersistentIndex);
					}
				}
			},
			SceneData->TaskHandles[FreeBufferSpaceTask],
			UE::Tasks::ETaskPriority::Normal,
			bEnableAsync
		);
	}

	SceneData->LoadBalancer.LoadBalancer = GraphBuilder.AllocObject<FLoadBalancer>();

	SceneData->TaskHandles[FillLoadBalancerDataTask] = GraphBuilder.AddSetupTask(
		[this, &PrimitiveUpdates = ChangeSet.PrimitiveUpdates]
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FSkinningSceneExtension::FUpdater::PostSceneUpdate::FillLoadBalancerData);

			PrimitiveUpdates.ForEachUpdateCommand(ESceneUpdateCommandFilter::Updated, EPrimitiveUpdateDirtyFlags::InstanceData, [&](const FPrimitiveUpdateCommand& Cmd)
			{
				FPrimitiveSceneInfo* PrimitiveSceneInfo = Cmd.GetSceneInfo();

				if (!PrimitiveSceneInfo->Proxy->IsNaniteMesh())
				{
					return;
				}

				auto* NaniteProxy = static_cast<Nanite::FSceneProxyBase*>(PrimitiveSceneInfo->Proxy);
				if (!NaniteProxy->IsSkinnedMesh())
				{
					return;
				}

				FHeaderData& UpdateHeader = SceneData->HeaderData[PrimitiveSceneInfo->GetPersistentIndex().Index];

				SceneData->LoadBalancer.SubReservedInstances(UpdateHeader.NumInstanceSceneDataEntries);
				UpdateHeader.InstanceSceneDataOffset = PrimitiveSceneInfo->GetInstanceSceneDataOffset();
				UpdateHeader.NumInstanceSceneDataEntries = PrimitiveSceneInfo->GetNumInstanceSceneDataEntries();
				SceneData->LoadBalancer.AddReservedInstances(UpdateHeader.NumInstanceSceneDataEntries);
			});

			SceneData->LoadBalancer.LoadBalancer->ReserveStorage(SceneData->LoadBalancer.NumReservedItems, SceneData->LoadBalancer.NumReservedItems);

			for (FHeaderData& HeaderDataItem : SceneData->HeaderData)
			{
				SceneData->LoadBalancer.LoadBalancer->Add(HeaderDataItem.InstanceSceneDataOffset, HeaderDataItem.NumInstanceSceneDataEntries, 0);
			}
		},
		MakeArrayView({ SceneData->TaskHandles[InitHeaderDataTask], SceneData->TaskHandles[FreeBufferSpaceTask] }),
		UE::Tasks::ETaskPriority::Normal,
		bEnableAsync
	);
}

static bool IsValidSkinnedSceneInfo(const FPrimitiveSceneInfo* SceneInfo)
{
	if (SceneInfo == nullptr || SceneInfo->Proxy == nullptr)
	{
		return false;
	}

	if (!SceneInfo->Proxy->IsNaniteMesh() || !SceneInfo->Proxy->IsSkinnedMesh())
	{
		return false;
	}

	return true;
}

void FSkinningSceneExtension::FUpdater::PostMeshUpdate(
	FRDGBuilder& GraphBuilder,
	const TConstArrayView<FPrimitiveSceneInfo*>& UpdatedSceneInfoList
)
{
	UpdateList = UpdatedSceneInfoList;

	if (SceneData->IsEnabled())
	{
		// Gets the information needed from the primitive for skinning and allocates the appropriate space in the buffer
		// for the primitive's bone transforms
		auto AllocSpaceForPrimitive = [this](FHeaderData& Data)
		{
			auto* NaniteProxy = static_cast<Nanite::FSceneProxyBase*>(Data.PrimitiveSceneInfo->Proxy);
			check(NaniteProxy->IsSkinnedMesh());

			auto* SkinnedProxy = static_cast<Nanite::FSkinnedSceneProxy*>(NaniteProxy);

			Data.MaxTransformCount		= SkinnedProxy->GetMaxBoneTransformCount();
			Data.MaxInfluenceCount		= SkinnedProxy->GetMaxBoneInfluenceCount();
			Data.UniqueAnimationCount	= SkinnedProxy->GetUniqueAnimationCount();

			bool bRequireUpload = false;

			const uint32 ObjectSpaceNeededSize = Data.MaxTransformCount * SkinnedProxy->GetObjectSpaceFloatCount();
			if (ObjectSpaceNeededSize != Data.ObjectSpaceBufferCount)
			{
				if (Data.ObjectSpaceBufferCount > 0)
				{
					SceneData->ObjectSpaceAllocator.Free(Data.ObjectSpaceBufferOffset, Data.ObjectSpaceBufferCount);
				}

				Data.ObjectSpaceBufferOffset = ObjectSpaceNeededSize > 0 ? SceneData->ObjectSpaceAllocator.Allocate(ObjectSpaceNeededSize) : INDEX_NONE;
				Data.ObjectSpaceBufferCount = ObjectSpaceNeededSize;

				if (!bForceFullUpload)
				{
					bRequireUpload = true;
				}
			}

			const uint32 HierarchyNeededSize = Data.MaxTransformCount;
			if (HierarchyNeededSize != Data.HierarchyBufferCount)
			{
				if (Data.HierarchyBufferCount > 0)
				{
					SceneData->HierarchyAllocator.Free(Data.HierarchyBufferOffset, Data.HierarchyBufferCount);
				}

				Data.HierarchyBufferOffset = HierarchyNeededSize > 0 ? SceneData->HierarchyAllocator.Allocate(HierarchyNeededSize) : INDEX_NONE;
				Data.HierarchyBufferCount = HierarchyNeededSize;

				if (!bForceFullUpload)
				{
					bRequireUpload = true;
				}
			}

			const uint32 TransformNeededSize = Data.UniqueAnimationCount * Data.MaxTransformCount * 2u; // Current and Previous
			if (bRequireUpload || (TransformNeededSize != Data.TransformBufferCount))
			{
				if (Data.TransformBufferCount > 0)
				{
					SceneData->TransformAllocator.Free(Data.TransformBufferOffset, Data.TransformBufferCount);
				}

				Data.TransformBufferOffset = TransformNeededSize > 0 ? SceneData->TransformAllocator.Allocate(TransformNeededSize) : INDEX_NONE;
				Data.TransformBufferCount = TransformNeededSize;

				if (!bForceFullUpload)
				{
					bRequireUpload = true;
				}
			}

			if (bRequireUpload)
			{
				DirtyPrimitiveList.Add(Data.PrimitiveSceneInfo->GetPersistentIndex().Index);
			}
		};

		// Kick off the allocate task (synced just prior to header uploads)
		SceneData->TaskHandles[AllocBufferSpaceTask] = GraphBuilder.AddSetupTask(
			[this, AllocSpaceForPrimitive]
			{
				if (bDefragging)
				{
					for (auto& Data : SceneData->HeaderData)
					{
						AllocSpaceForPrimitive(Data);
					}
				}
				else
				{
					// Only check to reallocate space for primitives that have requested an update
					for (auto PrimitiveSceneInfo : UpdateList)
					{
						const int32 Index = PrimitiveSceneInfo->GetPersistentIndex().Index;
						if (!SceneData->HeaderData.IsValidIndex(Index))
						{
							// Primitive in update list is either non-Nanite and/or not skinned
							continue;
						}
						AllocSpaceForPrimitive(SceneData->HeaderData[Index]);
					}
				}

				// Only create a new uploader here if one of the two dependent upload tasks will use it
				if (bForceFullUpload || DirtyPrimitiveList.Num() > 0 || UpdateList.Num() > 0)
				{
					SceneData->Uploader = MakeUnique<FUploader>();
				}
			},
			MakeArrayView(
				{
					SceneData->TaskHandles[FreeBufferSpaceTask],
					SceneData->TaskHandles[InitHeaderDataTask]
				}
			),
			UE::Tasks::ETaskPriority::Normal,
			bEnableAsync
		);

		auto UploadHeaderData = [this](const FHeaderData& Data)
		{
			const int32 PersistentIndex = Data.PrimitiveSceneInfo->GetPersistentIndex().Index;

			// Catch when/if no transform buffer data is allocated for a primitive we're tracking.
			// This should be indicative of a bug.
			ensure(Data.HierarchyBufferCount != INDEX_NONE && Data.TransformBufferCount != INDEX_NONE);

			check(SceneData->Uploader.IsValid()); // Sanity check
			SceneData->Uploader->HeaderDataUploader.Add(Data.Pack(), PersistentIndex);
		};

		// Kick off the header data upload task (synced when accessing the buffer)
		SceneData->TaskHandles[UploadHeaderDataTask] = GraphBuilder.AddSetupTask(
			[this, UploadHeaderData]
			{
				if (bForceFullUpload)
				{
					for (auto& Data : SceneData->HeaderData)
					{
						UploadHeaderData(Data);
					}
				}
				else
				{
					// Sort the array so we can skip duplicate entries
					DirtyPrimitiveList.Sort();
					int32 LastPersistentIndex = INDEX_NONE;
					for (auto PersistentIndex : DirtyPrimitiveList)
					{
						if (PersistentIndex != LastPersistentIndex &&
							SceneData->HeaderData.IsValidIndex(PersistentIndex))
						{
							UploadHeaderData(SceneData->HeaderData[PersistentIndex]);
						}
						LastPersistentIndex = PersistentIndex;
					}
				}
			},
			MakeArrayView(
				{
					SceneData->TaskHandles[AllocBufferSpaceTask]
				}
			),
			UE::Tasks::ETaskPriority::Normal,
			bEnableAsync
		);

		auto UploadHierarchyData = [this](const FHeaderData& Data)
		{
			auto SkinnedProxy = static_cast<const Nanite::FSkinnedSceneProxy*>(Data.PrimitiveSceneInfo->Proxy);
			const TArray<uint32>& BoneHierarchy = SkinnedProxy->GetBoneHierarchy();
			const TArray<float>& BoneObjectSpace = SkinnedProxy->GetBoneObjectSpace();

			const uint32 FloatCount = SkinnedProxy->GetObjectSpaceFloatCount();
			check(BoneHierarchy.Num() == Data.MaxTransformCount);
			check(BoneObjectSpace.Num() == Data.MaxTransformCount * FloatCount);
			check(SceneData->Uploader.IsValid());

			// Bone Hierarchy
			{
				auto UploadData = SceneData->Uploader->BoneHierarchyUploader.AddMultiple_GetRef(
					Data.HierarchyBufferOffset,
					Data.HierarchyBufferCount
				);

				uint32* DstBoneHierarchyPtr = UploadData.GetData();
				for (int32 BoneIndex = 0; BoneIndex < Data.MaxTransformCount; ++BoneIndex)
				{
					DstBoneHierarchyPtr[BoneIndex] = BoneHierarchy[BoneIndex];
				}
			}

			// Bone Object Space
			{
				auto UploadData = SceneData->Uploader->BoneObjectSpaceUploader.AddMultiple_GetRef(
					Data.ObjectSpaceBufferOffset,
					Data.ObjectSpaceBufferCount
				);

				float* DstBoneObjectSpacePtr = UploadData.GetData();
				for (uint32 BoneFloatIndex = 0; BoneFloatIndex < (Data.MaxTransformCount * FloatCount); ++BoneFloatIndex)
				{
					DstBoneObjectSpacePtr[BoneFloatIndex] = BoneObjectSpace[BoneFloatIndex];
				}
			}
		};

		auto UploadTransformData = [this](const FHeaderData& Data, bool bProvidersEnabled)
		{
			auto SkinnedProxy = static_cast<const Nanite::FSkinnedSceneProxy*>(Data.PrimitiveSceneInfo->Proxy);
			if (bProvidersEnabled && SkinnedProxy->GetTransformProviderId().IsValid())
			{
				return;
			}

			// NOTE: This path is purely for debugging now - should also set "r.Nanite.SkinningBuffers.ForceFullUpload 2" to avoid caching artifacts

			check(SceneData->Uploader.IsValid());
			auto UploadData = SceneData->Uploader->TransformDataUploader.AddMultiple_GetRef(
				Data.TransformBufferOffset,
				Data.TransformBufferCount
			);

			check(Data.UniqueAnimationCount* Data.MaxTransformCount * 2u == Data.TransformBufferCount);

			FCompressedBoneTransform* DstCurrentBoneTransformsPtr = UploadData.GetData();
			FCompressedBoneTransform* DstPreviousBoneTransformsPtr = DstCurrentBoneTransformsPtr + Data.MaxTransformCount;
			const uint32 StridedPtrStep = Data.MaxTransformCount * 2u;

			for (int32 UniqueAnimation = 0; UniqueAnimation < Data.UniqueAnimationCount; ++UniqueAnimation)
			{
				for (int32 TransformIndex = 0; TransformIndex < Data.MaxTransformCount; ++TransformIndex)
				{
					SetCompressedBoneTransformIdentity(DstCurrentBoneTransformsPtr[TransformIndex]);
					SetCompressedBoneTransformIdentity(DstPreviousBoneTransformsPtr[TransformIndex]);
				}

				DstCurrentBoneTransformsPtr += StridedPtrStep;
				DstPreviousBoneTransformsPtr += StridedPtrStep;
			}
		};

		// Kick off the hierarchy data upload task (synced when accessing the buffer)
		SceneData->TaskHandles[UploadHierarchyDataTask] = GraphBuilder.AddSetupTask(
			[this, UploadHierarchyData]
			{
				if (bForceFullUpload)
				{
					for (auto& Data : SceneData->HeaderData)
					{
						UploadHierarchyData(Data);
					}
				}
				else
				{
					for (auto PrimitiveSceneInfo : UpdateList)
					{
						const int32 PersistentIndex = PrimitiveSceneInfo->GetPersistentIndex().Index;
						if (!SceneData->HeaderData.IsValidIndex(PersistentIndex))
						{
							// Primitive in update list is either non-Nanite and/or not skinned
							continue;
						}
						check(IsValidSkinnedSceneInfo(PrimitiveSceneInfo));
						UploadHierarchyData(SceneData->HeaderData[PersistentIndex]);
					}
				}
			},
			MakeArrayView({ SceneData->TaskHandles[AllocBufferSpaceTask] }),
			UE::Tasks::ETaskPriority::Normal,
			bEnableAsync
		);

		// Kick off the transform data upload task (synced when accessing the buffer)
		SceneData->TaskHandles[UploadTransformDataTask] = GraphBuilder.AddSetupTask(
			[this, UploadTransformData]
			{
				const bool bProvidersEnabled = CVarSkinningTransformProviders.GetValueOnRenderThread();

				if (bForceFullUpload)
				{
					for (auto& Data : SceneData->HeaderData)
					{
						UploadTransformData(Data, bProvidersEnabled);
					}
				}
				else
				{
					for (auto PrimitiveSceneInfo : UpdateList)
					{
						const int32 PersistentIndex = PrimitiveSceneInfo->GetPersistentIndex().Index;
						if (!SceneData->HeaderData.IsValidIndex(PersistentIndex))
						{
							// Primitive in update list is either non-Nanite and/or not skinned
							continue;
						}
						check(IsValidSkinnedSceneInfo(PrimitiveSceneInfo));
						UploadTransformData(SceneData->HeaderData[PersistentIndex], bProvidersEnabled);
					}
				}
			},
			MakeArrayView({ SceneData->TaskHandles[AllocBufferSpaceTask] }),
			UE::Tasks::ETaskPriority::Normal,
			bEnableAsync
		);

		if (!bEnableAsync)
		{
			// If disabling async, just finish the upload immediately
			SceneData->FinishSkinningBufferUpload(GraphBuilder);
		}
	}
}

class FNaniteSkinningUpdateViewDataCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FNaniteSkinningUpdateViewDataCS);
	SHADER_USE_PARAMETER_STRUCT(FNaniteSkinningUpdateViewDataCS, FGlobalShader)

public:
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FNaniteSkinningParameters, Scene_NaniteSkinning)
		SHADER_PARAMETER_STRUCT_INCLUDE(FGPUSceneResourceParameters, GPUScene)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLoadBalancer::FShaderParameters, LoadBalancerParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(RendererViewData::FWriterParameters, ViewDataParametersWriter)
		SHADER_PARAMETER(float, DefaultAnimationMinScreenSize)		
	END_SHADER_PARAMETER_STRUCT()

	// This is probably fine even in instance list mode
	static constexpr int ThreadGroupSize = FLoadBalancer::ThreadGroupSize;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportNanite(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), ThreadGroupSize);
		OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), 1);
		OutEnvironment.SetDefine(TEXT("USE_GLOBAL_GPU_SCENE_DATA"), 1);
		OutEnvironment.SetDefine(TEXT("VIEW_DATA_ACCESS_MODE"), VIEW_DATA_ACCESS_RW);
		// Don't access the global Scene uniform buffer but map to indivdual UBs for each used module.
		OutEnvironment.SetDefine(TEXT("USE_EXPLICIT_SCENE_UB_MODULES"), 1);

		OutEnvironment.CompilerFlags.Add(CFLAG_HLSL2021);

		FGPUScene::FInstanceGPULoadBalancer::SetShaderDefines(OutEnvironment);
	}
};
IMPLEMENT_GLOBAL_SHADER(FNaniteSkinningUpdateViewDataCS, "/Engine/Private/Nanite/NaniteSkinningUpdateViewData.usf", "NaniteSkinningUpdateViewDataCS", SF_Compute);


void FSkinningSceneExtension::FRenderer::UpdateViewData(FRDGBuilder& GraphBuilder, const FRendererViewDataManager& ViewDataManager)
{
	if (!SceneData->LoadBalancer.LoadBalancer)
	{
		return;
	}

	SceneData->TaskHandles[FillLoadBalancerDataTask].Wait();

	FLoadBalancer::FGPUData LoadBalancerGpuData = SceneData->LoadBalancer.LoadBalancer->Upload(GraphBuilder);
	SceneData->LoadBalancer.LoadBalancer = nullptr;

	{
		FNaniteSkinningUpdateViewDataCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FNaniteSkinningUpdateViewDataCS::FParameters>();

		//PassParameters->Scene = SceneCullingInfo.SceneUniformBuffer;
		PassParameters->GPUScene = SceneData->Scene.GPUScene.GetShaderParameters(GraphBuilder);
		FNaniteSkinningParameters* NaniteSkinningParameters = GraphBuilder.AllocParameters<FNaniteSkinningParameters>();
		SceneData->FinishSkinningBufferUpload(GraphBuilder, NaniteSkinningParameters);
		PassParameters->Scene_NaniteSkinning = GraphBuilder.CreateUniformBuffer(NaniteSkinningParameters);
		PassParameters->ViewDataParametersWriter = ViewDataManager.GetWriterShaderParameters(GraphBuilder);
		PassParameters->DefaultAnimationMinScreenSize = CVarNaniteSkinningDefaultAnimationMinScreenSize.GetValueOnRenderThread();

		auto ComputeShader = GetGlobalShaderMap(SceneData->Scene.GetFeatureLevel())->GetShader<FNaniteSkinningUpdateViewDataCS>();

		LoadBalancerGpuData.AddPass(
			GraphBuilder, 
			RDG_EVENT_NAME("NaniteSkinningUpdateViewData (%d batches)", LoadBalancerGpuData.NumBatches),
			ComputeShader,
			PassParameters
		);

	}
}

void FSkinningSceneExtension::FRenderer::UpdateSceneUniformBuffer(
	FRDGBuilder& GraphBuilder,
	FSceneUniformBuffer& SceneUniformBuffer
)
{
	check(SceneData->IsEnabled());
	FNaniteSkinningParameters Parameters;
	SceneData->FinishSkinningBufferUpload(GraphBuilder, &Parameters);
	SceneUniformBuffer.Set(SceneUB::NaniteSkinning, Parameters);
	SceneData->PerformSkinning(Parameters, GraphBuilder);
}

void FSkinningSceneExtension::GetSkinnedPrimitives(TArray<FPrimitiveSceneInfo*>& OutPrimitives) const
{
	OutPrimitives.Reset();

	if (!IsEnabled())
	{
		return;
	}

	WaitForHeaderDataUpdateTasks();

	OutPrimitives.Reserve(HeaderData.Num());

	for (typename TSparseArray<FHeaderData>::TConstIterator It(HeaderData); It; ++It)
	{
		const FHeaderData& Header = *It;
		OutPrimitives.Add(Header.PrimitiveSceneInfo);
	}
}

const FSkinningTransformProvider::FProviderId& FSkinningSceneExtension::GetRefPoseProviderId()
{
	return RefPoseProviderId;
}

const FSkinningTransformProvider::FProviderId& FSkinningSceneExtension::GetAnimRuntimeProviderId()
{
	return AnimRuntimeProviderId;
}

void FSkinningSceneExtension::ProvideRefPoseTransforms(FSkinningTransformProvider::FProviderContext& Context)
{
	const uint32 TransformsPerGroup = FRefPoseTransformProviderCS::TransformsPerGroup;

	// TODO: Optimize further

	uint32 BlockCount = 0;
	for (const FUintVector2& Indirection : Context.Indirections)
	{
		const FPrimitiveSceneInfo* Primitive = Context.Primitives[Indirection.X];
		auto* SkinnedProxy = static_cast<Nanite::FSkinnedSceneProxy*>(Primitive->Proxy);
		const uint32 TransformCount = SkinnedProxy->GetMaxBoneTransformCount();
		const uint32 AnimationCount = SkinnedProxy->GetUniqueAnimationCount();
		BlockCount += FMath::DivideAndRoundUp(TransformCount * AnimationCount, TransformsPerGroup);
	}

	if (BlockCount == 0)
	{
		return;
	}

	FRDGBuilder& GraphBuilder = Context.GraphBuilder;
	FTransformBlockHeader* BlockHeaders = GraphBuilder.AllocPODArray<FTransformBlockHeader>(BlockCount);

	uint32 BlockWrite = 0;
	for (const FUintVector2& Indirection : Context.Indirections)
	{
		const FPrimitiveSceneInfo* Primitive = Context.Primitives[Indirection.X];
		auto* SkinnedProxy = static_cast<Nanite::FSkinnedSceneProxy*>(Primitive->Proxy);
		const uint32 TransformCount = SkinnedProxy->GetMaxBoneTransformCount();
		const uint32 AnimationCount = SkinnedProxy->GetUniqueAnimationCount();
		const uint32 TotalTransformCount = TransformCount * AnimationCount;

		uint32 TransformWrite = Indirection.Y;

		const uint32 FullBlockCount = TotalTransformCount / TransformsPerGroup;
		for (uint32 BlockIndex = 0; BlockIndex < FullBlockCount; ++BlockIndex)
		{
			BlockHeaders[BlockWrite].BlockLocalIndex = BlockIndex;
			BlockHeaders[BlockWrite].BlockTransformCount = TransformsPerGroup;
			BlockHeaders[BlockWrite].BlockTransformOffset = TransformWrite;
			++BlockWrite;

			TransformWrite += (TransformsPerGroup * 2 * sizeof(FCompressedBoneTransform));
		}

		const uint32 PartialTransformCount = TotalTransformCount - (FullBlockCount * TransformsPerGroup);
		if (PartialTransformCount > 0)
		{
			BlockHeaders[BlockWrite].BlockLocalIndex = FullBlockCount;
			BlockHeaders[BlockWrite].BlockTransformCount = PartialTransformCount;
			BlockHeaders[BlockWrite].BlockTransformOffset = TransformWrite;
			++BlockWrite;
		}
	}

	FRDGBufferRef BlockHeaderBuffer = CreateStructuredBuffer(
		GraphBuilder,
		TEXT("Skinning.RefPoseHeaders"),
		sizeof(FTransformBlockHeader),
		FMath::RoundUpToPowerOfTwo(FMath::Max(BlockCount, 1u)),
		BlockHeaders,
		sizeof(FTransformBlockHeader) * BlockCount,
		// The buffer data is allocated above on the RDG timeline
		ERDGInitialDataFlags::NoCopy
	);

	FRefPoseTransformProviderCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FRefPoseTransformProviderCS::FParameters>();
	PassParameters->TransformBuffer = GraphBuilder.CreateUAV(Context.TransformBuffer);
	PassParameters->HeaderBuffer = GraphBuilder.CreateSRV(BlockHeaderBuffer);

	auto ComputeShader = GetGlobalShaderMap(GMaxRHIFeatureLevel)->GetShader<FRefPoseTransformProviderCS>();
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("RefPoseProvider"),
		ComputeShader,
		PassParameters,
		FIntVector(BlockCount, 1, 1)
	);
}

BEGIN_SHADER_PARAMETER_STRUCT(FCopyBufferParameters, )
	RDG_BUFFER_ACCESS(SrcBuffer, ERHIAccess::CopySrc)
	RDG_BUFFER_ACCESS(DstBuffer, ERHIAccess::CopyDest)
END_SHADER_PARAMETER_STRUCT()

void FSkinningSceneExtension::ProvideAnimRuntimeTransforms(FSkinningTransformProvider::FProviderContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FSkinningSceneExtension::ProvideAnimRuntimeTransforms);
	uint32 GlobalTransformCount = 0;

	for (const FUintVector2& Indirection : Context.Indirections)
	{
		const FPrimitiveSceneInfo* Primitive = Context.Primitives[Indirection.X];
		auto* SkinnedProxy = static_cast<Nanite::FSkinnedSceneProxy*>(Primitive->Proxy);
		const uint32 TransformCount = SkinnedProxy->GetMaxBoneTransformCount();
		const uint32 AnimationCount = SkinnedProxy->GetUniqueAnimationCount();
		GlobalTransformCount += (TransformCount * AnimationCount) * 2; // Current and Previous
	}

	if (GlobalTransformCount == 0)
	{
		return;
	}

	FRDGBuilder& GraphBuilder = Context.GraphBuilder;
	FCompressedBoneTransform* Transforms = reinterpret_cast<FCompressedBoneTransform*>(GraphBuilder.Alloc(sizeof(FCompressedBoneTransform) * GlobalTransformCount));

	struct FCopyCommand
	{
		uint32 DstOffset	= 0;
		uint32 SrcOffset	= 0;
		uint32 NumBytes		= 0;
	};

	auto& CopyCommands = *GraphBuilder.AllocObject<TArray<FCopyCommand, SceneRenderingAllocator>>();
	CopyCommands.Reserve(Context.Indirections.Num());

	GraphBuilder.AddSetupTask([&CopyCommands, Transforms, Indirections = Context.Indirections, Primitives = Context.Primitives]
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FSkinningSceneExtension::ProvideAnimRuntimeTransformsTask);

		uint32 TransformWrite = 0;

		for (const FUintVector2& Indirection : Indirections)
		{
			const FPrimitiveSceneInfo* Primitive = Primitives[Indirection.X];
			auto* SkinnedProxy = static_cast<Nanite::FSkinnedSceneProxy*>(Primitive->Proxy);

			const uint32 TransformCount = SkinnedProxy->GetMaxBoneTransformCount();
			const uint32 AnimationCount = SkinnedProxy->GetUniqueAnimationCount();
			const uint32 TotalTransformCount = (TransformCount * AnimationCount) * 2; // Current and Previous

			// Fetch bone transforms from Nanite mesh object and upload to GPU (3x4 transposed)
			const TArray<FMatrix3x4>* SrcCurrentTransforms = SkinnedProxy->GetMeshObject()->GetCurrentBoneTransforms();
			const TArray<FMatrix3x4>* SrcPreviousTransforms = SkinnedProxy->GetMeshObject()->GetPreviousBoneTransforms();

			FCompressedBoneTransform* DstCurrentTransforms	= Transforms + TransformWrite;
			FCompressedBoneTransform* DstPreviousTransforms	= DstCurrentTransforms + TransformCount;

			if (SrcCurrentTransforms != nullptr && SrcPreviousTransforms != nullptr &&
				(uint32(SrcCurrentTransforms->Num() + SrcPreviousTransforms->Num()) == TotalTransformCount))
			{
				const FMatrix3x4* SrcCurrentTransformsPtr  = SrcCurrentTransforms->GetData();
				const FMatrix3x4* SrcPreviousTransformsPtr = SrcPreviousTransforms->GetData();

				const uint32 StridedPtrStep = TransformCount * 2u;

				for (uint32 UniqueAnimation = 0; UniqueAnimation < AnimationCount; ++UniqueAnimation)
				{
				#if USE_COMPRESSED_BONE_TRANSFORM
					for (uint32 i = 0; i < TransformCount; i++)
					{
						StoreCompressedBoneTransform(DstCurrentTransforms[i], SrcCurrentTransformsPtr[i]);
						StoreCompressedBoneTransform(DstPreviousTransforms[i], SrcPreviousTransformsPtr[i]);
					}
				#else			
					FMemory::Memcpy(DstCurrentTransforms,  SrcCurrentTransformsPtr,  sizeof(FCompressedBoneTransform) * TransformCount);
					FMemory::Memcpy(DstPreviousTransforms, SrcPreviousTransformsPtr, sizeof(FCompressedBoneTransform) * TransformCount);
				#endif

					DstCurrentTransforms  += StridedPtrStep;
					DstPreviousTransforms += StridedPtrStep;

					SrcCurrentTransforms  += TransformCount;
					SrcPreviousTransforms += TransformCount;
				}
			}
			else
			{
				// Data is invalid, replace with reference pose
				for (uint32 TransformIndex = 0; TransformIndex < (TransformCount * AnimationCount); ++TransformIndex)
				{
					SetCompressedBoneTransformIdentity(DstCurrentTransforms[TransformIndex]);
					SetCompressedBoneTransformIdentity(DstPreviousTransforms[TransformIndex]);
				}
			}

			FCopyCommand& Command = CopyCommands.Emplace_GetRef();
			Command.SrcOffset = TransformWrite * sizeof(FCompressedBoneTransform);
			Command.DstOffset = Indirection.Y;
			Command.NumBytes  = TotalTransformCount * sizeof(FCompressedBoneTransform);

			TransformWrite += TotalTransformCount;
		}

	}, UE::Tasks::ETaskPriority::High);

	FRDGBufferRef SrcTransformBuffer = CreateUploadBuffer(
		GraphBuilder,
		TEXT("Skinning.AnimTransforms"),
		sizeof(FCompressedBoneTransform),
		GlobalTransformCount,
		Transforms,
		sizeof(FCompressedBoneTransform) * GlobalTransformCount,
		// The buffer data is allocated above on the RDG timeline
		ERDGInitialDataFlags::NoCopy
	);

	FCopyBufferParameters* PassParameters = GraphBuilder.AllocParameters<FCopyBufferParameters>();
	PassParameters->SrcBuffer = SrcTransformBuffer;
	PassParameters->DstBuffer = Context.TransformBuffer;

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("CopyBuffer (%s Size=%ubytes)", SrcTransformBuffer->Name, SrcTransformBuffer->Desc.GetSize()),
		PassParameters,
		ERDGPassFlags::Copy,
		[PassParameters, &CopyCommands](FRDGAsyncTask, FRHICommandList& RHICmdList)
	{
		for (const FCopyCommand& Command : CopyCommands)
		{
			RHICmdList.CopyBufferRegion(PassParameters->DstBuffer->GetRHI(), Command.DstOffset, PassParameters->SrcBuffer->GetRHI(), Command.SrcOffset, Command.NumBytes);
		}
	});
}

} // Nanite

IMPLEMENT_SCENE_UB_STRUCT_EX(FNaniteSkinningParameters, NaniteSkinning, Nanite::GetDefaultSkinningParameters);
