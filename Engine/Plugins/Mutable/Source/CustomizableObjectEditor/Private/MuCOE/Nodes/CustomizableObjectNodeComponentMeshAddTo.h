// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNodeComponentMeshBase.h"

#include "CustomizableObjectNodeComponentMeshAddTo.generated.h"

class UCustomizableObjectNodeMacroInstance;

UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeComponentMeshAddTo : public UCustomizableObjectNode, public ICustomizableObjectNodeComponentMeshInterface
{
	GENERATED_BODY()

public:

	// UEdGraphNode interface
	virtual void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;
	virtual bool GetCanRenameNode() const override { return true; }
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual void OnRenameNode(const FString& NewName) override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;

	// UObject interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	//UCustomizableObjectNode interface
	virtual bool IsAffectedByLOD() const override;
	bool IsSingleOutputNode() const;
	virtual void BackwardsCompatibleFixup(int32 CustomizableObjectCustomVersion) override;

	// ComponentMesh Interface
	virtual int32 GetNumLODs() override;
	virtual ECustomizableObjectAutomaticLODStrategy GetAutoLODStrategy() override;
	virtual const TArray<FEdGraphPinReference>& GetLODPins() const override;
	virtual UEdGraphPin* GetOutputPin() const;
	virtual void SetOutputPin(const UEdGraphPin* Pin);
	virtual const UCustomizableObjectNode* GetOwningNode() const;

	// Own interface
	FName GetParentComponentName(TArray<const UCustomizableObjectNodeMacroInstance*>* MacroContext = nullptr) const;
	void SetParentComponentName(const FName& InComponentName);
	UEdGraphPin* GetParentComponentNamePin() const;

private:

	UPROPERTY(EditAnywhere, Category = ComponentMesh)
	FName ParentComponentName;

public:

	UPROPERTY(EditAnywhere, Category = ComponentMesh)
	int32 NumLODs = 1;

	UPROPERTY(EditAnywhere, Category = ComponentMesh, DisplayName = "Auto LOD Strategy")
	ECustomizableObjectAutomaticLODStrategy AutoLODStrategy = ECustomizableObjectAutomaticLODStrategy::AutomaticFromMesh;

	UPROPERTY()
	TArray<FEdGraphPinReference> LODPins;

	UPROPERTY()
	FEdGraphPinReference OutputPin;

private:
	
	UPROPERTY()
	FEdGraphPinReference ParentComponentNamePin;

};