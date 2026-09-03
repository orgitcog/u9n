// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rendering/NaniteResourcesHelper.h"
#include "SkinnedMeshSceneProxyDesc.h"
#include "Components/SkinnedMeshComponent.h"
#include "InstancedSkinnedMeshSceneProxyDesc.h"
#include "Engine/SkinnedAsset.h"

namespace Nanite
{
	namespace Private
	{
		template<>
		FString GetMaterialMeshName<USkinnedMeshComponent>(const USkinnedMeshComponent& Object)
		{
			return Object.GetSkinnedAsset()->GetName();
		}

		template<>
		FString GetMaterialMeshName<FSkinnedMeshSceneProxyDesc>(const FSkinnedMeshSceneProxyDesc& Object)
		{
			return Object.GetSkinnedAsset()->GetName();
		}

		template<>
		FString GetMaterialMeshName<FInstancedSkinnedMeshSceneProxyDesc>(const FInstancedSkinnedMeshSceneProxyDesc& Object)
		{
			return Object.GetSkinnedAsset()->GetName();
		}

		template<>
		bool IsMaterialSkeletalMesh<USkinnedMeshComponent>(const USkinnedMeshComponent& Object)
		{
			return true;
		}

		template<>
		bool IsMaterialSkeletalMesh<FSkinnedMeshSceneProxyDesc>(const FSkinnedMeshSceneProxyDesc& Object)
		{
			return true;
		}

		template<>
		bool IsMaterialSkeletalMesh<FInstancedSkinnedMeshSceneProxyDesc>(const FInstancedSkinnedMeshSceneProxyDesc& Object)
		{
			return true;
		}

		template<>
		TArray<FAuditMaterialSlotInfo, TInlineAllocator<32>> GetMaterialSlotInfos<USkinnedMeshComponent>(const USkinnedMeshComponent& Object)
		{
			TArray<FAuditMaterialSlotInfo, TInlineAllocator<32>> Infos;

			if (const USkinnedAsset* SkinnedAsset = Object.GetSkinnedAsset())
			{
				const TArray<FSkeletalMaterial>& Materials = SkinnedAsset->GetMaterials();
				for (int32 Index = 0; Index < Materials.Num(); ++Index)
				{
					const FSkeletalMaterial& Material = Materials[Index];
					Infos.Add({ Material.MaterialInterface, Material.MaterialSlotName, Material.UVChannelData });
				}
			}

			return Infos;
		}

		template<>
		TArray<FAuditMaterialSlotInfo, TInlineAllocator<32>> GetMaterialSlotInfos<FSkinnedMeshSceneProxyDesc>(const FSkinnedMeshSceneProxyDesc& Object)
		{
			TArray<FAuditMaterialSlotInfo, TInlineAllocator<32>> Infos;
			if (const USkinnedAsset* SkinnedAsset = Object.GetSkinnedAsset())
			{
				const TArray<FSkeletalMaterial>& Materials = SkinnedAsset->GetMaterials();
				for (int32 Index = 0; Index < Materials.Num(); ++Index)
				{
					const FSkeletalMaterial& Material = Materials[Index];
					Infos.Add({ Material.MaterialInterface, Material.MaterialSlotName, Material.UVChannelData });
				}
			}

			return Infos;
		}

		template<>
		TArray<FAuditMaterialSlotInfo, TInlineAllocator<32>> GetMaterialSlotInfos<FInstancedSkinnedMeshSceneProxyDesc>(const FInstancedSkinnedMeshSceneProxyDesc& Object)
		{
			return GetMaterialSlotInfos<FSkinnedMeshSceneProxyDesc>(Object);
		}
	}
}