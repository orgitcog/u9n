// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanCharacterEditorActor.h"

#include "MetaHumanDefaultEditorPipelineActor.generated.h"

/** Used to store the original materials for a component while they're being overridden */
USTRUCT()
struct FMetaHumanComponentMaterials
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<class UMeshComponent> MeshComponent;

	UPROPERTY()
	TArray<TObjectPtr<class UMaterialInterface>> Materials;
};

/**
 * A preview actor that supports all features of the MetaHuman Default Character Pipeline
 */
UCLASS()
class AMetaHumanDefaultEditorPipelineActor : public AMetaHumanCharacterEditorActor
{
	GENERATED_BODY()

public:

	AMetaHumanDefaultEditorPipelineActor();

	// Begin IMetaHumanCharacterEditorActorInterface
	virtual void InitializeMetaHumanCharacterEditorActor(
		TNotNull<const UMetaHumanCharacterInstance*> InCharacterInstance,
		TNotNull<UMetaHumanCharacter*> InCharacter,
		TNotNull<USkeletalMesh*> InFaceMesh,
		TNotNull<USkeletalMesh*> InBodyMesh,
		int32 InNumLODs,
		const TArray<int32>& InFaceLODMapping,
		const TArray<int32>& InBodyLODMapping) override;
	
	virtual void SetHairVisibilityState(EMetaHumanHairVisibilityState State) override;
	virtual void SetClothingVisibilityState(EMetaHumanClothingVisibilityState State, UMaterialInterface* OverrideMaterial = nullptr) override;
	// End IMetaHumanCharacterEditorActorInterface

protected:
	virtual void OnInstanceUpdated();

	virtual void SetUseCardsOnGroomComponents(bool bInUseCards);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character")
	TObjectPtr<class UGroomComponent> HairComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character")
	TObjectPtr<class UGroomComponent> EyebrowsComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character")
	TObjectPtr<class UGroomComponent> BeardComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character")
	TObjectPtr<class UGroomComponent> MustacheComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character")
	TObjectPtr<class UGroomComponent> EyelashesComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character")
	TObjectPtr<class UGroomComponent> PeachfuzzComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character")
	TArray<TObjectPtr<class UChaosClothComponent>> ClothComponents;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character")
	TArray<TObjectPtr<class USkeletalMeshComponent>> OutfitMeshComponents;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character")
	TArray<TObjectPtr<class USkeletalMeshComponent>> SkeletalMeshComponents;

private:
	UPROPERTY()
	TArray<FMetaHumanComponentMaterials> SavedMaterials;

	UPROPERTY()
	EMetaHumanHairVisibilityState CurrentHairState = EMetaHumanHairVisibilityState::Shown;

	UPROPERTY()
	EMetaHumanClothingVisibilityState CurrentClothingState = EMetaHumanClothingVisibilityState::Shown;
	
	UPROPERTY()
	TObjectPtr<UMaterialInterface> CurrentOverrideMaterial;

};
