// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkeletalRenderNanite.h"
#include "Animation/MeshDeformerInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "RenderUtils.h"
#include "SkeletalRender.h"
#include "GPUSkinCache.h"
#include "Rendering/RenderCommandPipes.h"
#include "ShaderParameterUtils.h"
#include "SceneInterface.h"
#include "SkeletalMeshSceneProxy.h"
#include "RenderGraphUtils.h"
#include "RenderCore.h"
#include "Engine/SkinnedAssetCommon.h"
#include "SkeletalRenderGPUSkin.h"
#include "SkinnedMeshSceneProxyDesc.h"

FDynamicSkelMeshObjectDataNanite::FDynamicSkelMeshObjectDataNanite(
	USkinnedMeshComponent* InComponent,
	FSkeletalMeshRenderData* InRenderData,
	int32 InLODIndex,
	EPreviousBoneTransformUpdateMode InPreviousBoneTransformUpdateMode,
	FSkeletalMeshObjectNanite* InMeshObject
)
	: FDynamicSkelMeshObjectDataNanite(
		FSkinnedMeshSceneProxyDynamicData(InComponent),
		InComponent->GetSkinnedAsset(),
		InRenderData,
		InLODIndex,
		InPreviousBoneTransformUpdateMode,
		InMeshObject)
{}

FDynamicSkelMeshObjectDataNanite::FDynamicSkelMeshObjectDataNanite(
	const FSkinnedMeshSceneProxyDynamicData& InDynamicData,
	const USkinnedAsset* InSkinnedAsset,
	FSkeletalMeshRenderData* InRenderData,
	int32 InLODIndex,
	EPreviousBoneTransformUpdateMode InPreviousBoneTransformUpdateMode,
	FSkeletalMeshObjectNanite* InMeshObject
)
:	LODIndex(InLODIndex)
{
#if RHI_RAYTRACING
	RayTracingLODIndex = FMath::Clamp(FMath::Max(LODIndex, InMeshObject->RayTracingMinLOD), LODIndex, InRenderData->LODRenderData.Num() - 1);
#endif

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	ComponentSpaceTransforms = InDynamicData.GetComponentSpaceTransforms();

	const bool bCalculateComponentSpaceTransformsFromLeader = ComponentSpaceTransforms.IsEmpty(); // This will be empty for follower components.
	TArray<FTransform>* const LeaderBoneMappedMeshComponentSpaceTransforms = bCalculateComponentSpaceTransformsFromLeader ? &ComponentSpaceTransforms : nullptr;
#else
	TArray<FTransform>* const LeaderBoneMappedMeshComponentSpaceTransforms = nullptr;
#endif

	UpdateRefToLocalMatrices(ReferenceToLocal, InDynamicData, InSkinnedAsset, InRenderData, LODIndex, nullptr, LeaderBoneMappedMeshComponentSpaceTransforms);
#if RHI_RAYTRACING
	if (RayTracingLODIndex != LODIndex)
	{
		UpdateRefToLocalMatrices(ReferenceToLocalForRayTracing, InDynamicData, InSkinnedAsset, InRenderData, RayTracingLODIndex, nullptr);
	}
#endif

	UpdateBonesRemovedByLOD(ReferenceToLocal, InDynamicData, InSkinnedAsset, ETransformsToUpdate::Current);

	CurrentBoneTransforms.SetNumUninitialized(ReferenceToLocal.Num());

	const int64 ReferenceToLocalCount = int64(ReferenceToLocal.Num());
	const FMatrix44f* ReferenceToLocalPtr = ReferenceToLocal.GetData();
	FMatrix3x4* CurrentBoneTransformsPtr = CurrentBoneTransforms.GetData();

	TransposeTransforms(CurrentBoneTransformsPtr, ReferenceToLocalPtr, ReferenceToLocalCount);

	bool bUpdatePrevious = false;

	switch (InPreviousBoneTransformUpdateMode)
	{
	case EPreviousBoneTransformUpdateMode::None:
		// Use previously uploaded buffer
		// TODO: Nanite-Skinning, optimize scene extension upload to keep cached GPU representation using PreviousBoneTransformRevisionNumber
		// For now we'll just redundantly update and upload previous transforms
		UpdatePreviousRefToLocalMatrices(PrevReferenceToLocal, InDynamicData, InSkinnedAsset, InRenderData, LODIndex);
#if RHI_RAYTRACING
		if (RayTracingLODIndex != LODIndex)
		{
			UpdatePreviousRefToLocalMatrices(PrevReferenceToLocalForRayTracing, InDynamicData, InSkinnedAsset, InRenderData, RayTracingLODIndex);
		}
#endif
		UpdateBonesRemovedByLOD(PrevReferenceToLocal, InDynamicData, InSkinnedAsset, ETransformsToUpdate::Previous);
		bUpdatePrevious = true;
		break;

	case EPreviousBoneTransformUpdateMode::UpdatePrevious:
		UpdatePreviousRefToLocalMatrices(PrevReferenceToLocal, InDynamicData, InSkinnedAsset, InRenderData, LODIndex);
#if RHI_RAYTRACING
		if (RayTracingLODIndex != LODIndex)
		{
			UpdatePreviousRefToLocalMatrices(PrevReferenceToLocalForRayTracing, InDynamicData, InSkinnedAsset, InRenderData, RayTracingLODIndex);
		}
#endif
		UpdateBonesRemovedByLOD(PrevReferenceToLocal, InDynamicData, InSkinnedAsset, ETransformsToUpdate::Previous);
		bUpdatePrevious = true;
		break;

	case EPreviousBoneTransformUpdateMode::DuplicateCurrentToPrevious:
		// TODO: Nanite-Skinning likely possible we can just return ReferenceToLocal here rather than cloning it into previous
		// Need to make sure it's safe when next update mode = None
		PrevReferenceToLocal = ReferenceToLocal;
#if RHI_RAYTRACING
		if (RayTracingLODIndex != LODIndex)
		{
			PrevReferenceToLocalForRayTracing = ReferenceToLocalForRayTracing;
		}
#endif
		PreviousBoneTransforms = CurrentBoneTransforms;
		break;
	}

	BoneTransformFrameNumber = GFrameCounter;
	RevisionNumber = InDynamicData.GetBoneTransformRevisionNumber();
	PreviousRevisionNumber = InDynamicData.GetPreviousBoneTransformRevisionNumber();
	bRecreating = InDynamicData.IsRenderStateRecreating();

	if (bUpdatePrevious)
	{
		PreviousBoneTransforms.SetNumUninitialized(PrevReferenceToLocal.Num());
		const FMatrix44f* PrevReferenceToLocalPtr = PrevReferenceToLocal.GetData();

		const int64 PrevReferenceToLocalCount = int64(PrevReferenceToLocal.Num());
		FMatrix3x4* PreviousBoneTransformsPtr = PreviousBoneTransforms.GetData();

		TransposeTransforms(PreviousBoneTransformsPtr, PrevReferenceToLocalPtr, PrevReferenceToLocalCount);
	}
}

