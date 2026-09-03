// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/InstancedSkinnedMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "ComponentRecreateRenderStateContext.h"
#include "Elements/Framework/EngineElementsLibrary.h"
#include "Elements/Interfaces/TypedElementWorldInterface.h"
#include "Elements/SMInstance/SMInstanceElementData.h" // For SMInstanceElementDataUtil::SMInstanceElementsEnabled
#include "Elements/SMInstance/SMInstanceElementId.h"
#include "Engine/StaticMesh.h"
#include "HitProxies.h"
#include "NaniteSceneProxy.h"
#include "PrimitiveSceneInfo.h"
#include "Rendering/NaniteResourcesHelper.h"
#include "Rendering/RenderCommandPipes.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "SceneInterface.h"
#include "SkeletalRenderPublic.h"
#include "SkinningDefinitions.h"
#include "SkinnedMeshSceneProxyDesc.h"
#include "InstanceData/InstanceUpdateChangeSet.h"
#include "InstanceData/InstanceDataUpdateUtils.h"
#include "InstancedSkinnedMeshComponentHelper.h"
#include "PrimitiveSceneDesc.h"

#if WITH_EDITOR
#include "WorldPartition/HLOD/HLODHashBuilder.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(InstancedSkinnedMeshComponent)

static TAutoConsoleVariable<int32> CVarInstancedSkinnedMeshesForceRefPose(
	TEXT("r.InstancedSkinnedMeshes.ForceRefPose"),
	0,
	TEXT("Whether to force ref pose for instanced skinned meshes"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarInstancedSkinnedMeshesSampledBounds(
	TEXT("r.InstancedSkinnedMeshes.SampledBounds"),
	1,
	TEXT("Whether to use sampled bounds for anim bank meshes"),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		FGlobalComponentRecreateRenderStateContext Context;
	}),
	ECVF_RenderThreadSafe
);

class FInstancedSkinnedMeshSceneProxy : public Nanite::FSkinnedSceneProxy
{
public:
	using Super = Nanite::FSkinnedSceneProxy;

	FInstancedSkinnedMeshSceneProxy(const Nanite::FMaterialAudit& MaterialAudit, UInstancedSkinnedMeshComponent* InComponent, FSkeletalMeshRenderData* InRenderData)
		: FInstancedSkinnedMeshSceneProxy(MaterialAudit, FInstancedSkinnedMeshSceneProxyDesc(InComponent), InRenderData)
	{
	}

	FInstancedSkinnedMeshSceneProxy(const Nanite::FMaterialAudit& MaterialAudit, const FInstancedSkinnedMeshSceneProxyDesc& InMeshDesc, FSkeletalMeshRenderData* InRenderData)
		: Super(MaterialAudit, InMeshDesc, InRenderData, false /* bAllowScale */)
		, AnimationMinScreenSize(InMeshDesc.AnimationMinScreenSize)
	{
#if WITH_EDITOR
		const bool bSupportInstancePicking = HasPerInstanceHitProxies() && SMInstanceElementDataUtil::SMInstanceElementsEnabled();
		HitProxyMode = bSupportInstancePicking ? EHitProxyMode::PerInstance : EHitProxyMode::MaterialSection;

		if (HitProxyMode == EHitProxyMode::PerInstance)
		{
			bHasSelectedInstances = InMeshDesc.SelectedInstances.Find(true) != INDEX_NONE;
			if (bHasSelectedInstances)
			{
				// If we have selected indices, mark scene proxy as selected.
				SetSelection_GameThread(true);
			}
		}
#endif
		const bool bForceRefPose = CVarInstancedSkinnedMeshesForceRefPose.GetValueOnAnyThread() != 0;
		const bool bUseAnimBank = !bForceRefPose && InMeshDesc.AnimBankItems.Num() > 0;

		InstanceMinDrawDistance = InMeshDesc.InstanceMinDrawDistance;
		InstanceStartCullDistance = InMeshDesc.InstanceStartCullDistance;
		InstanceEndCullDistance = InMeshDesc.InstanceEndCullDistance;

		InstanceDataSceneProxy = InMeshDesc.InstanceDataSceneProxy;
		SetupInstanceSceneDataBuffers(InstanceDataSceneProxy->GeInstanceSceneDataBuffers());

		// Note: ideally this would be picked up from the Flags.bHasPerInstanceDynamicData above, but that path is not great at the moment.
		bAlwaysHasVelocity = true;

		// ISKM doesn't currently support skinning in ray tracing geometry
		bDynamicRayTracingGeometry = false;

		if (bUseAnimBank)
		{
			static FGuid AnimBankGPUProviderId(ANIM_BANK_GPU_TRANSFORM_PROVIDER_GUID);
			static FGuid AnimBankCPUProviderId(ANIM_BANK_CPU_TRANSFORM_PROVIDER_GUID);

			static const auto AnimBankGPUVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AnimBank.GPU"));
			if (AnimBankGPUVar && AnimBankGPUVar->GetValueOnAnyThread() == 1)
			{
				TransformProviderId = AnimBankGPUProviderId;
			}
			else
			{
				TransformProviderId = AnimBankCPUProviderId;
			}

			AnimBankItems = InMeshDesc.AnimBankItems;
			AnimBankIds.Reset(AnimBankItems.Num());
			AnimBankHandles.Reset(AnimBankItems.Num());
			UniqueAnimationCount = AnimBankItems.Num();
		}
		else
		{
			UniqueAnimationCount = 1; // Ref Pose
			static FGuid RefPoseProviderId(REF_POSE_TRANSFORM_PROVIDER_GUID);
			TransformProviderId = RefPoseProviderId;
		}
	}

	virtual void CreateRenderThreadResources(FRHICommandListBase& RHICmdList) override
	{
		Super::CreateRenderThreadResources(RHICmdList);

		TArray<FAnimBankDesc> Descs;
		Descs.Reserve(AnimBankItems.Num());

		for (const FAnimBankItem& Item : AnimBankItems)
		{
			if (Item.BankAsset == nullptr || SkinnedAsset == nullptr)
			{
				Descs.Emplace(FAnimBankDesc());
				continue;
			}

			const FAnimBankData& BankData = Item.BankAsset->GetData();

			if (Item.SequenceIndex >= BankData.Entries.Num())
			{
				Descs.Emplace(FAnimBankDesc());
				continue;
			}

			const FAnimBankEntry& BankEntry = BankData.Entries[Item.SequenceIndex];

			FAnimBankDesc& Desc = Descs.Emplace_GetRef();

			Desc.BankAsset		= MakeWeakObjectPtr(Item.BankAsset);
			Desc.SequenceIndex	= Item.SequenceIndex;
			Desc.Asset			= MakeWeakObjectPtr(SkinnedAsset);
			Desc.Position		= BankEntry.Position;
			Desc.PlayRate		= BankEntry.PlayRate;
			Desc.bLooping		= BankEntry.IsLooping()   ? 1 : 0;
			Desc.bAutoStart		= BankEntry.IsAutoStart() ? 1 : 0;
		}

		AnimBankHandles = GetScene().RegisterAnimBank(Descs);
		AnimBankIds.Reset(AnimBankHandles.Num());
		for (const FAnimBankRecordHandle& Handle : AnimBankHandles)
		{
			AnimBankIds.Emplace((uint64)Handle.Id);
		}
	}

	virtual void DestroyRenderThreadResources() override
	{
		GetScene().UnregisterAnimBank(AnimBankHandles);
		AnimBankHandles.Reset();
		AnimBankIds.Reset();
		Super::DestroyRenderThreadResources();
	}

	// FSkinnedSceneProxy interface
	virtual const TConstArrayView<uint64> GetAnimationProviderData(bool& bOutValid) const override
	{
		bOutValid = (AnimBankIds.Num() == UniqueAnimationCount);
		return AnimBankIds;
	}
	virtual float GetAnimationMinScreenSize() const  override { return AnimationMinScreenSize; }

	virtual bool GetInstanceDrawDistanceMinMax(FVector2f& OutCullRange) const override
	{
		if (InstanceEndCullDistance > 0)
		{
			OutCullRange = FVector2f(float(InstanceMinDrawDistance), float(InstanceEndCullDistance));
			return true;
		}
		else
		{
			OutCullRange = FVector2f(0.0f);
			return false;
		}
	}

	virtual void SetInstanceCullDistance_RenderThread(float StartCullDistance, float EndCullDistance) override
	{
		InstanceStartCullDistance = StartCullDistance;
		InstanceEndCullDistance = EndCullDistance;
	}

	virtual FInstanceDataUpdateTaskInfo *GetInstanceDataUpdateTaskInfo() const override
	{
		return InstanceDataSceneProxy ? InstanceDataSceneProxy->GetUpdateTaskInfo() : nullptr;
	}

public:
	// FPrimitiveSceneProxy interface.
	virtual SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

private:
	TSharedPtr<FInstanceDataSceneProxy, ESPMode::ThreadSafe> InstanceDataSceneProxy; 
	TArray<uint64> AnimBankIds;
	TArray<FAnimBankRecordHandle> AnimBankHandles;
	TArray<FAnimBankItem> AnimBankItems;
	uint64 BankRegistration = 0;
	float AnimationMinScreenSize = 0.0f;
	uint32 InstanceMinDrawDistance = 0;
	uint32 InstanceStartCullDistance = 0;
	uint32 InstanceEndCullDistance = 0;
};

