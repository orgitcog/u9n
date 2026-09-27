// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNodeMesh.h"

#include "CustomizableObjectNodeMeshParameter.generated.h"

namespace ENodeTitleType { enum Type : int; }

class UCustomizableObjectLayout;
class UMaterialInterface;
struct FSkeletalMaterial;


/** PinData of a pin that belongs to a Skeletal Mesh Section. */
UCLASS()
class UCustomizableObjectNodeMeshParameterPinDataSection : public UCustomizableObjectNodePinData
{
	GENERATED_BODY()

public:
	void Init(int32 InSectionIndex, int32 NumTexCoords);

	int32 GetSectionIndex() const;

	/** Layouts related to this Mesh pin */
	UPROPERTY()
	TArray<TObjectPtr<UCustomizableObjectLayout>> Layouts;

protected:

	virtual bool Equals(const UCustomizableObjectNodePinData& Other) const override;

private:

	UPROPERTY()
	int32 SectionIndex = -1;

};


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeMeshParameter : public UCustomizableObjectNodeMesh
{
public:
	GENERATED_BODY()

	/** Default value of the parameter. */
	UPROPERTY(EditAnywhere, Category=CustomizableObject)
	TSoftObjectPtr<USkeletalMesh> DefaultValue;

	UPROPERTY(EditAnywhere, Category=CustomizableObject)
	FString ParameterName = "Mesh Param";

	UPROPERTY(EditAnywhere, Category = UI, meta = (DisplayName = "Parameter UI Metadata"))
	FMutableParamUIMetadata ParamUIMetadata;

	UPROPERTY()
	FEdGraphPinReference NamePin;

public:

	// UObject interface.
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	// EdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;
	virtual void OnRenameNode(const FString& NewName) override;
	virtual bool GetCanRenameNode() const override;
	virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;

	// UCustomizableObjectNode interface
	virtual void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;
	virtual bool IsAffectedByLOD() const override { return false; }
	virtual bool IsExperimental() const override;
	virtual void BackwardsCompatibleFixup(int32 CustomizableObjectCustomVersion) override;

	// UCustomizableObjectNodeMesh interface
	virtual UTexture2D* FindTextureForPin(const UEdGraphPin* Pin) const override;
	virtual TArray<UCustomizableObjectLayout*> GetLayouts(const UEdGraphPin& MeshPin) const override;
	virtual TSoftObjectPtr<UObject> GetMesh() const override;
	virtual UEdGraphPin* GetMeshPin(int32 LOD, int32 SectionIndex) const override;
	virtual void GetPinSection(const UEdGraphPin& Pin, int32& OutLODIndex, int32& OutSectionIndex, int32& OutLayoutIndex) const override;

	// Own interface
	void GetPinSection(const UEdGraphPin& Pin, int32& OutSectionIndex) const;
	UMaterialInterface* GetMaterialInterfaceFor(const int32 SectionIndex) const;
	FSkeletalMaterial* GetSkeletalMaterialFor(const int32 SectionIndex) const;
	int32 GetSkeletalMaterialIndexFor(const UEdGraphPin& Pin) const;
	int32 GetSkeletalMaterialIndexFor(const int32 SectionIndex) const;

};

