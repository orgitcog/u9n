// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/InstancedSkinnedMeshComponent.h"
#include "InstanceData/InstanceDataManager.h"
#include "InstanceData/InstanceUpdateChangeSet.h"
#include "InstancedSkinnedMeshSceneProxyDesc.h"
#include "SkinnedMeshComponentHelper.h"

/** Helper class used to share implementation for different InstancedSkinnedMeshComponent types */
class FInstancedSkinnedMeshComponentHelper
{
public:
	template <class T, bool bSupportHitProxies = true>
	static FInstanceDataManagerSourceDataDesc GetComponentDesc(T& InComponent, ERHIFeatureLevel::Type FeatureLevel);
	
	template <class T>
	static FBoxSphereBounds CalcBounds(const T& InComponent, const FTransform& LocalToWorld);

	template <class T>
	static FSkeletalMeshObject* CreateMeshObject(const T& InComponent, const FInstancedSkinnedMeshSceneProxyDesc& InSceneProxyDesc);

	template <class T>
	static bool IsEnabled(const T& InComponent);

	template <class T>
	static FPrimitiveSceneProxy* CreateSceneProxy(const T& InComponent, const FInstancedSkinnedMeshSceneProxyDesc& Desc);
};

template <class T, bool bSupportHitProxies>
FInstanceDataManagerSourceDataDesc FInstancedSkinnedMeshComponentHelper::GetComponentDesc(T& InComponent, ERHIFeatureLevel::Type FeatureLevel)
{
	FInstanceDataManagerSourceDataDesc ComponentDesc;


	ComponentDesc.PrimitiveMaterialDesc = FPrimitiveComponentHelper::GetUsedMaterialPropertyDesc(InComponent, FeatureLevel);

	FInstanceDataFlags Flags;
	Flags.bHasPerInstanceRandom = ComponentDesc.PrimitiveMaterialDesc.bAnyMaterialHasPerInstanceRandom;
	Flags.bHasPerInstanceCustomData = ComponentDesc.PrimitiveMaterialDesc.bAnyMaterialHasPerInstanceCustomData && InComponent.NumCustomDataFloats != 0;
#if WITH_EDITOR
	if constexpr (bSupportHitProxies)
	{
		Flags.bHasPerInstanceEditorData = GIsEditor != 0 && InComponent.bHasPerInstanceHitProxies;
	}
#endif

	USkinnedAsset* SkinnedAsset = InComponent.GetSkinnedAsset();
	TArrayView<const struct FAnimBankItem> AnimBankItems = InComponent.GetAnimBankItems();

	const bool bForceRefPose = UInstancedSkinnedMeshComponent::ShouldForceRefPose();
	const bool bUseAnimBank = !bForceRefPose && AnimBankItems.Num() > 0;

	Flags.bHasPerInstanceHierarchyOffset = false;
	Flags.bHasPerInstanceLocalBounds = bUseAnimBank && AnimBankItems.Num() > 1;
	Flags.bHasPerInstanceDynamicData = false;
	Flags.bHasPerInstanceSkinningData = true;


	Flags.bHasPerInstanceLMSMUVBias = false;//IsStaticLightingAllowed();

	ComponentDesc.Flags = Flags;

	// TODO: rename
	ComponentDesc.MeshBounds = SkinnedAsset->GetBounds();
	ComponentDesc.NumCustomDataFloats = InComponent.NumCustomDataFloats;
	ComponentDesc.NumInstances = InComponent.InstanceData.Num();

	ComponentDesc.PrimitiveLocalToWorld = InComponent.GetRenderMatrix();
	ComponentDesc.ComponentMobility = InComponent.GetMobility();

	const FReferenceSkeleton& RefSkeleton = SkinnedAsset->GetRefSkeleton();
	uint32 MaxBoneTransformCount = RefSkeleton.GetRawBoneNum();

	ComponentDesc.BuildChangeSet = [&, MaxBoneTransformCount, MeshBounds = ComponentDesc.MeshBounds](FInstanceUpdateChangeSet& ChangeSet)
	{
		// publish data
		ChangeSet.GetTransformWriter().Gather([&](int32 InstanceIndex) -> FRenderTransform { return FRenderTransform(InComponent.InstanceData[InstanceIndex].Transform.ToMatrixWithScale()); });
		ChangeSet.GetCustomDataWriter().Gather(MakeArrayView(InComponent.InstanceCustomData), InComponent.NumCustomDataFloats);

		ChangeSet.GetSkinningDataWriter().Gather(
			[&](int32 InstanceIndex) -> uint32
			{
				return InComponent.InstanceData[InstanceIndex].BankIndex * MaxBoneTransformCount * 2u;
			});

		ChangeSet.GetLocalBoundsWriter().Gather(
			[&](int32 InstanceIndex) -> FRenderBounds
			{
				uint32 BankIndex = InComponent.InstanceData[InstanceIndex].BankIndex;
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
		if constexpr (bSupportHitProxies)
		{
			if (ChangeSet.Flags.bHasPerInstanceEditorData)
			{
				// TODO: the way hit proxies are managed seems daft, why don't we just add them when needed and store them in an array alonside the instances?
				//       this will always force us to update all the hit proxy data for every instances.
				TArray<TRefCountPtr<HHitProxy>> HitProxies;
				InComponent.CreateHitProxyData(HitProxies);
				ChangeSet.SetEditorData(HitProxies, InComponent.SelectedInstances);
			}
		}
#endif


	};

	return ComponentDesc;
}

template <class T>
FBoxSphereBounds FInstancedSkinnedMeshComponentHelper::CalcBounds(const T& InComponent, const FTransform& LocalToWorld)
{
	const USkinnedAsset* SkinnedAssetPtr = InComponent.GetSkinnedAsset();
	if (SkinnedAssetPtr && InComponent.InstanceData.Num() > 0)
	{
		const FMatrix BoundTransformMatrix = LocalToWorld.ToMatrixWithScale();

		FBoxSphereBounds::Builder BoundsBuilder;
		
		TArrayView<const struct FAnimBankItem> AnimBankItems = InComponent.GetAnimBankItems();
		const bool bUseSampledBounds = UInstancedSkinnedMeshComponent::ShouldUseSampledBounds();
		
		if (bUseSampledBounds && AnimBankItems.Num() > 0)
		{
			// Trade per sequence bounds (tigher fitting) for faster builds with high instance counts.
			const bool bFastBuild = false;
			if (bFastBuild)
			{
				FBox MergedBounds;

				for (const FAnimBankItem& BankItem : AnimBankItems)
				{
					if (BankItem.BankAsset != nullptr)
					{
						const FAnimBankData& BankData = BankItem.BankAsset->GetData();
						for (const FAnimBankEntry& BankEntry : BankData.Entries)
						{
							MergedBounds += BankEntry.SampledBounds.GetBox();
						}
					}
				}

				if (MergedBounds.IsValid)
				{
					for (int32 InstanceIndex = 0; InstanceIndex < InComponent.InstanceData.Num(); InstanceIndex++)
					{
						const FSkinnedMeshInstanceData& Instance = InComponent.InstanceData[InstanceIndex];
						BoundsBuilder += MergedBounds.TransformBy(FTransform(Instance.Transform) * LocalToWorld);
					}
				}
			}
			else
			{
				for (int32 InstanceIndex = 0; InstanceIndex < InComponent.InstanceData.Num(); InstanceIndex++)
				{
					const FSkinnedMeshInstanceData& Instance = InComponent.InstanceData[InstanceIndex];
					if (Instance.BankIndex < uint32(AnimBankItems.Num()))
					{
						const FAnimBankItem& BankItem = AnimBankItems[Instance.BankIndex];
						if (BankItem.BankAsset != nullptr
#if WITH_EDITOR
							&& !BankItem.BankAsset->IsCompiling()
#endif
							)
						{
							const FAnimBankData& BankData = BankItem.BankAsset->GetData();
							if (BankItem.SequenceIndex < BankData.Entries.Num())
							{
								const FBox BankBounds = BankData.Entries[BankItem.SequenceIndex].SampledBounds.GetBox();
								BoundsBuilder += BankBounds.TransformBy(FTransform(Instance.Transform) * LocalToWorld);
							}
						}
					}
				}
			}

			// Only use bounds if valid, else continue with implementation not using AnimBankItems
			if (BoundsBuilder.IsValid())
			{
				return BoundsBuilder;
			}
		}
		
		const FBox InstanceBounds = SkinnedAssetPtr->GetBounds().GetBox();
		if (InstanceBounds.IsValid)
		{
			for (int32 InstanceIndex = 0; InstanceIndex < InComponent.InstanceData.Num(); InstanceIndex++)
			{
				BoundsBuilder += InstanceBounds.TransformBy(FTransform(InComponent.InstanceData[InstanceIndex].Transform) * LocalToWorld);
			}

			return BoundsBuilder;
		}
	}

	return InComponent.CalcMeshBound(FVector3f::ZeroVector, false, LocalToWorld);
}

template <class T>
FSkeletalMeshObject* FInstancedSkinnedMeshComponentHelper::CreateMeshObject(const T& InComponent, const FInstancedSkinnedMeshSceneProxyDesc& InSceneProxyDesc)
{
	return UInstancedSkinnedMeshComponent::CreateMeshObject(InSceneProxyDesc, InComponent.GetAnimBankItems(), FSkinnedMeshComponentHelper::GetSkeletalMeshRenderData(InComponent), InComponent.GetScene()->GetFeatureLevel());
}

template <class T>
bool FInstancedSkinnedMeshComponentHelper::IsEnabled(const T& InComponent)
{
	USkeletalMesh* SkeletalMeshPtr = Cast<USkeletalMesh>(InComponent.GetSkinnedAsset());
	if (SkeletalMeshPtr && SkeletalMeshPtr->GetResourceForRendering())
	{
		return InComponent.GetInstanceCount() > 0;
	}

	return false;
}

template <class T>
FPrimitiveSceneProxy* FInstancedSkinnedMeshComponentHelper::CreateSceneProxy(const T& InComponent, const FInstancedSkinnedMeshSceneProxyDesc& InSceneProxyDesc)
{
	const int32 MinLODIndex = FSkinnedMeshComponentHelper::ComputeMinLOD(InComponent);
	const bool bShouldNaniteSkin = FSkinnedMeshComponentHelper::ShouldNaniteSkin(InComponent);
	const bool bEnabled = FInstancedSkinnedMeshComponentHelper::IsEnabled(InComponent);
	return UInstancedSkinnedMeshComponent::CreateSceneProxy(InSceneProxyDesc, InComponent.bHideSkin, bShouldNaniteSkin, bEnabled, MinLODIndex);
}