FDynamicSkelMeshObjectDataNanite::~FDynamicSkelMeshObjectDataNanite() = default;

void FDynamicSkelMeshObjectDataNanite::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(sizeof(*this));
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(ReferenceToLocal.GetAllocatedSize());
}

void FDynamicSkelMeshObjectDataNanite::UpdateBonesRemovedByLOD(
	TArray<FMatrix44f>& PoseBuffer,
	const FSkinnedMeshSceneProxyDynamicData& InDynamicData,
	const USkinnedAsset* SkinnedAsset,
	ETransformsToUpdate CurrentOrPrevious) const
{
	// Why is this necessary?
	//
	// When the animation system removes bones at higher LODs, the pose in USkinnedMeshComponent::GetComponentSpaceTransforms()
	// will leave the LOD'd bone transforms at their last updated position/rotation. This is not a problem for GPU skinning
	// because the actual weight for those bones is pushed up the hierarchy onto the next non-LOD'd parent; making the transform irrelevant.
	//
	// But Nanite skinning only ever uses the LOD-0 weights (it dynamically interpolates weights for higher-LOD clusters)
	// This means that these "frozen" bone transforms actually affect the skin. Which is bad.
	//
	// So we do an FK update here of the frozen branch of transforms...

	const TArray<FBoneReference>& BonesToRemove = SkinnedAsset->GetLODInfo(LODIndex)->BonesToRemove;
	if (BonesToRemove.IsEmpty())
	{
		return; // no bones removed in this LOD
	}
	
	// get current OR previous component space pose (possibly from a leader component)
	// any LOD'd out bones in this pose are "frozen" since their last update
	TArrayView<const FTransform> ComponentSpacePose = [&InDynamicData, CurrentOrPrevious, SkinnedAsset]
	{
		const bool bIsLeaderCompValid = InDynamicData.HasLeaderPoseComponent() && InDynamicData.GetLeaderBoneMap().Num() == SkinnedAsset->GetRefSkeleton().GetNum();
		switch (CurrentOrPrevious)
		{
		case ETransformsToUpdate::Current:
			return InDynamicData.GetComponentSpaceTransforms();
		case ETransformsToUpdate::Previous:
			return InDynamicData.GetPreviousComponentTransformsArray();
		default:
			checkNoEntry();
			return TArrayView<const FTransform>();
		}
	}();
	
	// these are inverted ref pose matrices
	const TArray<FMatrix44f>* RefBasesInvMatrix = &SkinnedAsset->GetRefBasesInvMatrix();
	TArray<int32> AllChildrenBones;
	const FReferenceSkeleton& RefSkeleton = SkinnedAsset->GetRefSkeleton();
	for (const FBoneReference& RemovedBone : BonesToRemove)
	{
		AllChildrenBones.Reset();
		// can't use FBoneReference::GetMeshPoseIndex() because rendering operates at lower-level (on USkinnedMeshComponent)
		// but this call to FindBoneIndex is probably not so bad since there's typically only the parent bone of a branch in "BonesToRemove"
		const FBoneIndexType BoneIndex = RefSkeleton.FindBoneIndex(RemovedBone.BoneName);
		AllChildrenBones.Add(BoneIndex);
		RefSkeleton.GetRawChildrenIndicesRecursiveCached(BoneIndex, AllChildrenBones);

		// first pass to generate component space transforms
		for (int32 ChildIndex = 0; ChildIndex<AllChildrenBones.Num(); ++ChildIndex)
		{
			const FBoneIndexType ChildBoneIndex = AllChildrenBones[ChildIndex];
			const FBoneIndexType ParentIndex = RefSkeleton.GetParentIndex(ChildBoneIndex);

			FMatrix44f ParentComponentTransform;
			if (ParentIndex == INDEX_NONE)
			{
				ParentComponentTransform = FMatrix44f::Identity; // root bone transform is always component space
			}
			else if (ChildIndex == 0)
			{
				ParentComponentTransform = static_cast<FMatrix44f>(ComponentSpacePose[ParentIndex].ToMatrixWithScale());
			}
			else
			{
				ParentComponentTransform = PoseBuffer[ParentIndex];
			}

			const FMatrix44f RefLocalTransform = static_cast<FMatrix44f>(RefSkeleton.GetRefBonePose()[ChildBoneIndex].ToMatrixWithScale());
			PoseBuffer[ChildBoneIndex] = RefLocalTransform * ParentComponentTransform;
		}

		// second pass to make relative to ref pose
		for (const FBoneIndexType ChildBoneIndex : AllChildrenBones)
		{
			PoseBuffer[ChildBoneIndex] = (*RefBasesInvMatrix)[ChildBoneIndex] * PoseBuffer[ChildBoneIndex];
		}
	}
}