/** 
* Stores the updated matrices needed to skin the verts.
* Created by the game thread and sent to the rendering thread as an update 
*/
class FInstancedSkinnedMeshData
{
public:
	FInstancedSkinnedMeshData(
		const USkinnedAsset* InSkinnedAsset,
		TConstArrayView<FAnimBankItem> AnimBankItems,
		int32 InLODIndex,
		EPreviousBoneTransformUpdateMode InPreviousBoneTransformUpdateMode
	);

	virtual ~FInstancedSkinnedMeshData();

	// Current reference pose to local space transforms
	TArray<FMatrix44f> ReferenceToLocal;

	// Previous reference pose to local space transforms
	TArray<FMatrix44f> PrevReferenceToLocal;

	TArray<FMatrix3x4> CurrentBoneTransforms;
	TArray<FMatrix3x4> PreviousBoneTransforms;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) 
	// Component space bone transforms
	TArray<FTransform> ComponentSpaceTransforms;
#endif

	// Current LOD for bones being updated
	int32 LODIndex;

	// Returns the size of memory allocated by render data
	void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize);

private:

	enum class ETransformsToUpdate
	{
		Current,
		Previous,
	};
};

FInstancedSkinnedMeshData::FInstancedSkinnedMeshData(
	const USkinnedAsset* InSkinnedAsset,
	TConstArrayView<FAnimBankItem> AnimBankItems,
	int32 InLODIndex,
	EPreviousBoneTransformUpdateMode InPreviousBoneTransformUpdateMode
)
:	LODIndex(InLODIndex)
{
	int32 BoneCount = InSkinnedAsset->GetSkeleton() ? InSkinnedAsset->GetSkeleton()->GetReferenceSkeleton().GetRawBoneNum() : 0;
	ReferenceToLocal.SetNumUninitialized(BoneCount);
	for (int32 TransformIndex = 0; TransformIndex < BoneCount; ++TransformIndex)
	{
		ReferenceToLocal[TransformIndex].SetIdentity();
	}

	const int64 ReferenceToLocalCount = int64(ReferenceToLocal.Num());
	const FMatrix44f* ReferenceToLocalPtr = ReferenceToLocal.GetData();

	const int32 UniqueAnimationCount = FMath::Max(AnimBankItems.Num(), 1 /* Ref Pose */);

	CurrentBoneTransforms.SetNumUninitialized(BoneCount * UniqueAnimationCount);
	FMatrix3x4* CurrentBoneTransformsPtr = CurrentBoneTransforms.GetData();
	TransposeTransforms(CurrentBoneTransformsPtr, ReferenceToLocalPtr, ReferenceToLocalCount);

	// TODO: Temp, optimize out
	for (int32 AnimationIndex = 1; AnimationIndex < UniqueAnimationCount; ++AnimationIndex)
	{
		FMemory::Memcpy(CurrentBoneTransformsPtr + (BoneCount * AnimationIndex), CurrentBoneTransformsPtr, sizeof(FMatrix3x4) * BoneCount);
	}

	PrevReferenceToLocal = ReferenceToLocal;
	PreviousBoneTransforms = CurrentBoneTransforms;

#if 0
	UpdateRefToLocalMatrices(ReferenceToLocal, InComponent, InRenderData, LODIndex);
	UpdateBonesRemovedByLOD(ReferenceToLocal, InComponent, ETransformsToUpdate::Current);

	switch (InPreviousBoneTransformUpdateMode)
	{
	case EPreviousBoneTransformUpdateMode::None:
		// Use previously uploaded buffer
		// TODO: Nanite-Skinning, optimize scene extension upload to keep cached GPU representation using PreviousBoneTransformRevisionNumber
		// For now we'll just redundantly update and upload previous transforms
		UpdatePreviousRefToLocalMatrices(PrevReferenceToLocal, InComponent, InRenderData, LODIndex);
		UpdateBonesRemovedByLOD(PrevReferenceToLocal, InComponent, ETransformsToUpdate::Previous);
		break;

	case EPreviousBoneTransformUpdateMode::UpdatePrevious:
		UpdatePreviousRefToLocalMatrices(PrevReferenceToLocal, InComponent, InRenderData, LODIndex);
		UpdateBonesRemovedByLOD(PrevReferenceToLocal, InComponent, ETransformsToUpdate::Previous);
		break;

	case EPreviousBoneTransformUpdateMode::DuplicateCurrentToPrevious:
		// TODO: Nanite-Skinning likely possible we can just return ReferenceToLocal here rather than cloning it into previous
		// Need to make sure it's safe when next update mode = None
		PrevReferenceToLocal = ReferenceToLocal;
		break;
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	ComponentSpaceTransforms = InComponent->GetComponentSpaceTransforms();
#endif
#endif
}

FInstancedSkinnedMeshData::~FInstancedSkinnedMeshData() = default;

void FInstancedSkinnedMeshData::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(sizeof(*this));
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(ReferenceToLocal.GetAllocatedSize());
}

class FInstancedSkinnedMeshObject : public FSkeletalMeshObject
{
public:
	FInstancedSkinnedMeshObject(const FSkinnedMeshSceneProxyDesc& InMeshDesc, TConstArrayView<FAnimBankItem> InAnimBankItems, FSkeletalMeshRenderData* InRenderData, ERHIFeatureLevel::Type InFeatureLevel)
		: FSkeletalMeshObject(InMeshDesc, InRenderData, InFeatureLevel)
		, AnimBankItems(InAnimBankItems)
	{
		for (int32 LODIndex = 0; LODIndex < InRenderData->LODRenderData.Num(); ++LODIndex)
		{
			LODs.Emplace(InFeatureLevel, InRenderData, LODIndex);
		}

		InitResources(InMeshDesc);
	}

	FInstancedSkinnedMeshObject(USkinnedMeshComponent* InComponent, FSkeletalMeshRenderData* InRenderData, ERHIFeatureLevel::Type InFeatureLevel)
	: FInstancedSkinnedMeshObject(FSkinnedMeshSceneProxyDesc(InComponent), CastChecked<UInstancedSkinnedMeshComponent>(InComponent)->AnimBankItems, InRenderData, InFeatureLevel)
	{
	}

	virtual ~FInstancedSkinnedMeshObject()
	{
	}

	virtual void InitResources(const FSkinnedMeshSceneProxyDesc& InMeshDesc) override
	{
		for (int32 LODIndex = 0; LODIndex < LODs.Num(); ++LODIndex)
		{
			FInstancedSkinnedMeshObjectLOD& LOD = LODs[LODIndex];

			// Skip LODs that have their render data stripped
			if (LOD.RenderData->LODRenderData[LODIndex].GetNumVertices() > 0)
			{
				const FSkelMeshComponentLODInfo* InitLODInfo = nullptr;
				if (InMeshDesc.LODInfo.IsValidIndex(LODIndex))
				{
					InitLODInfo = &InMeshDesc.LODInfo[LODIndex];
				}

				LOD.InitResources(InitLODInfo);
			}
		}
	}

	virtual void ReleaseResources() override
	{
		for (int32 LODIndex = 0; LODIndex < LODs.Num(); ++LODIndex)
		{
			FInstancedSkinnedMeshObjectLOD& LOD = LODs[LODIndex];
			LOD.ReleaseResources();
		}
	}
	
	virtual void Update(
		int32 LODIndex,
		const FSkinnedMeshSceneProxyDynamicData& InDynamicData,
		const FPrimitiveSceneProxy* InSceneProxy,
		const USkinnedAsset* InSkinnedAsset,
		const FMorphTargetWeightMap& InActiveMorphTargets,
		const TArray<float>& MorphTargetWeights,
		EPreviousBoneTransformUpdateMode PreviousBoneTransformUpdateMode,
		const FExternalMorphWeightData& InExternalMorphWeightData) override
	{
		// Create the new dynamic data for use by the rendering thread
		// this data is only deleted when another update is sent
		TUniquePtr<FInstancedSkinnedMeshData> NewDynamicData = MakeUnique<FInstancedSkinnedMeshData>(InSkinnedAsset, AnimBankItems, LODIndex, PreviousBoneTransformUpdateMode);

		uint64 FrameNumberToPrepare = GFrameCounter;
		uint32 RevisionNumber = 0;

		if (InSceneProxy)
		{
			RevisionNumber = InDynamicData.GetBoneTransformRevisionNumber();
		}

		// Queue a call to update this data
		{
			FInstancedSkinnedMeshObject* MeshObject = this;
			ENQUEUE_RENDER_COMMAND(SkelMeshObjectUpdateDataCommand)(UE::RenderCommandPipe::SkeletalMesh,
				[MeshObject, FrameNumberToPrepare, RevisionNumber, NewDynamicData=MoveTemp(NewDynamicData)](FRHICommandList& RHICmdList) mutable
				{
					FScopeCycleCounter Context(MeshObject->GetStatId());
					MeshObject->UpdateDynamicData_RenderThread(RHICmdList, MoveTemp(NewDynamicData), FrameNumberToPrepare, RevisionNumber);
				}
			);
		}
	}

	void UpdateDynamicData_RenderThread(
		FRHICommandList& RHICmdList,
		TUniquePtr<FInstancedSkinnedMeshData>&& InDynamicData,
		uint64 FrameNumberToPrepare,
		uint32 RevisionNumber
	)
	{
		// Update with new data
		DynamicData = MoveTemp(InDynamicData);
		check(DynamicData);

		check(IsInParallelRenderingThread());
	}

