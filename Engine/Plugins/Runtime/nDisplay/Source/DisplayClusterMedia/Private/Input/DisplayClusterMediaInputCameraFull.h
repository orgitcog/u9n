// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Input/DisplayClusterMediaInputCameraBase.h"


/**
 * Camera input adapter (full frame)
 */
class FDisplayClusterMediaInputCameraFull
	: public FDisplayClusterMediaInputCameraBase
{
public:

	FDisplayClusterMediaInputCameraFull(
		const FString& MediaId,
		const FString& ClusterNodeId,
		const FString& CameraId,
		UMediaSource* MediaSource
	);
};
