// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneStateActorFactory.h"
#include "SceneStateActor.h"
#include "SceneStateBlueprint.h"
#include "SceneStateGeneratedClass.h"
#include "SceneStateObject.h"

#define LOCTEXT_NAMESPACE "SceneStateActorFactory"

USceneStateActorFactory::USceneStateActorFactory()
{
	DisplayName = LOCTEXT("SceneStateDisplayName", "Scene State");
	NewActorClass = ASceneStateActor::StaticClass();
}

bool USceneStateActorFactory::CanCreateActorFrom(const FAssetData& InAssetData, FText& OutErrorMessage)
{
	if (UActorFactory::CanCreateActorFrom(InAssetData, OutErrorMessage))
	{
		return true;
	}

	if (InAssetData.IsValid() && !InAssetData.IsInstanceOf<USceneStateBlueprint>())
	{
		OutErrorMessage = LOCTEXT("InvalidSceneStateAsset", "A valid Scene State asset must be specified.");
		return false;
	}

	return true;
}

void USceneStateActorFactory::PostSpawnActor(UObject* InAsset, AActor* InNewActor)
{
	Super::PostSpawnActor(InAsset, InNewActor);

	ASceneStateActor* SceneStateActor = CastChecked<ASceneStateActor>(InNewActor);
	SceneStateActor->SetSceneStateClass(GetSceneStateClass(InAsset));
}

TSubclassOf<USceneStateObject> USceneStateActorFactory::GetSceneStateClass(UObject* InAsset) const
{
	if (USceneStateBlueprint* Blueprint = Cast<USceneStateBlueprint>(InAsset))
	{
		return Blueprint->GeneratedClass.Get();
	}

	return Cast<USceneStateGeneratedClass>(InAsset);
}

#undef LOCTEXT_NAMESPACE
