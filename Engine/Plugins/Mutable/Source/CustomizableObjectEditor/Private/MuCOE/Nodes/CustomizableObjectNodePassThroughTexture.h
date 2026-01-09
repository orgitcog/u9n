// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNodeTexture.h"

#include "CustomizableObjectNodePassThroughTexture.generated.h"


UCLASS(hidecategories = ("Texture2D"))
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodePassThroughTexture : public UCustomizableObjectNodeTextureBase
{
public:
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Texture, Meta = (DisplayName = Texture))
	TObjectPtr<UTexture> PassThroughTexture = nullptr;

	// UCustomizableObjectNode interface
	virtual void BackwardsCompatibleFixup(int32 CustomizableObjectCustomVersion) override;
	virtual void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;

	// UCustomizableObjectNodeTextureBase interface
	virtual TObjectPtr<UTexture> GetTexture() override;

	// Begin EdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;

private:

	// For backwards compatibility
	UPROPERTY()
	TObjectPtr<UTexture2D> Texture_DEPRECATED = nullptr;

};
