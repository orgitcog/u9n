// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"

#include "MetaHumanSDKSettings.generated.h"

/**
 * Project Settings for the MetaHuman SDK
 */
UCLASS(defaultconfig, config = MetaHumanSDK, meta = (DisplayName = "MetaHuman SDK"))
class METAHUMANSDKEDITOR_API UMetaHumanSDKSettings : public UObject
{
	GENERATED_BODY()

public:
	// The URL for fetching version information and release notes
	UPROPERTY(Config)
	FString VersionServiceBaseUrl;

	// The asset path for importing Cinematic MetaHumans.
	UPROPERTY(EditAnywhere, Config, Category = "MetaHuman Import Paths", meta = (ContentDir, DisplayName = "Cinematic Characters"))
	FDirectoryPath CinematicImportPath{TEXT("/Game/MetaHumans")};

	// The asset path for importing Optimized MetaHumans.
	UPROPERTY(EditAnywhere, Config, Category = "MetaHuman Import Paths", meta = (ContentDir, DisplayName = "Optimized Characters"))
	FDirectoryPath OptimizedImportPath{TEXT("/Game/MetaHumans")};

	// The asset path for finding Editable MetaHuman Character Assets for Packaging.
	UPROPERTY(EditAnywhere, Config, Category = "MetaHuman Packaging Paths", meta = (ContentDir, DisplayName = "Editable Characters"))
	FDirectoryPath CharacterAssetPackagingPath{TEXT("/Game/EditableCharacters")};

	// The asset path for finding MetaHuman Character Assemblies for Packaging.
	UPROPERTY(EditAnywhere, Config, Category = "MetaHuman Packaging Paths", meta = (ContentDir, DisplayName = "Character Assemblies"))
	FDirectoryPath CharacterAssemblyPackagingPath{TEXT("/Game/CharacterAssemblies")};

	// The asset path for finding MetaHuman Skeletal Clothing Assets for Packaging.
	UPROPERTY(EditAnywhere, Config, Category = "MetaHuman Packaging Paths", meta = (ContentDir, DisplayName = "Skeletal Clothing"))
	FDirectoryPath SkeletalClothingPackagingPath{TEXT("/Game/SkeletalClothing")};

	// The asset path for finding MetaHuman Outfit-based Clothing Assets for Packaging.
	UPROPERTY(EditAnywhere, Config, Category = "MetaHuman Packaging Paths", meta = (ContentDir, DisplayName = "Outfits"))
	FDirectoryPath OutfitPackagingPath{TEXT("/Game/Outfits")};

	// The asset path for finding MetaHuman Groom Assets for Packaging.
	UPROPERTY(EditAnywhere, Config, Category = "MetaHuman Packaging Paths", meta = (ContentDir, DisplayName = "Grooms"))
	FDirectoryPath GroomPackagingPath{TEXT("/Game/Grooms")};
};
