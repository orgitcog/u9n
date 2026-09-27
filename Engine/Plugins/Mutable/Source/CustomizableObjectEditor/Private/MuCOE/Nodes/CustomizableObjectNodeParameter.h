// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNode.h"

#include "CustomizableObjectNodeParameter.generated.h"

namespace ENodeTitleType { enum Type : int; }

class UCustomizableObjectNodeRemapPins;
class UObject;


UCLASS(Abstract)
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeParameter : public UCustomizableObjectNode
{
public:
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = UI, meta = (DisplayName = "Parameter UI Metadata"))
	FMutableParamUIMetadata ParamUIMetadata;

	UPROPERTY()
	FEdGraphPinReference NamePin;

private:

	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	FString ParameterName = "Default Name";

public:

	// Begin EdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;
	virtual void OnRenameNode(const FString& NewName) override;
	virtual bool GetCanRenameNode() const override;
	virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;

	// UCustomizableObjectNode interface
	virtual void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;
	virtual bool IsAffectedByLOD() const override { return false; }
	virtual void BackwardsCompatibleFixup(int32 CustomizableObjectCustomVersion) override;

	// Own interface
	/** Return the pin category of this node. */
	virtual FName GetCategory() const PURE_VIRTUAL(UCustomizableObjectNodeParameter::GetCategory, return {}; );
	virtual FString GetParameterName(TArray<const UCustomizableObjectNodeMacroInstance*>* MacroContext = nullptr) const;
	virtual void SetParameterName(const FString& Name);
};

