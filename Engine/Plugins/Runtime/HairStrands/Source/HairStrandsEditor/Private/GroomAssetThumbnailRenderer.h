// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "GroomAssetThumbnailScene.h"
#include "Templates/UniquePtr.h"
#include "ThumbnailRendering/DefaultSizedThumbnailRenderer.h"
#include "UObject/ObjectMacros.h"
#include "GroomAssetThumbnailRenderer.generated.h"

class FCanvas;
class FRenderTarget;

UCLASS(config = Editor, MinimalAPI)
class UGroomAssetThumbnailRenderer : public UDefaultSizedThumbnailRenderer
{
	GENERATED_BODY()

public:
	// Begin UThumbnailRenderer Object
	HAIRSTRANDSEDITOR_API virtual bool CanVisualizeAsset(UObject* Object) override;
	HAIRSTRANDSEDITOR_API virtual void Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* RenderTarget, FCanvas* Canvas, bool bAdditionalViewFamily) override;
	// End UThumbnailRenderer Object

	// UObject implementation
	HAIRSTRANDSEDITOR_API virtual void BeginDestroy() override;

private:
	TUniquePtr<FGroomAssetThumbnailScene> ThumbnailScene;
};

