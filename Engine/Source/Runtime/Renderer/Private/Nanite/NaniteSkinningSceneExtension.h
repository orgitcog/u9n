// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SpanAllocator.h"
#include "Containers/Map.h"
#include "Containers/Ticker.h"
#include "SceneExtensions.h"
#include "Skinning/SkinningTransformProvider.h"
#include "NaniteDefinitions.h"
#include "SkinningDefinitions.h"
#include "RendererPrivateUtils.h"
#include "InstanceCulling/InstanceCullingManager.h"
#include "Matrix3x4.h"
#include "Delegates/DelegateCombinations.h"
#include "Delegates/Delegate.h"

class FNaniteSkinningParameters;


// TODO: move to sensible place (or find existing)
inline uint32 PackNormToUintCeil(float Value, uint32 MaxBits)
{
	return FMath::CeilToInt(Value * float((1u << MaxBits) - 1u));
}

namespace Nanite
{

class FSkinnedSceneProxy;
class FSkinningSceneExtension : public ISceneExtension
{
	DECLARE_SCENE_EXTENSION(RENDERER_API, FSkinningSceneExtension);

public:
	class FUpdater : public ISceneExtensionUpdater
	{
		DECLARE_SCENE_EXTENSION_UPDATER(FUpdater, FSkinningSceneExtension);

	public:
		FUpdater(FSkinningSceneExtension& InSceneData);

		virtual void End();
		virtual void PreSceneUpdate(FRDGBuilder& GraphBuilder, const FScenePreUpdateChangeSet& ChangeSet, FSceneUniformBuffer& SceneUniforms) override;
		virtual void PostSceneUpdate(FRDGBuilder& GraphBuilder, const FScenePostUpdateChangeSet& ChangeSet) override;
		
		void PostMeshUpdate(FRDGBuilder& GraphBuilder, const TConstArrayView<FPrimitiveSceneInfo*>& SceneInfosWithStaticDrawListUpdate);

	private:
		FSkinningSceneExtension* SceneData = nullptr;
		TConstArrayView<FPrimitiveSceneInfo*> AddedList;
		TConstArrayView<FPrimitiveSceneInfo*> UpdateList;
		TArray<int32, FSceneRenderingArrayAllocator> DirtyPrimitiveList;
		const bool bEnableAsync = true;
		bool bForceFullUpload = false;
		bool bDefragging = false;
	};

	class FRenderer : public ISceneExtensionRenderer
	{
		DECLARE_SCENE_EXTENSION_RENDERER(FRenderer, FSkinningSceneExtension);
	
	public:
		FRenderer(FSceneRendererBase& InSceneRenderer, FSkinningSceneExtension& InSceneData) : ISceneExtensionRenderer(InSceneRenderer), SceneData(&InSceneData) {}

		virtual void UpdateViewData(FRDGBuilder& GraphBuilder, const FRendererViewDataManager& ViewDataManager) override;

		virtual void UpdateSceneUniformBuffer(FRDGBuilder& GraphBuilder, FSceneUniformBuffer& Buffer) override;

	private:
		FSkinningSceneExtension* SceneData = nullptr;
	};

	friend class FUpdater;

	static bool ShouldCreateExtension(FScene& InScene);

	explicit FSkinningSceneExtension(FScene& InScene);
	virtual ~FSkinningSceneExtension();

	virtual void InitExtension(FScene& InScene) override;

	virtual ISceneExtensionUpdater* CreateUpdater() override;
	virtual ISceneExtensionRenderer* CreateRenderer(FSceneRendererBase& InSceneRenderer, const FEngineShowFlags& EngineShowFlags) override;

	RENDERER_API void GetSkinnedPrimitives(TArray<FPrimitiveSceneInfo*>& OutPrimitives) const;

	RENDERER_API static const FSkinningTransformProvider::FProviderId& GetRefPoseProviderId();
	RENDERER_API static const FSkinningTransformProvider::FProviderId& GetAnimRuntimeProviderId();

private:
	enum ETask : uint32
	{
		FreeBufferSpaceTask,
		InitHeaderDataTask,
		AllocBufferSpaceTask,
		UploadHeaderDataTask,
		UploadHierarchyDataTask,
		UploadTransformDataTask,
		FillLoadBalancerDataTask,

		NumTasks
	};

	struct FHeaderData
	{
		FPrimitiveSceneInfo* PrimitiveSceneInfo	= nullptr;
		FGuid ProviderId;
		uint32 InstanceSceneDataOffset			= 0;
		uint32 NumInstanceSceneDataEntries		= 0;
		uint32 ObjectSpaceBufferOffset			= INDEX_NONE;
		uint32 ObjectSpaceBufferCount			= 0;
		uint32 HierarchyBufferOffset			= INDEX_NONE;
		uint32 HierarchyBufferCount				= 0;
		uint32 TransformBufferOffset			= INDEX_NONE;
		uint32 TransformBufferCount				= 0;
		float AnimationMinScreenSize				= -1.0f;
		uint16 MaxTransformCount				= 0;
		uint8  MaxInfluenceCount				= 0;
		uint8  UniqueAnimationCount				= 1;
		uint8  bHasScale : 1					= false;