	virtual const FVertexFactory* GetSkinVertexFactory(const FSceneView* View, int32 LODIndex, int32 ChunkIdx, ESkinVertexFactoryMode VFMode = ESkinVertexFactoryMode::Default) const override
	{
		check(LODs.IsValidIndex(LODIndex));
		// TODO: Support skinning in ray tracing (currently representing with static geometry)
		return nullptr;
	}

	virtual const FVertexFactory* GetStaticSkinVertexFactory(int32 LODIndex, int32 ChunkIdx, ESkinVertexFactoryMode VFMode = ESkinVertexFactoryMode::Default) const override
	{
		check(LODs.IsValidIndex(LODIndex));
		return &LODs[LODIndex].VertexFactory;
	}

	virtual TArray<FTransform>* GetComponentSpaceTransforms() const override
	{
	#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (DynamicData)
		{
			return &(DynamicData->ComponentSpaceTransforms);
		}
		else
	#endif
		{
			return nullptr;
		}
	}

	virtual const TArray<FMatrix44f>& GetReferenceToLocalMatrices() const override
	{
		return DynamicData->ReferenceToLocal;
	}

	virtual const TArray<FMatrix44f>& GetPrevReferenceToLocalMatrices() const override
	{
		return DynamicData->PrevReferenceToLocal;
	}

	const TArray<FMatrix3x4>* GetCurrentBoneTransforms() const override
	{
		return &DynamicData->CurrentBoneTransforms;
	}

	const TArray<FMatrix3x4>* GetPreviousBoneTransforms() const override
	{
		return &DynamicData->PreviousBoneTransforms;
	}

	virtual int32 GetLOD() const override
	{
		// WorkingMinDesiredLODLevel can be a LOD that's not loaded, so need to clamp it to the first loaded LOD
		return 0;
		//return FMath::Max<int32>(WorkingMinDesiredLODLevel, SkeletalMeshRenderData->CurrentFirstLODIdx);
		/*if (DynamicData)
		{
			return DynamicData->LODIndex;
		}
		else
		{
			return 0;
		}*/
	}

	virtual bool HaveValidDynamicData() const override
	{
		return (DynamicData != nullptr);
	}

	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override
	{
		CumulativeResourceSize.AddDedicatedSystemMemoryBytes(sizeof(*this));

		if (DynamicData)
		{
			DynamicData->GetResourceSizeEx(CumulativeResourceSize);
		}

		CumulativeResourceSize.AddDedicatedSystemMemoryBytes(LODs.GetAllocatedSize());

		for (int32 Index = 0; Index < LODs.Num(); ++Index)
		{
			LODs[Index].GetResourceSizeEx(CumulativeResourceSize);
		}
	}

	virtual void UpdateSkinWeightBuffer(const TArrayView<const FSkelMeshComponentLODInfo> InLODInfo) override
	{
		for (int32 LODIndex = 0; LODIndex < LODs.Num(); ++LODIndex)
		{
			FInstancedSkinnedMeshObjectLOD& LOD = LODs[LODIndex];

			// Skip LODs that have their render data stripped
			if (LOD.RenderData->LODRenderData[LODIndex].GetNumVertices() > 0)
			{
				const FSkelMeshComponentLODInfo* UpdateLODInfo = nullptr;
				if (InLODInfo.IsValidIndex(LODIndex))
				{
					UpdateLODInfo = &InLODInfo[LODIndex];
				}

				LOD.UpdateSkinWeights(UpdateLODInfo);
			}
		}
	}

	virtual bool IsNaniteMesh() const override { return true; }

#if RHI_RAYTRACING
	// TODO: Support skinning in ray tracing (currently representing with static geometry)
	virtual const FRayTracingGeometry* GetStaticRayTracingGeometry() const override
	{
		const int32 RayTracingLODIndex = GetRayTracingLOD();
		return &LODs[RayTracingLODIndex].RenderData->LODRenderData[RayTracingLODIndex].StaticRayTracingGeometry;
	}
#endif

private:
	TUniquePtr<FInstancedSkinnedMeshData> DynamicData;

	struct FInstancedSkinnedMeshObjectLOD
	{
		FSkeletalMeshRenderData* RenderData;
		FLocalVertexFactory	VertexFactory;
		int32 LODIndex;
		bool bInitialized;

		/**
		* Whether InitStaticRayTracingGeometry(...) was called during initialization,
		* so ReleaseStaticRayTracingGeometry(...) must be called when releasing resources.
		*/
		bool bStaticRayTracingGeometryInitialized;

		FInstancedSkinnedMeshObjectLOD(ERHIFeatureLevel::Type InFeatureLevel, FSkeletalMeshRenderData* InRenderData, int32 InLOD)
		: RenderData(InRenderData)
		, VertexFactory(InFeatureLevel, "FInstancedSkinnedMeshObjectLOD")
		, LODIndex(InLOD)
		, bInitialized(false)
		, bStaticRayTracingGeometryInitialized(false)
		{
		}

		void InitResources(const FSkelMeshComponentLODInfo* InLODInfo)
		{
			check(RenderData);
			check(RenderData->LODRenderData.IsValidIndex(LODIndex));

			FSkeletalMeshLODRenderData& LODData = RenderData->LODRenderData[LODIndex];

		#if RHI_RAYTRACING
			if (IsRayTracingEnabled() && RenderData->bSupportRayTracing)
			{
				// TODO: Support skinning in ray tracing (currently representing with static geometry)
				RenderData->InitStaticRayTracingGeometry(LODIndex);

				bStaticRayTracingGeometryInitialized = true;

				FLocalVertexFactory* VertexFactoryPtr = &VertexFactory;
				FPositionVertexBuffer* PositionVertexBufferPtr = &LODData.StaticVertexBuffers.PositionVertexBuffer;
				FStaticMeshVertexBuffer* StaticMeshVertexBufferPtr = &LODData.StaticVertexBuffers.StaticMeshVertexBuffer;

				ENQUEUE_RENDER_COMMAND(InitSkeletalMeshStaticSkinVertexFactory)(UE::RenderCommandPipe::SkeletalMesh,
					[VertexFactoryPtr, PositionVertexBufferPtr, StaticMeshVertexBufferPtr](FRHICommandList& RHICmdList)
					{
						FLocalVertexFactory::FDataType Data;
						PositionVertexBufferPtr->InitResource(RHICmdList);
						StaticMeshVertexBufferPtr->InitResource(RHICmdList);

						PositionVertexBufferPtr->BindPositionVertexBuffer(VertexFactoryPtr, Data);
						StaticMeshVertexBufferPtr->BindTangentVertexBuffer(VertexFactoryPtr, Data);
						StaticMeshVertexBufferPtr->BindPackedTexCoordVertexBuffer(VertexFactoryPtr, Data);
						StaticMeshVertexBufferPtr->BindLightMapVertexBuffer(VertexFactoryPtr, Data, 0);

						VertexFactoryPtr->SetData(RHICmdList, Data);
						VertexFactoryPtr->InitResource(RHICmdList);
					});
			}
		#endif

			bInitialized = true;
		}

		void ReleaseResources()
		{
			check(RenderData);

			bInitialized = false;

			BeginReleaseResource(&VertexFactory, &UE::RenderCommandPipe::SkeletalMesh);

#if RHI_RAYTRACING
			if (bStaticRayTracingGeometryInitialized)
			{
				RenderData->ReleaseStaticRayTracingGeometry(LODIndex);
			}
#endif
		}

		void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
		{
		}

		void UpdateSkinWeights(const FSkelMeshComponentLODInfo* InLODInfo)
		{
			check(RenderData);
			check(RenderData->LODRenderData.IsValidIndex(LODIndex));

			//FSkeletalMeshLODRenderData& LODData = RenderData->LODRenderData[LODIndex];
			//MeshObjectWeightBuffer = FSkeletalMeshObject::GetSkinWeightVertexBuffer(LODData, InLODInfo);
		}
	};

	TConstArrayView<FAnimBankItem> AnimBankItems;
	TArray<FInstancedSkinnedMeshObjectLOD> LODs;

	mutable int32 CachedLOD;
};

static FSkeletalMeshObject* CreateInstancedSkinnedMeshObjectFn(void* UserData, USkinnedMeshComponent* InComponent, FSkeletalMeshRenderData* InRenderData, ERHIFeatureLevel::Type InFeatureLevel)
{
	if (InComponent->ShouldNaniteSkin())
	{
		return ::new FInstancedSkinnedMeshObject(InComponent, InRenderData, InFeatureLevel);
	}

	return nullptr;
}

UInstancedSkinnedMeshComponent::UInstancedSkinnedMeshComponent(FVTableHelper& Helper)
: Super(Helper)
, bInheritPerInstanceData(false)
, InstanceDataManager(this)
{
}

UInstancedSkinnedMeshComponent::UInstancedSkinnedMeshComponent(const FObjectInitializer& ObjectInitializer) 
: Super(ObjectInitializer) 
, bInheritPerInstanceData(false)
, InstanceDataManager(this)
{
}

UInstancedSkinnedMeshComponent::~UInstancedSkinnedMeshComponent()
{
}

bool UInstancedSkinnedMeshComponent::ShouldForceRefPose()
{ 
	return CVarInstancedSkinnedMeshesForceRefPose.GetValueOnAnyThread() != 0;
}

bool UInstancedSkinnedMeshComponent::ShouldUseSampledBounds()
{
	return CVarInstancedSkinnedMeshesSampledBounds.GetValueOnAnyThread() != 0;
}

