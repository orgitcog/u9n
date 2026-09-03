// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Capture/DisplayClusterMediaCaptureViewportBase.h"


/**
 * Viewport capture adapter (tile)
 */
class FDisplayClusterMediaCaptureViewportTile
	: public FDisplayClusterMediaCaptureViewportBase
{
public:

	FDisplayClusterMediaCaptureViewportTile(
		const FString& MediaId,
		const FString& ClusterNodeId,
		const FString& ViewportId,
		const FIntPoint& TilePosition,
		UMediaOutput* MediaOutput,
		UDisplayClusterMediaOutputSynchronizationPolicy* SyncPolicy
	);

protected:

	//~ Begin FDisplayClusterMediaCaptureViewport
	virtual bool GetCaptureSizeFromConfig(FIntPoint& OutSize) const override;
	//~ End FDisplayClusterMediaCaptureViewport
};
