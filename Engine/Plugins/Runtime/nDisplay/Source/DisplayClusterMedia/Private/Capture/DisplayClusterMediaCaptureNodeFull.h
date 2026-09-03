// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Capture/DisplayClusterMediaCaptureNodeBase.h"

class FRHICommandListImmediate;
class FViewport;
class UDisplayClusterMediaOutputSynchronizationPolicy;
class UMediaOutput;


/**
 * Node backbuffer media capture (whole buffer)
 */
class FDisplayClusterMediaCaptureNodeFull
	: public FDisplayClusterMediaCaptureNodeBase
{
public:

	/** Constructor */
	FDisplayClusterMediaCaptureNodeFull(
		const FString& MediaId,
		const FString& ClusterNodeId,
		UMediaOutput* MediaOutput,
		UDisplayClusterMediaOutputSynchronizationPolicy* SyncPolicy = nullptr
	);

protected:

	/** Returns backbuffer size */
	virtual FIntPoint GetCaptureSize() const override;

	/** PostBackbufferUpdated handler */
	virtual void ProcessPostBackbufferUpdated_RenderThread(FRHICommandListImmediate& RHICmdList, FViewport* Viewport) override;
};
