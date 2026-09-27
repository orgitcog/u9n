// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNode.h"

#include "CustomizableObjectNodeStaticString.generated.h"


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeStaticString : public UCustomizableObjectNode
{
public:
	GENERATED_BODY()

	/** Macro Library where the macro to instantiate belongs. */
	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	FString Value;

public:

	// EdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual bool GetCanRenameNode() const override;

	// UCustomizableObjectNode interface
	void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;

};