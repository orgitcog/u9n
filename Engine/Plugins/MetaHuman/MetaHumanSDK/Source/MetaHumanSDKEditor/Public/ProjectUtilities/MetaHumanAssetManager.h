// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MetaHumanTypes.h"

#include "AssetRegistry/AssetData.h"
#include "Async/Future.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "MetaHumanAssetManager.generated.h"

class UMetaHumanAssetReport;
class USkeleton;
class FZipArchiveWriter;

/**
 * Options to apply during the import of a MetaHuman Asset Archive
 */
USTRUCT(BlueprintType)
struct METAHUMANSDKEDITOR_API FMetaHumanImportOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ImportOptions")
	bool bVerbose = false;

	/**
	 * Ignores version information and always replaces files in the project with files in the imported archive
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ImportOptions")
	bool bForceUpdate = false;
};


/**
 * Describes the types of MetaHuman Assets that can be managed and packaged
 */
UENUM()
enum class EMetaHumanAssetType : uint8
{
	Character,
	CharacterAssembly,
	SkeletalClothing,
	OutfitClothing,
	Groom
};


/**
 * Details about the assets contained in a MetaHuman Package
 */
USTRUCT(BlueprintType)
struct METAHUMANSDKEDITOR_API FMetaHumanAggregateDetails
{
	GENERATED_BODY()

	// From the spec found here: https://docs.google.com/spreadsheets/d/1coHD8hnr4lxpDCfPCGCkCkZyffWCh3px83rAHTGXYTs/edit?gid=1418369635#gid=1418369635
	// Clothing
	/**
	 * Clothing will resize to blendable bodies in UEMHC
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AssetDescription")
	bool bResizesWithBlendableBodies = false;

	/**
	 * Clothing has a mask for hidden face removal in UEMHC
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AssetDescription")
	bool bHasClothingMask = false;

	/**
	 * Which LODs are included for this item
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AssetDescription")
	int32 IncludedLods = 0;

	/**
	 * Vert Count for LOD0 (if single item in listing)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AssetDescription")
	int32 Lod0VertCount = 0;

	/**
	 * Number of clothing items
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AssetDescription")
	int32 NumUniqueClothingItems = 0;

	// Characters

	/**
	 * Does this character contain one or more grooms
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AssetDescription")
	bool bContainsGrooms = false;

	/**
	 * Does this character come with clothing
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AssetDescription")
	bool bContainsClothing = false;

	/**
	 * Is this a character a user can open up in UEMHC and edit?
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AssetDescription")
	bool bIsEditableCharacter = false;

	/**
	 * Cinematic and/or Optimized
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AssetDescription")
	TArray<EMetaHumanQualityLevel> PlatformsIncluded;

	/**
	 * How many Characters are included in this listing
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AssetDescription")
	int32 NumUniqueCharacters = 0;

	// Grooms
	/**
	 * Number of grooms
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AssetDescription")
	int32 NumUniqueGrooms = 0;

	/**
	 * Simulation enabled
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AssetDescription")
	bool bPhysics = false;

	/**
	 * Number of curves
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AssetDescription")
	int32 StrandsCount = 0;

	/**
	 * Number of CVs
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AssetDescription")
	int32 StrandsPointCount = 0;

	/**
	 * LODs available
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AssetDescription")
	bool bHasLods = false;

	/**
	 * Number of card assets
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AssetDescription")
	int32 CardMeshCount = 0;

	/**
	 * Number of verts
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AssetDescription")
	int32 CardMeshVertices = 0;

	/**
	 * Texture Atlas resolution
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AssetDescription")
	FIntVector2 CardMeshTextureResolution = FIntVector2::ZeroValue;

	/**
	 * Number of meshes
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AssetDescription")
	int32 VolumeMeshCount = 0;

	/**
	 * Number of verts
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AssetDescription")
	int32 VolumeMeshVertices = 0;

	/**
	 * Textures resolution
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AssetDescription")
	FIntVector2 VolumeMeshTextureResolution = FIntVector2::ZeroValue;

	/**
	 * Number of material or material instances
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AssetDescription")
	int32 NumMaterials = 0;

	/**
	 * UE Version asset was packaged with
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AssetDescription")
	FString EngineVersion;
};

/**
 * Description of a MetaHuman asset including verification status
 */
USTRUCT(BlueprintType)
struct METAHUMANSDKEDITOR_API FMetaHumanAssetDescription
{
	GENERATED_BODY()

	FMetaHumanAssetDescription() = default;
	FMetaHumanAssetDescription(const FAssetData& InAssetData, EMetaHumanAssetType InAssetType, const FName& DisplayName = FName());

	/**
	 * The display name for the Asset. Normally the name of the Root Asset.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AssetDescription")
	FName Name;

	/**
	 * The FAssetData describing the Root Asset
	 */
	UPROPERTY(SkipSerialization, EditAnywhere, BlueprintReadOnly, Category="AssetDescription")
	FAssetData AssetData;

	/**
	 * A list of all the packages included in the asset
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AssetDescription")
	TArray<FName> DependentPackages;

	/**
	 * The asset type, i.e. Groom, Clothing, etc.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AssetDescription")
	EMetaHumanAssetType AssetType = EMetaHumanAssetType::CharacterAssembly;


	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AssetDescription")
	FMetaHumanAggregateDetails Details;

	/**
	 * The total size of all assets
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AssetDescription")
	int32 TotalSize = 0;

	/**
	 * If present, the verification report for this Asset
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Verification")
	TObjectPtr<UMetaHumanAssetReport> VerificationReport;
};

/**
 * Description of the contents of a multi-item archive
 */
USTRUCT(BlueprintType)
struct METAHUMANSDKEDITOR_API FMetaHumanMultiArchiveDescription
{
	GENERATED_BODY()

