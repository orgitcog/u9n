// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosClothAsset/ClothAssetBase.h"
#include "ChaosOutfitAsset/Outfit.h"
#include "OutfitAsset.generated.h"

#define UE_API CHAOSOUTFITASSETENGINE_API

struct FChaosOutfitPiece;
struct FSkeletalMaterial;
namespace UE::Dataflow { struct IContextAssetStoreInterface; }

/**
 * Outfit asset for character clothing and simulation.
 */
UCLASS(Experimental, MinimalAPI, HideCategories = Object, BlueprintType, PrioritizeCategories = ("Dataflow"))
class UChaosOutfitAsset final : public UChaosClothAssetBase
{
	GENERATED_BODY()
public:
	UE_API UChaosOutfitAsset(const FObjectInitializer& ObjectInitializer);
	UE_API UChaosOutfitAsset(FVTableHelper& Helper);  // This is declared so we can use TUniquePtr<FClothSimulationModel> with just a forward declare of that class
	UE_API virtual ~UChaosOutfitAsset() override;

	UE_API void Build(const TObjectPtr<const UChaosOutfit> InOutfit, UE::Dataflow::IContextAssetStoreInterface* ContextAssetStore = nullptr);

	//~ Begin UObject interface
	UE_API virtual void Serialize(FArchive& Ar) override;
	UE_API virtual void PostLoad() override;
	//~ End UObject interface

	const FManagedArrayCollection& GetOutfitCollection() const
	{
		return OutfitCollection;
	}

#if WITH_EDITORONLY_DATA
	TObjectPtr<const UChaosOutfit> GetOutfit() const
	{
		return Outfit;
	}
#endif

private:
	//~ Begin UChaosClothAssetBase interface
	UE_API virtual bool HasValidClothSimulationModels() const override;
	virtual int32 GetNumClothSimulationModels() const override
	{
		return Pieces.Num();
	}
	UE_API virtual FName GetClothSimulationModelName(int32 ModelIndex) const override;
	UE_API virtual TSharedPtr<const FChaosClothSimulationModel> GetClothSimulationModel(int32 ModelIndex) const override;
	UE_API virtual const TArray<TSharedRef<const FManagedArrayCollection>>& GetCollections(int32 ModelIndex) const override;
	UE_API virtual const UPhysicsAsset* GetPhysicsAssetForModel(int32 ModelIndex) const override;
	UE_API virtual FGuid GetAssetGuid(int32 ModelIndex) const override;
	//~ End UChaosClothAssetBase interface

	//~ Begin USkinnedAsset interface
	virtual UPhysicsAsset* GetPhysicsAsset() const override
	{
		return nullptr;  // There isn't a single Physics Asset anymore, this could return the first one but that wouldn't be accurate
	}
	virtual USkeleton* GetSkeleton() override
	{
		return nullptr;  // Note: The USkeleton isn't a reliable source of reference skeleton
	}
	virtual const USkeleton* GetSkeleton() const override
	{
		return nullptr;
	}
	virtual void SetSkeleton(USkeleton* InSkeleton) override {}
#if WITH_EDITOR
	UE_API virtual FString BuildDerivedDataKey(const ITargetPlatform* TargetPlatform) override;
	virtual bool IsInitialBuildDone() const override
	{
		return true;
	}
#endif
#if WITH_EDITORONLY_DATA
	virtual class FSkeletalMeshModel* GetImportedModel() const override
	{
		return nullptr;
	}
#endif
	//~ End USkinnedAsset interface

	UE_API void CalculateBounds();

	UPROPERTY()
	TArray<FChaosOutfitPiece> Pieces;

	UPROPERTY()
	TArray<TObjectPtr<USkeletalMesh>> Bodies;  // Only contains dependencies, populated from the outfit collection

	UPROPERTY()
	FManagedArrayCollection OutfitCollection;

#if WITH_EDITORONLY_DATA
	UPROPERTY(Instanced)
	TObjectPtr<UChaosOutfit> Outfit;  // Outfit source model used for generating this outfit asset
#endif
};

#undef UE_API
