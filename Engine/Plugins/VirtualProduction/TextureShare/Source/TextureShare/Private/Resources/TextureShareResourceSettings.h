// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "PixelFormat.h"

#include "Containers/TextureShareContainers.h"

struct FTextureShareCoreResourceRequest;
class FRHITexture;

/**
 * TextureShare resource settings
 */
struct FTextureShareResourceSettings
{
	FTextureShareResourceSettings() = default;
	FTextureShareResourceSettings(const FTextureShareCoreResourceRequest& InResourceRequest, FRHITexture* InTexture);

	/** Initialize the resource based on the request information. */
	bool Initialize(const FTextureShareCoreResourceRequest& InResourceRequest);

	/** Return true if the settings are equal and this resource can be reused. */
	bool Equals(const FTextureShareResourceSettings& In) const;


	FIntPoint Size;
	EPixelFormat Format = PF_Unknown;

	// A description of the colors of this resource (Gamma, sRGB, OCIO, etc)
	FTextureShareColorDesc ColorDesc;

	uint32 NumMips = 1;
	bool bShouldUseSRGB = false;
};
