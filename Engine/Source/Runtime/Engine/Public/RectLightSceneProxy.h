// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RectLightSceneProxy.h: FRectLightSceneProxy definition.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Components/RectLightComponent.h"
#include "LocalLightSceneProxy.h"
#include "SceneManagement.h"


class FRectLightSceneProxy : public FLocalLightSceneProxy
{
public:
	float		SourceWidth;
	float		SourceHeight;
	float		BarnDoorAngle;
	float		BarnDoorLength;
	UTexture*	SourceTexture;
	uint32		RectAtlasId;
	float		LightFunctionConeAngleTangent;	// Use Ortho projection if 0
	FVector4f	SourceTextureScaleOffset;

	FRectLightSceneProxy(const URectLightComponent* Component);
	virtual ~FRectLightSceneProxy();

	virtual bool IsRectLight() const override;

	virtual bool HasSourceTexture() const override;

	/** Accesses parameters needed for rendering the light. */
	virtual void GetLightShaderParameters(FLightRenderParameters& OutLightParameters, uint32 Flags=0) const override;

	/**
	* Sets up a projected shadow initializer for shadows from the entire scene.
	* @return True if the whole-scene projected shadow should be used.
	*/
	virtual bool GetWholeSceneProjectedShadowInitializer(const FSceneViewFamily& ViewFamily, TArray<FWholeSceneProjectedShadowInitializer, TInlineAllocator<6> >& OutInitializers) const;
};