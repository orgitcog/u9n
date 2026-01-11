// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "ThumbnailHelpers.h"
#include "UObject/ObjectPtr.h"

class UGroomAsset;
class AGroomActor;

class FGroomAssetThumbnailScene : public FThumbnailPreviewScene
{
public:
	/** Constructor */
	HAIRSTRANDSEDITOR_API FGroomAssetThumbnailScene();

	/** Sets the groom asset to use in the next CreateView() */
	HAIRSTRANDSEDITOR_API void SetGroomAsset(UGroomAsset* GroomAsset);


	HAIRSTRANDSEDITOR_API void CleanupSceneAfterThumbnailRender();
protected:
	// FThumbnailPreviewScene implementation
	HAIRSTRANDSEDITOR_API virtual void GetViewMatrixParameters(const float InFOVDegrees, FVector& OutOrigin, float& OutOrbitPitch, float& OutOrbitYaw, float& OutOrbitZoom) const override;

private:
	/** The static mesh actor used to display all static mesh thumbnails */
	TObjectPtr<AGroomActor> PreviewActor;
};
