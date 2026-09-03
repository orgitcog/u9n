// Copyright Epic Games, Inc. All Rights Reserved. 

#include "Scene/InterchangeActorHelper.h"

#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "GameFramework/WorldSettings.h"
#include "InterchangeActorFactoryNode.h"
#include "InterchangeAssetUserData.h"
#include "InterchangeImportCommon.h"
#include "InterchangeImportReset.h"
#include "InterchangeMaterialFactoryNode.h"
#include "InterchangeMeshActorFactoryNode.h"
#include "InterchangeMeshNode.h"
#include "InterchangeSceneNode.h"
#include "Nodes/InterchangeBaseNode.h"
#include "Nodes/InterchangeBaseNodeContainer.h"

#include "Components/MeshComponent.h"
#include "Engine/World.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Misc/SecureHash.h"

#if WITH_EDITOR
#include "Editor/EditorEngine.h"
#endif

namespace UE::Interchange::ActorHelper
{
	AActor* GetSpawnedParentActor(const UInterchangeBaseNodeContainer* NodeContainer, const UInterchangeActorFactoryNode* FactoryNode)
	{
		AActor* ParentActor = nullptr;

		if (const UInterchangeFactoryBaseNode* ParentFactoryNode = NodeContainer->GetFactoryNode(FactoryNode->GetParentUid()))
		{
			FSoftObjectPath ReferenceObject;
			ParentFactoryNode->GetCustomReferenceObject(ReferenceObject);
			ParentActor = Cast<AActor>(ReferenceObject.TryLoad());
		}

		return  ParentActor;
	}

	AActor* SpawnFactoryActor(const UInterchangeFactoryBase::FImportSceneObjectsParams& CreateSceneObjectsParams)
	{
		UInterchangeActorFactoryNode* FactoryNode = Cast<UInterchangeActorFactoryNode>(CreateSceneObjectsParams.FactoryNode);
		const UInterchangeBaseNodeContainer* NodeContainer = CreateSceneObjectsParams.NodeContainer;

		if (!FactoryNode || !NodeContainer)
		{
			return nullptr;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = FName(CreateSceneObjectsParams.ObjectName);
		SpawnParameters.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
		SpawnParameters.OverrideLevel = CreateSceneObjectsParams.Level;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AActor* ParentActor = GetSpawnedParentActor(NodeContainer, FactoryNode);
		UWorld* const World = [&SpawnParameters, &ParentActor]()
		{
			UWorld* DefaultWorld = nullptr;

			if (ParentActor)
			{
				DefaultWorld = ParentActor->GetWorld();
			}
#if WITH_EDITOR
			if (DefaultWorld == nullptr)
			{
				UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine);
				if (GIsEditor && EditorEngine != nullptr)
				{
					DefaultWorld = EditorEngine->GetEditorWorldContext().World();
				}
			}
#endif
			if (DefaultWorld == nullptr && GEngine)
			{
				DefaultWorld = GEngine->GetWorld();
			}

			return SpawnParameters.OverrideLevel ? SpawnParameters.OverrideLevel->GetWorld() : DefaultWorld;
		}();

		if (!World)
		{
			return nullptr;
		}

		UClass* ActorClass = FactoryNode->GetObjectClass();
		AActor* SpawnedActor = Cast<AActor>(CreateSceneObjectsParams.ReimportObject);
		if (SpawnedActor)
		{
			if (SpawnedActor->GetClass() == ActorClass)
			{
				// TODO: Check whether parenting is the same
			}
			else
			{
				SpawnedActor = nullptr;
			}
		}
		// The related actor has been deleted. Check on reimport policy
		else if (CreateSceneObjectsParams.ReimportFactoryNode && !FactoryNode->ShouldForceNodeReimport())
		{
			// if reimport policy does not prioritize new content, do not recreate the actor
			return nullptr;
		}

		if (!SpawnedActor)
		{
			SpawnedActor = World->SpawnActor<AActor>(ActorClass, SpawnParameters);
		}

		if (SpawnedActor)
		{
#if WITH_EDITOR
			SpawnedActor->SetActorLabel(FactoryNode->GetDisplayLabel());
#endif
			if (!SpawnedActor->GetRootComponent())
			{
				USceneComponent* RootComponent = NewObject<USceneComponent>(SpawnedActor, USceneComponent::GetDefaultSceneRootVariableName(), RF_Transactional);
#if WITH_EDITORONLY_DATA
				RootComponent->bVisualizeComponent = true;
#endif
				SpawnedActor->SetRootComponent(RootComponent);
				SpawnedActor->AddInstanceComponent(RootComponent);
			}

			if (USceneComponent* RootComponent = SpawnedActor->GetRootComponent())
			{
				uint8 Mobility;
				if (FactoryNode->GetCustomMobility(Mobility))
				{
					//Make sure we don't have a mobility that's more restrictive than our parent mobility, as that wouldn't be a valid setup.
					EComponentMobility::Type TargetMobility = (EComponentMobility::Type)Mobility;

					if (ParentActor && ParentActor->GetRootComponent())
					{
						TargetMobility = (EComponentMobility::Type)FMath::Max((uint8)Mobility, (uint8)ParentActor->GetRootComponent()->Mobility);
					}

					RootComponent->SetMobility(TargetMobility);
				}
			}

			if (ParentActor || !SpawnedActor->IsAttachedTo(ParentActor))
			{
				SpawnedActor->AttachToActor(ParentActor, FAttachmentTransformRules::KeepWorldTransform);
			}
		}

		return SpawnedActor;
	}

