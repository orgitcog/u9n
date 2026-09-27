// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/SoftObjectPtr.h"
#include "UObject/NameTypes.h"
#include "Engine/DataAsset.h"

#include "MetaHumanMigrationDatabase.generated.h"

UENUM()
enum class EMetaHumanMigrationDataAssetType : uint8
{
	Hair,
	Eyebrows,
	Eyelashes,
	Beard,
	Mustache,
	Peachfuzz,
};

/**
 * Contains assets that are available for migration.
 */
UCLASS(Blueprintable, BlueprintType)
class UMetaHumanMigrationAssetCollection : public UDataAsset
{
	GENERATED_BODY()

public:
	// Maps original MHC identifier against the wardrobe item.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Mapping")
	TMap<FName, TSoftObjectPtr<class UMetaHumanWardrobeItem>> GroomAssetMapping;
};

/**
 * Contains all assets collections that will be used for migration.
 */
UCLASS(Blueprintable, BlueprintType)
class UMetaHumanMigrationDatabase : public UDataAsset
{
	GENERATED_BODY()

public:
	// Maps migration asset type against the collection object
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Assets")
	TMap<EMetaHumanMigrationDataAssetType, TObjectPtr<UMetaHumanMigrationAssetCollection>> Assets;
};
