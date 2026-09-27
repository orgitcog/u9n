// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ThumbnailRendering/DefaultSizedThumbnailRenderer.h"
#include "ThumbnailHelpers.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "MetaHumanWardrobeItemThumbnailRenderer.generated.h"

/**
 * Class that does the thumbnail rendering for MetaHuman Wardrobe Item
 */
UCLASS(MinimalAPI)
class UMetaHumanWardrobeItemThumbnailRenderer : public UDefaultSizedThumbnailRenderer
{
	GENERATED_BODY()

public:
	//~Begin UDefaultSizedThumbnailRenderer interface
	virtual bool CanVisualizeAsset(UObject* InObject) override;
	virtual void Draw(UObject* InObject, int32 InX, int32 InY, uint32 InWidth, uint32 InHeight, class FRenderTarget* InRenderTarget, class FCanvas* InCanvas, bool bInAdditionalViewFamily) override;
	//~End UDefaultSizedThumbnailRenderer interface

};
