// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "MuCO/CustomizableObjectSystem.h"

#include "EditorImageProvider.generated.h"

#define UE_API CUSTOMIZABLEOBJECT_API

class UTexture2D;
class UCustomizableObjectInstance;


UCLASS(MinimalAPI)
class UEditorImageProvider : public UCustomizableSystemImageProvider
{
	GENERATED_BODY()

public:
	// UCustomizableSystemImageProvider interface
	UE_API virtual ValueType HasTextureParameterValue(const FName& ID) override;
	UE_API virtual UTexture2D* GetTextureParameterValue(const FName& ID) override;
};

#undef UE_API