//////////////////////////////////////////////////////////////////////////

class FSkeletalMeshUpdatePacketNanite : public TSkeletalMeshUpdatePacket<FSkeletalMeshObjectNanite, FDynamicSkelMeshObjectDataNanite>
{
public:
	void Init(const FInitializer& Initializer) override;
	void UpdateImmediate(FRHICommandList& RHICmdList, FSkeletalMeshObjectNanite* MeshObject, FDynamicSkelMeshObjectDataNanite* DynamicData);
	void Add(FSkeletalMeshObjectNanite* MeshObject, FDynamicSkelMeshObjectDataNanite* DynamicData);
	void ProcessStage_SkinCache(FRHICommandList& RHICmdList) override;
	void ProcessStage_Upload(FRHICommandList& RHICmdList) override;
	void Free(FDynamicSkelMeshObjectDataNanite* DynamicData);

private:
#if RHI_RAYTRACING
	TArray<FSkeletalMeshObjectNanite*, FConcurrentLinearArrayAllocator> SkinCacheRayTracing;
#endif
};

void FSkeletalMeshUpdatePacketNanite::Init(const FInitializer& Initializer)
{
#if RHI_RAYTRACING
	SkinCacheRayTracing.Reserve(Initializer.NumUpdates);
#endif
}

void FSkeletalMeshUpdatePacketNanite::UpdateImmediate(FRHICommandList& RHICmdList, FSkeletalMeshObjectNanite* MeshObject, FDynamicSkelMeshObjectDataNanite* DynamicData)
{
	MeshObject->UpdateDynamicData_RenderThread(RHICmdList, GPUSkinCache, DynamicData);
}