struct FSkinnedMeshInstanceData_Deprecated
{
	FMatrix Transform;
	uint32 BankIndex;
	uint32 Padding[3]; // Need to respect 16 byte alignment for bulk-serialization

	FSkinnedMeshInstanceData_Deprecated()
	: Transform(FMatrix::Identity)
	, BankIndex(0)
	{
		Padding[0] = 0;
		Padding[1] = 0;
		Padding[2] = 0;
	}

	FSkinnedMeshInstanceData_Deprecated(const FMatrix& InTransform, uint32 InBankIndex)
	: Transform(InTransform)
	, BankIndex(InBankIndex)
	{
		Padding[0] = 0;
		Padding[1] = 0;
		Padding[2] = 0;
	}

	friend FArchive& operator<<(FArchive& Ar, FSkinnedMeshInstanceData_Deprecated& InstanceData)
	{
		// @warning BulkSerialize: FSkinnedMeshInstanceData is serialized as memory dump
		// See TArray::BulkSerialize for detailed description of implied limitations.
		Ar << InstanceData.Transform;
		Ar << InstanceData.BankIndex;
		Ar << InstanceData.Padding[0];
		Ar << InstanceData.Padding[1];
		Ar << InstanceData.Padding[2];
		return Ar;
	}
};

void UInstancedSkinnedMeshComponent::Serialize(FArchive& Ar)
{
	LLM_SCOPE(ELLMTag::SkeletalMesh);
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FUE5MainStreamObjectVersion::GUID);

	bool bCooked = Ar.IsCooking();
	Ar << bCooked;

	// Inherit properties when bEditableWhenInherited == false || bInheritPerInstanceData == true (when the component isn't a template and we are persisting data)
	const UInstancedSkinnedMeshComponent* Archetype = Cast<UInstancedSkinnedMeshComponent>(GetArchetype());
	const bool bInheritSkipSerializationProperties = ShouldInheritPerInstanceData(Archetype) && Ar.IsPersistent();
	
	// Check if we need have SkipSerialization property data to load/save
	bool bHasSkipSerializationPropertiesData = !bInheritSkipSerializationProperties;
	Ar << bHasSkipSerializationPropertiesData;

	if (Ar.IsLoading())
	{
		// Read existing data if it was serialized
		TArray<FSkinnedMeshInstanceData> TempInstanceData;
		TArray<float> TempInstanceCustomData;

		if (bHasSkipSerializationPropertiesData)
		{
			if (Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) < FUE5MainStreamObjectVersion::SkinnedMeshInstanceDataSerializationV2)
			{
				TArray<FSkinnedMeshInstanceData_Deprecated> TempInstanceData_Deprecated;
				TempInstanceData_Deprecated.BulkSerialize(Ar, false /* force per element serialization */);

				TempInstanceData.Reserve(TempInstanceData_Deprecated.Num());
				for (const auto& Item : TempInstanceData_Deprecated)
				{
					TempInstanceData.Emplace(FTransform3f(FMatrix44f(Item.Transform)), Item.BankIndex);
				}
			}
			else
			{
				Ar << TempInstanceData;
			}
			TempInstanceCustomData.BulkSerialize(Ar);
		}

		// If we should inherit use Archetype Data
		if (bInheritSkipSerializationProperties)
		{
			ApplyInheritedPerInstanceData(Archetype);
		} 
		// It is possible for a component to lose its BP archetype between a save / load so in this case we have no per instance data (usually this component gets deleted through construction script)
		else if (bHasSkipSerializationPropertiesData)
		{
			InstanceData = MoveTemp(TempInstanceData);
			InstanceCustomData = MoveTemp(TempInstanceCustomData);
		}
	}
	else if (bHasSkipSerializationPropertiesData)
	{
		Ar << InstanceData;
		InstanceCustomData.BulkSerialize(Ar);
	}

#if WITH_EDITOR
	if (Ar.IsTransacting())
	{
		Ar << SelectedInstances;
	}
#endif

	if (Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) >= FUE5MainStreamObjectVersion::SkinnedMeshInstanceDataSerializationV2)
	{
		InstanceDataManager.Serialize(Ar, bCooked);
	}
	else if (Ar.IsLoading())
	{
		// Prior to this the id mapping was not saved so we need to reset it.
		InstanceDataManager.Reset(InstanceData.Num());
	}

	if (bCooked)
	{
		if (Ar.IsLoading())
		{
			InstanceDataManager.ReadCookedRenderData(Ar);
		}
#if WITH_EDITOR
		else if (Ar.IsSaving())
		{
			InstanceDataManager.WriteCookedRenderData(Ar, GetComponentDesc(GMaxRHIFeatureLevel));
		}
#endif
	}
}

void UInstancedSkinnedMeshComponent::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	if (GIsEditor)
	{
		SetSkinnedAssetCallback();
	}
#endif
}

void UInstancedSkinnedMeshComponent::BeginDestroy()
{
	Super::BeginDestroy();
}

void UInstancedSkinnedMeshComponent::OnRegister()
{
	Super::OnRegister();
}

void UInstancedSkinnedMeshComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

bool UInstancedSkinnedMeshComponent::IsEnabled() const
{
	return FInstancedSkinnedMeshComponentHelper::IsEnabled(*this);
}

int32 UInstancedSkinnedMeshComponent::GetInstanceCount() const
{
	return bIsInstanceDataGPUOnly ? NumInstancesGPUOnly : InstanceData.Num();
}

TArrayView<const struct FAnimBankItem> UInstancedSkinnedMeshComponent::GetAnimBankItems() const
{
	return MakeArrayView(AnimBankItems);
}

void UInstancedSkinnedMeshComponent::SetAnimBankItems(TArrayView<const struct FAnimBankItem> InAnimBankItems)
{
	AnimBankItems = InAnimBankItems;
	// We use the transform dirty state to drive the update of the animation data (to defer the need to add more bits), so we mark those as dirty here.
	InstanceDataManager.TransformsChangedAll();
	MarkRenderStateDirty();
}

template <typename ArrayType>
inline void ReorderArray(ArrayType& InOutDataArray, const TArray<int32>& OldIndexArray, int32 ElementStride = 1)
{
	// TODO: this fails for FSkinnedMeshInstanceData, it is not really a problem here because the ctor just performs initializaiton and we'll overwrite it anyway 
	// static_assert(std::is_trivially_constructible_v<decltype(InOutDataArray[0])>, "Must have trivially constructible type to safely call SetNumUninitialized below");
	ArrayType TmpDataArray = MoveTemp(InOutDataArray);
	InOutDataArray.Empty(TmpDataArray.Num());
	for (int32 NewIndex = 0; NewIndex < TmpDataArray.Num(); ++NewIndex)
	{
		int32 OldIndex = OldIndexArray[NewIndex];
		for (int32 SubIndex = 0; SubIndex < ElementStride; ++SubIndex)
		{
			InOutDataArray.Add(TmpDataArray[OldIndex * ElementStride + SubIndex]);
		}
	}
}

void UInstancedSkinnedMeshComponent::OptimizeInstanceData(bool bShouldRetainIdMap)
{
	// compute the optimal order 
	TArray<int32> IndexRemap = InstanceDataManager.Optimize(GetComponentDesc(GMaxRHIFeatureLevel), bShouldRetainIdMap);
	
	if (!IndexRemap.IsEmpty())
	{
		// Reorder instances according to the remap
		ReorderArray(InstanceData, IndexRemap);
		ReorderArray(InstanceCustomData, IndexRemap, NumCustomDataFloats);
#if WITH_EDITOR
		ReorderArray(SelectedInstances, IndexRemap);
#endif
	}
}

void UInstancedSkinnedMeshComponent::ApplyInheritedPerInstanceData(const UInstancedSkinnedMeshComponent* InArchetype)
{
	check(InArchetype);
	InstanceData = InArchetype->InstanceData;
	InstanceCustomData = InArchetype->InstanceCustomData;
	NumCustomDataFloats = InArchetype->NumCustomDataFloats;
}

bool UInstancedSkinnedMeshComponent::ShouldInheritPerInstanceData() const
{
	return ShouldInheritPerInstanceData(Cast<UInstancedSkinnedMeshComponent>(GetArchetype()));
}

bool UInstancedSkinnedMeshComponent::ShouldInheritPerInstanceData(const UInstancedSkinnedMeshComponent* InArchetype) const
{
	return (bInheritPerInstanceData || !bEditableWhenInherited) && InArchetype && InArchetype->IsInBlueprint() && !IsTemplate();
}

void UInstancedSkinnedMeshComponent::SetInstanceDataGPUOnly(bool bInInstancesGPUOnly)
{
	if (bIsInstanceDataGPUOnly != bInInstancesGPUOnly)
	{
		bIsInstanceDataGPUOnly = bInInstancesGPUOnly;

		if (bIsInstanceDataGPUOnly)
		{
			ClearInstances();
		}
	}
}

void UInstancedSkinnedMeshComponent::SetupNewInstanceData(FSkinnedMeshInstanceData& InOutNewInstanceData, int32 InInstanceIndex, const FTransform3f& InInstanceTransform, int32 InBankIndex)
{
	InOutNewInstanceData.Transform = InInstanceTransform;
	InOutNewInstanceData.BankIndex = InBankIndex;

	if (bPhysicsStateCreated)
	{
		// ..
	}
}

const Nanite::FResources* UInstancedSkinnedMeshComponent::GetNaniteResources() const
{
	return Super::GetNaniteResources();
}

