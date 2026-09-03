// Copyright Epic Games, Inc. All Rights Reserved.

#include "VideoProducer.h"

namespace UE::PixelStreaming2
{
	namespace VideoProducerIdentifiers
	{
		const FString FVideoProducer = TEXT("The default video producer - override me");
		const FString FVideoProducerBackBuffer = TEXT("the Back Buffer");
		const FString FVideoProducerMediaCapture = TEXT("a Media Capture Video Input");
		const FString FVideoProducerPIEViewport = TEXT("the PIE Viewport");
		const FString FVideoProducerRenderTarget = TEXT("a Render Target");
	}

	TSharedPtr<FVideoProducer> FVideoProducer::Create()
	{
		return TSharedPtr<FVideoProducer>(new FVideoProducer());
	}

	void FVideoProducer::PushFrame(const IPixelCaptureInputFrame& InputFrame)
	{
		OnFramePushed.Broadcast(InputFrame);
	}

	FString FVideoProducer::ToString()
	{
		return VideoProducerIdentifiers::FVideoProducer;
	}

} // namespace UE::PixelStreaming2