void FSkeletalMeshUpdatePacketNanite::Add(FSkeletalMeshObjectNanite* MeshObject, FDynamicSkelMeshObjectDataNanite* DynamicData)
{
	delete MeshObject->DynamicData;
	MeshObject->DynamicData = DynamicData;

#if RHI_RAYTRACING
	if (IsSkinCacheForRayTracingSupported() && MeshObject->SkeletalMeshRenderData->bSupportRayTracing)
	{
		SkinCacheRayTracing.Emplace(MeshObject);
	}
#endif
}

void FSkeletalMeshUpdatePacketNanite::ProcessStage_SkinCache(FRHICommandList& RHICmdList)
{
#if RHI_RAYTRACING
	if (!SkinCacheRayTracing.IsEmpty())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(SkinCacheRayTracing);
		for (FSkeletalMeshObjectNanite* MeshObject : SkinCacheRayTracing)
		{
			MeshObject->ProcessUpdatedDynamicData(RHICmdList, GPUSkinCache);
		}
	}
#endif
}

void FSkeletalMeshUpdatePacketNanite::ProcessStage_Upload(FRHICommandList& RHICmdList)
{
#if RHI_RAYTRACING
	for (FSkeletalMeshObjectNanite* MeshObject : SkinCacheRayTracing)
	{
		MeshObject->UpdateBoneData(RHICmdList);
	}
#endif
}

void FSkeletalMeshUpdatePacketNanite::Free(FDynamicSkelMeshObjectDataNanite* DynamicData)
{
	delete DynamicData;
}

REGISTER_SKELETAL_MESH_UPDATE_BACKEND(FSkeletalMeshUpdatePacketNanite);

//////////////////////////////////////////////////////////////////////////

FSkeletalMeshObjectNanite::FSkeletalMeshObjectNanite(USkinnedMeshComponent* InComponent, FSkeletalMeshRenderData* InRenderData, ERHIFeatureLevel::Type InFeatureLevel)
	: FSkeletalMeshObjectNanite(FSkinnedMeshSceneProxyDesc(InComponent), InRenderData, InFeatureLevel)
{ }

FSkeletalMeshObjectNanite::FSkeletalMeshObjectNanite(const FSkinnedMeshSceneProxyDesc& InMeshDesc, FSkeletalMeshRenderData* InRenderData, ERHIFeatureLevel::Type InFeatureLevel)
: FSkeletalMeshObject(InMeshDesc, InRenderData, InFeatureLevel)
, DynamicData(nullptr)
, CachedLOD(INDEX_NONE)
{
#if RHI_RAYTRACING
	FSkeletalMeshObjectNanite* PreviousMeshObject = nullptr;
	if (InMeshDesc.PreviousMeshObject && InMeshDesc.PreviousMeshObject->IsNaniteMesh())
	{
		PreviousMeshObject = (FSkeletalMeshObjectNanite*)InMeshDesc.PreviousMeshObject;

		// Don't use re-create data if the mesh or feature level changed
		if (PreviousMeshObject->SkeletalMeshRenderData != InRenderData || PreviousMeshObject->FeatureLevel != InFeatureLevel)
		{
			PreviousMeshObject = nullptr;
		}
	}

	if (PreviousMeshObject)
	{
		// Transfer GPU skin cache from PreviousMeshObject -- needs to happen on render thread.  PreviousMeshObject is defer deleted, so it's safe to access it there.
		ENQUEUE_RENDER_COMMAND(ReleaseSkeletalMeshSkinCacheResources)(UE::RenderCommandPipe::SkeletalMesh,
			[this, PreviousMeshObject](FRHICommandList& RHICmdList)
			{
				SkinCacheEntryForRayTracing = PreviousMeshObject->SkinCacheEntryForRayTracing;

				// patch entries to point to new GPUSkin
				FGPUSkinCache::SetEntryGPUSkin(SkinCacheEntryForRayTracing, this);

				PreviousMeshObject->SkinCacheEntryForRayTracing = nullptr;
			}
		);
	}	
#endif

	for (int32 LODIndex = 0; LODIndex < InRenderData->LODRenderData.Num(); ++LODIndex)
	{
		new(LODs) FSkeletalMeshObjectLOD(InFeatureLevel, InRenderData, LODIndex);
	}

	InitResources(InMeshDesc);

	AuditMaterials(&InMeshDesc, NaniteMaterials, true /* Set material usage flags */);

	const bool bIsMaskingAllowed = Nanite::IsMaskingAllowed(InMeshDesc.GetWorld(), false /* force Nanite for masked */);

	bHasValidMaterials = NaniteMaterials.IsValid(bIsMaskingAllowed);

	if (FSkeletalMeshUpdater* Updater = InMeshDesc.Scene ? InMeshDesc.Scene->GetSkeletalMeshUpdater() : nullptr)
	{
		UpdateHandle = Updater->Create(this);
	}
}