	const UInterchangeFactoryBaseNode* FindAssetInstanceFactoryNode(const UInterchangeBaseNodeContainer* NodeContainer, const UInterchangeFactoryBaseNode* ActorFactoryNode)
	{
		TArray<FString> ActorTargetNodes;
		ActorFactoryNode->GetTargetNodeUids(ActorTargetNodes);
		const UInterchangeSceneNode* SceneNode = ActorTargetNodes.IsEmpty() ? nullptr : Cast<UInterchangeSceneNode>(NodeContainer->GetNode(ActorTargetNodes[0]));
		if (!SceneNode)
		{
			return nullptr;
		}

		FString AssetInstanceUid;
		SceneNode->GetCustomAssetInstanceUid(AssetInstanceUid);
		const UInterchangeBaseNode* AssetNode = NodeContainer->GetNode(AssetInstanceUid);
		if (!AssetNode)
		{
			return nullptr;
		}

		TArray<FString> AssetTargetNodeIds;
		AssetNode->GetTargetNodeUids(AssetTargetNodeIds);
		return AssetTargetNodeIds.IsEmpty() ? nullptr : NodeContainer->GetFactoryNode(AssetTargetNodeIds[0]);
	}

	void ApplySlotMaterialDependencies(const UInterchangeBaseNodeContainer& NodeContainer, const UInterchangeMeshActorFactoryNode& MeshActorFactoryNode, UMeshComponent& MeshComponent)
	{
		// Set material slots from imported materials
		TMap<FString, FString> SlotMaterialDependencies;
		MeshActorFactoryNode.GetSlotMaterialDependencies(SlotMaterialDependencies);
		for (TPair<FString, FString>& SlotMaterialDependency : SlotMaterialDependencies)
		{
			const UInterchangeBaseMaterialFactoryNode* MaterialFactoryNode = Cast<UInterchangeBaseMaterialFactoryNode>(NodeContainer.GetNode(SlotMaterialDependency.Value));
			if (!MaterialFactoryNode || !MaterialFactoryNode->IsEnabled())
			{
				continue;
			}
			FSoftObjectPath ReferenceObject;
			MaterialFactoryNode->GetCustomReferenceObject(ReferenceObject);
			if (!ReferenceObject.IsValid())
			{
				continue;
			}

			FName MaterialSlotName = *SlotMaterialDependency.Key;
			UMaterialInterface* MaterialInterface = Cast<UMaterialInterface>(ReferenceObject.TryLoad());
			if (!MaterialInterface)
			{
				MaterialInterface = UMaterial::GetDefaultMaterial(MD_Surface);
			}

			int32 MaterialSlotIndex = MeshComponent.GetMaterialIndex(MaterialSlotName);
			if (MaterialSlotIndex != INDEX_NONE)
			{
				MeshComponent.SetMaterial(MaterialSlotIndex, MaterialInterface);
			}
		}
	}

	void ApplyAllCustomAttributes(const UInterchangeFactoryBase::FImportSceneObjectsParams& CreateSceneObjectsParams, UObject& ObjectToUpdate)
	{
		if (CreateSceneObjectsParams.ReimportObject && (ObjectToUpdate.GetOuter() == CreateSceneObjectsParams.ReimportObject))
		{
				UInterchangeFactoryBaseNode* CurrentNode = UInterchangeFactoryBaseNode::DuplicateWithObject(CreateSceneObjectsParams.FactoryNode, &ObjectToUpdate);

				FFactoryCommon::ApplyReimportStrategyToAsset(&ObjectToUpdate, CreateSceneObjectsParams.ReimportFactoryNode, CurrentNode, CreateSceneObjectsParams.FactoryNode);
		}
		else
		{
			CreateSceneObjectsParams.FactoryNode->ApplyAllCustomAttributeToObject(&ObjectToUpdate);
		}
	}

	void AddInterchangeAssetUserDataToActor(AActor* Actor, const UInterchangeSceneImportAsset* SceneImportAsset, const UInterchangeFactoryBaseNode* FactoryNode)
	{
		if (!Actor || !SceneImportAsset || !FactoryNode)
		{
			return;
		}

		const FSoftObjectPath SceneImportAssetPath((const UObject*)SceneImportAsset);
		if (USceneComponent* RootComponent = Actor->GetRootComponent())
		{
			UInterchangeAssetUserData* AssetUserData = NewObject<UInterchangeAssetUserData>(Actor);
			AssetUserData->MetaData.Add(UE::Interchange::InterchangeReset::Constants::SceneImportAssetPathKey, SceneImportAssetPath.ToString());
			AssetUserData->MetaData.Add(UE::Interchange::InterchangeReset::Constants::FactoryNodeUidPathKey, FactoryNode->GetUniqueID());
			RootComponent->AddAssetUserData(AssetUserData);
		}
	}
	
	void AddInterchangeLevelAssetUserDataToWorld(UWorld* World, const UInterchangeSceneImportAsset* SceneImportAsset)
	{
		if (!World || !SceneImportAsset)
		{
			return;
		}

		const FSoftObjectPath SceneImportAssetPath((const UObject*)SceneImportAsset);
		if (AWorldSettings* WorldSettings = World->GetWorldSettings())
		{
			UInterchangeLevelAssetUserData* AssetUserData = NewObject<UInterchangeLevelAssetUserData>(World);
			AssetUserData->SceneImportPaths.Add(SceneImportAssetPath);
			WorldSettings->AddAssetUserData(AssetUserData);
		}
	}
}