// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ThumbnailRendering/DefaultSizedThumbnailRenderer.h"
#include "ThumbnailHelpers.h"
#include "UObject/StrongObjectPtr.h"

#include "MetaHumanIdentityThumbnailRenderer.generated.h"

/////////////////////////////////////////////////////
// FMetaHumanIdentityThumbnailScene

class FMetaHumanIdentityThumbnailScene
	: public FThumbnailPreviewScene
{
public:

	FMetaHumanIdentityThumbnailScene();

	void SetMetaHumanIdentity(class UMetaHumanIdentity* InIdentity);

protected:

	//~ FThumbnailPreviewScene interface
	virtual void GetViewMatrixParameters(const float InFOVDegrees, FVector& OutOrigin, float& OutOrbitPitch, float& OutOrbitYaw, float& OutOrbitZoom) const override;

private:

	/** The actor used to preview the visual component of the Identity asset, which can be a static or skeletal mesh */
	TObjectPtr<class AActor> PreviewActor;

	/** A reference to the Identity asset which we need to generate the thumbnail for */
	TWeakObjectPtr<class UMetaHumanIdentity> Identity;

	/** The texture used to render the thumbnail */
	TStrongObjectPtr<class UTexture2D> FrameTexture;
};

/////////////////////////////////////////////////////
// UMetaHumanIdentityThumbnailRenderer

UCLASS(MinimalAPI)
class UMetaHumanIdentityThumbnailRenderer
	: public UDefaultSizedThumbnailRenderer
{
	GENERATED_BODY()

public:
	//~UDefaultSizedThumbnailRenderer interface
	virtual bool CanVisualizeAsset(UObject* InObject) override;
	virtual void Draw(UObject* InObject, int32 InX, int32 InY, uint32 InWidth, uint32 InHeight, class FRenderTarget* InRenderTarget, class FCanvas* InCanvas, bool bInAdditionalViewFamily) override;
	virtual void BeginDestroy() override;

private:

	TUniquePtr<FMetaHumanIdentityThumbnailScene> ThumbnailScene;
};