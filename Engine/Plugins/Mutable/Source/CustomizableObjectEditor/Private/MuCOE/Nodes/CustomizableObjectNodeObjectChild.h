// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNodeObject.h"

#include "CustomizableObjectNodeObjectChild.generated.h"

class UObject;


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeObjectChild : public UCustomizableObjectNodeObject
{
public:
	GENERATED_BODY()

	UCustomizableObjectNodeObjectChild();

	UPROPERTY()
	FEdGraphPinReference NamePin;

public:

	// UEdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual void PrepareForCopying() override;
	virtual FText GetTooltipText() const override;
	virtual bool CanUserDeleteNode() const override;
	virtual bool CanDuplicateNode() const override;
	virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;
	virtual bool GetCanRenameNode() const;

	// UCustomizableObjectNode interface
	virtual bool IsNodeSupportedInMacros() const override;
	virtual void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;
	virtual void BackwardsCompatibleFixup(int32 CustomizableObjectCustomVersion) override;

	// UCustomizableObjectNodeObject interface
	virtual FString GetObjectName(TArray<const UCustomizableObjectNodeMacroInstance*>* MacroContext = nullptr) const override;

};

