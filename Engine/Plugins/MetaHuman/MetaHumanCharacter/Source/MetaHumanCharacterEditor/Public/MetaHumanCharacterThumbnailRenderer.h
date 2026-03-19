// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ThumbnailRendering/DefaultSizedThumbnailRenderer.h"
#include "ThumbnailHelpers.h"
#include "UObject/StrongObjectPtr.h"
#include "EditorFramework/ThumbnailInfo.h"

#include "MetaHumanCharacterThumbnailRenderer.generated.h"

class UMetaHumanCharacter;
enum class EMetaHumanCharacterThumbnailCameraPosition : uint8;

/**
 * Scene containing a MetaHumanCharacter actor.
 */
class FMetaHumanCharacterThumbnailScene
	: public FThumbnailPreviewScene
{
public:
	FMetaHumanCharacterThumbnailScene();

	void CreatePreview(UMetaHumanCharacter* InCharacter, EMetaHumanCharacterThumbnailCameraPosition InCameraPosition);
	void DestroyPreview();

protected:

	//~Begin FThumbnailPreviewScene interface
	virtual void GetViewMatrixParameters(const float InFOVDegrees, FVector& OutOrigin, float& OutOrbitPitch, float& OutOrbitYaw, float& OutOrbitZoom) const override;
	virtual float GetFOV() const override;
	//~End FThumbnailPreviewScene interface

private:

	/** The actor used to preview the character. */
	TScriptInterface<class IMetaHumanCharacterEditorActorInterface> PreviewActor;

	/** A reference to the Character asset which we need to generate the thumbnail for. */
	TWeakObjectPtr<UMetaHumanCharacter> Character;

	/** Camera position for this scene */
	EMetaHumanCharacterThumbnailCameraPosition CameraPosition;
};

/**
 * Class that does the thumbnail rendering. It contains a reference to the MetaHumanCharacter
 * scene which will be spawned for the thumbnail renderer.
 */
UCLASS(MinimalAPI)
class UMetaHumanCharacterThumbnailRenderer
	: public UDefaultSizedThumbnailRenderer
{
	GENERATED_BODY()

public:
	UMetaHumanCharacterThumbnailRenderer();

	//~Begin UObject interface
	virtual void BeginDestroy() override;
	//~End UObject interface

	//~Begin UDefaultSizedThumbnailRenderer interface
	virtual bool CanVisualizeAsset(UObject* InObject) override;
	virtual void Draw(UObject* InObject, int32 InX, int32 InY, uint32 InWidth, uint32 InHeight, class FRenderTarget* InRenderTarget, class FCanvas* InCanvas, bool bInAdditionalViewFamily) override;
	virtual EThumbnailRenderFrequency GetThumbnailRenderFrequency(UObject* Object) const override;
	//~End UDefaultSizedThumbnailRenderer interface

	/** Specifies the camera to be used for the next thumbnail capture */
	EMetaHumanCharacterThumbnailCameraPosition CameraPosition;

private:
	/** Scene that we're rendering */
	TUniquePtr<FMetaHumanCharacterThumbnailScene> ThumbnailScene;
};
