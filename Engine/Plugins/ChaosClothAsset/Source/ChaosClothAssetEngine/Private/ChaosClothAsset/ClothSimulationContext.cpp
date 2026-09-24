// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/ClothSimulationContext.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "ChaosClothAsset/ClothComponent.h"
#include "ChaosClothAsset/ClothSimulationModel.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "SceneInterface.h"

namespace UE::Chaos::ClothAsset
{
	void FClothSimulationContext::Fill(const UChaosClothComponent& ClothComponent, float InDeltaTime, float MaxDeltaTime, bool bIsInitialization, FClothingSimulationCacheData* InCacheData)
	{
		// Set the time
		DeltaTime = FMath::Min(InDeltaTime, MaxDeltaTime);

		// Set the current LOD index
		LodIndex = ClothComponent.GetPredictedLODLevel();

		// Set the teleport mode
		static const IConsoleVariable* const CVarMaxDeltaTimeTeleportMultiplier = IConsoleManager::Get().FindConsoleVariable(TEXT("p.Cloth.MaxDeltaTimeTeleportMultiplier"));
		constexpr float MaxDeltaTimeTeleportMultiplierDefault = 1.5f;
		const float MaxDeltaTimeTeleportMultiplier = CVarMaxDeltaTimeTeleportMultiplier ? CVarMaxDeltaTimeTeleportMultiplier->GetFloat() : MaxDeltaTimeTeleportMultiplierDefault;

		const EClothingTeleportMode ComponentTeleportMode = ClothComponent.GetClothTeleportMode();

		bTeleport = (InDeltaTime > MaxDeltaTime * MaxDeltaTimeTeleportMultiplier) ? true : 
			(ComponentTeleportMode >= EClothingTeleportMode::Teleport);
		bReset = ComponentTeleportMode == EClothingTeleportMode::TeleportAndReset;

		VelocityScale = (!bTeleport && !bReset && InDeltaTime > 0.f) ?
			FMath::Min(InDeltaTime, MaxDeltaTime) / InDeltaTime :
			bReset ? 1.f : 0.f;  // Set to 0 when teleporting and 1 when resetting to match the internal solver's behavior

		// Copy component transform
		ComponentTransform = ClothComponent.GetComponentTransform();

		// Copy solver geometry scale
		SolverGeometryScale = ClothComponent.GetClothGeometryScale();;

		// Update bone transforms
		const UChaosClothAssetBase* const Asset = ClothComponent.GetAsset();
		const FReferenceSkeleton* const ReferenceSkeleton = Asset ? &Asset->GetRefSkeleton() : nullptr;
		int32 NumBones = ReferenceSkeleton ? ReferenceSkeleton->GetNum() : 0;

		if (USkinnedMeshComponent* const LeaderComponent = ClothComponent.LeaderPoseComponent.Get())
		{
			const TArray<int32>& LeaderBoneMap = ClothComponent.GetLeaderBoneMap();
			if (!LeaderBoneMap.Num())
			{
				// This case indicates an invalid leader pose component (e.g. no skeletal mesh)
				BoneTransforms.Empty(NumBones);
				BoneTransforms.AddDefaulted(NumBones);
			}
			else
			{
				NumBones = LeaderBoneMap.Num();
				BoneTransforms.Reset(NumBones);
				BoneTransforms.AddDefaulted(NumBones);

				if (!bIsInitialization)  // Initializations must be done in bind pose
				{
					const TArray<FTransform>& LeaderTransforms = LeaderComponent->GetComponentSpaceTransforms();
					for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
					{
						bool bFoundLeader = false;
						if (LeaderBoneMap.IsValidIndex(BoneIndex))
						{
							const int32 LeaderIndex = LeaderBoneMap[BoneIndex];
							if (LeaderIndex != INDEX_NONE && LeaderIndex < LeaderTransforms.Num())
							{
								BoneTransforms[BoneIndex] = LeaderTransforms[LeaderIndex];
								bFoundLeader = true;
							}
						}

						if (!bFoundLeader && ReferenceSkeleton)
						{
							const int32 ParentIndex = ReferenceSkeleton->GetParentIndex(BoneIndex);

							BoneTransforms[BoneIndex] =
								BoneTransforms.IsValidIndex(ParentIndex) && ParentIndex < BoneIndex ?
								BoneTransforms[ParentIndex] * ReferenceSkeleton->GetRefBonePose()[BoneIndex] :
								ReferenceSkeleton->GetRefBonePose()[BoneIndex];
						}
					}
				}
			}
		}
		else if (!bIsInitialization)  // Initializations must be done in bind pose
		{
			BoneTransforms = ClothComponent.GetComponentSpaceTransforms();
		}
		else
		{
			BoneTransforms.Reset(NumBones);
			BoneTransforms.AddDefaulted(NumBones);
		}

		// Update bone matrices
		RefToLocalMatrices.Reset(NumBones);
		RequiredExtraBones.Reset();

		bool bSetRefToLocalMatricesToIdentity = true;
		if (!bIsInitialization)
		{
			if (const FSkeletalMeshRenderData* const RenderData = Asset ? Asset->GetResourceForRendering() : nullptr)
			{
				if (RenderData->LODRenderData.IsValidIndex(LodIndex))  // TODO Disable this conditional to drive skeletal meshes
				{
					for (int32 ModelIndex = 0; ModelIndex < Asset->GetNumClothSimulationModels(); ++ModelIndex)
					{
						const TSharedPtr<const FChaosClothSimulationModel>& ClothModel = Asset->GetClothSimulationModel(ModelIndex);
						if (ClothModel && ClothModel->IsValidLodIndex(LodIndex))
						{
							RequiredExtraBones.Append(ClothModel->ClothSimulationLodModels[LodIndex].RequiredExtraBoneIndices);
						}
					}
					ClothComponent.GetCurrentRefToLocalMatrices(RefToLocalMatrices, LodIndex, &RequiredExtraBones);
					bSetRefToLocalMatricesToIdentity = false;
				}
			}
		}

		if (bSetRefToLocalMatricesToIdentity)
		{
			RefToLocalMatrices.AddUninitialized(NumBones);

			for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
			{
				RefToLocalMatrices[BoneIndex] = FMatrix44f::Identity;
			}
		}
		
		// Update gravity
		const UWorld* const World = ClothComponent.GetWorld();
		constexpr float EarthGravity = -981.f;
		WorldGravity = FVector(0.f, 0.f, World ? World->GetGravityZ() : EarthGravity);

		// Update wind velocity
		if (World && World->Scene && World->IsGameWorld())
		{
			const FVector Position = ClothComponent.GetComponentTransform().GetTranslation();

			float WindSpeed;
			float WindMinGust;
			float WindMaxGust;
			World->Scene->GetWindParameters_GameThread(Position, WindVelocity, WindSpeed, WindMinGust, WindMaxGust);

			WindVelocity *= WindSpeed;
		}
		else
		{
			WindVelocity = FVector::ZeroVector;
		}

		if (InCacheData)
		{
			CacheData = MoveTemp(*InCacheData);
		}
		else
		{
			CacheData.Reset();
		}
	}
}  // End namespace UE::Chaos::ClothAsset