#if WITH_EDITOR

void UInstancedSkinnedMeshComponent::PostAssetCompilation()
{
	InstanceDataManager.ClearChangeTracking();
	MarkRenderStateDirty();
}

#endif 

void UInstancedSkinnedMeshComponent::BuildSceneDesc(FPrimitiveSceneProxyDesc* InSceneProxyDesc, FPrimitiveSceneDesc& OutPrimitiveSceneDesc) // TODO try add FPrimitiveSceneDesc::InitFromPrimitiveComponent
{
	check(InSceneProxyDesc);

	OutPrimitiveSceneDesc.SceneProxy = GetSceneProxy();
	OutPrimitiveSceneDesc.ProxyDesc = InSceneProxyDesc;
	OutPrimitiveSceneDesc.PrimitiveSceneData = &GetSceneData();
	OutPrimitiveSceneDesc.RenderMatrix = GetRenderMatrix();
	OutPrimitiveSceneDesc.AttachmentRootPosition = GetComponentLocation();
	OutPrimitiveSceneDesc.LocalBounds = CalcBounds(FTransform::Identity);
	OutPrimitiveSceneDesc.Bounds = CalcBounds(GetComponentToWorld());
	OutPrimitiveSceneDesc.Mobility = InSceneProxyDesc->Mobility;
}

FInstanceDataManagerSourceDataDesc UInstancedSkinnedMeshComponent::GetComponentDesc(ERHIFeatureLevel::Type FeatureLevel)
{
	FInstanceDataManagerSourceDataDesc ComponentDesc;


	ComponentDesc.PrimitiveMaterialDesc = GetUsedMaterialPropertyDesc(FeatureLevel);

	FInstanceDataFlags Flags;
	Flags.bHasPerInstanceRandom = ComponentDesc.PrimitiveMaterialDesc.bAnyMaterialHasPerInstanceRandom;
	Flags.bHasPerInstanceCustomData = ComponentDesc.PrimitiveMaterialDesc.bAnyMaterialHasPerInstanceCustomData && NumCustomDataFloats != 0;
#if WITH_EDITOR
	Flags.bHasPerInstanceEditorData = GIsEditor != 0 && bHasPerInstanceHitProxies;
#endif
	
	const bool bForceRefPose = CVarInstancedSkinnedMeshesForceRefPose.GetValueOnAnyThread() != 0;
	const bool bUseAnimBank = !bForceRefPose && AnimBankItems.Num() > 0;

	Flags.bHasPerInstanceHierarchyOffset = false;
	Flags.bHasPerInstanceLocalBounds = bUseAnimBank && AnimBankItems.Num() > 1;
	Flags.bHasPerInstanceDynamicData = false;
	Flags.bHasPerInstanceSkinningData = true;


	Flags.bHasPerInstanceLMSMUVBias = false;//IsStaticLightingAllowed();

	ComponentDesc.Flags = Flags;
	
	// TODO: rename
	ComponentDesc.MeshBounds = GetSkinnedAsset()->GetBounds();
	ComponentDesc.NumCustomDataFloats = NumCustomDataFloats;
	ComponentDesc.NumInstances = InstanceData.Num();

	ComponentDesc.PrimitiveLocalToWorld = GetRenderMatrix();
	ComponentDesc.ComponentMobility = Mobility;

	const FReferenceSkeleton& RefSkeleton = GetSkinnedAsset()->GetRefSkeleton();
	uint32 MaxBoneTransformCount = RefSkeleton.GetRawBoneNum();

	ComponentDesc.BuildChangeSet = [&, MaxBoneTransformCount, MeshBounds = ComponentDesc.MeshBounds](FInstanceUpdateChangeSet& ChangeSet)
	{
		// publish data
		ChangeSet.GetTransformWriter().Gather([&](int32 InstanceIndex) -> FRenderTransform { return FRenderTransform(InstanceData[InstanceIndex].Transform.ToMatrixWithScale()); });
		ChangeSet.GetCustomDataWriter().Gather(MakeArrayView(InstanceCustomData), NumCustomDataFloats);

		ChangeSet.GetSkinningDataWriter().Gather(
			[&](int32 InstanceIndex) -> uint32 
			{ 
				return InstanceData[InstanceIndex].BankIndex * MaxBoneTransformCount * 2u; 
			});

		ChangeSet.GetLocalBoundsWriter().Gather(
			[&](int32 InstanceIndex) -> FRenderBounds 
			{				
				uint32 BankIndex = InstanceData[InstanceIndex].BankIndex;
				if (BankIndex < uint32(AnimBankItems.Num()))
				{
					const FAnimBankItem& BankItem = AnimBankItems[BankIndex];
					if (BankItem.BankAsset != nullptr)
					{
						const FAnimBankData& BankData = BankItem.BankAsset->GetData();
						if (BankItem.SequenceIndex < BankData.Entries.Num())
						{
							return BankData.Entries[BankItem.SequenceIndex].SampledBounds;
						}
					}
				}
				return MeshBounds; 
			});

#if WITH_EDITOR
	if (ChangeSet.Flags.bHasPerInstanceEditorData)
	{
		// TODO: the way hit proxies are managed seems daft, why don't we just add them when needed and store them in an array alonside the instances?
		//       this will always force us to update all the hit proxy data for every instances.
		TArray<TRefCountPtr<HHitProxy>> HitProxies;
		CreateHitProxyData(HitProxies);
		ChangeSet.SetEditorData(HitProxies, SelectedInstances);
	}
#endif


	};

	return ComponentDesc;
}

void UInstancedSkinnedMeshComponent::SendRenderInstanceData_Concurrent()
{
	Super::SendRenderInstanceData_Concurrent();

	// If instance data is entirely GPU driven, don't upload from CPU.
	if (bIsInstanceDataGPUOnly)
	{
		return;
	}

	// If the primitive isn't hidden update its instances.
	const bool bDetailModeAllowsRendering = true;//DetailMode <= GetCachedScalabilityCVars().DetailMode;
	// The proxy may not be created, this can happen when a SM is async loading for example.
	if (bDetailModeAllowsRendering && (ShouldRender() || bCastHiddenShadow || bAffectIndirectLightingWhileHidden || bRayTracingFarField))
	{
		if (SceneProxy != nullptr)
		{
			// Make sure the instance data proxy is up to date:
			if (InstanceDataManager.FlushChanges(GetComponentDesc(SceneProxy->GetScene().GetFeatureLevel())))
			{
				UpdateBounds();
				GetWorld()->Scene->UpdatePrimitiveInstances(this);
			}
		}
		else
		{
			UpdateBounds();
			GetWorld()->Scene->AddPrimitive(this);
		}
	}
}

bool UInstancedSkinnedMeshComponent::IsHLODRelevant() const
{
	if (!CanBeHLODRelevant(this))
	{
		return false;
	}

	if (!GetSkinnedAsset())
	{
		return false;
	}

	if (!IsVisible())
	{
		return false;
	}

	if (Mobility == EComponentMobility::Movable)
	{
		return false;
	}

#if WITH_EDITORONLY_DATA
	if (!bEnableAutoLODGeneration)
	{
		return false;
	}
#endif

	return true;
}

#if WITH_EDITOR
void UInstancedSkinnedMeshComponent::ComputeHLODHash(FHLODHashBuilder& HashBuilder) const
{
	Super::ComputeHLODHash(HashBuilder);

	FHLODHashScope HashScope(HashBuilder, TEXT("UInstancedSkinnedMeshComponent"));

	for (const FSkinnedMeshInstanceData& SkinnedMeshInstanceData : InstanceData)
	{
		HashBuilder << FTransform(SkinnedMeshInstanceData.Transform);
		HashBuilder << SkinnedMeshInstanceData.BankIndex;
	}
	HashBuilder << FHLODHashContext(GET_MEMBER_NAME_CHECKED(UInstancedSkinnedMeshComponent, InstanceData));

	for (FAnimBankItem AnimBankItem : AnimBankItems)
	{
		HashBuilder << AnimBankItem.BankAsset;
		HashBuilder << AnimBankItem.SequenceIndex;
	}
	HashBuilder << FHLODHashContext(GET_MEMBER_NAME_CHECKED(UInstancedSkinnedMeshComponent, AnimBankItems));

	HashBuilder << InstanceCustomData << FHLODHashContext(GET_MEMBER_NAME_CHECKED(UInstancedSkinnedMeshComponent, InstanceCustomData));
	HashBuilder << InstanceMinDrawDistance << FHLODHashContext(GET_MEMBER_NAME_CHECKED(UInstancedSkinnedMeshComponent, InstanceMinDrawDistance));
	HashBuilder << InstanceStartCullDistance << FHLODHashContext(GET_MEMBER_NAME_CHECKED(UInstancedSkinnedMeshComponent, InstanceStartCullDistance));
	HashBuilder << InstanceEndCullDistance << FHLODHashContext(GET_MEMBER_NAME_CHECKED(UInstancedSkinnedMeshComponent, InstanceEndCullDistance));
		
	HashBuilder << GetSkinnedAsset() << FHLODHashContext(TEXT("SkinnedAsset"));
}
#endif

void UInstancedSkinnedMeshComponent::CreateRenderState_Concurrent(FRegisterComponentContext* Context)
{
	MeshObjectFactory = &CreateInstancedSkinnedMeshObjectFn;
	Super::CreateRenderState_Concurrent(Context);
}

