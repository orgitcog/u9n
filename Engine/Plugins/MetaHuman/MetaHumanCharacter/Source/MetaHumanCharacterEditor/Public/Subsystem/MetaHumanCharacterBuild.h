// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "Misc/NotNull.h"
#include "UObject/SoftObjectPtr.h"
#include "Containers/Set.h"
#include "Containers/Map.h"
#include "Templates/Function.h"
#include "MetaHumanTypesEditor.h"

#include "MetaHumanCharacterBuild.generated.h"

/**
 * Parameters to configure MetaHuman Character build.
 */
USTRUCT()
struct METAHUMANCHARACTEREDITOR_API FMetaHumanCharacterEditorBuildParameters
{
	GENERATED_BODY()

	/**
	 * Absolute location where the built assets will end up in. If empty, build will unpack
	 * assets in respect to the options set by the palette.
	 */
	FString AbsoluteBuildPath;

	/**
	 * Optional string to be used instead of the character name for the final unpacking folder.
	 */
	FString NameOverride;

	/**
	 * Optional path to a directory where Common MH assets should be shared or copied if needed.
	 */
	FString CommonFolderPath;

	/**
	 * Specifies the pipeline override to use to build the character.
	 *
	 * If none, build will fallback on using the pipeline defined on the MetaHumanCharacter,
	 * otherwise it will try to use the given pipeline class.
	 */
	UPROPERTY()
	TObjectPtr<class UMetaHumanCollectionPipeline> PipelineOverride;
};

struct FMetaHumanCharacterEditorBuild
{
	/**
	 * For a given MetaHumanCharacter assembles the MetaHuman Blueprint along with other
	 * MetaHuman assets (palette and instance).
	 * @param InMetaHumanCharacter Character that we want to build
	 * @param InParams Parameters that control the build process
	 */
	METAHUMANCHARACTEREDITOR_API static void BuildMetaHumanCharacter(
		TNotNull<class UMetaHumanCharacter*> InMetaHumanCharacter,
		const FMetaHumanCharacterEditorBuildParameters& InParams);

	/**
	 * Remove LODs from a Skeletal Mesh and DNA if one is attached
	 *
	 * @param InLODsToKeep Which LODs to keep in the mesh. If InLODsToKeep is empty, any of the LOD indices in is invalid or if there are more values
	 *					   than the number of LODs in the skeletal mesh, this function does nothing.
	 */
	METAHUMANCHARACTEREDITOR_API static void StripLODsFromMesh(TNotNull<class USkeletalMesh*> InSkeletalMesh, const TArray<int32>& InLODsToKeep);

	/**
	 * Downsize a texture it is larger than the target resolution
	 */
	METAHUMANCHARACTEREDITOR_API static void DownsizeTexture(TNotNull<class UTexture*> InTexture, int32 InTargetResolution, TNotNull<const ITargetPlatform*> InTargetPlatform);

	/**
	 * Merges body and face skeletal meshes. Resulting mesh will have only joints from the
	 * body and skin weights from the face will be transfered to the body joints.
	 * 
	 * Resulting mesh will be standalone asset.
	 */
	METAHUMANCHARACTEREDITOR_API static USkeletalMesh* MergeHeadAndBody_CreateAsset(
		TNotNull<class USkeletalMesh*> InFaceMesh,
		TNotNull<class USkeletalMesh*> InBodyMesh,
		const FString& InAssetPathAndName);

	/**
	 * Merges body and face skeletal meshes. Resulting mesh will have only joints from the
	 * body and skin weights from the face will be transfered to the body joints.
	 * 
	 * Resulting mesh will be transient object on the given outer.
	 */
	METAHUMANCHARACTEREDITOR_API static USkeletalMesh* MergeHeadAndBody_CreateTransient(
		TNotNull<USkeletalMesh*> InFaceMesh,
		TNotNull<USkeletalMesh*> InBodyMesh,
		UObject* InOuter);

	/**
	 * Helper to report errors to the message log of the MetaHuman editor module.
	 */
	static void ReportMessageLogErrors(
		bool bWasSuccessful,
		const FText& InSuccessMessageText,
		const FText& FailureMessageText);

	/**
	 * Duplicates the dependency objects to input root path and resolves any references as needed.
	 * If a dependency object already exists in the root folder then it is not duplicated.
	 * 
	 * @param InDependencies set of all dependency objects to be duplicated
	 * @param InDependencyRootPath new root folder for the dependencies to be copied
	 * @param InOutObjectsToReplaceWithin set with all objects that reference the dependencies and need to resolve their references
	 * @param OutDuplicatedDependencies map for every dependency to its new referenced object
	 * @param InIsAssetSupported Callable to check if an asset should be duplicated
	 */
	METAHUMANCHARACTEREDITOR_API static void DuplicateDepedenciesToNewRoot(
		const TSet<UObject*>& InDependencies,
		const FString& InDependencyRootPath,
		TSet<UObject*>& InOutObjectsToReplaceWithin,
		TMap<UObject*, UObject*>& OutDuplicatedDependencies,
		TFunction<bool(const UObject*)> InIsAssetSupported);

	/**
	 * Finds all the outer objects that are dependencies of the input root objects by walking recursively over all referenced objects
	 * It limits the tracking to the MetaHuman Character plugin and Game mount point by default
	 * Note that dependencies do not have to be saved on disk
	 * 
	 * @param InRootObjects array of objects to look for their dependencies
	 * @param InAllowedMountPoints additional mount points that are allowed in tracking the references (on top for MHC and Game) 
	 * @param OutDependencies set with all dependency objects
	*/
	METAHUMANCHARACTEREDITOR_API static void CollectDependencies(const TArray<UObject*>& InRootObjects, const TSet<FString>& InAllowedMountPoints, TSet<UObject*>& OutDependencies);

	/**
	 * Populates an array with all the objects referenced by the input instanced struct
	 * 
	 * @param StructType UStruct instance type
	 * @param StructPtr pointer to UStruct data 
	 * @param OutObjects array of UObjects that are referenecd by the struct
	*/
	METAHUMANCHARACTEREDITOR_API static void CollectUObjectReferencesFromStruct(const UStruct* StructType, const void* StructPtr, TArray<UObject*>& OutObjects);

	/**
	 * Helper returning the latest Actor BP version used in the plugin
	 */
	METAHUMANCHARACTEREDITOR_API static UE::MetaHuman::FMetaHumanAssetVersion GetMetaHumanAssetVersion();

	/**
	 * Helper to check if provided asset has matching or higher version in metadata.
	 */
	METAHUMANCHARACTEREDITOR_API static bool MetaHumanAssetMetadataVersionIsCompatible(TNotNull<const UObject*> InAsset);

	/**
	 * Helper to set the latest MetaHuman Asset Version used in the plugin
	 */
	METAHUMANCHARACTEREDITOR_API static void SetMetaHumanVersionMetadata(TNotNull<UObject*> InObject);

};
