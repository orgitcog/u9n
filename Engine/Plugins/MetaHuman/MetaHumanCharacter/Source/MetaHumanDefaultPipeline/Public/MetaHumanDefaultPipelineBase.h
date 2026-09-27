// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Item/MetaHumanGroomPipeline.h"
#include "Item/MetaHumanSkeletalMeshPipeline.h"
#include "Item/MetaHumanOutfitPipeline.h"
#include "MetaHumanCharacter.h"
#include "MetaHumanCollectionPipeline.h"

#include "ChaosOutfitAsset/OutfitAsset.h"
#include "Misc/NotNull.h"
#include "Templates/SubclassOf.h"
#include "UObject/ScriptInterface.h"
#include "UObject/WeakObjectPtrFwd.h"

#include "MetaHumanDefaultPipelineBase.generated.h"

class AActor;
class ITargetPlatform;
class UMetaHumanCharacterInstance;
class UMetaHumanCharacterPalette;

USTRUCT(BlueprintType)
struct FMetaHumanMeshPartOutput
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Build")
	TObjectPtr<class USkeletalMesh> Mesh;

	// TODO: For now these additional materials are not used.
	// They're here as an example of additional data that may go alongside a mesh.
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Build")
	TArray<TObjectPtr<class UMaterialInterface>> OverrideMaterials;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Build")
	TArray<TObjectPtr<class UMaterialInterface>> BakedMaterials;
};

USTRUCT()
struct FMetaHumanCharacterPartOutput
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FMetaHumanCharacterGeneratedAssets GeneratedAssets;
};

USTRUCT(BlueprintType)
struct FMetaHumanDefaultAssemblyOutput
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Build")
	TObjectPtr<class USkeletalMesh> FaceMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Build")
	TObjectPtr<class USkeletalMesh> BodyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Build")
	FMetaHumanGroomPipelineAssemblyOutput Hair;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Build")
	FMetaHumanGroomPipelineAssemblyOutput Eyebrows;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Build")
	FMetaHumanGroomPipelineAssemblyOutput Beard;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Build")
	FMetaHumanGroomPipelineAssemblyOutput Mustache;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Build")
	FMetaHumanGroomPipelineAssemblyOutput Eyelashes;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Build")
	FMetaHumanGroomPipelineAssemblyOutput Peachfuzz;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Build")
	TArray<FMetaHumanSkeletalMeshPipelineAssemblyOutput> SkeletalMeshData;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Build")
	TArray<FMetaHumanOutfitPipelineAssemblyOutput> ClothData;
};

/**
 * The common base class for the current and legacy default MetaHuman pipelines.
 * 
 * Contains shared functionality for building simple MetaHumans.
 */
UCLASS(Abstract)
class METAHUMANDEFAULTPIPELINE_API UMetaHumanDefaultPipelineBase : public UMetaHumanCollectionPipeline
{
	GENERATED_BODY()

public:
	UMetaHumanDefaultPipelineBase();

	virtual void AssembleCollection(
		TNotNull<const UMetaHumanCollection*> Collection,
		EMetaHumanCharacterPaletteBuildQuality Quality,
		const TArray<FMetaHumanPipelineSlotSelectionData>& SlotSelections,
		const FInstancedStruct& AssemblyInput,
		TNotNull<UObject*> OuterForGeneratedObjects,
		const FOnAssemblyComplete& OnComplete) const override;

	virtual TNotNull<const UMetaHumanCharacterPipelineSpecification*> GetSpecification() const override
	{
		return Specification;
	}

	virtual const UMetaHumanItemPipeline* GetFallbackItemPipelineForAssetType(const TSoftClassPtr<UObject>& InAssetClass) const override;

private:
	/** The specification that this pipeline implements */
	UPROPERTY()
	TObjectPtr<UMetaHumanCharacterPipelineSpecification> Specification;

	/** Default item pipeline per asset type. Used if wardrobe item doesn't have a pipeline specified */
	UPROPERTY(EditAnywhere, Category = "Pipeline")
	TMap<TSoftClassPtr<UObject>, TSubclassOf<UMetaHumanItemPipeline>> DefaultAssetPipelines;
};
