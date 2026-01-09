// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "Engine/AssetUserData.h"
#include "Interfaces/Interface_AssetUserData.h"

#include "RigLogic.h"

#include "DNAAsset.generated.h"

#define UE_API RIGLOGICMODULE_API

DECLARE_LOG_CATEGORY_EXTERN(LogDNAAsset, Log, All);

class IDNAReader;
class IBehaviorReader;
class IGeometryReader;
class FRigLogicMemoryStream;
class UAssetUserData;
class USkeleton;
class USkeletalMesh;
class USkeletalMeshComponent;
struct FDNAIndexMapping;
struct FSharedRigRuntimeContext;
enum class EDNADataLayer: uint8;

 /** An asset holding the data needed to generate/update/animate a RigLogic character
  * It is imported from character's DNA file as a bit stream, and separated out it into runtime (behavior) and design-time chunks;
  * Currently, the design-time part still loads the geometry, as it is needed for the skeletal mesh update; once SkeletalMeshDNAReader is
  * fully implemented, it will be able to read the geometry directly from the SkeletalMesh and won't load it into this asset 
  **/
UCLASS(MinimalAPI, BlueprintType, meta = (DisplayName = "MetaHuman DNA data"))
class UDNAAsset : public UAssetUserData
{
	GENERATED_BODY()

public:
	UE_API UDNAAsset();
	UE_API ~UDNAAsset();

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Instanced, Category = ImportSettings)
	TObjectPtr<class UAssetImportData> AssetImportData;
#endif

	UE_API TSharedPtr<IDNAReader> GetBehaviorReader();
#if WITH_EDITORONLY_DATA
	UE_API TSharedPtr<IDNAReader> GetGeometryReader();
#endif

	UPROPERTY(AssetRegistrySearchable)
	FString DnaFileName;

	/** In non-editor builds, the DNA source data will be unloaded to save memory after the runtime
	  * data has been initialized from it.
	  * 
	  * Set this property to true to keep the DNA in memory, e.g. if you need to modify it at
	  * runtime. For most use cases, this shouldn't be needed.
	 **/
	UPROPERTY(EditAnywhere, Category = RuntimeSettings)
	bool bKeepDNAAfterInitialization;

	/** Collection of runtime metadata related to a specific character.
	  * The value field is a FString and requires casting for a derived types.
	 **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RuntimeSettings)
	TMap<FString, FString> MetaData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RuntimeSettings)
	FRigLogicConfiguration RigLogicConfiguration;

#if WITH_EDITOR
	UE_API void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UE_API bool Init(const FString& Filename);
	UE_API void Serialize(FArchive& Ar) override;

	/** Used when importing behavior into archetype SkelMesh in the editor,
	  * and when updating SkeletalMesh runtime with GeneSplicer
	**/
	UE_API void SetBehaviorReader(TSharedPtr<IDNAReader> SourceDNAReader);
	UE_API void SetGeometryReader(TSharedPtr<IDNAReader> SourceDNAReader);

	/** Initialize this object for use at runtime from another instance that has already been
	  * initialized. 
	  * 
	  * Overwrites all member variables. Only data needed for runtime evaluation will be copied.
	  * 
	  * Performs a shallow copy, so the runtime data is shared between the two instances and the
	  * memory cost of the copied UDNAAsset is very low.
	  *
	  * Note that the reference to the shared runtime data will be dropped if the source DNA is
	  * modified, so the two instances are effectively independent and can safely be modified or
	  * deleted without affecting the other.
	 **/
	UE_API void InitializeForRuntimeFrom(UDNAAsset* Other);
	UE_API TSharedPtr<FSharedRigRuntimeContext> GetRigRuntimeContext();
	UE_API TSharedPtr<FDNAIndexMapping> GetDNAIndexMapping(const USkeleton* Skeleton, const USkeletalMesh* SkeletalMesh);

private:
	friend struct FAnimNode_RigLogic;
	friend struct FRigUnit_RigLogic;

	UE_API void InvalidateRigRuntimeContext();
	UE_API void InitializeRigRuntimeContext();

private:
	struct FSkeletalMeshSkeletonToDNAIndexMappingKey
	{
		TWeakObjectPtr<const USkeletalMesh> SkeletalMesh;
		TWeakObjectPtr<const USkeleton> Skeleton;

		bool operator==(const FSkeletalMeshSkeletonToDNAIndexMappingKey& Other) const
		{
			return (SkeletalMesh == Other.SkeletalMesh) && (Skeleton == Other.Skeleton);
		}
		bool operator!=(const FSkeletalMeshSkeletonToDNAIndexMappingKey& Other) const
		{
			return !(*this == Other);
		}
		friend uint32 GetTypeHash(const FSkeletalMeshSkeletonToDNAIndexMappingKey& Instance)
		{
			return HashCombine(GetTypeHash(Instance.SkeletalMesh), GetTypeHash(Instance.Skeleton));
		}
	};

private:
	// Synchronize DNA updates
	FRWLock DNAUpdateLock;

	// Synchronize Rig Runtime Context updates
	FRWLock RigRuntimeContextUpdateLock;

	// Synchronize DNA Index Mapping updates
	FRWLock DNAIndexMappingUpdateLock;

	/** Part of the .dna file needed for run-time execution of RigLogic;
	 **/
	TSharedPtr<IDNAReader> BehaviorReader;

	/** Part of the .dna file used design-time for updating SkeletalMesh geometry
	 **/
	TSharedPtr<IDNAReader> GeometryReader;

	/** Runtime data necessary for rig computations that is shared between
	  * multiple rig instances based on the same DNA.
	**/
	TSharedPtr<FSharedRigRuntimeContext> RigRuntimeContext;

	/** Container for Skeleton <-> DNAAsset index mappings
	  * The mapping object owners will be the SkeletalMeshes, and periodic cleanups will
	  * ensure that dead objects are deleted from the map.
	 **/
	TMap<FSkeletalMeshSkeletonToDNAIndexMappingKey, TSharedPtr<FDNAIndexMapping>> DNAIndexMappingContainer;

};

#undef UE_API
