// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Camera/CameraShakeBase.h"
#include "DefaultCameraShakeBase.generated.h"

#define UE_API ENGINECAMERAS_API

/**
 * Like UCameraShakeBase but with a perlin noise shake pattern by default, for convenience.
 */
UCLASS(MinimalAPI)
class UDefaultCameraShakeBase : public UCameraShakeBase
{
	GENERATED_BODY()

public:

	UE_API UDefaultCameraShakeBase(const FObjectInitializer& ObjInit);
};

#undef UE_API
