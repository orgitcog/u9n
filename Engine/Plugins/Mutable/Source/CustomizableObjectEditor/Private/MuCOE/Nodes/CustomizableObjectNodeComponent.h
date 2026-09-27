// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNode.h"

#include "CustomizableObjectNodeComponent.generated.h"

class UCustomizableObjectNodeMacroInstance;
class UEdGraphPin;


UCLASS(abstract)
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeComponent : public UCustomizableObjectNode
{
public:
	GENERATED_BODY()

	// UEdGraphNode interface
	virtual void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;
	virtual bool GetCanRenameNode() const override;
	virtual void OnRenameNode(const FString& NewName) override;

	// UCustomizableObjectNode interface
	virtual bool IsAffectedByLOD() const override;
	virtual void BackwardsCompatibleFixup(int32 CustomizableObjectCustomVersion) override;

	//Own Interface
	FName GetComponentName(TArray<const UCustomizableObjectNodeMacroInstance*>* MacroContext = nullptr) const;
	void SetComponentName(const FName& InComponentName);
	UEdGraphPin* GetComponentNamePin() const;

protected:

	UPROPERTY(EditAnywhere, Category = ComponentMesh)
	FName ComponentName = "Default name";

public:

	UPROPERTY()
	FEdGraphPinReference OutputPin;

private:
	
	UPROPERTY()
	FEdGraphPinReference ComponentNamePin;

};

