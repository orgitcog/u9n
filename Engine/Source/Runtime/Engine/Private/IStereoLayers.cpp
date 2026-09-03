// Copyright Epic Games, Inc. All Rights Reserved.

#include "IStereoLayers.h"

#include "Engine/TextureRenderTarget2D.h"

IStereoLayers::FLayerDesc IStereoLayers::GetDebugCanvasLayerDesc(UTextureRenderTarget2D* Texture)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FLayerDesc Desc = GetDebugCanvasLayerDesc(static_cast<FTextureRHIRef>(nullptr));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	Desc.TextureObj = Texture;
	return Desc;
}
