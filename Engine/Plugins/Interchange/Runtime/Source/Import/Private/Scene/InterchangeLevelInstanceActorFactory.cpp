// Copyright Epic Games, Inc. All Rights Reserved. 
#include "Scene/InterchangeLevelInstanceActorFactory.h"

#include "Engine/Blueprint.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "InterchangeEditorUtilitiesBase.h"
#include "InterchangeImportLog.h"
#include "InterchangeLevelFactoryNode.h"
#include "InterchangeLevelInstanceActorFactoryNode.h"
#include "InterchangeManager.h"
#include "LevelInstance/LevelInstanceActor.h"
#include "LevelInstance/LevelInstanceSubsystem.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "PackedLevelActor/PackedLevelActor.h"
#include "PackedLevelActor/PackedLevelActorBuilder.h"
#include "Scene/InterchangeActorHelper.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeLevelInstanceActorFactory)

UClass* UInterchangeLevelInstanceActorFactory::GetFactoryClass() const
{
	return ALevelInstance::StaticClass();
}

UObject* UInterchangeLevelInstanceActorFactory::ImportSceneObject_GameThread(const UInterchangeFactoryBase::FImportSceneObjectsParams& CreateSceneObjectsParams)
{
#if !WITH_EDITOR
	return nullptr;
#else

	using namespace UE::Interchange;
	
	const bool bIsReimport = CreateSceneObjectsParams.bIsReimport || CreateSceneObjectsParams.ReimportObject || CreateSceneObjectsParams.ReimportFactoryNode;

	UInterchangeLevelInstanceActorFactoryNode* FactoryNode = Cast<UInterchangeLevelInstanceActorFactoryNode>(CreateSceneObjectsParams.FactoryNode);
	if (!ensure(FactoryNode) || !CreateSceneObjectsParams.NodeContainer)
	{
		return nullptr;
	}
	
	FString ReferenceLevelFactoryNodeUid;
	if (!FactoryNode->GetCustomLevelReference(ReferenceLevelFactoryNodeUid))
	{
		UE_LOG(LogInterchangeImport, Warning, TEXT("UInterchangeLevelInstanceActorFactory::ImportSceneObject_GameThread: Actor level instance do not reference any level factory node."));
		return nullptr;
	}

	UInterchangeLevelFactoryNode* ReferenceLevelFactoryNode = Cast<UInterchangeLevelFactoryNode>(CreateSceneObjectsParams.NodeContainer->GetFactoryNode(ReferenceLevelFactoryNodeUid));
	if (!ReferenceLevelFactoryNode)
	{
		UE_LOG(LogInterchangeImport, Warning, TEXT("UInterchangeLevelInstanceActorFactory::ImportSceneObject_GameThread: Reference Level factory node is invalid."));
		return nullptr;
	}

	FSoftObjectPath ReferenceWorldPath;
	if (!ReferenceLevelFactoryNode->GetCustomReferenceObject(ReferenceWorldPath))
	{
		UE_LOG(LogInterchangeImport, Warning, TEXT("UInterchangeLevelInstanceActorFactory::ImportSceneObject_GameThread: Reference Level factory node do not point on a valid UWorld."));
		return nullptr;
	}

	UWorld* ReferenceWorld = Cast<UWorld>(ReferenceWorldPath.TryLoad());
	if (!ReferenceWorld)
	{
		UE_LOG(LogInterchangeImport, Warning, TEXT("UInterchangeLevelInstanceActorFactory::ImportSceneObject_GameThread: Cannot load a valid Reference UWorld."));
		return nullptr;
	}

	//Reference world must be saved if we want to update the level instance actor
	if (UInterchangeEditorUtilitiesBase* EditorUtilities = UInterchangeManager::GetInterchangeManager().GetEditorUtilities())
	{
		if (!EditorUtilities->SaveAsset(ReferenceWorld))
		{
			UE_LOG(LogInterchangeImport, Warning, TEXT("UInterchangeLevelInstanceActorFactory::ImportSceneObject_GameThread: Cannot save the level instance actor (%s) referenced world (%s)"), *FactoryNode->GetDisplayLabel(), *ReferenceWorld->GetName());
		}
	}

	UWorld* TargetWorld = CreateSceneObjectsParams.Level ? CreateSceneObjectsParams.Level->GetWorld() : GWorld->GetCurrentLevel()->GetWorld();
	ensure (TargetWorld);

	const bool bIsPackedActor = FactoryNode->GetObjectClass()->IsChildOf<APackedLevelActor>();

	FString LevelInstanceActorName = FactoryNode->GetDisplayLabel();

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Name = FName(CreateSceneObjectsParams.ObjectName);
	SpawnParameters.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
	SpawnParameters.OverrideLevel = CreateSceneObjectsParams.Level;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	UClass* LevelInstanceClass = bIsPackedActor ? APackedLevelActor::StaticClass() : ALevelInstance::StaticClass();

	//Reference world must be saved if we want to update the level instance actor
	if (UInterchangeEditorUtilitiesBase* EditorUtilities = UInterchangeManager::GetInterchangeManager().GetEditorUtilities())
	{
		if (!EditorUtilities->SaveAsset(ReferenceWorld))
		{
			UE_LOG(LogInterchangeImport, Warning, TEXT("UInterchangeLevelInstanceActorFactory::ImportSceneObject_GameThread: Cannot save the level instance actor (%s) referenced world (%s)"), *FactoryNode->GetDisplayLabel(), *ReferenceWorld->GetName());
		}
	}

	ALevelInstance* LevelInstanceActor = nullptr;
	//Packed level actor need a blueprint between the reference level and the actor
	if (bIsPackedActor)
	{
		auto FindExisitingAsset = [](const FString& AssetPackagePath, const FString& AssetName)->UObject*
			{
				UPackage* Pkg = FindPackage(nullptr, *AssetPackagePath);
				if (!Pkg)
				{
					//Try to load the package from disk
					Pkg = LoadPackage(nullptr, *AssetPackagePath, LOAD_NoWarn | LOAD_Quiet);
				}
				if (Pkg)
				{
					return StaticFindObject(nullptr, Pkg, *AssetName);
				}
				return nullptr;
			};
		const FString ReferenceWorldPackageDir = FPaths::GetPath(ReferenceWorldPath.GetLongPackageName());
		const FString BPAssetName = FPackedLevelActorBuilder::GetPackedBPPrefix() + ReferenceWorldPath.GetAssetName();
		const FString BPPackageName = FString::Format(TEXT("{0}/{1}"), { ReferenceWorldPackageDir , BPAssetName });
	
		UBlueprint* NewBP = Cast<UBlueprint>(FindExisitingAsset(BPPackageName, BPAssetName));
		TSharedPtr<FPackedLevelActorBuilder> PackedLevelActorBuilder = FPackedLevelActorBuilder::CreateDefaultBuilder();

		if (!NewBP)
		{
			constexpr bool bCompileTrue = true;
			const FString BPAssetPath = FString::Format(TEXT("{0}.{1}"), { BPPackageName , BPAssetName });
			NewBP = PackedLevelActorBuilder->CreatePackedLevelActorBlueprint(TSoftObjectPtr<UBlueprint>(FSoftObjectPath(BPAssetPath)), ReferenceWorld, bCompileTrue);
		}

		if (ensure(NewBP))
		{
			TSharedPtr<UE::Interchange::FImportAsyncHelper, ESPMode::ThreadSafe> AsyncHelper = UInterchangeManager::GetInterchangeManager().GetAsyncHelper(CreateSceneObjectsParams.AsyncHelperUid);
			if (AsyncHelper.IsValid())
			{
				constexpr int32 SourceIndexZero = 0;
				NewBP->SetInternalFlags(EInternalObjectFlags::Async);
				FImportAsyncHelper::FImportedObjectInfo& AssetInfo = AsyncHelper->AddDefaultImportedAssetGetRef(SourceIndexZero);

				//We need a valid factory node to have a valid DefaultImportedAsset entry. Its mandatory to register all created asset
				//Currently we do not have a blueprint factory node we can simply use the base factory node class.
				UInterchangeBaseNodeContainer* MutableNodeContainer = const_cast<UInterchangeBaseNodeContainer*>(CreateSceneObjectsParams.NodeContainer);
				UInterchangeFactoryBaseNode* BpFactoryNode = NewObject<UInterchangeFactoryBaseNode>(MutableNodeContainer);
				FString UniqueId = FGuid::NewGuid().ToString(EGuidFormats::Base36Encoded);
				MutableNodeContainer->SetupNode(BpFactoryNode, UniqueId, NewBP->GetName(), EInterchangeNodeContainerType::FactoryData);
				BpFactoryNode->SetCustomReferenceObject(NewBP);

				AssetInfo.ImportedObject = NewBP;
				AssetInfo.Factory = this;
				AssetInfo.FactoryNode = BpFactoryNode;
				AssetInfo.bIsReimport = false;
			}

			LevelInstanceActor = Cast<ALevelInstance>(CreateSceneObjectsParams.ReimportObject);
			if(!LevelInstanceActor)
			{
				LevelInstanceActor = TargetWorld->SpawnActor<APackedLevelActor>(NewBP->GeneratedClass, SpawnParameters);
			}
		}

		if (!LevelInstanceActor)
		{
			UE_LOG(LogInterchangeImport, Warning, TEXT("UInterchangeLevelInstanceActorFactory::ImportSceneObject_GameThread: Failed to create packed level blueprint."));
			return nullptr;
		}
	}
	else
	{
		LevelInstanceActor = Cast<ALevelInstance>(CreateSceneObjectsParams.ReimportObject);
		if(!LevelInstanceActor)
		{
			//simple Level Instance actor simply need to spawn the actor, the reference level is already setup
			LevelInstanceActor = TargetWorld->SpawnActor<ALevelInstance>(ALevelInstance::StaticClass(), SpawnParameters);
		}
	}

	if (!LevelInstanceActor)
	{
		UE_LOG(LogInterchangeImport, Warning, TEXT("UInterchangeLevelInstanceActorFactory::ImportSceneObject_GameThread: Cannot create/Find a level instance actor in the target world."));
		return nullptr;
	}

	AActor* ParentActor = UE::Interchange::ActorHelper::GetSpawnedParentActor(CreateSceneObjectsParams.NodeContainer, FactoryNode);
	if (ParentActor || !LevelInstanceActor->IsAttachedTo(ParentActor))
	{
		LevelInstanceActor->AttachToActor(ParentActor, FAttachmentTransformRules::KeepWorldTransform);
	}

	FactoryNode->SetCustomReferenceObject(LevelInstanceActor);

	return LevelInstanceActor;
#endif //!WITH_EDITOR
}
