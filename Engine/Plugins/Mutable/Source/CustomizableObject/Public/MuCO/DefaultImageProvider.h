// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCO/CustomizableObjectSystem.h"

#include "DefaultImageProvider.generated.h"

#define UE_API CUSTOMIZABLEOBJECT_API

class UTexture2D;
class UCustomizableObjectInstance;

namespace mu
{
	class FParameters;
}

/** Simple image provider that translates UTexture2D to Mutable IDs.
 *
 * Allows the reuse of UTexture2D. */
UCLASS(MinimalAPI)
class UDefaultImageProvider : public UCustomizableSystemImageProvider
{
	GENERATED_BODY()

public:
	// UCustomizableSystemImageProvider interface
	UE_API virtual ValueType HasTextureParameterValue(const FName& ID) override;
	UE_API virtual UTexture2D* GetTextureParameterValue(const FName& ID) override;
	UE_API virtual void GetTextureParameterValues(TArray<FCustomizableObjectExternalTexture>& OutValues) override;
	
	/** Add a Texture to the provider. */
	UE_API FString Add(UTexture2D* Texture);

	/** Remove a Texture from the provider. */
	UE_API void Remove(UTexture2D* Texture); 

private:
	// Always contains valid pointers
	UPROPERTY()
	TMap<FName, TObjectPtr<UTexture2D>> Textures;
};

#undef UE_API
