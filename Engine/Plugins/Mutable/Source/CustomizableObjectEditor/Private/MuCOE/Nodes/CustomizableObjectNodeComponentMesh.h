// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CustomizableObjectNodeComponent.h"
#include "CustomizableObjectNodeComponentMeshBase.h"
#include "MuR/System.h"

#include "CustomizableObjectNodeComponentMesh.generated.h"

class UMaterialInterface;

UENUM()
enum class ECustomizableObjectSelectionOverride : uint8
{
	NoOverride = 0 UMETA(DisplayName = "No Override"),
	Disable    = 1 UMETA(DisplayName = "Disable"    ),
	Enable     = 2 UMETA(DisplayName = "Enable"     )
};


USTRUCT()
struct FBoneToRemove
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	bool bOnlyRemoveChildren = false;

	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	FName BoneName;
};


USTRUCT()
struct FLODReductionSettings
{
	GENERATED_USTRUCT_BODY()

	/** Selects which bones will be removed from the final skeleton
	* BoneName: Name of the bone that will be removed. Its children will be removed too.
	* Remove Only Children: If true, only the children of the selected bone will be removed. The selected bone will remain.
	*/
	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	TArray<FBoneToRemove> BonesToRemove;
};


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeComponentMesh : public UCustomizableObjectNodeComponent, public ICustomizableObjectNodeComponentMeshInterface
{
	GENERATED_BODY()

public:
	UCustomizableObjectNodeComponentMesh();
	
	// UObject interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void BackwardsCompatibleFixup(int32 CustomizableObjectCustomVersion) override;

	// UEdGraphNode interface
	virtual void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;

	//UCustomizableObjectNode interface
	bool IsSingleOutputNode() const;

	// Own interface
	TSoftObjectPtr<UMaterialInterface> GetOverlayMaterial() const;
	UEdGraphPin* GetOverlayMaterialAssetPin() const;

	// ComponentMesh Interface
	virtual int32 GetNumLODs() override;
	virtual ECustomizableObjectAutomaticLODStrategy GetAutoLODStrategy() override;
	virtual const TArray<FEdGraphPinReference>& GetLODPins() const override;
	virtual UEdGraphPin* GetOutputPin() const;
	virtual void SetOutputPin(const UEdGraphPin* Pin);
	virtual const UCustomizableObjectNode* GetOwningNode() const;

public:

	/*UPROPERTY(EditAnywhere, Category = ComponentMesh)
	FName ComponentName = "Default name";*/
	
	/** All the Skeletal Meshes generated for this component will use the Reference Skeletal Mesh properties
	 * for everything Mutable doesn't create or modify. This includes data like LOD distances, Physics
	 * properties, Bounding Volumes, Base Skeleton, and more.
	 *
	 * The Reference Skeletal Mesh can be used as a placeholder mesh when there are too many actors or in 
	 * situations of stress where the generation of the Skeletal Mesh might take a few seconds to complete. */
	UPROPERTY(EditAnywhere, Category = ComponentMesh)
	TObjectPtr<USkeletalMesh> ReferenceSkeletalMesh;

	/** LOD reduction settings to apply on top of the LOD settings of the Reference Skeletal Mesh. */
	UPROPERTY(EditAnywhere, Category = ComponentMesh)
	TArray<FLODReductionSettings> LODReductionSettings;

	UPROPERTY(EditAnywhere, Category = ComponentMesh)
	TSoftObjectPtr<UMaterialInterface> OverlayMaterial;

	UPROPERTY(EditAnywhere, Category = ComponentMesh)
	FMutableLODSettings LODSettings;

	/** Details selected LOD. */
	int32 SelectedLOD = 0;

	UPROPERTY(EditAnywhere, Category = ComponentMesh)
	int32 NumLODs = 1;

	UPROPERTY(EditAnywhere, Category = ComponentMesh, DisplayName = "Auto LOD Strategy")
	ECustomizableObjectAutomaticLODStrategy AutoLODStrategy = ECustomizableObjectAutomaticLODStrategy::AutomaticFromMesh;

	UPROPERTY()
	TArray<FEdGraphPinReference> LODPins;

};