		FNaniteSkinningHeader Pack() const
		{
			// Verify that the buffer offsets all fit within the encoded range prior to packing
			check(
				HierarchyBufferOffset	<= SKINNING_BUFFER_OFFSET_MAX &&
				TransformBufferOffset	<= SKINNING_BUFFER_OFFSET_MAX &&
				ObjectSpaceBufferOffset	<= SKINNING_BUFFER_OFFSET_MAX
			);

			FNaniteSkinningHeader Output;
			Output.HierarchyBufferOffset	= HierarchyBufferOffset;
			Output.TransformBufferOffset	= TransformBufferOffset;
			Output.ObjectSpaceBufferOffset	= ObjectSpaceBufferOffset;
			Output.MaxTransformCount		= MaxTransformCount;
			Output.MaxInfluenceCount		= MaxInfluenceCount;
			Output.UniqueAnimationCount		= UniqueAnimationCount;
			Output.bHasScale				= bHasScale;
			Output.bHasLODScreenSize		= AnimationMinScreenSize >= 0.0f;
			Output.AnimationMinScreenSize	= PackNormToUintCeil(FMath::Max(0.0f, AnimationMinScreenSize), SKINNING_LOD_SCREEN_SIZE_BITS);
			Output.Padding					= 0;
			return Output;
		}
	};

	class FBuffers
	{
	public:
		FBuffers();

		TPersistentByteAddressBuffer<FNaniteSkinningHeader> HeaderDataBuffer;
		TPersistentByteAddressBuffer<uint32> BoneHierarchyBuffer;
		TPersistentByteAddressBuffer<float> BoneObjectSpaceBuffer;
		TPersistentByteAddressBuffer<FCompressedBoneTransform> TransformDataBuffer;
	};
	
	class FUploader
	{
	public:
		TByteAddressBufferScatterUploader<FNaniteSkinningHeader> HeaderDataUploader;
		TByteAddressBufferScatterUploader<uint32> BoneHierarchyUploader;
		TByteAddressBufferScatterUploader<float> BoneObjectSpaceUploader;
		TByteAddressBufferScatterUploader<FCompressedBoneTransform> TransformDataUploader;
	};
	
	bool IsEnabled() const { return Buffers.IsValid(); }
	void SetEnabled(bool bEnabled);
	void SyncAllTasks() const { UE::Tasks::Wait(TaskHandles); }

	void FinishSkinningBufferUpload(
		FRDGBuilder& GraphBuilder,
		FNaniteSkinningParameters* OutParams = nullptr
	);

	void PerformSkinning(
		FNaniteSkinningParameters& Parameters,
		FRDGBuilder& GraphBuilder
	);

	bool ProcessBufferDefragmentation();

	UWorld* GetWorld() const;

	// Wait for tasks that modify HeaderData - after this the size and main fields do not change.
	void WaitForHeaderDataUpdateTasks() const;
private:
	FSpanAllocator ObjectSpaceAllocator;
	FSpanAllocator HierarchyAllocator;
	FSpanAllocator TransformAllocator;
	TSparseArray<FHeaderData> HeaderData;
	TUniquePtr<FBuffers> Buffers;
	TUniquePtr<FUploader> Uploader;
	TStaticArray<UE::Tasks::FTask, NumTasks> TaskHandles;

	struct
	{
		TInstanceCullingLoadBalancer<SceneRenderingAllocator>* LoadBalancer = nullptr;
		int32 NumReservedItems = 0;

		void AddReservedInstances(int32 NumInstances)
		{
			NumReservedItems += FMath::DivideAndRoundUp<int32>(NumInstances, TInstanceCullingLoadBalancer<SceneRenderingAllocator>::ThreadGroupSize) + 1;
		}

		void SubReservedInstances(int32 NumInstances)
		{
			NumReservedItems -= FMath::DivideAndRoundUp<int32>(NumInstances, TInstanceCullingLoadBalancer<SceneRenderingAllocator>::ThreadGroupSize) + 1;
		}

	} LoadBalancer;

	bool Tick(float DeltaTime);

	struct FTickState : public FRefCountBase
	{
		float DeltaTime = 0.0f;
		FVector CameraLocation = FVector::ZeroVector;
	};

	TRefCountPtr<FTickState> TickState{ new FTickState };
	FTSTicker::FDelegateHandle UpdateTimerHandle;

	TWeakObjectPtr<UWorld> WorldRef;

public:
	RENDERER_API static void ProvideRefPoseTransforms(FSkinningTransformProvider::FProviderContext& Context);
	RENDERER_API static void ProvideAnimRuntimeTransforms(FSkinningTransformProvider::FProviderContext& Context);
};

} // namespace Nanite