FSkeletalMeshObjectNanite::~FSkeletalMeshObjectNanite()
{
	delete DynamicData;
}

void FSkeletalMeshObjectNanite::InitResources(const FSkinnedMeshSceneProxyDesc& InMeshDesc)
{
	for (int32 LODIndex = 0; LODIndex < LODs.Num(); ++LODIndex)
	{
		FSkeletalMeshObjectLOD& LOD = LODs[LODIndex];

		// Skip LODs that have their render data stripped
		if (LOD.RenderData->LODRenderData[LODIndex].GetNumVertices() > 0)
		{
			const FSkelMeshComponentLODInfo* InitLODInfo = InMeshDesc.LODInfo.IsValidIndex(LODIndex) ? &InMeshDesc.LODInfo[LODIndex] : nullptr;
			LOD.InitResources(InitLODInfo, FeatureLevel);
		}
	}

#if RHI_RAYTRACING
	if (IsRayTracingEnabled() && bSupportRayTracing)
	{
		BeginInitResource(&RayTracingGeometry, &UE::RenderCommandPipe::SkeletalMesh);
	}
#endif
}

void FSkeletalMeshObjectNanite::ReleaseResources()
{
	UpdateHandle.Release();

	ENQUEUE_RENDER_COMMAND(FSkeletalMeshObjectNanite_ReleaseResources)(UE::RenderCommandPipe::SkeletalMesh,
		[this](FRHICommandList& RHICmdList)
	{
		for (FSkeletalMeshObjectLOD& LOD : LODs)
		{
			LOD.ReleaseResources();
		}

#if RHI_RAYTRACING
		RayTracingGeometry.ReleaseResource();
		FGPUSkinCache::Release(SkinCacheEntryForRayTracing);
#endif
	});
}

void FSkeletalMeshObjectNanite::Update(
	int32 InLODIndex,
	USkinnedMeshComponent* InComponent,
	const FMorphTargetWeightMap& InActiveMorphTargets,
	const TArray<float>& InMorphTargetWeights,
	EPreviousBoneTransformUpdateMode InPreviousBoneTransformUpdateMode,
	const FExternalMorphWeightData& InExternalMorphWeightData)
{
	if(InComponent)
	{
		Update(
			InLODIndex,
			FSkinnedMeshSceneProxyDynamicData(InComponent),
			InComponent->GetSceneProxy(),
			InComponent->GetSkinnedAsset(),
			InActiveMorphTargets,
			InMorphTargetWeights,
			InPreviousBoneTransformUpdateMode,
			InExternalMorphWeightData);
	}
}

void FSkeletalMeshObjectNanite::Update(int32 InLODIndex, const FSkinnedMeshSceneProxyDynamicData& InDynamicData, const FPrimitiveSceneProxy* InSceneProxy, const USkinnedAsset* InSkinnedAsset, const FMorphTargetWeightMap& InActiveMorphTargets, const TArray<float>& InMorphTargetWeights, EPreviousBoneTransformUpdateMode InPreviousBoneTransformUpdateMode, const FExternalMorphWeightData& InExternalMorphWeightData)
{
	// Create the new dynamic data for use by the rendering thread
	// this data is only deleted when another update is sent
	FDynamicSkelMeshObjectDataNanite* NewDynamicData = new FDynamicSkelMeshObjectDataNanite(InDynamicData, InSkinnedAsset, SkeletalMeshRenderData, InLODIndex, InPreviousBoneTransformUpdateMode, this);

	if (!UpdateHandle.IsValid() || !UpdateHandle.Update(NewDynamicData))
	{
		ENQUEUE_RENDER_COMMAND(SkelMeshObjectUpdateDataCommand)(UE::RenderCommandPipe::SkeletalMesh,
			[this, GPUSkinCache = InSceneProxy ? InSceneProxy->GetScene().GetGPUSkinCache() : nullptr, NewDynamicData](FRHICommandList& RHICmdList)
		{
			FScopeCycleCounter Context(GetStatId());
			UpdateDynamicData_RenderThread(RHICmdList, GPUSkinCache, NewDynamicData);
		});
	}
}