	/**
	 * A list of all the sub-archives included in the archive
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ArchiveDescription")
	TArray<FString> ContainedArchives;
};

/**
 * A single item in a MetaHuman Archive
 */
USTRUCT(BlueprintType)
struct METAHUMANSDKEDITOR_API FMetaHumanArchiveEntry
{
	GENERATED_BODY()

	/**
	 * The path to this file relative to the root of the archive
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ArchiveContents")
	FString Path;

	/**
	 * The version of the file in format Major.Minor. If no version is available the version should be 0.0
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ArchiveContents")
	FString Version;
};

/**
 * Description of the contents of a MetaHuman Archive
 */
USTRUCT(BlueprintType)
struct METAHUMANSDKEDITOR_API FMetaHumanArchiveContents
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ArchiveContents")
	TArray<FMetaHumanArchiveEntry> Files;
};

/**
 * Manages MetaHuman characters and compatible assets in the project
 */
UCLASS(BlueprintType)
class METAHUMANSDKEDITOR_API UMetaHumanAssetManager : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Finds all assets in the project that ore of the correct type and in the correct location to be packaged
	 *
	 * @param AssetType The type of asset to find
	 * @return The found assets
	 */
	UFUNCTION(BlueprintCallable, Category = " MetaHuman | AssetManager ")
	static TArray<FMetaHumanAssetDescription> FindAssetsForPackaging(EMetaHumanAssetType AssetType);

	/**
	 * Performs some simple tests to see if an asset is of the correct type and in the correct location to be
	 * a Root Asset of the given type of MetaHuman Asset.
	 *
	 * @param RootPackage The Root Package of the asset to be tested
	 * @param AssetType The type of asset to be tested against
	 * @return Whether the asset is a packageable asset of the given type
	 */
	UFUNCTION(BlueprintCallable, Category = " MetaHuman | AssetManager ")
	static bool IsAssetOfType(const FName& RootPackage, EMetaHumanAssetType AssetType);


	/**
	 * Tests whether the joint names and hierarchy in a skeleton match with those in the standard MetaHuman body skeleton
	 *
	 * @param ToTest The Skeleton to test
	 * @return whether ToTest is MetaHuman compatible
	 */
	static bool IsMetaHumanBodyCompatibleSkeleton(const USkeleton* ToTest);

	/**
	 * Tests whether the joint names and hierarchy in a skeleton match with those in the standard MetaHuman face skeleton
	 *
	 * @param ToTest The Skeleton to test
	 * @return whether ToTest is MetaHuman compatible
	 */
	static bool IsMetaHumanFaceCompatibleSkeleton(const USkeleton* ToTest);

	/**
	 * Packages up the described MetaHuman Asset (including dependencies) into a zip file
	 *
	 * @param Assets The MetaHuman Assets to be packaged
	 * @param ArchivePath The file to package up to
	 * @return Whether the archive was successfully created
	 */
	UFUNCTION(BlueprintCallable, Category = " MetaHuman | AssetManager ")
	static bool CreateArchive(const TArray<FMetaHumanAssetDescription>& Assets, const FString& ArchivePath);


	/**
	 * Updates the stored package dependencies for a MetaHuman Asset
	 *
	 * @param Asset The MetaHuman Asset to update
	 */
	UFUNCTION(BlueprintCallable, Category = " MetaHuman | AssetManager ")
	static FMetaHumanAssetDescription& UpdateAssetDependencies(UPARAM(ref)
		FMetaHumanAssetDescription& Asset);


	/**
	 * Updates the stored asset details for a MetaHuman Asset
	 *
	 * @param Asset The MetaHuman Asset to update
	 */
	UFUNCTION(BlueprintCallable, Category = " MetaHuman | AssetManager ")
	static FMetaHumanAssetDescription& UpdateAssetDetails(UPARAM(ref)
		FMetaHumanAssetDescription& Asset);

	/**
	 * Asynchronously imports a MetaHuman Asset in to a project
	 *
	 * @param ArchivePath The file to import
	 * @param ImportOptions Options such as whether to ignore version information
	 * @param Report A report to fill out with the information about the import operation
	 * @return a Future which will complete with the import status when the import is completed
	 */
	static TFuture<bool> ImportArchive(const FString& ArchivePath, const FMetaHumanImportOptions& ImportOptions, UMetaHumanAssetReport* Report);

	/**
	 * Gives the root folder to use for discovering packageable Asset Groups
	 * @param AssetType The type of asset to get the folder for
	 * @return The root folder to use for discovering packageable Asset Groups of this type
	 */
	static FString GetPackagingFolderForAssetType(const EMetaHumanAssetType AssetType);

	/**
	 * Gives the UCLass corresponding to the main asset for an AssetGroup
	 * @param AssetType The type of asset to get the Class for
	 * @return The UCLass corresponding to the main asset for an AssetGroup of this type
	 */
	static FTopLevelAssetPath GetMainAssetClassPathForAssetType(const EMetaHumanAssetType AssetType);

	/**
	 * Given a main asset from an Asset Group, find the package containing the relevant Wardrobe Item if it exists
	 *
	 * @param MainAssetPackage The main asset to use to look for a wardrobe item
	 * @return The package containing the wardrobe Item or FName::None
	 */
	static FName GetWardrobeItemPackage(FName MainAssetPackage);
};