void UInstancedSkinnedMeshComponent::DestroyRenderState_Concurrent()
{
	Super::DestroyRenderState_Concurrent();
}

FPrimitiveSceneProxy* UInstancedSkinnedMeshComponent::CreateSceneProxy()
{
	LLM_SCOPE(ELLMTag::SkeletalMesh);

	ERHIFeatureLevel::Type SceneFeatureLevel = GetWorld()->GetFeatureLevel();
	FPrimitiveSceneProxy* Result = nullptr;
	FSkeletalMeshRenderData* SkelMeshRenderData = GetSkeletalMeshRenderData();

#if WITH_EDITOR
	if (!bIsInstanceDataApplyCompleted)
	{
		return nullptr;
	}
#endif

	const USkinnedAsset* SkinnedAssetPtr = GetSkinnedAsset();
	if (GetInstanceCount() == 0 || SkinnedAssetPtr == nullptr || SkinnedAssetPtr->IsCompiling())
	{
		return nullptr;
	}

	for (FAnimBankItem& BankItem : AnimBankItems)
	{
		if (BankItem.BankAsset && BankItem.BankAsset->IsCompiling())
		{
			return nullptr;
		}
	}

	if (CheckPSOPrecachingAndBoostPriority() && GetPSOPrecacheProxyCreationStrategy() == EPSOPrecacheProxyCreationStrategy::DelayUntilPSOPrecached)
	{
		UE_LOG(LogAnimBank, Verbose, TEXT("Skipping CreateSceneProxy for UInstancedSkinnedMeshComponent %s (UInstancedSkinnedMeshComponent PSOs are still compiling)"), *GetFullName());
		return nullptr;
	}

	GetOrCreateInstanceDataSceneProxy();

	Result = CreateSceneProxy(FInstancedSkinnedMeshSceneProxyDesc(this), bHideSkin, ShouldNaniteSkin(), IsEnabled(), ComputeMinLOD());

	// Unclear exactly how this is supposed to work with a non-instanced proxy - will be interesting...
	// If GPU-only flag set, instance data is entirely GPU driven, don't upload from CPU.
	if (Result && !bIsInstanceDataGPUOnly)
	{
		InstanceDataManager.FlushChanges(GetComponentDesc(Result->GetScene().GetFeatureLevel()));
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	SendRenderDebugPhysics(Result);
#endif

	return Result;
}

void UInstancedSkinnedMeshComponent::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	Super::OnUpdateTransform(UpdateTransformFlags, Teleport);
	InstanceDataManager.PrimitiveTransformChanged();
}

#if WITH_EDITOR

void UInstancedSkinnedMeshComponent::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	// Always clear the change tracking because in the editor, attributes may have been set without any sort of notification
	InstanceDataManager.ClearChangeTracking();
	if (PropertyChangedEvent.Property != nullptr)
	{
		if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UInstancedSkinnedMeshComponent, InstanceData))
		{
			if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayAdd
				|| PropertyChangedEvent.ChangeType == EPropertyChangeType::Duplicate)
			{
				int32 AddedAtIndex = PropertyChangedEvent.GetArrayIndex(PropertyChangedEvent.Property->GetFName().ToString());
				check(AddedAtIndex != INDEX_NONE);

				AddInstanceInternal(
					AddedAtIndex,
					PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayAdd ? FTransform::Identity : FTransform(InstanceData[AddedAtIndex].Transform),
					PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayAdd ? 0 : InstanceData[AddedAtIndex].BankIndex,
					/*bWorldSpace*/false
				);

				// added via the property editor, so we will want to interactively work with instances
				//bHasPerInstanceHitProxies = true;
			}
			else if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayRemove)
			{
				int32 RemovedAtIndex = PropertyChangedEvent.GetArrayIndex(PropertyChangedEvent.Property->GetFName().ToString());
				check(RemovedAtIndex != INDEX_NONE);

				RemoveInstanceInternal(RemovedAtIndex, true);
			}
			else if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayClear)
			{
				ClearInstances();
			}
			else if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ValueSet)
			{
			}
			MarkRenderStateDirty();
		}
		else if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(FSkinnedMeshInstanceData, Transform)
			 || PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(FSkinnedMeshInstanceData, BankIndex))
		{
			MarkRenderStateDirty();
		}
		else if (PropertyChangedEvent.Property->GetFName() == "NumCustomDataFloats")
		{
			SetNumCustomDataFloats(NumCustomDataFloats);
		}
		else if (PropertyChangedEvent.PropertyChain.GetActiveMemberNode()->GetValue()->GetFName() == "InstanceCustomData")
		{
			int32 ChangedCustomValueIndex = PropertyChangedEvent.GetArrayIndex(PropertyChangedEvent.Property->GetFName().ToString());
			if (ensure(NumCustomDataFloats > 0))
			{
				int32 InstanceIndex = ChangedCustomValueIndex / NumCustomDataFloats;
			}
			MarkRenderStateDirty();
		}
		else if (
			(PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UInstancedSkinnedMeshComponent, AnimBankItems)) ||
			(PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(FAnimBankItem, BankAsset))
			)
		{
			for (FAnimBankItem& BankItem : AnimBankItems)
			{
				// TODO: BankItem.ValidatePosition();

				// Make sure the animation skeleton is valid
				bool bInvalid = true;
				if (BankItem.BankAsset && BankItem.BankAsset->Asset && BankItem.BankAsset->Asset->GetSkeleton())
				{
					USkeletalMesh* SkeletalMeshPtr = Cast<USkeletalMesh>(GetSkinnedAsset());
					if (SkeletalMeshPtr)
					{
						// Make sure the skeletons match!
						if (BankItem.BankAsset->Asset->GetSkeleton() == SkeletalMeshPtr->GetSkeleton())
						{
							bInvalid = false;
						}
					}
				}

				if (bInvalid)
				{
					UE_LOG(LogAnimation, Warning, TEXT("Invalid animation skeleton"));
					BankItem.BankAsset = nullptr;
				}
			}

			MarkRenderStateDirty();
		}
	}
	Super::PostEditChangeChainProperty(PropertyChangedEvent);
}

void UInstancedSkinnedMeshComponent::PostEditUndo()
{
	Super::PostEditUndo();
	MarkRenderStateDirty();
}

void UInstancedSkinnedMeshComponent::BeginCacheForCookedPlatformData(const ITargetPlatform* TargetPlatform)
{
	for (FAnimBankItem& BankItem : AnimBankItems)
	{
		if (BankItem.BankAsset)
		{
			BankItem.BankAsset->BeginCacheForCookedPlatformData(TargetPlatform);
		}
	}

}

bool UInstancedSkinnedMeshComponent::IsCachedCookedPlatformDataLoaded( const ITargetPlatform* TargetPlatform ) 
{
	for (FAnimBankItem& BankItem : AnimBankItems)
	{
		if (BankItem.BankAsset->IsCompiling())
		{
			return false;
		}
	}

	return Super::IsCachedCookedPlatformDataLoaded(TargetPlatform);
}

#endif

TStructOnScope<FActorComponentInstanceData> UInstancedSkinnedMeshComponent::GetComponentInstanceData() const
{
	TStructOnScope<FActorComponentInstanceData> ComponentInstanceData;
#if WITH_EDITOR
	ComponentInstanceData.InitializeAs<FInstancedSkinnedMeshComponentInstanceData>(this);
	FInstancedSkinnedMeshComponentInstanceData* SkinnedMeshInstanceData = ComponentInstanceData.Cast<FInstancedSkinnedMeshComponentInstanceData>();

	// Back up per-instance info (this is strictly for Comparison in UInstancedSkinnedMeshComponent::ApplyComponentInstanceData 
	// as this Property will get serialized by base class FActorComponentInstanceData through FComponentPropertyWriter which uses the PPF_ForceTaggedSerialization to backup all properties even the custom serialized ones
	SkinnedMeshInstanceData->InstanceData = InstanceData;

	// Back up instance selection
	SkinnedMeshInstanceData->SelectedInstances = SelectedInstances;

	// Back up per-instance hit proxies
	SkinnedMeshInstanceData->bHasPerInstanceHitProxies = bHasPerInstanceHitProxies;
#endif
	return ComponentInstanceData;
}

void UInstancedSkinnedMeshComponent::SetCullDistances(int32 StartCullDistance, int32 EndCullDistance)
{
	if (InstanceStartCullDistance != StartCullDistance || InstanceEndCullDistance != EndCullDistance)
	{
		InstanceStartCullDistance = StartCullDistance;
		InstanceEndCullDistance = EndCullDistance;

		if (GetScene() && SceneProxy)
		{
			GetScene()->UpdateInstanceCullDistance(this, StartCullDistance, EndCullDistance);
		}
	}
}

void UInstancedSkinnedMeshComponent::PreApplyComponentInstanceData(struct FInstancedSkinnedMeshComponentInstanceData* InstancedMeshData)
{
#if WITH_EDITOR
	// Prevent proxy recreate while traversing the ::ApplyToComponent stack
	bIsInstanceDataApplyCompleted = false;
#endif
}

