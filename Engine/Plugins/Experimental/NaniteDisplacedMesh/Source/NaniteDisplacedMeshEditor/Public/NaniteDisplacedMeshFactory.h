// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "NaniteDisplacedMeshFactory.generated.h"

class UNaniteDisplacedMesh;
struct FNaniteDisplacedMeshParams;
struct FValidatedNaniteDisplacedMeshParams;

#define NANITE_DISPLACED_MESH_ID_VERSION 3

UCLASS(hidecategories = Object)
class NANITEDISPLACEDMESHEDITOR_API UNaniteDisplacedMeshFactory : public UFactory
{
	GENERATED_BODY()

public:
	UNaniteDisplacedMeshFactory();

	//~ Begin UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	static UNaniteDisplacedMesh* StaticFactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn);

	bool bCreateReadOnlyAsset = false;
};

enum class ELinkDisplacedMeshAssetSetting : uint8
{
	LinkAgainstPersistentAsset,
	CanLinkAgainstPersistentAndTransientAsset,
	LinkAgainstTransientAsset,
	LinkAgainstExistingPersistentAsset,
};

struct FNaniteDisplacedMeshLinkParameters
{
	FStringView DisplacedMeshFolder;
	ELinkDisplacedMeshAssetSetting LinkDisplacedMeshAssetSetting = ELinkDisplacedMeshAssetSetting::LinkAgainstPersistentAsset;
	bool* bOutCreatedNewMesh = nullptr;
	bool bForcePackageToBePublic = false;
};

UE_DEPRECATED(5.6, "Use the other override with the FValidatedNaniteDisplacedMeshParams as second the argument type instead")
NANITEDISPLACEDMESHEDITOR_API UNaniteDisplacedMesh* LinkDisplacedMeshAsset(
	UNaniteDisplacedMesh* ExistingDisplacedMesh,
	const FNaniteDisplacedMeshParams& InParameters,
	const FString& DisplacedMeshFolder,
	ELinkDisplacedMeshAssetSetting LinkDisplacedMeshAssetSetting = ELinkDisplacedMeshAssetSetting::LinkAgainstPersistentAsset,
	bool* bOutCreatedNewMesh = nullptr
);

NANITEDISPLACEDMESHEDITOR_API UNaniteDisplacedMesh* LinkDisplacedMeshAsset(
	UNaniteDisplacedMesh* ExistingDisplacedMesh,
	FValidatedNaniteDisplacedMeshParams&& InParameters,
	const FNaniteDisplacedMeshLinkParameters& InLinkParameters
);

/**
 * Suggest a path to store the displaced base on if the asset used to generate the mesh all live in the same plugin.
 * @return A non empty string if it has a folder suggestion for the displaced mesh
 */
NANITEDISPLACEDMESHEDITOR_API FString GetSuggestedDisplacedMeshFolder(const FStringView& InSubPathForDisplacedMesh, const FValidatedNaniteDisplacedMeshParams& InParameters);

extern NANITEDISPLACEDMESHEDITOR_API const TCHAR* LinkedDisplacedMeshAssetNamePrefix;

NANITEDISPLACEDMESHEDITOR_API FString GenerateLinkedDisplacedMeshAssetName(const FNaniteDisplacedMeshParams& InParameters);

NANITEDISPLACEDMESHEDITOR_API FGuid GetAggregatedId(const FNaniteDisplacedMeshParams& DisplacedMeshParams);
NANITEDISPLACEDMESHEDITOR_API FGuid GetAggregatedId(const UNaniteDisplacedMesh& DisplacedMesh);

NANITEDISPLACEDMESHEDITOR_API FString GetAggregatedIdString(const FNaniteDisplacedMeshParams& DisplacedMeshParams);
NANITEDISPLACEDMESHEDITOR_API FString GetAggregatedIdString(const UNaniteDisplacedMesh& DisplacedMesh);
