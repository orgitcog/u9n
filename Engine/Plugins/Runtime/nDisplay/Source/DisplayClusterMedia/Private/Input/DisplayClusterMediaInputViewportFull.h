// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Input/DisplayClusterMediaInputViewportBase.h"


/**
 * Viewport input adapter (full frame)
 */
class FDisplayClusterMediaInputViewportFull
	: public FDisplayClusterMediaInputViewportBase
{
public:
	FDisplayClusterMediaInputViewportFull(
		const FString& MediaId,
		const FString& ClusterNodeId,
		const FString& ViewportId,
		UMediaSource* MediaSource
	);
};
