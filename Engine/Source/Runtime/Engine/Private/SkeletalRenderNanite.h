// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProfilingDebugging/ResourceSize.h"
#include "RenderResource.h"
#include "RayTracingGeometry.h"
#include "ShaderParameters.h"
#include "Components/ExternalMorphSet.h"
#include "Components/SkinnedMeshComponent.h"
#include "GlobalShader.h"
#include "SkeletalMeshUpdater.h"
#include "SkeletalRenderPublic.h"
#include "ClothingSystemRuntimeTypes.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Animation/MeshDeformerGeometry.h"
#include "NaniteSceneProxy.h"

class FPrimitiveDrawInterface;
class UMorphTarget;
class FSkeletalMeshObjectNanite;

/** 
* Stores the updated matrices needed to skin the verts.
* Created by the game thread and sent to the rendering thread as an update 
*/
class FDynamicSkelMeshObjectDataNanite final : public FSkeletalMeshDynamicData
{
public:
	FDynamicSkelMeshObjectDataNanite(
		USkinnedMeshComponent* InComponent,
		FSkeletalMeshRenderData* InRenderData,
		int32 InLODIndex,
		EPreviousBoneTransformUpdateMode InPreviousBoneTransformUpdateMode,
		FSkeletalMeshObjectNanite* InMeshObject
	);

	FDynamicSkelMeshObjectDataNanite(
		const FSkinnedMeshSceneProxyDynamicData& InDynamicData,
		const USkinnedAsset* InSkinnedAsset,
		FSkeletalMeshRenderData* InRenderData,
		int32 InLODIndex,
		EPreviousBoneTransformUpdateMode InPreviousBoneTransformUpdateMode,
		FSkeletalMeshObjectNanite* InMeshObject
	);

	virtual ~FDynamicSkelMeshObjectDataNanite();

	// Current reference pose to local space transforms
	TArray<FMatrix44f> ReferenceToLocal;
	TArray<FMatrix44f> ReferenceToLocalForRayTracing;

	// Previous reference pose to local space transforms
	TArray<FMatrix44f> PrevReferenceToLocal;
	TArray<FMatrix44f> PrevReferenceToLocalForRayTracing;

	TConstArrayView<FMatrix44f> GetPrevReferenceToLocal() const
	{
		return RayTracingLODIndex != LODIndex ? PrevReferenceToLocalForRayTracing : PrevReferenceToLocal;
	}

	TConstArrayView<FMatrix44f> GetReferenceToLocal() const
	{
		return RayTracingLODIndex != LODIndex ? ReferenceToLocalForRayTracing : ReferenceToLocal;
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) 
	// Component space bone transforms
	TArray<FTransform> ComponentSpaceTransforms;
#endif

	TArray<FMatrix3x4> CurrentBoneTransforms;
	TArray<FMatrix3x4> PreviousBoneTransforms;

	uint32 BoneTransformFrameNumber;
	uint32 RevisionNumber;
	uint32 PreviousRevisionNumber;
	uint8 bRecreating : 1;

	// Current LOD for bones being updated
	int32 LODIndex;
	int32 RayTracingLODIndex;

	// Returns the size of memory allocated by render data
	void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize);

private:

	enum class ETransformsToUpdate
	{
		Current,
		Previous,
	};
	
	void UpdateBonesRemovedByLOD(
		TArray<FMatrix44f>& PoseBuffer,
		const FSkinnedMeshSceneProxyDynamicData& InDynamicData,
		const USkinnedAsset* InSkinnedAsset,
		ETransformsToUpdate TransformsToUpdate) const;
};

class FSkeletalMeshObjectNanite final : public FSkeletalMeshObject
{
public:
	FSkeletalMeshObjectNanite(USkinnedMeshComponent* InComponent, FSkeletalMeshRenderData* InRenderData, ERHIFeatureLevel::Type InFeatureLevel);
	FSkeletalMeshObjectNanite(const FSkinnedMeshSceneProxyDesc& InMeshComponent, FSkeletalMeshRenderData* InSkelMeshRenderData, ERHIFeatureLevel::Type InFeatureLevel);
	virtual ~FSkeletalMeshObjectNanite();

	virtual void InitResources(const FSkinnedMeshSceneProxyDesc& InMeshDesc) override;
	virtual void ReleaseResources() override;
	
	void Update(
		int32 LODIndex,
		USkinnedMeshComponent* InComponent,
		const FMorphTargetWeightMap& InActiveMorphTargets,
		const TArray<float>& MorphTargetWeights,
		EPreviousBoneTransformUpdateMode PreviousBoneTransformUpdateMode,
		const FExternalMorphWeightData& InExternalMorphWeightData);
	
