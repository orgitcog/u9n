// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Containers/TextureShareSDKContainers.h"
#include "Containers/Resource/TextureShareCustomResource.h"

/**
 * Resource request
 */
struct FTextureShareResourceRequest
	: public FTextureShareCoreResourceRequest
{
public:
	FTextureShareResourceRequest(const FTextureShareCoreResourceDesc& InResourceDesc, const FTextureShareCustomResource& InCustomResource)
		: FTextureShareCoreResourceRequest(InResourceDesc, InCustomResource.CustomSize, InCustomResource.CustomFormat, InCustomResource.CustomGamma)
	{ }

	FTextureShareResourceRequest(const FTextureShareCoreResourceDesc& InResourceDesc, const float InCustomGamma)
		: FTextureShareCoreResourceRequest(InResourceDesc, InCustomGamma)
	{ }

	FTextureShareResourceRequest(const FTextureShareCoreResourceDesc& InResourceDesc, const DXGI_FORMAT InCustomFormat, const float InCustomGamma = -1.f)
		: FTextureShareCoreResourceRequest(InResourceDesc, InCustomFormat, InCustomGamma)
	{ }

	FTextureShareResourceRequest(const FTextureShareCoreResourceDesc& InResourceDesc, const FIntPoint& InSize, const float InCustomGamma = -1.f)
		: FTextureShareCoreResourceRequest(InResourceDesc, InSize, InCustomGamma)
	{ }

	FTextureShareResourceRequest(const FTextureShareCoreResourceDesc& InResourceDesc, const FIntPoint& InSize, const DXGI_FORMAT InFormat, const float InCustomGamma = -1.f)
		: FTextureShareCoreResourceRequest(InResourceDesc, InSize, InFormat, InCustomGamma)
	{ }
};