void FSkeletalMeshObjectNanite::ProcessUpdatedDynamicData(FRHICommandList& RHICmdList, FGPUSkinCache* GPUSkinCache)
{
	const int32 RayTracingLODIndex = DynamicData->RayTracingLODIndex;
	const TArray<FSkelMeshRenderSection>& Sections = GetRenderSections(RayTracingLODIndex);
	FSkeletalMeshObjectLOD& LOD = LODs[RayTracingLODIndex];

	const uint32 RevisionNumber = DynamicData->RevisionNumber;
	bool bRecreating = DynamicData->bRecreating;

	for (int32 SectionIdx = 0; SectionIdx < Sections.Num(); SectionIdx++)
	{
		const FSkelMeshRenderSection& Section = Sections[SectionIdx];

		if (Section.MaxBoneInfluences == 0)
		{
			continue;
		}

		FGPUBaseSkinVertexFactory* VertexFactory = LOD.VertexFactories[SectionIdx].Get();
		FGPUBaseSkinVertexFactory::FShaderDataType& ShaderData = VertexFactory->GetShaderData();

		ShaderData.SetRevisionNumbers(RevisionNumber, RevisionNumber);
		ShaderData.UpdatedFrameNumber = DynamicData->BoneTransformFrameNumber;

		{
			const bool bPrevious = false;
			FVertexBufferAndSRV& BoneBuffer = ShaderData.GetBoneBufferForWriting(bPrevious);
			ShaderData.AllocateBoneBuffer(RHICmdList, VertexFactory->GetBoneBufferSize(), BoneBuffer);
		}

		GPUSkinCache->ProcessEntry(
			EGPUSkinCacheEntryMode::RayTracing,
			RHICmdList,
			VertexFactory,
			LOD.PassthroughVertexFactory.Get(),
			Section,
			this,
			nullptr,
			nullptr,
			nullptr,
			FMatrix44f::Identity,
			0.0f,
			(FVector3f)FVector::OneVector,
			RevisionNumber,
			SectionIdx,
			RayTracingLODIndex,
			bRecreating,
			SkinCacheEntryForRayTracing);
	}
}

void FSkeletalMeshObjectNanite::UpdateBoneData(FRHICommandList& RHICmdList)
{
	const int32 RayTracingLODIndex = DynamicData->RayTracingLODIndex;
	const TArray<FSkelMeshRenderSection>& Sections = GetRenderSections(RayTracingLODIndex);
	const FName OwnerName = GetAssetPathName(RayTracingLODIndex);
	FSkeletalMeshObjectLOD& LOD = LODs[RayTracingLODIndex];

	TConstArrayView<FMatrix44f> ReferenceToLocalMatrices = DynamicData->GetReferenceToLocal();

	for (int32 SectionIdx = 0; SectionIdx < Sections.Num(); SectionIdx++)
	{
		const FSkelMeshRenderSection& Section = Sections[SectionIdx];
		FGPUBaseSkinVertexFactory* VertexFactory = LOD.VertexFactories[SectionIdx].Get();
		FGPUBaseSkinVertexFactory::FShaderDataType& ShaderData = VertexFactory->GetShaderData();

		{
			const bool bPrevious = false;
			if (FRHIBuffer* VertexBufferRHI = ShaderData.GetBoneBufferForWriting(bPrevious).VertexBufferRHI)
			{
				ShaderData.UpdateBoneData(RHICmdList, OwnerName, ReferenceToLocalMatrices, Section.BoneMap, VertexBufferRHI);
			}
		}
	}
}

void FSkeletalMeshObjectNanite::UpdateDynamicData_RenderThread(FRHICommandList& RHICmdList, FGPUSkinCache* GPUSkinCache, FDynamicSkelMeshObjectDataNanite* InDynamicData)
{
	delete DynamicData;
	DynamicData = InDynamicData;
	check(DynamicData);

#if RHI_RAYTRACING
	const bool bGPUSkinCacheEnabled = FGPUSkinCache::IsGPUSkinCacheRayTracingSupported() && GPUSkinCache && GEnableGPUSkinCache && IsRayTracingEnabled();

	if (bGPUSkinCacheEnabled && SkeletalMeshRenderData->bSupportRayTracing)
	{
		ProcessUpdatedDynamicData(RHICmdList, GPUSkinCache);
		UpdateBoneData(RHICmdList);
	}
#endif
}