	virtual void Update(int32 LODIndex, const FSkinnedMeshSceneProxyDynamicData& InSkeletalMeshDynamicData, const FPrimitiveSceneProxy* InSceneProxy, const USkinnedAsset* InSkinnedAsset, const FMorphTargetWeightMap& InActiveMorphTargets, const TArray<float>& InMorphTargetWeights, EPreviousBoneTransformUpdateMode PreviousBoneTransformUpdateMode, const FExternalMorphWeightData& InExternalMorphWeightData);

	void UpdateDynamicData_RenderThread(FRHICommandList& RHICmdList, FGPUSkinCache* GPUSkinCache, FDynamicSkelMeshObjectDataNanite* InDynamicData);

	virtual const FVertexFactory* GetSkinVertexFactory(const FSceneView* View, int32 LODIndex, int32 ChunkIdx, ESkinVertexFactoryMode VFMode = ESkinVertexFactoryMode::Default) const override;
	virtual const FVertexFactory* GetStaticSkinVertexFactory(int32 LODIndex, int32 ChunkIdx, ESkinVertexFactoryMode VFMode = ESkinVertexFactoryMode::Default) const override;
	virtual TArray<FTransform>* GetComponentSpaceTransforms() const override;
	virtual const TArray<FMatrix44f>& GetReferenceToLocalMatrices() const override;
	virtual const TArray<FMatrix44f>& GetPrevReferenceToLocalMatrices() const override;
	virtual const TArray<FMatrix3x4>* GetCurrentBoneTransforms() const override;
	virtual const TArray<FMatrix3x4>* GetPreviousBoneTransforms() const override;

	virtual int32 GetLOD() const override;

	virtual bool HaveValidDynamicData() const override;

	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;

	void UpdateSkinWeightBuffer(USkinnedMeshComponent* InMeshComponent);
	virtual void UpdateSkinWeightBuffer(const TArrayView<const FSkelMeshComponentLODInfo> InLODInfo) override;

	virtual bool IsNaniteMesh() const override { return true; }

	virtual FSkinWeightVertexBuffer* GetSkinWeightVertexBuffer(int32 LODIndex) const;

#if RHI_RAYTRACING	
	FRayTracingGeometry RayTracingGeometry;
	
	virtual void UpdateRayTracingGeometry(FRHICommandListBase& RHICmdList, FSkeletalMeshLODRenderData& LODModel, uint32 LODIndex, TArray<FBufferRHIRef>& VertexBuffers) override;
	
	// GetRayTracingGeometry()->IsInitialized() is checked as a workaround for UE-92634. FSkeletalMeshSceneProxy's resources may have already been released, but proxy has not removed yet)
	FRayTracingGeometry* GetRayTracingGeometry() { return RayTracingGeometry.HasValidInitializer() && RayTracingGeometry.IsInitialized() ? & RayTracingGeometry : nullptr; }
	const FRayTracingGeometry* GetRayTracingGeometry() const { return RayTracingGeometry.HasValidInitializer() && RayTracingGeometry.IsInitialized() ? & RayTracingGeometry : nullptr; }

	virtual int32 GetRayTracingLOD() const override
	{
		if (DynamicData)
		{
			return DynamicData->RayTracingLODIndex;
		}
		else
		{
			return 0;
		}
	}
#endif

	inline bool HasValidMaterials() const
	{
		return bHasValidMaterials;
	}

	inline const Nanite::FMaterialAudit& GetMaterials() const
	{
		return NaniteMaterials;
	}

private:
	FDynamicSkelMeshObjectDataNanite* DynamicData = nullptr;

	void ProcessUpdatedDynamicData(FRHICommandList& RHICmdList, FGPUSkinCache* GPUSkinCache);
	void UpdateBoneData(FRHICommandList& RHICmdList);

	struct FSkeletalMeshObjectLOD
	{
		FSkeletalMeshRenderData* RenderData;
		int32 LODIndex;
		bool bInitialized;
		
		// Needed for skin cache update for ray tracing
		TArray<TUniquePtr<FGPUBaseSkinVertexFactory>> VertexFactories;
		TUniquePtr<FGPUSkinPassthroughVertexFactory> PassthroughVertexFactory;

		FSkinWeightVertexBuffer* MeshObjectWeightBuffer = nullptr;

		FSkeletalMeshObjectLOD(ERHIFeatureLevel::Type InFeatureLevel, FSkeletalMeshRenderData* InRenderData, int32 InLOD)
		: RenderData(InRenderData)
		, LODIndex(InLOD)
		, bInitialized(false)
		{
		}

		void InitResources(const FSkelMeshComponentLODInfo* LODInfo, ERHIFeatureLevel::Type FeatureLevel);
		void ReleaseResources();
		void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize);
		void UpdateSkinWeights(const FSkelMeshComponentLODInfo* LODInfo);
	};

	TArray<FSkeletalMeshObjectLOD> LODs;

	FSkeletalMeshUpdateHandle UpdateHandle;

	Nanite::FMaterialAudit NaniteMaterials;
	bool bHasValidMaterials = false;

	mutable int32 CachedLOD;

	friend class FSkeletalMeshUpdatePacketNanite;
};