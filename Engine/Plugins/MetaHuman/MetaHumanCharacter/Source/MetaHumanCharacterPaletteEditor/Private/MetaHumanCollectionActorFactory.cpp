// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCollectionActorFactory.h"

#include "AssetRegistry/AssetData.h"
#include "MetaHumanCharacterActorInterface.h"
#include "MetaHumanCharacterInstance.h"
#include "MetaHumanCollection.h"
#include "MetaHumanCollectionEditorPipeline.h"
#include "MetaHumanCollectionPipeline.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "MetaHumanCollectionActorFactory"

UMetaHumanCollectionActorFactory::UMetaHumanCollectionActorFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
    DisplayName = LOCTEXT("MetaHumanCollectionDisplayName", "MetaHuman Collection");
}

UClass* UMetaHumanCollectionActorFactory::GetDefaultActorClass(const FAssetData& AssetData)
{
	UMetaHumanCollection* Collection = Cast<UMetaHumanCollection>(AssetData.GetAsset());
	if (!Collection)
	{
		if (UMetaHumanCharacterInstance* CharacterInstance = Cast<UMetaHumanCharacterInstance>(AssetData.GetAsset()))
		{
			Collection = CharacterInstance->GetMetaHumanCollection();
		}
	}

	if (Collection
		&& Collection->GetPipeline())
	{
		return Collection->GetPipeline()->GetActorClass();
	}
	else
	{
		return nullptr;
	}
}

void UMetaHumanCollectionActorFactory::PostSpawnActor(UObject* Asset, AActor* NewActor)
{
	Super::PostSpawnActor(Asset, NewActor);

	check(NewActor->Implements<UMetaHumanCharacterActorInterface>());

	UMetaHumanCharacterInstance* CharacterInstance = Cast<UMetaHumanCharacterInstance>(Asset);
	if (!CharacterInstance)
	{
		UMetaHumanCollection* Collection = CastChecked<UMetaHumanCollection>(Asset);
		CharacterInstance = Collection->GetMutableDefaultInstance();
	}

	// If CanCreateActorFrom returned true, there must be a valid Character Instance reachable
	// from this asset.
	check(CharacterInstance);

	IMetaHumanCharacterActorInterface::Execute_SetCharacterInstance(NewActor, CharacterInstance);
}

UObject* UMetaHumanCollectionActorFactory::GetAssetFromActorInstance(AActor* ActorInstance)
{
	// TODO: Find out if this function is worth implementing
	return nullptr;
}

bool UMetaHumanCollectionActorFactory::CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg)
{
	if (!AssetData.IsValid())
	{
		return false;
	}

	UMetaHumanCollection* Collection = Cast<UMetaHumanCollection>(AssetData.GetAsset());
	if (!Collection)
	{
		if (UMetaHumanCharacterInstance* CharacterInstance = Cast<UMetaHumanCharacterInstance>(AssetData.GetAsset()))
		{
			Collection = CharacterInstance->GetMetaHumanCollection();
		}
	}

	if (!Collection)
	{
        OutErrorMsg = LOCTEXT("NoValidAsset", "A valid MetaHuman Collection or Instance must be specified");
		return false;
	}

	if (!Collection->GetPipeline())
	{
        OutErrorMsg = LOCTEXT("NoValidPipeline", "The MetaHuman Collection doesn't have an associated Character Pipeline");
		return false;
	}

	if (!Collection->GetPipeline()->GetActorClass())
	{
        OutErrorMsg = LOCTEXT("NoActor", "The Character Pipeline doesn't specify a type of actor to spawn");
		return false;
	}

	if (!Collection->GetPipeline()->GetActorClass()->ImplementsInterface(UMetaHumanCharacterActorInterface::StaticClass()))
	{
        OutErrorMsg = LOCTEXT("NoActorInterface", "The Character Pipeline's actor doesn't implement IMetaHumanCharacterActorInterface");
		return false;
	}

    return true;
}

#undef LOCTEXT_NAMESPACE
