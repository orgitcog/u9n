// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ThumbnailRendering/DefaultSizedThumbnailRenderer.h"

#include "TakeThumbnailRenderer.generated.h"

class UObject;
class FRenderTarget;
class FCanvas;

UCLASS(MinimalAPI)
class UTakeThumbnailRenderer : public UDefaultSizedThumbnailRenderer
{
	GENERATED_BODY()

public:
	virtual bool CanVisualizeAsset(UObject* InObject) override;
	virtual void Draw(UObject* InObject, int32 InX, int32 InY, uint32 InWidth, uint32 InHeight, FRenderTarget* InRenderTarget, FCanvas* InCanvas, bool bInAdditionalViewFamily) override;
	virtual void BeginDestroy() override;
};
