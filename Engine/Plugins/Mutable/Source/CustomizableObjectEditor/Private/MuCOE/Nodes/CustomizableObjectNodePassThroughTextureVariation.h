// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNodeTextureVariation.h"

#include "CustomizableObjectNodePassThroughTextureVariation.generated.h"


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodePassThroughTextureVariation : public UCustomizableObjectNodeTextureVariation
{
public:
	GENERATED_BODY()

	// EdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;

	// UCustomizableObjectNode interface
	virtual void BackwardsCompatibleFixup(int32 CustomizableObjectCustomVersion) override;
	
	// UCustomizableObjectNodeVariation interface
	virtual FName GetCategory() const override;
};