//////////////////////////////////////////////////////////////////////////

const FVertexFactory* FSkeletalMeshObjectNanite::GetSkinVertexFactory(const FSceneView* View, int32 LODIndex, int32 ChunkIdx, ESkinVertexFactoryMode VFMode) const
{
	check(LODs.IsValidIndex(LODIndex));

	if (VFMode == ESkinVertexFactoryMode::RayTracing)
	{
		return LODs[LODIndex].PassthroughVertexFactory.Get();
	}

	return LODs[LODIndex].VertexFactories[ChunkIdx].Get();
}

const FVertexFactory* FSkeletalMeshObjectNanite::GetStaticSkinVertexFactory(int32 LODIndex, int32 ChunkIdx, ESkinVertexFactoryMode VFMode) const
{
	check(LODs.IsValidIndex(LODIndex));

	if (VFMode == ESkinVertexFactoryMode::RayTracing)
	{
		return LODs[LODIndex].PassthroughVertexFactory.Get();
	}

	return LODs[LODIndex].VertexFactories[ChunkIdx].Get();
}

TArray<FTransform>* FSkeletalMeshObjectNanite::GetComponentSpaceTransforms() const
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

const TArray<FMatrix44f>& FSkeletalMeshObjectNanite::GetReferenceToLocalMatrices() const
{
	return DynamicData->ReferenceToLocal;
}

const TArray<FMatrix44f>& FSkeletalMeshObjectNanite::GetPrevReferenceToLocalMatrices() const
{
	return DynamicData->PrevReferenceToLocal;
}

const TArray<FMatrix3x4>* FSkeletalMeshObjectNanite::GetCurrentBoneTransforms() const
{
	return &DynamicData->CurrentBoneTransforms;
}

const TArray<FMatrix3x4>* FSkeletalMeshObjectNanite::GetPreviousBoneTransforms() const
{
	return &DynamicData->PreviousBoneTransforms;
}

int32 FSkeletalMeshObjectNanite::GetLOD() const
{
	// WorkingMinDesiredLODLevel can be a LOD that's not loaded, so need to clamp it to the first loaded LOD
	return FMath::Max<int32>(WorkingMinDesiredLODLevel, SkeletalMeshRenderData->CurrentFirstLODIdx);
	/*if (DynamicData)
	{
		return DynamicData->LODIndex;
	}
	else
	{
		return 0;
	}*/
}

bool FSkeletalMeshObjectNanite::HaveValidDynamicData() const
{
	return (DynamicData != nullptr);
}

void FSkeletalMeshObjectNanite::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
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

void FSkeletalMeshObjectNanite::UpdateSkinWeightBuffer(USkinnedMeshComponent* InComponent)
{
	UpdateSkinWeightBuffer(InComponent->LODInfo);
}

void FSkeletalMeshObjectNanite::UpdateSkinWeightBuffer(const TArrayView<const FSkelMeshComponentLODInfo> InLODInfo)
{
	for (int32 LODIndex = 0; LODIndex < LODs.Num(); ++LODIndex)
	{
		FSkeletalMeshObjectLOD& LOD = LODs[LODIndex];

		// Skip LODs that have their render data stripped
		if (LOD.RenderData->LODRenderData[LODIndex].GetNumVertices() > 0)
		{
			const FSkelMeshComponentLODInfo* UpdateLODInfo = InLODInfo.IsValidIndex(LODIndex) ? &InLODInfo[LODIndex] : nullptr;
			LOD.UpdateSkinWeights(UpdateLODInfo);

			ENQUEUE_RENDER_COMMAND(UpdateSkinCacheSkinWeightBuffer)(UE::RenderCommandPipe::SkeletalMesh,
				[this](FRHICommandList& RHICmdList)
			{
				if (SkinCacheEntryForRayTracing)
				{
					FGPUSkinCache::UpdateSkinWeightBuffer(SkinCacheEntryForRayTracing);
				}
			});
		}
	}
}

