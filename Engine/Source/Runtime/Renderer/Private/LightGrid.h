// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LumenGrid.h:
=============================================================================*/

#pragma once

#include "GPUMessaging.h"
#include "RenderGraphResources.h"

class FViewInfo;

class FLightGridViewState
{
public:

	FLightGridViewState();

	void FeedbackStatus(FRDGBuilder& GraphBuilder, FViewInfo& View, FRDGBufferRef CulledLightDataAllocatorBuffer, uint32 NumCulledLightDataEntries, FRDGBufferRef CulledLightLinkAllocatorBuffer, uint32 NumCulledLightLinks, bool bUseAsyncCompute);

#if !UE_BUILD_SHIPPING
	uint32 GetStatusMessageId() const { return StatusFeedbackSocket.GetMessageId().GetIndex(); }
#endif

private:

#if !UE_BUILD_SHIPPING
	GPUMessage::FSocket StatusFeedbackSocket;
	uint32 MaxEntriesHighWaterMark = 0;
#endif
};