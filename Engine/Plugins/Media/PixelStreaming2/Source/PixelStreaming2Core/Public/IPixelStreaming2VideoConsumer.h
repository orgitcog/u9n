// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UObject/Interface.h"
#include "RHIFwd.h"

#include "IPixelStreaming2VideoConsumer.generated.h"

UINTERFACE(MinimalAPI)
class UPixelStreaming2VideoConsumer : public UInterface
{
	GENERATED_BODY()
};

/**
 * An "Video Consumer" is an object that is responsible for outputting the video received from a peer. For example, by
 * rendering to a render target.
 */
class PIXELSTREAMING2CORE_API IPixelStreaming2VideoConsumer
{
	GENERATED_BODY()

public:
	/**
	 * @brief Consume a texture as a video frame.
	 * @param Frame The Frame to consume.
	 */
	virtual void ConsumeFrame(FTextureRHIRef Frame) = 0;

	/**
	 * @brief Called when a video consumer is added.
	 */
	virtual void OnConsumerAdded() = 0;

	/**
	 * @brief Called when a video consumer is removed.
	 */
	virtual void OnConsumerRemoved() = 0;
};