void UInstancedSkinnedMeshComponent::ApplyComponentInstanceData(struct FInstancedSkinnedMeshComponentInstanceData* InstancedMeshData)
{
#if WITH_EDITOR
	check(InstancedMeshData);

	ON_SCOPE_EXIT
	{
		bIsInstanceDataApplyCompleted = true;
	};

	if (GetSkinnedAsset() != InstancedMeshData->SkinnedAsset)
	{
		return;
	}

	// If we should inherit from archetype do it here after data was applied and before comparing (RerunConstructionScript will serialize SkipSerialization properties and reapply them even if we want to inherit them)
	const UInstancedSkinnedMeshComponent* Archetype = Cast<UInstancedSkinnedMeshComponent>(GetArchetype());
	if (ShouldInheritPerInstanceData(Archetype))
	{
		ApplyInheritedPerInstanceData(Archetype);
	}

	SelectedInstances = InstancedMeshData->SelectedInstances;
	bHasPerInstanceHitProxies = InstancedMeshData->bHasPerInstanceHitProxies;
	PrimitiveBoundsOverride = InstancedMeshData->PrimitiveBoundsOverride;
	bIsInstanceDataGPUOnly = InstancedMeshData->bIsInstanceDataGPUOnly;
	NumInstancesGPUOnly = InstancedMeshData->NumInstancesGPUOnly;
#endif
}

FBoxSphereBounds UInstancedSkinnedMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	//SCOPE_CYCLE_COUNTER(STAT_CalcSkelMeshBounds);
	if (PrimitiveBoundsOverride.IsValid)
	{
		return PrimitiveBoundsOverride.InverseTransformBy(GetComponentTransform().Inverse() * LocalToWorld);
	}
	else
	{
		return FInstancedSkinnedMeshComponentHelper::CalcBounds(*this, LocalToWorld);
	}
}

void UInstancedSkinnedMeshComponent::SetSkinnedAssetCallback()
{
	MarkRenderStateDirty();
}

void UInstancedSkinnedMeshComponent::RefreshBoneTransforms(FActorComponentTickFunction* TickFunction)
{
	//SCOPE_CYCLE_COUNTER(STAT_RefreshBoneTransforms);

	// Can't do anything without a SkinnedAsset
	if (!GetSkinnedAsset())
	{
		return;
	}

	// Do nothing more if no bones in skeleton.
	if (GetNumComponentSpaceTransforms() == 0)
	{
		return;
	}

	// We need the mesh space bone transforms now for renderer to get delta from ref pose:
	//FillComponentSpaceTransforms();
	//FinalizeBoneTransform();

	//UpdateChildTransforms();
	UpdateBounds();
	MarkRenderTransformDirty();
	MarkRenderDynamicDataDirty();

	//bNeedsRefreshTransform = false;
}

void UInstancedSkinnedMeshComponent::SetNumGPUInstances(int32 InCount)
{
	NumInstancesGPUOnly = InCount;
}

FPrimitiveInstanceId UInstancedSkinnedMeshComponent::AddInstance(const FTransform& InstanceTransform, int32 BankIndex, bool bWorldSpace)
{
	return AddInstanceInternal(InstanceData.Num(), InstanceTransform, BankIndex, bWorldSpace);
}

TArray<FPrimitiveInstanceId> UInstancedSkinnedMeshComponent::AddInstances(const TArray<FTransform>& Transforms, const TArray<int32>& BankIndices, bool bShouldReturnIds, bool bWorldSpace)
{
	TArray<FPrimitiveInstanceId> NewInstanceIds;
	if (Transforms.IsEmpty() || (Transforms.Num() != BankIndices.Num()))
	{
		return NewInstanceIds;
	}

	Modify();

	const int32 NumToAdd = Transforms.Num();

	if (bShouldReturnIds)
	{
		NewInstanceIds.SetNumUninitialized(NumToAdd);
	}

	// Reserve memory space
	const int32 NewNumInstances = InstanceData.Num() + NumToAdd;
	InstanceData.Reserve(NewNumInstances);
	InstanceCustomData.Reserve(NumCustomDataFloats * NewNumInstances);
#if WITH_EDITOR
	SelectedInstances.Reserve(NewNumInstances);
#endif

	for (int32 AddIndex = 0; AddIndex < NumToAdd; ++AddIndex)
	{
		const FTransform& Transform = Transforms[AddIndex];
		const int32 BankIndex = BankIndices[AddIndex];
		FPrimitiveInstanceId InstanceId = AddInstanceInternal(InstanceData.Num(), Transform, BankIndex, bWorldSpace);
		if (bShouldReturnIds)
		{
			NewInstanceIds[AddIndex] = InstanceId;
		}
	}

	return NewInstanceIds;
}

bool UInstancedSkinnedMeshComponent::SetCustomDataValue(FPrimitiveInstanceId InstanceId, int32 CustomDataIndex, float CustomDataValue)
{
	int32 InstanceIndex = InstanceDataManager.IdToIndex(InstanceId);

	if (!InstanceData.IsValidIndex(InstanceIndex) || CustomDataIndex < 0 || CustomDataIndex >= NumCustomDataFloats)
	{
		return false;
	}

	Modify();

	InstanceDataManager.CustomDataChanged(InstanceIndex);
	InstanceCustomData[InstanceIndex * NumCustomDataFloats + CustomDataIndex] = CustomDataValue;

	return true;
}

bool UInstancedSkinnedMeshComponent::SetCustomData(FPrimitiveInstanceId InstanceId, TArrayView<const float> CustomDataFloats)
{
	int32 InstanceIndex = InstanceDataManager.IdToIndex(InstanceId);

	if (!InstanceData.IsValidIndex(InstanceIndex) || CustomDataFloats.Num() == 0)
	{
		return false;
	}

	Modify();

	const int32 NumToCopy = FMath::Min(CustomDataFloats.Num(), NumCustomDataFloats);
	InstanceDataManager.CustomDataChanged(InstanceIndex);
	FMemory::Memcpy(&InstanceCustomData[InstanceIndex * NumCustomDataFloats], CustomDataFloats.GetData(), NumToCopy * CustomDataFloats.GetTypeSize());
	return true;
}

void UInstancedSkinnedMeshComponent::SetNumCustomDataFloats(int32 InNumCustomDataFloats)
{
	if (FMath::Max(InNumCustomDataFloats, 0) != NumCustomDataFloats)
	{
		NumCustomDataFloats = FMath::Max(InNumCustomDataFloats, 0);
	}

	if (InstanceData.Num() * NumCustomDataFloats != InstanceCustomData.Num())
	{
		InstanceDataManager.NumCustomDataChanged();

		// Clear out and reinit to 0
		InstanceCustomData.Empty(InstanceData.Num() * NumCustomDataFloats);
		InstanceCustomData.SetNumZeroed(InstanceData.Num() * NumCustomDataFloats);
	}
}

bool UInstancedSkinnedMeshComponent::GetCustomData(FPrimitiveInstanceId InstanceId, TArrayView<float> CustomDataFloats) const
{
	int32 InstanceIndex = InstanceDataManager.IdToIndex(InstanceId);
	if (!InstanceData.IsValidIndex(InstanceIndex))
	{
		return false;
	}

	const int32 NumToCopy = FMath::Min(CustomDataFloats.Num(), NumCustomDataFloats);
	FMemory::Memcpy(CustomDataFloats.GetData(), &InstanceCustomData[InstanceIndex * NumCustomDataFloats], NumToCopy * CustomDataFloats.GetTypeSize());
	return true;
}

bool UInstancedSkinnedMeshComponent::GetInstanceTransform(FPrimitiveInstanceId InstanceId, FTransform& OutInstanceTransform, bool bWorldSpace) const
{
	int32 InstanceIndex = InstanceDataManager.IdToIndex(InstanceId);
	if (!InstanceData.IsValidIndex(InstanceIndex))
	{
		return false;
	}

	const FSkinnedMeshInstanceData& Instance = InstanceData[InstanceIndex];

	OutInstanceTransform = FTransform(Instance.Transform);
	if (bWorldSpace)
	{
		OutInstanceTransform = OutInstanceTransform * GetComponentTransform();
	}

	return true;
}

bool UInstancedSkinnedMeshComponent::GetInstanceBankIndex(FPrimitiveInstanceId InstanceId, int32& OutBankIndex) const
{
	int32 InstanceIndex = InstanceDataManager.IdToIndex(InstanceId);
	if (!InstanceData.IsValidIndex(InstanceIndex))
	{
		return false;
	}

	OutBankIndex = InstanceData[InstanceIndex].BankIndex;
	return true;
}

bool UInstancedSkinnedMeshComponent::RemoveInstance(FPrimitiveInstanceId InstanceId)
{
	int32 InstanceIndex = InstanceDataManager.IdToIndex(InstanceId);
	if (InstanceData.IsValidIndex(InstanceIndex))
	{
		Modify();
		return RemoveInstanceInternal(InstanceIndex, false);
	}
	return false;
}

void UInstancedSkinnedMeshComponent::RemoveInstances(const TArray<FPrimitiveInstanceId>& InstancesToRemove)
{
	Modify();

	for (FPrimitiveInstanceId InstanceId : InstancesToRemove)
	{
		int32 InstanceIndex = InstanceDataManager.IdToIndex(InstanceId);
		RemoveInstanceInternal(InstanceIndex, false);
	}
}

void UInstancedSkinnedMeshComponent::ClearInstances()
{
	Modify();

	// Clear all the per-instance data
	InstanceData.Empty();
	InstanceCustomData.Empty();

#if WITH_EDITOR
	SelectedInstances.Empty();
#endif
	InstanceDataManager.ClearInstances();
}

int32 UInstancedSkinnedMeshComponent::AddAnimBankItem(const FAnimBankItem& BankItem)
{
	int32 ItemIndex = AnimBankItems.Num();
	AnimBankItems.Emplace(BankItem);
	return ItemIndex;
}

