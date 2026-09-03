// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Input/DisplayClusterMediaInputCameraBase.h"


/**
 * Camera input adapter (tile)
 */
class FDisplayClusterMediaInputCameraTile
	: public FDisplayClusterMediaInputCameraBase
{
public:

	FDisplayClusterMediaInputCameraTile(
		const FString& MediaId,
		const FString& InClusterNodeId,
		const FString& CameraId,
		const FIntPoint& TilePosition,
		UMediaSource* MediaSource
	);
};