void FSkeletalMeshObjectNanite::FSkeletalMeshObjectLOD::InitResources(const FSkelMeshComponentLODInfo* InLODInfo, ERHIFeatureLevel::Type InFeatureLevel)
{
	check(RenderData);
	check(RenderData->LODRenderData.IsValidIndex(LODIndex));

	FSkeletalMeshLODRenderData& LODData = RenderData->LODRenderData[LODIndex];

	// Init vertex factories for ray tracing entry in skin cache
	if (IsRayTracingEnabled())
	{
		MeshObjectWeightBuffer = FSkeletalMeshObject::GetSkinWeightVertexBuffer(LODData, InLODInfo);

		FSkeletalMeshObjectGPUSkin::FVertexFactoryBuffers VertexBuffers;
		VertexBuffers.StaticVertexBuffers = &LODData.StaticVertexBuffers;
		VertexBuffers.ColorVertexBuffer = FSkeletalMeshObject::GetColorVertexBuffer(LODData, InLODInfo);
		VertexBuffers.SkinWeightVertexBuffer = MeshObjectWeightBuffer;
		VertexBuffers.MorphVertexBufferPool = nullptr; // MorphVertexBufferPool;
		VertexBuffers.APEXClothVertexBuffer = &LODData.ClothVertexBuffer;
		VertexBuffers.NumVertices = LODData.GetNumVertices();

		ENQUEUE_RENDER_COMMAND(FSkeletalMeshObjectLOD_InitResources)(UE::RenderCommandPipe::SkeletalMesh,
			[this, &LODData, VertexBuffers = MoveTemp(VertexBuffers), InFeatureLevel](FRHICommandList& RHICmdList)
		{
			VertexFactories.Empty(LODData.RenderSections.Num());

			const bool bUsedForPassthroughVertexFactory = true;
			const FGPUSkinPassthroughVertexFactory::EVertexAttributeFlags VertexAttributeMask = FGPUSkinPassthroughVertexFactory::EVertexAttributeFlags::Position | FGPUSkinPassthroughVertexFactory::EVertexAttributeFlags::Tangent;

			for (const FSkelMeshRenderSection& Section : LODData.RenderSections)
			{
				FSkeletalMeshObjectGPUSkin::CreateVertexFactory(
					RHICmdList,
					VertexFactories,
					&PassthroughVertexFactory,
					VertexBuffers,
					InFeatureLevel,
					VertexAttributeMask,
					Section.BoneMap.Num(),
					Section.BaseVertexIndex,
					bUsedForPassthroughVertexFactory);
			}
		});
	}

	bInitialized = true;
}

void FSkeletalMeshObjectNanite::FSkeletalMeshObjectLOD::ReleaseResources()
{
	bInitialized = false;

	for (auto& VertexFactory : VertexFactories)
	{
		if (VertexFactory)
		{
			VertexFactory->ReleaseResource();
		}
	}

	if (PassthroughVertexFactory)
	{
		PassthroughVertexFactory->ReleaseResource();
	}
}

#if RHI_RAYTRACING
void FSkeletalMeshObjectNanite::UpdateRayTracingGeometry(FRHICommandListBase& RHICmdList, FSkeletalMeshLODRenderData& LODModel, uint32 LODIndex, TArray<FBufferRHIRef>& VertexBuffers)
{
	// TODO: Support WPO
	const bool bAnySegmentUsesWorldPositionOffset = false;

	FSkeletalMeshObjectGPUSkin::UpdateRayTracingGeometry_Internal(LODModel, LODIndex, VertexBuffers, RayTracingGeometry, bAnySegmentUsesWorldPositionOffset, this);
}
#endif

FSkinWeightVertexBuffer* FSkeletalMeshObjectNanite::GetSkinWeightVertexBuffer(int32 LODIndex) const
{
	checkSlow(LODs.IsValidIndex(LODIndex));
	return LODs[LODIndex].MeshObjectWeightBuffer;
}

void FSkeletalMeshObjectNanite::FSkeletalMeshObjectLOD::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
}

void FSkeletalMeshObjectNanite::FSkeletalMeshObjectLOD::UpdateSkinWeights(const FSkelMeshComponentLODInfo* InLODInfo)
{
	check(RenderData);
	check(RenderData->LODRenderData.IsValidIndex(LODIndex));

	FSkeletalMeshLODRenderData& LODData = RenderData->LODRenderData[LODIndex];
	MeshObjectWeightBuffer = FSkeletalMeshObject::GetSkinWeightVertexBuffer(LODData, InLODInfo);
}