struct HSkinnedMeshInstance : public HHitProxy
{
	TObjectPtr<UInstancedSkinnedMeshComponent> Component;
	int32 InstanceIndex;

	DECLARE_HIT_PROXY(ENGINE_API);
	HSkinnedMeshInstance(UInstancedSkinnedMeshComponent* InComponent, int32 InInstanceIndex)
	: HHitProxy(HPP_World)
	, Component(InComponent)
	, InstanceIndex(InInstanceIndex)
	{
	}

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		Collector.AddReferencedObject(Component);
	}

	virtual FTypedElementHandle GetElementHandle() const override
	{
	#if WITH_EDITOR
		if (Component)
		{
		#if 0
			if (true)//if (CVarEnableViewportSMInstanceSelection.GetValueOnAnyThread() != 0)
			{
				// Prefer per-instance selection if available
				// This may fail to return a handle if the feature is disabled, or if per-instance editing is disabled for this component
				if (FTypedElementHandle ElementHandle = UEngineElementsLibrary::AcquireEditorSMInstanceElementHandle(Component, InstanceIndex))
				{
					return ElementHandle;
				}
			}
		#endif

			// If per-instance selection isn't possible, fallback to general per-component selection (which may choose to select the owner actor instead)
			return UEngineElementsLibrary::AcquireEditorComponentElementHandle(Component);
		}
	#endif	// WITH_EDITOR
		return FTypedElementHandle();
	}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::Crosshairs;
	}
};

IMPLEMENT_HIT_PROXY(HSkinnedMeshInstance, HHitProxy);

void UInstancedSkinnedMeshComponent::CreateHitProxyData(TArray<TRefCountPtr<HHitProxy>>& HitProxies)
{
	if (GIsEditor && bHasPerInstanceHitProxies)
	{
		int32 NumProxies = InstanceData.Num();
		HitProxies.Empty(NumProxies);

		for (int32 InstanceIdx = 0; InstanceIdx < NumProxies; ++InstanceIdx)
		{
			HitProxies.Add(new HSkinnedMeshInstance(this, InstanceIdx));
		}
	}
	else
	{
		HitProxies.Empty();
	}
}

FPrimitiveInstanceId UInstancedSkinnedMeshComponent::AddInstanceInternal(int32 InstanceIndex, const FTransform& InstanceTransform, int32 BankIndex, bool bWorldSpace)
{
	// This happens because the editor modifies the InstanceData array _before_ callbacks. If we could change the UI to not do that we could remove this ugly hack.
	if (!InstanceData.IsValidIndex(InstanceIndex))
	{
		check(InstanceIndex == InstanceData.Num());
		InstanceData.AddDefaulted();
	}

	FPrimitiveInstanceId InstanceId = InstanceDataManager.Add(InstanceIndex);

	const FTransform3f LocalTransform = FTransform3f(bWorldSpace ? InstanceTransform.GetRelativeTransform(GetComponentTransform()) : InstanceTransform);
	SetupNewInstanceData(InstanceData[InstanceIndex], InstanceIndex, LocalTransform, BankIndex);

	// Add custom data to instance
	InstanceCustomData.AddZeroed(NumCustomDataFloats);

#if WITH_EDITOR
	SelectedInstances.Add(false);
#endif

	return InstanceId;
}

bool UInstancedSkinnedMeshComponent::RemoveInstanceInternal(int32 InstanceIndex, bool bInstanceAlreadyRemoved)
{
	if (!ensure(bInstanceAlreadyRemoved || InstanceData.IsValidIndex(InstanceIndex)))
	{
		return false;
	}
	InstanceDataManager.RemoveAtSwap(InstanceIndex);
	
	// remove instance
	if (!bInstanceAlreadyRemoved)
	{
		InstanceData.RemoveAtSwap(InstanceIndex, EAllowShrinking::No);
	}
	
	if (InstanceCustomData.IsValidIndex(InstanceIndex * NumCustomDataFloats))
	{
		InstanceCustomData.RemoveAtSwap(InstanceIndex * NumCustomDataFloats, NumCustomDataFloats, EAllowShrinking::No);
	}

#if WITH_EDITOR
	// remove selection flag if array is filled in
	if (SelectedInstances.IsValidIndex(InstanceIndex))
	{
		SelectedInstances.RemoveAtSwap(InstanceIndex);
	}
#endif
	return true;
}

FSkeletalMeshObject* UInstancedSkinnedMeshComponent::CreateMeshObject(const FInstancedSkinnedMeshSceneProxyDesc& InMeshDesc, TConstArrayView<FAnimBankItem> InAnimBankItems, FSkeletalMeshRenderData* InRenderData, ERHIFeatureLevel::Type InFeatureLevel)
{
	return ::new FInstancedSkinnedMeshObject(InMeshDesc, InAnimBankItems, InRenderData, InFeatureLevel);
}

FPrimitiveSceneProxy* UInstancedSkinnedMeshComponent::CreateSceneProxy(const FInstancedSkinnedMeshSceneProxyDesc& Desc, bool bHideSkin, bool bShouldNaniteSkin, bool bIsEnabled, int32 MinLODIndex)
{
	LLM_SCOPE(ELLMTag::SkeletalMesh);
	FPrimitiveSceneProxy* Result = nullptr;
	FSkeletalMeshRenderData* SkelMeshRenderData = Desc.GetSkinnedAsset()->GetResourceForRendering();

	FSkeletalMeshObject* MeshObject = Desc.MeshObject;

	// Only create a scene proxy for rendering if properly initialized
	if (SkelMeshRenderData &&
		SkelMeshRenderData->LODRenderData.IsValidIndex(Desc.PredictedLODLevel) &&
		!bHideSkin &&
		MeshObject)
	{
		// Only create a scene proxy if the bone count being used is supported, or if we don't have a skeleton (this is the case with destructibles)
		int32 MaxBonesPerChunk = SkelMeshRenderData->GetMaxBonesPerSection(MinLODIndex);
		int32 MaxSupportedNumBones = MeshObject->IsCPUSkinned() ? MAX_int32 : FGPUBaseSkinVertexFactory::GetMaxGPUSkinBones();
		if (MaxBonesPerChunk <= MaxSupportedNumBones)
		{
			if (bShouldNaniteSkin && bIsEnabled)
			{
				Nanite::FMaterialAudit NaniteMaterials{};
				const bool bSetMaterialUsageFlags = true;
				Nanite::FNaniteResourcesHelper::AuditMaterials(&Desc, NaniteMaterials, bSetMaterialUsageFlags);

				const bool bForceNaniteForMasked = false;
				const bool bIsMaskingAllowed = Nanite::IsMaskingAllowed(Desc.GetWorld(), bForceNaniteForMasked);
				if (NaniteMaterials.IsValid(bIsMaskingAllowed))
				{
					Result = ::new FInstancedSkinnedMeshSceneProxy(NaniteMaterials, Desc, SkelMeshRenderData);
				}
			}

			if (Result == nullptr)
			{
				Result = FSkinnedMeshSceneProxyDesc::CreateSceneProxy(Desc, bHideSkin, MinLODIndex);
			}
		}
	}

	return Result;
}

TSharedPtr<FInstanceDataSceneProxy, ESPMode::ThreadSafe> UInstancedSkinnedMeshComponent::GetOrCreateInstanceDataSceneProxy()
{
	if (bIsInstanceDataGPUOnly)
	{
		return CreateInstanceDataProxyGPUOnly();
	}
	else
	{
		return InstanceDataManager.GetOrCreateProxy();
	}
}

TSharedPtr<FInstanceDataSceneProxy, ESPMode::ThreadSafe> UInstancedSkinnedMeshComponent::GetInstanceDataSceneProxy() const
{
	if (bIsInstanceDataGPUOnly)
	{
		return CreateInstanceDataProxyGPUOnly();
	}
	else
	{
		return const_cast<UInstancedSkinnedMeshComponent*>(this)->InstanceDataManager.GetProxy();
	}
}

TSharedPtr<FInstanceDataSceneProxy, ESPMode::ThreadSafe> UInstancedSkinnedMeshComponent::CreateInstanceDataProxyGPUOnly() const
{
	FInstanceSceneDataBuffers InstanceSceneDataBuffers(/*InbInstanceDataIsGPUOnly=*/true);
	{
		FInstanceSceneDataBuffers::FAccessTag AccessTag(PointerHash(this));
		FInstanceSceneDataBuffers::FWriteView ProxyData = InstanceSceneDataBuffers.BeginWriteAccess(AccessTag);

		InstanceSceneDataBuffers.SetPrimitiveLocalToWorld(GetRenderMatrix(), AccessTag);

		ProxyData.NumInstancesGPUOnly = GetInstanceCountGPUOnly();
		ProxyData.NumCustomDataFloats = NumCustomDataFloats;
		ProxyData.InstanceLocalBounds.SetNum(1);
		ProxyData.InstanceLocalBounds[0] = ensure(GetSkinnedAsset()) ? GetSkinnedAsset()->GetBounds() : FBox();

		ProxyData.Flags.bHasPerInstanceCustomData = ProxyData.NumCustomDataFloats > 0;

		InstanceSceneDataBuffers.EndWriteAccess(AccessTag);
		InstanceSceneDataBuffers.ValidateData();
	}

	return MakeShared<FInstanceDataSceneProxy, ESPMode::ThreadSafe>(MoveTemp(InstanceSceneDataBuffers));
}
